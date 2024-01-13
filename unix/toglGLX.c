/*
  This file contains implementations of the following platform specific
  functions declared in togl.h.  They comprise the platform interface.

void Togl_Update(const Togl *toglPtr);
Window Togl_MakeWindow(Tk_Window tkwin, Window parent, void* instanceData);
void Togl_WorldChanged(void* instanceData);
void Togl_MakeCurrent(const Togl *toglPtr);
void Togl_SwapBuffers(const Togl *toglPtr);
int Togl_TakePhoto(Togl *toglPtr, Tk_PhotoHandle photo);
int Togl_CopyContext(const Togl *from, const Togl *to, unsigned mask);
int Togl_CreateGLContext(Togl *toglPtr);
const char* Togl_GetExtensions(Togl *ToglPtr);
void Togl_FreeResources(Togl *ToglPtr);
*/

#include <stdbool.h>
#include "togl.h"
#include "toglPlatform.h"
#include "tkInt.h"  /* for TkWindow */

static Colormap get_rgb_colormap(Display *dpy, int scrnum,
		    const XVisualInfo *visinfo, Tk_Window tkwin);

static const int attributes_2_1[] = {
  GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
  GLX_CONTEXT_MINOR_VERSION_ARB, 1,
  None
};

static const int attributes_3_2[] = {
  GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
  GLX_CONTEXT_MINOR_VERSION_ARB, 2,
  None
};

static const int attributes_4_1[] = {
  GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
  GLX_CONTEXT_MINOR_VERSION_ARB, 1,
  None
};

#define ALL_EVENTS_MASK         \
   (KeyPressMask                \
   |KeyReleaseMask              \
   |ButtonPressMask             \
   |ButtonReleaseMask           \
   |EnterWindowMask             \
   |LeaveWindowMask             \
   |PointerMotionMask           \
   |ExposureMask                \
   |VisibilityChangeMask        \
   |FocusChangeMask             \
   |PropertyChangeMask          \
   |ColormapChangeMask)

static PFNGLXCHOOSEFBCONFIGPROC chooseFBConfig = NULL;
static PFNGLXGETFBCONFIGATTRIBPROC getFBConfigAttrib = NULL;
static PFNGLXGETVISUALFROMFBCONFIGPROC getVisualFromFBConfig = NULL;
static PFNGLXCREATEPBUFFERPROC createPbuffer = NULL;
static PFNGLXCREATEGLXPBUFFERSGIXPROC createPbufferSGIX = NULL;
static PFNGLXDESTROYPBUFFERPROC destroyPbuffer = NULL;
static PFNGLXQUERYDRAWABLEPROC queryPbuffer = NULL;
static Bool hasMultisampling = False;
static Bool hasPbuffer = False;

struct FBInfo
{
    int     acceleration;
    int     samples;
    int     depth;
    int     colors;
    GLXFBConfig fbcfg;
};
typedef struct FBInfo FBInfo;

static void getFBInfo(
   Display *display,
   GLXFBConfig cfg,
   FBInfo *info)
{
    info->fbcfg = cfg;
    /* GLX_NONE < GLX_SLOW_CONFIG < GLX_NON_CONFORMANT_CONFIG */
    getFBConfigAttrib(display, cfg, GLX_CONFIG_CAVEAT, &info->acceleration);
    /* Number of bits per color */
    getFBConfigAttrib(display, cfg, GLX_BUFFER_SIZE, &info->colors);
    /* Number of bits per depth value. */
    getFBConfigAttrib(display, cfg, GLX_DEPTH_SIZE, &info->depth);
    /* Number of samples per pixesl when multisampling. */
    getFBConfigAttrib(display, cfg, GLX_SAMPLES, &info->samples);
}

