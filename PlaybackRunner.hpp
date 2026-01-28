#ifndef __PLAYBACKRUNNER_HPP__
#define __PLAYBACKRUNNER_HPP__

#include "Common.h"
#include "AudioStream.hpp"
#include "AHIPlayback.hpp"

/* ------  m_PlayerStates  ------ */
enum m_PlayerStates
{
    PFLAG_NON_INIT = 0,         // Kein file, keine Kekse
    PFLAG_INIT_DONE = (1 << 0), // File geladen, alles initialisiert
    PFLAG_PLAYING = (1 << 1),   // Wir spielen
    PFLAG_STOP = (1 << 2),      // Just stopped _playback and _stream exists
    PFLAG_SEEK = (1 << 3),      // Wir seeken
    PFLAG_PAUSE = (1 << 4)      // Pause
};

struct PlayerArgs
{
    AudioStream *stream;
    uint16_t volumeLevel;
    ULONG songEndMask;
    struct Task *mainTask;
};

class PlaybackRunner
{
    public:
        static PlaybackRunner& getInstance()
        {
            static PlaybackRunner instance;
            return instance;
        }

        /// @brief Starts initalize and start the playback
        bool StartPlaybackTask(std::string file);

        /// @brief The Playback Task (don't call it Schnitzel)
        static void PlayerTaskFunc();

        /// @brief The cleanup, call it when app has finished
        void Cleanup();

        /// @brief Sets the volume
        void SetVolume(uint16_t vol);

        /// @brief Gets the current stream
        /// @return the current Audiostream
        AudioStream* GetStream() { return m_stream; }

        //Signalling
        ULONG GetSignal() { return m_songEndSignal;}
        ULONG GetMask() { return m_songEndMask;}

        //m_PlayerStates
        bool hasFlag(m_PlayerStates state) { return (m_PlayerState & state) != 0; }
        void setFlag(m_PlayerStates state) { m_PlayerState |= state; }
        void toggleFlag(m_PlayerStates state) { m_PlayerState ^= state; }
        void removeFlag(m_PlayerStates state) { m_PlayerState &= ~state; }
        void clearFlags() { m_PlayerState = 0; }

    private:
        PlaybackRunner();
        PlaybackRunner(const PlaybackRunner&);
        PlaybackRunner& operator=(const PlaybackRunner&);

        struct Process *m_playerProc;
        AudioStream    *m_stream;
        int             m_songEndSignal;
        ULONG           m_songEndMask;
        uint16_t        m_PlayerState;
};
#endif