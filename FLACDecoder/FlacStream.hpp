#ifndef FLAC_STREAM_HPP
#define FLAC_STREAM_HPP

#include "AudioStream.hpp"

#define DR_FLAC_NO_WCHAR
extern "C" {
    #include "dr_flac.h"
}

class FlacStream : public AudioStream
{
    public:
        FlacStream();
        ~FlacStream();

    // Implementierung des AudioStream Interfaces
    bool     open(const char* filename);
    int      readSamples(short* buffer, int samplesToRead);
    bool     seek(uint32_t targetSeconds);
    bool     seekRelative(int32_t targetSeconds);
    uint32_t getCurrentSeconds() const;
    uint32_t getDuration() const { return m_duration; }
    uint32_t getSampleRate() const;
    int      getChannels()   const;

private:
    void     readDuration(const char* filename);
    drflac*  m_flac;
    bool     m_initialized;
    uint32_t m_duration;
};

#endif