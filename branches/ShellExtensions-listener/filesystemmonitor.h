#ifndef WXFILESYSTEMMONITOR_H
#define WXFILESYSTEMMONITOR_H

#include <wx/wx.h>

#define MONITOR_FINISHED 0x010000
#define MONITOR_TOO_MANY_CHANGES 0x020000
#define MONITOR_FILE_CHANGED 0x001
#define MONITOR_FILE_DELETED 0x002
#define MONITOR_FILE_CREATED 0x004
//TODO: Decide if it is worth having these
#define MONITOR_FILE_ATTRIBUTES 0x080
#define MONITOR_FILE_STARTEXEC 0x010
#define MONITOR_FILE_STOPEXEC 0x020

#define DEFAULT_MONITOR_FILTER MONITOR_FILE_CHANGED|MONITOR_FILE_DELETED|MONITOR_FILE_CREATED|MONITOR_FILE_ATTRIBUTES

#ifdef __WXGTK__
#include <libgnomevfs/gnome-vfs.h>
#else //WINDOWS
class DirMonitorThread;
#endif

class wxFileSystemMonitor;

///////////////////////////////////////
// EVENT CODE /////////////////////////
///////////////////////////////////////

/*
Defines a wxFileSysMonitorEvent with public member naming the path
monitored, the file or directory creating the event and the code for
the event. Also used to send Termination events (on win32)

Also defines event table macro EVT_MONITOR_NOTIFY to notify the
caller of change events
*/

BEGIN_DECLARE_EVENT_TYPES()
DECLARE_LOCAL_EVENT_TYPE(wxEVT_MONITOR_NOTIFY, -1)
END_DECLARE_EVENT_TYPES()

class wxFileSysMonitorEvent: public wxEvent
{
public:
    wxFileSysMonitorEvent(const wxString &mon_dir, int event_type, const wxString &uri)
    {
        m_mon_dir=mon_dir;
        m_event_type=event_type;
        m_info_uri=wxString(uri.c_str());
    }
    wxFileSysMonitorEvent(const wxFileSysMonitorEvent& c) : wxEvent(c)
    {
        m_mon_dir=wxString(c.m_mon_dir.c_str());
        m_event_type=c.m_event_type;
        m_info_uri=wxString(c.m_info_uri.c_str());
    }
    wxEvent *Clone() const { return new wxFileSysMonitorEvent(*this); }
    ~wxFileSysMonitorEvent() {}
    wxString m_mon_dir;
    int m_event_type;
    wxString m_info_uri;
};

typedef void (wxEvtHandler::*wxFileSysMonitorEventFunction)(wxFileSysMonitorEvent&);

#define EVT_MONITOR_NOTIFY(id, fn) \
    DECLARE_EVENT_TABLE_ENTRY( wxEVT_MONITOR_NOTIFY, id, -1, \
    (wxObjectEventFunction) (wxEventFunction) \
    wxStaticCastEvent( wxFileSysMonitorEventFunction, & fn ), (wxObject *) NULL ),

///////////////////////////////////////
// DIRECTORY MONITOR CLASS ////////////
///////////////////////////////////////

class wxFileSystemMonitor: public wxEvtHandler
{
public:
    wxFileSystemMonitor(wxEvtHandler *parent, const wxArrayString &uri, int eventfilter=DEFAULT_MONITOR_FILTER);
    virtual ~wxFileSystemMonitor();
    bool Start();
    void OnMonitorEvent(wxFileSysMonitorEvent &e);
private:
    wxArrayString m_uri;
    wxEvtHandler *m_parent;
    int m_eventfilter;
#ifdef __WXGTK__
    void Callback(const wxString &mon_dir, int EventType, const wxString &uri);
    static void MonitorCallback(GnomeVFSMonitorHandle *handle, const gchar *monitor_uri, const gchar *info_uri, GnomeVFSMonitorEventType event_type, gpointer user_data);
    GnomeVFSMonitorHandle *m_h;
#else //WINDOWS
    DirMonitorThread *m_monitorthread;
#endif
    DECLARE_EVENT_TABLE()
};

#endif // WXFILESYSTEMMONITOR_H
