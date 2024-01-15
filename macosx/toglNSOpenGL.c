#include "togl.h"
#include <OpenGL/glext.h>
#include <OpenGL/gl.h>

#include <Foundation/Foundation.h>   /* for NSRect */
#include <OpenGL/OpenGL.h>
#include <AppKit/NSOpenGL.h>
#include <AppKit/NSView.h>
#include <tkMacOSXInt.h>              /* for MacDrawable */
#include <ApplicationServices/ApplicationServices.h>
#define Togl_MacOSXGetDrawablePort(togl) TkMacOSXGetDrawablePort((Drawable) ((TkWindow *) togl->TkWin)->privatePtr)

static NSOpenGLPixelFormat *
togl_pixelFormat(Togl *togl)
{
    NSOpenGLPixelFormatAttribute   attribs[32];
    int     na = 0;
    NSOpenGLPixelFormat *pix;

#if 0
    if (togl->multisampleFlag && !hasMultisampling) {
        Tcl_SetResult(togl->interp,
                "multisampling not supported", TCL_STATIC);
        return NULL;
    }
#endif

    if (togl->pBufferFlag && !togl->rgbaFlag) {
        Tcl_SetResult(togl->interp,
                "puffer must be RGB[A]", TCL_STATIC);
        return NULL;
    }

    attribs[na++] = NSOpenGLPFAMinimumPolicy;
    /* ask for hardware-accelerated onscreen */
    /* This is not needed, and can break virtual machines.
       Accelerated rendering is always preferred.
    attribs[na++] = NSOpenGLPFAAccelerated;
    attribs[na++] = NSOpenGLPFANoRecovery;
    */
    if (togl->rgbaFlag) {
        /* RGB[A] mode */
        attribs[na++] = NSOpenGLPFAColorSize;
	attribs[na++] = togl->rgbaRed + togl->rgbaGreen + togl->rgbaBlue;
	/* NSOpenGL does not take separate red,green,blue sizes. */
        if (togl->alphaFlag) {
            attribs[na++] = NSOpenGLPFAAlphaSize;
            attribs[na++] = togl->alphaSize;
        }
    } else {
        /* Color index mode */
        Tcl_SetResult(togl->interp,
                "Color index mode not supported", TCL_STATIC);
        return NULL;
    }
    if (togl->depthFlag) {
        attribs[na++] = NSOpenGLPFADepthSize;
        attribs[na++] = togl->depthSize;
    }
    if (togl->doubleFlag) {
        attribs[na++] = NSOpenGLPFADoubleBuffer;
    }
    if (togl->stencilFlag) {
        attribs[na++] = NSOpenGLPFAStencilSize;
        attribs[na++] = togl->stencilSize;
    }
    if (togl->accumFlag) {
        attribs[na++] = NSOpenGLPFAAccumSize;
        attribs[na++] = togl->accumRed + togl->accumGreen + togl->accumBlue + (togl->alphaFlag ? togl->accumAlpha : 0);
    }
    if (togl->multisampleFlag) {
        attribs[na++] = NSOpenGLPFAMultisample;
        attribs[na++] = NSOpenGLPFASampleBuffers;
        attribs[na++] = 1;
        attribs[na++] = NSOpenGLPFASamples;
        attribs[na++] = 2;
    }
    if (togl->auxNumber != 0) {
        attribs[na++] = NSOpenGLPFAAuxBuffers;
        attribs[na++] = togl->auxNumber;
    }
    if (togl->stereo == TOGL_STEREO_NATIVE) {
        attribs[na++] = NSOpenGLPFAStereo;
    }
    if (togl->fullscreenFlag) {
        Tcl_SetResult(togl->interp,
                "FullScreen mode not supported.", TCL_STATIC);
        return NULL;
    }
    printf("togl profile is %d\n", togl-> profile);
    switch(togl->profile) {
    case PROFILE_LEGACY:
	attribs[na++] = NSOpenGLPFAOpenGLProfile;
	attribs[na++] = NSOpenGLProfileVersionLegacy;
	break;
    case PROFILE_3_2:
	attribs[na++] = NSOpenGLPFAOpenGLProfile;
	attribs[na++] = NSOpenGLProfileVersion3_2Core;
	break;
    case PROFILE_SYSTEM:
    case PROFILE_4_1:
	attribs[na++] = NSOpenGLPFAOpenGLProfile;
	attribs[na++] = NSOpenGLProfileVersion4_1Core;
	break;
    }
    attribs[na++] = 0;	/* End of attributes. */

    pix = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
    if (pix == nil) {
        Tcl_SetResult(togl->interp, "couldn't choose pixel format",
                TCL_STATIC);
        return NULL;
    }
    return pix;
}

