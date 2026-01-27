# AROS Rendering Modernisering - Teknisk Rapport

## Sammendrag

Denne rapporten analyserer hvordan AROS sin Workbench/Wanderer-rendering kan moderniseres med GPU-drevet OpenGL som backend. Løsningen må støtte **både moderne zunegfx-vinduer OG legacy-vinduer som tegner direkte til RastPort**.

Anbefalt tilnærming: **Hybrid compositor** som:
1. Lar legacy-vinduer tegne til layer-bitmap som før
2. Bruker GPU-teksturer for moderne vinduer (zero-copy)
3. Compositor samler alt og blender på GPU

---

## 1. Problemet: For Mange Kopieringssteg

### 1.1 Nåværende (Ineffektiv) Flyt

```
App tegner til RastPort
       │
       ▼
RastPort → BitMap (CPU)
       │
       ▼
BitMap → FBO (glTexImage2D - CPU→GPU kopiering)
       │
       ▼
FBO → Compositor blending
       │
       ▼
Compositor → Skygge-rendering
       │
       ▼
Resultat → glReadPixels (GPU→CPU kopiering)
       │
       ▼
BitMap → HIDD_Gfx_Show
       │
       ▼
Skjerm
```

**Problem**: Minst 2-3 unødvendige kopieringer mellom CPU og GPU.

### 1.2 Ønsket (Effektiv) Flyt

```
App tegner direkte til GPU-ressurs
       │
       ▼
GPU-ressurs brukes direkte i compositor
       │
       ▼
Compositor blender på GPU (inkl. skygger)
       │
       ▼
Resultat vises via HIDD (ingen kopiering)
       │
       ▼
Skjerm
```

**Mål**: Alt forblir på GPU, ingen CPU-kopieringer.

---

## 2. Løsning: Direkte HIDD-Integrasjon

### 2.1 Arkitektur-oversikt

```
┌─────────────────────────────────────────────────────────────────┐
│                    APPLIKASJONER (Zune/MUI)                     │
│                                                                 │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    ZUNEGFX.LIBRARY                        │  │
│  │  ┌─────────────────┐    ┌─────────────────┐              │  │
│  │  │ RenderPort      │    │ DrawingBoard    │              │  │
│  │  │ (GL Context)    │───▶│ (pipe_resource) │              │  │
│  │  └─────────────────┘    └────────┬────────┘              │  │
│  └──────────────────────────────────┼────────────────────────┘  │
│                                     │                           │
├─────────────────────────────────────┼───────────────────────────┤
│         GPU COMPOSITOR (Ny)         │                           │
│  ┌──────────────────────────────────┼────────────────────────┐  │
│  │                                  ▼                        │  │
│  │  ┌─────────────┐  ┌─────────────────────────┐            │  │
│  │  │ Window      │  │ Compositor FBO          │            │  │
│  │  │ Textures    │─▶│ (alle vinduer blandet)  │            │  │
│  │  │ (GPU-side)  │  │ + skygger               │            │  │
│  │  └─────────────┘  └────────────┬────────────┘            │  │
│  │                                │                          │  │
│  │    Ingen CPU-kopiering!        │                          │  │
│  └────────────────────────────────┼──────────────────────────┘  │
│                                   │                             │
├───────────────────────────────────┼─────────────────────────────┤
│              HIDD DIREKTE         │                             │
│  ┌────────────────────────────────┼──────────────────────────┐  │
│  │                                ▼                          │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │ HIDD_Gfx_ShowViewPorts() eller                      │  │  │
│  │  │ HIDD_Gallium_DisplayResource()                      │  │  │
│  │  │                                                     │  │  │
│  │  │ GPU-ressurs → Display direkte (zero-copy)           │  │  │
│  │  └─────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                   │                             │
│                                   ▼                             │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                         SKJERM                            │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Nøkkelkomponenter

| Komponent | Formål |
|-----------|--------|
| `pipe_resource` | Gallium GPU-buffer som kan deles mellom kontekster |
| `HIDD_Gfx_ShowViewPorts` | Viser flere bitmaps med hardware-compositing |
| `HIDD_Gallium_DisplayResource` | Overfører GPU-ressurs til skjerm uten CPU |
| Compositor FBO | Alle vinduer blandes på GPU |

---

## 3. Implementasjonsstrategi

### 3.1 Tre Mulige Tilnærminger

#### Tilnærming A: Gallium pipe_resource Deling (Anbefalt)

```c
// DrawingBoard bruker Gallium pipe_resource direkte
struct DrawingBoard {
    struct pipe_resource *gpu_resource;  // Delt GPU-buffer
    ULONG fbo_id;                        // OpenGL FBO bundet til ressursen
    BOOL is_gpu_native;                  // TRUE = aldri kopiert til CPU
};

