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

#include <string.h>

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
static void     gwy_graph_epitome_save_graph        (GwyGraphEpitome *graph_epitome,
                                                     GwyGraph *graph);


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

    graph_epitome->graph = NULL;
    graph_epitome->graph_destroy_hid = 0;

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

    graph_epitome->graph = graph;
    if (graph) {
        graph_epitome->graph_destroy_hid
            = g_signal_connect_swapped(graph, "destroy",
                                       G_CALLBACK(gwy_graph_epitome_save_graph),
                                       graph_epitome);
    }

    return (GObject*)(graph_epitome);
}

static void
gwy_graph_epitome_clear(GwyGraphEpitome *graph_epitome)
{
    if (graph_epitome->curves) {
        gint i;

        g_assert(graph_epitome->ncurves);
        for (i = 0; i < graph_epitome->ncurves; i++) {
            GwyGraphEpitomeCurve *curve = graph_epitome->curves + i;

            g_free(curve->xdata);
            g_free(curve->ydata);
            if (curve->params->description)
                g_string_free(curve->params->description, TRUE);
            g_free(curve->params);
        }
        g_free(graph_epitome->curves);
    }

    if (graph_epitome->x_unit)
        g_string_free(graph_epitome->x_unit, TRUE);
    if (graph_epitome->y_unit)
        g_string_free(graph_epitome->y_unit, TRUE);
    if (graph_epitome->top_label)
        g_string_free(graph_epitome->top_label, TRUE);
    if (graph_epitome->bottom_label)
        g_string_free(graph_epitome->bottom_label, TRUE);
    if (graph_epitome->left_label)
        g_string_free(graph_epitome->left_label, TRUE);
    if (graph_epitome->right_label)
        g_string_free(graph_epitome->right_label, TRUE);

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

    /* do not reset widget references here! */
}

