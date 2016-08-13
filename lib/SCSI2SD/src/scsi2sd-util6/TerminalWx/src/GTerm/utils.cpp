/*
TerminalWx - A wxWidgets terminal widget
Copyright (C) 1999  Timothy Miller
              2004  Mark Erikson
              2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/


#include "gterm.hpp"
//#include <stdlib.h>

int GTerm::calc_color(int fg, int bg, int flags)
{
	return (flags & 15) | (fg << 4) | (bg << 8);
}

void GTerm::update_changes()
{
    int yp, start_x, mx;
    int blank, c, x, y;

    // prevent recursion for scrolls which cause exposures
    if (doing_update) return;
    doing_update = 1;

    // first perform scroll-copy
    mx = scroll_bot-scroll_top+1;
    if (!(mode_flags & TEXTONLY) && pending_scroll &&
	pending_scroll<mx && -pending_scroll<mx) {
	if (pending_scroll<0) {
	    MoveChars(0, scroll_top, 0, scroll_top-pending_scroll,
		width, scroll_bot-scroll_top+pending_scroll+1);
	} else {
	    MoveChars(0, scroll_top+pending_scroll, 0, scroll_top,
		width, scroll_bot-scroll_top-pending_scroll+1);
	}
    }
    pending_scroll = 0;

    // then update characters
    for (y=0; y<height; y++) {
	if (dirty_startx[y]>=MAXWIDTH) continue;
	yp = linenumbers[y]*MAXWIDTH;

	blank = !(mode_flags & TEXTONLY);
	start_x = dirty_startx[y];
	c = color[yp+start_x];
	for (x=start_x; x<=dirty_endx[y]; x++) {
	    if (text[yp+x]!=32 && text[yp+x]) blank = 0;
	    if (c != color[yp+x]) {
		if (!blank) {
                  if(mode_flags & PC)
		    DrawText((c>>4)&0xf, (c>>8)&0xf, c/*&15*/, start_x,
			y, x-start_x, text+yp+start_x);
                  else
		    DrawText((c>>4)&7, (c>>8)&7, c/*&15*/, start_x,
			y, x-start_x, text+yp+start_x);
		} else {
	    	    ClearChars((c>>8)&7, start_x, y, x-start_x, 1);
		}
		start_x = x;
		c = color[yp+x];
		blank = !(mode_flags & TEXTONLY);
	    	if (text[yp+x]!=32 && text[yp+x]) blank = 0;
	    }
	}
		if (!blank) {
                  if(mode_flags & PC)
		    DrawText((c>>4)&0xf, (c>>8)&0xf, c/*&15*/, start_x,
			y, x-start_x, text+yp+start_x);
                  else
		    DrawText((c>>4)&7, (c>>8)&7, c/*&15*/, start_x,
			y, x-start_x, text+yp+start_x);
		} else {
	    	    ClearChars((c>>8)&7, start_x, y, x-start_x, 1);
		}

	dirty_endx[y] = 0;
	dirty_startx[y] = MAXWIDTH;
    }

    if (!(mode_flags & CURSORINVISIBLE)) {
	x = cursor_x;
	if (x>=width) x = width-1;
        yp = linenumbers[cursor_y]*MAXWIDTH+x;
        c = color[yp];
        if(mode_flags & PC)
          DrawCursor((c>>4)&0xf, (c>>8)&0xf, c&15, x, cursor_y, text[yp]);
        else
          DrawCursor((c>>4)&7, (c>>8)&7, c&15, x, cursor_y, text[yp]);
    }

    doing_update = 0;
}

void GTerm::scroll_region(int start_y, int end_y, int num)
{
	int y, takey, fast_scroll, mx, clr, x, yp, c;
	short temp[MAXHEIGHT];
	unsigned char temp_sx[MAXHEIGHT], temp_ex[MAXHEIGHT];

	if (!num) return;
	mx = end_y-start_y+1;
	if (num > mx) num = mx;
	if (-num > mx) num = -mx;

	fast_scroll = (start_y == scroll_top && end_y == scroll_bot &&
		!(mode_flags & TEXTONLY));

	if (fast_scroll) pending_scroll += num;

	memcpy(temp, linenumbers, sizeof(linenumbers));
	if (fast_scroll) {
		memcpy(temp_sx, dirty_startx, sizeof(dirty_startx));
		memcpy(temp_ex, dirty_endx, sizeof(dirty_endx));
	}

        c = calc_color(fg_color, bg_color, mode_flags);

	// move the lines by renumbering where they point to
	if (num<mx && -num<mx) for (y=start_y; y<=end_y; y++) {
		takey = y+num;
		clr = (takey<start_y) || (takey>end_y);
		if (takey<start_y) takey = end_y+1-(start_y-takey);
		if (takey>end_y) takey = start_y-1+(takey-end_y);

		linenumbers[y] = temp[takey];
		if (!fast_scroll || clr) {
			dirty_startx[y] = 0;
			dirty_endx[y] = width-1;
		} else {
			dirty_startx[y] = temp_sx[takey];
			dirty_endx[y] = temp_ex[takey];
		}
		if (clr) {
			yp = linenumbers[y]*MAXWIDTH;
			memset(text+yp, 32, width);
			for (x=0; x<width; x++) {
				color[yp++] = c;
			}
		}
	}
}

void GTerm::shift_text(int y, int start_x, int end_x, int num)
{
     int x, yp, mx, c;

     if (!num) return;

     yp = linenumbers[y]*MAXWIDTH;

     mx = end_x-start_x+1;
     if (num>mx) num = mx;
     if (-num>mx) num = -mx;

     if (num<mx && -num<mx) {
	if (num<0) {
	    memmove(text+yp+start_x, text+yp+start_x-num, mx+num);
	    memmove(color+yp+start_x, color+yp+start_x-num, (mx+num)<<1);
	} else {
	    memmove(text+yp+start_x+num, text+yp+start_x, mx-num);
	    memmove(color+yp+start_x+num, color+yp+start_x, (mx-num)<<1);
	}
    }

    if (num<0) {
	x = yp+end_x+num+1;
    } else {
	x = yp+start_x;
    }
    num = abs(num);
    memset(text+x, 32, num);
    c = calc_color(fg_color, bg_color, mode_flags);
    while (num--) color[x++] = c;

    changed_line(y, start_x, end_x);
}

void GTerm::clear_area(int start_x, int start_y, int end_x, int end_y)
{
	int x, y, c, yp, w;

    	c = calc_color(fg_color, bg_color, mode_flags);

	w = end_x - start_x + 1;
	if (w<1) return;

	for (y=start_y; y<=end_y; y++) {
		yp = linenumbers[y]*MAXWIDTH;
		memset(text+yp+start_x, 32, w);
		for (x=start_x; x<=end_x; x++) {
			color[yp+x] = c;
		}
		changed_line(y, start_x, end_x);
	}
}

void GTerm::changed_line(int y, int start_x, int end_x)
{
	if (dirty_startx[y] > start_x) dirty_startx[y] = start_x;
	if (dirty_endx[y] < end_x) dirty_endx[y] = end_x;
}

void GTerm::move_cursor(int x, int y)
{
	if (cursor_x>=width) cursor_x = width-1;
	changed_line(cursor_y, cursor_x, cursor_x);
	cursor_x = x;
	cursor_y = y;
}

void GTerm::set_mode_flag(int flag)
{
	mode_flags |= flag;
	ModeChange(mode_flags);
}

void GTerm::clear_mode_flag(int flag)
{
	mode_flags &= ~flag;
	ModeChange(mode_flags);
}

