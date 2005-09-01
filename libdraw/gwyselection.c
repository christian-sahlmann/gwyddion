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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyserializable.h>
#include <libdraw/gwyselection.h>

enum {
    CHANGED,
    FINISHED,
    LAST_SIGNAL
};

static void      gwy_selection_finalize           (GObject *object);
static void      gwy_selection_serializable_init  (GwySerializableIface *iface);
static void      gwy_selection_clear_default      (GwySelection *selection);
static gint      gwy_selection_get_data_default   (GwySelection *selection,
                                                   gdouble *data);
static void      gwy_selection_set_data_default   (GwySelection *selection,
                                                   gint nselected,
                                                   const gdouble *data);
static void  gwy_selection_set_max_objects_default(GwySelection *selection,
                                                   gint max_objects);
static GByteArray* gwy_selection_serialize_default(GObject *obj,
                                                   GByteArray *buffer);
static GObject*  gwy_selection_deserialize_default(const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject*  gwy_selection_duplicate_default  (GObject *object);
static void      gwy_selection_clone_default      (GObject *source,
                                                   GObject *copy);

static guint selection_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwySelection, gwy_selection, G_TYPE_OBJECT, G_TYPE_FLAG_ABSTRACT,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_selection_serializable_init))

static void
gwy_selection_class_init(GwySelectionClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_selection_finalize;

    klass->clear = gwy_selection_clear_default;
    klass->get_data = gwy_selection_get_data_default;
    klass->set_data = gwy_selection_set_data_default;
    klass->set_max_objects = gwy_selection_set_max_objects_default;

    selection_signals[CHANGED]
        = g_signal_new("changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwySelectionClass, changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    selection_signals[FINISHED]
        = g_signal_new("finished",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwySelectionClass, finished),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_selection_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_selection_serialize_default;
    iface->deserialize = gwy_selection_deserialize_default;
    iface->duplicate = gwy_selection_duplicate_default;
    iface->clone = gwy_selection_clone_default;
}

static void
gwy_selection_init(G_GNUC_UNUSED GwySelection *selection)
{
}

static void
gwy_selection_finalize(GObject *object)
{
    GwySelection *selection = (GwySelection*)object;

    g_array_free(selection->objects, TRUE);
    G_OBJECT_CLASS(gwy_selection_parent_class)->finalize(object);
}

/**
 * gwy_selection_get_object_size:
 * @selection: A selection.
 *
 * Gets the number of coordinates that make up a one selection object.
 **/
gint
gwy_selection_get_object_size(GwySelection *selection)
{
    g_return_val_if_fail(GWY_IS_SELECTION(selection), 0);
    return GWY_SELECTION_GET_CLASS(selection)->object_size;
}

/**
 * gwy_selection_clear:
 * @selection: A selection.
 *
 * Clears a selection.
 **/
void
gwy_selection_clear(GwySelection *selection)
{
    g_return_if_fail(GWY_IS_SELECTION(selection));
    GWY_SELECTION_GET_CLASS(selection)->clear(selection);
}

/**
 * gwy_selection_get_data:
 * @selection: A selection.
 * @data: Pointer to array to store selection data to.  Selection data is an
 *        array of coordinates whose precise meaning is defined by particular
 *        selection types.
 *
 * Gets selection data.
 *
 * Returns: The number of selected objects.  This is *not* the required size
 *          of @data, which must be at least gwy_selection_get_object_size()
 *          times larger.
 **/
gint
gwy_selection_get_data(GwySelection *selection,
                       gdouble *data)
{
    g_return_val_if_fail(GWY_IS_SELECTION(selection), 0);
    return GWY_SELECTION_GET_CLASS(selection)->get_data(selection, data);
}

/**
 * gwy_selection_set_data:
 * @selection: A selection.
 * @nselected: The number of selected objects.
 * @data: Selection data, that is an array @nselected *
 *        gwy_selection_get_object_size() long with selected object
 *        coordinates.
 *
 * Sets selection data.
 **/
void
gwy_selection_set_data(GwySelection *selection,
                       gint nselected,
                       const gdouble *data)
{
    g_return_if_fail(GWY_IS_SELECTION(selection));
    GWY_SELECTION_GET_CLASS(selection)->set_data(selection, nselected, data);
}

/**
 * gwy_selection_get_max_objects:
 * @selection: A selection.
 *
 * Gets the maximum number of selected objects.
 *
 * Returns: The maximum number of selected objects;
 **/
gint
gwy_selection_get_max_objects(GwySelection *selection)
{
    g_return_val_if_fail(GWY_IS_SELECTION(selection), 0);
    return selection->max_objects;
}

/**
 * gwy_selection_set_max_objects:
 * @selection: A selection.
 * @max_objects: The maximum number of objects allowed to select.  Note
 *               particular selection types may allow only specific values.
 *
 * Sets the maximum number of objects allowed to select.
 *
 * When selection reaches this number of selected objects, it emits
 * "finished" signal.
 **/
void
gwy_selection_set_max_objects(GwySelection *selection,
                              gint max_objects)
{
    g_return_if_fail(GWY_IS_SELECTION(selection));
    GWY_SELECTION_GET_CLASS(selection)->set_max_objects(selection, max_objects);
}

/**
 * gwy_selection_changed:
 * @selection: A selection.
 *
 * Emits "changed" signal on a selection.
 **/
void
gwy_selection_changed(GwySelection *selection)
{
    g_return_if_fail(GWY_IS_SELECTION(selection));
    g_signal_emit(selection, selection_signals[CHANGED], 0);
}

/**
 * gwy_selection_finished:
 * @selection: A selection.
 *
 * Emits "finished" signal on a selection.
 **/
void
gwy_selection_finished(GwySelection *selection)
{
    g_return_if_fail(GWY_IS_SELECTION(selection));
    g_signal_emit(selection, selection_signals[FINISHED], 0);
}

static void
gwy_selection_clear_default(GwySelection *selection)
{
    if (!selection->objects->len)
        return;

    g_array_set_size(selection->objects, 0);
    g_signal_emit(selection, selection_signals[CHANGED], 0);
}

static gint
gwy_selection_get_data_default(GwySelection *selection,
                               gdouble *data)
{
    gint n, object_size;

    object_size = GWY_SELECTION_GET_CLASS(selection)->object_size;
    n = selection->objects->len/object_size;
    if (data && n)
        memcpy(data, selection->objects->data, n*sizeof(gdouble));

    return n;
}

static void
gwy_selection_set_data_default(GwySelection *selection,
                               gint nselected,
                               const gdouble *data)
{
    gint object_size;

    if (nselected > selection->max_objects) {
        g_warning("nselected larger than max. number of objects");
        nselected = selection->max_objects;
    }

    g_array_set_size(selection->objects, 0);
    if (!nselected) {
        gwy_selection_changed(selection);
        return;
    }

    g_return_if_fail(data);
    object_size = GWY_SELECTION_GET_CLASS(selection)->object_size;
    g_array_append_vals(selection->objects, data, nselected*object_size);

    g_signal_emit(selection, selection_signals[CHANGED], 0);
    if (nselected == selection->max_objects)
        g_signal_emit(selection, selection_signals[FINISHED], 0);
}

static void
gwy_selection_set_max_objects_default(GwySelection *selection,
                                      gint max_objects)
{
    gint n, object_size;

    g_return_if_fail(max_objects < 1);
    object_size = GWY_SELECTION_GET_CLASS(selection)->object_size;
    n = selection->objects->len/object_size;
    if (max_objects == n)
        return;

    g_array_set_size(selection->objects, max_objects*object_size);

    if (max_objects < n) {
        g_signal_emit(selection, selection_signals[CHANGED], 0);
        g_signal_emit(selection, selection_signals[FINISHED], 0);
    }
}

static GByteArray*
gwy_selection_serialize_default(GObject *obj,
                                GByteArray *buffer)
{
    GwySelection *selection;
    gint object_size;

    g_return_val_if_fail(GWY_IS_SELECTION(obj), NULL);

    selection = GWY_SELECTION(obj);
    object_size = GWY_SELECTION_GET_CLASS(selection)->object_size;
    {
        guint32 len = selection->objects->len * object_size;
        const gchar *name = g_type_name(G_TYPE_FROM_INSTANCE(obj));
        GwySerializeSpec spec[] = {
            { 'D', "data", &selection->objects->data, &len, },
        };

        return gwy_serialize_pack_object_struct(buffer, name,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_selection_deserialize_default(const guchar *buffer,
                                  gsize size,
                                  gsize *position)
{
    gdouble *data = NULL;
    guint32 len = 0;
    GwySerializeSpec spec[] = {
        { 'D', "data", &data, &len, },
    };
    gsize typenamesize;
    GType type;
    gint object_size;
    const gchar *typename;
    GwySelection *selection;

    g_return_val_if_fail(buffer, NULL);

    typenamesize = gwy_serialize_check_string(buffer, size, *position, NULL);
    if (!typenamesize)
        return NULL;
    typename = (const gchar*)(buffer + *position);

    if (!(type = g_type_from_name(typename))
        || !g_type_is_a(type, GWY_TYPE_SELECTION)
        || !G_TYPE_IS_INSTANTIATABLE(type))
        return NULL;

    if (!gwy_serialize_unpack_object_struct(buffer, size, position, typename,
                                            G_N_ELEMENTS(spec), spec))
        return NULL;

    selection = g_object_new(type, NULL);
    object_size = GWY_SELECTION_GET_CLASS(selection)->object_size;
    if (data && len) {
        if (len % object_size)
            g_warning("Selection data size not multiple of object size. "
                      "Ignoring it.");
        else {
            g_array_append_vals(selection->objects, data, len*object_size);
            /* Just set some reasonable value, it should be overriden later */
            selection->max_objects = len;
        }
        g_free(data);
    }

    return (GObject*)selection;
}

static GObject*
gwy_selection_duplicate_default(GObject *object)
{
    GwySelection *selection, *duplicate;

    g_return_val_if_fail(GWY_IS_SELECTION(object), NULL);
    selection = GWY_SELECTION(object);
    duplicate = g_object_new(G_TYPE_FROM_INSTANCE(object), NULL);
    g_array_append_vals(duplicate->objects,
                        selection->objects->data, selection->objects->len);
    duplicate->max_objects = selection->max_objects;

    return (GObject*)duplicate;
}

static void
gwy_selection_clone_default(GObject *source, GObject *copy)
{
    GwySelection *selection, *clone;
    gint object_size;

    g_return_if_fail(GWY_IS_SELECTION(source));
    g_return_if_fail(GWY_IS_SELECTION(copy));
    /* is-a relation is cheched by gwy_serlizable_clone() */

    selection = GWY_SELECTION(source);
    clone = GWY_SELECTION(copy);
    object_size = GWY_SELECTION_GET_CLASS(selection)->object_size;

    g_array_set_size(clone->objects, 0);
    g_array_append_vals(clone->objects,
                        selection->objects->data, selection->objects->len);
    clone->max_objects = selection->max_objects;

    g_signal_emit(clone, selection_signals[CHANGED], 0);
    if (clone->max_objects == clone->objects->len/object_size)
        g_signal_emit(clone, selection_signals[FINISHED], 0);
}

/************************** Documentation ****************************/

/**
 * GwySelection:
 *
 * The #GwySelection struct contains private data only and should be accessed
 * using the functions below.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
