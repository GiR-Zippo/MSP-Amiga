#include "gui.hpp"
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <stdio.h>
extern "C"
{
#include <proto/asl.h>
}

struct Window *win = NULL;
struct Gadget *gList = NULL;
struct Gadget *seekerGad = NULL;
struct Gadget *volGad = NULL;
struct Gadget *timeGad = NULL;
struct Library *AslBase = NULL;
void *visInfo = NULL;

static char timeBuffer[32] = "00:00 / 00:00";

void cleanupGUI()
{
    if (win)
        CloseWindow(win);
    if (gList)
        FreeGadgets(gList);
    if (visInfo)
        FreeVisualInfo(visInfo);
}

bool setupGUI()
{
    AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 37);
    if (!AslBase)
    {
        printf("Fehler: ASL nicht vorhanden\n");
    }

    struct Screen *scr = LockPubScreen(NULL);
    if (!scr)
        return false;

    visInfo = GetVisualInfo(scr, TAG_END);
    struct NewGadget ng;
    struct Gadget *context;

    context = CreateContext(&gList);

    // Gemeinsame Einstellungen für alle Gadgets
    ng.ng_VisualInfo = visInfo;
    ng.ng_TextAttr = NULL;
    ng.ng_Flags = 0;

    // Zeitanzeige
    ng.ng_LeftEdge = 20;
    ng.ng_TopEdge = 125;
    ng.ng_Width = 260;
    ng.ng_Height = 12;
    ng.ng_GadgetText = (CONST_STRPTR)"";
    ng.ng_GadgetID = ID_TIME_DISPLAY;
    timeGad = CreateGadget(TEXT_KIND, context, &ng, 
                        GTTX_Text, (Tag)timeBuffer,
                        GTTX_Border, TRUE,
                        TAG_END);

    // Der Seeker
    ng.ng_LeftEdge = 20;
    ng.ng_TopEdge = 140;
    ng.ng_Width = 260;
    ng.ng_Height = 12;
    ng.ng_GadgetText = (CONST_STRPTR)"";
    ng.ng_GadgetID = ID_SEEKER;
    seekerGad = CreateGadget(SLIDER_KIND, context, &ng,
                             GTSL_Min, 0,
                             GTSL_Max, 100,
                             GTSL_Level, 0,
                             GA_Immediate, TRUE,
                             GA_RelVerify, TRUE,
                             TAG_END);

    /*ng.ng_LeftEdge = 20;
    ng.ng_TopEdge = 30;
    ng.ng_Width = 16;
    ng.ng_Height = 80;
    ng.ng_GadgetText = (CONST_STRPTR)"Vol";
    ng.ng_GadgetID = ID_VOLUME;

    volGad = CreateGadget(SLIDER_KIND, context, &ng,
                        GTSL_Min, 0,
                        GTSL_Max, 64,       // 0 bis 64 ist typisch Amiga
                        GTSL_Level, 48,     // Startwert (75%)
                        GTSL_MaxLevelLen, 2,
                        GA_Immediate, TRUE, // Sofort reagieren beim Schieben
                        GA_RelVerify, TRUE,
                        TAG_END);
*/
    // Play Button
    ng.ng_LeftEdge = 20;
    ng.ng_TopEdge = 160;
    ng.ng_Width = 80;
    ng.ng_Height = 20;
    ng.ng_GadgetText = (CONST_STRPTR)"Play";
    ng.ng_GadgetID = ID_PLAY;
    context = CreateGadget(BUTTON_KIND, seekerGad, &ng, TAG_END);

    // Stop Button
    ng.ng_LeftEdge = 110;
    ng.ng_TopEdge = 160;
    ng.ng_Width = 80;
    ng.ng_Height = 20;
    ng.ng_GadgetText = (CONST_STRPTR)"Stop";
    ng.ng_GadgetID = ID_STOP;
    context = CreateGadget(BUTTON_KIND, context, &ng, TAG_END);

    // Stop Button
    ng.ng_LeftEdge = 200;
    ng.ng_TopEdge = 160;
    ng.ng_Width = 80;
    ng.ng_Height = 20;
    ng.ng_GadgetText = (CONST_STRPTR)"Open";
    ng.ng_GadgetID = ID_OPEN;
    context = CreateGadget(BUTTON_KIND, context, &ng, TAG_END);

    // Fenster öffnen
    win = OpenWindowTags(NULL,
                         WA_Title, (Tag) "My Shitty Player",
                         WA_Left, 50,
                         WA_Top, 50,
                         WA_Width, 300,
                         WA_Height, 190,
                         WA_IDCMP, IDCMP_GADGETUP | IDCMP_GADGETDOWN | IDCMP_MOUSEMOVE | IDCMP_INTUITICKS | IDCMP_CLOSEWINDOW,
                         WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE,
                         WA_PubScreen, (Tag)scr,
                         WA_Gadgets, (Tag)gList,
                         TAG_END);

    UnlockPubScreen(NULL, scr);

    if (!win)
        return false;

    GT_RefreshWindow(win, NULL);
    drawVideoPlaceholder();
