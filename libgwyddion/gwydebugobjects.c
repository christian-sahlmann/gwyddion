/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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

#include "gwymacros.h"
#include "gwydebugobjects.h"

typedef struct {
    gulong id;
    gdouble create_time;
    gdouble destroy_time;
    GType type;
    gpointer address;
} DebugObjectInfo;

static gboolean debug_objects_enabled = FALSE;
static GMemChunk *debug_objects_chunk = NULL;
static GTimer *debug_objects_timer = NULL;
static GList *debug_objects = NULL;
static gsize id = 0;

/**
 * gwy_debug_objects_enable:
 * @enable: Whether object creation/destruction debugger should be enabled.
 *
 * Enables or disables the object creation/destruction debugger.
 *
 * When debugger is disabled, no new objects are noted, but destruction of
 * already watched ones is still noted.
 *
 * Since: 1.4.
 **/
void
gwy_debug_objects_enable(gboolean enable)
{
    debug_objects_enabled = enable;
}

static void
debug_objects_set_time(gpointer data, G_GNUC_UNUSED GObject *exobject)
{
    gdouble *time = (gdouble*)data;

    *time = g_timer_elapsed(debug_objects_timer, NULL);
}

/**
 * gwy_debug_objects_creation:
 * @object: An object to watch.
 *
 * Notes down @object and sets up watch for its destruction.
 *
 * This function should be called on object creation to get accurate creation
 * time, but can be in fact called anytime in object existence.
 *
 * There are two possible uses: In class implementation, where it should be
 * put into instance init function (constructors are less suited for that,
 * as there can be more than one, there can be deserializators, duplicators,
 * etc., and you want to hook them all). Or on the side of object user who
 * is concerned with object lifetime rules, he then calls it just after
 * object creation.
 *
 * Since: 1.4.
 **/
void
gwy_debug_objects_creation(GObject *object)
{
    DebugObjectInfo *info;

    if (!G_UNLIKELY(debug_objects_enabled))
        return;

    if (!id) {
        g_assert(!debug_objects_chunk
                 && !debug_objects_timer
                 && !debug_objects);
        debug_objects_chunk = g_mem_chunk_create(DebugObjectInfo, 256,
                                                 G_ALLOC_ONLY);
        debug_objects_timer = g_timer_new();
    }
    info = g_chunk_new(DebugObjectInfo, debug_objects_chunk);
    info->id = ++id;
    info->type = G_TYPE_FROM_INSTANCE(object);
    info->address = object;
    info->create_time = g_timer_elapsed(debug_objects_timer, NULL);
    info->destroy_time = -1;
    g_object_weak_ref(info->address, &debug_objects_set_time,
                      &info->destroy_time);
    debug_objects = g_list_prepend(debug_objects, info);

    gwy_debug("Added watch for %s %p",
              g_type_name(info->type), info->address);

}

/**
 * gwy_debug_objects_dump_to_file:
 * @filehandle: A filehandle open for writing.
 *
 * Dumps all recorded objects to a file.
 *
 * The format of each line is: object type name, object address, creation time,
 * destruction time (or ALIVE! message with reference count).
 *
 * Since: 1.4.
 **/
void
gwy_debug_objects_dump_to_file(FILE *filehandle)
{
    GList *l;
    DebugObjectInfo *info;

    for (l = g_list_last(debug_objects); l; l = g_list_previous(l)) {
        info = (DebugObjectInfo*)l->data;
        fprintf(filehandle, "%s %p %.3f ",
                g_type_name(info->type), info->address, info->create_time);
        if (info->destroy_time > 0)
            fprintf(filehandle, "%.3f\n", info->destroy_time);
        else
            fprintf(filehandle, "ALIVE(%d)!\n",
                    G_OBJECT(info->address)->ref_count);
    }
}

/**
 * gwy_debug_objects_clear:
 *
 * Frees all memory taken by debugger, removes all watches.
 *
 * Eventual following call to gwy_debug_objects_creation() will behave like
 * the very first one, including time counting reset.
 *
 * Since: 1.4.
 **/
void
gwy_debug_objects_clear(void)
{
    GList *l;
    DebugObjectInfo *info;

    if (!id)
        return;

    for (l = debug_objects; l; l = g_list_next(l)) {
        info = (DebugObjectInfo*)l->data;
        if (info->destroy_time < 0.0)
            g_object_weak_unref(info->address, &debug_objects_set_time,
                                &info->destroy_time);
    }
    g_mem_chunk_destroy(debug_objects_chunk);
    g_list_free(debug_objects);
    g_timer_destroy(debug_objects_timer);

    id = 0;
    debug_objects_chunk = NULL;
    debug_objects = NULL;
    debug_objects_timer = NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
