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

#include <libgwyddion/gwywatchable.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwymacros.h>
#include "gwygraphepitome.h"

#define GWY_GRAPH_EPITOME_TYPE_NAME "GwyGraphEpitome"

static void     gwy_graph_epitome_class_init        (GwyGraphEpitomeClass *klass);
static void     gwy_graph_epitome_init              (GwyGraphEpitome *graph_epitome);
static void     gwy_graph_epitome_finalize          (GObject *object);
static void     gwy_graph_epitome_serializable_init (GwySerializableIface *iface);
static void     gwy_graph_epitome_watchable_init    (GwyWatchableIface *iface);
static GByteArray* gwy_graph_epitome_serialize      (GObject *obj,
                                                     GByteArray*buffer);
static GObject* gwy_graph_epitome_deserialize       (const guchar *buffer,
                                                     gsize size,
                                                     gsize *position);
static GObject* gwy_graph_epitome_duplicate         (GObject *object);


static GObjectClass *parent_class = NULL;


GType
gwy_graph_epitome_get_type(void)
{
    static GType gwy_graph_epitome_type = 0;

    if (!gwy_graph_epitome_type) {
        static const GTypeInfo gwy_graph_epitome_info = {
            sizeof(GwyGraphEpitomeClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_epitome_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphEpitome),
            0,
            (GInstanceInitFunc)gwy_graph_epitome_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_graph_epitome_serializable_init, NULL, 0
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_graph_epitome_watchable_init, NULL, 0
        };

        gwy_debug("");
        gwy_graph_epitome_type
          = g_type_register_static(G_TYPE_OBJECT,
                                   GWY_GRAPH_EPITOME_TYPE_NAME,
                                   &gwy_graph_epitome_info,
                                   0);
        g_type_add_interface_static(gwy_graph_epitome_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_graph_epitome_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_graph_epitome_type;
}

static void
gwy_graph_epitome_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_graph_epitome_serialize;
    iface->deserialize = gwy_graph_epitome_deserialize;
    iface->duplicate = gwy_graph_epitome_duplicate;
}

static void
gwy_graph_epitome_watchable_init(GwyWatchableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_graph_epitome_class_init(GwyGraphEpitomeClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_graph_epitome_finalize;
}

static void
gwy_graph_epitome_init(GwyGraphEpitome *graph_epitome)
{
    gwy_debug("");
    graph_epitome->ncurves = 0;
    graph_epitome->curves = NULL;
    graph_epitome->has_x_unit = FALSE;
    graph_epitome->has_y_unit = FALSE;
    graph_epitome->x_unit = NULL;
    graph_epitome->y_unit = NULL;
    graph_epitome->top_label = NULL;
    graph_epitome->bottom_label = NULL;
    graph_epitome->left_label = NULL;
    graph_epitome->right_label = NULL;

    graph_epitome->weak_graph = NULL;
}

/**
 * gwy_graph_epitome_new:
 * @graph: A graph to represent.
 *
 * Creates a new graph epitome.
 *
 * Returns: New graph epitome as a #GObject.
 **/
GObject*
gwy_graph_epitome_new(GwyGraph *graph)
{
    GwyGraphEpitome *graph_epitome;

    gwy_debug("");
    graph_epitome = g_object_new(GWY_TYPE_GRAPH_EPITOME, NULL);

    g_object_weak_ref(graph, NULL, graph_epitome);

    return (GObject*)(graph_epitome);
}

static void
gwy_graph_epitome_clear(GwyGraphEpitome *graph_epitome)
{
    if (graph_epitome->curves) {
        gint i;

        g_assert(graph_epitome->ncurves);
        for (i = 0; i < graph_epitome->ncurves; i++) {
            g_free(graph_epitome->curves[i].xdata);
            g_free(graph_epitome->curves[i].ydata);
            g_free(graph_epitome->curves[i].params);
            g_free(graph_epitome->curves[i].label);
        }
        g_free(graph_epitome->curves);
    }

    g_free(graph_epitome->x_unit);
    g_free(graph_epitome->y_unit);
    g_free(graph_epitome->top_label);
    g_free(graph_epitome->bottom_label);
    g_free(graph_epitome->left_label);
    g_free(graph_epitome->right_label);

    graph_epitome->ncurves = 0;
    graph_epitome->curves = NULL;
    graph_epitome->has_x_unit = FALSE;
    graph_epitome->has_y_unit = FALSE;
    graph_epitome->x_unit = NULL;
    graph_epitome->y_unit = NULL;
    graph_epitome->top_label = NULL;
    graph_epitome->bottom_label = NULL;
    graph_epitome->left_label = NULL;
    graph_epitome->right_label = NULL;
    /* do not reset weak_graph! */
}

static void
gwy_graph_epitome_finalize(GObject *object)
{
    GwyGraphEpitome *graph_epitome;

    gwy_debug("");

    graph_epitome = GWY_GRAPH_EPITOME(object);
    if (graph_epitome->weak_graph)
        g_object_weak_unref(graph_epitome->weak_graph,
                            NULL,
                            &graph_epitome->weak_graph);
    else
        gwy_graph_epitome_clear(graph_epitome);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* actually copy data from a -- usually dying -- graph */
static void
gwy_graph_epitome_save_graph(GwyGraphEpitome *graph_epitome,
                             GwyGraph *graph)
{
    gwy_debug("");
    /* TODO */
}

static GByteArray*
gwy_graph_epitome_serialize(GObject *obj,
                            GByteArray*buffer)
{
    GwyGraphEpitome *graph_epitome;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_EPITOME(obj), NULL);

    graph_epitome = GWY_GRAPH_EPITOME(obj);
    /* TODO
    {
        GwySerializeSpec spec[] = {
            { 'd', "theta", &graph_epitome->theta, NULL },
            { 'd', "phi", &graph_epitome->phi, NULL },
        };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_GRAPH_EPITOME_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
    */
}

static GObject*
gwy_graph_epitome_deserialize(const guchar *buffer,
                              gsize size,
                              gsize *position)
{
    /*
    gdouble theta, phi;
    GwySerializeSpec spec[] = {
        { 'd', "theta", &theta, NULL },
        { 'd', "phi", &phi, NULL },
    };
    */

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    /* TODO
    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_GRAPH_EPITOME_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec))
        return NULL;

    return (GObject*)gwy_graph_epitome_new(theta, phi);
    */
    return NULL;
}

static GObject*
gwy_graph_epitome_duplicate(GObject *object)
{
    GwyGraphEpitome *graph_epitome;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_EPITOME(object), NULL);

    graph_epitome = GWY_GRAPH_EPITOME(object);
    if (graph_epitome->weak_graph)
        return G_OBJECT(gwy_graph_epitome_new(graph_epitome->weak_graph));
    else {
        /* TODO: copy data the hard way */
        1;
    }

    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