static int
togl_describePixelFormat(Togl *togl)
{
    NSOpenGLPixelFormat *pfmt = togl->pixelFormat;

    /* fill in RgbaFlag, DoubleFlag, and Stereo */
    GLint   has_rgba, has_doublebuf, has_depth, has_accum, has_alpha,
            has_stencil, has_stereo, has_multisample;

    GLint   vscr = 0;
    [pfmt getValues:&has_rgba forAttribute:NSOpenGLPFAColorSize forVirtualScreen:vscr];
    [pfmt getValues:&has_doublebuf forAttribute:NSOpenGLPFADoubleBuffer forVirtualScreen:vscr];
    [pfmt getValues:&has_depth forAttribute:NSOpenGLPFADepthSize forVirtualScreen:vscr];
    [pfmt getValues:&has_accum forAttribute:NSOpenGLPFAAccumSize forVirtualScreen:vscr];
    [pfmt getValues:&has_alpha forAttribute:NSOpenGLPFAAlphaSize forVirtualScreen:vscr];
    [pfmt getValues:&has_stencil forAttribute:NSOpenGLPFAStencilSize forVirtualScreen:vscr];
    [pfmt getValues:&has_stereo forAttribute:NSOpenGLPFAStereo forVirtualScreen:vscr];
    [pfmt getValues:&has_multisample forAttribute:NSOpenGLPFASampleBuffers forVirtualScreen:vscr];

    togl->rgbaFlag = (has_rgba != 0);
    togl->doubleFlag = (has_doublebuf != 0);
    togl->depthFlag = (has_depth != 0);
    togl->accumFlag = (has_accum != 0);
    togl->alphaFlag = (has_alpha != 0);
    togl->stencilFlag = (has_stencil != 0);
    togl->stereo = (has_stereo ? TOGL_STEREO_NATIVE : TOGL_STEREO_NONE);
    togl->multisampleFlag = (has_multisample != 0);
    return True;
}

#define isPow2(x) (((x) & ((x) - 1)) == 0)

