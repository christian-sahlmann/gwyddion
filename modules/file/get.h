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
#ifndef __GWY_GET_H__
#define __GWY_GET_H__

static inline gsize
get_WORD(const guchar **p)
{
    gsize z = (gsize)(*p)[0] + ((gsize)(*p)[1] << 8);
    *p += 2;
    return z;
}

static inline gsize
get_DWORD(const guchar **p)
{
    gsize z = (gsize)(*p)[0] + ((gsize)(*p)[1] << 8)
              + ((gsize)(*p)[2] << 16) + ((gsize)(*p)[3] << 24);
    *p += 4;
    return z;
}

static inline gfloat
get_FLOAT(const guchar **p)
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
get_DOUBLE(const guchar **p)
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

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
