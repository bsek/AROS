# cgxvideo: existing public contract

This document describes the existing public CyberGraphX `cgxvideo.library`
contract: its entry points, established tags, source formats and error codes.
The current AROS implementation is incomplete, so the lifecycle and backend
sections below state the behaviour that the implementation must provide in
order to satisfy that contract.

The compatibility baseline is the MorphOS SDK 3.20 `cgxvideo.h` (43.17,
covering V42, V43 and V50). The AROS header carries the same tag, query,
format and error values; it adds nothing of its own.

The original public ABI is kept unchanged. This document does not propose new
public `VOA_*` tags.

## Required handle lifecycle

`CreateVLayerHandleTagList()` returns an opaque handle in the detached,
unlocked state. `VOA_SrcType` and `VOA_SrcHeight` are immutable. V50 lets
`SetVLayerAttrTagList()` change `VOA_SrcWidth`; whether that is honoured is
up to the backend.

```text
                 Attach
       +--------------------------+
       |                          v
  CREATED/DETACHED <--------> ATTACHED
       ^                          |
       +------------ Detach ------+
```

Lock/unlock is allowed in either non-locked state. Attach, detach and runtime
attribute updates are invalid while locked. Deletion is terminal and must be
safe in every state: unlock and detach first, then release backend resources
and the handle itself.

## Source buffer

`LockVLayer()` returns `TRUE` when the caller has temporary exclusive write
access. `GetVLayerAttr(VOA_BaseAddress)` returns the writable address only
while that lock is held; it must not be retained after `UnlockVLayer()`.

`UnlockVLayer()` publishes the completed source buffer to the backend. It does
not attach or detach the overlay. The lock does not nest: a second
`LockVLayer()` fails until `UnlockVLayer()`, also for the task holding it.

`SwapVLayerBuffer()` (V50) swaps the displayed and rendering buffers of a
double-buffered layer at the next vertical blank; the next `LockVLayer()` may
wait for it.

## Window attachment

`AttachVLayerTagList()` associates the overlay with the window's layer
hierarchy. The layer system, not `cgxvideo`, owns the relationship between the
window and the overlay's screen position. Consequently a window move, resize,
front/back change or clipping update must be reflected by the layer system;
clients must not reposition the overlay manually after attachment.

The four indent tags are attachment parameters passed to the layer/driver.
They reserve an inset from the window's video area, but this document does not
define them as a client-side `left/top/right/bottom` calculation. The exact
reference rectangle (window bounds versus inner bounds) is a property of the
CyberGraphX layer implementation and must follow the original
`AttachVLayerTagList()` behaviour. In particular, an omitted tag list must use
the implementation's normal window-inner offset rules.

An attached handle remains associated with one window until
`DetachVLayer()`; the layer system handles movement and visibility changes.
`VOA_ColorKeyFill` (default `TRUE`) lets the caller stop the window's video
area being filled with the color key.

## Existing attributes

Creation tags:

- `VOA_SrcType`: `SRCFMT_YUV16`, `SRCFMT_YCbCr16`, `SRCFMT_RGB15PC` or
  `SRCFMT_RGB16PC`.
- `VOA_SrcWidth`, `VOA_SrcHeight`: non-zero source dimensions.
- `VOA_UseColorKey`: request color-keyed composition.
- `VOA_UseBackfill`: request automatic backfill; valid only with color keying.
- `VOA_Error`: optional `ULONG *` receiving a `VOERR_*` value.
- `VOA_Modulo` (V43): bytes per source row.

The later CyberGraphX revisions also define `VOA_UseFilter`,
`VOA_Identifier`, `VOA_DoubleBuffer`, `VOA_InterLaced`, `VOA_CaptureMode`,
`VOA_FrameIndex`, `VOA_MultiBuffer`, `VOA_ZoomRect` and the V50 subpicture
tags (`VOA_SubPicture`, `VOA_SrcWidthSP`, `VOA_SrcHeightSP`, ...).

Runtime tags are the four indent tags, plus `VOA_SrcWidth`, `VOA_UseFilter`,
`VOA_ZoomRect` and the subpicture tags in V50. `VOA_BaseAddress`, `VOA_ColorKeyPen`
and `VOA_ColorKey` are read-only attributes in the established API;
`VOA_BaseAddress` is valid only while the handle is locked, and the color-key
attributes return `-1` when color keying is disabled or unavailable.

