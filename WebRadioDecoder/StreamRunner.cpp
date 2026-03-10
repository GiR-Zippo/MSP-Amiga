#include "StreamRunner.hpp"
#include "StreamClient.hpp"

#define MAX_HEADER_SIZE 8192 // HTTP header - Sicher ist sicher
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

extern "C"
{
#include "../Shared/libtremor/ivorbisfile.h"
}

extern "C"
{
#include "../Shared/libopus/include/opus.h"
}

#include "../Shared/AmiSSL.hpp"
#include "../AACDecoder/AACStream.hpp"
// die muss hier hin sonst gibts kein Socket
#include <inline/bsdsocket.h>

void StreamRunner::Run(NetworkStream *parent)
{
    StreamRunner worker(parent);
    parent->setArtist("Connecting...");
    worker.startup();
    parent->m_connected = false;
}

StreamRunner::StreamRunner(NetworkStream *parent)
    : m_parent(parent), m_SocketBase(NULL), m_amiSSL(NULL),
      m_socket(-1) {}

StreamRunner::~StreamRunner()
{
}

unsigned char *GetLinearBuffer(unsigned char *ring, int pos, int len, int ringSize, unsigned char *dest)
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

void StreamRunner::startup()
{
    m_ringBuffer = (unsigned char *)AllocVec(RING_SIZE, MEMF_PUBLIC | MEMF_CLEAR);
    if (!m_ringBuffer)
    {
        m_parent->setArtist("Err: Out of memory");
        return;
    }
    if (openSocket())
    {
        m_parent->m_totalSamples = 0;
        m_writePos = 0; // Hier schreibt das Netzwerk rein
        m_readPos = 0;  // Hier liest der Decoder raus
        m_filled = 0;   // Aktueller Füllstand
        if (readHeader())
        {
            m_parent->setArtist("Connected...");
            m_parent->m_connected = true;
            if (m_codec == 0)
                processMP3Stream();
            else if (m_codec == 1)
                processAACStream();
            else if (m_codec == 2)
                processVorbisStream();
            else if (m_codec == 3)
                processOpusStream();
        }
    }
    FreeVec(m_ringBuffer);
    closeSocket();
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
                     "Connection: keep-alive\r\n\r\n",
            m_parent->m_path, m_parent->m_host);
    if (m_amiSSL)
        SSL_write(m_amiSSL->GetSSL(), request, strlen(request));
    else
        send(m_socket, request, strlen(request), 0);

    return true;
}

