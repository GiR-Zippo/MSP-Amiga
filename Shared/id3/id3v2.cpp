#include "id3v2.hpp"

ID3Meta ID3V2ReaderWriter::ReadID3MetaData(const char *filename)
{
    ID3Meta meta;
    meta.hasTag = false;
    FILE *f = fopen(filename, "rb");
    if (!f)
        return meta;

    unsigned char h[10];
    if (fread(h, 1, 10, f) == 10 && h[0] == 'I' && h[1] == 'D' && h[2] == '3')
    {
        meta.hasTag = true;
        meta.version = h[3];
        uint32_t size = (h[6] << 21) | (h[7] << 14) | (h[8] << 7) | h[9];
        meta.tagSize = size + 10;
        meta.audioOffset = meta.tagSize;

        uint32_t read = 0;
        while (read < size)
        {
            char id[5] = {0};
            if (fread(id, 1, 4, f) < 4 || id[0] == 0)
                break;

            unsigned char s[4];
            fread(s, 1, 4, f);
            uint32_t fSize = (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];

            fseek(f, 2, SEEK_CUR); // Flags
            read += 10;

            if (fSize > 0 && fSize <= (size - read))
            {
                std::vector<char> buf(fSize + 1);
                fread(&buf[0], 1, fSize, f);
                buf[fSize] = '\0';
                const char *txt = &buf[1]; // Skip Encoding Byte

                std::string key(id);
                if (key == "TIT2")
                    meta.title = txt;
                else if (key == "TPE1")
                    meta.artist = txt;
                else if (key == "TALB")
                    meta.album = txt;
                else if (key == "TYER")
                    meta.year = txt;
                else if (key == "TRCK")
                    meta.track = txt;
                else if (key == "TCON")
                    meta.genre = txt;
                else if (key == "TCOM")
                    meta.composer = txt;
                else if (key == "TSSE")
                    meta.encoder = txt;
                else if (key == "TCOP")
                    meta.copyright = txt;
                else if (key == "WXXX")
                    meta.url = txt;
                else if (key == "COMM" && fSize > 4)
                    meta.comment = &buf[4];

                read += fSize;
            }
            else
                break;
        }
    }
    else
        meta.audioOffset = 0;

    meta.duration = calculate_duration(f, meta.tagSize);

    fclose(f);
    return meta;
}

bool ID3V2ReaderWriter::WriteID3MetaData(const char *filename, const ID3Meta &meta)
{
    // Gesamte Tag-Größe berechnen (ohne den 10-Byte-Hauptheader)
    uint32_t totalFramesSize = 0;
    totalFramesSize += calculateFrameSize(meta.title);
    totalFramesSize += calculateFrameSize(meta.artist);
    totalFramesSize += calculateFrameSize(meta.album);
    totalFramesSize += calculateFrameSize(meta.year);
    totalFramesSize += calculateFrameSize(meta.track);
    totalFramesSize += calculateFrameSize(meta.genre);
    totalFramesSize += calculateFrameSize(meta.composer);
    totalFramesSize += calculateFrameSize(meta.encoder);
    totalFramesSize += calculateFrameSize(meta.copyright);
    totalFramesSize += calculateFrameSize(meta.url);

    // Sonderbehandlung für COMM (da er 3 zusätzliche Bytes für die Sprache hat)
    if (!meta.comment.empty())
        totalFramesSize += (10 + 1 + 3 + (uint32_t)meta.comment.length());

    // Prüfen: Passt es in den alten Tag?
    if (meta.hasTag && totalFramesSize <= meta.tagSize)
    {
        FILE *f = fopen(filename, "rb+");
        if (!f)
            return false;

        writeTags(f, meta, totalFramesSize);

        // Den Rest des alten Tags mit Nullen füllen (Padding)
        long currentPos = ftell(f);
        long paddingNeeded = (long)meta.tagSize - currentPos;
        if (paddingNeeded > 0)
        {
            std::vector<char> zeroPadding(paddingNeeded, 0);
            fwrite(&zeroPadding[0], 1, paddingNeeded, f);
        }

        fclose(f);
        return true;
    }
    else
    {
        FILE *fIn = fopen(filename, "rb");
        if (!fIn)
            return false;

        std::string tempName = std::string(filename) + ".tmp";
        FILE *fOut = fopen(tempName.c_str(), "wb");
        if (!fOut)
        {
            fclose(fIn);
            return false;
        }

        // Tags schreiben
        writeTags(fOut, meta, totalFramesSize);

        // Die eigentlichen Audiodaten anhängen
        // Wir springen in der Quelldatei über den alten Tag hinweg
        fseek(fIn, meta.audioOffset, SEEK_SET);

        char buffer[8192];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), fIn)) > 0)
            fwrite(buffer, 1, bytesRead, fOut);

        fclose(fIn);
        fclose(fOut);

        // Alte Datei durch neue ersetzen
        remove(filename);
        rename(tempName.c_str(), filename);
    }
    return true;
}

