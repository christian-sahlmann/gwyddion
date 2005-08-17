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

#ifndef __GWY_DEBUG_OBJECTS_H__
#define __GWY_DEBUG_OBJECTS_H__

#include <glib.h>
#include <stdio.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    GWY_DEBUG_OBJECTS_DUMP_ONLY_ALIVE = 1 << 0
} GwyDebugObjectsDumpFlags;

#define gwy_debug_objects_creation(o) \
    gwy_debug_objects_creation_detailed((o), __FILE__ ":" G_STRINGIFY(__LINE__))

void gwy_debug_objects_creation_detailed (GObject *object,
                                          const gchar *details);
void gwy_debug_objects_enable            (gboolean enable);
void gwy_debug_objects_dump_to_file      (FILE *filehandle,
                                          GwyDebugObjectsDumpFlags flags);
void gwy_debug_objects_clear             (void);

G_END_DECLS

#endif /* __GWY_DEBUG_OBJECTS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