static NSOpenGLPixelBuffer *
togl_createPbuffer(Togl *togl)
{
    GLint   min_size[2], max_size[2];
    Bool    hasPbuffer;
    const char *extensions;
    GLint   target;
    GLint   virtualScreen;
    NSOpenGLPixelBuffer *pbuf;

    extensions = (const char *) glGetString(GL_EXTENSIONS);
    hasPbuffer = (strstr(extensions, "GL_APPLE_pixel_buffer") != NULL);
    if (!hasPbuffer) {
        Tcl_SetResult(togl->interp,
                "pbuffers are not supported", TCL_STATIC);
        return NULL;
    }
    glGetIntegerv(GL_MIN_PBUFFER_VIEWPORT_DIMS_APPLE, min_size);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_size);
    virtualScreen = [togl->context currentVirtualScreen];
    for (;;) {
        /* make sure we don't exceed the maximum size because if we do,
         * NSOpenGLPixelBuffer allocationmay succeed and later uses of
	 * the pbuffer fail
	 */
        if (togl->width < min_size[0])
            togl->width = min_size[0];
        else if (togl->width > max_size[0]) {
            if (togl->largestPbufferFlag)
                togl->width = max_size[0];
            else {
                Tcl_SetResult(togl->interp,
                        "pbuffer too large", TCL_STATIC);
                return NULL;
            }
        }
        if (togl->height < min_size[1])
            togl->height = min_size[1];
        else if (togl->height > max_size[1]) {
            if (togl->largestPbufferFlag)
                togl->height = max_size[1];
            else {
                Tcl_SetResult(togl->interp,
                        "pbuffer too large", TCL_STATIC);
                return NULL;
            }
        }

        if (isPow2(togl->width) && isPow2(togl->height))
            target = GL_TEXTURE_2D;
        else
            target = GL_TEXTURE_RECTANGLE_ARB;

	pbuf = [[NSOpenGLPixelBuffer alloc] initWithTextureTarget:target
		textureInternalFormat:(togl->alphaFlag ? GL_RGBA : GL_RGB)
		textureMaxMipMapLevel:0
		pixelsWide:togl->width pixelsHigh:togl->height];
        if (pbuf != nil) {
            /* setPixelBuffer allocates the framebuffer space */
	  [togl->context setPixelBuffer:pbuf cubeMapFace:0 mipMapLevel:0 
	   currentVirtualScreen:virtualScreen];
	  return pbuf;
	}
        if (!togl->largestPbufferFlag
                || togl->width == min_size[0] || togl->height == min_size[1]) {
            Tcl_SetResult(togl->interp,
                    "unable to create pbuffer", TCL_STATIC);
            return NULL;
        }
        /* largest unavailable, try something smaller */
        togl->width = togl->width / 2 + togl->width % 2;
        togl->height = togl->width / 2 + togl->height % 2;
    }
}

void
togl_destroyPbuffer(Togl *togl)
{
    [togl->pbuf release];
}

/* Declarations that Apple leaves out of gl.h */
extern const GLubyte *glGetStringi(GLenum name, GLuint index);
extern void glGetIntegerv(GLenum pname, GLint  *params);
#define GL_NUM_EXTENSIONS 0x821D

