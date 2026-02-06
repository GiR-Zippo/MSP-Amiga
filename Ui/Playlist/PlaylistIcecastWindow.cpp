#include "PlaylistWindow.hpp"
#include "../../Shared/Icecast.hpp"
#include "../../Shared/iTunes.hpp"
#include "../../PlaybackRunner.hpp"

void PlaylistWindow::getIceCastList()
{
    PlaybackRunner::getInstance()->StopPlayback();
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
    clearList();
    Icecast *_icecast = new Icecast();
    _icecast->FetchList(m_SongList, m_searchBuffer);
    delete _icecast;
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
}

void PlaylistWindow::getiTunesList()
{
    PlaybackRunner::getInstance()->StopPlayback();
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
    clearList();
    iTunes *_itunes = new iTunes();
    _itunes->FetchList(m_SongList, m_searchBuffer);
    delete _itunes;
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
}

void PlaylistWindow::getiTunesRSSList()
{
    PlaybackRunner::getInstance()->StopPlayback();
    clearSearch();
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
    struct Node *node;
    node = findNode(m_SelectedIndex);
    if (!node)
        return;

    SongNode *sn = (SongNode *)node;
    strncpy(selectedPath, sn->path, 255);
    clearList();

    iTunes *_itunes = new iTunes();
    _itunes->FetchRSS(m_SongList, selectedPath);
    delete _itunes;
    m_playlistMode = 0;
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_MODE], m_Window, NULL, 
                  GTCY_Active, 0, 
                  TAG_DONE);
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
}