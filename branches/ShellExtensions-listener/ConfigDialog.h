#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif

#include <sdk.h>
#include <configurationpanel.h>
#include "shellproperties.h"



class ShellExtensions;
class CommandCollection;

class ConfigDialog : public cbConfigurationPanel
{
  private:

  public:
    ConfigDialog(wxWindow* parent, ShellExtensions* plugin);
    virtual ~ConfigDialog();

    virtual wxString GetTitle() const { return _("Shell Extensions"); }
    virtual wxString GetBitmapBaseName() const { return _T("batch"); }
    virtual void OnApply();
    virtual void OnCancel(){}
  protected:
    void New(wxCommandEvent &event);
    //void Edit(wxCommandEvent &event);
    void Copy(wxCommandEvent &event);
    void Delete(wxCommandEvent &event);
    void ChangeSelection(wxCommandEvent &event);
    void NameChange(wxCommandEvent &event);
    void OnUp(wxCommandEvent &event);
    void OnDown(wxCommandEvent &event);
    // Controls in the Edit ShellCommand Box
    void OnEditOK(wxCommandEvent &event);
    void OnEditCancel(wxCommandEvent &event);
    void OnEditBrowseExec(wxCommandEvent &event);
    void SetDialogItems();
    void GetDialogItems();

//    void ListChange(wxCommandEvent &event);
//    void UpdateUI(wxUpdateUIEvent &event);
    wxStaticText* m_staticText1;
    wxButton* m_butnew;
    wxButton* m_butcopy;
    wxButton* m_butdelete;
    wxButton* m_butup;
    wxButton* m_down;
    wxListBox* m_interplist;
    wxStaticText* m_staticText2;
    wxTextCtrl* m_editname;
    wxStaticText* m_staticText3;
    wxTextCtrl* m_editexec;
    wxButton* m_butbrowse;
    wxStaticText* m_staticText5;
    wxTextCtrl* m_editext;
    wxStaticText* m_staticText4;
    wxTextCtrl* m_editactions;
    int m_activeinterp;

  private:
    CommandCollection m_ic;
    CommandCollection *m_icperm;
    ShellExtensions *m_plugin;
    void UpdateEntry(int index);
    void ChooseFile();

    DECLARE_EVENT_TABLE()
};



#endif // CONFIGDIALOG

