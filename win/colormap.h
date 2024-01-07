/*
 * Maximum size of a logical palette corresponding to a colormap in
 * color index mode.
 */

#  define MAX_CI_COLORMAP_SIZE 4096
#  define MAX_CI_COLORMAP_BITS 12



/*
 * Declaration of functions defined in colormap.c
 */

Colormap Win32CreateRgbColormap(PIXELFORMATDESCRIPTOR pfd);
Colormap Win32CreateCiColormap(Togl *toglPtr);

