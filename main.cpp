#include "Common.h"
#include "gui.hpp"
#include "PlaylistWindow.hpp"
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
#include "WebRadioDecoder/StreamClient.hpp"
#include <cstring>

/* ------  Playerstates  ------ */
enum PlayerStates
{
    PFLAG_NON_INIT = 0,         // Kein file, keine Kekse
    PFLAG_INIT_DONE = (1 << 0), // File geladen, alles initialisiert
    PFLAG_PLAYING = (1 << 1),   // Wir spielen
    PFLAG_STOP = (1 << 2),      // Just stopped _playback and _stream exists
    PFLAG_SEEK = (1 << 3),      // Wir seeken
    PFLAG_PAUSE = (1 << 4)      // Pause

};
uint16_t _playerState = PFLAG_NON_INIT;

bool hasFlag(PlayerStates state) { return (_playerState & state) != 0; }
void setFlag(PlayerStates state) { _playerState |= state; }
void toggleFlag(PlayerStates state) { _playerState ^= state; }
void removeFlag(PlayerStates state) { _playerState &= ~state; }
void clearFlags() { _playerState = 0; }
/* ----- Playerstates End ----- */


struct PlayerArgs
{
    AudioStream *stream;
    uint16_t volumeLevel;
};

/* ------  the audio playback task  ------ */
void PlayerTaskFunc()
{
    // wart mal kurz
    Delay(1);
    struct Process *me = (struct Process *)FindTask(NULL);
    PlayerArgs *pb = (PlayerArgs *)me->pr_Task.tc_UserData;

    bool _audioInitDone = false;
    bool _updateWait = false;

    //no data
    if (!pb)
        return;

    //create the AHIPlayback in this scope
    AHIPlayback *playback = new AHIPlayback(pb->stream);
    while (hasFlag(PFLAG_INIT_DONE))
    {
        //sanitycheck
        if (!pb || !pb->stream )
            break;

        if (playback->GetVolume() != pb->volumeLevel)
            playback->SetVolume(pb->volumeLevel);

        //wir sollen spielen, sind nicht initialisiert?
        if (hasFlag(PFLAG_PLAYING) && !_audioInitDone)
        {
            playback->Init();
            _audioInitDone = true;
            removeFlag(PFLAG_STOP);
        }

        //wir sollen stoppen
        if (hasFlag(PFLAG_STOP))
        {
            if (!_updateWait && hasFlag(PFLAG_PLAYING))
            {
                removeFlag(PFLAG_PLAYING);
                playback->Stop();
                pb->stream->seek(0);
                _audioInitDone = false;
            }
        }
        
        //die Dudelroutine
        if (hasFlag(PFLAG_PLAYING) && !hasFlag(PFLAG_SEEK) && !hasFlag(PFLAG_PAUSE))
        {
            _updateWait = true;
            if (!playback->Update())
            {
                removeFlag(PFLAG_PLAYING);
                setFlag(PFLAG_STOP);
                playback->Stop();
                pb->stream->seek(0);
                printf("Task: Song beendet.\n");
            }
            _updateWait = false;
        }
        else
            Delay(5); //gibt nix zu tun, also warte
    }

    //und weg damit
    delete playback;
    clearFlags();
    printf("Task: Beendet.\n");
}

