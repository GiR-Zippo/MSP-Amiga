#include "StreamClient.hpp"

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include "../Shared/AmiSSL.hpp"

/// @brief constructor
NetworkStream::NetworkStream() : m_connected(false), m_terminate(false),
                                 m_bytesRead(0), m_workerProc(NULL)
{
    m_socket = -1;
    m_q = NULL;
    m_channels = 2;
    m_sampleRate = 44100;
    m_totalSamples = 0;
    m_isHTTP = true;
}

/// @brief destructor
NetworkStream::~NetworkStream()
{
    closeStream();
}

/// @brief opens a stream by url HTTP://mystream.org/test.mp3
/// @param url
bool NetworkStream::open(const char *filename)
{
    if (m_connected)
        return false;

    std::string fName = filename;
    //decode URL and set up port, host, ...
    decodeUrlData(fName);

    // test if we have the right URL
    if (!testConnection())
        return false;

    m_terminate = false;
    m_bytesRead = 0;
    // Create our task
    struct TagItem playerTags[] = {
        {NP_Entry, (IPTR)NetworkStream::taskEntry},
        {NP_Name, (IPTR) "StreamWorker"},
        {NP_Priority, 0},
        {NP_StackSize, 65536},
        {TAG_DONE, 0}};
    m_workerProc = (struct Process *)CreateNewProc(playerTags);
    if (m_workerProc)
        m_workerProc->pr_Task.tc_UserData = (APTR)this;

    return (m_workerProc != NULL);
}

/// @brief the task entry for the networker
void NetworkStream::taskEntry()
{
    struct Task *me = FindTask(NULL);
    NetworkStream *self = (NetworkStream *)me->tc_UserData;
    if (self)
        self->StreamLoop();
}

/// @brief the Streamloop, handling the network and decoding
void NetworkStream::StreamLoop()
{
    struct Library *SocketBase = NULL;
    AmiSSL *sslWrap = NULL;

    //ein wenig Vorbereitung: Socket öffnen und so
    if (!m_isHTTP)
    {
        sslWrap = new AmiSSL();
        if (!sslWrap->Init() || !sslWrap->OpenConnection(m_host, m_port))
        {
            delete sslWrap;
            return;
        }
    }
    else
    {
        SocketBase = OpenLibrary((CONST_STRPTR) "bsdsocket.library", 4);
        if (!SocketBase)
            return;

        struct hostent *he = gethostbyname(m_host);
        struct sockaddr addr;
        m_socket = socket(AF_INET, SOCK_STREAM, 0);

        if (m_socket == -1 || !he)
        {
            if (m_socket != -1)
                CloseSocket(m_socket);
            CloseLibrary(SocketBase);
            return;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sa_family = AF_INET;
        uint16_t net_port = htons(m_port);
        memcpy(&addr.sa_data[0], &net_port, 2);
        memcpy(&addr.sa_data[2], he->h_addr, 4);

        if (connect(m_socket, &addr, sizeof(addr)) != 0)
        {
            CloseSocket(m_socket);
            CloseLibrary(SocketBase);
            return;
        }
    }

    m_connected = true;
    //Request senden
    char request[256];
    sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Amiga\r\nConnection: close\r\n\r\n", m_path, m_host);
    if (sslWrap)
        SSL_write(sslWrap->GetSSL(), request, strlen(request));
    else
        send(m_socket, request, strlen(request), 0);

    // Puffer für die rohen MP3-Daten vom Socket
    const int RAW_BUF_SIZE = 65536;
    unsigned char *rawBuffer = (unsigned char *)AllocVec(RAW_BUF_SIZE, MEMF_ANY);
    int currentDataInRaw = 0;

    // der PCMBuffer für AHI
    m_q = new AudioQueue(176400);

    mp3dec_t mp3d;
    mp3dec_init(&mp3d);
    mp3dec_frame_info_t info;
    short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME]; // Puffer für einen Frame
    m_totalSamples = 0;

    // decoder loop
    while (!m_terminate)
    {
        // Wir versuchen in einer kleinen internen Schleife, den Socket leerzupumpen,
        // bevor wir überhaupt an das teure Dekodieren denken.
        int receivedInThisCycle = 0;
        int consecutiveEmpty = 0;

        //read buffer
        while (currentDataInRaw < (RAW_BUF_SIZE - 2048) && consecutiveEmpty < 5)
        {
            long res;
            if (sslWrap)
                res = SSL_read(sslWrap->GetSSL(), (char *)(rawBuffer + currentDataInRaw), RAW_BUF_SIZE - currentDataInRaw);
            else
                res = recv(m_socket, (char *)(rawBuffer + currentDataInRaw), RAW_BUF_SIZE - currentDataInRaw, 0);

            if (res > 0)
            {
                currentDataInRaw += res;
                receivedInThisCycle += res;
                consecutiveEmpty = 0;
            }
            else if (res == 0)
            {
                m_terminate = true; // wir sind disconnected (wahrscheinlich)
                break;
            }
            else
                consecutiveEmpty++; // Vielleicht mal nen Delay(0) hier rein?
        }

        // Wir dekodieren erst, wenn wir entweder ordentlich Daten haben
        // oder der Audio-Queue der Saft ausgeht.
        if (currentDataInRaw > 8192 || (m_q->getFreeSpace() > 10000 && currentDataInRaw > 1024))
        {
            // Wir dekodieren in einem "Burst", um den Overhead klein zu halten
            uint8_t *readPtr = rawBuffer;
            while (currentDataInRaw > 1024 && m_q->getFreeSpace() >= 2304)
            {
                int samples = mp3dec_decode_frame(&mp3d, readPtr, currentDataInRaw, pcm, &info);
                if (info.frame_bytes > 0)
                {
                    Forbid();
                    m_sampleRate = info.hz;
                    m_channels = info.channels;
                    Permit();
                    if (samples > 0)
                    {
                        Forbid();
                        m_totalSamples += samples;
                        Permit();
                        // Ein kleiner "Safety Margin" (ca. 90% Lautstärke)
                        // Damit es nicht knackst
                        for (int i = 0; i < samples * info.channels; i++)
                            pcm[i] = (pcm[i] * 9) / 10;
                        m_q->put(pcm, samples * info.channels);
                    }
                    readPtr += info.frame_bytes;
                    currentDataInRaw -= info.frame_bytes;
                }
                else
                    break;
            }
            // Rest nach vorne schieben
            if (currentDataInRaw > 0 && readPtr != rawBuffer)
                memmove(rawBuffer, readPtr, currentDataInRaw);
        }
        // Wenn wir kaum Daten bekommen haben, warten wir etwas länger (TCP-Stack entlasten)
        // Wenn wir voll im Flow sind, machen wir Delay(0) oder gar nichts.
        Delay(receivedInThisCycle == 0 ? 1 : 0);
    }

    //Cleanup
    // Wir habens erstellt, also bauen wirs auch ab
    delete m_q;
    FreeVec(rawBuffer);
    if (sslWrap)
    {
        sslWrap->CleanupAll();
        delete sslWrap;
    }
    else
    {
        if (m_socket != -1)
            CloseSocket(m_socket);
        if (SocketBase)
            CloseLibrary(SocketBase);
    }
    m_socket = -1;
    m_connected = false;
}

