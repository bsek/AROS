/*
    Copyright (C) 2024, The AROS Development Team. All rights reserved.

    Header file for improved vector drawing functions in AROS MUI
*/

#ifndef _IMSPEC_VECTOR_H_
#define _IMSPEC_VECTOR_H_

#include <exec/types.h>
#include "mui.h"

/* Arrow direction constants */
#define IMPROVED_ARROW_UP    0
#define IMPROVED_ARROW_DOWN  1
#define IMPROVED_ARROW_LEFT  2
#define IMPROVED_ARROW_RIGHT 3

/* Arrow style constants */
#define ARROW_STYLE_TRIANGLE 0  /* Filled triangle arrows */
#define ARROW_STYLE_CHEVRON  1  /* Chevron/angle arrows */

/* Vector table indices for chevron arrows */
#define CHEVRON_ARROW_UP_VECTOR    23
#define CHEVRON_ARROW_DOWN_VECTOR  24
#define CHEVRON_ARROW_LEFT_VECTOR  25
#define CHEVRON_ARROW_RIGHT_VECTOR 26

/* Standard arrow functions (now with improved rendering) */
void arrowup_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                  LONG width, LONG height, LONG state);

void arrowdown_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                    LONG width, LONG height, LONG state);

void arrowleft_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                    LONG width, LONG height, LONG state);

void arrowright_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                     LONG width, LONG height, LONG state);

/* Chevron-style arrow functions for modern UI themes */
void chevron_arrowup_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                          LONG width, LONG height, LONG state);

void chevron_arrowdown_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                            LONG width, LONG height, LONG state);

void chevron_arrowleft_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                            LONG width, LONG height, LONG state);

void chevron_arrowright_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                             LONG width, LONG height, LONG state);

/* Other vector drawing functions */
void checkbox_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                   LONG width, LONG height, LONG state);

void mx_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
             LONG height, LONG state);

void cycle_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                LONG height, LONG state);

void popup_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                LONG height, LONG state);

void popfile_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                  LONG height, LONG state);

void popdrawer_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                    LONG height, LONG state);

void drawer_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                 LONG height, LONG state);

void harddisk_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                   LONG height, LONG state);

void disk_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
               LONG height, LONG state);

void ram_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
              LONG height, LONG state);

void volume_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                 LONG height, LONG state);

void network_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                  LONG height, LONG state);

void assign_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                 LONG height, LONG state);

void tape_play_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                    LONG height, LONG state);

void tape_playback_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                        LONG width, LONG height, LONG state);

void tape_pause_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                     LONG width, LONG height, LONG state);

void tape_stop_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                    LONG height, LONG state);

void tape_record_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
                      LONG width, LONG height, LONG state);

void tape_up_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                  LONG height, LONG state);

void tape_down_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
                    LONG height, LONG state);

/* Vector table management functions */
struct MUI_ImageSpec_intern *zune_imspec_create_vector(LONG vect);

BOOL zune_imspec_vector_get_minmax(struct MUI_ImageSpec_intern *spec,
                                   struct MUI_MinMax *minmax);

#endif /* _IMSPEC_VECTOR_H_ */