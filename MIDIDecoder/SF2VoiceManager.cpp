#include "SF2VoiceManager.hpp"
#include "../Common.h"
#include <math.h>
#include <string.h>

SF2VoiceManager::SF2VoiceManager(int maxVoices) : m_maxvoices(maxVoices)
{
    m_voices.resize(maxVoices);
    for (int i = 0; i < maxVoices; i++)
    {
        m_voices[i].active = false;
        m_voices[i].pendingRelease = false; // Direkt für das Pedal mit-resetten!
        m_voices[i].inRelease = false;
        m_voices[i].baseStep = 0;
        m_voices[i].hasLoop = false;
        m_voices[i].loopLen = 0;
    }

    for (int i = 0; i < 256; i++)
        m_pitchTable[i] = powf(2.0f, (i - 128) / 12.0f);
    m_pitchTable[128] = 1.0f;

    for (int i = 0; i < 1024; i++)
    {
        // Normiere Index auf -1.0 bis +1.0
        float norm = (i - 511.5f) / 511.5f;

        // Berechne Faktor für +/- 2 Halbtöne
        // (norm * 2.0f) macht aus dem Rad-Ausschlag die Halbtöne
        m_bendTable[i] = powf(2.0f, (norm * 2.0f) / 12.0f);
    }

    for (int i = 0; i < 256; i++)
        // Schwingt sauber zwischen -1.0 und 1.0
        m_lfoTable[i] = sinf((i / 256.0f) * 2.0f * M_PI);

    for (int i = 0; i <= 1000; i++)
        // Formel: 10^(-cB / 200)
        m_attenuationTable[i] = powf(10.0f, (float)i / -200.0f);

    m_globalAgeCounter = 0;
    m_masterGain = 1.0f;
}

SF2VoiceManager::~SF2VoiceManager()
{
    for (unsigned int i = 0; i < m_voices.size(); i++)
        m_voices[i].active = false;
    m_voices.clear();
}

void SF2VoiceManager::NoteOn(const SampleMatch &match, int note, float vol, uint8_t pan, int chan, float currentBend)
{
    if (!match.left || !match.left->data)
        return;

    int targetIdx = -1;

    // 1. Suche nach einer freien Stimme
    for (int i = 0; i < m_maxvoices; i++)
    {
        if (!m_voices[i].active)
        {
            targetIdx = i;
            break;
        }
    }

    // 2. VOICE STEALING: Wenn keine frei ist, nimm die älteste (oder die leiseste)
    if (targetIdx == -1)
        targetIdx = FindOldestVoice();

    Voice &v = m_voices[targetIdx];

    // Reset/Setup
    v.active = false; // Kurz deaktivieren während wir schrauben
    v.midiNote = note;
    v.channel = chan;
    v.sample = match.left;

    // Offsets berechnen
    uint32_t base = v.sample->start;
    v.sampleEnd = v.sample->end - base;
    v.loopStart = v.sample->startLoop - base;
    v.loopEnd = v.sample->endLoop - base;
    v.loopLen = v.loopEnd - v.loopStart;
    v.inRelease = false;
    v.releaseLevel = 1.0f;
    v.rootKey = match.rootKey;

    // Loop-Check
    if (!match.hasLoop)
    {
        v.hasLoop = false;
        v.loopStart = 0;
        v.loopEnd = 0;
        v.loopLen = 0;
    }
    else
        v.hasLoop = match.hasLoop;

    // PITCH-BERECHNUNG (Optimiert)
    float pitchRatio = 1.0f;
    if (chan == 9)
    {
        // Drum-Kanal: Wir ignorieren die Note-Differenz
        pitchRatio = 1.0f;
        v.hasLoop = false;
        v.loopStart = 0;
        v.loopEnd = 0;
    }
    else
    {
        // 1. Ganztöne über die LUT
        int diff = note - match.rootKey + 128;
        if (diff >= 0 && diff < 256)
            pitchRatio = m_pitchTable[diff];

        // 2. Pitch Bend dazu (hier ist powf okay, weil nur 1x pro NoteOn)
        if (currentBend != 0.0f)
        {
            float bendInSemitones = currentBend * 2.0f;
            pitchRatio *= powf(2.0f, bendInSemitones / 12.0f);
        }
        // 3. Fine-Tuning
        if (v.sample->pitchCorrection != 0)
            pitchRatio *= powf(2.0f, (float)v.sample->pitchCorrection / 1200.0f);
    }

    // Rate-Korrektur (Sample-Rate des WAVs vs. Mixer-Rate)
    float rateRatio = (float)v.sample->sampleRate / 44100.0f;
    uint32_t step =  (uint32_t)((pitchRatio * rateRatio) * 65536.0f);
    if (step == 0)
        step = (uint32_t)((rateRatio) * 65536.0f);

    v.step = step;
    v.baseStep = v.step;
    v.pos = 0;
    v.posHigh = 0; 

    // 4. PANNING & VOLUME
    // Attenuation aus LUT (0.1 dB Schritte)
    int cB = match.attenuation;
    if (cB < 0) cB = 0;
    if (cB > 1000) cB = 1000;
    float sfAttenuation = 0.5f;
    if (match.attenuation != 0)
    {
        // Wir begrenzen den Wert nach oben! 
        // Mehr als 250-300 cB (25-30 dB) ist bei normalen Instrumenten selten.
        int safeAtten = match.attenuation;
        if (safeAtten > 500) safeAtten = 500; 
        
        sfAttenuation = m_attenuationTable[safeAtten];
    }

    // Gain-Berechnung mit einem kleinen Headroom-Faktor (z.B. 1.5 oder 2.0)
    // Viele Sampler machen das, um die Dämpfung des Soundfonts zu kompensieren.
    float masterBoost = 1.5f; 
    float totalGain = vol * sfAttenuation * masterBoost;

    // 4. Stereo-Verteilung (Panning)
    float fPan = (float)pan / 127.0f;
    v.lGain = totalGain * (1.0f - fPan);
    v.rGain = totalGain * fPan;

    // 5. Voice aktivieren
    v.active = true;
}

