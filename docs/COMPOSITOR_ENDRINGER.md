# Konkrete Endringer i Eksisterende Compositor

Dette dokumentet beskriver de faktiske kodeendringene som trengs i `workbench/devs/monitors/Compositor/` for å støtte GPU-akselerert rendering.

---

## 1. Oversikt: Eksisterende vs Ny Arkitektur

### Eksisterende Compositor (i dag)

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
│  gpu_output_fbo ──► OpenGL FBO (NY)                         │
│  gpu_context ──► GL-kontekst (NY)                           │
│  bitmapstack ──► Liste av StackBitMapNode (utvidet)         │
│                                                             │
│  Rendering: OpenGL shaders for blending + skygger           │
│             Fallback til HIDD hvis ingen GPU                │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Endringer i compositor_intern.h

### 2.1 Utvidet StackBitMapNode

```c
// EKSISTERENDE struktur:
struct StackBitMapNode
{
    struct MinNode  n;
    OOP_Object      *bm;
    struct Region   *screenregion;
    SIPTR           leftedge;
    SIPTR           topedge;
    IPTR            sbmflags;
    struct Hook     *prealphacomphook;
};

// UTVIDET struktur (nye felter markert med /* NY */):
struct StackBitMapNode
{
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
        BOOL        is_gpu_native;      /* TRUE hvis DrawingBoard/FBO */
        APTR        pipe_resource;      /* Gallium ressurs (zero-copy) */
        struct Rectangle dirty_rect;    /* Dirty region for upload */
        BOOL        needs_upload;       /* TRUE hvis bitmap endret */
    } gpu;
};
```

### 2.2 Utvidet HIDDCompositorData

```c
// EKSISTERENDE felter beholdes, NYE felter legges til:
struct HIDDCompositorData
{
    /* ... alle eksisterende felter uendret ... */
    
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
        LONG        u_window_pos;
        LONG        u_window_size;
        LONG        u_shadow_color;
        LONG        u_shadow_offset;
        
        /* Geometri */
        ULONG       quad_vbo;           /* Vertex buffer for quads */
    } gpu;
    
    /* NY: Gallium integrasjon */
    OOP_Object      *galliumHidd;       /* Gallium HIDD for zero-copy */
};

/* NY: GPU-modus flagg */
#define COMPSTATEB_GPUACCEL     2
#define COMPSTATEF_GPUACCEL     (1 << COMPSTATEB_GPUACCEL)
```

---

## 3. Endringer i compositor_class.c

### 3.1 Ny funksjon: GPU-initialisering

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
    /* ... etc ... */
    
    /* Opprett quad VBO */
    CreateQuadVBO(&compdata->gpu.quad_vbo);
    
    compdata->gpu.available = TRUE;
    compdata->flags |= COMPSTATEF_GPUACCEL;
    
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
            
        glADestroyContext(compdata->gpu.gl_context);
    }
    
    compdata->gpu.available = FALSE;
    compdata->flags &= ~COMPSTATEF_GPUACCEL;
}
```

### 3.2 Endring i Root_New

```c
OOP_Object *METHOD(Compositor, Root, New)
{
    /* ... eksisterende kode ... */
    
    if ((compdata->gfx) && (compdata->gc))
    {
        /* NY: Prøv å initialisere GPU-akselerering */
        if (!InitGPUCompositor(compdata)) {
            D(bug("[Compositor] GPU acceleration not available, using software\n"));
        }
        
        return o;
    }
    
    /* ... */
}
```

### 3.3 Endring i Root_Dispose

```c
void METHOD(Compositor, Root, Dispose)
{
    struct HIDDCompositorData *compdata = OOP_INST_DATA(cl, o);
    
    /* NY: Rydd opp GPU-ressurser */
    CleanupGPUCompositor(compdata);
    
    OOP_DoSuperMethod(cl, o, &msg->mID);
}
```

### 3.4 NY funksjon: GPU-basert RedrawBitmap

```c
/* NY FUNKSJON - GPU-versjon av HIDDCompositorRedrawBitmap */

static void GPUCompositorRedrawBitmap(struct HIDDCompositorData *compdata,
                                       struct StackBitMapNode *n,
                                       struct Rectangle *rect)
{
    /* Sørg for at vi har en gyldig GPU-tekstur */
    if (n->gpu.texture_id == 0) {
        glGenTextures(1, &n->gpu.texture_id);
        n->gpu.needs_upload = TRUE;
    }
    
    /* Upload hvis nødvendig (legacy vinduer) */
    if (n->gpu.needs_upload && !n->gpu.is_gpu_native) {
        UploadBitmapToTexture(compdata, n);
    }
    