void StreamRunner::processMP3Stream()
{
    mp3dec_t mp3d;
    mp3dec_init(&mp3d);
    mp3dec_frame_info_t info = {0};

    short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    while (!m_parent->m_terminate)
    {
        if (!readStream(RING_SIZE, RING_MASK))
            return;

        // MP3-Frames sind klein (meist < 1440 Bytes), 4096 Bytes Puffer reicht dicke
        //while (!m_parent->m_terminate && m_filled > 1440 && m_parent->m_q->getFreeSpace() >= 2304)
        while (!m_parent->m_terminate && m_filled > 32768 && m_parent->m_q->getFreeSpace() >= 88200)
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
}

void StreamRunner::processAACStream()
{
    AACStream *aac = new AACStream();
    aac->justInit();

    short pcm[4096];             // Puffer für einen Frame
    const int volFactor = 29491; // 90% via Fixed Point

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
    //  Wir habens erstellt, also bauen wirs auch ab
    if (aac)
        delete aac;
}

void StreamRunner::processVorbisStream()
{
    short pcm[4096];

    struct RingCtx
    {
        unsigned char *buf;
        int *readPos;
        int *filled;
        int mask;
        volatile bool *terminate;
        int icyInterval;
        int *bytesUntilMeta;
    };

    RingCtx ctx;
    ctx.buf            = m_ringBuffer;
    ctx.readPos        = &m_readPos;
    ctx.filled         = &m_filled;
    ctx.mask           = RING_MASK;
    ctx.terminate      = &m_parent->m_terminate;
    ctx.icyInterval    = m_icyInterval;
    ctx.bytesUntilMeta = &m_bytesUntilMeta;

    ov_callbacks callbacks;

    callbacks.read_func = [](void *ptr, size_t size, size_t nmemb, void *ds) -> size_t
    {
        RingCtx *c      = (RingCtx *)ds;
        size_t   wanted = size * nmemb;
        size_t   avail  = (size_t)*c->filled;
        if (avail == 0) return 0;
        if (wanted > avail) wanted = avail;

        if (c->icyInterval > 0 && *c->bytesUntilMeta == 0)
        {
            int metaLen = c->buf[*c->readPos & c->mask] * 16;
            int skip    = 1 + metaLen;
            *c->readPos       = (*c->readPos + skip) & c->mask;
            *c->filled       -= skip;
            *c->bytesUntilMeta = c->icyInterval;
            avail = (size_t)*c->filled;
            if (wanted > avail) wanted = avail;
        }

        for (size_t i = 0; i < wanted; i++)
            ((unsigned char *)ptr)[i] = c->buf[(*c->readPos + i) & c->mask];

        *c->readPos  = (*c->readPos + wanted) & c->mask;
        *c->filled  -= wanted;
        if (c->icyInterval > 0)
            *c->bytesUntilMeta -= wanted;

        return wanted / size;
    };

    callbacks.seek_func  = NULL;
    callbacks.close_func = NULL;
    callbacks.tell_func  = NULL;

    while (m_filled < 16384 && !m_parent->m_terminate)
    {
        if (!readStream(RING_SIZE, RING_MASK))
            return;
        Delay(1);
    }

    OggVorbis_File vf;
    if (ov_open_callbacks(&ctx, &vf, NULL, 0, callbacks) < 0)
    {
        m_parent->setArtist("Err: Invalid Vorbis stream");
        return;
    }

    vorbis_info *vi = ov_info(&vf, -1);
    if (vi)
    {
        m_parent->m_sampleRate = vi->rate;
        m_parent->m_channels   = vi->channels;
    }

    vorbis_comment *vc = ov_comment(&vf, -1);
    if (vc)
        parseVorbisComments((const char **)vc->user_comments, vc->comments);

    int currentSection = 0;
    int lastSection = -1;
    
    while (!m_parent->m_terminate)
    {
        if (!readStream(RING_SIZE, RING_MASK))
            break;

        // Queue-Bremse - wie Opus
        if (m_parent->m_q->getFreeSpace() < 88200)
        {
            Delay(1);
            continue;
        }

        while (!m_parent->m_terminate &&
               m_filled > 16384 &&
               m_parent->m_q->getFreeSpace() >= 88200)
        {
            long ret = ov_read(&vf, (char *)pcm, sizeof(pcm), &currentSection);
            if (currentSection != lastSection)
            {
                lastSection = currentSection;
                vorbis_comment *vc = ov_comment(&vf, -1);
                if (vc)
                    parseVorbisComments((const char **)vc->user_comments, vc->comments);
            }
            if (ret == 0)
                break;

            if (ret < 0)
                continue;

            int numShorts = (int)(ret / sizeof(short));
            if (numShorts > 0)
            {
                m_parent->m_q->put(pcm, numShorts);
                m_parent->m_totalSamples += numShorts / m_parent->m_channels;
            }
        }

        Delay(m_filled > (RING_SIZE / 2) ? 1 : 0);
    }

    ov_clear(&vf);
}

void StreamRunner::processOpusStream()
{
    short pcm[5760 * 2];
    unsigned char packetBuf[4096];
    int packetLen = 0;
    ogg_sync_state oy;
    ogg_stream_state os;
    ogg_page og;
    ogg_packet op;

    ogg_sync_init(&oy);

    int error;
    OpusDecoder *decoder = opus_decoder_create(48000, 2, &error);
    if (!decoder)
    {
        m_parent->setArtist("Err: Opus init failed");
        return;
    }

    m_parent->m_sampleRate = 48000;
    m_parent->m_channels = 2;
    int preskip = 0;
    bool streamInitialized = false;
    bool headerDone = false;
    bool tagsDone = false;

    while (!m_parent->m_terminate)
    {
        if (!readStream(RING_SIZE, RING_MASK))
            break;

        if (m_parent->m_q->getFreeSpace() < 88200)
        {
            Delay(1);
            continue;
        }

        while (!m_parent->m_terminate && m_filled > 32768 && m_parent->m_q->getFreeSpace() >= 88200)
        {
            unsigned char frameStack[1440];
            unsigned char *decodePtr;
            m_metaLenBytes = 0;

            if (m_icyInterval > 0 && m_bytesUntilMeta < 1440)
            {
                decodePtr = readIcyMeta(RING_SIZE, RING_MASK, frameStack);
                if (decodePtr == NULL)
                    break;
            }
            else
                decodePtr = GetLinearBuffer(m_ringBuffer, m_readPos, 1440, RING_SIZE, frameStack);

            char *syncBuf = ogg_sync_buffer(&oy, 1440);
            if (!syncBuf)
                break;

            memcpy(syncBuf, decodePtr, 1440);
            ogg_sync_wrote(&oy, 1440);

            if (m_icyInterval > 0)
                icyAfterDecode(RING_SIZE, RING_MASK, 1440);
            else
            {
                m_readPos = (m_readPos + 1440) & RING_MASK;
                m_filled -= 1440;
            }

            int pageRet;
            while ((pageRet = ogg_sync_pageout(&oy, &og)) != 0)
            {
                if (pageRet == -1)
                    continue;
                else if (pageRet == 1)
                {
                    // Neue BOS Page = neuer Stream
                    if (ogg_page_bos(&og))
                    {
                        if (streamInitialized)
                            ogg_stream_clear(&os);
                        ogg_stream_init(&os, ogg_page_serialno(&og));
                        streamInitialized = true;
                        headerDone        = false;
                        tagsDone          = false;
                        preskip           = 0;
                        opus_decoder_ctl(decoder, OPUS_RESET_STATE);
                    }
                    if (!streamInitialized)
                        continue;
                    ogg_stream_pagein(&os, &og);

                    while (ogg_stream_packetout(&os, &op) == 1)
                    {
                        if (!headerDone)
                        {
                            if (op.bytes >= 19 && memcmp(op.packet, "OpusHead", 8) == 0)
                            {
                                preskip = op.packet[10] | (op.packet[11] << 8);
                                if (preskip > 3840) preskip = 3840; // Max 80ms bei 48kHz
                                headerDone = true;
                            }
                            continue;
                        }

                        if (!tagsDone)
                        {
                            if (op.bytes >= 8 && memcmp(op.packet, "OpusTags", 8) == 0)
                            {
                                parseOpusTags(op.packet, op.bytes);
                                tagsDone = true;
                            }
                            continue;
                        }

                        // Packet in eigenen Buffer kopieren bevor nächste Page reinkommt
                        if (op.bytes > 0 && op.bytes <= 4096)
                        {
                            memcpy(packetBuf, op.packet, op.bytes);
                            packetLen = op.bytes;
                        }
                        else
                            continue;

                        int frames = opus_decode(decoder, packetBuf, packetLen, pcm, 5760, 0);
                        if (frames > 0)
                        {
                            if (preskip > 0)
                            {
                                int skip = (frames < preskip) ? frames : preskip;
                                preskip -= skip;
                                // Diesen Frame verwerfen
                                continue;
                            }
                            int numShorts = frames * 2;
                            m_parent->m_q->put(pcm, numShorts);
                            m_parent->m_totalSamples += frames;
                        }
                    }
                }
            }
        }

        Delay(m_filled > (RING_SIZE / 2) ? 1 : 0);
    }

    if (streamInitialized)
        ogg_stream_clear(&os);
    ogg_sync_clear(&oy);
    opus_decoder_destroy(decoder);
}

bool StreamRunner::readHeader()
{
    struct Library *SocketBase = NULL;
    SocketBase = m_SocketBase;
    m_icyInterval = 0;
    m_bytesUntilMeta = m_icyInterval;
    m_codec = 255;

    while (m_writePos < MAX_HEADER_SIZE)
    {
        long res = 0;
        if (m_amiSSL)
            res = SSL_read(m_amiSSL->GetSSL(), (char *)(m_ringBuffer + m_writePos), MAX_HEADER_SIZE);
        else
            res = recv(m_socket, (char *)(m_ringBuffer + m_writePos), MAX_HEADER_SIZE, 0);

        if (res <= 0)
        {
            m_parent->setArtist("Err: No HTTP header...");
            return false;
        }
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
            DLog("ICY Metaint: %d\n", m_icyInterval);
        }
        if (strstr((const char *)m_ringBuffer, "\r\n\r\n"))
        {
            int offset = get_body_offset((const char *)m_ringBuffer, 8096);
            m_readPos = offset;
            m_bytesUntilMeta = m_icyInterval;

            // Codec-Erkennung aus Content-Type
            const char *hdr = (const char *)m_ringBuffer;

            if (strstr(hdr, "Content-Type: audio/mpeg") ||
                strstr(hdr, "Content-Type: audio/mp3"))
            {
                m_codec = 0;
            }
            else if (strstr(hdr, "Content-Type: audio/aac") ||
                     strstr(hdr, "Content-Type: audio/aacp") ||
                     strstr(hdr, "Content-Type: audio/mp4"))
            {
                m_codec = 1;
            }
            else if (strstr(hdr, "Content-Type: audio/ogg") ||
                     strstr(hdr, "Content-Type: application/ogg") ||
                     strstr(hdr, "Content-Type: audio/vorbis") ||
                     strstr(hdr, "Content-Type: audio/x-ogg") ||
                     strstr(hdr, "Content-Type: audio/opus"))
            {
                // Nachlesen bis genug Audio-Bytes für Erkennung da sind
                while ((m_filled - offset) < 64 && !m_parent->m_terminate)
                {
                    long res = 0;
                    if (m_amiSSL)
                        res = SSL_read(m_amiSSL->GetSSL(), (char *)(m_ringBuffer + m_writePos), MAX_HEADER_SIZE);
                    else
                        res = recv(m_socket, (char *)(m_ringBuffer + m_writePos), MAX_HEADER_SIZE, 0);
                    if (res <= 0)
                        break;
                    m_writePos = (m_writePos + res) & RING_MASK;
                    m_filled += res;
                }

                const unsigned char *body = m_ringBuffer + offset;
                int bodyAvail = m_filled - offset;

                if (bodyAvail >= 8 && findBytes(body, bodyAvail, "OpusHead", 8))
                    m_codec = 3;
                else if (bodyAvail >= 7 && findBytes(body, bodyAvail, "\x01vorbis", 7))
                    m_codec = 2;
                else
                    m_codec = 255;
            }
            if (m_codec == 255)
            {
                m_parent->setArtist("Err: Unsupported codec...");
                return false;
            }
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
                DLog("AmiSSL Error: %s\n", err_buf);

                int ssl_err = SSL_get_error(m_amiSSL->GetSSL(), res);
                DLog("SSL_get_error: %d\n", ssl_err);
                m_parent->setArtist("Err: AmiSSL read data...");
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
            m_parent->setArtist("Err: Socket read data...");
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
                // if 0x00 no title there
                if (titleStart[0] != 00)
                {
                    std::vector<std::string> parts = Split(std::string(titleStart), " - ");
                    m_parent->setTitle(parts[1].c_str());
                    m_parent->setArtist(parts[0].c_str());
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

void StreamRunner::parseVorbisComments(const char **comments, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (strnicmp(comments[i], "TITLE=", 6) == 0)
            m_parent->setTitle(comments[i] + 6);
        else if (strnicmp(comments[i], "ARTIST=", 7) == 0)
            m_parent->setArtist(comments[i] + 7);
    }
}

void StreamRunner::parseOpusTags(const unsigned char *data, long bytes)
{
    const unsigned char *p   = data + 8;
    const unsigned char *end = data + bytes;

    if (p + 4 > end) return;
    uint32_t vendorLen = p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
    p += 4 + vendorLen;

    if (p + 4 > end) return;
    uint32_t count = p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
    p += 4;

    for (uint32_t i = 0; i < count && p + 4 <= end; i++)
    {
        uint32_t len = p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
        p += 4;
        if (p + len > end) break;

        char comment[256] = {0};
        int copyLen = (len < 255) ? len : 255;
        memcpy(comment, p, copyLen);
        comment[copyLen] = 0;
        p += len;

        if (strnicmp(comment, "TITLE=", 6) == 0)
            m_parent->setTitle(comment + 6);
        else if (strnicmp(comment, "ARTIST=", 7) == 0)
            m_parent->setArtist(comment + 7);
    }
}

bool StreamRunner::pumpOggData(ogg_sync_state *oy)
{
    unsigned char frameStack[1440];
    unsigned char *decodePtr;
    m_metaLenBytes = 0;

    if (m_icyInterval > 0 && m_bytesUntilMeta < 1440)
    {
        decodePtr = readIcyMeta(RING_SIZE, RING_MASK, frameStack);
        if (decodePtr == NULL) return false;
    }
    else
        decodePtr = GetLinearBuffer(m_ringBuffer, m_readPos, 1440, RING_SIZE, frameStack);

    char *syncBuf = ogg_sync_buffer(oy, 1440);
    if (!syncBuf) return false;

    memcpy(syncBuf, decodePtr, 1440);
    ogg_sync_wrote(oy, 1440);

    if (m_icyInterval > 0)
        icyAfterDecode(RING_SIZE, RING_MASK, 1440);
    else
    {
        m_readPos = (m_readPos + 1440) & RING_MASK;
        m_filled -= 1440;
    }
    return true;
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