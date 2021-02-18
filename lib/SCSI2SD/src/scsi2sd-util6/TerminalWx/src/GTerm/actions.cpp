/*
TerminalWx - A wxWidgets terminal widget
Copyright (C) 1999 Timothy Miller
              2004  Mark Erikson
              2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/

#include "gterm.hpp"


// For efficiency, this grabs all printing characters from buffer, up to
// the end of the line or end of buffer
void GTerm::normal_input()
{
	int n, n_taken, i, c, y;
#if 0
char str[100];
#endif

#define IS_CTRL_CHAR(c) ((c)<32 || (c) ==127)

	if (IS_CTRL_CHAR(*input_data)) return;

	if (cursor_x >= width) {
		if (mode_flags & NOEOLWRAP) {
			cursor_x = width-1;
		} else {
			next_line();
		}
	}

	n = 0;
	if (mode_flags & NOEOLWRAP) {
		while (!IS_CTRL_CHAR(input_data[n]) && n<data_len) n++;
		n_taken = n;
		if (cursor_x+n>=width) n = width-cursor_x;
	} else {
		while (!IS_CTRL_CHAR(input_data[n]) && n<data_len && cursor_x+n<width) n++;
		n_taken = n;
	}

#if 0
memcpy(str, input_data, n);
str[n] = 0;
//printf("Processing %d characters (%d): %s\n", n, str[0], str);
#endif

	if (mode_flags & INSERT) {
		changed_line(cursor_y, cursor_x, width-1);
	} else {
		changed_line(cursor_y, cursor_x, cursor_x+n-1);
	}

	y = linenumbers[cursor_y]*MAXWIDTH;
	if (mode_flags & INSERT) for (i=width-1; i>=cursor_x+n; i--) {
		text[y+i] = text[y+i-n];
		color[y+i] = color[y+i-n];
	}

	c = calc_color(fg_color, bg_color, mode_flags);
	for (i=0; i<n; i++) {
		text[y+cursor_x] = input_data[i];
		color[y+cursor_x] = c;
		cursor_x++;
	}

	input_data += n_taken-1;
	data_len -= n_taken-1;
}

void GTerm::cr()
{
	move_cursor(0, cursor_y);
}

void GTerm::lf()
{
	if (cursor_y < scroll_bot) {
		move_cursor(cursor_x, cursor_y+1);
	} else {
		scroll_region(scroll_top, scroll_bot, 1);
	}
}

void GTerm::ff()
{
	clear_area(0, scroll_top, width-1, scroll_bot);
	move_cursor(0, scroll_top);
}

void GTerm::tab()
{
	int i, x = 0;

	for (i=cursor_x+1; i<width && !x; i++) if (tab_stops[i]) x = i;
	if (!x) x = (cursor_x+8) & -8;
	if (x < width) {
		move_cursor(x, cursor_y);
	} else {
		if (mode_flags & NOEOLWRAP) {
			move_cursor(width-1, cursor_y);
		} else {
			next_line();
		}
	}
}

void GTerm::bs()
{
	if (cursor_x>0) move_cursor(cursor_x-1, cursor_y);
	if (mode_flags & DESTRUCTBS) {
	    clear_area(cursor_x, cursor_y, cursor_x, cursor_y);
	}
}

void GTerm::bell()
{
	Bell();
}

void GTerm::clear_param()
{
	nparam = 0;
	memset(param, 0, sizeof(param));
	q_mode = 0;
	got_param = 0;
}

void GTerm::keypad_numeric() { clear_mode_flag(KEYAPPMODE); }
void GTerm::keypad_application() { set_mode_flag(KEYAPPMODE); }

void GTerm::save_cursor()
{
	save_attrib = mode_flags;
	save_x = cursor_x;
	save_y = cursor_y;
}

void GTerm::restore_cursor()
{
	mode_flags = (mode_flags & ~15) | (save_attrib & 15);
	move_cursor(save_x, save_y);
}

void GTerm::set_tab()
{
	tab_stops[cursor_x] = 1;
}

void GTerm::index_down()
{
	lf();
}

void GTerm::next_line()
{
	lf(); cr();
}

void GTerm::index_up()
{
	if (cursor_y > scroll_top) {
		move_cursor(cursor_x, cursor_y-1);
	} else {
		scroll_region(scroll_top, scroll_bot, -1);
	}
}

void GTerm::reset()
{
	int i;

	pending_scroll = 0;
	bg_color = 0;
	fg_color = 7;
	scroll_top = 0;
	scroll_bot = height-1;
	for (i=0; i<MAXHEIGHT; i++) linenumbers[i] = i;
	memset(tab_stops, 0, sizeof(tab_stops));
	current_state = GTerm::normal_state;

	clear_mode_flag(NOEOLWRAP | CURSORAPPMODE | CURSORRELATIVE |
		NEWLINE | INSERT | UNDERLINE | BLINK | KEYAPPMODE |
                CURSORINVISIBLE);

	clear_area(0, 0, width-1, height-1);
	move_cursor(0, 0);
}

void GTerm::set_q_mode()
{
	q_mode = 1;
}

// The verification test used some strange sequence which was
// ^[[61"p
// in a function called set_level,
// but it didn't explain the meaning.  Just in case I ever find out,
// and just so that it doesn't leave garbage on the screen, I accept
// the quote and mark a flag.
void GTerm::set_quote_mode()
{
	quote_mode = 1;
}

// for performance, this grabs all digits
void GTerm::param_digit()
{
	got_param = 1;
	param[nparam] = param[nparam]*10 + (*input_data)-'0';
}

void GTerm::next_param()
{
	nparam++;
}

void GTerm::cursor_left()
{
	int n, x;
	n = param[0]; if (n<1) n = 1;
	x = cursor_x-n; if (x<0) x = 0;
	move_cursor(x, cursor_y);
}

void GTerm::cursor_right()
{
	int n, x;
	n = param[0]; if (n<1) n = 1;
	x = cursor_x+n; if (x>=width) x = width-1;
	move_cursor(x, cursor_y);
}

void GTerm::cursor_up()
{
	int n, y;
	n = param[0]; if (n<1) n = 1;
	y = cursor_y-n; if (y<0) y = 0;
	move_cursor(cursor_x, y);
}

void GTerm::cursor_down()
{
	int n, y;
	n = param[0]; if (n<1) n = 1;
	y = cursor_y+n; if (y>=height) y = height-1;
	move_cursor(cursor_x, y);
}

void GTerm::cursor_position()
{
	int x, y;
	x = param[1];	if (x<1) x=1;
	y = param[0];	if (y<1) y=1;
	if (mode_flags & CURSORRELATIVE) {
		move_cursor(x-1, y-1+scroll_top);
	} else {
		move_cursor(x-1, y-1);
	}
}

void GTerm::device_attrib()
{
	char *str = "\033[?1;2c";
        ProcessOutput(strlen(str), (unsigned char *)str);
}

void GTerm::delete_char()
{
	int n, mx;
	n = param[0]; if (n<1) n = 1;
	mx = width-cursor_x;
	if (n>=mx) {
		clear_area(cursor_x, cursor_y, width-1, cursor_y);
	} else {
		shift_text(cursor_y, cursor_x, width-1, -n);
	}
}

void GTerm::set_mode()  // h
{
	switch (param[0] + 1000*q_mode) {
		case 1007:	clear_mode_flag(NOEOLWRAP);	break;
		case 1001:	set_mode_flag(CURSORAPPMODE);	break;
		case 1006:	set_mode_flag(CURSORRELATIVE);	break;
		case 4:		set_mode_flag(INSERT);		break;
		case 1003:	RequestSizeChange(132, height);	break;
		case 20:	set_mode_flag(NEWLINE);		break;
		case 12:	clear_mode_flag(LOCALECHO);	break;
		case 1025:
			clear_mode_flag(CURSORINVISIBLE);
			move_cursor(cursor_x, cursor_y);
			break;
	}
}

void GTerm::clear_mode()  // l
{
	switch (param[0] + 1000*q_mode) {
		case 1007:	set_mode_flag(NOEOLWRAP);	break;
		case 1001:	clear_mode_flag(CURSORAPPMODE);	break;
		case 1006:	clear_mode_flag(CURSORRELATIVE); break;
		case 4:		clear_mode_flag(INSERT);	break;
		case 1003:	RequestSizeChange(80, height);	break;
		case 20:	clear_mode_flag(NEWLINE);	break;
		case 1002:	current_state = vt52_normal_state; break;
		case 12:	set_mode_flag(LOCALECHO);	break;
		case 1025:
			set_mode_flag(CURSORINVISIBLE);	break;
			move_cursor(cursor_x, cursor_y);
			break;
	}
}

void GTerm::request_param()
{
	char str[40];
	sprintf(str, "\033[%d;1;1;120;120;1;0x", param[0]+2);
	ProcessOutput(strlen(str), (unsigned char *)str);
}

void GTerm::set_margins()
{
	int t, b;

	t = param[0];
	if (t<1) t = 1;
	b = param[1];
	if (b<1) b = height;

	if (pending_scroll) update_changes();

	scroll_top = t-1;
	scroll_bot = b-1;
	if (cursor_y < scroll_top) move_cursor(cursor_x, scroll_top);
	if (cursor_y > scroll_bot) move_cursor(cursor_x, scroll_bot);
}

void GTerm::delete_line()
{
	int n, mx;
	n = param[0]; if (n<1) n = 1;
	mx = scroll_bot-cursor_y+1;
	if (n>=mx) {
		clear_area(0, cursor_y, width-1, scroll_bot);
	} else {
		scroll_region(cursor_y, scroll_bot, n);
	}
}

void GTerm::status_report()
{
	char str[64];
	if (param[0] == 5) {
		char *str = "\033[0n";
                ProcessOutput(strlen(str), (unsigned char *)str);
	} else if (param[0] == 6) {
		sprintf(str, "\033[%d;%dR", cursor_y+1, cursor_x+1);
		ProcessOutput(strlen(str), (unsigned char *)str);
	}
}

void GTerm::erase_display()
{
	switch (param[0]) {
	case 0:
		clear_area(cursor_x, cursor_y, width-1, cursor_y);
		if (cursor_y<height-1)
			clear_area(0, cursor_y+1, width-1, height-1);
		break;
	case 1:
		clear_area(0, cursor_y, cursor_x, cursor_y);
		if (cursor_y>0)
			clear_area(0, 0, width-1, cursor_y-1);
		break;
	case 2:
		clear_area(0, 0, width-1, height-1);
		break;
	}
}

void GTerm::erase_line()
{
	switch (param[0]) {
	case 0:
		clear_area(cursor_x, cursor_y, width-1, cursor_y);
		break;
	case 1:
		clear_area(0, cursor_y, cursor_x, cursor_y);
		break;
	case 2:
		clear_area(0, cursor_y, width-1, cursor_y);
		break;
	}
}

void GTerm::insert_line()
{
	int n, mx;
	n = param[0]; if (n<1) n = 1;
	mx = scroll_bot-cursor_y+1;
	if (n>=mx) {
		clear_area(0, cursor_y, width-1, scroll_bot);
	} else {
		scroll_region(cursor_y, scroll_bot, -n);
	}
}

void GTerm::set_colors()
{
	int n;

	if (!nparam && param[0] == 0) {
		clear_mode_flag(15);
		fg_color = 7;
		bg_color = 0;
		return;
	}

	for (n=0; n<=nparam; n++) {
		if (param[n]/10 == 4) {
			bg_color = param[n]%10;
		} else if (param[n]/10 == 3) {
			fg_color = param[n]%10;
		} else switch (param[n]) {
		case 0:
			clear_mode_flag(15);
			fg_color = 7;
			bg_color = 0;
			break;
		case 1:
			set_mode_flag(BOLD);
			break;
		case 4:
			set_mode_flag(UNDERLINE);
			break;
		case 5:
			set_mode_flag(BLINK);
			break;
		case 7:
			set_mode_flag(INVERSE);
			break;
		}
	}
}

void GTerm::clear_tab()
{
	if (param[0] == 3) {
		memset(tab_stops, 0, sizeof(tab_stops));
	} else if (param[0] == 0) {
		tab_stops[cursor_x] = 0;
	}
}

void GTerm::insert_char()
{
	int n, mx;
	n = param[0]; if (n<1) n = 1;
	mx = width-cursor_x;
	if (n>=mx) {
		clear_area(cursor_x, cursor_y, width-1, cursor_y);
	} else {
		shift_text(cursor_y, cursor_x, width-1, n);
	}
}

void GTerm::screen_align()
{
	int y, yp, x, c;

	c = calc_color(7, 0, 0);
	for (y=0; y<height; y++) {
		yp = linenumbers[y]*MAXWIDTH;
		changed_line(y, 0, width-1);
		for (x=0; x<width; x++) {
			text[yp+x] = 'E';
			color[yp+x] = c;
		}
	}
}

void GTerm::erase_char()
{
	int n, mx;
	n = param[0]; if (n<1) n = 1;
	mx = width-cursor_x;
	if (n>mx) n = mx;
	clear_area(cursor_x, cursor_y, cursor_x+n-1, cursor_y);
}

void GTerm::vt52_cursory()
{
	// store y coordinate
	param[0] = (*input_data) - 32;
	if (param[0]<0) param[0] = 0;
	if (param[0]>=height) param[0] = height-1;
}

void GTerm::vt52_cursorx()
{
	int x;
	x = (*input_data)-32;
	if (x<0) x = 0;
	if (x>=width) x = width-1;
	move_cursor(x, param[0]);
}

void GTerm::vt52_ident()
{
	char *str = "\033/Z";
        ProcessOutput(strlen(str), (unsigned char *)str);
}


#ifdef GTERM_PC

void GTerm::pc_begin(void)
{
//printf("pc_begin...\n");
  set_mode_flag(PC);
//printf("pc_begin: mode_flags = %x\n", mode_flags);
  ProcessOutput((unsigned int)strlen(pc_machinename) + 1, (unsigned char *)pc_machinename);
  pc_oldWidth = Width();
  pc_oldHeight = Height();
  ResizeTerminal(80, 25);
  update_changes();
}

void GTerm::pc_end(void)
{
//  printf("pc_end...\n");
  clear_mode_flag(PC);
  ResizeTerminal(pc_oldWidth, pc_oldHeight);
  update_changes();
}

void GTerm::pc_cmd(void)
{
  pc_curcmd = *input_data;
//printf("pc_cmd: pc_curcmd = %d...\n", pc_curcmd);
  pc_argcount = 0;
  switch(pc_curcmd)
  {
    case GTERM_PC_CMD_CURONOFF :                 // <on/off>
      pc_numargs = 1;
    break;

    case GTERM_PC_CMD_MOVECURSOR :               // <x> <y>
      pc_numargs = 2;
    break;

    case GTERM_PC_CMD_PUTTEXT :                  // <x> <y> <wid> <len>
      pc_numargs = 4;
    break;

    case GTERM_PC_CMD_WRITE :                   // <x> <y> <wid> <attr>
      pc_numargs = 4;
    break;

    case GTERM_PC_CMD_MOVETEXT :                 // <sx> <sy> <wid> <len> <dx> <dy>
      pc_numargs = 6;
    break;

    case GTERM_PC_CMD_BEEP :
      Bell();
    break;

    case GTERM_PC_CMD_SELECTPRINTER :
      pc_numargs = 1;
    break;

    case GTERM_PC_CMD_PRINTCHAR :
      pc_numargs = 1;
    break;

    case GTERM_PC_CMD_PRINTCHARS :
      pc_numargs = 2;
    break;

    default :
      current_state = pc_cmd_state;
    break;
  }
}

void GTerm::pc_arg(void)
{
  int
    i,
    yp,
    yp2;

//printf("pc_arg: pc_curcmd = %d...\n", pc_curcmd);

  pc_args[pc_argcount++] = *input_data;
  if(pc_argcount == pc_numargs)
  {
    switch(pc_curcmd)
    {
      case GTERM_PC_CMD_CURONOFF :
//printf("pc_arg: curonoff got %d\n", *input_data);
        if(*input_data)
          clear_mode_flag(CURSORINVISIBLE);
        else
          set_mode_flag(CURSORINVISIBLE);
        current_state = pc_cmd_state;
        changed_line(cursor_y, cursor_x, cursor_x);
      break;

      case GTERM_PC_CMD_MOVECURSOR :
//printf("pc_arg: movecursor (%d, %d)\n", pc_args[0], pc_args[1]);
        move_cursor(pc_args[0], pc_args[1]);
        current_state = pc_cmd_state;
      break;

      case GTERM_PC_CMD_PUTTEXT :
//printf("pc_arg: puttext got %d, %d, %d, %d\n", pc_args[0], pc_args[1], pc_args[2], pc_args[3]);
        pc_numdata = pc_args[2] * pc_args[3] * 2;
        pc_datacount = 0;
        pc_curx = pc_args[0];
        pc_cury = pc_args[1];
        if(pc_numdata)
          current_state = pc_data_state;
        else
          current_state = pc_cmd_state;
      break;

      case GTERM_PC_CMD_WRITE :
//printf("pc_arg: write got %d, %d, %d, %d\n", pc_args[0], pc_args[1], pc_args[2], pc_args[3]);
        pc_numdata = pc_args[2];
        pc_datacount = 0;
        pc_curx = pc_args[0];
        pc_cury = pc_args[1];
        if(pc_numdata)
          current_state = pc_data_state;
        else
          current_state = pc_cmd_state;
      break;

      case GTERM_PC_CMD_MOVETEXT :  // <sx> <sy> <wid> <len> <dx> <dy>
        if(pc_args[1] < pc_args[5])
        {
          for(i = 0; i < pc_args[3]; i++)
          {
            yp = linenumbers[pc_args[1] + i] * MAXWIDTH;
            yp2 = linenumbers[pc_args[5] + i] * MAXWIDTH;
            memmove(&text[yp2 + pc_args[4]], &text[yp + pc_args[0]], pc_args[2]);
            memmove(&color[yp2 + pc_args[4]], &color[yp + pc_args[0]], pc_args[2]);
            changed_line(pc_args[5] + i, pc_args[4], pc_args[4] + pc_args[2] - 1);
          }
        }
        else
        {
          for(i = pc_args[3] - 1; i >= 0; i--)
          {
            yp = linenumbers[pc_args[1] + i] * MAXWIDTH;
            yp2 = linenumbers[pc_args[5] + i] * MAXWIDTH;
            memmove(&text[yp2 + pc_args[4]], &text[yp + pc_args[0]], pc_args[2]);
            memmove(&color[yp2 + pc_args[4]], &color[yp + pc_args[0]], pc_args[2]);
            changed_line(pc_args[5] + i, pc_args[4], pc_args[4] + pc_args[2] - 1);
          }
        }
        current_state = pc_cmd_state;
      break;

      case GTERM_PC_CMD_SELECTPRINTER :
        pc_numdata = pc_args[0];
        pc_datacount = 0;
        memset(pc_printername, 0, sizeof(pc_printername));
        if(pc_numdata)
          current_state = pc_data_state;
        else
        {
          SelectPrinter("");
          current_state = pc_cmd_state;
        }
      break;

      case GTERM_PC_CMD_PRINTCHAR :
        PrintChars(1, &pc_args[0]);
        current_state = pc_cmd_state;
      break;

      case GTERM_PC_CMD_PRINTCHARS :
        pc_numdata = (pc_args[0] << 8) + pc_args[1];
        pc_datacount = 0;
        if(pc_numdata)
          current_state = pc_data_state;
        else
          current_state = pc_cmd_state;
      break;
    }
  }
}

void GTerm::pc_data(void)
{
  int
    yp;

//printf("pc_data: pc_curcmd = %d, pc_datacount = %d, pc_numdata = %d, pc_curx = %d, pc_cur_y = %d...\n", pc_curcmd, pc_datacount, pc_numdata, pc_curx, pc_cury);
  switch(pc_curcmd)
  {
    case GTERM_PC_CMD_PUTTEXT :
      yp = linenumbers[pc_cury] * MAXWIDTH;
      if(!(pc_datacount & 1))
      {
//printf("pc_data: got char %d\n", *input_data);
        text[yp + pc_curx] = *input_data;
      }
      else
      {
//printf("pc_data: got attr %d\n", *input_data);
        color[yp + pc_curx] = *input_data << 4;
      }
      if(pc_datacount & 1)
      {
        changed_line(pc_cury, pc_args[0], pc_curx);
        pc_curx++;
        if(pc_curx == pc_args[0] + pc_args[2])
        {
          pc_curx = pc_args[0];
          pc_cury++;
        }
      }
    break;

    case GTERM_PC_CMD_WRITE :
      yp = linenumbers[pc_cury] * MAXWIDTH;
      text[yp + pc_curx] = *input_data;
      color[yp + pc_curx] = (unsigned short)pc_args[3] << 4;
      changed_line(pc_cury, pc_args[0], pc_curx);
      pc_curx++;
    break;

    case GTERM_PC_CMD_SELECTPRINTER :
      if(pc_datacount < GTERM_PC_MAXPRINTERNAME - 1)
        pc_printername[pc_datacount] = *input_data;
      if(pc_datacount == pc_numdata - 1)
        SelectPrinter(pc_printername);
    break;

    case GTERM_PC_CMD_PRINTCHARS :
      PrintChars(1, input_data);
    break;
  }

  pc_datacount++;
  if(pc_datacount == pc_numdata)
    current_state = pc_cmd_state;
}

#endif // GTERM_PC
