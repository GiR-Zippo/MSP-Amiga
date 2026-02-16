#include "PlaylistWindow.hpp"
#include "../SharedUiFunctions.hpp"
#include "PlaybackRunner.hpp"

PlaylistWindow* PlaylistWindow::instance = NULL;

struct GTMX_SpecialInfo
{
    uint16_t si_Active;
};

const char *playlistModi[] =
    {
        "Playlist",
        "Icecast",
        "iTunes Podcast (buggy)",
        NULL};

const char *playlistPModi[] =
    {
        "Nothing",
        "Single",
        "Repeat",
        "Shuffle",
        NULL};

static struct TagItem buttonTags[] = {
    {GTTX_Border, TRUE}, {TAG_DONE, 0}};

static struct GadgetDef playlistGadgets[] = {
    // Kind           X     Y       W       H       Label       ID                  Tags
    {CYCLE_KIND,     10,    20,     230,    20,     "Mode",     PLAYLIST_MODE,   buttonTags},
    {STRING_KIND,    10,    45,     230,    20,     "",         PLAYLIST_SEARCH, buttonTags},
    {LISTVIEW_KIND,  10,    70,     230,    200,    "",         PLAYLIST_LIST,   buttonTags},
    {BUTTON_KIND,    10,    265,    50,     20,     "Add",      PLAYLIST_ADD,    buttonTags},
    {BUTTON_KIND,    60,    265,    50,     20,     "Remove",   PLAYLIST_REMOVE, buttonTags},
    {BUTTON_KIND,    110,   265,    50,     20,     "Clear",    PLAYLIST_CLEAR,  buttonTags},
    {CYCLE_KIND,     160,   265,    80,     20,     "",         PLAYLIST_PMODE,  buttonTags},
};

PlaylistWindow::PlaylistWindow() : m_Window(NULL), m_GadgetList(NULL)
{
    NewList(&m_SongList); // Exec-Liste initialisieren
    NewList(&m_HiddenList);
    m_firstTime = true;
    m_opened = false;
    m_playlistInUse = false;
    m_allowNextSong = false;
    m_playlistMode = 0; // 0 = PlaylistMode
    m_playlistPMode = 0;
    m_SelectedIndex = -1;
    m_lastClickSeconds = 0;
    m_lastClickMicros = 0;
    m_searchBuffer[0] = '\0';
    srand(time(NULL));
}

bool PlaylistWindow::SetupGUI()
{
    struct Screen *scr = LockPubScreen(NULL);
    if (!scr)
        return false;

    m_VisInfo = GetVisualInfo(scr, TAG_END);
    if (!m_VisInfo)
    {
        UnlockPubScreen(NULL, scr);
        return false;
    }

    m_GadgetList = NULL;
    struct Gadget *context = CreateContext(&m_GadgetList);
    if (!context)
    {
        FreeVisualInfo(m_VisInfo);
        UnlockPubScreen(NULL, scr);
        m_VisInfo = NULL;
        m_GadgetList = NULL;
        return false;
    }

    struct NewGadget ng;
    ng.ng_VisualInfo = m_VisInfo;
    ng.ng_TextAttr = NULL;
    ng.ng_Flags = 0;
    for (int i = 0; i < PLAYLIST_MAX; i++)
    {
        ng.ng_LeftEdge = playlistGadgets[i].x;
        ng.ng_TopEdge = playlistGadgets[i].y;
        ng.ng_Width = playlistGadgets[i].w;
        ng.ng_Height = playlistGadgets[i].h;
        ng.ng_GadgetText = (CONST_STRPTR)playlistGadgets[i].label;
        ng.ng_GadgetID = playlistGadgets[i].id*1000;

        if (i == PLAYLIST_LIST) //playlist
        {
            m_Gads[i] = CreateGadget(playlistGadgets[i].kind, context, &ng,
                                     GTLV_Labels, (Tag)&m_SongList,
                                     GTLV_ShowSelected,  (ULONG)0,
                                     GTLV_ScrollWidth, 18L,
                                     TAG_DONE);
        }
        else if (i == PLAYLIST_SEARCH) // SuFu
        {
            m_Gads[i] = CreateGadget(playlistGadgets[i].kind, context, &ng,
                                     GTST_String, (Tag)m_searchBuffer, // Verknüpfung mit Puffer
                                     GTST_MaxChars, 63,           // Limit
                                     TAG_DONE);
        }
        else if (i == PLAYLIST_MODE) //Modus
        {
            m_Gads[i] = CreateGadget(playlistGadgets[i].kind, context, &ng,
                                     GTCY_Labels, (Tag)playlistModi,
                                     GTCY_Active, m_playlistMode, // Welcher Eintrag ist am Anfang aktiv?
                                     TAG_DONE);
        }
        else if (i == PLAYLIST_PMODE) //Modus
        {
            m_Gads[i] = CreateGadget(playlistGadgets[i].kind, context, &ng,
                                     GTCY_Labels, (Tag)playlistPModi,
                                     GTCY_Active, m_playlistPMode, // Welcher Eintrag ist am Anfang aktiv?
                                     TAG_DONE);
        }
        else
            m_Gads[i] = CreateGadgetA(playlistGadgets[i].kind, context, &ng, playlistGadgets[i].tags);

        if (m_Gads[i])
            context = m_Gads[i];
    }

    if (!m_Gads[PLAYLIST_LIST])
    {
        printf("Fehler: CreateGadget(LISTVIEW_KIND) liefert NULL!\n");
        FreeGadgets(m_GadgetList);
        FreeVisualInfo(m_VisInfo);
        UnlockPubScreen(NULL, scr);
        return false;
    }
    m_Window = OpenWindowTags(NULL,
                              WA_Left, 400L,
                              WA_Top, 100L,
                              WA_InnerWidth, 240L,
                              WA_InnerHeight, 280L,
                              WA_Title, (ULONG) "Playlist",
                              WA_Gadgets, (Tag)m_GadgetList, // Kontext-Kopf
                              WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP | IDCMP_MOUSEBUTTONS | IDCMP_INTUITICKS,
                              WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE,
                              WA_PubScreen, (Tag)scr,
                              TAG_DONE);

    UnlockPubScreen(NULL, scr);

    if (m_Window)
    {
        this->m_Port = m_Window->UserPort;
        GT_RefreshWindow(m_Window, NULL);
        m_opened = true;
        return true;
    }
    m_VisInfo = NULL;
    m_GadgetList = NULL;
    return false;
}