The established buffer attributes are `VOA_FrameBase0` through
`VOA_FrameBase1`, `VOA_FrameType`, `VOA_Width`, `VOA_Height`, `VOA_Modulo` and
`VOA_BaseOffset`. V50 adds `VOA_BaseOffset0` through `VOA_BaseOffset5`.

The canonical source-format names are `SRCFMT_YUV16` (obsolete),
`SRCFMT_YCbCr16`, `SRCFMT_RGB15`/`SRCFMT_R5G5B5PC`,
`SRCFMT_RGB16`/`SRCFMT_R5G6B5PC` and `SRCFMT_YCbCr420`.

## Backend boundary

The public entry points must not know a particular HIDD or display driver.
The internal backend creates an overlay bitmap through the existing display
object creation path. It does not add a new `NewOverlay` or `DisposeOverlay`
method to the HIDD interface. The relevant operations are equivalent to:

```text
display.CreateObject(overlay bitmap description) -> overlay bitmap or failure
overlay_bitmap.Dispose()
attach(object, window, rectangle)
detach(object)
lock(object) -> writable address
unlock(object)
get_attribute(object, attribute)
```

The overlay bitmap is a normal HIDD object. Its lifetime ends through the
normal root `Dispose` method; `cgxvideo` must not introduce a parallel destroy
operation. The driver advertises the overlay bitmap type and its supported
attributes through the existing HIDD class/object model.

The driver owns synchronization, clipping, scaling, format conversion and
color-key programming and layer integration. `cgxvideo` owns validation, the
public state machine and references to the overlay bitmap.

## Driver capability reporting and allocation

The driver must advertise support through the existing CyberGraphX video
query mechanism, `QueryVLayerAttr(screen, attr)` (V50), not through new
`VOA_*` tags. The canonical query attributes are `VSQ_SupportedFeatures`, `VSQ_SupportedFormats`, `VSQ_MaxWidth` and
`VSQ_MaxWidthSP`. Feature bits include `VSQ_FEAT_OVERLAY`,
`VSQ_FEAT_DOUBLEBUFFER`, `VSQ_FEAT_MULTIBUFFER`, `VSQ_FEAT_COLORKEYING`,
`VSQ_FEAT_FILTERING`, `VSQ_FEAT_CAPTUREMODE`, `VSQ_FEAT_INTERLACE`,
`VSQ_FEAT_ZOOMRECT` and `VSQ_FEAT_SUBPICTURE`.

After capability validation, `cgxvideo` creates the overlay through the
existing display `CreateObject` path, using the driver's overlay bitmap class.
The overlay is therefore a normal HIDD bitmap object and inherits normal
bitmap attributes and object lifetime rules. Disposal is performed by the
normal root `Dispose` method; no parallel `NewOverlay` or `DisposeOverlay`
interface is required.

The overlay bitmap class owns hardware-specific synchronization, clipping,
scaling, format conversion, color-key programming and the layer integration.
`cgxvideo` owns the public handle and translates the established CyberGraphX
calls into bitmap object operations.

## Entry points

| LVO | Function | |
|---|---|---|
| 5-12 | `CreateVLayerHandleTagList` ... `SetVLayerAttrTagList` | V41 |
| 13-15 | reserved | |
| 16 | `SwapVLayerBuffer` | V50 |
| 17 | `WriteSPLine` | V50 |
| 18 | `QueryVLayerAttr` | V50 |

The library is version 50, the first with all of these entry points.
`UnlockVLayer()` returns `ULONG` (always 0) as in the MorphOS prototypes.
`GetVLayerAttr()` returns `IPTR` so that `VOA_BaseAddress` fits on 64-bit.

## Current implementation status

The state machine above is implemented. No display driver provides an
overlay yet, so on a normal screen `CreateVLayerHandleTagList()` fails with
`VOERR_INVSCRMODE` and `QueryVLayerAttr()` returns 0.

Setting the `CGXVideoNullBackend` variable (local or global) selects a null
backend: the source buffer lives in system RAM and nothing is displayed. It
accepts the four 16-bit formats, rejects `SRCFMT_YCbCr420`, ignores indents
and runtime tags, and reports `VSQ_FEAT_OVERLAY` only. It exists for testing;
`developer/debug/test/cgxvideo/vlayertest` exercises it.

`SwapVLayerBuffer()` and `WriteSPLine()` do nothing until a backend supports
double buffering and subpictures.

The next milestone is the overlay bitmap backend through `CreateObject`
described above, followed by the layer integration.
