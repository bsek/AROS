#ifndef CYBERGFX_BACKEND_H
#define CYBERGFX_BACKEND_H

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - CyberGraphics Backend Header

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
void CybergfxDrawTexture(struct RenderContext *rctx, struct ZuneTexture *texture,
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
BOOL CybergfxDrawTextureTiledFast(struct RenderContext *rctx,
                                  struct ZuneTexture *texture,
                                  WORD dest_x, WORD dest_y,
                                  UWORD dest_width, UWORD dest_height);

/* Rectangle operations - defined in cybergfx_rectangle.c */
void CybergfxDrawRectangle(struct RenderContext *rctx, WORD x, WORD y, UWORD width,
                           UWORD height, UBYTE border_width,
                           UBYTE corner_radius,
                           struct ZuneBrush *fill_brush,
                           struct InternalColor *border_color, BOOL filled,
                           BOOL antialias);

/* Line operations - defined in cybergfx_line.c */
void CybergfxDrawLine(struct RenderContext *rctx, WORD startX, WORD startY,
                      WORD endX, WORD endY, UWORD width,
                      struct InternalColor *color, BOOL antialias);

/* Circle operations - defined in cybergfx_circle.c */
void CybergfxDrawCircle(struct RenderContext *rctx, WORD center_x, WORD center_y,
                        UWORD radius, UBYTE border_width,
                        struct ZuneBrush *fill_brush,
                        struct InternalColor *border_color, BOOL filled,
                        BOOL antialias);

/* Pixel operations */
void CybergfxDrawPixel(struct RenderContext *rctx, WORD x, WORD y,
                       struct InternalColor *color, BOOL antialias);

/* Clipping operations - defined in cybergfx_clipping.c */
BOOL CybergfxSetupClipping(struct RenderContext *rctx, struct Region *region);
BOOL CybergfxClipPixel(struct RenderContext *rctx, WORD x, WORD y);
void CybergfxClearClipping(struct RenderContext *rctx);
BOOL CybergfxClipRectangle(struct RenderContext *rctx, WORD x, WORD y, WORD width,
                           WORD height, WORD *out_x, WORD *out_y,
                           WORD *out_width, WORD *out_height);
BOOL CybergfxGetClipBounds(struct RenderContext *rctx, WORD *min_x, WORD *min_y,
                           WORD *max_x, WORD *max_y);
BOOL CybergfxClipLine(struct RenderContext *rctx, WORD *x1, WORD *y1, WORD *x2,
                      WORD *y2);
void CybergfxClipFillPixelArray(struct RenderContext *rctx,
                                struct RastPort *rastport, WORD x, WORD y,
                                WORD width, WORD height, ULONG color);
void CybergfxClipFillPixelArrayDirect(struct RenderContext *rctx, ULONG *pixels,
                                      UWORD pitch_pixels, UWORD buffer_width,
                                      UWORD buffer_height, WORD x, WORD y,
                                      WORD width, WORD height, ULONG pixel);
void CybergfxWritePixelClamped(ULONG *pixels, UWORD pitch_pixels,
                               UWORD buffer_width, UWORD buffer_height, WORD x,
                               WORD y, ULONG pixel);

/* Backend operations table - defined in cybergfx_backend.c */
extern ZuneBackendOps cybergfx_backend_ops;

#endif /* CYBERGFX_BACKEND_H */
