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
#include "gwygraphercurvemodel.h"

#define GWY_GRAPHER_CURVE_MODEL_TYPE_NAME "GwyGrapherCurveModel"

static void   gwy_grapher_curve_model_class_init        (GwyGrapherCurveModelClass *klass);
static void   gwy_grapher_curve_model_init              (GwyGrapherCurveModel *gcmodel);
static void   gwy_grapher_curve_model_finalize          (GObject *object);
static void   gwy_grapher_curve_model_serializable_init (GwySerializableIface *iface);
static void   gwy_grapher_curve_model_watchable_init    (GwyWatchableIface *iface);
static GByteArray* gwy_grapher_curve_model_serialize    (GObject *object,
                                                       GByteArray*buffer);
static GObject* gwy_grapher_curve_model_deserialize     (const guchar *buffer,
                                                       gsize size,
                                                       gsize *position);
static GObject* gwy_grapher_curve_model_duplicate       (GObject *object);


static GObjectClass *parent_class = NULL;


GType
gwy_grapher_curve_model_get_type(void)
{
    static GType gwy_grapher_curve_model_type = 0;

    if (!gwy_grapher_curve_model_type) {
        static const GTypeInfo gwy_grapher_curve_model_info = {
            sizeof(GwyGrapherCurveModelClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_grapher_curve_model_class_init,
            NULL,
            NULL,
            sizeof(GwyGrapherCurveModel),
            0,
            (GInstanceInitFunc)gwy_grapher_curve_model_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_grapher_curve_model_serializable_init, NULL, 0
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_grapher_curve_model_watchable_init, NULL, 0
        };

        gwy_debug("");
        gwy_grapher_curve_model_type
            = g_type_register_static(G_TYPE_OBJECT,
                                     GWY_GRAPHER_CURVE_MODEL_TYPE_NAME,
                                     &gwy_grapher_curve_model_info,
                                     0);
        g_type_add_interface_static(gwy_grapher_curve_model_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_grapher_curve_model_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_grapher_curve_model_type;
}

static void
gwy_grapher_curve_model_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_grapher_curve_model_serialize;
    iface->deserialize = gwy_grapher_curve_model_deserialize;
    iface->duplicate = gwy_grapher_curve_model_duplicate;
}

static void
gwy_grapher_curve_model_watchable_init(GwyWatchableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_grapher_curve_model_class_init(GwyGrapherCurveModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_grapher_curve_model_finalize;
}

static void
gwy_grapher_curve_model_init(GwyGrapherCurveModel *gcmodel)
{
    gwy_debug("");
    gwy_debug_objects_creation((GObject*)gcmodel);

    gcmodel->n = 0;
    gcmodel->xdata = NULL;
    gcmodel->ydata = NULL;

    gcmodel->description = g_string_new("");
    gcmodel->color.r = 0.5;
    gcmodel->color.g = 0.5;
    gcmodel->color.b = 0.5;
    gcmodel->color.a = 0.5;

    gcmodel->type = GWY_GRAPHER_CURVE_LINE_POINTS;

    gcmodel->point_type = GWY_GRAPHER_POINT_SQUARE;
    gcmodel->point_size = 8;
    gcmodel->is_point = TRUE;

    gcmodel->line_style = GDK_LINE_SOLID;
    gcmodel->line_size = 1;
    gcmodel->is_line = TRUE;
}

/**
 * gwy_grapher_curve_model_new:
 *
 * Creates a new grapher curve model.
 *
 * With current generation of grapher widgets it is useless without
 * gwy_grapher_curve_model_save_curve().
 *
 * Returns: New empty grapher curve model as a #GObject.
 **/
GObject*
gwy_grapher_curve_model_new(void)
{
    GwyGrapherCurveModel *gcmodel;

    gwy_debug("");
    gcmodel = (GwyGrapherCurveModel*)g_object_new(GWY_TYPE_GRAPHER_CURVE_MODEL,
                                                NULL);

    return (GObject*)(gcmodel);
}

static void
gwy_grapher_curve_model_finalize(GObject *object)
{
    GwyGrapherCurveModel *gcmodel;

    gwy_debug("");
    gcmodel = GWY_GRAPHER_CURVE_MODEL(object);

    g_string_free(gcmodel->description, TRUE);
    g_free(gcmodel->xdata);
    g_free(gcmodel->ydata);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}


gboolean
gwy_grapher_curve_model_save_curve(GwyGrapherCurveModel *gcmodel,
                                 GwyGrapher *grapher,
                                 gint index_)
{
    /*
    GwyGrapherAreaCurve *curve;
    GwyGrapherAreaCurveParams *params;
    GString *str;
    gint n;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPHER_CURVE_MODEL(gcmodel), FALSE);
    g_return_val_if_fail(GWY_IS_GRAPHER(grapher), FALSE);

    n = gwy_grapher_get_number_of_curves(grapher);
    if (index_ < 0 || index_ >= n)
        return FALSE;

     data */
    /*
    n = gwy_grapher_get_data_size(grapher, index_);
    if (n != gcmodel->n) {
        gcmodel->n = n;
        gcmodel->xdata = g_renew(gdouble, gcmodel->xdata, n);
        gcmodel->ydata = g_renew(gdouble, gcmodel->ydata, n);
    }
    gwy_grapher_get_data(grapher, gcmodel->xdata, gcmodel->ydata, index_);

     properties */
    /*
    str = gwy_grapher_get_label(grapher, index_);
    g_string_assign(gcmodel->description, str->str);
    curve = (GwyGrapherAreaCurve*)g_ptr_array_index(grapher->area->curves, index_);
    params = &curve->params;
    gwy_rgba_from_gdk_color(&gcmodel->color, &params->color);

    gcmodel->type = GWY_GRAPHER_CURVE_HIDDEN;
    if (params->is_point)
        gcmodel->type |= GWY_GRAPHER_CURVE_POINTS;
    if (params->is_line)
        gcmodel->type |= GWY_GRAPHER_CURVE_LINE;

    gcmodel->point_type = params->point_type;
    gcmodel->point_size = params->point_size;

    gcmodel->line_style = params->line_style;
    gcmodel->line_size = params->line_size;

    return TRUE;
    */
}


static GByteArray*
gwy_grapher_curve_model_serialize(GObject *object,
                                GByteArray *buffer)
{
    GwyGrapherCurveModel *gcmodel;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPHER_CURVE_MODEL(object), NULL);

    gcmodel = GWY_GRAPHER_CURVE_MODEL(object);
    {
        GwySerializeSpec spec[] = {
            { 'D', "xdata", &gcmodel->xdata, &gcmodel->n },
            { 'D', "ydata", &gcmodel->ydata, &gcmodel->n },
            { 's', "description", &gcmodel->description->str, NULL },
            { 'd', "color.red", &gcmodel->color.r, NULL },
            { 'd', "color.green", &gcmodel->color.g, NULL },
            { 'd', "color.blue", &gcmodel->color.b, NULL },
            { 'i', "type", &gcmodel->type, NULL },
            { 'i', "point_type", &gcmodel->point_type, NULL },
            { 'i', "point_size", &gcmodel->point_size, NULL },
            { 'i', "line_style", &gcmodel->line_style, NULL },
            { 'i', "line_size", &gcmodel->line_size, NULL },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_GRAPHER_CURVE_MODEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_grapher_curve_model_deserialize(const guchar *buffer,
                                  gsize size,
                                  gsize *position)
{
    GwyGrapherCurveModel *gcmodel;

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    gcmodel = (GwyGrapherCurveModel*)gwy_grapher_curve_model_new();
    {
        gint nxdata, nydata;
        gchar *description = NULL;
        GwySerializeSpec spec[] = {
            { 'D', "xdata", &gcmodel->xdata, &nxdata },
            { 'D', "ydata", &gcmodel->ydata, &nydata },
            { 's', "description", &description, NULL },
            { 'd', "color.red", &gcmodel->color.r, NULL },
            { 'd', "color.green", &gcmodel->color.g, NULL },
            { 'd', "color.blue", &gcmodel->color.b, NULL },
            { 'i', "type", &gcmodel->type, NULL },
            { 'i', "point_type", &gcmodel->point_type, NULL },
            { 'i', "point_size", &gcmodel->point_size, NULL },
            { 'i', "line_style", &gcmodel->line_style, NULL },
            { 'i', "line_size", &gcmodel->line_size, NULL },
        };
        if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                                GWY_GRAPHER_CURVE_MODEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec)) {
            g_free(description);
            g_object_unref(gcmodel);
            return NULL;
        }
        if (nxdata != nydata) {
            g_critical("Serialized xdata and ydata array sizes differ");
            g_free(description);
            g_object_unref(gcmodel);
            return NULL;
        }
        if (description) {
            g_string_assign(gcmodel->description, description);
            g_free(description);
        }
        gcmodel->n = nxdata;
    }

    return (GObject*)gcmodel;
}

static GObject*
gwy_grapher_curve_model_duplicate(GObject *object)
{
    GwyGrapherCurveModel *gcmodel, *duplicate;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPHER_CURVE_MODEL(object), NULL);

    gcmodel = GWY_GRAPHER_CURVE_MODEL(object);
    duplicate = (GwyGrapherCurveModel*)gwy_grapher_curve_model_new();

    if ((duplicate->n = gcmodel->n)) {
        duplicate->xdata = g_memdup(gcmodel->xdata, gcmodel->n*sizeof(gdouble));
        duplicate->ydata = g_memdup(gcmodel->ydata, gcmodel->n*sizeof(gdouble));
    }

    g_string_assign(duplicate->description, gcmodel->description->str);
    duplicate->color = gcmodel->color;
    duplicate->type = gcmodel->type;

    duplicate->point_type = gcmodel->point_type;
    duplicate->point_size = gcmodel->point_size;

    duplicate->line_style = gcmodel->line_style;
    duplicate->line_size = gcmodel->line_size;

    return (GObject*)duplicate;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
