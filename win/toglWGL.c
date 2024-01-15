/*
  This file implementats the following platform specific functions declared in
  togl.h.  They comprise the platform interface.

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
#include "tkWinInt.h" /* for TkWinDCState */
#include "tkIntPlatDecls.h" /* for TkWinChildProc */
#include "colormap.h"

#define TOGL_CLASS_NAME TEXT("Togl Class")
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <winnt.h>
#include <objbase.h>
#ifdef _MSC_VER
#  include <strsafe.h>
#  else
#    ifdef UNICODE
#      define StringCchPrintf snwprintf
#    else
#      define StringCchPrintf snprintf
#    endif
#endif

/*
 * In WGL, the Weird GL, construction of a GL rendering context requires
 * first that a device context be created.  A device context is a pointer
 * to an opaque type which describes a graphics card device driver.
 */

/*
 * These are static pointers to extension procedures provided by a graphics
 * card device driver, rather than by the openGL dynamic library.  These
 * cannot be initialized until a device context has been created.  Moreover,
 * these procedures may or may not be provided by any given driver.
 */

static PFNWGLCREATECONTEXTATTRIBSARBPROC   createContextAttribs = NULL;
static PFNWGLGETEXTENSIONSSTRINGARBPROC    getExtensionsString = NULL;
static PFNWGLCHOOSEPIXELFORMATARBPROC      choosePixelFormat = NULL;
static PFNWGLGETPIXELFORMATATTRIBIVARBPROC getPixelFormatAttribiv = NULL;
static PFNWGLCREATEPBUFFERARBPROC          createPbuffer = NULL;
static PFNWGLDESTROYPBUFFERARBPROC         destroyPbuffer = NULL;
static PFNWGLGETPBUFFERDCARBPROC           getPbufferDC = NULL;
static PFNWGLRELEASEPBUFFERDCARBPROC       releasePbufferDC = NULL;
static PFNWGLQUERYPBUFFERARBPROC           queryPbuffer = NULL;

static int hasMultisampling = FALSE;
static int hasPbuffer = FALSE;
static int hasARBPbuffer = FALSE;

/*
 * initializeDeviceProcs
 *
 * This function initializes the pointers above.  There must be a current
 * device context for this to have any effect.  We do not check the extensions
 * string to see if these procedures are available.  If one does not exist
 * wglGetProcAddress will return NULL and the function pointer will remain
 * NULL.  We do check if the pointer is NULL before setting a feature flag in
 * the widget record.
 */

static void
initializeDeviceProcs()
{
    createContextAttribs = (PFNWGLCREATECONTEXTATTRIBSARBPROC)
	wglGetProcAddress("wglCreateContextAttribsARB");
    getExtensionsString = (PFNWGLGETEXTENSIONSSTRINGARBPROC)
	wglGetProcAddress("wglGetExtensionsStringARB");
    /* First try to get the ARB versions of choosePixelFormat */
    choosePixelFormat = (PFNWGLCHOOSEPIXELFORMATARBPROC)
	wglGetProcAddress("wglChoosePixelFormatARB");
    getPixelFormatAttribiv = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)
	wglGetProcAddress("wglGetPixelFormatAttribivARB");
    if (choosePixelFormat == NULL || getPixelFormatAttribiv == NULL) {
	choosePixelFormat = NULL;
	getPixelFormatAttribiv = NULL;
    }
    /* If that fails, fall back to the EXT versions, which have the same
     *  signature, ignoring const.
    `*/
    if (choosePixelFormat == NULL) {
	choosePixelFormat = (PFNWGLCHOOSEPIXELFORMATARBPROC)
	    wglGetProcAddress("wglChoosePixelFrmatEXT");
	getPixelFormatAttribiv = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)
	    wglGetProcAddress("wglGetPixelFormatAttribivEXT");
	if (choosePixelFormat == NULL || getPixelFormatAttribiv == NULL) {
	    choosePixelFormat = NULL;
	    getPixelFormatAttribiv = NULL;
	}
    }
    createPbuffer = (PFNWGLCREATEPBUFFERARBPROC)
	wglGetProcAddress("wglCreatePbufferARB");
    destroyPbuffer = (PFNWGLDESTROYPBUFFERARBPROC)
	wglGetProcAddress("wglDestroyPbufferARB");
    getPbufferDC = (PFNWGLGETPBUFFERDCARBPROC)
	wglGetProcAddress("wglGetPbufferDCARB");
    releasePbufferDC = (PFNWGLRELEASEPBUFFERDCARBPROC)
	wglGetProcAddress("wglReleasePbufferDCARB");
    queryPbuffer = (PFNWGLQUERYPBUFFERARBPROC)           
	wglGetProcAddress("wglQueryPbufferARB");
}

