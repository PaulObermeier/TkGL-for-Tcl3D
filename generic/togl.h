#ifndef USE_TCL_STUBS
#   define USE_TCL_STUBS
#endif
#ifndef USE_TK_STUBS
#   define USE_TK_STUBS
#endif
#include "tk.h"

/* For compatibility with Tcl 8.6 */ 
#ifndef TCL_INDEX_NONE
#define TCL_INDEX_NONE (-1)
#define Tk_MakeWindow(tkwin, parent) TkpMakeWindow((TkWindow *)tkwin, parent)
#endif
#include "toglPlatform.h"

/*
 * Forward declarations
 */

typedef struct Togl Togl;

/*
 * Enum used for the -profile option to specify an OpenGL profile.
 */

enum profile {
    PROFILE_LEGACY, PROFILE_3_2, PROFILE_4_1, PROFILE_SYSTEM
};


/*
 * The Togl widget record.  Each Togl widget maintains one of these.
 */

typedef struct Togl {
    struct Togl *next;          /* Next in a linked list of all togl widgets.*/
    Tk_Window tkwin;		/* Window identifier for the togl widget.*/
    Display *display;		/* X token for the window's display. */
    Tcl_Interp *interp;		/* Interpreter associated with widget. */
    Tcl_Command widgetCmd;	/* Token for togl's widget command. */
    Tk_OptionTable optionTable; /* Token representing the option specs. */
    int updatePending;		/* A call to ToglDisplay has been scheduled. */
    int x, y;                   /* Upper left corner of Togl widget */
    int width;	                /* Width of togl widget in pixels. */
    int height;	                /* Height of togl widget in pixels. */
    int setGrid;                /* positive is grid size for window manager */
    int contextTag;             /* contexts with same tag share display lists */
    XVisualInfo *visInfo;       /* Visual info of the widget */
    Tk_Cursor cursor;           /* The widget's cursor */
    int     timerInterval;      /* Time interval for timer in milliseconds */
    Tcl_TimerToken timerHandler;  /* Token for togl's timer handler */
    Bool    rgbaFlag;           /* configuration flags (ala GLX parameters) */
    int     rgbaRed;
    int     rgbaGreen;
    int     rgbaBlue;
    Bool    doubleFlag;
    Bool    depthFlag;
    int     depthSize;
    Bool    accumFlag;
    int     accumRed;
    int     accumGreen;
    int     accumBlue;
    int     accumAlpha;
    Bool    alphaFlag;
    int     alphaSize;
    Bool    stencilFlag;
    int     stencilSize;
    Bool    privateCmapFlag;
    Bool    overlayFlag;
    int     stereo;
    double  eyeSeparation;
    double  convergence;
    GLuint  riStencilBit;       /* row interleaved stencil bit */
    int     auxNumber;
    enum    profile profile;
    int     swapInterval;
    Bool    multisampleFlag;
    Bool    fullscreenFlag;
    Bool    pBufferFlag;
    Bool    largestPbufferFlag;
    const char *shareList;      /* name (ident) of Togl to share dlists with */
    const char *shareContext;   /* name (ident) to share OpenGL context with */
    const char *ident;          /* User's identification string */
    void    *clientData;        /* Pointer to user data */
    Bool    UpdatePending;      /* Should normal planes be redrawn? */
    Tcl_Obj *createProc;        /* Callback when widget is realized */
    Tcl_Obj *displayProc;       /* Callback when widget is redrawn */
    Tcl_Obj *reshapeProc;       /* Callback when window size changes */
    Tcl_Obj *destroyProc;       /* Callback when widget is destroyed */
    Tcl_Obj *timerProc;         /* Callback when widget is idle */
    Window  overlayWindow;      /* The overlay window, or 0 */
    Tcl_Obj *overlayDisplayProc;     /* Overlay redraw proc */
    Bool    overlayUpdatePending;    /* Should overlay be redrawn? */
    Colormap overlayCmap;            /* colormap for overlay is created */
    int     overlayTransparentPixel; /* transparent pixel */
    Bool    overlayIsMapped;
    GLfloat *redMap;            /* Index2RGB Maps for Color index modes */
    GLfloat *greenMap;
    GLfloat *blueMap;
    GLint   mapSize;            /* Number of indices in our color map */
    int     currentStereoBuffer;
    int     badWindow;          /* true when Togl_MakeWindow fails or should
                                 * create a dummy window */
#if defined(TOGL_WGL)
    HGLRC   context;            /* OpenGL rendering context */
    HDC     deviceContext;      /* Device context */
    int     ciColormapSize;     /* (Maximum) size of indexed colormap */
    HWND    child;              /* A child window - the rendring surface. */
    HPBUFFERARB pbuf;
    int     pBufferLost;
    int     pixelFormat;
    const char *extensions;
    unsigned int glmajor;       /* GL version chosen for the dummy window */
    unsigned int glminor;
#elif defined(TOGL_X11)
    GLXContext context;         /* OpenGL context for normal planes */
  GLXContext overlayContext;    /* OpenGL context for overlay planes */
    GLXFBConfig fbcfg;          /* cached FBConfig */
    Tcl_WideInt pixelFormat;
    GLXPbuffer pbuf;
#elif defined(TOGL_NSOPENGL)
    NSOpenGLContext *context;
    NSOpenGLPixelFormat *pixelFormat;
    NSOpenGLPixelBuffer *pbuf;
    NSView *nsview;
    const char *extensions;
#endif
} Togl;

