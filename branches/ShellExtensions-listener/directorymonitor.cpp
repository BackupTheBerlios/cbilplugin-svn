#include "directorymonitor.h"
#include <vector>
#include "se_globals.h"

static int mon_count=0;

DEFINE_EVENT_TYPE(wxEVT_MONITOR_NOTIFY)
DEFINE_EVENT_TYPE(wxEVT_MONITOR_NOTIFY2)

wxDirectoryMonitorEvent::wxDirectoryMonitorEvent(const wxString &mon_dir, int event_type, const wxString &uri): wxNotifyEvent(wxEVT_MONITOR_NOTIFY)
{
    m_mon_dir=mon_dir;
    m_event_type=event_type;
    m_info_uri=wxString(uri.c_str());
}
wxDirectoryMonitorEvent::wxDirectoryMonitorEvent(const wxDirectoryMonitorEvent& c) : wxNotifyEvent(c)
{
    m_mon_dir=wxString(c.m_mon_dir.c_str());
    m_event_type=c.m_event_type;
    m_info_uri=wxString(c.m_info_uri.c_str());
}


#ifdef __WXGTK__

#include <libgnomevfs/gnome-vfs.h>
#include <map>
typedef std::map<GnomeVFSMonitorHandle *, DirMonitorThread *> MonMap;
static MonMap m;

class DirMonitorThread : public wxThread
{
public:
    DirMonitorThread(wxEvtHandler *parent, wxArrayString pathnames, bool singleshot, bool subtree, int notifyfilter, int waittime_ms)
        : wxThread(wxTHREAD_JOINABLE)
    { m_parent=parent; m_waittime=waittime_ms; m_subtree=subtree; m_singleshot=singleshot; m_pathnames=pathnames; m_notifyfilter=notifyfilter;
        return; }
    void *Entry()
    {
        context=g_main_context_new ();
        loop=g_main_loop_new(context,FALSE);

        for(unsigned int i=0;i<m_pathnames.GetCount();i++)
        {
            GnomeVFSMonitorHandle *h;
            if(gnome_vfs_monitor_add(&h, m_pathnames[i].ToUTF8(), GNOME_VFS_MONITOR_DIRECTORY, &DirMonitorThread::MonitorCallback, &m_pathnames[i])==GNOME_VFS_OK)
            {
                m_h.push_back(h);
                m[h]=this;
            } else
            {
                m_h.push_back(NULL);
                //TODO: Log an error
                //wxMessageBox(_T("fail ")+m_pathnames[i]);
            }
        }
        //TODO: Add a timer for killing singleshot instances

        g_main_loop_run(loop);

        for(unsigned int i=0;i<m_pathnames.GetCount();i++)
        {
            if(m_h[i])
            {
                gnome_vfs_monitor_cancel(m_h[i]);
                m.erase(m_h[i]);
            }
        }

        g_main_context_unref(context);
        return NULL;
    }
    ~DirMonitorThread()
    {
        g_main_loop_quit(loop);
        if(IsRunning())
            Delete();
    }
    void Callback(const wxString *mon_dir, int EventType, const wxString &uri)
    {
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
        if(action&m_notifyfilter)
        {
            wxDirectoryMonitorEvent e(mon_dir->c_str(),action,uri);
            m_parent->AddPendingEvent(e);
        }
    }
    static void MonitorCallback(GnomeVFSMonitorHandle *handle, const gchar *monitor_uri, const gchar *info_uri, GnomeVFSMonitorEventType event_type, gpointer user_data)
    {
//        wxMessageBox(_T("monitor event"));
        if(m.find(handle)!=m.end())
            m[handle]->Callback((wxString *)user_data, event_type, wxString::FromUTF8(info_uri));
        //TODO: ELSE WARNING/ERROR
    }

    GMainContext *context;
    GMainLoop *loop;
    int m_waittime;
    bool m_subtree;
    bool m_singleshot;
    wxArrayString m_pathnames;
    int m_notifyfilter;
    std::vector<GnomeVFSMonitorHandle *> m_h;
    wxEvtHandler *m_parent;
};

#endif
#ifdef __WXMSW__

#define DEFAULT_MONITOR_FILTER_WIN32 FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_ATTRIBUTES|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_CREATION

