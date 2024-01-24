/*
 * tkglPlatform.h (for Microsoft Windows) --
 *
 * Copyright (C) 2024, Marc Culler, Nathan Dunfield, Matthias Goerner
 *
 * This file is part of the TkGL project.  TkGL is derived from Togl, which
 * was written by Brian Paul, Ben Bederson and Greg Couch.  TkGL is licensed
 * under the Tcl license.  The terms of the license are described in the file
 * "license.terms" which should be included with this distribution.
 */
#define TKGL_WGL 1

#include <windows.h>
#include <wingdi.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>
#include <tk.h>
#include <tkPlatDecls.h>

#ifndef __GNUC__
#    define strncasecmp _strnicmp
#    define strcasecmp _stricmp
#endif

