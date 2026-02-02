#ifndef __MINIJSON_HPP__
#define __MINIJSON_HPP__
#include "../Common.h"

class MiniJson
{
    public:
        /*********************************************************/
        /***                   Json Stuff                      ***/
        /*********************************************************/

        /// @brief Get String vom json: getValue(json, "feed::entry::title")
        static bool GetValue(const std::string &json, const std::string &path, std::string &result);
        
        /// @brief Get Int vom json: getValue(json, "feed::entry::title")
        static bool GetIntValue(const std::string &json, const std::string &path, int &result);

        static std::string getArrayItem(const std::string& json, const std::string& key, int index) {
    std::string arrayData = getRawObject(json, key); // Holt den Inhalt von [ ... ]
    if (arrayData.empty()) return "";

    size_t pos = 0;
    for (int i = 0; i <= index; ++i) {
        pos = arrayData.find("{", pos);
        if (pos == std::string::npos) return "";
        
        // Finde das Ende dieses Objekts
        int count = 0;
        size_t j = pos;
        for (; j < arrayData.length(); ++j) {
            if (arrayData[j] == '{') count++;
            else if (arrayData[j] == '}') count--;
            if (count == 0) {
                if (i == index) return arrayData.substr(pos, j - pos + 1);
                pos = j + 1;
                break;
            }
        }
    }
    return "";
}
    private:
        /// @brief Findet ein Unter-Objekt { ... } oder Array [ ... ]
        static std::string getRawObject(const std::string &json, const std::string &key);

        /// @brief Extrahiert den tatsächlichen Wert (String/Zahl)
        static std::string extractLeaf(const std::string &json, const std::string &key);

        /// @brief Unescape den string und verwerfe Steuerzeichen
        static std::string unescape(std::string s);
};
#endif