#include "SharedUiFunctions.hpp"

/// @brief Opens a file open dialog
/// @param Searchmask 
/// @return the complete path+filename
std::string SharedUiFunctions::OpenFileRequest(const char* mask)
{
    std::string fullPath = "";
    struct FileRequester *fr = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, NULL);
    if (fr)
    {
        if (AslRequestTags(fr,
                           ASLFR_TitleText, (Tag) "Datei auswählen...",
                           ASLFR_InitialDrawer, (Tag) "RAM:",
                           ASLFR_DoPatterns, TRUE,           // Aktiviert das Pattern-Feld
                           ASLFR_InitialPattern, (Tag) mask, // Das eigentliche Pattern
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

bool SharedUiFunctions::DoubleCheck(ULONG s1, ULONG m1, ULONG s2, ULONG m2)
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