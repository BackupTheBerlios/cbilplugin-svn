#include <sdk.h> // Code::Blocks SDK
#include <configurationpanel.h>
#include "EditorTweaks.h"


#include <manager.h>
#include <configmanager.h>
#include <logmanager.h>
#include <editor_hooks.h>
#include <cbeditor.h>
#include <wx/wxscintilla.h>
#include <editormanager.h>
#include "cbstyledtextctrl.h"


// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.
namespace
{
    PluginRegistrant<EditorTweaks> reg(_T("EditorTweaks"));
}

int id_et= wxNewId();
int id_et_WordWrap= wxNewId();
int id_et_ShowLineNumbers= wxNewId();
int id_et_TabChar = wxNewId();
int id_et_TabIndent = wxNewId();
int id_et_TabSize2 = wxNewId();
int id_et_TabSize4 = wxNewId();
int id_et_TabSize6 = wxNewId();
int id_et_TabSize8 = wxNewId();
int id_et_ShowEOL = wxNewId();
int id_et_StripTrailingBlanks = wxNewId();
int id_et_EnsureConsistentEOL = wxNewId();
int id_et_EOLCRLF = wxNewId();
int id_et_EOLCR = wxNewId();
int id_et_EOLLF = wxNewId();



// events handling
BEGIN_EVENT_TABLE(EditorTweaks, cbPlugin)
    EVT_MENU(id_et_WordWrap, EditorTweaks::OnWordWrap)
    EVT_MENU(id_et_ShowLineNumbers, EditorTweaks::OnShowLineNumbers)
    EVT_MENU(id_et_TabChar, EditorTweaks::OnTabChar)
    EVT_MENU(id_et_TabIndent, EditorTweaks::OnTabIndent)
    EVT_MENU(id_et_TabSize2, EditorTweaks::OnTabSize2)
    EVT_MENU(id_et_TabSize4, EditorTweaks::OnTabSize4)
    EVT_MENU(id_et_TabSize6, EditorTweaks::OnTabSize6)
    EVT_MENU(id_et_TabSize8, EditorTweaks::OnTabSize8)
    EVT_MENU(id_et_ShowEOL, EditorTweaks::OnShowEOL)
    EVT_MENU(id_et_StripTrailingBlanks, EditorTweaks::OnStripTrailingBlanks)
    EVT_MENU(id_et_EnsureConsistentEOL, EditorTweaks::OnEnsureConsistentEOL)
    EVT_MENU(id_et_EOLCRLF, EditorTweaks::OnEOLCRLF)
    EVT_MENU(id_et_EOLCR, EditorTweaks::OnEOLCR)
    EVT_MENU(id_et_EOLLF, EditorTweaks::OnEOLLF)
END_EVENT_TABLE()

// constructor
EditorTweaks::EditorTweaks()
{
    // Make sure our resources are available.
    // In the generated boilerplate code we have no resources but when
    // we add some, it will be nice that this code is in place already ;)
    if(!Manager::LoadResource(_T("EditorTweaks.zip")))
    {
        NotifyMissingFile(_T("EditorTweaks.zip"));
    }
}

// destructor
EditorTweaks::~EditorTweaks()
{
}

