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

#ifndef __GWY_MODULE_ENUMS_H__
#define __GWY_MODULE_ENUMS_H__

#include <glib/gmacros.h>

G_BEGIN_DECLS

typedef enum {
    GWY_FILE_OPERATION_DETECT = 1 << 0,
    GWY_FILE_OPERATION_LOAD   = 1 << 1,
    GWY_FILE_OPERATION_SAVE   = 1 << 2,
    GWY_FILE_OPERATION_EXPORT = 1 << 3,
    GWY_FILE_OPERATION_MASK   = 0x0f
} GwyFileOperationType;

/* FIXME: remove, more detailed interface needed */
typedef enum {
    GWY_TOOL_SWITCH_WINDOW = 1,
    GWY_TOOL_SWITCH_TOOL
} GwyToolSwitchEvent;

typedef enum {
    GWY_RUN_NONE           = 0,
    GWY_RUN_NONINTERACTIVE = 1 << 0,
    GWY_RUN_INTERACTIVE    = 1 << 1,
    GWY_RUN_IMMEDIATE      = 1 << 2,
    GWY_RUN_MASK           = 0x07
} GwyRunType;

G_END_DECLS

#endif /* __GWY_MODULE_ENUMS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
