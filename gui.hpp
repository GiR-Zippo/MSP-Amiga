#ifndef GUI_H
#define GUI_H

#include "Common.h"

// Gadget IDs
#define ID_SEEKER 0
#define ID_VOLUME 1

#define ID_PLAY   2
#define ID_STOP   3
#define ID_OPEN   4
#define ID_TIME_DISPLAY 5
#define PLAYER_GADS_COUNT 6

struct PlayerGadgetDef {
    uint32_t kind;
    int16_t  x, y, w, h;
    const char* label;
    uint16_t id;
    struct TagItem *tags;
};

class MainUi
{
    public:
        MainUi();
        ~MainUi(){};
        // Funktions-Prototypen
        bool SetupGUI();
        void CleanupGUI();
        std::string OpenFileRequest();
        void UpdateSeeker(long percent);
        void UpdateTimeDisplay(uint32_t lap, uint32_t total);
        ULONG GetWinSignal(){ return 1L << m_win->UserPort->mp_SigBit; }
        Window* GetWindow() { return m_win; }
    private:
        void drawVideoPlaceholder();
        void drawVolumeLevel(long level);
        void formatTimeOldschool(char* b, uint32_t s);
        void createGadget();
        // Globale Pointer, damit main.cpp darauf zugreifen kann
        struct Window *m_win;
        struct Library *m_AslBase;
        struct Gadget *m_gList;
        struct Gadget *m_gads[PLAYER_GADS_COUNT];
        void *m_visInfo;
};
#endif