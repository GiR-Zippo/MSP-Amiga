#include "M4ADecoder.hpp"
#include "../Shared/M4AContainer.hpp"

M4AStream::M4AStream()
    : m_aac(NULL),
      m_sampleRate(0),
      m_channels(0),
      m_initialized(false),
      m_duration(0)
{
}

M4AStream::~M4AStream()
{
    if (m_aac)
        delete m_aac;
}

bool M4AStream::open(const char *filename)
{
    M4AMeta meta;
    if (!M4AReader::parse(filename, meta))
        return false;
    
    if (meta.audioTracks.size() > 0)
        m_duration = meta.audioTracks[0].duration;

    m_aac = new AACStream(); 
    if (!m_aac->openm4a(filename))
        return false;
    m_sampleRate = m_aac->getSampleRate();
    m_channels = m_aac->getChannels();
    m_initialized = true;
    return true;
}

int M4AStream::readSamples(short *targetBuffer, int samplesToRead)
{
    if (!m_initialized || !m_aac)
        return 0;
    m_sampleRate = m_aac->getSampleRate();
    m_channels = m_aac->getChannels();
    // Wir übergebendie vollen samplesToRead, den Rest macht der wrapper
    return m_aac->readSamples(targetBuffer, samplesToRead);
}

bool M4AStream::seekRelative(int32_t targetSeconds)
{
    uint32_t currentSec = getCurrentSeconds();
    uint32_t totalSec = m_duration;
    int32_t targetSec = (int32_t)currentSec + targetSeconds;
    if (targetSec < 0) targetSec = 0;
    if (targetSec > (int32_t)totalSec) targetSec = totalSec;
    return m_aac->seek(targetSec);
}

uint32_t M4AStream::getCurrentSeconds() const
{
    if (m_sampleRate == 0) return 0;

    uint64_t totalSamplesPlayed = (uint64_t)m_aac->getHandler()->m4aframe.currentSampleIndex * 1024;
    return (uint32_t)(totalSamplesPlayed / m_sampleRate);
}