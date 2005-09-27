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
#include <libdraw/gwyrgba.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/dataline.h>
#include "gwygraphcurvemodel.h"

#define GWY_GRAPH_CURVE_MODEL_TYPE_NAME "GwyGraphCurveModel"

static void   gwy_graph_curve_model_class_init        (GwyGraphCurveModelClass *klass);
static void   gwy_graph_curve_model_init              (GwyGraphCurveModel *gcmodel);
static void   gwy_graph_curve_model_finalize          (GObject *object);
static void   gwy_graph_curve_model_serializable_init (GwySerializableIface *iface);
static GByteArray* gwy_graph_curve_model_serialize    (GObject *object,
                                                       GByteArray*buffer);
static GObject* gwy_graph_curve_model_deserialize     (const guchar *buffer,
                                                       gsize size,
                                                       gsize *position);
static GObject* gwy_graph_curve_model_duplicate_real  (GObject *object);


static GObjectClass *parent_class = NULL;

enum {
    LAYOUT_UPDATED,
    LAST_SIGNAL
};

static guint graph_curve_model_signals[LAST_SIGNAL] = { 0 };

GType
gwy_graph_curve_model_get_type(void)
{
    static GType gwy_graph_curve_model_type = 0;

    if (!gwy_graph_curve_model_type) {
        static const GTypeInfo gwy_graph_curve_model_info = {
            sizeof(GwyGraphCurveModelClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_curve_model_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphCurveModel),
            0,
            (GInstanceInitFunc)gwy_graph_curve_model_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_graph_curve_model_serializable_init, NULL, 0
        };

        gwy_debug("");
        gwy_graph_curve_model_type
            = g_type_register_static(G_TYPE_OBJECT,
                                     GWY_GRAPH_CURVE_MODEL_TYPE_NAME,
                                     &gwy_graph_curve_model_info,
                                     0);
        g_type_add_interface_static(gwy_graph_curve_model_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
    }

    return gwy_graph_curve_model_type;
}

static void
gwy_graph_curve_model_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_graph_curve_model_serialize;
    iface->deserialize = gwy_graph_curve_model_deserialize;
    iface->duplicate = gwy_graph_curve_model_duplicate_real;
}