/// @brief Close the StreamTask
void NetworkStream::closeStream()
{
    m_terminate = true;
    int timeout = 100;
    while ((m_connected) && timeout-- > 0)
        Delay(2);
}

/// @brief Decodes the URl, sets m_port m_host m_path and m_isHTTP
/// @param url
void NetworkStream::decodeUrlData(std::string url)
{
    // to lowercase
    stringToLower(url);

    // set the port and proto
    if (strstr(url.c_str(), "http://"))
    {
        m_isHTTP = true;
        m_port = 80;
    }
    else
    {
        m_isHTTP = false;
        m_port = 443;
    }

    // set url and file-dest
    size_t pos = 3 + url.find("://"); // jap +3
    size_t portSep = url.find(':', pos) + 1;
    size_t slashPos = url.find('/', pos);
    // wenn ein Port angegeben ist, Port setzen
    if (portSep > 0)
    {
        m_port = atoi(url.substr(portSep, slashPos - portSep).c_str());
        strncpy(m_host, url.substr(pos, (portSep - 1) - pos).c_str(), 127);
    }
    else
        strncpy(m_host, url.substr(pos, slashPos - pos).c_str(), 127);
    strncpy(m_path, url.substr(slashPos).c_str(), 127);

    printf("Connection to: %s %u %s\n", m_host, m_port, m_path);
}

/// @brief test the connction and set port and url on redirect
/// @return true if everything is okay
bool NetworkStream::testConnection()
{
    char request[256];
    char buffer[512];
    int bytesRead = 0;
    bool resState = false;
    sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Amiga\r\nConnection: close\r\n\r\n", m_path, m_host);

    if (m_isHTTP)
    {
        struct Library *SocketBase = OpenLibrary((CONST_STRPTR) "bsdsocket.library", 4);
        if (!SocketBase)
            return false;

        struct hostent *he;
        struct sockaddr addr;

        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket == -1)
            return false;

        he = gethostbyname(m_host);
        if (he)
        {
            memset(&addr, 0, sizeof(struct sockaddr));
            addr.sa_family = AF_INET;
            uint16_t net_port = htons(m_port);
            memcpy(&addr.sa_data[0], &net_port, 2);
            memcpy(&addr.sa_data[2], he->h_addr, 4);

            if (connect(m_socket, &addr, sizeof(struct sockaddr)) == 0)
            {
                send(m_socket, request, strlen(request), 0);
                bytesRead = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
            }
        }
        if (m_socket != -1)
        {
            CloseSocket(m_socket);
            m_socket = -1;
        }
        CloseLibrary(SocketBase);
    }
    else
    {
        AmiSSL *sslWrap = new AmiSSL();
        if (sslWrap->Init() && sslWrap->OpenConnection(m_host, m_port))
        {
            SSL_write(sslWrap->GetSSL(), request, strlen(request));
            bytesRead = SSL_read(sslWrap->GetSSL(), buffer, sizeof(buffer) - 1);
        }
        sslWrap->CleanupAll();
        delete sslWrap;
    }

    if (bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        resState = handleServerResponse(buffer);
    }
    return resState;
}

/// @brief hendles the server response
/// @param response
/// @return true if everything okay
bool NetworkStream::handleServerResponse(std::string response)
{
    if (strstr(response.c_str(), "200 OK"))
        return true;
    else if (strstr(response.c_str(), "302 Found"))
    {
        // wenn redirect, dann neue URL holen
        size_t locPos = response.find("Location: ");
        if (locPos != std::string::npos)
        {
            size_t start = locPos + 10; // Hinter "Location: "
            size_t end = response.find("\r\n", start);
            decodeUrlData(response.substr(start, end - start));
            return true;
        }
    }
    else if (strstr(response.c_str(), "400 Bad Request"))
        return false;
    return false;
}