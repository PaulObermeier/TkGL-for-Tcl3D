/*
 * Declarations of custom option structs.
 * This file should only be included by tkgl.c,
 */
#ifndef TKGL_OPTIONS_H
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

static Tk_OptionSpec tkglOptionSpecs[] = {
    {TK_OPTION_INT, "-width", "width", "Width", DEFAULT_WIDTH,
     TCL_INDEX_NONE, offsetof(Tkgl, width), 0, NULL, GEOMETRY_MASK},
    {TK_OPTION_INT, "-height", "height", "Height", DEFAULT_HEIGHT,
     TCL_INDEX_NONE, offsetof(Tkgl, height),0, NULL, GEOMETRY_MASK},
    {TK_OPTION_BOOLEAN, "-rgba", "rgba", "RGBA", "true",
     TCL_INDEX_NONE, offsetof(Tkgl, rgbaFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-redsize", "redsize", "RedSize", "1",
     TCL_INDEX_NONE, offsetof(Tkgl, rgbaRed), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-greensize", "greensize", "GreenSize", "1",
     TCL_INDEX_NONE, offsetof(Tkgl, rgbaGreen), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-bluesize", "bluesize", "BlueSize", "1",
     TCL_INDEX_NONE, offsetof(Tkgl, rgbaBlue), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-double", "double", "Double", "false",
     TCL_INDEX_NONE, offsetof(Tkgl, doubleFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-depth", "depth", "Depth", "false",
     TCL_INDEX_NONE, offsetof(Tkgl, depthFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-depthsize", "depthsize", "DepthSize", "1",
     TCL_INDEX_NONE, offsetof(Tkgl, depthSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-accum", "accum", "Accum", "false",
     TCL_INDEX_NONE, offsetof(Tkgl, accumFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumredsize", "accumredsize", "accumRedSize", "1",
     TCL_INDEX_NONE, offsetof(Tkgl, accumRed), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumgreensize", "accumgreensize", "AccumGreenSize", "1",
     TCL_INDEX_NONE, offsetof(Tkgl, accumGreen), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumbluesize", "accumbluesize", "AccumBlueSize", "1",
     TCL_INDEX_NONE, offsetof(Tkgl, accumBlue), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-accumalphasize", "accumalphasize", "AccumAlphaSize", "1",
     TCL_INDEX_NONE, offsetof(Tkgl, accumAlpha), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-alpha", "alpha", "Alpha", "false",
     TCL_INDEX_NONE, offsetof(Tkgl, alphaFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-alphasize", "alphasize", "AlphaSize", "1",
     TCL_INDEX_NONE, offsetof(Tkgl, alphaSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-stencil", "stencil", "Stencil", "false",
     TCL_INDEX_NONE, offsetof(Tkgl, stencilFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-stencilsize", "stencilsize", "StencilSize", "1",
     TCL_INDEX_NONE, offsetof(Tkgl, stencilSize), 0, NULL, FORMAT_MASK},
    {TK_OPTION_INT, "-auxbuffers", "auxbuffers", "AuxBuffers", "0",
     TCL_INDEX_NONE, offsetof(Tkgl, auxNumber), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-privatecmap", "privateCmap", "PrivateCmap", "false",
     TCL_INDEX_NONE, offsetof(Tkgl, privateCmapFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-overlay", "overlay", "Overlay", "false",
     TCL_INDEX_NONE, offsetof(Tkgl, overlayFlag), 0, NULL, OVERLAY_MASK},
    {TK_OPTION_CUSTOM, "-stereo", "stereo", "Stereo", "",
     TCL_INDEX_NONE, offsetof(Tkgl, stereo), 0, (void*) &stereoOption,
     STEREO_FORMAT_MASK},
    {TK_OPTION_DOUBLE, "-eyeseparation", "eyeseparation", "EyeSeparation", "2.0",
     TCL_INDEX_NONE, offsetof(Tkgl, eyeSeparation), 0, NULL, STEREO_MASK},
    {TK_OPTION_DOUBLE, "-convergence", "convergence", "Convergence", "35.0",
     TCL_INDEX_NONE, offsetof(Tkgl, convergence), 0, NULL, STEREO_MASK},
    {TK_OPTION_CURSOR, "-cursor", "cursor", "Cursor", "",
     TCL_INDEX_NONE, offsetof(Tkgl, cursor), TK_OPTION_NULL_OK, NULL, CURSOR_MASK},
    {TK_OPTION_INT, "-setgrid", "setGrid", "SetGrid", "0",
     TCL_INDEX_NONE, offsetof(Tkgl, setGrid), 0, NULL, GEOMETRY_MASK},
    {TK_OPTION_INT, "-time", "time", "Time", DEFAULT_TIME,
     TCL_INDEX_NONE, offsetof(Tkgl, timerInterval), 0, NULL, TIMER_MASK},
    {TK_OPTION_STRING, "-sharelist", "sharelist", "ShareList", NULL,
     TCL_INDEX_NONE, offsetof(Tkgl, shareList), 0, NULL, FORMAT_MASK},
    {TK_OPTION_STRING, "-sharecontext", "sharecontext", "ShareContext", NULL,
     TCL_INDEX_NONE, offsetof(Tkgl, shareContext), 0, NULL, FORMAT_MASK},
    {TK_OPTION_STRING, "-ident", "ident", "Ident", DEFAULT_IDENT,
     TCL_INDEX_NONE, offsetof(Tkgl, ident), 0, NULL, 0},
    {TK_OPTION_CUSTOM, "-pixelformat", "pixelFormat", "PixelFormat", "0",
     TCL_INDEX_NONE, offsetof(Tkgl, pixelFormat), 0, (void *) &wideIntOption,
     FORMAT_MASK},
    {TK_OPTION_INT, "-swapinterval", "swapInterval", "SwapInterval", "1",
     TCL_INDEX_NONE, offsetof(Tkgl, swapInterval), 0, NULL, SWAP_MASK},
    {TK_OPTION_BOOLEAN, "-fullscreen", "fullscreen", "Fullscreen", "false",
     TCL_INDEX_NONE, offsetof(Tkgl, fullscreenFlag), 0, NULL,
     GEOMETRY_MASK|FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-multisample", "multisample", "Multisample", "false",
     TCL_INDEX_NONE, offsetof(Tkgl, multisampleFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-pbuffer", "pbuffer", "Pbuffer", "false",
     TCL_INDEX_NONE, offsetof(Tkgl, pBufferFlag), 0, NULL, FORMAT_MASK},
    {TK_OPTION_BOOLEAN, "-largestpbuffer", "largestpbuffer", "LargestPbuffer", "false",
     TCL_INDEX_NONE, offsetof(Tkgl, largestPbufferFlag), 0, NULL, 0},
    {TK_OPTION_STRING, "-createcommand", "createCommand", "CallbackCommand", NULL,
     offsetof(Tkgl, createProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-create", NULL, NULL, NULL, TCL_INDEX_NONE, TCL_INDEX_NONE, 0,
     (void *) "-createcommand", 0},
    {TK_OPTION_STRING, "-displaycommand", "displayCommand", "CallbackCommand", NULL,
     offsetof(Tkgl, displayProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-display", NULL, NULL, NULL,
     TCL_INDEX_NONE, TCL_INDEX_NONE, 0, (void *) "-displaycommand", 0},
    {TK_OPTION_STRING, "-reshapecommand", "reshapeCommand", "CallbackCommand", NULL,
     offsetof(Tkgl, reshapeProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-reshape", NULL, NULL, NULL,
     TCL_INDEX_NONE, TCL_INDEX_NONE, 0, (void *) "-reshapecommand", 0},
    {TK_OPTION_STRING, "-destroycommand", "destroyCommand", "CallbackCommand", NULL,
     offsetof(Tkgl, destroyProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-destroy", NULL, NULL, NULL,
     TCL_INDEX_NONE, TCL_INDEX_NONE, 0, (void *) "-destroycommand", 0},
    {TK_OPTION_STRING, "-timercommand", "timerCommand", "CallbackCommand", NULL,
     offsetof(Tkgl, timerProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, 0},
    {TK_OPTION_SYNONYM, "-timer", NULL, NULL, NULL,
     TCL_INDEX_NONE, TCL_INDEX_NONE, 0, (void *) "-timercommand", 0},
    {TK_OPTION_STRING, "-overlaydisplaycommand", "overlaydisplayCommand", "CallbackCommand", NULL,
     offsetof(Tkgl, overlayDisplayProc), TCL_INDEX_NONE, TK_OPTION_NULL_OK, NULL, OVERLAY_MASK},
    {TK_OPTION_SYNONYM, "-overlaydisplay", NULL, NULL, NULL,
     TCL_INDEX_NONE, TCL_INDEX_NONE, 0, (ClientData) "-overlaydisplaycommand", 0},
    {TK_OPTION_STRING_TABLE, "-profile", "profile", "Profile", "legacy",
     TCL_INDEX_NONE, offsetof(Tkgl, profile), 0, profileStrings, 0},
    {TK_OPTION_END, NULL, NULL, NULL, NULL, 0, 0, 0, NULL, 0}
};
#define TKGL_OPTIONS_H
#endif
