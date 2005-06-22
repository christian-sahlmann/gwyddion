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
#include "gwygraphcurvemodel.h"
#include "gwygraphmodel.h"
#include "gwygrapher.h"

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
/*static void   gwy_graph_model_save_graph        (GwyGraphModel *gmodel,
                                                 GwyGraph *graph);
*/
static void     gwy_graph_model_set_property  (GObject *object,
                                                guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec);
static void     gwy_graph_model_get_property  (GObject*object,
                                               guint prop_id,
                                               GValue *value,
                                               GParamSpec *pspec);


static GObjectClass *parent_class = NULL;


enum {
      PROP_0,
      PROP_N,
      PROP_TITLE,
      PROP_LAST
};

enum {
      LAYOUT_UPDATED,
      LAST_SIGNAL
};

static guint graph_model_signals[LAST_SIGNAL] = { 0 };


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
    gobject_class->set_property = gwy_graph_model_set_property;
    gobject_class->get_property = gwy_graph_model_get_property;
                                                                                                                                                                    

    g_object_class_install_property(gobject_class,
                                    PROP_N,
                                    g_param_spec_int("n",
                                                      "Number of curves",
                                                      "Changed number of curves in graph",
                                                      0,
                                                      100,
                                                      0,
                                                      G_PARAM_READABLE));
    g_object_class_install_property(gobject_class,
                                    PROP_TITLE,
                                    g_param_spec_string("title",
                                                      "Graph Title",
                                                      "Changed title of graph",
                                                      "new graph",
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

    graph_model_signals[LAYOUT_UPDATED]
                = g_signal_new("layout-updated",
                               G_OBJECT_CLASS_TYPE(gobject_class),
                               G_SIGNAL_RUN_FIRST,
                               G_STRUCT_OFFSET(GwyGraphModelClass, layout_updated),
                                NULL, NULL,
                               g_cclosure_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);
   
}

static void
gwy_graph_model_init(GwyGraphModel *gmodel)
{
    gwy_debug("");
    gwy_debug_objects_creation((GObject*)gmodel);

    gmodel->graph = NULL;
    gmodel->graph_destroy_hid = 0;

    gmodel->ncurves = 0;
    gmodel->curves = NULL;

    gmodel->x_min = 0.0;
    gmodel->x_max = 0.0;
    gmodel->y_min = 0.0;
    gmodel->y_max = 0.0;

    gmodel->has_x_unit = FALSE;
    gmodel->has_y_unit = FALSE;
    gmodel->x_unit = gwy_si_unit_new("");
    gmodel->y_unit = gwy_si_unit_new("");

    /* XXX: GwyGraph has no such thing */
    gmodel->title = g_string_new("FIXME: Mysterious Graph");
    gmodel->top_label = g_string_new("");
    gmodel->bottom_label = g_string_new("");
    gmodel->left_label = g_string_new("");
    gmodel->right_label = g_string_new("");

    gmodel->label_position = GWY_GRAPH_LABEL_NORTHEAST;
    gmodel->label_has_frame = 1;
    gmodel->label_frame_thickness = 1;
    gmodel->label_reverse = 0; /*designed to be added*/
    gmodel->label_visible = TRUE;
    
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
gwy_graph_model_new(GtkWidget *graph)
{
    GwyGraphModel *gmodel;
    GtkWidget *window;

    gwy_debug("");
    gmodel = g_object_new(GWY_TYPE_GRAPH_MODEL, NULL);

    gmodel->graph = graph;
    if (graph) {
        g_assert(GWY_IS_GRAPH(graph));
        window = gtk_widget_get_ancestor(GTK_WIDGET(graph), GTK_TYPE_WINDOW);
        if (window)
            g_string_assign(gmodel->title,
                            gtk_window_get_title(GTK_WINDOW(window)));
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

    g_string_free(gmodel->title, TRUE);
    g_string_free(gmodel->top_label, TRUE);
    g_string_free(gmodel->bottom_label, TRUE);
    g_string_free(gmodel->left_label, TRUE);
    g_string_free(gmodel->right_label, TRUE);

    for (i = 0; i < gmodel->ncurves; i++)
        g_object_unref(gmodel->curves[i]);
    g_free(gmodel->curves);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_graph_model_graph_destroyed(GwyGraph *graph,
                                  GwyGraphModel *gmodel)
{
    gwy_debug("");
    /*gwy_graph_model_save_graph(gmodel, graph);*/
    g_signal_handler_disconnect(gmodel->graph, gmodel->graph_destroy_hid);
    gmodel->graph_destroy_hid = 0;
    gmodel->graph = NULL;
}

/* actually copy save from a -- usually just dying -- graph */
/*
static void
gwy_graph_model_save_graph(GwyGraphModel *gmodel,
                           GwyGraph *graph)
{
    gint i, nacurves;
    GwyGraphCurveModel *gcmodel;

    gwy_debug("");
    g_assert(graph && graph == gmodel->graph);
/*
    /* FIXME: we access object fields directly now as we are supposed to know
     * some their internals anyway. */
    /* graph */
/*    if ((gmodel->has_x_unit = graph->has_x_unit))
        gwy_si_unit_set_unit_string(GWY_SI_UNIT(gmodel->x_unit),
                                    graph->x_unit);
    else
        gwy_object_unref(graph->x_unit);

    if ((gmodel->has_y_unit = graph->has_y_unit))
        gwy_si_unit_set_unit_string(GWY_SI_UNIT(gmodel->y_unit),
                                    graph->y_unit);
    else
        gwy_object_unref(graph->y_unit);

    gmodel->x_min = graph->x_reqmin;
    gmodel->y_min = graph->y_reqmin;
    gmodel->x_max = graph->x_reqmax;
    gmodel->y_max = graph->y_reqmax;
/*
    /* axes */
/*    g_string_assign(gmodel->top_label,
                    gwy_axis_get_label(graph->axis_top)->str);
    g_string_assign(gmodel->bottom_label,
                    gwy_axis_get_label(graph->axis_bottom)->str);
    g_string_assign(gmodel->left_label,
                    gwy_axis_get_label(graph->axis_left)->str);
    g_string_assign(gmodel->right_label,
                    gwy_axis_get_label(graph->axis_right)->str);

    /* label */
/*    gmodel->label_position = graph->area->lab->par.position;
    gmodel->label_has_frame = graph->area->lab->par.is_frame;
    gmodel->label_frame_thickness = graph->area->lab->par.frame_thickness;

    /* curves */
    /* somewhat hairy; trying to avoid redundant reallocations:
     * 1. clear extra curves that model has and graph has not
     * 2. realloc curves to the right size
     * 3. replace already existing curves  <-- if lucky, only this happens
     * 4. fill new curves
     */
/*    nacurves = graph->area->curves->len;
    /* 1. clear */
/*    for (i = nacurves; i < gmodel->ncurves; i++)
        gwy_object_unref(gmodel->curves[i]);
    /* 2. realloc */
/*    gmodel->curves = g_renew(GObject*, gmodel->curves, nacurves);
    /* 3. replace */
/*    for (i = 0; i < gmodel->ncurves; i++) {
        gcmodel = GWY_GRAPH_CURVE_MODEL(gmodel->curves[i]);
        gwy_graph_curve_model_save_curve(gcmodel, graph, i);
    }
    /* 4. fill */
/*    for (i = gmodel->ncurves; i < nacurves; i++) {
        gmodel->curves[i] = gwy_graph_curve_model_new();
        gcmodel = GWY_GRAPH_CURVE_MODEL(gmodel->curves[i]);
        gwy_graph_curve_model_save_curve(gcmodel, graph, i);
    }
    gmodel->ncurves = nacurves;
}

/*
GtkWidget*
gwy_graph_new_from_model(GwyGraphModel *gmodel)
{
    GtkWidget *graph_widget;
    GwyGraphCurveModel *gcmodel;
    gchar *BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS;
    GwyGraph *graph;
    gint i;

    g_return_val_if_fail(gmodel->graph == NULL, gwy_graph_new());

    graph_widget = gwy_graph_new();
    graph = GWY_GRAPH(graph_widget);

    gmodel->graph = graph;
    gmodel->graph_destroy_hid
        = g_signal_connect(graph, "destroy",
                           G_CALLBACK(gwy_graph_model_graph_destroyed), gmodel);

    graph->area->lab->par.position = gmodel->label_position;
    graph->area->lab->par.is_frame = gmodel->label_has_frame;
    graph->area->lab->par.frame_thickness = gmodel->label_frame_thickness;

    for (i = 0; i < gmodel->ncurves; i++) {
        gcmodel = GWY_GRAPH_CURVE_MODEL(gmodel->curves[i]);
        gwy_graph_add_curve_from_model(graph, gcmodel);
    }

    gwy_axis_set_label(graph->axis_top, gmodel->top_label);
    gwy_axis_set_label(graph->axis_bottom, gmodel->bottom_label);
    gwy_axis_set_label(graph->axis_left, gmodel->left_label);
    gwy_axis_set_label(graph->axis_right, gmodel->right_label);
    if (gmodel->has_x_unit) {
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gmodel->x_unit));
        gwy_axis_set_unit(graph->axis_top,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = g_strdup(BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        gwy_axis_set_unit(graph->axis_bottom,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
    }
    if (gmodel->has_y_unit) {
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gmodel->y_unit));
        gwy_axis_set_unit(graph->axis_left,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = g_strdup(BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        gwy_axis_set_unit(graph->axis_right,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
    }

    gwy_graph_set_boundaries(graph,
                             gmodel->x_min, gmodel->x_max,
                             gmodel->y_min, gmodel->y_max);

    return graph_widget;
}
*/

static GByteArray*
gwy_graph_model_serialize(GObject *obj,
                          GByteArray*buffer)
{
    GwyGraphModel *gmodel;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(obj), NULL);

    gmodel = GWY_GRAPH_MODEL(obj);
    /*if (gmodel->graph)
        gwy_graph_model_save_graph(gmodel, gmodel->graph);*/
    {
        GwySerializeSpec spec[] = {
            { 'b', "has_x_unit", &gmodel->has_x_unit, NULL },
            { 'b', "has_y_unit", &gmodel->has_y_unit, NULL },
            { 'o', "x_unit", &gmodel->x_unit, NULL },
            { 'o', "y_unit", &gmodel->y_unit, NULL },
            { 's', "title", &gmodel->title->str, NULL },
            { 's', "top_label", &gmodel->top_label->str, NULL },
            { 's', "bottom_label", &gmodel->bottom_label->str, NULL },
            { 's', "left_label", &gmodel->left_label->str, NULL },
            { 's', "right_label", &gmodel->right_label->str, NULL },
            { 'd', "x_reqmin", &gmodel->x_min, NULL },
            { 'd', "y_reqmin", &gmodel->y_min, NULL },
            { 'd', "x_reqmax", &gmodel->x_max, NULL },
            { 'd', "y_reqmax", &gmodel->y_max, NULL },
            { 'i', "label.position", &gmodel->label_position, NULL },
            { 'b', "label.has_frame", &gmodel->label_has_frame, NULL },
            { 'i', "label.frame_thickness", &gmodel->label_frame_thickness,
                NULL },
            { 'O', "curves", &gmodel->curves, &gmodel->ncurves },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_GRAPH_MODEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_graph_model_deserialize(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    GwyGraphModel *gmodel;

    g_return_val_if_fail(buffer, NULL);

    gmodel = (GwyGraphModel*)gwy_graph_model_new(NULL);
    {
        gchar *top_label, *bottom_label, *left_label, *right_label, *title;
        GwySerializeSpec spec[] = {
            { 'b', "has_x_unit", &gmodel->has_x_unit, NULL },
            { 'b', "has_y_unit", &gmodel->has_y_unit, NULL },
            { 'o', "x_unit", &gmodel->x_unit, NULL },
            { 'o', "y_unit", &gmodel->y_unit, NULL },
            { 's', "title", &title, NULL },
            { 's', "top_label", &top_label, NULL },
            { 's', "bottom_label", &bottom_label, NULL },
            { 's', "left_label", &left_label, NULL },
            { 's', "right_label", &right_label, NULL },
            { 'd', "x_reqmin", &gmodel->x_min, NULL },
            { 'd', "y_reqmin", &gmodel->y_min, NULL },
            { 'd', "x_reqmax", &gmodel->x_max, NULL },
            { 'd', "y_reqmax", &gmodel->y_max, NULL },
            { 'i', "label.position", &gmodel->label_position, NULL },
            { 'b', "label.has_frame", &gmodel->label_has_frame, NULL },
            { 'i', "label.frame_thickness", &gmodel->label_frame_thickness,
                NULL },
            { 'O', "curves", &gmodel->curves, &gmodel->ncurves },
        };

        top_label = bottom_label = left_label = right_label = title = NULL;
        if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                                GWY_GRAPH_MODEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec)) {
            g_free(top_label);
            g_free(bottom_label);
            g_free(left_label);
            g_free(right_label);
            g_free(title);
            g_free(gmodel->curves);
            g_object_unref(gmodel);
            return NULL;
        }

        if (title) {
            g_string_assign(gmodel->title, title);
            g_free(title);
        }
        if (top_label) {
            g_string_assign(gmodel->top_label, top_label);
            g_free(top_label);
        }
        if (bottom_label) {
            g_string_assign(gmodel->bottom_label, bottom_label);
            g_free(bottom_label);
        }
        if (left_label) {
            g_string_assign(gmodel->left_label, left_label);
            g_free(left_label);
        }
        if (right_label) {
            g_string_assign(gmodel->right_label, right_label);
            g_free(right_label);
        }
    }

    return (GObject*)gmodel;
}

static GObject*
gwy_graph_model_duplicate(GObject *object)
{
    GwyGraphModel *gmodel, *duplicate;
    gint i;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(object), NULL);

    gmodel = GWY_GRAPH_MODEL(object);
    if (gmodel->graph)
        return gwy_graph_model_new(gmodel->graph);

    duplicate = (GwyGraphModel*)gwy_graph_model_new_alike(gmodel);
    
    duplicate->ncurves = gmodel->ncurves;
    duplicate->curves = g_new(GObject*, gmodel->ncurves);
    for (i = 0; i < gmodel->ncurves; i++)
        duplicate->curves[i] = gwy_serializable_duplicate(gmodel->curves[i]);

    return (GObject*)duplicate;
}


static void     
gwy_graph_model_set_property  (GObject *object,
                                               guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec)
{
}

static void     
gwy_graph_model_get_property  (GObject*object,
                                               guint prop_id,
                                               GValue *value,
                                               GParamSpec *pspec)
{
}


GObject*   
gwy_graph_model_new_alike(GwyGraphModel *gmodel)
{
    GwyGraphModel *duplicate;

    gwy_debug("");

    duplicate = (GwyGraphModel*)gwy_graph_model_new(NULL);
    /* widget stuff is already initialized to NULL */
    duplicate->title = g_string_new(gmodel->title->str);;
    duplicate->has_x_unit = gmodel->has_x_unit;
    duplicate->has_y_unit = gmodel->has_y_unit;
    duplicate->x_min = gmodel->x_min;
    duplicate->y_min = gmodel->y_min;
    duplicate->x_max = gmodel->x_max;
    duplicate->y_max = gmodel->y_max;
    duplicate->label_position = gmodel->label_position;
    duplicate->label_has_frame = gmodel->label_has_frame;
    duplicate->label_frame_thickness = gmodel->label_frame_thickness;
    duplicate->label_visible = gmodel->label_visible;
    duplicate->x_unit = gwy_si_unit_duplicate(gmodel->x_unit);
    duplicate->y_unit = gwy_si_unit_duplicate(gmodel->y_unit);
    duplicate->top_label = g_string_new(gmodel->top_label->str);
    duplicate->bottom_label = g_string_new(gmodel->bottom_label->str);
    duplicate->left_label = g_string_new(gmodel->left_label->str);
    duplicate->right_label = g_string_new(gmodel->right_label->str);

    return (GObject*)duplicate;
}

#include <stdio.h>
void test_value_changed(GwyGraphModel *gmodel)
{

    printf("graphmodel: value changed refresh call got from curve\n");
}
/**
* gwy_graph_model_add_curve:
* @gmodel: A #GwyGraphModel.
* @curve: A #GwyGraphCurveModel representing curve.
*
* Adds a new curve to the model. All the curve parameters should be specified
* within the @curve structure. The curve is duplicated to put data inside
* model, therefore it should be freed by user when not necessary.
**/

void       
gwy_graph_model_add_curve(GwyGraphModel *gmodel, GwyGraphCurveModel *curve)
{
    GObject **newcurves;
    gint i;
    
    newcurves = g_new(GObject*, gmodel->ncurves+1);
    
    gmodel->curves = g_renew(GwyGraphCurveModel*,
                             gmodel->curves, gmodel->ncurves+1);
    gmodel->curves[gmodel->ncurves] = curve;
    g_object_ref(curve);
    gmodel->ncurves++;
            
    g_signal_connect_swapped(curve, "value_changed",
                      G_CALLBACK(gwy_watchable_value_changed), gmodel);
    
    g_signal_connect_swapped(curve, "value_changed",
                      G_CALLBACK(test_value_changed), gmodel);
    
    g_object_notify(G_OBJECT(gmodel), "n");
}


/**
* gwy_graph_model_get_n_curves:
* @model: A #GwyGraphModel.
*
* Returns: number of curves in graph model.
**/
gint
gwy_graph_model_get_n_curves(GwyGraphModel *gmodel)
{
    
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), 0);

    /*if (gmodel->graph)
        return gwy_graph_get_number_of_curves(gmodel->graph);
    else*/
        return gmodel->ncurves;
        
}

/**
* gwy_graph_model_remove_all_curves:
* @model: A #GwyGraphModel.
*
* Removes all the curves from graph model
**/
void
gwy_graph_model_remove_all_curves(GwyGraphModel *gmodel)
{
    gint i;
    
    for (i = 0; i < gmodel->ncurves; i++)
    {
        g_object_unref(gmodel->curves[i]);
    }
   /*g_free(gmodel->curves);    */

    
    gmodel->ncurves = 0;
    
    g_object_notify(G_OBJECT(gmodel), "n");
    
}

/**
* gwy_graph_model_remove_curve_by_desciption:
* @model: A #GwyGraphModel.
* @description: curve description (label)
*
* Removes all the curves having same description string as @description.
**/
void
gwy_graph_model_remove_curve_by_description(GwyGraphModel *gmodel,
                                             gchar *description)
{
    GObject **newcurves;
    GwyGraphCurveModel *cmodel;
    gint i, count = 0;
    
    newcurves = g_new(GObject*, gmodel->ncurves+1);
    
    for (i = 0; i < gmodel->ncurves; i++)
    {
        cmodel = GWY_GRAPH_CURVE_MODEL(gmodel->curves[i]);
        if (strcmp(description, cmodel->description->str)==0) continue;
       
        newcurves[i] = gwy_serializable_duplicate(gmodel->curves[i]);
        g_object_unref(gmodel->curves[i]);
        count++;
    }
    
    gmodel->curves = newcurves;
    gmodel->ncurves = count;
   
    g_object_notify(G_OBJECT(gmodel), "n");
}

/**
* gwy_graph_model_remove_curve_by_index:
* @model: A #GwyGraphModel.
* @index_: curve index (within GwyGraphModel structure)
*
* Removes the curve having given index. 
**/
void       
gwy_graph_model_remove_curve_by_index(GwyGraphModel *gmodel, gint index)
{
    GObject **newcurves;
    GwyGraphCurveModel *cmodel;
    gint i, count = 0;
    
    newcurves = g_new(GObject*, gmodel->ncurves+1);
    
    for (i = 0; i < gmodel->ncurves; i++)
    {
        if (i == index) continue;
        cmodel = GWY_GRAPH_CURVE_MODEL(gmodel->curves[i]); 
        newcurves[i] = gwy_serializable_duplicate(gmodel->curves[i]);
        g_object_unref(gmodel->curves[i]);
        count++;
    }
    
    gmodel->curves = newcurves;
    gmodel->ncurves = count;
   
    g_object_notify(G_OBJECT(gmodel), "n");    
}

/**
* gwy_graph_model_get_curve_by_desciption:
* @model: A #GwyGraphModel.
* @description: curve description (label)
*
* Returns: first curve that has sescription (label) given by @description. Note that this
* is pointer to data in GraphModel, therefore make a copy if you want to use
* it for some other purposes and do not free it unless you know what you are doing.
**/
GwyGraphCurveModel*  
gwy_graph_model_get_curve_by_description(GwyGraphModel *gmodel, gchar *description)
{
    GwyGraphCurveModel *cmodel;
    gint i;
    
    for (i = 0; i < gmodel->ncurves; i++)
    {
        cmodel = GWY_GRAPH_CURVE_MODEL(gmodel->curves[i]);
        if (strcmp(description, cmodel->description->str)==0) return cmodel;
    }

    return NULL;
}

/**
* gwy_graph_model_get_curve_by_desciption:
* @model: A #GwyGraphModel.
* @index_: curve index (within GwyGraphModel structure)
*
* Returns: curve with given index. Note that this
* is pointer to data in GraphModel, therefore make a copy if you want to use
* it for some other purposes and do not free it unless you know what you are doing.
**/
GwyGraphCurveModel*  
gwy_graph_model_get_curve_by_index(GwyGraphModel *gmodel, gint index)
{
    if (index>=gwy_graph_model_get_n_curves(gmodel) || index<0) return NULL;
    else return GWY_GRAPH_CURVE_MODEL(gmodel->curves[index]);
}


/**
* gwy_graph_model_signal_layout_changed:
* @model: A #GwyGraphModel.
*
* Emits signal that somehing general in graph layout (label settings) was changed.
* Graph widget or other widgets connected to graph model object should react somehow.
**/
void      
gwy_graph_model_signal_layout_changed(GwyGraphModel *model)
{
    g_signal_emit(model, graph_model_signals[LAYOUT_UPDATED], 0);
}


/**
* gwy_graph_model_set_title:
* @model: A #GwyGraphModel.
* @title: A new graphmodel title.
*
* Sets new title for the graph model.
**/

void       
gwy_graph_model_set_title(GwyGraphModel *model, gchar *title)
{
    g_string_assign(model->title, title);
    g_object_notify(G_OBJECT(model), "title");
}
                                                                                                                                                             
/**
* gwy_graph_model_set_label_position:
* @model: A #GwyGraphModel.
* @position: A new graphmodel label position.
*
* Sets label (curve desriptions) postion on graph widget.
**/
void       
gwy_graph_model_set_label_position(GwyGraphModel *model, GwyGraphLabelPosition position)
{
    model->label_position = position;
    gwy_graph_model_signal_layout_changed(model);
}

/**
* gwy_graph_model_set_label_has_frame:
* @model: A #GwyGraphModel.
* @label_has_frame: label frame mode.
*
* Sets whether graph widget label should have frame around. Note that the
* label must be visible (see #gwy_graph_model_set_label_visible) to see label.
**/
void       
gwy_graph_model_set_label_has_frame(GwyGraphModel *model, gboolean label_has_frame)
{
    model->label_has_frame = label_has_frame;
    gwy_graph_model_signal_layout_changed(model);
}

/**
* gwy_graph_model_set_label_frame_thickness:
* @model: A #GwyGraphModel.
* @thickness: Label frame thickness (in pixels).
*
* Sets the label frame thickness. Note that the
* both the frame and label must be visible (see #gwy_graph_model_set_label_visible) 
* to see label and label frame.
*
**/
void       
gwy_graph_model_set_label_frame_thickness(GwyGraphModel *model, gint thickness)
{
    model->label_frame_thickness = thickness;
    gwy_graph_model_signal_layout_changed(model);
}

/**
* gwy_graph_model_set_label_reverse:
* @model: A #GwyGraphModel.
* @reverse: Label alingment mode.
*
* Sets the label alingment (curve samples and their description postion).
* By setting the @reverse = TRUE you get alingment ("text", "sample"),
* otherwise you get alingment ("sample", "text").
**/
void       
gwy_graph_model_set_label_reverse(GwyGraphModel *model, gboolean reverse)
{
    model->label_reverse = reverse;
    gwy_graph_model_signal_layout_changed(model);
}

/**
* gwy_graph_model_set_label_visible:
* @model: A #GwyGraphModel.
* @visible: Label visibility.
*
* Sets the graph widget label visibility.
**/
void       
gwy_graph_model_set_label_visible(GwyGraphModel *model, gboolean visible)
{
    model->label_visible = visible;
    gwy_graph_model_signal_layout_changed(model);
}
                                                                                                                                                             
/**
* gwy_graph_model_get_title:
* @model: A #GwyGraphModel.
*
* Returns: graph title (newly allocated string).
**/
gchar*     
gwy_graph_model_get_title(GwyGraphModel *model)
{
    return g_strdup(model->title->str);
}
                                                                                                                                                             
/**
* gwy_graph_model_get_label_position:
* @model: A #GwyGraphModel.
*
* Returns: graph widget label posititon.
**/
GwyGraphLabelPosition  
gwy_graph_model_get_label_position(GwyGraphModel *model)
{
    return model->label_position;
}

/**
* gwy_graph_model_get_label_has_frame:
* @model: A #GwyGraphModel.
*
* Returns: graph widget label frame visibility.
**/
gboolean       
gwy_graph_model_get_label_has_frame(GwyGraphModel *model)
{
    return model->label_has_frame;
}

/**
* gwy_graph_model_get_label_frame_thickness:
* @model: A #GwyGraphModel.
*
* Returns: graph widget label frame thickness.
**/
gint           
gwy_graph_model_get_label_frame_thickness(GwyGraphModel *model)
{
    return model->label_frame_thickness;
}

/**
* gwy_graph_model_get_label_reverse:
* @model: A #GwyGraphModel.
*
* Returns: graph widget label alingment mode.
**/
gboolean       
gwy_graph_model_get_label_reverse(GwyGraphModel *model)
{
    return model->label_reverse;
}

/**
* gwy_graph_model_get_label_visible:
* @model: A #GwyGraphModel.
*
* Returns: graph widget label visibility.
**/
gboolean       
gwy_graph_model_get_label_visible(GwyGraphModel *model)
{
    return model->label_visible;
}


void       
gwy_graph_model_set_x_siunit(GwyGraphModel *model, GwySIUnit *siunit)
{
    if (model->x_unit) g_object_unref(model->x_unit);
    model->x_unit = GWY_SI_UNIT(gwy_serializable_duplicate(G_OBJECT(siunit)));
    gwy_graph_model_signal_layout_changed(model);
}

void       
gwy_graph_model_set_y_siunit(GwyGraphModel *model, GwySIUnit *siunit)
{
    if (model->y_unit) g_object_unref(model->y_unit);
    model->y_unit = GWY_SI_UNIT(gwy_serializable_duplicate(G_OBJECT(siunit)));
    gwy_graph_model_signal_layout_changed(model);
}

GwySIUnit*
gwy_graph_model_get_x_siunit(GwyGraphModel *model)
{
    return GWY_SI_UNIT(gwy_serializable_duplicate(G_OBJECT(model->x_unit)));
}

GwySIUnit*
gwy_graph_model_get_y_siunit(GwyGraphModel *model)
{
    return GWY_SI_UNIT(gwy_serializable_duplicate(G_OBJECT(model->y_unit)));
}

/**
* gwy_graph_model_export_ascii:
* @model: A #GwyGraphModel.
* @filename: name of file to be created 
* @export_units: export units in the column header
* @export_metadata: export all graph metadata within file header
* @export_style: style of values export to be readable by cetain program directly.
*
* Exports graph model into a file. The export options are specified by
* parameter @export_style.
**/
void
gwy_graph_model_export_ascii(GwyGraphModel *model, const gchar *filename,
                             gboolean export_units, gboolean export_metadata,
                             GwyGraphModelExportStyle export_style)
{
    FILE *fw;
    fw = fopen(filename, "w");
    
    switch(export_style) {
        case (GWY_GRAPH_MODEL_EXPORT_ASCII_PLAIN):
        fprintf(fw, "plain\n");
        break;

        case (GWY_GRAPH_MODEL_EXPORT_ASCII_GNUPLOT):
        fprintf(fw, "gnuplot\n");
        break;
        
        case (GWY_GRAPH_MODEL_EXPORT_ASCII_ORIGIN):
        fprintf(fw, "origin\n");
        break;
         
        case (GWY_GRAPH_MODEL_EXPORT_ASCII_CSV):
        fprintf(fw, "origin\n");
        break;

        default:
        break;
    }

    fclose(fw);
}




/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
