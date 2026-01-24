#include "PlaylistWindow.hpp"
#include <proto/alib.h>
#include <proto/intuition.h>

static struct TagItem buttonTags[] = {
    {GTTX_Border, TRUE}, {TAG_DONE, 0}};

static struct PlaylistGadgetDef playlistGadgets[] = {
    // Kind           X    Y    W    H    Label    ID               Tags
    {LISTVIEW_KIND, 10, 30, 230, 200, "Songs", 1000, buttonTags},
    {BUTTON_KIND, 10, 225, 50, 20, "Add", 1001, buttonTags},
    {BUTTON_KIND, 60, 225, 50, 20, "Remove", 1002, buttonTags},
    {BUTTON_KIND, 110, 225, 50, 20, "Clear", 1003, buttonTags}};

PlaylistWindow::PlaylistWindow() : m_Window(NULL), m_GadgetList(NULL)
{
    NewList(&m_SongList); // Exec-Liste initialisieren
    songSelected = false;
    m_opened = false;
    m_SelectedIndex = -1;
    m_lastClickSeconds = 0;
    m_lastClickMicros = 0;
}

PlaylistWindow::~PlaylistWindow()
{
    close();
    clearList();
}

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

bool PlaylistWindow::open()
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
    for (int i = 0; i < 4; i++)
    {
        ng.ng_LeftEdge = playlistGadgets[i].x;
        ng.ng_TopEdge = playlistGadgets[i].y;
        ng.ng_Width = playlistGadgets[i].w;
        ng.ng_Height = playlistGadgets[i].h;
        ng.ng_GadgetText = (CONST_STRPTR)playlistGadgets[i].label;
        ng.ng_GadgetID = playlistGadgets[i].id;

        if (i == 0)
        {
            m_Gads[i] = CreateGadget(playlistGadgets[i].kind, context, &ng,
                                     GTLV_Labels, (Tag)&m_SongList,
                                     GTLV_ShowSelected, NULL,
                                     GTLV_ScrollWidth, 18L,
                                     TAG_DONE);
        }
        else
            m_Gads[i] = CreateGadgetA(playlistGadgets[i].kind, context, &ng, playlistGadgets[i].tags);

        if (m_Gads[i])
            context = m_Gads[i];
    }

    if (!m_Gads[0])
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
                              WA_InnerWidth, 250L,
                              WA_InnerHeight, 230L,
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

void PlaylistWindow::addEntry(std::string name, std::string fullPath)
{
    SongNode *sn = new SongNode;
    strncpy(sn->name, name.c_str(), sizeof(sn->name) - 1);
    sn->name[sizeof(sn->name) - 1] = '\0'; // Null-Terminierung sicherstellen
    sn->node.ln_Name = sn->name;
    strncpy(sn->path, fullPath.c_str(), 255);
    sn->path[255] = '\0';

    AddTail(&m_SongList, (struct Node *)sn);

    if (m_Window && m_Gads[0])
    {
        GT_SetGadgetAttrs(m_Gads[0], m_Window, NULL, GTLV_Labels, (IPTR)-1, TAG_DONE);
        GT_SetGadgetAttrs(m_Gads[0], m_Window, NULL,
                          GTLV_Labels, (IPTR)&m_SongList,
                          GTLV_ShowSelected, NULL,
                          TAG_DONE);
    }
}

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

int16_t PlaylistWindow::HandleMessages()
{
    if (!m_Window)
        return -1;

    struct IntuiMessage *msg;
    while ((msg = GT_GetIMsg(m_Port)))
    {
        uint32_t classMsg = msg->Class;
        uint16_t code = msg->Code;

        struct Gadget *gad = (struct Gadget *)msg->IAddress;

        GT_ReplyIMsg(msg);

        if (classMsg == IDCMP_CLOSEWINDOW)
            close();

        if (classMsg == IDCMP_GADGETUP && gad->GadgetID == 1000)
        {
            int32_t selectedIndex = code;
            if (DoubleCheck(m_lastClickSeconds, m_lastClickMicros, msg->Seconds, msg->Micros))
            {
                Node *n = findNode(selectedIndex);
                if (!n)
                    return 0;
                m_SelectedIndex = selectedIndex;
                SongNode *sn = (SongNode *)n;
                strncpy(selectedPath, sn->path, 255);
                return 0;
            }
            m_lastClickSeconds = msg->Seconds;
            m_lastClickMicros = msg->Micros;
        }
        if (classMsg == IDCMP_GADGETUP && gad->GadgetID == 1001) // Add
        {
            std::string filename = openFileRequest();
            if (!filename.empty())
            {
                if (strstr(filename.c_str(), ".m3u"))   
                    parsePlaylist(filename.c_str());
                else
                    addEntry(FilePart((STRPTR)filename.c_str()), filename);
            }
        }
        if (classMsg == IDCMP_GADGETUP && gad->GadgetID == 1002) // Remove
        {
            if (m_SelectedIndex != -1)
            {
                GT_SetGadgetAttrs(m_Gads[0], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
                Node *n = findNode(m_SelectedIndex);
                if (!n)
                    return 0;
                Remove(n);
                m_SelectedIndex = -1;
                GT_SetGadgetAttrs(m_Gads[0], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
            }
        }
        if (classMsg == IDCMP_GADGETUP && gad->GadgetID == 1003) // Clear
        {
            GT_SetGadgetAttrs(m_Gads[0], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
            clearList();
            GT_SetGadgetAttrs(m_Gads[0], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
        }
    }
    return -1;
}

std::string PlaylistWindow::openFileRequest()
{
    std::string fullPath = "";
    struct FileRequester *fr = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, NULL);
    if (fr)
    {
        if (AslRequestTags(fr,
                           ASLFR_TitleText, (Tag) "Datei auswählen...",
                           ASLFR_InitialDrawer, (Tag) "RAM:",
                           ASLFR_DoPatterns, TRUE,                                  // Aktiviert das Pattern-Feld
                           ASLFR_InitialPattern, (Tag) "#?.(aac|m4a|flac|mp3|ogg|m3u)", // Das eigentliche Pattern
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

void PlaylistWindow::clearList()
{
    struct Node *n;
    while ((n = RemHead(&m_SongList)))
    {
        SongNode *sn = (SongNode *)n;
        if (sn->node.ln_Name)
            sn->node.ln_Name = NULL;
        delete sn;
    }
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
    line="";
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