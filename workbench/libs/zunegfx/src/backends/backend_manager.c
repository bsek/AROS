/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Backend Management Implementation (New Interface)

    This file implements the unified backend management system for ZuneRenderer
    using the new ZuneBackendOps interface. It handles backend registration,
    detection, selection, and provides the interface between the public API
    and the specific backend implementations.
*/

#include <exec/lists.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/screens.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "../zunegfx_intern.h"
#include "backend_interface.h"

#define DEBUG 0
#include <aros/debug.h>

/*****************************************************************************/
/* Global Backend Management */
/*****************************************************************************/

struct List ZuneBackendList;
struct SignalSemaphore ZuneBackendSemaphore;

/* Cached result for ZuneFindBestBackend(NULL) - doesn't change after init */
static ZuneBackend *g_default_backend_cached = NULL;
static BOOL g_default_backend_checked = FALSE;

/*****************************************************************************/
/* Backend Registration Functions */
/*****************************************************************************/

BOOL ZuneRegisterBackend(ZuneBackend *backend) {
  if (!backend || !backend->ops) {
    D(bug("ZuneRegisterBackend: Invalid backend parameter\n"));
    return FALSE;
  }

  D(bug("ZuneRegisterBackend: Registering backend %s (type %u)\n",
        (backend->ops->name ? (const char *)backend->ops->name : "Unknown"),
        backend->ops->type));

  ObtainSemaphore(&ZuneBackendSemaphore);

  /* Initialize backend context if needed */
  if (!backend->context) {
    backend->context = AllocVec(sizeof(ZuneBackendContext), MEMF_CLEAR);
    if (!backend->context) {
      ReleaseSemaphore(&ZuneBackendSemaphore);
      D(bug("ZuneRegisterBackend: Failed to allocate backend context\n"));
      return FALSE;
    }

    backend->context->type = backend->ops->type;
    backend->context->name = backend->ops->name;
    backend->context->capabilities =
        backend->ops->GetCapabilities ? backend->ops->GetCapabilities() : 0;
    backend->context->initialized = FALSE;
    backend->context->ref_count = 0;
  }

  /* Check if backend is available */
  backend->available =
      backend->ops->IsAvailable ? backend->ops->IsAvailable() : FALSE;

  /* Initialize backend if available */
  if (backend->available && backend->ops->InitBackend) {
    if (backend->ops->InitBackend(backend->context)) {
      backend->context->initialized = TRUE;
      D(bug("ZuneRegisterBackend: Backend %s initialized successfully\n",
            (const char *)backend->ops->name));
    } else {
      D(bug("ZuneRegisterBackend: Backend %s initialization failed\n",
            (const char *)backend->ops->name));
      backend->available = FALSE;
    }
  }

  /* Set backend name for node */
  backend->node.ln_Name = (char *)backend->ops->name;

  /* Add to backend list (sorted by priority) */
  ZuneBackend *current_backend;
  BOOL inserted = FALSE;

  ForeachNode(&ZuneBackendList, current_backend) {
    if (backend->priority > current_backend->priority) {
      Insert(&ZuneBackendList, &backend->node, current_backend->node.ln_Pred);
      inserted = TRUE;
      break;
    }
  }

  if (!inserted) {
    AddTail(&ZuneBackendList, &backend->node);
  }

  ReleaseSemaphore(&ZuneBackendSemaphore);

  D(bug("ZuneRegisterBackend: Backend %s registered successfully (%s)\n",
        (const char *)backend->ops->name,
        backend->available ? "available" : "not available"));

  return TRUE;
}

void ZuneUnregisterBackend(ZuneBackend *backend) {
  if (!backend)
    return;

  D(bug("ZuneUnregisterBackend: Unregistering backend %s\n",
        backend->ops && backend->ops->name ? (const char *)backend->ops->name
                                           : "Unknown"));

  ObtainSemaphore(&ZuneBackendSemaphore);

  /* Remove from list */
  Remove(&backend->node);

  /* Cleanup backend */
  if (backend->context && backend->context->initialized && backend->ops &&
      backend->ops->CleanupBackend) {
    backend->ops->CleanupBackend(backend->context);
  }

  /* Free context if we allocated it */
  if (backend->context && backend->context->ref_count == 0) {
    FreeVec(backend->context);
    backend->context = NULL;
  }

  ReleaseSemaphore(&ZuneBackendSemaphore);
}

/*****************************************************************************/
/* Backend Discovery Functions */
/*****************************************************************************/

