#include "VorbisStream.hpp"
#include "../Shared/oggtag/oggtag.hpp"

// WICHTIG für Amiga (Big Endian)
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

VorbisStream::VorbisStream() 
    : m_v(NULL), m_sampleRate(0), m_channels(0), m_duration(0)
{
    setTitle("Unknown Title");
    setArtist("Unknown Artist");
}

VorbisStream::~VorbisStream()
{
    if (m_v)
        stb_vorbis_close(m_v);
}

bool VorbisStream::open(const char* filename)
{
    OggMeta meta = OggOpusReaderWriter::ReadMetaData(filename);
    m_duration = meta.duration;
    if (!meta.title.empty())
        setTitle(meta.title.c_str());

    if (!meta.artist.empty())
        setArtist(meta.artist.c_str());

    int error;
    m_v = stb_vorbis_open_filename((char*)filename, &error, NULL);
    
    if (!m_v) {
        DLog("stb_vorbis: Fehler beim Öffnen von '%s'. Code: %d\n", filename, error);
        return false;
    }
    
    stb_vorbis_info info = stb_vorbis_get_info(m_v);
    m_sampleRate = info.sample_rate;
    m_channels   = info.channels;
    return true;
}

int VorbisStream::readSamples(short* buffer, int samplesToRead) {
    if (!m_v) return 0;

    // Wir fordern immer 2 Kanäle an (Stereo-Interleaved) für AHI
    // stb_vorbis übernimmt das Up/Downmixing automatisch
    return stb_vorbis_get_samples_short_interleaved(m_v, 2, buffer, samplesToRead);
}

uint32_t VorbisStream::getCurrentSeconds() const
{
    if (!m_v) return 0;

    stb_vorbis_info info = stb_vorbis_get_info(m_v);
    if (info.sample_rate == 0) return 0;
    return (uint32_t)(stb_vorbis_get_sample_offset(m_v) / info.sample_rate);
}

bool VorbisStream::seek(uint32_t targetSeconds)
{
    if (!m_v) return false;
    
    stb_vorbis_info info = stb_vorbis_get_info(m_v);
    uint32_t totalSec = m_duration;
    
    if (targetSeconds < 0) targetSeconds = 0;
    if (targetSeconds > totalSec) targetSeconds = totalSec;
    uint32_t targetSample = targetSeconds * info.sample_rate;
    
    return (bool)stb_vorbis_seek(m_v, targetSample);
}

bool VorbisStream::seekRelative(int32_t targetSeconds)
{
    if (!m_v) return false;
    
    stb_vorbis_info info = stb_vorbis_get_info(m_v);
    uint32_t currentSec = getCurrentSeconds();
    uint32_t totalSec = m_duration;
    int32_t targetSec = (int32_t)currentSec + targetSeconds;
    
    if (targetSec < 0) targetSec = 0;
    if (targetSec > (int32_t)totalSec) targetSec = totalSec;
    uint32_t targetSample = (uint32_t)targetSec * info.sample_rate;
    
    return (bool)stb_vorbis_seek(m_v, targetSample);
}
