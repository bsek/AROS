/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - OpenGL Backend Implementation (STUB)

    This file provides a stub implementation of the OpenGL rendering backend
    for ZuneRenderer using the new ZuneBackendOps interface. Currently,
    OpenGL support is not available in AROS, so this backend will report
    as unavailable and provide fallback behavior.

    This serves as a template for future OpenGL backend implementation when
    OpenGL support becomes available in AROS.
*/

#include <aros/debug.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "../backend_interface.h"
#include "opengl_backend.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************/
/* OpenGL Backend Context */
/*****************************************************************************/

typedef struct OpenGLPrivateData {
  BOOL initialized;
  BOOL context_available;

} OpenGLPrivateData;

/*****************************************************************************/
/* Forward Declarations */
/*****************************************************************************/

static BOOL OpenGLInitBackend(ZuneBackendContext *ctx);
static void OpenGLCleanupBackend(ZuneBackendContext *ctx);
static BOOL OpenGLIsAvailable(void);
static BOOL OpenGLIsCompatible(struct RenderPort *rp);
static ULONG OpenGLGetCapabilities(void);

static BOOL OpenGLInitRenderPort(struct RenderPort *rp);
static void OpenGLCleanupRenderPort(struct RenderPort *rp);

static BOOL OpenGLPrepareColor(struct RenderPort *rp,
                               struct InternalColor *color);
static void OpenGLReleaseColor(struct RenderPort *rp,
                               struct InternalColor *color);

static void OpenGLDrawPixel(struct RenderPort *rp, WORD x, WORD y,
                            struct InternalColor *color, BOOL antialias);
static void OpenGLDrawLine(struct RenderPort *rp, WORD startX, WORD startY,
                           WORD endX, WORD endY, UWORD width,
                           struct InternalColor *color, BOOL antialias);
static void OpenGLDrawRectangle(struct RenderPort *rp, WORD x, WORD y,
                                UWORD width, UWORD height, UBYTE lineWidth,
                                UBYTE cornerRadius,
                                struct InternalColor *fillColor,
                                struct InternalColor *borderColor, BOOL filled,
                                BOOL antialias);
static void OpenGLDrawCircle(struct RenderPort *rp, WORD centerX, WORD centerY,
                             UWORD radius, UBYTE borderWidth,
                             struct InternalColor *color,
                             struct InternalColor *borderColor, BOOL filled,
                             BOOL antialias);

static void OpenGLClearRenderPort(struct RenderPort *rp,
                                  struct InternalColor *color);

static APTR OpenGLLockPixels(struct DrawingBoard *board, ULONG *pitch_out);
static void OpenGLUnlockPixels(struct DrawingBoard *board);
static ULONG OpenGLGetPixel(struct DrawingBoard *board, WORD x, WORD y);
static void OpenGLSetPixel(struct DrawingBoard *board, WORD x, WORD y,
                           struct InternalColor *color);

static void OpenGLBeginBatch(struct RenderPort *rp);
static void OpenGLEndBatch(struct RenderPort *rp);
static void OpenGLFlushBatch(struct RenderPort *rp);
static BOOL OpenGLIsBatching(struct RenderPort *rp);

static void OpenGLBlitRenderPorts(struct RenderPort *source,
                                  struct RenderPort *dest, WORD src_x,
                                  WORD src_y, WORD dest_x, WORD dest_y,
                                  UWORD width, UWORD height);
static void OpenGLBlitToScreen(struct RenderPort *source,
                               struct RastPort *screen_rp, WORD src_x,
                               WORD src_y, WORD dest_x, WORD dest_y,
                               UWORD width, UWORD height);

static BOOL OpenGLInitDrawingBoard(struct DrawingBoard *board);
static void OpenGLCleanupDrawingBoard(struct DrawingBoard *board);

/*****************************************************************************/
/* Backend Operations Table */
/*****************************************************************************/

ZuneBackendOps opengl_backend_ops = {
    .name = "OpenGL (Not Available)",
    .type = BACKEND_OPENGL,
    .GetCapabilities = OpenGLGetCapabilities,

    .InitBackend = OpenGLInitBackend,
    .CleanupBackend = OpenGLCleanupBackend,
    .IsAvailable = OpenGLIsAvailable,
    .IsCompatible = OpenGLIsCompatible,

    .InitRenderPort = OpenGLInitRenderPort,
    .CleanupRenderPort = OpenGLCleanupRenderPort,

    .PrepareColor = OpenGLPrepareColor,
    .ReleaseColor = OpenGLReleaseColor,

    .DrawPixel = OpenGLDrawPixel,
    .DrawLine = OpenGLDrawLine,
//    .DrawRectangle = OpenGLDrawRectangle,
//    .DrawCircle = OpenGLDrawCircle,

    .ClearRenderPort = OpenGLClearRenderPort,

    .LockPixels = OpenGLLockPixels,
    .UnlockPixels = OpenGLUnlockPixels,
    .GetPixel = OpenGLGetPixel,
    .SetPixel = OpenGLSetPixel,

    .BeginBatch = OpenGLBeginBatch,
    .EndBatch = OpenGLEndBatch,
    .FlushBatch = OpenGLFlushBatch,
    .IsBatching = OpenGLIsBatching,

    .BlitRenderPorts = OpenGLBlitRenderPorts,
    .BlitToScreen = OpenGLBlitToScreen,

    .InitDrawingBoard = OpenGLInitDrawingBoard,
    .CleanupDrawingBoard = OpenGLCleanupDrawingBoard,

    .reserved = {NULL, NULL}};

