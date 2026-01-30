#ifndef __ICECAST_HPP__
#define __ICECAST_HPP__

#include "AmiSSL.hpp"
class Icecast
{
    public:
        Icecast();
        ~Icecast();

        void FetchList(List &songList, const char* filter);
    private:
        AmiSSL *m_amiSSL;
        std::string getJsonValue(const std::string &json, const std::string &key);
        int getJsonIntValue(const std::string &json, const std::string &key);
        void removeFromString(std::string &src, std::string arg);
        std::string simpleEncode(const char* src);
        bool ExistsInPlaylist(struct List* targetList, const char* searchName);
        void UpdateStationInPlaylist(struct List* targetList, const char* searchName, struct SongNode* newNode, bool preferHttp);
};
#endif