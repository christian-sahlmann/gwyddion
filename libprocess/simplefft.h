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

#ifndef __GWY_PROCESS_SIMPLEFFT_H__
#define __GWY_PROCESS_SIMPLEFFT_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <libprocess/gwyprocessenums.h>
#include <libprocess/datafield.h>

G_BEGIN_DECLS

void gwy_fft_simple(GwyTransformDirection dir,
                    const gdouble *re_in,
                    const gdouble *im_in,
                    gdouble *re_out,
                    gdouble *im_out,
                    gint n,
                    gint stride);

void gwy_fft_window(gdouble *data,
                    gint n,
                    GwyWindowingType windowing);

void gwy_fft_window_datafield(GwyDataField *dfield,
                              GwyOrientation orientation,
                              GwyWindowingType windowing);


G_END_DECLS


#endif /*__GWY_PROCESS_SIMPLEFFT__*/
