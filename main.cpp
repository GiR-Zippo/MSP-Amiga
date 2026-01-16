#include "Common.h"
#include "gui.hpp"
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/dos.h>
#include <dos/dostags.h>

#include "AHIPlayback.hpp"
#include "AudioStream.hpp"

#include "AACDecoder/AACStream.hpp"
#include "FLACDecoder/FlacStream.hpp"
#include "MP3Decoder/MP3Stream.hpp"
#include "VorbisDecoder/VorbisStream.hpp"
#include "M4ADecoder/M4ADecoder.hpp"
#include <cstring>

/* ------  Playerstates  ------ */
enum PlayerStates
{
    PFLAG_NON_INIT = 0,         // Kein file, keine Kekse
    PFLAG_INIT_DONE = (1 << 0), // File geladen, alles initialisiert
    PFLAG_PLAYING = (1 << 1),   // Wir spielen
    PFLAG_STOP = (1 << 2),      // Just stopped _playback and _stream exists
    PFLAG_SEEK = (1 << 3)       // Wir seeken

};
uint16_t _playerState = PFLAG_NON_INIT;

bool hasFlag(PlayerStates state) { return (_playerState & state) != 0; }
void setFlag(PlayerStates state) { _playerState |= state; }
void removeFlag(PlayerStates state) { _playerState &= ~state; }
/* ----- Playerstates End ----- */

struct PlayerArgs
{
    AHIPlayback *playback;
    AudioStream *stream;
};

void PlayerTaskFunc()
{
    // wart mal kurz
    Delay(1);
    struct Process *me = (struct Process *)FindTask(NULL);
    PlayerArgs *pb = (PlayerArgs *)me->pr_Task.tc_UserData;
    if (!pb)
        return;

    while (hasFlag(PFLAG_INIT_DONE))
    {
        if (hasFlag(PFLAG_PLAYING) && !hasFlag(PFLAG_SEEK))
        {
            if (!pb->playback->Update())
            {
                removeFlag(PFLAG_PLAYING);
                setFlag(PFLAG_STOP);
                pb->playback->Stop();
                pb->stream->seek(0);
                printf("Task: Song beendet.\n");
            }
        }
        Delay(1);
    }
    printf("Task: Beendet.\n");
}

