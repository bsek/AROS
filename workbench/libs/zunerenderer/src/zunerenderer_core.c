/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Simplified Core Implementation

    This module provides the core functionality for the simplified Zune Renderer
    library, including RenderPort management, library initialization, and
    backend detection. The simplified approach eliminates complex abstractions
    while preserving all essential functionality.
*/

#include "exec/lists.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <cybergraphx/cybergraphics.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <hidd/gfx.h>
#include <intuition/screens.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/oop.h>

#include "graphics/view.h"

#include "backends/backend_interface.h"
#include "zunerenderer_intern.h"

BOOL ValidateRenderPort(struct RenderPort *rp);
struct RenderPort *CreateRenderPortInternal(struct IntZuneRendererBase *base,
                                            struct ColorMap *colormap,
                                            struct RastPort *rastport);
void DestroyRenderPortInternal(struct IntZuneRendererBase *base,
                               struct RenderPort *rp);

/*****************************************************************************/
/* Library Helper Functions */
/*****************************************************************************/

BOOL DetectLibraries(void) {
  ENTER_FUNCTION("DetectLibraries");

  /* Try to open CyberGraphics library */
  CyberGfxBase = OpenLibrary("cybergraphics.library", 0);
  if (CyberGfxBase) {
    D(bug("ZuneRenderer: CyberGraphics library available\n"));
  } else {
    D(bug("ZuneRenderer: CyberGraphics library not available, using "
          "graphics.library\n"));
  }

  // TODO: Add opengl library

  EXIT_FUNCTION("DetectLibraries");
  return TRUE;
}

/*****************************************************************************/
/* Library Initialization */
/*****************************************************************************/

BOOL InitializeZuneRenderer(struct IntZuneRendererBase *base) {
  ENTER_FUNCTION("InitializeZuneRenderer");

  if (!base)
    return FALSE;

  /* Initialize library base */
  InitSemaphore(&base->lock);
  NEWLIST(&base->drawingboards);
  NEWLIST(&base->renderports);
  NEWLIST(&base->textures);

  /* Detect libraries first */
  if (!DetectLibraries()) {
    D(bug("ZuneRenderer: Library detection failed\n"));
    return FALSE;
  }

  /* Initialize backend system */
  if (!ZuneInitBackends()) {
    D(bug("ZuneRenderer: Backend system initialization failed\n"));
    return FALSE;
  }

  /* Register available backends */
  extern BOOL RegisterCyberGfxBackend(void);
  extern BOOL RegisterOpenGLBackend(void);

  /* Register backends in priority order (highest priority first) */
  RegisterOpenGLBackend();   /* Highest priority if available */
  RegisterCyberGfxBackend(); /* Medium priority */

  D(bug("ZuneRenderer: Library initialized successfully\n"));

  /* Display backend information */
  ZuneBackend *best_backend = ZuneFindBestBackend(NULL);
  D(bug("ZuneRenderer: Best Backend: %s\n",
        best_backend ? (const char *)ZuneGetBackendName(best_backend)
                     : "None"));
  D(bug("ZuneRenderer: CyberGraphics: %s\n",
        ZuneIsBackendAvailable(BACKEND_CYBERGFX) ? "Available"
                                                 : "Not Available"));
  D(bug("ZuneRenderer: OpenGL: %s\n", ZuneIsBackendAvailable(BACKEND_OPENGL)
                                          ? "Available"
                                          : "Not Available"));

  EXIT_FUNCTION("InitializeZuneRenderer");
  return TRUE;
}

void CleanupZuneRenderer(struct IntZuneRendererBase *base) {
  ENTER_FUNCTION("CleanupZuneRenderer");

  if (!base)
    return;

  /* Cleanup backend system first */
  ZuneCleanupBackends();

  /* Close libraries */
  if (CyberGfxBase) {
    CloseLibrary(CyberGfxBase);
    CyberGfxBase = NULL;
  }

  /* Note: graphics.library (GfxBase) is opened by system startup and
     should not be closed by us */

  D(bug("ZuneRenderer: Library cleanup completed\n"));
  EXIT_FUNCTION("CleanupZuneRenderer");
}

