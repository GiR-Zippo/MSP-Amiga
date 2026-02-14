#ifndef MIDIPARSER_HPP
#define MIDIPARSER_HPP

#include "../Common.h"

struct MidiEvent
{
    uint32_t deltaTicks;
    uint32_t originalDelta;
    uint8_t type;
    uint8_t channel;
    uint8_t data1;
    uint8_t data2;
};

struct MidiTrack
{
    std::vector<MidiEvent> events;
};

class MidiParser
{
public:
    MidiParser();
    ~MidiParser();
    bool Load(const char *path);

    uint16_t GetTicksPerQuarter() const { return m_ticksPerQuarter; }
    const std::vector<MidiTrack> &GetTracks() const { return m_tracks; }
    double CalculateDuration();
private:
    uint32_t ReadVLQ(FILE *f);
    uint16_t m_ticksPerQuarter;
    std::vector<MidiTrack> m_tracks;
};

#endif