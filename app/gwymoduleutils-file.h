/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_MODULEUTILS_FILE_H__
#define __GWY_MODULEUTILS_FILE_H__

#include <libgwyddion/gwycontainer.h>

G_BEGIN_DECLS

static inline gboolean
gwy_get_gboolean8(const guchar **ppv)
{
    const guint8 *pv = *(const guint8**)ppv;
    guint8 v = *pv;
    *ppv += sizeof(guint8);
    return !!v;
}

static inline gint16
gwy_get_gint16_le(const guchar **ppv)
{
    const gint16 *pv = *(const gint16**)ppv;
    gint16 v = GINT16_FROM_LE(*pv);
    *ppv += sizeof(gint16);
    return v;
}

static inline gint16
gwy_get_gint16_be(const guchar **ppv)
{
    const gint16 *pv = *(const gint16**)ppv;
    gint16 v = GINT16_FROM_BE(*pv);
    *ppv += sizeof(gint16);
    return v;
}

static inline guint16
gwy_get_guint16_le(const guchar **ppv)
{
    const guint16 *pv = *(const guint16**)ppv;
    guint16 v = GUINT16_FROM_LE(*pv);
    *ppv += sizeof(guint16);
    return v;
}

static inline guint16
gwy_get_guint16_be(const guchar **ppv)
{
    const guint16 *pv = *(const guint16**)ppv;
    guint16 v = GUINT16_FROM_BE(*pv);
    *ppv += sizeof(guint16);
    return v;
}

static inline gint32
gwy_get_gint32_le(const guchar **ppv)
{
    const gint32 *pv = *(const gint32**)ppv;
    gint32 v = GINT32_FROM_LE(*pv);
    *ppv += sizeof(gint32);
    return v;
}

static inline gint32
gwy_get_gint32_be(const guchar **ppv)
{
    const gint32 *pv = *(const gint32**)ppv;
    gint32 v = GINT32_FROM_BE(*pv);
    *ppv += sizeof(gint32);
    return v;
}

static inline guint32
gwy_get_guint32_le(const guchar **ppv)
{
    const guint32 *pv = *(const guint32**)ppv;
    guint32 v = GUINT32_FROM_LE(*pv);
    *ppv += sizeof(guint32);
    return v;
}

static inline guint32
gwy_get_guint32_be(const guchar **ppv)
{
    const guint32 *pv = *(const guint32**)ppv;
    guint32 v = GUINT32_FROM_BE(*pv);
    *ppv += sizeof(guint32);
    return v;
}

static inline gint64
gwy_get_gint64_le(const guchar **ppv)
{
    const gint64 *pv = *(const gint64**)ppv;
    gint64 v = GINT64_FROM_LE(*pv);
    *ppv += sizeof(gint64);
    return v;
}

static inline gint64
gwy_get_gint64_be(const guchar **ppv)
{
    const gint64 *pv = *(const gint64**)ppv;
    gint64 v = GINT64_FROM_BE(*pv);
    *ppv += sizeof(gint64);
    return v;
}

static inline guint64
gwy_get_guint64_le(const guchar **ppv)
{
    const guint64 *pv = *(const guint64**)ppv;
    guint64 v = GUINT64_FROM_LE(*pv);
    *ppv += sizeof(guint64);
    return v;
}

static inline guint64
gwy_get_guint64_be(const guchar **ppv)
{
    const guint64 *pv = *(const guint64**)ppv;
    guint64 v = GUINT64_FROM_BE(*pv);
    *ppv += sizeof(guint64);
    return v;
}

static inline gfloat
gwy_get_gfloat_le(const guchar **ppv)
{
    const guint32 *pv = *(const guint32**)ppv;
    guint32 v = GUINT32_FROM_LE(*pv);
    *ppv += sizeof(guint32);
    return *(gfloat*)&v;
}

static inline gfloat
gwy_get_gfloat_be(const guchar **ppv)
{
    const guint32 *pv = *(const guint32**)ppv;
    guint32 v = GUINT32_FROM_BE(*pv);
    *ppv += sizeof(guint32);
    return *(gfloat*)&v;
}

static inline gdouble
gwy_get_gdouble_le(const guchar **ppv)
{
    const guint64 *pv = *(const guint64**)ppv;
    guint64 v = GUINT64_FROM_LE(*pv);
    *ppv += sizeof(guint64);
    return *(gdouble*)&v;
}

static inline gdouble
gwy_get_gdouble_be(const guchar **ppv)
{
    const guint64 *pv = *(const guint64**)ppv;
    guint64 v = GUINT64_FROM_BE(*pv);
    *ppv += sizeof(guint64);
    return *(gdouble*)&v;
}

gboolean gwy_app_channel_check_nonsquare(GwyContainer *data,
                                         gint id);
gboolean gwy_app_channel_title_fall_back(GwyContainer *data,
                                         gint id);

G_END_DECLS

#endif /* __GWY_MODULEUTILS_FILE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
