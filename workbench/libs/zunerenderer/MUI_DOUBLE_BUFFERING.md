# MUI Double Buffering Implementation Guide

## Overview

This document describes how to implement efficient double buffering for MUI windows using the ZuneRenderer library. Double buffering eliminates flicker and provides smooth updates by rendering to an off-screen buffer before copying the final result to the visible window.

## Current Architecture Analysis

Based on the existing MUI code, you already have the foundation:

```c
struct MUI_RenderInfo {
    // Existing fields...
    struct BitMap *mri_BufferBM;        // Off-screen bitmap
    struct RastPort mri_BufferRP;       // RastPort for off-screen buffer
    struct RenderPort *mri_RenderPort;  // ZuneRenderer RenderPort (new)
    // ... other fields
};
```

## Double Buffering Strategy

### 1. Buffer Management

#### Buffer Creation
Location: `InstallBackbuffer()` in `window.c`

```c
void InstallBackbuffer(struct IClass *cl, Object *obj)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    struct Window *win = data->wd_RenderInfo.mri_Window;
    
    if (!win) return;
    
    // Create off-screen bitmap matching window dimensions
    data->wd_RenderInfo.mri_BufferBM = AllocBitMap(
        win->GZZWidth, 
        win->GZZHeight, 
        GetBitMapAttr(win->RPort->BitMap, BMA_DEPTH),
        BMF_DISPLAYABLE | BMF_CLEAR,
        win->RPort->BitMap
    );
    
    if (data->wd_RenderInfo.mri_BufferBM) {
        // Initialize off-screen RastPort
        InitRastPort(&data->wd_RenderInfo.mri_BufferRP);
        data->wd_RenderInfo.mri_BufferRP.BitMap = data->wd_RenderInfo.mri_BufferBM;
        
        // Create ZuneRenderer RenderPort for off-screen buffer
        if (data->wd_RenderInfo.mri_RenderPort) {
            DestroyRenderPort(data->wd_RenderInfo.mri_RenderPort);
        }
        data->wd_RenderInfo.mri_RenderPort = CreateRenderPortWithDrawingBoard(
            data->wd_RenderInfo.mri_Colormap,
            win->GZZWidth,
            win->GZZHeight,
            GetBitMapAttr(win->RPort->BitMap, BMA_DEPTH)
        );
        
        D(bug("Double buffer installed: %dx%dx%d\n", 
              win->GZZWidth, win->GZZHeight, 
              GetBitMapAttr(win->RPort->BitMap, BMA_DEPTH)));
    }
}
```

#### Buffer Cleanup
Location: `DeinstallBackbuffer()` in `window.c`

```c
void DeinstallBackbuffer(struct IClass *cl, Object *obj)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    struct MUI_RenderInfo *mri = &data->wd_RenderInfo;
    
    // Cleanup ZuneRenderer RenderPort
    if (mri->mri_RenderPort) {
        DestroyRenderPort(mri->mri_RenderPort);
        mri->mri_RenderPort = NULL;
    }
    
    // Cleanup traditional double buffer
    if (mri->mri_BufferBM) {
        DeinitRastPort(&mri->mri_BufferRP);
        FreeBitMap(mri->mri_BufferBM);
        mri->mri_BufferBM = NULL;
    }
}
```

### 2. Render Target Selection

#### Enhanced ShowRenderInfo
Location: `ShowRenderInfo()` in `window.c`

```c
static void ShowRenderInfo(struct MUI_RenderInfo *mri)
{
    if (mri->mri_BufferBM) {
        // Double buffering active - render to off-screen buffer
        mri->mri_RastPort = &mri->mri_BufferRP;
        
        // Use ZuneRenderer's DrawingBoard for modern rendering
        if (mri->mri_RenderPort && mri->mri_RenderPort->target_board) {
            // Modern path: render to DrawingBoard
            D(bug("Using ZuneRenderer DrawingBoard for double buffering\n"));
        } else {
            // Fallback: render to traditional off-screen bitmap
            D(bug("Using traditional off-screen bitmap for double buffering\n"));
        }
    } else {
        // Direct rendering to window
        mri->mri_RastPort = mri->mri_Window->RPort;
        
        // Create simple RenderPort for window
        if (mri->mri_RenderPort) {
            DestroyRenderPort(mri->mri_RenderPort);
        }
        mri->mri_RenderPort = CreateRenderPort(mri->mri_Colormap, mri->mri_RastPort);
    }
}
```

