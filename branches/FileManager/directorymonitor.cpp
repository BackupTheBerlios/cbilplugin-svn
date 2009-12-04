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

#include <map>

#ifdef __GIO__

#include <gio/gio.h>

struct MonMapData
{
    MonMapData(){m_monitor=NULL;}
    MonMapData(DirMonitorThread *owner_monitor,const wxString &path)
    {
        m_path=path.c_str();
        m_monitor=owner_monitor;
    }
    DirMonitorThread *m_monitor; //pointer to the DirMonitorThread class that created the GFileMonitor
    wxString m_path; //the directory associated with this monitor
};
typedef std::map<GFileMonitor *, MonMapData> MonMap;
static MonMap m;

class DirMonitorThread : public wxThread
{
public:
    DirMonitorThread(wxEvtHandler *parent, wxArrayString pathnames, bool singleshot, bool subtree, int notifyfilter, int waittime_ms)
        : wxThread(wxTHREAD_JOINABLE)
    {
        m_active=false;
        m_parent=parent;
        m_waittime=waittime_ms;
        m_subtree=subtree;
        m_singleshot=singleshot;
        for(unsigned int i=0;i<pathnames.GetCount();i++)
            m_pathnames.Add(pathnames[i].c_str());
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
        switch(c)
        {
            case 'm':
                mon->UpdatePathsThread();
                break;
            case 'q':
                g_main_loop_quit(loop);
                break;
        }
        mon->m_interrupt_mutex.Unlock();
        return true;
    }
    void UpdatePathsThread()
    {
        std::vector<GFileMonitor *> new_h(m_update_paths.GetCount(),NULL); //TODO: initialize vector size
        for(size_t i=0;i<m_pathnames.GetCount();i++) //delete monitors that aren't needed
        {
            int index=m_update_paths.Index(m_pathnames[i]);
            if(index==wxNOT_FOUND && m_h[i])
            {
                g_file_monitor_cancel(m_h[i]);
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
                GFile *file=g_file_new_for_path(m_update_paths[i].ToUTF8());
                GFileMonitor *h= g_file_monitor_directory(file,G_FILE_MONITOR_NONE,NULL,NULL);
                if(h)
                {
                    g_signal_connect(h,"changed",G_CALLBACK(DirMonitorThread::MonitorCallback),NULL);
                    new_h[i]=h;
                    m[h]=MonMapData(this,m_update_paths[i]);
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
            GFile *file=g_file_new_for_path(m_pathnames[i].ToUTF8());
            GFileMonitor *h= g_file_monitor_directory(file,G_FILE_MONITOR_NONE,NULL,NULL);
            if(h)
            {
                g_signal_connect(h,"changed",G_CALLBACK(DirMonitorThread::MonitorCallback),NULL);
                m_h.push_back(h);
                m[h]=MonMapData(this,m_pathnames[i]);
            } else
            {
                m_h.push_back(NULL);
                //TODO: Log an error
            }
        }
        //TODO: Add a timer for killing singleshot instances
        m_active=true;
        m_interrupt_mutex.Unlock();

        g_main_loop_run(loop);

        m_interrupt_mutex.Lock();
        for(unsigned int i=0;i<m_pathnames.GetCount();i++)
        {
            if(m_h[i])
            {
                g_file_monitor_cancel(m_h[i]);
                g_object_unref(m_h[i]);
                m.erase(m_h[i]);
            }
        }

        GError *err=NULL;
        GIOStatus s=g_io_channel_shutdown(m_msg_rcv_c, true, &err);

        g_main_context_unref(context);
        g_main_loop_unref(loop);
        m_interrupt_mutex.Unlock();
        return NULL;
    }
    ~DirMonitorThread()
    {
        m_interrupt_mutex.Lock();
        m_active=false
        m_interrupt_mutex.Unlock();
        char m='q';
        gsize num;
        write(m_msg_send,&m,1);
        if(IsRunning())
            Wait();//Delete();
        close(m_msg_rcv);
        close(m_msg_send);
    }

    void Callback(const wxString &mon_dir, int EventType, const wxString &uri)
    {
        int action=0;
        switch(EventType)
        {
            case G_FILE_MONITOR_EVENT_CHANGED:
            case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
                action=MONITOR_FILE_CHANGED;
                break;
            case G_FILE_MONITOR_EVENT_DELETED:
                action=MONITOR_FILE_DELETED;
                break;
            case G_FILE_MONITOR_EVENT_CREATED:
                action=MONITOR_FILE_CREATED;
                break;
            case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
                action=MONITOR_FILE_ATTRIBUTES;
                break;
        }
        if(action&m_notifyfilter)
        {
            wxDirectoryMonitorEvent e(mon_dir.c_str(),action,uri);
            m_parent->AddPendingEvent(e);
        }
    }

    static void MonitorCallback(GFileMonitor *handle, GFile *file, GFile *other_file, GFileMonitorEvent event_type,gpointer user_data)
    {
        if(m.find(handle)!=m.end())
            m[handle].m_monitor->Callback(m[handle].m_path, event_type, wxString::FromUTF8(g_file_get_path(file)));
        //TODO: ELSE WARNING/ERROR
    }
    void UpdatePaths(const wxArrayString &paths)
    {
        m_interrupt_mutex.Lock();
        if(!m_active)
            return false
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
    bool m_active;
    wxMutex m_interrupt_mutex;
    GMainContext *context;
    GMainLoop *loop;
    int m_waittime;
    bool m_subtree;
    bool m_singleshot;
    wxArrayString m_pathnames, m_update_paths;
    int m_notifyfilter;
    std::vector<GFileMonitor *> m_h;
    wxEvtHandler *m_parent;
};
#else //USE VFS INSTEAD

#include <libgnomevfs/gnome-vfs.h>

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
        for(unsigned int i=0;i<pathnames.GetCount();i++)
            m_pathnames.Add(pathnames[i].c_str());
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
            if(gnome_vfs_monitor_add(&h, m_pathnames[i].ToUTF8(), GNOME_VFS_MONITOR_DIRECTORY, G_CALLBACK(DirMonitorThread::MonitorCallback), &m_pathnames[i])==GNOME_VFS_OK)
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

        GError *err=NULL;
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
#endif //__GIO__

#endif
#ifdef __WXMSW__

#define DEFAULT_MONITOR_FILTER_WIN32 FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_ATTRIBUTES|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_LAST_ACCESS|FILE_NOTIFY_CHANGE_CREATION|FILE_NOTIFY_CHANGE_SECURITY


#include <map>

// Structure contains all of the data required to monitor a single directory
struct MonData
{
    OVERLAPPED m_overlapped;
    wxString m_path;
    HANDLE m_handle;
    PFILE_NOTIFY_INFORMATION m_changedata;
    DirMonitorThread *m_monitor;
    bool m_fail;
    bool m_cancel;
    MonData();
    MonData(DirMonitorThread *monitor, const wxString &path, bool subtree);
    void ReadCancel();
    void ReadRequest(bool subtree);
    ~MonData();
    static OVERLAPPED new_overlapped();
};


// watched directories are maintained as a map by pathname
typedef std::map<wxString,MonData*> MonMap;


//WIN32 ONLY THREADED CLASS TO HANDLE WAITING ON DIR CHANGES ASYNCHRONOUSLY
class DirMonitorThread : public wxThread
{
public:
    DirMonitorThread(wxEvtHandler *parent, wxArrayString pathnames, bool singleshot, bool subtree, DWORD notifyfilter, DWORD waittime_ms)
        : wxThread(wxTHREAD_JOINABLE)
    {
        m_interrupt_event[0]=CreateEvent(NULL, FALSE, FALSE, NULL); //used to signal update path request
        m_interrupt_event[1]=CreateEvent(NULL, FALSE, FALSE, NULL); //used to signal quit request

        m_parent=parent;
        m_waittime=waittime_ms;
        m_subtree=subtree;
        m_singleshot=singleshot;
        for(unsigned int i=0;i<pathnames.GetCount();i++)
            m_update_paths.Add(pathnames[i].c_str());
        m_notifyfilter=notifyfilter;
        return;

    }
    void WaitKill()
    {
        SetEvent(m_interrupt_event[1]);
    }
    void UpdatePaths(const wxArrayString &paths)
    {
        m_interrupt_mutex2.Lock();
        m_update_paths.Empty();
        for(unsigned int i=0;i<paths.GetCount();i++)
            m_update_paths.Add(paths[i].c_str());
        m_update_paths=paths;
        SetEvent(m_interrupt_event[0]);
        m_interrupt_mutex2.Unlock();
    }
    bool UpdatePathsThread()
    {
        m_interrupt_mutex2.Lock();
        wxArrayString update_paths;
        for(unsigned int i=0;i<m_update_paths.GetCount();i++)
            update_paths.Add(m_update_paths[i].c_str());

        for(MonMap::iterator it=m_monmap.begin();it!=m_monmap.end();++it)
        {
            int index=update_paths.Index(it->first);
            if(index==wxNOT_FOUND)
            {
                it->second->ReadCancel(); //request cancel. will be removed from the map
                if(it->second->m_fail)
                {
                    delete it->second;
                    m_monmap.erase(it);
                }
            }
        }
        for(size_t i=0;i<update_paths.GetCount();i++)
        {
            MonMap::iterator it=m_monmap.find(update_paths[i]);
            if(it==m_monmap.end())
            {
                MonData *md=new MonData(this,update_paths[i],m_subtree);
                if(md->m_fail)
                    delete md;
                else
                    m_monmap[update_paths[i]]=md;
            }
        }
        m_interrupt_mutex2.Unlock();
        return true;
    }

    void *Entry()
    {
        UpdatePathsThread();
        bool kill_request=false;
        //TODO: Error checking
        while(!(kill_request && m_monmap.size()==0)) //don't exit until we have gracefully closed all of the directory monitors
        {
            DWORD result=::WaitForMultipleObjectsEx(2,m_interrupt_event,false,INFINITE,true);
            if(result==WAIT_FAILED || result==WAIT_ABANDONED_0)
                break; //most likely will cause crash if this happens
            if(result==WAIT_OBJECT_0+1 && !kill_request)
            {
                kill_request=true;
                for(MonMap::iterator it=m_monmap.begin();it!=m_monmap.end();++it)
                {
                    it->second->ReadCancel();
                    if(it->second->m_fail)
                    {
                        delete it->second;
                        m_monmap.erase(it);
                    }
                }
            }
            if(result==WAIT_OBJECT_0)
            {
                m_interrupt_mutex2.Lock();
                UpdatePathsThread();
                ResetEvent(m_interrupt_event[0]);
                m_interrupt_mutex2.Unlock();
            }
            if(m_singleshot)
                break;
        }
        //wxMessageBox(_("quitting monitor"));
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
        CloseHandle(m_interrupt_event[0]);
        CloseHandle(m_interrupt_event[1]);
    }
    void ReadChanges(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, MonData *mondata)
    {
//        wxMessageBox(wxString::Format(_("%i"),dwErrorCode));
        //TODO: Error checking - dwErrorCode should be zero, then need to check other error conditions
        int off=0;
        unsigned int i=0;
//        if(mondata->m_cancel && dwNumberOfBytesTransfered>0)
//            wxMessageBox(wxString::Format(_("message, code %i, bytes %i, path %s"),dwErrorCode, dwNumberOfBytesTransfered,mondata->m_path.c_str()));
        if(dwNumberOfBytesTransfered==0 || dwErrorCode>0) //mondata->m_cancel ||
        {
            std::cout<<"LOG: FileIOCompletionRoutine Cancel "<<mondata->m_path.ToAscii()<<std::endl;
            if(!mondata->m_cancel)
            {
                std::cout<<"LOG: FileIOCompletionRoutine Cancel Fail "<<mondata->m_path.ToAscii()<<std::endl;
//                wxMessageBox(_("FileManager Directory Monitory is about to have a serious problem!"));
            }
            //wxMessageBox(_("canceling i/o ")+mondata->m_path);
            MonMap::iterator it=m_monmap.find(mondata->m_path);
            if(it!=m_monmap.end())
            {
                delete it->second;
                m_monmap.erase(it);
            }
            return;
        }
        PFILE_NOTIFY_INFORMATION chptr=&mondata->m_changedata[i];
        if(dwNumberOfBytesTransfered>0)
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
                wxDirectoryMonitorEvent e(mondata->m_path,action,filename);
                m_parent->AddPendingEvent(e);
            }
            off=chptr->NextEntryOffset;
            chptr=(PFILE_NOTIFY_INFORMATION)((char*)chptr+off);
        } while(off>0);
        else
        {
            //too many changes, tell parent to manually read the directory
            wxDirectoryMonitorEvent e(mondata->m_path,MONITOR_TOO_MANY_CHANGES,wxEmptyString);
            m_parent->AddPendingEvent(e);

        }
        std::cout<<"LOG: FileIOCompletionRoutine Read Request Restart "<<mondata->m_path.ToAscii()<<std::endl;
        mondata->ReadRequest(m_subtree);
    }
    static VOID CALLBACK FileIOCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
    {
        MonData *mondata=(MonData*)lpOverlapped;
        std::cout<<"LOG: FileIOCompletionRoutine Fired "<<mondata->m_path.ToAscii()<<" code "<<dwErrorCode<<" bytes "<<dwNumberOfBytesTransfered<<std::endl;
        mondata->m_monitor->ReadChanges(dwErrorCode, dwNumberOfBytesTransfered, mondata);
    }

    HANDLE m_interrupt_event[2];
    wxMutex m_interrupt_mutex2;
    DWORD m_waittime;
    bool m_subtree;
    bool m_singleshot;
    MonMap m_monmap;
    wxArrayString m_update_paths;
    DWORD m_notifyfilter;
    wxEvtHandler *m_parent;
};


//MonData implementations
MonData::MonData()
{
    std::cout<<"LOG: creating empty monitor structure"<<std::endl;
    m_path=_("");
    m_monitor=NULL;
    m_changedata=NULL;
    m_handle=NULL;
    m_fail=false;
    m_cancel=false;
}

MonData::MonData(DirMonitorThread *monitor, const wxString &path, bool subtree)
{
    std::cout<<"LOG: creating monitor for path "<<path.ToAscii()<<std::endl;
    MonData();
    m_monitor=monitor;
    m_path=path.c_str();
    m_handle=::CreateFile(path.c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,NULL);
    if(m_handle!=INVALID_HANDLE_VALUE)
    {
        m_overlapped=new_overlapped();
        m_changedata=(PFILE_NOTIFY_INFORMATION)(new char[4096]);
        std::cout<<"LOG: successfully created monitor "<<m_path.ToAscii()<<std::endl;
        ReadRequest(subtree);
    } else
    {
        wxMessageBox(_("WARNING: Failed to open handle for ")+m_path);
        std::cout<<"ERROR: created failed monitor "<<m_path.ToAscii()<<std::endl;
        m_handle=NULL;
        m_fail=true;
    }
}

void MonData::ReadCancel()
{
    if(!m_fail && m_handle!=NULL)
    {
        if(!::CancelIo(m_handle))
        {
            std::cout<<"ERROR: read cancel failed for "<<m_path.ToAscii()<<std::endl;
            wxMessageBox(_("WARNING: Failed to initiate cancel io for ")+m_path);
            m_fail=true;
        }
        else
        {
            std::cout<<"LOG: read cancel succeeded for "<<m_path.ToAscii()<<std::endl;
            m_cancel=true;
        }
    } else
        std::cout<<"LOG: redundant monitor read cancel request for "<<m_path.ToAscii()<<std::endl;
}

void MonData::ReadRequest(bool subtree)
{
    if(!::ReadDirectoryChangesW(m_handle, m_changedata, 4096, subtree, DEFAULT_MONITOR_FILTER_WIN32, NULL, &m_overlapped, m_monitor->FileIOCompletionRoutine))
    {
        m_fail=true;
        std::cout<<"ERROR: read changes failed for "<<m_path.ToAscii()<<std::endl;
        wxMessageBox(_("WARNING: Failed to initiate read request for ")+m_path);
    }
    else
    {
        std::cout<<"LOG: read changes initiated for "<<m_path.ToAscii()<<std::endl;
        m_fail=false;
    }
}

MonData::~MonData()
{
    std::cout<<"LOG: deleting monitor to "<<m_path.ToAscii()<<std::endl;
    if(m_handle)
    {
        if(!::CloseHandle(m_handle))
        {
            std::cout<<"ERROR: failed to close handle for "<<m_path.ToAscii()<<std::endl;
            wxMessageBox(_("WARNING: Failed to close monitor handle for ")+m_path);
        }
        else
            std::cout<<"LOG: closed handle for "<<m_path.ToAscii()<<std::endl;
    }
    if(m_changedata)
        delete m_changedata;
    std::cout<<"LOG: succesfully delete monitor for "<<m_path.ToAscii()<<std::endl;
}

OVERLAPPED MonData::new_overlapped()
{
    OVERLAPPED o;
    o.Internal=0;
    o.InternalHigh=0;
    o.Offset=0;
    o.OffsetHigh=0;
    o.hEvent=NULL;
    return o;
}


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
#ifndef __GIO__
    if(!gnome_vfs_initialized())
        gnome_vfs_init();
#endif
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
//    wxString p=_("Monitoring:");
//    for(int i=0;i<uri.GetCount();i++)
//        p+=uri[i]+_(", ");
//    LogMessage(p);
}

wxDirectoryMonitor::~wxDirectoryMonitor()
{
    mon_count++;
    delete m_monitorthread;
}
