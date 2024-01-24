/*
 * tkglPlatform.h (for macOS) --
 *
 * Copyright (C) 2024, Marc Culler, Nathan Dunfield, Matthias Goerner
 *
 * This file is part of the TkGL project.  TkGL is derived from Togl, which
 * was written by Brian Paul, Ben Bederson and Greg Couch.  TkGL is licensed
 * under the Tcl license.  The terms of the license are described in the file
 * "license.terms" which should be included with this distribution.
 */

#define TKGL_NSOPENGL 1

#include <Foundation/Foundation.h>
#include <AppKit/NSOpenGL.h>
#include <OpenGL/OpenGL.h>
#include <AppKit/NSView.h>
#include <tkMacOSXInt.h>    /* For MacDrawable */
#include <OpenGL/glext.h>
#include <OpenGL/gl.h>
