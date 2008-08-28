#include "filesystemmonitor.h"

DEFINE_EVENT_TYPE(wxEVT_MONITOR_NOTIFY)
DEFINE_EVENT_TYPE(wxEVT_MONITOR_NOTIFY2)

wxFileSysMonitorEvent::wxFileSysMonitorEvent(const wxString &mon_dir, int event_type, const wxString &uri): wxNotifyEvent(wxEVT_MONITOR_NOTIFY)
{
    m_mon_dir=mon_dir;
    m_event_type=event_type;
    m_info_uri=wxString(uri.c_str());
}
wxFileSysMonitorEvent::wxFileSysMonitorEvent(const wxFileSysMonitorEvent& c) : wxNotifyEvent(c)
{
    m_mon_dir=wxString(c.m_mon_dir.c_str());
    m_event_type=c.m_event_type;
    m_info_uri=wxString(c.m_info_uri.c_str());
}


#ifdef __WXGTK__

#include <map>
typedef std::map<GnomeVFSMonitorHandle *, wxFileSystemMonitor *> MonMap;
static MonMap m;

#else //WINDOWS

#define DEFAULT_MONITOR_FILTER_WIN32 FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_ATTRIBUTES|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_CREATION

//WIN32 ONLY THREADED CLASS TO HANDLE WAITING ON DIR CHANGES ASYNCHRONOUSLY
class DirMonitorThread : public wxThread
{
public:
    DirMonitorThread(wxEvtHandler *parent, wxArrayString pathnames, bool singleshot, bool subtree, DWORD notifyfilter, DWORD waittime_ms)
        : wxThread(wxTHREAD_JOINABLE)
    { m_parent=parent; m_waittime=waittime_ms; m_subtree=subtree; m_singleshot=singleshot; m_pathnames=pathnames; m_notifyfilter=notifyfilter; m_handles=new HANDLE[m_pathnames.GetCount()];
        return; }
    void *Entry()
    {
        for(unsigned int i=0;i<m_pathnames.GetCount();i++)
            m_handles[i]=::FindFirstChangeNotification(m_pathnames[i].c_str(), m_subtree, DEFAULT_MONITOR_FILTER_WIN32);
        PFILE_NOTIFY_INFORMATION changedata=(PFILE_NOTIFY_INFORMATION)(new char[4096]);
        while(!TestDestroy())
        {
            DWORD result=::MsgWaitForMultipleObjects(m_pathnames.GetCount(),m_handles,false,m_waittime,DEFAULT_MONITOR_FILTER_WIN32);
            if(result!=WAIT_TIMEOUT)
            {
                DWORD chda_len;
                if(false)//::ReadDirectoryChangesW(m_handles[result- WAIT_OBJECT_0], changedata, 4096, m_subtree, DEFAULT_MONITOR_FILTER_WIN32, &chda_len, NULL, NULL))
                {
                    if(chda_len>0)
                    {
                        int off=0;
                        PFILE_NOTIFY_INFORMATION chptr=changedata;
                        do
                        {
                            DWORD a=chptr->Action;
                            //TODO: Convert to the MONITOR_FILE_XXX action types, filtering those that aren't wanted
                            int action=0;
                            switch(a)
                            {
                                case FILE_ACTION_ADDED:
                                case FILE_ACTION_RENAMED_NEW_NAME:
                                    action=MONITOR_FILE_CREATED;
                                    break;
                                case FILE_ACTION_REMOVED:
                                case FILE_ACTION_RENAMED_OLD_NAME:
                                    action=MONITOR_FILE_DELETED;
                                    break;
                                case FILE_ACTION_MODIFIED:
                                    action=MONITOR_FILE_CHANGED;
                                    break;
                            }
                            if(action&m_notifyfilter)
                            {
                                wxString filename(chptr->FileName,chptr->FileNameLength);
                                wxMessageBox(_("changing ")+filename);
                                wxFileSysMonitorEvent e(m_pathnames[result- WAIT_OBJECT_0],action,filename);
                                m_parent->AddPendingEvent(e);
                            }
                            off=chptr->NextEntryOffset;
                            chptr=(PFILE_NOTIFY_INFORMATION)((char*)chptr+off);
                        } while(off>0);
                    }
                    else
                    {
                        //too many changes, tell parent to manually read the directory
                        wxFileSysMonitorEvent e(m_pathnames[result- WAIT_OBJECT_0],MONITOR_TOO_MANY_CHANGES,wxEmptyString);
                        m_parent->AddPendingEvent(e);
                    }
                } else
                {
                    //couldn't read changes, tell parent to manually read the directory
                    //wxCommandEvent e(wxEVT_NOTIFY_UPDATE_TREE);
                    wxFileSysMonitorEvent e(m_pathnames[result- WAIT_OBJECT_0],MONITOR_TOO_MANY_CHANGES,wxEmptyString);
                    m_parent->AddPendingEvent(e);
                }
            }
            for(unsigned int i=0;i<m_pathnames.GetCount();i++)
                if(!FindNextChangeNotification(m_handles[i]))
                    break;
            if(m_singleshot)
                break;
        }
        delete changedata;
        for(unsigned int i=0;i<m_pathnames.GetCount();i++)
            FindCloseChangeNotification(m_handles[i]);
//        wxFileSysMonitorEvent e(wxEmptyString,MONITOR_FINISHED,wxEmptyString);
//        m_parent->AddPendingEvent(e);
        return NULL;
    }
    ~DirMonitorThread()
    {
        if(IsRunning())
            Delete();
        delete [] m_handles;
    }
    DWORD m_waittime;
    bool m_subtree;
    bool m_singleshot;
    wxArrayString m_pathnames;
    DWORD m_notifyfilter;
    HANDLE *m_handles;
    wxEvtHandler *m_parent;
};

