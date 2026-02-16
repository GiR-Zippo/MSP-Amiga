#include "MidiAudioStreamRunner.hpp"
#include "MidiAudioStream.hpp"
#include "../Shared/Configuration.hpp"

void MidiAudioStreamRunner::Run(MidiAudioStream *parent)
{
    MidiAudioStreamRunner worker(parent);
    strcpy(parent->m_title, "...");
    if (worker.open(parent->m_file.c_str()))
    {
        printf("File open %s\n", parent->m_file.c_str());
        std::vector<std::string> strings = Split(parent->m_file, ":");
        strings = Split(strings.back(), "/");
        strcpy(parent->m_title, strings.back().c_str());
        worker.TaskLoop();
    }
}

MidiAudioStreamRunner::MidiAudioStreamRunner(MidiAudioStream *parent)
{
    m_sf2 = new SF2Parser();
    m_midi = new MidiParser();
    m_mixer = new SF2VoiceManager(128); //128 voices
    for (int i = 0; i < 16; i++)
    {
        m_chans[i].vol = 100;
        m_chans[i].pan = 64; // Mitte
        m_chans[i].prog = 0; // Piano
        m_chans[i].sustainPedal = false;
        m_chans[i].pitchBend = 0.0f;
        m_chans[i].bendRange = 2.0f;
    }
    m_totalFramesDone = 0.0;
    m_parent = parent;
}

MidiAudioStreamRunner::~MidiAudioStreamRunner()
{
    delete m_mixer;
    delete m_midi;
    delete m_sf2;
}

bool MidiAudioStreamRunner::open(const char *file)
{
    strcpy(m_parent->m_artist, "Loading Soundfont...\0");
    if (!m_sf2->Load(sConfiguration->GetConfigString("SoundFontFile", "default.sf2")))
    {
        strcpy(m_parent->m_artist, "Error loading SoundFont!\0");
        return false;
    }

    strcpy(m_parent->m_artist, "Loading Midi...\0");
    if (!m_midi->Load(file))
    {
        strcpy(m_parent->m_artist, "Error loading Midi!\0");
        return false;
    }

    m_parent->m_duration = m_midi->CalculateDuration();

    m_eventIdx.clear();
    m_eventIdx.resize(m_midi->GetTracks().size(), 0);

    double bpm = 120.0;
    double ppq = (double)m_midi->GetTicksPerQuarter(); // Meist 480

    if (ppq <= 0)
        ppq = 480; // Sicherheitsnetz

    // Formel: (Samples pro Minute) / (Ticks pro Minute)
    // (44100 * 60) / (BPM * PPQ)
    m_spt = ((44100.0 * 60.0) / (bpm * ppq)) *2;

    // m_spt = (44100.0 * 60.0) / (120.0 * m_midi.GetTicksPerQuarter());
    m_samplesToNext = 0;

    strcpy(m_parent->m_artist, "Playing Midi...\0");
    return true;
}

void MidiAudioStreamRunner::TaskLoop()
{
    const int CHUNK_FRAMES = 1024;
    short mixBuffer[CHUNK_FRAMES * 2];

    while (!m_parent->m_stop)
    {
        int framesDone = 0;
        short *writePtr = mixBuffer;

        if (m_parent->m_seekSeconds >= 0.0)
        {
            double target = m_parent->m_seekSeconds;
            m_parent->m_seekSeconds = -1;
            SeekTo(target);
            m_samplesToNext = 0;
        }

        while (framesDone < CHUNK_FRAMES && !m_parent->m_stop)
        {
            // MIDI Events fällig?
            if (m_samplesToNext == 0)
                ProcessMidiEvents();

            int framesUntilNextEvent = (int)m_samplesToNext;
            int framesRemaining = CHUNK_FRAMES - framesDone;

            int framesThisStep = (framesUntilNextEvent < framesRemaining)
                                     ? framesUntilNextEvent
                                     : framesRemaining;

            if (framesThisStep > 0)
            {
                m_mixer->Mix(writePtr, framesThisStep);

                m_totalFramesDone += framesThisStep;
                m_parent->m_currentTime = (double)m_totalFramesDone / 44100.0;

                writePtr += (framesThisStep * 2);
                framesDone += framesThisStep;
                m_samplesToNext -= framesThisStep;
            }
            else if (m_samplesToNext == 0)
                m_samplesToNext = CHUNK_FRAMES;
        }

        while (m_parent->m_q && !m_parent->m_stop && 
            !m_parent->m_q->put(mixBuffer, CHUNK_FRAMES * 2))
            Delay(10);
    }
}