//WIN32 ONLY THREADED CLASS TO HANDLE WAITING ON DIR CHANGES ASYNCHRONOUSLY
class DirMonitorThread : public wxThread
{
public:
    DirMonitorThread(wxEvtHandler *parent, wxArrayString pathnames, bool singleshot, bool subtree, DWORD notifyfilter, DWORD waittime_ms)
        : wxThread(wxTHREAD_JOINABLE)
    {
        m_parent=parent;
        m_waittime=waittime_ms;
        m_subtree=subtree;
        m_singleshot=singleshot;
        for(unsigned int i=0;i<pathnames.GetCount();i++)
            m_pathnames.Add(pathnames[i].c_str());
        m_notifyfilter=notifyfilter;
        m_handles=new HANDLE[m_pathnames.GetCount()];
        return;

    }
    void *Entry()
    {
        wxMessageBox(_("Thread entry"));
        bool handle_fail=false;
        for(unsigned int i=0;i<m_pathnames.GetCount();i++)
        {
            m_handles[i]=::FindFirstChangeNotification(m_pathnames[i].c_str(), m_subtree, DEFAULT_MONITOR_FILTER_WIN32);
            if(m_handles[i]==INVALID_HANDLE_VALUE)
            {
                handle_fail=true;
                wxMessageBox(_("Big ERROR!!"));
            }

        }
        //TODO: Error checking
        PFILE_NOTIFY_INFORMATION changedata=(PFILE_NOTIFY_INFORMATION)(new char[4096]);
        while(!handle_fail && !TestDestroy())
        {
            DWORD result=::MsgWaitForMultipleObjects(m_pathnames.GetCount(),m_handles,false,m_waittime,DEFAULT_MONITOR_FILTER_WIN32);
            if(result!=WAIT_TIMEOUT)
            {
                DWORD chda_len;
                HANDLE hDir = ::CreateFile(m_pathnames[result- WAIT_OBJECT_0].c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
                if(hDir==INVALID_HANDLE_VALUE)
                {
                    wxMessageBox(_T("Handle failure"));
                    handle_fail=true;
                }
                else
                if(::ReadDirectoryChangesW(hDir, changedata, 4096, m_subtree, DEFAULT_MONITOR_FILTER_WIN32, &chda_len, NULL, NULL))
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
                                wxString filename(chptr->FileName,chptr->FileNameLength/2); //TODO: check the div by 2
                                wxDirectoryMonitorEvent e(m_pathnames[result- WAIT_OBJECT_0],action,filename);
                                m_parent->AddPendingEvent(e);
                            }
                            off=chptr->NextEntryOffset;
                            chptr=(PFILE_NOTIFY_INFORMATION)((char*)chptr+off);
                        } while(off>0);
                        //wxMessageBox(_T("all changes read successfully"));
                    }
                    else
                    {
                        //too many changes, tell parent to manually read the directory
                        wxDirectoryMonitorEvent e(m_pathnames[result- WAIT_OBJECT_0],MONITOR_TOO_MANY_CHANGES,wxEmptyString);
                        m_parent->AddPendingEvent(e);
                    }
                } else
                {
                    //couldn't read changes, tell parent to manually read the directory
                    //wxCommandEvent e(wxEVT_NOTIFY_UPDATE_TREE);
                    wxDirectoryMonitorEvent e(m_pathnames[result- WAIT_OBJECT_0],MONITOR_TOO_MANY_CHANGES,wxEmptyString);
                    m_parent->AddPendingEvent(e);
                    //TODO: exit the thread and make a proper error event?
                }
                if(hDir!=INVALID_HANDLE_VALUE)
                    ::CloseHandle(hDir);
            }
            if(!handle_fail)
                for(unsigned int i=0;i<m_pathnames.GetCount();i++)
                    if(!FindNextChangeNotification(m_handles[i]))
                        break;
            if(m_singleshot)
                break;
        }
        delete changedata;
        for(unsigned int i=0;i<m_pathnames.GetCount();i++)
            if(m_handles[i]!=INVALID_HANDLE_VALUE)
                FindCloseChangeNotification(m_handles[i]);
//        wxDirectoryMonitorEvent e(wxEmptyString,MONITOR_FINISHED,wxEmptyString);
//        m_parent->AddPendingEvent(e);
        wxMessageBox(_("Thread exit"));
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

BEGIN_EVENT_TABLE(wxDirectoryMonitor, wxEvtHandler)
    EVT_MONITOR_NOTIFY(0, wxDirectoryMonitor::OnMonitorEvent)
//    EVT_COMMAND(0, wxEVT_MONITOR_NOTIFY2, FileExplorer::OnMonitorEvent2)
END_EVENT_TABLE()


void wxDirectoryMonitor::OnMonitorEvent(wxDirectoryMonitorEvent &e)
{
    if(m_parent)
        m_parent->AddPendingEvent(e);
}


wxDirectoryMonitor::wxDirectoryMonitor(wxEvtHandler *parent, const wxArrayString &uri, int eventfilter)
{
    //TODO: put init and shutdown in static members
#ifdef __WXGTK__
    if(!gnome_vfs_initialized())
        gnome_vfs_init();
#endif
    m_parent=parent;
    m_uri=uri;
    m_eventfilter=eventfilter;
}

bool wxDirectoryMonitor::Start()
{
    LogMessage(wxString::Format(_("monitor start %i"),mon_count));
    m_monitorthread=new DirMonitorThread(this, m_uri, false, false, m_eventfilter, 100);
    m_monitorthread->Create();
    m_monitorthread->Run();
    return true;
}

wxDirectoryMonitor::~wxDirectoryMonitor()
{
    LogMessage(wxString::Format(_("monitor stop %i"),mon_count));
    mon_count++;
    delete m_monitorthread;
}