static Bool isBetterFB(
    const FBInfo *x,
    const FBInfo *y)
{
    /* True if x is better than y */
    if (x->acceleration != y->acceleration)
        return (x->acceleration < y->acceleration);
    if (x->colors != y->colors)
        return (x->colors > y->colors);
    if (x->depth != y->depth)
        return (x->depth > y->depth);
    if (x->samples != y->samples)
        return (x->samples > y->samples);
    return false;
}

static Tcl_ThreadDataKey togl_XError;
struct ErrorData
{
    int     error_code;
    XErrorHandler prevHandler;
};
typedef struct ErrorData ErrorData;

static int
togl_HandleXError(Display *dpy, XErrorEvent * event)
{
    ErrorData *data = Tcl_GetThreadData(&togl_XError, (int) sizeof (ErrorData));

    data->error_code = event->error_code;
    return 0;
}

static void
togl_SetupXErrorHandler()
{
    ErrorData *data = Tcl_GetThreadData(&togl_XError, (int) sizeof (ErrorData));

    data->error_code = Success; /* 0 */
    data->prevHandler = XSetErrorHandler(togl_HandleXError);
}

static int
togl_CheckForXError(const Togl *toglPtr)
{
    ErrorData *data = Tcl_GetThreadData(&togl_XError, (int) sizeof (ErrorData));

    XSync(toglPtr->display, False);
    (void) XSetErrorHandler(data->prevHandler);
    return data->error_code;
}

static GLXPbuffer
togl_createPbuffer(Togl *toglPtr)
{
    int     attribs[32];
    int     na = 0;
    GLXPbuffer pbuf;

    togl_SetupXErrorHandler();
    if (toglPtr->largestPbufferFlag) {
        attribs[na++] = GLX_LARGEST_PBUFFER;
        attribs[na++] = True;
    }
    attribs[na++] = GLX_PRESERVED_CONTENTS;
    attribs[na++] = True;
    if (createPbuffer) {
        attribs[na++] = GLX_PBUFFER_WIDTH;
        attribs[na++] = toglPtr->width;
        attribs[na++] = GLX_PBUFFER_HEIGHT;
        attribs[na++] = toglPtr->width;
        attribs[na++] = None;
        pbuf = createPbuffer(toglPtr->display, toglPtr->fbcfg, attribs);
    } else {
        attribs[na++] = None;
        pbuf = createPbufferSGIX(toglPtr->display, toglPtr->fbcfg,
		   toglPtr->width, toglPtr->height, attribs);
    }
    if (togl_CheckForXError(toglPtr) || pbuf == None) {
        Tcl_SetResult(toglPtr->interp,
                      "unable to allocate pbuffer", TCL_STATIC);
        return None;
    }
    if (pbuf && toglPtr->largestPbufferFlag) {
        unsigned int     tmp;

        queryPbuffer(toglPtr->display, pbuf, GLX_WIDTH, &tmp);
        if (tmp != 0)
            toglPtr->width = tmp;
        queryPbuffer(toglPtr->display, pbuf, GLX_HEIGHT, &tmp);
        if (tmp != 0)
            toglPtr->height = tmp;
    }
    return pbuf;
}

