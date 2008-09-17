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


    static gboolean tn_prepare(GSource *source, gint *timeout_)
    {
        DirMonitorThread *mon=((DirMonitorThread **)(source+1))[0];
        mon->m_interrupt_mutex.Lock();
        gboolean notify=mon->m_thread_notify;
        mon->m_interrupt_mutex.Unlock();
        *timeout_=0;
        return notify;
    }
    static gboolean tn_check(GSource *source)
    {
        DirMonitorThread *mon=((DirMonitorThread **)(source+1))[0];
        mon->m_interrupt_mutex.Lock();
        gboolean notify=mon->m_thread_notify;
        mon->m_interrupt_mutex.Unlock();
        return notify;
    }
    static gboolean tn_dispatch (GSource *source,GSourceFunc callback, gpointer user_data)
    {
        DirMonitorThread *mon=((DirMonitorThread **)(source+1))[0];
        mon->m_interrupt_mutex.Lock();
        mon->m_thread_notify=false;
        mon->m_interrupt_mutex.Unlock();
        callback(user_data);
        return TRUE;
    }
    static void tn_finalize (GSource *source)
    {
        return;
    }
    static gboolean tn_callback(gpointer data)
    {
        DirMonitorThread *mon=(DirMonitorThread *)data;
        mon->m_interrupt_mutex.Lock();
        mon->UpdatePathsThread();
        mon->m_interrupt_mutex.Unlock();
        return true;
    }
    void UpdatePathsThread()
    {
        std::vector<GnomeVFSMonitorHandle *> new_h(m_update_paths.GetCount(),NULL); //TODO: initialize vector size
        for(size_t i=0;i<m_pathnames.GetCount();i++) //delete monitors that aren't needed
        {
            int index=m_update_paths.Index(m_pathnames[i]);
            if(index==wxNOT_FOUND && m_h[i])
            {
                gnome_vfs_monitor_cancel(m_h[i]);
                m.erase(m_h[i]);
            }
        }
        for(size_t i=0;i<m_update_paths.GetCount();i++) // copy existing monitors and add new ones
        {
            int index=m_pathnames.Index(m_update_paths[i]);
            if(index!=wxNOT_FOUND)
            {
                new_h[i]=m_h[index];
            }
            else
            {
                GnomeVFSMonitorHandle *h;
                if(gnome_vfs_monitor_add(&h, m_update_paths[i].ToUTF8(), GNOME_VFS_MONITOR_DIRECTORY, &DirMonitorThread::MonitorCallback, &m_update_paths[i])==GNOME_VFS_OK)
                {
                    new_h[i]=h;
                    m[h]=this;
                } else
                {
                    new_h[i]=NULL;
                    //TODO: Notify owner error
                }
            }
        }
        m_h=new_h; //replace the old with the new
        m_pathnames=m_update_paths;
    }
    void *Entry()
    {
        m_interrupt_mutex.Lock();
        m_thread_notify=false;
        context=g_main_context_new();
        loop=g_main_loop_new(context,FALSE);

        GSourceFuncs thread_notify_sf;
        thread_notify_sf.prepare=&tn_prepare;
        thread_notify_sf.check=&tn_check;
        thread_notify_sf.dispatch=&tn_dispatch;
        thread_notify_sf.finalize=&tn_finalize;

        GSource *thread_notify_s=g_source_new (&thread_notify_sf, sizeof(GSource)+sizeof(DirMonitorThread*));
        ((DirMonitorThread**)(thread_notify_s+1))[0]=this;
        g_source_attach(thread_notify_s,context);
        g_source_set_callback(thread_notify_s,&tn_callback,this,NULL);

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
            }
        }
        //TODO: Add a timer for killing singleshot instances
        m_interrupt_mutex.Unlock();

        g_main_loop_run(loop);

        m_interrupt_mutex.Lock();
        for(unsigned int i=0;i<m_pathnames.GetCount();i++)
        {
            if(m_h[i])
            {
                gnome_vfs_monitor_cancel(m_h[i]);
                m.erase(m_h[i]);
            }
        }

        g_source_destroy(thread_notify_s); //TODO: g_source_remove as well?
        g_main_context_unref(context);
        m_interrupt_mutex.Unlock();
        return NULL;
    }
    ~DirMonitorThread()
    {
        g_main_loop_quit(loop);
        if(IsRunning())
            Wait();//Delete();
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
        if(m.find(handle)!=m.end())
            m[handle]->Callback((wxString *)user_data, event_type, wxString::FromUTF8(info_uri));
        //TODO: ELSE WARNING/ERROR
    }
    void UpdatePaths(const wxArrayString &paths)
    {
        m_interrupt_mutex.Lock();
        m_update_paths.Empty();
        for(unsigned int i=0;i<paths.GetCount();i++)
            m_update_paths.Add(paths[i].c_str());
        m_thread_notify=true;
        m_interrupt_mutex.Unlock();

    }


    bool m_thread_notify;
    wxMutex m_interrupt_mutex;
    GMainContext *context;
    GMainLoop *loop;
    int m_waittime;
    bool m_subtree;
    bool m_singleshot;
    wxArrayString m_pathnames, m_update_paths;
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
        m_interrupt_event=CreateEvent(NULL, FALSE, FALSE, NULL);

        m_kill=false;
        m_parent=parent;
        m_waittime=waittime_ms;
        m_subtree=subtree;
        m_singleshot=singleshot;
        for(unsigned int i=0;i<pathnames.GetCount();i++)
            m_pathnames.Add(pathnames[i].c_str());
        m_notifyfilter=notifyfilter;
        return;

    }
    void WaitKill()
    {
        m_interrupt_mutex2.Lock();
        m_kill=true;
        m_interrupt_mutex2.Unlock();
        SetEvent(m_interrupt_event);
    }
    void UpdatePaths(const wxArrayString &paths)
    {
        m_interrupt_mutex2.Lock();
        m_update_paths.Empty();
        for(unsigned int i=0;i<paths.GetCount();i++)
            m_update_paths.Add(paths[i].c_str());
        m_interrupt_mutex2.Unlock();
        SetEvent(m_interrupt_event);
    }
    bool UpdatePathsThread()
    {
        HANDLE *handles=new HANDLE[m_update_paths.GetCount()+1];
        for(size_t i=0;i<m_pathnames.GetCount();i++)
        {
            int index=m_update_paths.Index(m_pathnames[i]);
            if(index==wxNOT_FOUND)
                ::FindCloseChangeNotification(m_handles[i]);
        }
        for(size_t i=0;i<m_update_paths.GetCount();i++)
        {
            int index=m_pathnames.Index(m_update_paths[i]);
            if(index!=wxNOT_FOUND)
            {
                handles[i]=m_handles[index];
            }
            else
            {
                handles[i]=::FindFirstChangeNotification(m_update_paths[i].c_str(), m_subtree, DEFAULT_MONITOR_FILTER_WIN32);
            }
        }
        delete [] m_handles;
        m_handles=handles;
        m_pathnames=m_update_paths;
        m_handles[m_pathnames.GetCount()]=m_interrupt_event;
        for(size_t i=0;i<m_update_paths.GetCount();i++)
            if(m_handles[i]==INVALID_HANDLE_VALUE)
                return false;
        return true;
    }
    void *Entry()
    {
        bool handle_fail=false;
        m_handles=new HANDLE[m_pathnames.GetCount()+1];
        for(unsigned int i=0;i<m_pathnames.GetCount();i++)
        {
            m_handles[i]=::FindFirstChangeNotification(m_pathnames[i].c_str(), m_subtree, DEFAULT_MONITOR_FILTER_WIN32);
            if(m_handles[i]==INVALID_HANDLE_VALUE)
            {
                handle_fail=true;
            }

        }
        m_handles[m_pathnames.GetCount()]=m_interrupt_event;
        //TODO: Error checking
        PFILE_NOTIFY_INFORMATION changedata=(PFILE_NOTIFY_INFORMATION)(new char[4096]);
        bool kill=false;
        while(!handle_fail && !TestDestroy() && !kill)
        {
            DWORD result=::WaitForMultipleObjects(m_pathnames.GetCount()+1,m_handles,false,INFINITE);
            //DWORD result=::MsgWaitForMultipleObjects(m_pathnames.GetCount()+1,m_handles,false,INFINITE,0);
//            DWORD result=::MsgWaitForMultipleObjects(m_pathnames.GetCount()+1,m_handles,false,INFINITE,DEFAULT_MONITOR_FILTER_WIN32);
            //wxMessageBox(wxString::Format(_("returned %i"),result-WAIT_OBJECT_0));
            if(result==WAIT_FAILED)
            {
                kill=true;
                wxMessageBox(_("dir mon failed"));
            }
            else
            if(result >= WAIT_ABANDONED_0 && result - WAIT_ABANDONED_0<=m_pathnames.GetCount())
                kill=true;
            else
            if(result - WAIT_OBJECT_0==m_pathnames.GetCount())
            {
                m_interrupt_mutex2.Lock();
                kill=m_kill;
                if(!m_kill)
                    if(!UpdatePathsThread())
                        handle_fail=true;
                m_interrupt_mutex2.Unlock();
            }
            else
            if(result>= WAIT_OBJECT_0 && result- WAIT_OBJECT_0<m_pathnames.GetCount())
            {
                //wxMessageBox(_("dir event on ")+m_pathnames[result- WAIT_OBJECT_0]);
                DWORD chda_len;
                HANDLE hDir = ::CreateFile(m_pathnames[result- WAIT_OBJECT_0].c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
                if(hDir==INVALID_HANDLE_VALUE)
                {
                    handle_fail=true;
                }
                else
                while(::ReadDirectoryChangesW(hDir, changedata, 4096, m_subtree, DEFAULT_MONITOR_FILTER_WIN32, &chda_len, NULL, NULL))
                {
                    if(chda_len==0)
                        break;
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
                    }
                    else
                    {
                        wxMessageBox(_T("mon error"));
                        //too many changes, tell parent to manually read the directory
                        wxDirectoryMonitorEvent e(m_pathnames[result- WAIT_OBJECT_0],MONITOR_TOO_MANY_CHANGES,wxEmptyString);
                        m_parent->AddPendingEvent(e);
                    }
                }
//               else {
//                        wxMessageBox(_T("mon error"));
//                    //couldn't read changes, tell parent to manually read the directory
//                    //wxCommandEvent e(wxEVT_NOTIFY_UPDATE_TREE);
//                    wxDirectoryMonitorEvent e(m_pathnames[result- WAIT_OBJECT_0],MONITOR_TOO_MANY_CHANGES,wxEmptyString);
//                    m_parent->AddPendingEvent(e);
//                    //TODO: exit the thread and make a proper error event?
//                }
                if(hDir!=INVALID_HANDLE_VALUE)
                    ::CloseHandle(hDir);
                if(!FindNextChangeNotification(m_handles[result- WAIT_OBJECT_0]))
                    break;
//                if(!handle_fail)
//                    for(unsigned int i=0;i<m_pathnames.GetCount();i++)
//                        if(!FindNextChangeNotification(m_handles[i]))
//                            break;
            }
            if(m_singleshot)
                break;
        }
        delete changedata;
        for(unsigned int i=0;i<m_pathnames.GetCount();i++)
            if(m_handles[i]!=INVALID_HANDLE_VALUE)
                FindCloseChangeNotification(m_handles[i]);
        delete [] m_handles;
        wxMessageBox(_("Monitor died"));
//        wxDirectoryMonitorEvent e(wxEmptyString,MONITOR_FINISHED,wxEmptyString);
//        m_parent->AddPendingEvent(e);
        return NULL;
    }
    ~DirMonitorThread()
    {
        if(IsRunning())
        {
            WaitKill();
            Wait();//Delete();
        }
        CloseHandle(m_interrupt_event);
    }
    HANDLE m_interrupt_event;
    wxMutex m_interrupt_mutex2;
    DWORD m_waittime;
    bool m_subtree;
    bool m_singleshot;
    bool m_kill;
    wxArrayString m_pathnames;
    wxArrayString m_update_paths;
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
    m_monitorthread=new DirMonitorThread(this, m_uri, false, false, m_eventfilter, 100);
    m_monitorthread->Create();
    m_monitorthread->Run();
    return true;
}

void wxDirectoryMonitor::ChangePaths(const wxArrayString &uri)
{
    m_uri=uri;
    m_monitorthread->UpdatePaths(uri);
    wxString p=_("Monitoring:");
    for(int i=0;i<uri.GetCount();i++)
        p+=uri[i]+_(", ");
    LogMessage(p);
}


wxDirectoryMonitor::~wxDirectoryMonitor()
{
    mon_count++;
    delete m_monitorthread;
}