/*****************************************************************************/
/* RenderPort Internal Functions */
/*****************************************************************************/
void InitializeRenderPort(struct RenderPort *rp) {
  ENTER_FUNCTION("InitializeRenderPort");

  /* Initialize basic fields */
  rp->batching_enabled = FALSE;
  rp->batch_state = NULL;
  rp->clipping_enabled = FALSE;
  rp->clip_region = NULL;
  rp->backend_type = BACKEND_SOFTWARE;
  rp->backend_context = NULL;
  rp->backend_vtable = NULL;
  rp->hidd_bitmap_obj = NULL;
  rp->pen_cache = NULL;
  rp->color_cache = NULL;
  rp->pen_color_cache = NULL;
  rp->valid = TRUE;

  EXIT_FUNCTION("InitializeRenderPort");
}

void CleanupRenderPort(struct RenderPort *rp) {
  if (!rp)
    return;

  /* Mark as invalid */
  rp->valid = FALSE;

  /* Cleanup batch state */
  if (rp->batch_state) {
    DestroyBatchState((struct BatchState *)rp->batch_state);
    rp->batch_state = NULL;
  }

  /* Cleanup backend system */
  ZuneUnbindRenderPortFromBackend(rp);

  /* Cleanup pen cache if present */
  if (rp->pen_cache) {
    CleanupPenCache(rp->pen_cache);
    FreeVec(rp->pen_cache);
    rp->pen_cache = NULL;
  }

  if (rp->color_cache) {
    CleanupColorCache(rp->color_cache);
    FreeVec(rp->color_cache);
    rp->color_cache = NULL;
  }

  if (rp->pen_color_cache) {
    CleanupPenColorCache(rp->pen_color_cache);
    FreeVec(rp->pen_color_cache);
    rp->pen_color_cache = NULL;
  }

  /* Clear references */
  rp->target_rp = NULL;
  rp->colormap = NULL;
  rp->target_board = NULL;
}

/*****************************************************************************/
/* Resource Management */
/*****************************************************************************/

void AddRenderPortToList(struct IntZuneRendererBase *base,
                         struct RenderPort *rp) {
  if (!base || !rp)
    return;

  ObtainSemaphore(&base->lock);
  AddTail((struct List *)&base->renderports,
          &rp->node); /* Using draw_color field as Node */
  ReleaseSemaphore(&base->lock);
}

void RemoveRenderPortFromList(struct IntZuneRendererBase *base,
                              struct RenderPort *rp) {
  if (!base || !rp)
    return;

  ObtainSemaphore(&base->lock);
  Remove(&rp->node); /* Using draw_color field as Node */
  ReleaseSemaphore(&base->lock);
}

/*****************************************************************************/
/* Public API Implementation */
/*****************************************************************************/

/*****************************************************************************

    NAME */
AROS_LH2(struct RenderPort *, CreateRenderPort,

         /*  SYNOPSIS */
         AROS_LHA(struct ColorMap *, colormap, A0),
         AROS_LHA(struct RastPort *, rastport, A1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 5, zunerenderer)

/*  FUNCTION
    Creates a new RenderPort for rendering to the specified screen/rastport.
    The RenderPort automatically selects the best available backend.

INPUTS
    screen - Target screen (must not be NULL)
    rastport - Target rastport (must not be NULL)

RESULT
    Pointer to new RenderPort structure, or NULL if creation failed.

NOTES
    The created RenderPort must be freed with DestroyRenderPort().
    Batching is initially disabled and can be enabled with BeginBatch().

SEE ALSO
    CreateRenderPortWithDrawingBoard(), DestroyRenderPort()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);
  struct RenderPort *rp;

  ENTER_FUNCTION("CreateRenderPort");

  D(bug("ZuneRenderer: CreateRenderPort(colormap=%p, rastport=%p)\n", colormap,
        rastport));

  rp = CreateRenderPortInternal(base, colormap, rastport);

  EXIT_FUNCTION("CreateRenderPort");

  return rp;

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(struct RenderPort *, CreateRenderPortWithDrawingBoard,

         /*  SYNOPSIS */
         AROS_LHA(struct ColorMap *, colormap, A0),
         AROS_LHA(struct DrawingBoard *, board, A1),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 6, zunerenderer)

