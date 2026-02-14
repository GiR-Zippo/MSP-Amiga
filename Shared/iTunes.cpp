#include "iTunes.hpp"
#include "../Ui/Playlist/PlaylistWindow.hpp"
#include "MiniJson.hpp"
#include "MiniRSS.hpp"

iTunes::iTunes()
{
    m_amiSSL = NULL;
}

iTunes::~iTunes()
{
    if (m_amiSSL)
        delete m_amiSSL;
}

std::string UrlEncodeUTF8(const std::string &value)
{
    std::string escaped = "";
    char buf[8];

    for (size_t i = 0; i < value.length(); ++i)
    {
        unsigned char c = (unsigned char)value[i];

        // 1. Standard ASCII (0-127)
        if (c < 128)
        {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            {
                escaped += (char)c;
            }
            else if (c == ' ')
            {
                escaped += '+';
            }
            else
            {
                sprintf(buf, "%%%02X", c);
                escaped += buf;
            }
        }
        // 2. Umlaute & Sonderzeichen (ISO-8859-1 -> UTF-8 Mapper)
        else
        {
            // Ein ISO-Latin-1 Zeichen > 127 entspricht in UTF-8
            // immer einer 2-Byte Sequenz:
            // Byte 1: 0xC0 | (c >> 6)
            // Byte 2: 0x80 | (c & 0x3F)
            unsigned char utf8_1 = 0xC0 | (c >> 6);
            unsigned char utf8_2 = 0x80 | (c & 0x3F);

            sprintf(buf, "%%%02X%%%02X", utf8_1, utf8_2);
            escaped += buf;
        }
    }
    return escaped;
}

void iTunes::FetchList(List &songList, const char *filter)
{
    m_amiSSL = new AmiSSL();
    if (!m_amiSSL->Init())
        return;

    static char buffer[4096];
    char host[256], path[256];
    int port = 443;

    std::string fName = "https://itunes.apple.com/search?term=librivox&entity=podcast";
    if (filter && !filter[0] == '\0')
        fName = "https://itunes.apple.com/search?term=" + UrlEncodeUTF8(filter) + "&entity=podcast";

    // fName = fName + "limit=300";

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
    bool foundResults = false;
    int nextIndex = 0;
    while ((bytes = SSL_read(m_amiSSL->GetSSL(), buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes] = '\0';
        streamBuffer += buffer;
        if (!foundResults)
        {
            size_t resultsStart = streamBuffer.find("\"results\": [");
            if (resultsStart != std::string::npos)
            {
                // Wir löschen ALLES vor dem "[", damit der Buffer leer wird
                streamBuffer.erase(0, resultsStart + 11);
                foundResults = true;
            }
            else
            {
                if (streamBuffer.length() > 8192)
                    streamBuffer = "";
                continue;
            }
        }

        size_t startPos;
        size_t processedUntil = 0;
        while ((startPos = streamBuffer.find('{', processedUntil)) != std::string::npos)
        {
            size_t endPos = streamBuffer.find('}', startPos);
            if (endPos != std::string::npos)
            {
                std::string stationJson = streamBuffer.substr(startPos, endPos - startPos + 1);
                std::string name, url;
                MiniJson::GetValue(stationJson, "collectionName", name);
                MiniJson::GetValue(stationJson, "feedUrl", url);

                if (!url.empty())
                {
                    if (!strstr(url.c_str(), "anchor.fm") &&
                        !strstr(url.c_str(), "megaphone.fm"))
                    {
                        RemoveFromString(name, "\\t");
                        RemoveFromString(name, "\\r");
                        RemoveFromString(name, "\\n");

                        SongNode *sn = new SongNode;
                        sprintf(sn->name, "%s", name.c_str());
                        UTF8ToAmiga(sn->name);
                        sn->name[sizeof(sn->name) - 1] = '\0'; // Null-Terminierung sicherstellen
                        sn->node.ln_Name = sn->name;
                        sn->node.ln_Type = NT_USER;
                        sn->OriginalIndex = nextIndex++;
                        strncpy(sn->path, url.c_str(), 255);
                        sn->path[254] = '\0';
                        AddTail(&songList, (struct Node *)sn);
                    }
                }

                processedUntil = endPos + 1;
            }
            else
                break;
        }
        if (processedUntil > 0)
        {
            streamBuffer.erase(0, processedUntil);
        }
    }
    if (m_amiSSL)
    {
        m_amiSSL->CleanupAll();
        delete m_amiSSL;
        m_amiSSL = NULL;
    }
}

void iTunes::FetchRSS(List &songList, const char *url)
{
    printf("%s\n", url);
    m_amiSSL = new AmiSSL();

    if (!m_amiSSL->Init())
        return;

    static char buffer[4096];
    char host[256], path[256];
    int port = 443;

    std::string fName = url;

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
    int nextIndex = 0;
    while ((bytes = SSL_read(m_amiSSL->GetSSL(), buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes] = '\0';
        streamBuffer += buffer;
        size_t itemStart;
        while ((itemStart = streamBuffer.find("<item>")) != std::string::npos)
        {
            size_t itemEnd = streamBuffer.find("</item>", itemStart);

            if (itemEnd != std::string::npos)
            {
                // Wir extrahieren den kompletten Block einer Episode
                std::string itemBlock = streamBuffer.substr(itemStart, itemEnd - itemStart + 7);

                PodcastEpisode ep;
                ep.title = MiniRSS::ExtractTag(itemBlock, "title");
                ep.author = MiniRSS::ExtractTag(itemBlock, "itunes:author"); // Oft mit Prefix
                if (ep.author.empty())
                    ep.author = MiniRSS::ExtractTag(itemBlock, "author");

                ep.audioUrl = MiniRSS::ExtractEnclosure(itemBlock);

                if (!ep.audioUrl.empty())
                {
                    RemoveFromString(ep.title, "\\t");
                    RemoveFromString(ep.title, "\\r");
                    RemoveFromString(ep.title, "\\n");
                    RemoveFromString(ep.title, "<![CDATA[");
                    RemoveFromString(ep.title, "]]");
                    SongNode *sn = new SongNode;
                    sprintf(sn->name, "%s", ep.title.c_str());
                    UTF8ToAmiga(sn->name);
                    sn->name[sizeof(sn->name) - 1] = '\0'; // Null-Terminierung sicherstellen
                    sn->node.ln_Name = sn->name;
                    sn->node.ln_Type = NT_USER;
                    sn->OriginalIndex = nextIndex++;
                    strncpy(sn->path, ep.audioUrl.c_str(), 255);
                    sn->path[254] = '\0';
                    AddHead(&songList, (struct Node *)sn);
                }

                // Verarbeiteten Teil löschen
                streamBuffer.erase(0, itemEnd + 7);
            }
            else
            {
                break; // Warten auf restliche Daten
            }
        }
    }
    // Nachdem der gesamte Feed geladen wurde:
    struct SongNode *node;
    int idx = 0;

    // Wir laufen einmal durch die nun korrekt sortierte Liste
    for (node = (struct SongNode *)songList.lh_Head;
         node->node.ln_Succ;
         node = (struct SongNode *)node->node.ln_Succ)
        node->OriginalIndex = idx++;

    if (m_amiSSL)
    {
        m_amiSSL->CleanupAll();
        delete m_amiSSL;
        m_amiSSL = NULL;
    }
}
