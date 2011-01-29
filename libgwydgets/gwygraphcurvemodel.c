/*
 *  @(#) $Id$
 *  Copyright (C) 2005-2006 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymath.h>
#include <libdraw/gwyrgba.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/dataline.h>
#include <libgwydgets/gwygraphcurvemodel.h>
#include <libgwydgets/gwydgettypes.h>

#define GWY_GRAPH_CURVE_MODEL_TYPE_NAME "GwyGraphCurveModel"

/* Cache operations */
#define CVAL(cmodel, b)  ((cmodel)->cache[GWY_GRAPH_CURVE_MODEL_CACHE_##b])
#define CBIT(b)          (1 << GWY_GRAPH_CURVE_MODEL_CACHE_##b)
#define CTEST(cmodel, b) ((cmodel)->cached & CBIT(b))

/* Cache2 operations, we use the same bitfield  but a different array */
#define CVAL2(cmodel, b)  ((cmodel)->cache2[GWY_GRAPH_CURVE_MODEL_CACHE2_##b \
                           - GWY_GRAPH_CURVE_MODEL_CACHE2_OFFSET])
#define CBIT2(b)          (1 << GWY_GRAPH_CURVE_MODEL_CACHE2_##b)
#define CTEST2(cmodel, b) ((cmodel)->cached & CBIT2(b))

typedef enum {
    GWY_GRAPH_CURVE_MODEL_CACHE_XMIN = 0,
    GWY_GRAPH_CURVE_MODEL_CACHE_XMAX,
    GWY_GRAPH_CURVE_MODEL_CACHE_YMIN,
    GWY_GRAPH_CURVE_MODEL_CACHE_YMAX,
    GWY_GRAPH_CURVE_MODEL_CACHE2_OFFSET,
    /* X values (x > 0) */
    GWY_GRAPH_CURVE_MODEL_CACHE2_XMIN_XPOS
        = GWY_GRAPH_CURVE_MODEL_CACHE2_OFFSET,
    /* Y values (x > 0) */
    GWY_GRAPH_CURVE_MODEL_CACHE2_YMIN_XPOS,
    /* Absolute y values (y != 0) */
    GWY_GRAPH_CURVE_MODEL_CACHE2_YAMIN,
    /* Absolute y values (y != 0, x > 0) */
    GWY_GRAPH_CURVE_MODEL_CACHE2_YAMIN_XPOS,
    GWY_GRAPH_CURVE_MODEL_CACHE2_LAST,
} GwyGraphCurveModelCached;

static void     gwy_graph_curve_model_finalize         (GObject *object);
static void     gwy_graph_curve_model_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_graph_curve_model_serialize     (GObject *object,
                                                        GByteArray*buffer);
static gsize    gwy_graph_curve_model_get_size         (GObject *object);
static GObject* gwy_graph_curve_model_deserialize      (const guchar *buffer,
                                                        gsize size,
                                                        gsize *position);
static GObject* gwy_graph_curve_model_duplicate_real   (GObject *object);
static void     gwy_graph_curve_model_set_property     (GObject *object,
                                                        guint prop_id,
                                                        const GValue *value,
                                                        GParamSpec *pspec);
static void     gwy_graph_curve_model_get_property     (GObject*object,
                                                        guint prop_id,
                                                        GValue *value,
                                                        GParamSpec *pspec);
static void   gwy_graph_curve_model_data_changed  (GwyGraphCurveModel *gcmodel);

enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_DESCRIPTION,
    PROP_MODE,
    PROP_POINT_TYPE,
    PROP_POINT_SIZE,
    PROP_LINE_STYLE,
    PROP_LINE_WIDTH,
    PROP_COLOR,
    PROP_LAST
};

static guint graph_curve_model_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwyGraphCurveModel, gwy_graph_curve_model, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_graph_curve_model_serializable_init))

static void
gwy_graph_curve_model_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_graph_curve_model_serialize;
    iface->deserialize = gwy_graph_curve_model_deserialize;
    iface->get_size = gwy_graph_curve_model_get_size;
    iface->duplicate = gwy_graph_curve_model_duplicate_real;
}

