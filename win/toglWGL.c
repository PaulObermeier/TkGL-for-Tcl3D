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
#include "tkWinInt.h" /* for TkWinDCState */
#include "tkIntPlatDecls.h" /* for TkWinChildProc */
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

/* Maximum size of a logical palette corresponding to a colormap in color index 
 * mode. */
#  define MAX_CI_COLORMAP_SIZE 4096
#  define MAX_CI_COLORMAP_BITS 12

static Bool ToglClassInitialized = False;

/* TODO: move these statics into global structure */
static PFNWGLGETEXTENSIONSSTRINGARBPROC getExtensionsString = NULL;
static PFNWGLCHOOSEPIXELFORMATARBPROC choosePixelFormat;
static PFNWGLGETPIXELFORMATATTRIBIVARBPROC getPixelFormatAttribiv;
static PFNWGLCREATEPBUFFERARBPROC createPbuffer = NULL;
static PFNWGLDESTROYPBUFFERARBPROC destroyPbuffer = NULL;
static PFNWGLGETPBUFFERDCARBPROC getPbufferDC = NULL;
static PFNWGLRELEASEPBUFFERDCARBPROC releasePbufferDC = NULL;
static PFNWGLQUERYPBUFFERARBPROC queryPbuffer = NULL;
static PFNWGLCREATECONTEXTATTRIBSARBPROC createContextAttribs = NULL;
static int hasMultisampling = FALSE;
static int hasPbuffer = FALSE;
static int hasARBPbuffer = FALSE;

/* Code to create RGB palette is taken from the GENGL sample program of Win32
 * SDK */

static const unsigned char threeto8[8] = {
    0, 0111 >> 1, 0222 >> 1, 0333 >> 1, 0444 >> 1, 0555 >> 1, 0666 >> 1, 0377
};

static const unsigned char twoto8[4] = {
    0, 0x55, 0xaa, 0xff
};

static const unsigned char oneto8[2] = {
    0, 255
};

static const int defaultOverride[13] = {
    0, 3, 24, 27, 64, 67, 88, 173, 181, 236, 247, 164, 91
};

static const PALETTEENTRY defaultPalEntry[20] = {
    {0, 0, 0, 0},
    {0x80, 0, 0, 0},
    {0, 0x80, 0, 0},
    {0x80, 0x80, 0, 0},
    {0, 0, 0x80, 0},
    {0x80, 0, 0x80, 0},
    {0, 0x80, 0x80, 0},
    {0xC0, 0xC0, 0xC0, 0},

    {192, 220, 192, 0},
    {166, 202, 240, 0},
    {255, 251, 240, 0},
    {160, 160, 164, 0},

    {0x80, 0x80, 0x80, 0},
    {0xFF, 0, 0, 0},
    {0, 0xFF, 0, 0},
    {0xFF, 0xFF, 0, 0},
    {0, 0, 0xFF, 0},
    {0xFF, 0, 0xFF, 0},
    {0, 0xFF, 0xFF, 0},
    {0xFF, 0xFF, 0xFF, 0}
};

static unsigned char
ComponentFromIndex(int i, UINT nbits, UINT shift)
{
    unsigned char val;

    val = (unsigned char) (i >> shift);
    switch (nbits) {

      case 1:
          val &= 0x1;
          return oneto8[val];

      case 2:
          val &= 0x3;
          return twoto8[val];

      case 3:
          val &= 0x7;
          return threeto8[val];

      default:
          return 0;
    }
}

