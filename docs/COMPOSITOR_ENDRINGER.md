# Konkrete Endringer i Eksisterende HIDD Compositor

Dette dokumentet beskriver de faktiske kodeendringene som trengs i `workbench/devs/monitors/Compositor/` for å støtte GPU-akselerert rendering.

---

## 1. Eksisterende HIDD Compositor Arkitektur

### 1.1 Filstruktur

```
workbench/devs/monitors/Compositor/
├── include/
│   └── compositor.h          # Offentlig HIDD interface
├── compositor_intern.h       # Interne strukturer
├── compositor_class.c        # Hovedimplementasjon
├── compositor_startup.c      # Initialisering
├── displaymode.c             # Modus-håndtering
└── mmakefile.src             # Build-konfigurasjon
```

### 1.2 Eksisterende StackBitMapNode

```c
struct StackBitMapNode
{
    struct MinNode  n;
    OOP_Object      *bm;              /* HIDD BitMap objekt */
    struct Region   *screenregion;   /* Synlig region på skjerm */
    SIPTR           leftedge;        /* X-offset */
    SIPTR           topedge;         /* Y-offset */
    IPTR            sbmflags;        /* Flagg (COMPF_ALPHA, STACKNODEF_VISIBLE, etc.) */
    struct Hook     *prealphacomphook; /* Hook før alpha-compositing */
};

/* sbmflags bits */
#define STACKNODEB_VISIBLE       16
#define STACKNODEF_VISIBLE       (1 << STACKNODEB_VISIBLE)
#define STACKNODEB_DISPLAYABLE   17
#define STACKNODEF_DISPLAYABLE   (1 << STACKNODEB_DISPLAYABLE)
```

### 1.3 Eksisterende HIDDCompositorData

```c
struct HIDDCompositorData
{
    struct GfxBase              *GraphicsBase;
    struct IntuitionBase        *IntuitionBase;

    ULONG                       capabilities;    /* COMPF_ABOVE, COMPF_BELOW, COMPF_ALPHA, etc. */
    ULONG                       flags;           /* COMPSTATEF_HASALPHA, COMPSTATEF_DEEPLUT */

    /* Bitmaps */
    OOP_Object                  *displaybitmap;  /* Composited resultat som vises */
    OOP_Object                  *intermedbitmap; /* Mellom-bitmap for alpha-blending */
    OOP_Object                  *screenbitmap;   /* Resultat av HIDD_Gfx_Show() */
    OOP_Object                  *topbitmap;      /* Øverste bitmap i stakken */

    struct Rectangle            displayrect;     /* Dimensjoner av synlig modus */
    struct Region               *alpharegion;    /* Region som krever alpha-blending */

    struct MinList              bitmapstack;     /* Liste av StackBitMapNode (z-order) */
    struct SignalSemaphore      semaphore;

    struct Hook                 *backfillhook;   /* Hook for bakgrunnsfylling */

    OOP_Object                  *gfx;            /* GFX driver objekt */
    OOP_Object                  *fb;             /* Framebuffer bitmap (hvis tilgjengelig) */
    OOP_Object                  *gc;             /* GC objekt for tegneoperasjoner */

    ULONG                       displayid;
    HIDDT_ModeID                displaymode;     /* ModeID av synlig modus */
    UBYTE                       displaydepth;

    struct Hook                 defaultbackfill;
    BOOL                        modeschanged;
};

/* Eksisterende flagg */
#define COMPSTATEB_HASALPHA     0
#define COMPSTATEF_HASALPHA     (1 << COMPSTATEB_HASALPHA)
#define COMPSTATEB_DEEPLUT      1
#define COMPSTATEF_DEEPLUT      (1 << COMPSTATEB_DEEPLUT)
```

### 1.4 Eksisterende Metoder

| Metode | Beskrivelse |
|--------|-------------|
| `BitMapStackChanged` | Kalles når skjerm-stakken endres (ny skjerm, z-order endring) |
| `BitMapRectChanged` | Kalles når en region av en bitmap er endret |
| `BitMapPositionChange` | Kalles når en bitmap flyttes (drag/scroll) |
| `BitMapValidate` | Validerer om en bitmap kan composites |
| `BitMapEnable` | Aktiverer compositing for en bitmap |

### 1.5 Eksisterende Rendering-funksjoner