int16_t PlaylistWindow::UpdateUi()
{
    if (!m_Window)
        return -1;

    struct IntuiMessage *msg;
    while ((msg = GT_GetIMsg(m_Window->UserPort)))
    {
        struct Gadget *gad = (struct Gadget *)msg->IAddress;
        uint16_t msgCode = msg->Code;
        GT_ReplyIMsg(msg);

        if (msg->Class == IDCMP_CLOSEWINDOW)
            close();

        if (msg->Class == IDCMP_GADGETUP && gad->GadgetID == PLAYLIST_MODE*1000)
        {
            m_playlistMode = msgCode;
            if (msgCode == 0)
            {
                GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
                clearList();
                GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
            }
            else if (msgCode == 1)
                getIceCastList();
            else if (msgCode == 2)
                getiTunesList();
        }
        if (msg->Class == IDCMP_GADGETUP && gad->GadgetID == PLAYLIST_SEARCH*1000)
        {
            struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
            strcpy(m_searchBuffer, (const char*)si->Buffer);
            if (m_playlistMode == 0)
            {
                if (m_searchBuffer[0] == '\0')
                    showAll();
                else
                    filter(m_searchBuffer);
            }
            if (m_playlistMode == 1)
                getIceCastList();
            if (m_playlistMode == 2)
                getiTunesList();
        }
        if (msg->Class == IDCMP_GADGETUP && gad->GadgetID == PLAYLIST_LIST*1000)
        {
            int32_t selectedIndex = msgCode;
            if (SharedUiFunctions::DoubleCheck(m_lastClickSeconds, m_lastClickMicros, msg->Seconds, msg->Micros))
            {
                SetUsePlaylist(true);
                if (m_firstTime)
                {
                    m_firstTime = false;
                    m_allowNextSong = true;
                }
                else
                    m_allowNextSong = false;

                m_SelectedIndex = selectedIndex;
                if (m_playlistMode == 2)
                    getiTunesRSSList();
                else
                    PlayNext(true);
            }
            m_lastClickSeconds = msg->Seconds;
            m_lastClickMicros = msg->Micros;
        }
        if (msg->Class == IDCMP_GADGETUP && gad->GadgetID == PLAYLIST_ADD*1000) // Add
        {
            std::string filename = SharedUiFunctions::OpenFileRequest("#?.(aac|m4a|flac|mp3|ogg|m3u)");
            if (!filename.empty())
            {
                if (strstr(filename.c_str(), ".m3u"))
                    parseM3UPlaylist(filename.c_str());
                else
                    AddEntry(FilePart((STRPTR)filename.c_str()), filename);
            }
        }
        if (msg->Class == IDCMP_GADGETUP && gad->GadgetID == PLAYLIST_REMOVE*1000) // Remove
        {
            if (m_SelectedIndex != -1)
            {
                GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
                Node *n = findNode(m_SelectedIndex);
                if (!n)
                    return 0;
                Remove(n);
                m_SelectedIndex = -1;
                GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
            }
        }
        if (msg->Class == IDCMP_GADGETUP && gad->GadgetID == PLAYLIST_CLEAR*1000) // Clear
        {
            GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
            clearList();
            GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
        }
        if (msg->Class == IDCMP_GADGETUP && gad->GadgetID == PLAYLIST_PMODE*1000)
            m_playlistPMode = msgCode;
    }
    return -1;
}

