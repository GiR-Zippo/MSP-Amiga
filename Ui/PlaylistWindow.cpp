#include "PlaylistWindow.hpp"
#include "../Shared/Icecast.hpp"
#include "SharedUiFunctions.hpp"
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
        NULL};

static struct TagItem buttonTags[] = {
    {GTTX_Border, TRUE}, {TAG_DONE, 0}};

static struct PlaylistGadgetDef playlistGadgets[] = {
    // Kind           X     Y       W       H       Label       ID                  Tags
    {CYCLE_KIND,     10,    20,     230,    20,     "Mode",     PLAYLIST_MODE,   buttonTags},
    {STRING_KIND,    10,    45,     230,    20,     "",         PLAYLIST_SEARCH, buttonTags},
    {LISTVIEW_KIND,  10,    70,     230,    200,    "",         PLAYLIST_LIST,   buttonTags},
    {BUTTON_KIND,    10,    265,    50,     20,     "Add",      PLAYLIST_ADD,    buttonTags},
    {BUTTON_KIND,    60,    265,    50,     20,     "Remove",   PLAYLIST_REMOVE, buttonTags},
    {BUTTON_KIND,    110,   265,    50,     20,     "Clear",    PLAYLIST_CLEAR,  buttonTags}
};

BOOL DoubleCheck(ULONG s1, ULONG m1, ULONG s2, ULONG m2)
{
    /* Wenn mehr als 1 Sekunde vergangen ist, ist es sicher kein Doppelklick */
    if (s2 - s1 > 1)
        return false;

    /* Wenn genau 1 Sekunde vergangen ist */
    if (s2 - s1 == 1)
    {
        if (m2 + (1000000 - m1) <= 400000)
            return true;
        return false;
    }
    if (m2 - m1 <= 400000)
        return true;
    return false;
}

PlaylistWindow::PlaylistWindow() : m_Window(NULL), m_GadgetList(NULL)
{
    NewList(&m_SongList); // Exec-Liste initialisieren
    NewList(&m_HiddenList);
    m_firstTime = true;
    m_opened = false;
    m_playlistInUse = false;
    m_allowNextSong = false;
    m_playlistMode = 0; // 0 = PlaylistMode
    m_SelectedIndex = -1;
    m_lastClickSeconds = 0;
    m_lastClickMicros = 0;
    m_searchBuffer[0] = '\0';
}

/// @brief Opens the window and init the gads
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
                                     GTLV_ShowSelected, NULL,
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
            printf("Suche jetzt nach: %s\n", playlistModi[msgCode]);
            GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
            clearList();
            m_playlistMode = msgCode;
            if (msgCode == 1)
            {
                Icecast *_icecast = new Icecast();
                _icecast->FetchList(m_SongList);
                delete _icecast;
            }
            GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
        }
        if (msg->Class == IDCMP_GADGETUP && gad->GadgetID == PLAYLIST_SEARCH*1000)
        {
            struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
            strcpy(m_searchBuffer, (const char*)si->Buffer);
            printf("Suche nach: %s\n", m_searchBuffer); 
            if (m_searchBuffer[0] == '\0')
                showAll();
            else
                filter(m_searchBuffer);
                
        }
        if (msg->Class == IDCMP_GADGETUP && gad->GadgetID == PLAYLIST_LIST*1000)
        {
            int32_t selectedIndex = msgCode;
            if (DoubleCheck(m_lastClickSeconds, m_lastClickMicros, msg->Seconds, msg->Micros))
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
                    parsePlaylist(filename.c_str());
                else
                    addEntry(FilePart((STRPTR)filename.c_str()), filename);
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
    }
    return -1;
}

void PlaylistWindow::CleanupGUI()
{
    close();
    clearList();
}

/// @brief Closes only the window
void PlaylistWindow::close()
{
    m_opened = false;
    if (m_Window)
    {
        CloseWindow(m_Window);
        m_Window = NULL;
    }
    if (m_GadgetList)
    {
        FreeGadgets(m_GadgetList);
        m_GadgetList = NULL;
    }
    if (m_VisInfo)
    {
        FreeVisualInfo(m_VisInfo);
        m_VisInfo = NULL;
    }
}

void PlaylistWindow::open()
{
    if (!m_Window)
        SetupGUI(); // Hier OpenWindow und CreateGadgets
    else {
        WindowToFront(m_Window);
        ActivateWindow(m_Window);
    }
}

