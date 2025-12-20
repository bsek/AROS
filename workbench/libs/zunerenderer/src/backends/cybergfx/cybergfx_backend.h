#ifndef CYBERGFX_BACKEND_H
#define CYBERGFX_BACKEND_H

/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - CyberGraphics Backend Header

    This header defines only the essential structures and exports needed
    for the CyberGraphics rendering backend. All operations are handled
    through the unified ZuneBackendOps interface.
*/

#include "../backend_interface.h"
#include <exec/types.h>
#include <graphics/gfx.h>
#include <libraries/cybergraphics.h>

/*****************************************************************************/
/* CyberGraphics Backend Constants */
/*****************************************************************************/

#if AROS_BIG_ENDIAN
#define CYBERGFX_PIXELFORMAT_ARGB32 RECTFMT_ARGB
#else
/* Packed 32-bit pixels sit in native byte order; use BGRA on little endian to
   match Write/ReadPixelArray expectations. */
#define CYBERGFX_PIXELFORMAT_ARGB32 RECTFMT_BGRA32
#endif

#define CYBERGFX_MIN_VERSION 41 /* Minimum CyberGraphics version */
#define CYBERGFX_MAX_LOCKED_SURFACES                                           \
  8 /* Maximum simultaneously locked surfaces */

/*****************************************************************************/
/* CyberGraphics Backend Export */
/*****************************************************************************/
APTR CybergfxLockPixels(struct DrawingBoard *board, ULONG *pitch_out);
void CybergfxUnlockPixels(struct DrawingBoard *board);
ULONG CybergfxGetPixel(struct DrawingBoard *board, WORD x, WORD y);
void CybergfxSetPixel(struct DrawingBoard *board, WORD x, WORD y,
                      struct InternalColor *color);

/* Texture operations - defined in cybergfx_texture.c */
BOOL CybergfxInitTexture(struct ZuneTexture *texture);
void CybergfxCleanupTexture(struct ZuneTexture *texture);
BOOL CybergfxUpdateTexture(struct ZuneTexture *texture, APTR data, UWORD x,
                           UWORD y, UWORD width, UWORD height);
void CybergfxDrawTexture(struct RenderPort *rp, struct ZuneTexture *texture,
                         WORD dest_x, WORD dest_y, UWORD dest_width,
                         UWORD dest_height, WORD src_x, WORD src_y,
                         UWORD src_width, UWORD src_height,
                         struct InternalColor *tint);
APTR CybergfxLockTexturePixels(struct ZuneTexture *texture, ULONG *pitch);
void CybergfxUnlockTexturePixels(struct ZuneTexture *texture);
ULONG CybergfxGetTexturePixel(struct ZuneTexture *texture, WORD x, WORD y);
void CybergfxSetTexturePixel(struct ZuneTexture *texture, WORD x, WORD y,
                             struct InternalColor *color);
ULONG CybergfxGetMaxTextureSize(void);
BOOL CybergfxSupportsTextureFormat(ULONG format);

/* Rectangle operations - defined in cybergfx_rectangle.c */
void CybergfxDrawRectangle(struct RenderPort *rp, WORD x, WORD y, UWORD width,
                           UWORD height, UBYTE border_width,
                           UBYTE corner_radius,
                           struct ZuneBrush *fill_brush,
                           struct InternalColor *border_color, BOOL filled,
                           BOOL antialias);

/* Line operations - defined in cybergfx_line.c */
void CybergfxDrawLine(struct RenderPort *rp, WORD startX, WORD startY,
                      WORD endX, WORD endY, UWORD width,
                      struct InternalColor *color, BOOL antialias);

/* Circle operations - defined in cybergfx_circle.c */
void CybergfxDrawCircle(struct RenderPort *rp, WORD center_x, WORD center_y,
                        UWORD radius, UBYTE border_width,
                        struct ZuneBrush *fill_brush,
                        struct InternalColor *border_color, BOOL filled,
                        BOOL antialias);

/* Pixel operations */
void CybergfxDrawPixel(struct RenderPort *rp, WORD x, WORD y,
                       struct InternalColor *color, BOOL antialias);

/* Clipping operations - defined in cybergfx_clipping.c */
BOOL CybergfxSetupClipping(struct RenderPort *rp, struct Region *region);
BOOL CybergfxClipPixel(struct RenderPort *rp, WORD x, WORD y);
void CybergfxClearClipping(struct RenderPort *rp);
BOOL CybergfxClipRectangle(struct RenderPort *rp, WORD x, WORD y, WORD width,
                           WORD height, WORD *out_x, WORD *out_y,
                           WORD *out_width, WORD *out_height);
BOOL CybergfxGetClipBounds(struct RenderPort *rp, WORD *min_x, WORD *min_y,
                           WORD *max_x, WORD *max_y);
BOOL CybergfxClipLine(struct RenderPort *rp, WORD *x1, WORD *y1, WORD *x2,
                      WORD *y2);
void CybergfxClipFillPixelArray(struct RenderPort *rp,
                                struct RastPort *rastport, WORD x, WORD y,
                                WORD width, WORD height, ULONG color);
void CybergfxClipFillPixelArrayDirect(struct RenderPort *rp, ULONG *pixels,
                                      UWORD pitch_pixels, UWORD buffer_width,
                                      UWORD buffer_height, WORD x, WORD y,
                                      WORD width, WORD height, ULONG pixel);
void CybergfxWritePixelClamped(ULONG *pixels, UWORD pitch_pixels,
                               UWORD buffer_width, UWORD buffer_height, WORD x,
                               WORD y, ULONG pixel);

/* Backend operations table - defined in cybergfx_backend.c */
extern ZuneBackendOps cybergfx_backend_ops;

#endif /* CYBERGFX_BACKEND_H */