static Bool ToglClassInitialized = False;

static LRESULT CALLBACK
Win32WinProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT answer;
    Togl   *toglPtr = (Togl *) GetWindowLongPtr(hwnd, 0);
    
    switch (message) {

      case WM_ERASEBKGND:
          /* We clear our own window */
          return 1;

      case WM_WINDOWPOSCHANGED:
          /* Should be processed by DefWindowProc, otherwise a double buffered
           * context is not properly resized when the corresponding window is
           * resized. */
          break;

      case WM_DESTROY:
          if (toglPtr && toglPtr->tkwin != NULL) {
              if (toglPtr->setGrid > 0) {
                  Tk_UnsetGrid(toglPtr->tkwin);
              }
              (void) Tcl_DeleteCommandFromToken(toglPtr->interp,
			 toglPtr->widgetCmd);
          }
          break;

      case WM_DISPLAYCHANGE:
          if (toglPtr->pBufferFlag && hasARBPbuffer && !toglPtr->pBufferLost) {
              queryPbuffer(toglPtr->pbuf, WGL_PBUFFER_LOST_ARB,
                      &toglPtr->pBufferLost);
          }
	  
      default:
          return TkWinChildProc(hwnd, message, wParam, lParam);
    }
    answer = DefWindowProc(hwnd, message, wParam, lParam);
    Tcl_ServiceAll();
    return answer;
}

/*
 * toglCreateDummyWindow
 *
 * This Windows-only static function creates a WGL device context and saves a
 * pointer to it in the Togl widget record.  The recommended and possibly only
 * way to do this is to create a hidden window which owns a device context and
 * call GetDC.  Once we have saved the device context we can destroy the
 * window.
 */

static HWND
toglCreateDummyWindow()
{
    static char ClassName[] = "ToglFakeWindow";
    WNDCLASS wc;
    HINSTANCE instance = GetModuleHandle(NULL);
    HWND    wnd;
    HDC     dc;
    PIXELFORMATDESCRIPTOR pfd;
    int     pixelFormat;
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = instance;
    wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = ClassName;
    if (!RegisterClass(&wc)) {
        DWORD   err = GetLastError();

        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            fprintf(stderr, "Unable to register Togl Test Window class\n");
            return NULL;
        }
    }
    wnd = CreateWindow(ClassName, "create WGL device context",
            WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            0, 0, 1, 1, NULL, NULL, instance, NULL);
    if (wnd == NULL) {
        fprintf(stderr, "Unable to create temporary OpenGL window\n");
        return NULL;
    }
    dc = GetDC(wnd);
    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 3;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pixelFormat = ChoosePixelFormat(dc, &pfd);
    if (pixelFormat == 0) {
        fprintf(stderr, "Unable to choose simple pixel format\n");
        ReleaseDC(wnd, dc);
        return NULL;
    }
    if (!SetPixelFormat(dc, pixelFormat, NULL)) {
        fprintf(stderr, "Unable to set simple pixel format\n");
        ReleaseDC(wnd, dc);
        return NULL;
    }
    ShowWindow(wnd, SW_HIDE);   // make sure it's hidden
    ReleaseDC(wnd, dc);
    return wnd;
}


static HPBUFFERARB
togl_createPbuffer(Togl *toglPtr)
{
    int     attribs[32];
    int     na = 0;
    HPBUFFERARB pbuf;

    if (toglPtr->largestPbufferFlag) {
        attribs[na++] = WGL_PBUFFER_LARGEST_ARB;
        attribs[na++] = 1;
    }
    attribs[na] = 0;
    pbuf = createPbuffer(toglPtr->deviceContext, (int) toglPtr->pixelFormat,
			 toglPtr->width, toglPtr->height, attribs);
    if (pbuf && toglPtr->largestPbufferFlag) {
        queryPbuffer(pbuf, WGL_PBUFFER_WIDTH_ARB, &toglPtr->width);
        queryPbuffer(pbuf, WGL_PBUFFER_HEIGHT_ARB, &toglPtr->height);
    }
    return pbuf;
}

