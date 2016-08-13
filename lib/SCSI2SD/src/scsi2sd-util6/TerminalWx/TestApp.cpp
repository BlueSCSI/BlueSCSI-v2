/*
TerminalWx - A wxWidgets terminal widget
Copyright (C) 2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/

#include "TestApp.h"

//(*AppHeaders
#include "TestMain.h"
#include <wx/image.h>
//*)

IMPLEMENT_APP(TestApp);

bool TestApp::OnInit()
{
    //(*AppInitialize
    bool wxsOK = true;
    wxInitAllImageHandlers();
    if ( wxsOK )
    {
    	TerminalWxFrame* Frame = new TerminalWxFrame(0);
    	Frame->Show();
    	SetTopWindow(Frame);
    }
    //*)
    return wxsOK;

}
