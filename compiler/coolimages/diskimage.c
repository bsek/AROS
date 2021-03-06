/*
    Copyright (C) 1995-2001, The AROS Development Team. All rights reserved.
*/

#include "coolimages.h"

#define DISKIMAGE_WIDTH     16
#define DISKIMAGE_HEIGHT    16
#define DISKIMAGE_COLORS    4

static const UBYTE diskimage_data[] =
{
        00,03,03,03,03,03,03,03,03,03,03,03,03,03,03,00,
        03,03,02,02,02,02,02,02,02,02,02,02,02,02,03,01,
        03,03,02,02,03,03,03,03,03,03,03,03,02,02,03,01,
        03,03,02,02,02,02,02,02,02,02,02,02,02,02,03,01,
        03,03,02,02,03,03,03,03,03,03,03,03,02,02,03,01,
        03,03,02,02,02,02,02,02,02,02,02,02,02,02,03,01,
        03,03,03,02,02,02,02,02,02,02,02,02,02,03,03,01,
        03,03,03,03,03,03,03,03,03,03,03,03,03,03,03,01,
        03,03,03,03,03,03,03,03,03,03,03,03,03,03,03,01,
        03,03,03,02,02,02,02,02,02,02,02,02,02,03,03,01,
        03,03,03,02,02,03,03,02,02,02,02,02,02,03,03,01,
        03,03,03,02,02,03,03,02,02,02,02,02,02,03,03,01,
        03,03,03,02,02,03,03,02,02,02,02,02,02,03,03,01,
        03,03,03,02,02,02,02,02,02,02,02,02,02,03,03,01,
        00,01,01,01,01,01,01,01,01,01,01,01,01,01,01,00,
        00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
};

static const UBYTE diskimage_pal[] =
{
        0xb3,0xb3,0xb3,0x00,0x00,0x00,
        0xe0,0xe0,0xe0,0x65,0x4b,0xbf
};

const struct CoolImage cool_diskimage =
{
        diskimage_data,
        diskimage_pal,
        DISKIMAGE_WIDTH,
        DISKIMAGE_HEIGHT,
        DISKIMAGE_COLORS
};
