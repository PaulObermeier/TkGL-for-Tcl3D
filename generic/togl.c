/*
 * togl.c --
 *
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include "togl.h"
#include "toglOptions.h" 
#include <string.h>

/*
 * Declarations of static functions defined in this file:
 */

static void ToglDeletedProc(void *clientData);
static int  ToglConfigure(Tcl_Interp *interp, Togl *toglPtr);
static void ToglDisplay(void *clientData);
static void ToglObjEventProc(void *clientData, XEvent *eventPtr);
static int  ToglWidgetObjCmd(void *clientData, Tcl_Interp *interp, int objc,
			     Tcl_Obj * const objv[]);
static int  ObjectIsEmpty(Tcl_Obj *objPtr);
static void ToglPostRedisplay(Togl *toglPtr);
static void ToglFrustum(const Togl *togl, GLdouble left, GLdouble right,
			GLdouble bottom, GLdouble top, GLdouble zNear,
			GLdouble zFar);
static void ToglOrtho(const Togl *togl, GLdouble left, GLdouble right,
		       GLdouble bottom, GLdouble top, GLdouble zNear,
		      GLdouble zFar);
static int GetToglFromObj(Tcl_Interp *interp, Tcl_Obj *obj, Togl **target);

/*
 * The Togl package maintains a per-thread list of all Togl widgets.
 */

typedef struct {
    Togl *toglHead;        /* Head of linked list of all Togl widgets. */
    int nextContextTag;    /* Used to assign similar context tags. */
    int initialized;       /* Set to 1 when the struct is initialized. */ 
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;
static void addToList(Togl *t);
static void removeFromList(Togl *t);


/*
 *--------------------------------------------------------------
 *
 * ToglObjCmd --
 *
 *	This procedure is invoked to process the "togl" Tcl command. It
 *	creates a new "togl" widget.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A new widget is created and configured.
 *
 *--------------------------------------------------------------
 */

int
ToglObjCmd(
    void *clientData,
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Togl *toglPtr;
    Tk_Window tkwin = NULL;
    Tk_OptionTable optionTable;

    /* 
     * Setup the Tk_ClassProcs callbacks.
     */

    static Tk_ClassProcs procs = {0};
    if (procs.size == 0) {
	procs.size = sizeof(Tk_ClassProcs);
	procs.worldChangedProc = Togl_WorldChanged;
	procs.createProc = Togl_MakeWindow;
	procs.modalProc = NULL;
    } 
   
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    if (!tsdPtr->initialized) {
	tsdPtr->initialized = 1;
    }
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "pathName ?-option value ...?");
	return TCL_ERROR;
    }
    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp),
    	    Tcl_GetString(objv[1]), NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    Tk_SetClass(tkwin, "Togl");

    /*
     * Create the option table for this widget class. If it has already been
     * created, the refcount will get bumped and just the pointer will be
     * returned. The refcount getting bumped does not concern us, because Tk
     * will ensure the table is deleted when the interpreter is destroyed.
     */

    optionTable = Tk_CreateOptionTable(interp, toglOptionSpecs);

    /*
     * Allocate and initialize the widget record.
     */

    toglPtr = (Togl *)ckalloc(sizeof(Togl));
    memset(toglPtr, 0, sizeof(Togl));
    toglPtr->tkwin = tkwin;
    toglPtr->display = Tk_Display(tkwin);
    toglPtr->interp = interp;
    toglPtr->widgetCmd = Tcl_CreateObjCommand(interp,
	    Tk_PathName(toglPtr->tkwin), ToglWidgetObjCmd, toglPtr,
	    ToglDeletedProc);
    toglPtr->optionTable = optionTable;
    if (Tk_InitOptions(interp, toglPtr, optionTable, tkwin)
	    != TCL_OK) {
	Tk_DestroyWindow(toglPtr->tkwin);
	ckfree(toglPtr);
	return TCL_ERROR;
    }
    Tk_SetClassProcs(toglPtr->tkwin, &procs, toglPtr);
    Tk_CreateEventHandler(toglPtr->tkwin, ExposureMask|StructureNotifyMask,
	    ToglObjEventProc, toglPtr);
    if (Tk_SetOptions(interp, toglPtr, optionTable, objc - 2,
	    objv + 2, tkwin, NULL, NULL) != TCL_OK) {
	goto error;
    }
    if (ToglConfigure(interp, toglPtr) != TCL_OK) {
	goto error;
    }
    if (Togl_CreateGLContext(toglPtr) != TCL_OK) {
         goto error;
    }
    addToList(toglPtr);
    Tcl_SetObjResult(interp,
	Tcl_NewStringObj(Tk_PathName(toglPtr->tkwin), TCL_INDEX_NONE));
    return TCL_OK;

  error:
    Tk_DestroyWindow(toglPtr->tkwin);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * ToglWidgetObjCmd --
 *
 *      When a Togl widget is created it registers its pathname as a new Tcl
 *      command. This procedure is invoked to process the options for that
 *      command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
