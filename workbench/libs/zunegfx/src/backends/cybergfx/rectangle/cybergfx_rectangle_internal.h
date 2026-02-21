#ifndef CYBERGFX_RECTANGLE_INTERNAL_H
#define CYBERGFX_RECTANGLE_INTERNAL_H

#ifndef DEBUG
#define DEBUG 0
#endif

#include <aros/debug.h>
#include <cybergraphx/cybergraphics.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <math.h>
#include <proto/cybergraphics.h>
#include <proto/graphics.h>

#include "../../../zunegfx_intern.h"
#include "../../backend_interface.h"
#include "../cybergfx_antialiasing.h"
#include "../cybergfx_backend.h"
#include "../cybergfx_pixel_format.h"
#include "../cybergfx_simd.h"

void CybergfxDrawRoundedRectangleToLockedDrawingBoard(struct RenderContext *renderport, UWORD x, UWORD y, UWORD width, UWORD height, UBYTE border_width,
                                                      struct ZuneBrush *fill_brush, ULONG border_color, UWORD border_radius, BOOL filled);

void CybergfxDrawRoundedRectangleToRasterPort(struct RenderContext *renderport, struct RastPort *rp, UWORD x, UWORD y, UWORD width, UWORD height,
                                              UBYTE border_width, struct ZuneBrush *fill_brush, ULONG border_color, UWORD border_radius, BOOL filled);

void CybergfxDrawRectangleToRasterPort(struct RenderContext *renderport, struct RastPort *rp, UWORD x, UWORD y, UWORD width, UWORD height,
                                       UBYTE border_width, struct ZuneBrush *fill_brush, ULONG border_color, BOOL filled);

void CybergfxDrawRectangleToLockedDrawingBoard(struct RenderContext *renderport, WORD x, WORD y, UWORD width, UWORD height, UBYTE border_width,
                                               struct ZuneBrush *fill_brush, ULONG border_color, BOOL filled);

void CybergfxAARectangleDrawingBoard(struct DrawingBoard *board, UWORD x, UWORD y, UWORD width, UWORD height, UBYTE radius, float line_width,
                                     struct ZuneBrush *fill_brush, struct InternalColor *outline, struct InternalColor *outline_tint, BOOL filled,
                                     BOOL draw_border);

void CybergfxAARectangleRasterPort(struct RastPort *rp, UWORD x, UWORD y, UWORD width, UWORD height, UBYTE radius, float lineWidth,
                                   struct ZuneBrush *fill_brush, struct InternalColor *outline, struct InternalColor *outline_tint, BOOL filled,
                                   BOOL draw_border);

#endif /* CYBERGFX_RECTANGLE_INTERNAL_H */
