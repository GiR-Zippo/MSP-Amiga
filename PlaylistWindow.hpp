#ifndef PLAYLISTWINDOW_HPP
#define PLAYLISTWINDOW_HPP

#include "Common.h"

struct SongNode
{
    struct Node node;
    char name[256];
    char path[256];
};

struct PlaylistGadgetDef
{
    uint32_t kind;
    int16_t x, y, w, h;
    const char *label;
    uint16_t id;
    struct TagItem *tags;
};

class PlaylistWindow
{

public:
    PlaylistWindow();
    ~PlaylistWindow();

    bool open();
    void close();
    void addEntry(std::string name, std::string fullPath);
    int16_t HandleMessages();
    ULONG GetWinSignal() { return 1L << m_Window->UserPort->mp_SigBit; }
    bool IsOpen() { return m_opened; }

    // Callback oder Variable für den gewählten Song
    char selectedPath[256];
    bool songSelected;

private:
    std::string openFileRequest();
    void clearList();
    struct Node *findNode(int index);
    bool getLine(BPTR file, std::string &line);
    void parsePlaylist(const char *filename);

    struct Window   *m_Window;
    struct MsgPort  *m_Port;
    void            *m_VisInfo;
    struct Gadget   *m_GadgetList;
    struct List      m_SongList; // Exec-Liste für GadTools
    struct Gadget   *m_Gads[4];
    int32_t          m_SelectedIndex;
    bool             m_opened;

    //DblClick
    ULONG            m_lastClickSeconds;
    ULONG            m_lastClickMicros;
};

#endif