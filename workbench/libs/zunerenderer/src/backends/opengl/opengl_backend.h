#ifndef OPENGL_BACKEND_H
#define OPENGL_BACKEND_H

/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    Zune Renderer Library - OpenGL Backend Header (STUB)

    This header defines only the essential exports needed for the OpenGL 
    rendering backend stub. All operations are handled through the unified 
    ZuneBackendOps interface.
    
    NOTE: This backend is currently a stub implementation since OpenGL 
    support is not yet available in AROS.
*/

#include <exec/types.h>
#include "../backend_interface.h"

/*****************************************************************************/
/* OpenGL Backend Constants */
/*****************************************************************************/

#define OPENGL_MIN_VERSION 1    /* Minimum OpenGL version (when available) */

/*****************************************************************************/
/* OpenGL Backend Export */
/*****************************************************************************/

/* Backend operations table - defined in opengl_backend.c */
extern ZuneBackendOps opengl_backend_ops;

/*****************************************************************************/
/* Implementation Notes */
/*****************************************************************************/

/*
 * This backend is currently a stub that always reports as unavailable.
 * Future implementation would require:
 * - OpenGL library support in AROS
 * - Mesa3D or similar implementation
 * - Window system integration
 * - Context creation and management
 */

#endif /* OPENGL_BACKEND_H */