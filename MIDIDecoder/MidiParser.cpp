#include "MidiParser.hpp"

MidiParser::MidiParser() : m_ticksPerQuarter(480) {}

MidiParser::~MidiParser()
{
    for (std::vector<MidiTrack>::iterator it = m_tracks.begin(); it != m_tracks.end(); it++)
        it->events.clear();
    m_tracks.clear();
}

uint32_t MidiParser::ReadVLQ(FILE *f)
{
    uint32_t value = 0;
    uint8_t byte;
    do
    {
        byte = fgetc(f);
        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80); // Solange das MSB 1 ist, folgen weitere Bytes
    return value;
}

// Hilfsfunktionen zum sicheren Lesen von Big-Endian (MIDI Standard)
uint16_t read16be(FILE *f)
{
    uint8_t b1 = fgetc(f);
    uint8_t b2 = fgetc(f);
    return (b1 << 8) | b2;
}

uint32_t read32be(FILE *f)
{
    uint8_t b1 = fgetc(f);
    uint8_t b2 = fgetc(f);
    uint8_t b3 = fgetc(f);
    uint8_t b4 = fgetc(f);
    return (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
}

bool MidiParser::Load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

    char id[4];
    fread(id, 1, 4, f);
    if (strncmp(id, "MThd", 4) != 0)
    {
        fclose(f);
        return false;
    }

    uint32_t hSize = read32be(f);
    uint16_t format = read16be(f);
    uint16_t numTracks = read16be(f);
    uint16_t division = read16be(f);

    m_ticksPerQuarter = division;

    if (hSize > 6)
        fseek(f, hSize - 6, SEEK_CUR);

    for (int t = 0; t < numTracks; t++)
    {
        if (fread(id, 1, 4, f) < 4 || strncmp(id, "MTrk", 4) != 0)
            break;

        uint32_t tSize = read32be(f);
        long trackEnd = ftell(f) + tSize;

        MidiTrack track;
        uint8_t lastStatus = 0;

        while (ftell(f) < trackEnd)
        {
            uint32_t delta = ReadVLQ(f);
            uint8_t status = fgetc(f);

            if (!(status & 0x80))
            {
                ungetc(status, f);
                status = lastStatus;
            }
            else if (status < 0xF0)
            {
                lastStatus = status;
            }

            if (status == 0xFF)
            { // META EVENT
                uint8_t metaType = fgetc(f);
                uint32_t metaLen = ReadVLQ(f);

                switch (metaType)
                {
                    case 0x02: // Copyright
                    case 0x03: // Sequence/Track Name
                    {
                        char buffer[128];
                        uint32_t i;
                        for (i = 0; i < metaLen; i++)
                        {
                            uint8_t c = fgetc(f);
                            if (i < 127) buffer[i] = c; // Buffer-Overflow Schutz
                        }
                        buffer[i > 127 ? 127 : i] = '\0'; // Null-Terminator setzen
                        
                        if (metaType == 0x03) printf("Titel: %s\n", buffer);
                        if (metaType == 0x02) printf("Copyright: %s\n", buffer);
                        break;
                    }
                    //MIDI Set Tempo meta message
                    case 0x51:
                    {
                        if (metaLen != 3)
                            break;
                        // TEMPO EVENT ABFANGEN
                        uint8_t t1 = fgetc(f);
                        uint8_t t2 = fgetc(f);
                        uint8_t t3 = fgetc(f);

                        // Wir mogeln das Tempo als spezielles Event in den Track
                        // Oder du speicherst es global.
                        // Vorschlag: Ein Event-Typ nutzen, den du erkennst.
                        MidiEvent tempoEv;
                        tempoEv.deltaTicks = delta;
                        tempoEv.type = 0xFF;    // Wir nutzen 0xFF als Markierung
                        tempoEv.channel = t1;   // Missbrauch: 1. Byte des Tempos
                        tempoEv.data1 = t2;     // Missbrauch: 2. Byte des Tempos
                        tempoEv.data2 = t3;     // Missbrauch: 3. Byte des Tempos
                        track.events.push_back(tempoEv);
                    }
                        break;
                    default:
                        fseek(f, metaLen, SEEK_CUR);
                        break;
                }
            }
            else if (status == 0xF0 || status == 0xF7)
            {
                fseek(f, ReadVLQ(f), SEEK_CUR);
            }
            else
            {
                MidiEvent ev;
                ev.deltaTicks = delta;
                ev.originalDelta = delta;
                ev.type = status & 0xF0;
                ev.channel = status & 0x0F;
                ev.data1 = fgetc(f);
                if (ev.type != 0xC0 && ev.type != 0xD0)
                    ev.data2 = fgetc(f);
                else
                    ev.data2 = 0;

                track.events.push_back(ev);
            }
        }
        m_tracks.push_back(track);
        fseek(f, trackEnd, SEEK_SET);
    }
    fclose(f);
    return true;
}

double MidiParser::CalculateDuration()
{
    double maxDuration = 0.0;
    for (size_t t = 0; t < m_tracks.size(); t++)
    {
        double trackTime = 0.0;
        uint32_t currentTempo = 500000;
        for (size_t e = 0; e < m_tracks[t].events.size(); e++)
        {
            MidiEvent &ev = m_tracks[t].events[e];
            
            if (ev.deltaTicks > 0)
            {
                double secondsPerTick = (double)currentTempo / 1000000.0 / (double)m_ticksPerQuarter;
                trackTime += (double)ev.deltaTicks * secondsPerTick;
            }

            if (ev.type == 0xFF)
                currentTempo = (ev.channel << 16) | (ev.data1 << 8) | ev.data2;
        }
        if (trackTime > maxDuration)
            maxDuration = trackTime;
    }
    return maxDuration;
}