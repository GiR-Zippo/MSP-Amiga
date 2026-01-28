#include "Common.h"
#include "Shared/AmiSSL.hpp"
#include "Ui/gui.hpp"
#include "Ui/PlaylistWindow.hpp"
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <cstring>

#include "Ui/SharedUiFunctions.hpp"
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

    if (!MainUi::getInstance().SetupGUI())
        return -1;

    bool running = true;
    while (running)
    {
        ULONG windowSig = MainUi::getInstance().GetWinSignal();
        ULONG pWindowSig = PlaylistWindow::getInstance().GetWinSignal();
        ULONG pPlaybackSig = PlaybackRunner::getInstance().GetSignal();

        ULONG signals = Wait(windowSig | pWindowSig | SIGBREAKF_CTRL_C | pPlaybackSig);
        if ((signals & pWindowSig) || (signals & PlaybackRunner::getInstance().GetMask()))
        {
            int16_t response = PlaylistWindow::getInstance().UpdateUi();
            // Spiel unser Lied
            if (signals & PlaybackRunner::getInstance().GetMask() && response == -1)
            {
                printf("Signal empfangen: Song zu Ende!\n");
                PlaylistWindow::getInstance().PlayNext();
                response = 0;
            }

            if (response == 0) // 0 == neuer Track ausgewählt
            {
                PlaybackRunner::getInstance().StartPlaybackTask(PlaylistWindow::getInstance().selectedPath);
            }
        }

        if (signals & windowSig)
        {
            if (!MainUi::getInstance().UpdateUi())
                running = false;
        }
    }

    printf("Cleanup: PlaybackRunner\n");
    PlaybackRunner::getInstance().Cleanup();

    printf("Cleanup: Playlist\n");
    PlaylistWindow::getInstance().CleanupGUI();

    printf("Cleanup: MainUi\n");
    MainUi::getInstance().CleanupGUI();
    printf("Cleabup done\n");

return 0;
}