/* -------------------------------------------------------------------------- */
/* main routine                                                               */
/* -------------------------------------------------------------------------- */
int main()
{
    AHIPlayback *_playback = NULL;
    AudioStream *_stream = NULL;

    if (setupGUI())
    {
        bool running = true;
        struct IntuiMessage *msg;
        _playerState = PFLAG_NON_INIT;

        struct TagItem playerTags[] = {
            {NP_Entry, (IPTR)PlayerTaskFunc},
            {NP_Name, (IPTR) "Audio_Engine"},
            {NP_Priority, 0},
            {NP_StackSize, 32768},
            {TAG_DONE, 0}};

        ULONG windowSig = getWinSignal();
        while (running)
        {
            ULONG signals = Wait(windowSig | SIGBREAKF_CTRL_C);

            if (signals & windowSig)
                while ((msg = GT_GetIMsg(win->UserPort)))
                {
                    struct Gadget *gad = (struct Gadget *)msg->IAddress;
                    uint16 msgCode = msg->Code;
                    GT_ReplyIMsg(msg);

                    if (msg->Class == IDCMP_CLOSEWINDOW)
                        running = false;

                    if (msg->Class == IDCMP_INTUITICKS)
                    {
                        if (hasFlag(PFLAG_INIT_DONE))
                        {
                            if (hasFlag(PFLAG_PLAYING) && !hasFlag(PFLAG_SEEK))
                            {
                                updateTimeDisplay(_stream->getCurrentSeconds(), _stream->getDuration());
                                updateSeeker((int)((_stream->getCurrentSeconds() * 100) / _stream->getDuration()));
                            }
                        }
                    }

                    if (msg->Class == IDCMP_GADGETUP)
                    {
                        if (gad->GadgetID == ID_SEEKER)
                        {
                            if (hasFlag(PFLAG_INIT_DONE))
                            {
                                uint32_t seekTime = ((uint32_t)msgCode * _stream->getDuration()) / 100;
                                _stream->seek(seekTime);
                                removeFlag(PFLAG_SEEK);
                            }
                        }
                    }

                    if (msg->Class == IDCMP_MOUSEMOVE)
                    {
                        if (gad->GadgetID == ID_SEEKER)
                        {
                            if (hasFlag(PFLAG_INIT_DONE))
                            {
                                uint32_t seekTime = (msgCode * _stream->getDuration()) / 100;
                                updateTimeDisplay(seekTime, _stream->getDuration());
                            }
                        }
                    }
                    if (msg->Class == IDCMP_GADGETDOWN)
                    {
                        if (gad->GadgetID == ID_SEEKER)
                            setFlag(PFLAG_SEEK);
                    }

                    if (msg->Class == IDCMP_GADGETUP)
                    {
                        struct Gadget *gad = (struct Gadget *)msg->IAddress;
                        if (gad->GadgetID == ID_PLAY)
                        {
                            if (hasFlag(PFLAG_INIT_DONE))
                            {
                                _playback->Init();
                                setFlag(PFLAG_PLAYING);
                                removeFlag(PFLAG_STOP);
                            }
                        }
                        else if (gad->GadgetID == ID_STOP)
                        {
                            if (hasFlag(PFLAG_PLAYING))
                            {
                                _playback->Stop();
                                _stream->seek(0);
                                removeFlag(PFLAG_PLAYING);
                                setFlag(PFLAG_STOP);
                            }
                        }
                        else if (gad->GadgetID == ID_OPEN)
                        {
                            if (hasFlag(PFLAG_PLAYING))
                            {
                                removeFlag(PFLAG_PLAYING);
                                _playback->Stop();
                            }

                            if (!hasFlag(PFLAG_NON_INIT))
                            {
                                if (_playback)
                                    delete _playback;
                                if (_stream)
                                    delete _stream;
                                _playback = NULL;
                                _stream = NULL;
                            }
                            _playerState = PFLAG_NON_INIT; // clear flags
                            std::string file = openFileRequest();
                            if (!file.empty())
                            {
                                if (strstr(file.c_str(), ".mp3"))
                                    _stream = new MP3Stream();
                                else if (strstr(file.c_str(), ".flac"))
                                    _stream = new FlacStream();
                                else if (strstr(file.c_str(), ".aac"))
                                    _stream = new AACStream();
                                else if (strstr(file.c_str(), ".ogg"))
                                    _stream = new VorbisStream();
                                else if (strstr(file.c_str(), ".m4a"))
                                    _stream = new M4AStream();

                                if (_stream->open(file.c_str()))
                                {
                                    _playback = new AHIPlayback(_stream);
                                    setFlag(PFLAG_INIT_DONE);
                                    // prepare audio task and start audio task
                                    PlayerArgs g_args;
                                    g_args.playback = _playback;
                                    g_args.stream = _stream;
                                    struct Process *playerProc = (struct Process *)CreateNewProc(playerTags);
                                    if (playerProc)
                                        playerProc->pr_Task.tc_UserData = (APTR)&g_args;
                                }
                            }
                        }
                    }
                }
        }

        // Schluss jetzt
        cleanupGUI();
        printf("Cleabup\n");

        if (hasFlag(PFLAG_PLAYING))
        {
            _playerState = PFLAG_NON_INIT;
            _playback->Stop();
        }

        _playerState = PFLAG_NON_INIT;
        while (FindTask((CONST_STRPTR)"Audio_Engine"))
            Delay(2);

        if (_playback)
            delete _playback;
        if (_stream)
            delete _stream;
        _playback = NULL;
        _stream = NULL;

        // clear flags
        printf("Cleabup done\n");
    }

    return 0;
}