ToglWidgetObjCmd(
    void *clientData,	        /* Information about togl widget. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj * const objv[])	/* Argument objects. */
{
    Togl *toglPtr = (Togl *)clientData;
    int result = TCL_OK;
    static const char *const toglOptions[] = {
        "cget", "configure", "extensions", "glversion", "postredisplay",
	"render", "swapbuffers", "makecurrent", "takephoto", "loadbitmapfont",
	"unloadbitmapfont", "write", "uselayer", "showoverlay",
	"hideoverlay", "postredisplayoverlay", "renderoverlay",
        "existsoverlay", "ismappedoverlay", "getoverlaytransparentvalue",
        "drawbuffer", "clear", "frustum", "ortho", "numeyes",
	"contexttag", "copycontextto",
        NULL
    };
    enum
    {
        TOGL_CGET, TOGL_CONFIGURE, TOGL_EXTENSIONS,
        TOGL_GLVERSION, TOGL_POSTREDISPLAY, TOGL_RENDER,
        TOGL_SWAPBUFFERS, TOGL_MAKECURRENT, TOGL_TAKEPHOTO,
        TOGL_LOADBITMAPFONT, TOGL_UNLOADBITMAPFONT, TOGL_WRITE,
        TOGL_USELAYER, TOGL_SHOWOVERLAY, TOGL_HIDEOVERLAY,
        TOGL_POSTREDISPLAYOVERLAY, TOGL_RENDEROVERLAY,
        TOGL_EXISTSOVERLAY, TOGL_ISMAPPEDOVERLAY,
        TOGL_GETOVERLAYTRANSPARENTVALUE,
        TOGL_DRAWBUFFER, TOGL_CLEAR, TOGL_FRUSTUM, TOGL_ORTHO,
        TOGL_NUMEYES, TOGL_CONTEXTTAG, TOGL_COPYCONTEXTTO
    };
    Tcl_Obj *resultObjPtr;
    int index;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObjStruct(interp, objv[1], toglOptions,
	    sizeof(char *), "command", 0, &index) != TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_Preserve(toglPtr);

    switch (index) {
    case TOGL_CGET:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "option");
	    goto error;
	}
	resultObjPtr = Tk_GetOptionValue(interp, toglPtr,
		toglPtr->optionTable, objv[2], toglPtr->tkwin);
	if (resultObjPtr == NULL) {
	    result = TCL_ERROR;
	} else {
	    Tcl_SetObjResult(interp, resultObjPtr);
	}
	break;
    case TOGL_CONFIGURE:
	resultObjPtr = NULL;
	if (objc == 2) {
	    resultObjPtr = Tk_GetOptionInfo(interp, toglPtr,
		    toglPtr->optionTable, NULL, toglPtr->tkwin);
	    if (resultObjPtr == NULL) {
		result = TCL_ERROR;
	    }
	} else if (objc == 3) {
	    resultObjPtr = Tk_GetOptionInfo(interp, toglPtr,
		    toglPtr->optionTable, objv[2], toglPtr->tkwin);
	    if (resultObjPtr == NULL) {
		result = TCL_ERROR;
	    }
	} else {
	    result = Tk_SetOptions(interp, toglPtr,
		    toglPtr->optionTable, objc - 2, objv + 2,
		    toglPtr->tkwin, NULL, NULL);
	    if (result == TCL_OK) {
		result = ToglConfigure(interp, toglPtr);
	    }
	    if (!toglPtr->updatePending) {
		Tcl_DoWhenIdle(ToglDisplay, toglPtr);
		toglPtr->updatePending = 1;
	    }
	}
	if (resultObjPtr != NULL) {
	    Tcl_SetObjResult(interp, resultObjPtr);
	}
	break;
    case TOGL_EXTENSIONS:
	/* Return a list of available OpenGL extensions
	 * TODO: -glu for glu extensions,
	 * -platform for glx/wgl extensions
	 */
	if (objc == 2) {
	    const char *extensions = Togl_GetExtensions(toglPtr);
	    Tcl_Obj *objPtr;
	    Tcl_Size length = -1;
	    if (extensions) {
		objPtr = Tcl_NewStringObj(extensions, -1);
		/* This will convert the object to a list. */
		(void) Tcl_ListObjLength(interp, objPtr, &length);
		Tcl_SetObjResult(interp, objPtr);
	    } else {
		Tcl_SetResult(toglPtr->interp,
		    "The extensions string is not available now.",
		    TCL_STATIC);
		result = TCL_ERROR;
	    }
	} else {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    result = TCL_ERROR;
	}
	break;
    case TOGL_GLVERSION:
	/* Return the GL version string of the current context. */
	if (objc == 2) {
	    const char *version = (const char *)glGetString(GL_VERSION);
	    Tcl_Obj *objPtr;
	    if (version) {
		objPtr = Tcl_NewStringObj(version, -1);
		Tcl_SetObjResult(interp, objPtr);
	    } else {
		Tcl_SetResult(toglPtr->interp,
		    "The version string is not available until "
                     "the widget is mapped.",
		     TCL_STATIC);
		result = TCL_ERROR;
	    }
	} else {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    result = TCL_ERROR;
	}
	break;
    case TOGL_POSTREDISPLAY:
	/* schedule the widget to be redrawn */
	if (objc == 2) {
	    ToglPostRedisplay(toglPtr);
	} else {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    result = TCL_ERROR;
	}
	break;
    case TOGL_RENDER:
	/* force the widget to be redrawn */
	if (objc == 2) {
	    ToglDisplay((void *) toglPtr);
	} else {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    result = TCL_ERROR;
	}
	break;
    case TOGL_SWAPBUFFERS:
	if (objc == 2) {
	    Togl_SwapBuffers(toglPtr);
	} else {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    result = TCL_ERROR;
	}
	break;
    case TOGL_MAKECURRENT:
	if (objc == 2) {
	    Togl_MakeCurrent(toglPtr);
	} else {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    result = TCL_ERROR;
	}
	break;
    case TOGL_TAKEPHOTO:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "name");
	    result = TCL_ERROR;
	} else {
	    const char *name;
	    Tk_PhotoHandle photo;

	    name = Tcl_GetStringFromObj(objv[2], NULL);
	    photo = Tk_FindPhoto(interp, name);
	    if (photo == NULL) {
		Tcl_AppendResult(interp, "image \"", name,
		    "\" doesn't exist or is not a photo image", NULL);
		result = TCL_ERROR;
		break;
	    }
	    glPushAttrib(GL_PIXEL_MODE_BIT);
	    if (toglPtr->doubleFlag) {
		glReadBuffer(GL_FRONT);
	    }
	    Togl_TakePhoto(toglPtr, photo);
	    glPopAttrib();    /* restore glReadBuffer */
          }
          break;
    case TOGL_LOADBITMAPFONT:
    case TOGL_UNLOADBITMAPFONT:
    case TOGL_WRITE:
