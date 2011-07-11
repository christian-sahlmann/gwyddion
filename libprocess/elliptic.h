/*
 *  @(#) $Id$
 *  Copyright (C) 2005-2006 David Necas (Yeti), Petr Klapetek, Chris Anderson.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, sidewinder.asu@gmail.com.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GWY_PROCESS_ELLIPTIC_H__
#define __GWY_PROCESS_ELLIPTIC_H__

#include <libprocess/datafield.h>

G_BEGIN_DECLS

gint gwy_data_field_elliptic_area_fill            (GwyDataField *data_field,
                                                   gint col,
                                                   gint row,
                                                   gint width,
                                                   gint height,
                                                   gdouble value);
gint gwy_data_field_elliptic_area_extract         (GwyDataField *data_field,
                                                   gint col,
                                                   gint row,
                                                   gint width,
                                                   gint height,
                                                   gdouble *data);
void gwy_data_field_elliptic_area_unextract       (GwyDataField *data_field,
                                                   gint col,
                                                   gint row,
                                                   gint width,
                                                   gint height,
                                                   const gdouble *data);
gint gwy_data_field_get_elliptic_area_size        (gint width,
                                                   gint height);
gint gwy_data_field_circular_area_fill            (GwyDataField *data_field,
                                                   gint col,
                                                   gint row,
                                                   gdouble radius,
                                                   gdouble value);
gint gwy_data_field_circular_area_extract         (GwyDataField *data_field,
                                                   gint col,
                                                   gint row,
                                                   gdouble radius,
                                                   gdouble *data);
gint gwy_data_field_circular_area_extract_with_pos(GwyDataField *data_field,
                                                   gint col,
                                                   gint row,
                                                   gdouble radius,
                                                   gdouble *data,
                                                   gint *xpos,
                                                   gint *ypos);
void gwy_data_field_circular_area_unextract       (GwyDataField *data_field,
                                                   gint col,
                                                   gint row,
                                                   gdouble radius,
                                                   const gdouble *data);
gint gwy_data_field_get_circular_area_size        (gdouble radius);

G_END_DECLS

#endif /* __GWY_PROCESS_ELLIPTIC_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
