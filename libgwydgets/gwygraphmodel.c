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
#include <stdio.h>
#include <glib/gstdio.h>

#include <libgwyddion/gwyddion.h>

#include "gwygraphcurvemodel.h"
#include "gwygraphmodel.h"
#include "gwygraph.h"

#define GWY_GRAPH_MODEL_TYPE_NAME "GwyGraphModel"

static void     gwy_graph_model_finalize         (GObject *object);
static void     gwy_graph_model_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_graph_model_serialize     (GObject *obj,
                                                  GByteArray*buffer);
static gsize    gwy_graph_model_get_size         (GObject *obj);
static GObject* gwy_graph_model_deserialize      (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
static GObject* gwy_graph_model_duplicate_real   (GObject *object);
static void     gwy_graph_model_set_property     (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_graph_model_get_property     (GObject*object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);

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


G_DEFINE_TYPE_EXTENDED
    (GwyGraphModel, gwy_graph_model, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_graph_model_serializable_init))

static void
gwy_graph_model_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_graph_model_serialize;
    iface->deserialize = gwy_graph_model_deserialize;
    iface->get_size = gwy_graph_model_get_size;
    iface->duplicate = gwy_graph_model_duplicate_real;
}


static void
gwy_graph_model_class_init(GwyGraphModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_graph_model_finalize;
    gobject_class->set_property = gwy_graph_model_set_property;
    gobject_class->get_property = gwy_graph_model_get_property;

    g_object_class_install_property
        (gobject_class,
         PROP_N,
         g_param_spec_int("n",
                          "Number of curves",
                          "Changed number of curves in graph",
                          0,
                          100,
                          0,
                          G_PARAM_READABLE));

    g_object_class_install_property
        (gobject_class,
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
    gwy_debug_objects_creation((GObject*)gmodel);

    gmodel->curves = g_ptr_array_new();

    gmodel->x_unit = gwy_si_unit_new("");
    gmodel->y_unit = gwy_si_unit_new("");

    /* XXX: GwyGraph has no such thing */
    gmodel->title = g_string_new("FIXME: Mysterious Graph");
    gmodel->top_label = g_string_new("");
    gmodel->bottom_label = g_string_new("x");
    gmodel->left_label = g_string_new("y");
    gmodel->right_label = g_string_new("");

    gmodel->label_position = GWY_GRAPH_LABEL_NORTHEAST;
    gmodel->label_has_frame = 1;
    gmodel->label_frame_thickness = 1;
    gmodel->label_reverse = 0; /*designed to be added*/
    gmodel->label_visible = TRUE;
}

/**
 * gwy_graph_model_new:
 *
 * Creates a new graph model.
 *
 * Returns: New graph model as a #GObject.
 **/
GwyGraphModel*
gwy_graph_model_new(void)
{
    GwyGraphModel *gmodel;

    gwy_debug("");
    gmodel = g_object_new(GWY_TYPE_GRAPH_MODEL, NULL);

    return gmodel;
}

static void
gwy_graph_model_finalize(GObject *object)
{
    GwyGraphModel *gmodel;
    gint i;

    gwy_debug("");

    gmodel = GWY_GRAPH_MODEL(object);

    gwy_object_unref(gmodel->x_unit);
    gwy_object_unref(gmodel->y_unit);

    g_string_free(gmodel->title, TRUE);
    g_string_free(gmodel->top_label, TRUE);
    g_string_free(gmodel->bottom_label, TRUE);
    g_string_free(gmodel->left_label, TRUE);
    g_string_free(gmodel->right_label, TRUE);

    for (i = 0; i < gmodel->curves->len; i++)
        g_object_unref(g_ptr_array_index(gmodel->curves, i));
    g_ptr_array_free(gmodel->curves, TRUE);

    G_OBJECT_CLASS(gwy_graph_model_parent_class)->finalize(object);
}

static GByteArray*
gwy_graph_model_serialize(GObject *obj,
                          GByteArray *buffer)
{
    GwyGraphModel *gmodel;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(obj), NULL);

    gmodel = GWY_GRAPH_MODEL(obj);
    {
        guint32 ncurves = gmodel->curves->len;
        GwySerializeSpec spec[] = {
            { 'b', "has_x_unit", &gmodel->has_x_unit, NULL },
            { 'b', "has_y_unit", &gmodel->has_y_unit, NULL },
            { 'b', "x_is_logarithmic", &gmodel->x_is_logarithmic, NULL },
            { 'b', "y_is_logarithmic", &gmodel->y_is_logarithmic, NULL },
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
            { 'O', "curves", &gmodel->curves->pdata, &ncurves },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_GRAPH_MODEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_graph_model_get_size(GObject *obj)
{
    GwyGraphModel *gmodel;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(obj), 0);

    gmodel = GWY_GRAPH_MODEL(obj);
    {
        guint32 ncurves = gmodel->curves->len;
        GwySerializeSpec spec[] = {
            { 'b', "has_x_unit", &gmodel->has_x_unit, NULL },
            { 'b', "has_y_unit", &gmodel->has_y_unit, NULL },
            { 'b', "x_is_logarithmic", &gmodel->x_is_logarithmic, NULL },
            { 'b', "y_is_logarithmic", &gmodel->y_is_logarithmic, NULL },
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
            { 'O', "curves", &gmodel->curves->pdata, &ncurves },
        };

        return gwy_serialize_get_struct_size(GWY_GRAPH_MODEL_TYPE_NAME,
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

    gmodel = gwy_graph_model_new();
    {
        gchar *top_label, *bottom_label, *left_label, *right_label, *title;
        GwyGraphCurveModel **curves = NULL;
        guint32 ncurves = 0;
        GwySerializeSpec spec[] = {
            { 'b', "has_x_unit", &gmodel->has_x_unit, NULL },
            { 'b', "has_y_unit", &gmodel->has_y_unit, NULL },
            { 'b', "x_is_logarithmic", &gmodel->x_is_logarithmic, NULL },
            { 'b', "y_is_logarithmic", &gmodel->y_is_logarithmic, NULL },
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
            { 'O', "curves", &curves, &ncurves },
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
            g_free(curves);
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
        if (curves) {
            guint i;

            for (i = 0; i < ncurves; i++)
                g_ptr_array_add(gmodel->curves, curves[i]);
            g_free(curves);
        }
    }

    return (GObject*)gmodel;
}

static GObject*
gwy_graph_model_duplicate_real(GObject *object)
{
    GwyGraphModel *gmodel, *duplicate;
    GwyGraphCurveModel *cmodel;
    gint i;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(object), NULL);

    gmodel = GWY_GRAPH_MODEL(object);
    duplicate = gwy_graph_model_new_alike(gmodel);

    for (i = 0; i < gmodel->curves->len; i++) {
        cmodel = g_ptr_array_index(gmodel->curves, i);
        cmodel = gwy_graph_curve_model_duplicate(cmodel);
        gwy_graph_model_add_curve(duplicate, cmodel);
        g_object_unref(cmodel);
    }

    return (GObject*)duplicate;
}


static void
gwy_graph_model_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyGraphModel *gmodel = GWY_GRAPH_MODEL(object);

    switch (prop_id) {
        case PROP_TITLE:
        gwy_graph_model_set_title(gmodel, g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_graph_model_get_property(GObject*object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GwyGraphModel *gmodel = GWY_GRAPH_MODEL(object);

    switch (prop_id) {
        case PROP_TITLE:
        g_value_set_string(value, gwy_graph_model_get_title(gmodel));
        break;

        case PROP_N:
        g_value_set_int(value, gwy_graph_model_get_n_curves(gmodel));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_graph_model_new_alike:
 * @gmodel: A #GwyGraphModel.
 *
 * Creates new graph model object that has the same settings as @gmodel.
 * This includes axis/label visibility, actual plotting range, etc.
 * Curves are not duplicated or referenced.
 *
 * Returns: New graph model.
 **/
GwyGraphModel*
gwy_graph_model_new_alike(GwyGraphModel *gmodel)
{
    GwyGraphModel *duplicate;

    gwy_debug("");

    duplicate = gwy_graph_model_new();
    /* widget stuff is already initialized to NULL */
    duplicate->title = g_string_new(gmodel->title->str);;
    duplicate->has_x_unit = gmodel->has_x_unit;
    duplicate->has_y_unit = gmodel->has_y_unit;
    duplicate->x_is_logarithmic = gmodel->x_is_logarithmic;
    duplicate->y_is_logarithmic = gmodel->y_is_logarithmic;
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

    return duplicate;
}


/**
 * gwy_graph_model_add_curve:
 * @gmodel: A #GwyGraphModel.
 * @curve: A #GwyGraphCurveModel representing the curve to add.
 *
 * Adds a new curve to the model. All the curve parameters should be specified
 * within the @curve structure.
 **/
void
gwy_graph_model_add_curve(GwyGraphModel *gmodel,
                          GwyGraphCurveModel *curve)
{
    g_object_ref(curve);
    g_ptr_array_add(gmodel->curves, curve);
    g_signal_connect_swapped(curve, "layout-updated",
                             G_CALLBACK(gwy_graph_model_signal_layout_changed),
                             gmodel);
    g_signal_connect_swapped(curve, "notify",
                             G_CALLBACK(gwy_graph_model_signal_layout_changed),
                             gmodel);

    g_object_notify(G_OBJECT(gmodel), "n");
}


/**
 * gwy_graph_model_get_n_curves:
 * @gmodel: A #GwyGraphModel.
 *
 * Returns: number of curves in graph model.
 **/
gint
gwy_graph_model_get_n_curves(GwyGraphModel *gmodel)
{
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), 0);
    return gmodel->curves->len;
}

/**
 * gwy_graph_model_remove_all_curves:
 * @gmodel: A #GwyGraphModel.
 *
 * Removes all the curves from graph model
 **/
void
gwy_graph_model_remove_all_curves(GwyGraphModel *gmodel)
{
    guint i;

    g_return_if_fail(GWY_IS_GRAPH_MODEL(gmodel));

    for (i = 0; i < gmodel->curves->len; i++) {
        GwyGraphCurveModel *cmodel;

        cmodel = g_ptr_array_index(gmodel->curves, i);
        g_signal_handlers_disconnect_by_func
                       (cmodel, gwy_graph_model_signal_layout_changed, gmodel);
        g_object_unref(cmodel);
    }
    g_ptr_array_set_size(gmodel->curves, 0);
    g_object_notify(G_OBJECT(gmodel), "n");
}

/**
 * gwy_graph_model_remove_curve_by_description:
 * @gmodel: A #GwyGraphModel.
 * @description: curve description (label)
 *
 * Removes all the curves having same description string as @description.
 *
 * Returns: The number of removed curves.
 **/
gint
gwy_graph_model_remove_curve_by_description(GwyGraphModel *gmodel,
                                            const gchar *description)
{
    GPtrArray *newcurves;
    GwyGraphCurveModel *cmodel;
    guint i;

    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), 0);

    newcurves = g_ptr_array_new();
    for (i = 0; i < gmodel->curves->len; i++) {
        cmodel = g_ptr_array_index(gmodel->curves, i);
        if (gwy_strequal(description, cmodel->description->str)) {
            g_signal_handlers_disconnect_by_func
                       (cmodel, gwy_graph_model_signal_layout_changed, gmodel);
            g_object_unref(cmodel);
        }
        else
            g_ptr_array_add(newcurves, cmodel);
    }

    /* Do nothing when no curve was actually removed */
    i = gmodel->curves->len - newcurves->len;
    if (i == 0) {
        g_ptr_array_free(newcurves, TRUE);
        return 0;
    }
    GWY_SWAP(GPtrArray*, gmodel->curves, newcurves);
    g_ptr_array_free(newcurves, TRUE);

    g_object_notify(G_OBJECT(gmodel), "n");
    return i;
}

/**
 * gwy_graph_model_remove_curve_by_index:
 * @gmodel: A #GwyGraphModel.
 * @cindex: Curve index in graph model.
 *
 * Removes the curve having given index.
 **/
void
gwy_graph_model_remove_curve_by_index(GwyGraphModel *gmodel, gint cindex)
{
    GwyGraphCurveModel *cmodel;

    g_return_if_fail(GWY_IS_GRAPH_MODEL(gmodel));
    g_return_if_fail(cindex >= 0 && cindex < gmodel->curves->len);

    cmodel = g_ptr_array_index(gmodel->curves, cindex);
    g_signal_handlers_disconnect_by_func
                       (cmodel, gwy_graph_model_signal_layout_changed, gmodel);
    g_object_unref(cmodel);

    g_ptr_array_remove_index(gmodel->curves, cindex);
    g_object_notify(G_OBJECT(gmodel), "n");
}

/**
 * gwy_graph_model_get_curve_by_description:
 * @gmodel: A #GwyGraphModel.
 * @description: curve description (label)
 *
 * Returns: First curve that has description (label) given by @description.
 **/
GwyGraphCurveModel*
gwy_graph_model_get_curve_by_description(GwyGraphModel *gmodel,
                                         const gchar *description)
{
    GwyGraphCurveModel *cmodel;
    guint i;

    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), NULL);

    for (i = 0; i < gmodel->curves->len; i++) {
        cmodel = g_ptr_array_index(gmodel->curves, i);
        if (gwy_strequal(description, cmodel->description->str))
            return cmodel;
    }

    return NULL;
}

/**
 * gwy_graph_model_get_curve_by_index:
 * @gmodel: A #GwyGraphModel.
 * @cindex: Curve index in graph model.
 *
 * Returns: Curve with given index.
 **/
GwyGraphCurveModel*
gwy_graph_model_get_curve_by_index(GwyGraphModel *gmodel, gint cindex)
{
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), NULL);
    g_return_val_if_fail(cindex >= 0 && cindex < gmodel->curves->len, NULL);

    return g_ptr_array_index(gmodel->curves, cindex);
}

