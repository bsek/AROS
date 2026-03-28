/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneBlit
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/graphics.h>

#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH8(void, ZuneBlit,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, src_rctx, A0),
         AROS_LHA(struct RenderContext *, dst_rctx, A1),
         AROS_LHA(WORD, src_x, D0),
         AROS_LHA(WORD, src_y, D1),
         AROS_LHA(WORD, dst_x, D2),
         AROS_LHA(WORD, dst_y, D3),
         AROS_LHA(UWORD, width, D4),
         AROS_LHA(UWORD, height, D5),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 25, zunegfx)

/*  FUNCTION
    General-purpose blit between two RenderContexts. Handles all combinations:
    - DrawingBoard to DrawingBoard
    - DrawingBoard to screen RastPort
    - Screen RastPort to DrawingBoard
    - Screen RastPort to screen RastPort

    Automatically syncs OpenGL FBO to bitmap when source is a DrawingBoard.

INPUTS
    src_rctx - Source RenderContext
    dst_rctx - Destination RenderContext
    src_x, src_y - Source coordinates
    dst_x, dst_y - Destination coordinates
    width, height - Size of area to blit

RESULT
    None

NOTES
    The function determines source/destination based on RenderContext targets:
    - If target_board is set, uses the DrawingBoard
    - Otherwise uses target_rastport or window->RPort

SEE ALSO
    ZunePresent(), ZuneCapture(), ZuneSetTarget()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct RastPort *src_rastport = NULL;
  struct RastPort *dst_rastport = NULL;
  struct BitMap *src_bitmap = NULL;

  ENTER_FUNCTION("ZuneBlit");

  D(bug("ZuneGfx: ZuneBlit(src_rctx=%p, dst_rctx=%p, src=%d,%d dst=%d,%d %dx%d)\n",
        src_rctx, dst_rctx, src_x, src_y, dst_x, dst_y, width, height));

  if (!src_rctx || !dst_rctx) {
    D(bug("ZuneGfx: Invalid RenderContext parameters\n"));
    EXIT_FUNCTION("ZuneBlit");
    return;
  }

  if (width == 0 || height == 0) {
    EXIT_FUNCTION("ZuneBlit");
    return;
  }

  /* Determine source */
  if (src_rctx->target_board && src_rctx->target_board->valid) {
    /* Source is DrawingBoard - sync FBO to bitmap first */
    ZuneBackend *backend = ZuneGetRenderContextBackend(src_rctx);
    if (backend && backend->ops) {
      if (backend->ops->FlushBatch) {
        backend->ops->FlushBatch(src_rctx);
      }
      if (backend->ops->CopyRegionFromDrawingBoard) {
        backend->ops->CopyRegionFromDrawingBoard(src_rctx, src_x, src_y, width, height);
      } else if (backend->ops->CopyFromDrawingBoard) {
        backend->ops->CopyFromDrawingBoard(src_rctx);
      }
    }
    src_bitmap = src_rctx->target_board->bitmap;
    D(bug("ZuneBlit: Source is DrawingBoard, bitmap=%p\n", src_bitmap));
  } else if (src_rctx->target_rastport) {
    src_rastport = src_rctx->target_rastport;
    src_bitmap = src_rastport->BitMap;
    D(bug("ZuneBlit: Source is target_rastport, rastport=%p\n", src_rastport));
  } else if (src_rctx->window && src_rctx->window->RPort) {
    src_rastport = src_rctx->window->RPort;
    src_bitmap = src_rastport->BitMap;
    D(bug("ZuneBlit: Source is window RPort, rastport=%p\n", src_rastport));
  }

  if (!src_bitmap) {
    D(bug("ZuneBlit: No valid source bitmap\n"));
    EXIT_FUNCTION("ZuneBlit");
    return;
  }

  /* Determine destination */
  if (dst_rctx->target_board && dst_rctx->target_board->valid) {
    /* Destination is DrawingBoard - use internal blit for board-to-board */
    if (src_rctx->target_board && src_rctx->target_board->valid) {
      D(bug("ZuneBlit: Board to Board blit\n"));
      BlitDrawingBoardInternal(src_rctx->target_board, dst_rctx->target_board,
                               src_x, src_y, dst_x, dst_y, width, height);
    } else {
      /* RastPort to DrawingBoard - use backend CopyFromRastPort */
      ZuneBackend *backend = ZuneGetRenderContextBackend(dst_rctx);
      if (backend && backend->ops && backend->ops->CopyFromRastPort && src_rastport) {
        D(bug("ZuneBlit: RastPort to Board via CopyFromRastPort\n"));
        backend->ops->CopyFromRastPort(dst_rctx, src_rastport, src_x, src_y,
                                       dst_x, dst_y, width, height);
      } else {
        D(bug("ZuneBlit: Backend does not support CopyFromRastPort\n"));
      }
    }
  } else {
    /* Destination is screen RastPort */
    if (dst_rctx->target_rastport) {
      dst_rastport = dst_rctx->target_rastport;
    } else if (dst_rctx->window && dst_rctx->window->RPort) {
      dst_rastport = dst_rctx->window->RPort;
    }

    if (!dst_rastport || !dst_rastport->BitMap) {
      D(bug("ZuneBlit: No valid destination RastPort\n"));
      EXIT_FUNCTION("ZuneBlit");
      return;
    }

    D(bug("ZuneBlit: Blitting to screen RastPort %p\n", dst_rastport));
    BltBitMapRastPort(src_bitmap, src_x, src_y, dst_rastport, dst_x, dst_y,
                      width, height, 0xC0);
  }

  EXIT_FUNCTION("ZuneBlit");

  AROS_LIBFUNC_EXIT
}