void SF2VoiceManager::NoteOff(int midiNote, int chan, bool sustainActive)
{
    for (int i = 0; i < m_maxvoices; i++)
    {
        if (m_voices[i].active && m_voices[i].midiNote == midiNote && m_voices[i].channel == chan)
        {
            if (sustainActive)
                m_voices[i].pendingRelease = true;
            else
            {
                m_voices[i].inRelease = true;
                m_voices[i].pendingRelease = false;
            }
        }
    }
}

int SF2VoiceManager::FindOldestVoice()
{
    int oldestIdx = 0;
    uint32_t maxAge = 0;

    // Strategie 1: Suche zuerst eine Stimme, die bereits im Release ist
    // (Denn die hört man am wenigsten, wenn sie abgebrochen wird)
    for (int i = 0; i < m_maxvoices; i++)
    {
        if (m_voices[i].inRelease)
        {
            uint32_t age = m_globalAgeCounter - m_voices[i].age;
            if (age > maxAge)
            {
                maxAge = age;
                oldestIdx = i;
            }
        }
    }

    // Wenn wir eine Stimme im Release gefunden haben, nimm die
    if (maxAge > 0)
        return oldestIdx;

    // Strategie 2: Wenn alle Stimmen noch aktiv gehalten werden (Sustain),
    // nimm die absolut älteste Note.
    maxAge = 0;
    for (int i = 0; i < m_maxvoices; i++)
    {
        uint32_t age = m_globalAgeCounter - m_voices[i].age;
        if (age > maxAge)
        {
            maxAge = age;
            oldestIdx = i;
        }
    }

    return oldestIdx;
}