/* -------------------------------------------------------------------------- */
/* main routine                                                               */
/* -------------------------------------------------------------------------- */
int main()
{
    AudioStream *_stream = NULL;
    MainUi *_mainUi = new MainUi();
    PlaylistWindow *_playlist = new PlaylistWindow();

    if (_mainUi->SetupGUI())
    {
        bool running = true;
        struct IntuiMessage *msg;
        _playerState = PFLAG_NON_INIT;
        struct Process *playerProc;
    
        struct TagItem playerTags[] = {
            {NP_Entry, (IPTR)PlayerTaskFunc},
            {NP_Name, (IPTR) "Audio_Engine"},
            {NP_Priority, 10},
            {NP_StackSize, 32768},
            {TAG_DONE, 0}};

        ULONG windowSig = _mainUi->GetWinSignal();
        ULONG pWindowSig = _playlist->GetWinSignal();
        while (running)
        {
            ULONG signals = Wait(windowSig | pWindowSig | SIGBREAKF_CTRL_C);
            if (signals & pWindowSig)
            {
                int16_t response = _playlist->HandleMessages();
                if (response == 0)
                {
                    //Check if flags are set
                    if(_playerState != PFLAG_NON_INIT)
                    {
                        setFlag(PFLAG_STOP);
                        removeFlag(PFLAG_INIT_DONE);
                        // wait for our task to be closed
                        while (!_playerState == PFLAG_NON_INIT)
                            Delay(2);
                        // remove stream only, task removed the audio
                        if (_stream)
                            delete _stream;
                        _stream = NULL;
                    }

                    std::string file = _playlist->selectedPath;
                    if (!file.empty())
                    {
                        if (strstr(file.c_str(), "http://"))
                            _stream = new NetworkStream();
                        else if (strstr(file.c_str(), ".mp3"))
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
                            // prepare audio task and start audio task
                            PlayerArgs g_args;
                            g_args.stream = _stream;
                            playerProc = (struct Process *)CreateNewProc(playerTags);
                            if (playerProc)
                                playerProc->pr_Task.tc_UserData = (APTR)&g_args;
                            // tell the audio to start
                            setFlag(PFLAG_INIT_DONE);
                            setFlag(PFLAG_PLAYING);
                            removeFlag(PFLAG_PAUSE);
                        }
                    }
                }
            }

            if (signals & windowSig)
            {
                while ((msg = GT_GetIMsg(_mainUi->GetWindow()->UserPort)))
                {
                    struct Gadget *gad = (struct Gadget *)msg->IAddress;
                    uint16_t msgCode = msg->Code;
                    GT_ReplyIMsg(msg);

                    if (msg->Class == IDCMP_CLOSEWINDOW)
                        running = false;
                    if (msg->Class == IDCMP_INTUITICKS)
                    {
                        if (hasFlag(PFLAG_INIT_DONE))
                        {
                            if (hasFlag(PFLAG_PLAYING) && !hasFlag(PFLAG_SEEK))
                            {
                                if (_stream->getCurrentSeconds() == 0 ||  _stream->getDuration() == 0)
                                    break;
                                _mainUi->UpdateTimeDisplay(_stream->getCurrentSeconds(), _stream->getDuration());
                                _mainUi->UpdateSeeker((int)((_stream->getCurrentSeconds() * 100) / _stream->getDuration()));
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
                        if (gad->GadgetID == ID_VOLUME)
                        {
                            if (hasFlag(PFLAG_INIT_DONE))
                            if(playerProc)
                            {
                                PlayerArgs *pb = (PlayerArgs *)playerProc->pr_Task.tc_UserData;
                                if (msgCode == 0) msgCode++;
                                pb->volumeLevel = ((msgCode*0x10000) / 100)-1;
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
                                _mainUi->UpdateTimeDisplay(seekTime, _stream->getDuration());
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
                            setFlag(PFLAG_PLAYING);
                            removeFlag(PFLAG_PAUSE);
                        }
                        else if (gad->GadgetID == ID_PAUSE)
                            toggleFlag(PFLAG_PAUSE);
                        else if (gad->GadgetID == ID_STOP)
                        {
                            setFlag(PFLAG_STOP);
                            removeFlag(PFLAG_PAUSE);
                        }
                        else if (gad->GadgetID == ID_OPEN)
                        {
                            //Check if flags are set
                            if(_playerState != PFLAG_NON_INIT)
                            {
                                setFlag(PFLAG_STOP);
                                removeFlag(PFLAG_INIT_DONE);
                                //wait for our task to be closed
                                while (!_playerState == PFLAG_NON_INIT)
                                    Delay(2);
                                //remove stream only, task removes the audio
                                if (_stream)
                                    delete _stream;
                                _stream = NULL;
                            }

                            std::string file = _mainUi->OpenFileRequest();
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
                                    // prepare audio task and start audio task
                                    PlayerArgs g_args;
                                    g_args.stream = _stream;
                                    playerProc = (struct Process *)CreateNewProc(playerTags);
                                    if (playerProc)
                                        playerProc->pr_Task.tc_UserData = (APTR)&g_args;
                                    //tell the audio to start
                                    setFlag(PFLAG_INIT_DONE);
                                }
                            }
                        }
                        else if (gad->GadgetID == ID_PLAYLIST)
                        {
                            if(_playlist->IsOpen())
                                _playlist->close();
                            else
                            {
                                _playlist->open();
                                pWindowSig = _playlist->GetWinSignal();
                            }
                        }
                    }
                }
            }
        }

        // Schluss jetzt
        if (_playlist)
            delete _playlist;
        
        _mainUi->CleanupGUI();
        delete _mainUi;
        printf("Cleabup\n");

        //wir spielen? Aber nicht mehr lange
        if (hasFlag(PFLAG_PLAYING))
            setFlag(PFLAG_STOP);

        //den Task beenden
        removeFlag(PFLAG_INIT_DONE);
        int timeout = 100;
        while ((!_playerState == PFLAG_NON_INIT) && timeout-- > 0)
            Delay(2);

        //stream löschen, der gehört zu uns
        if (_stream)
            delete _stream;
        printf("Cleabup done\n");
    }
    return 0;
}