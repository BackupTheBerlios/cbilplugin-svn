#include "PowerShell.h"
#include "se_globals.h"

// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.
namespace
{
    PluginRegistrant<PowerShell> reg(_T("PowerShell"));
}

int ID_UpdateUI=wxNewId();
int ID_LangMenu_Settings=wxNewId();
int ID_LangMenu_RunPiped=wxNewId();
int ID_LangMenu_ShowConsole=wxNewId();
int ID_PipedProcess=wxNewId();

int ID_LaunchPythonProcess=wxNewId();

// Ugly ugly hack to handle dynamic menus
int ID_ContextMenu_0=wxNewId();
int ID_ContextMenu_1=wxNewId();
int ID_ContextMenu_2=wxNewId();
int ID_ContextMenu_3=wxNewId();
int ID_ContextMenu_4=wxNewId();
int ID_ContextMenu_5=wxNewId();
int ID_ContextMenu_6=wxNewId();
int ID_ContextMenu_7=wxNewId();
int ID_ContextMenu_8=wxNewId();
int ID_ContextMenu_9=wxNewId();
int ID_ContextMenu_10=wxNewId();
int ID_ContextMenu_11=wxNewId();
int ID_ContextMenu_12=wxNewId();
int ID_ContextMenu_13=wxNewId();
int ID_ContextMenu_14=wxNewId();
int ID_ContextMenu_15=wxNewId();
int ID_ContextMenu_16=wxNewId();
int ID_ContextMenu_17=wxNewId();
int ID_ContextMenu_18=wxNewId();
int ID_ContextMenu_19=wxNewId();
int ID_ContextMenu_20=wxNewId();
int ID_ContextMenu_21=wxNewId();
int ID_ContextMenu_22=wxNewId();
int ID_ContextMenu_23=wxNewId();
int ID_ContextMenu_24=wxNewId();
int ID_ContextMenu_25=wxNewId();
int ID_ContextMenu_26=wxNewId();
int ID_ContextMenu_27=wxNewId();
int ID_ContextMenu_28=wxNewId();
int ID_ContextMenu_29=wxNewId();
int ID_ContextMenu_30=wxNewId();
int ID_ContextMenu_31=wxNewId();
int ID_ContextMenu_32=wxNewId();
int ID_ContextMenu_33=wxNewId();
int ID_ContextMenu_34=wxNewId();
int ID_ContextMenu_35=wxNewId();
int ID_ContextMenu_36=wxNewId();
int ID_ContextMenu_37=wxNewId();
int ID_ContextMenu_38=wxNewId();
int ID_ContextMenu_39=wxNewId();
int ID_ContextMenu_40=wxNewId();
int ID_ContextMenu_41=wxNewId();
int ID_ContextMenu_42=wxNewId();
int ID_ContextMenu_43=wxNewId();
int ID_ContextMenu_44=wxNewId();
int ID_ContextMenu_45=wxNewId();
int ID_ContextMenu_46=wxNewId();
int ID_ContextMenu_47=wxNewId();
int ID_ContextMenu_48=wxNewId();
int ID_ContextMenu_49=wxNewId();

