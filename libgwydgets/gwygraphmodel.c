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

#include <libgwyddion/gwyddion.h>
#include "gwygraphepitome.h"

#define GWY_GRAPH_EPITOME_TYPE_NAME "GwyGraphEpitome"

static void   gwy_graph_epitome_class_init        (GwyGraphEpitomeClass *klass);
static void   gwy_graph_epitome_init              (GwyGraphEpitome *gepitome);
static void   gwy_graph_epitome_finalize          (GObject *object);
static void   gwy_graph_epitome_serializable_init (GwySerializableIface *iface);
static void   gwy_graph_epitome_watchable_init    (GwyWatchableIface *iface);
static GByteArray* gwy_graph_epitome_serialize    (GObject *obj,
                                                   GByteArray*buffer);
static GObject* gwy_graph_epitome_deserialize     (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject* gwy_graph_epitome_duplicate       (GObject *object);
static void   gwy_graph_epitome_graph_destroyed   (GwyGraph *graph,
                                                   GwyGraphEpitome *gepitome);
static void   gwy_graph_epitome_save_graph        (GwyGraphEpitome *gepitome,
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
gwy_graph_epitome_init(GwyGraphEpitome *gepitome)
{
    gwy_debug("");

    gepitome->graph = NULL;
    gepitome->graph_destroy_hid = 0;

    gepitome->ncurves = 0;
    gepitome->curves = NULL;
    gepitome->has_x_unit = FALSE;
    gepitome->has_y_unit = FALSE;
    gepitome->x_unit = gwy_si_unit_new("");
    gepitome->y_unit = gwy_si_unit_new("");
    gepitome->top_label = g_string_new("");
    gepitome->bottom_label = g_string_new("");
    gepitome->left_label = g_string_new("");
    gepitome->right_label = g_string_new("");
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
    GwyGraphEpitome *gepitome;

    gwy_debug("");
    gepitome = g_object_new(GWY_TYPE_GRAPH_EPITOME, NULL);

    gepitome->graph = graph;
    if (graph) {
        gepitome->graph_destroy_hid
            = g_signal_connect(graph, "destroy",
                               G_CALLBACK(gwy_graph_epitome_graph_destroyed),
                               gepitome);
    }

    return (GObject*)(gepitome);
}

static void
gwy_graph_epitome_finalize(GObject *object)
{
    GwyGraphEpitome *gepitome;
    gint i;

    gwy_debug("");

    gepitome = GWY_GRAPH_EPITOME(object);
    if (gepitome->graph_destroy_hid) {
        g_assert(GWY_IS_GRAPH(gepitome->graph));
        g_signal_handler_disconnect(gepitome->graph,
                                    gepitome->graph_destroy_hid);
    }

    g_object_unref(gepitome->x_unit);
    g_object_unref(gepitome->y_unit);

    g_string_free(gepitome->top_label, TRUE);
    g_string_free(gepitome->bottom_label, TRUE);
    g_string_free(gepitome->left_label, TRUE);
    g_string_free(gepitome->right_label, TRUE);

    for (i = 0; i < gepitome->ncurves; i++) {
        GwyGraphEpitomeCurve *curve = gepitome->curves + i;

        g_free(curve->xdata);
        g_free(curve->ydata);
        g_string_free(curve->params->description, TRUE);
        g_free(curve->params);
    }
    g_free(gepitome->curves);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_graph_epitome_graph_destroyed(GwyGraph *graph,
                                  GwyGraphEpitome *gepitome)
{
    gwy_graph_epitome_save_graph(gepitome, graph);
    g_signal_handler_disconnect(gepitome->graph, gepitome->graph_destroy_hid);
    gepitome->graph_destroy_hid = 0;
    gepitome->graph = NULL;
}

/* actually copy save from a -- usually just dying -- graph */
static void
gwy_graph_epitome_save_graph(GwyGraphEpitome *gepitome,
                             GwyGraph *graph)
{
    GString *str;
    gint i, nacurves;

    gwy_debug("");
    g_assert(graph && graph == gepitome->graph);

    /* FIXME: we access object fields directly now as we are supposed to know
     * some their internals anyway. */
    /* graph */
    if ((gepitome->has_x_unit = graph->has_x_unit))
        gwy_si_unit_set_unit_string(GWY_SI_UNIT(gepitome->x_unit),
                                    graph->x_unit);
    else
        gwy_object_unref(graph->x_unit);

    if ((gepitome->has_y_unit = graph->has_y_unit))
        gwy_si_unit_set_unit_string(GWY_SI_UNIT(gepitome->y_unit),
                                    graph->y_unit);
    else
        gwy_object_unref(graph->y_unit);

    gepitome->x_reqmin = graph->x_reqmin;
    gepitome->y_reqmin = graph->y_reqmin;
    gepitome->x_reqmax = graph->x_reqmax;
    gepitome->y_reqmax = graph->y_reqmax;

    /* axes */
    g_string_assign(gepitome->top_label,
                    gwy_axis_get_label(graph->axis_top)->str);
    g_string_assign(gepitome->bottom_label,
                    gwy_axis_get_label(graph->axis_bottom)->str);
    g_string_assign(gepitome->left_label,
                    gwy_axis_get_label(graph->axis_left)->str);
    g_string_assign(gepitome->right_label,
                    gwy_axis_get_label(graph->axis_right)->str);

    /* curves */
    /* somewhat hairy; trying to avoid redundant reallocations:
     * 1. clear extra curves that epitome has and graph has not
     * 2. realloc curves to the right size
     * 3. replace already existing curves  <-- if lucky, only this happens
     * 4. fill new curves
     */
    nacurves = graph->area->curves->len;
    /* 1. clear */
    for (i = nacurves; i < gepitome->ncurves; i++) {
        GwyGraphEpitomeCurve *curve = gepitome->curves + i;

        g_free(curve->xdata);
        g_free(curve->ydata);
        g_string_free(curve->params->description, TRUE);
        g_free(curve->params);
    }
    /* 2. realloc */
    gepitome->curves = g_renew(GwyGraphEpitomeCurve,
                               gepitome->curves, nacurves);
    /* 3. replace */
    for (i = 0; i < gepitome->ncurves; i++) {
        GwyGraphEpitomeCurve *curve = gepitome->curves + i;
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
    for (i = gepitome->ncurves; i < nacurves; i++) {
        GwyGraphEpitomeCurve *curve = gepitome->curves + i;
        GwyGraphAreaCurve *acurve = g_ptr_array_index(graph->area->curves, i);

        curve->n = acurve->data.N;
        curve->xdata = g_memdup(acurve->data.xvals, curve->n*sizeof(gdouble));
        curve->ydata = g_memdup(acurve->data.yvals, curve->n*sizeof(gdouble));

        curve->params = g_memdup(&acurve->params,
                                 sizeof(GwyGraphAreaCurveParams));
        curve->params->description
            = g_string_new(acurve->params.description->str);
    }

    gepitome->ncurves = nacurves;
}

static GByteArray*
gwy_graph_epitome_serialize(GObject *obj,
                            GByteArray*buffer)
{
    GwyGraphEpitome *gepitome;
    gsize before_obj;
    gint i;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_EPITOME(obj), NULL);

    buffer = gwy_serialize_pack(buffer, "si", GWY_GRAPH_EPITOME_TYPE_NAME, 0);
    before_obj = buffer->len;

    gepitome = GWY_GRAPH_EPITOME(obj);
    gwy_graph_epitome_save_graph(gepitome, gepitome->graph);
    /* Global data, serialized as a fake subobject GwyGraphEpitome-graph */
    {
        gchar *x_unit, *y_unit;
        GwySerializeSpec spec[] = {
            { 'b', "has_x_unit", &gepitome->has_x_unit, NULL },
            { 'b', "has_y_unit", &gepitome->has_y_unit, NULL },
            { 's', "x_unit", &x_unit, NULL },
            { 's', "y_unit", &y_unit, NULL },
            { 's', "top_label", &gepitome->top_label->str, NULL },
            { 's', "bottom_label", &gepitome->bottom_label->str, NULL },
            { 's', "left_label", &gepitome->left_label->str, NULL },
            { 's', "right_label", &gepitome->right_label->str, NULL },
            { 'd', "x_reqmin", &gepitome->x_reqmin, NULL },
            { 'd', "y_reqmin", &gepitome->y_reqmin, NULL },
            { 'd', "x_reqmax", &gepitome->x_reqmax, NULL },
            { 'd', "y_reqmax", &gepitome->y_reqmax, NULL },
            { 'i', "ncurves", &gepitome->ncurves, NULL },
        };

        x_unit = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gepitome->x_unit));
        y_unit = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gepitome->y_unit));
        gwy_serialize_pack_object_struct(buffer,
                                         "GwyGraphEpitome-graph",
                                         G_N_ELEMENTS(spec), spec);
        /* XXX: why the fucking gwy_si_unit_get_unit_string() can't return
         * a const? */
        g_free(x_unit);
        g_free(y_unit);
    }
    /* Per-curve data, serialized as fake subobjects GwyGraphEpitome-curve */
    for (i = 0; i < gepitome->ncurves; i++) {
        GwyGraphEpitomeCurve *curve = gepitome->curves + i;
        GwyGraphAreaCurveParams *params = curve->params;
        gboolean is_line, is_point;
        GwySerializeSpec spec[] = {
            { 'D', "xdata", &curve->xdata, &curve->n },
            { 'D', "ydata", &curve->ydata, &curve->n },
            { 'b', "is_line", &is_line, NULL },
            { 'b', "is_point", &is_point, NULL },
            { 'i', "point_type", &params->point_type, NULL },
            { 'i', "point_size", &params->point_size, NULL },
            { 'i', "line_style", &params->line_style, NULL },
            { 'i', "line_size", &params->line_size, NULL },
            { 's', "description", &params->description->str, NULL },
            { 'i', "color.pixel", &params->color.pixel, NULL },
        };

        is_line = params->is_line;
        is_point = params->is_point;
        gwy_serialize_pack_object_struct(buffer,
                                         "GwyGraphEpitome-curve",
                                         G_N_ELEMENTS(spec), spec);
    }

    gwy_serialize_store_int32(buffer, before_obj - sizeof(guint32),
                              buffer->len - before_obj);
    return buffer;
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

    /*********************** unpack obj struct:
    gsize mysize;
    gboolean ok;

    mysize = gwy_serialize_check_string(buffer, size, *position, object_name);
    g_return_val_if_fail(mysize, FALSE);
    *position += mysize;

    mysize = gwy_serialize_unpack_int32(buffer, size, position);
    ok = gwy_serialize_unpack_struct(buffer + *position, mysize, nspec, spec);
    *position += mysize;

    return ok;
    ****************************/

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
    GwyGraphEpitome *gepitome;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_EPITOME(object), NULL);

    gepitome = GWY_GRAPH_EPITOME(object);
    if (gepitome->graph)
        return gwy_graph_epitome_new(gepitome->graph);
    else {
        GwyGraphEpitome *duplicate;
        gint i;

        duplicate = (GwyGraphEpitome*)gwy_graph_epitome_new(NULL);
        /* widget stuff is already initialized to NULL */
        duplicate->has_x_unit = gepitome->has_x_unit;
        duplicate->has_y_unit = gepitome->has_y_unit;
        duplicate->x_reqmin = gepitome->x_reqmin;
        duplicate->y_reqmin = gepitome->y_reqmin;
        duplicate->x_reqmax = gepitome->x_reqmax;
        duplicate->y_reqmax = gepitome->y_reqmax;
        duplicate->x_unit = gwy_serializable_duplicate(gepitome->x_unit);
        duplicate->y_unit = gwy_serializable_duplicate(gepitome->y_unit);
        duplicate->top_label = g_string_new(gepitome->top_label->str);
        duplicate->bottom_label = g_string_new(gepitome->bottom_label->str);
        duplicate->left_label = g_string_new(gepitome->left_label->str);
        duplicate->right_label = g_string_new(gepitome->right_label->str);
        duplicate->ncurves = gepitome->ncurves;
        duplicate->curves
            = g_memdup(gepitome->curves,
                       gepitome->ncurves*sizeof(GwyGraphEpitomeCurve));
        for (i = 0; i < duplicate->ncurves; i++) {
            GwyGraphEpitomeCurve *curve = duplicate->curves + i;

            curve->xdata = g_memdup(curve->xdata, curve->n*sizeof(gdouble));
            curve->ydata = g_memdup(curve->ydata, curve->n*sizeof(gdouble));
            curve->params = g_memdup(curve->params,
                                     sizeof(GwyGraphAreaCurveParams));
            curve->params->description
                = g_string_new(curve->params->description->str);
        }
    }

    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
