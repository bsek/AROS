/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - ZuneClearDrawingBoard
*/

#include <aros/libcall.h>
#include <exec/types.h>

#include "../backends/backend_interface.h"
#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH2(void, ZuneClearDrawingBoard,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 12, zunegfx)

/*  FUNCTION
    Clears the entire DrawingBoard with the specified color.

INPUTS
    board - DrawingBoard to clear (must not be NULL)
    color - Clear color in ARGB format (0xAARRGGBB)

RESULT
    None

NOTES
    This function uses the most efficient clearing method available
    based on the backend and whether pixels are currently locked.

SEE ALSO
    ZuneClearRenderContext(), FastFillRect()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneClearDrawingBoard");

  D(bug("ZuneGfx: ZuneClearDrawingBoard(board=%p, color=0x%08x)\n",
        rctx->target_board, color));

  if (!ValidateDrawingBoard(rctx->target_board)) {
    D(bug("ZuneGfx: Invalid DrawingBoard\n"));
    return;
  }

  /*
   * Use ZuneClearRenderContext which is more efficient than DrawRectangle:
   * - OpenGL: Uses glClear() which is hardware-optimized
   * - CyberGfx: Uses optimized fill operations
   */
  struct InternalColor internal_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, ClearRenderContext, &internal_color);

  D(bug("ZuneGfx: DrawingBoard cleared\n"));

  EXIT_FUNCTION("ZuneClearDrawingBoard");

  AROS_LIBFUNC_EXIT
}