```c
/* Tegner én bitmap - CPU-basert */
static inline void HIDDCompositorRedrawBitmap(
    struct HIDDCompositorData *compdata,
    OOP_Object *renderTarget,
    struct StackBitMapNode *n,
    struct Rectangle *rect)
{
    if (!(n->sbmflags & COMPF_ALPHA)) {
        /* Vanlig blit */
        HIDD_Gfx_CopyBox(compdata->gfx, n->bm, ...);
    } else {
        /* Alpha-blending via CPU */
        HIDD_BM_ObtainDirectAccess(n->bm, &baseaddress, ...);
        HIDD_BM_PutAlphaImage(renderTarget, compdata->gfx, baseaddress, ...);
        HIDD_BM_ReleaseDirectAccess(n->bm);
    }
}

/* Tegner alle synlige regioner */
static VOID HIDDCompositorRedrawVisibleRegions(
    struct HIDDCompositorData *compdata,
    struct Rectangle *drawrect);

/* Tegner alpha-regioner (bakfra og frem) */
static VOID HIDDCompositorRedrawAlphaRegions(
    struct HIDDCompositorData *compdata,
    struct Rectangle *drawrect);
```

### 1.6 Eksisterende Alpha-støtte

Compositoren har allerede grunnleggende alpha-støtte:

- **COMPF_ALPHA** flagg på StackBitMapNode og Screen
- **COMPSTATEF_HASALPHA** flagg på compositor
- **intermedbitmap** for å samle alpha-blending før final blit
- **HIDD_BM_PutAlphaImage()** for CPU-basert alpha-blending
- **prealphacomphook** for pre-processing før alpha (f.eks. bakgrunns-sampling)
- **alpharegion** for å tracke hvilke områder som trenger alpha-blending

---

## 2. Oversikt: Eksisterende vs Ny Arkitektur

### Eksisterende Compositor (CPU-basert)

```
┌─────────────────────────────────────────────────────────────┐
│  HIDDCompositorData                                         │
│                                                             │
│  displaybitmap ──► HIDD BitMap (CPU)                        │
│  intermedbitmap ──► HIDD BitMap for alpha (CPU)             │
│  bitmapstack ──► Liste av StackBitMapNode                   │
│                                                             │
│  Rendering: HIDD_Gfx_CopyBox() / HIDD_BM_PutAlphaImage()    │
│             (Alt på CPU via HIDD)                           │
└─────────────────────────────────────────────────────────────┘
```

### Ny Compositor (GPU-akselerert)

```
┌─────────────────────────────────────────────────────────────┐
│  HIDDCompositorData                                         │
│                                                             │
│  displaybitmap ──► HIDD BitMap (uendret for kompatibilitet) │
│  gpu.output_fbo ──► OpenGL FBO (NY)                         │
│  gpu.gl_context ──► GL-kontekst (NY)                        │
│  bitmapstack ──► Liste av StackBitMapNode (utvidet)         │
│                                                             │
│  Rendering: OpenGL shaders for blending + skygger           │
│             Fallback til HIDD hvis ingen GPU                │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Endringer i compositor_intern.h

### 3.1 Utvidet StackBitMapNode

```c
struct StackBitMapNode
{
    /* Eksisterende felter - UENDRET */
    struct MinNode  n;
    OOP_Object      *bm;
    struct Region   *screenregion;
    SIPTR           leftedge;
    SIPTR           topedge;
    IPTR            sbmflags;
    struct Hook     *prealphacomphook;
    
    /* NY: GPU-rendering støtte */
    struct {
        ULONG       texture_id;         /* Cachet GPU-tekstur */
        BOOL        is_gpu_native;      /* TRUE hvis zunegfx DrawingBoard/FBO */
        APTR        pipe_resource;      /* Gallium ressurs (zero-copy) */
        struct Rectangle dirty_rect;    /* Dirty region for inkrementell upload */
        BOOL        needs_upload;       /* TRUE hvis bitmap er endret */
    } gpu;
};
```

### 3.2 Utvidet HIDDCompositorData

```c
struct HIDDCompositorData
{
    /* Eksisterende felter - UENDRET */
    struct GfxBase              *GraphicsBase;
    struct IntuitionBase        *IntuitionBase;
    ULONG                       capabilities;
    ULONG                       flags;
    OOP_Object                  *displaybitmap;
    OOP_Object                  *intermedbitmap;
    OOP_Object                  *screenbitmap;
    OOP_Object                  *topbitmap;
    struct Rectangle            displayrect;
    struct Region               *alpharegion;
    struct MinList              bitmapstack;
    struct SignalSemaphore      semaphore;
    struct Hook                 *backfillhook;
    OOP_Object                  *gfx;
    OOP_Object                  *fb;
    OOP_Object                  *gc;
    ULONG                       displayid;
    HIDDT_ModeID                displaymode;
    UBYTE                       displaydepth;
    struct Hook                 defaultbackfill;
    BOOL                        modeschanged;
    