    /* Bind tekstur */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, n->gpu.texture_id);
    
    /* Sett uniforms */
    glUseProgram(compdata->gpu.composite_shader);
    glUniform1i(compdata->gpu.u_texture, 0);
    
    /* Beregn alpha */
    float alpha = 1.0f;
    if (n->sbmflags & COMPF_ALPHA) {
        /* Hent alpha fra bitmap attributter eller bruk default */
        alpha = 1.0f;  /* TODO: Hent faktisk alpha */
    }
    glUniform1f(compdata->gpu.u_alpha, alpha);
    
    /* Tegn quad */
    DrawTexturedQuad(compdata, 
                     rect->MinX, rect->MinY,
                     rect->MaxX - rect->MinX + 1,
                     rect->MaxY - rect->MinY + 1);
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
    
    /* Første gang: alloker tekstur */
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

### 3.5 Endring i HIDDCompositorRedrawVisibleRegions

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
    
    glAMakeCurrent(compdata->gpu.gl_context);
    
    /* Bind output FBO */
    glBindFramebuffer(GL_FRAMEBUFFER, compdata->gpu.output_fbo);
    glViewport(0, 0, 
               compdata->displayrect.MaxX - compdata->displayrect.MinX + 1,
               compdata->displayrect.MaxY - compdata->displayrect.MinY + 1);
    
    /* Clear med bakgrunnsfarge */
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);  /* TODO: Bruk faktisk bakgrunn */
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
                if (ShouldDrawShadow(n)) {
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
```

### 3.6 NY funksjon: Present til skjerm

```c
/* NY FUNKSJON */
static void GPUCompositorPresent(struct HIDDCompositorData *compdata,
                                  struct Rectangle *drawrect)
{
    /* Metode 1: Zero-copy via Gallium (best) */
    if (compdata->galliumHidd && compdata->gpu.output_pipe_res) {
        struct pHidd_Gallium_DisplayResource msg = {
            .mID = OOP_GetMethodID(IID_Hidd_Gallium, moHidd_Gallium_DisplayResource),
            .resource = compdata->gpu.output_pipe_res,
            .srcx = drawrect ? drawrect->MinX : 0,
            .srcy = drawrect ? drawrect->MinY : 0,
            .bitmap = NULL,  /* Direkte til display */
            .dstx = drawrect ? drawrect->MinX : 0,
            .dsty = drawrect ? drawrect->MinY : 0,
            .width = drawrect ? (drawrect->MaxX - drawrect->MinX + 1) 
                              : (compdata->displayrect.MaxX - compdata->displayrect.MinX + 1),
            .height = drawrect ? (drawrect->MaxY - drawrect->MinY + 1)
                               : (compdata->displayrect.MaxY - compdata->displayrect.MinY + 1)
        };
        OOP_DoMethod(compdata->galliumHidd, (OOP_Msg)&msg);
        return;
    }
    
    /* Metode 2: Les tilbake til CPU bitmap (fallback) */
    if (compdata->displaybitmap) {
        UBYTE *baseaddress;
        ULONG width, height, banksize, memsize;
        
        if (HIDD_BM_ObtainDirectAccess(compdata->displaybitmap, 
                                        &baseaddress, &width, &height, 
                                        &banksize, &memsize))
        {
            IPTR modulo;
            OOP_GetAttr(compdata->displaybitmap, aHidd_BitMap_BytesPerRow, &modulo);
            
            glBindFramebuffer(GL_FRAMEBUFFER, compdata->gpu.output_fbo);
            glReadPixels(0, 0, width, height, 
                         GL_BGRA, GL_UNSIGNED_BYTE, baseaddress);
            
            HIDD_BM_ReleaseDirectAccess(compdata->displaybitmap);
            
            /* Oppdater skjerm via HIDD */
            HIDD_BM_UpdateRect(compdata->displaybitmap,
                               0, 0, width, height);
        }
    }
}
```

### 3.7 NY funksjon: Skygge-rendering

```c
/* NY FUNKSJON */
static void GPUDrawWindowShadow(struct HIDDCompositorData *compdata,
                                 struct StackBitMapNode *n,
                                 struct Rectangle *rect)
{
    glUseProgram(compdata->gpu.shadow_shader);
    
    /* Skygge-offset (nedover og til høyre) */
    glUniform2f(compdata->gpu.u_shadow_offset, 8.0f, 8.0f);
    
    /* Skygge-farge (halvtransparent svart) */
    glUniform4f(compdata->gpu.u_shadow_color, 0.0f, 0.0f, 0.0f, 0.4f);
    
    /* Vindusstørrelse for SDF-beregning */
    IPTR bm_width, bm_height;
    OOP_GetAttr(n->bm, aHidd_BitMap_Width, &bm_width);
    OOP_GetAttr(n->bm, aHidd_BitMap_Height, &bm_height);
    glUniform2f(compdata->gpu.u_window_size, (float)bm_width, (float)bm_height);
    
    /* Vindusposisjon */
    glUniform2f(compdata->gpu.u_window_pos, (float)n->leftedge, (float)n->topedge);
    
    /* Tegn skygge-quad (litt større enn vinduet) */
    DrawShadowQuad(compdata,
                   n->leftedge - 16, n->topedge - 16,
                   bm_width + 32, bm_height + 32);
}
```

---

## 4. Endring i StackBitMapNode-allokering

### 4.1 I BitMapStackChanged

```c
OOP_Object *METHOD(Compositor, Hidd_Compositor, BitMapStackChanged)
{
    /* ... eksisterende kode ... */
    
    for (vpdata = msg->data; vpdata; vpdata = vpdata->Next)
    {
        n = AllocMem(sizeof(struct StackBitMapNode), MEMF_ANY | MEMF_CLEAR);
        if (!n) { /* ... */ }
        
        /* ... eksisterende felt-initialisering ... */
        
        /* NY: Initialiser GPU-felter */
        n->gpu.texture_id = 0;
        n->gpu.is_gpu_native = FALSE;
        n->gpu.pipe_resource = NULL;
        n->gpu.needs_upload = TRUE;
        n->gpu.dirty_rect.MinX = 0x7FFF;  /* Tom rect */
        n->gpu.dirty_rect.MaxX = 0;
        
        /* NY: Sjekk om dette er en GPU-native bitmap (zunegfx) */
        APTR pipe_res = NULL;
        OOP_GetAttr(n->bm, aHidd_BitMap_PipeResource, (IPTR*)&pipe_res);
        if (pipe_res) {
            n->gpu.is_gpu_native = TRUE;
            n->gpu.pipe_resource = pipe_res;
            n->gpu.needs_upload = FALSE;  /* Zero-copy! */
        }
        
        AddTail((struct List *)&compdata->bitmapstack, (struct Node *)n);
    }
    
    /* ... */
}
```

### 4.2 Frigjøring av GPU-teksturer

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

## 5. Ny fil: gpu_shaders.c

```c
/* workbench/devs/monitors/Compositor/gpu_shaders.c */

static const char *composite_vertex_src = 
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
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform float u_alpha;\n"
    "void main() {\n"
    "    vec4 color = texture2D(u_texture, v_texcoord);\n"
    "    gl_FragColor = vec4(color.rgb, color.a * u_alpha);\n"
    "}\n";

static const char *shadow_vertex_src =
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
    "precision mediump float;\n"
    "varying vec2 v_local_pos;\n"
    "uniform vec2 u_window_size;\n"
    "uniform vec4 u_shadow_color;\n"
    "float roundedBoxSDF(vec2 p, vec2 size, float radius) {\n"
    "    vec2 q = abs(p) - size + vec2(radius);\n"
    "    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;\n"
    "}\n"
    "void main() {\n"
    "    vec2 center = u_window_size * 0.5;\n"
    "    float dist = roundedBoxSDF(v_local_pos - center, center, 8.0);\n"
    "    float shadow = 1.0 - smoothstep(-16.0, 16.0, dist);\n"
    "    gl_FragColor = vec4(u_shadow_color.rgb, u_shadow_color.a * shadow);\n"
    "}\n";

ULONG CompileCompositeShader(void)
{
    return CompileShaderProgram(composite_vertex_src, composite_fragment_src);
}

ULONG CompileShadowShader(void)
{
    return CompileShaderProgram(shadow_vertex_src, shadow_fragment_src);
}

static ULONG CompileShaderProgram(const char *vert_src, const char *frag_src)
{
    ULONG vert = glCreateShader(GL_VERTEX_SHADER);
    ULONG frag = glCreateShader(GL_FRAGMENT_SHADER);
    ULONG prog = glCreateProgram();
    
    glShaderSource(vert, 1, &vert_src, NULL);
    glCompileShader(vert);
    
    glShaderSource(frag, 1, &frag_src, NULL);
    glCompileShader(frag);
    
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    
    glDeleteShader(vert);
    glDeleteShader(frag);
    
    return prog;
}
```

---

## 6. Endringer i mmakefile.src

```makefile
# Legg til nye filer
FILES := compositor_class compositor_startup displaymode gpu_shaders

# Legg til GL-avhengigheter
USER_LDFLAGS := -lGL
USER_INCLUDES := -I$(AROS_INCLUDES)/GL
```

---

## 7. Oppsummering av Endringer

| Fil | Type Endring | Beskrivelse |
|-----|--------------|-------------|
| `compositor_intern.h` | Utvidelse | Nye GPU-felter i strukturer |
| `compositor_class.c` | Endring | GPU-init, nye render-funksjoner |
| `gpu_shaders.c` | Ny fil | Shader-kode og kompilering |
| `mmakefile.src` | Endring | Nye filer og GL-linking |

### Antall Linjer (estimat)

| Endring | Linjer |
|---------|--------|
| Nye strukturfelter | ~30 |
| GPU-init/cleanup | ~80 |
| GPU-rendering | ~150 |
| Shader-kompilering | ~100 |
| Present-funksjon | ~50 |
| **Totalt** | **~410 linjer ny kode** |

### Bakoverkompatibilitet

- Alle eksisterende funksjoner beholdes
- GPU-kode aktiveres kun hvis initialisering lykkes
- Fallback til eksisterende CPU-kode hvis GPU ikke er tilgjengelig
- Ingen API-endringer utad
