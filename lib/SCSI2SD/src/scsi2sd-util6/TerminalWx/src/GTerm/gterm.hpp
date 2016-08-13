/*
TerminalWx - A wxWidgets terminal widget
Copyright (C) 1999  Timothy Miller
              2004  Mark Erikson
              2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/

#ifndef INCLUDED_GTERM_H
#define INCLUDED_GTERM_H

#ifdef __GNUG__
#pragma interface
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAXWIDTH 200
#define MAXHEIGHT 100


#define GTERM_PC


class GTerm;
typedef void (GTerm::*StateFunc)();

struct StateOption {
	int byte;	// char value to look for; -1==end/default
	StateFunc action;
	StateOption *next_state;
};

class GTerm {
public:
	// mode flags
	enum
        {
          BOLD=0x1,
          BLINK=0x2,
          UNDERLINE=0x4,
          INVERSE=0x8,
	  NOEOLWRAP=0x10,
          CURSORAPPMODE=0x20,
          CURSORRELATIVE=0x40,
  	  NEWLINE=0x80,
          INSERT=0x100,
          KEYAPPMODE=0x200,
	  DEFERUPDATE=0x400,
          DESTRUCTBS=0x800,
          TEXTONLY=0x1000,
	  LOCALECHO=0x2000,
          CURSORINVISIBLE=0x4000,
#ifdef GTERM_PC
//          ,PC=0x10000
          PC=0x8000,
#endif // GTERM_PC
          SELECTED=0x8000	// flag to indicate a char is selected
        } MODES;

private:
	// terminal info
	int width, height, scroll_top, scroll_bot;
	unsigned char *text;
	unsigned short *color;
	short linenumbers[MAXHEIGHT]; // text at text[linenumbers[y]*MAXWIDTH]
	unsigned char dirty_startx[MAXHEIGHT], dirty_endx[MAXHEIGHT];
	int pending_scroll; // >0 means scroll up
	int doing_update;

	// terminal state
	int cursor_x, cursor_y;
	int save_x, save_y, save_attrib;
	int fg_color, bg_color;
	int mode_flags;
	char tab_stops[MAXWIDTH];
	StateOption *current_state;
	static StateOption normal_state[], esc_state[], bracket_state[];
	static StateOption cset_shiftin_state[], cset_shiftout_state[];
	static StateOption hash_state[];
	static StateOption vt52_normal_state[], vt52_esc_state[];
	static StateOption vt52_cursory_state[], vt52_cursorx_state[];

public:
        // keycode translation
        enum
        {
          KEY_NULL = 0,
          KEY_BACK = 1000,
          KEY_TAB,
          KEY_RETURN,
          KEY_ESCAPE,
          KEY_SPACE,
          KEY_LEFT,
          KEY_UP,
          KEY_RIGHT,
          KEY_DOWN,
          KEY_NUMPAD_LEFT,
          KEY_NUMPAD_UP,
          KEY_NUMPAD_RIGHT,
          KEY_NUMPAD_DOWN,
          KEY_DIVIDE,
          KEY_NUMPAD_DIVIDE,
          KEY_MULTIPLY,
          KEY_NUMPAD_MULTIPLY,
          KEY_SUBTRACT,
          KEY_NUMPAD_SUBTRACT,
          KEY_ADD,
          KEY_NUMPAD_ADD,
          KEY_HOME,
          KEY_NUMPAD_HOME,
          KEY_END,
          KEY_NUMPAD_END,
          KEY_PAGEUP,
          KEY_NUMPAD_PAGEUP,
          KEY_PAGEDOWN,
          KEY_NUMPAD_PAGEDOWN,
          KEY_INSERT,
          KEY_NUMPAD_INSERT,
          KEY_DELETE,
          KEY_NUMPAD_DELETE,
          KEY_ENTER,
          KEY_NUMPAD_ENTER,
          KEY_NEXT,
          KEY_PRIOR,
          KEY_NUMPAD0,
          KEY_NUMPAD1,
          KEY_NUMPAD2,
          KEY_NUMPAD3,
          KEY_NUMPAD4,
          KEY_NUMPAD5,
          KEY_NUMPAD6,
          KEY_NUMPAD7,
          KEY_NUMPAD8,
          KEY_NUMPAD9,
          KEY_NUMPAD_BEGIN,
          KEY_NUMPAD_DECIMAL,
          KEY_F1,
          KEY_F2,
          KEY_F3,
          KEY_F4,
          KEY_F5,
          KEY_F6,
          KEY_F7,
          KEY_F8,
          KEY_F9,
          KEY_F10,
          KEY_F11,
          KEY_F12
        } KEYCODE;

private:
        typedef struct
        {
          int
            keyCode;

          char
            *seq;
        } VTKeySeq;

        static VTKeySeq cursor_keys[];
        static VTKeySeq cursor_app_keys[];
        static VTKeySeq keypad_keys[];
        static VTKeySeq keypad_app_keys[];
        static VTKeySeq other_keys[];

#ifdef GTERM_PC
        typedef struct
        {
          int
            keyCode;

          unsigned short
            normSeq,
            shiftSeq,
            ctrlSeq,
            altSeq;
        } PCKeySeq;

        static PCKeySeq pc_keys[];
#endif // GTERM_PC

        VTKeySeq *translate_vt_keycode(int keyCode, VTKeySeq *table);
#ifdef GTERM_PC
        unsigned short translate_pc_keycode(int KeyCode, int shift, int ctrl, int alt);
#endif // GTERM_PC

#ifdef GTERM_PC

#define GTERM_PC_MAXARGS             10

#define GTERM_PC_CMD_EXIT            0     // end console mode
#define GTERM_PC_CMD_CURONOFF        1     // cursor on/off
#define GTERM_PC_CMD_MOVECURSOR      2     // move cursor to x,y
#define GTERM_PC_CMD_PUTTEXT         3     // put text
#define GTERM_PC_CMD_WRITE           4     // write text
#define GTERM_PC_CMD_MOVETEXT        5     // move text
#define GTERM_PC_CMD_BEEP            6     // beep
#define GTERM_PC_CMD_SELECTPRINTER   7     // select printer
#define GTERM_PC_CMD_PRINTCHAR       8     // print a single character
#define GTERM_PC_CMD_PRINTCHARS      9     // prints multiple characters

#define GTERM_PC_MAXPRINTERNAME      100

        int pc_cury;
        int pc_curx;
        int pc_state;
        int pc_curcmd;
        int pc_numargs;
        int pc_argcount;
        int pc_numdata;
        int pc_datacount;
        int pc_oldWidth;
        int pc_oldHeight;
        unsigned char pc_args[GTERM_PC_MAXARGS];
        char *pc_machinename;
        char pc_printername[GTERM_PC_MAXPRINTERNAME];

//
//  Define state tables
//
        static StateOption pc_cmd_state[];
        static StateOption pc_arg_state[];
        static StateOption pc_data_state[];

//
//  Define actions
//
        void pc_begin(void);
        void pc_end(void);
        void pc_cmd(void);
        void pc_arg(void);
        void pc_data(void);

#endif  // GTERM_PC

	// utility functions
	void update_changes();
	void scroll_region(int start_y, int end_y, int num);	// does clear
	void shift_text(int y, int start_x, int end_x, int num); // ditto
	void clear_area(int start_x, int start_y, int end_x, int end_y);
	void changed_line(int y, int start_x, int end_x);
	void move_cursor(int x, int y);
	int calc_color(int fg, int bg, int flags);

	// action parameters
	int nparam, param[30];
	unsigned char *input_data;
	int data_len, q_mode, got_param, quote_mode;

	// terminal actions
	void normal_input();
	void set_q_mode();
	void set_quote_mode();
	void clear_param();
	void param_digit();
	void next_param();

	// non-printing characters
	void cr(), lf(), ff(), bell(), tab(), bs();

	// escape sequence actions
	void keypad_numeric();
	void keypad_application();
	void save_cursor();
	void restore_cursor();
	void set_tab();
	void index_down();
	void index_up();
	void next_line();
	void reset();

	void cursor_left();
	void cursor_down();
	void cursor_right();
	void cursor_up();
	void cursor_position();
	void device_attrib();
	void delete_char();
	void set_mode();
	void clear_mode();
	void request_param();
	void set_margins();
	void delete_line();
	void status_report();
	void erase_display();
	void erase_line();
	void insert_line();
	void set_colors();
	void clear_tab();
	void insert_char();
	void screen_align();
	void erase_char();

	// vt52 stuff
	void vt52_cursory();
	void vt52_cursorx();
	void vt52_ident();

public:
	GTerm(int w, int h);
	virtual ~GTerm();

	// function to control terminal
	virtual void ProcessInput(int len, unsigned char *data);
   virtual void ProcessOutput(int len, unsigned char *data) { SendBack(len, (char *)data); }
	virtual void ResizeTerminal(int width, int height);
	int Width() { return width; }
	int Height() { return height; }
	virtual void Update();
	virtual void ExposeArea(int x, int y, int w, int h);
	virtual void Reset();

	int GetMode() { return mode_flags; }
	void SetMode(int mode) { mode_flags = mode; }
	void set_mode_flag(int flag);
	void clear_mode_flag(int flag);

	// manditory child-supplied functions
	virtual void DrawText(int fg_color, int bg_color, int flags,
		int x, int y, int len, unsigned char *string) = 0;
	virtual void DrawCursor(int fg_color, int bg_color, int flags,
		int x, int y, unsigned char c) = 0;

	// optional child-supplied functions
	virtual void MoveChars(int sx, int sy, int dx, int dy, int w, int h) { }
	virtual void ClearChars(int bg_color, int x, int y, int w, int h) { }
   virtual void SendBack(int len, char *data) { }
	virtual void SendBack(char *data) { SendBack(strlen(data), data); }
	virtual void ModeChange(int state) { }
	virtual void Bell() { }
	virtual void RequestSizeChange(int w, int h) { }

        virtual int TranslateKeyCode(int keycode, int *len, char *data,
                                     int shift = 0, int ctrl = 0, int alt = 0);

#ifdef GTERM_PC
        virtual void SelectPrinter(char *PrinterName) {}
        virtual void PrintChars(int len, unsigned char *data) {}

        void SetMachineName(char *machinename);
        char *GetMachineName(void) { return pc_machinename; }
#endif // GTERM_PC

        virtual int IsSelected(int x, int y);
        virtual void Select(int x, int y, int select);
        virtual unsigned char GetChar(int x, int y);
};

#endif
