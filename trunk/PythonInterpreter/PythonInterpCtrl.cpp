#include <wx/notebook.h>
#include <wx/textctrl.h>
#include <wx/regex.h>
#include "PythonInterpCtrl.h"
#include <globals.h>

//DECLARE_LOCAL_EVENT_TYPE(wxEVT_PY_NOTIFY_UI_CODEOK, -1)
DEFINE_EVENT_TYPE( wxEVT_PY_NOTIFY_UI_CODEOK )

//DECLARE_LOCAL_EVENT_TYPE(wxEVT_PY_NOTIFY_UI_INPUT, -1)
DEFINE_EVENT_TYPE( wxEVT_PY_NOTIFY_UI_INPUT )

//DECLARE_LOCAL_EVENT_TYPE(wxEVT_PY_PROC_END, -1)
DEFINE_EVENT_TYPE( wxEVT_PY_PROC_END )

////////////////////////////////////// PythonIOCtrl //////////////////////////////////////////////

BEGIN_EVENT_TABLE(PythonIOCtrl, wxTextCtrl)
    EVT_CHAR(PythonIOCtrl::OnUserInput)
//    EVT_TEXT(wxID_ANY, PythonIOCtrl::OnTextChange)
//    EVT_COMMAND(0, wxEVT_PY_NOTIFY_UI_INPUT, PythonIOCtrl::OnLineInputRequest)
END_EVENT_TABLE()

void PythonIOCtrl::OnUserInput(wxKeyEvent& ke)
{
    if(ke.GetModifiers()==wxMOD_CONTROL)
    {
//        wxMessageBox(_T("control pressed"));
//        wxMessageBox(wxString::Format(_("Key: %i"),ke.GetKeyCode()));
//        if(ke.GetKeyCode()==4)
//        {
//            m_pyctrl->DispatchCode(GetValue()); // would need to retrieve the code control's value
////            if(m_pyctrl->DispatchCode(GetValue()))
////                ChangeValue(_T(""));
//            return;
//        }
        if(ke.GetKeyCode()==5)
        {
            m_pyctrl->BreakCode();
            return;
        }
    }
    if(ke.GetKeyCode()==WXK_RETURN)
    {
        if(!ke.ShiftDown() && !ke.ControlDown())
        {
            if(m_line_entry_mode)
            {
                m_line_entry_mode=false;
                this->SetEditable(false);
                wxString line;
                if(m_line_entry_point<GetLastPosition())
                    line=GetRange(m_line_entry_point,GetLastPosition());
                line.Replace(_T("\n"),_T("")); //will cause major problems if there is more than one line feed returned here, so we remove them (TODO: fix on server side?? server should treat buffered lines as new input without errors)
                line.Append(_T("\n"));
                m_pyctrl->stdin_append(line);
                return;
            }
        }
    }
    ke.Skip();
}

void PythonIOCtrl::OnTextChange(wxCommandEvent &e)
{
//    if(m_line_entry_mode && this->GetInsertionPoint()<m_line_entry_point)
//        return;
    e.Skip(true);
}

void PythonIOCtrl::LineInputRequest()
{
    if(!m_line_entry_mode)
    {
        m_line_entry_mode=true;
        m_line_entry_point=this->GetLastPosition();
        this->SetSelection(m_line_entry_point,m_line_entry_point);
        this->SetEditable(true);
    }
}
////////////////////////////////////// PythonCodeCtrl //////////////////////////////////////////////

BEGIN_EVENT_TABLE(PythonCodeCtrl, wxTextCtrl)
    EVT_KEY_DOWN(PythonCodeCtrl::OnUserInput)
END_EVENT_TABLE()

PythonCodeCtrl::PythonCodeCtrl(wxWindow *parent, PythonInterpCtrl *py)
    : wxTextCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, /*wxTE_RICH|*/ wxTE_MULTILINE|wxTE_PROCESS_ENTER|wxTE_PROCESS_TAB|wxEXPAND)
{
    m_pyctrl = py;
//    Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(PythonCodeCtrl::OnUserInput));
}