    /* NY: GPU-akselerering */
    struct {
        BOOL        available;          /* TRUE hvis GPU kan brukes */
        APTR        gl_context;         /* OpenGL kontekst */
        ULONG       output_fbo;         /* Output FBO */
        ULONG       output_texture;     /* FBO color attachment */
        APTR        output_pipe_res;    /* For zero-copy til HIDD */
        
        /* Shaders */
        ULONG       composite_shader;   /* Standard blending */
        ULONG       shadow_shader;      /* Vindus-skygger */
        
        /* Uniforms */
        LONG        u_texture;
        LONG        u_alpha;
        LONG        u_screen_size;
        LONG        u_window_pos;
        LONG        u_window_size;
        LONG        u_shadow_color;
        LONG        u_shadow_offset;
        
        /* Geometri */
        ULONG       quad_vbo;           /* Vertex buffer for quads */
        ULONG       quad_vao;           /* Vertex array object */
    } gpu;
    
    /* NY: Gallium integrasjon */
    OOP_Object      *galliumHidd;       /* Gallium HIDD for zero-copy */
};

/* NY: GPU-modus flagg */
#define COMPSTATEB_GPUACCEL     2
#define COMPSTATEF_GPUACCEL     (1 << COMPSTATEB_GPUACCEL)
```

---

## 4. Endringer i compositor_class.c

### 4.1 Ny funksjon: GPU-initialisering

```c
/* NY FUNKSJON - Legg til etter CompositorParseConfig() */

static BOOL InitGPUCompositor(struct HIDDCompositorData *compdata)
{
    /* Sjekk om Gallium/Mesa er tilgjengelig */
    compdata->galliumHidd = OOP_FindClass(CLID_Hidd_Gallium);
    
    /* Prøv å opprette GL-kontekst */
    compdata->gpu.gl_context = glACreateContext(NULL);  /* Offscreen */
    if (!compdata->gpu.gl_context) {
        compdata->gpu.available = FALSE;
        return FALSE;
    }
    
    glAMakeCurrent(compdata->gpu.gl_context);
    
    /* Opprett output FBO */
    glGenFramebuffers(1, &compdata->gpu.output_fbo);
    glGenTextures(1, &compdata->gpu.output_texture);
    
    /* Kompiler shaders */
    compdata->gpu.composite_shader = CompileCompositeShader();
    compdata->gpu.shadow_shader = CompileShadowShader();
    
    if (!compdata->gpu.composite_shader || !compdata->gpu.shadow_shader) {
        CleanupGPUCompositor(compdata);
        return FALSE;
    }
    
    /* Hent uniform locations */
    compdata->gpu.u_texture = glGetUniformLocation(
        compdata->gpu.composite_shader, "u_texture");
    compdata->gpu.u_alpha = glGetUniformLocation(
        compdata->gpu.composite_shader, "u_alpha");
    compdata->gpu.u_screen_size = glGetUniformLocation(
        compdata->gpu.composite_shader, "u_screen_size");
    /* ... etc for andre uniforms ... */
    
    /* Opprett quad VBO/VAO */
    CreateQuadGeometry(&compdata->gpu.quad_vbo, &compdata->gpu.quad_vao);
    
    compdata->gpu.available = TRUE;
    compdata->flags |= COMPSTATEF_GPUACCEL;
    
    D(bug("[Compositor] GPU acceleration initialized\n"));
    return TRUE;
}

static void CleanupGPUCompositor(struct HIDDCompositorData *compdata)
{
    if (compdata->gpu.gl_context) {
        glAMakeCurrent(compdata->gpu.gl_context);
        
        if (compdata->gpu.output_fbo)
            glDeleteFramebuffers(1, &compdata->gpu.output_fbo);
        if (compdata->gpu.output_texture)
            glDeleteTextures(1, &compdata->gpu.output_texture);
        if (compdata->gpu.composite_shader)
            glDeleteProgram(compdata->gpu.composite_shader);
        if (compdata->gpu.shadow_shader)
            glDeleteProgram(compdata->gpu.shadow_shader);
        if (compdata->gpu.quad_vbo)
            glDeleteBuffers(1, &compdata->gpu.quad_vbo);
        if (compdata->gpu.quad_vao)
            glDeleteVertexArrays(1, &compdata->gpu.quad_vao);
            
        glADestroyContext(compdata->gpu.gl_context);
        compdata->gpu.gl_context = NULL;
    }
    
    compdata->gpu.available = FALSE;
    compdata->flags &= ~COMPSTATEF_GPUACCEL;
}
```

### 4.2 Endring i Root_New

```c
OOP_Object *METHOD(Compositor, Root, New)
{
    /* ... eksisterende kode ... */
    
    if ((compdata->gfx) && (compdata->gc))
    {
        /* NY: Prøv å initialisere GPU-akselerering */
        if (!InitGPUCompositor(compdata)) {
            D(bug("[Compositor] GPU acceleration not available, using software fallback\n"));
        }
        
        return o;
    }
    
    /* ... feilhåndtering ... */
}
```

### 4.3 Endring i Root_Dispose

```c
void METHOD(Compositor, Root, Dispose)
{
    struct HIDDCompositorData *compdata = OOP_INST_DATA(cl, o);
    
    /* NY: Rydd opp GPU-ressurser */
    CleanupGPUCompositor(compdata);
    
    OOP_DoSuperMethod(cl, o, &msg->mID);
}
```

### 4.4 NY funksjon: GPU-basert RedrawBitmap

```c
/* NY FUNKSJON - GPU-versjon av HIDDCompositorRedrawBitmap */