ZuneBackend *ZuneFindBestBackend(struct RenderPort *rp) {
  ZuneBackend *best_backend = NULL;
  ZuneBackend *current_backend;

  /*
   * Fast path: Return cached result for NULL rp.
   * This is called very frequently from SelectTextureBackend and others.
   * The result doesn't change after initialization.
   */
  if (!rp && g_default_backend_checked) {
    return g_default_backend_cached;
  }

  D(bug("ZuneFindBestBackend: Finding best backend for RenderPort %p\n", rp));

  ObtainSemaphoreShared(&ZuneBackendSemaphore);

  /* List is already sorted by priority, so first available and compatible
   * backend is best */
  ForeachNode(&ZuneBackendList, current_backend) {
    if (current_backend->available) {
      /* Check backend-specific compatibility */
      if (current_backend->ops->IsCompatible) {
        if (!current_backend->ops->IsCompatible(rp)) {
          D(bug("ZuneFindBestBackend: Backend %s not compatible, skipping\n",
                current_backend->ops->name));
          continue;
        }
      }

      best_backend = current_backend;
      break;
    }
  }

  ReleaseSemaphore(&ZuneBackendSemaphore);

  /* Cache result for NULL rp */
  if (!rp && !g_default_backend_checked) {
    g_default_backend_cached = best_backend;
    g_default_backend_checked = TRUE;
  }

  D(bug("ZuneFindBestBackend: Selected backend %s\n",
        best_backend && best_backend->ops
            ? (const char *)best_backend->ops->name
            : "None"));

  return best_backend;
}

ZuneBackend *ZuneFindBackendByType(ZuneBackendType type) {
  ZuneBackend *backend;

  D(bug("ZuneFindBackendByType: Looking for type %d\n", type));

  ObtainSemaphoreShared(&ZuneBackendSemaphore);

  ForeachNode(&ZuneBackendList, backend) {
    D(bug("ZuneFindBackendByType: Checking backend %s (type %d, available=%d)\n",
          backend->ops ? backend->ops->name : "NULL", 
          backend->ops ? backend->ops->type : -1,
          backend->available));
          
    if (backend->ops && backend->ops->type == type) {
      /* 
       * Re-check availability - the backend might have become available
       * after initial registration (e.g., GL library opened later).
       */
      if (!backend->available && backend->ops->IsAvailable) {
        D(bug("ZuneFindBackendByType: Re-checking availability for %s\n", backend->ops->name));
        backend->available = backend->ops->IsAvailable();
        D(bug("ZuneFindBackendByType: IsAvailable returned %d\n", backend->available));
        if (backend->available && !backend->context->initialized && backend->ops->InitBackend) {
          D(bug("ZuneFindBackendByType: Initializing backend %s\n", backend->ops->name));
          backend->ops->InitBackend(backend->context);
          backend->context->initialized = TRUE;
        }
      }
      
      if (backend->available) {
        D(bug("ZuneFindBackendByType: Found available backend %s\n", backend->ops->name));
        ReleaseSemaphore(&ZuneBackendSemaphore);
        return backend;
      } else {
        D(bug("ZuneFindBackendByType: Backend %s not available\n", backend->ops->name));
      }
    }
  }

  D(bug("ZuneFindBackendByType: No backend found for type %d\n", type));
  ReleaseSemaphore(&ZuneBackendSemaphore);
  return NULL;
}

/*****************************************************************************/
/* Backend System Lifecycle */
/*****************************************************************************/

BOOL ZuneInitBackends(void) {
  D(bug("ZuneInitBackends: Initializing backend system\n"));

  /* Initialize global structures */
  NEWLIST(&ZuneBackendList);
  InitSemaphore(&ZuneBackendSemaphore);

  /* Backend registration will happen via external backend modules
     calling ZuneRegisterBackend() during their initialization */

  D(bug("ZuneInitBackends: Backend system initialized\n"));
  return TRUE;
}

void ZuneCleanupBackends(void) {
  ZuneBackend *backend, *next_backend;

  D(bug("ZuneCleanupBackends: Cleaning up backend system\n"));

  ObtainSemaphore(&ZuneBackendSemaphore);

  /* Cleanup and remove all backends */
  ForeachNodeSafe(&ZuneBackendList, backend, next_backend) {
    if (backend->context && backend->context->initialized && backend->ops &&
        backend->ops->CleanupBackend) {
      backend->ops->CleanupBackend(backend->context);
    }
    Remove(&backend->node);
  }

  ReleaseSemaphore(&ZuneBackendSemaphore);

  D(bug("ZuneCleanupBackends: Backend system cleaned up\n"));
}

/*****************************************************************************/
/* Backend Information Functions */
/*****************************************************************************/

ULONG ZuneGetBackendCapabilities(ZuneBackend *backend) {
  if (!backend || !backend->ops || !backend->ops->GetCapabilities) {
    return 0;
  }

  return backend->ops->GetCapabilities();
}

CONST_STRPTR ZuneGetBackendName(ZuneBackend *backend) {
  if (!backend || !backend->ops) {
    return "Unknown";
  }

  return backend->ops->name ? (const char *)backend->ops->name
                            : "Unnamed Backend";
}

BOOL ZuneIsBackendAvailable(ZuneBackendType type) {
  ZuneBackend *backend = ZuneFindBackendByType(type);
  return backend != NULL;
}

/*****************************************************************************/
/* RenderPort-Backend Binding */
/*****************************************************************************/

