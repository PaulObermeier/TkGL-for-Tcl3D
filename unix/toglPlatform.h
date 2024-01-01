#define TOGL_X11 1

/* sudo apt install libgl1-mesa-dev might produce the headers you need. */
/*
  This header file nightmare was copied from Togl 2.1.  I have no
  idea why this craziness is necessary.  But it seems to work.
*/
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#  include <X11/Xatom.h>        /* for XA_RGB_DEFAULT_MAP atom */
#  define GLX_GLXEXT_LEGACY     /* include glxext.h separately */
#  include <GL/glx.h>
   /* we want the prototype typedefs from glxext.h */
#  undef GLX_VERSION_1_3
#  undef GLX_VERSION_1_4
#  ifdef UNDEF_GET_PROC_ADDRESS
#    undef GLX_ARB_get_proc_address
#  endif
   /* we want to use glXCreateContextAttribsARB */
#  define GLX_GLXEXT_PROTOTYPES
#  include <GL/glxext.h>

#if 0 /* Maybe we will need some of these. */
#define GLX_GLXEXT_PROTOTYPES 1
#include <X11/Xutil.h>
#include <X11/Xatom.h>       /* for XA_RGB_DEFAULT_MAP atom */
#include <X11/StdCmap.h>     /* for XmuLookupStandardColormap */
#include <X11/Xmu/StdCmap.h> /* for XmuLookupStandardColormap */
#include <X11/extensions/SGIStereo.h>
#endif
