/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    ZuneGfx Library - CyberGraphX Backend Antialiasing Implementation
*/

#include <cybergraphx/cybergraphics.h>
#include <exec/types.h>

#include "cybergfx_antialiasing.h"
#include "cybergfx_backend.h"

WORD cybergfx_aa_quality = 2;
float cybergfx_aa_smoothness = CYBERGFX_AA_SMOOTHNESS;

/* Corner distance cache - pre-computed sqrt values for corner pixels */
CybergfxCornerCache cybergfx_corner_cache = { .valid = FALSE };