static void
togl_destroyPbuffer(Togl *toglPtr)
{
    destroyPbuffer(toglPtr->pbuf);
}

static int
togl_describePixelFormat(Togl *toglPtr)
{
    if (getPixelFormatAttribiv == NULL) {
        PIXELFORMATDESCRIPTOR pfd;

        DescribePixelFormat(toglPtr->deviceContext, (int) toglPtr->pixelFormat,
                sizeof (pfd), &pfd);
        /* fill in flags normally passed in that affect behavior */
        toglPtr->rgbaFlag = pfd.iPixelType == PFD_TYPE_RGBA;
        toglPtr->doubleFlag = (pfd.dwFlags & PFD_DOUBLEBUFFER) != 0;
        toglPtr->depthFlag = (pfd.cDepthBits != 0);
        toglPtr->accumFlag = (pfd.cAccumBits != 0);
        toglPtr->alphaFlag = (pfd.cAlphaBits != 0);
        toglPtr->stencilFlag = (pfd.cStencilBits != 0);
        if ((pfd.dwFlags & PFD_STEREO) != 0)
            toglPtr->stereo = TOGL_STEREO_NATIVE;
        else
            toglPtr->stereo = TOGL_STEREO_NONE;
    } else {
        static int attribs[] = {
            WGL_PIXEL_TYPE_ARB,
            WGL_DOUBLE_BUFFER_ARB,
            WGL_DEPTH_BITS_ARB,
            WGL_ACCUM_RED_BITS_ARB,
            WGL_ALPHA_BITS_ARB,
            WGL_STENCIL_BITS_ARB,
            WGL_STEREO_ARB,
            WGL_SAMPLES_ARB
        };
#define NUM_ATTRIBS (sizeof attribs / sizeof attribs[0])
        int     info[NUM_ATTRIBS];

        getPixelFormatAttribiv(toglPtr->deviceContext, (int) toglPtr->pixelFormat, 0,
                NUM_ATTRIBS, attribs, info);
#undef NUM_ATTRIBS
        toglPtr->rgbaFlag = info[0];
        toglPtr->doubleFlag = info[1];
        toglPtr->depthFlag = (info[2] != 0);
    toglPtr->accumFlag = (info[3] != 0);
        toglPtr->alphaFlag = (info[4] != 0);
        toglPtr->stencilFlag = (info[5] != 0);
        toglPtr->stereo = info[6] ? TOGL_STEREO_NATIVE : TOGL_STEREO_NONE;
        toglPtr->multisampleFlag = (info[7] != 0);
    }
    return True;
}

/*
 * Togl_CreateGLContext
 *
 * Creates an OpenGL rendering context for the widget.  It is called when the
 * widget is created, before it is mapped. For Windows and macOS, creating a
 * rendering context also requires creating the rendering surface, which is
 * a child window on Windows.  The child occupies  the rectangle
 * in the toplevel window belonging to the Togl widget.
 *
 * On Windows it is necessary to create a dummy rendering context, associate
 * with a hidden window, in order to query the OpenGL server to find out what
 * pixel formats are available and to obtain pointers to functions needed to
 * create a rendering context.  These functions are provided of the graphics
 * card driver rather than by the openGL library.
 *  
 * The OpenGL documentation acknowledges that this is weird, but proclaims
 * that it is just how WGL works.  So there.
 */

static const int attributes_2_1[] = {    
    WGL_CONTEXT_MAJOR_VERSION_ARB, 2,
    WGL_CONTEXT_MINOR_VERSION_ARB, 1,
    0                                       
};                                          

static const int attributes_3_0[] = {    
    WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
    WGL_CONTEXT_MINOR_VERSION_ARB, 0,
    0                                       
};                                          

static const int attributes_3_2[] = {       
    WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
    WGL_CONTEXT_MINOR_VERSION_ARB, 2,
    0                                       
};                                          
                                            
