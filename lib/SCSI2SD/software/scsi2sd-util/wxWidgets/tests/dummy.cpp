/*
 * File:  dummy.cc
 * Purpose:  See below
 * Author:  Julian Smart
 * Created:  1993
 * Updated:
 * Copyright:   (c) Julian Smart
 */

/* A dummy file to include wx.h. If precompiling wx.h, I
 * always start by compiling this and producing the PCH file.
 * Then subsequent source files use the PCH file.
 *
 * If precompiling wx.h for wxWidgets and derived apps,
 * link dummy.obj with your program (the MSC 7 linker complains otherwise).
 *
 * This is the only convenient way I found to use precompiled headers
 * under MSC 7.
 *
 * This will produce a big PCH file.
 */

/* RN:  MSVC Spits out annoying warnings about debug info - tell it to be quiet! */
#ifdef _MSC_VER
    #pragma warning(disable : 4786)
#endif // _MSC_VER

#include "testprec.h"
