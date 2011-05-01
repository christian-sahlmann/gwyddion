/*
 *  @(#) $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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

#ifndef __GWY_IMAGE_KEYS_H__
#define __GWY_IMAGE_KEYS_H__ 1

/* These are the same as GSF header fields, just prefixed with Gwy::.
 * Some image formats have a dedicated title field, we usually set both then. */
#define GWY_IMGKEY_TITLE "Gwy::Title"
#define GWY_IMGKEY_XREAL "Gwy::XReal"
#define GWY_IMGKEY_YREAL "Gwy::YReal"
#define GWY_IMGKEY_XOFFSET "Gwy::XOffset"
#define GWY_IMGKEY_YOFFSET "Gwy::YOffset"
#define GWY_IMGKEY_XYUNIT "Gwy::XYUnit"
#define GWY_IMGKEY_ZUNIT "Gwy::ZUnit"
/* ZScale is used for limited-range floating point data (such as halves) and
 * determines the value corresponding to 1.0 in the file. */
#define GWY_IMGKEY_ZSCALE "Gwy::ZScale"
/* ZMin and ZMax are used for integer data and correspond to 0 and the maximum
 * possible integer value. */
#define GWY_IMGKEY_ZMIN "Gwy::ZMin"
#define GWY_IMGKEY_ZMAX "Gwy::ZMax"

#endif