static void GPUCompositorRedrawBitmap(struct HIDDCompositorData *compdata,
                                       struct StackBitMapNode *n,
                                       struct Rectangle *rect)
{
    /* Sørg for at vi har en gyldig GPU-tekstur */
    if (n->gpu.texture_id == 0) {
        glGenTextures(1, &n->gpu.texture_id);
        glBindTexture(GL_TEXTURE_2D, n->gpu.texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        n->gpu.needs_upload = TRUE;
    }
    
    /* Upload hvis nødvendig (legacy vinduer) */
    if (n->gpu.needs_upload && !n->gpu.is_gpu_native) {
        UploadBitmapToTexture(compdata, n);
    }
    
    /* Bind tekstur */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, n->gpu.texture_id);
    
    /* Bruk composite shader */
    glUseProgram(compdata->gpu.composite_shader);
    glUniform1i(compdata->gpu.u_texture, 0);
    glUniform2f(compdata->gpu.u_screen_size, 
                compdata->displayrect.MaxX - compdata->displayrect.MinX + 1,
                compdata->displayrect.MaxY - compdata->displayrect.MinY + 1);
    
    /* Beregn alpha */
    float alpha = 1.0f;
    if (n->sbmflags & COMPF_ALPHA) {
        /* TODO: Hent faktisk alpha fra Screen-attributter */
        alpha = 0.9f;
    }
    glUniform1f(compdata->gpu.u_alpha, alpha);
    
    /* Tegn teksturert quad */
    DrawTexturedQuad(compdata, 
                     rect->MinX, rect->MinY,
                     rect->MaxX - rect->MinX + 1,
                     rect->MaxY - rect->MinY + 1,
                     n->leftedge, n->topedge);
}

static void UploadBitmapToTexture(struct HIDDCompositorData *compdata,
                                   struct StackBitMapNode *n)
{
    UBYTE *baseaddress;
    ULONG width, height, banksize, memsize;
    IPTR modulo;
    
    if (!HIDD_BM_ObtainDirectAccess(n->bm, &baseaddress, 
                                     &width, &height, &banksize, &memsize))
        return;
    
    OOP_GetAttr(n->bm, aHidd_BitMap_BytesPerRow, &modulo);
    OOP_GetAttr(n->bm, aHidd_BitMap_Width, (IPTR*)&width);
    OOP_GetAttr(n->bm, aHidd_BitMap_Height, (IPTR*)&height);
    
    glBindTexture(GL_TEXTURE_2D, n->gpu.texture_id);
    
