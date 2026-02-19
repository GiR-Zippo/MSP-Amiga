#include "gui.hpp"
#include "../PlaybackRunner.hpp"
#include "SharedUiFunctions.hpp"
#include "Playlist/PlaylistWindow.hpp"

MainUi* MainUi::instance = NULL;

static struct TagItem seekerTags[] = {
    {GTSL_Min, 0}, {GTSL_Max, 100}, {GTSL_Level, 0}, {GA_Immediate, TRUE}, {GA_RelVerify, TRUE}, {TAG_DONE, 0}
};

static struct TagItem volTags[] = { 
    {GTSL_Min, 0}, {GTSL_Max, 100}, {GTSL_Level, 100}, {PGA_Freedom, LORIENT_VERT}, {GA_Immediate, TRUE}, {GA_RelVerify, TRUE}, {TAG_DONE, 0}
};

static struct TagItem timeTags[] = {
    {GTTX_Border, TRUE}, {GTTX_Text, (IPTR)""}, {TAG_DONE, 0}
};

static struct TagItem buttonTags[] = {
    {GTTX_Border, TRUE}, {TAG_DONE, 0}
};

static struct GadgetDef playerGadgets[] = {
    // Kind         X    Y    W    H    Label    ID               Tags
    { SLIDER_KIND,  20, 142, 260, 12,   "",      ID_SEEKER,       seekerTags },
    { SLIDER_KIND, 285,  30,  10, 124,  "",      ID_VOLUME,       volTags },
    { BUTTON_KIND,  20, 160,  50,  20,  "Play",  ID_PLAY,         buttonTags },
    { BUTTON_KIND,  70, 160,  50,  20,  "Pause", ID_PAUSE,        buttonTags },
    { BUTTON_KIND, 120, 160,  50,  20,  "Stop",  ID_STOP,         buttonTags },
    { BUTTON_KIND, 170, 160,  50,  20,  "Open",  ID_OPEN,         buttonTags },
    { BUTTON_KIND, 227, 160,  50,  20,  "PList", ID_PLAYLIST,     buttonTags },
    { TEXT_KIND,    20, 125, 260, 14,   "",      ID_TIME_DISPLAY, timeTags }
};

static char timeBuffer[32] = "00:00 / 00:00";

MainUi::MainUi()
{
    m_Window = NULL;
    m_gList = NULL;
    m_visInfo = NULL;
    m_AslBase = NULL;
    m_VolumeLevel = 70;
}

bool MainUi::SetupGUI()
{
    m_AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 37);
    if (!m_AslBase)
    {
        printf("Fehler: ASL nicht vorhanden\n");
        return false;
    }

    struct Screen *scr = LockPubScreen(NULL);
    if (!scr)
        return false;

    m_visInfo = GetVisualInfo(scr, TAG_END);
    struct NewGadget ng;
    struct Gadget *context;

    context = CreateContext(&m_gList);

    // Gemeinsame Einstellungen für alle Gadgets
    ng.ng_VisualInfo = m_visInfo;
    ng.ng_TextAttr = NULL;
    ng.ng_Flags = 0;

    //lese gads ein und baue sie
    for (int i = 0; i < PLAYER_GADS_COUNT; i++)
    {
        ng.ng_LeftEdge   = playerGadgets[i].x;
        ng.ng_TopEdge    = playerGadgets[i].y;
        ng.ng_Width      = playerGadgets[i].w;
        ng.ng_Height     = playerGadgets[i].h;
        ng.ng_GadgetText = (CONST_STRPTR)playerGadgets[i].label;
        ng.ng_GadgetID   = playerGadgets[i].id;
        ng.ng_VisualInfo = m_visInfo;

        m_gads[i] = CreateGadgetA(playerGadgets[i].kind, context, &ng, playerGadgets[i].tags);

        //timedisplay will ne extrawurst
        if (playerGadgets[i].id == ID_SEEKER && m_gads[i])
            GT_SetGadgetAttrs(m_gads[i], m_Window, NULL, GTSL_Level, 0, TAG_END);
        if (playerGadgets[i].id == ID_VOLUME && m_gads[i])
            GT_SetGadgetAttrs(m_gads[i], m_Window, NULL, GTSL_Level, (IPTR)m_VolumeLevel, TAG_END);
        if (playerGadgets[i].id == ID_TIME_DISPLAY && m_gads[i])
            GT_SetGadgetAttrs(m_gads[i], m_Window, NULL, GTTX_Text, (IPTR)timeBuffer, TAG_DONE);

        if (m_gads[i])
            context = m_gads[i];
    }

    // Fenster öffnen
    m_Window = OpenWindowTags(NULL,
                         WA_Title, (Tag) "My Shitty Player",
                         WA_Left, 50,
                         WA_Top, 50,
                         WA_Width, 300,
                         WA_Height, 190,
                         WA_IDCMP, IDCMP_GADGETUP | IDCMP_GADGETDOWN | IDCMP_MOUSEMOVE | IDCMP_INTUITICKS | IDCMP_CLOSEWINDOW,
                         WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE,
                         WA_PubScreen, (Tag)scr,
                         WA_Gadgets, (Tag)m_gList,
                         TAG_END);

    UnlockPubScreen(NULL, scr);

    if (!m_Window)
        return false;

    GT_RefreshWindow(m_Window, NULL);
    drawVideoPlaceholder();
    return true;
}