// Compositor bruker ressursen direkte som tekstur
void CompositorBindWindowTexture(struct CompositorWindow *cw) {
    // Bind pipe_resource som GL-tekstur (zero-copy)
    // Mesa/Gallium håndterer dette internt
    glBindTexture(GL_TEXTURE_2D, cw->texture_from_pipe_resource);
}

// Vis resultat via HIDD
void CompositorPresent(struct LayerCompositor *comp) {
    // Hent compositor FBO som pipe_resource
    struct pipe_resource *result = GetFBOPipeResource(comp->output_fbo);
    
    // Send direkte til HIDD - ingen CPU-kopiering
    HIDD_Gallium_DisplayResource(galliumHidd, result, 
        0, 0,                    // src offset
        comp->screen->BitMap,   // target (for HIDD referanse)
        0, 0,                    // dst offset
        comp->width, comp->height);
}
```

**Fordeler**:
- Ingen CPU-kopiering i hele pipelinen
- Fungerer med eksisterende Mesa/Gallium
- Optimal ytelse

**Ulemper**:
- Krever Gallium-driver (ikke alle plattformer)

---

#### Tilnærming B: HIDD_Gfx_ShowViewPorts

```c
// Bygg HIDD_ViewPortData-liste for alle synlige vinduer
struct HIDD_ViewPortData *BuildViewPortList(struct LayerCompositor *comp) {
    struct HIDD_ViewPortData *head = NULL, *prev = NULL;
    
    ForeachNode(&comp->windows, cw) {
        struct HIDD_ViewPortData *vpd = AllocMem(sizeof(*vpd), MEMF_CLEAR);
        vpd->Bitmap = cw->hidd_bitmap;  // HIDD bitmap objekt
        vpd->vpe = cw->viewport_extra;
        
        // Legg til alpha-info via UserData
        vpd->UserData = (APTR)(IPTR)cw->alpha;
        
        if (prev) prev->Next = vpd;
        else head = vpd;
        prev = vpd;
    }
    return head;
}

// La HIDD gjøre compositing
BOOL CompositorPresent(struct LayerCompositor *comp) {
    struct HIDD_ViewPortData *vplist = BuildViewPortList(comp);
    
    // HIDD gjør hardware-compositing av alle vinduer
    BOOL ok = HIDD_Gfx_ShowViewPorts(comp->gfxHidd, vplist, comp->view);
    
    FreeViewPortList(vplist);
    return ok;
}
```

**Fordeler**:
- Standard HIDD-interface
- Driver kan optimalisere fritt
- Fungerer med ikke-Gallium drivere

**Ulemper**:
- Krever driver-støtte for ShowViewPorts
- Begrenset kontroll over compositing

---

#### Tilnærming C: Custom HIDD Compositor-Subklasse

```c
// Lag egen HIDD-subklasse for GPU-compositor
OOP_Object *GPUCompositor__Hidd_Gfx__Show(OOP_Class *cl, OOP_Object *o,
                                          struct pHidd_Gfx_Show *msg)
{
    struct GPUCompositorData *data = OOP_INST_DATA(cl, o);
    
    // Ikke bruk framebuffer-kopiering
    if (msg->bitMap == NULL) {
        // Blank skjerm
        return NULL;
    }
    
    // Sjekk om bitmap har tilknyttet GPU-ressurs
    APTR pipe_res = GetBitMapGPUResource(msg->bitMap);
    if (pipe_res) {
        // Direkte GPU→display overføring
        DirectGPUToDisplay(data->display, pipe_res);
        return msg->bitMap;
    }
    
    // Fallback til standard oppførsel
    return OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}
```

**Fordeler**:
- Full kontroll
- Kan optimalisere for spesifikk hardware

**Ulemper**:
- Mest arbeid å implementere
- Må vedlikeholdes per driver

---

### 3.2 Anbefalt Tilnærming: Kombinasjon av A og B

```
┌─────────────────────────────────────────────────────────────┐
│                    RENDERING PIPELINE                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. App tegner via zunegfx RenderPort                       │
│     │                                                       │
│     ▼                                                       │
│  2. DrawingBoard (pipe_resource / FBO)                      │
│     │                                                       │
│     ├──── Gallium tilgjengelig? ────┐                       │
│     │                               │                       │
│     ▼ JA                            ▼ NEI                   │
│  ┌──────────────────┐     ┌──────────────────┐             │
│  │ Zero-copy path   │     │ Fallback path    │             │
│  │                  │     │                  │             │
│  │ pipe_resource    │     │ CPU BitMap       │             │
│  │ deles direkte    │     │ synkroniseres    │             │
│  │ med compositor   │     │ til GPU-tekstur  │             │
│  └────────┬─────────┘     └────────┬─────────┘             │
│           │                        │                        │
│           └──────────┬─────────────┘                        │
│                      ▼                                      │
│  3. GPU Compositor                                          │
│     - Blend alle vinduer                                    │
│     - Legg til skygger                                      │
│     - Effekter (blur, etc)                                  │
│     │                                                       │
│     ▼                                                       │
│  4. Output                                                  │
│     │                                                       │
│     ├──── ShowViewPorts støttet? ────┐                      │
│     │                                │                       │
│     ▼ JA                             ▼ NEI                  │
│  ┌──────────────────┐     ┌──────────────────┐             │
│  │ HIDD_Gfx_        │     │ HIDD_Gallium_    │             │
│  │ ShowViewPorts    │     │ DisplayResource  │             │
│  │                  │     │ eller            │             │
│  │ (multi-bitmap    │     │ standard Show    │             │
│  │  hw-composite)   │     │                  │             │
│  └────────┬─────────┘     └────────┬─────────┘             │
│           │                        │                        │
│           └──────────┬─────────────┘                        │
│                      ▼                                      │
│  5. Skjerm                                                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Legacy RastPort-Vinduer: Integrasjon

