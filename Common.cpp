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

bool containsString(const char* haystack, const char* needle)
{
    if (!needle || !needle[0]) return true; // Leere Suche = Treffer
    
    const char* h = haystack;
    const char* n = needle;
    
    while (*h) {
        const char* h_ptr = h;
        const char* n_ptr = n;
        
        while (*h_ptr && *n_ptr && tolower(*h_ptr) == tolower(*n_ptr)) {
            h_ptr++;
            n_ptr++;
        }
        if (!*n_ptr) return true;
        h++;
    }
    return false;
}

void UTF8ToAmiga(char *str) {
    unsigned char *src = (unsigned char *)str;
    unsigned char *dst = (unsigned char *)str;

    while (*src) {
        if (*src == 0xC3) { // UTF-8 Startbyte für viele Sonderzeichen
            src++;
            if (*src == 0xA4)      *dst = 0xE4; // ä
            else if (*src == 0xB6) *dst = 0xF6; // ö
            else if (*src == 0xBC) *dst = 0xFC; // ü
            else if (*src == 0x84) *dst = 0xC4; // Ä
            else if (*src == 0x96) *dst = 0xD6; // Ö
            else if (*src == 0x9C) *dst = 0xDC; // Ü
            else if (*src == 0x9F) *dst = 0xDF; // ß
            else *dst = '?'; // Fallback
        } else if (*src < 128) {
            *dst = *src;
        } else {
            *dst = '?'; // Unbekanntes Multibyte-Zeichen
        }
        src++;
        dst++;
    }
    *dst = '\0';
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
