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
#include <libgwyddion/gwydebugobjects.h>
#include <libgwydgets/gwydgettypes.h>
#include <libgwydgets/gwygraphcurvemodel.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraph.h>

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
    PROP_N_CURVES,
    PROP_TITLE,
    PROP_AXIS_LABEL_BOTTOM,
    PROP_AXIS_LABEL_LEFT,
    PROP_AXIS_LABEL_RIGHT,
    PROP_AXIS_LABEL_TOP,
    PROP_X_LOGARITHMIC,
    PROP_Y_LOGARITHMIC,
    PROP_SI_UNIT_X,
    PROP_SI_UNIT_Y,
    PROP_LABEL_FRAME_THICKNESS,
    PROP_LABEL_HAS_FRAME,
    PROP_LABEL_POSITION,
    PROP_LABEL_REVERSE,
    PROP_LABEL_VISIBLE,
    PROP_LAST
};

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
         PROP_N_CURVES,
         g_param_spec_uint("n-curves",
                           "Number of curves",
                           "The number of curves in graph model",
                           0, G_MAXUINT, 0,
                           G_PARAM_READABLE));

    g_object_class_install_property
        (gobject_class,
         PROP_TITLE,
         g_param_spec_string("title",
                             "Title",
                             "The graph title",
                             "New graph",
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_AXIS_LABEL_BOTTOM,
         g_param_spec_string("axis-label-bottom",
                             "Axis label bottom",
                             "The label of the bottom axis",
                             "x",
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_AXIS_LABEL_LEFT,
         g_param_spec_string("axis-label-left",
                             "Axis label left",
                             "The label of the left axis",
                             "y",
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_AXIS_LABEL_RIGHT,
         g_param_spec_string("axis-label-right",
                             "Axis label right",
                             "The label of the right axis",
                             "",
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_AXIS_LABEL_TOP,
         g_param_spec_string("axis-label-top",
                             "Axis label top",
                             "The label of the top axis",
                             "",
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_X_LOGARITHMIC,
         g_param_spec_boolean("x-logarithmic",
                              "X logarithmic",
                              "TRUE if x coordinate is logarithimic",
                              FALSE,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_Y_LOGARITHMIC,
         g_param_spec_boolean("y-logarithmic",
                              "Y logarithmic",
                              "TRUE if y coordinate is logarithimic",
                              FALSE,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_SI_UNIT_X,
         g_param_spec_object("si-unit-x",
                             "X unit",
                             "Unit of x axis",
                             GWY_TYPE_SI_UNIT,
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_SI_UNIT_Y,
         g_param_spec_object("si-unit-y",
                             "Y unit",
                             "Unit of y axis",
                             GWY_TYPE_SI_UNIT,
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_LABEL_REVERSE,
         g_param_spec_boolean("label-reverse",
                              "Label reverse",
                              "TRUE if text and curve sample is switched in "
                                  "key",
                              FALSE,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_LABEL_VISIBLE,
         g_param_spec_boolean("label-visible",
                              "Label visible",
                              "TRUE if key label is visible",
                              TRUE,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_LABEL_HAS_FRAME,
         g_param_spec_boolean("label-has-frame",
                              "Label has frame",
                              "TRUE if key label has frame",
                              TRUE,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_LABEL_FRAME_THICKNESS,
         g_param_spec_int("label-frame-thickness",
                          "Label frame thickness",
                          "Thickness of key label frame",
                          0, 16, 1,
                          G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_LABEL_POSITION,
         g_param_spec_enum("label-position",
                           "Label position",
                           "Position type of key label",
                           GWY_TYPE_GRAPH_LABEL_POSITION,
                           GWY_GRAPH_LABEL_NORTHEAST,
                           G_PARAM_READWRITE));
}

static void
gwy_graph_model_init(GwyGraphModel *gmodel)
{
    gwy_debug_objects_creation((GObject*)gmodel);

    gmodel->curves = g_ptr_array_new();

    gmodel->x_unit = gwy_si_unit_new("");
    gmodel->y_unit = gwy_si_unit_new("");

    gmodel->title = g_string_new("Graph");
    gmodel->bottom_label = g_string_new("x");
    gmodel->top_label = g_string_new("");
    gmodel->left_label = g_string_new("y");
    gmodel->right_label = g_string_new("");

    gmodel->label_position = GWY_GRAPH_LABEL_NORTHEAST;
    gmodel->label_has_frame = TRUE;
    gmodel->label_frame_thickness = 1;
    gmodel->label_reverse = FALSE;
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
            { 'b', "label.reverse", &gmodel->label_reverse, NULL },
            { 'b', "label.visible", &gmodel->label_visible, NULL },
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
            { 'b', "label.reverse", &gmodel->label_reverse, NULL },
            { 'b', "label.visible", &gmodel->label_visible, NULL },
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
        gboolean b;
        GwyGraphCurveModel **curves = NULL;
        guint32 ncurves = 0;
        GwySerializeSpec spec[] = {
            /* Accept, but ignore */
            { 'b', "has_x_unit", &b, NULL },
            { 'b', "has_y_unit", &b, NULL },
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
            { 'b', "label.reverse", &gmodel->label_reverse, NULL },
            { 'b', "label.visible", &gmodel->label_visible, NULL },
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
        g_string_assign(gmodel->title, g_value_get_string(value));
        break;

        case PROP_AXIS_LABEL_BOTTOM:
        g_string_assign(gmodel->bottom_label, g_value_get_string(value));
        break;

        case PROP_AXIS_LABEL_LEFT:
        g_string_assign(gmodel->left_label, g_value_get_string(value));
        break;

        case PROP_AXIS_LABEL_RIGHT:
        g_string_assign(gmodel->right_label, g_value_get_string(value));
        break;

        case PROP_AXIS_LABEL_TOP:
        g_string_assign(gmodel->top_label, g_value_get_string(value));
        break;

        case PROP_SI_UNIT_X:
        gwy_graph_model_set_si_unit_x(gmodel, g_value_get_object(value));
        break;

        case PROP_SI_UNIT_Y:
        gwy_graph_model_set_si_unit_y(gmodel, g_value_get_object(value));
        break;

        case PROP_X_LOGARITHMIC:
        gmodel->x_is_logarithmic = g_value_get_boolean(value);
        break;

        case PROP_Y_LOGARITHMIC:
        gmodel->y_is_logarithmic = g_value_get_boolean(value);
        break;

        case PROP_LABEL_FRAME_THICKNESS:
        gmodel->label_frame_thickness = g_value_get_int(value);
        break;

        case PROP_LABEL_HAS_FRAME:
        gmodel->label_has_frame = g_value_get_boolean(value);
        break;

        case PROP_LABEL_POSITION:
        gmodel->label_position = g_value_get_enum(value);
        break;

        case PROP_LABEL_REVERSE:
        gmodel->label_reverse = g_value_get_boolean(value);
        break;

        case PROP_LABEL_VISIBLE:
        gmodel->label_visible = g_value_get_boolean(value);
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
        g_value_set_string(value, gmodel->title->str);
        break;

        case PROP_N_CURVES:
        g_value_set_uint(value, gmodel->curves->len);
        break;

        case PROP_AXIS_LABEL_BOTTOM:
        g_value_set_string(value, gmodel->bottom_label->str);
        break;

        case PROP_AXIS_LABEL_LEFT:
        g_value_set_string(value, gmodel->left_label->str);
        break;

        case PROP_AXIS_LABEL_RIGHT:
        g_value_set_string(value, gmodel->right_label->str);
        break;

        case PROP_AXIS_LABEL_TOP:
        g_value_set_string(value, gmodel->top_label->str);
        break;

        case PROP_SI_UNIT_X:
        /* Keep the idiosyncratic semantics of gwy_graph_model_get_si_unit_x */
        g_value_take_object(value, gwy_si_unit_duplicate(gmodel->x_unit));
        break;

        case PROP_SI_UNIT_Y:
        /* Keep the idiosyncratic semantics of gwy_graph_model_get_si_unit_y */
        g_value_take_object(value, gwy_si_unit_duplicate(gmodel->y_unit));
        break;

        case PROP_X_LOGARITHMIC:
        g_value_set_boolean(value, gmodel->x_is_logarithmic);
        break;

        case PROP_Y_LOGARITHMIC:
        g_value_set_boolean(value, gmodel->y_is_logarithmic);
        break;

        case PROP_LABEL_FRAME_THICKNESS:
        g_value_set_int(value, gmodel->label_frame_thickness);
        break;

        case PROP_LABEL_HAS_FRAME:
        g_value_set_boolean(value, gmodel->label_has_frame);
        break;

        case PROP_LABEL_POSITION:
        g_value_set_enum(value, gmodel->label_position);
        break;

        case PROP_LABEL_REVERSE:
        g_value_set_boolean(value, gmodel->label_reverse);
        break;

        case PROP_LABEL_VISIBLE:
        g_value_set_boolean(value, gmodel->label_visible);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_graph_model_new_alike:
 * @gmodel: A graph model.
 *
 * Creates new graph model object that has the same settings as @gmodel.
 *
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

    duplicate->title = g_string_new(gmodel->title->str);;
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
 * @gmodel: A graph model.
 * @curve: A #GwyGraphCurveModel representing the curve to add.
 *
 * Adds a new curve to a graph model.
 *
 * Returns: The index of the added curve in @gmodel.
 **/
gint
gwy_graph_model_add_curve(GwyGraphModel *gmodel,
                          GwyGraphCurveModel *curve)
{
    gint idx;

    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), -1);
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(curve), -1);

    g_object_ref(curve);
    g_ptr_array_add(gmodel->curves, curve);
    idx = gmodel->curves->len - 1;

    /* In principle, this can change gmodel->curves->len, so we have to save
     * the index in idx. */
    g_object_notify(G_OBJECT(gmodel), "n-curves");

    return idx;
}

/**
 * gwy_graph_model_get_n_curves:
 * @gmodel: A graph model.
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
 * @gmodel: A graph model.
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
        g_object_unref(cmodel);
    }
    g_ptr_array_set_size(gmodel->curves, 0);
    g_object_notify(G_OBJECT(gmodel), "n-curves");
}

/**
 * gwy_graph_model_remove_curve_by_description:
 * @gmodel: A graph model.
 * @description: Curve description (label).
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
    g_return_val_if_fail(description, 0);

    newcurves = g_ptr_array_new();
    for (i = 0; i < gmodel->curves->len; i++) {
        cmodel = g_ptr_array_index(gmodel->curves, i);
        if (gwy_strequal(description, cmodel->description->str)) {
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

    g_object_notify(G_OBJECT(gmodel), "n-curves");
    return i;
}

/**
 * gwy_graph_model_remove_curve:
 * @gmodel: A graph model.
 * @cindex: Curve index in graph model.
 *
 * Removes the curve having given index.
 **/
void
gwy_graph_model_remove_curve(GwyGraphModel *gmodel,
                             gint cindex)
{
    GwyGraphCurveModel *cmodel;

    g_return_if_fail(GWY_IS_GRAPH_MODEL(gmodel));
    g_return_if_fail(cindex >= 0 && cindex < gmodel->curves->len);

    cmodel = g_ptr_array_index(gmodel->curves, cindex);
    g_object_unref(cmodel);

    g_ptr_array_remove_index(gmodel->curves, cindex);
    g_object_notify(G_OBJECT(gmodel), "n-curves");
}

/**
 * gwy_graph_model_get_curve_by_description:
 * @gmodel: A graph model.
 * @description: Curve description (label).
 *
 * Returns: The first curve that has description (label) given by @description
 *          (no reference is added).
 **/
/* XXX: Malformed documentation. */
GwyGraphCurveModel*
gwy_graph_model_get_curve_by_description(GwyGraphModel *gmodel,
                                         const gchar *description)
{
    GwyGraphCurveModel *cmodel;
    guint i;

    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), NULL);
    g_return_val_if_fail(description, NULL);

    for (i = 0; i < gmodel->curves->len; i++) {
        cmodel = g_ptr_array_index(gmodel->curves, i);
        if (gwy_strequal(description, cmodel->description->str))
            return cmodel;
    }

    return NULL;
}

/**
 * gwy_graph_model_get_curve:
 * @gmodel: A graph model.
 * @cindex: Curve index in graph model.
 *
 * Gets a graph model curve by its index.
 *
 * Returns: The curve with index @cindex (no reference is added).
 **/
GwyGraphCurveModel*
gwy_graph_model_get_curve(GwyGraphModel *gmodel,
                          gint cindex)
{
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), NULL);
    g_return_val_if_fail(cindex >= 0 && cindex < gmodel->curves->len, NULL);

    return g_ptr_array_index(gmodel->curves, cindex);
}

/**
 * gwy_graph_model_get_curve_index:
 * @gmodel: A graph mode.
 * @curve: A curve model present in @gmodel to find.
 *
 * Finds the index of a graph model curve.
 *
 * Returns: The index of @curve in @gmodel, -1 if it is not present there.
 **/
gint
gwy_graph_model_get_curve_index(GwyGraphModel *gmodel,
                                GwyGraphCurveModel *curve)
{
    GwyGraphCurveModel *cmodel;
    guint i;

    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), -1);
    g_return_val_if_fail(GWY_IS_GRAPH_CURVE_MODEL(curve), -1);

    for (i = 0; i < gmodel->curves->len; i++) {
        cmodel = g_ptr_array_index(gmodel->curves, i);
        if (cmodel == curve)
            return (gint)i;
    }

    return -1;
}

/**
 * gwy_graph_model_set_title:
 * @model: A graph model.
 * @title: A new graphmodel title.
 *
 * Sets the title of a graph model.
 **/
void
gwy_graph_model_set_title(GwyGraphModel *model,
                          const gchar *title)
{
    g_return_if_fail(GWY_IS_GRAPH_MODEL(model));
    if (gwy_strequal(model->title->str, title))
        return;

    g_string_assign(model->title, title);
    g_object_notify(G_OBJECT(model), "title");
}

/**
 * gwy_graph_model_set_label_position:
 * @model: A graph model.
 * @position: A new graphmodel label position.
 *
 * Sets the label (curve desriptions) postion on graph widget.
 **/
void
gwy_graph_model_set_label_position(GwyGraphModel *model,
                                   GwyGraphLabelPosition position)
{
    g_return_if_fail(GWY_IS_GRAPH_MODEL(model));
    if (model->label_position == position)
        return;

    model->label_position = position;
    g_object_notify(G_OBJECT(model), "label-position");
}

/**
 * gwy_graph_model_set_label_has_frame:
 * @model: A graph model.
 * @label_has_frame: %TRUE to make label frame visible.
 *
 * Sets whether graph label has a frame around it.
 *
 * Note that the label must be visible
 * (see #gwy_graph_model_set_label_visible()) to see the label.
 **/
void
gwy_graph_model_set_label_has_frame(GwyGraphModel *model,
                                    gboolean label_has_frame)
{
    g_return_if_fail(GWY_IS_GRAPH_MODEL(model));
    if (model->label_has_frame == label_has_frame)
        return;

    model->label_has_frame = label_has_frame;
    g_object_notify(G_OBJECT(model), "label-has-frame");
}

/**
 * gwy_graph_model_set_label_frame_thickness:
 * @model: A graph model.
 * @thickness: Label frame thickness (in pixels).
 *
 * Sets the label frame thickness. Note that both the frame and label must
 * be visible (see #gwy_graph_model_set_label_visible()) to see label and label
 * frame.
 **/
/* XXX: Malformed documentation. */
void
gwy_graph_model_set_label_frame_thickness(GwyGraphModel *model, gint thickness)
{
    g_return_if_fail(GWY_IS_GRAPH_MODEL(model));
    if (model->label_frame_thickness == thickness)
        return;

    model->label_frame_thickness = thickness;
    g_object_notify(G_OBJECT(model), "label-frame-thickness");
}

/**
 * gwy_graph_model_set_label_reverse:
 * @model: A graph model.
 * @reverse: Label alingment mode.
 *
 * Sets the label alignment (curve samples and their description postion).
 * By setting the @reverse = TRUE you get alignment ("text", "sample"),
 * otherwise you get alignment ("sample", "text").
 **/
/* XXX: Malformed documentation. */
void
gwy_graph_model_set_label_reverse(GwyGraphModel *model, gboolean reverse)
{
    g_return_if_fail(GWY_IS_GRAPH_MODEL(model));
    if (model->label_reverse == reverse)
        return;

    model->label_reverse = reverse;
    g_object_notify(G_OBJECT(model), "label-reverse");
}

/**
 * gwy_graph_model_set_label_visible:
 * @model: A graph model.
 * @visible: Label visibility.
 *
 * Sets the graph widget label visibility.
 **/
void
gwy_graph_model_set_label_visible(GwyGraphModel *model, gboolean visible)
{
    g_return_if_fail(GWY_IS_GRAPH_MODEL(model));
    if (model->label_visible == visible)
        return;

    model->label_visible = visible;
    g_object_notify(G_OBJECT(model), "label-visible");
}

/**
 * gwy_graph_model_get_title:
 * @model: A graph model.
 *
 * Returns: graph title.
 **/
/* XXX: Malformed documentation. */
const gchar*
gwy_graph_model_get_title(GwyGraphModel *model)
{
    return model->title->str;
}

/**
 * gwy_graph_model_get_label_position:
 * @model: A graph model.
 *
 * Returns: graph widget label position.
 **/
/* XXX: Malformed documentation. */
GwyGraphLabelPosition
gwy_graph_model_get_label_position(GwyGraphModel *model)
{
    return model->label_position;
}

/**
 * gwy_graph_model_get_label_has_frame:
 * @model: A graph model.
 *
 * Returns: graph widget label frame visibility.
 **/
/* XXX: Malformed documentation. */
gboolean
gwy_graph_model_get_label_has_frame(GwyGraphModel *model)
{
    return model->label_has_frame;
}

/**
 * gwy_graph_model_get_label_frame_thickness:
 * @model: A graph model.
 *
 * Returns: graph widget label frame thickness.
 **/
/* XXX: Malformed documentation. */
gint
gwy_graph_model_get_label_frame_thickness(GwyGraphModel *model)
{
    return model->label_frame_thickness;
}

/**
 * gwy_graph_model_get_label_reverse:
 * @model: A graph model.
 *
 * Returns: graph widget label alignment mode.
 **/
/* XXX: Malformed documentation. */
gboolean
gwy_graph_model_get_label_reverse(GwyGraphModel *model)
{
    return model->label_reverse;
}

/**
 * gwy_graph_model_get_label_visible:
 * @model: A graph model.
 *
 * Returns: graph widget label visibility.
 **/
/* XXX: Malformed documentation. */
gboolean
gwy_graph_model_get_label_visible(GwyGraphModel *model)
{
    return model->label_visible;
}

/**
 * gwy_graph_model_set_si_unit_x:
 * @model: A graph model.
 * @siunit: Physical unit for x axis.
 *
 * Sets the physical unit for graph x axis.
 *
 * The unit is only assigned by value, the graph model does not directly use
 * @siunit nor it takes any reference to it.
 **/
void
gwy_graph_model_set_si_unit_x(GwyGraphModel *model, GwySIUnit *siunit)
{
    g_return_if_fail(GWY_IS_GRAPH_MODEL(model));
    if (gwy_si_unit_equal(siunit, model->x_unit))
        return;

    gwy_serializable_clone(G_OBJECT(siunit), G_OBJECT(model->x_unit));
    g_object_notify(G_OBJECT(model), "si-unit-x");
}

/**
 * gwy_graph_model_set_si_unit_y:
 * @model: A graph model.
 * @siunit: physical unit for y axis
 *
 * Sets the physical unit for graph y axis.
 *
 * The unit is only assigned by value, the graph model does not directly use
 * @siunit nor it takes any reference to it.
 **/
void
gwy_graph_model_set_si_unit_y(GwyGraphModel *model, GwySIUnit *siunit)
{
    g_return_if_fail(GWY_IS_GRAPH_MODEL(model));
    if (gwy_si_unit_equal(siunit, model->y_unit))
        return;

    gwy_serializable_clone(G_OBJECT(siunit), G_OBJECT(model->y_unit));
    g_object_notify(G_OBJECT(model), "si-unit-y");
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
 * @model: A graph model.
 *
 * Gets the physical unit of graph x axis.
 *
 * Returns: A newly allocated #GwySIUnit with the physical unit of the x-axis
 *          of @model.
 **/
GwySIUnit*
gwy_graph_model_get_si_unit_x(GwyGraphModel *model)
{
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(model), NULL);
    return gwy_si_unit_duplicate(model->x_unit);
}

/**
 * gwy_graph_model_get_si_unit_y:
 * @model: A graph model.
 *
 * Gets the physical unit of graph y axis.
 *
 * Returns: A newly allocated #GwySIUnit with the physical unit of the y-axis
 *          of @model.
 **/
GwySIUnit*
gwy_graph_model_get_si_unit_y(GwyGraphModel *model)
{
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(model), NULL);
    return gwy_si_unit_duplicate(model->y_unit);
}

/**
 * gwy_graph_model_set_direction_logarithmic:
 * @model: A graph model.
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
    g_return_if_fail(GWY_IS_GRAPH_MODEL(model));

    if (direction == GTK_ORIENTATION_VERTICAL) {
        if (model->y_is_logarithmic == is_logarithmic)
            return;

        model->y_is_logarithmic = is_logarithmic;
        g_object_notify(G_OBJECT(model), "y-logarithmic");
    }
    else {
        if (model->x_is_logarithmic == is_logarithmic)
            return;

        model->x_is_logarithmic = is_logarithmic;
        g_object_notify(G_OBJECT(model), "x-logarithmic");
    }
}

/**
 * gwy_graph_model_get_direction_logarithmic:
 * @model: A graph model.
 * @direction: axis orientation (e. g. horizontal, vertical).
 *
 * Returns: TRUE if the axis specified by @direction is currently set to display
 * logarithmically. FALSE if it is set to display normally.
 **/
/* XXX: Malformed documentation. */
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
 * @model: A graph model.
 *
 * Returns: TRUE if all x-values are greater than zero (thus logarithmic
 * display of x-data is safe).
 **/
/* XXX: Malformed documentation. */
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
 * @model: A graph model.
 *
 * Returns: TRUE if all y-values are greater than zero (thus logarithmic
 * display of y-data is safe).
 **/
/* XXX: Malformed documentation. */
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
 * gwy_graph_model_get_xmin:
 * @model: A graph model.
 *
 * Returns bounding value for all the curves within model.
 * This value is recomputed while adding curves to the
 * graph depending on actual graph settings.
 *
 * Returns: x minimum bounding value of the curves
 **/
gdouble
gwy_graph_model_get_xmin(GwyGraphModel *model)
{
    return model->x_min;
}
/**
 * gwy_graph_model_get_ymin:
 * @model: A graph model.
 *
 * Returns bounding value for all the curves within model.
 * This value is recomputed while adding curves to the
 * graph depending on actual graph settings.
 *
 * Returns: y minimum bounding value of the curves
 **/
gdouble
gwy_graph_model_get_ymin(GwyGraphModel *model)
{
    return model->y_min;
}

/**
 * gwy_graph_model_get_xmax:
 * @model: A graph model.
 *
 * Returns bounding value for all the curves within model.
 * This value is recomputed while adding curves to the
 * graph depending on actual graph settings.
 *
 * Returns: x maximum bounding value of the curves
 **/
gdouble
gwy_graph_model_get_xmax(GwyGraphModel *model)
{
    return model->x_max;
}

/**
 * gwy_graph_model_get_ymax:
 * @model: A graph model.
 *
 * Returns bounding value for all the curves within model.
 * This value is recomputed while adding curves to the
 * graph depending on actual graph settings.
 *
 * Returns: y maximum bounding value of the curves
 **/
gdouble
gwy_graph_model_get_ymax(GwyGraphModel *model)
{
    return model->y_max;
}

/**
 * gwy_graph_model_set_xmin:
 * @model: A graph model.
 *
 * Sets bounding value for all the curves within model.
 * This value is recomputed while adding curves to the
 * graph depending on actual graph settings.
 *
 **/
void
gwy_graph_model_set_xmin(GwyGraphModel *model, gdouble value)
{
    model->x_min = value;
}

/**
 * gwy_graph_model_set_xmax:
 * @model: A graph model.
 *
 * Sets bounding value for all the curves within model.
 * This value is recomputed while adding curves to the
 * graph depending on actual graph settings.
 *
 **/
void
gwy_graph_model_set_xmax(GwyGraphModel *model, gdouble value)
{
    model->x_max = value;
}
/**
 * gwy_graph_model_set_ymin:
 * @model: A graph model.
 *
 * Sets bounding value for all the curves within model.
 * This value is recomputed while adding curves to the
 * graph depending on actual graph settings.
 *
 **/
void
gwy_graph_model_set_ymin(GwyGraphModel *model, gdouble value)
{
    model->y_min = value;
}
/**
 * gwy_graph_model_set_ymax:
 * @model: A graph model.
 *
 * Sets bounding value for all the curves within model.
 * This value is recomputed while adding curves to the
 * graph depending on actual graph settings.
 *
 **/
void
gwy_graph_model_set_ymax(GwyGraphModel *model, gdouble value)
{
    model->y_max = value;
}

/**
 * gwy_graph_model_get_axis_label:
 * @model: A graph model.
 * @pos: Axis position.
 *
 * Gets the label of a one graph model axis.
 *
 * Returns: The label as a string owned by the model.
 **/
const gchar*
gwy_graph_model_get_axis_label(GwyGraphModel *model,
                               GtkPositionType pos)
{
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(model), NULL);
    switch (pos) {
        case GTK_POS_BOTTOM:
        return model->bottom_label->str;
        break;

        case GTK_POS_LEFT:
        return model->left_label->str;
        break;

        case GTK_POS_RIGHT:
        return model->right_label->str;
        break;

        case GTK_POS_TOP:
        return model->top_label->str;
        break;

        default:
        g_return_val_if_reached(NULL);
        break;
    }
}

/**
 * gwy_graph_model_set_axis_label:
 * @model: A graph model.
 * @pos: Axis position.
 * @label: The new label.
 *
 * Sets one axis label of a graph model.
 **/
void
gwy_graph_model_set_axis_label(GwyGraphModel *model,
                               GtkPositionType pos,
                               const gchar *label)
{
    g_return_if_fail(GWY_IS_GRAPH_MODEL(model));
    if (!label)
        label = "";

    switch (pos) {
        case GTK_POS_BOTTOM:
        if (!gwy_strequal(model->bottom_label->str, label)) {
            g_string_assign(model->bottom_label, label);
            g_object_notify(G_OBJECT(model), "axis-label-bottom");
        }
        break;

        case GTK_POS_LEFT:
        if (!gwy_strequal(model->left_label->str, label)) {
            g_string_assign(model->left_label, label);
            g_object_notify(G_OBJECT(model), "axis-label-left");
        }
        break;

        case GTK_POS_RIGHT:
        if (!gwy_strequal(model->right_label->str, label)) {
            g_string_assign(model->right_label, label);
            g_object_notify(G_OBJECT(model), "axis-label-right");
        }
        break;

        case GTK_POS_TOP:
        if (!gwy_strequal(model->top_label->str, label)) {
            g_string_assign(model->top_label, label);
            g_object_notify(G_OBJECT(model), "axis-label-top");
        }
        break;

        default:
        g_return_if_reached();
        break;
    }
}

/**
 * gwy_graph_model_export_ascii:
 * @model: A graph model.
 * @export_units: %TRUE to export units in the column header.
 * @export_labels: %TRUE to export labels in the column header.
 * @export_metadata: %TRUE to export all graph metadata within file header.
 * @export_style: File format subtype to export to (e. g. plain, csv, gnuplot,
 *                etc.).
 * @string: A string to append the text dump to, or %NULL to allocate a new
 *          string.
 *
 * Exports a graph model data to a file.
 *
 * The export format is specified by parameter @export_style.
 *
 * Returns: Either @string itself if it was not %NULL, or a newly allocated
 *          #GString.
 **/
GString*
gwy_graph_model_export_ascii(GwyGraphModel *model,
                             gboolean export_units,
                             gboolean export_labels,
                             gboolean export_metadata,
                             GwyGraphModelExportStyle export_style,
                             GString* string)
{
    GwyGraphCurveModel *cmodel;
    GwySIValueFormat *xformat = NULL, *yformat = NULL;
    gdouble xaverage, xrange, yaverage, yrange;
    gdouble xmult, ymult;
    GString *labels, *descriptions, *units;
    gint i, j, max, ndata;

    if (!string)
        string = g_string_new(NULL);

    if (export_units) {
        xaverage = (model->x_max + model->x_min)/2;
        xrange = model->x_max - model->x_min;
        xformat = gwy_si_unit_get_format(model->x_unit,
                                         GWY_SI_UNIT_FORMAT_MARKUP,
                                         MAX(xaverage, xrange), xformat);
        xmult = xformat->magnitude;

        yaverage = (model->y_max + model->y_min)/2;
        yrange = model->y_max - model->y_min;
        yformat = gwy_si_unit_get_format(model->y_unit,
                                         GWY_SI_UNIT_FORMAT_MARKUP,
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
        if (export_metadata)
            g_string_append_printf(string, "%s\n", descriptions->str);
        if (export_labels)
            g_string_append_printf(string, "%s\n", labels->str);
        if (export_units)
            g_string_append_printf(string, "%s\n", units->str);
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
                    g_string_append_printf(string, "%g  %g            ",
                                           cmodel->xdata[j]/xmult,
                                           cmodel->ydata[j]/ymult);
                else
                    g_string_append_printf(string,
                                           "-          -              ");
            }
            g_string_append_printf(string, "\n");
        }
        break;

        case GWY_GRAPH_MODEL_EXPORT_ASCII_GNUPLOT:
        for (i = 0; i < model->curves->len; i++) {
            cmodel = g_ptr_array_index(model->curves, i);
            if (export_metadata)
                g_string_append_printf(string, "# %s\n",
                                       cmodel->description->str);
            if (export_labels)
                g_string_append_printf(string, "# %s      %s\n",
                                       model->bottom_label->str,
                                       model->left_label->str);
            if (export_units)
                g_string_append_printf(string, "# [%s]    [%s]\n",
                                       xformat->units, yformat->units);
            for (j = 0; j < cmodel->n; j++)
                g_string_append_printf(string, "%g   %g\n",
                                       cmodel->xdata[j]/xmult,
                                       cmodel->ydata[j]/ymult);
            g_string_append_printf(string, "\n\n");
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
            g_string_append_printf(string, "%s\n", descriptions->str);
        if (export_labels)
            g_string_append_printf(string, "%s\n", labels->str);
        if (export_units)
            g_string_append_printf(string, "%s\n", units->str);
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
                    g_string_append_printf(string, "%g;%g;",
                                           cmodel->xdata[j]/xmult,
                                           cmodel->ydata[j]/ymult);
                else
                    g_string_append_printf(string, ";;");
            }
            g_string_append_printf(string, "\n");
        }
        break;

        default:
        g_return_val_if_reached(string);
        break;
    }

    return string;
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
