/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: keymap.library function MapRawKeyUTF8()
*/
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/codesets.h>
#include <proto/keymap.h>
#include <libraries/codesets.h>
#include <devices/inputevent.h>
#include <devices/keymap.h>
#include <string.h>
#include "keymap_intern.h"

#include <aros/debug.h>

/* A keymap file names its charset with this after the keymap's name */
#define KMCS_MAGIC  "KMCS"

static BOOL same_name(CONST_STRPTR a, CONST_STRPTR b)
{
    for (; *a && *b; a++, b++)
        if ((*a | 0x20) != (*b | 0x20))
            return FALSE;
    return *a == *b;
}

/* The charset a loaded keymap names, copied to name; FALSE if none */
static BOOL keymap_charset(struct KeyMap *km, char *name, struct KeymapBase *KeymapBase)
{
    struct KeyMapNode *kn;
    BOOL found = FALSE;

    Forbid();
    ForeachNode(&KeymapBase->KeymapResource.kr_List, kn)
    {
        if (&kn->kn_KeyMap == km)
        {
            CONST_STRPTR s = kn->kn_Node.ln_Name;

            if (s)
            {
                s += strlen(s) + 1;
                if (!memcmp(s, KMCS_MAGIC, 4) && strlen(s + 4) < KM_CHARSETLEN)
                {
                    strcpy(name, s + 4);
                    found = TRUE;
                }
            }
            break;
        }
    }
    Permit();

    return found;
}

/* ENV:CHARSET can only be read by a process; tasks get the last value read */
static void system_charset(char *name, struct KeymapBase *KeymapBase)
{
    struct Task *me = FindTask(NULL);

    if (me->tc_Node.ln_Type == NT_PROCESS)
    {
        struct Library *DOSBase = KeymapBase->DOSBase;
        char buf[KM_CHARSETLEN];
        LONG n = -1;

        if (!DOSBase)
            DOSBase = KeymapBase->DOSBase = OpenLibrary("dos.library", 36);
        if (DOSBase)
            n = GetVar("CHARSET", buf, sizeof(buf), GVF_GLOBAL_ONLY);

        Forbid();
        if (n > 0)
            strcpy(KeymapBase->SysCharset, buf);
        else if (DOSBase)
            KeymapBase->SysCharset[0] = 0;
        Permit();
    }

    Forbid();
    strcpy(name, KeymapBase->SysCharset);
    Permit();
}

/* The table for name. NULL with *known TRUE is Latin-1, built in; with
   *known FALSE no table is known, and nothing above ASCII is guessed */
static struct codeset *find_codeset(CONST_STRPTR name, BOOL *known,
    struct KeymapBase *KeymapBase)
{
    struct Library *CodesetsBase = KeymapBase->CodesetsBase;
    struct codeset *cs;
    BOOL latin1 = !name[0] || same_name(name, "ISO-8859-1") || same_name(name, "ISO8859-1");

    *known = TRUE;
    if (!name[0])
        return NULL;

    /* Only a process may have codesets.library loaded from disk */
    if (!CodesetsBase && FindTask(NULL)->tc_Node.ln_Type == NT_PROCESS)
        CodesetsBase = KeymapBase->CodesetsBase = OpenLibrary("codesets.library", 6);

    if (!CodesetsBase || same_name(name, "UTF-8"))
    {
        *known = latin1;
        return NULL;
    }

    Forbid();
    cs = same_name(KeymapBase->CachedName, name) ? KeymapBase->CachedCodeset : NULL;
    Permit();

    if (!cs)
    {
        struct TagItem tags[] = { { CSA_FallbackToDefault, FALSE }, { TAG_DONE, 0 } };

        cs = CodesetsFindA((STRPTR)name, tags);
        if (!cs)
        {
            *known = latin1;
            return NULL;
        }

        Forbid();
        strcpy(KeymapBase->CachedName, name);
        KeymapBase->CachedCodeset = cs;
        Permit();
    }

    return cs;
}

static ULONG encode_utf8(ULONG cp, UBYTE *u)
{
    if (cp < 0x80)
    {
        u[0] = cp;
        return 1;
    }
    if (cp < 0x800)
    {
        u[0] = 0xC0 | (cp >> 6);
        u[1] = 0x80 | (cp & 0x3F);
        return 2;
    }
    if (cp < 0x10000)
    {
        u[0] = 0xE0 | (cp >> 12);
        u[1] = 0x80 | ((cp >> 6) & 0x3F);
        u[2] = 0x80 | (cp & 0x3F);
        return 3;
    }
    u[0] = 0xF0 | (cp >> 18);
    u[1] = 0x80 | ((cp >> 12) & 0x3F);
    u[2] = 0x80 | ((cp >> 6) & 0x3F);
    u[3] = 0x80 | (cp & 0x3F);
    return 4;
}

/*****************************************************************************

    NAME */
#include <clib/keymap_protos.h>

        AROS_LH4(LONG, MapRawKeyUTF8,

/*  SYNOPSIS */
        AROS_LHA(struct InputEvent *, event, A0),
        AROS_LHA(STRPTR             , buffer, A1),
        AROS_LHA(LONG               , length, D1),
        AROS_LHA(struct KeyMap     *, keyMap, A2),

/*  LOCATION */
        struct Library *, KeymapBase, 9, Keymap)

/*  FUNCTION
        Like MapRawKey(), but the bytes are given as UTF-8.

        The bytes a keymap gives are in its charset. A keymap names it
        with "charset:" in its descriptor; one that does not is taken to
        be in the system's 8-bit charset, ENV:CHARSET (Latin-1 if unset).
        When no table is known for the charset, bytes above 0x7F are
        left out rather than guessed.

    INPUTS
        event  - InputEvent to convert, ie_NextEvent is ignored.
        buffer - buffer for the UTF-8 bytes, not NUL terminated.
        length - length of buffer.
        keyMap - keymap to use, NULL for the default keymap.

    RESULT
        Number of bytes written, -1 if the buffer overflowed.

    NOTES
        Control characters are kept; leave them out after decoding, as
        bytes 0x80-0x9F are letters in some charsets.

        ENV:CHARSET is read when a process calls; a task gets the value
        a process read last.

    SEE ALSO
        MapRawKey()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct KeymapBase *kb = KMBase(KeymapBase);
    struct codeset *cs;
    char name[KM_CHARSETLEN];
    UBYTE keys[64], u[4];
    LONG n, i, out = 0;
    BOOL known;

    if (!keyMap)
        keyMap = kb->DefaultKeymap;

    n = MapRawKey(event, keys, sizeof(keys), keyMap);
    if (n <= 0)
        return n;

    if (!keymap_charset(keyMap, name, kb))
        system_charset(name, kb);
    cs = find_codeset(name, &known, kb);

    D(bug("[keymap] MapRawKeyUTF8: %ld bytes, charset '%s' known %d\n", n, name, known));

    for (i = 0; i < n; i++)
    {
        ULONG cp = keys[i], len;

        if (cp >= 0x80)
        {
            if (!known)
                continue;
            if (cs)
            {
                cp = cs->table[keys[i]].ucs4;
                if (!cp)
                    continue;
            }
        }

        len = encode_utf8(cp, u);
        if (out + len > length)
            return -1;
        CopyMem(u, buffer + out, len);
        out += len;
    }

    return out;

    AROS_LIBFUNC_EXIT
}
