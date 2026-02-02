#include "PlaylistWindow.hpp"

/*********************************************************/
/***               Playlist Common Stuff               ***/
/*********************************************************/

void PlaylistWindow::AddEntry(std::string name, std::string fullPath)
{
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)-1, TAG_DONE);

    // letzten Index ziehen
    int nextIndex = 0;
    struct SongNode *lastNode = (struct SongNode *)m_SongList.lh_TailPred;
    if (lastNode->node.ln_Pred)
        nextIndex = lastNode->OriginalIndex + 1;

    SongNode *sn = new SongNode;
    strncpy(sn->name, name.c_str(), sizeof(sn->name) - 1);
    sn->name[sizeof(sn->name) - 1] = '\0'; // Null-Terminierung sicherstellen
    sn->node.ln_Name = sn->name;
    sn->OriginalIndex = nextIndex;
    strncpy(sn->path, fullPath.c_str(), 255);
    sn->path[255] = '\0';

    AddTail(&m_SongList, (struct Node *)sn);

    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL,
                      GTLV_Labels, (IPTR)&m_SongList,
                      GTLV_ShowSelected, NULL,
                      TAG_DONE);
}

void PlaylistWindow::clearList()
{
    struct Node *n;
    while ((n = RemHead(&m_SongList)))
    {
        SongNode *sn = (SongNode *)n;
        if (sn->node.ln_Name)
            sn->node.ln_Name = NULL;
        delete sn;
    }
    while ((n = RemHead(&m_HiddenList)))
    {
        SongNode *sn = (SongNode *)n;
        if (sn->node.ln_Name)
            sn->node.ln_Name = NULL;
        delete sn;
    }
    NewList(&m_SongList);
    NewList(&m_HiddenList);
    m_SelectedIndex = -1;
}

void PlaylistWindow::clearSearch()
{
    m_searchBuffer[0] = '\0';
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_SEARCH], m_Window, NULL,
    GTST_String, (Tag)"", 
    TAG_DONE);
}

Node *PlaylistWindow::findNode(int index)
{
    if (index == -1)
        return NULL;

    struct Node *n = (struct Node *)m_SongList.lh_Head;
    for (int i = 0; i < index && n->ln_Succ; i++)
        n = n->ln_Succ;

    if (n && n->ln_Succ) // ln_Succ Check stellt sicher, dass es nicht der Tail-Wächter ist
        return n;
    return NULL;
}

void PlaylistWindow::filter(const char *searchCrit)
{
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
    struct SongNode *node;
    struct List tmpList;
    NewList(&tmpList); // Temporärer Container

    while ((node = (struct SongNode *)RemHead(&m_SongList)))
        AddTail(&tmpList, (struct Node *)node);
    while ((node = (struct SongNode *)RemHead(&m_HiddenList)))
        AddTail(&tmpList, (struct Node *)node);

    bool isSearchEmpty = (!searchCrit || searchCrit[0] == '\0');
    while ((node = (struct SongNode *)RemHead(&tmpList)))
    {
        bool visible = isSearchEmpty ? TRUE : containsString(node->name, searchCrit);
        node->Visible = visible;

        if (visible)
            AddTail(&m_SongList, (struct Node *)node);
        else
            AddTail(&m_HiddenList, (struct Node *)node);
    }
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
}

void PlaylistWindow::sortPlaylist()
{
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, -1, TAG_DONE);
    struct SongNode *node;
    BOOL swapped;

    if (!m_SongList.lh_Head->ln_Succ->ln_Succ)
    {
        GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
        return;
    }

    do
    {
        swapped = FALSE;
        node = (struct SongNode *)m_SongList.lh_Head;
        while (node->node.ln_Succ->ln_Succ)
        {
            struct SongNode *nextNode = (struct SongNode *)node->node.ln_Succ;

            if (node->OriginalIndex > nextNode->OriginalIndex)
            {
                Remove((struct Node *)nextNode);
                Insert((struct List *)&m_SongList, (struct Node *)nextNode, node->node.ln_Pred);
                swapped = TRUE;
            }
            else
                node = nextNode;
        }
    } while (swapped);
    GT_SetGadgetAttrs(m_Gads[PLAYLIST_LIST], m_Window, NULL, GTLV_Labels, (IPTR)&m_SongList, TAG_DONE);
}

void PlaylistWindow::showAll()
{
    filter("");
    sortPlaylist();
}