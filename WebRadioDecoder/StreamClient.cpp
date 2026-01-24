#include "StreamClient.hpp"

extern "C"
{
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/bsdsocket.h>
}

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

struct Library *SocketBase = NULL;

/// @brief constructor
NetworkStream::NetworkStream() : m_connected(false), m_terminate(false),
                                 m_bytesRead(0), m_workerProc(NULL)
{
    m_socket = -1;
    m_q = NULL;
    m_channels = 2;
    m_sampleRate =44100;
}

/// @brief destructor
NetworkStream::~NetworkStream()
{
    CloseStream();
}

/// @brief opens a stream by url HTTP://mystream.org/test.mp3
/// @param filename
/// @return
bool NetworkStream::open(const char *filename)
{
    if (m_connected)
        return false;

    std::string fName = filename;
    // to lowercase
    stringToLower(fName);

    // set the port
    if (strstr(fName.c_str(), "http://"))
        m_port = 80;
    else
    {
        printf("No HHTPS\n");
        return false;
    }
    // set url and file-dest
    size_t pos = fName.find("://");
    size_t slashPos = fName.find('/', pos + 3);
    strncpy(m_host, fName.substr(pos + 3, slashPos - (pos + 3)).c_str(), 127);
    strncpy(m_path, fName.substr(slashPos).c_str(), 127);

    printf("Connection to: %s %u %s\n", m_host, m_port, m_path);
    m_terminate = false;
    m_bytesRead = 0;

    //test if we have the right URL
    if (!testConnection())
        return false;

    // Create our task
    struct TagItem playerTags[] = {
        {NP_Entry, (IPTR)NetworkStream::TaskEntry},
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
void NetworkStream::TaskEntry()
{
    struct Task *me = FindTask(NULL);
    NetworkStream *self = (NetworkStream *)me->tc_UserData;
    if (self)
        self->StreamLoop();
}

bool NetworkStream::testConnection()
{
    struct Library *SocketBase = OpenLibrary((CONST_STRPTR) "bsdsocket.library", 4);
    if (!SocketBase)
        return false;

    struct hostent *he;
    struct sockaddr addr;

    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == -1)
    {
        CloseLibrary(SocketBase);
        return false;
    }

    bool resState = false;
    if ((he = gethostbyname(m_host)))
    {
        memset(&addr, 0, sizeof(struct sockaddr));
        addr.sa_family = AF_INET;
        uint16_t net_port = htons(m_port);
        memcpy(&addr.sa_data[0], &net_port, 2);
        memcpy(&addr.sa_data[2], he->h_addr, he->h_length);

        if (connect(m_socket, &addr, sizeof(struct sockaddr)) == 0)
        {
            char request[256];
            sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Amiga\r\nAccept: */*\r\nIcy-MetaData: 0\r\nConnection: close\r\n\r\n", m_path, m_host);
            send(m_socket, request, strlen(request), 0);

            //hol die server response
            char buffer[512];
            recv(m_socket, buffer, 511, 0);
            printf("Server Antwort: %s\n", buffer);

            //und werte sie aus
            std::string response(buffer);
            if (strstr(response.c_str(), "200 OK"))
                resState = true;
            else if (strstr(response.c_str(), "302 Found"))
            {
                size_t locPos = response.find("Location: ");
                if (locPos != std::string::npos)
                {
                    size_t start = locPos + 10; // Hinter "Location: "
                    size_t end = response.find("\r\n", start);

                    std::string newUrl = response.substr(start, end - start);
                    printf("Redirect gefunden! Neue Adresse: %s\n", newUrl.c_str());

                    size_t pos = newUrl.find("://");
                    size_t slashPos = newUrl.find('/', pos + 3);
                    strncpy(m_host, newUrl.substr(pos + 3, slashPos - (pos + 3)).c_str(), 127);
                    strncpy(m_path, newUrl.substr(slashPos).c_str(), 127);
                    printf("Connection to: %s %u %s\n", m_host, m_port, m_path);
                    resState = true;
                }
            }
        }
    }

    if (m_socket != -1)
    {
        CloseSocket(m_socket);
        m_socket = -1;
    }
    CloseLibrary(SocketBase);
    return resState;
}

/// @brief the Streamloop, handling the network and decoding
void NetworkStream::StreamLoop()
{
    struct Library *SocketBase = OpenLibrary((CONST_STRPTR) "bsdsocket.library", 4);
    if (!SocketBase)
        return;

    struct hostent *he;
    struct sockaddr addr;

    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == -1)
    {
        CloseLibrary(SocketBase);
        return;
    }

    if ((he = gethostbyname(m_host)))
    {
        memset(&addr, 0, sizeof(struct sockaddr));
        addr.sa_family = AF_INET;
        uint16_t net_port = htons(m_port);
        memcpy(&addr.sa_data[0], &net_port, 2);
        memcpy(&addr.sa_data[2], he->h_addr, he->h_length);

        if (connect(m_socket, &addr, sizeof(struct sockaddr)) == 0)
        {
            m_connected = true;
            char request[256];
            sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Amiga\r\nAccept: */*\r\nIcy-MetaData: 0\r\nConnection: close\r\n\r\n", m_path, m_host);
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
            while (!m_terminate)
            {
                // Wir versuchen in einer kleinen internen Schleife, den Socket leerzupumpen,
                // bevor wir überhaupt an das teure Dekodieren denken.
                int receivedInThisCycle = 0;
                int consecutiveEmpty = 0;

                while (currentDataInRaw < (RAW_BUF_SIZE - 2048) && consecutiveEmpty < 5)
                {
                    long res = recv(m_socket, (char *)(rawBuffer + currentDataInRaw),
                                    RAW_BUF_SIZE - currentDataInRaw, 0);

                    if (res > 0)
                    {
                        currentDataInRaw += res;
                        receivedInThisCycle += res;
                        consecutiveEmpty = 0;
                    }
                    else
                    {
                        // Vielleicht mal nen Delay(0) hier in?
                        consecutiveEmpty++;
                    }
                }

                // Wir dekodieren erst, wenn wir entweder ordentlich Daten haben
                // oder der Audio-Queue der Saft ausgeht.
                if (currentDataInRaw > 8192 || (m_q->getFreeSpace() > 10000 && currentDataInRaw > 1024))
                {
                    uint8_t *readPtr = rawBuffer;

                    // Wir dekodieren in einem "Burst", um den Overhead klein zu halten
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
                if (receivedInThisCycle == 0)
                    Delay(1);
                else
                    Delay(0);
            }
        }
    }

    // Cleanup
    // Wir habens erstellt, also bauen wirs auch ab
    delete m_q;
    if (m_socket != -1)
    {
        CloseSocket(m_socket);
        m_socket = -1;
    }
    CloseLibrary(SocketBase);
    m_connected = false;
}

/// @brief Close the StreamTask
void NetworkStream::CloseStream()
{
    m_terminate = true;
    int timeout = 100;
    while ((m_connected) && timeout-- > 0)
        Delay(2);
}