void EditorTweaks::OnAttach()
{
    // do whatever initialization you need for your plugin
    // NOTE: after this function, the inherited member variable
    // m_IsAttached will be TRUE...
    // You should check for it in other functions, because if it
    // is FALSE, it means that the application did *not* "load"
    // (see: does not need) this plugin...

//    EditorHooks::HookFunctorBase* myhook = new EditorHooks::HookFunctor<EditorTweaks>(this, &EditorTweaks::EditorEventHook);
//    m_EditorHookId = EditorHooks::RegisterHook(myhook);
    Manager* pm = Manager::Get();
    pm->RegisterEventSink(cbEVT_EDITOR_OPEN, new cbEventFunctor<EditorTweaks, CodeBlocksEvent>(this, &EditorTweaks::OnEditorOpen));
    pm->RegisterEventSink(cbEVT_EDITOR_CLOSE, new cbEventFunctor<EditorTweaks, CodeBlocksEvent>(this, &EditorTweaks::OnEditorClose));
//    pm->RegisterEventSink(cbEVT_EDITOR_UPDATE_UI, new cbEventFunctor<EditorTweaks, CodeBlocksEvent>(this, &EditorTweaks::OnEditorUpdateUI));
    pm->RegisterEventSink(cbEVT_EDITOR_ACTIVATED, new cbEventFunctor<EditorTweaks, CodeBlocksEvent>(this, &EditorTweaks::OnEditorActivate));
    pm->RegisterEventSink(cbEVT_EDITOR_DEACTIVATED, new cbEventFunctor<EditorTweaks, CodeBlocksEvent>(this, &EditorTweaks::OnEditorDeactivate));

    m_tweakmenu=NULL;

    EditorManager* em = Manager::Get()->GetEditorManager();
    for(int i=0;i<em->GetEditorsCount();i++)
    {
        cbEditor* ed=em->GetBuiltinEditor(i);
        if(ed && ed->GetControl())
        {
            ed->GetControl()->SetOvertype(false);
            ed->GetControl()->Connect(wxEVT_KEY_DOWN,(wxObjectEventFunction) (wxEventFunction) (wxCharEventFunction)&EditorTweaks::OnKeyPress,NULL,this);
        }
    }

}

void EditorTweaks::OnRelease(bool appShutDown)
{
    m_tweakmenu=NULL;

    EditorHooks::UnregisterHook(m_EditorHookId, true);
    EditorManager* em = Manager::Get()->GetEditorManager();
    for(int i=0;i<em->GetEditorsCount();i++)
    {
        cbEditor* ed=em->GetBuiltinEditor(i);
        if(ed && ed->GetControl())
            ed->GetControl()->Disconnect(wxEVT_NULL,(wxObjectEventFunction) (wxEventFunction) (wxCharEventFunction)&EditorTweaks::OnKeyPress);
    }
}

void EditorTweaks::BuildMenu(wxMenuBar* menuBar)
{
    Manager::Get()->GetLogManager()->DebugLog(_("Editor Tweaks plugin: Building menu"));
    int i=menuBar->FindMenu(_T("Edit"));
    if(i==wxNOT_FOUND)
    {
        Manager::Get()->GetLogManager()->DebugLog(_("Editor Tweaks plugin: edit menu not found"));
        return;
    }
    wxMenu *menu=menuBar->GetMenu(i);
    for(i=0;i<menu->GetMenuItemCount();i++)
    {
        wxMenuItem *mm=menu->FindItemByPosition(i);
        if(mm->GetLabel()==_("End-of-line mode"))
            menu->Remove(mm);
        if(mm->GetLabel()==_("Special commands"))
            break;
    }
    if(i==menu->GetMenuItemCount())
    {
        Manager::Get()->GetLogManager()->DebugLog(_("Editor Tweaks plugin: Special commands menu not found"));
        return;
    }
    Manager::Get()->GetLogManager()->DebugLog(wxString::Format(_("Editor Tweaks plugin: making the menu %i"),i));
    m_tweakmenu=new wxMenu();
    m_tweakmenuitem=new wxMenuItem(menu, id_et, _("Editor Tweaks"), _("Tweak the settings of the active editor"), wxITEM_NORMAL, m_tweakmenu);
    menu->Insert(i+1,m_tweakmenuitem);

    wxMenu *submenu=m_tweakmenu; //_("Editor Tweaks")

    submenu->AppendCheckItem( id_et_WordWrap, _( "Word wrap" ), _( "Wrap text" ) );
    submenu->AppendCheckItem( id_et_ShowLineNumbers, _( "Show Line Numbers" ), _( "Show Line Numbers" ) );
    submenu->AppendSeparator();
    submenu->AppendCheckItem( id_et_TabChar, _( "Use Tab Character" ), _( "Use Tab Character" ) );
    submenu->AppendCheckItem( id_et_TabIndent, _( "Tab Indents" ), _( "Tab Indents" ) );
    wxMenu *tabsizemenu=new wxMenu();
    tabsizemenu->AppendRadioItem( id_et_TabSize2, _( "2" ), _( "Tab Width of 2" ) );
    tabsizemenu->AppendRadioItem( id_et_TabSize4, _( "4" ), _( "Tab Width of 4" ) );
    tabsizemenu->AppendRadioItem( id_et_TabSize6, _( "6" ), _( "Tab Width of 6" ) );
    tabsizemenu->AppendRadioItem( id_et_TabSize8, _( "8" ), _( "Tab Width of 8" ) );
    submenu->Append(wxID_ANY,_("Tab Size"),tabsizemenu);
    submenu->AppendSeparator();
    wxMenu *eolmenu=new wxMenu();
    eolmenu->AppendRadioItem( id_et_EOLCRLF, _( "CR LF" ), _( "Carriage Return - Line Feed (Windows Default)" ) );
    eolmenu->AppendRadioItem( id_et_EOLCR, _( "CR" ), _( "Carriage Return (Mac Default)" ) );
    eolmenu->AppendRadioItem( id_et_EOLLF, _( "LF" ), _( "Line Feed (Unix Default)" ) );
    submenu->Append(wxID_ANY,_("End-of-Line Mode"),eolmenu);
    submenu->AppendCheckItem( id_et_ShowEOL, _( "Show EOL Chars" ), _( "Show End-of-Line Characters" ) );
    submenu->Append( id_et_StripTrailingBlanks, _( "Strip Trailing Blanks Now" ), _( "Strip trailing blanks from each line" ) );
    submenu->Append( id_et_EnsureConsistentEOL, _( "Make EOLs Consistent Now" ), _( "Convert End-of-Line Characters to the Active Setting" ) );

    UpdateUI();
}


