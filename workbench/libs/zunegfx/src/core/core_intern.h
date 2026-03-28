/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - Core Internal Header
*/

#ifndef ZUNEGFX_CORE_INTERN_H
#define ZUNEGFX_CORE_INTERN_H

#include <exec/types.h>

/* Forward declarations */
struct IntZuneGfxBase;
struct RenderContext;
struct Window;
struct ColorMap;

/* Internal core functions */
BOOL DetectLibraries(void);
BOOL InitializeZuneGfx(struct IntZuneGfxBase *base);
void CleanupZuneGfx(struct IntZuneGfxBase *base);

void InitializeRenderContext(struct RenderContext *rctx);
void CleanupRenderContext(struct RenderContext *rctx);

void AddRenderContextToList(struct IntZuneGfxBase *base, struct RenderContext *rctx);
void RemoveRenderContextFromList(struct IntZuneGfxBase *base, struct RenderContext *rctx);

struct RenderContext *CreateRenderContextForWindowInternal(
    struct IntZuneGfxBase *base,
    struct Window *window,
    struct ColorMap *colormap,
    UWORD backend_type);

struct RenderContext *CreateRenderContextInternal(
    struct IntZuneGfxBase *base,
    struct ColorMap *colormap,
    struct RastPort *rastport);

void DestroyRenderContextInternal(struct IntZuneGfxBase *base,
                                  struct RenderContext *rctx);

#endif /* ZUNEGFX_CORE_INTERN_H */
