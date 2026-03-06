#ifndef __OGGOPUSREADERWRITER_H__
#define __OGGOPUSREADERWRITER_H__

#include <cstdio>
#include <string>
#include <vector>
#include <stdint.h>

struct OggMeta
{
    std::string title;
    std::string artist;
    std::string album;
    std::string date;
    std::string tracknumber;
    std::string genre;
    std::string comment;

    // Technische Daten
    bool hasTag;
    uint32_t sampleRate;
    uint8_t channels;
    float duration; // in Sekunden
};

class OggOpusReaderWriter
{
    public:
        static OggMeta ReadMetaData(const char *filename);
        // Hinweis: Write ist bei Ogg destruktiv (erfordert Datei-Rewrite),
        // daher hier primär auf Performance für den 68k optimiert.
        static bool WriteMetaData(const char *filename, const OggMeta &meta);

    private:
        static uint32_t readLE32(const uint8_t *buf);
        static void writeLE32(uint8_t *buf, uint32_t val);
        static uint32_t update_crc(uint32_t crc, const uint8_t *data, int len);
        static void parseVorbisComments(FILE *f, uint32_t length, OggMeta &meta);
        static float calculate_duration(FILE* f);
};

#endif