int update_count=0;

void EditorTweaks::UpdateUI()
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    wxMenu *submenu=m_tweakmenu; //_("Editor Tweaks") TODO: Retrieve actual menu

    submenu->Check(id_et_WordWrap,ed->GetControl()->GetWrapMode()>0);
    submenu->Check(id_et_ShowLineNumbers,ed->GetControl()->GetMarginWidth(0)>0);
    submenu->Check(id_et_TabChar,ed->GetControl()->GetUseTabs());
    submenu->Check(id_et_TabIndent,ed->GetControl()->GetTabIndents());
    submenu->Check(id_et_TabSize2,ed->GetControl()->GetTabWidth()==2);
    submenu->Check(id_et_TabSize4,ed->GetControl()->GetTabWidth()==4);
    submenu->Check(id_et_TabSize6,ed->GetControl()->GetTabWidth()==6);
    submenu->Check(id_et_TabSize8,ed->GetControl()->GetTabWidth()==8);
    submenu->Check(id_et_EOLCRLF,ed->GetControl()->GetEOLMode()==wxSCI_EOL_CRLF);
    submenu->Check(id_et_EOLCR,ed->GetControl()->GetEOLMode()==wxSCI_EOL_CR);
    submenu->Check(id_et_EOLLF,ed->GetControl()->GetEOLMode()==wxSCI_EOL_LF);
    submenu->Check(id_et_ShowEOL,ed->GetControl()->GetViewEOL());
}

void EditorTweaks::OnEditorUpdateUI(CodeBlocksEvent& event)
{
    Manager::Get()->GetLogManager()->DebugLog(wxString::Format(_("Editor Update UI")));
    if(!m_IsAttached || !m_tweakmenu)
        return;    return;
    UpdateUI();
}


void EditorTweaks::OnEditorActivate(CodeBlocksEvent& event)
{
    Manager::Get()->GetLogManager()->DebugLog(wxString::Format(_("Editor Activate")));
    if(!m_IsAttached || !m_tweakmenu)
        return;
    m_tweakmenuitem->Enable(true);
    UpdateUI();
}

void EditorTweaks::OnEditorDeactivate(CodeBlocksEvent& event)
{
    Manager::Get()->GetLogManager()->DebugLog(wxString::Format(_("Editor Deactivate")));
    if(!m_IsAttached || !m_tweakmenu)
        return;
    m_tweakmenuitem->Enable(false);
}


void EditorTweaks::OnEditorOpen(CodeBlocksEvent& event)
{
    Manager::Get()->GetLogManager()->DebugLog(wxString::Format(_("Editor Open")));
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(ed && ed->GetControl())
    {
        ed->GetControl()->SetOvertype(false);
        ed->GetControl()->Connect(wxEVT_KEY_DOWN,(wxObjectEventFunction) (wxEventFunction) (wxCharEventFunction)&EditorTweaks::OnKeyPress,NULL,this);
    }
}