static XVisualInfo *
togl_pixelFormat(
    Togl *toglPtr,
    int scrnum)
{
    int attribs[256];
    int na = 0;
    int i;
    XVisualInfo *visinfo;
    int dummy, major, minor;
    const char *extensions;

    /*
     * Make sure OpenGL's GLX extension is supported.
     */

    if (!glXQueryExtension(toglPtr->display, &dummy, &dummy)) {
      Tcl_SetResult(toglPtr->interp,
                    "X server is missing OpenGL GLX extension",
                    TCL_STATIC);
      return NULL;
    }

#ifdef DEBUG_GLX
    (void) XSetErrorHandler(fatal_error);
#endif

    glXQueryVersion(toglPtr->display, &major, &minor);
    extensions = glXQueryExtensionsString(toglPtr->display, scrnum);

    if (major > 1 || (major == 1 && minor >= 4)) {
        chooseFBConfig = glXChooseFBConfig;
        getFBConfigAttrib = glXGetFBConfigAttrib;
        getVisualFromFBConfig = glXGetVisualFromFBConfig;
        createPbuffer = glXCreatePbuffer;
        destroyPbuffer = glXDestroyPbuffer;
        queryPbuffer = glXQueryDrawable;
        hasPbuffer = True;
    } else {
	Tcl_SetResult(toglPtr->interp,
	    "Togl 3.0 requires GLX 1.4 or newer.", TCL_STATIC);
	return NULL;
    }
    if (hasPbuffer && !chooseFBConfig) {
      hasPbuffer = False;
    }

    if (strstr(extensions, "GLX_ARB_multisample") != NULL
        || strstr(extensions, "GLX_SGIS_multisample") != NULL) {
      hasMultisampling = True;
    }

    if (toglPtr->multisampleFlag && !hasMultisampling) {
        Tcl_SetResult(toglPtr->interp,
                      "multisampling not supported", TCL_STATIC);
        return NULL;
    }

    if (toglPtr->pBufferFlag && !hasPbuffer) {
        Tcl_SetResult(toglPtr->interp,
                      "pbuffers are not supported", TCL_STATIC);
        return NULL;
    }

    if (chooseFBConfig) {
        int     count;
        GLXFBConfig *cfgs;

        attribs[na++] = GLX_RENDER_TYPE;
        if (toglPtr->rgbaFlag) {
            /* RGB[A] mode */
            attribs[na++] = GLX_RGBA_BIT;
            attribs[na++] = GLX_RED_SIZE;
            attribs[na++] = toglPtr->rgbaRed;
            attribs[na++] = GLX_GREEN_SIZE;
            attribs[na++] = toglPtr->rgbaGreen;
            attribs[na++] = GLX_BLUE_SIZE;
            attribs[na++] = toglPtr->rgbaBlue;
            if (toglPtr->alphaFlag) {
                attribs[na++] = GLX_ALPHA_SIZE;
                attribs[na++] = toglPtr->alphaSize;
            }
        } else {
            /* Color index mode */
            attribs[na++] = GLX_COLOR_INDEX_BIT;
            attribs[na++] = GLX_BUFFER_SIZE;
            attribs[na++] = 1;
        }
        if (toglPtr->depthFlag) {
            attribs[na++] = GLX_DEPTH_SIZE;
            attribs[na++] = toglPtr->depthSize;
        }
        if (toglPtr->doubleFlag) {
            attribs[na++] = GLX_DOUBLEBUFFER;
            attribs[na++] = True;
        }
        if (toglPtr->stencilFlag) {
            attribs[na++] = GLX_STENCIL_SIZE;
            attribs[na++] = toglPtr->stencilSize;
        }
        if (toglPtr->accumFlag) {
            attribs[na++] = GLX_ACCUM_RED_SIZE;
            attribs[na++] = toglPtr->accumRed;
            attribs[na++] = GLX_ACCUM_GREEN_SIZE;
            attribs[na++] = toglPtr->accumGreen;
            attribs[na++] = GLX_ACCUM_BLUE_SIZE;
            attribs[na++] = toglPtr->accumBlue;
            if (toglPtr->alphaFlag) {
                attribs[na++] = GLX_ACCUM_ALPHA_SIZE;
                attribs[na++] = toglPtr->accumAlpha;
            }
        }
        if (toglPtr->stereo == TOGL_STEREO_NATIVE) {
            attribs[na++] = GLX_STEREO;
            attribs[na++] = True;
        }
        if (toglPtr->multisampleFlag) {
            attribs[na++] = GLX_SAMPLE_BUFFERS_ARB;
            attribs[na++] = 1;
            attribs[na++] = GLX_SAMPLES_ARB;
            attribs[na++] = 2;
        }
        if (toglPtr->pBufferFlag) {
            attribs[na++] = GLX_DRAWABLE_TYPE;
            attribs[na++] = GLX_WINDOW_BIT | GLX_PBUFFER_BIT;
        }
        if (toglPtr->auxNumber != 0) {
            attribs[na++] = GLX_AUX_BUFFERS;
            attribs[na++] = toglPtr->auxNumber;
        }
        attribs[na++] = None;

        cfgs = chooseFBConfig(toglPtr->display, scrnum, attribs, &count);
        if (cfgs == NULL || count == 0) {
            Tcl_SetResult(toglPtr->interp, "Couldn't choose pixel format.",
			  TCL_STATIC);
            return NULL;
        }

        /*
         * Pick the best available pixel format.
         */

	FBInfo bestFB, nextFB;
	getFBInfo(toglPtr->display, cfgs[0], &bestFB);
	for (i=1; i < count; i++) {
	    getFBInfo(toglPtr->display, cfgs[i], &nextFB);
	    if (isBetterFB(&nextFB, &bestFB)) {
		bestFB = nextFB;
	    }
	}
#if 0
	printf(" acc: %d ", bestFB.acceleration);
	printf(" colors: %d ", bestFB.colors);
	printf(" depth: %d ", bestFB.depth);
	printf(" samples: %d\n", bestFB.samples);
#endif
	toglPtr->fbcfg = bestFB.fbcfg;
	visinfo = getVisualFromFBConfig(toglPtr->display, bestFB.fbcfg);
    }
    if (visinfo == NULL) {
        Tcl_SetResult(toglPtr->interp,
                      "couldn't choose pixel format", TCL_STATIC);
        return NULL;
    }
    return visinfo;
}

