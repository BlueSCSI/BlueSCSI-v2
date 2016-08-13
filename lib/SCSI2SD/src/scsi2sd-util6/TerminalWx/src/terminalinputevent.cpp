/*
TerminalWx - A wxWidgets terminal widget
Copyright (C) 2004  Mark Erikson
              2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/

#include "terminalinputevent.h"


DEFINE_EVENT_TYPE(chEVT_TERMINAL_INPUT)

TerminalInputEvent::TerminalInputEvent(wxString str)
	: wxEvent(0, chEVT_TERMINAL_INPUT),m_string(str)
{
	//
}

TerminalInputEvent::TerminalInputEvent(const TerminalInputEvent& otherEvent)
: wxEvent(otherEvent)
{
	this->SetString(otherEvent.GetString());
}
