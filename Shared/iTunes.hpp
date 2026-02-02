#ifndef __ITUNES_HPP__
#define __ITUNES_HPP__

#include "../Common.h"
#include "AmiSSL.hpp"

class iTunes
{
    public:
        iTunes();
        ~iTunes();
        void FetchList(List &songList, const char* filter);
        void FetchRSS(List &songList, const char* url);

    private:
        AmiSSL *m_amiSSL;
};
#endif 