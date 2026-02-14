#ifndef __CONFIGURATION_HPP__
#define __CONFIGURATION_HPP__

#include "../Common.h"

struct ConfigEntry
{
    char key[32];
    char value[128];
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

    private:
        static Configuration* instance;
        Configuration();
        Configuration(const Configuration&); // Prevent copy
        Configuration& operator=(const Configuration&); // Prevent assignment
        
        ConfigEntry m_configEntries[20];
        int m_numConfigEntries = 0;
};
#define sConfiguration Configuration::getInstance()
#endif