#ifndef __MIDIAUDIOSTREAMRUNNER_HPP__
#define __MIDIAUDIOSTREAMRUNNER_HPP__

#include "Common.h"
#include "../Shared/AudioQueue.hpp"
#include "MidiParser.hpp"
#include "SF2VoiceManager.hpp"

struct MidiChanState
{
    uint8_t vol, pan, prog, modulation, rpnMSB, rpnLSB, bendRange;
    bool sustainPedal;
    float pitchBend;
    MidiChanState() : vol(100), pan(64), prog(0) {}
};

class MidiAudioStream;
class MidiAudioStreamRunner
{
    public:
        static void Run(MidiAudioStream *parent);

    private:
        MidiAudioStreamRunner(MidiAudioStream *parent);
        ~MidiAudioStreamRunner();
        bool open(const char *file);
        void TaskLoop();
        void ExecuteMidiEvent(const MidiEvent &ev);
        void ProcessMidiEvents();
        void SeekTo(double targetSeconds);
        MidiAudioStream *m_parent;
        SF2Parser *m_sf2;
        MidiParser *m_midi;
        SF2VoiceManager *m_mixer;
        MidiChanState m_chans[16];
        uint32_t m_samplesToNext;
        double m_spt; // samples per tick
        float m_ppq;
        std::vector<size_t> m_eventIdx;
        double m_totalFramesDone;
};
#endif