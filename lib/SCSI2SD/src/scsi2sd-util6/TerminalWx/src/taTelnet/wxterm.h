/*
    taTelnet - A cross-platform telnet program.
    Copyright (c) 2000 Derry Bryson
                  2004 Mark Erikson
                  2012-2013 Jeremy Salwen

License: wxWindows License Version 3.1 (See the file license3.txt)

*/


#ifndef INCLUDE_WXTERM
#define INCLUDE_WXTERM

#ifdef __GNUG__
#pragma interface
#endif

#include "../GTerm/gterm.hpp"
#include <wx/dcmemory.h>
#include <wx/timer.h>
#include <wx/colour.h>
#include <wx/event.h>
#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/window.h>

#define wxEVT_COMMAND_TERM_RESIZE        wxEVT_USER_FIRST + 1000
#define wxEVT_COMMAND_TERM_NEXT          wxEVT_USER_FIRST + 1001

#define EVT_TERM_RESIZE(id, fn) { wxEVT_COMMAND_TERM_RESIZE, id, -1, (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction) &fn, (wxObject *)NULL },

class wxTerm : public wxWindow, public GTerm
{
  int
    m_charWidth,
    m_charHeight,
    m_init,
    m_width,
    m_height,
    m_selx1,
    m_sely1,
    m_selx2,
    m_sely2,
    m_curX,
    m_curY,
    m_curFG,
    m_curBG,
    m_curFlags,
    m_curState,
    m_curBlinkRate;

      int m_charsInLine;
  int m_linesDisplayed;

  unsigned char
    m_curChar;

  bool
    m_selecting,
    m_marking;

  bool m_inUpdateSize;

  wxColour
    m_vt_colors[16],
    m_pc_colors[16],
    *m_colors;

  wxPen
    m_vt_colorPens[16],
    m_pc_colorPens[16],
    *m_colorPens;

  wxFont
    m_normalFont,
    m_underlinedFont,
    m_boldFont,
    m_boldUnderlinedFont;

  wxDC
    *m_curDC;

  wxMemoryDC
    m_memDC;

  wxBitmap
    *m_bitmap;

  FILE
    *m_printerFN;

  char
    *m_printerName;

  wxTimer
    m_timer;

public:
  enum BOLDSTYLE
  {
    DEFAULT = -1,
    COLOR = 0,
    OVERSTRIKE = 1,
    FONT = 2
  };

private:
  BOLDSTYLE
    m_boldStyle;

  typedef struct
  {
    wxKeyCode
      keyCode;

    int
      VTKeyCode;
  } TermKeyMap;

  static TermKeyMap keyMapTable[];

public:
  wxTerm(wxWindow* parent, wxWindowID id,
         const wxPoint& pos = wxDefaultPosition,
         int width = 80, int height = 24,
         const wxString& name = "wxTerm");

  virtual ~wxTerm();

  bool SetFont(const wxFont& font);

  void GetDefVTColors(wxColor colors[16], wxTerm::BOLDSTYLE boldStyle = wxTerm::DEFAULT);
  void GetVTColors(wxColour colors[16]);
  void SetVTColors(wxColour colors[16]);
  void GetDefPCColors(wxColour colors[16]);
  void GetPCColors(wxColour colors[16]);
  void SetPCColors(wxColour colors[16]);
  int GetCursorBlinkRate() { return m_curBlinkRate; }
  void SetCursorBlinkRate(int rate);

  void SetBoldStyle(wxTerm::BOLDSTYLE boldStyle);
  wxTerm::BOLDSTYLE GetBoldStyle(void) { return m_boldStyle; }

  void ScrollTerminal(int numLines, bool scrollUp = true);

  void ClearSelection();
  bool HasSelection();
  wxString GetSelection();
  void SelectAll();

  void UpdateSize();
  //void UpdateSize(int &termheight, int &linesReceived);
  //void UpdateSize(wxSizeEvent &event);

  /*
  **  GTerm stuff
  */
  virtual void DrawText(int fg_color, int bg_color, int flags,
                        int x, int y, int len, unsigned char *string);
  virtual void DrawCursor(int fg_color, int bg_color, int flags,
                          int x, int y, unsigned char c);

  virtual void MoveChars(int sx, int sy, int dx, int dy, int w, int h);
  virtual void ClearChars(int bg_color, int x, int y, int w, int h);
//  virtual void SendBack(int len, char *data);
  virtual void ModeChange(int state);
  virtual void Bell();
  virtual void ResizeTerminal(int w, int h);
  virtual void RequestSizeChange(int w, int h);

  virtual void ProcessInput(int len, unsigned char *data);
//  virtual void ProcessOutput(int len, unsigned char *data);

  virtual void SelectPrinter(char *PrinterName);
  virtual void PrintChars(int len, unsigned char *data);

  virtual void UpdateRemoteSize(int width, int height);
  int GetTermWidth(){return m_charsInLine;}
  int GetTermHeight(){return m_linesDisplayed;}

private:
  int MapKeyCode(int keyCode);
  void MarkSelection();
  void DoDrawCursor(int fg_color, int bg_color, int flags,
                    int x, int y, unsigned char c);

  void OnChar(wxKeyEvent& event);
  void OnKeyDown(wxKeyEvent& event);
  void OnPaint(wxPaintEvent& event);
  void OnLeftDown(wxMouseEvent& event);
  void OnLeftUp(wxMouseEvent& event);
  void OnMouseMove(wxMouseEvent& event);
  void OnTimer(wxTimerEvent& event);
  void OnSize(wxSizeEvent &event);

  void OnGainFocus(wxFocusEvent &event);
  void OnLoseFocus(wxFocusEvent &event);

  //private wxScrollBar* m_scrollbar;

  DECLARE_EVENT_TABLE()
};

#endif /* INCLUDE_WXTERM */
