#include "AudioStream.hpp"

AudioStream::AudioStream()
{
    InitSemaphore(&m_semaStation);
    InitSemaphore(&m_semaTitle);
    InitSemaphore(&m_semaArtist);
    m_station[0] = m_title[0] = m_artist[0] = '\0';
}

const char* AudioStream::getStation(char* dest, int maxLen) const {
    ObtainSemaphoreShared(&m_semaStation);
    strncpy(dest, m_station, maxLen - 1);
    dest[maxLen - 1] = '\0';
    ReleaseSemaphore(&m_semaStation);
    return dest;
}

const char *AudioStream::getTitle(char* dest, int maxLen) const
{
    ObtainSemaphoreShared(&m_semaTitle);
    strncpy(dest, m_title, maxLen - 1);
    dest[maxLen - 1] = '\0';
    ReleaseSemaphore(&m_semaTitle);
    return dest;
}

const char *AudioStream::getArtist(char* dest, int maxLen) const
{
    ObtainSemaphoreShared(&m_semaArtist);
    strncpy(dest, m_artist, maxLen - 1);
    dest[maxLen - 1] = '\0';
    ReleaseSemaphore(&m_semaArtist);
    return dest;
}

void AudioStream::setStation(const char *s)
{
    if (!s)
        return;
    Forbid();
    strncpy(m_station, s, 127);
    m_station[127] = '\0';
    Permit();
}

void AudioStream::setTitle(const char *t)
{
    if (!t)
        return;
    Forbid();
    strncpy(m_title, t, 127);
    m_title[127] = '\0';
    Permit();
}

void AudioStream::setArtist(const char *a)
{
    if (!a)
        return;
    Forbid();
    strncpy(m_artist, a, 127);
    m_artist[127] = '\0';
    Permit();
}