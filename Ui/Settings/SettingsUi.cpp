#include "SettingsUi.hpp"

SettingsUi *SettingsUi::instance = NULL;

#define ID_TAB_BASE 2000

static struct TagItem buttonTags[] = {{GTTX_Border, TRUE}, {TAG_DONE, 0}};
static struct GadgetDef pageOneGadgets[] =
{
    {CHECKBOX_KIND, 160, 20, 120, 20, "Soft Volume", SETTINGS_SOFTVOL, buttonTags}
};

static struct GadgetDef pageTwoGadgets[] =
{
    {STRING_KIND, 160, 20, 130, 20, "Max Voices", SETTINGS_MIDI_VOICES,  buttonTags},
    {STRING_KIND, 160, 40, 130, 20, "SF2",        SETTINGS_MIDI_SF,      buttonTags},
    {BUTTON_KIND, 290, 40, 10, 20, "^",          SETTINGS_MIDI_SF_OPEN, buttonTags},
};


SettingsUi::SettingsUi() : m_Window(NULL), m_Port(NULL), m_VisInfo(NULL),
                           m_MainGlist(NULL), m_PageGlist(NULL), m_CurrentTab(0)
{
}

SettingsUi::~SettingsUi()
{
    CloseGUI();
}

bool SettingsUi::SetupGUI()
{
    if (m_Window)
        return true;
    struct Screen *scr = LockPubScreen(NULL);
    if (!scr)
        return false;

    m_VisInfo = GetVisualInfo(scr, TAG_DONE);
    if (!m_VisInfo)
    {
        UnlockPubScreen(NULL, scr);
        return false;
    }

    struct Gadget *context = CreateContext(&m_MainGlist);
    struct NewGadget ng;
    const char *labels[] = {"Audio", "Midi"};
    struct Gadget *last = context;

    for (int i = 0; i < 2; i++)
    {
        ng.ng_LeftEdge = 4;            // Immer ganz links
        ng.ng_TopEdge = 20 + (i * 18); // Untereinander (14px Höhe + 4px Abstand)
        ng.ng_Width = 80;              // Feste Breite für die Sidebar
        ng.ng_Height = 14;
        ng.ng_GadgetText = (STRPTR)labels[i];
        ng.ng_GadgetID = ID_TAB_BASE + i;
        ng.ng_VisualInfo = m_VisInfo;
        ng.ng_TextAttr = NULL;
        ng.ng_Flags = 0;
        last = CreateGadget(BUTTON_KIND, last, &ng, TAG_DONE);
    }

    m_Window = OpenWindowTags(NULL,
                              WA_Title, (Tag) "Settings",
                              WA_InnerWidth, 300,
                              WA_InnerHeight, 200,
                              WA_Gadgets, (Tag)m_MainGlist,
                              WA_IDCMP, IDCMP_GADGETUP | IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
                              WA_Flags, WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH,
                              WA_PubScreen, (Tag)scr,
                              TAG_DONE);

    UnlockPubScreen(NULL, scr);

    if (m_Window)
    {
        GT_RefreshWindow(m_Window, NULL);
        createPage(0); // Standardmäßig erste Seite laden
        return true;
    }

    return false;
}

void SettingsUi::renderDecorations()
{
    DrawBevelBox(m_Window->RPort,
                 88, m_Window->BorderTop,
                 2, m_Window->Height - m_Window->BorderBottom - m_Window->BorderTop,
                 GT_VisualInfo, (Tag)m_VisInfo,
                 GTBB_FrameType, BBFT_ICONDROPBOX,
                 TAG_DONE);
}

void SettingsUi::createPage(uint16_t pageNum)
{
    m_CurrentTab = pageNum;
    struct Gadget *context = CreateContext(&m_PageGlist);
    if (!context)
        return;

    if (m_CurrentTab == 0) // Audio
        createGads(context, SETTINGS_PAGETWO, pageOneGadgets);
    else if (m_CurrentTab == 1) // Midi
        createGads(context, SETTINGS_MAX-SETTINGS_PAGETWO-1, pageTwoGadgets);

    if (m_PageGlist)
    {
        AddGList(m_Window, m_PageGlist, -1, -1, NULL);
        RefreshGList(m_PageGlist, m_Window, NULL, -1);
        RefreshGList(m_MainGlist, m_Window, NULL, -1);
        GT_RefreshWindow(m_Window, NULL);
    }

    Move(m_Window->RPort, 4, 20);
    Draw(m_Window->RPort, m_Window->Width - 4, 20);

    renderDecorations();
}

void SettingsUi::clearPage()
{
    if (m_PageGlist)
    {
        RemoveGList(m_Window, m_PageGlist, -1);
        FreeGadgets(m_PageGlist);
        m_PageGlist = NULL;
        for (int i = 0; i < SETTINGS_MAX; i++)
            m_Gads[i] = NULL;
    }
    // Content-Bereich im Fenster schwarz/leer übermalen
    SetAPen(m_Window->RPort, 0);
    RectFill(m_Window->RPort, m_Window->BorderLeft, 19,
             m_Window->Width - m_Window->BorderRight, m_Window->Height - m_Window->BorderBottom);
}

