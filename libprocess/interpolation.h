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

#ifndef __GWY_INTERPOLATION_H__
#define __GWY_INTERPOLATION_H__
#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
  GWY_INTERPOLATION_NONE      = 0,
  GWY_INTERPOLATION_ROUND     = 1,
  GWY_INTERPOLATION_BILINEAR  = 2,
  GWY_INTERPOLATION_KEY       = 3,
  GWY_INTERPOLATION_BSPLINE   = 4,
  GWY_INTERPOLATION_OMOMS     = 5,
  GWY_INTERPOLATION_NNA       = 6
} GwyInterpolationType;

gdouble gwy_interpolation_get_dval(gdouble x,
                                   gdouble x1_,
                                   gdouble y1_,
                                   gdouble x2_,
                                   gdouble y2_,
                                   GwyInterpolationType interpolation);

gdouble
gwy_interpolation_get_dval_of_equidists(gdouble x,
                                        gdouble *data,
                                        GwyInterpolationType interpolation);


G_END_DECLS

#endif /*__GWY_INTERPOLATION_H__*/




/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
