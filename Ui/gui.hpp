#ifndef GUI_H
#define GUI_H

#include "../Common.h"
#include "SharedUiFunctions.hpp"

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

class MainUi
{
    public:
        /// @brief Gets the Ui instance
        /// @return Ui instance
        static MainUi* getInstance()
        {
            if (instance == NULL)
                instance = new MainUi();
            return instance;
        }

        /*********************************************************/
        /***                  Ui related Stuff                 ***/
        /*********************************************************/
        /// @brief Initialize the Ui
        /// @return true if okay
        bool SetupGUI();

        /// @brief Updates the Ui
        bool UpdateUi();

        /// @brief Shutsdown the Ui
        void CleanupGUI();

        /// @brief Get the window signal
        ULONG GetWinSignal() { return 1L << m_Window->UserPort->mp_SigBit; }
    
        /// @brief Update the seeker
        /// @param percent 
        void UpdateSeeker(long percent);

        /// @brief Update the lap and total time display
        /// @param lap laptime in seconds
        /// @param total total time in seconds
        void UpdateTimeDisplay(uint32_t lap, uint32_t total);

        /// @brief Updates the track and artist screen
        void UpdateDisplayInformation();

        /// @brief Set the volumelevel
        /// @param vol vollevel in percent
        void SetVolume(uint16_t vol) {m_VolumeLevel = vol;}

        /// @brief Get the volume level
        /// @return volume in percent
        uint16_t GetVolume() {return m_VolumeLevel;}

    private:
        static MainUi* instance;
        MainUi(); // Private constructor
        MainUi(const MainUi&); // Prevent copy
        MainUi& operator=(const MainUi&); // Prevent assignment

        /*********************************************************/
        /***                  Ui related Stuff                 ***/
        /*********************************************************/

        /// @brief Draw the black frame
        void drawVideoPlaceholder();
        
        /// @brief Draws the black frame with title and artist
        void drawVideoPlaceholder(const char* title, const char* artist);

        /// @brief Helper to draw a text centered
        void drawCenteredText(struct RastPort *rp, const char *text, int x1, int boxWidth, int yPos);

        /// @brief format timestring for timedisplay
        void formatTimeOldschool(char* b, uint32_t s);

        void drawVolumeLevel(long level);

        struct Window*  m_Window;
        struct Library* m_AslBase;
        struct Gadget*  m_gList;
        struct Gadget*  m_gads[PLAYER_GADS_COUNT];
        void *          m_visInfo;
        uint16_t        m_VolumeLevel;
};
#endif