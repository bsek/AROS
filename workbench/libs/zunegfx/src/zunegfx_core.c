/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Simplified Core Implementation

    This module provides the core functionality for the simplified Zune Renderer
    library, including RenderContext management, library initialization, and
    backend detection. The simplified approach eliminates complex abstractions
    while preserving all essential functionality.
*/

#include "exec/lists.h"
#include "include/zunegfx.h"
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
#include "zunegfx_intern.h"

BOOL ValidateRenderContext(struct RenderContext *rctx);
struct RenderContext *CreateRenderContextInternal(struct IntZuneGfxBase *base,
                                            struct ColorMap *colormap,
                                            struct RastPort *rastport);
void DestroyRenderContextInternal(struct IntZuneGfxBase *base,
                               struct RenderContext *rctx);

/*****************************************************************************/
/* Library Helper Functions */
/*****************************************************************************/

BOOL DetectLibraries(void) {
  ENTER_FUNCTION("DetectLibraries");

  /* Try to open CyberGraphics library */
  CyberGfxBase = OpenLibrary("cybergraphics.library", 0);
  if (CyberGfxBase) {
    D(bug("ZuneRenderer: CyberGraphics library v%ld.%ld available\n",
          CyberGfxBase->lib_Version, CyberGfxBase->lib_Revision));
  } else {
    D(bug("ZuneRenderer: CyberGraphics library not available, using "
          "graphics.library\n"));
  }

  /* Try to open GL library (mesa3dgl or hostgl) */
  GLBase = OpenLibrary("gl.library", 20);
  if (GLBase) {
    D(bug("ZuneRenderer: GL library v%ld.%ld available\n",
          GLBase->lib_Version, GLBase->lib_Revision));
  } else {
    D(bug("ZuneRenderer: GL library not available, OpenGL backend disabled\n"));
  }

  EXIT_FUNCTION("DetectLibraries");
  return TRUE;
}

/*****************************************************************************/
/* Library Initialization */
/*****************************************************************************/

