/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - DrawingBoard Internal Header
*/

#ifndef ZUNEGFX_DRAWINGBOARD_INTERN_H
#define ZUNEGFX_DRAWINGBOARD_INTERN_H

#include <exec/types.h>
#include <graphics/gfx.h>

/* Forward declarations */
struct IntZuneGfxBase;
struct RenderContext;
struct DrawingBoard;
struct RastPort;

/* Internal DrawingBoard functions */
void SetPixelInternal(struct RenderContext *rctx, WORD x, WORD y, ULONG color);
ULONG GetPixelInternal(struct RenderContext *rctx, WORD x, WORD y);

BOOL AllocateDrawingBoardBitmap(struct DrawingBoard *board,
                                UWORD backend_type,
                                struct BitMap *friend_bitmap);
void FreeDrawingBoardBitmap(struct DrawingBoard *board);
void InitDrawingBoard(struct DrawingBoard *board);
void CleanupDrawingBoard(struct RenderContext *rctx, struct DrawingBoard *board);

APTR LockDrawingBoardPixelsInternal(struct RenderContext *rctx, ULONG *pitch);
void UnlockDrawingBoardPixelsInternal(struct RenderContext *rctx);

void AddDrawingBoardToList(struct IntZuneGfxBase *base,
                           struct DrawingBoard *board);
void RemoveDrawingBoardFromList(struct IntZuneGfxBase *base,
                                struct DrawingBoard *board);

struct DrawingBoard *CreateDrawingBoardForRenderContextInternal(
    struct IntZuneGfxBase *base,
    struct RenderContext *rctx,
    UWORD width, UWORD height, ULONG flags);

BOOL ValidateDrawingBoard(struct DrawingBoard *board);
ULONG GetDrawingBoardPixelFormat(struct DrawingBoard *board);
ULONG GetBytesPerPixel(ULONG pixel_format);
BOOL IsPixelFormatSupported(ULONG pixel_format);
void DumpDrawingBoard(struct DrawingBoard *board);

void BlitDrawingBoardInternal(struct DrawingBoard *src,
                              struct DrawingBoard *dst, WORD src_x, WORD src_y,
                              WORD dst_x, WORD dst_y, UWORD width,
                              UWORD height);
void FastBlitInternal(struct DrawingBoard *src, struct DrawingBoard *dst,
                      WORD src_x, WORD src_y, WORD dst_x, WORD dst_y,
                      UWORD width, UWORD height);

#endif /* ZUNEGFX_DRAWINGBOARD_INTERN_H */