static int
togl_describePixelFormat(Togl *toglPtr)
{
    int tmp = 0;

    /* fill in flags normally passed in that affect behavior */
    (void) glXGetConfig(toglPtr->display, toglPtr->visInfo, GLX_RGBA,
               &toglPtr->rgbaFlag);
    (void) glXGetConfig(toglPtr->display, toglPtr->visInfo, GLX_DOUBLEBUFFER,
               &toglPtr->doubleFlag);
    (void) glXGetConfig(toglPtr->display, toglPtr->visInfo, GLX_DEPTH_SIZE,
	       &tmp);
    toglPtr->depthFlag = (tmp != 0);
    (void) glXGetConfig(toglPtr->display, toglPtr->visInfo, GLX_ACCUM_RED_SIZE,
	       &tmp);
    toglPtr->accumFlag = (tmp != 0);
    (void) glXGetConfig(toglPtr->display, toglPtr->visInfo, GLX_ALPHA_SIZE,
	       &tmp);
    toglPtr->alphaFlag = (tmp != 0);
    (void) glXGetConfig(toglPtr->display, toglPtr->visInfo, GLX_STENCIL_SIZE,
	       &tmp);
    toglPtr->stencilFlag = (tmp != 0);
    (void) glXGetConfig(toglPtr->display, toglPtr->visInfo, GLX_STEREO, &tmp);
    toglPtr->stereo = tmp ? TOGL_STEREO_NATIVE : TOGL_STEREO_NONE;
    if (hasMultisampling) {
        (void) glXGetConfig(toglPtr->display, toglPtr->visInfo, GLX_SAMPLES, &tmp);
        toglPtr->multisampleFlag = (tmp != 0);
    }
    return True;
}

/*
 * Togl_CreateGLContext
 *
 * Creates an OpenGL rendering context for the widget.  It is called when the
 * widget is created, before it is mapped. For Windows and macOS, creating a
 * rendering context also requires creating the rendering surface, which is
 * an NSView on macOS and a child window on Windows.  These fill the rectangle
 * in the toplevel window occupied by the Togl widget.  GLX handles creation
 * of the rendering surface automatically.
 */

