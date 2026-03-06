#include "OpusStream.hpp"
#include "../Shared/oggtag/oggtag.hpp"
extern "C"
{
#include "../Shared/libopus/include/opus.h"
}

#define OPUS_OK 0
#define OPUS_RESET_STATE 4028

OpusStream::OpusStream()
    : m_file(NULL), m_decoder(NULL), m_serialNo(0), m_granulePos(0), m_pageSequence(0), m_channels(2), m_preskip(0), m_samplesDecoded(0), m_totalSamples(0), m_packetSize(0), m_pcmBufferSize(0), m_pcmBufferPos(0), m_initialized(false), m_headerParsed(false)
{
    m_currentSegmentIdx = 0;
    m_currentPage.numSegments = 0;
    m_duration = 0;
    setTitle("Unknown Title");
    setArtist("Unknown Artist");
}

OpusStream::~OpusStream()
{
    close();
}

bool OpusStream::open(const char *filename)
{
    if (!filename)
        return false;

    DLog("OpusStream: Opening '%s'\n", filename);
    m_file = fopen(filename, "rb");
    if (!m_file)
    {
        DLog("OpusStream: Failed to open file\n");
        return false;
    }

    OggMeta meta = OggOpusReaderWriter::ReadMetaData(filename);
    m_duration = meta.duration;
    if (!meta.title.empty())
        setTitle(meta.title.c_str());

    if (!meta.artist.empty())
        setArtist(meta.artist.c_str());

    // Parse Opus Header
    if (!parseOpusHeader())
    {
        DLog("OpusStream: Failed to parse Opus header\n");
        fclose(m_file);
        m_file = NULL;
        return false;
    }

    // Parse Tags
    if (!parseOpusTags())
    {
        DLog("OpusStream: Warning - failed to parse tags\n");
        // Not fatal, continue
    }

    // Create Opus Decoder
    int error;
    m_decoder = opus_decoder_create(48000, m_channels, &error);

    if (error != OPUS_OK || !m_decoder)
    {
        DLog("OpusStream: Failed to create decoder (error %d)\n", error);
        fclose(m_file);
        m_file = NULL;
        return false;
    }

    DLog("OpusStream: Initialized successfully\n");
    DLog("  Channels: %d\n", m_channels);
    DLog("  Sample Rate: 48000 Hz\n");
    DLog("  Pre-skip: %d samples\n", m_preskip);

    m_initialized = true;
    m_samplesDecoded = 0;
    m_pcmBufferSize = 0;
    m_pcmBufferPos = 0;

    return true;
}

void OpusStream::close()
{
    if (m_decoder)
    {
        opus_decoder_destroy(m_decoder);
        m_decoder = NULL;
    }

    if (m_file)
    {
        fclose(m_file);
        m_file = NULL;
    }

    m_initialized = false;
    m_headerParsed = false;
}

int OpusStream::readSamples(short *buffer, int samplesToRead)
{
    if (!m_initialized || !buffer)
        return 0;

    int samplesRead = 0;

    while (samplesRead < samplesToRead)
    {
        // Erst Buffer leeren falls noch Daten da sind
        if (m_pcmBufferPos < m_pcmBufferSize)
        {
            int available = m_pcmBufferSize - m_pcmBufferPos;
            int toCopy = (samplesToRead - samplesRead);
            if (toCopy > available)
                toCopy = available;

            memcpy(buffer + samplesRead, m_pcmBuffer + m_pcmBufferPos, toCopy * sizeof(short));

            m_pcmBufferPos += toCopy;
            samplesRead += toCopy;

            continue;
        }

        // Buffer leer - decode nächstes Packet
        if (!decodePacket())
        {
            // End of stream
            memset(buffer, 0, (samplesToRead) * sizeof(short));
            return 0;
            break;
        }
    }

    return samplesRead / 2;
}

uint32_t OpusStream::getCurrentSeconds() const
{
    if (!m_initialized)
        return 0.0;

    // Granule Position ist in Samples @ 48kHz
    return (double)m_samplesDecoded / 48000.0;
}

bool OpusStream::seek(uint32_t targetSeconds)
{
    uint32_t totalsecs = m_duration;
    if (!m_initialized || !m_file || totalsecs == 0)
        return false;

    if (targetSeconds > totalsecs)
        targetSeconds = totalsecs;

    fseek(m_file, 0, SEEK_END);
    long fileSize = ftell(m_file);
    double ratio = (double)targetSeconds / (double)totalsecs;
    long targetOffset = (long)(ratio * fileSize);

    fseek(m_file, targetOffset, SEEK_SET);

    bool found = false;
    PageHeader header;

    // Wir scannen bis zu 64KB ab dem Target, um eine Ogg-Page zu finden
    uint8_t syncBuf[4096];
    for (int retry = 0; retry < 16; retry++)
    {
        long currentSearchPos = ftell(m_file);
        size_t readLen = fread(syncBuf, 1, 4096, m_file);
        if (readLen < 27)
            break;

        for (size_t i = 0; i < readLen - 27; i++)
        {
            if (syncBuf[i] == 'O' && syncBuf[i + 1] == 'g' && syncBuf[i + 2] == 'g' && syncBuf[i + 3] == 'S')
            {
                // Wir haben ein Ogg-S gefunden, jetzt prüfen wir die Serial
                fseek(m_file, currentSearchPos + i, SEEK_SET);
                if (readPageHeader(header))
                {
                    if (header.serialNumber == m_serialNo)
                    {
                        found = true;
                        m_currentPage = header;
                        m_currentSegmentIdx = 0;
                        if (header.granulePosition != (uint64_t)-1 && header.granulePosition > (uint64_t)m_preskip)
                            m_samplesDecoded = header.granulePosition - m_preskip;
                        else
                            m_samplesDecoded = 0;
                        break;
                    }
                    // Falsche Serial? Weitersuchen (fseek ist durch readPageHeader schon weiter)
                }
            }
        }
        if (found)
            break;
    }

    if (!found)
    {
        // Notfall: Wenn nichts gefunden wurde, an den Anfang zurück
        fseek(m_file, 0, SEEK_SET);
        parseOpusHeader();
        return false;
    }

    m_currentPage = header;
    m_currentSegmentIdx = 0; // Wir fangen die gefundene Page von vorne an

    opus_decoder_ctl(m_decoder, OPUS_RESET_STATE);
    m_pcmBufferSize = 0;
    m_pcmBufferPos = 0;

    return true;
}

