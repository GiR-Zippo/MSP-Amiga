#include "Common.h"
#include "Shared/AmiSSL.hpp"
#include "Ui/gui.hpp"
#include "Ui/PlaylistWindow.hpp"
#include "PlaybackRunner.hpp"

/* -------------------------------------------------------------------------- */
/* main routine                                                               */
/* -------------------------------------------------------------------------- */
int main()
{
    // Gott weiß was AmiSSL macht, aber fummelt am Stack rum
    // Also einmalig Init hier
    AmiSSL *ssl = new AmiSSL();
    ssl->Init();
    ssl->Cleanup();
    delete ssl;

    if (!MainUi::getInstance()->SetupGUI())
        return -1;

    bool running = true;
    while (running)
    {
        // hier hin, da singals nicht komplett da sind, ja ich schaue zu dir PlaylistWindow
        ULONG windowSig = MainUi::getInstance()->GetWinSignal();
        ULONG pWindowSig = PlaylistWindow::getInstance()->GetWinSignal();
        ULONG pPlaybackSig = PlaybackRunner::getInstance()->GetSignal();

        ULONG signals = Wait(windowSig | pWindowSig | SIGBREAKF_CTRL_C | pPlaybackSig);

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

    printf("Cleanup: PlaybackRunner\n");
    PlaybackRunner::getInstance()->Cleanup();

    printf("Cleanup: Playlist\n");
    PlaylistWindow::getInstance()->CleanupGUI();

    printf("Cleanup: MainUi\n");
    MainUi::getInstance()->CleanupGUI();
    printf("Cleabup done\n");

    return 0;
}