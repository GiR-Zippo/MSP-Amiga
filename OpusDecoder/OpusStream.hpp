/* OpusStream.hpp - Opus Audio Stream für Player
 * Dekodiert Ogg/Opus Dateien zu PCM
 */

#ifndef OPUS_STREAM_HPP
#define OPUS_STREAM_HPP

#include "AudioStream.hpp"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Forward declarations (aus libopus)
typedef struct OpusDecoder OpusDecoder;

class OpusStream : public AudioStream
{
private:
    FILE *m_file;
    OpusDecoder *m_decoder;
    
    // Metadata
    char m_title[256];
    char m_artist[256];
    
    // Ogg State
    uint32_t m_serialNo;
    uint64_t m_granulePos;
    uint32_t m_pageSequence;
    
    // Stream Info
    int m_channels;
    int m_preskip;
    uint64_t m_samplesDecoded;
    uint64_t m_totalSamples;
    
    // Buffering
    uint8_t m_packetBuffer[65536];
    int m_packetSize;
    
    // Pre-decoded PCM buffer (für smooth playback)
    int16_t m_pcmBuffer[5760 * 2];  // Max Opus frame * stereo
    int m_pcmBufferSize;
    int m_pcmBufferPos;
    
    bool m_initialized;
    bool m_headerParsed;
    uint8_t m_currentSegmentIdx;
    uint32_t m_duration;
    
public:
    OpusStream();
    ~OpusStream();
    
    // AudioStream Interface
    bool     open(const char *filename);
    void     close();
    bool     seek(uint32_t targetSeconds);
    bool     seekRelative(int32_t targetSeconds) {return true;}
    int      readSamples(short *buffer, int samplesToRead);
    uint32_t getCurrentSeconds() const;
    uint32_t getDuration() const { return m_duration; }
    uint32_t getSampleRate() const { return 48000; }  // Opus ist immer 48kHz
    int      getChannels() const { return 2;}
    
private:
    // Ogg parsing
    struct PageHeader
    {
        uint8_t version;
        uint8_t headerType;
        uint64_t granulePosition;
        uint32_t serialNumber;
        uint32_t pageSequenceNumber;
        uint32_t checksum;
        uint8_t numSegments;
        uint8_t segmentTable[255];
    };
    
    PageHeader m_currentPage;

    bool readPageHeader(PageHeader &header);
    bool readPageData(const PageHeader &header);
    bool parseOpusHeader();
    bool parseOpusTags();
    bool readNextPacket();
    bool decodePacket();
    bool calculateTotalTime(const char *filename);
    // Helper
    void extractMetadata(const uint8_t *data, int size);
};

#endif /* OPUS_STREAM_HPP */