Dette er kritisk: De fleste eksisterende AROS-applikasjoner tegner direkte til `Window->RPort` via graphics.library-funksjoner som `Draw()`, `RectFill()`, `BltBitMap()`, etc. Disse må fungere sømløst med den nye GPU-compositoren.

### 4.1 Hvordan Legacy-Vinduer Fungerer I Dag

```
┌─────────────────────────────────────────────────────────────┐
│  LEGACY APP                                                 │
│                                                             │
│  Draw(window->RPort, x, y);                                 │
│  RectFill(window->RPort, x1, y1, x2, y2);                   │
│       │                                                     │
│       ▼                                                     │
├─────────────────────────────────────────────────────────────┤
│  GRAPHICS.LIBRARY                                           │
│                                                             │
│  do_render_with_gc(rp, ...)                                 │
│       │                                                     │
│       ├── rp->Layer != NULL?                                │
│       │        │                                            │
│       │        ▼ JA                                         │
│       │   ┌────────────────────────────────────┐            │
│       │   │ Iterer gjennom Layer->ClipRect     │            │
│       │   │                                    │            │
│       │   │ For hver synlig ClipRect:          │            │
│       │   │   → Tegn til rp->BitMap (skjerm)   │            │
│       │   │                                    │            │
│       │   │ For hver skjult ClipRect:          │            │
│       │   │   → Tegn til CR->BitMap (backup)   │            │
│       │   └────────────────────────────────────┘            │
│       │                                                     │
│       ▼                                                     │
│  HIDD_BM_* operasjoner på Screen->BitMap                    │
│       │                                                     │
│       ▼                                                     │
│  Skjerm (direkte, ingen compositor)                         │
└─────────────────────────────────────────────────────────────┘
```

**Nøkkelpunkt**: `Window->RPort` er faktisk `Window->WLayer->rp` - de er samme objekt! RastPort peker tilbake til Layer via `rp->Layer`.

### 4.2 Compositor Hook for Legacy-Vinduer

Det finnes allerede en hook-mekanisme i `do_render_with_gc()` som kalles etter tegning:

```c
// Fra rom/graphics/gfxfuncsupport.c (linje ~280-315)
// Etter at tegning er ferdig:

if (pixwritten > 0 &&
    (IL(L)->il_AlphaFlags & ILAF_ALPHA) &&
    L->LayerInfo && LIE(L->LayerInfo)->lie_CompositorHook)
{
    struct CompositorMsg msg;
    msg.cm_Method = COMP_DIRTYLAYER;  // "Layer innhold er endret"
    msg.cm_Layer = L;
    msg.cm_Region = &changed_rect;
    
    // COMPOSITOR HOOK KALLES HER
    CallHookPkt(LIE(L->LayerInfo)->lie_CompositorHook, L->LayerInfo, &msg);
}
```

Dette betyr: **Legacy-vinduer kan allerede trigge compositor-oppdatering!**

