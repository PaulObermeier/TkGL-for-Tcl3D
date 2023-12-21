#include "toglPlatform.h"

/*
 * Forward declarations
 */

typedef struct ToglXX ToglXX;
typedef struct Togl_PackageGlobals Togl_PackageGlobals;

/*
 * The Togl package maintains a list containing a reference to each Togl
 * widget and another list of context tags.  This struct is initialized
 * when the first Togle widget gets created.  A reference to this struct
 * is passed to ToglObjCmd as the clientData parameter.
 */

struct Togl_PackageGlobals
{
    Tk_OptionTable optionTable; /* Used to parse options */
    ToglXX   *toglHead;           /* Head of linked list of all Togl widgets */
    int     nextContextTag;     /* Used to assign similar context tags */
};

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
    Tk_OptionTable optionTable;	/* Token representing the configuration
				 * specifications. */
  // Fields from the Square widget
    Tcl_Obj *xPtr, *yPtr;	/* Position of togl's upper-left corner
				 * within widget. */
    int x, y;
    Tcl_Obj *sizeObjPtr;	/* Width and height of togl. */
    Tcl_Obj *borderWidthPtr;	/* Width of 3-D border around whole widget. */
    Tcl_Obj *bgBorderPtr;
    Tcl_Obj *fgBorderPtr;
    Tcl_Obj *reliefPtr;
  // GC gc;			/* Graphics context for copying from
  //			 * off-screen pixmap onto screen. */
  //    Tcl_Obj *doubleBufferPtr;	/* Non-zero means double-buffer redisplay with
  //				 * pixmap; zero means draw straight onto the
  //				 * display. */
    int updatePending;		/* Non-zero means a call to ToglDisplay has
				 * already been scheduled. */
  //=================================//
    Tcl_Obj *widthObjPtr;	/* Width of togl widget. */
    Tcl_Obj *heightObjPtr;	/* Height of togl widget. */
    int     setGrid;            /* positive is grid size for window manager */
    int     contextTag;         /* contexts with same tag share display lists */
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
    void    *clientData;      /* Pointer to user data */

    Bool    UpdatePending;      /* Should normal planes be redrawn? */

    Tcl_Obj *createProc;        /* Callback when widget is realized */
    Tcl_Obj *displayProc;       /* Callback when widget is redrawn */
    Tcl_Obj *reshapeProc;       /* Callback when window size changes */
    Tcl_Obj *destroyProc;       /* Callback when widget is destroyed */
    Tcl_Obj *timerProc;         /* Callback when widget is idle */

    Window  overlayWindow;      /* The overlay window, or 0 */
    Tcl_Obj *overlayDisplayProc;        /* Overlay redraw proc */
    Bool    overlayUpdatePending;       /* Should overlay be redrawn? */
    Colormap overlayCmap;       /* colormap for overlay is created */
    int     overlayTransparentPixel;    /* transparent pixel */
    Bool    overlayIsMapped;

    GLfloat *redMap;            /* Index2RGB Maps for Color index modes */
    GLfloat *greenMap;
    GLfloat *blueMap;
    GLint   mapSize;            /* = Number of indices in our Togl */
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