/*****************************************************************************/
/* Backend Implementation */
/*****************************************************************************/

static BOOL OpenGLInitBackend(ZuneBackendContext *ctx) {
  if (!ctx) {
    return FALSE;
  }

  D(bug("OpenGLInitBackend: Initializing OpenGL backend (stub)\n"));

  /* Allocate private data */
  OpenGLPrivateData *priv = AllocVec(sizeof(OpenGLPrivateData), MEMF_CLEAR);
  if (!priv) {
    D(bug("OpenGLInitBackend: Failed to allocate private data\n"));
    return FALSE;
  }

  /* OpenGL is not available in AROS */
  priv->initialized = TRUE;
  priv->context_available = FALSE;
  /* Store private data */
  ctx->private_data = priv;
  ctx->capabilities = 0; /* No capabilities */
  ctx->initialized = TRUE;

  D(bug("OpenGLInitBackend: OpenGL backend initialized (not available)\n"));
  return TRUE;
}

static void OpenGLCleanupBackend(ZuneBackendContext *ctx) {
  if (!ctx || !ctx->private_data) {
    return;
  }

  D(bug("OpenGLCleanupBackend: Cleaning up OpenGL backend\n"));

  OpenGLPrivateData *priv = (OpenGLPrivateData *)ctx->private_data;

  /* Free private data */
  FreeVec(priv);
  ctx->private_data = NULL;
  ctx->initialized = FALSE;
}

static BOOL OpenGLIsAvailable(void) {
  /* OpenGL is not available in AROS yet */
  return FALSE;
}

static BOOL OpenGLIsCompatible(struct RenderPort *rp) {
  /* OpenGL backend is not yet implemented, so it's not compatible with anything
   */
  D(bug("OpenGLIsCompatible: OpenGL backend not yet implemented\n"));
  return FALSE;
}

static ULONG OpenGLGetCapabilities(void) {
  /* No OpenGL capabilities available */
  return 0;
}

/*****************************************************************************/
/* RenderPort Management */
/*****************************************************************************/

static BOOL OpenGLInitRenderPort(struct RenderPort *rp) {
  if (!rp) {
    return FALSE;
  }

  D(bug("OpenGLInitRenderPort: RenderPort %p (stub - no operation)\n", rp));

  /* Nothing to initialize since OpenGL is not available */
  return TRUE;
}

static void OpenGLCleanupRenderPort(struct RenderPort *rp) {
  if (!rp) {
    return;
  }

  D(bug("OpenGLCleanupRenderPort: RenderPort %p (stub - no operation)\n", rp));

  /* Nothing to cleanup */
}

/*****************************************************************************/
/* Color Management */
/*****************************************************************************/

static BOOL OpenGLPrepareColor(struct RenderPort *rp,
                               struct InternalColor *color) {
  if (!color) {
    return FALSE;
  }

  /* Basic color preparation - just store the ARGB value */
  /* In a real OpenGL implementation, this would convert to OpenGL color format
   */

  return TRUE;
}

static void OpenGLReleaseColor(struct RenderPort *rp,
                               struct InternalColor *color) {
  /* Nothing to release in stub */
}

/*****************************************************************************/
/* Drawing Operations (All Stubs) */
/*****************************************************************************/

static void OpenGLDrawPixel(struct RenderPort *rp, WORD x, WORD y,
                            struct InternalColor *color, BOOL antialias) {
  /* Stub - no operation */
  D(bug("OpenGLDrawPixel: (%d,%d) - stub implementation\n", x, y));
}

static void OpenGLDrawLine(struct RenderPort *rp, WORD startX, WORD startY,
                           WORD endX, WORD endY, UWORD width,
                           struct InternalColor *color, BOOL antialias) {
  /* Stub - no operation */
  D(bug("OpenGLDrawLine: (%d,%d) to (%d,%d) - stub implementation\n", startX,
        startY, endX, endY));
}

