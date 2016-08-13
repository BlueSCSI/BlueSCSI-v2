/*
TerminalWx - A wxWidgets terminal widget
Copyright (C) 2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/

#ifndef TESTMAIN_H
#define TESTMAIN_H

//(*Headers(TerminalWxFrame)
#include <wx/menu.h>
#include <wx/statusbr.h>
#include <wx/frame.h>
//*)

#include <wx/event.h>
#include <wx/window.h>

#include "src/terminalwx.h"

class TerminalWxFrame: public wxFrame
{
    public:

        TerminalWxFrame(wxWindow* parent,wxWindowID id = -1);
        virtual ~TerminalWxFrame();

    private:

        //(*Handlers(TerminalWxFrame)
        void OnQuit(wxCommandEvent& event);
        void OnAbout(wxCommandEvent& event);
        void OnClose(wxCloseEvent& event);
        void OnCustom1Paint(wxPaintEvent& event);
        //*)

        //(*Identifiers(TerminalWxFrame)
        static const long ID_TERM;
        static const long idMenuQuit;
        static const long idMenuAbout;
        static const long ID_STATUSBAR1;
        //*)

        //(*Declarations(TerminalWxFrame)
        wxStatusBar* StatusBar1;
        TerminalWx* Term1;
        //*)

        DECLARE_EVENT_TABLE()
};

#endif // TERMINALWXMAIN_H
