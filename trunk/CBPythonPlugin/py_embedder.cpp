#include "py_embedder.h"
#include <wx/listimpl.cpp>
#include <wx/arrimpl.cpp>
WX_DEFINE_OBJARRAY(PyInstanceCollection);
WX_DEFINE_LIST(PyJobQueue);

using namespace std;

int ID_PY_PROC=wxNewId();



//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// classes PyNotifyIntepreterEvent and PyNotifyUIEvent
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

DEFINE_EVENT_TYPE( wxEVT_PY_NOTIFY_INTERPRETER )

PyNotifyIntepreterEvent::PyNotifyIntepreterEvent(int id) : wxEvent(id, wxEVT_PY_NOTIFY_INTERPRETER)
{
}

DEFINE_EVENT_TYPE( wxEVT_PY_NOTIFY_UI )

PyNotifyUIEvent::PyNotifyUIEvent(int id, PyInstance *inst, wxWindow *window, JobStates js) : wxEvent(id, wxEVT_PY_NOTIFY_UI)
{
    instance=inst;
    jobstate=js;
    job=NULL;
    parent=window;
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// class PyJob
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

PyJob::PyJob(PyInstance *pyinst, wxWindow *p, int id, bool selfdestroy):
wxThread(wxTHREAD_JOINABLE)
{
    parent=p;
    this->id=id;
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

void *PyJob::Entry()
{
    PyNotifyUIEvent pe(id,pyinst,parent,PYSTATE_STARTEDJOB);
    ::wxPostEvent(pyinst,pe);
    if((*this)())
        pe.SetState(PYSTATE_FINISHEDJOB);
    else
        pe.SetState(PYSTATE_ABORTEDJOB);
    ::wxPostEvent(pyinst,pe);
    Exit();
    return NULL;
}


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// class PyInstance
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(PyInstance, wxEvtHandler)
    EVT_PY_NOTIFY_UI(wxID_ANY, PyInstance::OnJobNotify)
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
    // check if any running jobs, kill them
    // delete the client
    delete m_client;
    // kill python process if still running
}

PyInstance::PyInstance(const wxString &processcmd, const wxString &hostaddress, int port)
{
  m_port=port;
  m_hostaddress=hostaddress;
  // Launch process
  LaunchProcess(_T("python pyinterp.py")); //TODO: The command for the interpreter process should come from the manager (and be stored in a config file)
  // Setup XMLRPC client and use introspection API to look up the supported methods
  m_client = new XmlRpc::XmlRpcClient(hostaddress.char_str(), port);
  XmlRpc::XmlRpcValue noArgs, result;
  if (m_client->execute("system.listMethods", noArgs, result))
    std::cout << "\nMethods:\n " << result << "\n\n";
  else
    std::cout << "Error calling 'listMethods'\n\n";
}

long PyInstance::LaunchProcess(const wxString &processcmd)
{
    if(!m_proc_dead)
        return -1;
    if(m_proc) //this should never happen
        m_proc->Detach(); //self cleanup
    m_proc=new wxProcess(this,ID_PY_PROC);
    m_proc->Redirect();
    m_proc_id=wxExecute(processcmd,wxEXEC_ASYNC,m_proc);
    if(m_proc_id>0)
    {
        m_proc_dead=false;
        m_proc_killlevel=0;
    }
    return m_proc_id;
}

PyInstance::PyInstance(const PyInstance &copy)
{
    m_paused=copy.m_paused;
    m_queue=copy.m_queue;
    m_hostaddress=copy.m_hostaddress;
    m_port=copy.m_port;
    m_proc=copy.m_proc;
    m_proc_id=copy.m_proc_id;
    m_proc_killlevel=copy.m_proc_killlevel;
    m_proc_dead=copy.m_proc_dead;
}

bool PyInstance::RunCode(const wxString &codestr, const wxString &stdinstr, bool &unfinished, wxString &stdoutstr, wxString & stderrstr)
{
    XmlRpc::XmlRpcValue args, result;
    args[0]=codestr.utf8_str();
    args[1]=stdinstr.utf8_str();
    if(Exec(_("run_code"), args, result))
    {
        unfinished=result[0];
        std::string r1=result[1];
        stdoutstr.FromUTF8(r1.c_str());
        std::string r2=result[2];
        stderrstr.FromUTF8(r2.c_str());
        return true;
    }
    return false;
}

bool PyInstance::Continue(const wxString &stdinstr, bool &unfinished, wxString &stdoutstr, wxString & stderrstr)
{
    XmlRpc::XmlRpcValue args, result;
    args[0]=stdinstr.utf8_str();
    if(Exec(_("cont"), args, result))
    {
        unfinished=result[0];
        std::string r1=result[1];
        stdoutstr.FromUTF8(r1.c_str());
        std::string r2=result[2];
        stderrstr.FromUTF8(r2.c_str());
        return true;
    }
    return false;
}

bool PyInstance::Kill()
{
    XmlRpc::XmlRpcValue args, result;
    if(Exec(_("end"), args, result))
    {
        return true;
    }
    return false;
}

bool PyInstance::Exec(const wxString &method, XmlRpc::XmlRpcValue &inarg, XmlRpc::XmlRpcValue &result)
{
    wxMutexLocker ml(exec_mutex);
    return m_client->execute(method.mb_str(), inarg, result);
}

void PyInstance::KillProcess(bool force)
{
    if(m_proc_dead)
        return;
    long pid=GetPid();
    if(m_proc_killlevel==0)
    {
        m_proc_killlevel=1;
        if(wxProcess::Exists(pid))
            wxProcess::Kill(pid,wxSIGTERM);
        return;
    }
    if(m_proc_killlevel==1)
    {
        if(wxProcess::Exists(pid))
        {
//                cbMessageBox(_T("Forcing..."));
            wxProcess::Kill(pid,wxSIGKILL);
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
    }
    return true;
}

void PyInstance::OnJobNotify(PyNotifyUIEvent &event)
{
    if(event.jobstate==PYSTATE_ABORTEDJOB||event.jobstate==PYSTATE_FINISHEDJOB)
    {
        event.job=m_queue.GetFirst()->GetData();
        event.job->Wait();
        if(event.job->killonexit)
        {
            delete event.job;
            event.job=NULL;
        }
        m_queue.DeleteNode(m_queue.GetFirst());
    }
    if(event.parent)
        ::wxPostEvent(event.parent,event);
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