bool MainUi::UpdateUi()
{
    struct IntuiMessage *msg;
    while ((msg = GT_GetIMsg(m_Window->UserPort)))
    {
        struct Gadget *gad = (struct Gadget *)msg->IAddress;
        uint16_t msgCode = msg->Code;
        GT_ReplyIMsg(msg);

        if (msg->Class == IDCMP_CLOSEWINDOW)
            return false;
        //if (msg->Class == IDCMP_INTUITICKS) Nutze es wenn wir es wieder brauchen

        if (msg->Class == IDCMP_GADGETUP)
        {
            if (gad->GadgetID == ID_SEEKER)
            {
                if (PlaybackRunner::getInstance()->hasFlag(PFLAG_INIT_DONE))
                {
                    uint32_t seekTime = ((uint32_t)msgCode * PlaybackRunner::getInstance()->GetStream()->getDuration()) / 100;
                    PlaybackRunner::getInstance()->GetStream()->seek(seekTime);
                    PlaybackRunner::getInstance()->removeFlag(PFLAG_SEEK);
                }
            }
            if (gad->GadgetID == ID_VOLUME)
            {
                if (PlaybackRunner::getInstance()->hasFlag(PFLAG_INIT_DONE))
                {
                    PlaybackRunner::getInstance()->SetVolume(msgCode);
                    SetVolume(msgCode);
                }
            }
        }

        if (msg->Class == IDCMP_MOUSEMOVE)
        {
            if (gad->GadgetID == ID_SEEKER)
            {
                if (PlaybackRunner::getInstance()->hasFlag(PFLAG_INIT_DONE))
                {
                    uint32_t duration = PlaybackRunner::getInstance()->GetStream()->getDuration();
                    uint32_t seekTime = (msgCode * duration) / 100;
                    UpdateTimeDisplay(seekTime, duration);
                }
            }
        }
        if (msg->Class == IDCMP_GADGETDOWN)
        {
            if (gad->GadgetID == ID_SEEKER)
                PlaybackRunner::getInstance()->setFlag(PFLAG_SEEK);
        }

        if (msg->Class == IDCMP_GADGETUP)
        {
            struct Gadget *gad = (struct Gadget *)msg->IAddress;
            if (gad->GadgetID == ID_PLAY)
            {
                PlaybackRunner::getInstance()->setFlag(PFLAG_PLAYING);
                PlaybackRunner::getInstance()->removeFlag(PFLAG_PAUSE);
            }
            else if (gad->GadgetID == ID_PAUSE)
                PlaybackRunner::getInstance()->toggleFlag(PFLAG_PAUSE);
            else if (gad->GadgetID == ID_STOP)
            {
                PlaybackRunner::getInstance()->setFlag(PFLAG_STOP);
                PlaybackRunner::getInstance()->removeFlag(PFLAG_PAUSE);
            }
            else if (gad->GadgetID == ID_OPEN)
            {
                // keine playlist mehr
                PlaylistWindow::getInstance()->SetUsePlaylist(false);
                PlaybackRunner::getInstance()->StartPlaybackTask(SharedUiFunctions::OpenFileRequest("#?.(aac|m4a|flac|mp3|ogg|mid)"));
            }
            else if (gad->GadgetID == ID_PLAYLIST)
            {
                if (PlaylistWindow::getInstance()->IsOpen())
                    PlaylistWindow::getInstance()->close();
                else
                    PlaylistWindow::getInstance()->open();
            }
        }
    }
    return true;
}

void MainUi::CleanupGUI()
{
    if (m_Window)
        CloseWindow(m_Window);
    if (m_gList)
        FreeGadgets(m_gList);
    if (m_visInfo)
        FreeVisualInfo(m_visInfo);
}

void MainUi::UpdateSeeker(long percent)
{
    if (m_Window && m_gads[ID_SEEKER])
        GT_SetGadgetAttrs(m_gads[ID_SEEKER], m_Window, NULL, GTSL_Level, percent, TAG_END);
}

