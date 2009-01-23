#include "directorymonitor.h"
#include <vector>
#include "se_globals.h"

#include <iostream>

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
    {
        m_parent=parent;
        m_waittime=waittime_ms;
        m_subtree=subtree;
        m_singleshot=singleshot;
        m_pathnames=pathnames;
        m_notifyfilter=notifyfilter;
        int pipehandles[2];
        pipe(pipehandles);
        m_msg_rcv=pipehandles[0];
        m_msg_send=pipehandles[1];
        m_msg_rcv_c=g_io_channel_unix_new(pipehandles[0]);
//        write(m_msg_send,"ab",2);
//        char d='1';
//        read(m_msg_rcv,&d,1);
//        wxMessageBox(wxString::FromUTF8(&d,1));
//        GError *err;
//        gsize len;
//        gchar c='1';
//        g_io_channel_read_chars(m_msg_rcv_c,&c,1,&len,&err);
//        wxMessageBox(wxString::FromUTF8(&c,1));
        return;
    }
    static gboolean tn_callback(GIOChannel *channel, GIOCondition cond, gpointer data)
    {
        DirMonitorThread *mon=(DirMonitorThread *)data;
        mon->m_interrupt_mutex.Lock();
        char c;
//        GError *err;
//        gsize read;
        read(mon->m_msg_rcv, &c, 1);
//        GIOStatus s=g_io_channel_read_chars(mon->m_msg_rcv, &c, 1, &read, &err);
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

        guint result=g_io_add_watch(m_msg_rcv_c, G_IO_IN, &tn_callback, this);
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

        GError *err;
        GIOStatus s=g_io_channel_shutdown(m_msg_rcv_c, true, &err);

        g_main_context_unref(context);
        m_interrupt_mutex.Unlock();
        return NULL;
    }
    ~DirMonitorThread()
    {
        g_main_loop_quit(loop);
        if(IsRunning())
            Wait();//Delete();
        close(m_msg_rcv);
        close(m_msg_send);
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
        GError *err;
        //GIOStatus s=g_io_channel_write_unichar(m_msg_send, 'm',&err);
        char m='m';
        gsize num;
        write(m_msg_send,&m,1);
        //flush(m_msg_send);
//        GIOStatus s=g_io_channel_write_chars(m_msg_send, &m, 1, &num,&err);
//        if(s!=G_IO_STATUS_NORMAL)
//            wxMessageBox(_("Write error!"));
//        s=g_io_channel_flush(m_msg_send, &err);
//        if(s!=G_IO_STATUS_NORMAL)
//            wxMessageBox(_("Flush error!"));
        m_interrupt_mutex.Unlock();

    }

    int m_msg_rcv;
    int m_msg_send;
    GIOChannel *m_msg_rcv_c;
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
        m_interrupt_mutex2.Lock();
        HANDLE *handles=new HANDLE[m_update_paths.GetCount()+1];
        HANDLE *filehandles=new HANDLE[m_update_paths.GetCount()];
        for(size_t i=0;i<m_pathnames.GetCount();i++)
        {
            int index=m_update_paths.Index(m_pathnames[i]);
            if(index==wxNOT_FOUND)
            {
                ::FindCloseChangeNotification(m_handles[i]);
                ::CloseHandle(m_filehandles[i]);
            }
        }
        for(size_t i=0;i<m_update_paths.GetCount();i++)
        {
            int index=m_pathnames.Index(m_update_paths[i]);
            if(index!=wxNOT_FOUND)
            {
                handles[i]=m_handles[index];
                filehandles[i]=m_filehandles[index];
            }
            else
            {
                filehandles[i] = ::CreateFile(m_update_paths[i].c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
                handles[i]=::FindFirstChangeNotification(m_update_paths[i].c_str(), m_subtree, DEFAULT_MONITOR_FILTER_WIN32);
            }
        }
        delete [] m_handles;
        delete [] m_filehandles;
        m_handles=handles;
        m_filehandles=filehandles;
        m_pathnames=m_update_paths;
        m_handles[m_pathnames.GetCount()]=m_interrupt_event;
        m_interrupt_mutex2.Unlock();
        for(size_t i=0;i<m_pathnames.GetCount();i++)
        {
            if(m_handles[i]==INVALID_HANDLE_VALUE || m_filehandles[i]==INVALID_HANDLE_VALUE)
            {
                wxMessageBox(_("ERROR: Invalid handle"));
                return false;
            }
        }
        return true;
    }
    void *Entry()
    {
        bool handle_fail=false;
        m_handles=new HANDLE[m_pathnames.GetCount()+1];
        m_filehandles=new HANDLE[m_pathnames.GetCount()];
        for(unsigned int i=0;i<m_pathnames.GetCount();i++)
        {
            m_filehandles[i] = ::CreateFile(m_pathnames[i].c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
            if(m_filehandles[i]==INVALID_HANDLE_VALUE)
            {
                handle_fail=true;
                continue;
            }
            m_handles[i]=::FindFirstChangeNotification(m_pathnames[i].c_str(), m_subtree, DEFAULT_MONITOR_FILTER_WIN32);
            if(m_handles[i]==INVALID_HANDLE_VALUE)
            {
                handle_fail=true;
                continue;
            }
        }
        m_handles[m_pathnames.GetCount()]=m_interrupt_event;
        //TODO: Error checking
        PFILE_NOTIFY_INFORMATION changedata=(PFILE_NOTIFY_INFORMATION)(new char[4096]);
        bool kill=false;
        while(!handle_fail && !TestDestroy() && !kill)
        {
            //wxMessageBox(_("monitor wait start\n"));
            DWORD result=::MsgWaitForMultipleObjects(m_pathnames.GetCount()+1,m_handles,false,INFINITE,0);
            //DWORD result=::MsgWaitForMultipleObjects(m_pathnames.GetCount()+1,m_handles,false,INFINITE,0);
//            DWORD result=::MsgWaitForMultipleObjects(m_pathnames.GetCount()+1,m_handles,false,INFINITE,DEFAULT_MONITOR_FILTER_WIN32);
            //wxMessageBox(wxString::Format(_("returned %i"),result-WAIT_OBJECT_0));
            if(result==WAIT_FAILED)
            {
                kill=true;
            }
            else
            if(result >= WAIT_ABANDONED_0 && result - WAIT_ABANDONED_0<=m_pathnames.GetCount())
                kill=true;
            else
            if(result - WAIT_OBJECT_0==m_pathnames.GetCount())
            {
                //wxMessageBox(_("monitor wait timeout - update request\n"));
                m_interrupt_mutex2.Lock();
                kill=m_kill;
                if(!m_kill)
                    if(!UpdatePathsThread())
                        handle_fail=true;
                m_interrupt_mutex2.Unlock();
                ResetEvent(m_interrupt_event);
            }
            else
            if(result>= WAIT_OBJECT_0 && result- WAIT_OBJECT_0<m_pathnames.GetCount())
            {
                //wxMessageBox(_("monitor wait directory change\n"));
                wxMessageBox(_("dir event on ")+m_pathnames[result- WAIT_OBJECT_0]);
                DWORD chda_len;
                //wxMessageBox(_("open directory change handle\n"));
                if(::ReadDirectoryChangesW(m_filehandles[result- WAIT_OBJECT_0], changedata, 4096, m_subtree, DEFAULT_MONITOR_FILTER_WIN32, &chda_len, NULL, NULL))
                {
                    //if(chda_len==0)
                    //    break;
                    //wxMessageBox(_("reading directory change\n"));
                    if(false && chda_len>0)
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
                if(m_filehandles[result-WAIT_OBJECT_0]!=INVALID_HANDLE_VALUE)
                    ::CloseHandle(m_filehandles[result-WAIT_OBJECT_0]);
                if(!FindNextChangeNotification(m_handles[result- WAIT_OBJECT_0]))
                {
                    wxMessageBox(_("ERROR: Could not set next change notification"));
                    break;
                }
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
        wxMessageBox(_("ERROR: Monitor died"));
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
    HANDLE *m_handles,*m_filehandles;
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
