/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */
#ifndef __GWY_FILE_GET_H__
#define __GWY_GILE_GET_H__

static inline gulong
get_WORD_LE(const guchar **p)
{
    gulong z = (gulong)(*p)[0] | ((gulong)(*p)[1] << 8);
    *p += 2;
    return z;
}

static inline gulong
get_WORD_BE(const guchar **p)
{
    gulong z = (gulong)((*p)[0] << 8) | (gulong)(*p)[1];
    *p += 2;
    return z;
}

static inline gulong
get_DWORD_LE(const guchar **p)
{
    gulong z = (gulong)(*p)[0] | ((gulong)(*p)[1] << 8)
               | ((gulong)(*p)[2] << 16) | ((gulong)(*p)[3] << 24);
    *p += 4;
    return z;
}

static inline gulong
get_DWORD_BE(const guchar **p)
{
    gulong z = ((gulong)(*p)[0] << 24) | ((gulong)(*p)[1] << 16)
               | ((gulong)(*p)[2] << 8) | (gulong)(*p)[3];
    *p += 4;
    return z;
}

static inline guint64
get_QWORD_LE(const guchar **p)
{
    guint64 z = (guint64)(*p)[0] | ((guint64)(*p)[1] << 8)
                | ((guint64)(*p)[2] << 16) | ((guint64)(*p)[3] << 24)
                | ((guint64)(*p)[4] << 32) | ((guint64)(*p)[5] << 40)
                | ((guint64)(*p)[4] << 48) | ((guint64)(*p)[5] << 56);
    *p += 8;
    return z;
}

static inline guint64
get_QWORD_BE(const guchar **p)
{
    guint64 z = ((guint64)(*p)[0] << 56) | ((guint64)(*p)[1] << 48)
                | ((guint64)(*p)[2] << 40) | ((guint64)(*p)[3] << 32)
                | ((guint64)(*p)[2] << 24) | ((guint64)(*p)[3] << 16)
                | ((guint64)(*p)[2] << 8) | (guint64)(*p)[3];
    *p += 8;
    return z;
}

static inline gdouble
get_FLOAT_LE(const guchar **p)
{
    union { guchar pp[4]; float f; } z;

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    memcpy(z.pp, *p, sizeof(float));
#else
    z.pp[0] = (*p)[3];
    z.pp[1] = (*p)[2];
    z.pp[2] = (*p)[1];
    z.pp[3] = (*p)[0];
#endif
    *p += sizeof(float);
    return z.f;
}

static inline gdouble
get_FLOAT_BE(const guchar **p)
{
    union { guchar pp[4]; float f; } z;

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    memcpy(z.pp, *p, sizeof(float));
#else
    z.pp[0] = (*p)[3];
    z.pp[1] = (*p)[2];
    z.pp[2] = (*p)[1];
    z.pp[3] = (*p)[0];
#endif
    *p += sizeof(float);
    return z.f;
}

static inline gdouble
get_DOUBLE_LE(const guchar **p)
{
    union { guchar pp[8]; double d; } z;

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    memcpy(z.pp, *p, sizeof(double));
#else
    z.pp[0] = (*p)[7];
    z.pp[1] = (*p)[6];
    z.pp[2] = (*p)[5];
    z.pp[3] = (*p)[4];
    z.pp[4] = (*p)[3];
    z.pp[5] = (*p)[2];
    z.pp[6] = (*p)[1];
    z.pp[7] = (*p)[0];
#endif
    *p += sizeof(double);
    return z.d;
}

static inline gdouble
get_DOUBLE_BE(const guchar **p)
{
    union { guchar pp[8]; double d; } z;

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    memcpy(z.pp, *p, sizeof(double));
#else
    z.pp[0] = (*p)[7];
    z.pp[1] = (*p)[6];
    z.pp[2] = (*p)[5];
    z.pp[3] = (*p)[4];
    z.pp[4] = (*p)[3];
    z.pp[5] = (*p)[2];
    z.pp[6] = (*p)[1];
    z.pp[7] = (*p)[0];
#endif
    *p += sizeof(double);
    return z.d;
}

static inline void
get_CHARS(gchar *dest, const guchar **p, guint size)
{
    memcpy(dest, *p, size);
    *p += size;
}

static inline void
get_CHARS0(gchar *dest, const guchar **p, guint size)
{
    memcpy(dest, *p, size);
    *p += size;
    dest[size-1] = '\0';
}

#define get_CHARARRAY(dest, p) get_CHARS(dest, p, sizeof(dest))
#define get_CHARARRAY0(dest, p) get_CHARS0(dest, p, sizeof(dest))

static inline gboolean
get_BBOOLEAN(const guchar **p)
{
    gboolean b;

    b = (**p != 0);
    (*p)++;
    return b;
}

/* Get a non-terminated string preceded by one byte containing the length.
 * Size is the size of buffer pointer by *p.  Returns %NULL if size is too
 * small. */
static inline gchar*
get_PASCAL_STRING(const guchar **p,
                  gsize size)
{
    guint len;
    gchar *s;

    if (!size)
        return NULL;

    len = **p;
    (*p)++;
    if (size < len + 1)
        return NULL;

    s = g_new(gchar, len+1);
    memcpy(s, *p, len);
    s[len] = '\0';
    *p += len;

    return s;
}

/* Get a non-terminated string preceded by one byte containing the length.
 * Size is the maximum size of the string and the number of bytes the pointer
 * will move forward.
 * Dest must be one byte larger to hold the terminating NUL. */
static inline void
get_PASCAL_CHARS0(gchar *dest,
                  const guchar **p,
                  gsize size)
{
    guint len;

    len = MIN(**p, size);
    (*p)++;
    memcpy(dest, *p, len);
    dest[len] = '\0';
    *p += size;
}

#define get_PASCAL_CHARARRAY0(dest, p) \
    get_PASCAL_CHARS0(dest, p, sizeof(dest)-1)

static inline gdouble
get_PASCAL_REAL_LE(const guchar **p)
{
    gint power;
    gdouble x;

    if (!(*p)[0]) {
        *p += 6;
        return 0.0;
    }
    x = 1.0 + (((((*p)[1]/256.0 + (*p)[2])/256.0 + (*p)[3])/256.0
                + (*p)[4])/256.0 + ((*p)[5] & 0x7f))/128.0;
    if ((*p)[5] & 0x80)
        x = -x;

    power = (gint)(*p)[0] - 129;
    while (power > 0) {
        x *= 2.0;
        power--;
    }
    while (power < 0) {
        x /= 2.0;
        power++;
    }

    *p += 6;

    return x;
}

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
