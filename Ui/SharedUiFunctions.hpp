#ifndef __SHAREDUIFUNCTIONS_HPP__
#define __SHAREDUIFUNCTIONS_HPP__

#include "../Common.h"

/*********************************************************/
/***                  Ui Gadget Stuff                  ***/
/*********************************************************/

/// @brief Default Gadget struct 
/// [Kind X Y W H Label ID Tags]
struct GadgetDef 
{
    uint32_t kind;
    int16_t  x, y, w, h;
    const char* label;
    uint16_t id;
    struct TagItem *tags;
};

/// @brief Default Menu struct
struct MenuDef
{
    uint8_t     type;      // NM_TITLE, NM_ITEM oder NM_SUB
    const char* label;     // Text im Menu
    const char* shortcut;  // Tastenkuerzel (z.B. "Q")
    uint16_t    id;        // interne ID für das Event-Handling
    uint32_t    flags;
};

/// @brief TabStrip strucs
struct TabDef
{
    const char* label;
    uint16_t    id;
};

class SharedUiFunctions
{
    public:
        /// @brief Openfile requester
        /// @param mask the search filter mask
        /// @return the filepath or empty string
        static std::string OpenFileRequest(const char* mask);

        /// @brief Checks for doubleclick
        /// @param s1 lastClickSeconds
        /// @param m1 lastClickMicroseconds
        /// @param s2 IntuiMessage->Seconds
        /// @param m2 IntuiMessage->Micros
        /// @return true if double click
        static bool DoubleCheck(ULONG s1, ULONG m1, ULONG s2, ULONG m2);
};
#endif