static Tk_OptionSpec toglOptionSpecs[] = {
  // From the square widget.  Remove most of these.
    {TK_OPTION_BORDER, "-background", "background", "Background",
	    "#d9d9d9", offsetof(Togl, bgBorderPtr), TCL_INDEX_NONE, 0,
	    "white", 0},
    {TK_OPTION_SYNONYM, "-bd", NULL, NULL, NULL, 0, TCL_INDEX_NONE, 0,
	    "-borderwidth", 0},
    {TK_OPTION_SYNONYM, "-bg", NULL, NULL, NULL, 0, TCL_INDEX_NONE, 0,
	    "-background", 0},
    {TK_OPTION_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	    "2", offsetof(Togl, borderWidthPtr), TCL_INDEX_NONE, 0, NULL, 0},
    {TK_OPTION_SYNONYM, "-fg", NULL, NULL, NULL, 0, TCL_INDEX_NONE, 0,
	    "-foreground", 0},
    {TK_OPTION_BORDER, "-foreground", "foreground", "Foreground",
	    "#b03060", offsetof(Togl, fgBorderPtr), TCL_INDEX_NONE, 0,
	    "black", 0},
    {TK_OPTION_PIXELS, "-posx", "posx", "PosX", "0",
	    offsetof(Togl, xPtr), TCL_INDEX_NONE, 0, NULL, 0},
    {TK_OPTION_PIXELS, "-posy", "posy", "PosY", "0",
	    offsetof(Togl, yPtr), TCL_INDEX_NONE, 0, NULL, 0},
    {TK_OPTION_RELIEF, "-relief", "relief", "Relief",
	    "raised", offsetof(Togl, reliefPtr), TCL_INDEX_NONE, 0, NULL, 0},
    /*==========================================*/
    {TK_OPTION_PIXELS, "-width", "width", "Width", DEFAULT_WIDTH,
     offsetof(Togl, widthObjPtr), TCL_INDEX_NONE, 0, NULL, GEOMETRY_MASK},
    {TK_OPTION_PIXELS, "-height", "height", "Height", DEFAULT_HEIGHT,
     offsetof(Togl, heightObjPtr), TCL_INDEX_NONE, 0, NULL, GEOMETRY_MASK},
#if 0
    {TK_OPTION_BOOLEAN, "-rgba", "rgba", "Rgba",
            "true", -1, Tk_Offset(Togl, RgbaFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-redsize", "redsize", "RedSize",
            "1", -1, Tk_Offset(Togl, RgbaRed), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-greensize", "greensize", "GreenSize",
            "1", -1, Tk_Offset(Togl, RgbaGreen), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-bluesize", "bluesize", "BlueSize",
            "1", -1, Tk_Offset(Togl, RgbaBlue), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-double", "double", "Double",
            "false", -1, Tk_Offset(Togl, DoubleFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-depth", "depth", "Depth",
            "false", -1, Tk_Offset(Togl, DepthFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-depthsize", "depthsize", "DepthSize",
            "1", -1, Tk_Offset(Togl, DepthSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-accum", "accum", "Accum",
            "false", -1, Tk_Offset(Togl, AccumFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumredsize", "accumredsize", "AccumRedSize",
            "1", -1, Tk_Offset(Togl, AccumRed), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumgreensize", "accumgreensize",
                "AccumGreenSize",
            "1", -1, Tk_Offset(Togl, AccumGreen), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumbluesize", "accumbluesize",
                "AccumBlueSize",
            "1", -1, Tk_Offset(Togl, AccumBlue), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumalphasize", "accumalphasize",
                "AccumAlphaSize",
            "1", -1, Tk_Offset(Togl, AccumAlpha), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-alpha", "alpha", "Alpha",
            "false", -1, Tk_Offset(Togl, AlphaFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-alphasize", "alphasize", "AlphaSize",
            "1", -1, Tk_Offset(Togl, AlphaSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-stencil", "stencil", "Stencil",
            "false", -1, Tk_Offset(Togl, StencilFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-stencilsize", "stencilsize", "StencilSize",
            "1", -1, Tk_Offset(Togl, StencilSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-auxbuffers", "auxbuffers", "AuxBuffers",
            "0", -1, Tk_Offset(Togl, AuxNumber), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-privatecmap", "privateCmap", "PrivateCmap",
                "false", -1, Tk_Offset(Togl, PrivateCmapFlag), 0, NULL,
            FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-overlay", "overlay", "Overlay",
            "false", -1, Tk_Offset(Togl, OverlayFlag), 0, NULL, OVERLAY_MASK},
    {TK_OPTION_CUSTOM, "-stereo", "stereo", "Stereo",
                "", -1, Tk_Offset(Togl, Stereo), 0,
            (ClientData) &stereoOption, STEREO_FORMAT_MASK},
    {TK_OPTION_DOUBLE, "-eyeseparation", "eyeseparation",
                "EyeSeparation",
            "2.0", -1, Tk_Offset(Togl, EyeSeparation), 0, NULL, STEREO_MASK},
    {TK_OPTION_DOUBLE, "-convergence", "convergence", "Convergence",
            "35.0", -1, Tk_Offset(Togl, Convergence), 0, NULL, STEREO_MASK},
    {TK_OPTION_CURSOR, "-cursor", "cursor", "Cursor",
                "", -1, Tk_Offset(Togl, Cursor), TK_OPTION_NULL_OK, NULL,
            CURSOR_MASK},
    {TK_OPTION_INT, "-setgrid", "setGrid", "SetGrid",
            "0", -1, Tk_Offset(Togl, SetGrid), 0, NULL, GEOMETRY_MASK},
    {TK_OPTION_INT, "-time", "time", "Time",
                DEFAULT_TIME, -1, Tk_Offset(Togl, TimerInterval), 0, NULL,
            TIMER_MASK},
    {TK_OPTION_STRING, "-sharelist", "sharelist", "ShareList",
            NULL, -1, Tk_Offset(Togl, ShareList), 0, NULL, FORMAT_MASK},
    {TK_OPTION_STRING, "-sharecontext", "sharecontext",
                "ShareContext", NULL,
            -1, Tk_Offset(Togl, ShareContext), 0, NULL, FORMAT_MASK},
    {TK_OPTION_STRING, "-ident", "ident", "Ident",
            DEFAULT_IDENT, -1, Tk_Offset(Togl, Ident), 0, NULL, 0},
    {TK_OPTION_BOOLEAN, "-indirect", "indirect", "Indirect",
            "false", -1, Tk_Offset(Togl, Indirect), 0, NULL, FORMAT_MASK},
    {TK_OPTION_CUSTOM, "-pixelformat", "pixelFormat", "PixelFormat",
                "0", -1, Tk_Offset(Togl, PixelFormat), 0,
            (ClientData) &wideIntOption, FORMAT_MASK},
    {TK_OPTION_INT, "-swapinterval", "swapInterval", "SwapInterval",
            "1", -1, Tk_Offset(Togl, SwapInterval), 0, NULL, SWAP_MASK},
    {TK_OPTION_BOOLEAN, "-fullscreen", "fullscreen", "Fullscreen",
                "false", -1, Tk_Offset(Togl, FullscreenFlag), 0, NULL,
            GEOMETRY_MASK|FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-multisample", "multisample", "Multisample",
                "false", -1, Tk_Offset(Togl, MultisampleFlag), 0, NULL,
            FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-pbuffer", "pbuffer", "Pbuffer",
            "false", -1, Tk_Offset(Togl, PbufferFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-largestpbuffer", "largestpbuffer",
                "LargestPbuffer",
            "false", -1, Tk_Offset(Togl, LargestPbufferFlag), 0, NULL, 0},
    {TK_OPTION_STRING, "-createcommand", "createCommand",
                "CallbackCommand", NULL,
            Tk_Offset(Togl, CreateProc), -1, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-create", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-createcommand", 0},
    {TK_OPTION_STRING, "-displaycommand", "displayCommand",
                "CallbackCommand", NULL,
            Tk_Offset(Togl, DisplayProc), -1, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-display", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-displaycommand", 0},
    {TK_OPTION_STRING, "-reshapecommand", "reshapeCommand",
                "CallbackCommand", NULL,
            Tk_Offset(Togl, ReshapeProc), -1, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-reshape", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-reshapecommand", 0},
    {TK_OPTION_STRING, "-destroycommand", "destroyCommand",
                "CallbackCommand", NULL,
            Tk_Offset(Togl, DestroyProc), -1, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-destroy", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-destroycommand", 0},
    {TK_OPTION_STRING, "-timercommand", "timerCommand",
                "CallbackCommand", NULL,
            Tk_Offset(Togl, TimerProc), -1, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-timer", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-timercommand", 0},
    {TK_OPTION_STRING, "-overlaydisplaycommand",
                "overlaydisplayCommand", "CallbackCommand", NULL,
                Tk_Offset(Togl, OverlayDisplayProc), -1,
            TK_OPTION_NULL_OK, NULL, OVERLAY_MASK},
    {TK_OPTION_SYNONYM, "-overlaydisplay", NULL, NULL,
            NULL, -1, -1, 0, (ClientData) "-overlaydisplaycommand", 0},
    {TK_OPTION_STRING_TABLE, "-profile", "profile", "Profile",
     "legacy", -1, Tk_Offset(Togl, profile), 0, profileStrings, 0},
#endif
    {TK_OPTION_END, NULL, NULL, NULL, NULL, 0, 0, 0, NULL, 0}
};