/**
 * gwy_graph_model_signal_layout_changed:
 * @model: A #GwyGraphModel.
 *
 * Emits signal that something general in graph layout (label settings) was changed.
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
gwy_graph_model_set_title(GwyGraphModel *model,
                          const gchar *title)
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
gwy_graph_model_set_label_position(GwyGraphModel *model,
                                   GwyGraphLabelPosition position)
{
    model->label_position = position;
    gwy_graph_model_signal_layout_changed(model);
}

/**
 * gwy_graph_model_set_label_has_frame:
 * @model: A #GwyGraphModel.
 * @label_has_frame: label frame mode.
 *
 * Sets whether graph label widget should have a frame around it. Note that the
 * label must be visible (see #gwy_graph_model_set_label_visible())
 * to see label.
 **/
void
gwy_graph_model_set_label_has_frame(GwyGraphModel *model,
                                    gboolean label_has_frame)
{
    model->label_has_frame = label_has_frame;
    gwy_graph_model_signal_layout_changed(model);
}

/**
 * gwy_graph_model_set_label_frame_thickness:
 * @model: A #GwyGraphModel.
 * @thickness: Label frame thickness (in pixels).
 *
 * Sets the label frame thickness. Note that both the frame and label must
 * be visible (see #gwy_graph_model_set_label_visible()) to see label and label
 * frame.
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
 * Sets the label alignment (curve samples and their description postion).
 * By setting the @reverse = TRUE you get alignment ("text", "sample"),
 * otherwise you get alignment ("sample", "text").
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
 * Returns: graph widget label position.
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
 * Returns: graph widget label alignment mode.
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

/**
 * gwy_graph_model_set_si_unit_x:
 * @model: A #GwyGraphModel.
 * @siunit: physical unit for x axis
 *
 * Sets the physical unit for graph x axis. The unit is duplicated, so you are
 * responsible for freeing @siunit.
 **/
