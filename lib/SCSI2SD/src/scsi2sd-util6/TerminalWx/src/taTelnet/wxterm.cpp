/*
    taTelnet - A cross-platform telnet program.
    Copyright (c) 2000 Derry Bryson
                  2004 Mark Erikson
                  2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/

#ifdef __GNUG__
    #pragma implementation "wxterm.h"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include <wx/timer.h>
#include <wx/event.h>
#include <wx/bitmap.h>
#include <wx/brush.h>
#include <wx/cursor.h>
#include <wx/dcclient.h>
#include <wx/log.h>
#include <wx/pen.h>
#include <wx/settings.h>
#include <wx/utils.h>

#include <ctype.h>

#include "../GTerm/gterm.hpp"
#include "wxterm.h"



#define CURSOR_BLINK_DEFAULT_TIMEOUT	300
#define CURSOR_BLINK_MAX_TIMEOUT	2000

/*
**  Keycode translation tables
*/
wxTerm::TermKeyMap wxTerm::keyMapTable[] =
{
  { WXK_BACK, GTerm::KEY_BACK },
  { WXK_TAB, GTerm::KEY_TAB },
  { WXK_RETURN, GTerm::KEY_RETURN },
  { WXK_ESCAPE, GTerm::KEY_ESCAPE },
  { WXK_SPACE, GTerm::KEY_SPACE },
  { WXK_LEFT, GTerm::KEY_LEFT },
  { WXK_UP, GTerm::KEY_UP },
  { WXK_RIGHT, GTerm::KEY_RIGHT },
  { WXK_DOWN, GTerm::KEY_DOWN },
//  { WXK_DIVIDE, GTerm::KEY_DIVIDE },
//  { WXK_MULTIPLY, GTerm::KEY_MULTIPLY },
//  { WXK_SUBTRACT, GTerm::KEY_SUBTRACT },
//  { WXK_ADD, GTerm::KEY_ADD },
  { WXK_HOME, GTerm::KEY_HOME },
  { WXK_END, GTerm::KEY_END },
  { WXK_PAGEUP, GTerm::KEY_PAGEUP },
  { WXK_PAGEDOWN, GTerm::KEY_PAGEDOWN },
  { WXK_INSERT, GTerm::KEY_INSERT },
  { WXK_DELETE, GTerm::KEY_DELETE },
//  { WXK_NEXT, GTerm::KEY_NEXT },
//  { WXK_PRIOR, GTerm::KEY_PRIOR },
//  { WXK_NUMPAD0, GTerm::KEY_NUMPAD0 },
//  { WXK_NUMPAD1, GTerm::KEY_NUMPAD1 },
//  { WXK_NUMPAD2, GTerm::KEY_NUMPAD2 },
//  { WXK_NUMPAD3, GTerm::KEY_NUMPAD3 },
//  { WXK_NUMPAD4, GTerm::KEY_NUMPAD4 },
//  { WXK_NUMPAD5, GTerm::KEY_NUMPAD5 },
//  { WXK_NUMPAD6, GTerm::KEY_NUMPAD6 },
//  { WXK_NUMPAD7, GTerm::KEY_NUMPAD7 },
//  { WXK_NUMPAD8, GTerm::KEY_NUMPAD8 },
//  { WXK_NUMPAD9, GTerm::KEY_NUMPAD9 },
//  { WXK_DECIMAL, GTerm::KEY_NUMPAD_DECIMAL },
  { WXK_F1, GTerm::KEY_F1 },
  { WXK_F2, GTerm::KEY_F2 },
  { WXK_F3, GTerm::KEY_F3 },
  { WXK_F4, GTerm::KEY_F4 },
  { WXK_F5, GTerm::KEY_F5 },
  { WXK_F6, GTerm::KEY_F6 },
  { WXK_F7, GTerm::KEY_F7 },
  { WXK_F8, GTerm::KEY_F8 },
  { WXK_F9, GTerm::KEY_F9 },
  { WXK_F10, GTerm::KEY_F10 },
  { WXK_F11, GTerm::KEY_F11 },
  { WXK_F12, GTerm::KEY_F12 },
  { (wxKeyCode)0, GTerm::KEY_NULL }
};