void PythonCodeCtrl::OnUserInput(wxKeyEvent& ke)
{
    Manager::Get()->GetLogManager()->Log(_T("Key event"));
    if(ke.GetModifiers()==wxMOD_CONTROL)
    {
//        wxMessageBox(_T("control pressed"));
//        wxMessageBox(wxString::Format(_("Key: %i"),ke.GetKeyCode()));
        if(ke.GetKeyCode()=='D')
        {
            m_pyctrl->DispatchCode(GetValue());
//            if(m_pyctrl->DispatchCode(GetValue()))
//                ChangeValue(_T(""));
            return;
        }
        if(ke.GetKeyCode()=='K')
        {
            m_pyctrl->BreakCode();
            return;
        }
        if(ke.GetKeyCode()==WXK_RETURN)
        {
            m_pyctrl->DispatchCode(GetValue());
            return;
        }
        if(ke.GetKeyCode()==WXK_UP) //TODO: If the text is ever changed, set m_history_position to -1
        {
            if (m_history_position < 0)
                m_history_working = GetValue();
            m_history_position--;
            if (m_history_position < -1)
                m_history_position = m_history_commands.GetCount()-1;
            if (m_history_position >= 0)
                SetValue(m_history_commands[m_history_position]);
            else
                SetValue(m_history_working);
            int pos = GetLastPosition();
            SetSelection(pos,pos);
            return;
        }
        if(ke.GetKeyCode()==WXK_DOWN)
        {
            if (m_history_position < 0)
                m_history_working = GetValue();
            m_history_position++;
            if (m_history_position >= int(m_history_commands.GetCount()))
                m_history_position = -1;
            if (m_history_position >= 0)
                SetValue(m_history_commands[m_history_position]);
            else
                SetValue(m_history_working);
            int pos = GetLastPosition();
            SetSelection(pos,pos);
            return;
        }

    }
    if(ke.GetModifiers()==wxMOD_NONE && ke.GetKeyCode()==WXK_RETURN)
    {
        wxString code = GetValue();
        wxString lastline = code.AfterLast(_T('\n'));
        if (!lastline.StartsWith(_T("\t")) && !lastline.StartsWith(_T("\t")) &&
            !lastline.EndsWith(_T(":")))
        {
            long rs,re;
            GetSelection(&rs,&re);
            if(rs==re && this->GetLastPosition()==rs)
            {
                m_pyctrl->DispatchCode(code);
                return;
            }
        }
    }
    ke.Skip();
}

////////////////////////////////////// PythonInterpCtrl /////////////////////////////////////////////
int ID_PROC=wxNewId();

IMPLEMENT_DYNAMIC_CLASS(PythonInterpCtrl, wxPanel)

BEGIN_EVENT_TABLE(PythonInterpCtrl, wxPanel)
    EVT_CHAR(PythonInterpCtrl::OnUserInput)
//    EVT_PY_NOTIFY_UI(PythonInterpCtrl::OnPyNotify)
//    EVT_END_PROCESS(ID_PROC, PythonInterpCtrl::OnEndProcess)
//    EVT_LEFT_DCLICK(PythonInterpCtrl::OnDClick)
    EVT_XMLRPC_RESPONSE(wxID_ANY, PythonInterpCtrl::OnPyNotify)
//    EVT_COMMAND(0, wxEVT_PY_NOTIFY_UI_CODEOK, PythonInterpCtrl::OnPyCode)
//    EVT_COMMAND(0, wxEVT_PY_NOTIFY_UI_INPUT, PythonInterpCtrl::OnLineInputRequest)
    EVT_COMMAND(0, wxEVT_XMLRPC_PROC_END, PythonInterpCtrl::OnEndProcess)

    EVT_SIZE    (PythonInterpCtrl::OnSize)
END_EVENT_TABLE()

