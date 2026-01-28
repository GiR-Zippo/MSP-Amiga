#include "M4AContainer.hpp"
#include <cstring>

uint32_t M4AReader::read32BE(FILE *f)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4)
        return 0;
    return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

std::string M4AReader::readStringData(FILE *f, uint32_t parentSize)
{
    // Wir sind jetzt am Anfang des Inhalts des Metadaten-Atoms (z.B. nach '©nam')
    uint32_t size = read32BE(f);
    char name[5] = {0};
    fread(name, 1, 4, f);

    if (strcmp(name, "data") == 0)
    {
        fseek(f, 8, SEEK_CUR); // Überspringe type(4) und locale(4)
        int32_t payloadLen = size - 16;

        if (payloadLen > 0)
        {
            char *buf = new char[payloadLen + 1];
            fread(buf, 1, payloadLen, f);
            buf[payloadLen] = '\0';
            std::string s(buf);
            delete[] buf;
            return s;
        }
    }
    return "";
}

void M4AReader::handleMetadataAtom(FILE *f, const char *name, uint32_t size, M4AMeta &meta)
{
    long nextAtomPos = ftell(f) + (size - 8);
    if (strcmp(name, "\xA9"
                     "nam") == 0)
        meta.title = readStringData(f, size);
    else if (strcmp(name, "\xA9"
                          "ART") == 0)
        meta.artist = readStringData(f, size);
    else if (strcmp(name, "\xA9"
                          "alb") == 0)
        meta.album = readStringData(f, size);
    else if (strcmp(name, "\xA9"
                          "day") == 0)
        meta.date = readStringData(f, size);
    else if (strcmp(name, "\xA9"
                          "gen") == 0)
        meta.genre = readStringData(f, size);
    else if (strcmp(name, "\xA9"
                          "cmt") == 0)
        meta.comment = readStringData(f, size);
    else if (strcmp(name, "\xA9"
                          "wrt") == 0)
        meta.composer = readStringData(f, size);
    else if (strcmp(name, "\xA9"
                          "too") == 0)
        meta.encoder = readStringData(f, size);
    else if (strcmp(name, "trkn") == 0)
    {
        // Spezialfall: Tracknummer ist oft binär in der 'data' Box
        // [Size][data][Type][Locale][0][Track][Total][0]
        fseek(f, 16 + 2, SEEK_CUR); // Skip data-header + 2 reservierte bytes
        uint8_t track = 0, total = 0;
        fread(&track, 1, 1, f);
        fseek(f, 1, SEEK_CUR);
        fread(&total, 1, 1, f);
        meta.trackNumber = track;
        meta.trackTotal = total;
    }
    else
        fseek(f, size - 8, SEEK_CUR);
    fseek(f, nextAtomPos, SEEK_SET);
}