### 3. Enhanced Refresh System

#### Modified RefreshWindow with Double Buffering
Location: `RefreshWindow()` in `window.c`

```c
static VOID RefreshWindow(Object *oWin, struct MUI_WindowData *data)
{
    struct MUI_RenderInfo *mri = &data->wd_RenderInfo;
    BOOL using_double_buffer = (mri->mri_BufferBM != NULL);
    
    if (data->wd_Flags & MUIWF_RESIZING) {
        if (MUI_BeginRefresh(mri, 0)) {
            // Clear background if double buffering
            if (using_double_buffer) {
                ClearDrawingBoard(mri);
            }
            
            MUI_EndRefresh(mri, 0);
        }
        
        RefreshWindowFrame(mri->mri_Window);
        data->wd_Flags &= ~MUIWF_RESIZING;
        
        _width(data->wd_RootObject) = data->wd_Width;
        _height(data->wd_RootObject) = data->wd_Height;
        DoMethod(data->wd_RootObject, MUIM_Layout);
        DoShowMethod(data->wd_RootObject);

        if (muiGlobalInfo(oWin)->mgi_Prefs->window_redraw == WINDOW_REDRAW_WITH_CLEAR) {
            LONG left = mri->mri_Window->BorderLeft;
            LONG top = mri->mri_Window->BorderTop;
            LONG width = mri->mri_Window->Width - mri->mri_Window->BorderRight - left;
            LONG height = mri->mri_Window->Height - mri->mri_Window->BorderBottom - top;

            if (data->wd_Flags & MUIWF_ERASEAREA) {
                // Render background
                if (using_double_buffer) {
                    RenderBackgroundToBuffer(data, left, top, width, height);
                } else {
                    zune_imspec_draw(data->wd_Background, mri, 
                                   left, top, width, height, left, top, 0);
                }
            }
            
            MUI_Redraw(data->wd_RootObject, MADF_DRAWALL);
            
            // Copy buffer to screen if double buffering
            if (using_double_buffer) {
                FlushDoubleBuffer(mri);
            }
        } else {
            MUI_Redraw(data->wd_RootObject, MADF_DRAWOBJECT);
            if (using_double_buffer) {
                FlushDoubleBuffer(mri);
            }
        }
        
        ActivateObject(data);
    } else {
        if (MUI_BeginRefresh(mri, 0)) {
            if (using_double_buffer) {
                ClearDrawingBoard(mri);
            }
            
            MUI_Redraw(data->wd_RootObject, MADF_DRAWALL);
            ActivateObject(data);
            
            if (using_double_buffer) {
                FlushDoubleBuffer(mri);
            }
            
            MUI_EndRefresh(mri, 0);
        }
    }
}
```

### 4. Buffer Management Functions

#### Clear Drawing Board
```c
static void ClearDrawingBoard(struct MUI_RenderInfo *mri)
{
    if (!mri->mri_BufferBM) return;
    
    if (mri->mri_RenderPort && mri->mri_RenderPort->target_board) {
        // Modern path: clear DrawingBoard
        struct ZuneRect rect = {
            0, 0,
            mri->mri_RenderPort->target_board->width,
            mri->mri_RenderPort->target_board->height
        };
        ZuneFillRectangle(mri->mri_RenderPort, &rect, 
                         ZUNE_BRUSH_SOLID(ZUNE_COLOR_ARGB32(0, 0, 0, 0)));
    } else {
        // Fallback: clear traditional bitmap
        struct RastPort *rp = &mri->mri_BufferRP;
        SetAPen(rp, 0);
        SetBPen(rp, 0);
        SetDrMd(rp, JAM1);
        RectFill(rp, 0, 0, 
                 GetBitMapAttr(mri->mri_BufferBM, BMA_WIDTH) - 1,
                 GetBitMapAttr(mri->mri_BufferBM, BMA_HEIGHT) - 1);
    }
}
```