#if TOGL_USE_FONTS != 1
	Tcl_AppendResult(interp, "unsupported", NULL);
	result = TCL_ERROR;
	break;
#else	
ERROR
#endif
    case TOGL_USELAYER:
    case TOGL_SHOWOVERLAY:
    case TOGL_HIDEOVERLAY:
    case TOGL_POSTREDISPLAYOVERLAY:
    case TOGL_RENDEROVERLAY:
    case TOGL_EXISTSOVERLAY:
    case TOGL_ISMAPPEDOVERLAY:
    case TOGL_GETOVERLAYTRANSPARENTVALUE:
#if TOGL_USE_OVERLAY != 1
	Tcl_AppendResult(interp, "unsupported", NULL);
	result = TCL_ERROR;
	break;
#else
ERROR
#endif
    case TOGL_DRAWBUFFER:
    case TOGL_CLEAR:
#if 1
	Tcl_AppendResult(interp, "unsupported", NULL);
	result = TCL_ERROR;
	break;
#endif
    case TOGL_FRUSTUM:
	if (objc != 8) {
	    Tcl_WrongNumArgs(interp, 2, objv,
			     "left right bottom top near far");
	    result = TCL_ERROR;
	} else {
	    double  left, right, bottom, top, zNear, zFar;

	    if (Tcl_GetDoubleFromObj(interp, objv[2], &left) == TCL_ERROR
		|| Tcl_GetDoubleFromObj(interp, objv[3],
					&right) == TCL_ERROR
		|| Tcl_GetDoubleFromObj(interp, objv[4],
					&bottom) == TCL_ERROR
		|| Tcl_GetDoubleFromObj(interp, objv[5],
					&top) == TCL_ERROR
		|| Tcl_GetDoubleFromObj(interp, objv[6],
					&zNear) == TCL_ERROR
		|| Tcl_GetDoubleFromObj(interp, objv[7],
					&zFar) == TCL_ERROR) {
		result = TCL_ERROR;
		break;
	    }
	    ToglFrustum(toglPtr, left, right, bottom, top, zNear, zFar);
	}
	break;
    case TOGL_ORTHO:
	if (objc != 8) {
	    Tcl_WrongNumArgs(interp, 2, objv,
			     "left right bottom top near far");
	    result = TCL_ERROR;
	} else {
	    double  left, right, bottom, top, zNear, zFar;

	    if (Tcl_GetDoubleFromObj(interp, objv[2], &left) == TCL_ERROR
		|| Tcl_GetDoubleFromObj(interp, objv[3],
					&right) == TCL_ERROR
		|| Tcl_GetDoubleFromObj(interp, objv[4],
					&bottom) == TCL_ERROR
		|| Tcl_GetDoubleFromObj(interp, objv[5],
					&top) == TCL_ERROR
		|| Tcl_GetDoubleFromObj(interp, objv[6],
					&zNear) == TCL_ERROR
		|| Tcl_GetDoubleFromObj(interp, objv[7],
					&zFar) == TCL_ERROR) {
		result = TCL_ERROR;
		break;
	    }
	    ToglOrtho(toglPtr, left, right, bottom, top, zNear, zFar);
	}
	break;
    case TOGL_NUMEYES:
	if (objc == 2) {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(
		toglPtr->stereo > TOGL_STEREO_ONE_EYE_MAX ? 2 : 1));
	} else {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    result = TCL_ERROR;
	}
	break;
    case TOGL_CONTEXTTAG:
	if (objc == 2) {
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(toglPtr->contextTag));
	} else {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    result = TCL_ERROR;
	}
	break;
    case TOGL_COPYCONTEXTTO:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    result = TCL_ERROR;
	} else {
	    Togl *to;
	    unsigned int mask;

	    if (GetToglFromObj(toglPtr->interp, objv[2], &to) == TCL_ERROR
		|| Tcl_GetIntFromObj(toglPtr->interp, objv[3],
				     (int *) &mask) == TCL_ERROR) {
		result = TCL_ERROR;
		break;
	    }
	    result = Togl_CopyContext(toglPtr, to, mask);
	}
	break;
    default:
	break;
    }
    Tcl_Release(toglPtr);
    return result;

  error:
    Tcl_Release(toglPtr);
    return TCL_ERROR;
}

