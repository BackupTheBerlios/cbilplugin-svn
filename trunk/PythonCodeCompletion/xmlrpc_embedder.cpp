#include "xmlrpc_embedder.h"
#include <wx/listimpl.cpp>
#include <wx/arrimpl.cpp>
#include "ExecHiddenMSW.h"
WX_DEFINE_OBJARRAY(XmlRpcInstanceCollection);
WX_DEFINE_LIST(XmlRpcJobQueue);

using namespace std;
using namespace XmlRpc;

int ID_XMLRPC_PROC=wxNewId();


class ExecAsyncJob: public XmlRpcJob
{
public:
    ExecAsyncJob(XmlRpcInstance *inst, const wxString &method, const XmlRpc::XmlRpcValue &inarg, wxEvtHandler *hndlr, int id=wxID_ANY): XmlRpcJob(inst,hndlr,id)
    {
        m_method=wxString(method.mb_str(wxConvUTF8),wxConvUTF8);
        m_inarg=inarg;
    }
    virtual bool operator()()
    {
        if(xmlrpc_instance->Exec(m_method,m_inarg,m_result))
        {
            XmlRpcResponseEvent pe(id,xmlrpc_instance,parent,XMLRPC_STATE_RESPONSE,m_result);
            ::wxPostEvent(parent,pe);
            return true;
        } else
        {
            XmlRpcResponseEvent pe(id,xmlrpc_instance,parent,XMLRPC_STATE_REQUEST_FAILED,m_result);
            ::wxPostEvent(parent,pe);
            return true;
        }
    }
protected:
    wxString m_method;
    XmlRpc::XmlRpcValue m_inarg;
    XmlRpc::XmlRpcValue m_result;
};


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// class XmlRpcJob
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////


//IMPLEMENT_DYNAMIC_CLASS(XmlRpcJob, wxThread)

XmlRpcJob::XmlRpcJob(XmlRpcInstance *xmlrpc_instance, wxEvtHandler *p, int id, bool selfdestroy):
wxThread(wxTHREAD_JOINABLE)
{
    parent=p;
    this->id=id;
    this->xmlrpc_instance=xmlrpc_instance;
    finished=false;
    started=false;
    killonexit=selfdestroy;
}

void XmlRpcJob::Abort()
{
    if(this->IsAlive())
        this->Kill();
}

XmlRpcJob::~XmlRpcJob()
{
    Abort();
}

//void *XmlRpcJob::Entry()
//{
//    wxMessageBox(_("entered thread"));
//    XmlRpcNotifyUIEvent pe(id,xmlrpc_instance,parent,PYSTATE_STARTEDJOB);
//    ::wxPostEvent(xmlrpc_instance,pe);
//    if((*this)())
//        pe.SetState(PYSTATE_FINISHEDJOB);
//    else
//        pe.SetState(PYSTATE_ABORTEDJOB);
//    ::wxPostEvent(xmlrpc_instance,pe);
//    Exit();
//    return NULL;
//}

void *XmlRpcJob::Entry()
{
//    wxMutexGuiEnter();
    wxCommandEvent pe(wxEVT_XMLRPC_STARTED,0);
    parent->AddPendingEvent(pe);
//    wxMutexGuiLeave();
    if((*this)())
    {
        wxCommandEvent pe(wxEVT_XMLRPC_FINISHED,0);
        xmlrpc_instance->AddPendingEvent(pe);
    }
    else
    {
        wxCommandEvent pe(wxEVT_XMLRPC_ABORTED,0);
        xmlrpc_instance->AddPendingEvent(pe);
    }
    Exit();
    return NULL;
}


const char REQUEST_BEGIN[] =
  "<?xml version=\"1.0\"?>\r\n"
  "<methodCall><methodName>";
const char REQUEST_END_METHODNAME[] = "</methodName>\r\n";
const char PARAMS_TAG[] = "<params>";
const char PARAMS_ETAG[] = "</params>";
const char PARAM_TAG[] = "<param>";
const char PARAM_ETAG[] =  "</param>";
const char REQUEST_END[] = "</methodCall>\r\n";
const char METHODRESPONSE_TAG[] = "<methodResponse>";
const char FAULT_TAG[] = "<fault>";

