#include "MiniJson.hpp"

bool MiniJson::GetValue(const std::string &json, const std::string &path, std::string &result)
{
    std::string currentJson = json;
    std::string delimiter = "::";
    size_t start = 0;
    size_t end = path.find(delimiter);

    // Wir hangeln uns durch die Pfad-Ebenen
    while (end != std::string::npos)
    {
        std::string key = path.substr(start, end - start);
        currentJson = getRawObject(currentJson, key);

        if (currentJson.empty())
            return false;

        start = end + delimiter.length();
        end = path.find(delimiter, start);
    }

    // Das letzte Element im Pfad extrahieren
    result = extractLeaf(currentJson, path.substr(start));
    return true;
}

bool MiniJson::GetIntValue(const std::string &json, const std::string &path, int &result)
{
    std::string currentJson = json;
    std::string delimiter = "::";
    size_t start = 0;
    size_t end = path.find(delimiter);

    // Wir hangeln uns durch die Pfad-Ebenen
    while (end != std::string::npos)
    {
        std::string key = path.substr(start, end - start);
        currentJson = getRawObject(currentJson, key);

        if (currentJson.empty())
            return false;

        start = end + delimiter.length();
        end = path.find(delimiter, start);
    }

    // Das letzte Element im Pfad extrahieren
    result = atoi(extractLeaf(currentJson, path.substr(start)).c_str());
    return true;
}

std::string MiniJson::getRawObject(const std::string &json, const std::string &key)
{
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos)
        return "";

    size_t colonPos = json.find(":", keyPos + searchKey.length());
    if (colonPos == std::string::npos)
        return "";

    size_t valStart = json.find_first_not_of(" \t\n\r", colonPos + 1);
    if (valStart == std::string::npos)
        return "";

    // Hier fängt das Objekt oder Array an
    int bracketCount = 0;
    char openBracket = json[valStart];
    char closeBracket;

    if (openBracket == '{')
        closeBracket = '}';
    else if (openBracket == '[')
        closeBracket = ']';
    else
        return ""; // Kein Objekt/Array

    size_t i = valStart;
    for (; i < json.length(); ++i)
    {
        if (json[i] == openBracket)
            bracketCount++;
        else if (json[i] == closeBracket)
            bracketCount--;

        if (bracketCount == 0)
            return json.substr(valStart, i - valStart + 1);
    }
    return "";
}

std::string MiniJson::extractLeaf(const std::string &json, const std::string &key)
{
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos)
        return "";

    size_t colonPos = json.find(":", keyPos + searchKey.length());
    size_t valStart = json.find_first_not_of(" \t\n\r", colonPos + 1);

    if (json[valStart] == '\"')
    {
        size_t valEnd = json.find("\"", valStart + 1);
        return unescape(json.substr(valStart + 1, valEnd - valStart - 1));
    }
    else
    {
        size_t valEnd = json.find_first_of(", \t\n\r}", valStart);
        return json.substr(valStart, valEnd - valStart);
    }
}

std::string MiniJson::unescape(std::string s)
{
    std::string res;
    res.reserve(s.length());

    for (size_t i = 0; i < s.length(); ++i)
    {
        if (s[i] == '\\' && i + 1 < s.length())
        {
            char next = s[++i];
            switch (next)
            {
            case '/':
                res += '/';
                break;
            case '\"':
                res += '\"';
                break;
            case '\\':
                res += '\\';
                break;
            case 't':
                res += ' ';
                break; // Tab durch Leerzeichen ersetzen
            case 'n':
                res += ' ';
                break; // Newline durch Leerzeichen ersetzen
            case 'r':
                res += ' ';
                break; // Carriage Return durch Leerzeichen ersetzen
                // Alles andere (wie \uXXXX) ignorieren wir hier der Einfachheit halber
            }
        }
        else
        {
            unsigned char c = (unsigned char)s[i];
            if (c == '\t' || c == '\n' || c == '\r')
                res += ' ';   // Tab/CR/LF durch Leerzeichen ersetzen
            else if (c >= 32) // Nur druckbare Zeichen übernehmen (verhindert Amiga-Glitches)
                res += (char)c;
        }
    }
    return res;
}