    /* Sjekk om dette er første upload eller inkrementell */
    if (n->gpu.dirty_rect.MinX > n->gpu.dirty_rect.MaxX) {
        /* Full upload */
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 
                     width, height, 0,
                     GL_BGRA, GL_UNSIGNED_BYTE, baseaddress);
    } else {
        /* Inkrementell upload av dirty region */
        struct Rectangle *d = &n->gpu.dirty_rect;
        ULONG dw = d->MaxX - d->MinX + 1;
        ULONG dh = d->MaxY - d->MinY + 1;
        
        glPixelStorei(GL_UNPACK_ROW_LENGTH, modulo / 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        d->MinX, d->MinY, dw, dh,
                        GL_BGRA, GL_UNSIGNED_BYTE,
                        baseaddress + d->MinY * modulo + d->MinX * 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    
    HIDD_BM_ReleaseDirectAccess(n->bm);
    
    /* Nullstill dirty tracking */
    n->gpu.needs_upload = FALSE;
    n->gpu.dirty_rect.MinX = width;
    n->gpu.dirty_rect.MaxX = 0;
}
```

### 4.5 Endring i HIDDCompositorRedrawVisibleRegions

```c
static VOID HIDDCompositorRedrawVisibleRegions(struct HIDDCompositorData *compdata, 
                                                struct Rectangle *drawrect)
{
    /* NY: Sjekk om vi skal bruke GPU */
    if (compdata->flags & COMPSTATEF_GPUACCEL) {
        GPUCompositorRedrawVisibleRegions(compdata, drawrect);
        return;
    }
    
    /* ... eksisterende CPU-kode uendret ... */
}

/* NY FUNKSJON */
static VOID GPUCompositorRedrawVisibleRegions(struct HIDDCompositorData *compdata,
                                               struct Rectangle *drawrect)
{
    struct StackBitMapNode *n;
    ULONG screen_width = compdata->displayrect.MaxX - compdata->displayrect.MinX + 1;
    ULONG screen_height = compdata->displayrect.MaxY - compdata->displayrect.MinY + 1;
    
    glAMakeCurrent(compdata->gpu.gl_context);
    
    /* Bind output FBO */
    glBindFramebuffer(GL_FRAMEBUFFER, compdata->gpu.output_fbo);
    glViewport(0, 0, screen_width, screen_height);
    
    /* Clear med bakgrunnsfarge */
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    /* Enable blending */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    /* Tegn alle synlige bitmaps (bakfra og frem) */
    ForeachNode(&compdata->bitmapstack, n)
    {
        if (!(n->sbmflags & STACKNODEF_VISIBLE))
            continue;
            
        if (!n->screenregion)
            continue;
        
        struct RegionRectangle *srrect = n->screenregion->RegionRectangle;
        while (srrect)
        {
            struct Rectangle rect;
            rect.MinX = srrect->bounds.MinX + n->screenregion->bounds.MinX;
            rect.MinY = srrect->bounds.MinY + n->screenregion->bounds.MinY;
            rect.MaxX = srrect->bounds.MaxX + n->screenregion->bounds.MinX;
            rect.MaxY = srrect->bounds.MaxY + n->screenregion->bounds.MinY;
            
            if (!drawrect || AndRectRect(drawrect, &rect, &rect))
            {
                /* Tegn skygge først (hvis aktivert) */
                if (ShouldDrawShadow(compdata, n)) {
                    GPUDrawWindowShadow(compdata, n, &rect);
                }
                
                /* Tegn vinduet */
                GPUCompositorRedrawBitmap(compdata, n, &rect);
            }
            
            srrect = srrect->Next;
        }
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    /* Overfør resultat til skjerm */
    GPUCompositorPresent(compdata, drawrect);
}

static BOOL ShouldDrawShadow(struct HIDDCompositorData *compdata, 
                              struct StackBitMapNode *n)
{
    /* Tegn skygge kun for vinduer som ikke dekker hele skjermen */
    if (n == (struct StackBitMapNode *)compdata->bitmapstack.mlh_Head)
        return FALSE;  /* Ingen skygge for bakgrunns-skjerm */
    
    /* TODO: Sjekk om skygger er aktivert i preferanser */
    return TRUE;
}
```

### 4.6 NY funksjon: Present til skjerm

```c
/* NY FUNKSJON */
static void GPUCompositorPresent(struct HIDDCompositorData *compdata,
                                  struct Rectangle *drawrect)
{
    ULONG width = compdata->displayrect.MaxX - compdata->displayrect.MinX + 1;
    ULONG height = compdata->displayrect.MaxY - compdata->displayrect.MinY + 1;
    
    /* Metode 1: Zero-copy via Gallium (best ytelse) */
    if (compdata->galliumHidd && compdata->gpu.output_pipe_res) {
        struct pHidd_Gallium_DisplayResource msg = {
            .mID = OOP_GetMethodID(IID_Hidd_Gallium, moHidd_Gallium_DisplayResource),
            .resource = compdata->gpu.output_pipe_res,
            .srcx = drawrect ? drawrect->MinX : 0,
            .srcy = drawrect ? drawrect->MinY : 0,
            .bitmap = NULL,  /* Direkte til display */
            .dstx = drawrect ? drawrect->MinX : 0,
            .dsty = drawrect ? drawrect->MinY : 0,
            .width = drawrect ? (drawrect->MaxX - drawrect->MinX + 1) : width,
            .height = drawrect ? (drawrect->MaxY - drawrect->MinY + 1) : height
        };
        OOP_DoMethod(compdata->galliumHidd, (OOP_Msg)&msg);
        return;
    }
    
    /* Metode 2: Les tilbake til CPU bitmap (fallback) */
    if (compdata->displaybitmap) {
        UBYTE *baseaddress;
        ULONG bm_width, bm_height, banksize, memsize;
        
        if (HIDD_BM_ObtainDirectAccess(compdata->displaybitmap, 
                                        &baseaddress, &bm_width, &bm_height, 
                                        &banksize, &memsize))
        {
            glBindFramebuffer(GL_FRAMEBUFFER, compdata->gpu.output_fbo);
            glReadPixels(0, 0, width, height, 
                         GL_BGRA, GL_UNSIGNED_BYTE, baseaddress);
            
            HIDD_BM_ReleaseDirectAccess(compdata->displaybitmap);
            
            /* Oppdater skjerm via HIDD */
            HIDD_BM_UpdateRect(compdata->displaybitmap, 0, 0, width, height);
        }
    }
}
```

### 4.7 NY funksjon: Skygge-rendering

```c
/* NY FUNKSJON */
static void GPUDrawWindowShadow(struct HIDDCompositorData *compdata,
                                 struct StackBitMapNode *n,
                                 struct Rectangle *rect)
{
    IPTR bm_width, bm_height;
    OOP_GetAttr(n->bm, aHidd_BitMap_Width, &bm_width);
    OOP_GetAttr(n->bm, aHidd_BitMap_Height, &bm_height);
    
    glUseProgram(compdata->gpu.shadow_shader);
    
    /* Skjermstørrelse */
    glUniform2f(compdata->gpu.u_screen_size,
                compdata->displayrect.MaxX - compdata->displayrect.MinX + 1,
                compdata->displayrect.MaxY - compdata->displayrect.MinY + 1);
    
    /* Vindusposisjon og størrelse */
    glUniform2f(compdata->gpu.u_window_pos, (float)n->leftedge, (float)n->topedge);
    glUniform2f(compdata->gpu.u_window_size, (float)bm_width, (float)bm_height);
    
    /* Skygge-offset (nedover og til høyre) */
    glUniform2f(compdata->gpu.u_shadow_offset, 8.0f, 8.0f);
    
    /* Skygge-farge (halvtransparent svart) */
    glUniform4f(compdata->gpu.u_shadow_color, 0.0f, 0.0f, 0.0f, 0.4f);
    
    /* Tegn skygge-quad (litt større enn vinduet for blur) */
    DrawShadowQuad(compdata,
                   n->leftedge - 16, n->topedge - 16,
                   bm_width + 32, bm_height + 32);
}
```

---

## 5. Endring i StackBitMapNode-allokering

### 5.1 I BitMapStackChanged

```c
OOP_Object *METHOD(Compositor, Hidd_Compositor, BitMapStackChanged)
{
    /* ... eksisterende kode ... */
    
    for (vpdata = msg->data; vpdata; vpdata = vpdata->Next)
    {
        n = AllocMem(sizeof(struct StackBitMapNode), MEMF_ANY | MEMF_CLEAR);
        if (!n) { /* ... feilhåndtering ... */ }
        
        /* Eksisterende felt-initialisering */
        n->bm = vpdata->Bitmap;
        n->sbmflags = STACKNODEF_DISPLAYABLE;
        n->leftedge = vpdata->vpe->ViewPort->DxOffset;
        n->topedge = vpdata->vpe->ViewPort->DyOffset;
        n->screenregion = NewRegion();
        
        /* NY: Initialiser GPU-felter */
        n->gpu.texture_id = 0;
        n->gpu.is_gpu_native = FALSE;
        n->gpu.pipe_resource = NULL;
        n->gpu.needs_upload = TRUE;
        n->gpu.dirty_rect.MinX = 0x7FFF;  /* Tom rect */
        n->gpu.dirty_rect.MaxX = 0;
        
        /* NY: Sjekk om dette er en GPU-native bitmap (zunegfx DrawingBoard) */
        if (compdata->flags & COMPSTATEF_GPUACCEL) {
            APTR pipe_res = NULL;
            OOP_GetAttr(n->bm, aHidd_BitMap_PipeResource, (IPTR*)&pipe_res);
            if (pipe_res) {
                n->gpu.is_gpu_native = TRUE;
                n->gpu.pipe_resource = pipe_res;
                n->gpu.needs_upload = FALSE;  /* Zero-copy! */
            }
        }
        
        AddTail((struct List *)&compdata->bitmapstack, (struct Node *)n);
    }
    
    /* ... resten av eksisterende kode ... */
}
```

### 5.2 I BitMapRectChanged - Dirty Tracking

```c
VOID METHOD(Compositor, Hidd_Compositor, BitMapRectChanged)
{
    /* ... eksisterende kode ... */
    
    n = HIDDCompositorFindBitMapStackNode(compdata, msg->bm);
    if (n && (n->sbmflags & STACKNODEF_VISIBLE))
    {
        /* NY: Oppdater dirty rect for GPU-modus */
        if ((compdata->flags & COMPSTATEF_GPUACCEL) && !n->gpu.is_gpu_native)
        {
            /* Utvid dirty rect til å inkludere endret område */
            if (msg->x < n->gpu.dirty_rect.MinX)
                n->gpu.dirty_rect.MinX = msg->x;
            if (msg->y < n->gpu.dirty_rect.MinY)
                n->gpu.dirty_rect.MinY = msg->y;
            if (msg->x + msg->width - 1 > n->gpu.dirty_rect.MaxX)
                n->gpu.dirty_rect.MaxX = msg->x + msg->width - 1;
            if (msg->y + msg->height - 1 > n->gpu.dirty_rect.MaxY)
                n->gpu.dirty_rect.MaxY = msg->y + msg->height - 1;
            
            n->gpu.needs_upload = TRUE;
        }
        
        /* ... eksisterende redraw-kode ... */
    }
}
```

### 5.3 Frigjøring av GPU-teksturer

```c
static VOID HIDDCompositorPurgeBitMapStack(struct HIDDCompositorData *compdata)
{
    struct StackBitMapNode *curr, *next;
    
    ForeachNodeSafe(&compdata->bitmapstack, curr, next)
    {
        if (curr->screenregion)
            DisposeRegion(curr->screenregion);
        
        /* NY: Frigjør GPU-tekstur */
        if (curr->gpu.texture_id && compdata->gpu.gl_context) {
            glAMakeCurrent(compdata->gpu.gl_context);
            glDeleteTextures(1, &curr->gpu.texture_id);
        }
        
        FreeMem(curr, sizeof(struct StackBitMapNode));
    }
    
    NEWLIST(&compdata->bitmapstack);
}
```

---

## 6. Ny fil: gpu_shaders.c

```c
/* workbench/devs/monitors/Compositor/gpu_shaders.c */

#include <GL/gl.h>

/*
 * Composite Shader - Tegner teksturert quad med alpha
 */
static const char *composite_vertex_src = 
    "#version 120\n"
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "uniform vec2 u_screen_size;\n"
    "void main() {\n"
    "    v_texcoord = a_texcoord;\n"
    "    vec2 pos = a_position / u_screen_size * 2.0 - 1.0;\n"
    "    gl_Position = vec4(pos.x, -pos.y, 0.0, 1.0);\n"
    "}\n";

static const char *composite_fragment_src =
    "#version 120\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform float u_alpha;\n"
    "void main() {\n"
    "    vec4 color = texture2D(u_texture, v_texcoord);\n"
    "    gl_FragColor = vec4(color.rgb, color.a * u_alpha);\n"
    "}\n";

/*
 * Shadow Shader - Tegner myk skygge med SDF (Signed Distance Field)
 */
static const char *shadow_vertex_src =
    "#version 120\n"
    "attribute vec2 a_position;\n"
    "uniform vec2 u_screen_size;\n"
    "uniform vec2 u_window_pos;\n"
    "uniform vec2 u_window_size;\n"
    "uniform vec2 u_shadow_offset;\n"
    "varying vec2 v_local_pos;\n"
    "void main() {\n"
    "    vec2 expanded = u_window_size + vec2(32.0);\n"
    "    vec2 pos = u_window_pos + u_shadow_offset + a_position * expanded - vec2(16.0);\n"
    "    gl_Position = vec4(pos / u_screen_size * 2.0 - 1.0, 0.0, 1.0);\n"
    "    gl_Position.y = -gl_Position.y;\n"
    "    v_local_pos = a_position * expanded - vec2(16.0);\n"
    "}\n";

static const char *shadow_fragment_src =
    "#version 120\n"
    "varying vec2 v_local_pos;\n"
    "uniform vec2 u_window_size;\n"
    "uniform vec4 u_shadow_color;\n"
    "\n"
    "float roundedBoxSDF(vec2 p, vec2 size, float radius) {\n"
    "    vec2 q = abs(p) - size + vec2(radius);\n"
    "    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 center = u_window_size * 0.5;\n"
    "    float dist = roundedBoxSDF(v_local_pos - center, center, 8.0);\n"
    "    float shadow = 1.0 - smoothstep(-16.0, 16.0, dist);\n"
    "    gl_FragColor = vec4(u_shadow_color.rgb, u_shadow_color.a * shadow);\n"
    "}\n";

/*
 * Shader kompilering
 */
static ULONG CompileShader(GLenum type, const char *source)
{
    ULONG shader = glCreateShader(type);
    GLint status;
    
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        D(bug("[Compositor] Shader compile error: %s\n", log));
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

static ULONG CompileShaderProgram(const char *vert_src, const char *frag_src)
{
    ULONG vert = CompileShader(GL_VERTEX_SHADER, vert_src);
    ULONG frag = CompileShader(GL_FRAGMENT_SHADER, frag_src);
    GLint status;
    
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return 0;
    }
    
    ULONG prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (!status) {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        D(bug("[Compositor] Shader link error: %s\n", log));
        glDeleteProgram(prog);
        prog = 0;
    }
    
    glDeleteShader(vert);
    glDeleteShader(frag);
    
    return prog;
}

ULONG CompileCompositeShader(void)
{
    return CompileShaderProgram(composite_vertex_src, composite_fragment_src);
}

ULONG CompileShadowShader(void)
{
    return CompileShaderProgram(shadow_vertex_src, shadow_fragment_src);
}

/*
 * Quad geometri
 */
void CreateQuadGeometry(ULONG *vbo, ULONG *vao)
{
    static const float quad_vertices[] = {
        /* pos.x, pos.y, tex.u, tex.v */
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };
    
    glGenBuffers(1, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    
    glGenVertexArrays(1, vao);
    glBindVertexArray(*vao);
    
    glEnableVertexAttribArray(0);  /* a_position */
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);  /* a_texcoord */
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void DrawTexturedQuad(struct HIDDCompositorData *compdata,
                      WORD x, WORD y, WORD width, WORD height,
                      WORD src_x, WORD src_y)
{
    /* TODO: Implementer faktisk quad-tegning med transformasjoner */
    glBindVertexArray(compdata->gpu.quad_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

void DrawShadowQuad(struct HIDDCompositorData *compdata,
                    WORD x, WORD y, WORD width, WORD height)
{
    glBindVertexArray(compdata->gpu.quad_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}
```

---

## 7. Endringer i mmakefile.src

```makefile
#MM workbench-devs-monitors-Compositor : \
#MM     kernel-hidd-gfx-includes \
#MM     includes-copy \
#MM     workbench-libs-mesa    # NY: For OpenGL støtte

FILES := compositor_class compositor_startup displaymode gpu_shaders

USER_CPPFLAGS := -DUSE_FAST_HIDD_BM

# NY: OpenGL includes og linking
USER_INCLUDES := -I$(AROS_INCLUDES)/GL
USER_LDFLAGS := -lGL

%build_prog mmake=workbench-devs-monitors-Compositor \
    targetdir=$(AROS_DEVS)/Monitors files=$(FILES) \
    uselibs="hiddstubs oop"
```

---

## 8. Oppsummering av Endringer

| Fil | Type Endring | Beskrivelse |
|-----|--------------|-------------|
| `compositor_intern.h` | Utvidelse | GPU-felter i StackBitMapNode og HIDDCompositorData |
| `compositor_class.c` | Endring | GPU-init, GPU-rendering funksjoner, dirty tracking |
| `gpu_shaders.c` | **Ny fil** | Shader-kode og kompilering |
| `mmakefile.src` | Endring | Ny fil, Mesa-avhengighet, GL-linking |

### Antall Linjer (estimat)

| Komponent | Linjer |
|-----------|--------|
| Nye strukturfelter (`compositor_intern.h`) | ~35 |
| GPU-init/cleanup | ~80 |
| GPU-rendering funksjoner | ~180 |
| Dirty tracking | ~30 |
| gpu_shaders.c | ~200 |
| **Totalt** | **~525 linjer ny kode** |

### Bakoverkompatibilitet

- Alle eksisterende funksjoner og strukturer beholdes uendret
- GPU-kode aktiveres kun hvis `InitGPUCompositor()` lykkes
- Automatisk fallback til eksisterende CPU-kode hvis:
  - OpenGL/Mesa ikke er tilgjengelig
  - GL-kontekst ikke kan opprettes
  - Shader-kompilering feiler
- Ingen endringer i det offentlige HIDD Compositor API

### Ytelsesfordeler

| Scenario | CPU-modus | GPU-modus |
|----------|-----------|-----------|
| Vanlig blit | HIDD_Gfx_CopyBox | GPU tekstur-binding (zero-copy for zunegfx) |
| Alpha-blending | HIDD_BM_PutAlphaImage (CPU) | GPU shader |
| Skygger | Ikke støttet | GPU SDF shader |
| Multi-vindu | Sekvensiell CPU-blit | Parallell GPU-rendering |
