#include "StreamRunner.hpp"
#include "StreamClient.hpp"

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include "../Shared/AmiSSL.hpp"
#include "../AACDecoder/AACStream.hpp"
// die muss hier hin sonst gibts kein Socket
#include <inline/bsdsocket.h>

void StreamRunner::Run(NetworkStream *parent)
{
    StreamRunner worker(parent);
    if (worker.openSocket())
    {
        parent->m_connected = true;
        if (parent->m_codec == 0)
            worker.processMP3Stream();
        else if (parent->m_codec == 1)
            worker.processAACStream();
    }
    worker.closeSocket();
    parent->m_connected = false;
}

StreamRunner::StreamRunner(NetworkStream *parent)
    : m_parent(parent), m_SocketBase(NULL), m_amiSSL(NULL),
      m_socket(-1) {}

StreamRunner::~StreamRunner()
{
}

unsigned char *GetLinearBuffer(unsigned char *ring, int pos, int minSize, int ringSize, unsigned char *tempStack)
{
    int linearData = ringSize - pos;
    if (linearData >= minSize)
        return ring + pos;

    // Wrap-around Handling: Wir kopieren das Fragment kurz auf den Stack
    memcpy(tempStack, ring + pos, linearData);
    memcpy(tempStack + linearData, ring, minSize - linearData);
    return tempStack;
}

bool StreamRunner::openSocket()
{
    struct Library *SocketBase = NULL;
    if (!m_parent->m_isHTTP)
    {
        m_amiSSL = new AmiSSL();
        if (!m_amiSSL->Init() || !m_amiSSL->OpenConnection(m_parent->m_host, m_parent->m_port))
        {
            if (m_amiSSL)
            {
                delete m_amiSSL;
                m_amiSSL = NULL;
            }
            return false;
        }
        m_socket = m_amiSSL->GetSocket();
        m_SocketBase = m_amiSSL->GetSocketBase();
    }
    else
    {
        m_SocketBase = OpenLibrary((CONST_STRPTR) "bsdsocket.library", 4);
        if (!m_SocketBase)
            return false;
        SocketBase = m_SocketBase;
        struct hostent *he = gethostbyname(m_parent->m_host);
        m_socket = socket(AF_INET, SOCK_STREAM, 0);

        if (m_socket == -1 || !he)
            return false;

        struct sockaddr addr;
        memset(&addr, 0, sizeof(addr));
        addr.sa_family = AF_INET;
        uint16_t net_port = htons(m_parent->m_port);
        memcpy(&addr.sa_data[0], &net_port, 2);
        memcpy(&addr.sa_data[2], he->h_addr, 4);

        if (connect(m_socket, &addr, sizeof(addr)) != 0)
            return false;
    }

    char request[383];
    sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Amiga\r\nConnection: close\r\n\r\n", m_parent->m_path, m_parent->m_host);
    if (m_amiSSL)
        SSL_write(m_amiSSL->GetSSL(), request, strlen(request));
    else
        send(m_socket, request, strlen(request), 0);

    return true;
}

void StreamRunner::processMP3Stream()
{
    struct Library *SocketBase = NULL;
    SocketBase = m_SocketBase;

    m_parent->m_connected = true;

    // Ringpuffer Setup (64KB ist ideal für MP3)
    const int RING_SIZE = 65536;
    const int RING_MASK = RING_SIZE - 1;
    unsigned char *ringBuffer = (unsigned char *)AllocVec(RING_SIZE, MEMF_PUBLIC | MEMF_CLEAR);

    int writePos = 0; // Netzwerk-Schreibzeiger
    int readPos = 0;  // Decoder-Lesezeiger
    int filled = 0;   // Wie viele Bytes sind "ungelesen" im Puffer

    mp3dec_t mp3d;
    mp3dec_init(&mp3d);
    mp3dec_frame_info_t info;
    short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    while (!m_parent->m_terminate)
    {
        int space = (RING_SIZE - filled) - 1;
        if (space > 512)
        {
            int toRead = (writePos + space > RING_SIZE) ? (RING_SIZE - writePos) : space;
            long res = 0;
            if (m_amiSSL)
                res = SSL_read(m_amiSSL->GetSSL(), (char *)(ringBuffer + writePos), toRead);
            else
                res = recv(m_socket, (char *)(ringBuffer + writePos), toRead, 0);

            if (res > 0)
            {
                writePos = (writePos + res) & RING_MASK;
                filled += res;
            }
            else if (res == 0 && !m_amiSSL)
            { // Bei SSL bedeutet res=0 nicht immer Ende
                break;
            }
        }
        // MP3-Frames sind klein (meist < 1440 Bytes), 4096 Bytes Puffer reicht dicke
        while (filled > 1440 && m_parent->m_q->getFreeSpace() >= 2304)
        {
            unsigned char frameStack[2048]; // Temporär für "Knick" im Ring
            unsigned char *decodePtr = GetLinearBuffer(ringBuffer, readPos, 1440, RING_SIZE, frameStack);
            int samples = mp3dec_decode_frame(&mp3d, decodePtr, filled, pcm, &info);
            if (info.frame_bytes > 0)
            {
                // Sync Metadaten (Samplerate etc.)
                m_parent->m_sampleRate = info.hz;
                m_parent->m_channels = info.channels;

                if (samples > 0)
                {
                    // Volume Scaling (90%)
                    for (int i = 0; i < samples * info.channels; i++)
                        pcm[i] = (pcm[i] * 9) / 10;

                    m_parent->m_q->put(pcm, samples * info.channels);
                    m_parent->m_totalSamples += samples;
                }

                readPos = (readPos + info.frame_bytes) & RING_MASK;
                filled -= info.frame_bytes;
            }
            else
            {
                // Kein Frame gefunden? Ein Byte skippen (Sync-Suche)
                readPos = (readPos + 1) & RING_MASK;
                filled--;
            }
        }
        // CPU entlasten, wenn Puffer voll genug
        Delay(filled > (RING_SIZE / 2) ? 1 : 0);
    }
    FreeVec(ringBuffer);
}

