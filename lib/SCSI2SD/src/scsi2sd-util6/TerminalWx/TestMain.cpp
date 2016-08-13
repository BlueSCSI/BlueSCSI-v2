/*
TerminalWx - A wxWidgets terminal widget
Copyright (C) 2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/

#include <wx/gdicmn.h>
#include <wx/menuitem.h>
#include <wx/utils.h>
#include <wx/msgdlg.h>

#include "TestMain.h"
#include "src/terminalinputevent.h"

//(*InternalHeaders(TerminalWxFrame)
#include <wx/string.h>
#include <wx/intl.h>
//*)

//helper functions
enum wxbuildinfoformat {
    short_f, long_f };

wxString wxbuildinfo(wxbuildinfoformat format)
{
    wxString wxbuild(wxVERSION_STRING);

    if (format == long_f )
    {
#if defined(__WXMSW__)
        wxbuild << _T("-Windows");
#elif defined(__UNIX__)
        wxbuild << _T("-Linux");
#endif

#if wxUSE_UNICODE
        wxbuild << _T("-Unicode build");
#else
        wxbuild << _T("-ANSI build");
#endif // wxUSE_UNICODE
    }

    return wxbuild;
}

//(*IdInit(TerminalWxFrame)
const long TerminalWxFrame::ID_TERM = wxNewId();
const long TerminalWxFrame::idMenuQuit = wxNewId();
const long TerminalWxFrame::idMenuAbout = wxNewId();
const long TerminalWxFrame::ID_STATUSBAR1 = wxNewId();
//*)

BEGIN_EVENT_TABLE(TerminalWxFrame,wxFrame)
    //(*EventTable(TerminalWxFrame)
    //*)
END_EVENT_TABLE()

TerminalWxFrame::TerminalWxFrame(wxWindow* parent,wxWindowID id)
{
    //(*Initialize(TerminalWxFrame)
    wxMenuItem* MenuItem2;
    wxMenuItem* MenuItem1;
    wxMenu* Menu1;
    wxMenuBar* MenuBar1;
    wxMenu* Menu2;

    Create(parent, id, _("Test TerminalWx App"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, _T("id"));
    Term1 = new TerminalWx(this,ID_TERM,wxPoint(72,56),80,24,_T("ID_TERM"));
    MenuBar1 = new wxMenuBar();
    Menu1 = new wxMenu();
    MenuItem1 = new wxMenuItem(Menu1, idMenuQuit, _("Quit\tAlt-F4"), _("Quit the application"), wxITEM_NORMAL);
    Menu1->Append(MenuItem1);
    MenuBar1->Append(Menu1, _("&File"));
    Menu2 = new wxMenu();
    MenuItem2 = new wxMenuItem(Menu2, idMenuAbout, _("About\tF1"), _("Show info about this application"), wxITEM_NORMAL);
    Menu2->Append(MenuItem2);
    MenuBar1->Append(Menu2, _("Help"));
    SetMenuBar(MenuBar1);
    StatusBar1 = new wxStatusBar(this, ID_STATUSBAR1, 0, _T("ID_STATUSBAR1"));
    int __wxStatusBarWidths_1[1] = { -1 };
    int __wxStatusBarStyles_1[1] = { wxSB_NORMAL };
    StatusBar1->SetFieldsCount(1,__wxStatusBarWidths_1);
    StatusBar1->SetStatusStyles(1,__wxStatusBarStyles_1);
    SetStatusBar(StatusBar1);

    Connect(idMenuQuit,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&TerminalWxFrame::OnQuit);
    Connect(idMenuAbout,wxEVT_COMMAND_MENU_SELECTED,(wxObjectEventFunction)&TerminalWxFrame::OnAbout);
    Connect(wxID_ANY,wxEVT_CLOSE_WINDOW,(wxObjectEventFunction)&TerminalWxFrame::OnClose);
    //*)
}

TerminalWxFrame::~TerminalWxFrame()
{
    //(*Destroy(TerminalWxFrame)
    //*)
}

void TerminalWxFrame::OnQuit(wxCommandEvent& event)
{
    Close();
}

void TerminalWxFrame::OnAbout(wxCommandEvent& event)
{
    wxString msg = wxbuildinfo(long_f);
    wxMessageBox(msg, _("Welcome to..."));

    Term1->DisplayCharsUnsafe(msg);
}

void TerminalWxFrame::OnClose(wxCloseEvent& event)
{
    Destroy();
}
