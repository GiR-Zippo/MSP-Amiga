#ifndef GUI_H
#define GUI_H

#include "../Common.h"

// Gadget IDs
#define ID_SEEKER   0
#define ID_VOLUME   1

#define ID_PLAY     2
#define ID_PAUSE    3
#define ID_STOP     4
#define ID_OPEN     5
#define ID_PLAYLIST 6
#define ID_TIME_DISPLAY 7
#define PLAYER_GADS_COUNT 8

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
        static MainUi& getInstance()
        {
            static MainUi instance;
            return instance;
        }

        bool SetupGUI();
        bool UpdateUi();
        void CleanupGUI();

        void UpdateSeeker(long percent);
        void UpdateTimeDisplay(uint32_t lap, uint32_t total);
        ULONG GetWinSignal(){ return 1L << m_Window->UserPort->mp_SigBit; }
        Window* GetWindow() { return m_Window; }
        void SetVolume(uint16_t vol) {m_VolumeLevel = vol;}
        uint16_t GetVolume() {return m_VolumeLevel;}
    private:
        MainUi();
        MainUi(const MainUi&);
        MainUi& operator=(const MainUi&);

        void drawVideoPlaceholder();
        void drawVolumeLevel(long level);
        void formatTimeOldschool(char* b, uint32_t s);
        void createGadget();
        struct Window *m_Window;
        struct Library *m_AslBase;
        struct Gadget *m_gList;
        struct Gadget *m_gads[PLAYER_GADS_COUNT];
        void *m_visInfo;
        uint16_t m_VolumeLevel;
};
#endif