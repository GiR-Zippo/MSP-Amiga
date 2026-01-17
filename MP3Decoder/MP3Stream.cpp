#include "MP3Stream.hpp"
#include "../Shared/id3/id3v2.hpp"

extern "C"
{
#define DR_MP3_IMPLEMENTATION
#ifdef OLD_GCC 
    #define DR_MP3_NO_WCHAR
#endif
#include "dr_mp3.h"
}

MP3Stream::MP3Stream() : m_initialized(false) {}

MP3Stream::~MP3Stream()
{
    if (m_initialized)
        drmp3_uninit(&m_mp3);
}

bool MP3Stream::open(const char *filename)
{
    ID3Meta meta = ID3V2ReaderWriter::ReadID3MetaData(filename);
    m_duration = (uint32_t)meta.duration;
    if (m_initialized)
    {
        drmp3_uninit(&m_mp3);
        m_initialized = false;
    }

    if (!drmp3_init_file(&m_mp3, filename, NULL))
        return false;

    m_initialized = true;
    return true;
}

int MP3Stream::readSamples(short *buffer, int samplesToRead)
{
    if (!m_initialized)
        return 0;

    uint64_t framesRead = drmp3_read_pcm_frames_s16(&m_mp3, samplesToRead / 2, buffer);
    return framesRead;
}

bool MP3Stream::seek(uint32_t targetSeconds)
{
    if (m_duration == 0)
        return false;

    uint64_t targetFrame = (uint64_t)targetSeconds * m_mp3.sampleRate;
    bool result = drmp3_seek_to_pcm_frame(&m_mp3, targetFrame);
    return result;
}

bool MP3Stream::seekRelative(int32_t targetSeconds) 
{
    uint32_t currentSec = getCurrentSeconds();
    uint32_t totalSec = m_duration;
    int32_t targetSec = (int32_t)currentSec + targetSeconds;
    
    if (targetSec < 0) targetSec = 0;
    if (targetSec > (int32_t)totalSec) targetSec = totalSec;

    uint64_t targetFrame = (uint64_t)targetSec * m_mp3.sampleRate;
    return drmp3_seek_to_pcm_frame(&m_mp3, targetFrame);
}

uint32_t MP3Stream::getCurrentSeconds() const
{
    if (m_mp3.sampleRate == 0)
        return 0;

    // Einfache 32-Bit Division (reicht für Songs bis ~13 Stunden bei 44.1kHz)
    return (uint32_t)(m_mp3.currentPCMFrame / m_mp3.sampleRate);
}

uint32_t MP3Stream::getSampleRate() const { return m_mp3.sampleRate; }
int MP3Stream::getChannels() const { return m_mp3.channels; }