static const int attributes_4_1[] = {       
    WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
    WGL_CONTEXT_MINOR_VERSION_ARB, 1,
    0                                       
};                                          

/* Used for the hidden test window. */
static const int attribList[] = {
    WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
    WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
    WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
    WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
    WGL_COLOR_BITS_ARB,     24,
    WGL_ALPHA_BITS_ARB,     8,	
    WGL_DEPTH_BITS_ARB,     24,
    WGL_STENCIL_BITS_ARB,   8,
    /* NOTE: these produce a broken context on my system
       which supports 4.6
       WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
       WGL_CONTEXT_MINOR_VERSION_ARB, 1,
    */
    0,
};

static int
toglCreateChildWindow(
    Togl *toglPtr)
{
    HWND    parentWin;
    DWORD   style;
    int     width, height;
    HINSTANCE hInstance= Tk_GetHINSTANCE();
    Bool    createdPbufferDC = False;
    // We assume this is called with the dummy context current
    // and with a pixelformat stored in the widget record.
    
    if (!ToglClassInitialized) {
        WNDCLASS ToglClass;

        ToglClassInitialized = True;
        ToglClass.style = CS_HREDRAW | CS_VREDRAW;
        ToglClass.cbClsExtra = 0;
        ToglClass.cbWndExtra = sizeof (LONG_PTR);       /* to save Togl* */
        ToglClass.hInstance = hInstance;
        ToglClass.hbrBackground = NULL;
        ToglClass.lpszMenuName = NULL;
        ToglClass.lpszClassName = TOGL_CLASS_NAME;
        ToglClass.lpfnWndProc = Win32WinProc;
        ToglClass.hIcon = NULL;
        ToglClass.hCursor = NULL;
        if (!RegisterClass(&ToglClass)) {
            Tcl_SetResult(toglPtr->interp,
                    "unable register Togl window class", TCL_STATIC);
            goto error;
        }
    }

    if (!toglPtr->pBufferFlag) {
	//parentWin = Tk_GetHWND(parent);
	parentWin = Tk_GetHWND(Tk_WindowId(Tk_Parent(toglPtr->tkwin)));
        style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    } else {
        parentWin = NULL;
        style = WS_POPUP | WS_CLIPCHILDREN;
    }
    if (toglPtr->pBufferFlag) {
        width = height = 1;     /* TODO: demo code mishaves when set to 1000 */
    } else {
        width = toglPtr->width;
        height = toglPtr->height;
    }
    toglPtr->child = CreateWindowEx(WS_EX_NOPARENTNOTIFY, TOGL_CLASS_NAME,
	 NULL, style, 0, 0, width, height, parentWin, NULL, hInstance, NULL);
    if (!toglPtr->child) {
      char *msg;
      DWORD errorcode = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		    NULL, errorcode, 0, (LPSTR)&msg, 0, NULL);
      fprintf(stderr, "%s\n", msg);
      goto error;
    }
    SetWindowLongPtr(toglPtr->child, 0, (LONG_PTR) toglPtr);
    SetWindowPos(toglPtr->child, HWND_TOP, 0, 0, 0, 0,
		 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    if (toglPtr->pBufferFlag) {
        ShowWindow(toglPtr->child, SW_HIDE); /* make sure it's hidden */
    }

    /* Update the widget record from the pixel formatl. */
    if (!togl_describePixelFormat(toglPtr)) {
	Tcl_SetResult(toglPtr->interp,
	    "Pixel format is not consistent with widget configurait.",
	    TCL_STATIC);
	goto error;
    }
    if (toglPtr->pBufferFlag) {
        toglPtr->pbuf = togl_createPbuffer(toglPtr);
        if (toglPtr->pbuf == NULL) {
            Tcl_SetResult(toglPtr->interp,
                    "couldn't create pbuffer", TCL_STATIC);
            goto error;
        }
        ReleaseDC(toglPtr->child, toglPtr->deviceContext);
        toglPtr->deviceContext = getPbufferDC(toglPtr->pbuf);
        createdPbufferDC = True;
    } else {
	/* nstalling the pixelFormat in the child device context */
	toglPtr->deviceContext = GetDC(toglPtr->child);
	// FIX ME: release it when the child is destroyed.
	bool result = SetPixelFormat(toglPtr->deviceContext,
				     (int) toglPtr->pixelFormat, NULL);
	if (result == FALSE) {
	    Tcl_SetResult(toglPtr->interp,
		"Couldn't set child's pixel format", TCL_STATIC);
	    goto error;
	}
    }
    
    /* 
     * Create an OpenGL rendering context for the child, if necessary.
     */
    
    if (toglPtr->shareContext &&
	FindTogl(toglPtr, toglPtr->shareContext)) {
        /* share OpenGL context with existing Togl widget */
        Togl   *shareWith = FindTogl(toglPtr, toglPtr->shareContext);

        if (toglPtr->pixelFormat != shareWith->pixelFormat) {
            Tcl_SetResult(toglPtr->interp,
                    "Unable to share OpenGL context.", TCL_STATIC);
            goto error;
        }
        toglPtr->context = shareWith->context;
    } else {
	int *attributes = NULL;
	switch(toglPtr->profile) {
	case PROFILE_LEGACY:
	    attributes = attributes_2_1;
	    break;
	case PROFILE_3_2:
	    attributes = attributes_3_2;
	    break;
	case PROFILE_4_1:
	    attributes = attributes_4_1;
	    break;
	case PROFILE_SYSTEM:
	    break;
	}
	if (createContextAttribs && attributes) {
	    toglPtr->context = createContextAttribs(
	        toglPtr->deviceContext, 0, attributes);
	    Togl_MakeCurrent(toglPtr);
	} else {
	    fprintf(stderr,
	    "WARNING: wglCreateContextAttribsARB is not being used.\n"
	    "Your GL version will depend on your graphics driver.\n");
	}
    }
    if (toglPtr->shareList) {
        /* share display lists with existing togl widget */
        Togl *shareWith = FindTogl(toglPtr, toglPtr->shareList);

        if (shareWith) {
            if (!wglShareLists(shareWith->context, toglPtr->context)) {
                Tcl_SetResult(toglPtr->interp,
                        "unable to share display lists", TCL_STATIC);
                goto error;
            }
            toglPtr->contextTag = shareWith->contextTag;
        }
    }
    if (toglPtr->context == NULL) {
        Tcl_SetResult(toglPtr->interp,
                "Could not create rendering context", TCL_STATIC);
        goto error;
    }    
    return TCL_OK;

 error:
    toglPtr->badWindow = True;
    if (toglPtr->deviceContext) {
        if (createdPbufferDC) {
            releasePbufferDC(toglPtr->pbuf, toglPtr->deviceContext);
        }
	if (toglPtr->child) {
	    if (toglPtr->deviceContext) {
                ReleaseDC(toglPtr->child, toglPtr->deviceContext);
		toglPtr->deviceContext = NULL;
	    }
	    DestroyWindow(toglPtr->child);
	    toglPtr->child = NULL;
	}
    }
    return TCL_ERROR;    
}

