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
#define DEBUG 1
#include <string.h>

#include <libgwyddion/gwyddion.h>
#include "gwygraphmodel.h"

#define GWY_GRAPH_MODEL_TYPE_NAME "GwyGraphModel"

static void   gwy_graph_model_class_init        (GwyGraphModelClass *klass);
static void   gwy_graph_model_init              (GwyGraphModel *gmodel);
static void   gwy_graph_model_finalize          (GObject *object);
static void   gwy_graph_model_serializable_init (GwySerializableIface *iface);
static void   gwy_graph_model_watchable_init    (GwyWatchableIface *iface);
static GByteArray* gwy_graph_model_serialize    (GObject *obj,
                                                   GByteArray*buffer);
static GObject* gwy_graph_model_deserialize     (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject* gwy_graph_model_duplicate       (GObject *object);
static void   gwy_graph_model_graph_destroyed   (GwyGraph *graph,
                                                   GwyGraphModel *gmodel);
static void   gwy_graph_model_save_graph        (GwyGraphModel *gmodel,
                                                   GwyGraph *graph);


static GObjectClass *parent_class = NULL;


GType
gwy_graph_model_get_type(void)
{
    static GType gwy_graph_model_type = 0;

    if (!gwy_graph_model_type) {
        static const GTypeInfo gwy_graph_model_info = {
            sizeof(GwyGraphModelClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_model_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphModel),
            0,
            (GInstanceInitFunc)gwy_graph_model_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_graph_model_serializable_init, NULL, 0
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_graph_model_watchable_init, NULL, 0
        };

        gwy_debug("");
        gwy_graph_model_type
          = g_type_register_static(G_TYPE_OBJECT,
                                   GWY_GRAPH_MODEL_TYPE_NAME,
                                   &gwy_graph_model_info,
                                   0);
        g_type_add_interface_static(gwy_graph_model_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_graph_model_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_graph_model_type;
}

static void
gwy_graph_model_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_graph_model_serialize;
    iface->deserialize = gwy_graph_model_deserialize;
    iface->duplicate = gwy_graph_model_duplicate;
}

static void
gwy_graph_model_watchable_init(GwyWatchableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_graph_model_class_init(GwyGraphModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_graph_model_finalize;
}

static void
gwy_graph_model_init(GwyGraphModel *gmodel)
{
    gwy_debug("");

    gmodel->graph = NULL;
    gmodel->graph_destroy_hid = 0;

    gmodel->ncurves = 0;
    gmodel->curves = NULL;
    gmodel->has_x_unit = FALSE;
    gmodel->has_y_unit = FALSE;
    gmodel->x_unit = gwy_si_unit_new("");
    gmodel->y_unit = gwy_si_unit_new("");
    gmodel->top_label = g_string_new("");
    gmodel->bottom_label = g_string_new("");
    gmodel->left_label = g_string_new("");
    gmodel->right_label = g_string_new("");
}

/**
 * gwy_graph_model_new:
 * @graph: A graph to represent.
 *
 * Creates a new graph model.
 *
 * Returns: New graph model as a #GObject.
 **/
GObject*
gwy_graph_model_new(GwyGraph *graph)
{
    GwyGraphModel *gmodel;

    gwy_debug("");
    gmodel = g_object_new(GWY_TYPE_GRAPH_MODEL, NULL);

    gmodel->graph = graph;
    if (graph) {
        gmodel->graph_destroy_hid
            = g_signal_connect(graph, "destroy",
                               G_CALLBACK(gwy_graph_model_graph_destroyed),
                               gmodel);
    }

    return (GObject*)(gmodel);
}

static void
gwy_graph_model_finalize(GObject *object)
{
    GwyGraphModel *gmodel;
    gint i;

    gwy_debug("");

    gmodel = GWY_GRAPH_MODEL(object);
    if (gmodel->graph_destroy_hid) {
        g_assert(GWY_IS_GRAPH(gmodel->graph));
        g_signal_handler_disconnect(gmodel->graph,
                                    gmodel->graph_destroy_hid);
    }

    g_object_unref(gmodel->x_unit);
    g_object_unref(gmodel->y_unit);

    g_string_free(gmodel->top_label, TRUE);
    g_string_free(gmodel->bottom_label, TRUE);
    g_string_free(gmodel->left_label, TRUE);
    g_string_free(gmodel->right_label, TRUE);

    for (i = 0; i < gmodel->ncurves; i++) {
        GwyGraphModelCurve *curve = gmodel->curves + i;

        g_free(curve->xdata);
        g_free(curve->ydata);
        g_string_free(curve->params->description, TRUE);
        g_free(curve->params);
    }
    g_free(gmodel->curves);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_graph_model_graph_destroyed(GwyGraph *graph,
                                  GwyGraphModel *gmodel)
{
    gwy_debug("");
    gwy_graph_model_save_graph(gmodel, graph);
    g_signal_handler_disconnect(gmodel->graph, gmodel->graph_destroy_hid);
    gmodel->graph_destroy_hid = 0;
    gmodel->graph = NULL;
}

/* actually copy save from a -- usually just dying -- graph */
static void
gwy_graph_model_save_graph(GwyGraphModel *gmodel,
                             GwyGraph *graph)
{
    GString *str;
    gint i, nacurves;

    gwy_debug("");
    g_assert(graph && graph == gmodel->graph);

    /* FIXME: we access object fields directly now as we are supposed to know
     * some their internals anyway. */
    /* graph */
    if ((gmodel->has_x_unit = graph->has_x_unit))
        gwy_si_unit_set_unit_string(GWY_SI_UNIT(gmodel->x_unit),
                                    graph->x_unit);
    else
        gwy_object_unref(graph->x_unit);

    if ((gmodel->has_y_unit = graph->has_y_unit))
        gwy_si_unit_set_unit_string(GWY_SI_UNIT(gmodel->y_unit),
                                    graph->y_unit);
    else
        gwy_object_unref(graph->y_unit);

    gmodel->x_reqmin = graph->x_reqmin;
    gmodel->y_reqmin = graph->y_reqmin;
    gmodel->x_reqmax = graph->x_reqmax;
    gmodel->y_reqmax = graph->y_reqmax;

    /* axes */
    g_string_assign(gmodel->top_label,
                    gwy_axis_get_label(graph->axis_top)->str);
    g_string_assign(gmodel->bottom_label,
                    gwy_axis_get_label(graph->axis_bottom)->str);
    g_string_assign(gmodel->left_label,
                    gwy_axis_get_label(graph->axis_left)->str);
    g_string_assign(gmodel->right_label,
                    gwy_axis_get_label(graph->axis_right)->str);

    /* curves */
    /* somewhat hairy; trying to avoid redundant reallocations:
     * 1. clear extra curves that model has and graph has not
     * 2. realloc curves to the right size
     * 3. replace already existing curves  <-- if lucky, only this happens
     * 4. fill new curves
     */
    nacurves = graph->area->curves->len;
    /* 1. clear */
    for (i = nacurves; i < gmodel->ncurves; i++) {
        GwyGraphModelCurve *curve = gmodel->curves + i;

        g_free(curve->xdata);
        g_free(curve->ydata);
        g_string_free(curve->params->description, TRUE);
        g_free(curve->params);
    }
    /* 2. realloc */
    gmodel->curves = g_renew(GwyGraphModelCurve,
                               gmodel->curves, nacurves);
    /* 3. replace */
    for (i = 0; i < gmodel->ncurves; i++) {
        GwyGraphModelCurve *curve = gmodel->curves + i;
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
    for (i = gmodel->ncurves; i < nacurves; i++) {
        GwyGraphModelCurve *curve = gmodel->curves + i;
        GwyGraphAreaCurve *acurve = g_ptr_array_index(graph->area->curves, i);

        curve->n = acurve->data.N;
        curve->xdata = g_memdup(acurve->data.xvals, curve->n*sizeof(gdouble));
        curve->ydata = g_memdup(acurve->data.yvals, curve->n*sizeof(gdouble));

        curve->params = g_memdup(&acurve->params,
                                 sizeof(GwyGraphAreaCurveParams));
        curve->params->description
            = g_string_new(acurve->params.description->str);
    }

    gmodel->ncurves = nacurves;
}

GtkWidget*
gwy_graph_new_from_model(GwyGraphModel *gmodel)
{
    GtkWidget *graph_widget;
    gchar *BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS;
    GwyGraph *graph;
    gint i;

    g_return_val_if_fail(gmodel->graph == NULL, gwy_graph_new());

    graph_widget = gwy_graph_new();
    graph = GWY_GRAPH(graph_widget);

    gwy_debug("ncurves = %d", gmodel->ncurves);
    for (i = 0; i < gmodel->ncurves; i++) {
        GwyGraphModelCurve *curve = gmodel->curves + i;

        gwy_graph_add_datavalues(graph, curve->xdata, curve->ydata,
                                 curve->n, curve->params->description,
                                 curve->params);
    }

    gwy_axis_set_label(graph->axis_top, gmodel->top_label);
    gwy_axis_set_label(graph->axis_bottom, gmodel->bottom_label);
    gwy_axis_set_label(graph->axis_left, gmodel->left_label);
    gwy_axis_set_label(graph->axis_right, gmodel->right_label);
    if (gmodel->has_x_unit) {
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gmodel->x_unit));
        gwy_axis_set_unit(graph->axis_top,
                          g_strdup(BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS));
        gwy_axis_set_unit(graph->axis_bottom,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
    }
    if (gmodel->has_y_unit) {
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gmodel->y_unit));
        gwy_axis_set_unit(graph->axis_left,
                          g_strdup(BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS));
        gwy_axis_set_unit(graph->axis_right,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
    }

    gwy_graph_set_boundaries(graph,
                             gmodel->x_reqmin, gmodel->x_reqmax,
                             gmodel->y_reqmin, gmodel->y_reqmax);

    return graph_widget;
}

static GByteArray*
gwy_graph_model_serialize(GObject *obj,
                            GByteArray*buffer)
{
    GwyGraphModel *gmodel;
    gsize before_obj;
    gint i;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(obj), NULL);

    buffer = gwy_serialize_pack(buffer, "si", GWY_GRAPH_MODEL_TYPE_NAME, 0);
    before_obj = buffer->len;

    gmodel = GWY_GRAPH_MODEL(obj);
    gwy_graph_model_save_graph(gmodel, gmodel->graph);
    /* Global data, serialized as a fake subobject GwyGraphModel-graph */
    {
        gchar *x_unit, *y_unit;
        GwySerializeSpec spec[] = {
            { 'b', "has_x_unit", &gmodel->has_x_unit, NULL },
            { 'b', "has_y_unit", &gmodel->has_y_unit, NULL },
            { 's', "x_unit", &x_unit, NULL },
            { 's', "y_unit", &y_unit, NULL },
            { 's', "top_label", &gmodel->top_label->str, NULL },
            { 's', "bottom_label", &gmodel->bottom_label->str, NULL },
            { 's', "left_label", &gmodel->left_label->str, NULL },
            { 's', "right_label", &gmodel->right_label->str, NULL },
            { 'd', "x_reqmin", &gmodel->x_reqmin, NULL },
            { 'd', "y_reqmin", &gmodel->y_reqmin, NULL },
            { 'd', "x_reqmax", &gmodel->x_reqmax, NULL },
            { 'd', "y_reqmax", &gmodel->y_reqmax, NULL },
            { 'i', "ncurves", &gmodel->ncurves, NULL },
        };

        x_unit = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gmodel->x_unit));
        y_unit = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gmodel->y_unit));
        gwy_serialize_pack_object_struct(buffer,
                                         "GwyGraphModel-graph",
                                         G_N_ELEMENTS(spec), spec);
        /* XXX: why the fucking gwy_si_unit_get_unit_string() can't return
         * a const? */
        g_free(x_unit);
        g_free(y_unit);
    }
    /* Per-curve data, serialized as fake subobjects GwyGraphModel-curve */
    for (i = 0; i < gmodel->ncurves; i++) {
        GwyGraphModelCurve *curve = gmodel->curves + i;
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
                                         "GwyGraphModel-curve",
                                         G_N_ELEMENTS(spec), spec);
    }

    gwy_serialize_store_int32(buffer, before_obj - sizeof(guint32),
                              buffer->len - before_obj);
    return buffer;
}

