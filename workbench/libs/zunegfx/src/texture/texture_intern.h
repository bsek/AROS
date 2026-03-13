/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - Texture Internal Header
*/

#ifndef ZUNEGFX_TEXTURE_INTERN_H
#define ZUNEGFX_TEXTURE_INTERN_H

#include <exec/types.h>
#include "../backends/backend_interface.h"

/* Forward declarations */
struct IntZuneGfxBase;
struct RenderContext;
struct ZuneTexture;
struct DrawingBoard;
struct ZuneRect;
struct Screen;
struct BitMap;

/* Texture allocation and initialization */
struct ZuneTexture *AllocateTexture(void);
void InitializeTexture(struct ZuneTexture *texture, UWORD width, UWORD height,
                       UBYTE depth, ULONG format, ULONG flags);

/* Format helpers */
ULONG ConvertCyberGfxFormatToZuneFormat(ULONG cybergfx_format);
ULONG GetTextureFormatBPP(ULONG format);

/* List management */
void AddTextureToList(struct IntZuneGfxBase *base, struct ZuneTexture *texture);
void RemoveTextureFromList(struct IntZuneGfxBase *base, struct ZuneTexture *texture);

/* Data management */
BOOL AllocateTextureData(struct ZuneTexture *texture);
void FreeTextureData(struct ZuneTexture *texture);

ULONG CalculateTexturePitch(UWORD width, ULONG format);
ULONG CalculateTextureSize(UWORD width, UWORD height, ULONG format);

/* Backend selection */
ZuneBackend *GetTextureBackend(struct RenderContext *rctx, struct ZuneTexture *texture);

/* Validation */
BOOL ValidateTexture(struct ZuneTexture *texture);
void UnlockTexturePixelsInternal(struct ZuneTexture *texture);

/* Internal creation functions */
struct ZuneTexture *CreateTextureFromDataInternal(APTR data, UWORD width,
    UWORD height, UBYTE depth, ULONG format, ULONG pitch, ULONG flags);

struct ZuneTexture *CreateTextureFromDatatypeInternal(APTR dt_handle, ULONG flags);

struct ZuneTexture *CreateTextureFromDrawingBoardInternal(
    struct IntZuneGfxBase *base, struct RenderContext *rctx, ULONG flags);

#endif /* ZUNEGFX_TEXTURE_INTERN_H */