void StreamRunner::processAACStream()
{
    struct Library *SocketBase = NULL;
    SocketBase = m_SocketBase;

    AACStream *aac = new AACStream();
    aac->justInit();

    short pcm[4096]; // Puffer für einen Frame
    m_parent->m_totalSamples = 0;

    const int RING_SIZE = 65536;
    const int RING_MASK = RING_SIZE - 1; // Für schnellen Modulo via AND
    unsigned char *ringBuffer = (unsigned char *)AllocVec(RING_SIZE, MEMF_PUBLIC | MEMF_CLEAR);

    int writePos = 0; // Hier schreibt das Netzwerk rein
    int readPos = 0;  // Hier liest der Decoder raus
    int filled = 0;   // Aktueller Füllstand

    const int volFactor = 29491; // 90% via Fixed Point

    // Decoder loop
    while (!m_parent->m_terminate)
    {
        // Wir schauen, wie viel Platz am Stück im Ring ist (ohne Wrap)
        int spaceToEnd = RING_SIZE - writePos;
        int maxToRead = (RING_SIZE - filled) - 1; // 1 Byte Sicherheit

        if (maxToRead > 0)
        {
            int toRead = (spaceToEnd < maxToRead) ? spaceToEnd : maxToRead;
            long res;

            if (m_amiSSL)
                res = SSL_read(m_amiSSL->GetSSL(), (char *)(ringBuffer + writePos), toRead);
            else
                res = recv(m_socket, (char *)(ringBuffer + writePos), toRead, 0);

            if (res > 0)
            {
                writePos = (writePos + res) & RING_MASK;
                filled += res;
            }
            else if (res == 0)
                m_parent->m_terminate = true;
        }

        // Wir dekodieren im Burst, solange genug Daten für einen Frame da sind
        while (filled > 4096 && m_parent->m_q->getFreeSpace() >= 4096)
        {
            size_t consumed = 0;
            // Problem: AAC-Frames können über den Rand des Puffers ragen (Wrap-around).
            // Lösung: Wir kopieren das Frame kurz in einen kleinen linearen Puffer,
            // wenn es am Rand "knickt".
            unsigned char frameStack[4096];
            unsigned char *decodePtr = GetLinearBuffer(ringBuffer, readPos, 1440, RING_SIZE, frameStack);
            size_t samplesProduced = aac->decodeFrame(decodePtr, filled, &consumed, pcm, 4096);

            if (consumed > 0)
            {
                // Volume-Scaling
                for (int i = 0; i < (int)samplesProduced; i++)
                    pcm[i] = (short)(((int)pcm[i] * volFactor) >> 15);

                m_parent->m_q->put(pcm, samplesProduced);

                readPos = (readPos + consumed) & RING_MASK;
                filled -= consumed;
                m_parent->m_totalSamples += (samplesProduced / 2);
            }
            else
            {
                // Wenn nichts konsumiert wurde (z.B. Header-Suche), 1 Byte skippen
                readPos = (readPos + consumed) & RING_MASK;
                filled--;
            }
        }
        if (m_parent->m_q->getFreeSpace() < 8192)
        {
            Delay(1);
        }
        else
        {
            // Falls Netzwerk gerade nichts liefert, ganz kurzes Yield
            if (filled < 4096)
                Delay(1);
        }
    }

    // Cleanup
    FreeVec(ringBuffer);

    // Cleanup
    //  Wir habens erstellt, also bauen wirs auch ab
    if (aac)
        delete aac;
}

void StreamRunner::closeSocket()
{
    if (m_amiSSL)
    {
        m_amiSSL->CleanupAll();
        delete m_amiSSL;
    }
    else
    {
        struct Library *SocketBase = NULL;
        SocketBase = m_SocketBase;
        if (m_socket != -1)
            CloseSocket(m_socket);
        if (m_SocketBase)
        {
            CloseLibrary(m_SocketBase);
            m_SocketBase = NULL;
        }
    }
    m_socket = -1;
}