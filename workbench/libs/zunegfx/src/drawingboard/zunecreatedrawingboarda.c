/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneCreateDrawingBoardA
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <utility/tagitem.h>

#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

#define DEBUG 1
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH1(struct DrawingBoard *, ZuneCreateDrawingBoardA,

         /*  SYNOPSIS */
         AROS_LHA(struct TagItem *, tags, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 121, zunegfx)

/*  FUNCTION
    Creates a new DrawingBoard from a TagItem list.

    This is the tag-based equivalent of ZuneCreateDrawingBoardForRenderContext().

INPUTS
    tags - TagItem list with the following tags:
        ZUNE_DrawingBoard_RenderContext (struct RenderContext *) - Required
        ZUNE_DrawingBoard_Width        (UWORD)                  - Required
        ZUNE_DrawingBoard_Height       (UWORD)                  - Required
        ZUNE_DrawingBoard_Flags        (ULONG)                  - Default: 0

RESULT
    Pointer to new DrawingBoard, or NULL on failure.

SEE ALSO
    ZuneCreateDrawingBoardForRenderContext(), ZuneDestroyDrawingBoard()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct RenderContext *rctx;
  UWORD width, height;
  ULONG flags;

  ENTER_FUNCTION("ZuneCreateDrawingBoardA");

  if (!tags) {
    D(bug("ZuneRenderer: ZuneCreateDrawingBoardA - NULL tags\n"));
    EXIT_FUNCTION("ZuneCreateDrawingBoardA");
    return NULL;
  }

  rctx   = (struct RenderContext *)LibGetTagData(ZUNE_DrawingBoard_RenderContext, 0, tags);
  width  = (UWORD)LibGetTagData(ZUNE_DrawingBoard_Width, 0, tags);
  height = (UWORD)LibGetTagData(ZUNE_DrawingBoard_Height, 0, tags);
  flags  = (ULONG)LibGetTagData(ZUNE_DrawingBoard_Flags, 0, tags);

  D(bug("ZuneRenderer: ZuneCreateDrawingBoardA(rctx=%p, %dx%d, flags=0x%08x)\n",
        rctx, width, height, flags));

  if (!rctx || !rctx->valid || width == 0 || height == 0) {
    D(bug("ZuneRenderer: ZuneCreateDrawingBoardA - invalid parameters\n"));
    EXIT_FUNCTION("ZuneCreateDrawingBoardA");
    return NULL;
  }

  struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);
  struct DrawingBoard *board = CreateDrawingBoardForRenderContextInternal(base, rctx, width, height, flags);

  EXIT_FUNCTION("ZuneCreateDrawingBoardA");
  return board;
  AROS_LIBFUNC_EXIT
}
