/*
TerminalWx - A wxWidgets terminal widget
Copyright (C) 1999  Timothy Miller
              2004  Mark Erikson
              2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/

#ifdef __GNUG__
    #pragma implementation "gterm.hpp"
#endif

#include "gterm.hpp"
#include <algorithm>

using namespace std;

void GTerm::Update()
{
	update_changes();
}

void GTerm::ProcessInput(int len, unsigned char *data)
{
//printf("ProcessInput called...\n");
	int i;
	StateOption *last_state;

	data_len = len;
	input_data = data;

	while (data_len) {
//printf("ProcessInput() processing %d...\n", *input_data);
		i = 0;
		while (current_state[i].byte != -1 &&
		       current_state[i].byte != *input_data) i++;

		// action must be allowed to redirect state change
		last_state = current_state+i;
		current_state = last_state->next_state;
		if (last_state->action)
			(this->*(last_state->action))();
		input_data++;
		data_len--;
	}

	if (!(mode_flags & DEFERUPDATE) ||
	    (pending_scroll > scroll_bot-scroll_top)) update_changes();
}

void GTerm::Reset()
{
	reset();
}

void GTerm::ExposeArea(int x, int y, int w, int h)
{
	int i;
	for (i=0; i<h; i++) changed_line(i+y, x, x+w-1);
	if (!(mode_flags & DEFERUPDATE)) update_changes();
}

void GTerm::ResizeTerminal(int w, int h)
{
	int cx, cy;
	clear_area(min(width,w), 0, MAXWIDTH-1, MAXHEIGHT-1);
	clear_area(0, min(height,h), min(width,w)-1, MAXHEIGHT-1);
	width = w;
	height = h;
	scroll_bot = height-1;
	if (scroll_top >= height) scroll_top = 0;
	cx = min(width-1, cursor_x);
	cy = min(height-1, cursor_y);
	move_cursor(cx, cy);
}

GTerm::GTerm(int w, int h) : width(w), height(h)
{
	int i;

	doing_update = 0;

	// could make this dynamic
	text = new unsigned char[MAXWIDTH*MAXHEIGHT+1];
	color = new unsigned short[MAXWIDTH*MAXHEIGHT];

	for (i=0; i<MAXHEIGHT; i++) {
		// make it draw whole terminal to start
		dirty_startx[i] = 0;
		dirty_endx[i] = MAXWIDTH-1;
	}

#ifdef GTERM_PC
        pc_machinename = new char[7];
        strcpy(pc_machinename, "pcterm");
#endif
	cursor_x = 0;
	cursor_y = 0;
	save_x = 0;
	save_y = 0;
        mode_flags = 0;
	reset();
}

GTerm::~GTerm()
{
	delete[] text;
	delete[] color;
#ifdef GTERM_PC
        if(pc_machinename)
          delete[] pc_machinename;
#endif // GTERM_PC
}

#ifdef GTERM_PC
void
GTerm::SetMachineName(char *machinename)
{
  if(pc_machinename)
    delete pc_machinename;

  pc_machinename = new char[strlen(machinename) + 1];
  strcpy(pc_machinename, machinename);
}
#endif // GTERM_PC

int
GTerm::IsSelected(int x, int y)
{
  if(color && x >= 0 && x < Width() && y >= 0 && y < Height())
    return color[(linenumbers[y] * MAXWIDTH) + x] & SELECTED;
  return 0;
}

void
GTerm::Select(int x, int y, int select)
{
  if(color && x >= 0 && x < Width() && y >= 0 && y < Height())
  {
    if(select)
      color[(linenumbers[y] * MAXWIDTH) + x] |= SELECTED;
    else
      color[(linenumbers[y] * MAXWIDTH) + x] &= ~SELECTED;
    changed_line(y, x, x);
//    update_changes();
  }
}

unsigned char
GTerm::GetChar(int x, int y)
{
  if(text && x >= 0 && x < Width() && y >= 0 && y < Height())
    return text[(linenumbers[y] * MAXWIDTH) + x];

  return 0;
}



