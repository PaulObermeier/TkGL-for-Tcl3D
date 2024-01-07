
/* Code to create RGB palette is taken from the GENGL sample program of Win32
 * SDK */

#include "togl.h"
#include "toglPlatform.h"
#include "tkInt.h"  /* for TkWindow */
#include "tkWinInt.h" /* for TkWinDCState */
#include "tkIntPlatDecls.h" /* for TkWinChildProc */

static const unsigned char threeto8[8] = {
    0, 0111 >> 1, 0222 >> 1, 0333 >> 1, 0444 >> 1, 0555 >> 1, 0666 >> 1, 0377
};

static const unsigned char twoto8[4] = {
    0, 0x55, 0xaa, 0xff
};

static const unsigned char oneto8[2] = {
    0, 255
};

static const int defaultOverride[13] = {
    0, 3, 24, 27, 64, 67, 88, 173, 181, 236, 247, 164, 91
};


static const PALETTEENTRY defaultPalEntry[20] = {
    {0, 0, 0, 0},
    {0x80, 0, 0, 0},
    {0, 0x80, 0, 0},
    {0x80, 0x80, 0, 0},
    {0, 0, 0x80, 0},
    {0x80, 0, 0x80, 0},
    {0, 0x80, 0x80, 0},
    {0xC0, 0xC0, 0xC0, 0},

    {192, 220, 192, 0},
    {166, 202, 240, 0},
    {255, 251, 240, 0},
    {160, 160, 164, 0},

    {0x80, 0x80, 0x80, 0},
    {0xFF, 0, 0, 0},
    {0, 0xFF, 0, 0},
    {0xFF, 0xFF, 0, 0},
    {0, 0, 0xFF, 0},
    {0xFF, 0, 0xFF, 0},
    {0, 0xFF, 0xFF, 0},
    {0xFF, 0xFF, 0xFF, 0}
};

static unsigned char
ComponentFromIndex(int i, UINT nbits, UINT shift)
{
    unsigned char val;

    val = (unsigned char) (i >> shift);
    switch (nbits) {

      case 1:
          val &= 0x1;
          return oneto8[val];

      case 2:
          val &= 0x3;
          return twoto8[val];

      case 3:
          val &= 0x7;
          return threeto8[val];

      default:
          return 0;
    }
}

Colormap
Win32CreateRgbColormap(PIXELFORMATDESCRIPTOR pfd)
{
    TkWinColormap *cmap = (TkWinColormap *) ckalloc(sizeof (TkWinColormap));
    LOGPALETTE *pPal;
    int     n, i;

    n = 1 << pfd.cColorBits;
    pPal = (PLOGPALETTE) LocalAlloc(LMEM_FIXED, sizeof (LOGPALETTE)
            + n * sizeof (PALETTEENTRY));
    pPal->palVersion = 0x300;
    pPal->palNumEntries = n;
    for (i = 0; i < n; i++) {
        pPal->palPalEntry[i].peRed =
                ComponentFromIndex(i, pfd.cRedBits, pfd.cRedShift);
        pPal->palPalEntry[i].peGreen =
                ComponentFromIndex(i, pfd.cGreenBits, pfd.cGreenShift);
        pPal->palPalEntry[i].peBlue =
                ComponentFromIndex(i, pfd.cBlueBits, pfd.cBlueShift);
        pPal->palPalEntry[i].peFlags = 0;
    }

    /* fix up the palette to include the default GDI palette */
    if ((pfd.cColorBits == 8)
            && (pfd.cRedBits == 3) && (pfd.cRedShift == 0)
            && (pfd.cGreenBits == 3) && (pfd.cGreenShift == 3)
            && (pfd.cBlueBits == 2) && (pfd.cBlueShift == 6)) {
        for (i = 1; i <= 12; i++)
            pPal->palPalEntry[defaultOverride[i]] = defaultPalEntry[i];
    }

    cmap->palette = CreatePalette(pPal);
    LocalFree(pPal);
    cmap->size = n;
    cmap->stale = 0;

    /* Since this is a private colormap of a fix size, we do not need a valid
     * hash table, but a dummy one */

    Tcl_InitHashTable(&cmap->refCounts, TCL_ONE_WORD_KEYS);
    return (Colormap) cmap;
}

/*
 * Code to create RGB palettes is taken from the GENGL sample program
 * of Win32 SDK
 */

Colormap
Win32CreateCiColormap(Togl *toglPtr)
{
    /* Create a colormap with size of togl->ciColormapSize and set all entries
     * to black */

    LOGPALETTE logPalette;
    TkWinColormap *cmap = (TkWinColormap *) ckalloc(sizeof (TkWinColormap));

    logPalette.palVersion = 0x300;
    logPalette.palNumEntries = 1;
    logPalette.palPalEntry[0].peRed = 0;
    logPalette.palPalEntry[0].peGreen = 0;
    logPalette.palPalEntry[0].peBlue = 0;
    logPalette.palPalEntry[0].peFlags = 0;

    cmap->palette = CreatePalette(&logPalette);
    cmap->size = toglPtr->ciColormapSize;
    ResizePalette(cmap->palette, cmap->size);   /* sets new entries to black */
    cmap->stale = 0;

    /* Since this is a private colormap of a fix size, we do not need a valid
     * hash table, but a dummy one */

    Tcl_InitHashTable(&cmap->refCounts, TCL_ONE_WORD_KEYS);
    return (Colormap) cmap;
}