PythonInterpCtrl::PythonInterpCtrl(wxWindow* parent, int id, const wxString &name, ShellManager *shellmgr) : ShellCtrlBase(parent, id, name, shellmgr)
{
    m_killlevel=0;
    m_port=0;
    m_pyinterp=NULL;
    m_sw=new wxSplitterWindow(this, wxID_ANY);
    m_ioctrl=new PythonIOCtrl(m_sw, this);//new wxTextCtrl(m_sw, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_RICH|wxTE_MULTILINE|wxTE_READONLY|wxTE_PROCESS_ENTER|wxTE_PROCESS_TAB|wxEXPAND);
    m_codectrl=new PythonCodeCtrl(m_sw, this);
    m_codectrl->AppendText(_("print 'Python'"));
    m_sw->SetMinimumPaneSize(20);
    m_sw->SetSashGravity(0.3);
    int sash_pos=parent->GetClientSize().GetHeight()/5;
    if(sash_pos<20)
        sash_pos=0;
    m_sw->SplitHorizontally(m_codectrl, m_ioctrl, sash_pos);
    wxBoxSizer* bs = new wxBoxSizer(wxVERTICAL);
    bs->Add(m_sw, 1, wxEXPAND | wxALL);
    SetAutoLayout(TRUE);
    SetSizer(bs);
}

PortAllocator PythonInterpCtrl::m_portalloc;

long PythonInterpCtrl::LaunchProcess(const wxString &processcmd, const wxArrayString &options) // bool ParseLinks, bool LinkClicks, const wxString &LinkRegex
{
    if(!IsDead())
        return -1;
//TODO: Option to use XMLRPC over localhost or pipe, currently always use pipe
//    m_port=m_portalloc.RequestPort(); //TODO: Request port limited to 3 ports
//    if(m_port<0)
//        return -1;
    m_port = -1; //Use XmlRpc over pipe
    //TODO: get the command and working dir from config
#ifdef __WXMSW__
//    wxString cmd=_T("cmd /c interp.py ")+wxString::Format(_T(" %i"),m_port); //TODO: this could have process destruction issues on earlier version of wxWidgets (kills cmd, but not python)
    wxString cmd=_T("python -u interp.py ")+wxString::Format(_T(" %i"),m_port); //TODO: this could have process destruction issues on earlier version of wxWidgets (kills cmd, but not python)
    wxString python=_T("\\python");
    wxString interp=_T("\\interp.py");
#else
    wxString cmd=_T("python -u interp.py ")+wxString::Format(_T(" %i"),m_port);
    wxString python=_T("/python");
    wxString interp=_T("/interp.py");
#endif
    wxString gpath = ConfigManager::GetDataFolder(true)+python;
    wxString lpath = ConfigManager::GetDataFolder(false)+python;
    bool global=false,local=false;
    if(wxFileName::FileExists(gpath+interp))
    {
        wxSetWorkingDirectory(gpath);
        global=true;
    }
    if(wxFileName::FileExists(lpath+interp))
    {
        wxSetWorkingDirectory(lpath);
        local=true;
    }
    if(!global&&!local) //No interpreter script found, return failure.
        return -2; //TODO: Return meaningful messages (or at least use the codeblocks logger)

    m_pyinterp = new XmlRpcInstance(cmd,m_port,_T("localhost"),this);
    if(m_pyinterp->IsDead())
    {
        Manager::Get()->GetLogManager()->Log(_("Error Starting Interpreter"));
        return -1;
    }
    //TODO: Perform any initial communication with the running python process...
    return 1;
}

void PythonInterpCtrl::KillProcess()
{
    if(IsDead())
        return;
    if(m_killlevel==0)
    {
        SendKill();
        m_killlevel=1;
        return;
    }
    if(m_killlevel==1)
    {
        m_pyinterp->KillProcess();
        return;
    }
}

