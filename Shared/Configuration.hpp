#ifndef __CONFIGURATION_HPP__
#define __CONFIGURATION_HPP__

#include "../Common.h"

struct ConfigEntry
{
    char key[32];
    char value[128];
};

enum 
{
    CONF_SOFT_VOL = 0,
    CONF_MIDI_VOICES,
    CONF_SOUNDFONT,
    CONF_COUNT
};

static const char* configKeys[] =
{
    "UseSoftVolume",
    "MaxMidiVoices",
    "SoundFontFile",
    NULL
};

class Configuration
{
    public:
        static Configuration* getInstance()
        {
            if (instance == NULL)
                instance = new Configuration();
            return instance;
        }

        /*********************************************************/
        /***                    Config Ui                      ***/
        /*********************************************************/

        /*********************************************************/
        /***                 Config Read/Write                 ***/
        /*********************************************************/
        void LoadConfig();
        void SaveConfig();
        int GetConfigInt(const char *key, int defaultValue)
        {
            for (int i = 0; i < m_numConfigEntries; i++)
                if (strcmp(m_configEntries[i].key, key) == 0)
                    return atoi(m_configEntries[i].value);
            return defaultValue;
        }

        const char* GetConfigString(const char *key, const char *defaultValue)
        {
            for (int i = 0; i < m_numConfigEntries; i++)
                if (strcmp(m_configEntries[i].key, key) == 0)
                    return m_configEntries[i].value;
            
            return defaultValue;
        }

        void SetConfigInt(const char *key, int value)
        {
            char valueStr[32];
            snprintf(valueStr, sizeof(valueStr), "%d", value);
            SetConfigString(key, valueStr);
        }
        
        void SetConfigString(const char *key, const char *value)
        {
            if (!key || !value) return;
            
            // Check if key already exists - update it:
            for (int i = 0; i < m_numConfigEntries; i++)
            {
                if (strcmp(m_configEntries[i].key, key) == 0)
                {
                    strncpy(m_configEntries[i].value, value, sizeof(m_configEntries[i].value) - 1);
                    m_configEntries[i].value[sizeof(m_configEntries[i].value) - 1] = '\0';
                    return;
                }
            }
            
            // Key not found - add new entry:
            if (m_numConfigEntries < 20)
            {
                strncpy(m_configEntries[m_numConfigEntries].key, key, sizeof(m_configEntries[m_numConfigEntries].key) - 1);
                m_configEntries[m_numConfigEntries].key[sizeof(m_configEntries[m_numConfigEntries].key) - 1] = '\0';
                
                strncpy(m_configEntries[m_numConfigEntries].value, value, sizeof(m_configEntries[m_numConfigEntries].value) - 1);
                m_configEntries[m_numConfigEntries].value[sizeof(m_configEntries[m_numConfigEntries].value) - 1] = '\0';
                
                m_numConfigEntries++;
            }
            else
            {
                printf("WARNING: Config entries full! Cannot add '%s'\n", key);
            }
        }

    private:
        static Configuration* instance;
        Configuration();
        Configuration(const Configuration&); // Prevent copy
        Configuration& operator=(const Configuration&); // Prevent assignment
        
        ConfigEntry m_configEntries[20];
        int m_numConfigEntries;
};
#define sConfiguration Configuration::getInstance()
#endif

/*
[UseSoftVolume]=0/1
[MaxMidiVoices]=128
[SoundFontFile]=default.sf2

*/