int ID_SubMenu_0=wxNewId();
int ID_SubMenu_1=wxNewId();
int ID_SubMenu_2=wxNewId();
int ID_SubMenu_3=wxNewId();
int ID_SubMenu_4=wxNewId();
int ID_SubMenu_5=wxNewId();
int ID_SubMenu_6=wxNewId();
int ID_SubMenu_7=wxNewId();
int ID_SubMenu_8=wxNewId();
int ID_SubMenu_9=wxNewId();
int ID_SubMenu_10=wxNewId();
int ID_SubMenu_11=wxNewId();
int ID_SubMenu_12=wxNewId();
int ID_SubMenu_13=wxNewId();
int ID_SubMenu_14=wxNewId();
int ID_SubMenu_15=wxNewId();
int ID_SubMenu_16=wxNewId();
int ID_SubMenu_17=wxNewId();
int ID_SubMenu_18=wxNewId();
int ID_SubMenu_19=wxNewId();
int ID_SubMenu_20=wxNewId();
int ID_SubMenu_21=wxNewId();
int ID_SubMenu_22=wxNewId();
int ID_SubMenu_23=wxNewId();
int ID_SubMenu_24=wxNewId();
int ID_SubMenu_25=wxNewId();
int ID_SubMenu_26=wxNewId();
int ID_SubMenu_27=wxNewId();
int ID_SubMenu_28=wxNewId();
int ID_SubMenu_29=wxNewId();
int ID_SubMenu_30=wxNewId();
int ID_SubMenu_31=wxNewId();
int ID_SubMenu_32=wxNewId();
int ID_SubMenu_33=wxNewId();
int ID_SubMenu_34=wxNewId();
int ID_SubMenu_35=wxNewId();
int ID_SubMenu_36=wxNewId();
int ID_SubMenu_37=wxNewId();
int ID_SubMenu_38=wxNewId();
int ID_SubMenu_39=wxNewId();
int ID_SubMenu_40=wxNewId();
int ID_SubMenu_41=wxNewId();
int ID_SubMenu_42=wxNewId();
int ID_SubMenu_43=wxNewId();
int ID_SubMenu_44=wxNewId();
int ID_SubMenu_45=wxNewId();
int ID_SubMenu_46=wxNewId();
int ID_SubMenu_47=wxNewId();
int ID_SubMenu_48=wxNewId();
int ID_SubMenu_49=wxNewId();

// events handling
BEGIN_EVENT_TABLE(PowerShell, cbPlugin)
    EVT_MENU_RANGE(ID_ContextMenu_0,ID_ContextMenu_49,PowerShell::OnRunTarget)
    EVT_MENU_RANGE(ID_SubMenu_0, ID_SubMenu_49, PowerShell::OnRunTarget)
    EVT_MENU(ID_LangMenu_ShowConsole,PowerShell::OnShowConsole)
//    EVT_MENU(ID_LaunchPythonProcess,PowerShell::OnLaunchPythonProcess)
    EVT_UPDATE_UI(ID_LangMenu_ShowConsole, PowerShell::OnUpdateUI)
END_EVENT_TABLE()


void PowerShell::OnUpdateUI(wxUpdateUIEvent& event)
{
    m_LangMenu->Check(ID_LangMenu_ShowConsole,IsWindowReallyShown(m_shellmgr));
    // allow other UpdateUI handlers to process this event
    // *very* important! don't forget it...
    event.Skip();
}


void PowerShell::OnShowConsole(wxCommandEvent& event)
{
    // This toggles display of the console I/O window
    CodeBlocksDockEvent evt(event.IsChecked() ? cbEVT_SHOW_DOCK_WINDOW : cbEVT_HIDE_DOCK_WINDOW);
    evt.pWindow = m_shellmgr;
    Manager::Get()->ProcessEvent(evt);
}

void PowerShell::ShowConsole()
{
    // This shows the console I/O window
    CodeBlocksDockEvent evt(cbEVT_SHOW_DOCK_WINDOW);
    evt.pWindow = m_shellmgr;
    Manager::Get()->ProcessEvent(evt);
}

void PowerShell::HideConsole()
{
    // This hides display of the console I/O window
    CodeBlocksDockEvent evt(cbEVT_HIDE_DOCK_WINDOW);
    evt.pWindow = m_shellmgr;
    Manager::Get()->ProcessEvent(evt);
}

void PowerShell::OnSettings(wxCommandEvent& event)
{
    cbMessageBox(_T("Settings..."));
}

void PowerShell::OnSubMenuSelect(wxUpdateUIEvent& event)
{
//    int num=event.GetId()-ID_Menu_0;
//    if(num>=0 && num<=9)
//    {
//        m_interpnum=num;
//        if(num==1)
//        {
//        wxString a;
//        a<<_T("Sub menu")<<m_interpnum<<_T(" opened");
//        cbMessageBox(a);
//        }
//    }
}

void PowerShell::OnSetTarget(wxCommandEvent& event)
{
    wxString wild(m_wildcard);
    if(wild==_T(""))
#ifdef __WXMSW__
        wild=_T("*.*");
#else
        wild=_T("*");
#endif
    wxFileDialog *fd=new wxFileDialog(NULL,_T("Choose the Command Target"),_T(""),_T(""),wild,wxOPEN|wxFILE_MUST_EXIST);
    if(fd->ShowModal()==wxID_OK)
    {
        m_RunTarget=fd->GetPath();
    } else
        m_RunTarget=_T("");
    delete fd;
}

