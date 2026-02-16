#ifndef SF2PARSER_HPP
#define SF2PARSER_HPP

#include "Common.h"

struct SFSampleHeader
{
    char name[20];          // 20 Bytes: Name des Samples
    uint32_t start;         //  4 Bytes: Start im sdta-Chunk
    uint32_t end;           //  4 Bytes: Ende
    uint32_t startLoop;     //  4 Bytes: Loop Start
    uint32_t endLoop;       //  4 Bytes: Loop Ende
    uint32_t sampleRate;    //  4 Bytes: z.B. 44100
    uint8_t originalPitch;  //  1 Byte : MIDI Note (60 = C4)
    int8_t pitchCorrection; //  1 Byte : Feintuning in Cents
    uint16_t sampleLink;    //  2 Bytes: Index des Partner-Samples (Stereo)
    uint16_t type;          //  2 Bytes: Mono, Left, Right oder Linked

    // --- AB HIER: Nicht im File, nur für unseren Amiga-Manager ---
    int16_t *data; //  4 Bytes: Pointer ins Fast-RAM
};

struct SF2Generator
{
    uint16_t genOper;
    uint16_t genAmount; // Kann signed oder unsigned sein
};

struct SF2Bag
{
    uint16_t genIdx;
};

struct SF2Preset
{
    char name[21];
    uint16_t preset;
    uint16_t bank;
    uint16_t bagIdx;
};

struct SF2Instrument
{
    char name[21];
    uint16_t bagIdx;
};

struct SampleMatch
{
    SFSampleHeader *left;
    SFSampleHeader *right; // nullptr, wenn Mono
    int rootKey;
    int attenuation;
    bool hasLoop;
    int fineTune;
};

class SF2Parser
{
    public:
        SF2Parser();
        ~SF2Parser();

        /// @brief Load the SF2 header and metadata
        bool Load(const std::string &path);

        /// @brief Clear cached samples, header and meta
        void Clear();

        ///@brief Get the right sample for the MIDI-Note
        SampleMatch GetSampleForNote(int bankNum, int presetNum, int midiNote, int velocity);

        ///@brief Load the sample from the SF2 if needed
        int16_t *EnsureSampleLoaded(SFSampleHeader *shdr);

    private:
        /// @brief parse meta block
        void ParsePdta(FILE *f, uint32_t size);
        
        // Endian-Korrektur
        uint32_t Read32LE(FILE *f);
        uint16_t Read16LE(FILE *f);
        uint16_t SWAP16(uint16_t v) { return (v >> 8) | (v << 8); }
        uint32_t SWAP32(uint32_t v)
        {
            return ((v >> 24) & 0xff) | ((v << 8) & 0xff0000) | ((v >> 8) & 0xff00) | ((v << 24) & 0xff000000);
        }

        std::string m_path;
        uint32_t m_sdtaOffset;

        std::vector<SFSampleHeader> m_samples;
        std::vector<SF2Preset> m_presets;
        std::vector<SF2Instrument> m_instruments;
        std::vector<SF2Bag> m_pBags, m_iBags;
        std::vector<SF2Generator> m_pGens, m_iGens;
};
#endif