const char* Togl_GetExtensions(
    Togl *toglPtr)
{
    char *buffer = NULL;
    int bufsize = 0;
    int strsize = 0;
    int num;
    
    if (toglPtr->profile == PROFILE_LEGACY) {
	return (const char *)glGetString(GL_EXTENSIONS);
    }
    if (toglPtr->extensions) {
	return toglPtr->extensions;
    }
    glGetIntegerv(GL_NUM_EXTENSIONS, &num);
    buffer = ckalloc(1536);
    if (!buffer) {
	return NULL;
    }
    bufsize = 1536;
    for (int i = 0; i < num; i++) {
	char *ext = (char *)glGetStringi(GL_EXTENSIONS, i);
	int len = strlen(ext);
	if (strsize + len > bufsize) {
	    buffer = ckrealloc(buffer, len + 2*bufsize);
	    if (buffer == 0) {
		strsize = bufsize = 0;
		return NULL;
	    }
	    bufsize = len + 2*bufsize;
	}
	strsize += strlcpy(buffer + strsize, ext, bufsize);
	strsize += strlcpy(buffer + strsize, " ", bufsize);
    }
    buffer[strsize - 1] = '\0';
    toglPtr->extensions = buffer;
    return toglPtr->extensions;
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

void Togl_Update(const Togl *toglPtr)
{
  int x = toglPtr->x, y = toglPtr->y;
  int width = toglPtr->width, height = toglPtr->height;
  NSRect frameRect = NSMakeRect(x, y, width, height);
  // The coordinates of the frame of an NSView are in points, but the
  // coordinates of the bounds of an NSView managed by an
  // NSOpenGLContext are in pixels.  (There are 2.0 pixels per point on
  // a retina display.)  If we need to modify the bounds we should use
  // [NSView convertRectToBacking:(NSRect)rect];
  [toglPtr->nsview setFrame: frameRect];
  if (toglPtr->context && [toglPtr->context view] != toglPtr->nsview) {
    [toglPtr->context setView:toglPtr->nsview];
  }
}

/* Display reconfiguration callback. Documented as needed by Apple QA1209.
 * Updated for 10.3 (and later) to use 
 * CGDisplayRegisterReconfigurationCallback.
 */

static void
SetMacBufRect(Togl *togl)
{
    Rect r, rt;
    NSRect    rect;
    TkWindow *w = (TkWindow *) togl->tkwin;
    TkWindow *t = w->privatePtr->toplevel->winPtr;

    TkMacOSXWinBounds(w, &r);
    TkMacOSXWinBounds(t, &rt);

    rect.origin.x = r.left - rt.left;
    rect.origin.y = rt.bottom - r.bottom;
    rect.size.width = r.right - r.left;
    rect.size.height = r.bottom - r.top;

    [togl->nsview setFrame:rect];
    [togl->context update];
    
    /* TODO: Support full screen. */
}

static void
ReconfigureCB(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags,
        void *closure)
{
    Togl   *togl = (Togl *) closure;

    if (0 != (flags & kCGDisplayBeginConfigurationFlag))
        return;                 /* wait until display is reconfigured */

    SetMacBufRect(togl);
    Togl_MakeCurrent(togl);
    if (togl->context) {
        if (togl->reshapeProc) {
            Togl_CallCallback(togl, togl->reshapeProc);
        } else {
            glViewport(0, 0, togl->width, togl->height);
        }
    }
}

/*
 *  Togl_CreateGLContext
 *
 *  Creates an NSOpenGLContext and assigns it to toglPtr->context.
 *  The pixelFormat index is saved ins ToglPtr->pixelFormat.  Also
 *  creates and NSView to serve as the rendering surface.  The NSView
 *  is assigned as a subview and occupies the rectangle in the content
 *  view which is assigned as to the Togl widget.
 *
 *  Returns a standard Tcl result.
 */

int
Togl_CreateGLContext(Togl *toglPtr)
{
    if (toglPtr->context) {
	return TCL_OK;
    }
    //// FIX ME - the sharing does not make sense.
    // shareList and shareContext are supposed to be mutually exclusive.
    if (toglPtr->shareList) {
        /* We will share the display lists with an existing togl widget. */
        Togl *shareWith = FindTogl(toglPtr, toglPtr->shareList);
        if (shareWith) {
	    toglPtr->pixelFormat = shareWith->pixelFormat;
            toglPtr->context = shareWith->context;
            toglPtr->contextTag = shareWith->contextTag;
        } else {
	    Tcl_SetResult(toglPtr->interp,
		"Invalid widget specified in the sharelist option.",
		TCL_STATIC);
	  return TCL_ERROR;
	}
    } else if (toglPtr->shareContext) {
        /* We will share the OpenGL context of an existing Togl widget. */
        Togl *shareWith = FindTogl(toglPtr, toglPtr->shareContext);
	if (shareWith == NULL) {
	    Tcl_SetResult(toglPtr->interp,
		"Invalid widget specified in the sharecontext option.",
		TCL_STATIC);
	  return TCL_ERROR;
	}
	toglPtr->pixelFormat = shareWith->pixelFormat;
	toglPtr->context = [[NSOpenGLContext alloc]
	    initWithCGLContextObj: (CGLContextObj) shareWith->context];
    } else {
	toglPtr->context = [NSOpenGLContext alloc];
	toglPtr->pixelFormat = togl_pixelFormat(toglPtr);
	[toglPtr->context initWithFormat:toglPtr->pixelFormat
			    shareContext:nil];
	if (toglPtr->context== nil){
	    [toglPtr->pixelFormat release];
	    toglPtr->pixelFormat = nil;
	    Tcl_SetResult(toglPtr->interp,
		"Could not create OpenGL context", TCL_STATIC);
	    return TCL_ERROR;
	}
	// Make the new context current.  This ensures that there is
	// always a current context whenever a Togl exists, so GL
	// calls made before mapping the widget will not crash.
	[toglPtr->context makeCurrentContext];
    }
    return TCL_OK;
}
 
/* 
 * Togl_MakeWindow
 *
 *   Window creation function, invoked as a callback from Tk_MakeWindowExist.
 */

Window
Togl_MakeWindow(Tk_Window tkwin, Window parent, void* instanceData)
{
    Togl   *toglPtr = (Togl *) instanceData;
    Display *display;
    Colormap cmap;
    int     scrnum;
    Window  window = None;

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
 
    display = Tk_Display(tkwin);
    scrnum = Tk_ScreenNumber(tkwin);

    /* ????
     * Windows and Mac OS X need the window created before OpenGL context
     * is created.  So do that now and set the window variable. 
     */
    window = Tk_MakeWindow(tkwin, parent);

    /* if (!toglPtr->pBufferFlag) { */
    /*     printf("Togl_MakeWindow: calling XMapWindow\n"); */
    /*     (void) XMapWindow(display, window); */
    /* } */

    if (toglPtr->pixelFormat) {
        if (!togl_describePixelFormat(toglPtr)) {
            Tcl_SetResult(toglPtr->interp,
                    "couldn't choose pixel format", TCL_STATIC);
            goto error;
        }
    } else {
        toglPtr->pixelFormat = (void *)togl_pixelFormat(toglPtr);
        if (toglPtr->pixelFormat == nil) {
            goto error;
        }
    }
    
    if (toglPtr->visInfo == NULL) {
        Visual *visual= DefaultVisual(display, scrnum);
        toglPtr->visInfo = (XVisualInfo *) calloc(1, sizeof (XVisualInfo));
        toglPtr->visInfo->screen = scrnum;
        toglPtr->visInfo->visual = visual;
        toglPtr->visInfo->visualid = visual->visualid;
#  if defined(__cplusplus) || defined(c_plusplus)
        toglPtr->visInfo->c_class = visual->c_class;
#  else
        toglPtr->visInfo->class = visual->class;
#  endif
        toglPtr->visInfo->depth = visual->bits_per_rgb;
    }

    /* 
     * We should already have a context, but ...
     */
    if (Togl_CreateGLContext(toglPtr) != TCL_OK) {
         goto error;
    }
    if (!toglPtr->pBufferFlag) {
      toglPtr->nsview = [[NSView alloc] initWithFrame:NSZeroRect];
      [toglPtr->nsview setWantsBestResolutionOpenGLSurface:NO];
      MacDrawable *d = ((TkWindow *) toglPtr->tkwin)->privatePtr;
      NSView *topview = d->toplevel->view;	
      [topview addSubview:toglPtr->nsview];

      /* TODO: Appears setView has to be deferred until the window is mapped,
       * or it gives "invalid drawable" error.  But MapNotify doesn't happen.
       * I think toplevel is already mapped.  Iconifying and deiconifying
       * the main window makes the graphics work.
       */
      /*      [toglPtr->context setView:toglPtr->nsview];*/
    }
    if (toglPtr->context == NULL) {
        Tcl_SetResult(toglPtr->interp,
            "could not create rendering context", TCL_STATIC);
        goto error;
    }
    CGDisplayRegisterReconfigurationCallback(ReconfigureCB, toglPtr);
    if (toglPtr->pBufferFlag) {
        toglPtr->pbuf = togl_createPbuffer(toglPtr);
        if (!toglPtr->pbuf) {
            /* tcl result set in togl_createPbuffer */
            if (!toglPtr->shareContext) {
	        [toglPtr->context release];
		[toglPtr->pixelFormat release];
            }
            toglPtr->context = NULL;
            toglPtr->pixelFormat = nil;
            goto error;
        }
        return window;
    }

    /* 
     * find a colormap
     */
    if (toglPtr->rgbaFlag) {
        /* Colormap for RGB mode */
        cmap = DefaultColormap(display, scrnum);
    } else {
        /* Colormap for CI mode */
        if (toglPtr->privateCmapFlag) {
            /* need read/write colormap so user can store own color entries */
            /* need to figure out how to do this correctly on Mac... */
            cmap = DefaultColormap(display, scrnum);
        } else {
            if (toglPtr->visInfo->visual == DefaultVisual(display, scrnum)) {
                /* share default/root colormap */
                cmap = Tk_Colormap(tkwin);
            } else {
                /* make a new read-only colormap */
                cmap = XCreateColormap(display,
                        XRootWindow(display, toglPtr->visInfo->screen),
                        toglPtr->visInfo->visual, AllocNone);
            }
        }
    }

    /* Make sure Tk knows to switch to the new colormap when the cursor is over
     * this window when running in color index mode. */
    (void) Tk_SetWindowVisual(tkwin, toglPtr->visInfo->visual,
            toglPtr->visInfo->depth, cmap);

#if TOGL_USE_OVERLAY
    if (toglPtr->OverlayFlag) {
        if (SetupOverlay(toglPtr) == TCL_ERROR) {
            fprintf(stderr, "Warning: couldn't setup overlay.\n");
            toglPtr->OverlayFlag = False;
        }
    }
#endif

    /* Request the X window to be displayed */
    (void) XMapWindow(display, window);
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
    if (toglPtr->Stereo == TOGL_STEREO_NATIVE) {
        if (!toglPtr->as_initialized) {
            const char *autostereod;

            toglPtr->as_initialized = True;
            if ((autostereod = getenv("AUTOSTEREOD")) == NULL)
                autostereod = AUTOSTEREOD;
            if (autostereod && *autostereod) {
                if (ASInitialize(toglPtr->display, autostereod) == Success) {
                    toglPtr->ash = ASCreatedStereoWindow(display);
                }
            }
        } else {
            toglPtr->ash = ASCreatedStereoWindow(display);
        }
    }
#endif
    return window;

  error:
    toglPtr->badWindow = True;
    return window;
}

/* 
 * Togl_WorldChanged
 *
 *    Add support for setgrid option.
 */
void
Togl_WorldChanged(ClientData instanceData)
{
    Togl   *togl = (Togl *) instanceData;
    int     width;
    int     height;

    if (togl->pBufferFlag)
        width = height = 1;
    else {
        width = togl->width;
        height = togl->height;
    }
    Tk_GeometryRequest(togl->tkwin, width, height);
    Tk_SetInternalBorder(togl->tkwin, 0);
    if (togl->setGrid > 0) {
        Tk_SetGrid(togl->tkwin,
		   width / togl->setGrid,
		   height / togl->setGrid,
		   togl->setGrid, togl->setGrid);
    } else {
        Tk_UnsetGrid(togl->tkwin);
    }
}

/* 
 * Togl_TakePhoto
 *
 *   Take a photo image of the current OpenGL window.  May have problems
 *   if window is partially obscured, either by other windows or by the
 *   edges of the display.
 */

int
Togl_TakePhoto(Togl *toglPtr, Tk_PhotoHandle photo)
{
    GLubyte *buffer;
    unsigned char *cp;
    int y, midy, width = toglPtr->width, height = toglPtr->height;
    buffer = (GLubyte *) ckalloc(width * height * 4);
    Tk_PhotoImageBlock photoBlock;
    photoBlock.pixelPtr = buffer;
    photoBlock.width = width;
    photoBlock.height = height;
    photoBlock.pitch = width * 4;
    photoBlock.pixelSize = 4;
    photoBlock.offset[0] = 0;
    photoBlock.offset[1] = 1;
    photoBlock.offset[2] = 2;
    photoBlock.offset[3] = 3;
    glPushAttrib(GL_PIXEL_MODE_BIT);
    if (toglPtr->doubleFlag) {
        glReadBuffer(GL_FRONT);
    }
    if (!toglPtr->rgbaFlag) {
        glPixelMapfv(GL_PIXEL_MAP_I_TO_R, toglPtr->mapSize, toglPtr->redMap);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_G, toglPtr->mapSize, toglPtr->greenMap);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_B, toglPtr->mapSize, toglPtr->blueMap);
    }
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);        /* guarantee performance */
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    /* OpenGL's origin is bottom-left, Tk Photo image's is top-left, so
     * reflect around the middle row. */
    midy = height / 2;
    cp = buffer;
    for (y = 0; y < midy; ++y) {
        int     m_y = height - 1 - y;   /* mirror y */
        unsigned char *m_cp = buffer + m_y * photoBlock.pitch;
        int     x;

        for (x = 0; x < photoBlock.pitch; ++x) {
            unsigned char c = *cp;

            *cp++ = *m_cp;
            *m_cp++ = c;
        }
    }
    Tk_PhotoPutBlock(toglPtr->interp, photo, &photoBlock, 0, 0,
	width, height, TK_PHOTO_COMPOSITE_SET);
    glPopClientAttrib();
    glPopAttrib();    /* glReadBuffer */
    ckfree((char *) buffer);
    return TCL_OK;
}