void PowerShell::OnSetMultiTarget(wxCommandEvent& event)
{
    wxString wild(m_wildcard);
    if(wild==_T(""))
#ifdef __WXMSW__
        wild=_T("*.*");
#else
        wild=_T("*");
#endif
    wxFileDialog *fd=new wxFileDialog(NULL,_T("Choose the Command Targets"),_T(""),_T(""),wild,wxOPEN|wxFILE_MUST_EXIST|wxMULTIPLE);
    if(fd->ShowModal()==wxID_OK)
    {
        wxArrayString paths;
        fd->GetPaths(paths);
        m_RunTarget=paths[0];
        for(size_t i=1;i<paths.GetCount();i++)
            m_RunTarget+=_T(" ")+paths[i];
    } else
        m_RunTarget=_T("");
    delete fd;
}


void PowerShell::OnSetDirTarget(wxCommandEvent& event)
{
    wxDirDialog *dd=new wxDirDialog(NULL,_T("Choose the Target Directory"),_T(""));
    if(dd->ShowModal()==wxID_OK)
    {
        m_RunTarget=dd->GetPath();
    } else
        m_RunTarget=_T("");
    delete dd;
}

void PowerShell::OnRunTarget(wxCommandEvent& event)
{
    int ID=event.GetId();
    wxString commandstr;
    wxString consolename;
    wxString workingdir;
    bool windowed=false;
    bool console=false;
    if(ID>=ID_ContextMenu_0&&ID<=ID_ContextMenu_49)
    {
        m_interpnum=m_contextvec[ID-ID_ContextMenu_0];
        commandstr=m_ic.interps[m_interpnum].command;
        consolename=m_ic.interps[m_interpnum].name;
        windowed=(m_ic.interps[m_interpnum].mode==_("W"));
        console=(m_ic.interps[m_interpnum].mode==_("C"));
        workingdir=m_ic.interps[m_interpnum].wdir;
    } else
    if(ID>=ID_SubMenu_0&&ID<=ID_SubMenu_49)
    {
        m_interpnum=ID-ID_SubMenu_0;
        commandstr=m_ic.interps[m_interpnum].command;
        consolename=m_ic.interps[m_interpnum].name;
        windowed=(m_ic.interps[m_interpnum].mode==_("W"));
        console=(m_ic.interps[m_interpnum].mode==_("C"));
        workingdir=m_ic.interps[m_interpnum].wdir;
        m_wildcard=m_ic.interps[m_interpnum].wildcards;
        if(m_ic.interps[m_interpnum].command.Find(_T("$file"))>0 ||
            m_ic.interps[m_interpnum].command.Find(_T("$path"))>0)
        {
            OnSetTarget(event);
            if(!wxFileName::FileExists(m_RunTarget))
            {
                LogMessage(_("PowerShell: ")+m_RunTarget+_(" not found"));
                return;
            }
        }
        if(m_ic.interps[m_interpnum].command.Find(_T("$dir"))>0)
        {
            OnSetDirTarget(event);
            if(!wxFileName::DirExists(m_RunTarget))
            {
                LogMessage(_("Shell Extensions: ")+m_RunTarget+_(" not found"));
                return;
            }
            if(m_RunTarget==_T(""))
                return;
        }
        if(m_ic.interps[m_interpnum].command.Find(_T("$mpaths"))>0)
        {
            OnSetMultiTarget(event);
            if(m_RunTarget==_T(""))
                return;
        }
    }
    else
    {
        LogMessage(wxString::Format(_T("WARNING: Unprocessed ShellCommand Menu Message: ID %i, IDbase %i, IDend %i, num items on menu %i"),ID,ID_ContextMenu_0,ID_ContextMenu_49,(int)m_contextvec.size()));
        return;
    }

    m_RunTarget.Replace(_T("*"),_T(" "));

    bool setdir=true;
    commandstr.Replace(_T("$file"),wxFileName(m_RunTarget).GetShortPath());
    commandstr.Replace(_T("$relfile"),wxFileName(m_RunTarget).GetFullName());
    commandstr.Replace(_T("$fname"),wxFileName(m_RunTarget).GetName());
    commandstr.Replace(_T("$fext"),wxFileName(m_RunTarget).GetExt());
    commandstr.Replace(_T("$dir"),wxFileName(m_RunTarget).GetShortPath());
    commandstr.Replace(_T("$reldir"),wxFileName(m_RunTarget).GetFullName());
    commandstr.Replace(_T("$path"),wxFileName(m_RunTarget).GetShortPath());
    commandstr.Replace(_T("$relpath"),wxFileName(m_RunTarget).GetFullPath());
    if(commandstr.Replace(_T("$mpaths"),m_RunTarget)>0)
        setdir=false;

    // substitute user prompted values in the format: $inputstr{Enter your message}
    int promptind=commandstr.Find(_T("$inputstr{"));
    wxString substitution;
    while(promptind>=0)
    {
        int promptend=commandstr.Mid(promptind+10).Find(_T("}"));
        if(promptend<=0)
        {
            cbMessageBox(_T("Malformed $inputstr in command line -- no closing '}' found: ")+commandstr);
            return;
        }
        else
            promptend++;
        wxTextEntryDialog ted(NULL,commandstr.Mid(promptind+10,promptend-1),consolename,_T(""),wxOK|wxCANCEL);
        if(ted.ShowModal()==wxID_OK)
            substitution=ted.GetValue();
        else
            return;
        commandstr=commandstr.Left(promptind)+substitution+commandstr.Mid(promptind+10+promptend);
        int nextind=commandstr.Mid(promptind+substitution.Len()).Find(_T("$inputstr"));
        if(nextind>=0)
            promptind+=nextind+substitution.Len();
        else
            promptind=-1;
    }

    commandstr.Replace(_T("$interpreter"),wxFileName(m_ic.interps[m_interpnum].command).GetShortPath());
    workingdir.Replace(_T("$parentdir"),wxFileName(m_RunTarget).GetPath());
    if(wxFileName::DirExists(m_RunTarget))
        workingdir.Replace(_T("$dir"),wxFileName(m_RunTarget).GetFullPath());

    if(Manager::Get()->GetMacrosManager())
    {
        Manager::Get()->GetMacrosManager()->RecalcVars(0, 0, 0); // hack to force-update macros
        Manager::Get()->GetMacrosManager()->ReplaceMacros(commandstr);
        Manager::Get()->GetMacrosManager()->ReplaceMacros(workingdir);
    }
    wxString olddir=wxGetCwd();
    if(setdir && workingdir!=_T(""))
    {
        if(!wxSetWorkingDirectory(workingdir))
        {
            LogMessage(_T("Shell Extensions Plugin: Can't change to working directory to ")+workingdir);
            return;
        }
    }

    LogMessage(wxString::Format(_("Launching '%s': %s (in %s)"), consolename.c_str(), commandstr.c_str(), workingdir.c_str()));

    if(windowed)
    {
        wxArrayString astr;
        m_shellmgr->LaunchProcess(commandstr,consolename,_T("Piped Process Control"),astr);
        ShowConsole();
    } else if (console)
    {
        wxString cmdline;
#ifndef __WXMSW__
        // for non-win platforms, use m_ConsoleTerm to run the console app
        wxString term = Manager::Get()->GetConfigManager(_T("app"))->Read(_T("/console_terminal"), DEFAULT_CONSOLE_TERM);
        term.Replace(_T("$TITLE"), _T("'") + consolename + _T("'"));
        cmdline<< term << _T(" ");
        #define CONSOLE_RUNNER "cb_console_runner"
#else
        #define CONSOLE_RUNNER "cb_console_runner.exe"
#endif
        wxString baseDir = ConfigManager::GetExecutableFolder();
        if (wxFileExists(baseDir + wxT("/" CONSOLE_RUNNER)))
            cmdline << baseDir << wxT("/" CONSOLE_RUNNER " ");
        cmdline<<commandstr;

        if(!wxExecute(cmdline))
            cbMessageBox(_T("Command Launch Failed: ")+commandstr);
    } else
    {
        if(!wxExecute(commandstr))
            cbMessageBox(_T("Command Launch Failed: ")+commandstr);
    }
    wxSetWorkingDirectory(olddir);
}