static void
ToglPostRedisplay(Togl *toglPtr)
{
    if (!toglPtr->UpdatePending) {
        toglPtr->UpdatePending = True;
        Tcl_DoWhenIdle(ToglDisplay, (void *) toglPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ToglConfigure --
 *
 *	This procedure is called to process an argv/argc list in conjunction
 *	with the Tk option database to configure (or reconfigure) a togl
 *	widget.
 *
 * Results:
 *	The return value is a standard Tcl result. If TCL_ERROR is returned,
 *	then the interp's result contains an error message.
 *
 * Side effects:
 *	Configuration information gets set for toglPtr.
 *
 *----------------------------------------------------------------------
 */

static int
ToglConfigure(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Togl *toglPtr)		/* Information about widget. */
{
    /*
     * Register the desired geometry for the window. Then arrange for the
     * window to be redisplayed.
     */

    Tk_GeometryRequest(toglPtr->tkwin, toglPtr->width, toglPtr->height);
    if (!toglPtr->updatePending) {
	Tcl_DoWhenIdle(ToglDisplay, toglPtr);
	toglPtr->updatePending = 1;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ToglObjEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various events on
 *	togls.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get cleaned up. When
 *	it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
ToglObjEventProc(
    void *clientData,	/* Information about window. */
    XEvent *eventPtr)		/* Information about event. */
{
    Togl *toglPtr = (Togl *)clientData;

    switch(eventPtr->type) {
    case Expose:
	if (!toglPtr->updatePending) {
	    Tcl_DoWhenIdle(ToglDisplay, toglPtr);
	    toglPtr->updatePending = 1;
	}
	break;
    case ConfigureNotify:
	toglPtr->width = Tk_Width(toglPtr->tkwin);
	toglPtr->height = Tk_Height(toglPtr->tkwin);
	XResizeWindow(Tk_Display(toglPtr->tkwin), Tk_WindowId(toglPtr->tkwin),
		      toglPtr->width, toglPtr->height);
	if (!toglPtr->updatePending) {
	    Tcl_DoWhenIdle(ToglDisplay, toglPtr);
	    toglPtr->updatePending = 1;
	}
	break;
    case DestroyNotify:
	if (toglPtr->tkwin != NULL) {
	    Tk_FreeConfigOptions((char *) toglPtr, toglPtr->optionTable,
		    toglPtr->tkwin);
	    toglPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(toglPtr->interp,
		    toglPtr->widgetCmd);
	}
	if (toglPtr->updatePending) {
	    Tcl_CancelIdleCall(ToglDisplay, toglPtr);
	}
	Tcl_EventuallyFree(toglPtr, TCL_DYNAMIC);
	break;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ToglDeletedProc --
 *
 *	This procedure is invoked when a widget command is deleted. If the
 *	widget isn't already in the process of being destroyed, this command
 *	destroys it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
ToglDeletedProc(
    void *clientData)	/* Pointer to widget record for widget. */
{
    Togl *toglPtr = (Togl *)clientData;
    Tk_Window tkwin = toglPtr->tkwin;

    /*
     * This procedure could be invoked either because the window was destroyed
     * and the command was then deleted (in which case tkwin is NULL) or
     * because the command was deleted, and then this procedure destroys the
     * widget.
     */

    if (tkwin != NULL) {
	Tk_DestroyWindow(tkwin);
    }
    removeFromList(toglPtr);
    Togl_FreeResources(toglPtr);
}

/*
 *--------------------------------------------------------------
 *
 * ToglDisplay --
 *
 *	This procedure redraws the contents of a togl window. It is invoked
 *	as a do-when-idle handler, so it only runs when there's nothing else
 *	for the application to do.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */

static void
ToglDisplay(
    void *clientData)	/* Information about window. */
{
    Togl *toglPtr = (Togl *)clientData;
    Tk_Window tkwin = toglPtr->tkwin;

    printf("ToglDisplay\n");
    
    toglPtr->updatePending = 0;
    if (!Tk_IsMapped(tkwin)) {
	printf("%s is not mapped\n", Tk_PathName(tkwin));
	return;
    }
    Togl_Update(toglPtr);
    if (toglPtr->displayProc) {
        Togl_MakeCurrent(toglPtr);
        Togl_CallCallback(toglPtr, toglPtr->displayProc);
    }
    /* Simple test */
#if 1
    printf("Running test\n");
    Togl_MakeCurrent(toglPtr);	
    glClearColor(1, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    Togl_SwapBuffers(toglPtr);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Togl_Init --
 *
 *	Initialize the new package.  The string "Sample" in the
 *	function name must match the PACKAGE declaration at the top of
 *	configure.ac.
 *
 * Results:
 *	A standard Tcl result
 *
 * Side effects:
 *	The Togl package is created.
 *
 *----------------------------------------------------------------------
 */

#ifndef STRINGIFY
#  define STRINGIFY(x) STRINGIFY1(x)
#  define STRINGIFY1(x) #x
#endif

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

DLLEXPORT int
Togl_Init(
    Tcl_Interp* interp)		/* Tcl interpreter */
{
    if (Tcl_InitStubs(interp, "9.0", 0) == NULL) {
	return TCL_ERROR;
    }

    if (Tk_InitStubs(interp, "9.0", 0) == NULL) {
        return TCL_ERROR;
    }
    if (Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION, NULL) != TCL_OK) {
	return TCL_ERROR;
    }
    if (!Tcl_CreateObjCommand(interp, "togl", (Tcl_ObjCmdProc *)ToglObjCmd,
			      NULL, NULL)) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

/* Support for the -stereo custom option. */

static Tk_CustomOptionSetProc SetStereo;
static Tk_CustomOptionGetProc GetStereo;
static Tk_CustomOptionRestoreProc RestoreStereo;

static Tk_ObjCustomOption stereoOption = {
    "stereo",                   /* name */
    SetStereo,                  /* setProc */
    GetStereo,                  /* getProc */
    RestoreStereo,              /* restoreProc */
    NULL,                       /* freeProc */
    0
};

static Tcl_Obj *GetStereo(
    void *clientData,
    Tk_Window tkwin,
    char *recordPtr,
    Tcl_Size internalOffset)
{
    int     stereo = *(int *) (recordPtr + internalOffset);
    const char *name = "unknown";

    switch (stereo) {
      case TOGL_STEREO_NONE:
          name = "";
          break;
      case TOGL_STEREO_LEFT_EYE:
          name = "left eye";
          break;
      case TOGL_STEREO_RIGHT_EYE:
          name = "right eye";
          break;
      case TOGL_STEREO_NATIVE:
          name = "native";
          break;
      case TOGL_STEREO_SGIOLDSTYLE:
          name = "sgioldstyle";
          break;
      case TOGL_STEREO_ANAGLYPH:
          name = "anaglyph";
          break;
      case TOGL_STEREO_CROSS_EYE:
          name = "cross-eye";
          break;
      case TOGL_STEREO_WALL_EYE:
          name = "wall-eye";
          break;
      case TOGL_STEREO_DTI:
          name = "dti";
          break;
      case TOGL_STEREO_ROW_INTERLEAVED:
          name = "row interleaved";
          break;
    }
    return Tcl_NewStringObj(name, -1);
}

/* 
 *----------------------------------------------------------------------
 *
 * SetStereo --
 *
 *      Converts a Tcl_Obj representing a widgets stereo into an
 *      integer value.
 *
 * Results:
 *      Standard Tcl result.
 *
 * Side effects:
 *      May store the integer value into the internal representation
 *      pointer.  May change the pointer to the Tcl_Obj to NULL to indicate
 *      that the specified string was empty and that is acceptable.
 *
 *----------------------------------------------------------------------
 */

static int SetStereo(
    void *clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **value,
    char *recordPtr,
    Tcl_Size internalOffset,
    char *saveInternalPtr,
    int flags)
    /* interp is the current interp; may be used for errors. */
    /* tkwin is the Window for which option is being set. */
    /* value is a pointer to the pointer to the value object. We use a pointer
     * to the pointer because we may need to return a value (NULL). */
    /* recordPtr is a pointer to storage for the widget record. */
    /* internalOffset is the offset within *recordPtr at which the internal
     * value is to be stored. */
    /* saveInternalPtr is a pointer to storage for the old value. */
    /* flags are the flags for the option, set Tk_SetOptions. */
{
    int     stereo = 0;
    char   *string, *internalPtr;

    internalPtr = (internalOffset > 0) ? recordPtr + internalOffset : NULL;

    if ((flags & TK_OPTION_NULL_OK) && ObjectIsEmpty(*value)) {
        *value = NULL;
    } else {
        /* 
         * Convert the stereo specifier into an integer value.
         */

        if (Tcl_GetBooleanFromObj(NULL, *value, &stereo) == TCL_OK) {
            stereo = stereo ? TOGL_STEREO_NATIVE : TOGL_STEREO_NONE;
        } else {
            string = Tcl_GetString(*value);

            if (strcmp(string, "") == 0 || strcasecmp(string, "none") == 0) {
                stereo = TOGL_STEREO_NONE;
            } else if (strcasecmp(string, "native") == 0) {
                stereo = TOGL_STEREO_NATIVE;
                /* check if available when creating visual */
            } else if (strcasecmp(string, "left eye") == 0) {
                stereo = TOGL_STEREO_LEFT_EYE;
            } else if (strcasecmp(string, "right eye") == 0) {
                stereo = TOGL_STEREO_RIGHT_EYE;
            } else if (strcasecmp(string, "sgioldstyle") == 0) {
                stereo = TOGL_STEREO_SGIOLDSTYLE;
            } else if (strcasecmp(string, "anaglyph") == 0) {
                stereo = TOGL_STEREO_ANAGLYPH;
            } else if (strcasecmp(string, "cross-eye") == 0) {
                stereo = TOGL_STEREO_CROSS_EYE;
            } else if (strcasecmp(string, "wall-eye") == 0) {
                stereo = TOGL_STEREO_WALL_EYE;
            } else if (strcasecmp(string, "dti") == 0) {
                stereo = TOGL_STEREO_DTI;
            } else if (strcasecmp(string, "row interleaved") == 0) {
                stereo = TOGL_STEREO_ROW_INTERLEAVED;
            } else {
                Tcl_ResetResult(interp);
                Tcl_AppendResult(interp, "bad stereo value \"",
                        Tcl_GetString(*value), "\"", NULL);
                return TCL_ERROR;
            }
        }
    }

    if (internalPtr != NULL) {
        *((int *) saveInternalPtr) = *((int *) internalPtr);
        *((int *) internalPtr) = stereo;
    }
    return TCL_OK;
}

/* 
 *----------------------------------------------------------------------
 * RestoreStereo --
 *
 *      Restore a stereo option value from a saved value.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Restores the old value.
 *
 *----------------------------------------------------------------------
 */

static void
RestoreStereo(
    void *clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr)
{
    *(int *) internalPtr = *(int *) saveInternalPtr;
}
/* 
 * The following structure contains pointers to functions used for
 * processing the custom "-pixelformat" option.  Copied from tkPanedWindow.c.
 */
/* 
 * Support for the custom "-pixelformat" option.
 */
static Tk_CustomOptionSetProc SetWideInt;
static Tk_CustomOptionGetProc GetWideInt;
static Tk_CustomOptionRestoreProc RestoreWideInt;

static Tk_ObjCustomOption wideIntOption = {
    "wide int",                 /* name */
    SetWideInt,                 /* setProc */
    GetWideInt,                 /* getProc */
    RestoreWideInt,             /* restoreProc */
    NULL,                       /* freeProc */
    0
};
/* 
 *----------------------------------------------------------------------
 *
 * GetWideInt -
 *
 *      Converts an internal wide integer into a a Tcl WideInt obj.
 *
 * Results:
 *      Tcl_Obj containing the wide int value.
 *
 * Side effects:
 *      Creates a new Tcl_Obj.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *GetWideInt(
    void *clientData,
    Tk_Window tkwin,
    char *recordPtr,
    Tcl_Size internalOffset)
{
    Tcl_WideInt wi = *(Tcl_WideInt *) (recordPtr + internalOffset);
    return Tcl_NewWideIntObj(wi);
}

/* 
 *----------------------------------------------------------------------
 *
 * SetWideInt --
 *
 *      Converts a Tcl_Obj representing a Tcl_WideInt.
 *
 * Results:
 *      Standard Tcl result.
 *
 * Side effects:
 *      May store the wide int value into the internal representation
 *      pointer.  May change the pointer to the Tcl_Obj to NULL to indicate
 *      that the specified string was empty and that is acceptable.
 *
 *----------------------------------------------------------------------
 */

static int SetWideInt(
    void *clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **value,
    char *recordPtr,
    Tcl_Size internalOffset,
    char *saveInternalPtr,
    int flags)
{
    Tcl_WideInt w;
    char   *internalPtr;
    internalPtr = (internalOffset > 0) ? recordPtr + internalOffset : NULL;

    if ((flags & TK_OPTION_NULL_OK) && ObjectIsEmpty(*value)) {
        *value = NULL;
        w = 0;
    } else {
        if (Tcl_GetWideIntFromObj(interp, *value, &w) != TCL_OK) {
            return TCL_ERROR;
        }
    }

    if (internalPtr != NULL) {
        *((Tcl_WideInt *) saveInternalPtr) = *((Tcl_WideInt *) internalPtr);
        *((Tcl_WideInt *) internalPtr) = w;
    }
    return TCL_OK;
}
/* 
 *----------------------------------------------------------------------
 * RestoreWideInt --
 *
 *      Restore a wide int option value from a saved value.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Restores the old value.
 *
 *----------------------------------------------------------------------
 */


static void
RestoreWideInt(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr)
{
    *(Tcl_WideInt *) internalPtr = *(Tcl_WideInt *) saveInternalPtr;
}

/* 
 *----------------------------------------------------------------------
 *
 * ObjectIsEmpty --
 *
 *      This procedure tests whether the string value of an object is
 *      empty.
 *
 * Results:
 *      The return value is 1 if the string value of objPtr has length
 *      zero, and 0 otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
ObjectIsEmpty(Tcl_Obj *objPtr)
/* objPtr = Object to test.  May be NULL. */
{
    Tcl_Size length;

    if (objPtr == NULL) {
        return 1;
    }
    if (objPtr->bytes != NULL) {
        return (objPtr->length == 0);
    }
    Tcl_GetStringFromObj(objPtr, &length);
    return (length == 0);
}

/* 
 * Togl_CallCallback
 *
 * Call command with togl widget as only argument
 */

int
Togl_CallCallback(Togl *togl, Tcl_Obj *cmd)
{
    int     result;
    Tcl_Obj *objv[3];

    if (cmd == NULL || togl->widgetCmd == NULL)
        return TCL_OK;

    objv[0] = cmd;
    Tcl_IncrRefCount(objv[0]);
    objv[1] =
            Tcl_NewStringObj(Tcl_GetCommandName(togl->interp, togl->widgetCmd),
            -1);
    Tcl_IncrRefCount(objv[1]);
    objv[2] = NULL;
    result = Tcl_EvalObjv(togl->interp, 2, objv, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(objv[1]);
    Tcl_DecrRefCount(objv[0]);
    if (result != TCL_OK)
        Tcl_BackgroundError(togl->interp);
    return result;
}


/* 
 *----------------------------------------------------------------------
 *
 * Utilities for managing the list of all Togl widgets.
 *
 *----------------------------------------------------------------------
 */

/* 
 * Add a togl widget to the top of the linked list.
 */
static void
addToList(Togl *t)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    t->next = tsdPtr->toglHead;
    tsdPtr->toglHead = t;
}

/* 
 * Remove a togl widget from the linked list.
 */
static void
removeFromList(Togl *t)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    Togl   *prev, *cur;
    for (cur = tsdPtr->toglHead, prev = NULL; cur; prev = cur, cur = cur->next) {
        if (t != cur)
            continue;
        if (prev) {
            prev->next = cur->next;
        } else {
            tsdPtr->toglHead = cur->next;
        }
        break;
    }
    if (cur)
        cur->next = NULL;
}

/* 
 * Return a pointer to the widget record of the Togl with a given pathname.
 */
Togl *
FindTogl(Togl *togl, const char *ident)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    Togl   *t;

    if (ident[0] != '.') {
        for (t = tsdPtr->toglHead; t; t = t->next) {
            if (strcmp(t->ident, ident) == 0)
                break;
        }
    } else {
        for (t = tsdPtr->toglHead; t; t = t->next) {
            const char *pathname = Tk_PathName(t->tkwin);

            if (strcmp(pathname, ident) == 0)
                break;
        }
    }
    return t;
}

/* 
 * Return pointer to another togl widget with same OpenGL context.
 */
Togl *
FindToglWithSameContext(const Togl *togl)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
        Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    Togl   *t;

    for (t = tsdPtr->toglHead; t != NULL; t = t->next) {
        if (t == togl)
            continue;
        if ((void *)t->context == (void *)togl->context) {
            return t;
        }
    }
    return NULL;
}

/*
 * ToglFrustum and ToglOrtho:
 *
 *     eyeOffset is the distance from the center line
 *     and is negative for the left eye and positive for right eye.
 *     eyeDist and eyeOffset need to be in the same units as your model space.
 *     In physical space, eyeDist might be 30 inches from the screen
 *     and eyeDist would be +/- 1.25 inch (for a total interocular distance
 *     of 2.5 inches).
 */
static void
ToglFrustum(const Togl *togl, GLdouble left, GLdouble right,
        GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    GLdouble eyeOffset = 0, eyeShift = 0;

    if (togl->stereo == TOGL_STEREO_LEFT_EYE
            || togl->currentStereoBuffer == STEREO_BUFFER_LEFT)
        eyeOffset = -togl->eyeSeparation / 2;   /* for left eye */
    else if (togl->stereo == TOGL_STEREO_RIGHT_EYE
            || togl->currentStereoBuffer == STEREO_BUFFER_RIGHT)
        eyeOffset = togl->eyeSeparation / 2;    /* for right eye */
    eyeShift = (togl->convergence - zNear) * (eyeOffset / togl->convergence);

    /* compenstate for altered viewports */
    switch (togl->stereo) {
      default:
          break;
      case TOGL_STEREO_SGIOLDSTYLE:
      case TOGL_STEREO_DTI:
          /* squished image is expanded, nothing needed */
          break;
      case TOGL_STEREO_CROSS_EYE:
      case TOGL_STEREO_WALL_EYE:{
          GLdouble delta = (top - bottom) / 2;

          top += delta;
          bottom -= delta;
          break;
      }
    }

    glFrustum(left + eyeShift, right + eyeShift, bottom, top, zNear, zFar);
    glTranslated(-eyeShift, 0, 0);
}

static void
ToglOrtho(const Togl *togl, GLdouble left, GLdouble right,
        GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    /* TODO: debug this */
    GLdouble eyeOffset = 0, eyeShift = 0;

    if (togl->currentStereoBuffer == STEREO_BUFFER_LEFT)
        eyeOffset = -togl->eyeSeparation / 2;   /* for left eye */
    else if (togl->currentStereoBuffer == STEREO_BUFFER_RIGHT)
        eyeOffset = togl->eyeSeparation / 2;    /* for right eye */
    eyeShift = (togl->convergence - zNear) * (eyeOffset / togl->convergence);

    /* compenstate for altered viewports */
    switch (togl->stereo) {
      default:
          break;
      case TOGL_STEREO_SGIOLDSTYLE:
      case TOGL_STEREO_DTI:
          /* squished image is expanded, nothing needed */
          break;
      case TOGL_STEREO_CROSS_EYE:
      case TOGL_STEREO_WALL_EYE:{
          GLdouble delta = (top - bottom) / 2;

          top += delta;
          bottom -= delta;
          break;
      }
    }

    glOrtho(left + eyeShift, right + eyeShift, bottom, top, zNear, zFar);
    glTranslated(-eyeShift, 0, 0);
}

static int
GetToglFromObj(Tcl_Interp *interp, Tcl_Obj *obj, Togl **toglPtr)
{
    Tcl_Command toglCmd;
    Tcl_CmdInfo info;

    toglCmd = Tcl_GetCommandFromObj(interp, obj);
    if (Tcl_GetCommandInfoFromToken(toglCmd, &info) == 0
            || info.objProc != ToglWidgetObjCmd) {
        Tcl_AppendResult(interp, "expected togl command argument", NULL);
        return TCL_ERROR;
    }
    *toglPtr = (Togl *) info.objClientData;
    return TCL_OK;
}

    
#ifdef __cplusplus
}
#endif  /* __cplusplus */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