class XmlRpcPipeClient
{
public:
    XmlRpcPipeClient(wxProcess *proc)
    {
        m_proc=proc;
        m_istream=proc->GetInputStream();
        m_ostream=proc->GetOutputStream();
        m_estream=proc->GetErrorStream();
    }
    bool execute(const char* method, XmlRpc::XmlRpcValue const& params, XmlRpc::XmlRpcValue& result)
    {
        std::cout<<"PyCC Execute start";
        std::string msg;
        if(!generateRequest(method,params,msg))
        {
            std::cout<<"bad request value"<<std::endl;
            return false;
        }
        size_t r_size=msg.size();
        m_ostream->Write(&r_size,sizeof(size_t));
        m_ostream->Write(msg.c_str(),msg.size());
        std::cout<<"wrote "<<msg<<std::endl;
        std::cout<<"wrote "<<r_size<<std::endl;
        char ch=m_istream->GetC();
        std::cout<<"read ch "<<ch<<std::endl;
        for(int i=0;i<sizeof(size_t);i++)
            ((char*)(&r_size))[i]=m_istream->GetC();
        std::cout<<"response size is "<<r_size<<std::endl;
        char *buf = new char[r_size+1];
        for(int i=0;i<r_size;i++)
            buf[i]=m_istream->GetC();
////        m_istream->Read(buf,r_size);
        buf[r_size]=0;
        int offset=0;
        std::cout<<"response was "<<std::string(buf)<<std::endl;
        if(parseResponse(std::string(buf), result))
        {
            return true;
        }
        std::cout<<"bad xml response"<<std::endl;
        return false;
    }
    // Convert the response xml into a result value
    bool parseResponse(std::string _response, XmlRpcValue& result)
    {
      // Parse response xml into result
      int offset = 0;
      bool _isFault=false;
      if ( ! XmlRpcUtil::findTag(METHODRESPONSE_TAG,_response,&offset)) {
        XmlRpcUtil::error("Error in XmlRpcClient::parseResponse: Invalid response - no methodResponse. Response:\n%s", _response.c_str());
        return false;
      }

      // Expect either <params><param>... or <fault>...
      if ((XmlRpcUtil::nextTagIs(PARAMS_TAG,_response,&offset) &&
           XmlRpcUtil::nextTagIs(PARAM_TAG,_response,&offset)) ||
          XmlRpcUtil::nextTagIs(FAULT_TAG,_response,&offset) && (_isFault = true))
      {
        if ( ! result.fromXml(_response, &offset)) {
          XmlRpcUtil::error("Error in XmlRpcClient::parseResponse: Invalid response value. Response:\n%s", _response.c_str());
          _response = "";
          return false;
        }
      } else {
        XmlRpcUtil::error("Error in XmlRpcClient::parseResponse: Invalid response - no param or fault tag. Response:\n%s", _response.c_str());
        _response = "";
        return false;
      }

      _response = "";
      return result.valid();
    }
    // Encode the request to call the specified method with the specified parameters into xml
    bool generateRequest(const char* methodName, XmlRpcValue const& params, std::string &request)
    {
      std::string body = REQUEST_BEGIN;
      body += methodName;
      body += REQUEST_END_METHODNAME;

      // If params is an array, each element is a separate parameter
      if (params.valid()) {
        body += PARAMS_TAG;
        if (params.getType() == XmlRpcValue::TypeArray)
        {
          for (int i=0; i<params.size(); ++i) {
            body += PARAM_TAG;
            body += params[i].toXml();
            body += PARAM_ETAG;
          }
        }
        else
        {
          body += PARAM_TAG;
          body += params.toXml();
          body += PARAM_ETAG;
        }

        body += PARAMS_ETAG;
      }
      body += REQUEST_END;

      std::string header;
      XmlRpcUtil::log(4, "XmlRpcClient::generateRequest: header is %d bytes, content-length is %d.",
                      header.length(), body.length());

      request = body;
      return true;
    }



private:
    wxProcess *m_proc;
    wxInputStream *m_istream;
    wxOutputStream *m_ostream;
    wxInputStream *m_estream;
};


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// class XmlRpcInstance
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

//IMPLEMENT_DYNAMIC_CLASS(XmlRpcInstance, wxEvtHandler)

IMPLEMENT_CLASS(XmlRpcInstance, wxEvtHandler)


BEGIN_EVENT_TABLE(XmlRpcInstance, wxEvtHandler)
//    EVT_PY_NOTIFY_UI(XmlRpcInstance::OnJobNotify)
    EVT_COMMAND(0, wxEVT_XMLRPC_FINISHED, XmlRpcInstance::OnJobNotify)
    EVT_COMMAND(0, wxEVT_XMLRPC_ABORTED, XmlRpcInstance::OnJobNotify)
    EVT_END_PROCESS(ID_XMLRPC_PROC, XmlRpcInstance::OnEndProcess)
END_EVENT_TABLE()

//XmlRpcInstance::XmlRpcInstance()
//{
//    // create python process
//    // create xmlrpc client
//    // get & store methods from the xmlrpc server
//    // set running state flag
//}

XmlRpcInstance::~XmlRpcInstance()
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
    if(m_client)
        delete m_client;
    if(m_pipeclient)
        delete m_pipeclient;
    m_client=NULL;
    m_pipeclient=NULL;
}

XmlRpcInstance::XmlRpcInstance(const wxString &processcmd, const wxString &hostaddress, int port, wxWindow *parent)
{
    m_parent=parent;
    m_port=port;
    m_proc=NULL;
    m_client=NULL;
    m_pipeclient=NULL;
    m_proc_dead=true;
    m_hostaddress=hostaddress;
    m_paused=false;
    m_proc_killlevel=0;
    m_jobrunning=false;
    // Launch process
    LaunchProcess(processcmd); //TODO: The command for the interpreter process should come from the manager (and be stored in a config file)
    // Setup XMLRPC client and use introspection API to look up the supported methods
    if(port==-1)
        m_pipeclient = new XmlRpcPipeClient(m_proc);
    else
        m_client = new XmlRpc::XmlRpcClient(hostaddress.char_str(), port);
}

