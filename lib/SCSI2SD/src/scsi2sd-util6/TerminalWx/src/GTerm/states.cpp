/*
TerminalWx - A wxWidgets terminal widget
Copyright (C) 1999  Timothy Miller
              2004  Mark Erikson
              2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/


/* GRG: Added a lot of GTerm:: prefixes to correctly form pointers
 *   to functions in all the tables. Example: &cr -> &GTerm::cr
 */

#include "gterm.hpp"

// state machine transition tables
StateOption GTerm::normal_state[] = {
    { 13, &GTerm::cr,      normal_state },
    { 10, &GTerm::lf,      normal_state },
    { 12, &GTerm::ff,      normal_state },
    { 9,  &GTerm::tab,     normal_state },
    { 8,  &GTerm::bs,      normal_state },
    { 7,  &GTerm::bell,        normal_state },
	{ 27, 0,		esc_state },
    { -1, &GTerm::normal_input,    normal_state} };

StateOption GTerm::esc_state[] = {
    { '[', &GTerm::clear_param,    bracket_state },
    { '>', &GTerm::keypad_numeric, normal_state },
    { '=', &GTerm::keypad_application, normal_state },
    { '7', &GTerm::save_cursor,    normal_state },
    { '8', &GTerm::restore_cursor, normal_state },
    { 'H', &GTerm::set_tab,    normal_state },
    { 'D', &GTerm::index_down, normal_state },
    { 'M', &GTerm::index_up,   normal_state },
    { 'E', &GTerm::next_line,  normal_state },
    { 'c', &GTerm::reset,      normal_state },
	{ '(', 0,		cset_shiftin_state },
	{ ')', 0,		cset_shiftout_state },
	{ '#', 0,		hash_state },
#ifdef GTERM_PC
        { '!', &GTerm::pc_begin,       pc_cmd_state },
#endif

    { 13, &GTerm::cr,      esc_state },    // standard VT100 wants
    { 10, &GTerm::lf,      esc_state },    // cursor controls in
    { 12, &GTerm::ff,      esc_state },    // the middle of ESC
    { 9,  &GTerm::tab,     esc_state },    // sequences
    { 8,  &GTerm::bs,      esc_state },
    { 7,  &GTerm::bell,        esc_state },
	{ -1, 0, normal_state} };

// Should but cursor control characters in these groups as well.
// Maybe later.

StateOption GTerm::cset_shiftin_state[] = {
	{ 'A', 0,		normal_state },	// should set UK characters
	{ '0', 0,		normal_state },	// should set Business Gfx
	{ -1, 0,		normal_state },	// default to ASCII
	};

StateOption GTerm::cset_shiftout_state[] = {
	{ 'A', 0,		normal_state },	// should set UK characters
	{ '0', 0,		normal_state },	// should set Business Gfx
	{ -1, 0,		normal_state },	// default to ASCII
	};

StateOption GTerm::hash_state[] = {
    { '8', &GTerm::screen_align,   normal_state },
	{ -1, 0, normal_state} };

StateOption GTerm::bracket_state[] = {
    { '?', &GTerm::set_q_mode, bracket_state },
    { '"', &GTerm::set_quote_mode, bracket_state },
    { '0', &GTerm::param_digit,    bracket_state },
    { '1', &GTerm::param_digit,    bracket_state },
    { '2', &GTerm::param_digit,    bracket_state },
    { '3', &GTerm::param_digit,    bracket_state },
    { '4', &GTerm::param_digit,    bracket_state },
    { '5', &GTerm::param_digit,    bracket_state },
    { '6', &GTerm::param_digit,    bracket_state },
    { '7', &GTerm::param_digit,    bracket_state },
    { '8', &GTerm::param_digit,    bracket_state },
    { '9', &GTerm::param_digit,    bracket_state },
    { ';', &GTerm::next_param, bracket_state },
    { 'D', &GTerm::cursor_left,    normal_state },
    { 'B', &GTerm::cursor_down,    normal_state },
    { 'C', &GTerm::cursor_right,   normal_state },
    { 'A', &GTerm::cursor_up,  normal_state },
    { 'H', &GTerm::cursor_position,    normal_state },
    { 'f', &GTerm::cursor_position,    normal_state },
    { 'c', &GTerm::device_attrib,  normal_state },
    { 'P', &GTerm::delete_char,    normal_state },
    { 'h', &GTerm::set_mode,   normal_state },
    { 'l', &GTerm::clear_mode, normal_state },
    { 's', &GTerm::save_cursor,    normal_state },
    { 'u', &GTerm::restore_cursor, normal_state },
    { 'x', &GTerm::request_param,  normal_state },
    { 'r', &GTerm::set_margins,    normal_state },
    { 'M', &GTerm::delete_line,    normal_state },
    { 'n', &GTerm::status_report,  normal_state },
    { 'J', &GTerm::erase_display,  normal_state },
    { 'K', &GTerm::erase_line, normal_state },
    { 'L', &GTerm::insert_line,    normal_state },
    { 'm', &GTerm::set_colors, normal_state },
    { 'g', &GTerm::clear_tab,  normal_state },
    { '@', &GTerm::insert_char,    normal_state },
    { 'X', &GTerm::erase_char, normal_state },
	{ 'p', 0, 		normal_state },	// something to do with levels

    { 13, &GTerm::cr,      bracket_state },// standard VT100 wants
    { 10, &GTerm::lf,      bracket_state },// cursor controls in
    { 12, &GTerm::ff,      bracket_state },// the middle of ESC
    { 9,  &GTerm::tab,     bracket_state },// sequences
    { 8,  &GTerm::bs,      bracket_state },
    { 7,  &GTerm::bell,        bracket_state },
	{ -1, 0, normal_state} };

#ifdef GTERM_PC

StateOption GTerm::pc_cmd_state[] =
{
        { GTERM_PC_CMD_CURONOFF,        &GTerm::pc_cmd,     pc_arg_state },
        { GTERM_PC_CMD_MOVECURSOR,      &GTerm::pc_cmd,     pc_arg_state },
        { GTERM_PC_CMD_PUTTEXT,         &GTerm::pc_cmd,     pc_arg_state },
        { GTERM_PC_CMD_WRITE,           &GTerm::pc_cmd,     pc_arg_state },
        { GTERM_PC_CMD_MOVETEXT,        &GTerm::pc_cmd,     pc_arg_state },
        { GTERM_PC_CMD_EXIT,            &GTerm::pc_end,     normal_state },
        { GTERM_PC_CMD_BEEP,            &GTerm::pc_cmd,     pc_cmd_state },
        { GTERM_PC_CMD_SELECTPRINTER,   &GTerm::pc_cmd,     pc_arg_state },
        { GTERM_PC_CMD_PRINTCHAR,       &GTerm::pc_cmd,     pc_arg_state },
        { GTERM_PC_CMD_PRINTCHARS,      &GTerm::pc_cmd,     pc_arg_state },
        { -1,                           &GTerm::pc_cmd,     pc_cmd_state }
};

StateOption GTerm::pc_arg_state[] =
{
        { -1,                           &GTerm::pc_arg,     pc_arg_state }
};

StateOption GTerm::pc_data_state[] =
{
        { -1,                           &GTerm::pc_data,    pc_data_state }
};

#endif // GTERM_PC