void MidiAudioStreamRunner::ProcessMidiEvents()
{
    // WICHTIG: minNextDelta muss jedes Mal frisch auf "Unendlich" gesetzt werden
    uint32_t minNextDelta = 0xFFFFFFFF;
    bool anyEventsLeft = false;

    std::vector<MidiTrack> &allTracks = (std::vector<MidiTrack> &)m_midi->GetTracks();

    for (size_t i = 0; i < allTracks.size(); i++)
    {
        MidiTrack &track = allTracks[i];
        size_t &idx = m_eventIdx[i];

        while (idx < track.events.size())
        {
            MidiEvent &ev = track.events[idx];

            if (ev.deltaTicks == 0)
            {
                ExecuteMidiEvent(ev);
                idx++;
            }
            else
            {
                // Hier finden wir die Zeit bis zum nächsten Event
                if (ev.deltaTicks < minNextDelta)
                    minNextDelta = ev.deltaTicks;
                anyEventsLeft = true;
                break;
            }
        }
    }
    if (anyEventsLeft && minNextDelta != 0xFFFFFFFF)
    {
        // Berechne die Samples bis zum nächsten Aufruf
        m_samplesToNext = (uint32_t)(minNextDelta * m_spt);

        // Notbremse: Falls m_spt oder minNextDelta Murks sind
        if (m_samplesToNext == 0)
            m_samplesToNext = 1;

        // WICHTIG: Wir müssen die verbrauchte Zeit von ALLEN Tracks abziehen
        for (size_t i = 0; i < allTracks.size(); i++)
        {
            size_t idx = m_eventIdx[i];
            if (idx < allTracks[i].events.size())
                allTracks[i].events[idx].deltaTicks -= minNextDelta;
        }
    }
    else
    {
        // Keine Events mehr gefunden
        m_samplesToNext = 2048;
        Forbid();
        m_parent->m_stop = true;
        Permit();
    }
}

void MidiAudioStreamRunner::ExecuteMidiEvent(const MidiEvent &ev)
{
    // Spezial-Check für "gequetschtes" Tempo-Event
    if (ev.type == 0xFF)
    {
        // Tempo in channel, data1 und data2 versteckt
        uint32_t mpqn = (ev.channel << 16) | (ev.data1 << 8) | ev.data2;

        // Safety 1: PPQ darf nicht 0 sein. Standard ist 480.
        m_ppq = m_midi->GetTicksPerQuarter();
        if (m_ppq <= 24)
            m_ppq = 480;

        if (mpqn > 0)
        {
            float bpm = 60000000.0f / (float)mpqn;

            // Safety 2: BPM check
            if (bpm < 1.0f)
                bpm = 120.0f;

            double num = 44100.0 * (double)mpqn;
            double den = 1000000.0 * (double)m_ppq;

            m_spt = num / den;
            printf("MPQN: %u, PPQ: %f -> SPT: %f BPM: %f \n", mpqn, m_ppq, m_spt, bpm);
        }
    }

    uint8_t status = ev.type & 0xF0;
    uint8_t chan = ev.channel;

    switch (status)
    {
        // Note On
        case 0x90:
            if (ev.data2 > 0)
            {
                int bank = (chan == 9) ? 128 : 0;
                SampleMatch m = m_sf2->GetSampleForNote(bank, m_chans[chan].prog, ev.data1, ev.data2);
                if (m.left)
                {
                    m_sf2->EnsureSampleLoaded(m.left);
                    if (m.right)
                        m_sf2->EnsureSampleLoaded(m.right);

                    float vol = (ev.data2 / 127.0f) * (m_chans[chan].vol / 127.0f);
                    m_mixer->NoteOn(m, ev.data1, vol, m_chans[chan].pan, chan, m_chans[chan].pitchBend);
                }
            }
            else
                // Note On mit Velocity 0 ist ein Note Off
                m_mixer->NoteOff(ev.data1, chan, m_chans[chan].sustainPedal);
            break;
        
        // Note Off
        case 0x80:
                m_mixer->NoteOff(ev.data1, chan, m_chans[chan].sustainPedal);
            break;

        // Controller Change
        case 0xB0:
            if (ev.data1 == 1) // Modulation
            {
                //printf("Mod \n");
                m_chans[chan].modulation = ev.data2;
                //m_mixer->UpdateChannelModulation(chan, ev.data2);
            }
            else if (ev.data1 == 6)
            {
                if (m_chans[chan].rpnMSB == 0 && m_chans[chan].rpnLSB == 0)
                // Wir haben RPN 00:00 erkannt -> Pitch Bend Sensitivity
                m_chans[chan].bendRange = ev.data2; // Wert in Halbtönen (z.B. 12)
            }
            else if (ev.data1 == 7)
                m_chans[chan].vol = ev.data2;
            else if (ev.data1 == 10)
                m_chans[chan].pan = ev.data2;
            else if (ev.data1 == 64) // SUSTAIN PEDAL
            {
                bool pedalPressed = (ev.data2 >= 64);
                m_chans[chan].sustainPedal = pedalPressed;
                if (!pedalPressed)
                    // Wenn Pedal losgelassen wird: Alle wartenden Noten ausklingen lassen
                    m_mixer->ReleaseSustainedNotes(chan); 
            }
            else if (ev.data1 == 100)
                m_chans[chan].rpnLSB = ev.data2;
            else if (ev.data1 == 101)
                m_chans[chan].rpnMSB = ev.data2;
            else if (ev.data1 == 121 || ev.data1 == 123)
            {
                // All Notes Off für diesen Kanal
                for (int n = 0; n < 128; n++)
                    m_mixer->NoteOff(n, chan, false);
            }
            break;
        
        // Program Change
        case 0xC0:
            m_chans[chan].prog = ev.data1;
            break;
        // Pitch Bend
        case 0xE0:
        {
            uint16_t bendRaw = (ev.data2 << 7) | ev.data1; 
            float bendNorm = ((float)bendRaw - 8192.0f) / 8192.0f;
            if (bendRaw > 8190 && bendRaw < 8194) bendNorm = 0.0f;
            m_chans[chan].pitchBend = bendNorm;
            m_mixer->UpdateChannelPitch(chan, bendNorm);
            break;
        }
    }
}

