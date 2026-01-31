#include "StreamClient.hpp"
#include "../Shared/AmiSSL.hpp"
#include "StreamRunner.hpp"
/// @brief constructor
NetworkStream::NetworkStream() : m_connected(false), m_terminate(false),
                                 m_workerProc(NULL)
{
    m_socket = -1;
    m_q = NULL;
    m_channels = 2;
    m_sampleRate = 44100;
    m_totalSamples = 0;
    m_isHTTP = true;
    m_stop = false;
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
    Forbid();
    m_stop = false;
    struct Task *oldTask = (struct Task *)FindTask((CONST_STRPTR) "StreamWorker");
    Permit();

    if (oldTask)
    {
        printf("Alter Task läuft noch! Erst beenden...\n");
        m_terminate = true; // Signal zum Abbruch senden

        // Kurz warten, bis er weg ist (Active Waiting)
        int timeout = 100; // max 2 Sekunden
        while (FindTask((CONST_STRPTR) "StreamWorker") && timeout-- > 0)
        {
            Delay(2);
        }
    }

    m_terminate = false;

    std::string fName = filename;
    // decode URL and set up port, host, ...
    decodeUrlData(fName);

    // test if we have the right URL
    int redirects = 3;
    while (redirects-- > 0)
    {
        if (!testConnection())
            return false;
        if (m_codec != 255)
            break;
    }

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
    {
        // Wir initialisieren die AudioQueue einmal hier,
        // bevor wir in den Worker springen
        self->m_q = new AudioQueue(176400);
        StreamRunner::Run(self);

        // Wenn der Worker fertig ist (Stream Ende oder Terminate)
        Forbid();
        self->m_stop = true;
        Permit();
         if (self->m_q)
        {
            delete self->m_q;
            self->m_q = NULL;
        }
    }
    Forbid();
    self->m_stop = true;
    Permit();
}

/// @brief Close the StreamTask
void NetworkStream::closeStream()
{
    m_terminate = true;
    int timeout = 100;
    while ((m_connected) && timeout-- > 0)
        Delay(2);
}

/// @brief test the connction and set port and url on redirect
/// @return true if everything is okay
bool NetworkStream::testConnection()
{
    char request[256];
    char buffer[512];
    int bytesRead = 0;
    bool resState = false;
    sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Amiga\r\nIcy-MetaData: 1\r\nConnection: close\r\n\r\n", m_path, m_host);

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