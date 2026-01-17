#ifndef AAC_STREAM_HPP
#define AAC_STREAM_HPP

#include "AudioStream.hpp"

extern "C" {
#include "../Shared/libfaad/faad.h"
}

class AACStream : public AudioStream 
{
    public:
        AACStream();
        ~AACStream();

        bool     open(const char* filename);
        bool     openm4a(const char* filename);
        bool     seek(uint32_t targetSeconds);
        bool     seekRelative(int32_t targetSeconds);
        int      readSamples(short* buffer, int samplesToRead);
        uint32_t getCurrentSeconds() const;
        uint32_t getDuration() const { return m_duration; }
        uint32_t getSampleRate() const { return m_sampleRate; }
        int      getChannels() const { return m_channels; }

        dr_aac*  getHandler() { return m_aac; }

    private:
        dr_aac*        m_aac;

        uint32_t       m_sampleRate;
        unsigned char  m_channels;
        bool           m_initialized;
        uint32_t       m_duration;
};

#endif