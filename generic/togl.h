#include "toglPlatform.h"

/*
 * Forward declarations
 */

typedef struct Togl Togl;
typedef struct Togl_PackageGlobals Togl_PackageGlobals;

/*
 * Enum used for the -profile option to specify an OpenGL profile.
 */

enum profile {
  PROFILE_LEGACY, PROFILE_3_2, PROFILE_4_1
};


/*
 * The Togl widget record.  Each Togl widget maintains one of these.
 */

typedef struct Togl {
    struct Togl *next;          /* Next in a linked list of all togl widgets.*/
    Tk_Window tkwin;		/* Window containing the togl widget. NULL means
				 * the window has been deleted but the widget
				 * record hasn't been cleaned up yet. */
    Display *display;		/* X's token for the window's display. */
    Tcl_Interp *interp;		/* Interpreter associated with widget. */
    Tcl_Command widgetCmd;	/* Token for togl's widget command. */
    Tk_OptionTable optionTable;	/* Token representing the option specifications. */
    Tcl_Obj *sizeObjPtr;	/* Width and height of Togl widget. */
    Tcl_Obj *borderWidthPtr;	/* width of border around widget. */
    Tcl_Obj *reliefPtr;         /* border style */
    Tcl_Obj *bgPtr;             /* background color */
    Tcl_Obj *fgPtr;             /* foreground color */
    int updatePending;		/* Non-zero if a call to ToglDisplay is scheduled. */
    int x, y;                   /* Upper left corner of Togl widget */
    int width;	                /* Width of togl widget in pixels. */
    int height;	                /* Height of togl widget in pixels. */
    int setGrid;                /* positive is grid size for window manager */
    int contextTag;             /* contexts with same tag share display lists */
    XVisualInfo *visInfo;       /* Visual info of the current */
    Togl_PackageGlobals *tpg;   /* Used to access package global data */
    Tk_Cursor cursor;           /* The widget's cursor */
    int     timerInterval;      /* Time interval for timer in milliseconds */
    Tcl_TimerToken timerHandler;        /* Token for togl's timer handler */
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
    Bool    indirect;
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
    HDC     toglGLHdc;          /* Device context */
    int     ciColormapSize;     /* (Maximum) size of indexed colormap */
    HPBUFFERARB pbuf;
    int     pbufferLost;
#elif defined(TOGL_X11)
    GLXContext context;         /* OpenGL context for normal planes */
    GLXContext overlayContext;  /* OpenGL context for overlay planes */
    GLXFBConfig fbcfg;          /* cache FBConfig for pbuffer creation */
    Tcl_WideInt pixelFormat;
    GLXPbuffer pbuf;
#elif defined(TOGL_NSOPENGL)
    NSOpenGLContext *context;
    NSOpenGLPixelFormat *pixelFormat;
    NSOpenGLPixelBuffer *pbuf;
    NSView *nsview;
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

/*
 * The following table defines the legal values for the -profile
 * option, which is only used in toglNSOpenGL.c.
 */

static const char *const profileStrings[] = {
    "legacy", "3_2", "4_1", NULL
};


/* Declarations of custom option structs. */
static Tk_ObjCustomOption stereoOption;
static Tk_ObjCustomOption wideIntOption;

static Tk_OptionSpec toglOptionSpecs[] = {
  // From the square widget.  Remove most of these.
    {TK_OPTION_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	    "0", offsetof(Togl, borderWidthPtr), TCL_INDEX_NONE, 0, NULL, 0},
    {TK_OPTION_SYNONYM, "-bd", NULL, NULL, NULL, 0, TCL_INDEX_NONE, 0,
	    "-borderwidth", 0},
    {TK_OPTION_BORDER, "-background", "background", "Background",
	    "#ffffff", offsetof(Togl, bgPtr), TCL_INDEX_NONE, 0,
	    "white", 0},
    {TK_OPTION_SYNONYM, "-bg", NULL, NULL, NULL, 0, TCL_INDEX_NONE, 0,
	    "-background", 0},
    {TK_OPTION_BORDER, "-foreground", "foreground", "Foreground", "#000000",
     offsetof(Togl, fgPtr), TCL_INDEX_NONE, 0, "black", 0},
    {TK_OPTION_SYNONYM, "-fg", NULL, NULL, NULL, 0, TCL_INDEX_NONE, 0,
	    "-foreground", 0},
    {TK_OPTION_INT, "-width", "width", "Width", DEFAULT_WIDTH,
     TCL_INDEX_NONE, offsetof(Togl, width), 0, NULL, GEOMETRY_MASK},
    {TK_OPTION_INT, "-height", "height", "Height", DEFAULT_HEIGHT,
     TCL_INDEX_NONE, offsetof(Togl, height),0, NULL, GEOMETRY_MASK},
    {TK_OPTION_RELIEF, "-relief", "relief", "Relief", "flat",
     offsetof(Togl, reliefPtr), TCL_INDEX_NONE, 0, NULL, 0},
    {TK_OPTION_BOOLEAN, "-rgba", "rgba", "RGBA", "true",
     TCL_INDEX_NONE, offsetof(Togl, rgbaFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-redsize", "redsize", "RedSize", "1",
     TCL_INDEX_NONE, offsetof(Togl, rgbaRed), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-greensize", "greensize", "GreenSize", "1",
     TCL_INDEX_NONE, offsetof(Togl, rgbaGreen), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-bluesize", "bluesize", "BlueSize", "1",
     TCL_INDEX_NONE, offsetof(Togl, rgbaBlue), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-double", "double", "Double", "false",
     TCL_INDEX_NONE, offsetof(Togl, doubleFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-depth", "depth", "Depth", "false",
     TCL_INDEX_NONE, offsetof(Togl, depthFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-depthsize", "depthsize", "DepthSize", "1",
     TCL_INDEX_NONE, offsetof(Togl, depthSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-accum", "accum", "Accum", "false",
     TCL_INDEX_NONE, offsetof(Togl, accumFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumredsize", "accumredsize", "accumRedSize", "1",
     TCL_INDEX_NONE, offsetof(Togl, accumRed), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumgreensize", "accumgreensize", "AccumGreenSize", "1",
     TCL_INDEX_NONE, offsetof(Togl, accumGreen), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumbluesize", "accumbluesize", "AccumBlueSize", "1",
     TCL_INDEX_NONE, offsetof(Togl, accumBlue), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumalphasize", "accumalphasize", "AccumAlphaSize", "1",
     TCL_INDEX_NONE, offsetof(Togl, accumAlpha), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-alpha", "alpha", "Alpha", "false",
     TCL_INDEX_NONE, offsetof(Togl, alphaFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-alphasize", "alphasize", "AlphaSize", "1",
     TCL_INDEX_NONE, offsetof(Togl, alphaSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-stencil", "stencil", "Stencil", "false",
     TCL_INDEX_NONE, offsetof(Togl, stencilFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-stencilsize", "stencilsize", "StencilSize", "1",
     TCL_INDEX_NONE, offsetof(Togl, stencilSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-auxbuffers", "auxbuffers", "AuxBuffers", "0",
     TCL_INDEX_NONE, offsetof(Togl, auxNumber), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-privatecmap", "privateCmap", "PrivateCmap", "false",
     TCL_INDEX_NONE, offsetof(Togl, privateCmapFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-overlay", "overlay", "Overlay", "false",
     TCL_INDEX_NONE, offsetof(Togl, overlayFlag), 0, NULL, OVERLAY_MASK},
    {TK_OPTION_CUSTOM, "-stereo", "stereo", "Stereo", "",
     TCL_INDEX_NONE, offsetof(Togl, stereo), 0, (void*) &stereoOption,
     STEREO_FORMAT_MASK},
    {TK_OPTION_DOUBLE, "-eyeseparation", "eyeseparation", "EyeSeparation", "2.0",
     TCL_INDEX_NONE, offsetof(Togl, eyeSeparation), 0, NULL, STEREO_MASK},
    {TK_OPTION_DOUBLE, "-convergence", "convergence", "Convergence", "35.0",
     TCL_INDEX_NONE, offsetof(Togl, convergence), 0, NULL, STEREO_MASK},
    {TK_OPTION_CURSOR, "-cursor", "cursor", "Cursor", "",
     TCL_INDEX_NONE, offsetof(Togl, cursor), TK_OPTION_NULL_OK, NULL, CURSOR_MASK},
    {TK_OPTION_INT, "-setgrid", "setGrid", "SetGrid", "0",
     TCL_INDEX_NONE, offsetof(Togl, setGrid), 0, NULL, GEOMETRY_MASK},
    {TK_OPTION_INT, "-time", "time", "Time", DEFAULT_TIME,
     TCL_INDEX_NONE, offsetof(Togl, timerInterval), 0, NULL, TIMER_MASK},
    {TK_OPTION_STRING, "-sharelist", "sharelist", "ShareList", NULL,
     TCL_INDEX_NONE, offsetof(Togl, shareList), 0, NULL, FORMAT_MASK},
    {TK_OPTION_STRING, "-sharecontext", "sharecontext", "ShareContext", NULL,
     TCL_INDEX_NONE, offsetof(Togl, shareContext), 0, NULL, FORMAT_MASK},
    {TK_OPTION_STRING, "-ident", "ident", "Ident", DEFAULT_IDENT,
     TCL_INDEX_NONE, offsetof(Togl, ident), 0, NULL, 0},
    {TK_OPTION_BOOLEAN, "-indirect", "indirect", "Indirect", "false",
     TCL_INDEX_NONE, offsetof(Togl, indirect), 0, NULL, FORMAT_MASK},
    {TK_OPTION_CUSTOM, "-pixelformat", "pixelFormat", "PixelFormat", "0",
     TCL_INDEX_NONE, offsetof(Togl, pixelFormat), 0, (void *) &wideIntOption,
     FORMAT_MASK},
    {TK_OPTION_INT, "-swapinterval", "swapInterval", "SwapInterval", "1",
     TCL_INDEX_NONE, offsetof(Togl, swapInterval), 0, NULL, SWAP_MASK},
    {TK_OPTION_BOOLEAN, "-fullscreen", "fullscreen", "Fullscreen", "false",
     TCL_INDEX_NONE, offsetof(Togl, fullscreenFlag), 0, NULL,
     GEOMETRY_MASK|FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-multisample", "multisample", "Multisample", "false",
     TCL_INDEX_NONE, offsetof(Togl, multisampleFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-pbuffer", "pbuffer", "Pbuffer", "false",
     TCL_INDEX_NONE, offsetof(Togl, pBufferFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-largestpbuffer", "largestpbuffer", "LargestPbuffer", "false",
     TCL_INDEX_NONE, offsetof(Togl, largestPbufferFlag), 0, NULL, 0},
    {TK_OPTION_STRING, "-createcommand", "createCommand", "CallbackCommand", NULL,
     offsetof(Togl, createProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-create", NULL, NULL, NULL, TCL_INDEX_NONE, TCL_INDEX_NONE, 0,
     (void *) "-createcommand", 0},
    {TK_OPTION_STRING, "-displaycommand", "displayCommand", "CallbackCommand", NULL,
     offsetof(Togl, displayProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-display", NULL, NULL, NULL,
     TCL_INDEX_NONE, TCL_INDEX_NONE, 0, (void *) "-displaycommand", 0},
    {TK_OPTION_STRING, "-reshapecommand", "reshapeCommand", "CallbackCommand", NULL,
     offsetof(Togl, reshapeProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-reshape", NULL, NULL, NULL,
     TCL_INDEX_NONE, TCL_INDEX_NONE, 0, (void *) "-reshapecommand", 0},
    {TK_OPTION_STRING, "-destroycommand", "destroyCommand", "CallbackCommand", NULL,
     offsetof(Togl, destroyProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-destroy", NULL, NULL, NULL,
     TCL_INDEX_NONE, TCL_INDEX_NONE, 0, (void *) "-destroycommand", 0},
    {TK_OPTION_STRING, "-timercommand", "timerCommand", "CallbackCommand", NULL,
     offsetof(Togl, timerProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-timer", NULL, NULL, NULL,
     TCL_INDEX_NONE, TCL_INDEX_NONE, 0, (void *) "-timercommand", 0},
    {TK_OPTION_STRING, "-overlaydisplaycommand", "overlaydisplayCommand", "CallbackCommand", NULL,
     offsetof(Togl, overlayDisplayProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, OVERLAY_MASK},
    {TK_OPTION_SYNONYM, "-overlaydisplay", NULL, NULL, NULL,
     TCL_INDEX_NONE, TCL_INDEX_NONE, 0, (ClientData) "-overlaydisplaycommand", 0},
    {TK_OPTION_STRING_TABLE, "-profile", "profile", "Profile", "legacy",
     TCL_INDEX_NONE, offsetof(Togl, profile), 0, profileStrings, 0},
    {TK_OPTION_END, NULL, NULL, NULL, NULL, 0, 0, 0, NULL, 0}
};

/*
 * Declarations of utility functions defined in togl.c.
 */

Togl* FindTogl(Togl *togl, const char *ident);
Togl* FindToglWithSameContext(const Togl *togl);
int   Togl_CallCallback(Togl *togl, Tcl_Obj *cmd);

/*
 * Declarations of platform specific utility functions.
 */

const char *Togl_GetExtensions(const Togl *toglPtr);
void Togl_MakeCurrent(const Togl *toglPtr);
void Togl_Update(const Togl *toglPtr);
Window Togl_MakeWindow(Tk_Window tkwin, Window parent, void* instanceData);
void Togl_WorldChanged(void* instanceData);