void PlaylistWindow::CleanupGUI()
{
    clearList();
    close();
}


void PlaylistWindow::open()
{
    if (!m_Window)
        SetupGUI();
    else
    {
        WindowToFront(m_Window);
        ActivateWindow(m_Window);
    }
}

void PlaylistWindow::close()
{
    if (m_Window)
    {
        if ((ULONG)m_Window > 0x1000 && 
            (ULONG)m_Window < 0x80000000 &&
            TypeOfMem(m_Window))
        CloseWindow(m_Window);
        m_Window = NULL;
    }
    if (m_GadgetList)
    {
        if ((ULONG)m_GadgetList > 0x1000 && 
            (ULONG)m_GadgetList < 0x80000000 &&
            TypeOfMem(m_VisInfo))
        FreeGadgets(m_GadgetList);
        m_GadgetList = NULL;
        for (int i = 0; i < PLAYLIST_MAX; i++)
            m_Gads[i] = NULL;
    }
    if (m_VisInfo && m_opened)
    {
        if ((ULONG)m_VisInfo > 0x1000 && 
            (ULONG)m_VisInfo < 0x80000000 &&
            TypeOfMem(m_VisInfo))
        FreeVisualInfo(m_VisInfo);
        m_VisInfo = NULL;
    }
    m_opened = false;
}

void PlaylistWindow::PlayNext(bool noadvance)
{
    if (!m_playlistInUse)
        return;

    int count = 0;
    struct Node *node;

    for (node = m_SongList.lh_Head; node->ln_Succ; node = node->ln_Succ)
        count++;

    if (m_playlistPMode == 0)
        m_playlistInUse = false;
    // single repeat
    if (m_playlistPMode == 1)
        noadvance = true;
    // shuffle mode
    if (m_playlistPMode == 3)
        m_SelectedIndex = rand() % count-1;

    if (!noadvance)
    {
        if (m_SelectedIndex > count)
            m_SelectedIndex = 0;
        else
            m_SelectedIndex++;
    }

    node = findNode(m_SelectedIndex);
    if (!node)
        return;

    SongNode *sn = (SongNode *)node;
    strncpy(selectedPath, sn->path, 255);

    if (m_playlistPMode == 1)
        noadvance = false;

    if (m_Window && m_Gads[PLAYLIST_LIST])
    {
        GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL,
                          GTLV_Labels, (IPTR)&m_SongList,
                          GTLV_ShowSelected, (long unsigned int)m_SelectedIndex,
                          TAG_DONE);
    }
    PlaybackRunner::getInstance()->StartPlaybackTask(PlaylistWindow::getInstance()->selectedPath);
}

bool PlaylistWindow::getLineFomFile(BPTR file, std::string &line)
{
    line = "";
    char c;
    while (Read(file, &c, 1) > 0)
    {
        if (c == '\n')
            return true;
        if (c != '\r')
            line += c; // Windows \r ignorieren
    }
    return !line.empty();
}

void PlaylistWindow::parseM3UPlaylist(const char *filename)
{
    BPTR file = Open((STRPTR)filename, MODE_OLDFILE);
    if (file)
    {
        std::string line;
        std::string lastTitle = "Unbekannter Titel";

        while (getLineFomFile(file, line))
        {
            if (line.empty())
                continue;

            if (line.substr(0, 8) == "#EXTINF:")
            {
                size_t commaPos = line.find(',');
                if (commaPos != std::string::npos)
                {
                    lastTitle = line.substr(commaPos + 1);
                }
                continue;
            }

            if (line[0] != '#')
            {
                AddEntry((STRPTR)lastTitle.c_str(), line);
                lastTitle = "Unbekannter Titel";
            }
        }
        Close(file);
    }
}

