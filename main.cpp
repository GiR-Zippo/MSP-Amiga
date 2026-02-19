#include "Common.h"
#include "Shared/AmiSSL.hpp"
#include "Shared/Configuration.hpp"
#include "Ui/gui.hpp"
#include "Ui/Playlist/PlaylistWindow.hpp"
#include "PlaybackRunner.hpp"

/* -------------------------------------------------------------------------- */
/* main routine                                                               */
/* -------------------------------------------------------------------------- */
int main()
{
    // Gott weiß was AmiSSL macht, aber fummelt am Stack rum
    // Also einmalig Init hier
    AmiSSL *ssl = new AmiSSL();
    if (ssl->Init())
        ssl->Cleanup();
    delete ssl;

    sConfiguration->LoadConfig();

    if (!MainUi::getInstance()->SetupGUI())
        return -1;

    // --- Timer Setup Start ---
    struct MsgPort *timerPort = CreateMsgPort();
    struct timerequest *timerIO = (struct timerequest *)CreateIORequest(timerPort, sizeof(struct timerequest));
    bool timerOpen = (OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)timerIO, 0) == 0);

    auto StartTimer = [&](ULONG secs, ULONG micros)
    {
        timerIO->tr_node.io_Command = TR_ADDREQUEST;
        timerIO->tr_time.tv_secs = secs;
        timerIO->tr_time.tv_micro = micros;
        SendIO((struct IORequest *)timerIO);
    };

    if (timerOpen)
        StartTimer(0, 500000); // 500ms
    // --- Timer Setup Ende ---

    bool running = true;
    while (running)
    {
        // hier hin, da singals nicht komplett da sind, ja ich schaue zu dir PlaylistWindow
        ULONG windowSig = MainUi::getInstance()->GetWinSignal();
        ULONG pWindowSig = PlaylistWindow::getInstance()->GetWinSignal();
        ULONG pPlaybackSig = PlaybackRunner::getInstance()->GetSignal();
        ULONG timerSig = timerPort ? (1L << timerPort->mp_SigBit) : 0;

        ULONG signals = Wait(windowSig | pWindowSig | SIGBREAKF_CTRL_C | pPlaybackSig | timerSig);

        if (signals & timerSig)
        {
            MainUi::getInstance()->UpdateDisplayInformation();
            StartTimer(0, 500000);
        }

        if (signals & pPlaybackSig)
        {
            if (PlaylistWindow::getInstance()->GetAllowNextSong())
                PlaylistWindow::getInstance()->PlayNext();
            else
                PlaylistWindow::getInstance()->SetAllowNextSong();
        }
        // PlaybackWindow update
        if ((signals & pWindowSig) || (signals & pPlaybackSig))
            PlaylistWindow::getInstance()->UpdateUi();

        // MainUi update
        if (signals & windowSig)
        {
            if (!MainUi::getInstance()->UpdateUi())
                running = false;
        }
    }

    // --- Cleanup Timer ---
    if (timerOpen)
    {
        if (!CheckIO((struct IORequest *)timerIO)) AbortIO((struct IORequest *)timerIO);
        WaitIO((struct IORequest *)timerIO);
        CloseDevice((struct IORequest *)timerIO);
    }
    if (timerIO) DeleteIORequest((struct IORequest *)timerIO);
    if (timerPort) DeleteMsgPort(timerPort);

    printf("Cleanup: PlaybackRunner\n");
    PlaybackRunner::getInstance()->Cleanup();

    printf("Cleanup: Playlist\n");
    PlaylistWindow::getInstance()->CleanupGUI();

    printf("Cleanup: MainUi\n");
    MainUi::getInstance()->CleanupGUI();
    printf("Cleabup done\n");
    return 0;
}