static void
gwy_graph_curve_model_class_init(GwyGraphCurveModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    gobject_class->finalize = gwy_graph_curve_model_finalize;
    gobject_class->set_property = gwy_graph_curve_model_set_property;
    gobject_class->get_property = gwy_graph_curve_model_get_property;

    /**
     * GwyGraphCurveModel::data-changed:
     * @gwygraphcurvemodel: The #GwyGraphCurveModel which received the signal.
     *
     * The ::data-changed signal is emitted whenever curve data is set with
     * a function like gwy_graph_curve_model_set_data().
     **/
    graph_curve_model_signals[DATA_CHANGED]
        = g_signal_new("data-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyGraphCurveModelClass, data_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    g_object_class_install_property
        (gobject_class,
         PROP_DESCRIPTION,
         g_param_spec_string("description",
                             "Curve description",
                             "Curve description.  It appears on graph key.",
                             "curve",
                             G_PARAM_READABLE | G_PARAM_WRITABLE));

    g_object_class_install_property
        (gobject_class,
         PROP_MODE,
         g_param_spec_enum("mode",
                           "Mode",
                           "Curve plotting mode (line, points, ...)",
                           GWY_TYPE_GRAPH_CURVE_TYPE,
                           GWY_GRAPH_CURVE_LINE,
                           G_PARAM_READABLE | G_PARAM_WRITABLE));

     g_object_class_install_property
         (gobject_class,
          PROP_POINT_TYPE,
          g_param_spec_enum("point-type",
                            "Point type",
                            "Curve point symbol type.  Curve mode has to"
                            "include points for the symbols to be visible.",
                            GWY_TYPE_GRAPH_POINT_TYPE,
                            GWY_GRAPH_POINT_SQUARE,
                            G_PARAM_READABLE | G_PARAM_WRITABLE));

     g_object_class_install_property
         (gobject_class,
          PROP_POINT_SIZE,
          g_param_spec_int("point-size",
                           "Point size",
                           "Curve point symbol size",
                           0, 100,
                           5,
                           G_PARAM_READABLE | G_PARAM_WRITABLE));

     g_object_class_install_property
         (gobject_class,
          PROP_LINE_STYLE,
          g_param_spec_enum("line-style",
                            "Line style",
                            "Curve line style.  Curve mode has to include "
                            "lines for the line to be visible.",
                            GDK_TYPE_LINE_STYLE,
                            GDK_LINE_SOLID,
                            G_PARAM_READABLE | G_PARAM_WRITABLE));

     g_object_class_install_property
         (gobject_class,
          PROP_LINE_WIDTH,
          g_param_spec_int("line-width",
                           "Line width",
                           "Curve line width.",
                           0, 100,
                           1,
                           G_PARAM_READABLE | G_PARAM_WRITABLE));

     g_object_class_install_property
         (gobject_class,
          PROP_COLOR,
          g_param_spec_boxed("color",
                             "Color",
                             "Curve color",
                             GWY_TYPE_RGBA,
                             G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gwy_graph_curve_model_set_property(GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
    GwyGraphCurveModel *gcmodel = GWY_GRAPH_CURVE_MODEL(object);
    GwyRGBA *color;

    switch (prop_id) {
        case PROP_DESCRIPTION:
        g_string_assign(gcmodel->description, g_value_get_string(value));
        break;

        case PROP_MODE:
        gcmodel->mode = g_value_get_enum(value);
        gwy_graph_curve_model_data_changed(gcmodel);
        break;

        case PROP_POINT_TYPE:
        gcmodel->point_type = g_value_get_enum(value);
        break;

        case PROP_LINE_STYLE:
        gcmodel->line_style = g_value_get_enum(value);
        break;

        case PROP_LINE_WIDTH:
        gcmodel->line_width = g_value_get_int(value);
        break;

        case PROP_POINT_SIZE:
        gcmodel->point_size = g_value_get_int(value);
        break;

        case PROP_COLOR:
        color = g_value_get_boxed(value);
        gcmodel->color = *color;
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_curve_model_get_property(GObject*object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
    GwyGraphCurveModel *gcmodel = GWY_GRAPH_CURVE_MODEL(object);

    switch (prop_id) {
        case PROP_DESCRIPTION:
        g_value_set_string(value, gcmodel->description->str);
        break;

        case PROP_MODE:
        g_value_set_enum(value, gcmodel->mode);
        break;

        case PROP_POINT_TYPE:
        g_value_set_enum(value, gcmodel->point_type);
        break;

        case PROP_LINE_STYLE:
        g_value_set_enum(value, gcmodel->line_style);
        break;

        case PROP_LINE_WIDTH:
        g_value_set_int(value, gcmodel->line_width);
        break;

        case PROP_POINT_SIZE:
        g_value_set_int(value, gcmodel->point_size);
        break;

        case PROP_COLOR:
        g_value_set_boxed(value, &gcmodel->color);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_curve_model_init(GwyGraphCurveModel *gcmodel)
{
    gwy_debug("");
    gwy_debug_objects_creation((GObject*)gcmodel);

    gcmodel->description = g_string_new(NULL);
    gcmodel->color.r = 0;
    gcmodel->color.g = 0;
    gcmodel->color.b = 0;
    gcmodel->color.a = 1;

    gcmodel->mode = GWY_GRAPH_CURVE_LINE_POINTS;

    gcmodel->point_type = GWY_GRAPH_POINT_SQUARE;
    gcmodel->point_size = 8;

    gcmodel->line_style = GDK_LINE_SOLID;
    gcmodel->line_width = 1;

    gcmodel->calibration = NULL;
}

/**
 * gwy_graph_curve_model_new:
 *
 * Creates a new graph curve model.
 *
 * Returns: New empty graph curve model as a #GObject.
 **/
GwyGraphCurveModel*
gwy_graph_curve_model_new(void)
{
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    gcmodel = (GwyGraphCurveModel*)g_object_new(GWY_TYPE_GRAPH_CURVE_MODEL,
                                                NULL);

    return (gcmodel);
}

/**
 * gwy_graph_curve_model_new_alike:
 * @gcmodel: A graph curve model.
 *
 * Creates new graph curve model object that has the same settings as @gcmodel.
 *
 * Curve data are not duplicated.
 *
 * Returns: New graph curve model.
 **/
GwyGraphCurveModel*
gwy_graph_curve_model_new_alike(GwyGraphCurveModel *gcmodel)
{
    GwyGraphCurveModel *duplicate;

    gwy_debug("");

    duplicate = gwy_graph_curve_model_new();
    duplicate->description = g_string_new(gcmodel->description->str);
    duplicate->color = gcmodel->color;
    duplicate->mode = gcmodel->mode;
    duplicate->point_type = gcmodel->point_type;
    duplicate->point_size = gcmodel->point_size;
    duplicate->line_style = gcmodel->line_style;
    duplicate->line_width = gcmodel->line_width;

    return duplicate;
}

static void
gwy_graph_curve_model_finalize(GObject *object)
{
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    gcmodel = GWY_GRAPH_CURVE_MODEL(object);

    g_string_free(gcmodel->description, TRUE);
    g_free(gcmodel->xdata);
    g_free(gcmodel->ydata);
    g_free(gcmodel->cache2);
    G_OBJECT_CLASS(gwy_graph_curve_model_parent_class)->finalize(object);
}

static GByteArray*
gwy_graph_curve_model_serialize(GObject *object,
                                GByteArray *buffer)
{
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(object), NULL);

    gcmodel = GWY_GRAPH_CURVE_MODEL(object);
    {
        GwySerializeSpec spec[] = {
            { 'D', "xdata", &gcmodel->xdata, &gcmodel->n },
            { 'D', "ydata", &gcmodel->ydata, &gcmodel->n },
            { 's', "description", &gcmodel->description->str, NULL },
            { 'd', "color.red", &gcmodel->color.r, NULL },
            { 'd', "color.green", &gcmodel->color.g, NULL },
            { 'd', "color.blue", &gcmodel->color.b, NULL },
            /* XXX: Legacy */
            { 'i', "type", &gcmodel->mode, NULL },
            { 'i', "point_type", &gcmodel->point_type, NULL },
            { 'i', "point_size", &gcmodel->point_size, NULL },
            { 'i', "line_style", &gcmodel->line_style, NULL },
            /* XXX: Legacy */
            { 'i', "line_size", &gcmodel->line_width, NULL },
            { 'D', "xerr", &gcmodel->calibration->xerr, &gcmodel->n },
            { 'D', "yerr", &gcmodel->calibration->yerr, &gcmodel->n },
            { 'D', "zerr", &gcmodel->calibration->zerr, &gcmodel->n },
            { 'D', "xunc", &gcmodel->calibration->xunc, &gcmodel->n },
            { 'D', "yunc", &gcmodel->calibration->yunc, &gcmodel->n },
            { 'D', "zunc", &gcmodel->calibration->zunc, &gcmodel->n },
        };

        if (gcmodel->calibration) {
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_GRAPH_CURVE_MODEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
        }
        else {
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_GRAPH_CURVE_MODEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec)-6, spec);
        }
        
    }
}

static gsize
gwy_graph_curve_model_get_size(GObject *object)
{
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(object), 0);

    gcmodel = GWY_GRAPH_CURVE_MODEL(object);
    {
        GwySerializeSpec spec[] = {
            { 'D', "xdata", &gcmodel->xdata, &gcmodel->n },
            { 'D', "ydata", &gcmodel->ydata, &gcmodel->n },
            { 's', "description", &gcmodel->description->str, NULL },
            { 'd', "color.red", &gcmodel->color.r, NULL },
            { 'd', "color.green", &gcmodel->color.g, NULL },
            { 'd', "color.blue", &gcmodel->color.b, NULL },
            /* XXX: Legacy */
            { 'i', "type", &gcmodel->mode, NULL },
            { 'i', "point_type", &gcmodel->point_type, NULL },
            { 'i', "point_size", &gcmodel->point_size, NULL },
            { 'i', "line_style", &gcmodel->line_style, NULL },
            /* XXX: Legacy */
            { 'i', "line_size", &gcmodel->line_width, NULL },
            { 'D', "xerr", &gcmodel->calibration->xerr, &gcmodel->n },
            { 'D', "yerr", &gcmodel->calibration->yerr, &gcmodel->n },
            { 'D', "zerr", &gcmodel->calibration->zerr, &gcmodel->n },
            { 'D', "xunc", &gcmodel->calibration->xunc, &gcmodel->n },
            { 'D', "yunc", &gcmodel->calibration->yunc, &gcmodel->n },
            { 'D', "zunc", &gcmodel->calibration->zunc, &gcmodel->n },
        };

        if (gcmodel->calibration)
        return gwy_serialize_get_struct_size(GWY_GRAPH_CURVE_MODEL_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
        else  return gwy_serialize_get_struct_size(GWY_GRAPH_CURVE_MODEL_TYPE_NAME,
                                             G_N_ELEMENTS(spec)-6, spec);
    }
}

static GObject*
gwy_graph_curve_model_deserialize(const guchar *buffer,
                                  gsize size,
                                  gsize *position)
{
    GwyGraphCurveModel *gcmodel;
    gdouble *xerr, *yerr, *zerr, *xunc, *yunc, *zunc;
    gint nxerr, nyerr, nzerr, nxunc, nyunc, nzunc;
    xerr = yerr = zerr = xunc = yunc = zunc = NULL;

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    gcmodel = gwy_graph_curve_model_new();
    {
        gint nxdata, nydata, ncaldata;
        gchar *description = NULL;
        GwySerializeSpec spec[] = {
            { 'D', "xdata", &gcmodel->xdata, &nxdata },
            { 'D', "ydata", &gcmodel->ydata, &nydata },
            { 's', "description", &description, NULL },
            { 'd', "color.red", &gcmodel->color.r, NULL },
            { 'd', "color.green", &gcmodel->color.g, NULL },
            { 'd', "color.blue", &gcmodel->color.b, NULL },
            /* XXX: Legacy */
            { 'i', "type", &gcmodel->mode, NULL },
            /* Accept mode too */
            { 'i', "mode", &gcmodel->mode, NULL },
            { 'i', "point_type", &gcmodel->point_type, NULL },
            { 'i', "point_size", &gcmodel->point_size, NULL },
            { 'i', "line_style", &gcmodel->line_style, NULL },
            /* XXX: Legacy */
            { 'i', "line_size", &gcmodel->line_width, NULL },
            /* Accept line_width too */
            { 'i', "line_width", &gcmodel->line_width, NULL },
            { 'D', "xerr", &xerr, &nxerr }, 
            { 'D', "yerr", &yerr, &nyerr },
            { 'D', "zerr", &zerr, &nzerr },
            { 'D', "xunc", &xunc, &nxunc },
            { 'D', "yunc", &yunc, &nyunc },
            { 'D', "zunc", &zunc, &nzunc },
        };
        if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                                GWY_GRAPH_CURVE_MODEL_TYPE_NAME,
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
        if (nxerr == nxdata && nyerr == nxdata && nzerr == nxdata 
            && nxunc == nxdata && nyunc == nxdata && nzunc == nxdata)
        {
            gcmodel->calibration = (GwyCurveCalibrationData *) g_malloc(sizeof(GwyCurveCalibrationData));
            gcmodel->calibration->xerr = xerr;
            gcmodel->calibration->yerr = yerr;
            gcmodel->calibration->zerr = zerr;
            gcmodel->calibration->xunc = xunc;
            gcmodel->calibration->yunc = yunc;
            gcmodel->calibration->zunc = zunc;
        } else {
            if (xerr) g_free(xerr);
            if (yerr) g_free(yerr);
            if (zerr) g_free(zerr);
            if (xunc) g_free(xunc);
            if (yunc) g_free(yunc);
            if (zunc) g_free(zunc);
            gcmodel->calibration = NULL;
        }
    }

    return (GObject*)gcmodel;
}

static GObject*
gwy_graph_curve_model_duplicate_real(GObject *object)
{
    GwyGraphCurveModel *gcmodel, *duplicate;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(object), NULL);

    gcmodel = GWY_GRAPH_CURVE_MODEL(object);
    duplicate = gwy_graph_curve_model_new_alike(gcmodel);

    if ((duplicate->n = gcmodel->n)) {
        duplicate->xdata = g_memdup(gcmodel->xdata, gcmodel->n*sizeof(gdouble));
        duplicate->ydata = g_memdup(gcmodel->ydata, gcmodel->n*sizeof(gdouble));
    }

    return (GObject*)duplicate;
}

/**
 * gwy_graph_curve_model_set_data:
 * @gcmodel: A graph curve model.
 * @xdata: X data points (array of size @n).
 * @ydata: Y data points (array of size @n).
 * @n: Number of points, i.e. items in @xdata and @ydata.
 *
 * Sets curve model data. 
 *
 * If there are any calibration data, they are freed,
 * as they might be not actual.
 **/
void
gwy_graph_curve_model_set_data(GwyGraphCurveModel *gcmodel,
                               const gdouble *xdata,
                               const gdouble *ydata,
                               gint n)
{
    if (gcmodel->n == n) {
        memcpy(gcmodel->xdata, xdata, n*sizeof(gdouble));
        memcpy(gcmodel->ydata, ydata, n*sizeof(gdouble));
    }
    else {
        gdouble *old;

        old = gcmodel->xdata;
        gcmodel->xdata = g_memdup(xdata, n*sizeof(gdouble));
        g_free(old);

        old = gcmodel->ydata;
        gcmodel->ydata = g_memdup(ydata, n*sizeof(gdouble));
        g_free(old);

        gcmodel->n = n;
    }
    if (gcmodel->calibration)
    {
        g_free(gcmodel->calibration->xerr);
        g_free(gcmodel->calibration->yerr);
        g_free(gcmodel->calibration->zerr);
        g_free(gcmodel->calibration->xunc);
        g_free(gcmodel->calibration->yunc);
        g_free(gcmodel->calibration->zunc);
        gcmodel->calibration = NULL;
    }
    gwy_graph_curve_model_data_changed(gcmodel);
}

/**
 * gwy_graph_curve_model_get_xdata:
 * @gcmodel: A graph curve model.
 *
 * Gets pointer to x data points.
 *
 * Data are used within the graph and cannot be freed.
 *
 * Returns: X data points, owned by the curve model.
 **/
const gdouble*
gwy_graph_curve_model_get_xdata(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->xdata;
}

/**
 * gwy_graph_curve_model_get_ydata:
 * @gcmodel: A graph curve model.
 *
 * Gets pointer to y data points.
 *
 * Data are used within the graph and cannot be freed.
 *
 * Returns: Y data points, owned by the curve model.
 **/
const gdouble*
gwy_graph_curve_model_get_ydata(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->ydata;
}

/**
 * gwy_graph_curve_model_get_ndata:
 * @gcmodel: A graph curve model.
 *
 * Returns: number of data points within the curve data
 **/
/* XXX: Malformed documentation. */
gint
gwy_graph_curve_model_get_ndata(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->n;
}

/**
 * gwy_graph_curve_model_set_data_from_dataline:
 * @gcmodel: A graph curve model.
 * @dline: A #GwyDataLine
 * @from_index: index where to start
 * @to_index: where to stop
 *
 * Sets the curve data from #GwyDataLine. The range of import can be
 * modified using parameters @from_index and @to_index that are
 * interpreted directly as data indices within the #GwyDataLine.
 * In the case that @from_index == @to_index, the full #GwyDataLine is used.
 **/
/* XXX: Malformed documentation. */
void
gwy_graph_curve_model_set_data_from_dataline(GwyGraphCurveModel *gcmodel,
                                             GwyDataLine *dline,
                                             gint from_index,
                                             gint to_index)
{
    gdouble *xdata;
    gdouble *ydata;
    const gdouble *ldata;
    gint res, i;
    gdouble realmin, realmax, offset;

    if (from_index == to_index || from_index > to_index) {
        res = gwy_data_line_get_res(dline);
        realmin = 0;
        realmax = gwy_data_line_get_real(dline);
        from_index = 0;
    }
    else {
        res = to_index - from_index;
        realmin = gwy_data_line_itor(dline, from_index);
        realmax = gwy_data_line_itor(dline, to_index);
    }

    xdata = g_new(gdouble, res);
    ydata = g_new(gdouble, res);

    offset = gwy_data_line_get_offset(dline);

    ldata = gwy_data_line_get_data_const(dline);
    for (i = 0; i < res; i++) {
        xdata[i] = realmin + i*(realmax - realmin)/res + offset;
        ydata[i] = ldata[i + from_index];
    }

    gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, res);

    g_free(xdata);
    g_free(ydata);
    gwy_graph_curve_model_data_changed(gcmodel);
}


/**
 * gwy_graph_curve_model_get_x_range:
 * @gcmodel: A graph curve model.
 * @x_min: Location to store the minimum abscissa value, or %NULL.
 * @x_max: Location to store the maximum abscissa value, or %NULL.
 *
 * Gets the abscissa range of a graph curve.
 *
 * The values are cached in the curve model therefore repeated calls to this
 * function (with unchanged data) are cheap.
 *
 * If there are no data points in the curve, @x_min and @x_max are untouched
 * and the function returns %FALSE.
 *
 * See also gwy_graph_curve_model_get_ranges() for a more high-level function.
 *
 * Returns: %TRUE if there are any data points in the curve and @x_min, @x_max
 *          were set.
 **/
gboolean
gwy_graph_curve_model_get_x_range(GwyGraphCurveModel *gcmodel,
                                  gdouble *x_min,
                                  gdouble *x_max)
{
    gdouble xmin, xmax;
    gboolean must_calculate = FALSE;
    gint i;

    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(gcmodel), FALSE);

    if (!gcmodel->n)
        return FALSE;

    if (x_min) {
        if (CTEST(gcmodel, XMIN))
            *x_min = CVAL(gcmodel, XMIN);
        else
            must_calculate = TRUE;
    }

    if (x_max) {
        if (CTEST(gcmodel, XMAX))
            *x_max = CVAL(gcmodel, XMAX);
        else
            must_calculate = TRUE;
    }

    if (!must_calculate)
        return TRUE;

    xmin = xmax = gcmodel->xdata[0];
    for (i = 1; i < gcmodel->n; i++) {
        if (G_UNLIKELY(gcmodel->xdata[i] < xmin))
            xmin = gcmodel->xdata[i];
        if (G_LIKELY(gcmodel->xdata[i] > xmax))
            xmax = gcmodel->xdata[i];
    }

    CVAL(gcmodel, XMIN) = xmin;
    CVAL(gcmodel, XMAX) = xmax;
    gcmodel->cached |= CBIT(XMIN) | CBIT(XMAX);

    if (x_min)
        *x_min = xmin;
    if (x_max)
        *x_max = xmax;

    return TRUE;
}

/**
 * gwy_graph_curve_model_get_y_range:
 * @gcmodel: A graph curve model.
 * @y_min: Location to store the minimum ordinate value, or %NULL.
 * @y_max: Location to store the maximum ordinate value, or %NULL.
 *
 * Gets the ordinate range of a graph curve.
 *
 * The values are cached in the curve model therefore repeated calls to this
 * function (with unchanged data) are cheap.
 *
 * If there are no data points in the curve, @x_min and @x_max are untouched
 * and the function returns %FALSE.
 *
 * See also gwy_graph_curve_model_get_ranges() for a more high-level function.
 *
 * Returns: %TRUE if there are any data points in the curve and @x_min, @x_max
 *          were set.
 **/
gboolean
gwy_graph_curve_model_get_y_range(GwyGraphCurveModel *gcmodel,
                                  gdouble *y_min,
                                  gdouble *y_max)
{
    gdouble ymin, ymax;
    gboolean must_calculate = FALSE;
    gint i;

    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(gcmodel), FALSE);
    if (!gcmodel->n)
        return FALSE;

    if (y_min) {
        if (CTEST(gcmodel, YMIN))
            *y_min = CVAL(gcmodel, YMIN);
        else
            must_calculate = TRUE;
    }

    if (y_max) {
        if (CTEST(gcmodel, YMAX))
            *y_max = CVAL(gcmodel, YMAX);
        else
            must_calculate = TRUE;
    }

    if (!must_calculate)
        return TRUE;

    ymin = ymax = gcmodel->ydata[0];
    for (i = 1; i < gcmodel->n; i++) {
        if (gcmodel->ydata[i] < ymin)
            ymin = gcmodel->ydata[i];
        if (gcmodel->ydata[i] > ymax)
            ymax = gcmodel->ydata[i];
    }

    CVAL(gcmodel, YMIN) = ymin;
    CVAL(gcmodel, YMAX) = ymax;
    gcmodel->cached |= CBIT(YMIN) | CBIT(YMAX);

    if (y_min)
        *y_min = ymin;
    if (y_max)
        *y_max = ymax;

    return TRUE;
}

/**
 * gwy_graph_curve_model_get_min_log:
 * @gcmodel: A graph curve model.
 * @x_logscale: %TRUE if logarithmical scale is intended for the abscissa.
 * @y_logscale: %TRUE if logarithmical scale is intended for the ordinate.
 * @x_min: Location to store the minimum abscissa value to, or %NULL.
 * @x_max: Location to store the maximum abscissa value to, or %NULL.
 * @y_min: Location to store the minimum ordinate value to, or %NULL.
 * @y_max: Location to store the maximum ordinate value to, or %NULL.
 *
 * Gets the log-scale suitable range minima of a graph curve.
 *
 * Parameters @x_logscale and @y_logscale determine which axis or axes are
 * intended to use logarithmical scale.  The range of displayble values for
 * an axis generally depends on the other axis too as it acts as a filter.
 * When both @x_logscale and @y_logscale are %FALSE, the returned minima are
 * identical to those returned by gwy_graph_curve_model_get_x_range()
 * and gwy_graph_curve_model_get_y_range().
 *
 * The return values are cached in the curve model therefore repeated calls to
 * this function (with unchanged data) are cheap.
 *
 * If there are no data points that would be displayable with the intended
 * logarithmical scale setup, the output arguments are untouched and %FALSE is
 * returned.
 *
 * Returns: %TRUE if the output arguments were filled with the ranges.
 *
 * Since: 2.8
 **/
gboolean
gwy_graph_curve_model_get_ranges(GwyGraphCurveModel *gcmodel,
                                 gboolean x_logscale,
                                 gboolean y_logscale,
                                 gdouble *x_min,
                                 gdouble *x_max,
                                 gdouble *y_min,
                                 gdouble *y_max)
{
    gdouble xmin_xpos, ymin_xpos, ya0min, ya0min_xpos;
    gdouble xret = 0.0, yret = 0.0;
    gboolean ok;
    gint i;

    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(gcmodel), FALSE);

    if (!x_logscale && !y_logscale)
        return gwy_graph_curve_model_get_x_range(gcmodel, x_min, x_max)
               && gwy_graph_curve_model_get_y_range(gcmodel, y_min, y_max);

    if (!gcmodel->n)
        return FALSE;

    if (!gcmodel->cache2)
        gcmodel->cache2 = g_new0(gdouble,
                                 GWY_GRAPH_CURVE_MODEL_CACHE2_LAST
                                 - GWY_GRAPH_CURVE_MODEL_CACHE2_OFFSET);

    /* We know all cache2 values are always calculated and cleared
     * at once, so test just one of them. */
    if (CTEST2(gcmodel, XMIN_XPOS)) {
        xmin_xpos = CVAL2(gcmodel, XMIN_XPOS);
        ymin_xpos = CVAL2(gcmodel, YMIN_XPOS);
        ya0min = CVAL2(gcmodel, YAMIN);
        ya0min_xpos = CVAL2(gcmodel, YAMIN_XPOS);
    }
    else {
        xmin_xpos = ymin_xpos = ya0min = ya0min_xpos = G_MAXDOUBLE;
        for (i = 0; i < gcmodel->n; i++) {
            gdouble x = gcmodel->xdata[i];
            gdouble y = gcmodel->ydata[i];

            if (x > 0.0) {
                if (x < xmin_xpos)
                    xmin_xpos = x;
                if (y < ymin_xpos)
                    ymin_xpos = y;
            }

            y = fabs(y);
            if (y > 0.0) {
                if (x > 0.0 && y < ya0min_xpos)
                    ya0min_xpos = y;
                if (y < ya0min)
                    ya0min = y;
            }
        }

        /* Note we cache failures too. */
        CVAL2(gcmodel, XMIN_XPOS) = xmin_xpos;
        CVAL2(gcmodel, YMIN_XPOS) = ymin_xpos;
        CVAL2(gcmodel, YAMIN) = ya0min;
        CVAL2(gcmodel, YAMIN_XPOS) = ya0min_xpos;
        gcmodel->cached |= (CBIT2(XMIN_XPOS) | CBIT2(YMIN_XPOS)
                            | CBIT2(YAMIN) | CBIT2(YAMIN_XPOS));
    }

    ok = TRUE;
    if (x_logscale) {
        if (xmin_xpos != G_MAXDOUBLE) {
            xret = xmin_xpos;
            if (y_logscale) {
                if (ya0min_xpos != G_MAXDOUBLE)
                    yret = ya0min_xpos;
                else
                    ok = FALSE;
            }
            else {
                if (ymin_xpos != G_MAXDOUBLE)
                    yret = ymin_xpos;
                else
                    ok = FALSE;
            }
        }
        else
            ok = FALSE;
    }
    else {
        if ((ok = gwy_graph_curve_model_get_x_range(gcmodel, &xret, NULL))) {
            if (y_logscale) {
                if (ya0min != G_MAXDOUBLE)
                    yret = ya0min;
                else
                    ok = FALSE;
            }
            else
                ok = ok && gwy_graph_curve_model_get_y_range(gcmodel,
                                                             &yret, NULL);
        }
    }

    if (ok) {
        if (x_min)
            *x_min = xret;
        if (y_min)
            *y_min = yret;

        /* If ok is TRUE, minimum is positive so maximum is always safe. */
        if (x_max)
            gwy_graph_curve_model_get_x_range(gcmodel, NULL, x_max);

        if (y_max) {
            if (y_logscale) {
                gdouble ym, yp;

                gwy_graph_curve_model_get_y_range(gcmodel, &ym, &yp);
                *y_max = MAX(fabs(ym), fabs(yp));
            }
            else
                gwy_graph_curve_model_get_y_range(gcmodel, NULL, y_max);
        }
    }

    return ok;
}

