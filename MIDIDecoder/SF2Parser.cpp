#include "SF2Parser.hpp"
#include <string.h>


SF2Parser::SF2Parser() : m_sdtaOffset(0)
{
}
SF2Parser::~SF2Parser() { Clear(); }

uint32_t SF2Parser::Read32LE(FILE *f)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) < 4)
        return 0;
    return (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
}

uint16_t SF2Parser::Read16LE(FILE *f)
{
    uint8_t b[2];
    if (fread(b, 1, 2, f) < 2)
        return 0;
    return (uint16_t)(b[0] | (b[1] << 8));
}

void SF2Parser::Clear()
{
    for (std::vector<SFSampleHeader>::iterator it = m_samples.begin(); it != m_samples.end(); ++it)
    {
        if (it->data != NULL)
        {
            FreeVec(it->data);
            it->data = NULL;
        }
    }
    m_samples.clear();
    m_presets.clear();
    m_instruments.clear();
    m_pBags.clear();
    m_iBags.clear();
    m_pGens.clear();
    m_iGens.clear();
}

bool SF2Parser::Load(const std::string &path)
{
    m_path = path;
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return false;

    char id[4];
    fread(id, 1, 4, f);    // RIFF
    fseek(f, 8, SEEK_CUR); // Skip Size & sfbk

    while (!feof(f))
    {
        char chunkID[4];
        uint32_t size;
        if (fread(chunkID, 1, 4, f) < 4)
            break;
        fread(&size, 4, 1, f);
        size = SWAP32(size);
        long next = ftell(f) + size;

        if (strncmp(chunkID, "LIST", 4) == 0)
        {
            char type[4];
            fread(type, 1, 4, f);
            if (strncmp(type, "pdta", 4) == 0)
                ParsePdta(f, size - 4);
            else if (strncmp(type, "sdta", 4) == 0)
            {
                // Suche den "smpl" chunk im sdta LIST
                long subEnd = ftell(f) + size - 4;
                while (ftell(f) < subEnd)
                {
                    char subId[4];
                    fread(subId, 1, 4, f);
                    uint32_t subSize = Read32LE(f);
                    if (strncmp(subId, "smpl", 4) == 0)
                    {
                        m_sdtaOffset = ftell(f); // HIER fangen die echten Daten an
                        break;
                    }
                    fseek(f, subSize, SEEK_CUR);
                }
            }
        }
        fseek(f, next, SEEK_SET);
    }
    fclose(f);
    return true;
}

