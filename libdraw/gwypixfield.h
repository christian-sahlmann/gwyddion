/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

/*PROVISORY routines to debug datafield procedures*/

#ifndef __GWY_PIXFIELD__
#define __GWY_PIXFIELD__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libprocess/datafield.h>
#include <libdraw/gwypalette.h>
#include <libdraw/gwygradient.h>

#ifndef GWY_DISABLE_DEPRECATED
void     gwy_pixfield_do            (GdkPixbuf *pixbuf,
                                     GwyDataField *data_field,
                                     GwyPalette *palette);
void     gwy_pixfield_do_with_range (GdkPixbuf *pixbuf,
                                     GwyDataField *data_field,
                                     GwyPalette *palette,
                                     gdouble minimum,
                                     gdouble maximum);
void     gwy_pixfield_do_mask       (GdkPixbuf *pixbuf,
                                     GwyDataField *data_field,
                                     GwyRGBA *color);
#endif

void gwy_pixbuf_draw_data_field            (GdkPixbuf *pixbuf,
                                            GwyDataField *data_field,
                                            GwyGradient *gradient);
void gwy_pixbuf_draw_data_field_with_range (GdkPixbuf *pixbuf,
                                            GwyDataField *data_field,
                                            GwyGradient *gradient,
                                            gdouble minimum,
                                            gdouble maximum);
void gwy_pixbuf_draw_data_field_with_rms   (GdkPixbuf *pixbuf,
                                            GwyDataField *data_field,
                                            GwyGradient *gradient,
                                            gdouble rmsrange);
void gwy_pixbuf_draw_data_field_as_mask    (GdkPixbuf *pixbuf,
                                            GwyDataField *data_field,
                                            GwyRGBA *color);

#endif /*__GWY_PIXFIELD__*/
