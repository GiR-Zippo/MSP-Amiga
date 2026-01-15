#ifndef DR_AAC_H
#define DR_AAC_H

#include <stdio.h>
#include <stdlib.h>
#include "neaacdec.h"

#ifdef __cplusplus
extern "C"
{
#endif
    /* Struktur zur Verwaltung des AM4A-Frames */
    typedef struct
    {
        unsigned long firstFrameOffset;
        unsigned long *sampleSizes;
        unsigned long sampleCount;
        unsigned char asc[2]; // AudioSpecificConfig (2 Bytes)
        unsigned long currentSampleIndex;
    } M4AFrame;

    /* Struktur zur Verwaltung des AAC-Streams */
    typedef struct
    {
        NeAACDecHandle handle;
        FILE *file;
        unsigned char *bitstreamBuffer;
        unsigned long samplerate;
        unsigned char channels;

        /* Interner Buffer für das aktuelle Frame */
        void *currentFramePtr;
        unsigned long samplesRemaining;

        /* Wenn wir m4a nutzen, brauchen wir das hier */
        bool is_m4a;
        M4AFrame m4aframe;
    } dr_aac;

    /* API Funktionen */
    dr_aac *dr_aac_open_file(const char *filename);
    dr_aac *dr_aac_open_m4a(const char *filename);
    bool dr_aac_seek_to(dr_aac *pAac, unsigned long targetMs);
    size_t dr_aac_read_s16(dr_aac *pAac, size_t samplesToRead, short *pOutput);
    void dr_aac_close(dr_aac *pAac);

    unsigned long read32BE(FILE *f);
    bool parseM4A(const char *filename, M4AFrame *stream);

#ifdef __cplusplus
}
#endif
#endif /* DR_AAC_H */

/* -------------------------------------------------------------------------- */
/* Implementations                                                            */
/* -------------------------------------------------------------------------- */
//#define DEBUG
#ifdef DR_AAC_IMPLEMENTATION
#define AAC_READ_SIZE 4096

unsigned long read32BE(FILE *f)
{
    unsigned char b[4];
    if (fread(b, 1, 4, f) < 4)
        return 0;
    return (unsigned long)((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]);
}

bool parseM4A(const char *filename, M4AFrame *stream)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
        return false;

    stream->sampleSizes = NULL;
    stream->sampleCount = 0;
    stream->firstFrameOffset = 0;
    bool foundASC = false;

#ifdef DEBUG
    printf("Starte Box-Scan...\n");