/*********************************************************/
/***                  Helper Functions                 ***/
/*********************************************************/
float ID3V2ReaderWriter::calculate_duration(FILE* file, long tagSize) {
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    fseek(file, tagSize, SEEK_SET);

    // Ersten MP3-Frame suchen (Sync-Word 0xFF + 0xE0)
    unsigned char frame[4];
    int bitrate = 0;

    while (fread(frame, 1, 4, file) == 4) {
        if (frame[0] == 0xFF && (frame[1] & 0xE0) == 0xE0) {
            // Layer III, Version 1 Bitrate Tabelle
            int brTable[] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
            bitrate = brTable[(frame[2] >> 4) & 0x0F];
            break;
        }
        // Wenn kein Sync gefunden, ein Byte zurück und weitersuchen
        fseek(file, -3, SEEK_CUR);
    }

    //zurück an den Anfang
    fseek(file, 0, SEEK_SET);

    if (bitrate > 0) {
        long audioSize = fileSize - tagSize;
        // Dauer = (Bytes * 8) / (kbps * 1000)
        return (audioSize * 8.0f) / (bitrate * 1000.0f);
    }

    return 0;
}

void ID3V2ReaderWriter::writeTags(FILE *f, const ID3Meta &meta, int totalFramesSize)
{
    // ID3v2 Haupt-Header schreiben (10 Bytes)
    fwrite("ID3", 1, 3, f);
    fputc(3, f); // Version 2.3
    fputc(0, f); // Revision 0
    fputc(0, f); // Flags
    writeSyncsafe(f, totalFramesSize);

    // Frames schreiben
    writeTextFrame(f, "TIT2", meta.title);
    writeTextFrame(f, "TPE1", meta.artist);
    writeTextFrame(f, "TALB", meta.album);
    writeTextFrame(f, "TYER", meta.year);
    writeTextFrame(f, "TRCK", meta.track);
    writeTextFrame(f, "TCON", meta.genre);
    writeTextFrame(f, "TCOM", meta.composer);
    writeTextFrame(f, "TSSE", meta.encoder);
    writeTextFrame(f, "TCOP", meta.copyright);
    writeTextFrame(f, "WXXX", meta.url);

    // Kommentar-Frame (spezielles Format)
    if (!meta.comment.empty())
    {
        fwrite("COMM", 1, 4, f);
        writeInt32BE(f, (uint32_t)meta.comment.length() + 4);
        unsigned char cFlags[2] = {0, 0};
        fwrite(cFlags, 1, 2, f);
        fputc(0, f);            // Encoding
        fwrite("eng", 1, 3, f); // Sprache (fest auf Englisch)
        fwrite(meta.comment.c_str(), 1, meta.comment.length(), f);
    }
}

// Hilfsfunktion: Berechnet Frame-Header (10) + Encoding-Byte (1) + Textlänge
uint32_t ID3V2ReaderWriter::calculateFrameSize(const std::string &s)
{
    if (s.empty())
        return 0;
    return 10 + 1 + (uint32_t)s.length();
}

void ID3V2ReaderWriter::writeTextFrame(FILE *f, const char *id, const std::string &val)
{
    if (val.empty())
        return;

    fwrite(id, 1, 4, f);                         // 4 Bytes ID
    writeInt32BE(f, (uint32_t)val.length() + 1); // Größe (Text + Encoding-Byte)

    unsigned char flags[2] = {0, 0};
    fwrite(flags, 1, 2, f); // 2 Bytes Flags

    fputc(0, f);                             // 1 Byte Encoding (0 = Latin1)
    fwrite(val.c_str(), 1, val.length(), f); // Der Text
}

void ID3V2ReaderWriter::writeFrame(FILE *f, const char *id, const std::string &val)
{
    if (val.empty())
        return;
    fwrite(id, 1, 4, f);
    writeInt32BE(f, val.length() + 1);
    unsigned char flags[2] = {0, 0};
    fwrite(flags, 1, 2, f);
    fputc(0, f); // Encoding: Latin1
    fwrite(val.c_str(), 1, val.length(), f);
}

// Schreibt 32-Bit Big Endian (für Frame-Größen)
void ID3V2ReaderWriter::writeInt32BE(FILE *f, uint32_t val)
{
    unsigned char b[4];
    b[0] = (val >> 24) & 0xFF;
    b[1] = (val >> 16) & 0xFF;
    b[2] = (val >> 8) & 0xFF;
    b[3] = val & 0xFF;
    fwrite(b, 1, 4, f);
}

// Schreibt 28-Bit Syncsafe (für den ID3-Hauptheader)
void ID3V2ReaderWriter::writeSyncsafe(FILE *f, uint32_t val)
{
    unsigned char b[4];
    b[0] = (val >> 21) & 0x7F;
    b[1] = (val >> 14) & 0x7F;
    b[2] = (val >> 7) & 0x7F;
    b[3] = val & 0x7F;
    fwrite(b, 1, 4, f);
}