/*
  This file contains implementations of the following platform specific
  functions declared in togl.h.  They comprise the X11 interface.

void Togl_Update(const Togl *toglPtr);
Window Togl_MakeWindow(Tk_Window tkwin, Window parent, void* instanceData);
void Togl_WorldChanged(void* instanceData);
void Togl_MakeCurrent(const Togl *toglPtr);
void Togl_SwapBuffers(const Togl *toglPtr);
int Togl_TakePhoto(Togl *toglPtr, Tk_PhotoHandle photo);
int Togl_CopyContext(const Togl *from, const Togl *to, unsigned mask);
int Togl_CreateGLContext(Togl *toglPtr);
*/

#include "togl.h"

void
Togl_Update(
    const Togl *toglPtr) {
}

Window
Togl_MakeWindow(
    Tk_Window tkwin,
    Window parent,
    void* instanceData){
    printf("MakeWindow\n");
    return None;
}

void
Togl_WorldChanged(
    void* instanceData){
    printf("WorldChanged\n");
}

void
Togl_MakeCurrent(
    const Togl *toglPtr){
    printf("MakeCurrent\n");
}

void
Togl_SwapBuffers(
    const Togl *toglPtr){
    printf("SwapBuffers\n");
}

int
Togl_TakePhoto(
    Togl *toglPtr,
    Tk_PhotoHandle photo){
    printf("TakePhoto\n");
    return TCL_OK;
}

int
Togl_CopyContext(
    const Togl *from,
    const Togl *to,
    unsigned mask){
    printf("CopyContext\n");
    return TCL_OK;
}

int
Togl_CreateGLContext(
    Togl *toglPtr){
    printf("CreateGLContext\n");
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