#endif
    while (!feof(f))
    {
        long boxStart = ftell(f);
        unsigned long size = read32BE(f);
        unsigned long type = read32BE(f);

        if (size == 0)
            break;

#ifdef DEBUG
        // Erzeuge einen String aus dem Typ (für printf)
        char typeStr[5] = {(char)((type >> 24) & 0xff), (char)((type >> 16) & 0xff),
                           (char)((type >> 8) & 0xff), (char)(type & 0xff), 0};
        printf("Box: %s, Groesse: %u, Offset: %ld\n", typeStr, size, boxStart);
#endif

        // CONTAINER: Hier tauchen wir ein
        if (type == 0x6d6f6f76 || type == 0x7472616b || type == 0x6d646961 ||
            type == 0x6d696e66 || type == 0x7374626c)
        {
            // Wir bleiben im Header und lesen die nächste Box innerhalb
            continue;
        }

        // SPEZIAL-CONTAINER (mit Headern zum Überspringen)
        if (type == 0x73747364)
        {
            fseek(f, 8, SEEK_CUR);
            continue;
        }
        if (type == 0x6d703461)
        {
            fseek(f, 28, SEEK_CUR);
            continue;
        }

        // DATEN-BOXEN
        if (type == 0x7374737a)
        {
            fseek(f, 4, SEEK_CUR);
            unsigned long defSize = read32BE(f);
            stream->sampleCount = read32BE(f);
#ifdef DEBUG
            printf("  -> Gefunden: %u Samples\n", stream->sampleCount);
#endif
            stream->sampleSizes = (unsigned long *)malloc(stream->sampleCount * sizeof(unsigned long));
            for (unsigned long i = 0; i < stream->sampleCount; i++)
            {
                stream->sampleSizes[i] = (defSize > 0) ? defSize : read32BE(f);
            }
        }
        else if (type == 0x7374636f || type == 0x636f3634)
        { // 'stco' oder 'co64'
            fseek(f, 4, SEEK_CUR);
            unsigned long count = read32BE(f);
            if (count > 0)
            {
                if (type == 0x7374636f)
                    stream->firstFrameOffset = read32BE(f);
                else
                {                                           // co64 (8-byte offsets)
                    read32BE(f);                            // High 32-bit (meist 0)
                    stream->firstFrameOffset = read32BE(f); // Low 32-bit
                }
#ifdef DEBUG
                printf("  -> Audio Start Offset: %u\n", stream->firstFrameOffset);
#endif
            }
        }
        else if (type == 0x65736473)
        { // 'esds'
            fseek(f, 4, SEEK_CUR);
            unsigned char buf[64];
            fread(buf, 1, 64, f);
            for (int i = 0; i < 60; i++)
            {
                if (buf[i] == 0x05)
                { // DecSpecificInfoTag
                    int pos = i + 1;
                    while (buf[pos] & 0x80)
                        pos++; // Skip length
                    pos++;
                    stream->asc[0] = buf[pos];
                    stream->asc[1] = buf[pos + 1];
                    foundASC = true;
#ifdef DEBUG
                    printf("  -> ASC gefunden: %02x %02x\n", stream->asc[0], stream->asc[1]);
#endif
                    break;
                }
            }
        }

        // WICHTIG: Überspringe die Box, um zur nächsten zu kommen
        fseek(f, boxStart + size, SEEK_SET);

        // Abbruch-Bedingung
        if (foundASC && stream->sampleCount > 0 && stream->firstFrameOffset > 0)
        {
            fclose(f);
            return true;
        }
    }
#ifdef DEBUG
    printf("Fehler: Header unvollstaendig (ASC:%d, Count:%d, Offset:%u)\n",
           foundASC, stream->sampleCount, stream->firstFrameOffset);
#endif
    fclose(f);
    return false;
}

bool buildSeekTable(FILE* f, M4AFrame *stream)
{
    fseek(f, 0, SEEK_END);
    unsigned long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Grobe Schätzung der Frame-Anzahl (ca. 25-30 ms pro Frame)
    // Ein 4-Minuten-Song hat ca. 10.000 Frames.
    unsigned long maxFrames = fileSize / 200; // Konservative Schätzung
    stream->sampleSizes = (unsigned long*)malloc(maxFrames * sizeof(unsigned long));
    stream->sampleCount = 0;

    unsigned char header[7];
    unsigned long currentPos = 0;

    while (currentPos < fileSize - 7)
    {
        fseek(f, currentPos, SEEK_SET);
        fread(header, 1, 7, f);

        // Sync-Wort prüfen (0xFFF)
        if (header[0] == 0xFF && (header[1] & 0xF0) == 0xF0)
        {
            if (stream->sampleCount == 0) stream->firstFrameOffset = currentPos;

            // Frame-Länge aus ADTS-Header extrahieren
            // Die Länge steht in den Bits 30 bis 42 des Headers
            unsigned long frameLen = ((header[3] & 0x03) << 11) | 
                                     (header[4] << 3) | 
                                     ((header[5] & 0xE0) >> 5);

            if (frameLen < 7) break; // Fehlerhafter Header
            stream->sampleSizes[stream->sampleCount++] = frameLen;
            currentPos += frameLen;
        }
        else
            currentPos++;
    }
    fseek(f, 0, SEEK_SET);
    return true;
}

