#include <wx/notebook.h>
#include <wx/textctrl.h>
#include <wx/regex.h>
#include "PipedProcessCtrl.h"
#include <globals.h>

////////////////////////////////////// PipedProcessCtrl /////////////////////////////////////////////
#define PP_ERROR_STYLE 1
#define PP_LINK_STYLE 2


int ID_PROC=wxNewId();

BEGIN_EVENT_TABLE(PipedTextCtrl, wxScintilla)
    EVT_LEFT_DCLICK(PipedTextCtrl::OnDClick)
    EVT_KEY_DOWN(PipedTextCtrl::OnUserInput)
END_EVENT_TABLE()

PipedTextCtrl::PipedTextCtrl(wxWindow *parent, PipedProcessCtrl *pp) : wxScintilla(parent, wxID_ANY)
{
    m_pp = pp;
    this->StyleSetForeground(PP_ERROR_STYLE,wxColor(200,0,0));
    this->StyleSetForeground(PP_LINK_STYLE,wxColor(0,0,200));
    this->StyleSetUnderline(PP_LINK_STYLE,true);
}


void PipedTextCtrl::OnDClick(wxMouseEvent &e)
{
    m_pp->OnDClick(e);
}


void PipedTextCtrl::OnUserInput(wxKeyEvent &e)
{
    m_pp->OnUserInput(e);
}


BEGIN_EVENT_TABLE(PipedProcessCtrl, wxPanel)
    EVT_CHAR(PipedProcessCtrl::OnUserInput)
    EVT_END_PROCESS(ID_PROC, PipedProcessCtrl::OnEndProcess)
    EVT_SIZE    (PipedProcessCtrl::OnSize)
END_EVENT_TABLE()

IMPLEMENT_DYNAMIC_CLASS(PipedProcessCtrl, wxPanel)


PipedProcessCtrl::PipedProcessCtrl(wxWindow* parent, int id, const wxString &name, ShellManager *shellmgr) : ShellCtrlBase(parent, id, name, shellmgr)
{
    m_shellmgr=shellmgr;
    m_name=name;
    m_dead=true;
    m_proc=NULL;
    m_killlevel=0;
    m_textctrl=new PipedTextCtrl(this,this);//(this, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_RICH|wxTE_MULTILINE|wxTE_READONLY|wxTE_PROCESS_ENTER|wxEXPAND);
    wxBoxSizer* bs = new wxBoxSizer(wxVERTICAL);
    bs->Add(m_textctrl, 1, wxEXPAND | wxALL);
    SetAutoLayout(TRUE);
    SetSizer(bs);
}


void PipedProcessCtrl::OnEndProcess(wxProcessEvent &event)
{
    m_exitcode=event.GetExitCode();
    SyncOutput(-1); //read any left over output TODO: while loop to handle extremely large amount of output
    m_dead=true;
    delete m_proc;
    m_proc=NULL;
    m_killlevel=0;
    if(m_shellmgr)
        m_shellmgr->OnShellTerminate(this);
}

long PipedProcessCtrl::LaunchProcess(const wxString &processcmd, const wxArrayString &options) // bool ParseLinks, bool LinkClicks, const wxString &LinkRegex
{
    if(!m_dead)
        return -1;
    if(m_proc) //this should never happen
        m_proc->Detach(); //self cleanup
    m_proc=new wxProcess(this,ID_PROC);
    m_proc->Redirect();
    m_procid=wxExecute(processcmd,wxEXEC_ASYNC,m_proc);
    m_parselinks=true;//ParseLinks;
    m_linkclicks=true;//LinkClicks;
    m_linkregex=LinkRegexDefault; //LinkRegex;
    if(m_procid>0)
    {
        m_ostream=m_proc->GetOutputStream();
        m_istream=m_proc->GetInputStream();
        m_estream=m_proc->GetErrorStream();
        m_dead=false;
        m_killlevel=0;
    }
    return m_procid;
}

void PipedProcessCtrl::KillProcess()
{
    if(m_dead)
        return;
//    if(m_killlevel==0) //some process will complete if we send EOF. TODO: make sending EOF a separate option
//    {
//        m_proc->CloseOutput();
//        m_killlevel=1;
//        return;
//    }
    long pid=GetPid();
#ifdef __WXGTK__
    if(m_killlevel==0)
    {
        m_killlevel=1;
        if(wxProcess::Exists(pid))
            wxProcess::Kill(pid,wxSIGTERM);
        return;
    }
    if(m_killlevel==1)
    {
        if(wxProcess::Exists(pid))
        {
            wxProcess::Kill(pid,wxSIGKILL);
        }
    }
#else
    if(wxProcess::Exists(pid))
    {
        wxProcess::Kill(pid,wxSIGKILL);
    }
#endif //__WXGTK__
}

wxString PipedProcessCtrl::LinkRegexDefault=
_T("[\"']?([^'\",\\s:;*?]+?)[\"']?[\\s]*(\\:|\\(|\\[|\\,?\\s*[Ll]ine)?\\s*(\\d*)");