void SF2Parser::ParsePdta(FILE *f, uint32_t size)
{
    long endOfPdta = ftell(f) + size;
    while (ftell(f) < endOfPdta)
    {
        char id[4];
        uint32_t chunkSize;
        if (fread(id, 1, 4, f) < 4)
            break;
        fread(&chunkSize, 4, 1, f);
        chunkSize = SWAP32(chunkSize);
        long nextChunk = ftell(f) + chunkSize;

        if (strncmp(id, "phdr", 4) == 0)
        {
            int count = chunkSize / 38;
            for (int i = 0; i < count; i++)
            {
                SF2Preset p;
                fread(p.name, 1, 20, f);
                p.name[20] = '\0';
                fread(&p.preset, 2, 1, f);
                p.preset = SWAP16(p.preset);
                fread(&p.bank, 2, 1, f);
                p.bank = SWAP16(p.bank);
                fread(&p.bagIdx, 2, 1, f);
                p.bagIdx = SWAP16(p.bagIdx);
                fseek(f, 12, SEEK_CUR);
                m_presets.push_back(p);
            }
        }
        else if (strncmp(id, "pbag", 4) == 0)
        {
            int count = chunkSize / 4;
            for (int i = 0; i < count; i++)
            {
                SF2Bag b;
                fread(&b.genIdx, 2, 1, f);
                b.genIdx = SWAP16(b.genIdx);
                fseek(f, 2, SEEK_CUR);
                m_pBags.push_back(b);
            }
        }
        else if (strncmp(id, "pgen", 4) == 0)
        {
            int count = chunkSize / 4;
            for (int i = 0; i < count; i++)
            {
                SF2Generator g;
                fread(&g.genOper, 2, 1, f);
                g.genOper = SWAP16(g.genOper);
                fread(&g.genAmount, 2, 1, f);
                g.genAmount = SWAP16(g.genAmount);
                m_pGens.push_back(g);
            }
        }
        else if (strncmp(id, "inst", 4) == 0)
        {
            int count = chunkSize / 22;
            for (int i = 0; i < count; i++)
            {
                SF2Instrument ins;
                fread(ins.name, 1, 20, f);
                ins.name[20] = '\0';
                fread(&ins.bagIdx, 2, 1, f);
                ins.bagIdx = SWAP16(ins.bagIdx);
                m_instruments.push_back(ins);
            }
        }
        else if (strncmp(id, "ibag", 4) == 0)
        {
            int count = chunkSize / 4;
            for (int i = 0; i < count; i++)
            {
                SF2Bag b;
                fread(&b.genIdx, 2, 1, f);
                b.genIdx = SWAP16(b.genIdx);
                fseek(f, 2, SEEK_CUR);
                m_iBags.push_back(b);
            }
        }
        else if (strncmp(id, "igen", 4) == 0)
        {
            int count = chunkSize / 4;
            for (int i = 0; i < count; i++)
            {
                SF2Generator g;
                fread(&g.genOper, 2, 1, f);
                g.genOper = SWAP16(g.genOper);
                fread(&g.genAmount, 2, 1, f);
                g.genAmount = SWAP16(g.genAmount);
                m_iGens.push_back(g);
            }
        }
        else if (strncmp(id, "shdr", 4) == 0)
        {
            int count = chunkSize / 46;
            m_samples.reserve(count); // Speicher vorreservieren für Stabilität
            for (int i = 0; i < count; i++)
            {
                SFSampleHeader s;
                memset(&s, 0, sizeof(s));

                fread(s.name, 1, 20, f);

                s.start = Read32LE(f);
                s.end = Read32LE(f);
                s.startLoop = Read32LE(f);
                s.endLoop = Read32LE(f);
                s.sampleRate = Read32LE(f);

                s.originalPitch = (uint8_t)fgetc(f);
                s.pitchCorrection = (int8_t)fgetc(f);
                s.sampleLink = Read16LE(f);
                /*uint16_t sampleType =*/ Read16LE(f);
                s.data = NULL;
                m_samples.push_back(s);
            }
        }

        fseek(f, nextChunk, SEEK_SET);
    }
}

int16_t *SF2Parser::EnsureSampleLoaded(SFSampleHeader *shdr)
{
    if (shdr->data)
        return shdr->data;

    FILE *f = fopen(m_path.c_str(), "rb");
    if (!f)
        return NULL;

    if (shdr->end <= shdr->start)
    {
        fclose(f);
        return NULL;
    }
    // Berechnung der Länge
    uint32_t numSamples = shdr->end - shdr->start;
    uint32_t sizeBytes = numSamples * 2;
    if (numSamples > 0x7FFFFFFF / 2)  // Overflow-Check
    {
        fclose(f);
        return NULL;
    }

    shdr->data = (int16_t *)AllocVec(sizeBytes, MEMF_ANY);
    if (shdr->data)
    {
        // shdr->start ist der Offset in SAMPLES ab Beginn des 'smpl' sub-chunks
        fseek(f, m_sdtaOffset + (shdr->start * 2), SEEK_SET);
        size_t bytesRead = fread(shdr->data, 1, sizeBytes, f);

        if (bytesRead != sizeBytes)
        {
            FreeVec(shdr->data);
            shdr->data = NULL;
            fclose(f);
            return NULL;
        }

        // Byteswap für Amiga
        uint16_t *ptr = (uint16_t *)shdr->data;
        for (uint32_t i = 0; i < numSamples; i++)
        {
            uint16_t v = *ptr;
            *ptr++ = (v >> 8) | (v << 8);
        }
    }

    fclose(f);
    return shdr->data;
}

