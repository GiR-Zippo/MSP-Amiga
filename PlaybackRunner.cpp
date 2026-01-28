#include "PlaybackRunner.hpp"

#include "AACDecoder/AACStream.hpp"
#include "FLACDecoder/FlacStream.hpp"
#include "MP3Decoder/MP3Stream.hpp"
#include "VorbisDecoder/VorbisStream.hpp"
#include "M4ADecoder/M4ADecoder.hpp"
#include "WebRadioDecoder/StreamClient.hpp"

#include "Ui/gui.hpp"

PlaybackRunner::PlaybackRunner()
{
    m_PlayerState = PFLAG_NON_INIT;
    m_songEndSignal = AllocSignal(-1); // -1 sucht das nächste freie Bit
    if (m_songEndSignal == -1)
        printf("Sig Err\n");
    m_songEndMask = (1L << m_songEndSignal);
    m_stream = NULL;
}

void PlaybackRunner::Cleanup()
{
    // wir spielen? Aber nicht mehr lange
    if (hasFlag(PFLAG_PLAYING))
        setFlag(PFLAG_STOP);

    // den Task beenden
    removeFlag(PFLAG_INIT_DONE);
    int timeout = 100;
    while ((!m_PlayerState == PFLAG_NON_INIT) && timeout-- > 0) // warten...
        Delay(2);

    if (m_stream)
        delete m_stream;
    FreeSignal(m_songEndSignal);
}

void PlaybackRunner::SetVolume(uint16_t vol)
{
    if (m_playerProc)
    {
        PlayerArgs *pb = (PlayerArgs *)m_playerProc->pr_Task.tc_UserData;
        if (vol == 0)
            vol += 1;
        pb->volumeLevel = ((vol * 0x10000) / 100) - 1;
    }
}

bool PlaybackRunner::StartPlaybackTask(std::string file)
{
    struct TagItem playerTags[] = {
        {NP_Entry, (IPTR)PlayerTaskFunc},
        {NP_Name, (IPTR) "Audio_Engine"},
        {NP_Priority, 10},
        {NP_StackSize, 32768},
        {TAG_DONE, 0}};

    // Check if flags are set
    if (m_PlayerState != PFLAG_NON_INIT)
    {
        setFlag(PFLAG_STOP);
        removeFlag(PFLAG_INIT_DONE);
        // wait for our task to be closed
        while (!m_PlayerState == PFLAG_NON_INIT)
            Delay(5);
        // remove stream only, task removed the audio
        if (m_stream)
            delete m_stream;
        m_stream = NULL;
    }

    if (!file.empty())
    {
        if (strstr(file.c_str(), "http://"))
            m_stream = new NetworkStream();
        else if (strstr(file.c_str(), "https://"))
            m_stream = new NetworkStream();
        else if (strstr(file.c_str(), ".mp3"))
            m_stream = new MP3Stream();
        else if (strstr(file.c_str(), ".flac"))
            m_stream = new FlacStream();
        else if (strstr(file.c_str(), ".aac"))
            m_stream = new AACStream();
        else if (strstr(file.c_str(), ".ogg"))
            m_stream = new VorbisStream();
        else if (strstr(file.c_str(), ".m4a"))
            m_stream = new M4AStream();

        if (m_stream != NULL && m_stream->open(file.c_str()))
        {
            // prepare audio task and start audio task
            PlayerArgs *g_args = new PlayerArgs();
            g_args->stream = m_stream;
            g_args->songEndMask = m_songEndMask;
            g_args->mainTask = FindTask(NULL);

            g_args->volumeLevel = ((MainUi::getInstance().GetVolume() * 0x10000) / 100) - 1;
            m_playerProc = (struct Process *)CreateNewProc(playerTags);
            if (m_playerProc)
                m_playerProc->pr_Task.tc_UserData = (APTR)g_args;
            // tell the audio to start
            setFlag(PFLAG_INIT_DONE);
            setFlag(PFLAG_PLAYING);
            removeFlag(PFLAG_PAUSE);
        }
    }
    return true;
}

void PlaybackRunner::PlayerTaskFunc()
{
    // wart mal kurz
    Delay(1);
    struct Process *me = (struct Process *)FindTask(NULL);
    PlayerArgs *pb = (PlayerArgs *)me->pr_Task.tc_UserData;

    bool _audioInitDone = false;
    bool _updateWait = false;

    // no data
    if (!pb)
        return;

    // create the AHIPlayback in this scope
    AHIPlayback *playback = new AHIPlayback(pb->stream);
    while (PlaybackRunner::getInstance().hasFlag(PFLAG_INIT_DONE))
    {
        // sanitycheck
        if (!pb || !pb->stream)
            break;

        // die Laustärke hier setzen
        if (playback->GetVolume() != pb->volumeLevel)
            playback->SetVolume(pb->volumeLevel);

        // wir sollen spielen, sind nicht initialisiert?
        if (PlaybackRunner::getInstance().hasFlag(PFLAG_PLAYING) && !_audioInitDone)
        {
            playback->Init();
            _audioInitDone = true;
            PlaybackRunner::getInstance().removeFlag(PFLAG_STOP);
        }

        // wir sollen stoppen
        if (PlaybackRunner::getInstance().hasFlag(PFLAG_STOP))
        {
            if (!_updateWait && PlaybackRunner::getInstance().hasFlag(PFLAG_PLAYING))
            {
                PlaybackRunner::getInstance().removeFlag(PFLAG_PLAYING);
                playback->Stop();
                pb->stream->seek(0);
                _audioInitDone = false;
            }
        }

        // die Dudelroutine
        if (PlaybackRunner::getInstance().hasFlag(PFLAG_PLAYING) && 
            !PlaybackRunner::getInstance().hasFlag(PFLAG_SEEK) && 
            !PlaybackRunner::getInstance().hasFlag(PFLAG_PAUSE))
        {
            _updateWait = true;
            if (!playback->Update())
            {
                PlaybackRunner::getInstance().removeFlag(PFLAG_PLAYING);
                PlaybackRunner::getInstance().setFlag(PFLAG_STOP);
                playback->Stop();
                pb->stream->seek(0);
                printf("Task: Song beendet.\n");
                //raus hier
                break;
            }
            _updateWait = false;
        }
        else
            Delay(5); // gibt nix zu tun, also warte
    }

    // und weg damit
    delete playback;
    PlaybackRunner::getInstance().clearFlags();

    // gib mal der Playlist bescheid
    Signal(pb->mainTask, pb->songEndMask);
    delete pb;
    printf("Task: Beendet.\n");
}