// ============================================================================
// Private Methods - Ogg Parsing
// ============================================================================

bool OpusStream::readPageHeader(PageHeader &header)
{
    uint8_t buffer[27];

    if (fread(buffer, 1, 27, m_file) != 27)
        return false;

    // Check "OggS" magic
    if (memcmp(buffer, "OggS", 4) != 0)
        return false;

    header.version = buffer[4];
    header.headerType = buffer[5];
    header.granulePosition =
        ((uint64_t)buffer[6]) |
        ((uint64_t)buffer[7] << 8) |
        ((uint64_t)buffer[8] << 16) |
        ((uint64_t)buffer[9] << 24) |
        ((uint64_t)buffer[10] << 32) |
        ((uint64_t)buffer[11] << 40) |
        ((uint64_t)buffer[12] << 48) |
        ((uint64_t)buffer[13] << 56);
    // memcpy(&header.granulePosition, buffer + 6, 8);
    memcpy(&header.serialNumber, buffer + 14, 4);
    memcpy(&header.pageSequenceNumber, buffer + 18, 4);
    memcpy(&header.checksum, buffer + 22, 4);
    header.numSegments = buffer[26];

    // Read segment table
    if (fread(header.segmentTable, 1, header.numSegments, m_file) != header.numSegments)
        return false;

    return true;
}

bool OpusStream::parseOpusHeader()
{
    m_currentPage.numSegments = 0;
    m_currentSegmentIdx = 0;

    if (!readNextPacket())
        return false;

    // Check "OpusHead"
    if (m_packetSize < 19 || memcmp(m_packetBuffer, "OpusHead", 8) != 0)
    {
        DLog("OpusStream: Not an Opus stream\n");
        return false;
    }

    uint8_t version = m_packetBuffer[8];
    m_channels = m_packetBuffer[9];
    m_preskip = (uint16_t)m_packetBuffer[10] | ((uint16_t)m_packetBuffer[11] << 8);

    if (version != 1)
    {
        DLog("OpusStream: Unsupported version %d\n", version);
        return false;
    }

    if (m_channels < 1 || m_channels > 2)
    {
        DLog("OpusStream: Unsupported channels %d\n", m_channels);
        return false;
    }

    m_serialNo = m_currentPage.serialNumber;
    m_headerParsed = true;

    return true;
}

bool OpusStream::parseOpusTags()
{
    if (!readNextPacket())
        return false;

    // Check "OpusTags"
    if (m_packetSize < 8 || memcmp(m_packetBuffer, "OpusTags", 8) != 0)
        return true; // Not fatal

    return true;
}

bool OpusStream::readNextPacket()
{
    m_packetSize = 0;

    while (true)
    {
        // Do we need to read a new page?
        if (m_currentSegmentIdx >= m_currentPage.numSegments)
        {
            if (!readPageHeader(m_currentPage))
            {
                return false; // EOF
            }
            m_currentSegmentIdx = 0;
        }

        // We are on a valid page, let's get segments.
        while (m_currentSegmentIdx < m_currentPage.numSegments)
        {
            uint8_t segLen = m_currentPage.segmentTable[m_currentSegmentIdx++];

            if (m_packetSize + segLen > (int)sizeof(m_packetBuffer))
            {
                // Packet larger than our buffer. This is a problem.
                return false;
            }

            if (fread(m_packetBuffer + m_packetSize, 1, segLen, m_file) != (size_t)segLen)
            {
                return false; // File read error
            }

            m_packetSize += segLen;

            if (segLen < 255)
            {
                // Packet is complete!
                m_granulePos = m_currentPage.granulePosition;
                return true;
            }
        }

        // If we get here, the page ended, but the packet is not complete.
        // The loop will continue, read the next page, and continue appending segments.
    }
}

bool OpusStream::decodePacket()
{
    // Read next Ogg packet
    if (!readNextPacket())
        return false;

    // Decode Opus packet
    int framesPerChannel = opus_decode(
        m_decoder,
        m_packetBuffer,
        m_packetSize,
        m_pcmBuffer,
        5760, // Max frame size
        0     // No FEC
    );

    if (framesPerChannel < 0)
    {
        DLog("OpusStream: Decode error %d\n", framesPerChannel);
        return false;
    }

    m_pcmBufferSize = framesPerChannel * m_channels;
    m_pcmBufferPos = 0;
    m_samplesDecoded += framesPerChannel;

    return true;
}
