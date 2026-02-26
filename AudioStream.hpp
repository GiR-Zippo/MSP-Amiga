#ifndef AUDIO_STREAM_HPP
#define AUDIO_STREAM_HPP

#include "Common.h"

class AudioStream
{
    public:
        AudioStream();
        virtual ~AudioStream() {}
        virtual bool        open(const char* filename) = 0;
        virtual bool        seek(uint32_t targetSeconds) =0;
        virtual bool        seekRelative(int32_t targetSeconds) =0;
        virtual int         readSamples(short* buffer, int samplesToRead) = 0;
        virtual uint32_t    getCurrentSeconds() const = 0;
        virtual uint32_t    getDuration() const = 0;
        virtual uint32_t    getSampleRate() const = 0;
        virtual int         getChannels() const = 0;
        virtual const char* getStation(char* dest, int maxLen) const;
        virtual const char* getTitle(char* dest, int maxLen) const;
        virtual const char* getArtist(char* dest, int maxLen) const;

        virtual void        setStation(const char* s);
        virtual void        setTitle(const char* s);
        virtual void        setArtist(const char* s);

    protected:
        mutable struct SignalSemaphore m_semaStation;
        mutable struct SignalSemaphore m_semaTitle;
        mutable struct SignalSemaphore m_semaArtist;
        char m_station[128];
        char m_title[128];
        char m_artist[128];
};

#endif