#ifndef __ICECAST_HPP__
#define __ICECAST_HPP__

#include "AmiSSL.hpp"
class Icecast
{
    public:
        Icecast();
        ~Icecast();

        void FetchList(List &songList);
    private:
        AmiSSL *m_amiSSL;
        std::string getJsonValue(const std::string &json, const std::string &key);
        int getJsonIntValue(const std::string &json, const std::string &key);
        void removeFromString(std::string &src, std::string arg);

};
#endif