void EditorTweaks::OnEditorClose(CodeBlocksEvent& event)
{
    Manager::Get()->GetLogManager()->DebugLog(wxString::Format(_("Editor Close")));
    if(!m_IsAttached || !m_tweakmenu)
        return;
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(ed && ed->GetControl())
        return;
    m_tweakmenuitem->Enable(false);
}

void EditorTweaks::OnKeyPress(wxKeyEvent& event)
{
    if(event.GetKeyCode()==WXK_INSERT)
        event.Skip(false);
    else
        event.Skip(true);
}


//void EditorTweaks::EditorEventHook(cbEditor* editor, wxScintillaEvent& event)
//{
//}

void EditorTweaks::BuildModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data)
{
    //Some library module is ready to display a pop-up menu.
    //Check the parameter \"type\" and see which module it is
    //and append any items you need in the menu...
    //TIP: for consistency, add a separator as the first item...

    return; //DISABLED!!

    if(type!=mtEditorManager)
        return;
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    wxMenu *submenu=new wxMenu(); //_("Editor Tweaks")

    menu->Append(id_et,_("Editor Tweaks"),submenu);

    submenu->AppendCheckItem( id_et_WordWrap, _( "Word wrap" ), _( "Wrap text" ) );
    if(ed->GetControl()->GetWrapMode()>0)
        submenu->Check(id_et_WordWrap,true);

    submenu->AppendCheckItem( id_et_ShowLineNumbers, _( "Show Line Numbers" ), _( "Show Line Numbers" ) );
    if(ed->GetControl()->GetMarginWidth(0)>0)
        submenu->Check(id_et_ShowLineNumbers,true);

    submenu->AppendSeparator();

    submenu->AppendCheckItem( id_et_TabChar, _( "Use Tab Character" ), _( "Use Tab Character" ) );
    if(ed->GetControl()->GetUseTabs())
        submenu->Check(id_et_TabChar,true);

    submenu->AppendCheckItem( id_et_TabIndent, _( "Tab Indents" ), _( "Tab Indents" ) );
    if(ed->GetControl()->GetTabIndents())
        submenu->Check(id_et_TabIndent,true);

    wxMenu *tabsizemenu=new wxMenu();
    tabsizemenu->AppendRadioItem( id_et_TabSize2, _( "2" ), _( "Tab Width of 2" ) );
    if(ed->GetControl()->GetTabWidth()==2)
        tabsizemenu->Check(id_et_TabSize2,true);
    tabsizemenu->AppendRadioItem( id_et_TabSize4, _( "4" ), _( "Tab Width of 4" ) );
    if(ed->GetControl()->GetTabWidth()==4)
        tabsizemenu->Check(id_et_TabSize4,true);
    tabsizemenu->AppendRadioItem( id_et_TabSize6, _( "6" ), _( "Tab Width of 6" ) );
    if(ed->GetControl()->GetTabWidth()==6)
        tabsizemenu->Check(id_et_TabSize6,true);
    tabsizemenu->AppendRadioItem( id_et_TabSize8, _( "8" ), _( "Tab Width of 8" ) );
    if(ed->GetControl()->GetTabWidth()==8)
        tabsizemenu->Check(id_et_TabSize8,true);
    submenu->Append(wxID_ANY,_("Tab Size"),tabsizemenu);

    submenu->AppendSeparator();

    wxMenu *eolmenu=new wxMenu();
    eolmenu->AppendRadioItem( id_et_EOLCRLF, _( "CR LF" ), _( "Carriage Return - Line Feed (Windows Default)" ) );
    if(ed->GetControl()->GetEOLMode()==wxSCI_EOL_CRLF)
        eolmenu->Check(id_et_EOLCRLF,true);
    eolmenu->AppendRadioItem( id_et_EOLCR, _( "CR" ), _( "Carriage Return (Mac Default)" ) );
    if(ed->GetControl()->GetEOLMode()==wxSCI_EOL_CR)
        eolmenu->Check(id_et_EOLCR,true);
    eolmenu->AppendRadioItem( id_et_EOLLF, _( "LF" ), _( "Line Feed (Unix Default)" ) );
    if(ed->GetControl()->GetEOLMode()==wxSCI_EOL_LF)
        eolmenu->Check(id_et_EOLLF,true);
    submenu->Append(wxID_ANY,_("End-of-Line Mode"),eolmenu);

    submenu->AppendCheckItem( id_et_ShowEOL, _( "Show EOL Chars" ), _( "Show End-of-Line Characters" ) );
    if(ed->GetControl()->GetViewEOL())
        submenu->Check(id_et_ShowEOL,true);

    submenu->Append( id_et_StripTrailingBlanks, _( "Strip Trailing Blanks Now" ), _( "Strip trailing blanks from each line" ) );

    submenu->Append( id_et_EnsureConsistentEOL, _( "Make EOLs Consistent Now" ), _( "Convert End-of-Line Characters to the Active Setting" ) );


}