static unsigned char
  xCharMap[] =
  {
    0, // 0
    1, // 1
    2, // 2
    3, // 3
    1, // 4
    5, // 5
    6, // 6
    7, // 7
    8, // 8
    9, // 9
    10, // 10
    11, // 11
    12, // 12
    13, // 13
    14, // 14
    15, // 15
    62, // 16
    60, // 17
    18, // 18
    19, // 19
    20, // 20
    21, // 21
    22, // 22
    23, // 23
    24, // 24
    25, // 25
    26, // 26
    27, // 27
    28, // 28
    29, // 29
    94, // 30
    31, // 31
    32, // 32
    33, // 33
    34, // 34
    35, // 35
    36, // 36
    37, // 37
    38, // 38
    39, // 39
    40, // 40
    41, // 41
    42, // 42
    43, // 43
    44, // 44
    45, // 45
    46, // 46
    47, // 47
    48, // 48
    49, // 49
    50, // 50
    51, // 51
    52, // 52
    53, // 53
    54, // 54
    55, // 55
    56, // 56
    57, // 57
    58, // 58
    59, // 59
    60, // 60
    61, // 61
    62, // 62
    63, // 63
    64, // 64
    65, // 65
    66, // 66
    67, // 67
    68, // 68
    69, // 69
    70, // 70
    71, // 71
    72, // 72
    73, // 73
    74, // 74
    75, // 75
    76, // 76
    77, // 77
    78, // 78
    79, // 79
    80, // 80
    81, // 81
    82, // 82
    83, // 83
    84, // 84
    85, // 85
    86, // 86
    87, // 87
    88, // 88
    89, // 89
    90, // 90
    91, // 91
    92, // 92
    93, // 93
    94, // 94
    95, // 95
    96, // 96
    97, // 97
    98, // 98
    99, // 99
    100, // 100
    101, // 101
    102, // 102
    103, // 103
    104, // 104
    105, // 105
    106, // 106
    107, // 107
    108, // 108
    109, // 109
    110, // 110
    111, // 111
    112, // 112
    113, // 113
    114, // 114
    115, // 115
    116, // 116
    117, // 117
    118, // 118
    119, // 119
    120, // 120
    121, // 121
    122, // 122
    123, // 123
    124, // 124
    125, // 125
    126, // 126
    127, // 127
    128, // 128
    129, // 129
    130, // 130
    131, // 131
    132, // 132
    133, // 133
    134, // 134
    135, // 135
    136, // 136
    137, // 137
    138, // 138
    139, // 139
    140, // 140
    141, // 141
    142, // 142
    143, // 143
    144, // 144
    145, // 145
    146, // 146
    147, // 147
    148, // 148
    149, // 149
    150, // 150
    151, // 151
    152, // 152
    153, // 153
    154, // 154
    155, // 155
    156, // 156
    157, // 157
    158, // 158
    159, // 159
    160, // 160
    161, // 161
    162, // 162
    163, // 163
    164, // 164
    165, // 165
    166, // 166
    167, // 167
    168, // 168
    169, // 169
    170, // 170
    171, // 171
    172, // 172
    173, // 173
    174, // 174
    175, // 175
    2, // 176
    2, // 177
    2, // 178
    25, // 179
    22, // 180
    22, // 181
    22, // 182
    12, // 183
    12, // 184
    22, // 185
    25, // 186
    12, // 187
    11, // 188
    11, // 189
    11, // 190
    12, // 191
    14, // 192
    23, // 193
    24, // 194
    21, // 195
    18, // 196
    15, // 197
    21, // 198
    21, // 199
    14, // 200
    13, // 201
    23, // 202
    24, // 203
    21, // 204
    18, // 205
    15, // 206
    23, // 207
    23, // 208
    24, // 209
    24, // 210
    14, // 211
    14, // 212
    13, // 213
    13, // 214
    15, // 215
    15, // 216
    11, // 217
    13, // 218
    0, // 219
    220, // 220
    221, // 221
    222, // 222
    223, // 223
    224, // 224
    225, // 225
    226, // 226
    227, // 227
    228, // 228
    229, // 229
    230, // 230
    231, // 231
    232, // 232
    233, // 233
    234, // 234
    235, // 235
    236, // 236
    237, // 237
    238, // 238
    239, // 239
    240, // 240
    241, // 241
    242, // 242
    243, // 243
    244, // 244
    245, // 245
    246, // 246
    247, // 247
    248, // 248
    249, // 249
    250, // 250
    251, // 251
    252, // 252
    253, // 253
    254, // 254
    255  // 255
  };

BEGIN_EVENT_TABLE(wxTerm, wxWindow)
  EVT_PAINT						(wxTerm::OnPaint)
  EVT_CHAR						(wxTerm::OnChar)
  EVT_LEFT_DOWN					(wxTerm::OnLeftDown)
  EVT_LEFT_UP					(wxTerm::OnLeftUp)
  EVT_MOTION					(wxTerm::OnMouseMove)
  EVT_TIMER						(-1, wxTerm::OnTimer)
#if 0
  EVT_KEY_DOWN(wxTerm::OnKeyDown)
#endif

  EVT_SIZE						(wxTerm::OnSize)
  EVT_SET_FOCUS					(wxTerm::OnGainFocus)
  EVT_KILL_FOCUS				(wxTerm::OnLoseFocus)
END_EVENT_TABLE()

wxTerm::wxTerm(wxWindow* parent, wxWindowID id,
               const wxPoint& pos,
               int width, int height,
               const wxString& name) :
  wxWindow(parent, id, pos, wxSize(-1, -1), wxWANTS_CHARS, name),
  GTerm(width, height)
{
  int
    i;

  m_inUpdateSize = false;
  m_init = 1;

  m_curDC = 0;

  m_charsInLine = width;
  m_linesDisplayed = height;

  m_selecting = FALSE;
  m_selx1 = m_sely1 = m_selx2 = m_sely2 = 0;
  m_marking = FALSE;
  m_curX = -1;
  m_curY = -1;
  m_curBlinkRate = CURSOR_BLINK_DEFAULT_TIMEOUT;
  m_timer.SetOwner(this);
  if(m_curBlinkRate)
    m_timer.Start(m_curBlinkRate);

  m_boldStyle = COLOR;

  GetDefVTColors(m_vt_colors);
  GetDefPCColors(m_pc_colors);

  m_colors = m_vt_colors;

  SetBackgroundColour(m_colors[0]);

  for(i = 0; i < 16; i++)
    m_vt_colorPens[i] = wxPen(m_vt_colors[i], 1, wxSOLID);

  for(i = 0; i < 16; i++)
    m_pc_colorPens[i] = wxPen(m_pc_colors[i], 1, wxSOLID);

  m_colorPens = m_vt_colorPens;

  m_width = width;
  m_height = height;

  m_printerFN = 0;
  m_printerName = 0;



  m_normalFont = GetFont();
  m_underlinedFont = GetFont();
  m_underlinedFont.SetUnderlined(TRUE);
  m_boldFont = GetFont();
  m_boldFont.SetWeight(wxBOLD);
  m_boldUnderlinedFont = m_boldFont;
  m_boldUnderlinedFont.SetUnderlined(TRUE);

  m_bitmap = 0;


  //ResizeTerminal(width, height);


  //SetVirtualSize(m_charWidth * 80, m_charHeight * 100);
  //SetScrollRate(m_charWidth, m_charHeight);

  m_init = 0;

  SetCursor(wxCursor(wxCURSOR_IBEAM));

  wxFont monospacedFont(10, wxMODERN, wxNORMAL, wxNORMAL, false, "Courier New");

  SetFont(monospacedFont);

  // 10pt Courier New is 8 pixels wide and 16 pixels high... set up
  // a default client size to match
  SetClientSize(m_charsInLine * 8, m_linesDisplayed * 16);
  UpdateSize();
}

wxTerm::~wxTerm()
{
  if(m_bitmap)
  {
    m_memDC.SelectObject(wxNullBitmap);
    delete m_bitmap;
  }
}

