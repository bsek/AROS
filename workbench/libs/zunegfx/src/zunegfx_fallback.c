/*
    Zune Renderer Library - Software Fallbacks

    Provides basic graphics.library based implementations that are used when
    no backend (or no specific backend function) is available. These routines
    intentionally favour correctness over performance; they keep the library
    functional on systems without CyberGraphics or other accelerators.
*/

#define DEBUG 0
#include <aros/debug.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <math.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "backends/backend_interface.h"
#include "clib/graphics_protos.h"
#include "zunegfx_intern.h"

static struct RastPort *fallback_get_rastport(struct RenderPort *rp) {
  if (!rp)
    return NULL;
  if (rp->target_board && rp->target_board->rastport)
    return rp->target_board->rastport;
  return rp->target_rp;
}

static void fallback_save_rastport_state(struct RastPort *rast, UWORD *apen,
                                         UWORD *bpen, UBYTE *drawmode) {
  if (!rast)
    return;
  if (apen)
    *apen = rast->FgPen;
  if (bpen)
    *bpen = rast->BgPen;
  if (drawmode)
    *drawmode = rast->DrawMode;
}

static void fallback_restore_rastport_state(struct RastPort *rast, UWORD apen,
                                            UWORD bpen, UBYTE drawmode) {
  if (!rast)
    return;
  SetAPen(rast, apen);
  SetBPen(rast, bpen);
  SetDrMd(rast, drawmode);
}

static struct PenCache *fallback_ensure_pen_cache(struct RenderPort *rp,
                                                  struct ColorMap *cmap) {
  static struct PenCache global_cache;
  static struct ColorMap *global_cache_cmap;

  if (!cmap)
    return NULL;

  if (rp) {
    if (!rp->pen_cache) {
      rp->pen_cache =
          AllocVec(sizeof(struct PenCache), MEMF_CLEAR | MEMF_PUBLIC);
      if (!rp->pen_cache)
        return NULL;
      InitPenCache(rp->pen_cache, cmap);
    }
    return rp->pen_cache;
  }

  /* Fallback: shared cache when no RenderPort is available */
  if (global_cache_cmap != cmap) {
    InitPenCache(&global_cache, cmap);
    global_cache_cmap = cmap;
  }
  return &global_cache;
}

static BOOL fallback_set_pen(struct RenderPort *rp, struct ColorMap *cmap,
                             struct InternalColor *color,
                             struct RastPort *rast) {
  struct PenCache *cache = fallback_ensure_pen_cache(rp, cmap);
  if (!cache || !rast)
    return FALSE;

  LONG pen = GetCachedPen(cache, color->original_pixel & 0x00FFFFFF);
  if (pen == -1)
    return FALSE;

  color->pen = pen;
  color->pen_allocated = FALSE;
  SetAPen(rast, pen);
  return TRUE;
}

void ZuneFallback_DrawPixel(struct RenderPort *rp, WORD x, WORD y,
                            struct InternalColor *color, BOOL antialias) {
  (void)antialias;

  struct RastPort *rast = fallback_get_rastport(rp);
  if (!rast || !color)
    return;

  UWORD saved_apen = 0, saved_bpen = 0;
  UBYTE saved_dm = 0;
  fallback_save_rastport_state(rast, &saved_apen, &saved_bpen, &saved_dm);

  if (fallback_set_pen(rp, rp ? rp->colormap : NULL, color, rast)) {
    WritePixel(rast, x, y);
  }

  fallback_restore_rastport_state(rast, saved_apen, saved_bpen, saved_dm);
}

void ZuneFallback_DrawLine(struct RenderPort *rp, WORD start_x, WORD start_y,
                           WORD end_x, WORD end_y, UWORD line_width,
                           struct InternalColor *color, BOOL antialias) {
  (void)antialias;
  (void)line_width;

  struct RastPort *rast = fallback_get_rastport(rp);
  if (!rast || !color)
    return;

  UWORD saved_apen = 0, saved_bpen = 0;
  UBYTE saved_dm = 0;
  fallback_save_rastport_state(rast, &saved_apen, &saved_bpen, &saved_dm);

  if (!fallback_set_pen(rp, rp ? rp->colormap : NULL, color, rast))
    goto restore;

  /* Bresenham fallback */
  int dx = ABS(end_x - start_x);
  int sx = start_x < end_x ? 1 : -1;
  int dy = -ABS(end_y - start_y);
  int sy = start_y < end_y ? 1 : -1;
  int err = dx + dy;

  int cx = start_x;
  int cy = start_y;
  while (1) {
    WritePixel(rast, cx, cy);
    if (cx == end_x && cy == end_y)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      cx += sx;
    }
    if (e2 <= dx) {
      err += dx;
      cy += sy;
    }
  }