static void
gwy_graph_curve_model_data_changed(GwyGraphCurveModel *gcmodel)
{
    gcmodel->cached = 0;
    g_signal_emit(gcmodel, graph_curve_model_signals[DATA_CHANGED], 0);
}

/**
 * gwy_graph_curve_model_get_calibration_data:
 * @gcmodel: A graph curve model.
 *
 * Get pointer to actual calibration data for curve.
 *
 * Returns: Pointer to the calibration data of present curve (NULL if none).
 *         
 **/
GwyCurveCalibrationData *
gwy_graph_curve_model_get_calibration_data(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->calibration;
}

/**
 * gwy_graph_curve_model_set_calibration_data:
 * @gcmodel: A graph curve model.
 * @calibration: Curve calibration data
 *
 * Set pointer to actual calibration data for curve. Frees old ones (if any).
 * Does not allocate new calibration data instance.
 *         
 **/
void 
gwy_graph_curve_model_set_calibration_data(GwyGraphCurveModel *gcmodel,
                                           GwyCurveCalibrationData *calibration)
{
    if (gcmodel->calibration)
    {
        g_free(gcmodel->calibration->xerr);
        g_free(gcmodel->calibration->yerr);
        g_free(gcmodel->calibration->zerr);
        g_free(gcmodel->calibration->xunc);
        g_free(gcmodel->calibration->yunc);
        g_free(gcmodel->calibration->zunc);
    }
    gcmodel->calibration = calibration;
}



/************************** Documentation ****************************/

/**
 * SECTION:gwygraphcurvemodel
 * @title: GwyGraphCurveModel
 * @short_description: Representation of one graph curve
 *
 * #GwyGraphCurveModel represents information about a graph curve necessary to
 * fully reconstruct it.
 **/

/**
 * gwy_graph_curve_model_duplicate:
 * @gcmodel: A graph curve model to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
