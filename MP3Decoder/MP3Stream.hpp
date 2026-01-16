#ifndef MP3_STREAM_HPP
#define MP3_STREAM_HPP

#include "AudioStream.hpp"

#ifdef OLD_GCC 
    #define DR_MP3_NO_WCHAR
#endif
extern "C"
{
#include "dr_mp3.h"
}

class MP3Stream : public AudioStream
{
    public:
        MP3Stream();
        ~MP3Stream();

        bool     open(const char *filename);
        bool     seek(uint32_t targetSeconds);
        bool     seekRelative(int32_t targetSeconds);
        int      readSamples(short *buffer, int samplesToRead);
        uint32_t getCurrentSeconds() const;
        uint32_t getDuration() const { return m_duration; }
        uint32_t getSampleRate() const;
        int      getChannels() const;

    private:
        drmp3    m_mp3;
        bool     m_initialized;
        uint32_t m_duration;
};

#endif