bool PythonInterpCtrl::DispatchCode(const wxString &code)
{
    if(m_pyinterp->IsJobRunning())
        return false;
    m_code=code;
    m_codectrl->m_history_commands.Add(code);
    m_codectrl->m_history_position = -1;
    if (RunCode(wxString(code.c_str())))
    {
        m_codectrl->Enable(false);
//        wxCommandEvent pe(wxEVT_PY_NOTIFY_UI_CODEOK,0);
//        ::wxPostEvent(this,pe);
    }
    return true;
}

void PythonInterpCtrl::OnPyNotify(XmlRpcResponseEvent& event)
{
//    if(m_ioctrl->m_line_entry_mode)
//        return;
    if (event.GetState()==XMLRPC_STATE_RESPONSE)
    {
        XmlRpc::XmlRpcValue val = event.GetResponse();
        int return_code = val[0];
        std::string sstdout(val[1]);
        std::string sstderr(val[2]);
        bool input_request = val[3];

        if (return_code>=0 && m_code!=wxEmptyString)
        {
            wxTextAttr oldta=m_ioctrl->GetDefaultStyle();
            wxTextAttr ta=oldta;
            wxFont f=ta.GetFont();
            f.SetWeight(wxFONTWEIGHT_BOLD);
            ta.SetFont(f);
            m_ioctrl->SetDefaultStyle(ta);
            m_ioctrl->AppendText(m_code);
            m_ioctrl->SetDefaultStyle(oldta);
            m_ioctrl->AppendText(_T("\n"));
            m_code = wxEmptyString;
        }

        if (return_code == -2)
        {
            m_codectrl->AppendText(_T("\n"));
        }

        m_ioctrl->AppendText(wxString(sstdout.c_str(),wxConvUTF8));
        //TODO: Col or errors
        m_ioctrl->AppendText(wxString(sstderr.c_str(),wxConvUTF8));

        if (input_request)
            m_ioctrl->ProcessEvent(event); //TODO: Need to set up the handler on the ioctrl

        if (return_code == 1) // Eval was successful and is still running
        {
            m_codectrl->Clear();
            this->Continue();
            return;
        }
        if (return_code == 0) //Eval has finished
        {
            m_codectrl->Clear();
            //Now fall through to re-enable the control
        }
        //Otherwise it's an error (-1 syntax error, -2 statement incomplete, -3 code already running
        m_codectrl->Enable();
        m_codectrl->SetFocus();
    } else
    {
        if (event.GetState()==XMLRPC_STATE_REQUEST_FAILED)
            Manager::Get()->GetLogManager()->LogError(_T("Error parsing response from python interpreter console\n")+wxString(event.GetResponse().toXml().c_str(),wxConvUTF8));
        //Maybe show a popup?
    }
    //TODO: If not in error and more input available, need to call RunCode("cont")
}

void PythonInterpCtrl::OnLineInputRequest(wxCommandEvent& event)
{
    m_ioctrl->ProcessEvent(event);
}


void PythonInterpCtrl::OnPyCode(wxCommandEvent& event)
{
    wxTextAttr oldta=m_ioctrl->GetDefaultStyle();
    wxTextAttr ta=oldta;
    wxFont f=ta.GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    ta.SetFont(f);
    m_ioctrl->SetDefaultStyle(ta);
    m_ioctrl->AppendText(m_code);
    m_ioctrl->SetDefaultStyle(oldta);
    m_ioctrl->AppendText(_T("\n"));
}

void PythonInterpCtrl::OnPyJobDone(wxCommandEvent& event)
{
    m_codectrl->ChangeValue(_T(""));
    m_code=_T("");
}

void PythonInterpCtrl::OnPyJobAbort(wxCommandEvent& event)
{
    m_codectrl->AppendText(_T("\n"));
    m_code=_T("");
}

void PythonInterpCtrl::OnEndProcess(wxCommandEvent& event)
{
    m_portalloc.ReleasePort(m_port);
    if(m_shellmgr)
        m_shellmgr->OnShellTerminate(this);
}

void PythonInterpCtrl::stdin_append(const wxString &data)
{ //asynchronously dispatch data to python interpreter's stdin
    stdin_data+=data;
}