### 4.3 Hybrid Rendering Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│                      ALLE VINDUER                               │
│                                                                 │
│  ┌─────────────────────┐        ┌─────────────────────┐        │
│  │ LEGACY VINDU        │        │ ZUNEGFX VINDU       │        │
│  │                     │        │                     │        │
│  │ Draw(rp, ...)       │        │ ZuneDrawLine(...)   │        │
│  │ RectFill(rp, ...)   │        │ ZuneFillRect(...)   │        │
│  │      │              │        │      │              │        │
│  │      ▼              │        │      ▼              │        │
│  │ Layer->BitMap       │        │ DrawingBoard/FBO    │        │
│  │ (screen bitmap)     │        │ (GPU-native)        │        │
│  └──────────┬──────────┘        └──────────┬──────────┘        │
│             │                              │                    │
│             │ COMP_DIRTYLAYER              │ Direkte GPU        │
│             │                              │                    │
├─────────────┼──────────────────────────────┼────────────────────┤
│             │      GPU COMPOSITOR          │                    │
│             │                              │                    │
│             ▼                              ▼                    │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                                                         │   │
│  │  For hvert vindu (bakfra og frem):                      │   │
│  │                                                         │   │
│  │  ┌─────────────────┐    ┌─────────────────┐            │   │
│  │  │ Legacy vindu?   │    │ Zunegfx vindu?  │            │   │
│  │  │                 │    │                 │            │   │
│  │  │ Upload region   │    │ Bind tekstur    │            │   │
│  │  │ fra screen BM   │    │ direkte         │            │   │
│  │  │ til GPU-tekstur │    │ (zero-copy)     │            │   │
│  │  └────────┬────────┘    └────────┬────────┘            │   │
│  │           │                      │                      │   │
│  │           └──────────┬───────────┘                      │   │
│  │                      ▼                                  │   │
│  │           GPU Blending + Skygger                        │   │
│  │                      │                                  │   │
│  └──────────────────────┼──────────────────────────────────┘   │
│                         │                                      │
│                         ▼                                      │
│              HIDD Display (zero-copy output)                   │
│                         │                                      │
│                         ▼                                      │
│                      SKJERM                                    │
└─────────────────────────────────────────────────────────────────┘
```

### 4.4 CompositorWindow for Legacy vs Zunegfx

```c
struct CompositorWindow {
    struct MinNode node;
    struct Window *window;
    struct Layer *layer;
    
    // Type vindu
    enum {
        CW_TYPE_LEGACY,      // Tegner til RastPort/Layer bitmap
        CW_TYPE_ZUNEGFX      // Tegner til DrawingBoard (GPU-native)
    } type;
    
    // For LEGACY vinduer
    struct {
        struct Rectangle dirty_rect;  // Område som er endret
        BOOL needs_upload;            // Må uploade til GPU-tekstur
        ULONG cached_texture_id;      // Cachet GPU-tekstur
    } legacy;
    
    // For ZUNEGFX vinduer  
    struct {
        struct DrawingBoard *board;   // GPU-native DrawingBoard
        ULONG texture_id;             // = board->gpu.gl_texture_id (zero-copy)
    } zunegfx;
    
    // Felles
    WORD x, y;
    UWORD width, height;
    UBYTE alpha;
    BOOL has_shadow;
    BOOL visible;
};
```

### 4.5 Håndtering av COMP_DIRTYLAYER

Når et legacy-vindu tegner, mottar compositor denne meldingen:

```c
void CompositorHookFunc(struct Hook *hook, struct Layer_Info *li, 
                        struct CompositorMsg *msg)
{
    struct GPUCompositor *comp = (struct GPUCompositor *)hook->h_Data;
    
    switch (msg->cm_Method) {
    
    case COMP_DIRTYLAYER: {
        // Legacy-vindu har tegnet noe
        struct Layer *layer = msg->cm_Layer;
        struct CompositorWindow *cw = FindCompositorWindow(comp, layer);
        
        if (cw && cw->type == CW_TYPE_LEGACY) {
            // Merk regionen som dirty - må uploades til GPU
            if (msg->cm_Region) {
                OrRectRegion(&cw->legacy.dirty_rect, msg->cm_Region);
            } else {
                // Hele vinduet er dirty
                cw->legacy.dirty_rect.MinX = 0;
                cw->legacy.dirty_rect.MinY = 0;
                cw->legacy.dirty_rect.MaxX = cw->width - 1;
                cw->legacy.dirty_rect.MaxY = cw->height - 1;
            }
            cw->legacy.needs_upload = TRUE;
            
            // Trigger compositor-oppdatering
            SignalCompositorUpdate(comp);
        }
        break;
    }
    
    case COMP_SHOWLAYER:
    case COMP_MOVELAYER:
        // Vindu ble synlig eller flyttet
        RecalculateWindowPositions(comp);
        break;
        
    case COMP_HIDELAYER:
        // Vindu ble skjult
        MarkWindowHidden(comp, msg->cm_Layer);
        break;
    }
}
```

### 4.6 GPU-Tekstur Upload for Legacy-Vinduer

```c
void UploadLegacyWindowToGPU(struct GPUCompositor *comp, 
                              struct CompositorWindow *cw)
{
    if (!cw->legacy.needs_upload) return;
    
    struct Layer *layer = cw->layer;
    struct BitMap *screen_bm = layer->rp->BitMap;
    
    // Beregn vinduets posisjon i screen bitmap
    WORD src_x = cw->x;  // layer->bounds.MinX
    WORD src_y = cw->y;  // layer->bounds.MinY
    
