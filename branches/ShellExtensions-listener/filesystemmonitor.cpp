#include "filesystemmonitor.h"

void wxFileSystemMonitor::MonitorCallback(GnomeVFSMonitorHandle *handle, const gchar *monitor_uri, const gchar *info_uri, GnomeVFSMonitorEventType event_type, gpointer user_data)
{
    if(m.find(handle)!=m.end())
        m[handle]->Callback(event_type, wxString::FromUTF8(info_uri));
    //TODO: ELSE WARNING/ERROR
}

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
    gnome_vfs_monitor_add(&m_h, uri.ToUTF8(), GNOME_VFS_MONITOR_DIRECTORY, &wxFileSystemMonitor::MonitorCallback, NULL);
    m[m_h]=this;
}

wxFileSystemMonitor::~wxFileSystemMonitor()
{
    gnome_vfs_monitor_cancel(m_h);
    m.erase(m_h);
    //dtor
}