void PowerShell::OnLaunchPythonProcess(wxCommandEvent &event)
{
    wxString olddir=wxGetCwd();
    wxArrayString astr;
    m_shellmgr->LaunchProcess(_T(""),_T("Python"),_T("Python Interpreter"),astr);
    wxSetWorkingDirectory(olddir);
}


// DEPRECATED - NO LONGER REQUIRED
void PowerShell::OnRun(wxCommandEvent& event)
{
//    int ID=event.GetId();
//    wxString commandstr;
//    wxString consolename;
//    wxMenu *m=m_LangMenu->FindItem(ID)->GetMenu(); // get pointer object to selected item in submenu
//    for(m_interpnum=0;m_interpnum<m_ic.interps.size()&&m_interpnum<10;m_interpnum++)
//    {
//        if(m_LangMenu->FindItem(ID_Menu_0+m_interpnum)->GetSubMenu()==m) //compare pointer to submenu with known submenus and break out if loop if matched
//            break;
//    }
//    if(m_interpnum>=m_ic.interps.size()||m_interpnum>=10)
//    {
//        cbMessageBox(_T("Warning: Sub menu not found - cancelling command"));
//        return;
//    }
//    commandstr=m_ic.interps[m_interpnum].command;
//    consolename=m_ic.interps[m_interpnum].name;
//
//    wxExecute(commandstr,wxEXEC_ASYNC);
////    m_shellmgr->LaunchProcess(commandstr,consolename,0);
}

