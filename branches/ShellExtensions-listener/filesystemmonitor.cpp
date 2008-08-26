#include "filesystemmonitor.h"

class DirMonitorThread : public wxThread
{
public:
    DirMonitorThread(wxEvtHandler *parent, wxString pathname, bool singleshot, bool subtree, DWORD notifyfilter, DWORD waittime_ms)
        : wxThread(wxTHREAD_JOINABLE)
    { m_waittime=waittime_ms; m_subtree=subtree; m_singleshot=singleshot; m_pathname=pathname; m_notifyfilter=notifyfilter;
        return; }
    void *Entry()
    {
        bool done=false;
        m_handle=::FindFirstChangeNotification(m_pathname.c_str(), m_subtree, m_notifyfilter);
        while(!done && !TestDestroy())
        {
            DWORD result=::MsgWaitForMultipleObjects(1,&m_handle,false,m_waittime,m_notifyfilter);
            if(!result==WAIT_TIMEOUT)
            {
                PFILE_NOTIFY_INFORMATION changedata=(PFILE_NOTIFY_INFORMATION)(new char[4096]);
                DWORD chda_len;
                if(ReadDirectoryChangesW(m_handle, changedata, 4096, m_subtree, m_notifyfilter, &chda_len, NULL, NULL))
                {
                    if(chda_len>0)
                    {
                        do
                        {
                            DWORD a=changedata->Action;
                            wxString filename(changedata->FileName,changedata->FileNameLength);
                            changedata=(PFILE_NOTIFY_INFORMATION)((char*)changedata+changedata->NextEntryOffset);
                            //TODO: Notify parent - one event for each change
                        } while(changedata->NextEntryOffset>0);
                    }
                    else
                    {
                        //too many changes, tell parent to manually read the directory
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
    ~DirMonitorThread();
    DWORD m_waittime;
    bool m_subtree;
    bool m_singleshot;
    wxString m_pathname;
    DWORD m_notifyfilter;
    HANDLE m_handle;
};

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
    //TODO: ASSUME THIS CALLBACK HAPPENS ON A THREAD?? IF SO, NEED TO MESSAGE IT TO SELF USING THE EVENT SYSTEM
    //POST AN EVENT TO PARENT, OR IF PARENT IS NULL PASS TO SELF
    wxFileSysMonitorEvent e(this,EventType,info_uri);
    if(m_parent)
        m_parent->AddPendingEvent(e);
    else
        this->AddPendingEvent(e); //TODO: ADD EVENT HANDLER
}


wxFileSystemMonitor::wxFileSystemMonitor(wxEvtHandler *parent, const wxString &uri)
{
    m_parent=parent;
    m_uri=uri;
#ifdef __WXGTK__
    gnome_vfs_monitor_add(&m_h, uri.ToUTF8(), GNOME_VFS_MONITOR_DIRECTORY, &wxFileSystemMonitor::MonitorCallback, NULL);
    m[m_h]=this;
#else
#endif
}

wxFileSystemMonitor::~wxFileSystemMonitor()
{
#ifdef __WXGTK__
    gnome_vfs_monitor_cancel(m_h);
    m.erase(m_h);
#else
#endif
    //dtor
}
