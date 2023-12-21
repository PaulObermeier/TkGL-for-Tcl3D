/*
 * togl.c --
 *
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef USE_TCL_STUBS
#   define USE_TCL_STUBS
#endif
#ifndef USE_TK_STUBS
#   define USE_TK_STUBS
#endif
#include <string.h>
#include "tk.h"
#include "togl.h"

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ToglDeletedProc(void *clientData);
static int		ToglConfigure(Tcl_Interp *interp, Togl *toglPtr);
static void		ToglDisplay(void *clientData);
static void		ToglObjEventProc(void *clientData,
			    XEvent *eventPtr);
static int		ToglWidgetObjCmd(void *clientData,
			    Tcl_Interp *, int objc, Tcl_Obj * const objv[]);

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
    // clientData should point to the global package data

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
     * Allocate and initialize the widget record. The memset allows us to set
     * just the non-NULL/0 items.
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

    Tk_CreateEventHandler(toglPtr->tkwin, ExposureMask|StructureNotifyMask,
	    ToglObjEventProc, toglPtr);
    if (Tk_SetOptions(interp, toglPtr, optionTable, objc - 2,
	    objv + 2, tkwin, NULL, NULL) != TCL_OK) {
	goto error;
    }
    if (ToglConfigure(interp, toglPtr) != TCL_OK) {
	goto error;
    }

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
 *	This procedure is invoked to process the Tcl command that corresponds
 *	to a widget managed by this module. See the user documentation for
 *	details on what it does.
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
    void *clientData,	/* Information about togl widget. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj * const objv[])	/* Argument objects. */
{
    Togl *toglPtr = (Togl *)clientData;
    int result = TCL_OK;
    static const char *const toglOptions[] = {"cget", "configure", NULL};
    enum {
	TOGL_CGET, TOGL_CONFIGURE
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
    }
    Tcl_Release(toglPtr);
    return result;

  error:
    Tcl_Release(toglPtr);
    return TCL_ERROR;
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
 *	Configuration information, such as colors, border width, etc. get set
 *	for toglPtr; old resources get freed, if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ToglConfigure(
    Tcl_Interp *interp,		/* Used for error reporting. */
    Togl *toglPtr)		/* Information about widget. */
{
    int borderWidth;
    Tk_3DBorder bgBorder;
    //    int doubleBuffer;
    int width;
    int height;

    Tk_GetPixelsFromObj(NULL, toglPtr->tkwin, toglPtr->widthObjPtr, &width);
    Tk_GetPixelsFromObj(NULL, toglPtr->tkwin, toglPtr->heightObjPtr, &height);

    /*
     * Set the background for the window and create a graphics context for use
     * during redisplay.
     */

    bgBorder = Tk_Get3DBorderFromObj(toglPtr->tkwin,
	    toglPtr->bgBorderPtr);
    Tk_SetWindowBackground(toglPtr->tkwin,
	    Tk_3DBorderColor(bgBorder)->pixel);
    /*
     * Register the desired geometry for the window. Then arrange for the
     * window to be redisplayed.
     */

    Tk_GeometryRequest(toglPtr->tkwin, width, height);
    Tk_GetPixelsFromObj(NULL, toglPtr->tkwin, toglPtr->borderWidthPtr,
	    &borderWidth);
    Tk_SetInternalBorder(toglPtr->tkwin, borderWidth);
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

    if (eventPtr->type == Expose) {
	if (!toglPtr->updatePending) {
	    Tcl_DoWhenIdle(ToglDisplay, toglPtr);
	    toglPtr->updatePending = 1;
	}
    } else if (eventPtr->type == ConfigureNotify) {
	//KeepInWindow(toglPtr);
	if (!toglPtr->updatePending) {
	    Tcl_DoWhenIdle(ToglDisplay, toglPtr);
	    toglPtr->updatePending = 1;
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (toglPtr->tkwin != NULL) {
	    Tk_FreeConfigOptions((char *) toglPtr, toglPtr->optionTable,
		    toglPtr->tkwin);
	    //	    if (toglPtr->gc != NULL) {
	    //	Tk_FreeGC(toglPtr->display, toglPtr->gc);
	    //}
	    toglPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(toglPtr->interp,
		    toglPtr->widgetCmd);
	}
	if (toglPtr->updatePending) {
	    Tcl_CancelIdleCall(ToglDisplay, toglPtr);
	}
	Tcl_EventuallyFree(toglPtr, TCL_DYNAMIC);
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
    Drawable d;
    int borderWidth, width, height, relief;
    Tk_3DBorder bgBorder, fgBorder;
    
    toglPtr->updatePending = 0;
    if (!Tk_IsMapped(tkwin)) {
	return;
    }
    d = Tk_WindowId(tkwin);
    Tk_GetPixelsFromObj(NULL, toglPtr->tkwin, toglPtr->widthObjPtr, &width);
    Tk_GetPixelsFromObj(NULL, toglPtr->tkwin, toglPtr->heightObjPtr, &height);

    /*
     * Redraw the widget's background and border.
     */

    Tk_GetPixelsFromObj(NULL, toglPtr->tkwin, toglPtr->borderWidthPtr,
	    &borderWidth);
    bgBorder = Tk_Get3DBorderFromObj(toglPtr->tkwin,
	    toglPtr->bgBorderPtr);
    Tk_GetReliefFromObj(NULL, toglPtr->reliefPtr, &relief);
    Tk_Fill3DRectangle(tkwin, d, bgBorder, 0, 0, Tk_Width(tkwin),
	    Tk_Height(tkwin), borderWidth, relief);

    /*
     * Display the togl widget.
     */
    fgBorder = Tk_Get3DBorderFromObj(toglPtr->tkwin,
	    toglPtr->fgBorderPtr);

    Tk_Fill3DRectangle(tkwin, d, fgBorder, toglPtr->x, toglPtr->y, width,
           height, borderWidth, TK_RELIEF_RAISED);

    Tk_Fill3DRectangle(tkwin, d, fgBorder, toglPtr->x, toglPtr->y, width,
	    height, borderWidth, TK_RELIEF_RAISED);
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