// constructor
PowerShell::PowerShell()
{
    // Make sure our resources are available.
    // In the generated boilerplate code we have no resources but when
    // we add some, it will be nice that this code is in place already ;)
    if(!Manager::LoadResource(_T("PowerShell.zip")))
    {
        NotifyMissingFile(_T("PowerShell.zip"));
    }
}

cbConfigurationPanel* PowerShell::GetConfigurationPanel(wxWindow* parent)
{
//    MyDialog* dlg = new MyDialog(this, *m_pKeyProfArr, parent,
//        wxT("Keybindings"), mode);

    return new CmdConfigDialog(parent, this);
}

// destructor
PowerShell::~PowerShell()
{

}

void PowerShell::OnAttach()
{
	// do whatever initialization you need for your plugin
	// NOTE: after this function, the inherited member variable
	// m_IsAttached will be TRUE...
	// You should check for it in other functions, because if it
	// is FALSE, it means that the application did *not* "load"
	// (see: does not need) this plugin...

	m_ic.ReadConfig();
	m_ic.ImportLegacyConfig();

	m_pipeoutput=true;

    m_shellmgr = new ShellManager(Manager::Get()->GetAppWindow());

    CodeBlocksDockEvent evt(cbEVT_ADD_DOCK_WINDOW);
    evt.name = _T("Shells");
    evt.title = _T("Shells");
    evt.pWindow = m_shellmgr;
    evt.dockSide = CodeBlocksDockEvent::dsFloating;
    evt.desiredSize.Set(400, 300);
    evt.floatingSize.Set(400, 300);
    evt.minimumSize.Set(200, 150);
    Manager::Get()->ProcessEvent(evt);

}

void PowerShell::OnRelease(bool appShutDown)
{
	// do de-initialization for your plugin
	// if appShutDown is false, the plugin is unloaded because Code::Blocks is being shut down,
	// which means you must not use any of the SDK Managers
	// NOTE: after this function, the inherited member variable
	// m_IsAttached will be FALSE...

    if (m_shellmgr) //remove the Shell Terminals Notebook from its dockable window and delete it
    {
        CodeBlocksDockEvent evt(cbEVT_REMOVE_DOCK_WINDOW);
        evt.pWindow = m_shellmgr;
        Manager::Get()->ProcessEvent(evt);
        m_shellmgr->Destroy();
    }
    m_shellmgr = 0;
}