wxString PythonInterpCtrl::stdin_retrieve()
 {
    wxString s(stdin_data);
    stdin_data=_T("");
    return s;
}

void PythonInterpCtrl::SyncOutput(int maxchars)
{
//    if(!m_proc)
//        return;
//    bool oneshot=true;
//    if(maxchars<=0)
//    {
//        maxchars=20000;
//        oneshot=false;
//    }
//    int lineno=m_textctrl->GetNumberOfLines()-1;
//    if(lineno<0)
//        lineno=0;
//    while(m_proc->IsInputAvailable())
//    {
//        char buf0[maxchars+1];
//        for(int i=0;i<maxchars+1;i++)
//            buf0[i]=0;
//        m_istream->Read(buf0,maxchars);
//        wxString m_latest=wxString::FromAscii(buf0);
//        m_textctrl->AppendText(m_latest);
//        if(oneshot)
//            break;
//    }
//    if(m_proc->IsErrorAvailable())
//    {
//        wxTextAttr ta(wxColour(255,0,0));
//        wxTextAttr oldta=m_textctrl->GetDefaultStyle();
//        m_textctrl->SetDefaultStyle(ta);
//        while(m_proc->IsErrorAvailable())
//        {
//            char buf0[maxchars+1];
//            for(int i=0;i<maxchars+1;i++)
//                buf0[i]=0;
//            m_estream->Read(buf0,maxchars);
//            wxString m_latest=wxString::FromAscii(buf0);
//            m_textctrl->AppendText(m_latest);
//            if(oneshot)
//                break;
//        }
//        m_textctrl->SetDefaultStyle(oldta);
//    }
//    if(m_parselinks)
//        ParseLinks(lineno,m_textctrl->GetNumberOfLines());
}

void PythonInterpCtrl::OnSize(wxSizeEvent& event)
{
    m_sw->SetSize(event.GetSize());
}

void PythonInterpCtrl::OnUserInput(wxKeyEvent& ke)
{
    if(!IsDead())
    {
        ke.Skip();
        return;
    }
//    if(ke.GetModifiers()==wxMOD_CONTROL && ke.GetKeyCode()==WXK_RETURN)
//    {
//        PyInterpJob m_job(m_codectrl->GetValue(),m_pyinterp,this);
//        m_pyinterp->AddJob(m_job);
//        m_codectrl->ChangeValue(_T(""));
//        wxMessageBox(_T("command dispatched"));
//    }
//    wxMessageBox(_T("key captured"));

//    char* kc1=new char[2];
//    kc1[0]=ke.GetKeyCode()%256;
//    kc1[1]=0;
//    if(kc1[0]=='\r')
//    {
//        kc1[0]='\n';
//    }
//    wxChar kc2=ke.GetUnicodeKey();
//    wxString buf(kc2);
//    //TODO: queue the key press for dispatch to the interpreter
//    m_ioctrl->AppendText(kc2);
//    m_ioctrl->SetInsertionPointEnd();
}


bool PythonInterpCtrl::RunCode(const wxString &codestr)
{
//TODO: Should use a common ID for this and cont
    XmlRpc::XmlRpcValue args;
    args[0]=codestr.utf8_str();
    args[1]=stdin_retrieve().utf8_str();
    return m_pyinterp->ExecAsync(_T("run_code"), args, this);
}

bool PythonInterpCtrl::Continue()
{
    XmlRpc::XmlRpcValue args;
    args[0]=stdin_retrieve().utf8_str();
    return m_pyinterp->ExecAsync(_T("cont"), args, this);
}

bool PythonInterpCtrl::BreakCode()
{
//TODO: Need to use a separate ID for this
    XmlRpc::XmlRpcValue args;
    return m_pyinterp->ExecAsync(_T("break_code"), args, this);
}

bool PythonInterpCtrl::SendKill()
{
//TODO: Need to use a separate ID for this
    XmlRpc::XmlRpcValue args;
    return m_pyinterp->ExecAsync(_T("end"), args, this);
}

