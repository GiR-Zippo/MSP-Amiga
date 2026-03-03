#ifndef __SETTINGS_HPP__
#define __SETTINGS_HPP__
#include "../../Common.h"
#include "../../Shared/Configuration.hpp"
#include "../SharedUiFunctions.hpp"

enum SettingsGads
{
    SETTINGS_SOFTVOL = 0,
    SETTINGS_AHIDEVICE,
    SETTINGS_PAGETWO,
    SETTINGS_MIDI_VOICES,
    SETTINGS_MIDI_SF,
    SETTINGS_MIDI_SF_OPEN,
    SETTINGS_PAGETHREE,
    SETTINGS_LISTSIZE,
    SETTINGS_MAX,
};

class SettingsUi
{
    public:
        static SettingsUi* getInstance()
        {
            if (instance == NULL)
                instance = new SettingsUi();
            return instance;
        }

        bool SetupGUI();
        void UpdateUi();
        void CloseGUI();
        /// @brief Get the window signal
        ULONG GetWinSignal() { return 1L << m_Window->UserPort->mp_SigBit; }

    private:
        static SettingsUi* instance;
        SettingsUi();
        ~SettingsUi();
        SettingsUi(const SettingsUi&);
        SettingsUi& operator=(const SettingsUi&);

        void renderDecorations();
        void createPage(uint16_t pageNum);
        void clearPage();
        void createGads(Gadget *context, int end, GadgetDef *defs);

        struct Window*  m_Window;
        struct MsgPort* m_Port;
        void*           m_VisInfo;
        struct Gadget*  m_Gads[SETTINGS_MAX];
        struct Gadget*  m_MainGlist; // Die Tabs (bleiben)
        struct Gadget*  m_PageGlist; // Der Inhalt (wechselt)
        uint16_t        m_CurrentTab;
        int             m_topOffset;
};
#define sSettingsUi SettingsUi::getInstance()
#endif