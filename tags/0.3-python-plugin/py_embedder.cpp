#include "py_embedder.h"
#include <wx/listimpl.cpp>
#include <wx/arrimpl.cpp>
#include "ExecHiddenMSW.h"
WX_DEFINE_OBJARRAY(PyInstanceCollection);
WX_DEFINE_LIST(PyJobQueue);

using namespace std;

int ID_PY_PROC=wxNewId();



//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// class PyJob
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////


//IMPLEMENT_DYNAMIC_CLASS(PyJob, wxThread)

PyJob::PyJob(PyInstance *pyinst, wxWindow *p, int id, bool selfdestroy):
wxThread(wxTHREAD_JOINABLE)
{
    parent=p;
    this->id=id;
    this->pyinst=pyinst;
    finished=false;
    started=false;
    killonexit=selfdestroy;
}

void PyJob::Abort()
{
    if(this->IsAlive())
        this->Kill();
}

PyJob::~PyJob()
{
    Abort();
}

//void *PyJob::Entry()
//{
//    wxMessageBox(_("entered thread"));
//    PyNotifyUIEvent pe(id,pyinst,parent,PYSTATE_STARTEDJOB);
//    ::wxPostEvent(pyinst,pe);
//    if((*this)())
//        pe.SetState(PYSTATE_FINISHEDJOB);
//    else
//        pe.SetState(PYSTATE_ABORTEDJOB);
//    ::wxPostEvent(pyinst,pe);
//    Exit();
//    return NULL;
//}

void *PyJob::Entry()
{
//    wxMutexGuiEnter();
    wxCommandEvent pe(wxEVT_PY_NOTIFY_UI_STARTED,0);
    parent->AddPendingEvent(pe);
//    wxMutexGuiLeave();
    if((*this)())
    {
        wxCommandEvent pe(wxEVT_PY_NOTIFY_UI_FINISHED,0);
        pyinst->AddPendingEvent(pe);
    }
    else
    {
        wxCommandEvent pe(wxEVT_PY_NOTIFY_UI_ABORTED,0);
        pyinst->AddPendingEvent(pe);
    }
    Exit();
    return NULL;
}


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// class PyInstance
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

//IMPLEMENT_DYNAMIC_CLASS(PyInstance, wxEvtHandler)

IMPLEMENT_CLASS(PyInstance, wxEvtHandler)


BEGIN_EVENT_TABLE(PyInstance, wxEvtHandler)
//    EVT_PY_NOTIFY_UI(PyInstance::OnJobNotify)
    EVT_COMMAND(0, wxEVT_PY_NOTIFY_UI_FINISHED, PyInstance::OnJobNotify)
    EVT_COMMAND(0, wxEVT_PY_NOTIFY_UI_ABORTED, PyInstance::OnJobNotify)
    EVT_END_PROCESS(ID_PY_PROC, PyInstance::OnEndProcess)
END_EVENT_TABLE()

//PyInstance::PyInstance()
//{
//    // create python process
//    // create xmlrpc client
//    // get & store methods from the xmlrpc server
//    // set running state flag
//}

PyInstance::~PyInstance()
{
    // kill python process if still running
    if (!m_proc_dead) //TODO: killing while job running could cause probs...
    {
        m_proc->Detach();
        if(wxProcess::Exists(m_proc_id))
            wxProcess::Kill(m_proc_id,wxSIGKILL,wxKILL_CHILDREN);
    }
    // check if any running jobs, kill them
    // delete the client
    wxMutexLocker ml(exec_mutex); //TODO: This may result in huge delays?
    delete m_client;
    m_client=NULL;

}

PyInstance::PyInstance(const wxString &processcmd, const wxString &hostaddress, int port, wxWindow *parent)
{
  m_parent=parent;
  m_port=port;
  m_proc=NULL;
  m_proc_dead=true;
  m_hostaddress=hostaddress;
  m_paused=false;
  m_proc_killlevel=0;
  m_jobrunning=false;
  // Launch process
  LaunchProcess(processcmd); //TODO: The command for the interpreter process should come from the manager (and be stored in a config file)
  // Setup XMLRPC client and use introspection API to look up the supported methods
  m_client = new XmlRpc::XmlRpcClient(hostaddress.char_str(), port);
//  XmlRpc::XmlRpcValue noArgs, result;
//  if (m_client->execute("system.listMethods", noArgs, result))
//  {
////    wxMessageBox(wxString::Format(_T("Called 'listMethods'\n\n")));
////    std::string s(result[0]);
////    wxMessageBox(wxString::Format(_T("\nMethods: %s\n "), s.c_str()));
//  }
//  else
////    wxMessageBox(wxString::Format(_T("Error calling 'listMethods'\n\n")));
}