static void OpenGLDrawRectangle(struct RenderPort *rp, WORD x, WORD y,
                                UWORD width, UWORD height, UBYTE lineWidth,
                                UBYTE cornerRadius,
                                struct InternalColor *fill_color,
                                struct InternalColor *border_color, BOOL filled,
                                BOOL antialias) {
  /* Stub - no operation */
  D(bug("OpenGLDrawRectangle: (%d,%d) %dx%d - stub implementation\n", x, y,
        width, height));
}

static void OpenGLDrawCircle(struct RenderPort *rp, WORD centerX, WORD centerY,
                             UWORD radius, UBYTE borderWidth,
                             struct InternalColor *color,
                             struct InternalColor *borderColor, BOOL filled,
                             BOOL antialias) {
  /* Stub - no operation */
  D(bug("OpenGLDrawCircle: (%d,%d) radius %.2f - stub implementation\n",
        centerX, centerY, radius));
}

/*****************************************************************************/
/* Surface Operations */
/*****************************************************************************/

static void OpenGLClearRenderPort(struct RenderPort *rp,
                                  struct InternalColor *color) {
  /* Stub - no operation */
  D(bug("OpenGLClearRenderPort: RenderPort %p - stub implementation\n", rp));
}

/*****************************************************************************/
/* Direct Pixel Access */
/*****************************************************************************/

static APTR OpenGLLockPixels(struct DrawingBoard *board, ULONG *pitch_out) {
  D(bug("OpenGLLockPixels: Not supported in OpenGL stub\n"));
  if (pitch_out)
    *pitch_out = 0;
  return NULL;
}

static void OpenGLUnlockPixels(struct DrawingBoard *board) {
  /* Nothing to unlock */
}

static ULONG OpenGLGetPixel(struct DrawingBoard *board, WORD x, WORD y) {
  /* Return black as default */
  return 0x00000000;
}

static void OpenGLSetPixel(struct DrawingBoard *board, WORD x, WORD y,
                           struct InternalColor *color) {
  /* Stub - no operation */
}

/*****************************************************************************/
/* Batching Operations */
/*****************************************************************************/

static void OpenGLBeginBatch(struct RenderPort *rp) {
  /* Stub - no operation */
  D(bug("OpenGLBeginBatch: RenderPort %p - stub implementation\n", rp));
}

static void OpenGLEndBatch(struct RenderPort *rp) {
  /* Stub - no operation */
  D(bug("OpenGLEndBatch: RenderPort %p - stub implementation\n", rp));
}

static void OpenGLFlushBatch(struct RenderPort *rp) {
  /* Stub - no operation */
}

static BOOL OpenGLIsBatching(struct RenderPort *rp) {
  return FALSE; /* Never batching in stub */
}

/*****************************************************************************/
/* Blitting Operations */
/*****************************************************************************/

static void OpenGLBlitRenderPorts(struct RenderPort *source,
                                  struct RenderPort *dest, WORD src_x,
                                  WORD src_y, WORD dest_x, WORD dest_y,
                                  UWORD width, UWORD height) {
  /* Stub - no operation */
  D(bug("OpenGLBlitRenderPorts: %dx%d - stub implementation\n", width, height));
}

static void OpenGLBlitToScreen(struct RenderPort *source,
                               struct RastPort *screen_rp, WORD src_x,
                               WORD src_y, WORD dest_x, WORD dest_y,
                               UWORD width, UWORD height) {
  /* Stub - no operation */
  D(bug("OpenGLBlitToScreen: %dx%d - stub implementation\n", width, height));
}

/*****************************************************************************/
/* Backend Information */
/*****************************************************************************/

void OpenGLDumpDebugInfo(void) {
  D(bug("=== OpenGL Backend Debug Info (STUB) ===\n"));
  D(bug("  Status: Not Available\n"));
  D(bug("  OpenGL Implementation: None\n"));
  D(bug("  Capabilities: 0x%08x\n", OpenGLGetCapabilities()));
  D(bug("  Note: OpenGL support not yet implemented in AROS\n"));
  D(bug("=== End OpenGL Debug Info ===\n"));
}

/*****************************************************************************/
/* DrawingBoard Management */
/*****************************************************************************/

static BOOL OpenGLInitDrawingBoard(struct DrawingBoard *board) {
  if (!board) {
    return FALSE;
  }

  D(bug("OpenGLInitDrawingBoard: Initializing DrawingBoard %p with OpenGL "
        "(stub)\n",
        board));

  return TRUE;
}

static void OpenGLCleanupDrawingBoard(struct DrawingBoard *board) {
  D(bug("OpenGLCleanupDrawingBoard: Cleaning up DrawingBoard %p with OpenGL "
        "(stub)\n",
        board));
}