void M4AReader::parseTrack(FILE *f, long trakEnd, M4AAudioTrackInfo &info)
{
    // Wir scannen die trak-Box nach mdia
    while (ftell(f) + 8 <= trakEnd)
    {
        long startPos = ftell(f);
        uint32_t size = read32BE(f);
        char name[5] = {0};
        fread(name, 1, 4, f);

        if (size == 0)
            break;

        if (strcmp(name, "hdlr") == 0)
        {
            fseek(f, 8, SEEK_CUR); // Skip version(1), flags(3), component_type(4)
            char handler[5] = {0};
            fread(handler, 1, 4, f);
            if (strcmp(handler, "soun") == 0)
                info.is_audio = true;
        }
        else if (strcmp(name, "mdhd") == 0)
        {
            fseek(f, 12, SEEK_CUR);
            uint32_t timeScale = read32BE(f);
            uint32_t duration = read32BE(f);
            info.duration = ((uint32_t)(((double)duration * 1000.0) / (double)timeScale) / 1000);
            fseek(f, 4, SEEK_CUR);
        }
        else if (strcmp(name, "stsd") == 0)
        {
            fseek(f, 8, SEEK_CUR); // Skip version/flags (4) und entry_count (4)

            // Hier beginnt der erste Sample-Eintrag (z.B. 'mp4a')
            /*uint32_t entrySize =*/ read32BE(f);
            char codecName[5] = {0};
            fread(codecName, 1, 4, f);
            //info.codec = codecName;
            strncpy(info.codec, codecName, 5);
            // In einem Audio-Eintrag (mp4a) liegen die Daten an festen Offsets:
            fseek(f, 16, SEEK_CUR); // Skip reserved(6), data_ref_idx(2), version(2), rev(2), vendor(4)

            uint16_t channels = 0;
            uint8_t chanBuf[2];
            fread(chanBuf, 1, 2, f);
            channels = (chanBuf[0] << 8) | chanBuf[1];
            info.channels = channels;

            fseek(f, 2, SEEK_CUR); // Skip sampleSize(2)
            fseek(f, 4, SEEK_CUR); // Skip pre-defined/reserved

            uint32_t sampleRate = read32BE(f);
            // Samplerate wird als 16.16 Fixed-Point gespeichert, wir brauchen nur den oberen Teil
            info.sampleRate = sampleRate >> 16;
        }
        // Wir müssen in mdia -> hdlr und mdia -> minf -> stbl -> stsd
        else if (strcmp(name, "mdia") == 0 || strcmp(name, "minf") == 0 || strcmp(name, "stbl") == 0)
        {
            // Rekursiv weiter in die Unterboxen
            parseTrack(f, startPos + size, info);
        }
        fseek(f, startPos + size, SEEK_SET);
    }
    return;
}

void M4AReader::parseBox(FILE *f, long endPos, M4AMeta &meta)
{
    while (ftell(f) + 8 <= endPos)
    {
        long startOfBox = ftell(f);
        uint32_t size = read32BE(f);
        char name[5] = {0};
        fread(name, 1, 4, f);

        if (size == 0)
            break;

        // Container-Hierarchie durchlaufen
        if (strcmp(name, "moov") == 0 || strcmp(name, "udta") == 0 ||
            strcmp(name, "ilst") == 0 || strcmp(name, "mdia") == 0 ||
            strcmp(name, "minf") == 0 || strcmp(name, "stbl") == 0)
        {
            parseBox(f, startOfBox + size, meta);
        }
        else if (strcmp(name, "trak") == 0)
        {
            // Wir haben einen Track gefunden!
            M4AAudioTrackInfo info;
            parseTrack(f, startOfBox + size, info);
            if (info.is_audio)
                meta.audioTracks.push_back(info);
            fseek(f, startOfBox + size, SEEK_SET);
        }
        else if (strcmp(name, "meta") == 0)
        {
            fseek(f, 4, SEEK_CUR); // Meta-Box hat nach dem Header 4 Bytes Flags
            parseBox(f, startOfBox + size, meta);
        }
        // Audio-Daten Block
        else if (strcmp(name, "mdat") == 0)
        {
            meta.audioOffset = (uint32_t)ftell(f);
            meta.audioSize = size - 8;
            fseek(f, startOfBox + size, SEEK_SET);
        }
        // Metadaten-Blätter (Inside 'ilst')
        else if (name[0] == '\xA9' || strcmp(name, "trkn") == 0 || strcmp(name, "cprt") == 0)
            handleMetadataAtom(f, name, size, meta);
        else
            fseek(f, startOfBox + size, SEEK_SET);
    }
}

bool M4AReader::parse(const char *filepath, M4AMeta &meta)
{
    FILE *f = fopen(filepath, "rb");
    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Jede MP4 Datei muss mit 'ftyp' starten
    read32BE(f);
    char firstName[5] = {0};
    fread(firstName, 1, 4, f);
    if (strcmp(firstName, "ftyp") != 0)
    {
        fclose(f);
        return false;
    }
    fseek(f, 0, SEEK_SET);

    parseBox(f, fileSize, meta);

    fclose(f);
    meta.valid = true;
    return true;
}