void MidiAudioStreamRunner::SeekTo(double targetSeconds)
{
    // 1. Alle Noten stoppen
    for (int chan = 0; chan < 16; chan++)
        for (int n = 0; n < 128; n++)
            m_mixer->NoteOff(n, chan, false);
    
    // 2. State zurücksetzen
    m_parent->m_currentTime = 0.0;
    m_totalFramesDone = 0;
    m_samplesToNext = 0;
    
    // 3. Event-Indices zurücksetzen
    for (size_t i = 0; i < m_eventIdx.size(); i++)
        m_eventIdx[i] = 0;
    
    // 4. Deltas wiederherstellen
    std::vector<MidiTrack> &allTracks = (std::vector<MidiTrack> &)m_midi->GetTracks();
    for (size_t t = 0; t < allTracks.size(); t++)
    {
        for (size_t e = 0; e < allTracks[t].events.size(); e++)
            allTracks[t].events[e].deltaTicks = allTracks[t].events[e].originalDelta;
    }
    
    // 5. Akkumulierte Ticks bis Ziel berechnen
    double ppq = (double)m_midi->GetTicksPerQuarter();
    if (ppq <= 0) ppq = 480;
    
    uint32_t currentTempo = 500000; // 120 BPM default
    double currentTime = 0.0;
    
    // Sicherheits-Counter
    int maxIterations = 1000000;
    int iteration = 0;
    
    // 6. Fast-forward durch Events
    while (currentTime < targetSeconds && iteration++ < maxIterations)
    {
        // Finde nächstes Event über alle Tracks
        uint32_t minDelta = 0xFFFFFFFF;
        int minTrack = -1;
        
        for (size_t t = 0; t < allTracks.size(); t++)
        {
            size_t idx = m_eventIdx[t];
            if (idx < allTracks[t].events.size())
            {
                uint32_t delta = allTracks[t].events[idx].deltaTicks;
                if (delta < minDelta)
                {
                    minDelta = delta;
                    minTrack = t;
                }
            }
        }
        
        // Keine Events mehr
        if (minTrack == -1)
            break;
        
        // Zeit für dieses Delta berechnen
        double secondsPerTick = ((double)currentTempo / 1000000.0) / ppq;
        double deltaTime = (double)minDelta * secondsPerTick;
        
        // Würden wir überschießen?
        if (currentTime + deltaTime > targetSeconds)
            break;
        
        // Zeit vorspulen
        currentTime += deltaTime;
        
        // Delta von ALLEN Tracks abziehen
        for (size_t t = 0; t < allTracks.size(); t++)
        {
            size_t idx = m_eventIdx[t];
            if (idx < allTracks[t].events.size())
                allTracks[t].events[idx].deltaTicks -= minDelta;
        }
        
        // Events mit deltaTicks == 0 verarbeiten
        for (size_t t = 0; t < allTracks.size(); t++)
        {
            size_t &idx = m_eventIdx[t];
            
            while (idx < allTracks[t].events.size() && 
                   allTracks[t].events[idx].deltaTicks == 0)
            {
                MidiEvent &ev = allTracks[t].events[idx];
                uint8_t status = ev.type & 0xF0;
                
                // Nur State-relevante Events verarbeiten (KEINE NoteOns!)
                if (status == 0xB0 || status == 0xC0 || status == 0xE0)
                {
                    ExecuteMidiEvent(ev);
                }
                else if (ev.type == 0xFF)  // Tempo
                {
                    uint32_t mpqn = (ev.channel << 16) | (ev.data1 << 8) | ev.data2;
                    m_ppq = m_midi->GetTicksPerQuarter();
                    if (m_ppq <= 24)
                        m_ppq = 480;

                    if (mpqn > 0)
                    {
                        float bpm = 60000000.0f / (float)mpqn;
                        if (bpm < 1.0f)
                            bpm = 120.0f;

                        double num = 44100.0 * (double)mpqn;
                        double den = 1000000.0 * (double)m_ppq;

                        m_spt = num / den;
                    }
                }
                
                idx++;
            }
        }
    }
    m_parent->m_currentTime = currentTime;
    m_totalFramesDone = (uint32_t)(currentTime * 44100.0);
    m_samplesToNext = 0;
}