SampleMatch SF2Parser::GetSampleForNote(int bankNum, int presetNum, int midiNote, int velocity)
{
    SampleMatch match = {NULL, NULL, 60, false};

    int phdrIdx = -1;
    for (size_t i = 0; i != m_presets.size(); i++)
    {
        if (m_presets[i].bank == bankNum && m_presets[i].preset == presetNum)
        {
            phdrIdx = (int)i;
            break;
        }
    }
    if (phdrIdx == -1 || phdrIdx >= (int)m_presets.size() - 1)
        return match;

    uint16_t pStart = m_presets[phdrIdx].bagIdx;
    uint16_t pEnd = m_presets[phdrIdx + 1].bagIdx;

    int pLoK = 0, pHiK = 127, pLoV = 0, pHiV = 127;
    int presetAttenuation = 0;

    for (int b = pStart; b < pEnd && b < (int)m_pBags.size() - 1; b++)
    {
        int instID = -1;
        int loK = pLoK, hiK = pHiK, loV = pLoV, hiV = pHiV;
        int currentBagPresetAtten = 0;

        for (int g = m_pBags[b].genIdx; g < m_pBags[b + 1].genIdx; g++)
        {
            uint16_t op = m_pGens[g].genOper;
            uint16_t val = m_pGens[g].genAmount;
            if (op == 43)
            {
                loK = val & 0xFF;
                hiK = val >> 8;
            }
            if (op == 44)
            {
                loV = val & 0xFF;
                hiV = val >> 8;
            }
            if (op == 41)
                instID = val;
            if (op == 48)
                currentBagPresetAtten = (int16_t)val;
        }

        if (instID == -1 && b == pStart)
        {
            pLoK = loK;
            pHiK = hiK;
            pLoV = loV;
            pHiV = hiV;
            presetAttenuation = currentBagPresetAtten;
            continue;
        }

        if (midiNote >= loK && midiNote <= hiK && velocity >= loV && velocity <= hiV && instID != -1 && instID >= 0 && instID < (int)m_instruments.size() - 1)
        {
            int finalPresetAtten = presetAttenuation + currentBagPresetAtten;
            uint16_t iStart = m_instruments[instID].bagIdx;
            uint16_t iEnd = m_instruments[instID + 1].bagIdx;
            int iLoK = 0, iHiK = 127, iLoV = 0, iHiV = 127;
            int instAttenuation = 0;

            for (int ib = iStart; ib < iEnd && ib < (int)m_iBags.size() - 1; ib++)
            {
                int sampleIdx = -1;
                int loKi = iLoK, hiKi = iHiK, loVi = iLoV, hiVi = iHiV;
                int16_t rootOver = -1;
                int mode = 0;
                int currentBagInstAtten = 0;

                for (int ig = m_iBags[ib].genIdx; ig < m_iBags[ib + 1].genIdx; ig++)
                {
                    uint16_t op = m_iGens[ig].genOper;
                    uint16_t val = m_iGens[ig].genAmount;
                    if (op == 43)
                    {
                        loKi = val & 0xFF;
                        hiKi = val >> 8;
                    }
                    if (op == 44)
                    {
                        loVi = val & 0xFF;
                        hiVi = val >> 8;
                    }
                    if (op == 48) 
                        currentBagInstAtten = (int16_t)val;
                    if (op == 53)
                        sampleIdx = val;
                    if (op == 58)
                        rootOver = (int16_t)val;
                    if (op == 54)
                        mode = val;
                }

                if (sampleIdx == -1 && ib == iStart)
                {
                    iLoK = loKi;
                    iHiK = hiKi;
                    iLoV = loVi;
                    iHiV = hiVi;
                    instAttenuation = currentBagInstAtten;
                    continue;
                }

                if (midiNote >= loKi && midiNote <= hiKi && velocity >= loVi && velocity <= hiVi && sampleIdx != -1 && sampleIdx >= 0 && sampleIdx < (int)m_samples.size())
                {
                    match.left = &m_samples[sampleIdx];
                    match.rootKey = (rootOver != -1) ? rootOver : (int)match.left->originalPitch;
                    match.attenuation = finalPresetAtten + instAttenuation + currentBagInstAtten;
                    // mode 0: no loop
                    // mode 1: loop continuously
                    // mode 3: loop during note-on, then play to end
                    if (mode == 1 || mode == 3)
                        match.hasLoop = true;
                    else
                        match.hasLoop = false;
                    return match;
                }
            }
        }
    }
    return match;
}