restore:
  fallback_restore_rastport_state(rast, saved_apen, saved_bpen, saved_dm);
}

static void fallback_rect_fill(struct RenderPort *rp, struct RastPort *rast,
                               WORD x, WORD y, UWORD width, UWORD height,
                               struct InternalColor *color) {
  WORD x2 = x + (WORD)width - 1;
  WORD y2 = y + (WORD)height - 1;

  if (!fallback_set_pen(rp, rp ? rp->colormap : NULL, color, rast))
    return;

  RectFill(rast, x, y, x2, y2);
}

static void fallback_rect_outline(struct RenderPort *rp, struct RastPort *rast,
                                  WORD x, WORD y, UWORD width, UWORD height,
                                  struct InternalColor *color) {
  if (!rast || !color)
    return;

  WORD x2 = x + (WORD)width - 1;
  WORD y2 = y + (WORD)height - 1;

  if (!fallback_set_pen(rp, rp ? rp->colormap : NULL, color, rast))
    return;

  Move(rast, x, y);
  Draw(rast, x2, y);
  Draw(rast, x2, y2);
  Draw(rast, x, y2);
  Draw(rast, x, y);
}

void ZuneFallback_DrawRectangle(struct RenderPort *rp, WORD x, WORD y,
                                UWORD width, UWORD height, UBYTE border_width,
                                UBYTE corner_radius,
                                struct ZuneBrush *fill_brush,
                                struct InternalColor *border_color, BOOL filled,
                                BOOL antialias) {
  (void)corner_radius;
  (void)antialias;

  struct RastPort *rast = fallback_get_rastport(rp);
  if (!rast)
    return;

  UWORD saved_apen = 0, saved_bpen = 0;
  UBYTE saved_dm = 0;
  fallback_save_rastport_state(rast, &saved_apen, &saved_bpen, &saved_dm);

  if (filled && fill_brush) {
    struct InternalColor fill_color;
    if (ZuneBrushToInternalColor(rp, fill_brush, &fill_color)) {
      fallback_rect_fill(rp, rast, x, y, width, height, &fill_color);
    }
  }

  if (border_width > 0 && border_color) {
    fallback_rect_outline(rp, rast, x, y, width, height, border_color);
  }

  fallback_restore_rastport_state(rast, saved_apen, saved_bpen, saved_dm);
}

void ZuneFallback_DrawCircle(struct RenderPort *rp, WORD center_x,
                             WORD center_y, UWORD radius, UBYTE border_width,
                             struct ZuneBrush *fill_brush,
                             struct InternalColor *border_color, BOOL filled,
                             BOOL antialias) {
  (void)border_width;
  (void)antialias;

  struct RastPort *rast = fallback_get_rastport(rp);
  if (!rast || radius == 0)
    return;

  UWORD saved_apen = 0, saved_bpen = 0;
  UBYTE saved_dm = 0;
  fallback_save_rastport_state(rast, &saved_apen, &saved_bpen, &saved_dm);

  struct InternalColor fill_color;
  BOOL have_fill = filled && fill_brush &&
                   ZuneBrushToInternalColor(rp, fill_brush, &fill_color) &&
                   fallback_set_pen(rp, rp ? rp->colormap : NULL, &fill_color,
                                    rast);
  BOOL have_border =
      border_color && fallback_set_pen(rp, rp ? rp->colormap : NULL,
                                       border_color, rast);

  int x = radius;
  int y = 0;
  int decision_over2 = 1 - x;

  while (y <= x) {
    if (have_fill) {
      WORD left_x = center_x - x;
      WORD right_x = center_x + x;
      for (WORD px = left_x; px <= right_x; ++px) {
        WritePixel(rast, px, center_y + y);
        WritePixel(rast, px, center_y - y);
      }

      left_x = center_x - y;
      right_x = center_x + y;
      for (WORD px = left_x; px <= right_x; ++px) {
        WritePixel(rast, px, center_y + x);
        WritePixel(rast, px, center_y - x);
      }
    }

    if (have_border) {
      WritePixel(rast, center_x + x, center_y + y);
      WritePixel(rast, center_x + y, center_y + x);
      WritePixel(rast, center_x - x, center_y + y);
      WritePixel(rast, center_x - y, center_y + x);
      WritePixel(rast, center_x - x, center_y - y);
      WritePixel(rast, center_x - y, center_y - x);
      WritePixel(rast, center_x + x, center_y - y);
      WritePixel(rast, center_x + y, center_y - x);
    }

    y++;
    if (decision_over2 <= 0) {
      decision_over2 += 2 * y + 1;
    } else {
      x--;
      decision_over2 += 2 * (y - x) + 1;
    }
  }

  fallback_restore_rastport_state(rast, saved_apen, saved_bpen, saved_dm);
}

