#define TOGL_WGL 1

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

