#ifndef VORBIS_STREAM_HPP
#define VORBIS_STREAM_HPP

#include <exec/types.h>
#include <vector>
#include "../AudioStream.hpp"

struct stb_vorbis;
class VorbisStream : public AudioStream
{
    public:
        VorbisStream();
        ~VorbisStream();

        bool     open(const char* filename);
        bool     seek(uint32_t targetSeconds);
        bool     seekRelative(int32_t targetSeconds);
        int      readSamples(short* buffer, int samplesToRead);
        uint32_t getCurrentSeconds() const;
        uint32_t getDuration()   const { return m_duration; }
        uint32_t getSampleRate() const { return m_sampleRate; }
        int      getChannels()   const { return m_channels; }
        bool     isValid()       const { return m_v != NULL; }

    private:
        bool readDuration(const char *filename);
        struct stb_vorbis* m_v;
        int      m_sampleRate;
        int      m_channels;
        uint32_t m_duration;
};

#endif