void
gwy_graph_model_set_si_unit_x(GwyGraphModel *model, GwySIUnit *siunit)
{
    gwy_object_unref(model->x_unit);
    model->x_unit = gwy_si_unit_duplicate(siunit);
    gwy_graph_model_signal_layout_changed(model);
}

/**
 * gwy_graph_model_set_si_unit_y:
 * @model: A #GwyGraphModel.
 * @siunit: physical unit for y axis
 *
 * Sets the physical unit for graph y axis. The unit is duplicated, so you are
 * responsible for freeing @siunit.
 **/
void
gwy_graph_model_set_si_unit_y(GwyGraphModel *model, GwySIUnit *siunit)
{
    gwy_object_unref(model->y_unit);
    model->y_unit = gwy_si_unit_duplicate(siunit);
    gwy_graph_model_signal_layout_changed(model);
}

/**
 * gwy_graph_model_set_units_from_data_line:
 * @model: A graph model.
 * @data_line: A data line to take units from.
 *
 * Sets x and y graph model units to match a data line.
 **/
void
gwy_graph_model_set_units_from_data_line(GwyGraphModel *model,
                                         GwyDataLine *data_line)
{
    gwy_graph_model_set_si_unit_x(model,
                                  gwy_data_line_get_si_unit_x(data_line));
    gwy_graph_model_set_si_unit_y(model,
                                  gwy_data_line_get_si_unit_y(data_line));
}