//drawVolumeLevel(10);
    return true;
}

// Zeichnet das schwarze "Video"-Viereck
void drawVideoPlaceholder()
{
    if (!win)
        return;
    struct RastPort *rp = win->RPort;

    // Rahmen zeichnen
    SetAPen(rp, 1); // Meist Schwarz/Blau je nach Palette
    RectFill(rp, 20, 30, 280, 120);

    // Kleiner Text-Indikator
    SetAPen(rp, 2); // Weiß/Gelb
    Move(rp, 110, 80);
    Text(rp, "AAC Player", 10);
}

// Aktualisiert den Slider von außen
void updateSeeker(long percent)
{
    if (win && seekerGad)
    {
        GT_SetGadgetAttrs(seekerGad, win, NULL, GTSL_Level, percent, TAG_END);
    }
}

void drawVolumeLevel(long level) {
    if (!win) return;
    struct RastPort *rp = win->RPort;

    int x = 40; // Rechts neben dem Volume Slider
    int y_bottom = 110;
    int height = (level * 80) / 64; // Skaliert auf die Slider-Höhe

    // Hintergrund (Schwarz)
    SetAPen(rp, 1); 
    RectFill(rp, x, 30, x + 5, 110);

    // Pegel (z.B. Farbe 3 = Rot oder Grün)
    if (level > 0) {
        SetAPen(rp, (level > 50) ? 3 : 2); // Rot bei hoher Lautstärke
        RectFill(rp, x, y_bottom - height, x + 5, y_bottom);
    }
}

static void formatTimeOldschool(char* b, uint32_t s)
{
    uint32_t h = s / 3600;
    uint32_t m = (s % 3600) / 60;
    uint32_t sec = s % 60;

    if (h > 0)
        sprintf(b, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)sec);
    else
        sprintf(b, "%02lu:%02lu", (unsigned long)m, (unsigned long)sec);
}

void updateTimeDisplay(uint32_t lap, uint32_t total)
{
    if (!win || !timeGad) return;

    // Zeit berechnen
    uint32_t curSec  = lap;
    uint32_t totalSec =total;

    char lapBuf[16], durBuf[16];
    formatTimeOldschool(lapBuf, curSec);
    formatTimeOldschool(durBuf, totalSec);

    // Neuen String bauen
    sprintf(timeBuffer, "%s / %s", lapBuf, durBuf);

    // Gadget aktualisieren
    GT_SetGadgetAttrs(timeGad, win, NULL, GTTX_Text, (Tag)timeBuffer, TAG_DONE);
}

std::string openFileRequest()
{
    std::string fullPath = "";

    // 1. Erstelle den Requester
    struct FileRequester *fr = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, NULL);

    if (fr)
    {
        // 2. Dialog anzeigen
        // ASLFR_Title: Der Titel des Fensters
        // ASLFR_InitialDrawer: Wo der Dialog startet (z.B. "Work:")
        if (AslRequestTags(fr,
                           ASLFR_TitleText, (Tag) "Datei auswählen...",
                           ASLFR_InitialDrawer, (Tag) "RAM:",
                           ASLFR_DoPatterns, TRUE,               // Aktiviert das Pattern-Feld
                           ASLFR_InitialPattern, (Tag) "#?.(aac|m4a|flac|mp3|ogg)", // Das eigentliche Pattern
                           TAG_DONE))
        {
            // 3. Ergebnis zusammenbauen
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

        // 4. Speicher wieder freigeben
        FreeAslRequest(fr);
    }

    return fullPath;
}

ULONG getWinSignal()
{
    return 1L << win->UserPort->mp_SigBit;
}