#### Render Background to Buffer
```c
static void RenderBackgroundToBuffer(struct MUI_WindowData *data, 
                                   LONG left, LONG top, LONG width, LONG height)
{
    struct MUI_RenderInfo *mri = &data->wd_RenderInfo;
    
    if (mri->mri_RenderPort && mri->mri_RenderPort->target_board) {
        // Modern path: render background with ZuneRenderer
        struct ZuneRect rect = { (WORD)left, (WORD)top, (UWORD)width, (UWORD)height };
        
        // Convert background spec to brush
        struct ZuneBrush bg_brush;
        if (ConvertImageSpecToBrush(data->wd_Background, &bg_brush, mri)) {
            ZuneFillRectangle(mri->mri_RenderPort, &rect, &bg_brush);
        } else {
            // Fallback to solid color
            ZuneFillRectangle(mri->mri_RenderPort, &rect, 
                             ZUNE_BRUSH_SOLID(_pens(data->wd_RenderInfo)[MPEN_BACKGROUND]));
        }
    } else {
        // Fallback: traditional background rendering
        zune_imspec_draw(data->wd_Background, mri, 
                        left, top, width, height, left, top, 0);
    }
}
```

#### Flush Double Buffer to Screen
```c
static void FlushDoubleBuffer(struct MUI_RenderInfo *mri)
{
    if (!mri->mri_BufferBM || !mri->mri_Window) return;
    
    struct Window *win = mri->mri_Window;
    
    if (mri->mri_RenderPort && mri->mri_RenderPort->target_board) {
        // Modern path: blit DrawingBoard to window
        struct ZuneRect src_rect = { 0, 0, 
                                   mri->mri_RenderPort->target_board->width,
                                   mri->mri_RenderPort->target_board->height };
        struct ZuneRect dst_rect = { win->BorderLeft, win->BorderTop,
                                   win->GZZWidth, win->GZZHeight };
        
        // Create temporary RenderPort for window
        struct RenderPort *win_rp = CreateRenderPort(mri->mri_Colormap, win->RPort);
        if (win_rp) {
            BlitDrawingBoardToRenderPort(mri->mri_RenderPort->target_board,
                                       win_rp, &src_rect, &dst_rect);
            DestroyRenderPort(win_rp);
        }
    } else {
        // Fallback: traditional bitmap blit
        ClipBlit(&mri->mri_BufferRP, 0, 0,
                win->RPort, win->BorderLeft, win->BorderTop,
                win->GZZWidth, win->GZZHeight, 0xC0);
    }
    
    D(bug("Double buffer flushed to screen\n"));
}
```

### 5. Integration with MUI Refresh System

#### Enhanced MUI_BeginRefresh
Location: `mui_beginrefresh.c`

```c
AROS_LH2(BOOL, MUI_BeginRefresh,
        AROS_LHA(struct MUI_RenderInfo *, mri, A0),
        AROS_LHA(ULONG, flags, D0),
        struct Library *, MUIMasterBase, 32, MUIMaster)
{
    AROS_LIBFUNC_INIT

    struct Window *w = mri->mri_Window;
    struct Layer *l;

    if ((w == NULL) || !(w->Flags & WFLG_SIMPLE_REFRESH))
        return 0;

    l = w->WLayer;

    if (!(l->Flags & LAYERREFRESH))
        return 0;

    if (mri->mri_Flags & MUIMRI_REFRESHMODE)
        return 0;

    mri->mri_Flags |= MUIMRI_REFRESHMODE;
    
    // Only lock layers if not using double buffering
    if (!mri->mri_BufferBM) {
        LockLayerInfo(&w->WScreen->LayerInfo);
        BeginRefresh(w);
    }
    
    // Begin batching for performance optimization
    if (mri->mri_RenderPort) {
        BeginBatch(mri->mri_RenderPort);
    }
    
    return 1;

    AROS_LIBFUNC_EXIT
}
```

#### Enhanced MUI_EndRefresh
Location: `mui_endrefresh.c`