void PlaylistWindow::addEntry(std::string name, std::string fullPath)
{
    //letzten Index ziehen
    int nextIndex = 0;
    struct SongNode *lastNode = (struct SongNode *)m_SongList.lh_TailPred;
    if (lastNode->node.ln_Pred)
        nextIndex = lastNode->OriginalIndex + 1;

    SongNode *sn = new SongNode;
    strncpy(sn->name, name.c_str(), sizeof(sn->name) - 1);
    sn->name[sizeof(sn->name) - 1] = '\0'; // Null-Terminierung sicherstellen
    sn->node.ln_Name = sn->name;
    sn->OriginalIndex = nextIndex;
    strncpy(sn->path, fullPath.c_str(), 255);
    sn->path[255] = '\0';

    AddTail(&m_SongList, (struct Node *)sn);

    if (m_Window && m_Gads[PLAYLIST_LIST])
    {
        GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)-1, TAG_DONE);
        GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL,
                          GTLV_Labels, (IPTR)&m_SongList,
                          GTLV_ShowSelected, NULL,
                          TAG_DONE);
    }
}

void PlaylistWindow::PlayNext(bool noadvance)
{
    if (!m_playlistInUse)
        return;

    int count = 0;
    struct Node *node;
    // Wir starten beim Kopf und wandern bis zum Ende (NULL)
    for (node = m_SongList.lh_Head; node->ln_Succ; node = node->ln_Succ)
        count++;

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

    if (m_Window && m_Gads[PLAYLIST_LIST])
    {
        GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL,
                          GTLV_Labels, (IPTR)&m_SongList,
                          GTLV_ShowSelected, (long unsigned int)m_SelectedIndex,
                          TAG_DONE);
    }
    PlaybackRunner::getInstance()->StartPlaybackTask(PlaylistWindow::getInstance()->selectedPath);
}

void PlaylistWindow::clearList()
{
    if (m_Window && m_Gads[PLAYLIST_LIST])
        GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);

    struct Node *n;
    while ((n = RemHead(&m_SongList)))
    {
        SongNode *sn = (SongNode *)n;
        if (sn->node.ln_Name)
            sn->node.ln_Name = NULL;
        delete sn;
    }
    while ((n = RemHead(&m_HiddenList)))
    {
        SongNode *sn = (SongNode *)n;
        if (sn->node.ln_Name)
            sn->node.ln_Name = NULL;
        delete sn;
    }
    NewList(&m_SongList);
    NewList(&m_HiddenList);
}

Node *PlaylistWindow::findNode(int index)
{
    if (index == -1)
        return NULL;

    struct Node *n = (struct Node *)m_SongList.lh_Head;
    // Durch die Liste bis zum Index wandern
    for (int i = 0; i < index && n->ln_Succ; i++)
        n = n->ln_Succ;

    if (n && n->ln_Succ) // ln_Succ Check stellt sicher, dass es nicht der Tail-Wächter ist
        return n;
    return NULL;
}

bool PlaylistWindow::getLine(BPTR file, std::string &line)
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

void PlaylistWindow::parsePlaylist(const char *filename)
{
    BPTR file = Open((STRPTR)filename, MODE_OLDFILE);
    if (file)
    {
        std::string line;
        std::string lastTitle = "Unbekannter Titel";

        while (getLine(file, line))
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
                addEntry((STRPTR)lastTitle.c_str(), line);
                lastTitle = "Unbekannter Titel";
            }
        }
        Close(file);
    }
}

void PlaylistWindow::filter(const char *searchCrit)
{
    struct SongNode *node;
    struct List tmpList;
    NewList(&tmpList); // Temporärer Container

    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);

    while ((node = (struct SongNode *)RemHead(&m_SongList)))   AddTail(&tmpList, (struct Node *)node);
    while ((node = (struct SongNode *)RemHead(&m_HiddenList))) AddTail(&tmpList, (struct Node *)node);

    bool isSearchEmpty = (!searchCrit || searchCrit[0] == '\0');
    while ((node = (struct SongNode *)RemHead(&tmpList)))
    {
        bool visible = isSearchEmpty ? TRUE : containsString(node->name, searchCrit);
        node->Visible = visible;

        if (visible)
            AddTail(&m_SongList, (struct Node *)node);
        else
            AddTail(&m_HiddenList, (struct Node *)node);
    }
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
}

void PlaylistWindow::sortPlaylist()
{
    struct SongNode *node;
    BOOL swapped;

    if (!m_SongList.lh_Head->ln_Succ->ln_Succ) return;

    do {
        swapped = FALSE;
        node = (struct SongNode *)m_SongList.lh_Head;

        // Solange ein "echter" Nachfolger da ist
        while (node->node.ln_Succ->ln_Succ)
        {
            struct SongNode *nextNode = (struct SongNode *)node->node.ln_Succ;

            if (node->OriginalIndex > nextNode->OriginalIndex)
            {
                Remove((struct Node *)nextNode);
                Insert((struct List *)&m_SongList, (struct Node *)nextNode, node->node.ln_Pred);
                swapped = TRUE;
            }
            else
                node = nextNode;
        }
    } while (swapped);
}

void PlaylistWindow::showAll()
{
    filter(""); 
    sortPlaylist();
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, 
        GTLV_Labels, (IPTR)&m_SongList, 
        TAG_DONE);
}