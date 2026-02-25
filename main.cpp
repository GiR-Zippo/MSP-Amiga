#include "Common.h"
#include "Shared/AmiSSL.hpp"
#include "Shared/Configuration.hpp"
#include "Ui/gui.hpp"
#include "Ui/Playlist/PlaylistWindow.hpp"
#include "Ui/Settings/SettingsUi.hpp"
#include "arexx/arexx.hpp"
#include "PlaybackRunner.hpp"

/// @brief Helper for our timer
void StartTimer(struct timerequest *timerIO, ULONG secs, ULONG micros)
{
    timerIO->tr_node.io_Command = TR_ADDREQUEST;
    timerIO->tr_time.tv_secs    = secs;
    timerIO->tr_time.tv_micro   = micros;
    SendIO((struct IORequest *)timerIO);
}

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

    if (!sArexx->Init())
        DLog("Error init Arexx\n");

    // --- Timer Setup Start ---
    struct MsgPort *timerPort = CreateMsgPort();
    struct timerequest *timerIO = (struct timerequest *)CreateIORequest(timerPort, sizeof(struct timerequest));
    bool timerOpen = (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest *)timerIO, 0) == 0);

    if (timerOpen)
        StartTimer(timerIO, 0, 500000); // 500ms
    // --- Timer Setup Ende ---

    bool running = true;
    while (running)
    {
        // hier hin, da singals nicht komplett da sind, ja ich schaue zu dir PlaylistWindow
        ULONG windowSig = MainUi::getInstance()->GetWinSignal();
        ULONG pWindowSig = PlaylistWindow::getInstance()->GetWinSignal();
        ULONG pPlaybackSig = PlaybackRunner::getInstance()->GetSignal();
        ULONG pSettings = sSettingsUi->GetWinSignal();
        ULONG pArexxSig = sArexx->GetSignal();
        ULONG timerSig = timerPort ? (1L << timerPort->mp_SigBit) : 0;
        ULONG mainDnDSig = MainUi::getInstance()->GetDnDSignal();
        ULONG playlistDnDSig = PlaylistWindow::getInstance()->GetDnDSignal();

        ULONG signals = Wait(windowSig | pWindowSig | SIGBREAKF_CTRL_C | pPlaybackSig | timerSig | pArexxSig | pSettings | mainDnDSig | playlistDnDSig);

        if (signals & mainDnDSig)
            MainUi::getInstance()->UpdateDragNDrop();

        if (signals & playlistDnDSig)
            PlaylistWindow::getInstance()->UpdateDragNDrop();

        if (signals & timerSig)
        {
            MainUi::getInstance()->UpdateDisplayInformation();
            StartTimer(timerIO, 0, 500000);
        }

        if (signals & pPlaybackSig)
        {
            if (PlaylistWindow::getInstance()->GetAllowNextSong())
                PlaylistWindow::getInstance()->PlayNext();
            else
                PlaylistWindow::getInstance()->SetAllowNextSong();
        }
        //Arexx update
        if (signals & pArexxSig)
            sArexx->Update();

        // PlaybackWindow update
        if ((signals & pWindowSig) || (signals & pPlaybackSig))
            PlaylistWindow::getInstance()->UpdateUi();

        // PlaybackWindow update
        if (signals & pSettings)
            sSettingsUi->UpdateUi();

        // MainUi update
        if (signals & windowSig)
        {
            if (!MainUi::getInstance()->UpdateUi())
                running = false;
        }
    }

    if (timerOpen)
    {
        if (!CheckIO((struct IORequest *)timerIO)) AbortIO((struct IORequest *)timerIO);
        WaitIO((struct IORequest *)timerIO);
        CloseDevice((struct IORequest *)timerIO);
    }
    if (timerIO) DeleteIORequest((struct IORequest *)timerIO);
    if (timerPort) DeleteMsgPort(timerPort);

    DLog("Cleanup: Arexx\n");
    sArexx->Cleanup();

    DLog("Cleanup: Settings\n");
    sSettingsUi->CloseGUI();

    DLog("Cleanup: PlaybackRunner\n");
    PlaybackRunner::getInstance()->Cleanup();

    DLog("Cleanup: Playlist\n");
    PlaylistWindow::getInstance()->CleanupGUI();

    DLog("Cleanup: MainUi\n");
    MainUi::getInstance()->CleanupGUI();
    DLog("Cleabup done\n");
    return 0;
}