/**
 * gwy_graph_model_get_si_unit_x:
 * @model: A #GwyGraphModel.
 *
 * Returns: A new #GwySIUnit containing the physical unit for graph x axis
 **/
GwySIUnit*
gwy_graph_model_get_si_unit_x(GwyGraphModel *model)
{
    return gwy_si_unit_duplicate(model->x_unit);
}

/**
 * gwy_graph_model_get_si_unit_y:
 * @model: A #GwyGraphModel.
 *
 * Returns: A new #GwySIUnit containing the physical unit for graph y axis
 **/
GwySIUnit*
gwy_graph_model_get_si_unit_y(GwyGraphModel *model)
{
    return gwy_si_unit_duplicate(model->y_unit);
}

/**
 * gwy_graph_model_set_direction_logarithmic:
 * @model: a #GwyGraphModel.
 * @direction: axis orientation (e. g. horizontal, vertical).
 * @is_logarithmic: the logarithmic mode
 *
 * Sets data along the axis specified by @direction to display either
 * logarithmically (@is_logarithmic=TRUE) or normally (@is_logarithmic=FALSE).
 **/
void
gwy_graph_model_set_direction_logarithmic(GwyGraphModel *model,
                                          GtkOrientation direction,
                                          gboolean is_logarithmic)
{
    if (direction == GTK_ORIENTATION_VERTICAL)
        model->y_is_logarithmic = is_logarithmic;
    else
        model->x_is_logarithmic = is_logarithmic;

    gwy_graph_model_signal_layout_changed(model);
}

