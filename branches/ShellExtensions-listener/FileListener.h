#ifndef FILEEXPLORERUPDATER_H_INCLUDED
#define FILEEXPLORERUPDATER_H_INCLUDED

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
	#include <wx/wx.h>
    #include <wx/treectrl.h>
#endif

#include <wx/treectrl.h>


#include <vector>

class FileExplorer;

BEGIN_DECLARE_EVENT_TYPES()
// SIMPLE wxCommandEvent DERIVED CUSTOM EVENTS THAT USE THE BUILTIN EVT_COMMAND EVENT TABLE ENTRY
DECLARE_LOCAL_EVENT_TYPE(wxEVT_NOTIFY_EXEC_REQUEST, -1)
DECLARE_LOCAL_EVENT_TYPE(wxEVT_NOTIFY_UPDATE_TREE, -1)
END_DECLARE_EVENT_TYPES()


//WX_DEFINE_ARRAY_INT(int, wxArrayInt);


struct FileData
{
    wxString name;
    int state;
    // could also add full path, modified time
};

typedef std::vector<FileData> FileDataVec;

class FileExplorerUpdater: public wxThread
{
    FileExplorerUpdater(FileExplorer *fe) : wxThread(wxTHREAD_JOINABLE) { m_fe=fe; return;}
    FileDataVec m_adders;
    FileDataVec m_removers;
    void Update(const wxTreeItemId &ti); //call on main thread to do the background magic
private:
    FileExplorer *m_fe;
    FileDataVec m_treestate;
    FileDataVec m_currentstate;
    wxString m_path;
    virtual void *Entry();
    void GetTreeState(const wxTreeItemId &ti);
    bool GetCurrentState(const wxString &path);
    void CalcChanges(); //creates the vector of adders and removers
};


#endif //FILEEXPLORERUPDATER_H_INCLUDED
