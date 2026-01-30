#ifndef M4A_STREAM_HPP
#define M4A_STREAM_HPP

#include "AudioStream.hpp"
#include "../AACDecoder/AACStream.hpp"

/*********************************************************/
/***                  Nur ein Wrapper                  ***/
/*********************************************************/
class M4AStream : public AudioStream 
{
    public:
        M4AStream();
        ~M4AStream();

        bool     open(const char* filename);
        bool     seek(uint32_t targetSeconds) { return m_aac->seek(targetSeconds); }
        bool     seekRelative(int32_t targetSeconds);
        int      readSamples(short* buffer, int samplesToRead);
        uint32_t getCurrentSeconds() const;
        uint32_t getDuration() const { return m_duration; }
        uint32_t getSampleRate() const { return m_sampleRate; }
        int      getChannels() const { return m_channels; }
        const char* getTitle() const { return m_title; }
        const char* getArtist() const { return m_artist; }

    private:
        AACStream*     m_aac;

        uint32_t       m_sampleRate;
        unsigned char  m_channels;
        bool           m_initialized;
        uint32_t       m_duration;
        char           m_title[128];
        char           m_artist[128];
};

#endif