ZuneBackend *ZuneGetRenderPortBackend(struct RenderPort *rp) {
  if (!rp || !rp->backend_vtable) {
    return NULL;
  }

  /* The backend is stored in the RenderPort's backend_vtable field */
  return (ZuneBackend *)rp->backend_vtable;
}

BOOL ZuneBindRenderPortToBackend(struct RenderPort *rp, ZuneBackend *backend) {
  if (!rp || !backend || !backend->available) {
    D(bug("ZuneBindRenderPortToBackend: Invalid parameters\n"));
    return FALSE;
  }

  D(bug("ZuneBindRenderPortToBackend: Binding RenderPort %p to backend %s\n",
        rp, backend->ops->name));

  /* Unbind from current backend first */
  if (rp->backend_context || rp->backend_vtable) {
    ZuneUnbindRenderPortFromBackend(rp);
  }

  /* Store backend information in RenderPort BEFORE calling InitRenderPort */
  rp->backend_type = backend->ops->type;
  rp->backend_vtable = backend; /* Store the whole backend structure */

  /*
   * Note: We do NOT set rp->backend_context here!
   * The backend's InitRenderPort() is responsible for setting up
   * rp->backend_context with backend-specific per-RenderPort data.
   *
   * For example, the OpenGL backend needs to store per-RenderPort GL
   * contexts, not the global backend context. Setting backend_context
   * here would overwrite what the backend sets up.
   */

  /* Initialize RenderPort with backend */
  if (backend->ops->InitRenderPort && !backend->ops->InitRenderPort(rp)) {
    D(bug("ZuneBindRenderPortToBackend: Backend InitRenderPort failed\n"));
    rp->backend_type = BACKEND_SOFTWARE;
    rp->backend_vtable = NULL;
    return FALSE;
  }

  /* Increment reference count */
  if (backend->context) {
    backend->context->ref_count++;
  }

  D(bug("ZuneBindRenderPortToBackend: RenderPort bound to backend %s "
        "successfully\n",
        backend->ops->name));

  return TRUE;
}

void ZuneUnbindRenderPortFromBackend(struct RenderPort *rp) {
  if (!rp)
    return;

  ZuneBackend *backend = ZuneGetRenderPortBackend(rp);
  if (!backend)
    return;

  D(bug("ZuneUnbindRenderPortFromBackend: Unbinding RenderPort %p from backend "
        "%s\n",
        rp, backend->ops->name));

  /* Cleanup RenderPort with backend */
  if (backend->ops->CleanupRenderPort) {
    backend->ops->CleanupRenderPort(rp);
  }

  /* Decrement reference count */
  if (backend->context && backend->context->ref_count > 0) {
    backend->context->ref_count--;
  }

  /* Clear RenderPort backend information */
  rp->backend_context = NULL;
  rp->backend_vtable = NULL;
  rp->backend_type = BACKEND_SOFTWARE;
}

/*****************************************************************************/
/* DrawingBoard Backend Management Helper Functions */
/*****************************************************************************/

BOOL InitializeDrawingBoardBackend(struct DrawingBoard *board,
                                   ZuneBackend *backend) {
  if (!board)
    return FALSE;

  D(bug("InitializeDrawingBoardBackend: Initializing backend %s for "
        "DrawingBoard %p\n",
        backend->context->name, board));

  /* Initialize backend-specific DrawingBoard data */
  if (backend->ops->InitDrawingBoard) {
    return backend->ops->InitDrawingBoard(board);
  }

  return TRUE;
}

void CleanupDrawingBoardBackend(struct DrawingBoard *board,
                                struct ZuneBackend *backend) {
  if (!board)
    return;

  D(bug("CleanupDrawingBoardBackend: Cleaning up backend for DrawingBoard %p\n",
        board));

  if (backend && backend->ops->CleanupDrawingBoard) {
    backend->ops->CleanupDrawingBoard(board);
  }
}

/*****************************************************************************/
/* Backend Registration Functions for Individual Backends */
/*****************************************************************************/

/* These will be called by the individual backend implementations */
extern ZuneBackendOps cybergfx_backend_ops;
extern ZuneBackendOps opengl_backend_ops;

/* Global backend structures */
static ZuneBackend cybergfx_backend = {0};
static ZuneBackend opengl_backend = {0};

BOOL RegisterCyberGfxBackend(void) {
  D(bug("RegisterCyberGfxBackend: Registering CyberGraphics backend\n"));

  cybergfx_backend.ops = &cybergfx_backend_ops;
  cybergfx_backend.priority = 50; /* Medium priority */

  return ZuneRegisterBackend(&cybergfx_backend);
}

BOOL RegisterOpenGLBackend(void) {
  D(bug("RegisterOpenGLBackend: Registering OpenGL backend\n"));

  opengl_backend.ops = &opengl_backend_ops;
  opengl_backend.priority = 100; /* Highest priority */

  return ZuneRegisterBackend(&opengl_backend);
}