int
Togl_CreateGLContext(
    Togl *toglPtr)
{
    HDC     dc;  /* Device context handle */
    HGLRC   rc;  /* Rendering context handle */
    HWND    dummy = NULL;
    int pixelFormat;
    UINT numFormats;

    dummy = toglCreateDummyWindow();
    if (dummy == NULL) {
	Tcl_SetResult(toglPtr->interp,
		      "can't create dummy OpenGL window",
		      TCL_STATIC);
	return 0;
    }
    dc = GetDC(dummy);
    rc = wglCreateContext(dc);
    wglMakeCurrent(dc, rc);
    ReleaseDC(dummy, dc);

    /*
     * Now that we have a device context we can use wglGetProcAddress to fill
     * in our function pointers.
     */
    initializeDeviceProcs();

    /* Cache the extension string in the widget record. */
    toglPtr->extensions = (const char *) getExtensionsString(dc);

    glGetIntegerv(GL_MAJOR_VERSION, &toglPtr->glmajor);
    glGetIntegerv(GL_MINOR_VERSION, &toglPtr->glminor);

    /* Check for multisampling. */
    if (strstr(toglPtr->extensions, "WGL_ARB_multisample") != NULL
	|| strstr(toglPtr->extensions, "WGL_EXT_multisample") != NULL) {
	hasMultisampling = TRUE;
    }

    /* Get a suitable pixel format. */
    if (choosePixelFormat == NULL) {
	Tcl_SetResult(toglPtr->interp,
	    "Neither wglChoosePixelFormatARB nor wglChoosePixelFormatEXT "
	    "are available in this openGL.\n"
	    "We cannot create an OpenGL rendering context.",
	    TCL_STATIC);
	return TCL_ERROR;
    }
    choosePixelFormat(dc, attribList, NULL, 1, &pixelFormat,
			  &numFormats);

    /* Save the pixel format and contexts in the widget record. */
    toglPtr->pixelFormat = pixelFormat;
    if (toglCreateChildWindow(toglPtr) != TCL_OK) {
	fprintf(stderr, "Failed to create child window.\n");
	return TCL_ERROR;
    }

    toglCreateChildWindow(toglPtr);
    
    /* Destroy the dummy window. */
    if (dummy != NULL) {
	DestroyWindow(dummy);
    } else {
	return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 * Togl_MakeWindow
 *
 * This is a callback function registered to be called by Tk_MakeWindowExist
 * when the togl widget is mapped. It sets up the widget record and does
 * other Tk-related initialization.  This function is not allowed to fail.  It
 * must return a valid X window identifier.  If something goes wrong, it sets
 * the badWindow flag in the widget record, which is passed as the
 * instanceData.
 */

Window
Togl_MakeWindow(Tk_Window tkwin, Window parent, ClientData instanceData)
{
    Togl   *toglPtr = (Togl *) instanceData;
    Display *dpy = Tk_Display(tkwin);
    int     scrnum = Tk_ScreenNumber(tkwin);
    Colormap cmap;
    Window  window = None;
    PIXELFORMATDESCRIPTOR pfd;
    Bool    createdPbufferDC = False;

    if (toglPtr->badWindow) {
	/*
	 * This function has been called before and it failed.  This test
	 * exists because this callback function is not allowed to fail.  It
	 * must return a valid X Window Id.
	 */
        return Tk_MakeWindow(tkwin, parent);
    }
    /* We require that ToglCreateGLContext has been called. */
    if (toglPtr->child) {
	window = Tk_AttachHWND(tkwin, toglPtr->child);
    } else {
	goto error;
    }
    
    /* For color index mode photos */
    if (toglPtr->redMap)
        free(toglPtr->redMap);
    if (toglPtr->greenMap)
        free(toglPtr->greenMap);
    if (toglPtr->blueMap)
        free(toglPtr->blueMap);
    toglPtr->redMap = toglPtr->greenMap = toglPtr->blueMap = NULL;
    toglPtr->mapSize = 0;

    if ( toglPtr->pBufferFlag) {
	DescribePixelFormat(toglPtr->deviceContext,
           (int) toglPtr->pixelFormat, sizeof (pfd), &pfd);
       }
    
    if ( toglPtr->pBufferFlag) {
	/* Don't need a colormap, nor overlay, nor be displayed */
	goto done;
    }

    /* 
     * find a colormap
     */
    DescribePixelFormat(toglPtr->deviceContext,
	(int) toglPtr->pixelFormat, sizeof (pfd), &pfd);
    if (toglPtr->rgbaFlag) {
        /* Colormap for RGB mode */
        if (pfd.dwFlags & PFD_NEED_PALETTE) {
            cmap = Win32CreateRgbColormap(pfd);
        } else {
            cmap = DefaultColormap(dpy, scrnum);
        }
    } else {
        /* Colormap for CI mode */
        /* this logic is to overcome a combination driver/compiler bug: (1)
         * cColorBits may be unusally large (e.g., 32 instead of 8 or 12) and
         * (2) 1 << 32 might be 1 instead of zero (gcc for ia32) */
        if (pfd.cColorBits >= MAX_CI_COLORMAP_BITS) {
            toglPtr->ciColormapSize = MAX_CI_COLORMAP_SIZE;
        } else {
            toglPtr->ciColormapSize = 1 << pfd.cColorBits;
            if (toglPtr->ciColormapSize >= MAX_CI_COLORMAP_SIZE)
                toglPtr->ciColormapSize = MAX_CI_COLORMAP_SIZE;
        }
        if (toglPtr->privateCmapFlag) {
            /* need read/write colormap so user can store own color entries */
            cmap = Win32CreateCiColormap(toglPtr);
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

    /* Install the colormap */
    SelectPalette(toglPtr->deviceContext, ((TkWinColormap *) cmap)->palette, TRUE);
    RealizePalette(toglPtr->deviceContext);

    if (!toglPtr->doubleFlag) {
        /* See if we requested single buffering but had to accept a double
         * buffered visual.  If so, set the GL draw buffer to be the front
         * buffer to simulate single buffering. */
        if (getPixelFormatAttribiv == NULL) {
            /* pfd is already set */
            if ((pfd.dwFlags & PFD_DOUBLEBUFFER) != 0) {
                wglMakeCurrent(toglPtr->deviceContext, toglPtr->context);
                glDrawBuffer(GL_FRONT);
                glReadBuffer(GL_FRONT);
            }
        } else {
            static int attribs[] = {
                WGL_DOUBLE_BUFFER_ARB,
            };
#define NUM_ATTRIBS (sizeof attribs / sizeof attribs[0])
            int     info[NUM_ATTRIBS];

            getPixelFormatAttribiv(toglPtr->deviceContext,
		(int) toglPtr->pixelFormat, 0, NUM_ATTRIBS, attribs, info);
#undef NUM_ATTRIBS
            if (info[0]) {
                wglMakeCurrent(toglPtr->deviceContext, toglPtr->context);
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

        index_size = toglPtr->ciColormapSize;
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

    /* Create visual info */
    if (toglPtr->visInfo == NULL) {
        /* 
         * Create a new OpenGL rendering context. And check whether
	 * to share lists.
         */
        Visual *visual;
        /* Just for portability, define the simplest visinfo */
        visual = DefaultVisual(dpy, scrnum);
        toglPtr->visInfo = (XVisualInfo *) calloc(1, sizeof (XVisualInfo));
        toglPtr->visInfo->screen = scrnum;
        toglPtr->visInfo->visual = visual;
        toglPtr->visInfo->visualid = visual->visualid;
#if defined(__cplusplus) || defined(c_plusplus)
        toglPtr->visInfo->c_class = visual->c_class;
        toglPtr->visInfo->depth = visual->bits_per_rgb;
#endif
    }
    /*
     * Make sure Tk knows to switch to the new colormap when the cursor is
     * over this window when running in color index mode.
     */
    (void) Tk_SetWindowVisual(tkwin, toglPtr->visInfo->visual,
            toglPtr->visInfo->depth, cmap);

 done:
    return window;

 error:
    toglPtr->badWindow = True;
    if (toglPtr->deviceContext) {
        if (createdPbufferDC) {
            releasePbufferDC(toglPtr->pbuf, toglPtr->deviceContext);
        }
        toglPtr->deviceContext = NULL;
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
    bool result = wglMakeCurrent(toglPtr->deviceContext,
				 toglPtr->context);
    if (!result) {
	fprintf(stderr, "wglMakeCurrent failed\n");
    }
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
    const Togl *toglPtr)
{
    if (toglPtr->doubleFlag) {
        int result = SwapBuffers(toglPtr->deviceContext);
	if (!result) {
	    fprintf(stderr, "SwapBuffers failed\n");
	}
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
    /*
     * We already requested, and cached the extensions string in
     * Togl_CreateGLContext, so we can just return the cached string.
     */

    return toglPtr->extensions;
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


void
Togl_FreeResources(
    Togl *toglPtr)
{
    wglMakeCurrent(NULL, NULL);
    if (toglPtr->deviceContext) {
        ReleaseDC(toglPtr->child, toglPtr->deviceContext);
	toglPtr->deviceContext = NULL;
	if (toglPtr->pBufferFlag) {
	    releasePbufferDC(toglPtr->pbuf, toglPtr->deviceContext);
	}
    }
    if (toglPtr->context) {
	if (FindToglWithSameContext(toglPtr) == NULL) {
	    wglDeleteContext(toglPtr->context);
	    toglPtr->context = NULL;
	    if (toglPtr->visInfo) {
		free(toglPtr->visInfo);
		toglPtr->visInfo = NULL;
	    }
	}
    }
    if (toglPtr->child) {
	DestroyWindow(toglPtr->child);
    }
}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
