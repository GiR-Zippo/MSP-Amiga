#include "Common.h"

void stringToLower(std::string &s)
{
    for (size_t i = 0; i < s.length(); i++)
    {
        // Wenn das Zeichen zwischen 'A' und 'Z' liegt
        if (s[i] >= 'A' && s[i] <= 'Z')
        {
            // Addiere 32, um zum Kleinbuchstaben zu kommen
            s[i] = s[i] + 32;
        }
    }
}

bool containsString(const char *haystack, const char *needle)
{
    if (!needle || !needle[0])
        return true; // Leere Suche = Treffer

    const char *h = haystack;
    const char *n = needle;

    while (*h)
    {
        const char *h_ptr = h;
        const char *n_ptr = n;

        while (*h_ptr && *n_ptr && tolower(*h_ptr) == tolower(*n_ptr))
        {
            h_ptr++;
            n_ptr++;
        }
        if (!*n_ptr)
            return true;
        h++;
    }
    return false;
}

char *strcasestr(const char *haystack, const char *needle)
{
    if (!*needle)
        return (char *)haystack;

    for (; *haystack; haystack++)
    {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle))
        {
            const char *h = haystack;
            const char *n = needle;
            while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n))
            {
                h++;
                n++;
            }
            if (!*n)
                return (char *)haystack;
        }
    }
    return NULL;
}

std::vector<std::string> Split(const std::string &text, const std::string &delimiter)
{
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = text.find(delimiter);

    while (end != std::string::npos)
    {
        tokens.push_back(text.substr(start, end - start));
        start = end + delimiter.length();
        end = text.find(delimiter, start);
    }

    tokens.push_back(text.substr(start));
    return tokens;
}

void UTF8ToAmiga(char *str)
{
    unsigned char *src = (unsigned char *)str;
    unsigned char *dst = (unsigned char *)str;

    while (*src)
    {
        if (*src == 0xC3)
        { // UTF-8 Startbyte für viele Sonderzeichen
            src++;
            if (*src == 0xA4)
                *dst = 0xE4; // ä
            else if (*src == 0xB6)
                *dst = 0xF6; // ö
            else if (*src == 0xBC)
                *dst = 0xFC; // ü
            else if (*src == 0x84)
                *dst = 0xC4; // Ä
            else if (*src == 0x96)
                *dst = 0xD6; // Ö
            else if (*src == 0x9C)
                *dst = 0xDC; // Ü
            else if (*src == 0x9F)
                *dst = 0xDF; // ß
            else
                *dst = '?'; // Fallback
        }
        else if (*src < 128)
        {
            *dst = *src;
        }
        else
        {
            *dst = '?'; // Unbekanntes Multibyte-Zeichen
        }
        src++;
        dst++;
    }
    *dst = '\0';
}

void RemoveFromString(std::string &src, std::string arg)
{
    if (arg.empty())
        return;
    std::string::size_type pos = 0;
    while ((pos = src.find(arg, pos)) != std::string::npos)
        src.erase(pos, arg.length());
}

std::string SimpleEncode(const char *src)
{
    std::string out;
    while (*src)
    {
        if (*src == ' ')
            out += "%20";
        else
            out += *src;
        src++;
    }
    return out;
}

// Bleibt so, dann compiled es
size_t wcsrtombs(char *dest, const wchar_t **src, size_t len, mbstate_t *ps)
{
    (void)ps; // Status wird am Amiga nicht benötigt

    if (src == NULL || *src == NULL)
        return 0;

    size_t count = 0;
    const wchar_t *s = *src;

    // Wenn dest NULL ist, will der Aufrufer nur die benötigte Länge wissen
    if (dest == NULL)
    {
        while (*s++)
            count++;
        return count;
    }

    // Eigentliche Konvertierung
    while (count < len)
    {
        wchar_t wc = *s;

        // Cast von wchar_t (16/32 bit) auf char (8 bit)
        dest[count] = static_cast<char>(wc);

        if (wc == L'\0')
        {
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

size_t wcslen(const wchar_t *s)
{
    if (s == NULL)
        return 0;

    const wchar_t *p = s;
    while (*p != L'\0')
    { // L'\0' ist der Wide-Null-Terminator
        p++;
    }

    // Die Differenz der Zeiger ergibt die Anzahl der Elemente
    return static_cast<size_t>(p - s);
}

void dump_packet(const uint8_t *buffer, int len)
{
    printf("\n--- Packet Dump (%d bytes) ---\n", len);

    for (int i = 0; i < len; i += 16)
    {
        // 1. Offset anzeigen (Hex)
        printf("%04x: ", i);

        // 2. Hex-Teil (16 Bytes pro Zeile)
        for (int j = 0; j < 16; j++)
        {
            if (i + j < len)
                printf("%02x ", buffer[i + j]);
            else
                printf("   "); // Auffüllen bei kürzeren Zeilen
        }

        printf(" | ");

        // 3. String-Teil (ASCII)
        for (int j = 0; j < 16; j++)
        {
            if (i + j < len)
            {
                uint8_t c = buffer[i + j];
                // Nur druckbare Zeichen anzeigen, sonst einen Punkt
                printf("%c", isprint(c) ? c : '.');
            }
        }
        printf("\n");
    }
    printf("------------------------------\n");
}

#ifdef OLD_GCC
float powf(float base, float exp)
{
    return (float)pow(base, exp);
}
#endif