#endif

BEGIN_EVENT_TABLE(wxFileSystemMonitor, wxEvtHandler)
    EVT_MONITOR_NOTIFY(0, wxFileSystemMonitor::OnMonitorEvent)
//    EVT_COMMAND(0, wxEVT_MONITOR_NOTIFY2, FileExplorer::OnMonitorEvent2)
END_EVENT_TABLE()

#ifdef __WXGTK__
void wxFileSystemMonitor::MonitorCallback(GnomeVFSMonitorHandle *handle, const gchar *monitor_uri, const gchar *info_uri, GnomeVFSMonitorEventType event_type, gpointer user_data)
{
    wxMessageBox(_T("monitor event"));
    if(m.find(handle)!=m.end())
        m[handle]->Callback((wxString *)user_data, event_type, wxString::FromUTF8(info_uri));
    //TODO: ELSE WARNING/ERROR
}

void wxFileSystemMonitor::Callback(wxString *mon_dir,  int EventType, const wxString &info_uri)
{
    //TODO: DETERMINE WHETHER THIS CALLBACK HAPPENS ON A THREAD?
    //TODO: Convert to gnomevfs events to the MONITOR_FILE_XXX action types, filtering those that aren't wanted
    int action=0;
    switch(EventType)
    {
        case GNOME_VFS_MONITOR_EVENT_CHANGED:
            action=MONITOR_FILE_CHANGED;
            break;
        case GNOME_VFS_MONITOR_EVENT_DELETED:
            action=MONITOR_FILE_DELETED;
            break;
        case GNOME_VFS_MONITOR_EVENT_STARTEXECUTING:
            action=MONITOR_FILE_STARTEXEC;
            break;
        case GNOME_VFS_MONITOR_EVENT_STOPEXECUTING:
            action=MONITOR_FILE_STOPEXEC;
            break;
        case GNOME_VFS_MONITOR_EVENT_CREATED:
            action=MONITOR_FILE_CREATED;
            break;
        case GNOME_VFS_MONITOR_EVENT_METADATA_CHANGED:
            action=MONITOR_FILE_ATTRIBUTES;
            break;
    }
    if(action&m_eventfilter)
    {
        wxFileSysMonitorEvent e(mon_dir->c_str(),action,info_uri);
        this->AddPendingEvent(e);
    }
}
#endif


void wxFileSystemMonitor::OnMonitorEvent(wxFileSysMonitorEvent &e)
{
    if(m_parent)
        m_parent->AddPendingEvent(e);
}


wxFileSystemMonitor::wxFileSystemMonitor(wxEvtHandler *parent, const wxArrayString &uri, int eventfilter)
{
    if(!gnome_vfs_initialized())
        gnome_vfs_init();
    m_parent=parent;
    m_uri=uri;
    m_eventfilter=eventfilter;
}

bool wxFileSystemMonitor::Start()
{
#ifdef __WXGTK__
    for(unsigned int i=0;i<m_uri.GetCount();i++)
    {
        GnomeVFSMonitorHandle *h;
        if(gnome_vfs_monitor_add(&h, m_uri[i].ToUTF8(), GNOME_VFS_MONITOR_DIRECTORY, &wxFileSystemMonitor::MonitorCallback, &m_uri[i])==GNOME_VFS_OK)
        {
            m_h.push_back(h);
            m[h]=this;
        } else
        {
            m_h.push_back(NULL);
            wxMessageBox(_T("fail ")+m_uri[i]);
        }
    }
#else
    m_monitorthread=new DirMonitorThread(this, m_uri, false, false, m_eventfilter, 100);
    m_monitorthread->Create();
    m_monitorthread->Run();
#endif
    return true;
}

wxFileSystemMonitor::~wxFileSystemMonitor()
{
#ifdef __WXGTK__
    for(unsigned int i=0;i<m_uri.GetCount();i++)
    {
        if(m_h[i])
        {
            gnome_vfs_monitor_cancel(m_h[i]);
            m.erase(m_h[i]);
        }
    }
#else
    delete m_monitorthread;
#endif
}
