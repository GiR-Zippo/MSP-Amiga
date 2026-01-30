#include "StreamClient.hpp"

/// @brief Decodes the URl, sets m_port m_host m_path and m_isHTTP
/// @param url
void NetworkStream::decodeUrlData(std::string url)
{
    // to lowercase
    stringToLower(url);

    // set the port and proto
    if (strstr(url.c_str(), "http://"))
    {
        m_isHTTP = true;
        m_port = 80;
    }
    else
    {
        m_isHTTP = false;
        m_port = 443;
    }

    size_t pos = url.find("://");
    size_t hostStart = pos + 3;
    size_t slashPos = url.find('/', hostStart);
    size_t portSep = url.find(':', hostStart);
    size_t hostEnd = (slashPos != std::string::npos) ? slashPos : url.length();

    // Port extrahieren (nur wenn ':' VOR dem ersten '/' kommt)
    if (portSep != std::string::npos && (slashPos == std::string::npos || portSep < slashPos))
    {
        // Port vorhanden
        std::string portStr = url.substr(portSep + 1, hostEnd - portSep - 1);
        m_port = atoi(portStr.c_str());
        strncpy(m_host, url.substr(hostStart, portSep - hostStart).c_str(), 127);
    }
    else
        // Kein Port im String -> Standardport
        strncpy(m_host, url.substr(hostStart, hostEnd - hostStart).c_str(), 127);

    if (slashPos != std::string::npos)
        strncpy(m_path, url.substr(slashPos).c_str(), 255);
    else
        strncpy(m_path, "/", 255);

    printf("Decoded to: %s %u %s\n", m_host, m_port, m_path);
}

/// @brief hendles the server response
/// @param response
/// @return true if everything okay
bool NetworkStream::handleServerResponse(std::string response)
{
    printf("%s\n", response.c_str());
    m_codec = 255;
    if (strstr(response.c_str(), "200 OK"))
    {
        //get Icy-MetaData for titleinformation
        if (strstr(response.c_str(), "icy-metaint:"))
        {
            size_t locPos = response.find("icy-metaint:");
            if (locPos != std::string::npos)
            {
                size_t start = locPos + 13;
                size_t end = response.find("\r\n", start);
                m_icyInterval = atoi(response.substr(start, end - start).c_str());
            }
        }
        //get codec-type
        if (strstr(response.c_str(), "Content-Type: audio/mpeg"))
        {
            // Es ist MP3
            m_codec = 0;
            return true;
        }
        else if (strstr(response.c_str(), "Content-Type: audio/aac") ||
                 strstr(response.c_str(), "Content-Type: audio/aacp") ||
                 strstr(response.c_str(), "Content-Type: audio/mp4"))
        {
            // Es ist AAC / AAC+
            m_codec = 1;
            return true;
        }
    }
    else if (strstr(response.c_str(), "302 Found") || strstr(response.c_str(), "301 Moved"))
    {
        // wenn redirect, dann neue URL holen
        size_t locPos = response.find("Location: ");
        if (locPos != std::string::npos)
        {
            size_t start = locPos + 10; // Hinter "Location: "
            size_t end = response.find("\r\n", start);
            decodeUrlData(response.substr(start, end - start));
            m_isHTTP = (response.substr(start, end - start).substr(0, 5) != "https");
            return true;
        }
    }
    else if (strstr(response.c_str(), "400 Bad Request"))
        return false;
    return true;
}