int
Togl_CreateGLContext(
    Togl *toglPtr)
{
    GLXContext context = NULL;
    GLXContext shareCtx = NULL;
    /* If this is false, GLX reports GLXBadFBConfig. */
    Bool direct = true;

    if (toglPtr->fbcfg == NULL) {
	int scrnum = Tk_ScreenNumber(toglPtr->tkwin);
	toglPtr->visInfo = togl_pixelFormat(toglPtr, scrnum);
    }
    switch(toglPtr->profile) {
    case PROFILE_LEGACY:
	context = glXCreateContextAttribsARB(toglPtr->display, toglPtr->fbcfg,
	    shareCtx, direct, attributes_2_1);
	break;
    case PROFILE_3_2:
	context = glXCreateContextAttribsARB(toglPtr->display, toglPtr->fbcfg,
	    shareCtx, direct, attributes_3_2);
	break;
    case PROFILE_4_1:
	context = glXCreateContextAttribsARB(toglPtr->display, toglPtr->fbcfg,
	    shareCtx, direct, attributes_4_1);
	break;
    default:
	context = glXCreateContext(toglPtr->display, toglPtr->visInfo,
	    shareCtx, direct);
	break;
    }
    if (context == NULL) {
	Tcl_SetResult(toglPtr->interp,
            "Failed to create GL rendering context", TCL_STATIC);
	return TCL_ERROR;
    }
    toglPtr->context = context;
    return TCL_OK;
}


/*
 * Togl_MakeWindow
 *
 * This is a callback function which is called by Tk_MakeWindowExist
 * when the togl widget is mapped.  It sets up the widget record and
 * does other Tk-related initialization.  This function is not allowed
 * to fail.  I must return a valid X window identifier.  If something
 * goes wrong, it sets the badWindow flag in the widget record,
 * which is passed as the instanceData.
 */