/**
 * gwy_graph_model_get_direction_logarithmic:
 * @model: a #GwyGraphModel.
 * @direction: axis orientation (e. g. horizontal, vertical).
 *
 * Returns: TRUE if the axis specified by @direction is currently set to display
 * logarithmically. FALSE if it is set to display normally.
 **/
gboolean
gwy_graph_model_get_direction_logarithmic(GwyGraphModel *model,
                                          GtkOrientation direction)
{
    if (direction == GTK_ORIENTATION_VERTICAL)
        return model->y_is_logarithmic;

    return model->x_is_logarithmic;
}

/**
 * gwy_graph_model_x_data_can_be_logarithmed:
 * @model: a #GwyGraphModel.
 *
 * Returns: TRUE if all x-values are greater than zero (thus logarithmic
 * display of x-data is safe).
 **/
gboolean
gwy_graph_model_x_data_can_be_logarithmed(GwyGraphModel *model)
{
    GwyGraphCurveModel *cmodel;
    guint i, j, n;
    const gdouble *data;

    for (i = 0; i < model->curves->len; i++) {
        cmodel = g_ptr_array_index(model->curves, i);
        data = gwy_graph_curve_model_get_xdata(cmodel);
        n = gwy_graph_curve_model_get_ndata(cmodel);
        for (j = 0; j < n; j++) {
            if (data[j] <= 0)
                return FALSE;
        }
    }
    return TRUE;
}