dr_aac *dr_aac_open_file(const char *filename)
{
    unsigned long sr;
    unsigned char ch;
    size_t bytesRead;

    FILE *f = fopen(filename, "rb");
    if (!f)
        return NULL;

    dr_aac *pAac = (dr_aac *)calloc(1, sizeof(dr_aac));
    if (!pAac)
    {
        fclose(f);
        return NULL;
    }

    if (!buildSeekTable(f, &pAac->m4aframe))
        return NULL;

    pAac->is_m4a = false;
    pAac->file = f;
    pAac->handle = NeAACDecOpen();
    pAac->bitstreamBuffer = (unsigned char *)malloc(AAC_READ_SIZE);

    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(pAac->handle);
    config->outputFormat = FAAD_FMT_16BIT; // Wir arbeiten mit 16Bit
    config->downMatrix = 1;                // und das soll so bleiben
    // Wichtig kein FIXED_POINT
    NeAACDecSetConfiguration(pAac->handle, config);

    /* Initialen Header lesen für FAAD Init */
    bytesRead = fread(pAac->bitstreamBuffer, 1, AAC_READ_SIZE, f);

    /* FAAD initialisieren */
    if (NeAACDecInit(pAac->handle, pAac->bitstreamBuffer, bytesRead, &sr, &ch) < 0)
    {
        dr_aac_close(pAac);
        return NULL;
    }

    pAac->samplerate = sr;
    pAac->channels = ch;

    /* Datei-Pointer zurücksetzen, damit Decode von vorne beginnt */
    fseek(f, 0, SEEK_SET);
    return pAac;
}

dr_aac *dr_aac_open_m4a(const char *filename)
{
    unsigned long sr;
    unsigned char ch;

    dr_aac *pAac = (dr_aac *)calloc(1, sizeof(dr_aac));
    if (!pAac)
        return NULL;

    // check header
    if (!parseM4A(filename, &pAac->m4aframe))
        return NULL;

    pAac->is_m4a = true;
    pAac->handle = NeAACDecOpen();
    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(pAac->handle);
    config->outputFormat = FAAD_FMT_16BIT; // Wir arbeiten mit 16Bit
    config->downMatrix = 1;                // und das soll so bleiben
    NeAACDecSetConfiguration(pAac->handle, config);

    if (NeAACDecInit2(pAac->handle, pAac->m4aframe.asc, 2, &sr, &ch) < 0)
        return NULL;

    pAac->samplerate = sr;
    pAac->channels = ch;

    FILE *f = fopen(filename, "rb");
    fseek(f, pAac->m4aframe.firstFrameOffset, SEEK_SET);
    pAac->file = f;
    return pAac;
}

bool dr_aac_seek_to(dr_aac *pAac, unsigned long targetMs)
{
    if (!pAac->m4aframe.sampleSizes || pAac->m4aframe.sampleCount == 0) 
        return false;

    // Ziel-Frame-Index berechnen
    // Ein AAC-Frame entspricht meist 1024 Samples.
    // Zeit pro Frame (ms) = 1024 * 1000 / sampleRate
    double msPerFrame = (1024.0 * 1000.0) / (double)pAac->samplerate;
    unsigned long targetFrameIndex = (unsigned long)(targetMs / msPerFrame);

    if (targetFrameIndex >= pAac->m4aframe.sampleCount) 
        targetFrameIndex = pAac->m4aframe.sampleCount - 1;

    // Byte-Offset berechnen
    // Wir summieren alle Frame-Größen bis zum Ziel-Frame
    unsigned long byteOffset = pAac->m4aframe.firstFrameOffset;
    for (unsigned long i = 0; i < targetFrameIndex; i++)
        byteOffset += pAac->m4aframe.sampleSizes[i];

    // Hardware/Lib Seek
    fseek(pAac->file, byteOffset, SEEK_SET);
    pAac->m4aframe.currentSampleIndex = targetFrameIndex;
    return true;
}