//////////////////////////////////////////////////////////////////////////////
///  public SetBoldStyle
///  Sets the bold style for the terminal
///
///  @param  boldStyle wxTerm::BOLDSTYLE & The style to be used
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::SetBoldStyle(wxTerm::BOLDSTYLE boldStyle)
{
//  wxColour colors[16];

  if(boldStyle == DEFAULT)
    boldStyle = COLOR;

  m_boldStyle = boldStyle;
//  GetDefVTColors(colors, m_boldStyle);
//  SetVTColors(colors);
  Refresh();
}

//////////////////////////////////////////////////////////////////////////////
///  public SetFont
///  Sets the font for the terminal
///
///  @param  font const wxFont & The font to be used
///
///  @return bool Unused (returns true)
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
bool
wxTerm::SetFont(const wxFont& font)
{
  m_init = 1;

  wxWindow::SetFont(font);
  m_normalFont = font;
  m_underlinedFont = font;
  m_underlinedFont.SetUnderlined(TRUE);
  m_boldFont = GetFont();
  m_boldFont.SetWeight(wxBOLD);
  m_boldUnderlinedFont = m_boldFont;
  m_boldUnderlinedFont.SetUnderlined(TRUE);
  m_init = 0;

  ResizeTerminal(m_width, m_height);
  Refresh();

  return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
///  public GetDfVTColors
///  Gets the colors for a VT100 terminal
///
///  @param  colors wxColour [] The colors that need to be assigned to
///  @param  boldStyle wxTerm::BOLDSTYLE The bold style used in the terminal
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::GetDefVTColors(wxColour colors[16], wxTerm::BOLDSTYLE boldStyle)
{
  if(boldStyle == DEFAULT)
    boldStyle = m_boldStyle;

  if(boldStyle != COLOR)
  {
    colors[0] = wxColour(0, 0, 0);                             // black
    colors[1] = wxColour(255, 0, 0);                           // red
    colors[2] = wxColour(0, 255, 0);                           // green
    colors[3] = wxColour(255, 0, 255);                         // yellow
    colors[4] = wxColour(0, 0, 255);                           // blue
    colors[5] = wxColour(255, 255, 0);                         // magenta
    colors[6] = wxColour(0, 255, 255);                         // cyan
    colors[7] = wxColour(255, 255, 255);                       // white
    colors[8] = wxColour(0, 0, 0);                             // black
    colors[9] = wxColour(255, 0, 0);                           // red
    colors[10] = wxColour(0, 255, 0);                          // green
    colors[11] = wxColour(255, 0, 255);                        // yellow
    colors[12] = wxColour(0, 0, 255);                          // blue
    colors[13] = wxColour(255, 255, 0);                        // magenta
    colors[14] = wxColour(0, 255, 255);                        // cyan
    colors[15] = wxColour(255, 255, 255);                      // white
  }
  else
  {
    colors[0] = wxColour(0, 0, 0);                             // black
    colors[1] = wxColour(170, 0, 0);                           // red
    colors[2] = wxColour(0, 170, 0);                           // green
    colors[3] = wxColour(170, 0, 170);                         // yellow
    colors[4] = wxColour(0, 0, 170);                           // blue
    colors[5] = wxColour(170, 170, 0);                         // magenta
    colors[6] = wxColour(0, 170, 170);                         // cyan
    colors[7] = wxColour(192, 192, 192);                       // white
//    colors[7] = wxColour(170, 170, 170);                       // white
#if 0
    colors[8] = wxColour(85, 85, 85);                          // bold black
    colors[9] = wxColour(255, 85, 85);                         // bold red
    colors[10] = wxColour(85, 255, 85);                        // bold green
    colors[11] = wxColour(255, 85, 255);                       // bold yellow
    colors[12] = wxColour(85, 85, 255);                        // bold blue
    colors[13] = wxColour(255, 255, 85);                       // bold magenta
    colors[14] = wxColour(85, 255, 255);                       // bold cyan
    colors[15] = wxColour(255, 255, 255);                      // bold white
#else
    colors[8] = wxColour(85, 85, 85);                          // bold black
    colors[9] = wxColour(255, 0, 0);                         // bold red
    colors[10] = wxColour(0, 255, 0);                        // bold green
    colors[11] = wxColour(255, 0, 255);                       // bold yellow
    colors[12] = wxColour(0, 0, 255);                        // bold blue
    colors[13] = wxColour(255, 255, 0);                       // bold magenta
    colors[14] = wxColour(0, 255, 255);                       // bold cyan
    colors[15] = wxColour(255, 255, 255);                      // bold white
#endif
  }
}

//////////////////////////////////////////////////////////////////////////////
///  public GetVTColors
///  Retrieves a copy of the VT100 colors
///
///  @param  colors wxColour [] An array to be filled with the VT100 colors
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::GetVTColors(wxColour colors[16])
{
  int
    i;

  for(i = 0; i < 16; i++)
    colors[i] = m_vt_colors[i];
}

//////////////////////////////////////////////////////////////////////////////
///  public SetVTColors
///  Sets the VT100 colors
///
///  @param  colors wxColour [] The new colors to be used
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::SetVTColors(wxColour colors[16])
{
  int
    i;

  m_init = 1;
  for(i = 0; i < 16; i++)
    m_vt_colors[i] = colors[i];

  if(!(GetMode() & PC))
    SetBackgroundColour(m_vt_colors[0]);

  for(i = 0; i < 16; i++)
    m_vt_colorPens[i] = wxPen(m_vt_colors[i], 1, wxSOLID);
  m_init = 0;

  Refresh();
}

//////////////////////////////////////////////////////////////////////////////
///  public GetDefPCColors
///  Gets the default PC colors
///
///  @param  colors wxColour [] Filled with the colors to be used
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::GetDefPCColors(wxColour colors[16])
{
#if 0
  /*
  **  These colors need tweaking.  I'm sure they are not correct.
  */
  colors[0] = wxColour(0, 0, 0);                             // black
  colors[1] = wxColour(0, 0, 128);                           // blue
  colors[2] = wxColour(0, 128, 0);                           // green
  colors[3] = wxColour(0, 128, 128);                         // cyan
  colors[4] = wxColour(128, 0, 0);                           // red
  colors[5] = wxColour(128, 0, 128);                         // magenta
  colors[6] = wxColour(128, 128, 0);                         // brown
  colors[7] = wxColour(128, 128, 128);                       // white
  colors[8] = wxColour(64, 64, 64);                          // gray
  colors[9] = wxColour(0, 0, 255);                           // lt blue
  colors[10] = wxColour(0, 255, 0);                          // lt green
  colors[11] = wxColour(0, 255, 255);                        // lt cyan
  colors[12] = wxColour(255, 0, 0);                          // lt red
  colors[13] = wxColour(255, 0, 255);                        // lt magenta
  colors[14] = wxColour(255, 255, 0);                        // yellow
  colors[15] = wxColour(255, 255, 255);                      // white
#else
  /*
  **  These are much better
  */
  colors[0] = wxColour(0, 0, 0);                             // black
  colors[1] = wxColour(0, 0, 170);                           // blue
  colors[2] = wxColour(0, 170, 0);                           // green
  colors[3] = wxColour(0, 170, 170);                         // cyan
  colors[4] = wxColour(170, 0, 0);                           // red
  colors[5] = wxColour(170, 0, 170);                         // magenta
  colors[6] = wxColour(170, 170, 0);                         // brown
  colors[7] = wxColour(170, 170, 170);                       // white
#if 0
  colors[8] = wxColour(85, 85, 85);                          // gray
  colors[9] = wxColour(85, 85, 255);                         // lt blue
  colors[10] = wxColour(85, 255, 85);                        // lt green
  colors[11] = wxColour(85, 255, 255);                       // lt cyan
  colors[12] = wxColour(255, 85, 85);                        // lt red
  colors[13] = wxColour(255, 85, 255);                       // lt magenta
  colors[14] = wxColour(255, 255, 85);                       // yellow
  colors[15] = wxColour(255, 255, 255);                      // white
#else
  colors[8] = wxColour(50, 50, 50);                          // gray
  colors[9] = wxColour(0, 0, 255);                         // lt blue
  colors[10] = wxColour(0, 255, 0);                        // lt green
  colors[11] = wxColour(0, 255, 255);                       // lt cyan
  colors[12] = wxColour(255, 0, 0);                        // lt red
  colors[13] = wxColour(255, 0, 255);                       // lt magenta
  colors[14] = wxColour(255, 255, 0);                       // yellow
  colors[15] = wxColour(255, 255, 255);                      // white
#endif
#endif
}

//////////////////////////////////////////////////////////////////////////////
///  public GetPCColors
///  Retrieves the PC colors
///
///  @param  colors wxColour [] Filled with the PC colors
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::GetPCColors(wxColour colors[16])
{
  int
    i;

  for(i = 0; i < 16; i++)
    colors[i] = m_pc_colors[i];
}

//////////////////////////////////////////////////////////////////////////////
///  public SetPCColors
///  Sets the PC colors
///
///  @param  colors wxColour [] The new colors to be used
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::SetPCColors(wxColour colors[16])
{
  int
    i;

  m_init = 1;
  for(i = 0; i < 16; i++)
    m_pc_colors[i] = colors[i];

  if(GetMode() & PC)
    SetBackgroundColour(m_pc_colors[0]);

  for(i = 0; i < 16; i++)
    m_pc_colorPens[i] = wxPen(m_pc_colors[i], 1, wxSOLID);
  m_init = 0;

  Refresh();
}

//////////////////////////////////////////////////////////////////////////////
///  public SetCursorBlinkRate
///  Sets how often the cursor blinks
///
///  @param  rate int  How many milliseconds between blinks
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::SetCursorBlinkRate(int rate)
{
  if(rate < 0 || rate > CURSOR_BLINK_MAX_TIMEOUT)
    return;

  m_init = 1;
  if(rate != m_curBlinkRate)
  {
    m_curBlinkRate = rate;
    if(!m_curBlinkRate)
      m_timer.Stop();
    else
      m_timer.Start(m_curBlinkRate);
  }
  m_init = 0;
}

//////////////////////////////////////////////////////////////////////////////
///  private OnChar
///  Handles user keyboard input and begins processing the server's response
///
///  @param  event wxKeyEvent & The generated key event
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::OnChar(wxKeyEvent& event)
{
  if(!(GetMode() & PC) && event.AltDown())
    event.Skip();
  else
  {

    int
      rc,
      keyCode = 0,
      len;

    unsigned char
      buf[10];

    /*
    **  Map control characters
    */
    if(event.ControlDown())
    {
      if(event.GetKeyCode() >= 'a' && event.GetKeyCode() <= 'z')
        keyCode = event.GetKeyCode() - 'a' + 1;
      else if(event.GetKeyCode() >= '[' && event.GetKeyCode() <= '_')
        keyCode = event.GetKeyCode() - '[' + 0x1b;
      else if(event.GetKeyCode() == '6')
        keyCode = 0x1e;
      else if(event.GetKeyCode() == '-')
        keyCode = 0x1f;
    }

    if(!keyCode && !(keyCode = MapKeyCode((int)event.GetKeyCode())))
    {
      /*
      **  If the keycode wasn't mapped in the table and it is a special
      **  key, then we just ignore it.
      */
      if(event.GetKeyCode() >= WXK_START)
      {
        event.Skip();
        return;
      }
      /*
      **  Otherwise, it must just be an ascii character
      */
      keyCode = (int)event.GetKeyCode();
    }

    if(GetMode() & PC)
      rc = TranslateKeyCode(keyCode, &len, (char *)buf, event.ShiftDown(),
                        event.ControlDown(), event.AltDown());
    else
      rc = TranslateKeyCode(keyCode, &len, (char *)buf);

    if(rc)
    {
      if((GetMode() & NEWLINE) && !(GetMode() & PC) && (buf[len - 1] == 10))
      {
        buf[len - 1] = 13;
        buf[len] = 10;
        len++;
      }
      ProcessOutput(len, buf);
      if((GetMode() & LOCALECHO) && !(GetMode() & PC))
        ProcessInput(len, buf);
    }
    else if(!(GetMode() & PC))
    {
      if((GetMode() & NEWLINE) && !(GetMode() & PC) && (keyCode == 10))
      {
        len = 2;
        buf[0] = 13;
        buf[1] = keyCode;
      }
      else
      {
        len = 1;
        buf[0] = keyCode;
      }
      ProcessOutput(len, buf);
      if((GetMode() & LOCALECHO) && !(GetMode() & PC))
        ProcessInput(len, buf);
    }
    else
      event.Skip();
  }
}

//////////////////////////////////////////////////////////////////////////////
///  private OnKeyDown
///  Appears to be unused
///
///  @param  event wxKeyEvent & The generated key event
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::OnKeyDown(wxKeyEvent& event)
{
  if(!(GetMode() & PC) && event.AltDown())
    event.Skip();
  else if(event.AltDown())
  {
//    wxLogMessage("OnKeyDown() got KeyCode = %d", event.KeyCode());
//    if(event.KeyCode() != 309)
//      OnChar(event);
  }
  else
    event.Skip();
}

//////////////////////////////////////////////////////////////////////////////
///  private OnPaint
///  Redraws the terminal widget
///
///  @param  event wxPaintEvent & The generated paint event
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::OnPaint(wxPaintEvent& WXUNUSED(event))
{
  wxPaintDC
    dc(this);

  m_curDC = &dc;
  ExposeArea(0, 0, m_width, m_height);
  m_curDC = 0;
}

//////////////////////////////////////////////////////////////////////////////
///  private OnLeftDown
///  Begins selection of terminal text
///
///  @param  event wxMouseEvent & The generated mouse event
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::OnLeftDown(wxMouseEvent& event)
{
	SetFocus();

  ClearSelection();
  m_selx1 = m_selx2 = event.GetX() / m_charWidth;
  m_sely1 = m_sely2 = event.GetY() / m_charHeight;
  m_selecting = TRUE;
  CaptureMouse();

}

//////////////////////////////////////////////////////////////////////////////
///  private OnLeftUp
///  Ends text selection
///
///  @param  event wxMouseEvent & The generated mouse event
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::OnLeftUp(wxMouseEvent& event)
{

  m_selecting = FALSE;
  if(GetCapture() == this)
  {
	ReleaseMouse();
  }

}

//////////////////////////////////////////////////////////////////////////////
///  private OnMouseMove
///  Changes the selection if the mouse button is down
///
///  @param  event wxMouseEvent & The generated mouse event
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::OnMouseMove(wxMouseEvent& event)
{

  if(m_selecting)
  {
    m_selx2 = event.GetX() / m_charWidth;
    if(m_selx2 >= Width())
      m_selx2 = Width() - 1;
    m_sely2 = event.GetY() / m_charHeight;
    if(m_sely2 >= Height())
      m_sely2 = Height() - 1;

    MarkSelection();
  }
}

//////////////////////////////////////////////////////////////////////////////
///  public ClearSelection
///  De-selects all selected text
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::ClearSelection()
{
  int
    x,
    y;

  wxClientDC
    *dc = 0;

  m_selx1 = m_sely1 = m_selx2 = m_sely2 = 0;

  if(!m_curDC)
  {
    dc = new wxClientDC(this);
    m_curDC = dc;
  }

  for(y = 0; y < Height(); y++)
    for(x = 0; x < Width(); x++)
      Select(x, y, 0);

  if(dc)
  {
	  this->wxWindow::Update();

    m_curDC = 0;
    delete dc;
  }
}

//////////////////////////////////////////////////////////////////////////////
///  private MarkSelection
///  Does _something_ as far as selecting text, but not really sure... used for SelectAll, I think
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::MarkSelection()
{
  int
    x,
    y;

  wxClientDC
    *dc = 0;

  m_marking = TRUE;

  if(!m_curDC)
  {
    dc = new wxClientDC(this);
    m_curDC = dc;
  }

  for(y = 0; y < Height(); y++)
    for(x = 0; x < Width(); x++)
      Select(x, y, 0);

  if(m_sely1 == m_sely2)
  {
    if(m_selx1 >= m_selx2)
      for(x = m_selx1; x <= m_selx2; x++)
        Select(x, m_sely1, 1);
    else
      for(x = m_selx2; x >= m_selx1; x--)
        Select(x, m_sely1, 1);
  }
  else if(m_sely1 < m_sely2)
  {
    for(x = m_selx1; x < Width(); x++)
      Select(x, m_sely1, 1);

    for(y = m_sely1 + 1; y < m_sely2; y++)
      for(x = 0; x < Width(); x++)
        Select(x, y, 1);

    for(x = 0; x <= m_selx2; x++)
      Select(x, m_sely2, 1);
  }
  else
  {
    for(x = 0; x <= m_selx1; x++)
      Select(x, m_sely1, 1);

    for(y = m_sely2 + 1; y < m_sely1; y++)
      for(x = 0; x < Width(); x++)
        Select(x, y, 1);

    for(x = m_selx2; x < Width(); x++)
      Select(x, m_sely2, 1);
  }

  this->wxWindow::Update();

  if(dc)
  {
    m_curDC = 0;
    delete dc;
  }

  m_marking = FALSE;
}

//////////////////////////////////////////////////////////////////////////////
///  public HasSelection
///  Checks if any text is selected
///
///  @return bool Whether or not there's any text selected
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
bool
wxTerm::HasSelection()
{
  return(m_selx1 != m_selx2 || m_sely1 != m_sely2);
}

wxString
//////////////////////////////////////////////////////////////////////////////
///  public GetSelection
///  Returns the selected text
///
///  @return wxString The selected text
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
wxTerm::GetSelection()
{
  int
    x1,
    y1,
    x2,
    y2;

  wxString
    sel;

  if(m_sely1 <= m_sely2)
  {
    x1 = m_selx1;
    y1 = m_sely1;
    x2 = m_selx2;
    y2 = m_sely2;
  }
  else
  {
    x1 = m_selx2;
    y1 = m_sely2;
    x2 = m_selx1;
    y2 = m_sely1;
  }

  while(x1 != x2 || y1 != y2)
  {
    if(GetChar(x1, y1))
      sel.Append(GetChar(x1, y1));

    x1++;
    if(x1 == Width())
    {
      sel.Append('\n');
      x1 = 0;
      y1++;
    }
  }
  if(GetChar(x1, y1))
    sel.Append(GetChar(x1, y1));

  return sel;
}

//////////////////////////////////////////////////////////////////////////////
///  public SelectAll
///  Selects the whole terminal
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::SelectAll()
{
  m_selx1 = 0;
  m_sely1 = 0;
  m_selx2 = Width() - 1;
  m_sely2 = Height() - 1;
  MarkSelection();
}

/*
**  GTerm stuff
*/
//////////////////////////////////////////////////////////////////////////////
///  public virtual DrawText
///  Responsible for actually drawing the terminal text on the widget.  This virtual
///  function is called from GTerm::update_changes.
///
///  @param  fg_color int             The index of the foreground color
///  @param  bg_color int             The index of the background color
///  @param  flags    int             Modifiers for drawing the text
///  @param  x        int             The x position in character cells
///  @param  y        int             The y position in character cells
///  @param  len      int             The number of characters to draw
///  @param  string   unsigned char * The characters to draw
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::DrawText(int fg_color, int bg_color, int flags,
                 int x, int y, int len, unsigned char *string)
{
  int
    t;

  if(flags & BOLD && m_boldStyle == COLOR)
    fg_color = (fg_color % 8) + 8;

  if(flags & SELECTED)
  {
    fg_color = 0;
    bg_color = 15;
  }

  if(flags & INVERSE)
  {
    t = fg_color;
    fg_color = bg_color;
    bg_color = t;
  }

  if(!m_curDC)
    return;

#if defined(__WXGTK__) || defined(__WXMOTIF__)
  int
    i;

  for(i = 0; string[i]; i++)
    string[i] = xCharMap[string[i]];
#endif

  wxString
    str(string, len);

  if(m_boldStyle != FONT)
  {
    if(flags & UNDERLINE)
      m_curDC->SetFont(m_underlinedFont);
    else
      m_curDC->SetFont(m_normalFont);
  }
  else
  {
    if(flags & BOLD)
    {
      if(flags & UNDERLINE)
        m_curDC->SetFont(m_boldUnderlinedFont);
      else
        m_curDC->SetFont(m_boldFont);
    }
    else
    {
      if(flags & UNDERLINE)
        m_curDC->SetFont(m_underlinedFont);
      else
        m_curDC->SetFont(m_normalFont);
    }
  }

  x = x * m_charWidth;
  y = y * m_charHeight;
  m_curDC->SetBackgroundMode(wxSOLID);
  m_curDC->SetTextBackground(m_colors[bg_color]);
  m_curDC->SetTextForeground(m_colors[fg_color]);
  m_curDC->DrawText(str, x, y);
  if(flags & BOLD && m_boldStyle == OVERSTRIKE)
  {
    m_curDC->SetBackgroundMode(wxTRANSPARENT);
    m_curDC->DrawText(str, x + 1, y);
  }
}

//////////////////////////////////////////////////////////////////////////////
///  private DoDrawCursor
///  Does the actual work of drawing the cursor
///
///  @param  fg_color int            The index of the foreground color
///  @param  bg_color int            The index of the background color
///  @param  flags    int            Modifier flags
///  @param  x        int            The x position of the cursor, in characters
///  @param  y        int            The y position of the cursor, in characters
///  @param  c        unsigned char  The character the cursor is over
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::DoDrawCursor(int fg_color, int bg_color, int flags,
                   int x, int y, unsigned char c)
{

  if(flags & BOLD && m_boldStyle == COLOR)
    fg_color = (fg_color % 8) + 8;

  if(flags & INVERSE)
  {
    int t = fg_color;
    fg_color = bg_color;
    bg_color = t;
  }

  if(!m_curDC)
    return;

#if defined(__WXGTK__) || defined(__WXMOTIF__)
  c = xCharMap[c];
#endif

  wxString
    str((char)c);

  if(m_boldStyle != FONT)
  {
    if(flags & UNDERLINE)
      m_curDC->SetFont(m_underlinedFont);
    else
      m_curDC->SetFont(m_normalFont);
  }
  else
  {
    if(flags & BOLD)
    {
      if(flags & UNDERLINE)
        m_curDC->SetFont(m_boldUnderlinedFont);
      else
        m_curDC->SetFont(m_boldFont);
    }
    else
    {
      if(flags & UNDERLINE)
        m_curDC->SetFont(m_underlinedFont);
      else
        m_curDC->SetFont(m_normalFont);
    }
  }

  x = x * m_charWidth;
  y = y * m_charHeight;
  m_curDC->SetBackgroundMode(wxSOLID);
  m_curDC->SetTextBackground(m_colors[fg_color]);
  m_curDC->SetTextForeground(m_colors[bg_color]);
  m_curDC->DrawText(str, x, y);
  if(flags & BOLD && m_boldStyle == OVERSTRIKE)
    m_curDC->DrawText(str, x + 1, y);
}

//////////////////////////////////////////////////////////////////////////////
///  public virtual DrawCursor
///  Draws the cursor on the terminal widget.  This virtual function is called
///  from GTerm::update_changes.
///
///  @param  fg_color int            The index of the foreground color
///  @param  bg_color int            The index of the background color
///  @param  flags    int            Modifiers for drawing the cursor
///  @param  x        int            The x position in character cells
///  @param  y        int            The y position in character cells
///  @param  c        unsigned char  The character that underlies the cursor
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::DrawCursor(int fg_color, int bg_color, int flags,
                   int x, int y, unsigned char c)
{
  m_curX = x;
  m_curY = y;
  m_curFG = fg_color;
  m_curBG = bg_color,
  m_curFlags = flags;
  m_curChar = c;



  if(m_timer.IsRunning())
    m_timer.Stop();
  DoDrawCursor(fg_color, bg_color, flags, x, y, c);
  if(m_curBlinkRate)
  {
    m_timer.Start(m_curBlinkRate);
    m_curState = 1;
  }
}

//////////////////////////////////////////////////////////////////////////////
///  private OnTimer
///  Blinks the cursor each time it goes off
///
///  @param  event wxTimerEvent & The generated timer event
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::OnTimer(wxTimerEvent& WXUNUSED(event))
{
  wxClientDC
    *dc = 0;

  if(m_init)
    return;

  if(m_curX == -1 || m_curY == -1)
    return;

  if(GetMode() & CURSORINVISIBLE)
  {
	  //wxLogDebug("Skipping cursor");
    return;
  }
  //wxLogDebug("Drawing cursor");
  if(!m_curDC)
  {
    dc = new wxClientDC(this);
    m_curDC = dc;
  }

  if(m_curBlinkRate)
  {
    m_curState++;
    if(m_curState & 1 && m_curX != -1 && m_curY != -1)
      DoDrawCursor(m_curFG, m_curBG, m_curFlags, m_curX, m_curY, m_curChar);
    else
      DoDrawCursor(m_curBG, m_curFG, m_curFlags, m_curX, m_curY, m_curChar);
  }

  if(dc)
  {
    delete dc;
    m_curDC = 0;
  }
}

//////////////////////////////////////////////////////////////////////////////
///  public virtual MoveChars
///  Moves characters on the screen.  This virtual function is called from
///  GTerm::update_changes.
///
///  @param  sx   int  The starting x position, in character cells
///  @param  sy   int  The starting y position, in character cells
///  @param  dx   int  The number of character cells to shift in the x direction
///  @param  dy   int  The number of character cells to shift in the y direction
///  @param  w    int  The width in characters of the area to be moved
///  @param  h    int  The height in characters of the area to be moved
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::MoveChars(int sx, int sy, int dx, int dy, int w, int h)
{
  if(!m_marking)
    ClearSelection();

  sx = sx * m_charWidth;
  sy = sy * m_charHeight;
  dx = dx * m_charWidth;
  dy = dy * m_charHeight;
  w = w * m_charWidth;
  h = h * m_charHeight;

  m_memDC.Blit(0, 0, w, h, m_curDC, sx, sy);
  m_curDC->Blit(dx, dy, w, h, &m_memDC, 0, 0);
}

//////////////////////////////////////////////////////////////////////////////
///  public virtual ClearChars
///  Clears a section of characters from the screen.  This virtual function
///  is called from GTerm::update_changes.
///
///  @param  bg_color int  The background color to replace the characters with
///  @param  x        int  The starting x position, in characters
///  @param  y        int  The starting y position, in characters
///  @param  w        int  The width of the area to be cleared, in characters
///  @param  h        int  The height of the area to be cleared, in characters
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::ClearChars(int bg_color, int x, int y, int w, int h)
{
  if(!m_marking)
    ClearSelection();

  x = x * m_charWidth;
  y = y * m_charHeight;
  w = w * m_charWidth;
  h = h * m_charHeight;

  bool deleteDC = false;
  if(!m_curDC)
  {
	  m_curDC = new wxClientDC(this);
	  deleteDC = true;
  }
  m_curDC->SetPen(m_colorPens[bg_color]);
  m_curDC->SetBrush(wxBrush(m_colors[bg_color], wxSOLID));
  m_curDC->DrawRectangle(x, y, w /* + 1*/, h /*+ 1*/);

  if(deleteDC)
  {
	  delete m_curDC;
	  m_curDC = 0;
  }
}

//////////////////////////////////////////////////////////////////////////////
///  public virtual ModeChange
///  Changes the drawing mode between VT100 and PC
///
///  @param  state int  The new state
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::ModeChange(int state)
{
  ClearSelection();

  if(state & GTerm::PC)
  {
    m_colors = m_pc_colors;
    m_colorPens = m_pc_colorPens;
  }
  else
  {
    m_colors = m_vt_colors;
    m_colorPens = m_vt_colorPens;
  }
  GTerm/*lnet*/::ModeChange(state);
}

//////////////////////////////////////////////////////////////////////////////
///  public virtual Bell
///  Rings the system bell
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::Bell()
{
  wxBell();
}

//////////////////////////////////////////////////////////////////////////////
///  public UpdateSize
///  Updates the terminal's size in characters after it has been resized on the screen.
///
///  @return void
///
///  @author Mark Erikson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void wxTerm::UpdateSize()
{
	// prevent any nasty recursion
	if(m_inUpdateSize)
	{
		return;
	}

	m_inUpdateSize = true;
	int charWidth, charHeight;

	wxClientDC* dc = new wxClientDC(this);

	if(!m_curDC)
	{
		m_curDC = dc;
	}

	dc->SetFont(m_normalFont);
	dc->GetTextExtent("M", &charWidth, &charHeight);
	wxSize currentClientSize = GetClientSize();
	int numCharsInLine = currentClientSize.GetX() / charWidth;
	int numLinesShown = currentClientSize.GetY() / charHeight;



	if( (numCharsInLine != m_charsInLine) || (numLinesShown != m_linesDisplayed))
	{
		wxString message;

		// FINALLY!  Finally killed the memory leak!  The problem is that somehow a size event
		// was generating negative numbers for these values, which led to weird things happening.
		if( (numCharsInLine > 0) && (numLinesShown > 0))
		{
			m_charsInLine = numCharsInLine;
			m_linesDisplayed = numLinesShown;
			// tell the GTerm core to resize itself
			ResizeTerminal(numCharsInLine, numLinesShown);

			UpdateRemoteSize(m_charsInLine, m_linesDisplayed);
			/*
			wxString remoteResizeCommand = wxString::Format("stty rows %d cols %d", m_linesDisplayed, m_charsInLine);
			wxLogDebug("Resizing terminal: %s", remoteResizeCommand);
			wxStringBuffer tempBuffer(remoteResizeCommand, 256);
			SendBack(tempBuffer);
			*/
		}
	}

	m_inUpdateSize = false;

	if(dc)
	{
		delete dc;
		m_curDC = 0;
	}
}

//////////////////////////////////////////////////////////////////////////////
///  public virtual ResizeTerminal
///  <Resizes the terminal to a given number of characters high and wide
///
///  @param  width    int  The new number of characters wide
///  @param  height    int  The new number of characters high
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::ResizeTerminal(int width, int height)
{
  int
    w,
    h;

  ClearSelection();

  /*
  **  Determine window size from current font
  */
  wxClientDC
    dc(this);

  if(m_boldStyle != FONT)
    dc.SetFont(m_normalFont);
  else
    dc.SetFont(m_boldFont);
  dc.GetTextExtent("M", &m_charWidth, &m_charHeight);
  w = width * m_charWidth;
  h = height * m_charHeight;

  /*
  **  Create our bitmap for copying
  */
  if(m_bitmap)
  {
    m_memDC.SelectObject(wxNullBitmap);
    delete m_bitmap;
  }
  m_bitmap = new wxBitmap(w, h);
  m_memDC.SelectObject(*m_bitmap);

  /*
  **  Set window size
  */
#if defined(__WXGTK__) || defined(__WXMOTIF__)
  SetSize(w, h + 4);
#else
  SetSize(w, h);
#endif

  /*
  **  Set terminal size
  */
  GTerm::ResizeTerminal(width, height);
  m_width = width;
  m_height = height;

  /*
  **  Send event
  */
  if(!m_init)
  {
    wxCommandEvent e(wxEVT_COMMAND_TERM_RESIZE, GetId());
    e.SetEventObject(this);
    GetParent()->GetEventHandler()->ProcessEvent(e);
  }
}

//////////////////////////////////////////////////////////////////////////////
///  public virtual RequestSizeChange
///  A virtual function, used by GTerm to update the size
///
///  @param  w    int  The new width in characters
///  @param  h    int  The new height in characters
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::RequestSizeChange(int w, int h)
{
  ResizeTerminal(w, h);
}

//////////////////////////////////////////////////////////////////////////////
///  public virtual ProcessInput
///  Processes text received from the server
///
///  @param  len  int             The number of characters received
///  @param  data unsigned char * The received text
///
///  @return void
///
///  @remarks This function is *not* safe to call from anywhere but
///  @remarks the main wxwidgets event loop, as it draws
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::ProcessInput(int len, unsigned char *data)
{
  wxClientDC
    dc(this);

  ClearSelection();
  m_curDC = &dc;
  GTerm::ProcessInput(len, data);
  m_curDC = 0;
}

//////////////////////////////////////////////////////////////////////////////
///  private MapKeyCode
///  Converts from WXWidgets special keycodes to VT100
///  special keycodes.
///
///
///  @param  keyCode int  The keycode to check
///
///  @return int     The returned keycode
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
int
wxTerm::MapKeyCode(int keyCode)
{
  int
    i;

  for(i = 0; keyMapTable[i].keyCode; i++)
    if(keyMapTable[i].keyCode == keyCode)
      return keyMapTable[i].VTKeyCode;
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
///  public virtual SelectPrinter
///  Selects the printer to be used
///
///  @param  PrinterName char * The text of the printer name
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::SelectPrinter(char *PrinterName)
{
  if(m_printerFN)
  {
    if(m_printerName[0] == '#')
      fclose(m_printerFN);
    else
#if defined(__WXGTK__) || defined(__WXMOTIF__)
      pclose(m_printerFN);
#endif
#if defined(__WXMSW__)
      fclose(m_printerFN);
#endif

    m_printerFN = 0;
  }

  if(m_printerName)
  {
    free(m_printerName);
    m_printerName = 0;
  }

  if(strlen(PrinterName))
  {
    m_printerName = strdup(PrinterName);
  }
}

//////////////////////////////////////////////////////////////////////////////
///  public virtual PrintChars
///  Prints stuff to the currently selected printer
///
///  @param  len  int             The number of characters
///  @param  data unsigned char * The text
///
///  @return void
///
///  @author Derry Bryson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void
wxTerm::PrintChars(int len, unsigned char *data)
{
  char
    pname[100];

  if(!m_printerFN)
  {
    if(!m_printerName)
      return;

    if(m_printerName[0] == '#')
    {
#if defined(__WXGTK__) || defined(__WXMOTIF__)
      sprintf(pname, "/dev/lp%d", m_printerName[1] - '0');
#endif
#if defined(__WXMSW__)
      sprintf(pname, "lpt%d", m_printerName[1] - '0' + 1);
#endif
      m_printerFN = fopen(pname, "wb");
    }
    else
    {
#if defined(__WXGTK__) || defined(__WXMOTIF__)
      sprintf(pname, "lpr -P%s", m_printerName);
      m_printerFN = popen(pname, "w");
#endif
#if defined(__WXMSW__)
      m_printerFN = fopen(m_printerName, "wb");
#endif
    }
  }

  if(m_printerFN)
  {
    fwrite(data, len, 1, m_printerFN);
  }
}


//////////////////////////////////////////////////////////////////////////////
///  private OnGainFocus
///  Enables the cursor
///
///  @param  event wxFocusEvent & The generated focus event
///
///  @return void
///
///  @author Mark Erikson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void wxTerm::OnGainFocus(wxFocusEvent &event)
{
	this->clear_mode_flag(CURSORINVISIBLE);
	//wxLogDebug("TerminalWx Gained focus");
	GTerm::Update();
}

//////////////////////////////////////////////////////////////////////////////
///  private OnLoseFocus
///  Disables the cursor
///
///  @param  event wxFocusEvent & The generated focus event
///
///  @return void
///
///  @author Mark Erikson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void wxTerm::OnLoseFocus(wxFocusEvent &event)
{
	this->set_mode_flag(CURSORINVISIBLE);
	//wxLogDebug("TerminalWx Lost focus");
	GTerm::Update();
}

//////////////////////////////////////////////////////////////////////////////
///  private OnSize
///  Lets the terminal resize the text whenever the window is resized
///
///  @param  event wxSizeEvent & The generated size event
///
///  @return void
///
///  @author Mark Erikson @date 04-22-2004
//////////////////////////////////////////////////////////////////////////////
void wxTerm::OnSize(wxSizeEvent &event)
{
	UpdateSize();
}

void wxTerm::UpdateRemoteSize(int width, int height)
{

}