/**
 * gwy_graph_model_y_data_can_be_logarithmed:
 * @model: a #GwyGraphModel.
 *
 * Returns: TRUE if all y-values are greater than zero (thus logarithmic
 * display of y-data is safe).
 **/
gboolean
gwy_graph_model_y_data_can_be_logarithmed(GwyGraphModel *model)
{
    GwyGraphCurveModel *cmodel;
    gint i, j, n;
    const gdouble *data;

    for (i = 0; i < model->curves->len; i++) {
        cmodel = g_ptr_array_index(model->curves, i);
        data = gwy_graph_curve_model_get_ydata(cmodel);
        n = gwy_graph_curve_model_get_ndata(cmodel);
        for (j = 0; j < n; j++) {
            if (data[j] <= 0)
                return FALSE;
        }
    }
    return TRUE;
}

/**
 * gwy_graph_model_export_ascii:
 * @model: A #GwyGraphModel.
 * @filename: name of file to be created
 * @export_units: export units in the column header
 * @export_labels: export labels in the column header
 * @export_metadata: export all graph metadata within file header
 * @export_style: specifies the file format to export to (e. g. plain, csv,
 * gnuplot, etc.)
 *
 * Exports graph model into a file. The export format is specified by
 * parameter @export_style.
 **/
void
gwy_graph_model_export_ascii(GwyGraphModel *model, const gchar *filename,
                             gboolean export_units, gboolean export_labels,
                             gboolean export_metadata, GwyGraphModelExportStyle export_style)
{
    FILE *fw;
    GwyGraphCurveModel *cmodel;
    GwySIValueFormat *xformat = NULL, *yformat = NULL;
    gdouble xaverage, xrange, yaverage, yrange;
    gdouble xmult, ymult;
    GString *labels, *descriptions, *units;
    gint i, j, max, ndata;

    fw = g_fopen(filename, "w");

    if (export_units) {
        xaverage = (model->x_max + model->x_min)/2;
        xrange = model->x_max - model->x_min;
        xformat = gwy_si_unit_get_format(model->x_unit, GWY_SI_UNIT_FORMAT_MARKUP,
                                        MAX(xaverage, xrange), xformat);
        xmult = xformat->magnitude;

        yaverage = (model->y_max + model->y_min)/2;
        yrange = model->y_max - model->y_min;
        yformat = gwy_si_unit_get_format(model->y_unit, GWY_SI_UNIT_FORMAT_MARKUP,
                                        MAX(yaverage, yrange), yformat);
        ymult = yformat->magnitude;
    }
    else {
        xmult = 1;
        ymult = 1;
    }

    switch (export_style) {
        case GWY_GRAPH_MODEL_EXPORT_ASCII_PLAIN:
        case GWY_GRAPH_MODEL_EXPORT_ASCII_ORIGIN:
        labels = g_string_new("");
        descriptions = g_string_new("");
        units = g_string_new("");
        for (i = 0; i < model->curves->len; i++) {
            cmodel = g_ptr_array_index(model->curves, i);
            if (export_metadata)
                g_string_append_printf(descriptions, "%s             ",
                                       cmodel->description->str);
            if (export_labels)
                g_string_append_printf(labels, "%s       %s           ",
                                       model->bottom_label->str,
                                       model->left_label->str);
            if (export_units)
                g_string_append_printf(units, "[%s]     [%s]         ",
                                       xformat->units, yformat->units);
        }
        if (export_metadata) fprintf(fw, "%s\n", descriptions->str);
        if (export_labels) fprintf(fw, "%s\n", labels->str);
        if (export_units) fprintf(fw, "%s\n", units->str);
        g_string_free(descriptions, TRUE);
        g_string_free(labels, TRUE);
        g_string_free(units, TRUE);

        max = 0;
        for (i = 0; i < model->curves->len; i++) {
            cmodel = g_ptr_array_index(model->curves, i);
            if ((ndata = gwy_graph_curve_model_get_ndata(cmodel)) > max)
                max = ndata;
        }

        for (j = 0; j < max; j++) {
            for (i = 0; i < model->curves->len; i++) {
                cmodel = g_ptr_array_index(model->curves, i);
                if (gwy_graph_curve_model_get_ndata(cmodel) > j)
                    fprintf(fw, "%g  %g            ",
                            cmodel->xdata[j]/xmult, cmodel->ydata[j]/ymult);
                else
                    fprintf(fw, "-          -              ");
            }
            fprintf(fw, "\n");
        }
        break;

        case GWY_GRAPH_MODEL_EXPORT_ASCII_GNUPLOT:
        for (i = 0; i < model->curves->len; i++) {
            cmodel = g_ptr_array_index(model->curves, i);
            if (export_metadata)
                fprintf(fw, "# %s\n", cmodel->description->str);
            if (export_labels)
                fprintf(fw, "# %s      %s\n",
                        model->bottom_label->str, model->left_label->str);
            if (export_units)
                fprintf(fw, "# [%s]    [%s]\n",
                        xformat->units, yformat->units);
            for (j = 0; j < cmodel->n; j++)
                fprintf(fw, "%g   %g\n",
                        cmodel->xdata[j]/xmult, cmodel->ydata[j]/ymult);
            fprintf(fw, "\n\n");
        }

        break;

        case GWY_GRAPH_MODEL_EXPORT_ASCII_CSV:
        labels = g_string_new("");
        descriptions = g_string_new("");
        units = g_string_new("");
        for (i = 0; i < model->curves->len; i++) {
            cmodel = g_ptr_array_index(model->curves, i);
            if (export_metadata)
                g_string_append_printf(descriptions, "%s;",
                                       cmodel->description->str);
            if (export_labels)
                g_string_append_printf(labels, "%s;%s;",
                                       model->bottom_label->str,
                                       model->left_label->str);
            if (export_units)
                g_string_append_printf(units, "[%s];[%s];",
                                       xformat->units, yformat->units);
        }
        if (export_metadata)
            fprintf(fw, "%s\n", descriptions->str);
        if (export_labels)
            fprintf(fw, "%s\n", labels->str);
        if (export_units)
            fprintf(fw, "%s\n", units->str);
        g_string_free(descriptions, TRUE);
        g_string_free(labels, TRUE);
        g_string_free(units, TRUE);

        max = 0;
        for (i = 0; i < model->curves->len; i++) {
            cmodel = g_ptr_array_index(model->curves, i);
            if ((ndata = gwy_graph_curve_model_get_ndata(cmodel)) > max)
                max = ndata;
        }

        for (j = 0; j < max; j++) {
            for (i = 0; i < model->curves->len; i++) {
                cmodel = g_ptr_array_index(model->curves, i);
                if (gwy_graph_curve_model_get_ndata(cmodel) > j)
                    fprintf(fw, "%g;%g;",
                            cmodel->xdata[j]/xmult, cmodel->ydata[j]/ymult);
                else
                    fprintf(fw, ";;");
            }
            fprintf(fw, "\n");
        }
        break;

        default:
        break;
    }

    fclose(fw);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygraphmodel
 * @title: GwyGraphModel
 * @short_description: Representation of a graph
 *
 * #GwyGraphModel represents information about a graph necessary to fully
 * reconstruct it.
 **/

/**
 * gwy_graph_model_duplicate:
 * @gmodel: A graph model to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
