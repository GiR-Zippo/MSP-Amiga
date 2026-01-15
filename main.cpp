#include "gui.hpp"
#include <proto/intuition.h>
#include <proto/gadtools.h>

#include "AHIPlayback.hpp"
#include "AudioStream.hpp"

#include "AACDecoder/AACStream.hpp"
#include "FLACDecoder/FlacStream.hpp"
#include "MP3Decoder/MP3Stream.hpp"
#include "VorbisDecoder/VorbisStream.hpp"
#include "M4ADecoder/M4ADecoder.hpp"

enum PlayerStates
{
    PFLAG_NON_INIT  = 0,         //Kein file, keine Kekse
    PFLAG_INIT_DONE = (1 << 0),  //File geladen, alles initialisiert
    PFLAG_PLAYING   = (1 << 1),  //Wir spielen
    PFLAG_STOP      = (1 << 2),  //Just stopped _playback and _stream exists
    PFLAG_SEEK      = (1 << 3),   //Wir seeken
    PFLAG_SEEK_DONE = (1 << 4),
};
uint16_t _playerState = PFLAG_NON_INIT;

bool hasFlag(PlayerStates state) { return (_playerState & state) != 0; }
void setFlag(PlayerStates state) { _playerState |= state; }
void removeFlag(PlayerStates state) { _playerState &= ~state; }

int main()
{
    AHIPlayback *_playback = NULL;
    AudioStream *_stream = NULL;
    uint32_t _seektime =0;
    if (setupGUI())
    {
        bool running = true;
        struct IntuiMessage *msg;
        _playerState = PFLAG_NON_INIT;
        while (running)
        {
            while ((msg = GT_GetIMsg(win->UserPort)))
            {
                struct Gadget *gad = (struct Gadget *)msg->IAddress;
                uint16 msgCode = msg->Code;
                GT_ReplyIMsg(msg);
                if (msg->Class == IDCMP_CLOSEWINDOW)
                    running = false;
                
                if (msg->Class ==IDCMP_INTUITICKS)
                {
                    if (hasFlag(PFLAG_INIT_DONE))
                    {
                        if (hasFlag(PFLAG_PLAYING) && !hasFlag(PFLAG_SEEK) )
                        {
                            updateTimeDisplay(_stream->getCurrentSeconds() , _stream->getDuration());
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
                            long newPercent = msgCode; 
                            uint32_t seekTime = (newPercent * _stream->getDuration()) / 100;
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
                            updateTimeDisplay(seekTime , _stream->getDuration());
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
                            _playback->Stop();

                        if (!hasFlag(PFLAG_NON_INIT))
                        {
                            if (_playback)
                                delete _playback;
                            if (_stream)
                                delete _stream;
                            _playback = NULL;
                            _stream = NULL;
                        }
                        _playerState = PFLAG_NON_INIT; //clear flags
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
                            }
                        }
                    }
                }
            }
            if (hasFlag(PFLAG_PLAYING) && !hasFlag(PFLAG_SEEK) )
            {
                if(!_playback->Update())
                {
                    _playback->Stop();
                    setFlag(PFLAG_STOP);
                }
            }

            if (hasFlag(PFLAG_SEEK_DONE))
            {
                if (_seektime > 0)
                {
                    _stream->seek(_seektime);
                    _seektime =0;
                    removeFlag(PFLAG_SEEK_DONE);
                    removeFlag(PFLAG_SEEK);
                }
            }

        }
        cleanupGUI();
        if (hasFlag(PFLAG_PLAYING))
            _playback->Stop();
        printf("Cleabup\n");
        if (_playback)
            delete _playback;
        if (_stream)
            delete _stream;
        printf("Cleabup done\n");
    }


    return 0;
}