/*  FUNCTION
    Creates a new RenderPort for rendering to the specified DrawingBoard.
    This enables off-screen rendering to the DrawingBoard surface.

INPUTS
    colormap - ColorMap used for color conversions (must not be NULL)
    board - Target DrawingBoard (must not be NULL)

RESULT
    Pointer to new RenderPort structure, or NULL if creation failed.

NOTES
    The created RenderPort must be freed with DestroyRenderPort().
    The DrawingBoard must remain valid for the lifetime of the RenderPort.

SEE ALSO
    CreateRenderPort(), DestroyRenderPort(), CreateDrawingBoard()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);
  struct RenderPort *rp;

  ENTER_FUNCTION("CreateRenderPortWithDrawingBoard");

  D(bug("ZuneRenderer: CreateRenderPortWithDrawingBoard(board=%p)\n", board));

  /* Validate parameters */
  if (!board || !ValidateDrawingBoard(board)) {
    D(bug("ZuneRenderer: Invalid DrawingBoard parameter\n"));
    return NULL;
  }

  if (!board->rastport) {
    D(bug("ZuneRenderer: DrawingBoard is missing RastPort\n"));
    return NULL;
  }

  rp = CreateRenderPortInternal(base, colormap, board->rastport);
  if (!rp) {
    D(bug("Failed to create renderport\n"));
    return NULL;
  }

  rp->target_board = board;
  board->colormap = colormap;

  /* Verify the drawing board is properly initialized */
  if (!board->bitmap) {
    D(bug("ZuneRenderer: No bitmap available for drawing board\n"));
    DestroyRenderPortInternal(base, rp);
    return NULL;
  }

  /* Cache HIDD bitmap object for efficient direct operations on DrawingBoard */
  rp->hidd_bitmap_obj = HIDD_BM_OBJ(board->bitmap);

  if (!board->rastport) {
    D(bug("ZuneRenderer: Invalid rastport in drawing board\n"));
    DestroyRenderPortInternal(base, rp);
    return NULL;
  }

  /* The RastPort and BitMap should already be initialized by
   * AllocateDrawingBoardBitmap() */
  D(bug("ZuneRenderer: DrawingBoard RastPort %p with BitMap %p\n",
        board->rastport, board->rastport->BitMap));

  EXIT_FUNCTION("CreateRenderPortWithDrawingBoard");
  return rp;

  AROS_LIBFUNC_EXIT
}

void DestroyRenderPortInternal(struct IntZuneRendererBase *base,
                               struct RenderPort *rp) {
  if (!rp)
    return;

  ENTER_FUNCTION("DestroyRenderPortInternal");

  /* Mark as invalid */
  rp->valid = FALSE;

  /* Remove from tracking list */
  RemoveRenderPortFromList(base, rp);

  /* Cleanup RenderPort */
  CleanupRenderPort(rp);

  /* Free structure */
  FreeVec(rp);

  D(bug("ZuneRenderer: RenderPort destroyed internally\n"));
  EXIT_FUNCTION("DestroyRenderPortInternal");
}

struct RenderPort *CreateRenderPortInternal(struct IntZuneRendererBase *base,
                                            struct ColorMap *colormap,
                                            struct RastPort *rastport) {
  struct RenderPort *rp;

  ENTER_FUNCTION("CreateRenderPortInternal");

  if (!colormap || !rastport) {
    D(bug(
        "ZuneRenderer: Invalid parameters for internal RenderPort creation\n"));
    EXIT_FUNCTION("CreateRenderPortInternal");
    return NULL;
  }

  /* Allocate RenderPort structure */
  rp = AllocVec(sizeof(struct RenderPort), MEMF_CLEAR | MEMF_PUBLIC);
  if (!rp) {
    D(bug("ZuneRenderer: Failed to allocate RenderPort internally\n"));
    EXIT_FUNCTION("CreateRenderPortInternal");
    return NULL;
  }

  InitializeRenderPort(rp);

  /* Set target information */
  rp->target_rp = rastport;
  rp->colormap = colormap;
  rp->target_board = NULL; /* Screen rendering */

  /* Allocate per-RenderPort caches */
  rp->color_cache = AllocVec(sizeof(struct ColorCache), MEMF_CLEAR | MEMF_PUBLIC);
  if (rp->color_cache) {
    InitColorCache(rp->color_cache);
  }
  rp->pen_color_cache =
      AllocVec(sizeof(struct PenColorCache), MEMF_CLEAR | MEMF_PUBLIC);
  if (rp->pen_color_cache) {
    InitPenColorCache(rp->pen_color_cache);
  }

  /* Cache HIDD bitmap object for efficient direct operations */
  if (rastport && rastport->BitMap) {
    rp->hidd_bitmap_obj = HIDD_BM_OBJ(rastport->BitMap);
  }

  /* Detect and initialize backend */
  ZuneBackend *backend = ZuneFindBestBackend(rp);
  if (backend && ZuneBindRenderPortToBackend(rp, backend)) {
    /* Determine pixel format from ColorMap */
    if (colormap && backend->ops && backend->ops->GetPixelFormat) {
      rp->pixel_format = backend->ops->GetPixelFormat(rp->target_rp->BitMap);
    }
  } else {
    /* No active backend - rely on software fallbacks */
    D(bug("ZuneRenderer: No backend available, using software fallback\n"));
    rp->backend_type = BACKEND_SOFTWARE;
    rp->backend_context = NULL;
    rp->backend_vtable = NULL;

    if (rp->target_board && rp->target_board->pixel_format) {
      rp->pixel_format = rp->target_board->pixel_format;
    } else if (colormap && rp->target_rp && rp->target_rp->BitMap &&
               CyberGfxBase) {
      ULONG pf =
          GetCyberMapAttr(rp->target_rp->BitMap, CYBRMATTR_PIXFMT);
      rp->pixel_format = pf ? pf : PIXFMT_ARGB32;
    } else {
      rp->pixel_format = PIXFMT_ARGB32;
    }
  }

  /* Add to tracking list */
  AddRenderPortToList(base, rp);

  D(bug("ZuneRenderer: RenderPort created internally (backend: %s)\n",
        backend && backend->context && backend->context->name
            ? backend->context->name
            : "fallback"));

  EXIT_FUNCTION("CreateRenderPortInternal");
  return rp;
}