static GObject*
gwy_graph_model_deserialize(const guchar *buffer,
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
                                            GWY_GRAPH_MODEL_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec))
        return NULL;

    return (GObject*)gwy_graph_model_new(theta, phi);
    */
    return NULL;
}

static GObject*
gwy_graph_model_duplicate(GObject *object)
{
    GwyGraphModel *gmodel;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(object), NULL);

    gmodel = GWY_GRAPH_MODEL(object);
    if (gmodel->graph)
        return gwy_graph_model_new(gmodel->graph);
    else {
        GwyGraphModel *duplicate;
        gint i;

        duplicate = (GwyGraphModel*)gwy_graph_model_new(NULL);
        /* widget stuff is already initialized to NULL */
        duplicate->has_x_unit = gmodel->has_x_unit;
        duplicate->has_y_unit = gmodel->has_y_unit;
        duplicate->x_reqmin = gmodel->x_reqmin;
        duplicate->y_reqmin = gmodel->y_reqmin;
        duplicate->x_reqmax = gmodel->x_reqmax;
        duplicate->y_reqmax = gmodel->y_reqmax;
        duplicate->x_unit = gwy_serializable_duplicate(gmodel->x_unit);
        duplicate->y_unit = gwy_serializable_duplicate(gmodel->y_unit);
        duplicate->top_label = g_string_new(gmodel->top_label->str);
        duplicate->bottom_label = g_string_new(gmodel->bottom_label->str);
        duplicate->left_label = g_string_new(gmodel->left_label->str);
        duplicate->right_label = g_string_new(gmodel->right_label->str);
        duplicate->ncurves = gmodel->ncurves;
        duplicate->curves
            = g_memdup(gmodel->curves,
                       gmodel->ncurves*sizeof(GwyGraphModelCurve));
        for (i = 0; i < duplicate->ncurves; i++) {
            GwyGraphModelCurve *curve = duplicate->curves + i;

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
