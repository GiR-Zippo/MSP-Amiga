#include "VorbisStream.hpp"
#include "../Shared/oggtag/oggtag.hpp"

VorbisStream::VorbisStream()
    : m_file(NULL), m_initialized(false), m_channels(2),
      m_sampleRate(44100), m_duration(0), m_currentSection(0),
      m_pcmBufferSize(0), m_pcmBufferPos(0)
{
    memset(&m_vf, 0, sizeof(m_vf));
    setTitle("Unknown Title");
    setArtist("Unknown Artist");
}

VorbisStream::~VorbisStream()
{
    close();
}

bool VorbisStream::open(const char *filename)
{
    if (!filename)
        return false;

    m_file = fopen(filename, "rb");
    if (!m_file)
        return false;

    OggMeta meta = OggOpusReaderWriter::ReadMetaData(filename);
    m_duration = meta.duration;
    if (!meta.title.empty())
        setTitle(meta.title.c_str());

    if (!meta.artist.empty())
        setArtist(meta.artist.c_str());

    if (ov_open(m_file, &m_vf, NULL, 0) < 0)
    {
        fclose(m_file);
        m_file = NULL;
        return false;
    }

    vorbis_info *vi = ov_info(&m_vf, -1);
    if (!vi)
    {
        ov_clear(&m_vf);
        m_file = NULL;
        return false;
    }

    m_channels   = vi->channels;
    m_sampleRate = vi->rate;

    // Duration in Sekunden
    ogg_int64_t total = ov_pcm_total(&m_vf, -1);
    if (total > 0)
        m_duration = (uint32_t)(total / m_sampleRate);

    m_pcmBufferSize = 0;
    m_pcmBufferPos  = 0;
    m_initialized   = true;

    return true;
}

void VorbisStream::close()
{
    if (m_initialized)
    {
        ov_clear(&m_vf);   // Schließt auch m_file intern
        m_file        = NULL;
        m_initialized = false;
    }
    else if (m_file)
    {
        fclose(m_file);
        m_file = NULL;
    }
}

bool VorbisStream::seek(uint32_t targetSeconds)
{
    if (!m_initialized)
        return false;

    ogg_int64_t pcmPos = (ogg_int64_t)targetSeconds * m_sampleRate;
    if (ov_pcm_seek(&m_vf, pcmPos) != 0)
        return false;

    // PCM-Buffer nach Seek leeren
    m_pcmBufferSize = 0;
    m_pcmBufferPos  = 0;
    return true;
}

int VorbisStream::readSamples(short *buffer, int samplesToRead)
{
    if (!m_initialized || !buffer)
        return 0;

    int samplesRead = 0;

    while (samplesRead < samplesToRead)
    {
        // Erst internen Buffer leeren
        if (m_pcmBufferPos < m_pcmBufferSize)
        {
            int available = m_pcmBufferSize - m_pcmBufferPos;
            int toCopy    = samplesToRead - samplesRead;
            if (toCopy > available) toCopy = available;

            memcpy(buffer + samplesRead, m_pcmBuffer + m_pcmBufferPos, toCopy * sizeof(short));

            m_pcmBufferPos += toCopy;
            samplesRead    += toCopy;
            continue;
        }

        // Buffer leer - nächsten Block dekodieren
        // libtremor ov_read gibt signed 16-bit PCM direkt zurück
        // Kein bigendianp Parameter in tremor - gibt immer native Endian aus
        long ret = ov_read(&m_vf, (char *)m_pcmBuffer, sizeof(m_pcmBuffer), &m_currentSection);

        if (ret == 0)
            break;

        if (ret < 0)
            continue;

        m_pcmBufferSize = (int)(ret / sizeof(short));
        m_pcmBufferPos  = 0;
    }
    return samplesRead / m_channels;
}

uint32_t VorbisStream::getCurrentSeconds() const
{
    if (!m_initialized)
        return 0;

    ogg_int64_t pos = ov_pcm_tell(&m_vf);
    if (pos < 0) return 0;
    return (uint32_t)(pos / m_sampleRate);
}
