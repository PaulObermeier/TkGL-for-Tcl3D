/*
 * Declarations of custom option structs.
 * This file should only be included by togl.c,
 */
#ifndef TOGL_OPTIONS_H
/* For compatibility with Tcl 8.6 */ 
#ifndef TCL_INDEX_NONE
#define TCL_INDEX_NONE {-1}
#endif

/*
 * The following table defines the legal values for the -profile
 * option:
 */

static const char *const profileStrings[] = {
  "legacy", "3_2", "4_1", "system", NULL
};

static Tk_ObjCustomOption stereoOption;
static Tk_ObjCustomOption wideIntOption;

static Tk_OptionSpec toglOptionSpecs[] = {
    {TK_OPTION_INT, "-width", "width", "Width", DEFAULT_WIDTH,
     TCL_INDEX_NONE, offsetof(Togl, width), 0, NULL, GEOMETRY_MASK},
    {TK_OPTION_INT, "-height", "height", "Height", DEFAULT_HEIGHT,
     TCL_INDEX_NONE, offsetof(Togl, height),0, NULL, GEOMETRY_MASK},
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
#define TOGL_OPTIONS_H
#endif
