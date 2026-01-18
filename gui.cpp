#include "gui.hpp"


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

static struct PlayerGadgetDef playerGadgets[] = {
    // Kind         X    Y    W    H    Label    ID               Tags
    { TEXT_KIND,    20, 125, 260, 14,   "",      ID_TIME_DISPLAY, timeTags },
    { SLIDER_KIND,  20, 142, 260, 12,   "",      ID_SEEKER,       seekerTags },
    { SLIDER_KIND, 285,  30,  10, 124,  "",      ID_VOLUME,       volTags },
    { BUTTON_KIND,  20, 160,  80,  20,  "Play",  ID_PLAY,         buttonTags },
    { BUTTON_KIND, 110, 160,  80,  20,  "Stop",  ID_STOP,         buttonTags },
    { BUTTON_KIND, 200, 160,  80,  20,  "Open",  ID_OPEN,         buttonTags }
};

static char timeBuffer[32] = "00:00 / 00:00";

MainUi::MainUi()
{
    m_win = NULL;
    m_gList = NULL;
    m_visInfo = NULL;
    m_AslBase = NULL;
}

void MainUi::CleanupGUI()
{
    if (m_win)
        CloseWindow(m_win);
    if (m_gList)
        FreeGadgets(m_gList);
    if (m_visInfo)
        FreeVisualInfo(m_visInfo);
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
        if (playerGadgets[i].id == ID_TIME_DISPLAY && m_gads[i])
            GT_SetGadgetAttrs(m_gads[i], m_win, NULL, GTTX_Text, (IPTR)timeBuffer, TAG_DONE);

        if (m_gads[i])
            context = m_gads[i];
    }

    // Fenster öffnen
    m_win = OpenWindowTags(NULL,
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

    if (!m_win)
        return false;

    GT_RefreshWindow(m_win, NULL);
    drawVideoPlaceholder();
drawVolumeLevel(30);
    return true;
}

// Zeichnet das schwarze "Video"-Viereck
void MainUi::drawVideoPlaceholder()
{
    if (!m_win)
        return;
    struct RastPort *rp = m_win->RPort;

    // Rahmen zeichnen
    SetAPen(rp, 1); // Meist Schwarz/Blau je nach Palette
    RectFill(rp, 20, 30, 280, 120);

    // Kleiner Text-Indikator
    SetAPen(rp, 2); // Weiß/Gelb
    Move(rp, 110, 80);
    Text(rp, "AAC Player", 10);
}

// Aktualisiert den Slider von außen
void MainUi::UpdateSeeker(long percent)
{
    if (m_win && m_gads[ID_SEEKER])
        GT_SetGadgetAttrs(m_gads[ID_SEEKER], m_win, NULL, GTSL_Level, percent, TAG_END);
}

void MainUi::drawVolumeLevel(long level) {
    return;
    //noch nicht
    if (!m_win) return;
    struct RastPort *rp = m_win->RPort;

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

void MainUi::UpdateTimeDisplay(uint32_t lap, uint32_t total)
{
    if (!m_win || !m_gads[ID_TIME_DISPLAY]) return;

    // Zeit berechnen
    uint32_t curSec  = lap;
    uint32_t totalSec =total;

    char lapBuf[16], durBuf[16];
    formatTimeOldschool(lapBuf, curSec);
    formatTimeOldschool(durBuf, totalSec);

    // Neuen String bauen
    sprintf(timeBuffer, "%s / %s", lapBuf, durBuf);

    // Gadget aktualisieren
    GT_SetGadgetAttrs(m_gads[ID_TIME_DISPLAY], m_win, NULL, GTTX_Text, (Tag)timeBuffer, TAG_DONE);
}

std::string MainUi::OpenFileRequest()
{
    std::string fullPath = "";
    struct FileRequester *fr = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, NULL);
    if (fr)
    {
        if (AslRequestTags(fr,
                           ASLFR_TitleText, (Tag) "Datei auswählen...",
                           ASLFR_InitialDrawer, (Tag) "RAM:",
                           ASLFR_DoPatterns, TRUE,               // Aktiviert das Pattern-Feld
                           ASLFR_InitialPattern, (Tag) "#?.(aac|m4a|flac|mp3|ogg)", // Das eigentliche Pattern
                           TAG_DONE))
        {
            std::string drawer = (char *)fr->fr_Drawer;
            std::string file = (char *)fr->fr_File;

            // Pfad-Logik (Prüfen ob Drawer mit '/' oder ':' endet)
            fullPath = drawer;
            if (!fullPath.empty())
            {
                char lastChar = fullPath[fullPath.length() - 1];
                if (lastChar != ':' && lastChar != '/')
                    fullPath += "/";
            }
            fullPath += file;
        }
        FreeAslRequest(fr);
    }
    return fullPath;
}