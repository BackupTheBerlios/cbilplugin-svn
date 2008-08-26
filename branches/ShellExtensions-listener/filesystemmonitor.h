#ifndef WXFILESYSTEMMONITOR_H
#define WXFILESYSTEMMONITOR_H

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif

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
Defines a wxFileSysMonitorEvent with public members pointing to the wxFileSystenerMonitor

Also defines event table macro EVT_MONITOR_NOTIFY to notify the caller of change events
*/

BEGIN_DECLARE_EVENT_TYPES()
DECLARE_LOCAL_EVENT_TYPE(wxEVT_MONITOR_NOTIFY, -1)
END_DECLARE_EVENT_TYPES()

class wxFileSysMonitorEvent: public wxEvent
{
public:
    wxFileSysMonitorEvent(wxFileSystemMonitor *fsm, int event_type, const wxString &uri)
    {
        m_fsm=fsm;
        m_event_type=event_type;
        m_info_uri=wxString(uri.c_str());
    }
    wxFileSysMonitorEvent(const wxFileSysMonitorEvent& c) : wxEvent(c)
    {
        m_fsm=c.m_fsm;
        m_event_type=c.m_event_type;
        m_info_uri=wxString(c.m_info_uri.c_str());
    }
    wxEvent *Clone() const { return new wxFileSysMonitorEvent(*this); }
    ~wxFileSysMonitorEvent() {}
    wxFileSystemMonitor *m_fsm;
    int m_event_type;
    wxString m_info_uri;
};

typedef void (wxEvtHandler::*wxFileSysMonitorEventFunction)(wxFileSysMonitorEvent&);

#define EVT_MONITOR_NOTIFY(id, fn) \
    DECLARE_EVENT_TABLE_ENTRY( wxEVT_MONITOR_NOTIFY, id, -1, \
    (wxObjectEventFunction) (wxEventFunction) \
    wxStaticCastEvent( wxFileSysMonitorEventFunction, & fn ), (wxObject *) NULL ),


/*
LISTENER_TYPE_FILE      //GNOME_VFS_MONITOR_FILE
LISTENER_TYPE_DIRECTORY //GNOME_VFS_MONITOR_DIRECTORY
*/

/*
Gnome enum for monitor change events
GNOME_VFS_MONITOR_EVENT_CHANGED,
GNOME_VFS_MONITOR_EVENT_DELETED,
GNOME_VFS_MONITOR_EVENT_STARTEXECUTING,
GNOME_VFS_MONITOR_EVENT_STOPEXECUTING,
GNOME_VFS_MONITOR_EVENT_CREATED,
GNOME_VFS_MONITOR_EVENT_METADATA_CHANGED
*/

/*
Win32
Want to watch for all of these
FILE_NOTIFY_CHANGE_FILE_NAME 0x00000001
FILE_NOTIFY_CHANGE_DIR_NAME 0x00000002
FILE_NOTIFY_CHANGE_ATTRIBUTES 0x00000004
FILE_NOTIFY_CHANGE_SIZE 0x00000008
FILE_NOTIFY_CHANGE_LAST_WRITE 0x00000010
FILE_NOTIFY_CHANGE_CREATION 0x00000040

But possibly exclude these
FILE_NOTIFY_CHANGE_LAST_ACCESS 0x00000020
FILE_NOTIFY_CHANGE_SECURITY 0x00000100

These actions will be reported:
FILE_ACTION_ADDED
FILE_ACTION_REMOVED
FILE_ACTION_MODIFIED
FILE_ACTION_RENAMED_OLD_NAME
FILE_ACTION_RENAMED_NEW_NAME
*/

#define MONITOR_FILE_CHANGED 0x001
#define MONITOR_FILE_DELETED 0x002
#define MONITOR_FILE_CREATED 0x004
#define MONITOR_FILE_ATTRIBUTES 0x080
//TODO: Decide if it is worth having these
#define MONITOR_FILE_STARTEXEC 0x010
#define MONITOR_FILE_STOPEXEC 0x020

#define DEFAULT_MONITOR_FILTER_WIN32 FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_ATTRIBUTES|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_CREATION

#define DEFAULT_MONITOR_FILTER MONITOR_FILE_CHANGED|MONITOR_FILE_DELETED|MONITOR_FILE_CREATED|MONITOR_FILE_ATTRIBUTES


class wxFileSystemMonitor: public wxEvtHandler
{
public:
    wxFileSystemMonitor(wxEvtHandler *parent=NULL, const wxString &uri=wxEmptyString, int eventfilter=DEFAULT_MONITOR_FILTER);
    virtual ~wxFileSystemMonitor();
    bool Start();
    virtual void Callback(int EventType, const wxString &uri);
    void OnMonitorEvent(wxFileSysMonitorEvent &e);
protected:
private:
    wxString m_uri;
    wxEvtHandler *m_parent;
    int m_eventfilter;
#ifdef __WXGTK__
    static void MonitorCallback(GnomeVFSMonitorHandle *handle, const gchar *monitor_uri, const gchar *info_uri, GnomeVFSMonitorEventType event_type, gpointer user_data);
    GnomeVFSMonitorHandle *m_h;
#else
    DirMonitorThread *m_monitorthread;
#endif
    DECLARE_EVENT_TABLE()
};

#endif // WXFILESYSTEMMONITOR_H
