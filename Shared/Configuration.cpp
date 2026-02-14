#include "Configuration.hpp"

Configuration* Configuration::instance = NULL;

void Configuration::LoadConfig()
{
    FILE *f = fopen("S:MSP.cfg", "r");
    if (!f) return;

    char line[256];
    m_numConfigEntries = 0;

    while (fgets(line, sizeof(line), f) && m_numConfigEntries < 20)
    {
        // Kommentare und Leerzeilen ignorieren
        if (line[0] == '#' || line[0] == ';' || line[0] == '\n' || line[0] == '\r')
            continue;

        char *kStart = strchr(line, '[');
        char *kEnd = strchr(line, ']');
        char *valStart = strchr(line, '=');

        if (kStart && kEnd && valStart && kEnd < valStart)
        {
            int keyLen = kEnd - kStart - 1;
            if (keyLen > 31) keyLen = 31;
            strncpy(m_configEntries[m_numConfigEntries].key, kStart + 1, keyLen);
            m_configEntries[m_numConfigEntries].key[keyLen] = '\0';

            char *vPtr = valStart + 1;
            char *nl = strpbrk(vPtr, "\r\n");
            if (nl) *nl = '\0';

            strncpy(m_configEntries[m_numConfigEntries].value, vPtr, 127);
            m_configEntries[m_numConfigEntries].value[127] = '\0';

            m_numConfigEntries++;
        }
    }
    fclose(f);
}

void Configuration::SaveConfig()
{
    FILE *f = fopen("S:MSP.cfg", "w");
    if (!f) return;

    fprintf(f, "# MSP Configuration File\n");
    
    for (int i = 0; i < m_numConfigEntries; i++)
        fprintf(f, "[%s]=%s\n", m_configEntries[i].key, m_configEntries[i].value);

    fclose(f);
}

Configuration::Configuration()
{
}