    // Alloker/gjenbruk GPU-tekstur
    if (cw->legacy.cached_texture_id == 0) {
        glGenTextures(1, &cw->legacy.cached_texture_id);
        glBindTexture(GL_TEXTURE_2D, cw->legacy.cached_texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 
                     cw->width, cw->height, 0,
                     GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    }
    
    // Les kun dirty-region fra screen bitmap
    struct Rectangle *dirty = &cw->legacy.dirty_rect;
    ULONG dirty_width = dirty->MaxX - dirty->MinX + 1;
    ULONG dirty_height = dirty->MaxY - dirty->MinY + 1;
    
    // Alloker temp-buffer for dirty region
    ULONG *pixels = AllocMem(dirty_width * dirty_height * 4, MEMF_ANY);
    
    // Les fra screen bitmap (HIDD operasjon)
    ReadPixelArray(pixels, 0, 0, dirty_width * 4,
                   layer->rp,
                   src_x + dirty->MinX, 
                   src_y + dirty->MinY,
                   dirty_width, dirty_height,
                   RECTFMT_ARGB);
    
    // Upload kun dirty-region til GPU-tekstur
    glBindTexture(GL_TEXTURE_2D, cw->legacy.cached_texture_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    dirty->MinX, dirty->MinY,      // Offset i tekstur
                    dirty_width, dirty_height,
                    GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    
    FreeMem(pixels, dirty_width * dirty_height * 4);
    
    // Nullstill dirty-tracking
    cw->legacy.needs_upload = FALSE;
    cw->legacy.dirty_rect.MinX = cw->width;  // Tom rect
    cw->legacy.dirty_rect.MinY = cw->height;
    cw->legacy.dirty_rect.MaxX = 0;
    cw->legacy.dirty_rect.MaxY = 0;
}
```

### 4.7 Optimalisering: Lazy Upload

For å unngå å uploade for hver lille tegneoperasjon:

```c
void CompositorUpdateFrame(struct GPUCompositor *comp)
{
    // Samle opp alle dirty regions fra denne framen
    // før vi gjør noen GPU-operasjoner
    
    struct CompositorWindow *cw;
    ForeachNode(&comp->windows, cw) {
        if (cw->type == CW_TYPE_LEGACY && cw->legacy.needs_upload) {
            // Batch upload - gjøres én gang per frame
            UploadLegacyWindowToGPU(comp, cw);
        }
    }
    
    // Nå kan vi composite alle vinduer
    GPUCompositorRender(comp);
}
```

### 4.8 Ytelseskarakteristikker

| Vindustype | Tegning | Upload til GPU | Compositing |
|------------|---------|----------------|-------------|
| **Legacy** | CPU → Screen BitMap | Dirty-region upload | GPU blend |
| **Zunegfx** | GPU → FBO | Zero-copy (ingen) | GPU blend |

**Legacy-overhead**:
- Én `ReadPixelArray()` per dirty-vindu per frame
- Én `glTexSubImage2D()` per dirty-vindu per frame
- Mye bedre enn å uploade hele vinduet hver gang!

**Optimalisering for legacy**:
- Dirty-region tracking minimerer upload-størrelse
- Batch uploads per frame (ikke per tegneoperasjon)
- Tekstur-caching (alloker én gang, oppdater inkrementelt)

---

## 5. Implementasjonsdetaljer

### 5.1 DrawingBoard med pipe_resource

```c
// Utvidet DrawingBoard-struktur
struct DrawingBoard {
    // Eksisterende felter
    struct BitMap *bitmap;
    struct RastPort *rastport;
    UWORD width, height;
    
    // Nye GPU-native felter
    struct {
        APTR pipe_resource;          // Gallium pipe_resource
        ULONG gl_texture_id;         // GL-tekstur bundet til ressursen
        ULONG gl_fbo_id;             // FBO for rendering
        BOOL is_gpu_native;          // TRUE = data lever på GPU
        BOOL needs_sync;             // TRUE = må synkronisere før CPU-tilgang
    } gpu;
    
    // Fallback for ikke-Gallium
    struct {
        APTR pixels;                 // CPU pixel-buffer
        ULONG pitch;
    } cpu;
};

// Opprett GPU-native DrawingBoard
struct DrawingBoard *CreateGPUDrawingBoard(UWORD width, UWORD height,
                                           struct Window *window)
{
    struct DrawingBoard *board = AllocMem(sizeof(*board), MEMF_CLEAR);
    
    board->width = width;
    board->height = height;
    
    // Prøv Gallium først
    if (GalliumBase && window) {
        // Opprett pipe_resource
        board->gpu.pipe_resource = CreatePipeResource(width, height, 
                                                       PIPE_FORMAT_B8G8R8A8_UNORM);
        if (board->gpu.pipe_resource) {
            board->gpu.is_gpu_native = TRUE;
            
            // Bind som GL-tekstur (zero-copy via EGL_MESA_image_dma_buf)
            board->gpu.gl_texture_id = CreateTextureFromPipeResource(
                board->gpu.pipe_resource);
            
            // Opprett FBO med teksturen som color attachment
            glGenFramebuffers(1, &board->gpu.gl_fbo_id);
            glBindFramebuffer(GL_FRAMEBUFFER, board->gpu.gl_fbo_id);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, board->gpu.gl_texture_id, 0);
            
            return board;
        }
    }
    
    // Fallback til CPU-buffer
    board->gpu.is_gpu_native = FALSE;
    board->bitmap = AllocBitMap(width, height, 32, BMF_MINPLANES, NULL);
    // ... standard oppsett
    
    return board;
}
```

### 5.2 Compositor med Zero-Copy

```c
struct GPUCompositor {
    OOP_Object *gfxHidd;
    OOP_Object *galliumHidd;
    
