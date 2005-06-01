/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWYDDION_ENUMS_H__
#define __GWYDDION_ENUMS_H__

#include <glib/gmacros.h>

G_BEGIN_DECLS

typedef enum {
    GWY_SI_UNIT_FORMAT_PLAIN,
    GWY_SI_UNIT_FORMAT_MARKUP,
    GWY_SI_UNIT_FORMAT_VFMARKUP,
    GWY_SI_UNIT_FORMAT_TEX
} GwySIUnitFormatStyle;

G_END_DECLS

#endif /*__GWYDDION_ENUMS_H__ */

