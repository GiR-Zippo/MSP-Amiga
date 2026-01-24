#include "Common.h"

void stringToLower(std::string& s)
{
    for (size_t i = 0; i < s.length(); i++) {
        // Wenn das Zeichen zwischen 'A' und 'Z' liegt
        if (s[i] >= 'A' && s[i] <= 'Z') {
            // Addiere 32, um zum Kleinbuchstaben zu kommen
            s[i] = s[i] + 32;
        }
    }
}

//Bleibt so, dann compiled es
size_t wcsrtombs(char* dest, const wchar_t** src, size_t len, mbstate_t* ps) {
    (void)ps; // Status wird am Amiga nicht benötigt
    
    if (src == NULL || *src == NULL) return 0;

    size_t count = 0;
    const wchar_t* s = *src;

    // Wenn dest NULL ist, will der Aufrufer nur die benötigte Länge wissen
    if (dest == NULL) {
        while (*s++) count++;
        return count;
    }

    // Eigentliche Konvertierung
    while (count < len) {
        wchar_t wc = *s;
        
        // Cast von wchar_t (16/32 bit) auf char (8 bit)
        dest[count] = static_cast<char>(wc);
        
        if (wc == L'\0') {
            *src = NULL;
            return count; // Nullterminator nicht mitzählen
        }

        s++;
        count++;
    }

    // Wenn wir hier ankommen, war der Puffer zu klein
    *src = s;
    return count;
}

size_t wcslen(const wchar_t* s) {
    if (s == NULL) return 0;

    const wchar_t* p = s;
    while (*p != L'\0') { // L'\0' ist der Wide-Null-Terminator
        p++;
    }
    
    // Die Differenz der Zeiger ergibt die Anzahl der Elemente
    return static_cast<size_t>(p - s);
}
