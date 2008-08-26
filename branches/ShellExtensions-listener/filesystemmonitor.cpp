#include "filesystemmonitor.h"

DEFINE_EVENT_TYPE(wxEVT_MONITOR_NOTIFY)

#ifdef __WXGTK__

#include <map>
typedef std::map<GnomeVFSMonitorHandle *, wxFileSystemMonitor *> MonMap;
static MonMap m;

#else //WINDOWS

class DirMonitorThread : public wxThread
{
public:
    DirMonitorThread(wxEvtHandler *parent, wxString pathname, bool singleshot, bool subtree, DWORD notifyfilter, DWORD waittime_ms)
        : wxThread(wxTHREAD_JOINABLE)
    { m_parent=parent; m_waittime=waittime_ms; m_subtree=subtree; m_singleshot=singleshot; m_pathname=pathname.c_str(); m_notifyfilter=notifyfilter;
        return; }
    void *Entry()
    {
        m_handle=::FindFirstChangeNotification(m_pathname.c_str(), m_subtree, DEFAULT_MONITOR_FILTER_WIN32);
        while(!TestDestroy())
        {
            DWORD result=::MsgWaitForMultipleObjects(1,&m_handle,false,m_waittime,DEFAULT_MONITOR_FILTER_WIN32);
            if(!result==WAIT_TIMEOUT)
            {
                PFILE_NOTIFY_INFORMATION changedata=(PFILE_NOTIFY_INFORMATION)(new char[4096]);
                DWORD chda_len;
                if(::ReadDirectoryChangesW(m_handle, changedata, 4096, m_subtree, m_notifyfilter, &chda_len, NULL, NULL))
                {
                    if(chda_len>0)
                    {
                        do
                        {
                            DWORD a=changedata->Action;
                            //TODO: Convert to the MONITOR_FILE_XXX action types, filtering those that aren't wanted
                            wxString filename(changedata->FileName,changedata->FileNameLength);
                            changedata=(PFILE_NOTIFY_INFORMATION)((char*)changedata+changedata->NextEntryOffset);
                            wxFileSysMonitorEvent e(NULL,a,filename);
                            m_parent->AddPendingEvent(e);
                        } while(changedata->NextEntryOffset>0);
                    }
                    else
                    {
                        //too many changes, tell parent to manually read the directory
                        wxFileSysMonitorEvent e(NULL,0,wxEmptyString);
                        m_parent->AddPendingEvent(e);
                    }
                }
                delete changedata;
            } else
                break;
            if(!FindNextChangeNotification(m_handle))
                break;
            if(m_singleshot)
                break;

        }
        FindCloseChangeNotification(m_handle);
        return NULL;
    }
    ~DirMonitorThread()
    {
        if(IsRunning())
            Delete();
    }
    DWORD m_waittime;
    bool m_subtree;
    bool m_singleshot;
    wxString m_pathname;
    DWORD m_notifyfilter;
    HANDLE m_handle;
    wxEvtHandler *m_parent;
};

#endif



BEGIN_EVENT_TABLE(wxFileSystemMonitor, wxEvtHandler)
    EVT_MONITOR_NOTIFY(wxID_ANY, wxFileSystemMonitor::OnMonitorEvent)
END_EVENT_TABLE()


#ifdef __WXGTK__
void wxFileSystemMonitor::MonitorCallback(GnomeVFSMonitorHandle *handle, const gchar *monitor_uri, const gchar *info_uri, GnomeVFSMonitorEventType event_type, gpointer user_data)
{
    if(m.find(handle)!=m.end())
        m[handle]->Callback(event_type, wxString::FromUTF8(info_uri));
    //TODO: ELSE WARNING/ERROR
}
#endif

void wxFileSystemMonitor::Callback(int EventType, const wxString &info_uri)
{
    //TODO: DETERMINE WHETHER THIS CALLBACK HAPPENS ON A THREAD?
    //TODO: Convert to gnomevfs events to the MONITOR_FILE_XXX action types, filtering those that aren't wanted
    wxFileSysMonitorEvent e(this,EventType,info_uri);
    this->AddPendingEvent(e);
}

void wxFileSystemMonitor::OnMonitorEvent(wxFileSysMonitorEvent &e)
{
    if(m_parent)
        m_parent->AddPendingEvent(e);
}

wxFileSystemMonitor::wxFileSystemMonitor(wxEvtHandler *parent, const wxString &uri, int eventfilter)
{
    m_parent=parent;
    m_uri=uri;
    m_eventfilter=eventfilter;
}

bool wxFileSystemMonitor::Start()
{
#ifdef __WXGTK__
    gnome_vfs_monitor_add(&m_h, uri.ToUTF8(), GNOME_VFS_MONITOR_DIRECTORY, &wxFileSystemMonitor::MonitorCallback, NULL);
    m[m_h]=this;
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
    gnome_vfs_monitor_cancel(m_h);
    m.erase(m_h);
#else
    delete m_monitorthread;
#endif
    //dtor
}