static void
gwy_graph_curve_model_class_init(GwyGraphCurveModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_graph_curve_model_finalize;

    graph_curve_model_signals[LAYOUT_UPDATED]
                       = g_signal_new("layout-updated",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyGraphCurveModelClass, layout_updated),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_graph_curve_model_init(GwyGraphCurveModel *gcmodel)
{
    gwy_debug("");
    gwy_debug_objects_creation((GObject*)gcmodel);

    gcmodel->n = 0;
    gcmodel->xdata = NULL;
    gcmodel->ydata = NULL;

    gcmodel->description = g_string_new("");
    gcmodel->color.r = 0;
    gcmodel->color.g = 0;
    gcmodel->color.b = 0;
    gcmodel->color.a = 1;

    gcmodel->type = GWY_GRAPH_CURVE_LINE_POINTS;

    gcmodel->point_type = GWY_GRAPH_POINT_SQUARE;
    gcmodel->point_size = 8;

    gcmodel->line_style = GDK_LINE_SOLID;
    gcmodel->line_size = 1;
}

/**
 * gwy_graph_curve_model_new:
 *
 * Creates a new graph curve model.
 *
 * With current generation of graph widgets it is useless without
 * gwy_graph_curve_model_save_curve().
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

static void
gwy_graph_curve_model_finalize(GObject *object)
{
    GwyGraphCurveModel *gcmodel;
    gwy_debug("");
    gcmodel = GWY_GRAPH_CURVE_MODEL(object);

    g_string_free(gcmodel->description, TRUE);
    g_free(gcmodel->xdata);
    g_free(gcmodel->ydata);
    G_OBJECT_CLASS(parent_class)->finalize(object);
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
            { 'i', "type", &gcmodel->type, NULL },
            { 'i', "point_type", &gcmodel->point_type, NULL },
            { 'i', "point_size", &gcmodel->point_size, NULL },
            { 'i', "line_style", &gcmodel->line_style, NULL },
            { 'i', "line_size", &gcmodel->line_size, NULL },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_GRAPH_CURVE_MODEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_graph_curve_model_deserialize(const guchar *buffer,
                                  gsize size,
                                  gsize *position)
{
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    gcmodel = gwy_graph_curve_model_new();
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
    duplicate = gwy_graph_curve_model_new();

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

/**
* gwy_graph_curve_model_set_data:
* @gcmodel: A #GwyGraphCurveModel.
* @xdata: x data points (array of size @n)
* @ydata: y data points (array of size @n)
* @n: data array size (number of data points)
*
* Sets curve model data. Curve model does not make a copy of the data,
* therefore you should not free the data unless you know what
* are you doing.
**/
void       
gwy_graph_curve_model_set_data(GwyGraphCurveModel *gcmodel,
                               gdouble *xdata,
                               gdouble *ydata,
                               gint n)
{
    gcmodel->xdata = g_memdup(xdata, n*sizeof(gdouble));
    gcmodel->ydata = g_memdup(ydata, n*sizeof(gdouble));
    gcmodel->n = n;
    gwy_graph_curve_model_signal_layout_changed(gcmodel);
}

/**
* gwy_graph_curve_model_set_description:
* @gcmodel: A #GwyGraphCurveModel.
* @description: curve description text
*
* Sets curve model description. The description should appear on graph label, for example.
**/
void       
gwy_graph_curve_model_set_description(GwyGraphCurveModel *gcmodel,
                                      gchar *description)
{
    g_string_assign(gcmodel->description, description);
    gwy_graph_curve_model_signal_layout_changed(gcmodel);
}
                        
/**
* gwy_graph_curve_model_set_curve_type:
* @gcmodel: A #GwyGraphCurveModel.
* @type: curve type
*
* Sets curve type for plotting the curve. This includes setting points, linespoints, line, etc.
**/
void       
gwy_graph_curve_model_set_curve_type(GwyGraphCurveModel *gcmodel,
                                     GwyGraphCurveType type)
{
    gcmodel->type = type;
    gwy_graph_curve_model_signal_layout_changed(gcmodel);
}

/**
* gwy_graph_curve_model_set_curve_point_type:
* @gcmodel: A #GwyGraphCurveModel.
* @point_type: point type to be used for plot
*
* Sets curve point type for plotting the curve. Curve type that is chosen must include
* some kind of point plot to see any change (e. g. GWY_GRAPH_CURVE_POINTS).
**/
void       
gwy_graph_curve_model_set_curve_point_type(GwyGraphCurveModel *gcmodel,
                                           GwyGraphPointType point_type)
{
    gcmodel->point_type = point_type;
    gwy_graph_curve_model_signal_layout_changed(gcmodel);
}
                                                                                                                                                             
/**
* gwy_graph_curve_model_set_curve_point_size:
* @gcmodel: A #GwyGraphCurveModel.
* @point_size: point size to be used for plot (in pixels)
*
* Sets curve point size for plotting the curve. Curve type that is chosen must include
* some kind of point plot to see any change (e. g. GWY_GRAPH_CURVE_POINTS).
**/
void       
gwy_graph_curve_model_set_curve_point_size(GwyGraphCurveModel *gcmodel,
                                           gint point_size)
{
    gcmodel->point_size = point_size;
    gwy_graph_curve_model_signal_layout_changed(gcmodel);
}
                                                                                                                                                             
/**
* gwy_graph_curve_model_set_line_style:
* @gcmodel: A #GwyGraphCurveModel.
* @description: line style to be used for plot
*
* Sets curve line style for plotting the curve. Curve type that is chosen must include
* some kind of line plot to see any change (e. g. GWY_GRAPH_CURVE_LINE).
**/
void       
gwy_graph_curve_model_set_curve_line_style(GwyGraphCurveModel *gcmodel,
                                           GdkLineStyle line_style)
{
    gcmodel->line_style = line_style;
    gwy_graph_curve_model_signal_layout_changed(gcmodel);
}
                                                                                                                                                             
/**
* gwy_graph_curve_model_set_line_size:
* @gcmodel: A #GwyGraphCurveModel.
* @description: line size to be used for plot (in pixels)
*
* Sets curve line size description. Curve type that is chosen must include
* some kind of line plot to see any change (e. g. GWY_GRAPH_CURVE_LINE).
**/
void       
gwy_graph_curve_model_set_curve_line_size(GwyGraphCurveModel *gcmodel,
                                          gint line_size)
{
    gcmodel->line_size = line_size;
    gwy_graph_curve_model_signal_layout_changed(gcmodel);
}
                                                                                                                                                             
/**
* gwy_graph_curve_model_get_xdata:
* @gcmodel: A #GwyGraphCurveModel.
*
* Gets pointer to x data points. Data are used within the graph and cannot be freed.
*
* Returns: x data points
**/
const gdouble*   
gwy_graph_curve_model_get_xdata(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->xdata;
}
                                                                                                                                                             
/**
* gwy_graph_curve_model_get_ydata:
* @gcmodel: A #GwyGraphCurveModel.
*
* Gets pointer to y data points. Data are used within the graph and cannot be freed.
*
* Returns: y data points
**/
const gdouble*   
gwy_graph_curve_model_get_ydata(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->ydata;
}
                   
/**
* gwy_graph_curve_model_get_ndata:
* @gcmodel: A #GwyGraphCurveModel.
*
* Returns: number of data points within the curve data
**/
gint       
gwy_graph_curve_model_get_ndata(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->n;
}
                                                                                                                                                             
/**
* gwy_graph_curve_model_get_description:
* @gcmodel: A #GwyGraphCurveModel.
*
* Returns: curve data description (what appears as curve label on graph) as newly
* allocated string.
**/
gchar*     
gwy_graph_curve_model_get_description(GwyGraphCurveModel *gcmodel)
{
    return g_strdup(gcmodel->description->str);
}
                                                                                                                                                             

/**
* gwy_graph_curve_model_get_curve_type:
* @gcmodel: A #GwyGraphCurveModel.
*
* Returns: curve plot type (as points, lines, linespoints, etc.)
**/
GwyGraphCurveType  
gwy_graph_curve_model_get_curve_type(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->type;
}
                                                                                                                                                             
/**
* gwy_graph_curve_model_get_curve_point_type:
* @gcmodel: A #GwyGraphCurveModel.
*
* Returns: curve plot point type (square, circle, etc.)
**/
GwyGraphPointType  
gwy_graph_curve_model_get_curve_point_type(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->point_type;
}
                                                                                                                                                             
/**
* gwy_graph_curve_model_get_curve_point_size:
* @gcmodel: A #GwyGraphCurveModel.
*
* Returns: curve plot point size (in pixels)
**/
gint       
gwy_graph_curve_model_get_curve_point_size(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->point_size;
}
                                                                                                                                                             
/**
* gwy_graph_curve_model_get_curve_line_style:
* @gcmodel: A #GwyGraphCurveModel.
*
* Returns: curve plot line style
**/
GdkLineStyle  
gwy_graph_curve_model_get_curve_line_style(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->line_style;
}
                                                                                                                                                             
/**
* gwy_graph_curve_model_get_curve_line_size:
* @gcmodel: A #GwyGraphCurveModel.
*
* Returns: curve plot line size (in pixels)
**/
gint       
gwy_graph_curve_model_get_curve_line_size(GwyGraphCurveModel *gcmodel)
{
    return gcmodel->line_size;
}

/**
* gwy_graph_curve_model_set_data_from_dataline:
* @gcmodel: A #GwyGraphCurveModel.
* @dline: A #GwyDataLine
* @from_index: index where to start
* @to_index: where to stop
* 
* Sets the curve data from #GwyDataLine. The range of import can be
* modified using parameters @from_index and @to_index that are
* interpreted directly as data indices within the #GwyDataLine.
* In case that @from_index == @to_index full #GwyDataLine is used.
**/
void
gwy_graph_curve_model_set_data_from_dataline(GwyGraphCurveModel *gcmodel,
                                            GwyDataLine *dline,
                                            gint from_index,
                                            gint to_index)
{
    gdouble *xdata;
    gdouble *ydata;
    gint res, i;
    gdouble realmin, realmax;
             
    if (from_index == to_index || from_index > to_index)
    {
        res = gwy_data_line_get_res(dline);
        realmin = 0;
        realmax = gwy_data_line_get_real(dline);
        from_index = 0;
    }
    else
    {
        res = to_index - from_index;
        realmin = gwy_data_line_itor(dline, from_index);
        realmax = gwy_data_line_itor(dline, to_index);                
    }

    xdata = (gdouble *)g_malloc(sizeof(gdouble)*res);
    ydata = (gdouble *)g_malloc(sizeof(gdouble)*res);                                                                                                                                                             
    for (i=0; i<res; i++)
    {
        xdata[i] = realmin + (gdouble)i*(realmax - realmin)/(gdouble)res;
        ydata[i] = dline->data[i + from_index];
    }

    gwy_graph_curve_model_set_data(gcmodel,
                                   xdata,
                                   ydata,
                                   res);
    
                    
}


/**
* gwy_graph_curve_model_set_curve_color:
* @gcmodel: A #GwyGraphCurveModel.
* @color: GwyRGBA color structure (will not be directly used)
*
* Sets the curve color.
**/
void
gwy_graph_curve_model_set_curve_color(GwyGraphCurveModel *gcmodel,
                                                   GwyRGBA color)
{
    gcmodel->color.r = color.r;
    gcmodel->color.g = color.g;
    gcmodel->color.b = color.b;
    gcmodel->color.a = color.a;
    
}

/**
* gwy_graph_curve_model_get_curve_color:
* @gcmodel: A #GwyGraphCurveModel.
*
* Returns: curve color structure (directly used by curve model, not free it after use).
**/
GwyRGBA *
gwy_graph_curve_model_get_curve_color(GwyGraphCurveModel *gcmodel)
{
    return &gcmodel->color;
}

/**
  * gwy_graph_curve_model_signal_layout_changed:
  * @model: A #GwyGraphCurveModel.
  *
  * Emits signal that somehing general in curve layout (plotting style) was changed.
  * Graph widget or other widgets connected to graph model object should react somehow.
  **/
void
gwy_graph_curve_model_signal_layout_changed(GwyGraphCurveModel *model)
{
    g_signal_emit(model, graph_curve_model_signals[LAYOUT_UPDATED], 0);
}




/************************** Documentation ****************************/

/**
 * gwy_graph_curve_model_duplicate:
 * @gcmodel: A graph curve model to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
