#include "StreamRunner.hpp"
#include "StreamClient.hpp"

#define MAX_HEADER_SIZE 8192 // HTTP header - Sicher ist sicher
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include "../Shared/AmiSSL.hpp"
#include "../AACDecoder/AACStream.hpp"
// die muss hier hin sonst gibts kein Socket
#include <inline/bsdsocket.h>

void StreamRunner::Run(NetworkStream *parent)
{
    StreamRunner worker(parent);
    writeToBuffer(parent->m_artist, "Connecting...");
    if (worker.openSocket())
    {
        writeToBuffer(parent->m_artist, "Connected...");
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

unsigned char* GetLinearBuffer(unsigned char* ring, int pos, int len, int ringSize, unsigned char* dest) 
{
    int linear = ringSize - pos;

    if (linear >= len)
    {
        if (dest)
        {
            memcpy(dest, ring + pos, len);
            return dest;
        }
        return ring + pos;
    }
    else
    {
        memcpy(dest, ring + pos, linear);
        memcpy(dest + linear, ring, len - linear);
        return dest;
    }
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
    sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\n"
                     "User-Agent: Amiga\r\n"
                     "Icy-MetaData: 1\r\n"
                     "Connection: close\r\n\r\n",
            m_parent->m_path, m_parent->m_host);
    if (m_amiSSL)
        SSL_write(m_amiSSL->GetSSL(), request, strlen(request));
    else
        send(m_socket, request, strlen(request), 0);

    return true;
}

void StreamRunner::processMP3Stream()
{
    m_parent->m_connected = true;

    // Ringpuffer Setup (64KB ist ideal für MP3)
    const int RING_SIZE = 65536;
    const int RING_MASK = RING_SIZE - 1;
    m_ringBuffer = (unsigned char *)AllocVec(RING_SIZE, MEMF_PUBLIC | MEMF_CLEAR);

    m_writePos = 0; // Netzwerk-Schreibzeiger
    m_readPos = 0;  // Decoder-Lesezeiger
    m_filled = 0;   // Wie viele Bytes sind "ungelesen" im Puffer

    mp3dec_t mp3d;
    mp3dec_init(&mp3d);
    mp3dec_frame_info_t info = {0};

    short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

    if (!readHeader())
    {
        writeToBuffer(m_parent->m_artist, "Err: No HTTP header...");
        FreeVec(m_ringBuffer);
        return;
    }

    while (!m_parent->m_terminate)
    {
        if (!readStream(RING_SIZE, RING_MASK))
            return;

        // MP3-Frames sind klein (meist < 1440 Bytes), 4096 Bytes Puffer reicht dicke
        while (!m_parent->m_terminate && m_filled > 1440 && m_parent->m_q->getFreeSpace() >= 2304)
        {
            unsigned char frameStack[4096]; // Temporär für "Knick" im Ring + Metadata Stitching
            unsigned char *decodePtr;
            m_metaLenBytes = 0;

            // ICY Metadata Handling
            if (m_icyInterval > 0 && m_bytesUntilMeta < 1440)
            {
                decodePtr = readIcyMeta(RING_SIZE, RING_MASK, frameStack);
                if (decodePtr == NULL)
                    break;
            }
            else
                decodePtr = GetLinearBuffer(m_ringBuffer, m_readPos, 1440, RING_SIZE, frameStack);

            int samples = mp3dec_decode_frame(&mp3d, decodePtr, 1440, pcm, &info);
            if (info.frame_bytes > 0)
            {
                // Sync Metadaten (Samplerate etc.)
                m_parent->m_sampleRate = info.hz == 0 ? 44100 : info.hz;
                m_parent->m_channels = info.channels == 0 ? 2 : info.channels;
                if (samples > 0)
                {
                    // Volume Scaling (90%)
                    for (int i = 0; i < samples * info.channels; i++)
                        pcm[i] = (pcm[i] * 9) / 10;

                    // für streams mit 1 kanal
                    if (info.channels == 1)
                    {
                        short stereoTemp[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
                        for (int i = 0; i < samples; i++)
                        {
                            stereoTemp[i * 2] = pcm[i];
                            stereoTemp[i * 2 + 1] = pcm[i];
                        }
                        m_parent->m_q->put(stereoTemp, samples * 2);
                        m_parent->m_channels = 2;
                    }
                    else
                        m_parent->m_q->put(pcm, samples * info.channels);

                    m_parent->m_totalSamples += samples;
                }

                if (m_icyInterval > 0)
                    icyAfterDecode(RING_SIZE, RING_MASK, info.frame_bytes);
                else
                {
                    m_readPos = (m_readPos + info.frame_bytes) & RING_MASK;
                    m_filled -= info.frame_bytes;
                }
            }
            else
            {
                // Kein Frame gefunden? Ein Byte skippen (Sync-Suche)
                if (m_icyInterval > 0 && m_bytesUntilMeta == 0)
                {
                    // Skip metadata block + 1 byte
                    int totalAdvance = m_metaLenBytes + 1;
                    m_readPos = (m_readPos + totalAdvance) & RING_MASK;
                    m_filled -= totalAdvance;
                    m_bytesUntilMeta = m_icyInterval - 1;
                }
                else
                {
                    m_readPos = (m_readPos + 1) & RING_MASK;
                    m_filled--;
                    if (m_icyInterval > 0)
                        m_bytesUntilMeta--;
                }
            }
        }
        // CPU entlasten, wenn Puffer voll genug
        Delay(m_filled > (RING_SIZE / 2) ? 1 : 0);
    }
    FreeVec(m_ringBuffer);
}

void StreamRunner::processAACStream()
{
    AACStream *aac = new AACStream();
    aac->justInit();

    short pcm[4096]; // Puffer für einen Frame
    m_parent->m_totalSamples = 0;

    const int RING_SIZE = 65536;
    const int RING_MASK = RING_SIZE - 1; // Für schnellen Modulo via AND
    m_ringBuffer = (unsigned char *)AllocVec(RING_SIZE, MEMF_PUBLIC | MEMF_CLEAR);

    m_writePos = 0; // Hier schreibt das Netzwerk rein
    m_readPos = 0;  // Hier liest der Decoder raus
    m_filled = 0;   // Aktueller Füllstand

    const int volFactor = 29491; // 90% via Fixed Point

    if (!readHeader())
    {
        writeToBuffer(m_parent->m_artist, "Err: No HTTP header...");
        FreeVec(m_ringBuffer);
        return;
    }

    // Decoder loop
    while (!m_parent->m_terminate)
    {
        if (!readStream(RING_SIZE, RING_MASK))
            return;

        // Wir dekodieren im Burst, solange genug Daten für einen Frame da sind
        while (!m_parent->m_terminate && m_filled > 4096 && m_parent->m_q->getFreeSpace() >= 4096)
        {
            unsigned char frameStack[4096]; // Temporär für "Knick" im Ring + Metadata Stitching
            unsigned char *decodePtr;
            m_metaLenBytes = 0;
            // ICY Metadata Handling
            if (m_icyInterval > 0 && m_bytesUntilMeta < 1440)
            {
                decodePtr = readIcyMeta(RING_SIZE, RING_MASK, frameStack);
                if (decodePtr == NULL)
                    break;
            }
            else
                decodePtr = GetLinearBuffer(m_ringBuffer, m_readPos, 1440, RING_SIZE, frameStack);

            size_t consumed = 0;
            size_t samples = aac->decodeFrame(decodePtr, m_filled, &consumed, pcm, 4096);
            if (consumed > 0)
            {
                m_parent->m_sampleRate = aac->getSampleRate() == 0 ? 44100 : aac->getSampleRate();
                m_parent->m_channels = aac->getChannels() == 0 ? 2 : aac->getChannels();
                if (samples > 0)
                {
                    // Volume-Scaling
                    for (int i = 0; i < (int)samples; i++)
                        pcm[i] = (short)(((int)pcm[i] * volFactor) >> 15);

                    m_parent->m_q->put(pcm, samples);
                    m_parent->m_totalSamples += (samples / 2);
                }

                if (m_icyInterval > 0)
                    icyAfterDecode(RING_SIZE, RING_MASK, consumed);
                else
                {
                    m_readPos = (m_readPos + consumed) & RING_MASK;
                    m_filled -= consumed;
                }
            }
            else
            {
                // Kein Frame gefunden? Ein Byte skippen (Sync-Suche)
                if (m_icyInterval > 0 && m_bytesUntilMeta == 0)
                {
                    // Skip metadata block + 1 byte
                    int totalAdvance = m_metaLenBytes + 1;
                    m_readPos = (m_readPos + totalAdvance) & RING_MASK;
                    m_filled -= totalAdvance;
                    m_bytesUntilMeta = m_icyInterval - 1;
                }
                else
                {
                    m_readPos = (m_readPos + 1) & RING_MASK;
                    m_filled--;
                    if (m_icyInterval > 0)
                        m_bytesUntilMeta--;
                }
            }
        }
        // CPU entlasten, wenn Puffer voll genug
        Delay(m_filled > (RING_SIZE / 2) ? 1 : 0);
    }

    // Cleanup
    FreeVec(m_ringBuffer);

    // Cleanup
    //  Wir habens erstellt, also bauen wirs auch ab
    if (aac)
        delete aac;
}

bool StreamRunner::readHeader()
{
    struct Library *SocketBase = NULL;
    SocketBase = m_SocketBase;
    const int RING_SIZE = 65536;
    const int RING_MASK = RING_SIZE - 1;

    m_icyInterval = 0;
    m_bytesUntilMeta = m_icyInterval;

    while (m_writePos < MAX_HEADER_SIZE)
    {
        long res = 0;
        if (m_amiSSL)
            res = SSL_read(m_amiSSL->GetSSL(), (char *)(m_ringBuffer + m_writePos), MAX_HEADER_SIZE);
        else
            res = recv(m_socket, (char *)(m_ringBuffer + m_writePos), MAX_HEADER_SIZE, 0);

        if (res <= 0)
            return false;

        m_writePos = (m_writePos + res) & RING_MASK;
        m_filled += res;

        if (strstr((const char *)m_ringBuffer, "icy-metaint"))
        {
            long metaInt = extract_icy_metaint((const char *)m_ringBuffer);
            if (metaInt > 0)
            {
                m_icyInterval = (int)metaInt;
                m_parent->m_icyInterval = m_icyInterval;
            }
            printf("ICY Metaint: %d\n", m_icyInterval);
        }
        if (strstr((const char *)m_ringBuffer, "\r\n\r\n"))
        {
            int offset = get_body_offset((const char *)m_ringBuffer, 8096);
            m_readPos = offset;
            // Reset bytesUntilMeta because body starts here
            m_bytesUntilMeta = m_icyInterval;
            break;
        }
    }
    return true;
}

bool StreamRunner::readStream(int RING_SIZE, int RING_MASK)
{
    struct Library *SocketBase = NULL;
    SocketBase = m_SocketBase;
    int space = (RING_SIZE - m_filled) - 1;
    if (space > 512)
    {
        int toRead = (m_writePos + space > RING_SIZE) ? (RING_SIZE - m_writePos) : space;
        long res = 0;
        if (m_amiSSL)
        {
            res = SSL_read(m_amiSSL->GetSSL(), (char *)(m_ringBuffer + m_writePos), toRead);
            if (res <= 0)
            {
                unsigned long err = ERR_get_error();
                char err_buf[256];
                ERR_error_string_n(err, err_buf, sizeof(err_buf));
                printf("AmiSSL Error: %s\n", err_buf);

                int ssl_err = SSL_get_error(m_amiSSL->GetSSL(), res);
                printf("SSL_get_error: %d\n", ssl_err);
                FreeVec(m_ringBuffer);
                writeToBuffer(m_parent->m_artist, "Err: AmiSSL read data...");
                return false;
            }
        }
        else
            res = recv(m_socket, (char *)(m_ringBuffer + m_writePos), toRead, 0);

        if (res > 0)
        {
            m_writePos = (m_writePos + res) & RING_MASK;
            m_filled += res;
        }
        else if (res == 0 && !m_amiSSL) // Bei SSL bedeutet res=0 nicht immer Ende
        {
            writeToBuffer(m_parent->m_artist, "Err: Socket read data...");
            FreeVec(m_ringBuffer);
            return false;
        }
    }
    return true;
}

unsigned char *StreamRunner::readIcyMeta(int RING_SIZE, int RING_MASK, unsigned char *tempStack)
{
    // Check if we have enough data for length byte
    if (m_filled < m_bytesUntilMeta + 1)
        return NULL;

    int metaPos = (m_readPos + m_bytesUntilMeta) & RING_MASK;
    unsigned char lenByte = m_ringBuffer[metaPos];
    int metaSize = lenByte * 16;

    // Check if we have full metadata + some frame data
    if (m_filled < m_bytesUntilMeta + 1 + metaSize + 100)
        return NULL;

    GetLinearBuffer(m_ringBuffer, m_readPos, m_bytesUntilMeta, RING_SIZE, tempStack);
    if (metaSize > 0)
    {
        char metadata[4096];
        int metaContentPos = (metaPos + 1) & RING_MASK;
        GetLinearBuffer(m_ringBuffer, metaContentPos, metaSize, RING_SIZE, (unsigned char *)metadata);
        metadata[metaSize] = 0;
        // Parse StreamTitle
        char *titleStart = strstr(metadata, "StreamTitle='");
        if (titleStart)
        {
            titleStart += 13;
            char *titleEnd = strstr(titleStart, "';");
            if (titleEnd)
            {
                *titleEnd = 0;
                //if 0x00 no title there
                if (titleStart[0] != 00)
                {
                    std::vector<std::string> parts = Split(std::string(titleStart), " - ");
                    writeToBuffer(m_parent->m_title, parts[1].c_str());
                    writeToBuffer(m_parent->m_artist, parts[0].c_str());
                }
            }
        }
    }

    // Copy Part 2 (after meta)
    int part2Len = 1440 - m_bytesUntilMeta;
    int part2Pos = (metaPos + 1 + metaSize) & RING_MASK;
    GetLinearBuffer(m_ringBuffer, part2Pos, part2Len, RING_SIZE, tempStack + m_bytesUntilMeta);

    m_metaLenBytes = 1 + metaSize;
    return tempStack;
}

void StreamRunner::icyAfterDecode(int RING_SIZE, int RING_MASK, int frame_bytes)
{
    if (m_bytesUntilMeta < frame_bytes)
    {
        // We crossed metadata
        int part1 = m_bytesUntilMeta;
        int part2 = frame_bytes - part1;
        int totalAdvance = part1 + m_metaLenBytes + part2;
        m_readPos = (m_readPos + totalAdvance) & RING_MASK;
        m_filled -= totalAdvance;
        m_bytesUntilMeta = m_icyInterval - part2;
    }
    else
    {
        m_readPos = (m_readPos + frame_bytes) & RING_MASK;
        m_filled -= frame_bytes;
        m_bytesUntilMeta -= frame_bytes;
    }
}

void StreamRunner::closeSocket()
{
    if (m_amiSSL)
    {
        m_amiSSL->CleanupAll();
        delete m_amiSSL;
        m_amiSSL = NULL;
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

int StreamRunner::get_body_offset(const char *buffer, int len)
{
    char *pos = strstr(buffer, "\r\n\r\n");
    if (pos)
    {
        // Die Position nach den 4 Zeichen (\r \n \r \n) berechnen
        int offset = (pos - buffer) + 4;

        // Sicherheitscheck: Ist der Offset noch innerhalb der empfangenen Daten?
        if (offset <= len)
            return offset;
    }
    return -1; // Header noch unvollständig
}

long StreamRunner::extract_icy_metaint(const char *header)
{
    char *field = strcasestr(header, "icy-metaint:");
    if (field)
    {
        char *value_start = strchr(field, ':');
        if (value_start)
        {
            value_start++;
            return atol(value_start);
        }
    }
    return 0;
}