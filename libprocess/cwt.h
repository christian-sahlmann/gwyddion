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

#ifndef __GWY_PROCESS_CWT_H__
#define __GWY_PROCESS_CWT_H__

#include <glib.h>
#include <libprocess/gwyprocessenums.h>

G_BEGIN_DECLS

gdouble gwy_cwt_wfunc_2d(gdouble scale,
                         gdouble mval,
                         gint xres,
                         Gwy2DCWTWaveletType wtype);

G_END_DECLS

#endif /*__GWY_PROCESS_CWT__*/
