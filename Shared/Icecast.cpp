#include "Common.h"
#include "Icecast.hpp"
#include "../Ui/Playlist/PlaylistWindow.hpp"
#include "MiniJson.hpp"

Icecast::Icecast()
{
    m_amiSSL = NULL;
}

Icecast::~Icecast()
{
    if (m_amiSSL)
        delete m_amiSSL;
}

void Icecast::FetchList(List &songList, const char* filter)
{
    m_amiSSL = new AmiSSL();

    if (!m_amiSSL->Init())
        return;

    static char buffer[4096];
    char host[256], path[256];
    int port = 443;

    std::string fName = "https://de1.api.radio-browser.info/json/stations/search?";
    if (filter && !filter[0] == '\0')
        fName = fName + "/search?hidebroken=true&name=" + SimpleEncode(filter) +"&";
    
    fName = fName + "limit=300";
    stringToLower(fName);

    size_t pos = fName.find("://");
    if (pos == std::string::npos)
        return;

    size_t slashPos = fName.find('/', pos + 3);

    if (slashPos == std::string::npos)
    {
        strncpy(host, fName.substr(pos + 3).c_str(), 255);
        strcpy(path, "/");
    }
    else
    {
        strncpy(host, fName.substr(pos + 3, slashPos - (pos + 3)).c_str(), 255);
        strncpy(path, fName.substr(slashPos).c_str(), 255);
    }

    printf("Connecting to: %s:%d%s\n", host, port, path);

    if (!m_amiSSL->OpenConnection(host, port))
    {
        printf("Con ERR\n");
        return;
    }

    uint32_t ret = sprintf(buffer, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Amiga\r\nConnection: close\r\n\r\n", path, host);
    ret = SSL_write(m_amiSSL->GetSSL(), buffer, strlen(buffer));
    if (ret <= 0)
    {
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        printf("AmiSSL Error: %s\n", err_buf);

        int ssl_err = SSL_get_error(m_amiSSL->GetSSL(), ret);
        printf("SSL_get_error: %d\n", ssl_err);
        return;
    }

    // lese die Json
    std::string streamBuffer;
    int bytes;
    while ((bytes = SSL_read(m_amiSSL->GetSSL(), buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes] = '\0';
        streamBuffer += buffer;

        size_t startPos;
        int nextIndex = 0;
        struct SongNode *lastNode = (struct SongNode *)songList.lh_TailPred;
        if (lastNode && lastNode->node.ln_Pred)
            nextIndex = lastNode->OriginalIndex + 1;

        // Suche nach dem Anfang eines Objekts
        while ((startPos = streamBuffer.find('{')) != std::string::npos)
        {
            size_t endPos = streamBuffer.find('}', startPos);

            // Haben wir ein vollständiges Objekt im aktuellen Buffer?
            if (endPos != std::string::npos)
            {
                std::string stationJson = streamBuffer.substr(startPos, endPos - startPos + 1);
                std::string name, url, codec;
                int lastcheckok  = 0;
                MiniJson::GetValue(stationJson, "name", name);
                MiniJson::GetValue(stationJson, "url_resolved", url);
                MiniJson::GetValue(stationJson, "codec", codec);
                MiniJson::GetIntValue(stationJson, "lastcheckok", lastcheckok);
                if (!name.empty())
                {
                    // MP3 and AAC+ only atm
                    if ((strstr(codec.c_str(), "AAC+") || strstr(codec.c_str(), "MP3")) && lastcheckok == 1)
                    {
                        SongNode *sn = new SongNode;
                        snprintf(sn->name, sizeof(sn->name), "[%s] %s", codec.c_str(), name.c_str());
                        sn->name[sizeof(sn->name) - 1] = '\0'; // Null-Terminierung sicherstellen
                        sn->node.ln_Name = sn->name;
                        sn->OriginalIndex = nextIndex++;
                        strncpy(sn->path, url.c_str(), 255);
                        sn->path[254] = '\0';

                        if (!existsInPlaylist(&songList, sn->name))
                            AddTail(&songList, (struct Node *)sn);
                        else
                            updateStationInPlaylist(&songList, sn->name, sn, true); // if entry already exists, update the old one, preferring http over https
                    }
                }
                streamBuffer.erase(0, endPos + 1);
            }
            else
                break;
        }
        if (streamBuffer.length() > 10000)
            streamBuffer = "";
    }
    if (m_amiSSL)
    {
        m_amiSSL->CleanupAll();
        delete m_amiSSL;
        m_amiSSL = NULL;
    }
}

bool Icecast::existsInPlaylist(struct List* targetList, const char* searchName)
{
    struct Node* node = targetList->lh_Head;
    while (node->ln_Succ)
    {
        if (node->ln_Name && strcmp(node->ln_Name, searchName) == 0)
            return true; // Gefunden!
        node = node->ln_Succ;
    }
    return false; // Nicht in der Liste
}

void Icecast::updateStationInPlaylist(struct List* targetList, const char* searchName, struct SongNode* newNode, bool preferHttp)
{
    struct Node* node = targetList->lh_Head;
    while (node->ln_Succ)
    {
        if (node->ln_Name && strcmp(node->ln_Name, searchName) == 0)
        {
            SongNode *sn = (SongNode*)node;
            if (preferHttp && strstr(sn->path, "https"))
            {
                newNode->OriginalIndex = sn->OriginalIndex;
                Remove(node);
                AddTail(targetList, (struct Node *)newNode);
            }
            if (!preferHttp && strstr(sn->path, "http"))
            {
                newNode->OriginalIndex = sn->OriginalIndex;
                Remove(node);
                AddTail(targetList, (struct Node *)newNode);
            }
            break; // Suche beenden
        }
        node = node->ln_Succ;
    }
}

