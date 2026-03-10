#include "oggtag.hpp"
#include <cstring>
#include "../libtremor/ogg/crctable.h"

// Ogg CRC-Tabelle für den 68k (Polynomial 0x04c11db7)
uint32_t OggOpusReaderWriter::readLE32(const uint8_t *buf)
{
    return (uint32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
}

void OggOpusReaderWriter::writeLE32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

void OggOpusReaderWriter::parseVorbisComments(FILE *f, uint32_t length, OggMeta &meta)
{
    uint8_t buf[4];

    // Vendor String überspringen
    fread(buf, 1, 4, f);
    uint32_t vendorLen = readLE32(buf);
    fseek(f, vendorLen, SEEK_CUR);

    // Anzahl der User-Kommentare
    fread(buf, 1, 4, f);
    uint32_t commentCount = readLE32(buf);

    for (uint32_t i = 0; i < commentCount; ++i)
    {
        if (fread(buf, 1, 4, f) < 4)
            break;
        uint32_t len = readLE32(buf);

        // SANITY CHECK: Wenn ein String länger als 10 KB ist,
        // ist es entweder ein Cover-Bild oder Schrott.
        if (len > 10240 || len == 0)
        {
            fseek(f, len, SEEK_CUR); // Überspringen statt abstürzen
            continue;
        }

        // Nutze einen lokalen Puffer statt std::vector für kleine Strings
        char localBuf[1024];
        if (len < sizeof(localBuf))
        {
            fread(localBuf, 1, len, f);
            localBuf[len] = '\0';
            std::string comment = localBuf;

            size_t sep = comment.find('=');
            if (sep != std::string::npos)
            {
                std::string key = comment.substr(0, sep);
                std::string val = comment.substr(sep + 1);

                if (key == "TITLE")
                    meta.title = val;
                else if (key == "ARTIST")
                    meta.artist = val;
                else if (key == "ALBUM")
                    meta.album = val;
                else if (key == "DATE")
                    meta.date = val;
                else if (key == "GENRE")
                    meta.genre = val;
                else if (key == "TRACKNUMBER")
                    meta.tracknumber = val;
                else if (key == "COMMENT")
                    meta.comment = val;
            }
            else
                fseek(f, len, SEEK_CUR);
        }
    }
}
OggMeta OggOpusReaderWriter::ReadMetaData(const char *filename)
{
    OggMeta meta;
    meta.hasTag = false;
    FILE *f = fopen(filename, "rb");
    if (!f)
        return meta;

    meta.duration = calculate_duration(f);
    fseek(f, 0, SEEK_SET);

    uint8_t pageHeader[27];
    // Wir scannen die ersten paar Pages (Metadaten sind meist in Page 2)
    while (fread(pageHeader, 1, 27, f) == 27)
    {
        if (memcmp(pageHeader, "OggS", 4) != 0)
            break;

        uint8_t segments = pageHeader[26];
        uint8_t segmentTable[255];
        fread(segmentTable, 1, segments, f);

        uint32_t pageSize = 0;
        for (int i = 0; i < segments; i++)
            pageSize += segmentTable[i];

        // Header Check
        uint8_t sig[8];
        fread(sig, 1, 8, f);

        // Vorbis Signature: 0x03 + "vorbis" (7 bytes)
        // Opus Signature: "OpusTags" (8 bytes)
        if (sig[0] == 0x03 && memcmp(&sig[1], "vorbis", 6) == 0)
        {
            meta.hasTag = true;
            parseVorbisComments(f, pageSize, meta);
            break;
        }
        else if (memcmp(sig, "OpusTags", 8) == 0)
        {
            meta.hasTag = true;
            parseVorbisComments(f, pageSize, meta);
            break;
        }

        // Nicht die richtige Page? Weiter zur nächsten.
        fseek(f, pageSize - 8, SEEK_CUR);
    }

    fclose(f);
    return meta;
}

// CRC-Tabellen-Generierung für Ogg (Polynomial 0x04c11db7)
uint32_t OggOpusReaderWriter::update_crc(uint32_t crc, const uint8_t *data, int len)
{
    for (int i = 0; i < len; ++i)
    {
        crc = (crc << 8) ^ crc_lookup[0][((crc >> 24) ^ data[i]) & 0xff];
    }
    return crc;
}

bool OggOpusReaderWriter::WriteMetaData(const char *filename, const OggMeta &meta)
{
    std::string tempName = std::string(filename) + ".tmp";
    FILE *oldFile = fopen(filename, "rb");
    FILE *newFile = fopen(tempName.c_str(), "wb");
    if (!oldFile || !newFile)
        return false;

    uint8_t header[27];
    bool commentWritten = false;

    while (fread(header, 1, 27, oldFile) == 27)
    {
        uint8_t segments = header[26];
        std::vector<uint8_t> segmentTable(segments);
        fread(segmentTable.data(), 1, segments, oldFile);

        uint32_t pageSize = 0;
        for (uint8_t s : segmentTable)
            pageSize += s;

        uint8_t firstBytes[8];
        fread(firstBytes, 1, 8, oldFile);

        // Check ob Vorbis (0x03vorbis) oder Opus (OpusTags)
        bool isCommentPage = (firstBytes[0] == 0x03 && memcmp(&firstBytes[1], "vorbis", 6) == 0) ||
                             (memcmp(firstBytes, "OpusTags", 8) == 0);

        if (isCommentPage && !commentWritten)
        {
            // --- Neue Comment-Page bauen ---
            std::vector<uint8_t> newPacket;
            if (firstBytes[0] == 0x03)
            { // Vorbis
                newPacket.push_back(0x03);
                newPacket.insert(newPacket.end(), (uint8_t *)"vorbis", (uint8_t *)"vorbis" + 6);
            }
            else
            { // Opus
                newPacket.insert(newPacket.end(), (uint8_t *)"OpusTags", (uint8_t *)"OpusTags" + 8);
            }

            // Vendor (wir übernehmen einen Standard-String)
            uint8_t vLen[4];
            writeLE32(vLen, 7);
            newPacket.insert(newPacket.end(), vLen, vLen + 4);
            newPacket.insert(newPacket.end(), (uint8_t *)"Gemini", (uint8_t *)"Gemini" + 6);

            // Kommentare sammeln (TITLE, ARTIST, etc.)
            std::vector<std::string> tags;
            if (!meta.title.empty())
                tags.push_back("TITLE=" + meta.title);
            if (!meta.artist.empty())
                tags.push_back("ARTIST=" + meta.artist);
            // ... weitere Tags hier ...

            uint8_t cCount[4];
            writeLE32(cCount, tags.size());
            newPacket.insert(newPacket.end(), cCount, cCount + 4);

            for (const auto &t : tags)
            {
                uint8_t tLen[4];
                writeLE32(tLen, t.length());
                newPacket.insert(newPacket.end(), tLen, tLen + 4);
                newPacket.insert(newPacket.end(), t.begin(), t.end());
            }

            // Ogg-Header für die neue Page schreiben
            // (Achtung: Hier muss die CRC über die gesamte neue Page berechnet werden!)
            // ... CRC Berechnung und Header-Write Logik ...

            commentWritten = true;
            fseek(oldFile, pageSize - 8, SEEK_CUR); // Alte Page im Original überspringen
        }
        else
        {
            // Page einfach 1:1 kopieren
            fwrite(header, 1, 27, newFile);
            fwrite(segmentTable.data(), 1, segments, newFile);
            fwrite(firstBytes, 1, 8, newFile);

            std::vector<uint8_t> body(pageSize - 8);
            fread(body.data(), 1, pageSize - 8, oldFile);
            fwrite(body.data(), 1, pageSize - 8, newFile);
        }
    }

    fclose(oldFile);
    fclose(newFile);
    // Rename tmp to original
    return true;
}

float OggOpusReaderWriter::calculate_duration(FILE *f)
{
    if (!f)
        return 0.0f;

    // 1. Sample-Rate bestimmen
    fseek(f, 0, SEEK_SET);
    uint8_t idHeader[64];
    fread(idHeader, 1, 64, f);

    uint32_t sampleRate = 0;
    // Vorbis: "vorbis" startet bei Byte 29 (nach Ogg-Header + Packettitle)
    // Wir suchen einfach nach dem String im ersten Puffer
    for (int i = 0; i < 50; i++)
    {
        if (memcmp(&idHeader[i], "vorbis", 6) == 0)
        {
            sampleRate = readLE32(&idHeader[i + 11]); // Sample Rate steht 11 Bytes nach "vorbis"
            break;
        }
        if (memcmp(&idHeader[i], "OpusHead", 8) == 0)
        {
            sampleRate = 48000; // Immer 48k bei Opus
            break;
        }
    }

    if (sampleRate == 0)
        return 0.0f;

    // Suche nach der LETZTEN Page mit gültiger Granule Position
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);

    // Wir scannen die letzten 8KB
    long searchRange = (fileSize > 8192) ? 8192 : fileSize;
    fseek(f, fileSize - searchRange, SEEK_SET);

    std::vector<uint8_t> buf(searchRange);
    fread(buf.data(), 1, searchRange, f);

    long long lastGranulePos = -1;
    for (int i = (int)searchRange - 27; i >= 0; i--)
    {
        if (buf[i] == 'O' && buf[i + 1] == 'g' && buf[i + 2] == 'g' && buf[i + 3] == 'S')
        {
            // Granule Position: 8 Bytes ab Offset 6
            uint8_t *gp = &buf[i + 6];

            // Wir bauen den 64-Bit Wert händisch zusammen (Little Endian -> Big Endian)
            // Auch wenn wir nur 32-Bit brauchen, lesen wir sicherheitshalber korrekt aus:
            uint32_t low = (uint32_t)(gp[0] | (gp[1] << 8) | (gp[2] << 16) | (gp[3] << 24));
            uint32_t high = (uint32_t)(gp[4] | (gp[5] << 8) | (gp[6] << 16) | (gp[7] << 24));

            // Wenn GranulePos -1 (alle Bits gesetzt), ist diese Page nicht das Ende eines Streams
            if (low == 0xFFFFFFFF && high == 0xFFFFFFFF)
                continue;

            lastGranulePos = low; // Reicht für die meisten Songs aus
            if (high > 0)
            {
                // Falls der Song extrem lang ist (über 24h)
                lastGranulePos = ((long long)high << 32) | low;
            }
            break;
        }
    }

    if (lastGranulePos > 0)
        return (float)lastGranulePos / (float)sampleRate;
    return 0.0f;
}