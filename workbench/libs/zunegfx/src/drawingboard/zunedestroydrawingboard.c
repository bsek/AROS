/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDestroyDrawingBoard
*/

#include <aros/libcall.h>
#include <exec/types.h>
#include <proto/exec.h>

#include "../zunegfx_intern.h"
#include "../../include/zunegfx.h"
#include "drawingboard_intern.h"

#define DEBUG 1
#include <aros/debug.h>

/*****************************************************************************

    NAME */
AROS_LH2(void, ZuneDestroyDrawingBoard,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct DrawingBoard *, board, A1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 11, zunegfx)

/*  FUNCTION
    Destroys a DrawingBoard and frees all associated resources including
    the bitmap, pixel buffers, and any locked memory.

INPUTS
    rctx    - RenderContext associated with the DrawingBoard
    board - DrawingBoard to destroy (may be NULL)

RESULT
    None

NOTES
    After calling this function, the DrawingBoard pointer is no longer valid.
    It is safe to pass NULL board to this function.

SEE ALSO
    CreateDrawingBoard()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);

  ENTER_FUNCTION("ZuneDestroyDrawingBoard");

  D(bug("ZuneRenderer: ZuneDestroyDrawingBoard(rctx=%p, board=%p)\n", rctx, board));

  if (!board) {
    D(bug("ZuneRenderer: NULL DrawingBoard, nothing to destroy\n"));
    return;
  }

  /* Mark as invalid */
  board->valid = FALSE;

  /* Remove from resource tracking */
  RemoveDrawingBoardFromList(base, board);

  /* Cleanup backend-specific data and free resources */
  CleanupDrawingBoard(rctx, board);

  /* Free structure */
  FreeVec(board);

  D(bug("ZuneRenderer: DrawingBoard destroyed\n"));

  EXIT_FUNCTION("ZuneDestroyDrawingBoard");

  AROS_LIBFUNC_EXIT
}
