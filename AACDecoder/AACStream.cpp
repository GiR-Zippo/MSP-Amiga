#include "AACStream.hpp"
#include "../Shared/M4AContainer.hpp"
#include <string.h>

extern "C"
{
#define DR_AAC_IMPLEMENTATION
#include "../Shared/libfaad/faad.h"
}

AACStream::AACStream()
    : m_aac(NULL),
      m_sampleRate(0),
      m_channels(0),
      m_initialized(false),
      m_duration(0)
{
}

AACStream::~AACStream()
{
    if (m_aac)
    {
        dr_aac_close(m_aac);
    }
}

bool AACStream::open(const char *filename)
{
    printf("Building seektable please wait ...\n");
    m_aac = dr_aac_open_file(filename);
    if (!m_aac)
        return false;
    m_sampleRate = m_aac->samplerate;
    m_channels = m_aac->channels;
    m_duration = (m_aac->m4aframe.sampleCount * 1024) / m_sampleRate;
    m_initialized = true;
    return true;
}

bool AACStream::openm4a(const char *filename)
{
    m_aac = dr_aac_open_m4a(filename);
    if (!m_aac)
        return false;

    m_sampleRate = m_aac->samplerate;
    m_channels = m_aac->channels;
    m_initialized = true;
    return true;
}

bool AACStream::seek(uint32_t targetSeconds)
{
    dr_aac_seek_to(m_aac, (targetSeconds*1000));
    return true;
}

bool AACStream::seekRelative(int32_t targetSeconds)
{
    uint32_t currentSec = getCurrentSeconds();
    uint32_t totalSec = m_duration;
    int32_t targetSec = (int32_t)currentSec + targetSeconds;
    if (targetSec < 0) targetSec = 0;
    if (targetSec > (int32_t)totalSec) targetSec = totalSec;
    return seek(targetSec);
}

int AACStream::readSamples(short *targetBuffer, int samplesToRead)
{
    if (!m_initialized || !m_aac)
        return 0;
    m_sampleRate = m_aac->samplerate;
    m_channels = m_aac->channels;
    // Wir übergeben die vollen samplesToRead, den Rest macht der wrapper
    return dr_aac_read_s16(m_aac, samplesToRead, targetBuffer);
}

uint32_t AACStream::getCurrentSeconds() const
{
    if (m_sampleRate == 0) return 0;

    uint64_t totalSamplesPlayed = (uint64_t)m_aac->m4aframe.currentSampleIndex * 1024;
    return (uint32_t)(totalSamplesPlayed / m_sampleRate);
}