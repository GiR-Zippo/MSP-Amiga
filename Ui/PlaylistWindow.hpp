#ifndef PLAYLISTWINDOW_HPP
#define PLAYLISTWINDOW_HPP

#include "../Common.h"

struct SongNode
{
    struct Node node;
    char name[256];
    char path[256];
    int OriginalIndex;
    BOOL Visible;
};

struct PlaylistGadgetDef
{
    uint32_t kind;
    int16_t x, y, w, h;
    const char *label;
    uint16_t id;
    struct TagItem *tags;
};

enum PlaylistGads
{
    PLAYLIST_MODE = 0,
    PLAYLIST_SEARCH,
    PLAYLIST_LIST,
    PLAYLIST_ADD,
    PLAYLIST_REMOVE,
    PLAYLIST_CLEAR,
    PLAYLIST_MAX,
};

class PlaylistWindow
{
    public:
        static PlaylistWindow& getInstance()
        {
            static PlaylistWindow instance;
            return instance;
        }
        bool SetupGUI();
        int16_t UpdateUi();
        void CleanupGUI();

        void open();
        void close();

        void addEntry(std::string name, std::string fullPath);

        ULONG GetWinSignal() { return 1L << m_Window->UserPort->mp_SigBit; }
        bool IsOpen() { return m_opened; }
        void SetUsePlaylist(bool yes) { m_playlistInUse = yes; }
        bool GetAllowNextSong() { return m_allowNextSong; }
        void SetAllowNextSong() { m_allowNextSong = true; }
        void PlayNext(bool noadvance = false);
        // Callback oder Variable für den gewählten Song
        char selectedPath[256];

    private:
        PlaylistWindow();
        PlaylistWindow(const PlaylistWindow&);
        PlaylistWindow& operator=(const PlaylistWindow&);

        void clearList();
        struct Node *findNode(int index);
        bool getLine(BPTR file, std::string &line);
        void parsePlaylist(const char *filename);
        void filter(const char *searchCrit);
        void sortPlaylist();
        void showAll();

        struct Window *m_Window;
        struct MsgPort *m_Port;
        void *m_VisInfo;
        struct Gadget *m_GadgetList;
        struct Gadget *m_Gads[PLAYLIST_MAX];
        
        struct List m_SongList;   // Die sichtbare SongListe
        struct List m_HiddenList; // Die Shadowliste
        int32_t m_SelectedIndex;
        bool m_opened;
        bool m_playlistInUse;
        bool m_allowNextSong;
        bool m_firstTime;
        uint8_t m_playlistMode;

        // SuFu
        char m_searchBuffer[64];
        // DblClick
        ULONG m_lastClickSeconds;
        ULONG m_lastClickMicros;
};

#endif