long XmlRpcInstance::LaunchProcess(const wxString &processcmd)
{
    std::cout<<"PyCC: LAUNCHING PROCESS"<<std::endl;
    if(!m_proc_dead)
        return -1;
    if(m_proc) //this should never happen
        m_proc->Detach(); //self cleanup
    m_proc=new wxProcess(this,ID_XMLRPC_PROC);
    if(m_port==-1)
        m_proc->Redirect();
//    m_proc->Redirect(); //TODO: this only needs to be done on windows and buffers must be flushed periodically if there is any I/O to/from the process
#ifdef __WXMSW__
    //by default wxExecute shows the terminal window on MSW (redirecting would hide it, but that breaks the process if buffers are not flushed periodically)
    if(m_port==-1)
        m_proc_id=wxExecute(processcmd,wxEXEC_ASYNC|wxEXEC_MAKE_GROUP_LEADER,m_proc);
    else
        m_proc_id=wxExecuteHidden(processcmd,wxEXEC_ASYNC|wxEXEC_MAKE_GROUP_LEADER,m_proc);
#else
    m_proc_id=wxExecute(processcmd,wxEXEC_ASYNC|wxEXEC_MAKE_GROUP_LEADER,m_proc);
#endif /*__WXMSW__*/
    if(m_proc_id>0)
    {
        std::cout<<"PyCC: LAUNCHING PROCESS SUCCEEDED"<<std::endl;
        m_proc_dead=false;
        m_proc_killlevel=0;
    }
    return m_proc_id;
}

XmlRpcInstance::XmlRpcInstance(const XmlRpcInstance &copy)
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

bool XmlRpcInstance::Exec(const wxString &method, const XmlRpc::XmlRpcValue &inarg, XmlRpc::XmlRpcValue &result)
{
    wxMutexLocker ml(exec_mutex);
    if(m_client)
        return m_client->execute(method.utf8_str(), inarg, result);
    else if(m_pipeclient)
        return m_pipeclient->execute(method.utf8_str(), inarg, result);
    return false;
}

bool XmlRpcInstance::ExecAsync(const wxString &method, const XmlRpc::XmlRpcValue &inarg, wxEvtHandler *hndlr, int id)
{
    return AddJob(new ExecAsyncJob(this,method,inarg,hndlr,id));
}

void XmlRpcInstance::OnEndProcess(wxProcessEvent &event)
{
    std::cout<<"PYCC: PROCESS DIED!!"<<std::endl;
    //TODO: m_exitcode=event.GetExitCode();
    m_proc_dead=true;
    delete m_proc;
    m_proc=NULL;
    m_proc_id=-1;
    m_proc_killlevel=0;
    wxCommandEvent ce(wxEVT_XMLRPC_PROC_END,0);
    if(m_parent)
        m_parent->AddPendingEvent(ce);
}

void XmlRpcInstance::Break()
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


void XmlRpcInstance::KillProcess(bool force)
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

bool XmlRpcInstance::AddJob(XmlRpcJob *job)
{
    if(job->Create()!=wxTHREAD_NO_ERROR)
        return false;
    m_queue.Append(job);
    if(m_paused)
        return true;
    XmlRpcJob *newjob=m_queue.GetFirst()->GetData();
    if(!newjob->finished && !newjob->started)
    {
        newjob->started=true;
        newjob->Run();
        m_jobrunning=true;
    }
    return true;
}

void XmlRpcInstance::OnJobNotify(wxCommandEvent &event)
{
//    if(event.jobstate==PYSTATE_ABORTEDJOB||event.jobstate==PYSTATE_FINISHEDJOB)
//    {
    XmlRpcJob *job=m_queue.GetFirst()->GetData();
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
    wxXmlRpcJobQueueNode *node=m_queue.GetFirst();
    if(!node)
        return;
    XmlRpcJob *newjob=node->GetData();
    if(!newjob->finished && !newjob->started)
    {
        newjob->started=true;
        newjob->Run();
        m_jobrunning=true;
    }
}

XmlRpcMgr::XmlRpcMgr()
{
}

XmlRpcMgr::~XmlRpcMgr()
{
    m_Interpreters.Empty();
}

XmlRpcInstance *XmlRpcMgr::LaunchInterpreter(const wxString &cmd,int port)
{
    XmlRpcInstance *p=new XmlRpcInstance(cmd,_("localhost"),port);
    if(p)
        m_Interpreters.Add(p);
    return p;
}

XmlRpcMgr &XmlRpcMgr::Get()
{
    if (theSingleInstance.get() == 0)
      theSingleInstance.reset(new XmlRpcMgr);
    return *theSingleInstance;
}

std::auto_ptr<XmlRpcMgr> XmlRpcMgr::theSingleInstance;

