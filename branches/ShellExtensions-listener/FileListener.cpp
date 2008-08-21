#include "FileListener.h"
#include "FileExplorer.h"
#include "se_globals.h"

DEFINE_EVENT_TYPE(wxEVT_NOTIFY_UPDATE_TREE)
DEFINE_EVENT_TYPE(wxEVT_NOTIFY_EXEC_REQUEST)

void FileExplorerUpdater::Update(const wxTreeItemId &ti)
{
    m_path=m_fe->GetFullPath(ti);
    GetTreeState(ti);
    if(Create()==wxTHREAD_NO_ERROR)
        Run();
}

void *FileExplorerUpdater::Entry()
{
    GetCurrentState(m_path);
    CalcChanges();
    wxCommandEvent ne(wxEVT_NOTIFY_UPDATE_TREE,0);
    m_fe->AddPendingEvent(ne);
    return NULL;
}


// Call from main thread prior to thread entry point
void FileExplorerUpdater::GetTreeState(const wxTreeItemId &ti)
{
    wxTreeItemIdValue cookie;
    wxTreeItemId ch=m_fe->m_Tree->GetFirstChild(ti,cookie);
    m_treestate.clear();
    while(ch.IsOk())
    {
        FileData fd;
        fd.name=m_fe->m_Tree->GetItemText(ch);
        fd.state=m_fe->m_Tree->GetItemImage(ch);
        m_treestate.push_back(fd);
        ch=m_fe->m_Tree->GetNextChild(ti,cookie);
    }
}


// called from Thread::Entry
bool FileExplorerUpdater::GetCurrentState(const wxString &path)
{
    wxString wildcard=m_fe->m_WildCards->GetValue();
//    m_Tree->DeleteChildren(ti); //Don't delete everything, we will add new files/dirs, remove deleted files/dirs and update state of all other files.

    m_currentstate.clear();
    wxDir dir(path);

    if (!dir.IsOpened())
    {
        // deal with the error here - wxDir would already log an error message
        // explaining the exact reason for the failure
        return false;
    }
    wxString filename;
    int flags=wxDIR_FILES|wxDIR_DIRS;
    if(m_fe->m_show_hidden)
        flags|=wxDIR_HIDDEN;
    VCSstatearray sa;
    bool is_vcs=false;
    bool is_cvs=false;

    // TODO: THIS NEEDS TO BE CALLED FROM MAIN THREAD BUT CAN'T BE UI-BLOCKING (CAN'T CALL wxExecute FROM THREADS)
    // TODO: IDEALLY THE VCS COMMAND LINE PROGRAM SHOULD BE CALLED ONCE ON THE BASE DIRECTORY (SINCE THEY ARE USUALLY RECURSIVE) TO AVOID REPEATED CALLS FOR SUB-DIRS
/*    if(m_fe->m_parse_svn)
        if(m_fe->ParseSVNstate(path,sa))
            is_vcs=true;
    if(m_fe->m_parse_bzr && !is_vcs)
        if(m_fe->ParseBZRstate(path,sa))
            is_vcs=true;
    if(m_fe->m_parse_hg && !is_vcs)
        if(m_fe->ParseHGstate(path,sa))
            is_vcs=true;
    if(m_fe->m_parse_cvs && !is_vcs)
        if(m_fe->ParseCVSstate(path,sa))
        {
            is_vcs=true;
            is_cvs=true;
        }*/

    bool cont = dir.GetFirst(&filename,wxEmptyString,flags);
    while ( cont )
    {
        int itemstate;
        bool match=true;
        wxString fullpath=wxFileName(path,filename).GetFullPath();
        if(wxFileName::DirExists(fullpath))
            itemstate=fvsFolder;
        if(wxFileName::FileExists(fullpath))
        {
            if(is_vcs&&!is_cvs)
                itemstate=fvsVcUpToDate;
            else
                itemstate=fvsNormal;
            wxFileName fn(path,filename);
#if wxCHECK_VERSION(2,8,0)
            if(!fn.IsFileWritable())
                itemstate=fvsReadOnly;
#endif
            int deli=-1;
            for(size_t i=0;i<sa.GetCount();i++)
            {
                if(fn.SameAs(sa[i].path))
                {
                    itemstate=sa[i].state;
                    deli=i;
                    break;
                }
            }
            if(deli>=0)
                sa.RemoveAt(deli);
//            cbMessageBox(filename);
            if(!WildCardListMatch(wildcard,filename))
                match=false;
        }
        if(match)
        {
            FileData fd;
            fd.name=filename;
            fd.state=itemstate;
            m_currentstate.push_back(fd);
        }
        cont = dir.GetNext(&filename);
    }
    return true;
}


void FileExplorerUpdater::CalcChanges()
{
    m_adders.clear();
    m_removers.clear();
    FileDataVec::iterator tree_it=m_treestate.begin();
    while(tree_it!=m_treestate.end())
    {
        bool match=false;
        for(FileDataVec::iterator it=m_currentstate.begin();it!=m_currentstate.end();it++)
            if(it->name==tree_it->name)
            {
                match=true;
                if(it->state!=tree_it->state)
                {
//                    cbMessageBox(wxString::Format(_T("Conflicting state %s"),it->name.c_str()));
                    m_adders.push_back(*it);
                    m_removers.push_back(*tree_it);
                }
//                cbMessageBox(wxString::Format(_T("Already present %s"),it->name.c_str()));
                m_currentstate.erase(it);
                tree_it=m_treestate.erase(tree_it);
                break;
            }
        if(!match)
            tree_it++;
    }
    for(FileDataVec::iterator tree_it=m_treestate.begin();tree_it!=m_treestate.end();tree_it++)
        m_removers.push_back(*tree_it);
    for(FileDataVec::iterator it=m_currentstate.begin();it!=m_currentstate.end();it++)
        m_adders.push_back(*it);

}