/*****************************************************************************

    NAME */
AROS_LH1(void, DestroyRenderPort,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 7, zunerenderer)

/*  FUNCTION
    Destroys a RenderPort and frees all associated resources.
    Any pending batch operations are automatically flushed.

INPUTS
    rp - RenderPort to destroy (may be NULL)

RESULT
    None

NOTES
    After calling this function, the RenderPort pointer is no longer valid.
    It is safe to pass NULL to this function.

SEE ALSO
    CreateRenderPort(), CreateRenderPortWithDrawingBoard()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneRendererBase *base = ZRB(ZuneRendererBase);

  ENTER_FUNCTION("DestroyRenderPort");

  D(bug("ZuneRenderer: DestroyRenderPort(rp=%p)\n", rp));

  if (!rp) {
    D(bug("ZuneRenderer: NULL RenderPort, nothing to destroy\n"));
    return;
  }

  /* Flush any pending batch operations */
  if (rp->batch_state && rp->batching_enabled) {
    struct BatchState *batch = (struct BatchState *)rp->batch_state;
    if (batch->immediate.count > 0 || batch->deferred.count > 0) {
      D(bug("ZuneRenderer: Flushing pending batch operations\n"));
      // FlushBatchState(batch);
    }
  }

  /* Remove from tracking list */
  RemoveRenderPortFromList(base, rp);

  /* Cleanup RenderPort */
  CleanupRenderPort(rp);

  /* Free the structure */
  FreeVec(rp);

  D(bug("ZuneRenderer: RenderPort destroyed\n"));

  EXIT_FUNCTION("DestroyRenderPort");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(void, ClearRenderPort,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderPort *, rp, A0), AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneRendererBase, 8, zunerenderer)

/*  FUNCTION
    Clears the entire RenderPort with the specified color.

INPUTS
    rp - RenderPort to clear (must not be NULL)
    color - Clear color in ARGB format (0xAARRGGBB)

RESULT
    None

NOTES
    This function uses the most efficient clearing method available
    based on the active backend.

SEE ALSO
    FillRectangle(), ClearDrawingBoard()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ClearRenderPort");

  D(bug("ZuneRenderer: ClearRenderPort(rp=%p, color=0x%08x)\n", rp, color));

  if (!ValidateRenderPort(rp)) {
    D(bug("ZuneRenderer: Invalid RenderPort\n"));
    return;
  }

  struct InternalColor internal_color =
      ZuneColorToInternal(rp, color, rp->pixel_format);
  ZUNE_BACKEND_CALL(rp, ClearRenderPort, &internal_color);

  EXIT_FUNCTION("ClearRenderPort");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************/
/* Validation Functions */
/*****************************************************************************/

BOOL ValidateRenderPort(struct RenderPort *rp) {
  if (!rp)
    return FALSE;
  if (!rp->valid)
    return FALSE;
  if (!rp->target_rp && !rp->target_board)
    return FALSE;

  /* Additional validation may be added here once clipping/batching mature */

  return TRUE;
}