size_t dr_aac_read_s16(dr_aac *pAac, size_t samplesToRead, short *pOutput)
{
    if(!pAac->handle)
        return 0;
    size_t totalGrabbed = 0;
    static unsigned char bitstream[AAC_READ_SIZE * 2]; // Lokaler Cache
    static size_t bytesInCache = 0;

    while (totalGrabbed < samplesToRead)
    {
        /* Vorhandene Samples ausgeben */
        if (pAac->samplesRemaining > 0)
        {
            size_t canCopy = (pAac->samplesRemaining < (samplesToRead - totalGrabbed))
                                 ? pAac->samplesRemaining
                                 : (samplesToRead - totalGrabbed);

            short *src = (short *)pAac->currentFramePtr;

            for (size_t i = 0; i < canCopy; i++)
                pOutput[totalGrabbed + i] = src[i];

            // Pointer-Arithmetik:
            // Da src ein short* ist, rückt (src + canCopy) den Pointer um
            // canCopy * 2 Bytes nach vorne.
            pAac->currentFramePtr = (void *)(src + canCopy);
            pAac->samplesRemaining -= canCopy;
            totalGrabbed += canCopy;
        }
        else
        {
            //if (pAac->is_m4a)
            {
                unsigned long nextFrameSize = pAac->m4aframe.sampleSizes[pAac->m4aframe.currentSampleIndex++];
                if (bytesInCache < nextFrameSize)
                {
                    bytesInCache = 0;
                    size_t read = fread(bitstream, 1, nextFrameSize, pAac->file);
                    bytesInCache = read; // wir arbeiten mit frames, also bytesInCache immer was gelesen wurde
                    if (read < nextFrameSize)
                        break;
                }
            }
            /*else
            {
                /* Cache auffüllen, falls zu wenig Daten da sind 
                if (bytesInCache < AAC_READ_SIZE)
                {
                    size_t read = fread(bitstream + bytesInCache, 1, AAC_READ_SIZE, pAac->file);
                    if (read == 0 && bytesInCache == 0)
                        break; // EOF
                    bytesInCache += read;
                }
            }*/
            NeAACDecFrameInfo info;
            void *pDecoded = NeAACDecDecode(pAac->handle, &info, bitstream, bytesInCache);

            if (pDecoded && info.error == 0 && info.samples > 0)
            {
                pAac->currentFramePtr = pDecoded;
                pAac->samplesRemaining = info.samples;

                /* WICHTIG: Verbrauchte Bytes aus dem Cache schieben */
                bytesInCache -= info.bytesconsumed;
                memmove(bitstream, bitstream + info.bytesconsumed, bytesInCache);
            }
            else
            {
#ifdef DEBUG
                const char *errorMsg = NeAACDecGetErrorMessage(info.error);
                printf("FAAD Error #%u %s\n", (int)info.error, errorMsg);
#endif
                /* Wenn Müll gefunden wurde: 1 Byte wegwerfen und weitersuchen */
                if (bytesInCache > 0)
                {
                    bytesInCache--;
                    memmove(bitstream, bitstream + 1, bytesInCache);
                }
                else
                {
                    break;
                }
            }
        }
    }
    return totalGrabbed / 2; // Das muss so!
}

void dr_aac_close(dr_aac *pAac)
{
    if (!pAac)
        return;
    if (pAac->handle)
        NeAACDecClose(pAac->handle);
    if (pAac->file)
        fclose(pAac->file);
    if (pAac->m4aframe.sampleSizes)
    {
        free(pAac->m4aframe.sampleSizes);
        pAac->m4aframe.sampleSizes = NULL;
    }
    if (pAac->bitstreamBuffer)
        free(pAac->bitstreamBuffer);
    free(pAac);
}

#endif /* DR_AAC_IMPLEMENTATION */