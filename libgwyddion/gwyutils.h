/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#ifndef __GWY_UTILS_H__
#define __GWY_UTILS_H__

#include <glib.h>

typedef struct {
    const gchar *name;
    gint value;
} GwyEnum;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void   gwy_hash_table_to_slist_cb (gpointer unused_key,
                                   gpointer value,
                                   gpointer user_data);
void   gwy_hash_table_to_list_cb  (gpointer unused_key,
                                   gpointer value,
                                   gpointer user_data);
gint   gwy_string_to_enum         (const gchar *str,
                                   const GwyEnum *enum_table,
                                   gint n);
G_CONST_RETURN
gchar* gwy_enum_to_string         (gint enumval,
                                   const GwyEnum *enum_table,
                                   gint n);
gint   gwy_string_to_flags        (const gchar *str,
                                   const GwyEnum *enum_table,
                                   gint n,
                                   const gchar *delimiter);
gchar* gwy_flags_to_string        (gint enumval,
                                   const GwyEnum *enum_table,
                                   gint n,
                                   const gchar *delimiter);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_UTILS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