int PowerShell::Configure()
{
	//create and display the configuration dialog for your plugin
	cbConfigurationDialog dlg(Manager::Get()->GetAppWindow(), wxID_ANY, _("ShellCommand Settings"));
	cbConfigurationPanel* panel = GetConfigurationPanel(&dlg);
	if (panel)
	{
		dlg.AttachConfigurationPanel(panel);
		PlaceWindow(&dlg);
		return dlg.ShowModal() == wxID_OK ? 0 : -1;
	}
	return -1;
}

void PowerShell::CreateMenu()
{
    for(unsigned int i=0;i<m_ic.interps.size();i++)
    {
        wxString tail;
        if(m_ic.interps[i].command.Find(_T("$file"))>0||
            m_ic.interps[i].command.Find(_T("$relfile"))>0||
            m_ic.interps[i].command.Find(_T("$dir"))>0||
            m_ic.interps[i].command.Find(_T("$dir"))>0||
            m_ic.interps[i].command.Find(_T("$reldir"))>0||
            m_ic.interps[i].command.Find(_T("$path"))>0||
            m_ic.interps[i].command.Find(_T("$relpath"))>0||
            m_ic.interps[i].command.Find(_T("$fname"))>0||
            m_ic.interps[i].command.Find(_T("$fext"))>0||
            m_ic.interps[i].command.Find(_T("$mpaths"))>0)
            tail=_T("...");
        wxString menuloc=m_ic.interps[i].menu;
        if(menuloc.StartsWith(_T(".")))
            continue;
        wxString newmenutext=menuloc.BeforeFirst('/');
        wxMenu *menu=m_LangMenu;
        while(menuloc.Find('/')!=wxNOT_FOUND)
        {
            menuloc=menuloc.AfterFirst('/');
            wxMenu *submenu=NULL;
            wxMenuItem *mi=menu->FindItem(menu->FindItem(newmenutext));
            if(mi)
                submenu=mi->GetSubMenu();
            if(!submenu)
            {
                submenu=new wxMenu();
                menu->Append(wxID_ANY,newmenutext,submenu); //TODO: insert into correct position determined by priority
            }
            menu=submenu;
            newmenutext=menuloc.BeforeFirst('/');
        }
        if(menuloc.IsEmpty())
            menu->Append(ID_SubMenu_0+i,m_ic.interps[i].name);
        else
            menu->Append(ID_SubMenu_0+i,menuloc);
    }
    m_LangMenu->Append(ID_LaunchPythonProcess,_T("Launch Python Interpreter"),_T(""));
    m_LangMenu->Append(ID_LangMenu_ShowConsole,_T("Toggle Shell Extensions I/O Window"),_T(""),wxITEM_CHECK);
}

void PowerShell::AddModuleMenuEntry(wxMenu *modmenu,int entrynum, int idref)
{
    wxString menuloc=m_ic.interps[entrynum].cmenu;
    if(menuloc==_T("."))
        return;
    wxString newmenutext=menuloc.BeforeFirst('/');
    wxMenu *menu=modmenu;
    while(menuloc.Find('/')!=wxNOT_FOUND)
    {
        menuloc=menuloc.AfterFirst('/');
        wxMenu *submenu=NULL;
        wxMenuItem *mi=menu->FindItem(menu->FindItem(newmenutext));
        if(mi)
            submenu=mi->GetSubMenu();
        if(!submenu)
        {
            submenu=new wxMenu();
            menu->Append(wxID_ANY,newmenutext,submenu); //TODO: insert into correct position determined by priority
        }
        menu=submenu;
        newmenutext=menuloc.BeforeFirst('/');
    }
    if(menuloc.IsEmpty())
        menu->Append(ID_ContextMenu_0+idref,m_ic.interps[entrynum].name);
    else
        menu->Append(ID_ContextMenu_0+idref,menuloc);
}


void PowerShell::UpdateMenu()
{
    //delete the old menu items
    if(m_LangMenu)
    {
//        for(unsigned int i=0;i<m_ic.interps.Len();i++)
//            m_LangMenu->Destroy(ID_SubMenu_0+i);
//        m_LangMenu->Destroy(ID_LangMenu_ShowConsole);
        size_t count=m_LangMenu->GetMenuItemCount();
        for(size_t i=0;i<count;i++)
            m_LangMenu->Destroy(m_LangMenu->FindItemByPosition(0));
        CreateMenu();
    }
}

