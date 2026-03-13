/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - ZuneDrawTextureTiled
*/

#include "../backends/backend_interface.h"
#include "../../include/zunegfx.h"
#include <aros/libcall.h>
#define DEBUG 0
#include <aros/debug.h>
#include <cybergraphx/cybergraphics.h>
#include <exec/types.h>

#include "../zunegfx_intern.h"
#include "../backends/cybergfx/cybergfx_backend.h"
#include "texture_intern.h"

AROS_LH3(void, ZuneDrawTextureTiled,
         AROS_LHA(struct RenderContext *, rctx, A0),
         AROS_LHA(struct ZuneTexture *, texture, A1),
         AROS_LHA(struct ZuneRect *, dest_rect, A2),
         struct Library *, ZuneGfxBase, 89, zunegfx)
{
    AROS_LIBFUNC_INIT

    ENTER_FUNCTION("ZuneDrawTextureTiled");

    D(bug("ZuneRenderer: ZuneDrawTextureTiled(rctx=%p, texture=%p, dest_rect=%p)\n", rctx, texture, dest_rect));

    if (!ValidateRenderContext(rctx) || !texture || !dest_rect) {
        D(bug("ZuneRenderer: Invalid parameters\n"));
        return;
    }

    if (!ValidateTexture(texture)) {
        D(bug("ZuneRenderer: Invalid texture\n"));
        return;
    }

    UWORD texture_width = texture->width;
    UWORD texture_height = texture->height;

    if (texture_width == 0 || texture_height == 0) {
        D(bug("ZuneRenderer: Texture has zero dimensions\n"));
        return;
    }

    WORD dest_x = dest_rect->x;
    WORD dest_y = dest_rect->y;
    UWORD dest_width = dest_rect->width;
    UWORD dest_height = dest_rect->height;

    /*
     * FAST PATH: Try optimized tiled rendering for ARGB32 textures.
     * 
     * CybergfxDrawTextureTiledFast uses row-by-row WritePixelArray calls
     * which is significantly faster than drawing individual tiles.
     * This matches the performance characteristics of the legacy
     * dt_put_on_rastport_tiled() function.
     */
    if (CybergfxDrawTextureTiledFast(rctx, texture, dest_x, dest_y, 
                                     dest_width, dest_height)) {
        /* Fast path succeeded */
        D(bug("ZuneRenderer: Used fast tiled rendering path\n"));
        EXIT_FUNCTION("ZuneDrawTextureTiled");
        return;
    }

    /*
     * SLOW PATH: Fall back to drawing individual tiles.
     * Used when fast path is unavailable (non-ARGB32, DrawingBoard target, etc.)
     */
    D(bug("ZuneRenderer: Using slow tiled rendering path\n"));

    /* Calculate how many complete tiles we need */
    UWORD tiles_x = (dest_width + texture_width - 1) / texture_width;
    UWORD tiles_y = (dest_height + texture_height - 1) / texture_height;

    D(bug("ZuneRenderer: Tiling %dx%d texture across %dx%d area (%d x %d tiles)\n", texture_width, texture_height, dest_width, dest_height, tiles_x,
          tiles_y));

    /* Draw tiles row by row */
    UWORD ty, tx;
    for (ty = 0; ty < tiles_y; ty++) {
        for (tx = 0; tx < tiles_x; tx++) {
            WORD tile_x = dest_x + (tx * texture_width);
            WORD tile_y = dest_y + (ty * texture_height);

            /* Calculate the actual size of this tile (may be clipped at edges) */
            UWORD tile_width = texture_width;
            UWORD tile_height = texture_height;

            /* Clip tile dimensions if we're at the edge */
            if (tile_x + tile_width > dest_x + dest_width) {
                tile_width = (dest_x + dest_width) - tile_x;
            }
            if (tile_y + tile_height > dest_y + dest_height) {
                tile_height = (dest_y + dest_height) - tile_y;
            }

            /* Only draw if the tile has positive dimensions */
            if (tile_width > 0 && tile_height > 0) {
                /* Use backend to draw the tile (may be clipped) */
                ZUNE_BACKEND_CALL(rctx, DrawTexture, texture, tile_x, tile_y, tile_width, tile_height, 0, 0, tile_width, tile_height, NULL);
            }
        }
    }

    EXIT_FUNCTION("ZuneDrawTextureTiled");

    AROS_LIBFUNC_EXIT
}