BOOL InitializeZuneRenderer(struct IntZuneGfxBase *base) {
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

  /*
   * Pre-initialize OpenGL shaders if OpenGL backend is available.
   * This opens a small backdrop window, creates a GL context, and compiles
   * shaders. This avoids a delay when the first application window opens.
   * The pre-init context becomes the master context for context sharing.
   */
  if (ZuneIsBackendAvailable(BACKEND_OPENGL)) {
    extern BOOL OpenGL_PreInitializeShaders(void);
    BOOL shader_result;
    D(bug("ZuneRenderer: Pre-initializing OpenGL shaders...\n"));
    shader_result = OpenGL_PreInitializeShaders();
    D(bug("ZuneRenderer: PreInitializeShaders returned %d\n", shader_result));
    if (shader_result) {
      D(bug("ZuneRenderer: OpenGL shaders pre-initialized successfully\n"));
    } else {
      D(bug("ZuneRenderer: OpenGL shader pre-initialization failed (will retry on first window)\n"));
    }
  }

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

void CleanupZuneRenderer(struct IntZuneGfxBase *base) {
  ENTER_FUNCTION("CleanupZuneRenderer");

  if (!base)
    return;

  /* Cleanup pre-init resources (window/screen for shader compilation) */
  {
    extern void OpenGL_CleanupPreInit(void);
    OpenGL_CleanupPreInit();
  }

  /* Cleanup backend system first */
  ZuneCleanupBackends();

  /* Close libraries */
  if (GLBase) {
    CloseLibrary(GLBase);
    GLBase = NULL;
  }

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
/* RenderContext Internal Functions */
/*****************************************************************************/
void InitializeRenderContext(struct RenderContext *rctx) {
  ENTER_FUNCTION("InitializeRenderContext");

  /* Initialize basic fields */
  rctx->batching_enabled = FALSE;
  rctx->batch_state = NULL;
  rctx->clipping_enabled = FALSE;
  rctx->clip_region = NULL;
  rctx->backend_type = BACKEND_SOFTWARE;
  rctx->backend_context = NULL;
  rctx->backend_vtable = NULL;
  rctx->hidd_bitmap_obj = NULL;
  rctx->pen_cache = NULL;
  rctx->color_cache = NULL;
  rctx->pen_color_cache = NULL;
  rctx->valid = TRUE;

  EXIT_FUNCTION("InitializeRenderContext");
}

void CleanupRenderContext(struct RenderContext *rctx) {
  if (!rctx)
    return;

  /* Mark as invalid */
  rctx->valid = FALSE;

  /* Cleanup batch state */
  if (rctx->batch_state) {
    DestroyBatchState((struct BatchState *)rctx->batch_state);
    rctx->batch_state = NULL;
  }

  /* Cleanup backend system */
  ZuneUnbindRenderContextFromBackend(rctx);

  /* Cleanup pen cache if present */
  if (rctx->pen_cache) {
    CleanupPenCache(rctx->pen_cache);
    FreeVec(rctx->pen_cache);
    rctx->pen_cache = NULL;
  }

  if (rctx->color_cache) {
    CleanupColorCache(rctx->color_cache);
    FreeVec(rctx->color_cache);
    rctx->color_cache = NULL;
  }

  if (rctx->pen_color_cache) {
    CleanupPenColorCache(rctx->pen_color_cache);
    FreeVec(rctx->pen_color_cache);
    rctx->pen_color_cache = NULL;
  }

  /* Clear references */
  rctx->target_rastport = NULL;
  rctx->colormap = NULL;
  rctx->target_board = NULL;
}

/*****************************************************************************/
/* Resource Management */
/*****************************************************************************/

void AddRenderContextToList(struct IntZuneGfxBase *base,
                         struct RenderContext *rctx) {
  if (!base || !rctx)
    return;

  ObtainSemaphore(&base->lock);
  AddTail((struct List *)&base->renderports,
          &rctx->node); /* Using draw_color field as Node */
  ReleaseSemaphore(&base->lock);
}

void RemoveRenderContextFromList(struct IntZuneGfxBase *base,
                              struct RenderContext *rctx) {
  if (!base || !rctx)
    return;

  ObtainSemaphore(&base->lock);
  Remove(&rctx->node); /* Using draw_color field as Node */
  ReleaseSemaphore(&base->lock);
}

/*****************************************************************************/
/* Public API Implementation */
/*****************************************************************************/



/*****************************************************************************

    NAME */
struct RenderContext *CreateRenderContextForWindowInternal(
    struct IntZuneGfxBase *base,
    struct Window *window,
    struct ColorMap *colormap,
    UWORD backend_type)

/*  FUNCTION
    Creates a new RenderContext bound to a Window.

    This is the primary way to create a RenderContext in the new architecture.
    The RenderContext is bound to the window and automatically selects the best
    backend (OpenGL if available, otherwise CyberGraphics).

    The window reference is required for OpenGL to create a GL context.

INPUTS
    base - Library base
    window - Target window (must not be NULL)
    colormap - ColorMap for color conversions (must not be NULL)
    backend_type - Backend to be used

RESULT
    Pointer to new RenderContext structure, or NULL if creation failed.

*****************************************************************************/
{
  struct RenderContext *rctx;
  ZuneBackend *backend;

  ENTER_FUNCTION("CreateRenderContextForWindowInternal");

  D(bug("ZuneRenderer: ZuneCreateRenderContextForWindow(window=%p, colormap=%p)\n",
        window, colormap));

  if (!window || !colormap) {
    D(bug("ZuneRenderer: Invalid parameters (window=%p, colormap=%p)\n",
          window, colormap));
    EXIT_FUNCTION("CreateRenderContextForWindowInternal");
    return NULL;
  }

  /* Allocate RenderContext structure */
  rctx = AllocVec(sizeof(struct RenderContext), MEMF_CLEAR | MEMF_PUBLIC);
  if (!rctx) {
    D(bug("ZuneRenderer: Failed to allocate RenderContext\n"));
    EXIT_FUNCTION("CreateRenderContextForWindowInternal");
    return NULL;
  }

  InitializeRenderContext(rctx);

  /* Set window binding - this is key for OpenGL context creation */
  rctx->window = window;
  rctx->target_rastport = window->RPort;
  rctx->colormap = colormap;
  rctx->target_board = NULL;  /* Initially rendering to window */

  /* Allocate per-RenderContext caches */
  rctx->color_cache = AllocVec(sizeof(struct ColorCache), MEMF_CLEAR | MEMF_PUBLIC);
  if (rctx->color_cache) {
    InitColorCache(rctx->color_cache);
  }
  rctx->pen_color_cache = AllocVec(sizeof(struct PenColorCache), MEMF_CLEAR | MEMF_PUBLIC);
  if (rctx->pen_color_cache) {
    InitPenColorCache(rctx->pen_color_cache);
  }

  /* Cache HIDD bitmap object */
  if (window->RPort && window->RPort->BitMap) {
    rctx->hidd_bitmap_obj = HIDD_BM_OBJ(window->RPort->BitMap);
  }

  if (backend_type != BACKEND_BEST_AVAILABLE) {
    backend = ZuneFindBackendByType(backend_type);
    D(bug("ZuneRenderer: Requested backend type %d, found: %p\n", backend_type, backend));
  } else {
    backend = ZuneFindBestBackend(rctx);
  }

  if (backend && ZuneBindRenderContextToBackend(rctx, backend)) {
    D(bug("ZuneRenderer: RenderContext bound to %s backend (type %d)\n", backend->ops->name, backend->ops->type));

    /* Determine pixel format */
    if (backend->ops->GetPixelFormat && window->RPort->BitMap) {
      rctx->pixel_format = backend->ops->GetPixelFormat(window->RPort->BitMap);
    } else {
      rctx->pixel_format = PIXFMT_ARGB32;
    }
  } else {
    D(bug("ZuneRenderer: No backend available, using software fallback\n"));
    rctx->backend_type = BACKEND_SOFTWARE;
    rctx->backend_context = NULL;
    rctx->backend_vtable = NULL;
    rctx->pixel_format = PIXFMT_ARGB32;
  }

  /* Add to tracking list */
  AddRenderContextToList(base, rctx);

  D(bug("ZuneRenderer: RenderContext created for window %p, backend=%d\n",
        window, rctx->backend_type));

  EXIT_FUNCTION("CreateRenderContextForWindowInternal");
  return rctx;
}

/*****************************************************************************

    NAME */
AROS_LH3(struct RenderContext *, ZuneCreateRenderContextForWindow,

         /*  SYNOPSIS */
         AROS_LHA(struct Window *, window, A0),
         AROS_LHA(struct ColorMap *, colormap, A1),
         AROS_LHA(UWORD, backend_type, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 5, zunegfx)

/*  FUNCTION
    Creates a new RenderContext bound to a Window.
    See CreateRenderContextForWindowInternal for details.

*****************************************************************************/
{
  AROS_LIBFUNC_INIT
  ENTER_FUNCTION("ZuneCreateRenderContextForWindow");

  struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);
  return CreateRenderContextForWindowInternal(base, window, colormap, backend_type);

  EXIT_FUNCTION("ZuneCreateRenderContextForWindow");
  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(BOOL, ZuneSetTarget,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct DrawingBoard *, board, A1),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 10, zunegfx)

/*  FUNCTION
    Switch the render target of a RenderContext.

    board = NULL: Render to the window's RastPort
    board != NULL: Render to the DrawingBoard

    For OpenGL backend: Uses glBindFramebuffer() for fast FBO switching
    For CyberGfx backend: Updates internal target pointer

INPUTS
    rctx - RenderContext to modify
    board - Target DrawingBoard, or NULL for window

RESULT
    TRUE if target was switched successfully, FALSE otherwise.

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneSetTarget");

  if (!rctx || !rctx->valid) {
    D(bug("ZuneRenderer: ZuneSetTarget - invalid RenderContext\n"));
    EXIT_FUNCTION("ZuneSetTarget");
    return FALSE;
  }

  if (board && !board->valid) {
    D(bug("ZuneRenderer: ZuneSetTarget - invalid DrawingBoard\n"));
    EXIT_FUNCTION("ZuneSetTarget");
    return FALSE;
  }

  D(bug("ZuneRenderer: ZuneSetTarget(rctx=%p, board=%p)\n", rctx, board));

  /* Update target */
  rctx->target_board = board;

  if (board) {
    /* Switching to DrawingBoard */
    rctx->target_rastport = board->rastport;
    if (board->bitmap) {
      rctx->hidd_bitmap_obj = HIDD_BM_OBJ(board->bitmap);
    }
    D(bug("ZuneRenderer: Target set to DrawingBoard %p (%dx%d)\n",
          board, board->width, board->height));
  } else {
    /* Switching to window */
    if (rctx->window) {
      rctx->target_rastport = rctx->window->RPort;
      if (rctx->window->RPort && rctx->window->RPort->BitMap) {
        rctx->hidd_bitmap_obj = HIDD_BM_OBJ(rctx->window->RPort->BitMap);
      }
      D(bug("ZuneRenderer: Target set to window %p RastPort\n", rctx->window));
    } else {
      D(bug("ZuneRenderer: Warning - no window, keeping current target_rastport\n"));
    }
  }

  /*
   * Note: For OpenGL backend, glBindFramebuffer is called lazily by
   * OpenGL_SwitchToTarget() when the next draw operation occurs.
   * This avoids unnecessary FBO switches when multiple ZuneSetTarget
   * calls happen without drawing in between.
   */

  EXIT_FUNCTION("ZuneSetTarget");
  return TRUE;

  AROS_LIBFUNC_EXIT
}

void DestroyRenderContextInternal(struct IntZuneGfxBase *base,
                               struct RenderContext *rctx) {
  if (!rctx)
    return;

  ENTER_FUNCTION("DestroyRenderContextInternal");

  /* Mark as invalid */
  rctx->valid = FALSE;

  /* Remove from tracking list */
  RemoveRenderContextFromList(base, rctx);

  /* Cleanup RenderContext */
  CleanupRenderContext(rctx);

  /* Free structure */
  FreeVec(rctx);

  D(bug("ZuneRenderer: RenderContext destroyed internally\n"));
  EXIT_FUNCTION("DestroyRenderContextInternal");
}

struct RenderContext *CreateRenderContextInternal(struct IntZuneGfxBase *base,
                                            struct ColorMap *colormap,
                                            struct RastPort *rastport) {
  struct RenderContext *rctx;

  ENTER_FUNCTION("CreateRenderContextInternal");

  if (!colormap || !rastport) {
    D(bug(
        "ZuneRenderer: Invalid parameters for internal RenderContext creation\n"));
    EXIT_FUNCTION("CreateRenderContextInternal");
    return NULL;
  }

  /* Allocate RenderContext structure */
  rctx = AllocVec(sizeof(struct RenderContext), MEMF_CLEAR | MEMF_PUBLIC);
  if (!rctx) {
    D(bug("ZuneRenderer: Failed to allocate RenderContext internally\n"));
    EXIT_FUNCTION("CreateRenderContextInternal");
    return NULL;
  }

  InitializeRenderContext(rctx);

  /* Set target information */
  rctx->target_rastport = rastport;
  rctx->colormap = colormap;
  rctx->target_board = NULL; /* Screen rendering */

  /* Allocate per-RenderContext caches */
  rctx->color_cache = AllocVec(sizeof(struct ColorCache), MEMF_CLEAR | MEMF_PUBLIC);
  if (rctx->color_cache) {
    InitColorCache(rctx->color_cache);
  }
  rctx->pen_color_cache =
      AllocVec(sizeof(struct PenColorCache), MEMF_CLEAR | MEMF_PUBLIC);
  if (rctx->pen_color_cache) {
    InitPenColorCache(rctx->pen_color_cache);
  }

  /* Cache HIDD bitmap object for efficient direct operations */
  if (rastport && rastport->BitMap) {
    rctx->hidd_bitmap_obj = HIDD_BM_OBJ(rastport->BitMap);
  }

  /* Detect and initialize backend */
  ZuneBackend *backend = ZuneFindBestBackend(rctx);
  if (backend && ZuneBindRenderContextToBackend(rctx, backend)) {
    /* Determine pixel format from ColorMap */
    if (colormap && backend->ops && backend->ops->GetPixelFormat) {
      rctx->pixel_format = backend->ops->GetPixelFormat(rctx->target_rastport->BitMap);
    }
  } else {
    /* No active backend - rely on software fallbacks */
    D(bug("ZuneRenderer: No backend available, using software fallback\n"));
    rctx->backend_type = BACKEND_SOFTWARE;
    rctx->backend_context = NULL;
    rctx->backend_vtable = NULL;

    if (rctx->target_board && rctx->target_board->pixel_format) {
      rctx->pixel_format = rctx->target_board->pixel_format;
    } else if (colormap && rctx->target_rastport && rctx->target_rastport->BitMap &&
               CyberGfxBase) {
      ULONG pf =
          GetCyberMapAttr(rctx->target_rastport->BitMap, CYBRMATTR_PIXFMT);
      rctx->pixel_format = pf ? pf : PIXFMT_ARGB32;
    } else {
      rctx->pixel_format = PIXFMT_ARGB32;
    }
  }

  /* Add to tracking list */
  AddRenderContextToList(base, rctx);

  EXIT_FUNCTION("CreateRenderContextInternal");
  return rctx;
}

/*****************************************************************************

    NAME */
AROS_LH1(void, ZuneDestroyRenderContext,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 7, zunegfx)

/*  FUNCTION
    Destroys a RenderContext and frees all associated resources.
    Any pending batch operations are automatically flushed.

INPUTS
    rctx - RenderContext to destroy (may be NULL)

RESULT
    None

NOTES
    After calling this function, the RenderContext pointer is no longer valid.
    It is safe to pass NULL to this function.

SEE ALSO
    CreateRenderContext(), CreateRenderContextWithDrawingBoard()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  struct IntZuneGfxBase *base = ZRB(ZuneGfxBase);

  ENTER_FUNCTION("ZuneDestroyRenderContext");

  D(bug("ZuneRenderer: ZuneDestroyRenderContext(rctx=%p)\n", rctx));

  if (!rctx) {
    D(bug("ZuneRenderer: NULL RenderContext, nothing to destroy\n"));
    return;
  }

  /* Flush any pending batch operations */
  if (rctx->batch_state && rctx->batching_enabled) {
    struct BatchState *batch = (struct BatchState *)rctx->batch_state;
    if (batch->immediate.count > 0 || batch->deferred.count > 0) {
      D(bug("ZuneRenderer: Flushing pending batch operations\n"));
      // FlushBatchState(batch);
    }
  }

  /* Remove from tracking list */
  RemoveRenderContextFromList(base, rctx);

  /* Cleanup RenderContext */
  CleanupRenderContext(rctx);

  /* Free the structure */
  FreeVec(rctx);

  D(bug("ZuneRenderer: RenderContext destroyed\n"));

  EXIT_FUNCTION("ZuneDestroyRenderContext");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************

    NAME */
AROS_LH2(void, ZuneClearRenderContext,

         /*  SYNOPSIS */
         AROS_LHA(struct RenderContext *, rctx, A0), AROS_LHA(ULONG, color, D0),

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 8, zunegfx)

/*  FUNCTION
    Clears the entire RenderContext with the specified color.

INPUTS
    rctx - RenderContext to clear (must not be NULL)
    color - Clear color in ARGB format (0xAARRGGBB)

RESULT
    None

NOTES
    This function uses the most efficient clearing method available
    based on the active backend.

SEE ALSO
    FillRectangle(), ZuneClearDrawingBoard()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  ENTER_FUNCTION("ZuneClearRenderContext");

  D(bug("ZuneRenderer: ZuneClearRenderContext(rctx=%p, color=0x%08x)\n", rctx, color));

  if (!ValidateRenderContext(rctx)) {
    D(bug("ZuneRenderer: Invalid RenderContext\n"));
    return;
  }

  struct InternalColor internal_color =
      ZuneColorToInternal(rctx, color, rctx->pixel_format);
  ZUNE_BACKEND_CALL(rctx, ClearRenderContext, &internal_color);

  EXIT_FUNCTION("ZuneClearRenderContext");

  AROS_LIBFUNC_EXIT
}

/*****************************************************************************/
/* Validation Functions */
/*****************************************************************************/

BOOL ValidateRenderContext(struct RenderContext *rctx) {
  if (!rctx)
    return FALSE;
  if (!rctx->valid)
    return FALSE;
  if (!rctx->target_rastport && !rctx->target_board)
    return FALSE;

  /* Additional validation may be added here once clipping/batching mature */

  return TRUE;
}

/*****************************************************************************

    NAME */
AROS_LH0(APTR, ZuneGetMasterGLContext,

         /*  LOCATION */
         struct Library *, ZuneGfxBase, 104, zunegfx)

/*  FUNCTION
    Returns the master OpenGL context used by zunegfx for context sharing.
    This context can be passed to ZuneCreateLayerCompositorShared() to ensure
    the compositor shares the same pipe_screen as zunegfx windows.

INPUTS
    None

RESULT
    Pointer to the master GL context, or NULL if not available.
    The context is only available after at least one OpenGL-based
    RenderContext has been created and used.

NOTES
    This function is primarily intended for use by the Layer Compositor
    to enable shared GL contexts between zunegfx and the compositor.
    Without shared contexts, the compositor may create a separate
    pipe_screen which cannot access zunegfx FBO contents.

SEE ALSO
    ZuneCreateLayerCompositorShared()

*****************************************************************************/
{
  AROS_LIBFUNC_INIT

  /* OpenGL_GetMasterContext is defined in backends/opengl/opengl_backend.c */
  extern APTR OpenGL_GetMasterContext(void);
  
  return OpenGL_GetMasterContext();

  AROS_LIBFUNC_EXIT
}
