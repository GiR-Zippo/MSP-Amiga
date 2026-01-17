#include "FlacStream.hpp"

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_WCHAR
extern "C"
{
#include "dr_flac.h"
}

FlacStream::FlacStream() : m_flac(NULL), m_initialized(false), m_duration(0) {}

FlacStream::~FlacStream()
{
    if (m_flac)
        drflac_close(m_flac);
}

bool FlacStream::open(const char *filename)
{
    readDuration(filename);
    m_flac = drflac_open_file(filename, NULL);
    if (!m_flac)
        return false;

    // mehr als 2? Nope
    if (m_flac->channels > 2)
        return false;

    m_initialized = true;
    return true;
}

int FlacStream::readSamples(short *buffer, int samplesToRead)
{
    if (!m_initialized || !m_flac)
        return 0;
    return (int)drflac_read_pcm_frames_s16(m_flac, samplesToRead / 2, buffer);
}

bool FlacStream::seek(uint32_t targetSeconds)
{
    if (m_duration * 1000 == 0)
        return false;
    uint64_t targetFrame = (uint64_t)targetSeconds * m_flac->sampleRate;
    return drflac_seek_to_pcm_frame(m_flac, targetFrame);
}

bool FlacStream::seekRelative(int32_t targetSeconds)
{
   uint32_t currentSec = getCurrentSeconds();
    uint32_t totalSec = m_duration;
    int32_t targetSec = (int32_t)currentSec + targetSeconds;
    
    if (targetSec < 0) targetSec = 0;
    if (targetSec > (int32_t)totalSec) targetSec = totalSec;

    uint64_t targetFrame = (uint64_t)targetSec * m_flac->sampleRate;
    return drflac_seek_to_pcm_frame(m_flac, targetFrame);
}

uint32_t FlacStream::getCurrentSeconds() const
{
    if (!m_flac || m_flac->sampleRate == 0) return 0;
    return (uint32_t)(m_flac->currentPCMFrame / m_flac->sampleRate);
}

uint32_t FlacStream::getSampleRate() const
{
    return m_flac ? m_flac->sampleRate : 0;
}

int FlacStream::getChannels() const
{
    return m_flac ? m_flac->channels : 0;
}

void FlacStream::readDuration(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
        return;

    uint8_t streaminfo[7];
    fseek(f, 18, SEEK_SET);
    fread(streaminfo, 1, 7, f);
    uint16_t sampleRate = ((uint8_t)streaminfo[0] << 12) | ((uint8_t)streaminfo[1] << 4) | ((uint8_t)streaminfo[2] >> 4);
    uint64_t totalSamples = ((uint64_t)(streaminfo[2] & 0x0F) << 32) | 
                        ((uint64_t)streaminfo[3] << 24) | 
                        ((uint64_t)streaminfo[4] << 16) | 
                        ((uint64_t)streaminfo[5] << 8)  | 
                        ((uint64_t)streaminfo[6]);
    if (sampleRate > 0)
        m_duration = ((uint64_t)((totalSamples) / sampleRate)/1000);
    fclose(f);
}