static Colormap
Win32CreateRgbColormap(PIXELFORMATDESCRIPTOR pfd)
{
    TkWinColormap *cmap = (TkWinColormap *) ckalloc(sizeof (TkWinColormap));
    LOGPALETTE *pPal;
    int     n, i;

    n = 1 << pfd.cColorBits;
    pPal = (PLOGPALETTE) LocalAlloc(LMEM_FIXED, sizeof (LOGPALETTE)
            + n * sizeof (PALETTEENTRY));
    pPal->palVersion = 0x300;
    pPal->palNumEntries = n;
    for (i = 0; i < n; i++) {
        pPal->palPalEntry[i].peRed =
                ComponentFromIndex(i, pfd.cRedBits, pfd.cRedShift);
        pPal->palPalEntry[i].peGreen =
                ComponentFromIndex(i, pfd.cGreenBits, pfd.cGreenShift);
        pPal->palPalEntry[i].peBlue =
                ComponentFromIndex(i, pfd.cBlueBits, pfd.cBlueShift);
        pPal->palPalEntry[i].peFlags = 0;
    }

    /* fix up the palette to include the default GDI palette */
    if ((pfd.cColorBits == 8)
            && (pfd.cRedBits == 3) && (pfd.cRedShift == 0)
            && (pfd.cGreenBits == 3) && (pfd.cGreenShift == 3)
            && (pfd.cBlueBits == 2) && (pfd.cBlueShift == 6)) {
        for (i = 1; i <= 12; i++)
            pPal->palPalEntry[defaultOverride[i]] = defaultPalEntry[i];
    }

    cmap->palette = CreatePalette(pPal);
    LocalFree(pPal);
    cmap->size = n;
    cmap->stale = 0;

    /* Since this is a private colormap of a fix size, we do not need a valid
     * hash table, but a dummy one */

    Tcl_InitHashTable(&cmap->refCounts, TCL_ONE_WORD_KEYS);
    return (Colormap) cmap;
}

static Colormap
Win32CreateCiColormap(Togl *toglPtr)
{
    /* Create a colormap with size of togl->ciColormapSize and set all entries
     * to black */

    LOGPALETTE logPalette;
    TkWinColormap *cmap = (TkWinColormap *) ckalloc(sizeof (TkWinColormap));

    logPalette.palVersion = 0x300;
    logPalette.palNumEntries = 1;
    logPalette.palPalEntry[0].peRed = 0;
    logPalette.palPalEntry[0].peGreen = 0;
    logPalette.palPalEntry[0].peBlue = 0;
    logPalette.palPalEntry[0].peFlags = 0;

    cmap->palette = CreatePalette(&logPalette);
    cmap->size = toglPtr->ciColormapSize;
    ResizePalette(cmap->palette, cmap->size);   /* sets new entries to black */
    cmap->stale = 0;

    /* Since this is a private colormap of a fix size, we do not need a valid
     * hash table, but a dummy one */

    Tcl_InitHashTable(&cmap->refCounts, TCL_ONE_WORD_KEYS);
    return (Colormap) cmap;
}

/* ErrorExit is from <http://msdn2.microsoft.com/en-us/library/ms680582.aspx> */
static void
ErrorExit(LPTSTR lpszFunction)
{
    /* Retrieve the system error message for the last-error code */
    LPTSTR  lpMsgBuf;
    LPTSTR  lpDisplayBuf;
    DWORD   err = GetLastError();

    if (err == 0) {
        /* The function said it failed, but GetLastError says it didn't, so
         * pretend it didn't. */
        return;
    }

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf, 0, NULL);

    /* Display the error message and exit the process */

    lpDisplayBuf = (LPTSTR) LocalAlloc(LMEM_ZEROINIT,
            (lstrlen(lpMsgBuf) + lstrlen(lpszFunction) + 40) * sizeof (TCHAR));
    StringCchPrintf(lpDisplayBuf, LocalSize(lpDisplayBuf),
            TEXT("%s failed with error %ld: %s"), lpszFunction, err, lpMsgBuf);
    MessageBox(NULL, lpDisplayBuf, TEXT("Error"), MB_OK);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(err);
}

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
              (void) Tcl_DeleteCommandFromToken(toglPtr->interp, toglPtr->widgetCmd);
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