void PipedProcessCtrl::SyncOutput(int maxchars)
{
    if(!m_proc)
        return;
    bool oneshot=true;
    if(maxchars<=0)
    {
        maxchars=20000;
        oneshot=false;
    }
    int lineno=m_textctrl->GetLineCount()-1;
    while(m_proc->IsInputAvailable())
    {
        char buf0[maxchars+1];
        for(int i=0;i<maxchars+1;i++)
            buf0[i]=0;
        m_istream->Read(buf0,maxchars);
        wxString m_latest=wxString::FromAscii(buf0);
        int start,end;
        m_textctrl->GetSelection(&start,&end);
        int pos=start>end?start:end;
        bool move_caret=(pos==m_textctrl->PositionAfter(pos))||(start!=end);
        m_textctrl->AppendText(m_latest);
        if(move_caret)
            m_textctrl->SetSelection(-1,-1);
        if(oneshot)
            break;
    }
    if(m_proc->IsErrorAvailable())
    {
        while(m_proc->IsErrorAvailable())
        {
            char buf0[maxchars+1];
            for(int i=0;i<maxchars+1;i++)
                buf0[i]=0;
            m_estream->Read(buf0,maxchars);
            wxString m_latest=wxString::FromAscii(buf0);
            int start,end;
            m_textctrl->GetSelection(&start,&end);
            int pos=start>end?start:end;
            bool move_caret=(pos==m_textctrl->PositionAfter(pos))||(start!=end);
            int style_start=m_textctrl->PositionFromLine(m_textctrl->GetLineCount());
            m_textctrl->AppendText(m_latest);
            if(move_caret)
                m_textctrl->SetSelection(-1,-1);

            m_textctrl->StartStyling(style_start,0x1F);
            m_textctrl->SetStyling(m_textctrl->PositionFromLine(m_textctrl->GetLineCount())-style_start,PP_ERROR_STYLE);
            if(oneshot)
                break;
        }
    }
    if(m_parselinks)
        ParseLinks(lineno,m_textctrl->GetLineCount());
}

void PipedProcessCtrl::ParseLinks(int lineno, int lastline)
{
    wxRegEx re(m_linkregex,wxRE_ADVANCED|wxRE_NEWLINE);
    while(lineno<lastline)
    {
        int col=0;
        wxString text=m_textctrl->GetLine(lineno);
        wxString file;
        while(re.Matches(text))
        {
            size_t start,len;
            if(re.GetMatch(&start,&len,0))
            {
                size_t fstart, flen;
                if(re.GetMatch(&fstart,&flen,1))
                    file=text.Mid(fstart,flen);
                wxFileName f(file);
                if(f.FileExists())
                {
                    int pos=m_textctrl->PositionFromLine(lineno)+col+start;
                    m_textctrl->StartStyling(pos,0x1F);
                    m_textctrl->SetStyling(len,PP_LINK_STYLE);

                }
            }
            col+=start+len;
            text=text.Mid(start+len);
        }
        lineno++;
    }
}

void PipedProcessCtrl::OnSize(wxSizeEvent& event)
{
    m_textctrl->SetSize(event.GetSize());
}


void PipedProcessCtrl::OnUserInput(wxKeyEvent& ke)
{
    if(m_dead)
    {
        ke.Skip();
        return;
    }
    //todo: if user presses navigational keys accept them as navigational (also copy/paste shortcuts?)
    char* kc1=new char[2];
    kc1[0]=ke.GetKeyCode()%256;
    kc1[1]=0;
    if(kc1[0]=='\r')
    {
        //cbMessageBox(_T("converting keystroke"));
        kc1[0]='\n';
    }
//    cbMessageBox(_T("key press: ")+wxString::FromAscii(kc1)+wxString::Format(_T(" keycode: %i"),ke.GetKeyCode()));
    wxChar kc2=ke.GetUnicodeKey();
    wxString buf(kc2);
    //kc1[0]=buf[0];
    m_ostream->Write(kc1,1);
//    m_proc->GetOutputStream()->Write(kc1,1);
//    cbMessageBox(_T("bytes written: ")+wxString::Format(_T("code: %u"),m_ostream->LastWrite()));
    m_textctrl->AppendText(kc2);
//    m_textctrl->SetInsertionPointEnd();
}



/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// HyperLinking ////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////



void PipedProcessCtrl::OnDClick(wxMouseEvent &e)
{
//    if(!m_linkclicks)
//        return;
//    wxTextCoord x,y;
//    m_textctrl->HitTest(e.GetPosition(),&x,&y);
//    wxRegEx re(m_linkregex,wxRE_ADVANCED|wxRE_NEWLINE);
//    wxString text=m_textctrl->GetLineText(y);
//    wxString file;
//    long line;
//    while(1)
//    {
//        if(!re.Matches(text))
//            return;
//        size_t start,len;
//        re.GetMatch(&start,&len,0);
//        if(static_cast<size_t>(x)<start)
//            return;
//        if(static_cast<size_t>(x)>start+len)
//        {
//            text=text.Mid(start+len);
//            x-=start+len;
//            continue;
//        }
//        if(re.GetMatch(&start,&len,1))
//            file=text.Mid(start,len);
//        else
//            file=wxEmptyString;
//        if(re.GetMatch(&start,&len,3))
//            text.Mid(start,len).ToLong(&line);
//        else
//            line=0;
//        break;
////        re.GetMatch(&start,&len,0);
////        cbMessageBox(wxString::Format(_T("match '%s'\nfile '%s'\nline '%i'"), text.Mid(start,len).c_str(), file.c_str(), line));
//    }
//    wxFileName f(file);
//    if(f.FileExists())
//    {
//        cbEditor* ed = Manager::Get()->GetEditorManager()->Open(f.GetFullPath());
//        if (ed)
//        {
//            ed->Show(true);
////            if (!ed->GetProjectFile())
////                ed->SetProjectFile(f.GetFullPath());
//            ed->GotoLine(line - 1, false);
//            if(line>0)
//                if(!ed->HasBookmark(line - 1))
//                    ed->ToggleBookmark(line -1);
//        }
//    }
//
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
