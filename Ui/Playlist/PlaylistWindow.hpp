#ifndef PLAYLISTWINDOW_HPP
#define PLAYLISTWINDOW_HPP

#include "../../Common.h"
#include "../SharedUiFunctions.hpp"

/// @brief The songlist struct
struct SongNode
{
    struct Node node;
    char name[256];
    char path[256];
    int OriginalIndex;
    BOOL Visible;
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

/// @brief The Playlist Window as a Singleton
class PlaylistWindow
{
    public:
        /// @brief Gets the Ui instance
        /// @return Ui instance
        static PlaylistWindow* getInstance()
        {
            if (instance == NULL)
                instance = new PlaylistWindow();
            return instance;
        }

        /*********************************************************/
        /***                  Ui related Stuff                 ***/
        /*********************************************************/
        /// @brief Initialize the Ui
        /// @return true if okay
        bool SetupGUI();

        /// @brief Updates the Ui
        int16_t UpdateUi();

        /// @brief Shutsdown the Ui
        void CleanupGUI();

        /// @brief Opens the window and init the gads on first call
        void open();

        /// @brief Closes only the window
        void close();

        /// @brief Is window open / closed
        /// @return true if open
        bool IsOpen() { return m_opened; }

        /// @brief Get the window signal
        ULONG GetWinSignal() { return 1L << m_Window->UserPort->mp_SigBit; }

        /*********************************************************/
        /***               Playlist related Stuff              ***/
        /*********************************************************/
        /// @brief Adds an entry to the songlist
        /// @param name displayed name
        /// @param fullPath filepath or url
        void AddEntry(std::string name, std::string fullPath);

        /// @brief Use the auto advance
        /// @param yes true if using it
        void SetUsePlaylist(bool yes) { m_playlistInUse = yes; }

        /// @brief Helper to avoid autoadvance
        bool GetAllowNextSong() { return m_allowNextSong; }

        /// @brief Helper to avoid autoadvance
        void SetAllowNextSong() { m_allowNextSong = true; }
        
        /// @brief Auto advance function
        /// @param noadvance - if the same song should be played again
        void PlayNext(bool noadvance = false);

        /// @brief The loaded song path
        char selectedPath[256];

    private:
        static PlaylistWindow* instance;
        PlaylistWindow(); // Private constructor
        PlaylistWindow(const PlaylistWindow&); // Prevent copy
        PlaylistWindow& operator=(const PlaylistWindow&); // Prevent assignment

        /*********************************************************/
        /***               Playlist related Stuff              ***/
        /*********************************************************/
        /// @brief Clears the playlist
        void clearList();

        /// @brief Clears the search text
        void clearSearch();

        /// @brief Find a node by index in playlist
        struct Node *findNode(int index);

        /// @brief Filter playlist
        /// @param searchCrit - search crit case sensitive
        void filter(const char *searchCrit);

        /// @brief Sort playlist by node internal index
        void sortPlaylist();

        /// @brief Remove filter and show all entries
        void showAll();

        /*********************************************************/
        /***               Webstream related Stuff             ***/
        /*********************************************************/
        /// @brief Gets the list from icecast, respects m_searchBuffer
        void getIceCastList();

        /// @brief Gets the list from iTunes, respects m_searchBuffer
        void getiTunesList();

        void getiTunesRSSList();

        /*********************************************************/
        /***                 M3U related Stuff                 ***/
        /*********************************************************/
        /// @brief Simple line separation
        /// @param file input file
        /// @param line output line
        /// @return true if okay
        bool getLineFomFile(BPTR file, std::string &line);

        /// @brief Reads a m3u playlist and fill the internal one
        /// @param filename 
        void parseM3UPlaylist(const char *filename);

        /*********************************************************/
        /***                     Variables                     ***/
        /*********************************************************/
        struct Window*  m_Window;       // The Window
        struct MsgPort* m_Port;         // The MessagePort
        void*           m_VisInfo;      // The VisualInfo
        struct Gadget*  m_GadgetList;   // The GadgetList
        struct Gadget*  m_Gads[PLAYLIST_MAX];
        
        struct List     m_SongList;     // Die sichtbare SongListe
        struct List     m_HiddenList;   // Die Shadowliste
        int32_t         m_SelectedIndex;// The selected index of our songlist
        bool            m_opened;       // Window open/close state
        bool            m_playlistInUse;// use the playlist advance
        bool            m_allowNextSong;// Helper for signal proc. in main.cpp
        bool            m_firstTime;    // Helper for signal proc. in main.cpp
        uint8_t         m_playlistMode; // Our mode - 0 = Playlist - 1 = Icecast

        // SuFu
        char            m_searchBuffer[64]; // Search textbuffer

        // DblClick
        ULONG           m_lastClickSeconds;
        ULONG           m_lastClickMicros;
};

#endif