void PowerShell::BuildMenu(wxMenuBar* menuBar)
{
	//The application is offering its menubar for your plugin,
	//to add any menu items you want...
	//Append any items you need in the menu...
	//NOTE: Be careful in here... The application's menubar is at your disposal.
	m_LangMenu=new wxMenu;
	CreateMenu();
	int pos = menuBar->FindMenu(_T("Plugins"));
	if(pos!=wxNOT_FOUND)
        menuBar->Insert(pos, m_LangMenu, _T("E&xtensions"));
    else
    {
        delete m_LangMenu;
        m_LangMenu=0;
    }
//	NotImplemented(_T("PowerShell::BuildMenu()"));
}

void PowerShell::BuildModuleMenu(const ModuleType type, wxMenu* menu, const FileTreeData* data)
{
	//Some library module is ready to display a pop-up menu.
	//Check the parameter \"type\" and see which module it is
	//and append any items you need in the menu...
	//TIP: for consistency, add a separator as the first item...
	m_contextvec.Empty();
	if(type==mtProjectManager)
	{
	    if(data)
	    {
            if(data->GetKind()==FileTreeData::ftdkProject)
            {
                cbProject* prj = data->GetProject();
                wxString filename=wxFileName(prj->GetFilename()).GetPath();
                wxString name=_T("");
                size_t sep_pos=menu->GetMenuItemCount();
                size_t added=0;
                for(unsigned int i=0;i<m_ic.interps.size();i++)
                    if(WildCardListMatch(m_ic.interps[i].wildcards,name))
                    {
                        m_RunTarget=filename;
                        if(m_ic.interps[i].command.Find(_T("$dir"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$reldir"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$path"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$relpath"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$mpaths"))>=0)
                        {
                            wxString menutext=m_ic.interps[i].name;
                            m_contextvec.Add(i);
                            AddModuleMenuEntry(menu,i,added);
                            added++;
                        }
                    }
                if(added>0)
                    menu->InsertSeparator(sep_pos);
            }
            if(data->GetKind()==FileTreeData::ftdkFile)
            {
                ProjectFile *f=data->GetProjectFile();
                if(f)
                {
                    wxString filename=f->file.GetFullPath();
                    wxString name=f->file.GetFullName();
                    size_t sep_pos=menu->GetMenuItemCount();
                    size_t added=0;
                    for(unsigned int i=0;i<m_ic.interps.size();i++)
                        if(WildCardListMatch(m_ic.interps[i].wildcards,name))
                        {
                            m_RunTarget=filename;
                            if(m_ic.interps[i].command.Find(_T("$file"))>=0 ||
                                m_ic.interps[i].command.Find(_T("$relfile"))>=0 ||
                                m_ic.interps[i].command.Find(_T("$fname"))>=0 ||
                                m_ic.interps[i].command.Find(_T("$fext"))>=0 ||
                                m_ic.interps[i].command.Find(_T("$path"))>=0 ||
                                m_ic.interps[i].command.Find(_T("$relpath"))>=0 ||
                                m_ic.interps[i].command.Find(_T("$mpaths"))>=0)
                            {
                                wxString menutext=m_ic.interps[i].name;
                                m_contextvec.Add(i);
                                AddModuleMenuEntry(menu,i,added);
                                added++;
                            }
                        }
                    if(added>0)
                        menu->InsertSeparator(sep_pos);
                }
            }
	    }
	}
	if(type==mtEditorManager) // also type==mtOpenFilesList - not sure how to find out which file has been right clicked.
	{
        EditorManager* edMan = Manager::Get()->GetEditorManager();
        wxFileName activefile(edMan->GetActiveEditor()->GetFilename());
        wxString filename=activefile.GetFullPath();
        wxString name=activefile.GetFullName();
        size_t sep_pos=menu->GetMenuItemCount();
        size_t added=0;
        for(unsigned int i=0;i<m_ic.interps.size();i++)
            if(WildCardListMatch(m_ic.interps[i].wildcards,name))
            {
                m_RunTarget=filename;
                    if(m_ic.interps[i].command.Find(_T("$file"))>=0 ||
                        m_ic.interps[i].command.Find(_T("$relfile"))>=0 ||
                        m_ic.interps[i].command.Find(_T("$fname"))>=0 ||
                        m_ic.interps[i].command.Find(_T("$fext"))>=0 ||
                        m_ic.interps[i].command.Find(_T("$path"))>=0 ||
                        m_ic.interps[i].command.Find(_T("$relpath"))>=0 ||
                        m_ic.interps[i].command.Find(_T("$mpaths"))>=0)
                    {
                        wxString menutext=m_ic.interps[i].name;
                        m_contextvec.Add(i);
                        AddModuleMenuEntry(menu,i,added);
                        added++;
                    }
            }
        if(added>0)
            menu->InsertSeparator(sep_pos);
	}
    if(type==mtUnknown) //Assuming file explorer -- fileexplorer fills the filetreedata with ftdkFile or ftdkFolder as "kind", the folder is the full path of the entry
	    if(data)
	    {
            size_t sep_pos=menu->GetMenuItemCount();
            size_t added=0;
            if(data->GetKind()==FileTreeData::ftdkFile)  //right clicked on folder in file explorer
            {
                wxFileName f(data->GetFolder());
                wxString filename=f.GetFullPath();
                wxString name=f.GetFullName();
                for(unsigned int i=0;i<m_ic.interps.size();i++)
                    if(WildCardListMatch(m_ic.interps[i].wildcards,name))
                    {
                        m_RunTarget=filename;
                        if(m_ic.interps[i].command.Find(_T("$file"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$relfile"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$fname"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$fext"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$path"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$relpath"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$mpaths"))>=0)
                        {
                            wxString menutext=m_ic.interps[i].name;
                            m_contextvec.Add(i);
                            AddModuleMenuEntry(menu,i,added);
                            added++;
                        }
                    }
            }
            if(data->GetKind()==FileTreeData::ftdkFolder) //right clicked on folder in file explorer
            {
                wxFileName f(data->GetFolder());
                wxString filename=f.GetFullPath();
                wxString name=f.GetFullName();
                for(unsigned int i=0;i<m_ic.interps.size();i++)
                    if(WildCardListMatch(m_ic.interps[i].wildcards,name))
                    {
                        m_RunTarget=filename;
                        if(m_ic.interps[i].command.Find(_T("$dir"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$reldir"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$path"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$relpath"))>=0 ||
                            m_ic.interps[i].command.Find(_T("$mpaths"))>=0)
                        {
                            wxString menutext=m_ic.interps[i].name;
                            m_contextvec.Add(i);
                            AddModuleMenuEntry(menu,i,added);
                            added++;
                        }
                    }
            }
            if(data->GetKind()==FileTreeData::ftdkVirtualGroup) //right clicked on multiple selections in file explorer
            {
                wxString paths=data->GetFolder(); //get folder contains a space separated list of the files/directories selected
                for(unsigned int i=0;i<m_ic.interps.size();i++)
                {
                    bool match=true; // all selected items must have names that match the wildcard for this grouping
                    wxString pathlist=paths;
                    wxString ipath=paths.BeforeFirst('*'); // '*' separated list
                    if(m_ic.interps[i].wildcards!=_T(""));
                        while(match && pathlist!=_T(""))
                        {
                            wxString name=wxFileName(ipath).GetFullName();
                            if(ipath!=_T("") && !WildCardListMatch(m_ic.interps[i].wildcards,ipath))
                                match=false;
                            pathlist=pathlist.AfterFirst('*');
                            ipath=pathlist.BeforeFirst('*');
                        }
                    if(match)
                    {
                        m_RunTarget=paths;
                        //TODO: need a m_TargetParent to allow the FileExplorer to define the parent of a selection (usually the root of the fileexplorer view?)
                        if(m_ic.interps[i].command.Find(_T("$mpaths"))>=0)
                        {
                            wxString menutext=m_ic.interps[i].name;
                            m_contextvec.Add(i);
                            AddModuleMenuEntry(menu,i,added);
                            added++;
                        }
                    }
                }
            }
            if(added>0)
                menu->InsertSeparator(sep_pos);
	    }
//	NotImplemented(_T("PowerShell::BuildModuleMenu()"));
}