/* 
 * Togl_MakeCurrent
 *
 *   Bind the OpenGL rendering context to the specified
 *   Togl widget.  ????If given a NULL argument, then the
 *   OpenGL context is released without assigning a new one.????
 */

void
Togl_MakeCurrent(const Togl *toglPtr)
{
    if (toglPtr != NULL && toglPtr->context != NULL) {
        [toglPtr->context makeCurrentContext];
	// If our context is in use by another view or pixel buffer,
	// reassign it to our view or pixel buffer.
        if (FindToglWithSameContext(toglPtr) != NULL) {
            if (!toglPtr->pBufferFlag) {
	        [toglPtr->context setView:toglPtr->nsview];
            } else {
	        GLint virtualScreen = [toglPtr->context currentVirtualScreen];
                [toglPtr->context setPixelBuffer:toglPtr->pbuf
				  cubeMapFace:0
				  mipMapLevel:0
			 currentVirtualScreen:virtualScreen];
            }
        }
    }
}

void
Togl_SwapBuffers(const Togl *toglPtr)
{
    if (toglPtr->doubleFlag) {
        [toglPtr->context flushBuffer];
    } else {
        glFlush();
    }
}

int
Togl_CopyContext(const Togl *from, const Togl *to, unsigned mask)
{
    int same = (from->context == to->context);

    if (same) {
      [NSOpenGLContext clearCurrentContext];
    }
    [to->context copyAttributesFromContext:from->context withMask:mask];
    if (same)
        Togl_MakeCurrent(to);
    return TCL_OK;
}

void Togl_FreeResources(
    Togl *toglPtr)
{
    [NSOpenGLContext clearCurrentContext];
    if (toglPtr->extensions) {
	ckfree((void *)toglPtr->extensions);
	toglPtr->extensions = NULL;
    }
    if (toglPtr->context) {
	if (FindToglWithSameContext(toglPtr) == NULL) {
	    [toglPtr->context release];
	    toglPtr->context = nil;
	}
	[toglPtr->nsview removeFromSuperview];
	[toglPtr->nsview release];
	toglPtr->nsview = nil;
	CGDisplayRemoveReconfigurationCallback(ReconfigureCB, toglPtr);
	free(toglPtr->visInfo);
    }
    if (toglPtr->pBufferFlag && toglPtr->pbuf) {
	togl_destroyPbuffer(toglPtr);
	toglPtr->pbuf = 0;
    }
    toglPtr->context = NULL;
    toglPtr->visInfo = NULL;
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
