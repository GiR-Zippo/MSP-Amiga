#include "VorbisStream.hpp"
#include <cstdio>

// WICHTIG für Amiga (Big Endian)
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

VorbisStream::VorbisStream() 
    : m_v(NULL), m_sampleRate(0), m_channels(0), m_duration(0)
{
}

VorbisStream::~VorbisStream()
{
    if (m_v)
        stb_vorbis_close(m_v);
}

bool VorbisStream::open(const char* filename)
{
    readDuration(filename);

    int error;
    m_v = stb_vorbis_open_filename((char*)filename, &error, NULL);
    
    if (!m_v) {
        printf("stb_vorbis: Fehler beim Öffnen von '%s'. Code: %d\n", filename, error);
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

bool VorbisStream::readDuration(const char *filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("Datei konnte nicht geöffnet werden\n");
        return false;
    }

    uint8_t headBuf[4];
    uint32_t sampleRate = 0;

    // Der Vorbis-Ident-Header startet nach dem Ogg-Header (28 Bytes)
    fseek(f, 0x28, SEEK_SET); 
    fread(headBuf, 1, 4, f);
    sampleRate = (uint32_t)headBuf[0] | 
                 ((uint32_t)headBuf[1] << 8) | 
                 ((uint32_t)headBuf[2] << 16) | 
                 ((uint32_t)headBuf[3] << 24);

    if (sampleRate == 0)
        return false;

    // Letzte Ogg-Page am Ende der Datei suchen
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    long currentEnd = fileSize;
    const int CHUNK_SIZE = 4096; // Wir lesen in 4KB Schritten rückwärts
    uint8_t buffer[CHUNK_SIZE];

    while (currentEnd > 0) 
    {
        long readStart = currentEnd - CHUNK_SIZE;
        if (readStart < 0) readStart = 0;
        int toRead = currentEnd - readStart;

        fseek(f, readStart, SEEK_SET);
        if (fread(buffer, 1, toRead, f) != (size_t)toRead) break;

        // Suche innerhalb des aktuellen Buffers von hinten nach vorne
        for (int i = toRead - 4; i >= 0; i--) {
            if (buffer[i] == 'O' && buffer[i+1] == 'g' && buffer[i+2] == 'g' && buffer[i+3] == 'S') {
                uint8_t* p = &buffer[i + 6];
                uint64_t granule = (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) | 
                                   ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) | 
                                   ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);

                if (granule != 0 && granule != 0xFFFFFFFFFFFFFFFFULL) {
                    // GEFUNDEN!
                    double durationSec = (double)granule / (double)sampleRate;
                    m_duration = (uint32_t)durationSec;
                    return true;
                }
            }
        }

        // Wenn in diesem Block nichts war, gehen wir einen Block weiter vor
        currentEnd = readStart;
        
        // Sicherheitsstopp: Wir scannen nicht die ganze Datei (Metadaten sind selten > 1MB)
        if (fileSize - currentEnd > 1024 * 1024) break; 
    }
    return false;
}