Window
Togl_MakeWindow(
    Tk_Window tkwin,
    Window parent,
    void* instanceData)
{
    Togl   *toglPtr = (Togl *) instanceData;
    Display *dpy;
    Colormap cmap;
    int     scrnum;
    Window  window = None;
    XSetWindowAttributes swa;
    int     width, height;

    if (toglPtr->badWindow) {
        return Tk_MakeWindow(tkwin, parent);
    }

    /* for color index mode photos */
    if (toglPtr->redMap) {
        free(toglPtr->redMap);
    }
    if (toglPtr->greenMap) {
        free(toglPtr->greenMap);
    }
    if (toglPtr->blueMap) {
        free(toglPtr->blueMap);
    }
    toglPtr->redMap = toglPtr->greenMap = toglPtr->blueMap = NULL;
    toglPtr->mapSize = 0;

    dpy = Tk_Display(tkwin);
    scrnum = Tk_ScreenNumber(tkwin);

    /*
     * Figure out which OpenGL context to use
     */

    if (toglPtr->pixelFormat) {
        XVisualInfo template;
        int     count = 0;

        template.visualid = toglPtr->pixelFormat;
        toglPtr->visInfo = XGetVisualInfo(dpy, VisualIDMask, &template, &count);
        if (toglPtr->visInfo == NULL) {
            Tcl_SetResult(toglPtr->interp,
                    "missing visual information", TCL_STATIC);
            goto error;
        }
        if (!togl_describePixelFormat(toglPtr)) {
            Tcl_SetResult(toglPtr->interp,
                    "couldn't choose pixel format", TCL_STATIC);
            goto error;
        }
    } else {
        toglPtr->visInfo = togl_pixelFormat(toglPtr, scrnum);
        if (toglPtr->visInfo == NULL) {
            goto error;
        }
    }

    /*
     * Create a new OpenGL rendering context.
     */

    if (toglPtr->shareList) {
        /* share display lists with existing togl widget */
        Togl   *shareWith = FindTogl(toglPtr, toglPtr->shareList);
        GLXContext shareCtx;
        int     error_code;

        if (shareWith) {
            shareCtx = shareWith->context;
            toglPtr->contextTag = shareWith->contextTag;
        } else {
            shareCtx = None;
        }
        if (shareCtx) {
            togl_SetupXErrorHandler();
        }
	if (Togl_CreateGLContext(toglPtr) != TCL_OK) {
            Tcl_SetResult(toglPtr->interp,
		 "Failed to create GL context", TCL_STATIC);
            goto error;

	}
        if (shareCtx && (error_code = togl_CheckForXError(toglPtr))) {
            char    buf[256];

            toglPtr->context = NULL;
            XGetErrorText(dpy, error_code, buf, sizeof buf);
            Tcl_AppendResult(toglPtr->interp,
                    "unable to share display lists: ", buf, NULL);
            goto error;
        }
    } else {
        if (toglPtr->shareContext && FindTogl(toglPtr, toglPtr->shareContext)) {
            /* share OpenGL context with existing Togl widget */
            Togl   *shareWith = FindTogl(toglPtr, toglPtr->shareContext);

            if (toglPtr->visInfo->visualid != shareWith->visInfo->visualid) {
                Tcl_SetResult(toglPtr->interp,
                        "unable to share OpenGL context",
                        TCL_STATIC);
                goto error;
            }
            toglPtr->context = shareWith->context;
        } else {
            /* don't share display lists */
            toglPtr->shareContext = False;
            if (Togl_CreateGLContext(toglPtr) != TCL_OK) {
                Tcl_SetResult(toglPtr->interp,
                        "failed to create GL context", TCL_STATIC);
		goto error;
	    }
	}
    }
    if (toglPtr->context == NULL) {
        Tcl_SetResult(toglPtr->interp,
                "could not create rendering context", TCL_STATIC);
        goto error;
    }
    if (toglPtr->pBufferFlag) {
        /* Don't need a colormap, nor overlay, nor be displayed */
        toglPtr->pbuf = togl_createPbuffer(toglPtr);
        if (!toglPtr->pbuf) {
            /* tcl result set in togl_createPbuffer */
            goto error;
        }
        window = Tk_MakeWindow(tkwin, parent);
        return window;
    }

    /*
     * find a colormap
     */
    if (toglPtr->rgbaFlag) {
        /* Colormap for RGB mode */
        cmap = get_rgb_colormap(dpy, scrnum, toglPtr->visInfo, tkwin);
    } else {
        /* Colormap for CI mode */
        if (toglPtr->privateCmapFlag) {
            /* need read/write colormap so user can store own color entries */
            cmap = XCreateColormap(dpy,
		       XRootWindow(dpy, toglPtr->visInfo->screen),
                       toglPtr->visInfo->visual, AllocAll);
        } else {
            if (toglPtr->visInfo->visual == DefaultVisual(dpy, scrnum)) {
                /* share default/root colormap */
                cmap = Tk_Colormap(tkwin);
            } else {
                /* make a new read-only colormap */
                cmap = XCreateColormap(dpy,
                        XRootWindow(dpy, toglPtr->visInfo->screen),
                        toglPtr->visInfo->visual, AllocNone);
            }
        }
    }

    /* Make sure Tk knows to switch to the new colormap when the cursor is over
     * this window when running in color index mode. */
    (void) Tk_SetWindowVisual(tkwin, toglPtr->visInfo->visual,
            toglPtr->visInfo->depth, cmap);
    swa.background_pixmap = None;
    swa.border_pixel = 0;
    swa.colormap = cmap;
    swa.event_mask = ALL_EVENTS_MASK;
    if (toglPtr->pBufferFlag) {
        width = height = 1;
    } else {
        width = toglPtr->width;
        height = toglPtr->height;
    }
    window = XCreateWindow(dpy, parent,
            0, 0, width, height,
            0, toglPtr->visInfo->depth, InputOutput, toglPtr->visInfo->visual,
            CWBackPixmap | CWBorderPixel | CWColormap | CWEventMask, &swa);
    /* Make sure window manager installs our colormap */
    (void) XSetWMColormapWindows(dpy, window, &window, 1);

    if (!toglPtr->doubleFlag) {
        int     dbl_flag;

        /* See if we requested single buffering but had to accept a double
         * buffered visual.  If so, set the GL draw buffer to be the front
         * buffer to simulate single buffering. */
        if (glXGetConfig(dpy, toglPtr->visInfo, GLX_DOUBLEBUFFER, &dbl_flag)) {
            if (dbl_flag) {
                glXMakeCurrent(dpy, window, toglPtr->context);
                glDrawBuffer(GL_FRONT);
                glReadBuffer(GL_FRONT);
            }
        }
    }
#if TOGL_USE_OVERLAY
    if (toglPtr->overlayFlag) {
        if (SetupOverlay(togl) == TCL_ERROR) {
            fprintf(stderr, "Warning: couldn't setup overlay.\n");
            toglPtr->overlayFlag = False;
        }
    }
#endif
    if (!toglPtr->rgbaFlag) {
        int     index_size;
        GLint   index_bits;

        glGetIntegerv(GL_INDEX_BITS, &index_bits);
        index_size = 1 << index_bits;
        if (toglPtr->mapSize != index_size) {
            if (toglPtr->redMap)
                free(toglPtr->redMap);
            if (toglPtr->greenMap)
                free(toglPtr->greenMap);
            if (toglPtr->blueMap)
                free(toglPtr->blueMap);
            toglPtr->mapSize = index_size;
            toglPtr->redMap = (GLfloat *) calloc(index_size, sizeof (GLfloat));
            toglPtr->greenMap = (GLfloat *) calloc(index_size, sizeof (GLfloat));
            toglPtr->blueMap = (GLfloat *) calloc(index_size, sizeof (GLfloat));
        }
    }
#ifdef HAVE_AUTOSTEREO
    if (toglPtr->stereo == TOGL_STEREO_NATIVE) {
        if (!toglPtr->as_initialized) {
            const char *autostereod;

            toglPtr->as_initialized = True;
            if ((autostereod = getenv("AUTOSTEREOD")) == NULL)
                autostereod = AUTOSTEREOD;
            if (autostereod && *autostereod) {
                if (ASInitialize(toglPtr->display, autostereod) == Success) {
                    toglPtr->ash = ASCreatedStereoWindow(dpy);
                }
            }
        } else {
            toglPtr->ash = ASCreatedStereoWindow(dpy);
        }
    }
#endif
    return window;

  error:
    toglPtr->badWindow = True;
    if (window == None) {
        window = Tk_MakeWindow(tkwin, parent);
    }
    return window;
}

