/*
TerminalWx - A wxWidgets terminal widget
Copyright (C) 1999  Derry Bryson
              2004  Mark Erikson
              2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/


#include "gterm.hpp"
#include <ctype.h>

//
//  Keycode translation tables
//
GTerm::VTKeySeq GTerm::cursor_keys[] =
{
  { KEY_LEFT, "\033[D" },
  { KEY_UP, "\033[A" },
  { KEY_RIGHT, "\033[C" },
  { KEY_DOWN, "\033[B" },
  { KEY_NUMPAD_LEFT, "\033[D" },
  { KEY_NUMPAD_UP, "\033[A" },
  { KEY_NUMPAD_RIGHT, "\033[C" },
  { KEY_NUMPAD_DOWN, "\033[B" },
  { KEY_NULL, 0 }
};

GTerm::VTKeySeq GTerm::cursor_app_keys[] =
{
  { KEY_LEFT, "\033OD" },
  { KEY_UP, "\033OA" },
  { KEY_RIGHT, "\033OC" },
  { KEY_DOWN, "\033OB" },
  { KEY_NUMPAD_LEFT, "\033OD" },
  { KEY_NUMPAD_UP, "\033OA" },
  { KEY_NUMPAD_RIGHT, "\033OC" },
  { KEY_NUMPAD_DOWN, "\033OB" },
  { KEY_NULL, 0 }
};

GTerm::VTKeySeq GTerm::keypad_keys[] =
{
  { KEY_NUMPAD_DIVIDE, "/" },
  { KEY_NUMPAD_MULTIPLY, "*" },
  { KEY_NUMPAD_SUBTRACT, "-" },
  { KEY_NUMPAD_HOME, "\033[\000" },
  { KEY_NUMPAD7, "7" },
  { KEY_NUMPAD8, "8" },
  { KEY_NUMPAD_PAGEUP, "\033[5~" },
  { KEY_NUMPAD9, "9" },
  { KEY_NUMPAD_ADD, "+" },
  { KEY_NUMPAD4, "4" },
  { KEY_NUMPAD_BEGIN, "\033[s" },
  { KEY_NUMPAD5, "5" },
  { KEY_NUMPAD6, "6" },
  { KEY_NUMPAD_END, "\033[e" },
  { KEY_NUMPAD1, "1" },
  { KEY_NUMPAD2, "2" },
  { KEY_NUMPAD_PAGEDOWN, "\033[6~" },
  { KEY_NUMPAD3, "3" },
  { KEY_NUMPAD_INSERT, "\033[2~" },
  { KEY_NUMPAD0, "0" },
  { KEY_NUMPAD_DELETE, "\033[3~" },
  { KEY_NUMPAD_DECIMAL, "." },
  { KEY_NUMPAD_ENTER, "\r" },
  { KEY_NULL, 0 }
};

GTerm::VTKeySeq GTerm::keypad_app_keys[] =
{
  { KEY_NUMPAD_DIVIDE, "\033Oo" },
  { KEY_NUMPAD_MULTIPLY, "\033Oj" },
  { KEY_NUMPAD_SUBTRACT, "\033Om" },
  { KEY_NUMPAD_HOME, "\033[\000" },
  { KEY_NUMPAD7, "\033Ow" },
  { KEY_NUMPAD8, "\033Ox" },
  { KEY_NUMPAD_PAGEUP, "\033[5~" },
  { KEY_NUMPAD9, "\033Oy" },
  { KEY_NUMPAD_ADD, "\033Ok" },
  { KEY_NUMPAD4, "\033Ot" },
  { KEY_NUMPAD_BEGIN, "\033[s" },
  { KEY_NUMPAD5, "\033Ou" },
  { KEY_NUMPAD6, "\033Ov" },
  { KEY_NUMPAD_END, "\033[e" },
  { KEY_NUMPAD1, "\033Oq" },
  { KEY_NUMPAD2, "\033Or" },
  { KEY_NUMPAD_PAGEDOWN, "\033[6~" },
  { KEY_NUMPAD3, "\033Os" },
  { KEY_NUMPAD_INSERT, "\033[2~" },
  { KEY_NUMPAD0, "\033Op" },
  { KEY_NUMPAD_DELETE, "\033[3~" },
  { KEY_NUMPAD_DECIMAL, "\033On" },
  { KEY_NUMPAD_ENTER, "\033OM" },
  { KEY_NULL, 0 }
};

GTerm::VTKeySeq GTerm::other_keys[] =
{
  { KEY_HOME, "\033[\000" },
  { KEY_PAGEUP, "\033[5~" },
  { KEY_PRIOR, "\033[5~" },
  { KEY_PAGEDOWN, "\033[6~"},
  { KEY_NEXT, "\033[6~" },
  { KEY_END, "\033[e" },
  { KEY_INSERT, "\033[2~" },
  { KEY_F1, "\033[11~" },
  { KEY_F2, "\033[12~" },
  { KEY_F3, "\033[13~" },
  { KEY_F4, "\033[14~" },
  { KEY_F5, "\033[15~" },
  { KEY_F6, "\033[17~" },
  { KEY_F7, "\033[18~" },
  { KEY_F8, "\033[19~" },
  { KEY_F9, "\033[20~" },
  { KEY_F10, "\033[21~" },
  { KEY_F11, "\033[23~" },
  { KEY_F12, "\033[24~" },
  { KEY_RETURN, "\r" },
  { KEY_SPACE, " " },
  { KEY_BACK, "\x8" },
  { KEY_ESCAPE, "\x1b" },
  { KEY_TAB, "\x9" },
  { KEY_DELETE, "\x7f" },
  { KEY_NULL, 0 }
};

#ifdef GTERM_PC
GTerm::PCKeySeq GTerm::pc_keys[] =
{
  { '0', 0x0030, 0x0029, 0x0000, 0x8100 },
  { '1', 0x0031, 0x0021, 0x0000, 0x7800 },
  { '2', 0x0032, 0x0040, 0x0000, 0x7900 },
  { '3', 0x0033, 0x0023, 0x0000, 0x7a00 },
  { '4', 0x0034, 0x0024, 0x0000, 0x7b00 },
  { '5', 0x0035, 0x0025, 0x0000, 0x7c00 },
  { '6', 0x0036, 0x005e, 0x0000, 0x7d00 },
  { '7', 0x0037, 0x0026, 0x0000, 0x7e00 },
  { '8', 0x0038, 0x002a, 0x0000, 0x7f00 },
  { '9', 0x0039, 0x0028, 0x0000, 0x8000 },

  { ')', 0x0030, 0x0029, 0x0000, 0x8100 },
  { '!', 0x0031, 0x0021, 0x0000, 0x7800 },
  { '@', 0x0032, 0x0040, 0x0000, 0x7900 },
  { '#', 0x0033, 0x0023, 0x0000, 0x7a00 },
  { '$', 0x0034, 0x0024, 0x0000, 0x7b00 },
  { '%', 0x0035, 0x0025, 0x0000, 0x7c00 },
  { '^', 0x0036, 0x005e, 0x0000, 0x7d00 },
  { '&', 0x0037, 0x0026, 0x0000, 0x7e00 },
  { '*', 0x0038, 0x002a, 0x0000, 0x7f00 },
  { '(', 0x0039, 0x0028, 0x0000, 0x8000 },

  { 'a', 0x0061, 0x0041, 0x0001, 0x1e00 },
  { 'b', 0x0062, 0x0042, 0x0002, 0x3000 },
  { 'c', 0x0063, 0x0043, 0x0003, 0x2e00 },
  { 'd', 0x0064, 0x0044, 0x0004, 0x2000 },
  { 'e', 0x0065, 0x0045, 0x0005, 0x1200 },
  { 'f', 0x0066, 0x0046, 0x0006, 0x2100 },
  { 'g', 0x0067, 0x0047, 0x0007, 0x2200 },
  { 'h', 0x0068, 0x0048, 0x0008, 0x2300 },
  { 'i', 0x0069, 0x0049, 0x0009, 0x1700 },
  { 'j', 0x006a, 0x004a, 0x000a, 0x2400 },
  { 'k', 0x006b, 0x004b, 0x000b, 0x2500 },
  { 'l', 0x006c, 0x004c, 0x000c, 0x2600 },
  { 'm', 0x006d, 0x004d, 0x000d, 0x3200 },
  { 'n', 0x006e, 0x004e, 0x000e, 0x3100 },
  { 'o', 0x006f, 0x004f, 0x000f, 0x1800 },
  { 'p', 0x0070, 0x0050, 0x0010, 0x1900 },
  { 'q', 0x0071, 0x0051, 0xff91, 0x1000 },
  { 'r', 0x0072, 0x0052, 0x0012, 0x1300 },
  { 's', 0x0073, 0x0053, 0xff93, 0x1f00 },
  { 't', 0x0074, 0x0054, 0x0014, 0x1400 },
  { 'u', 0x0075, 0x0055, 0x0015, 0x1600 },
  { 'v', 0x0076, 0x0056, 0x0016, 0x2f00 },
  { 'w', 0x0077, 0x0057, 0x0017, 0x1100 },
  { 'x', 0x0078, 0x0058, 0x0018, 0x2d00 },
  { 'y', 0x0079, 0x0059, 0x0019, 0x1500 },
  { 'z', 0x007a, 0x005a, 0x001a, 0x2c00 },

  { '`', 0x0060, 0x007e, 0x0000, 0x0000 },
  { '~', 0x0060, 0x007e, 0x0000, 0x0000 },
  { '-', 0x002d, 0x005f, 0x0000, 0x0000 },
  { '_', 0x002d, 0x005f, 0x0000, 0x0000 },
  { '=', 0x003d, 0x002b, 0x0000, 0x0000 },
  { '+', 0x003d, 0x002b, 0x0000, 0x0000 },
  { ',', 0x002c, 0x003c, 0x0000, 0x0000 },
  { '<', 0x002c, 0x003c, 0x0000, 0x0000 },
  { '.', 0x002e, 0x003e, 0x0000, 0x0000 },
  { '>', 0x002e, 0x003e, 0x0000, 0x0000 },
  { ';', 0x003b, 0x003a, 0x0000, 0x0000 },
  { ':', 0x003b, 0x003a, 0x0000, 0x0000 },
  { '\'', 0x002c, 0x0022, 0x0000, 0x0000 },
  { '"', 0x002c, 0x0022, 0x0000, 0x0000 },
  { '[', 0x005b, 0x007b, 0x0000, 0x0000 },
  { '{', 0x005b, 0x007b, 0x0000, 0x0000 },
  { ']', 0x005d, 0x007d, 0x0000, 0x0000 },
  { '}', 0x005d, 0x007d, 0x0000, 0x0000 },
  { '\\', 0x005c, 0x007c, 0x0000, 0x0000 },
  { '|', 0x005c, 0x007c, 0x0000, 0x0000 },
  { '/', 0x002f, 0x003f, 0x0000, 0x0000 },
  { '?', 0x002f, 0x003f, 0x0000, 0x0000 },

  { KEY_BACK, 0x0008, 0x0008, 0x007f, 0x0000 },
  { KEY_TAB, 0x0009, 0x0f00, 0x0000, 0x0000 },
  { KEY_RETURN, 0x000d, 0x000d, 0x000a, 0x0000 },
  { KEY_ESCAPE, 0x001b, 0x001b, 0x001b, 0x0000 },
  { KEY_SPACE, 0x0020, 0x0020, 0x0000, 0x0000 },
  { KEY_LEFT, 0x4b00, 0x0034, 0x7300, 0xb200 },
  { KEY_UP, 0x4800, 0x0038, 0xa000, 0xaf00 },
  { KEY_RIGHT, 0x4d00, 0x0036, 0x7400, 0xb400 },
  { KEY_DOWN, 0x5000, 0x0032, 0xa400, 0xb700 },
  { KEY_NUMPAD_LEFT, 0x4b00, 0x0034, 0x7300, 0xb200 },
  { KEY_NUMPAD_UP, 0x4800, 0x0038, 0xa000, 0xaf00 },
  { KEY_NUMPAD_RIGHT, 0x4d00, 0x0036, 0x7400, 0xb400 },
  { KEY_NUMPAD_DOWN, 0x5000, 0x0032, 0xa400, 0xb700 },
  { KEY_DIVIDE, 0x002f, 0x002f, 0x0000, 0x0000 },
  { KEY_MULTIPLY, 0x0038, 0x0038, 0x0000, 0x7f00 },
  { KEY_SUBTRACT, 0x002d, 0x002d, 0x0000, 0x0000 },
  { KEY_ADD, 0x003d, 0x003d, 0x0000, 0x0000 },
  { KEY_HOME, 0x4700, 0x0037, 0x7700, 0xae00 },
  { KEY_END, 0x4f00, 0x0031, 0x7500, 0xb600 },
  { KEY_PAGEUP, 0x4900, 0x0039, 0x8400, 0xb000 },
  { KEY_PAGEDOWN, 0x5100, 0x0033, 0x7600, 0xb800 },
  { KEY_INSERT, 0x5200, 0x0030, 0xa500, 0xb900 },
  { KEY_DELETE, 0x5300, 0x002e, 0xa600, 0xba00 },
  { KEY_ENTER, 0x000d, 0x000d, 0x000a, 0x0000 },
  { KEY_NEXT, 0x5100, 0x0033, 0x7600, 0xb800 },
  { KEY_PRIOR, 0x4900, 0x0039, 0x8400, 0xb000 },

  { KEY_NUMPAD0, 0x0030, 0x0029, 0x0000, 0x8100 },
  { KEY_NUMPAD1, 0x0031, 0x0021, 0x0000, 0x7800 },
  { KEY_NUMPAD2, 0x0032, 0x0040, 0x0000, 0x7900 },
  { KEY_NUMPAD3, 0x0033, 0x0023, 0x0000, 0x7a00 },
  { KEY_NUMPAD4, 0x0034, 0x0024, 0x0000, 0x7b00 },
  { KEY_NUMPAD5, 0x0035, 0x0025, 0x0000, 0x7c00 },
  { KEY_NUMPAD6, 0x0036, 0x005e, 0x0000, 0x7d00 },
  { KEY_NUMPAD7, 0x0037, 0x0026, 0x0000, 0x7e00 },
  { KEY_NUMPAD8, 0x0038, 0x002a, 0x0000, 0x7f00 },
  { KEY_NUMPAD9, 0x0039, 0x0028, 0x0000, 0x8000 },

  { KEY_F1, 0x3b00, 0x5400, 0x5e00, 0x6800 },
  { KEY_F2, 0x3c00, 0x5500, 0x5f00, 0x6900 },
  { KEY_F3, 0x3d00, 0x5600, 0x6000, 0x6a00 },
  { KEY_F4, 0x3e00, 0x5700, 0x6100, 0x6b00 },
  { KEY_F5, 0x3f00, 0x5800, 0x6200, 0x6c00 },
  { KEY_F6, 0x4000, 0x5900, 0x6300, 0x6d00 },
  { KEY_F7, 0x4100, 0x5a00, 0x6400, 0x6e00 },
  { KEY_F8, 0x4200, 0x5b00, 0x6500, 0x6f00 },
  { KEY_F9, 0x4300, 0x5c00, 0x6600, 0x7000 },
  { KEY_F10, 0x4400, 0x5d00, 0x6700, 0x7100 },
  { KEY_F11, 0x0000, 0x0000, 0x0000, 0x0000 },
  { KEY_F12, 0x0000, 0x0000, 0x0000, 0x0000 },
  { KEY_NULL, 0, 0, 0, 0 }
};
#endif // GTERM_PC

GTerm::VTKeySeq *
GTerm::translate_vt_keycode(int keyCode, VTKeySeq *table)
{
  while(table->keyCode != KEY_NULL)
  {
    if(table->keyCode == keyCode)
      return table;
    table++;
  }
  return 0;
}

#ifdef GTERM_PC
unsigned short
GTerm::translate_pc_keycode(int keyCode, int shift, int ctrl, int alt)
{
  int
    i;

  if(keyCode < 1000)
  {
    if(isupper(keyCode))
    {
      if(!alt)
        shift = 1;
      keyCode = tolower(keyCode);
    }

    if(iscntrl(keyCode))
    {
      ctrl = 1;
      keyCode = keyCode + 'a' - 1;
    }
  }

//printf("looking for %d...", keyCode);
  for(i = 0; pc_keys[i].keyCode; i++)
  {
    if(keyCode == pc_keys[i].keyCode)
    {
//printf("found!\n");
      if(shift)
        return pc_keys[i].shiftSeq;
      if(ctrl)
        return pc_keys[i].ctrlSeq;
      if(alt)
        return pc_keys[i].altSeq;
      return pc_keys[i].normSeq;
    }
  }
//printf("not found!\n");
  return 0;
}
#endif // GTERM_PC

int
GTerm::TranslateKeyCode(int keyCode, int *len, char *data, int shift,
                        int ctrl, int alt)
{
  int
    mode = GetMode();

#ifdef GTERM_PC
  unsigned short
    pcKeySeq;
#endif // GTERM_PC

  VTKeySeq
    *keySeq;

//printf("keycode = %d, shift = %d, ctrl = %d, alt = %d\n", keyCode, shift, ctrl, alt);
//printf("GTerm::TranslateKeyCode(): mode = %x\n", mode);
#ifdef GTERM_PC
  if(mode & PC)
  {
    pcKeySeq = translate_pc_keycode(keyCode, shift, ctrl, alt);
    if(pcKeySeq)
    {
//printf("keySeq = %x\n", pcKeySeq);
      *(unsigned char *)data++ = pcKeySeq >> 8;
      *(unsigned char *)data = pcKeySeq & 0xff;
      *len = 2;
      return 1;
    }
    return 0;
  }
  else
  {
#endif // GTERM_PC
//printf("mode = %x\n", mode);
    if(mode & KEYAPPMODE)
    {
//printf("KEYAPPMODE\n");
      keySeq = translate_vt_keycode(keyCode, keypad_app_keys);
    }
    else
      keySeq = translate_vt_keycode(keyCode, keypad_keys);

    if(!keySeq)
    {
      if(mode & CURSORAPPMODE)
      {
//printf("CURSORAPPMODE\n");
        keySeq = translate_vt_keycode(keyCode, cursor_app_keys);
//printf("keySeq = %08x\n", keySeq);
      }
      else
        keySeq = translate_vt_keycode(keyCode, cursor_keys);
    }

    if(!keySeq)
      keySeq = translate_vt_keycode(keyCode, other_keys);

    if(keySeq)
    {
      *len = strlen(keySeq->seq);
      strcpy(data, keySeq->seq);
      return 1;
    }
    return 0;
#ifdef GTERM_PC
  }
#endif // GTERM_PC
}