long PyInstance::LaunchProcess(const wxString &processcmd)
{
    if(!m_proc_dead)
        return -1;
    if(m_proc) //this should never happen
        m_proc->Detach(); //self cleanup
    m_proc=new wxProcess(this,ID_PY_PROC);
//    m_proc->Redirect(); //TODO: this only needs to be done on windows and buffers must be flushed periodically if there is any I/O to/from the process
#ifdef __WXMSW__
    //by default wxExecute shows the terminal window on MSW (redirecting would hide it, but that breaks the process if buffers are not flushed periodically)
    m_proc_id=wxExecuteHidden(processcmd,wxEXEC_ASYNC|wxEXEC_MAKE_GROUP_LEADER,m_proc);
#else
    m_proc_id=wxExecute(processcmd,wxEXEC_ASYNC|wxEXEC_MAKE_GROUP_LEADER,m_proc);
#endif /*__WXMSW__*/
    if(m_proc_id>0)
    {
        m_proc_dead=false;
        m_proc_killlevel=0;
    }
    return m_proc_id;
}

PyInstance::PyInstance(const PyInstance &copy)
{
    m_parent=copy.m_parent;
    m_paused=copy.m_paused;
    m_queue=copy.m_queue;
    m_hostaddress=copy.m_hostaddress;
    m_port=copy.m_port;
    m_proc=copy.m_proc;
    m_proc_id=copy.m_proc_id;
    m_proc_killlevel=copy.m_proc_killlevel;
    m_proc_dead=copy.m_proc_dead;
}

bool PyInstance::Exec(const wxString &method, XmlRpc::XmlRpcValue &inarg, XmlRpc::XmlRpcValue &result)
{
    wxMutexLocker ml(exec_mutex);
    if(m_client)
        return m_client->execute(method.utf8_str(), inarg, result);
    return false;
}

void PyInstance::OnEndProcess(wxProcessEvent &event)
{
    //TODO: m_exitcode=event.GetExitCode();
    m_proc_dead=true;
    delete m_proc;
    m_proc=NULL;
    m_proc_id=-1;
    m_proc_killlevel=0;
    wxCommandEvent ce(wxEVT_PY_PROC_END,0);
    if(m_parent)
        m_parent->AddPendingEvent(ce);
}

void PyInstance::Break()
{
    if(!m_jobrunning)
        return;
    long pid=GetPid();
    if(wxProcess::Exists(pid))
    {
#ifdef __WXMSW__
        //TODO: Verify that this actually works
        // Use the WIN32 native call to send a CTRL+C signal
        GenerateConsoleCtrlEvent(CTRL_C_EVENT,pid); //may need to #include <Windows.h> and link to Kernel32.dll
        // also need to check whether the pid is valid or something else needs to be used.
#else
        wxProcess::Kill(pid,wxSIGINT); //,wxKILL_CHILDREN?
#endif
    }
}


void PyInstance::KillProcess(bool force)
{
    if(m_proc_dead)
        return;
    long pid=GetPid();
//    if(m_proc_killlevel==0)
//    {
//        m_proc_killlevel=1;
//        if(wxProcess::Exists(pid))
//            wxProcess::Kill(pid,wxSIGTERM);
//        return;
//    }
    if(m_proc_killlevel==0)
    {
        if(wxProcess::Exists(pid))
        {
            wxProcess::Kill(pid,wxSIGKILL,wxKILL_CHILDREN);
        }
    }
}

bool PyInstance::AddJob(PyJob *job)
{
    if(job->Create()!=wxTHREAD_NO_ERROR)
        return false;
    m_queue.Append(job);
    if(m_paused)
        return true;
    PyJob *newjob=m_queue.GetFirst()->GetData();
    if(!newjob->finished && !newjob->started)
    {
        newjob->started=true;
        newjob->Run();
        m_jobrunning=true;
    }
    return true;
}

void PyInstance::OnJobNotify(wxCommandEvent &event)
{
//    if(event.jobstate==PYSTATE_ABORTEDJOB||event.jobstate==PYSTATE_FINISHEDJOB)
//    {
    PyJob *job=m_queue.GetFirst()->GetData();
    job->Wait();
    if(job->killonexit)
    {
        delete job;
        job=NULL;
    }
    m_jobrunning=false;
    m_queue.DeleteNode(m_queue.GetFirst());
//    }
//    if(event.parent)
//        ::wxPostEvent(event.parent,event);
    if(m_paused)
        return;
    wxPyJobQueueNode *node=m_queue.GetFirst();
    if(!node)
        return;
    PyJob *newjob=node->GetData();
    if(!newjob->finished && !newjob->started)
    {
        newjob->started=true;
        newjob->Run();
        m_jobrunning=true;
    }
}

void PyInstance::EvalString(char *str, bool wait)
{
//    PyRun_SimpleString(str);
}

PyMgr::PyMgr()
{
}

PyMgr::~PyMgr()
{
    m_Interpreters.Empty();
}

PyInstance *PyMgr::LaunchInterpreter()
{
    PyInstance *p=new PyInstance(_("python interp.py"),_("localhost"),8000);
    if(p)
        m_Interpreters.Add(p);
    return p;
}

PyMgr &PyMgr::Get()
{
    if (theSingleInstance.get() == 0)
      theSingleInstance.reset(new PyMgr);
    return *theSingleInstance;
}

std::auto_ptr<PyMgr> PyMgr::theSingleInstance;