static void
gwy_graph_epitome_finalize(GObject *object)
{
    GwyGraphEpitome *graph_epitome;

    gwy_debug("");

    graph_epitome = GWY_GRAPH_EPITOME(object);
    if (graph_epitome->graph_destroy_hid) {
        g_assert(GWY_IS_GRAPH(graph_epitome->graph));
        g_signal_handler_disconnect(graph_epitome->graph,
                                    graph_epitome->graph_destroy_hid);
    }
    else
        gwy_graph_epitome_clear(graph_epitome);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static GString*
gwy_gstring_update(GString *dest, gchar *src)
{
    if (!src && !dest)
        return NULL;

    if (!src) {
        g_string_free(dest, TRUE);
        return NULL;
    }

    if (!dest)
        return g_string_new(src);

    /* FIXME: efficient? */
    if (strcmp(src, dest->str) == 0)
        return dest;

    return g_string_assign(dest, src);
}

/* actually copy data from a -- usually just dying -- graph */
static void
gwy_graph_epitome_save_graph(GwyGraphEpitome *graph_epitome,
                             GwyGraph *graph)
{
    GString *str;
    gint i, nacurves;

    gwy_debug("");
    g_assert(graph == graph_epitome->graph);

    /* FIXME: we access object fields directly now as we are supposed to know
     * some their internals anyway. */
    /* graph */
    graph_epitome->has_x_unit = graph->has_x_unit;
    graph_epitome->x_unit = gwy_gstring_update(graph_epitome->x_unit,
                                               graph->x_unit);

    graph_epitome->has_y_unit = graph->has_y_unit;
    graph_epitome->y_unit = gwy_gstring_update(graph_epitome->y_unit,
                                               graph->y_unit);

    graph_epitome->x_reqmin = graph->x_reqmin;
    graph_epitome->y_reqmin = graph->y_reqmin;
    graph_epitome->x_reqmax = graph->x_reqmax;
    graph_epitome->y_reqmax = graph->y_reqmax;

    /* axes */
    str = gwy_axis_get_label(graph->axis_top);
    graph_epitome->top_label
        = gwy_gstring_update(graph_epitome->top_label,
                             str ? str->str : NULL);

    str = gwy_axis_get_label(graph->axis_bottom);
    graph_epitome->bottom_label
        = gwy_gstring_update(graph_epitome->bottom_label,
                             str ? str->str : NULL);

    str = gwy_axis_get_label(graph->axis_left);
    graph_epitome->left_label
        = gwy_gstring_update(graph_epitome->left_label,
                             str ? str->str : NULL);

    str = gwy_axis_get_label(graph->axis_right);
    graph_epitome->right_label
        = gwy_gstring_update(graph_epitome->right_label,
                             str ? str->str : NULL);

    /* curves */
    /* somewhat hairy; trying to avoid redundant reallocations:
     * 1. clear extra curves that epitome has and graph has not
     * 2. realloc curves to the right size
     * 3. replace already existing curves  <-- if lucky, only this happens
     * 4. fill new curves
     */
    nacurves = graph->area->curves->len;
    /* 1. clear */
    for (i = nacurves; i < graph_epitome->ncurves; i++) {
        GwyGraphEpitomeCurve *curve = graph_epitome->curves + i;

        g_free(curve->xdata);
        g_free(curve->ydata);
        if (curve->params->description)
            g_string_free(curve->params->description, TRUE);
        g_free(curve->params);
    }
    /* 2. realloc */
    graph_epitome->curves = g_renew(GwyGraphEpitomeCurve,
                                    graph_epitome->curves,
                                    nacurves);
    /* 3. replace */
    for (i = 0; i < graph_epitome->ncurves; i++) {
        GwyGraphEpitomeCurve *curve = graph_epitome->curves + i;
        GwyGraphAreaCurve *acurve = g_ptr_array_index(graph->area->curves, i);

        curve->n = acurve->data.N;
        curve->xdata = g_renew(gdouble, curve->xdata, curve->n);
        memcpy(curve->xdata, acurve->data.xvals, curve->n*sizeof(gdouble));
        curve->ydata = g_renew(gdouble, curve->ydata, curve->n);
        memcpy(curve->ydata, acurve->data.yvals, curve->n*sizeof(gdouble));

        /* save description GString before overwrite, then set it again */
        str = curve->params->description;
        memcpy(curve->params, &acurve->params, sizeof(GwyGraphAreaCurveParams));
        curve->params->description
            = g_string_assign(str, acurve->params.description->str);
    }
    /* 4. fill */
    for (i = graph_epitome->ncurves; i < nacurves; i++) {
        GwyGraphEpitomeCurve *curve = graph_epitome->curves + i;
        GwyGraphAreaCurve *acurve = g_ptr_array_index(graph->area->curves, i);

        curve->n = acurve->data.N;
        curve->xdata = g_memdup(acurve->data.xvals,
                                curve->n*sizeof(gdouble));
        curve->ydata = g_memdup(acurve->data.yvals,
                                curve->n*sizeof(gdouble));

        curve->params = g_memdup(&acurve->params,
                                 sizeof(GwyGraphAreaCurveParams));
        curve->params->description
            = g_string_new(acurve->params.description->str);
    }

    graph_epitome->ncurves = nacurves;

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
    if (graph_epitome->graph)
        return gwy_graph_epitome_new(graph_epitome->graph);
    else {
        GwyGraphEpitome *duplicate;
        gint i;

        duplicate = (GwyGraphEpitome*)gwy_graph_epitome_new(NULL);
        /* widget stuff is already initialized to NULL */
        duplicate->has_x_unit = graph_epitome->has_x_unit;
        duplicate->has_y_unit = graph_epitome->has_y_unit;
        duplicate->x_reqmin = graph_epitome->x_reqmin;
        duplicate->y_reqmin = graph_epitome->y_reqmin;
        duplicate->x_reqmax = graph_epitome->x_reqmax;
        duplicate->y_reqmax = graph_epitome->y_reqmax;
        duplicate->x_unit
            = graph_epitome->x_unit
              ? g_string_new(graph_epitome->x_unit->str) : NULL;
        duplicate->y_unit
            = graph_epitome->y_unit
              ? g_string_new(graph_epitome->y_unit->str) : NULL;
        duplicate->top_label
            = graph_epitome->top_label
              ? g_string_new(graph_epitome->top_label->str) : NULL;
        duplicate->bottom_label
            = graph_epitome->bottom_label
              ? g_string_new(graph_epitome->bottom_label->str) : NULL;
        duplicate->left_label
            = graph_epitome->left_label
              ? g_string_new(graph_epitome->left_label->str) : NULL;
        duplicate->right_label
            = graph_epitome->right_label
              ? g_string_new(graph_epitome->right_label->str) : NULL;
        duplicate->ncurves = graph_epitome->ncurves;
        duplicate->curves
            = g_memdup(graph_epitome->curves,
                       graph_epitome->ncurves*sizeof(GwyGraphEpitomeCurve));
        for (i = 0; i < duplicate->ncurves; i++) {
            GwyGraphEpitomeCurve *curve = duplicate->curves + i;

            curve->xdata = g_memdup(curve->xdata, curve->n*sizeof(gdouble));
            curve->ydata = g_memdup(curve->ydata, curve->n*sizeof(gdouble));
            curve->params = g_memdup(curve->params,
                                     sizeof(GwyGraphAreaCurveParams));
            curve->params->description = g_strdup(curve->params->description);
        }
    }

    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
