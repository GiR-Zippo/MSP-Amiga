#include "Common.h"
#include "Icecast.hpp"
#include "../Ui/PlaylistWindow.hpp"

Icecast::Icecast()
{
    m_amiSSL = NULL;
}

Icecast::~Icecast()
{
    if (m_amiSSL)
        delete m_amiSSL;
}

void Icecast::FetchList(List &songList)
{
    m_amiSSL = new AmiSSL();

    if (!m_amiSSL->Init())
        return;

    static char buffer[4096];
    char host[127], path[127];
    int port = 443;

    std::string fName = "https://de1.api.radio-browser.info/json/stations";
    stringToLower(fName);

    size_t pos = fName.find("://");
    if (pos == std::string::npos)
        return;

    size_t slashPos = fName.find('/', pos + 3);

    if (slashPos == std::string::npos)
    {
        strncpy(host, fName.substr(pos + 3).c_str(), 127);
        strcpy(path, "/");
    }
    else
    {
        strncpy(host, fName.substr(pos + 3, slashPos - (pos + 3)).c_str(), 127);
        strncpy(path, fName.substr(slashPos).c_str(), 127);
    }

    printf("Connecting to: %s:%d%s\n", host, port, path);

    if (!m_amiSSL->OpenConnection(host, port))
    {
        printf("Con ERR\n");
        return;
    }
    uint32_t ret = sprintf(buffer, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Amiga\r\nConnection: close\r\n\r\n", path, host);
    printf("Sende\n");

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
        if (lastNode->node.ln_Pred)
            nextIndex = lastNode->OriginalIndex + 1;

        // Suche nach dem Anfang eines Objekts
        while ((startPos = streamBuffer.find('{')) != std::string::npos)
        {
            size_t endPos = streamBuffer.find('}', startPos);

            // Haben wir ein vollständiges Objekt im aktuellen Buffer?
            if (endPos != std::string::npos)
            {
                std::string stationJson = streamBuffer.substr(startPos, endPos - startPos + 1);

                // Jetzt extrahieren wir die Daten aus dem kleinen Stück
                std::string name = getJsonValue(stationJson, "name");
                std::string url = getJsonValue(stationJson, "url_resolved");
                std::string codec = getJsonValue(stationJson, "codec");
                int lastcheckok = getJsonIntValue(stationJson, "lastcheckok");
                if (!name.empty())
                {
                    // MP3 and AAC+ only atm
                    if ((strstr(codec.c_str(), "AAC+") || strstr(codec.c_str(), "MP3")) && lastcheckok == 1)
                    {
                        removeFromString(name, "\\t");
                        removeFromString(name, "\\r");
                        removeFromString(name, "\\n");
                        
                        SongNode *sn = new SongNode;
                        sprintf(sn->name, "[%s] %s", codec.c_str(), name.c_str());
                        sn->name[sizeof(sn->name) - 1] = '\0'; // Null-Terminierung sicherstellen
                        sn->node.ln_Name = sn->name;
                        sn->OriginalIndex = nextIndex++;
                        strncpy(sn->path, url.c_str(), 255);
                        sn->path[255] = '\0';

                        AddTail(&songList, (struct Node *)sn);
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
    m_amiSSL->CleanupAll();
}

std::string Icecast::getJsonValue(const std::string &json, const std::string &key)
{
    std::string searchKey = "\"" + key + "\":\"";
    size_t start = json.find(searchKey);
    if (start == std::string::npos)
        return "";

    start += searchKey.length();
    size_t end = json.find("\"", start);
    if (end == std::string::npos)
        return "";

    return json.substr(start, end - start);
}

int Icecast::getJsonIntValue(const std::string &json, const std::string &key)
{
    std::string searchKey = "\"" + key + "\":";
    size_t start = json.find(searchKey);
    if (start == std::string::npos)
        return -1;

    start += searchKey.length();
    size_t end = json.find("\"", start);
    if (end == std::string::npos)
        return -1;

    return atoi(json.substr(start, end - start).c_str());
}

void Icecast::removeFromString(std::string &src, std::string arg)
{
    if (arg.empty())
        return;
    std::string::size_type pos = 0;
    while ((pos = src.find(arg, pos)) != std::string::npos)
        src.erase(pos, arg.length());
}