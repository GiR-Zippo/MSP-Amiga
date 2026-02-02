#ifndef __MINIRSS_HPP__
#define __MINIRSS_HPP__
#include "../Common.h"

struct PodcastEpisode
{
    std::string title;
    std::string author;
    std::string audioUrl;
};

class MiniRSS
{
public:
    static std::string ExtractTag(const std::string &data, const std::string &tag)
    {
        std::string openTag = "<" + tag + ">";
        std::string closeTag = "</" + tag + ">";

        size_t start = data.find(openTag);
        if (start == std::string::npos)
            return "";
        start += openTag.length();

        size_t end = data.find(closeTag, start);
        if (end == std::string::npos)
            return "";

        return data.substr(start, end - start);
    }

    static std::string ExtractEnclosure(const std::string &data)
    {
        size_t pos = data.find("<enclosure");
        if (pos == std::string::npos)
            return "";

        size_t urlPos = data.find("url=\"", pos);
        if (urlPos == std::string::npos)
            return "";
        urlPos += 5;

        size_t urlEnd = data.find("\"", urlPos);
        if (urlEnd == std::string::npos)
            return "";

        return data.substr(urlPos, urlEnd - urlPos);
    }
};

#endif