    // Output FBO
    ULONG output_fbo;
    APTR output_pipe_resource;
    UWORD width, height;
    
    // Shader-program
    ULONG composite_shader;
    ULONG shadow_shader;
    
    // Registrerte vinduer
    struct MinList windows;  // Liste av CompositorWindow
};

struct CompositorWindow {
    struct MinNode node;
    struct Window *window;
    struct DrawingBoard *board;
    
    // GPU-ressurser (eid av DrawingBoard, referert her)
    ULONG texture_id;        // = board->gpu.gl_texture_id
    
    // Vindusattributter
    WORD x, y;
    UWORD width, height;
    UBYTE alpha;             // Global alpha
    BOOL has_shadow;
    BOOL visible;
    BOOL dirty;
};

// Compositing uten CPU-kopiering
void GPUCompositorUpdate(struct GPUCompositor *comp)
{
    // Bind output FBO
    glBindFramebuffer(GL_FRAMEBUFFER, comp->output_fbo);
    glViewport(0, 0, comp->width, comp->height);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Tegn alle vinduer (bakfra og frem)
    struct CompositorWindow *cw;
    ForeachNode(&comp->windows, cw) {
        if (!cw->visible) continue;
        
        // Tegn skygge først (hvis aktivert)
        if (cw->has_shadow) {
            glUseProgram(comp->shadow_shader);
            DrawWindowShadow(cw);
        }
        
        // Tegn vindusinnhold
        glUseProgram(comp->composite_shader);
        
        // Bind vinduets tekstur (zero-copy - teksturen ER pipe_resource)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cw->texture_id);
        
        // Sett uniforms
        glUniform1f(glGetUniformLocation(comp->composite_shader, "u_alpha"),
                    cw->alpha / 255.0f);
        
        // Tegn quad
        DrawTexturedQuad(cw->x, cw->y, cw->width, cw->height);
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Presenter til skjerm via HIDD (zero-copy)
    GPUCompositorPresent(comp);
}

void GPUCompositorPresent(struct GPUCompositor *comp)
{
    if (comp->galliumHidd && comp->output_pipe_resource) {
        // BEST: Direkte GPU→display via Gallium
        struct pHidd_Gallium_DisplayResource msg = {
            .mID = OOP_GetMethodID(IID_Hidd_Gallium, moHidd_Gallium_DisplayResource),
            .resource = comp->output_pipe_resource,
            .srcx = 0, .srcy = 0,
            .bitmap = comp->screen->BitMap,
            .dstx = 0, .dsty = 0,
            .width = comp->width,
            .height = comp->height
        };
        OOP_DoMethod(comp->galliumHidd, (OOP_Msg)&msg);
    }
    else {
        // Fallback: Les tilbake og bruk standard HIDD
        // (Mindre effektivt, men fungerer alltid)
        SyncFBOToBitMap(comp->output_fbo, comp->screen->BitMap);
        HIDD_BM_UpdateRect(HIDD_BM_OBJ(comp->screen->BitMap),
                          0, 0, comp->width, comp->height);
    }
}
```

### 5.3 Skygge-Shader

```glsl
// shadow_vertex.glsl
attribute vec2 a_position;
uniform vec2 u_window_pos;
uniform vec2 u_window_size;
uniform vec2 u_shadow_offset;
uniform vec2 u_screen_size;

varying vec2 v_local_pos;

void main() {
    // Utvid for skygge-blur radius
    vec2 expanded_size = u_window_size + vec2(32.0, 32.0);
    vec2 pos = u_window_pos + u_shadow_offset + a_position * expanded_size - vec2(16.0);
    
    // Normaliser til clip space
    gl_Position = vec4(pos / u_screen_size * 2.0 - 1.0, 0.0, 1.0);
    gl_Position.y = -gl_Position.y;
    
    v_local_pos = a_position * expanded_size - vec2(16.0);
}

// shadow_fragment.glsl
varying vec2 v_local_pos;
uniform vec2 u_window_size;
uniform float u_shadow_blur;
uniform vec4 u_shadow_color;

float roundedBoxSDF(vec2 p, vec2 size, float radius) {
    vec2 q = abs(p) - size + vec2(radius);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

void main() {
    vec2 center = u_window_size * 0.5;
    float dist = roundedBoxSDF(v_local_pos - center, center, 8.0);
    
    // Soft shadow falloff
    float shadow = 1.0 - smoothstep(-u_shadow_blur, u_shadow_blur, dist);
    shadow = pow(shadow, 1.5);  // Gamma-korreksjon for mykere skygge
    
    gl_FragColor = vec4(u_shadow_color.rgb, u_shadow_color.a * shadow);
}
```

---

## 6. HIDD-Integrasjon

### 6.1 Eksisterende HIDD-Metoder

| Metode | Beskrivelse | Bruk |
|--------|-------------|------|
| `HIDD_Gfx_Show` | Vis én bitmap | Enkel modus, krever framebuffer |
| `HIDD_Gfx_ShowViewPorts` | Vis flere bitmaps | Hardware-compositing |
| `HIDD_Gallium_DisplayResource` | Overfør GPU-ressurs til skjerm | Zero-copy optimal |
| `HIDD_BM_UpdateRect` | Oppdater skjermregion | For mirror-mode |

### 6.2 ShowViewPorts for Multi-Window

```c
// Hvis driver støtter ShowViewPorts, bruk det for multi-window
ULONG GFXHIDD__Hidd_Gfx__ShowViewPorts(OOP_Class *cl, OOP_Object *o,
                                        struct pHidd_Gfx_ShowViewPorts *msg)
{
    struct HiddGfxData *data = OOP_INST_DATA(cl, o);
    struct HIDD_ViewPortData *vpd;
    
    // Iterer gjennom alle viewports (bakfra og frem)
    for (vpd = msg->data; vpd; vpd = vpd->Next) {
        OOP_Object *bm = vpd->Bitmap;
        IPTR leftedge, topedge, alpha;
        
        OOP_GetAttr(bm, aHidd_BitMap_LeftEdge, &leftedge);
        OOP_GetAttr(bm, aHidd_BitMap_TopEdge, &topedge);
        alpha = (IPTR)vpd->UserData;  // Alpha sendt via UserData
        
        // Sjekk om bitmap har pipe_resource
        APTR pipe_res = GetBitMapPipeResource(bm);
        
        if (pipe_res && data->supports_gpu_composite) {
            // GPU-path: Bind som tekstur og blend
            GPUCompositeLayer(data, pipe_res, leftedge, topedge, alpha);
        }
        else {
            // CPU-path: Standard blit med alpha
            HIDD_BM_PutAlphaImage(data->framebuffer, ...);
        }
    }
    
    return TRUE;  // Vi støtter ShowViewPorts
}
```

### 6.3 Direkte Gallium DisplayResource

```c
// I Gallium HIDD-driver
VOID METHOD(HiddGallium, Hidd_Gallium, DisplayResource)
{
    struct pHidd_Gallium_DisplayResource *msg = ...;
    struct pipe_context *pipe = GetPipeContext();
    struct pipe_resource *src = (struct pipe_resource *)msg->resource;
    
    // Hent display scanout-buffer
    struct pipe_resource *display = GetDisplayScanout();
    
    // Kopier GPU→GPU (ingen CPU involvert)
    struct pipe_box box = {
        .x = msg->srcx, .y = msg->srcy, .z = 0,
        .width = msg->width, .height = msg->height, .depth = 1
    };
    
    pipe->resource_copy_region(pipe, display, 0,
                               msg->dstx, msg->dsty, 0,
                               src, 0, &box);
    
    // Flush for å sikre at kopien er ferdig
    pipe->flush(pipe, NULL, 0);
}
```

---

## 6. Implementasjonsplan

### Fase 1: pipe_resource i DrawingBoard

**Mål**: DrawingBoard bruker GPU-buffer direkte.

**Filer**:
- `workbench/libs/zunegfx/src/core/drawingboard.c`
- `workbench/libs/zunegfx/src/backends/opengl/opengl_fbo.c`

**Endringer**:
1. Legg til `pipe_resource`-felt i DrawingBoard
2. Opprett FBO fra pipe_resource via dma-buf
3. Alle GL-operasjoner går til FBO

### Fase 2: Zero-Copy Compositor

**Mål**: Compositor bruker DrawingBoard-teksturer direkte.

**Filer**:
- `workbench/libs/zunegfx/src/compositor/gpu_compositor.c` (ny)
- `workbench/libs/zunegfx/src/compositor/shadow_renderer.c` (ny)

**Endringer**:
1. Ny GPUCompositor-klasse
2. Shader-basert skygge-rendering
3. Direkte tekstur-binding (ingen kopiering)

### Fase 3: HIDD-Integrasjon

**Mål**: Presenter uten CPU-kopiering.

**Filer**:
- `workbench/hidds/gallium/gallium_class.c`
- `workbench/devs/monitors/*/compositor.c`

**Endringer**:
1. Implementer fullstendig `HIDD_Gallium_DisplayResource`
2. Utvid ShowViewPorts med alpha-støtte
3. Legg til GPU-composite path i monitor-driver

### Fase 4: Layer-System Bypass (Valgfritt)

**Mål**: Helt GPU-drevet for moderne apps.

**Filer**:
- `rom/intuition/openwindow.c`
- `workbench/libs/muimaster/classes/window.c`

**Endringer**:
1. Ny vindus-tag `WA_GPUNative`
2. Vinduer med denne tagen går rett til compositor
3. Layer-systemet informeres men gjør ingen blitting

---

## 7. Ytelsessammenligning

| Operasjon | Nåværende | Med Zero-Copy |
|-----------|-----------|---------------|
| App → DrawingBoard | CPU write | GPU draw |
| DrawingBoard → Compositor | glTexImage2D (CPU→GPU) | Zero-copy bind |
| Compositor blending | GPU | GPU |
| Skygge-rendering | Ikke støttet / CPU | GPU shader |
| Resultat → Skjerm | glReadPixels + HIDD Show | DisplayResource |
| **Totalt** | **3-4 kopieringer** | **0 kopieringer** |

### Forventet Forbedring

- **Latency**: 50-70% reduksjon (fjerner CPU-GPU synkronisering)
- **Throughput**: 2-3x høyere (GPU gjør alt)
- **CPU-bruk**: 80% reduksjon (CPU gjør nesten ingenting)

---

## 9. Kompatibilitet

### 9.1 Fallback-Strategi

```
Sjekk tilgjengelige funksjoner:
│
├── Gallium + pipe_resource? 
│   └── JA: Zero-copy path (optimal)
│
├── OpenGL + FBO?
│   └── JA: GPU compositor, men med sync (god)
│
├── CyberGraphics?
│   └── JA: CPU compositor, software blending (ok)
│
└── Bare graphics.library
    └── Standard layer-basert rendering (bakoverkompatibel)
```

### 9.2 Runtime-Deteksjon

```c
BOOL DetectGPUCapabilities(struct GPUCompositor *comp)
{
    // Sjekk Gallium
    comp->has_gallium = (OpenLibrary("gallium.library", 0) != NULL);
    
    // Sjekk GL-extensions
    if (comp->gl_context) {
        const char *ext = glGetString(GL_EXTENSIONS);
        comp->has_fbo = (strstr(ext, "GL_ARB_framebuffer_object") != NULL);
        comp->has_dma_buf = (strstr(ext, "EGL_MESA_image_dma_buf_export") != NULL);
    }
    
    // Velg beste path
    if (comp->has_gallium && comp->has_dma_buf) {
        comp->mode = COMPOSITOR_ZERO_COPY;
    }
    else if (comp->has_fbo) {
        comp->mode = COMPOSITOR_GPU_WITH_SYNC;
    }
    else {
        comp->mode = COMPOSITOR_SOFTWARE;
    }
    
    return TRUE;
}
```

---

## 9. Konklusjon

### Anbefaling

**Implementer direkte HIDD-integrasjon med pipe_resource-deling** for å oppnå:

1. **Zero-copy rendering** - Alt forblir på GPU
2. **Hardware-akselererte skygger** - Shader-basert, ingen CPU
3. **Bakoverkompatibilitet** - Fallback til eksisterende metoder
4. **Inkrementell utvikling** - Kan implementeres stegvis

### Prioritert Rekkefølge

1. **DrawingBoard med pipe_resource** - Grunnlaget for alt annet
2. **GPUCompositor** - Erstatter eksisterende compositor
3. **Skygge-shader** - Visuell forbedring
4. **HIDD DisplayResource** - Full zero-copy

### Estimert Innsats

| Fase | Kompleksitet | Avhengigheter |
|------|--------------|---------------|
| DrawingBoard GPU | Medium | Gallium headers |
| GPUCompositor | Medium | DrawingBoard GPU |
| Skygge-shader | Lav | GPUCompositor |
| HIDD-integrasjon | Høy | Alt over |

---

## Vedlegg: Nøkkelfiler

| Komponent | Plassering |
|-----------|------------|
| Gallium HIDD | `workbench/hidds/gallium/` |
| GFX HIDD | `rom/hidds/gfx/gfx_hiddclass.c` |
| Compositor HIDD | `workbench/devs/monitors/Compositor/` |
| zunegfx | `workbench/libs/zunegfx/` |
| Mesa GL | `workbench/libs/mesa/` |