/*
 * Togl_MakeCurrent
 *
 * This is the key function of the Togl widget in its role as the
 * manager of an NSOpenGL rendering context.  Must be called by
 * a GL client before drawing into the widget.
 */

void
Togl_MakeCurrent(
    const Togl *toglPtr)
{
    if (!toglPtr->context) {
	return;
    }
    Display *display = toglPtr ? toglPtr->display : glXGetCurrentDisplay();
    if (!display) {
	return;
    }
    GLXDrawable drawable;

    if (!toglPtr)
	drawable = None;
    else if (toglPtr->pBufferFlag)
	drawable = toglPtr->pbuf;
    else if (toglPtr->tkwin)
	drawable = Tk_WindowId(toglPtr->tkwin);
    else
	drawable = None;
    if (drawable == None) {
    }
    (void) glXMakeCurrent(display, drawable,
	     drawable ? toglPtr->context : NULL);
}


/*
 * Togl_SwapBuffers
 *
 * Called by the GL Client after updating the image.  If the Togl
 * is double-buffered it interchanges the front and back framebuffers.
 * otherwise it calls GLFlush.
 */

void
Togl_SwapBuffers(
    const Togl *toglPtr){
    if (toglPtr->doubleFlag) {
        glXSwapBuffers(Tk_Display(toglPtr->tkwin),
		       Tk_WindowId(toglPtr->tkwin));
    } else {
        glFlush();
    }
}

