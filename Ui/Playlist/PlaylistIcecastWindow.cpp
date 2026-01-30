#include "PlaylistWindow.hpp"
#include "../../Shared/Icecast.hpp"

void PlaylistWindow::getIceCastList()
{
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
    clearList();
    Icecast *_icecast = new Icecast();
    _icecast->FetchList(m_SongList, m_searchBuffer);
    delete _icecast;
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
}