/* The typeMasks used in option specs. */

#define GEOMETRY_MASK 0x1       /* widget geometry */
#define FORMAT_MASK 0x2         /* pixel format */
#define CURSOR_MASK 0x4
#define TIMER_MASK 0x8
#define OVERLAY_MASK 0x10
#define SWAP_MASK 0x20
#define STEREO_MASK 0x40
#define STEREO_FORMAT_MASK 0x80

/* Default values for options. */

#define DEFAULT_WIDTH		"400"
#define DEFAULT_HEIGHT		"400"
#define DEFAULT_IDENT		""
#define DEFAULT_FONTNAME	"Courier"
#define DEFAULT_TIME		"1"

/* 
 * Stereo techniques:
 *      Only the native method uses OpenGL quad-buffered stereo.
 *      All need the eye offset and eye distance set properly.
 */
/* These versions need one eye drawn */
#  define TOGL_STEREO_NONE		0
#  define TOGL_STEREO_LEFT_EYE		1       /* just the left eye */
#  define TOGL_STEREO_RIGHT_EYE		2       /* just the right eye */
#  define TOGL_STEREO_ONE_EYE_MAX	127
/* These versions need both eyes drawn */
#  define TOGL_STEREO_NATIVE		128
#  define TOGL_STEREO_SGIOLDSTYLE	129     /* interlaced, SGI API */
#  define TOGL_STEREO_ANAGLYPH		130
#  define TOGL_STEREO_CROSS_EYE		131
#  define TOGL_STEREO_WALL_EYE		132
#  define TOGL_STEREO_DTI		133     /* dti3d.com */
#  define TOGL_STEREO_ROW_INTERLEAVED	134     /* www.vrex.com/developer/interleave.htm */

#ifndef STEREO_BUFFER_NONE
/* From <X11/extensions/SGIStereo.h>, but we use this constants elsewhere */
#  define STEREO_BUFFER_NONE 0
#  define STEREO_BUFFER_LEFT 1
#  define STEREO_BUFFER_RIGHT 2
#endif

/*
 * Declarations of utility functions defined in togl.c.
 */

Togl* FindTogl(Togl *togl, const char *ident);
Togl* FindToglWithSameContext(const Togl *togl);
int   Togl_CallCallback(Togl *togl, Tcl_Obj *cmd);

/*
 * The functions declared below constitute the interface
 * provided by the platform code for each platform.
 */

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

int Togl_CreateGLContext(Togl *toglPtr);

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

Window Togl_MakeWindow(Tk_Window tkwin, Window parent, void* instanceData);

/*
 * Togl_MakeCurrent
 *
 * This is the key function of the Togl widget in its role as the
 * manager of an NSOpenGL rendering context.  Must be called by
 * a GL client before drawing into the widget.
 */

void Togl_MakeCurrent(const Togl *toglPtr);

/*
 * Togl_SwapBuffers
 *
 * Called by the GL Client after updating the image.  If the Togl
 * is double-buffered it interchanges the front and back framebuffers.
 * otherwise it calls GLFlush.
 */

void Togl_SwapBuffers(const Togl *toglPtr);

/*
 * ToglUpdate
 *
 * Called by ToglDisplay whenever the size of the Togl widget may
 * have changed.  On macOS it adjusts the frame of the NSView that
 * is being used as the rendering surface.  The other platforms
 * handle the size changes automatically.
 */

void Togl_Update(const Togl *toglPtr);

/*
 * Togl_GetExtensions
 *
 * Queries the rendering context for its extension string, a
 * space-separated list of the names of all supported GL extensions.
 * The string is cached in the widget record and the cached
 * string is returned in subsequent calls.
 */

const char* Togl_GetExtensions(Togl *toglPtr);

void Togl_FreeResources(Togl *toglPtr);
int Togl_TakePhoto(Togl *toglPtr, Tk_PhotoHandle photo);
int Togl_CopyContext(const Togl *from, const Togl *to, unsigned mask);
void Togl_WorldChanged(void* instanceData);

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