void SF2VoiceManager::Mix(short *outBuffer, uint32_t numFrames)
{
    static int32_t temp[2048 * 2];
    uint32_t frames = (numFrames > 2048) ? 2048 : numFrames;
    memset(temp, 0, frames * 2 * sizeof(int32_t));

    for (int vIdx = 0; vIdx < m_maxvoices; vIdx++)
    {
        Voice &v = m_voices[vIdx];
        if (!v.active || !v.sample || !v.sample->data)
            continue;

        const int16_t *__restrict sData = v.sample->data;
        uint32_t pos = v.pos;
        uint32_t posHigh = v.posHigh;
        uint32_t step = v.step;
        uint32_t loopStart = v.loopStart;
        uint32_t loopEnd = v.loopEnd;
        uint32_t sampleEnd = v.sampleEnd;
        uint32_t loopLen = v.loopLen;
        bool hasLoop = v.hasLoop;

        int32_t baseLG = (int32_t)(v.lGain * 256.0f);
        int32_t baseRG = (int32_t)(v.rGain * 256.0f);

        static const int32_t RELEASE_MUL = 32750;
        int32_t relVal = (int32_t)(v.releaseLevel * 32768.0f);

        for (uint32_t i = 0; i < frames; i++)
        {
            uint32_t idx1 = (posHigh << 16) | (pos >> 16);  
            if (idx1 >= sampleEnd) 
            {
                v.active = false;
                break;
            }

            if (hasLoop)
            {
                if (idx1 >= loopEnd)
                {
                    if (loopLen > 0)
                    {
                        uint32_t overShot = idx1 - loopEnd;
                        idx1 = loopStart + (overShot % loopLen);
                        posHigh = idx1 >> 16;
                        pos = (idx1 & 0xFFFF) << 16;
                    }
                    else
                    {
                        v.active = false;
                        break;
                    }
                }
            }
            else
            {
                if (idx1 >= sampleEnd - 1)
                {
                    v.active = false;
                    break;
                }
            }

            // --- 2. INTERPOLATION ---
            uint32_t fract = pos & 0xFFFF;
            int32_t s1 = sData[idx1];
            uint32_t idx2 = idx1 + 1;

            if (hasLoop && idx2 >= loopEnd)
                idx2 = loopStart;
            else if (idx2 >= sampleEnd)
                idx2 = sampleEnd - 1;

            int32_t s2 = sData[idx2];
            int32_t sample = s1 + (((s2 - s1) * (int32_t)fract) >> 16);

            // --- 3. VOLUME & RELEASE ---
            int32_t currentLG, currentRG;
            if (v.inRelease)
            {
                relVal = (relVal * RELEASE_MUL) >> 15;
                if (relVal < 10)
                {
                    v.active = false;
                    break;
                }
                currentLG = (baseLG * relVal) >> 15;
                currentRG = (baseRG * relVal) >> 15;
            }
            else
            {
                currentLG = baseLG;
                currentRG = baseRG;
            }

            temp[i * 2] += (sample * currentLG) >> 8;
            temp[i * 2 + 1] += (sample * currentRG) >> 8;

            uint32_t oldPos = pos;
            pos += step;
            if (pos < oldPos)
                posHigh++;
        }

        // Status zurückschreiben
        v.pos = pos;
        v.posHigh = posHigh;
        v.releaseLevel = (float)relVal / 32768.0f;
    }
    ApplyLimiter(temp, outBuffer, frames);
}

void SF2VoiceManager::ReleaseSustainedNotes(uint8_t channel)
{
    for (int i = 0; i < m_maxvoices; i++)
    {
        if (m_voices[i].active && m_voices[i].channel == channel && m_voices[i].pendingRelease)
        {
            m_voices[i].inRelease = true; // Jetzt darf die Hüllkurve ausklingen
            m_voices[i].pendingRelease = false;
        }
    }
}

void SF2VoiceManager::UpdateChannelPitch(uint8_t chan, float bendNorm)
{
    int bendIdx = (int)((bendNorm + 1.0f) * 511.5f);
    if (bendIdx < 0)
        bendIdx = 0;
    if (bendIdx > 1023)
        bendIdx = 1023;

    float bendFactor = m_bendTable[bendIdx];

    for (int i = 0; i < m_maxvoices; i++)
    {
        Voice &v = m_voices[i];
        // Drums (9) ignorieren Pitch Bend normalerweise
        if (v.active && v.channel == chan && chan != 9)
        {
            // v.baseStep enthält schon Note + Samplerate + FineTune
            // Wir wenden NUR noch den Bend-Faktor an.
            v.step = (uint32_t)((float)v.baseStep * bendFactor);
        }
    }
}

void SF2VoiceManager::ApplyLimiter(int32_t *temp, int16_t *outBuffer, uint32_t frames)
{
    const float attack = 0.05f;      // Wie schnell er leiser macht
    const float release = 0.001f;    // Wie langsam er wieder lauter wird
    const int32_t threshold = 28000; // Ab hier greift der Limiter (etwas unter 32768)

    for (uint32_t i = 0; i < frames * 2; i++)
    {
        // 1. Absolutwert des aktuellen Summen-Samples holen
        int32_t absVal = (temp[i] < 0) ? -temp[i] : temp[i];

        // 2. Ziel-Gain berechnen
        float targetGain = 1.0f;
        if (absVal > threshold)
        {
            targetGain = (float)threshold / (float)absVal;
        }

        // 3. MasterGain anpassen (Slew Rate Limiting)
        if (targetGain < m_masterGain)
        {
            // Schnell leiser machen (Attack)
            m_masterGain += (targetGain - m_masterGain) * attack;
        }
        else
        {
            // Langsam wieder lauter machen (Release)
            m_masterGain += (targetGain - m_masterGain) * release;
        }

        // 4. Sample mit MasterGain multiplizieren und clippen
        int32_t finalVal = (int32_t)(temp[i] * m_masterGain);

        // Hard-Clipping als letzte Sicherung
        if (finalVal > 32767)
            finalVal = 32767;
        else if (finalVal < -32768)
            finalVal = -32768;

        outBuffer[i] = (int16_t)finalVal;
    }
}
