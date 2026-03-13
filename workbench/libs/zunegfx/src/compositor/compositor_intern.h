/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Compositor Internal Header

    Forward declarations of internal functions from layer_compositor.c
*/

#ifndef ZUNEGFX_COMPOSITOR_INTERN_H
#define ZUNEGFX_COMPOSITOR_INTERN_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>

/* Forward declarations */
struct LayerCompositor;
struct CompositorWindow;
struct DrawingBoard;

/* Internal functions implemented in layer_compositor.c */
struct LayerCompositor *CreateLayerCompositorInternal(struct Screen *screen);
struct LayerCompositor *CreateLayerCompositorSharedInternal(struct Screen *screen,
                                                            APTR masterGLContext);
void DestroyLayerCompositorInternal(struct LayerCompositor *comp);
BOOL ActivateLayerCompositorInternal(struct LayerCompositor *comp);
void DeactivateLayerCompositorInternal(struct LayerCompositor *comp);
struct CompositorWindow *CompositorRegisterWindowInternal(
    struct LayerCompositor *comp,
    struct Window *window,
    APTR glContext,
    struct DrawingBoard *board,
    UBYTE alpha);
void CompositorUnregisterWindowInternal(struct LayerCompositor *comp,
                                        struct Window *window);
void CompositorSetWindowAlphaInternal(struct LayerCompositor *comp,
                                      struct Window *window,
                                      UBYTE alpha);
void CompositorMarkWindowDirtyInternal(struct LayerCompositor *comp,
                                       struct Window *window);
struct CompositorWindow *CompositorFindWindowInternal(struct LayerCompositor *comp,
                                                      struct Window *window);
void CompositorUpdateInternal(struct LayerCompositor *comp);
void CompositorRefreshInternal(struct LayerCompositor *comp);
void CompositorSetShadowInternal(struct LayerCompositor *comp,
                                 WORD offsetX, WORD offsetY,
                                 UWORD blur, UBYTE alpha);

#endif /* ZUNEGFX_COMPOSITOR_INTERN_H */
