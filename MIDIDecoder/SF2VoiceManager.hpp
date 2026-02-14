#ifndef SF2VOICEMANAGER_HPP
#define SF2VOICEMANAGER_HPP

#include "SF2Parser.hpp"

struct Voice
{
    bool active;
    SFSampleHeader *sample;
    uint32_t pos;  // 16.16 Fixed Point
    uint32_t posHigh;
    uint32_t step; // 16.16 Fixed Point
    uint32_t baseStep; // 16.16 Fixed Point
    float lastPitchBend;
    uint8_t lastMod;
    float lGain, rGain;
    int midiNote;
    int rootKey;
    bool hasLoop;       // Vorberechnet beim NoteOn
    uint32_t start;     //  4 Bytes: Start im sdta-Chunk
    uint32_t sampleEnd; //  4 Bytes: Ende
    uint32_t loopStart; //  4 Bytes: Loop Start
    uint32_t loopEnd;   //  4 Bytes: Loop Ende
    uint32_t loopLen;   //  4 Bytes: Loop Laenge
    bool inRelease;
    float releaseLevel;
    uint8_t channel;
    uint32_t age;
    bool pendingRelease;
};

class SF2VoiceManager
{
public:
    SF2VoiceManager(int maxVoices = 32);
    ~SF2VoiceManager();
    void NoteOn(const SampleMatch &match, int note, float vol, uint8_t pan, int chan, float currentBend);
    void NoteOff(int midiNote, int chan, bool sustainActive);
    int FindOldestVoice();
    void ApplyLimiter(int32_t* temp, int16_t* outBuffer, uint32_t frames);
    //void ApplyModulation();
    void Mix(short *outBuffer, uint32_t numFrames);
    void ReleaseSustainedNotes(uint8_t channel);
    void UpdateChannelPitch(uint8_t chan, float bend);
    void UpdateChannelModulation(uint8_t chan, uint8_t mod);

private:
    int m_maxvoices;
    uint32_t m_globalAgeCounter;
    float m_masterGain;
    std::vector<Voice> m_voices;
    float m_pitchTable[256];
    float m_bendTable[1024];
    float m_lfoTable[256];
    float m_attenuationTable[1001];
    uint8_t m_lfoPos;
};
#endif