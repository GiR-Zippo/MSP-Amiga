#ifndef __ICECAST_HPP__
#define __ICECAST_HPP__

#include "AmiSSL.hpp"
class Icecast
{
    public:
        Icecast();
        ~Icecast();

        /// @brief Fetch list
        /// @param songList the Amiga-List
        /// @param filter the search filter
        void FetchList(List &songList, const char* filter);
    private:
        AmiSSL *m_amiSSL;

        /// @brief Entry exists in list
        /// @param targetList the amiga list
        /// @param searchName the name
        /// @return true if exists
        bool existsInPlaylist(struct List* targetList, const char* searchName);

        /*********************************************************/
        /***                     Misc Stuff                    ***/
        /*********************************************************/
        /// @brief Update an entry in the list
        /// @param targetList the amiga list
        /// @param searchName the name
        /// @param newNode the new generated node to insert
        /// @param preferHttp remove https doubles if true
        void updateStationInPlaylist(struct List* targetList, const char* searchName, struct SongNode* newNode, bool preferHttp);
};
#endif