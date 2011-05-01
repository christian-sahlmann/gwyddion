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
/* The value scaling meaning differs for integral and floating point data.
 * For integers is the LSB value, for floats it is the value corresponding to
 * 1.0.  Floats with reasonable dynamic range can usually omit it for no
 * scaling. */
#define GWY_IMGKEY_ZSCALE "Gwy::ZScale"

#endif