void MainUi::UpdateTimeDisplay(uint32_t lap, uint32_t total)
{
    if (!m_Window || !m_gads[ID_TIME_DISPLAY]) return;

    // Zeit berechnen
    uint32_t curSec  = lap;
    uint32_t totalSec =total;

    char lapBuf[16], durBuf[16];
    formatTimeOldschool(lapBuf, curSec);
    formatTimeOldschool(durBuf, totalSec);

    sprintf(timeBuffer, "%s / %s", lapBuf, durBuf);
    GT_SetGadgetAttrs(m_gads[ID_TIME_DISPLAY], m_Window, NULL, GTTX_Text, (Tag)timeBuffer, TAG_DONE);
}

void MainUi::UpdateDisplayInformation()
{
    if (!m_Window)
        return;

    if (PlaybackRunner::getInstance()->hasFlag(PFLAG_INIT_DONE))
    {
        if (PlaybackRunner::getInstance()->hasFlag(PFLAG_PLAYING) &&
            !PlaybackRunner::getInstance()->hasFlag(PFLAG_SEEK))
        {
            uint32_t currSec = PlaybackRunner::getInstance()->GetStream()->getCurrentSeconds();
            uint32_t duration = PlaybackRunner::getInstance()->GetStream()->getDuration();
            if (currSec == 0 || duration == 0)
                UpdateTimeDisplay(currSec, duration);
            else
            {
                UpdateTimeDisplay(currSec, duration);
                UpdateSeeker((int)((currSec * 100) / duration));
            }
        }
    }

    if (PlaybackRunner::getInstance()->GetStream() != NULL)
        drawVideoPlaceholder(PlaybackRunner::getInstance()->GetStream()->getTitle(), PlaybackRunner::getInstance()->GetStream()->getArtist());

}

void MainUi::drawVideoPlaceholder()
{
    if (!m_Window)
        return;
    struct RastPort *rp = m_Window->RPort;

    // Rahmen zeichnen
    SetAPen(rp, 1); // Meist Schwarz/Blau je nach Palette
    RectFill(rp, 20, 30, 280, 120);

    // Kleiner Text-Indikator
    SetAPen(rp, 2); // Weiß/Gelb
    Move(rp, 110, 80);
    Text(rp, "AAC Player", 10);
}

void MainUi::drawVideoPlaceholder(const char *title, const char *artist)
{
    if (!m_Window) return;

    struct RastPort *rp = m_Window->RPort;

    int x1 = 20, y1 = 30;
    int x2 = 280, y2 = 120;
    int boxWidth = x2 - x1;

    // 1. Hintergrund füllen (Löschen)
    SetAPen(rp, 1); 
    RectFill(rp, x1, y1, x2, y2);

    // 2. Textfarbe
    SetAPen(rp, 2); 

    // Aufruf der Hilfsmethode (Ganz ohne auto/Lambda)
    drawCenteredText(rp, title, x1, boxWidth, 75);
    drawCenteredText(rp, artist, x1, boxWidth, 100);

    // 3. Rahmen
    SetAPen(rp, 3);
    Move(rp, x1, y1);
    Draw(rp, x2, y1);
    Draw(rp, x2, y2);
    Draw(rp, x1, y2);
    Draw(rp, x1, y1);
}

void MainUi::drawCenteredText(struct RastPort *rp, const char *text, int x1, int boxWidth, int yPos)
{
    if (text && text[0] != '\0') {
        // HIER: Die tatsächliche Länge des Strings ermitteln
        int len = strlen(text); 
        
        // TextLength braucht die Länge, um die Pixelbreite zu berechnen
        int pWidth = TextLength(rp, (CONST_STRPTR)text, len);

        int xPos = x1 + (boxWidth - pWidth) / 2;
        if (xPos < x1 + 5) xPos = x1 + 5; 

        Move(rp, xPos, yPos);
        
        // HIER: Auch Text() muss 'len' bekommen, nicht eine feste Zahl!
        Text(rp, (CONST_STRPTR)text, len); 
    }
}

void MainUi::formatTimeOldschool(char* b, uint32_t s)
{
    uint32_t h = s / 3600;
    uint32_t m = (s % 3600) / 60;
    uint32_t sec = s % 60;

    if (h > 0)
        sprintf(b, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)sec);
    else
        sprintf(b, "%02lu:%02lu", (unsigned long)m, (unsigned long)sec);
}

//Unused atm
void MainUi::drawVolumeLevel(long level) {
    return;
    //noch nicht
    if (!m_Window) return;
    struct RastPort *rp = m_Window->RPort;

    int x = 287; // Rechts neben dem Volume Slider
    int y_bottom = 123;
    int height = (level * 80) / 64; // Skaliert auf die Slider-Höhe

    // Hintergrund (Schwarz)
    SetAPen(rp, 1); 
    RectFill(rp, x, 287, x + 5, 123);

    // Pegel (z.B. Farbe 3 = Rot oder Grün)
    if (level > 0) {
        SetAPen(rp, (level > 50) ? 3 : 2); // Rot bei hoher Lautstärke
        RectFill(rp, x, y_bottom - height, x + 5, y_bottom);
    }
}