```c
AROS_LH2(VOID, MUI_EndRefresh,
        AROS_LHA(struct MUI_RenderInfo *, mri, A0),
        AROS_LHA(ULONG, flags, D0),
        struct Library *, MUIMasterBase, 33, MUIMaster)
{
    AROS_LIBFUNC_INIT

    struct Window *w = mri->mri_Window;

    if (w == NULL)
        return;

    // End batching before finishing refresh
    if (mri->mri_RenderPort) {
        EndBatch(mri->mri_RenderPort);
    }

    // Handle layer cleanup only if not double buffering
    if (!mri->mri_BufferBM) {
        EndRefresh(w, TRUE);
        UnlockLayerInfo(&w->WScreen->LayerInfo);
    } else {
        // For double buffering, we handle the refresh ourselves
        // The buffer has already been flushed in RefreshWindow()
        D(bug("Double buffer refresh completed\n"));
    }
    
    mri->mri_Flags &= ~MUIMRI_REFRESHMODE;
    return;

    AROS_LIBFUNC_EXIT
}
```

### 6. Window Resize Handling

#### Buffer Resize on Window Changes
```c
static void HandleWindowResize(Object *obj, struct MUI_WindowData *data)
{
    struct MUI_RenderInfo *mri = &data->wd_RenderInfo;
    struct Window *win = mri->mri_Window;
    
    if (!win || !mri->mri_BufferBM) return;
    
    // Check if buffer size needs updating
    LONG current_width = GetBitMapAttr(mri->mri_BufferBM, BMA_WIDTH);
    LONG current_height = GetBitMapAttr(mri->mri_BufferBM, BMA_HEIGHT);
    
    if (current_width != win->GZZWidth || current_height != win->GZZHeight) {
        D(bug("Resizing double buffer: %dx%d -> %dx%d\n", 
              current_width, current_height, win->GZZWidth, win->GZZHeight));
        
        // Recreate buffer with new dimensions
        DeinstallBackbuffer(NULL, obj);
        InstallBackbuffer(NULL, obj);
        
        // Update render info
        ShowRenderInfo(mri);
    }
}
```

### 7. Performance Considerations

#### When to Enable Double Buffering
```c
static BOOL ShouldEnableDoubleBuffering(struct MUI_WindowData *data)
{
    struct Window *win = data->wd_RenderInfo.mri_Window;
    
    if (!win) return FALSE;
    
    // Enable for larger windows (reduces flicker benefit)
    if (win->GZZWidth * win->GZZHeight > 100000) return TRUE;
    
    // Enable for complex UIs (many objects)
    Object *root = data->wd_RootObject;
    LONG object_count = 0;
    get(root, MUIA_Group_ChildCount, &object_count);
    if (object_count > 20) return TRUE;
    
    // Enable based on user preference
    if (muiGlobalInfo(data)->mgi_Prefs->window_redraw == WINDOW_REDRAW_DOUBLE_BUFFER) {
        return TRUE;
    }
    
    return FALSE;
}
```

### 8. Integration Points Summary

#### Required Changes:

1. **window.c**:
   - Enhance `InstallBackbuffer()` and `DeinstallBackbuffer()`
   - Modify `RefreshWindow()` to handle double buffering
   - Add buffer management functions
   - Handle window resize events

2. **mui_beginrefresh.c**:
   - Skip layer locking for double buffering
   - Always enable batching

3. **mui_endrefresh.c**:
   - Skip layer unlocking for double buffering
   - End batching before completion

4. **MUI Preferences**:
   - Add double buffering option to preferences
   - Allow per-application control

### 9. Benefits of This Approach

#### Performance Benefits:
- **Eliminates Flicker**: Complete frame rendered before display
- **Batched Operations**: All drawing operations cached and optimized
- **GPU Acceleration**: Modern path uses ZuneRenderer's hardware acceleration
- **Adaptive**: Falls back to traditional methods when needed

#### Compatibility Benefits:
- **Backward Compatible**: Works with existing MUI applications
- **Progressive Enhancement**: Modern apps get better performance automatically
- **Graceful Degradation**: Falls back if memory allocation fails

### 10. Testing and Debugging

#### Debug Output:
```c
#define DEBUG_DOUBLE_BUFFER 1

#if DEBUG_DOUBLE_BUFFER
#define DB(x) D(x)
#else
#define DB(x)
#endif
```

#### Performance Monitoring:
- Track buffer allocation success/failure
- Monitor resize frequency
- Measure rendering performance with/without double buffering
- Cache hit rates from batching system

This implementation provides a complete double buffering solution that integrates seamlessly with your existing ZuneRenderer caching system while maintaining full MUI compatibility.