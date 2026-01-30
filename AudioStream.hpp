#ifndef AUDIO_STREAM_HPP
#define AUDIO_STREAM_HPP

#include "Common.h"

class AudioStream
{
    public:
        virtual ~AudioStream() {}
        virtual bool        open(const char* filename) = 0;
        virtual bool        seek(uint32_t targetSeconds) =0;
        virtual bool        seekRelative(int32_t targetSeconds) =0;
        virtual int         readSamples(short* buffer, int samplesToRead) = 0;
        virtual uint32_t    getCurrentSeconds() const = 0;
        virtual uint32_t    getDuration() const = 0;
        virtual uint32_t    getSampleRate() const = 0;
        virtual int         getChannels() const = 0;
        virtual const char* getTitle() const { return "Unknown Title"; }
        virtual const char* getArtist() const { return "Unknown Artist"; }
};

#endif