void ZuneFallback_ClearRenderPort(struct RenderPort *rp,
                                  struct InternalColor *color) {
  struct RastPort *rast = fallback_get_rastport(rp);
  if (!rp || !rast || !color)
    return;

  UWORD saved_apen = 0, saved_bpen = 0;
  UBYTE saved_dm = 0;
  fallback_save_rastport_state(rast, &saved_apen, &saved_bpen, &saved_dm);

  UWORD width = 0;
  UWORD height = 0;

  if (rp->target_board) {
    width = rp->target_board->width;
    height = rp->target_board->height;
  } else if (rp->target_rp && rp->target_rp->BitMap) {
    width = (UWORD)GetBitMapAttr(rp->target_rp->BitMap, BMA_WIDTH);
    height = (UWORD)GetBitMapAttr(rp->target_rp->BitMap, BMA_HEIGHT);
  }

  if (width == 0 || height == 0)
    return;

  fallback_rect_fill(rp, rast, 0, 0, width, height, color);

  fallback_restore_rastport_state(rast, saved_apen, saved_bpen, saved_dm);
}

APTR ZuneFallback_LockPixels(struct DrawingBoard *board, ULONG *pitch_out) {
  (void)board;
  if (pitch_out)
    *pitch_out = 0;
  /* graphics.library cannot expose a pixel pointer here */
  return NULL;
}

void ZuneFallback_UnlockPixels(struct DrawingBoard *board) { (void)board; }

ULONG ZuneFallback_GetPixel(struct DrawingBoard *board, WORD x, WORD y) {
  if (!board || !board->rastport)
    return 0;
  return ReadPixel(board->rastport, x, y);
}

void ZuneFallback_SetPixel(struct DrawingBoard *board, WORD x, WORD y,
                           struct InternalColor *color) {
  if (!board || !board->rastport || !color)
    return;

  struct RastPort *rast = board->rastport;
  UWORD saved_apen = 0, saved_bpen = 0;
  UBYTE saved_dm = 0;
  fallback_save_rastport_state(rast, &saved_apen, &saved_bpen, &saved_dm);

  if (!fallback_set_pen(NULL, board->colormap, color, rast))
    goto restore;

  WritePixel(rast, x, y);

restore:
  fallback_restore_rastport_state(rast, saved_apen, saved_bpen, saved_dm);
}

void ZuneFallback_BlitRenderPorts(struct RenderPort *source,
                                  struct RenderPort *dest, WORD src_x,
                                  WORD src_y, WORD dest_x, WORD dest_y,
                                  UWORD width, UWORD height) {
  if (!source || !dest)
    return;

  struct RastPort *src_rast = fallback_get_rastport(source);
  struct RastPort *dst_rast = fallback_get_rastport(dest);
  if (!src_rast || !dst_rast)
    return;

  ClipBlit(src_rast, src_x, src_y, dst_rast, dest_x, dest_y, width, height,
           0xC0);
}

void ZuneFallback_BlitToScreen(struct RenderPort *source,
                               struct RastPort *screen_rp, WORD src_x,
                               WORD src_y, WORD dest_x, WORD dest_y,
                               UWORD width, UWORD height) {
  if (!source || !screen_rp)
    return;

  struct RastPort *src_rast = fallback_get_rastport(source);
  if (!src_rast)
    return;

  ClipBlit(src_rast, src_x, src_y, screen_rp, dest_x, dest_y, width, height,
           0xC0);
}

void ZuneFallback_DrawTexture(struct RenderPort *rp,
                              struct ZuneTexture *texture, WORD dest_x,
                              WORD dest_y, UWORD dest_width, UWORD dest_height,
                              WORD src_x, WORD src_y, UWORD src_width,
                              UWORD src_height, struct InternalColor *tint) {
  /* Minimal software fallback: do nothing but avoid crashes */
  (void)rp;
  (void)texture;
  (void)dest_x;
  (void)dest_y;
  (void)dest_width;
  (void)dest_height;
  (void)src_x;
  (void)src_y;
  (void)src_width;
  (void)src_height;
  (void)tint;
}

BOOL ZuneFallback_CopyFromDrawingBoard(struct RenderPort *rp) {
  /*
   * Software fallback: No synchronization needed.
   * For software rendering, the bitmap IS the render target.
   */
  (void)rp;
  return TRUE;
}
