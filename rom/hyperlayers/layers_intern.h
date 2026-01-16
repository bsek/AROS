/*
    Copyright (C) 1995-2011, The AROS Development Team. All rights reserved.

    Desc: Internal information for layers.library.
*/
#ifndef _LAYERS_INTERN_H_
#define _LAYERS_INTERN_H_

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/libraries.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <utility/utility.h>
#include <setjmp.h>
#include <dos/dos.h>   /* BPTR below */
#include <proto/alib.h> /* We redefine NewRectRegion() */

#include LC_LIBDEFS_FILE

#include "intregions.h"

LIBBASETYPE
{
    struct Library   	    lb_LibNode;

    APTR    	    	    lb_ClipRectPool;

    struct GfxBase *        lb_GfxBase;
    struct UtilityBase *    lb_UtilityBase;
};

/* Store library bases in our base, not in .bss. Why not ? */
#define GfxBase     (LIBBASE->lb_GfxBase)
#define UtilityBase (LIBBASE->lb_UtilityBase)

struct IntLayer
{
    struct Layer    lay;
    struct Hook	   *shapehook;
    IPTR	    window;	/* This is passed to shape hook. Comes from Intuition. */
    ULONG	    intflags;
    struct RastPort rp;		/* lay.rp */

    /* Compositor support */
    UBYTE           il_Alpha;          /* Global alpha 0-255, 255=opaque (default) */
    UBYTE           il_AlphaFlags;     /* LAYERF_ALPHA, LAYERF_NOSHADOW, etc. */
    UBYTE           il_Pad[2];         /* Padding for alignment */
    APTR            il_CompositorData; /* Compositor-private per-layer data */
};

#define IL(x) ((struct IntLayer *)(x))

/* Access LayerInfo_extra from Layer_Info */
#define LIE(li) ((struct LayerInfo_extra *)((li)->LayerInfo_extra))

/* Alpha flags for il_AlphaFlags */
#define ILAF_ALPHA           (1 << 0)  /* Layer has per-pixel alpha or global alpha < 255 */
#define ILAF_NOSHADOW        (1 << 1)  /* Compositor should not draw shadow for this layer */
#define ILAF_COMPOSITEDIRTY  (1 << 2)  /* Layer content changed, needs recomposite */

/* Standard layer priorities */
#define ROOTPRIORITY		0
#define BACKDROPPRIORITY	10
#define UPFRONTPRIORITY		20

#define INTFLAG_AVOID_BACKFILL 1

struct LayerInfo_extra
{
#if 0
    ULONG          lie_ReturnAddr;     // used by setjmp/longjmp, equals jmp_buf
    ULONG          lie_Regs[12];       // D2-D7/A2-SP
#else
    jmp_buf        lie_JumpBuf;
#endif
    struct MinList lie_ResourceList;
    UBYTE          lie_pad[4];

    /* Compositor support */
    struct Hook   *lie_CompositorHook; /* External compositor callback, NULL if none */
    APTR           lie_CompositorData; /* Compositor-private data for this Layer_Info */
};

/* Compositor hook message */
struct CompositorMsg
{
    ULONG           cm_Method;   /* COMP_SHOWLAYER, COMP_HIDELAYER, etc. */
    struct Layer   *cm_Layer;    /* Layer being affected */
    struct Region  *cm_Region;   /* Region affected (may be NULL) */
    APTR            cm_Reserved[4]; /* Reserved for future use */
};

/* Compositor hook methods */
#define COMP_SHOWLAYER      1    /* Layer (part) becoming visible */
#define COMP_HIDELAYER      2    /* Layer (part) becoming hidden */
#define COMP_MOVELAYER      3    /* Layer moved or resized */
#define COMP_DIRTYLAYER     4    /* Layer content changed */
#define COMP_CREATELAYER    5    /* New layer created */
#define COMP_DELETELAYER    6    /* Layer being deleted */

/*
 * These are special types of ResData resources. If layers finds one of
 * these values in ResData->Size, it performs some special handling to
 * properly dispose of the allocated Region or BitMap, respectively
 * (throught DisposeRegion or FreeBitMap). In all other cases,
 * ResData->Size is an argument for a freemem operation.
 */
#define RD_REGION -1
#define RD_BITMAP -2

struct ResData
{
    void *ptr;
    ULONG Size;
};

struct ResourceNode
{
    struct Node	    rn_Link;
    struct ResData *rn_FirstFree;
    LONG            rn_FreeCnt;
    struct ResData  rn_Data[48];
};


/*
** The smart refresh flag is set for super bitmap as well as smart refresh
** layers 
*/
#define IS_SIMPLEREFRESH(l) (0 != ((l)->Flags & LAYERSIMPLE))
#define IS_SMARTREFRESH(l)  (LAYERSMART == ((l)->Flags & (LAYERSMART|LAYERSUPER)))
#define IS_SUPERREFRESH(l)  (0 != ((l)->Flags & LAYERSUPER))

int _MoveLayerBehind(struct Layer *l,
                     struct Layer *lfront,
                     LIBBASETYPEPTR LayersBase);
int _MoveLayerToFront(struct Layer * l,
                      struct Layer * lbehind,
                      LIBBASETYPEPTR LayersBase);

#define NewRectRegion(MinX, MinY, MaxX, MaxY) _NewRectRegion(MinX, MinY, MaxX, MaxY, LIBBASE->lb_GfxBase)

#endif /* _LAYERS_INTERN_H */