void EditorTweaks::OnWordWrap(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    bool enabled=ed->GetControl()->GetWrapMode()>0;

    if(enabled)
        ed->GetControl()->SetWrapMode(wxSCI_WRAP_NONE);
    else
        ed->GetControl()->SetWrapMode(wxSCI_WRAP_WORD);

}

void EditorTweaks::OnShowLineNumbers(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    bool enabled=ed->GetControl()->GetMarginWidth(0)>0;

//    bool old_state=mgr->ReadBool(_T("/show_line_numbers"), true);
//    mgr->Write(_T("/show_line_numbers"), !enabled);
//    ed->SetEditorStyleAfterFileOpen();
//    mgr->Write(_T("/show_line_numbers"), old_state);

    if(enabled)
    {
        ed->GetControl()->SetMarginWidth(0, 0);
    }
    else
    {
        ConfigManager* cfg = Manager::Get()->GetConfigManager(_T("editor"));
        int pixelWidth = ed->GetControl()->TextWidth(wxSCI_STYLE_LINENUMBER, _T("9"));

        if(cfg->ReadBool(_T("/margin/dynamic_width"), false))
        {
            int lineNumWidth = 1;
            int lineCount = ed->GetControl()->GetLineCount();

            while (lineCount >= 10)
            {
                lineCount /= 10;
                ++lineNumWidth;
            }

            ed->GetControl()->SetMarginWidth(0, 6 + lineNumWidth * pixelWidth);
        }
        else
        {
            ed->GetControl()->SetMarginWidth(0, 6 + cfg->ReadInt(_T("/margin/width_chars"), 6) * pixelWidth);
        }
    }
}

void EditorTweaks::OnTabChar(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    ed->GetControl()->SetUseTabs(!ed->GetControl()->GetUseTabs());
}

void EditorTweaks::OnTabIndent(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    ed->GetControl()->SetTabIndents(!ed->GetControl()->GetTabIndents());
}

void EditorTweaks::OnTabSize2(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    ed->GetControl()->SetTabWidth(2);
}

void EditorTweaks::OnTabSize4(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    ed->GetControl()->SetTabWidth(4);
}

void EditorTweaks::OnTabSize6(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    ed->GetControl()->SetTabWidth(6);
}

void EditorTweaks::OnTabSize8(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    ed->GetControl()->SetTabWidth(8);
}

void EditorTweaks::OnShowEOL(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    ed->GetControl()->SetViewEOL(!ed->GetControl()->GetViewEOL());
}

void EditorTweaks::OnStripTrailingBlanks(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    wxMessageBox(_("Not Implemented"));
}

void EditorTweaks::OnEnsureConsistentEOL(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    ed->GetControl()->ConvertEOLs(ed->GetControl()->GetEOLMode());
}

void EditorTweaks::OnEOLCRLF(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    ed->GetControl()->SetEOLMode(wxSCI_EOL_CRLF);
}

void EditorTweaks::OnEOLCR(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    ed->GetControl()->SetEOLMode(wxSCI_EOL_CR);
}

void EditorTweaks::OnEOLLF(wxCommandEvent &event)
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if(!ed || !ed->GetControl())
        return;

    ed->GetControl()->SetEOLMode(wxSCI_EOL_LF);
}

