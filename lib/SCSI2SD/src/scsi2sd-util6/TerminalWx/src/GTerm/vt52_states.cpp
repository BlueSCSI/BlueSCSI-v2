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

// I'm certain that this set is incomplete, but I got these from reverse-
// engineering a VT100 verification test program.


// state machine transition tables
StateOption GTerm::vt52_normal_state[] = {
    { 13, &GTerm::cr,      vt52_normal_state },
    { 10, &GTerm::lf,      vt52_normal_state },
    { 12, &GTerm::ff,      vt52_normal_state },
    { 9,  &GTerm::tab,     vt52_normal_state },
    { 8,  &GTerm::bs,      vt52_normal_state },
    { 7,  &GTerm::bell,        vt52_normal_state },
    { 27, &GTerm::clear_param, vt52_esc_state },
    { -1, &GTerm::normal_input,    vt52_normal_state } };

StateOption GTerm::vt52_esc_state[] = {
    { 'D', &GTerm::cursor_left,    vt52_normal_state },
    { 'B', &GTerm::cursor_down,    vt52_normal_state },
    { 'C', &GTerm::cursor_right,   vt52_normal_state },
    { 'A', &GTerm::cursor_up,  vt52_normal_state },
	{ 'Y', 0,		vt52_cursory_state },
    { 'J', &GTerm::erase_display,  vt52_normal_state },
    { 'K', &GTerm::erase_line, vt52_normal_state },
    { 'H', &GTerm::cursor_position,    vt52_normal_state },
    { 'I', &GTerm::index_up,   vt52_normal_state },
	{ 'F', 0,		vt52_normal_state }, // graphics mode
	{ 'G', 0,		vt52_normal_state }, // ASCII mode
    { 'Z', &GTerm::vt52_ident, vt52_normal_state }, // identify
	{ '<', 0,		normal_state },
	{ -1, 0, vt52_normal_state }};

StateOption GTerm::vt52_cursory_state[] = {
    { -1, &GTerm::vt52_cursory,    vt52_cursorx_state } };

StateOption GTerm::vt52_cursorx_state[] = {
    { -1, &GTerm::vt52_cursorx,    vt52_normal_state } };

