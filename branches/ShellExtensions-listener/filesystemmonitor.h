#ifndef WXFILESYSTEMMONITOR_H
#define WXFILESYSTEMMONITOR_H

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif

#ifdef __WXGTK__
#include <libgnomevfs/gnome-vfs.h>
#endif

#include <map>

class wxFileSystemMonitor;

#ifdef __WXGTK__
typedef std::map<GnomeVFSMonitorHandle *, wxFileSystemMonitor *> MonMap;
static MonMap m;
#endif


/*
WIN32
Before starting loop
HANDLE WINAPI FindFirstChangeNotification(
  __in  LPCTSTR lpPathName,
  __in  BOOL bWatchSubtree,
  __in  DWORD dwNotifyFilter
);

Enter loop, then wait
DWORD WINAPI MsgWaitForMultipleObjects(
  __in  DWORD nCount,
  __in  const HANDLE *pHandles,
  __in  BOOL bWaitAll,
  __in  DWORD dwMilliseconds,
  __in  DWORD dwWakeMask
);

Queue next wait
BOOL WINAPI FindNextChangeNotification(
  __in  HANDLE hChangeHandle
);

Clean up when done
BOOL WINAPI FindCloseChangeNotification(
  __in  HANDLE hChangeHandle
);


BOOL WINAPI ReadDirectoryChangesW(
  __in         HANDLE hDirectory,
  __out        LPVOID lpBuffer,
  __in         DWORD nBufferLength,
  __in         BOOL bWatchSubtree,
  __in         DWORD dwNotifyFilter,
  __out_opt    LPDWORD lpBytesReturned,
  __inout_opt  LPOVERLAPPED lpOverlapped,
  __in_opt     LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);

lpBuffer contains:

typedef struct _FILE_NOTIFY_INFORMATION {
  DWORD NextEntryOffset;
  DWORD Action;
  DWORD FileNameLength;
  WCHAR FileName[1];
} FILE_NOTIFY_INFORMATION,
 *PFILE_NOTIFY_INFORMATION;
*/

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
FILE_NOTIFY_CHANGE_FILE_NAME 0x00000001
FILE_NOTIFY_CHANGE_DIR_NAME 0x00000002
FILE_NOTIFY_CHANGE_ATTRIBUTES 0x00000004
FILE_NOTIFY_CHANGE_SIZE 0x00000008
FILE_NOTIFY_CHANGE_LAST_WRITE 0x00000010
FILE_NOTIFY_CHANGE_LAST_ACCESS 0x00000020
FILE_NOTIFY_CHANGE_CREATION 0x00000040
FILE_NOTIFY_CHANGE_SECURITY 0x00000100
*/

//void MonitorCallback(GnomeVFSMonitorHandle *handle, const gchar *monitor_uri, const gchar *info_uri, GnomeVFSMonitorEventType event_type, gpointer user_data);

class wxFileSystemMonitor: public wxEvtHandler
{
public:
    wxFileSystemMonitor(wxEvtHandler *parent=NULL, const wxString &uri=wxEmptyString);
    virtual ~wxFileSystemMonitor();
    virtual void Callback(int EventType, const wxString &uri);
protected:
private:
    wxString m_uri;
    wxEvtHandler *m_parent;
#ifdef __WXGTK__
    static void MonitorCallback(GnomeVFSMonitorHandle *handle, const gchar *monitor_uri, const gchar *info_uri, GnomeVFSMonitorEventType event_type, gpointer user_data);
    GnomeVFSMonitorHandle *m_h;
#else
#endif
};

#endif // WXFILESYSTEMMONITOR_H
