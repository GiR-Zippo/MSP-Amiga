#ifndef VORBIS_STREAM_HPP
#define VORBIS_STREAM_HPP

#include "../AudioStream.hpp"
#include "../Common.h"

extern "C"
{
#include "../Shared/libtremor/ivorbisfile.h"
}

class VorbisStream : public AudioStream
{
private:
    FILE           *m_file;
    OggVorbis_File  m_vf;
    bool            m_initialized;
    int             m_channels;
    long            m_sampleRate;
    uint32_t        m_duration;
    int             m_currentSection;

    // Pre-decoded PCM buffer (libtremor gibt direkt int16 raus)
    int16_t         m_pcmBuffer[4096 * 2];  // 4096 samples * stereo
    int             m_pcmBufferSize;        // Anzahl shorts im Buffer
    int             m_pcmBufferPos;         // Aktuelle Leseposition

public:
    VorbisStream();
    ~VorbisStream();

    bool        open(const char *filename);
    void        close();
    bool        seek(uint32_t targetSeconds);
    bool        seekRelative(int32_t targetSeconds) { return true; }
    int         readSamples(short *buffer, int samplesToRead);
    uint32_t    getCurrentSeconds() const;
    uint32_t    getDuration() const   { return m_duration; }
    uint32_t    getSampleRate() const { return (uint32_t)m_sampleRate; }
    int         getChannels() const   { return m_channels; }
};

#endif /* VORBIS_STREAM_HPP */