void SettingsUi::createGads(Gadget *context, int end, GadgetDef *defs)
{
    struct NewGadget ng;
    ng.ng_VisualInfo = (APTR)m_VisInfo;
    ng.ng_TextAttr = NULL;
    ng.ng_Flags = 0;

    for (int i = 0; i < end; i++)
    {
        ng.ng_LeftEdge = defs[i].x;
        ng.ng_TopEdge = defs[i].y;
        ng.ng_Width = defs[i].w;
        ng.ng_Height = defs[i].h;
        ng.ng_GadgetText = (CONST_STRPTR)defs[i].label;
        ng.ng_GadgetID = defs[i].id;

        //def values
        if (defs[i].id == SETTINGS_SOFTVOL)
        {
            context = CreateGadget(defs[i].kind, context, &ng,
                                   GTCB_Checked, (bool)sConfiguration->GetConfigInt(configKeys[CONF_SOFT_VOL], 0),
                                   TAG_MORE, (Tag)defs[i].tags);
        }
        else if (defs[i].id == SETTINGS_MIDI_VOICES)
        {
            context = CreateGadget(defs[i].kind, context, &ng,
                                   GTST_String, (Tag)sConfiguration->GetConfigString(configKeys[CONF_MIDI_VOICES], "128"),
                                   TAG_MORE, (Tag)defs[i].tags);
        }
        else if (defs[i].id == SETTINGS_MIDI_SF)
        {
            context = CreateGadget(defs[i].kind, context, &ng,
                                   GTST_MaxChars, 1024,
                                   GTST_String, (Tag)sConfiguration->GetConfigString(configKeys[CONF_SOUNDFONT], "default.sf2"),
                                   TAG_MORE, (Tag)defs[i].tags);
        }
        else
            context = CreateGadgetA(defs[i].kind, context, &ng, defs[i].tags);

        m_Gads[i] = context;
    }
}

void SettingsUi::UpdateUi()
{
    if (!m_Window)
        return;

    struct IntuiMessage *msg;
    while ((msg = GT_GetIMsg(m_Window->UserPort)))
    {
        uint32_t classMsg = msg->Class;
        struct Gadget *gad = (struct Gadget *)msg->IAddress;
        GT_ReplyIMsg(msg);

        if (classMsg == IDCMP_CLOSEWINDOW)
            CloseGUI();

        if (classMsg == IDCMP_GADGETUP)
        {
            if (gad->GadgetID >= ID_TAB_BASE && gad->GadgetID < ID_TAB_BASE + 3)
            {
                clearPage();
                createPage(gad->GadgetID - ID_TAB_BASE);
            }
            else if (gad->GadgetID == SETTINGS_SOFTVOL)
            {
                sConfiguration->SetConfigInt(configKeys[CONF_SOFT_VOL], msg->Code);
                sConfiguration->SaveConfig();
            }
            else if (gad->GadgetID == SETTINGS_MIDI_VOICES)
            {
                char buffer[5];
                struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
                strcpy(buffer, (const char*)si->Buffer);
                sConfiguration->SetConfigString(configKeys[CONF_MIDI_VOICES], buffer);
                sConfiguration->SaveConfig();
            }
            else if (gad->GadgetID == SETTINGS_MIDI_SF)
            {
                char buffer[256];
                struct StringInfo *si = (struct StringInfo *)gad->SpecialInfo;
                strcpy(buffer, (const char*)si->Buffer);
                sConfiguration->SetConfigString(configKeys[CONF_SOUNDFONT], buffer);
                sConfiguration->SaveConfig();
            }
            else if (gad->GadgetID == SETTINGS_MIDI_SF_OPEN)
            {
                std::string filename = SharedUiFunctions::OpenFileRequest("#?.(sf2)");
                if (filename.empty())
                    return;
                sConfiguration->SetConfigString(configKeys[CONF_SOUNDFONT], filename.c_str());
                sConfiguration->SaveConfig();
                clearPage();
                createPage(1);
            }
        }
    }
}

void SettingsUi::CloseGUI()
{
    if (m_Window)
    {
        if (m_PageGlist)
            RemoveGList(m_Window, m_PageGlist, -1);
        CloseWindow(m_Window);
        m_Window = NULL;
    }

    if (m_PageGlist)
    {
        FreeGadgets(m_PageGlist);
        m_PageGlist = NULL;
        for (int i = 0; i < SETTINGS_MAX; i++)
            m_Gads[i] = NULL;
    }

    if (m_MainGlist)
    {
        FreeGadgets(m_MainGlist);
        m_MainGlist = NULL;
    }

    if (m_VisInfo)
    {
        FreeVisualInfo(m_VisInfo);
        m_VisInfo = NULL;
    }
}