#define TOGL_X11 1

/* sudo apt install libgl1-mesa-dev might produce these headers. */

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>
#if 0 /* Maybe we will need some of these. */
#include <X11/Xutil.h>
#include <X11/Xatom.h>       /* for XA_RGB_DEFAULT_MAP atom */
#include <X11/StdCmap.h>     /* for XmuLookupStandardColormap */
#include <X11/Xmu/StdCmap.h> /* for XmuLookupStandardColormap */
#include <X11/extensions/SGIStereo.h>
#endif