static HWND
toglCreateTestWindow(HWND parent)
{
    static char ClassName[] = "ToglTestWindow";
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

    wnd = CreateWindow(ClassName, "test OpenGL capabilities",
            WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            0, 0, 1, 1, parent, NULL, instance, NULL);
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
    if (!SetPixelFormat(dc, pixelFormat, &pfd)) {
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
 * Togl_MakeWindow
 *
 *   Window creation function, invoked as a callback from Tk_MakeWindowExist.
 */

Window
Togl_MakeWindow(Tk_Window tkwin, Window parent, ClientData instanceData)
{
    Togl   *toglPtr = (Togl *) instanceData;
    Display *dpy;
    Colormap cmap;
    int     scrnum;
    Window  window = None;
    HWND    hwnd, parentWin;
    DWORD   style;
    HINSTANCE hInstance;
    PIXELFORMATDESCRIPTOR pfd;
    int     width, height;
    Bool    createdPbufferDC = False;

    if (toglPtr->badWindow) {
        return Tk_MakeWindow(tkwin, parent);
    }

    /* for color index mode photos */
    if (toglPtr->redMap)
        free(toglPtr->redMap);
    if (toglPtr->greenMap)
        free(toglPtr->greenMap);
    if (toglPtr->blueMap)
        free(toglPtr->blueMap);
    toglPtr->redMap = toglPtr->greenMap = toglPtr->blueMap = NULL;
    toglPtr->mapSize = 0;

    dpy = Tk_Display(tkwin);
    scrnum = Tk_ScreenNumber(tkwin);

    /* 
     * Windows and Mac OS X need the window created before OpenGL context
     * is created.  So do that now and set the window variable. 
     */
    hInstance = Tk_GetHINSTANCE();
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

    /* duplicate tkpMakeWindow logic from tk8.[45]/win/tkWinWindow.c */
    if (parent != None) {
        parentWin = Tk_GetHWND(parent);
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
    hwnd = CreateWindowEx(WS_EX_NOPARENTNOTIFY, TOGL_CLASS_NAME, NULL, style,
            0, 0, width, height, parentWin, NULL, hInstance, NULL);
    if (!hwnd) {
      char *msg;
      DWORD errorcode = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		    NULL, errorcode, 0, (LPSTR)&msg, 0, NULL);
    }
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
		 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    window = Tk_AttachHWND(tkwin, hwnd);
    SetWindowLongPtr(hwnd, 0, (LONG_PTR) toglPtr);
    if (toglPtr->pBufferFlag) {
        ShowWindow(hwnd, SW_HIDE);      /* make sure it's hidden */
    }

    /* 
     * Figure out which OpenGL context to use
     */
    toglPtr->deviceContext = GetDC(hwnd);
    //    if (toglPtr->pixelFormat) {
    //	printf("Togl_MakeWindow: pixelFormat is defined\n");
        if (!togl_describePixelFormat(toglPtr)) {
            Tcl_SetResult(toglPtr->interp,
                    "couldn't choose pixel format", TCL_STATIC);
            goto error;
        }
#if 0
    } else {
	printf("No pixelFormat\n");
#if 0
        toglPtr->pixelFormat = togl_pixelFormat(toglPtr, hwnd);
#endif
        if (toglPtr->pixelFormat == 0) {
            goto error;
        }
    }
#endif
    if (toglPtr->pBufferFlag) {
        toglPtr->pbuf = togl_createPbuffer(toglPtr);
        if (toglPtr->pbuf == NULL) {
            Tcl_SetResult(toglPtr->interp,
                    "couldn't create pbuffer", TCL_STATIC);
            goto error;
        }
        ReleaseDC(hwnd, toglPtr->deviceContext);
        toglPtr->deviceContext = getPbufferDC(toglPtr->pbuf);
        createdPbufferDC = True;
    } else if (SetPixelFormat(toglPtr->deviceContext,
		  (int) toglPtr->pixelFormat, NULL) == FALSE) {
        Tcl_SetResult(toglPtr->interp, "couldn't set pixel format",
                TCL_STATIC);
        goto error;
    }    
    if (toglPtr->visInfo == NULL) {
        /* 
         * Create a new OpenGL rendering context. And check to share lists.
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
     * Create a new OpenGL rendering context.
     */
    if (toglPtr->shareContext &&
	FindTogl(toglPtr, toglPtr->shareContext)) {
        /* share OpenGL context with existing Togl widget */
        Togl   *shareWith = FindTogl(toglPtr, toglPtr->shareContext);

        if (toglPtr->pixelFormat != shareWith->pixelFormat) {
            Tcl_SetResult(toglPtr->interp,
                    "unable to share OpenGL context", TCL_STATIC);
            goto error;
        }
        toglPtr->context = shareWith->context;
    } else {
	toglPtr->context = wglCreateContext(toglPtr->deviceContext);
    }
    if (toglPtr->shareList) {
        /* share display lists with existing togl widget */
        Togl   *shareWith = FindTogl(toglPtr, toglPtr->shareList);

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
                "could not create rendering context", TCL_STATIC);
        goto error;
    }
    if (toglPtr->pBufferFlag) {
        /* Don't need a colormap, nor overlay, nor be displayed */
        return window;
    }
    DescribePixelFormat(toglPtr->deviceContext,
	(int) toglPtr->pixelFormat, sizeof (pfd), &pfd);

    /* 
     * find a colormap
     */
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

    /* Make sure Tk knows to switch to the new colormap when the cursor is over
     * this window when running in color index mode. */
    (void) Tk_SetWindowVisual(tkwin, toglPtr->visInfo->visual,
            toglPtr->visInfo->depth, cmap);

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

    return window;

  error:

    toglPtr->badWindow = True;
    if (toglPtr->deviceContext) {
        if (createdPbufferDC) {
            releasePbufferDC(toglPtr->pbuf, toglPtr->deviceContext);
        } else {
            ReleaseDC(hwnd, toglPtr->deviceContext);
	}
        toglPtr->deviceContext = NULL;
    }
    return window;
}

/*
 *  Togl_Update
 *
 *    Called by ToglDisplay.  On macOS this sets the size of the NSView being
 *    used as the OpenGL drawing surface.  Also, if the widget's NSView has
 *    not been assigned to its NSOpenGLContext, that will be done here.
 *    This step is not needed on other platforms, where the surface is
 *    managed by the window.
 */

void
Togl_Update(
    const Togl *toglPtr) {
}

/*
 *  Togl_CreateGLContext
 *
 *  Creates an OpenGL rendering context. On Windows this context
 *  is associated with a hidden window, which is then destroyed.
 *  The reason for this is that a rendering context can only be
 *  created after a device context is created, and that requires
 *  a window.  It is necessary to create a context before querying
 *  the OpenGL server to find out what pixel formats are available.
 *  This function chooses an optimal pixel format and saves it
 *  in ToglPtr->pixelFormat.  When Togl_MakeWindow is called
 *  later a new context is created using the saved pixelFormat.
 *  
 *  The OpenGL documentation acknowledges that this is weird, but
 *  proclaims that it is just how WGL works.  So there.
 *
 *  Returns a standard Tcl result.
 */

int
Togl_CreateGLContext(
    Togl *toglPtr)
{
    GLenum result;
    HDC     dc;  /* Device context handle */
    HGLRC   rc;  /* Rendering context handle */
    HWND    test = NULL;
    const int attribList[] = {
	WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
	WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
	WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
	WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
	WGL_COLOR_BITS_ARB,     32,
	WGL_DEPTH_BITS_ARB,     24,
	WGL_STENCIL_BITS_ARB,   8,
	0,
    };
    int pixelFormat;
    UINT numFormats;
    printf("Togl_CreateContext\n");
    if (wglGetCurrentContext() != NULL) {
	dc = wglGetCurrentDC();
    } else {
	test = toglCreateTestWindow(None);
	if (test == NULL) {
	    Tcl_SetResult(toglPtr->interp,
			  "can't create dummy OpenGL window",
			  TCL_STATIC);
	    return 0;
	}
	dc = GetDC(test);
	rc = wglCreateContext(dc);
	wglMakeCurrent(dc, rc);
	result = glewInit();
	if(result != GLEW_OK) {
	    fprintf(stderr, "glewInit error: %s\n",
		    glewGetErrorString(result));
	}
    }
    wglChoosePixelFormatARB(dc, attribList, NULL, 1,
			    &pixelFormat, &numFormats);
    if (test != NULL) {
	ReleaseDC(test, dc);
	DestroyWindow(test);
    }
    wglMakeCurrent(NULL, NULL);
    toglPtr->pixelFormat = pixelFormat;
    return TCL_OK;
}

void
Togl_MakeCurrent(
    const Togl *toglPtr)
{
    printf("MakeCurrent\n");
    wglMakeCurrent(toglPtr->deviceContext, toglPtr->context);
}

void
Togl_SwapBuffers(
    const Togl *toglPtr)
{
    printf("SwapBuffers\n");
    wglSwapLayerBuffers(toglPtr->deviceContext, WGL_SWAP_MAIN_PLANE);
}

const char* Togl_GetExtensions(
    Togl *toglPtr)
{
    const char *extensions = NULL;
    wglMakeCurrent(toglPtr->deviceContext, toglPtr->context);
    if (wglGetCurrentContext() == NULL) {
	return extensions;
    }
    printf("GL version: %s\n", glGetString(GL_VERSION));
    getExtensionsString = (PFNWGLGETEXTENSIONSSTRINGARBPROC)
	wglGetProcAddress("wglGetExtensionsStringARB");
    if (getExtensionsString == NULL)
	getExtensionsString = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)
	    wglGetProcAddress("wglGetExtensionsStringEXT");
    if (getExtensionsString) {
	extensions = getExtensionsString(toglPtr->deviceContext);
    }
    return extensions;
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
    Togl *ToglPtr)
{
    printf("FreeResources\n");
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