/*
 * ToglUpdate
 *
 * Called by ToglDisplay whenever the size of the Togl widget may
 * have changed.  On macOS it adjusts the frame of the NSView that
 * is being used as the rendering surface.  The other platforms
 * handle the size changes automatically.
 */

void
Togl_Update(
    const Togl *toglPtr) {
}

/*
 * Togl_GetExtensions
 *
 * Queries the rendering context for its extension string, a
 * space-separated list of the names of all supported GL extensions.
 * The string is cached in the widget record and the cached
 * string is returned in subsequent calls.
 */

const char* Togl_GetExtensions(
    Togl *toglPtr)
{
    int scrnum = Tk_ScreenNumber(toglPtr->tkwin);
    return glXQueryExtensionsString(toglPtr->display, scrnum);

}

void Togl_FreeResources(
    Togl *ToglPtr)
{
    // Does X11 need this?
}

void
Togl_WorldChanged(
    void* instanceData){
    printf("WorldChanged\n");
}

int
Togl_TakePhoto(
    Togl *toglPtr,
    Tk_PhotoHandle photo)
{
    printf("TakePhoto\n");
    return TCL_OK;
}

int
Togl_CopyContext(
    const Togl *from,
    const Togl *to,
    unsigned mask)
{
    printf("CopyContext\n");
    return TCL_OK;
}

/*
 * Return an X colormap to use for OpenGL RGB-mode rendering.
 * Input:  dpy - the X display
 *         scrnum - the X screen number
 *         visinfo - the XVisualInfo as returned by glXChooseVisual()
 * Return:  an X Colormap or 0 if there's a _serious_ error.
 */
static Colormap
get_rgb_colormap(Display *dpy,
        int scrnum, const XVisualInfo *visinfo, Tk_Window tkwin)
{
    Atom    hp_cr_maps;
    Status  status;
    int     numCmaps;
    int     i;
    XStandardColormap *standardCmaps;
    Window  root = XRootWindow(dpy, scrnum);
    Bool    using_mesa;

    /*
     * First check if visinfo's visual matches the default/root visual.
     */
    if (visinfo->visual == Tk_Visual(tkwin)) {
        /* use the default/root colormap */
        Colormap cmap;

        cmap = Tk_Colormap(tkwin);
#  ifdef MESA_COLOR_HACK
        (void) get_free_color_cells(dpy, scrnum, cmap);
#  endif
        return cmap;
    }

    /*
     * Check if we're using Mesa.
     */
    if (strstr(glXQueryServerString(dpy, scrnum, GLX_VERSION), "Mesa")) {
        using_mesa = True;
    } else {
        using_mesa = False;
    }

    /*
     * Next, if we're using Mesa and displaying on an HP with the "Color
     * Recovery" feature and the visual is 8-bit TrueColor, search for a
     * special colormap initialized for dithering.  Mesa will know how to
     * dither using this colormap.
     */
    if (using_mesa) {
        hp_cr_maps = XInternAtom(dpy, "_HP_RGB_SMOOTH_MAP_LIST", True);
        if (hp_cr_maps
#  ifdef __cplusplus
                && visinfo->visual->c_class == TrueColor
#  else
                && visinfo->visual->class == TrueColor
#  endif
                && visinfo->depth == 8) {
            status = XGetRGBColormaps(dpy, root, &standardCmaps,
                    &numCmaps, hp_cr_maps);
            if (status) {
                for (i = 0; i < numCmaps; i++) {
                    if (standardCmaps[i].visualid == visinfo->visual->visualid) {
                        Colormap cmap = standardCmaps[i].colormap;

                        (void) XFree(standardCmaps);
                        return cmap;
                    }
                }
                (void) XFree(standardCmaps);
            }
        }
    }

    /*
     * If we get here, give up and just allocate a new colormap.
     */
    return XCreateColormap(dpy, root, visinfo->visual, AllocNone);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
