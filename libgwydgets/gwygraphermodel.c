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
#include "gwygraphermodel.h"

#define GWY_GRAPHER_MODEL_TYPE_NAME "GwyGrapherModel"

static void   gwy_grapher_model_class_init        (GwyGrapherModelClass *klass);
static void   gwy_grapher_model_init              (GwyGrapherModel *gmodel);
static void   gwy_grapher_model_finalize          (GObject *object);
static void   gwy_grapher_model_serializable_init (GwySerializableIface *iface);
static void   gwy_grapher_model_watchable_init    (GwyWatchableIface *iface);
static GByteArray* gwy_grapher_model_serialize    (GObject *obj,
                                                 GByteArray*buffer);
static GObject* gwy_grapher_model_deserialize     (const guchar *buffer,
                                                 gsize size,
                                                 gsize *position);
static GObject* gwy_grapher_model_duplicate       (GObject *object);
static void   gwy_grapher_model_grapher_destroyed   (GwyGrapher *grapher,
                                                 GwyGrapherModel *gmodel);
static void   gwy_grapher_model_save_grapher        (GwyGrapherModel *gmodel,
                                                 GwyGrapher *grapher);
static void     gwy_grapher_model_set_property  (GObject *object,
                                                guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec);
static void     gwy_grapher_model_get_property  (GObject*object,
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


GType
gwy_grapher_model_get_type(void)
{
    static GType gwy_grapher_model_type = 0;

    if (!gwy_grapher_model_type) {
        static const GTypeInfo gwy_grapher_model_info = {
            sizeof(GwyGrapherModelClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_grapher_model_class_init,
            NULL,
            NULL,
            sizeof(GwyGrapherModel),
            0,
            (GInstanceInitFunc)gwy_grapher_model_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_grapher_model_serializable_init, NULL, 0
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_grapher_model_watchable_init, NULL, 0
        };

        gwy_debug("");
        gwy_grapher_model_type
          = g_type_register_static(G_TYPE_OBJECT,
                                   GWY_GRAPHER_MODEL_TYPE_NAME,
                                   &gwy_grapher_model_info,
                                   0);
        g_type_add_interface_static(gwy_grapher_model_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_grapher_model_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_grapher_model_type;
}

static void
gwy_grapher_model_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_grapher_model_serialize;
    iface->deserialize = gwy_grapher_model_deserialize;
    iface->duplicate = gwy_grapher_model_duplicate;
}

static void
gwy_grapher_model_watchable_init(GwyWatchableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_grapher_model_class_init(GwyGrapherModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_grapher_model_finalize;
    gobject_class->set_property = gwy_grapher_model_set_property;
    gobject_class->get_property = gwy_grapher_model_get_property;

    g_object_class_install_property(gobject_class,
                                    PROP_N,
                                    g_param_spec_int("n",
                                                      "Number of curves",
                                                      "Changed number of curves in graph",
                                                      0,
                                                      100,
                                                      0,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(gobject_class,
                                    PROP_TITLE,
                                    g_param_spec_string("title",
                                                      "Graph Title",
                                                      "Changed title of graph",
                                                      "new graph",
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
    /*
    g_object_class_install_property(gobject_class,
                                    PROP_LABEL_POSITION,
                                    g_param_spec_string("label-position",
                                                      "Graph Label Position",
                                                      "Changed label position",
                                                      "new graph",
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class,
                                    PROP_LABEL_REVERSE,
                                    g_param_spec_string("label-reverse",
                                                      "Graph Label Alingment",
                                                      "Changed label alingment",
                                                      "new graph",
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
 */
}

static void     
gwy_grapher_model_set_property  (GObject *object,
                                                guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec)
{
    GwyGrapherModel *model = GWY_GRAPHER_MODEL(object);
}

static void     
gwy_grapher_model_get_property  (GObject*object,
                                               guint prop_id,
                                               GValue *value,
                                               GParamSpec *pspec)
{
    GwyGrapherModel *model = GWY_GRAPHER_MODEL(object);
}


static void
gwy_grapher_model_init(GwyGrapherModel *gmodel)
{
    gwy_debug("");
    gwy_debug_objects_creation((GObject*)gmodel);

    gmodel->grapher = NULL;
    gmodel->grapher_destroy_hid = 0;

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

    /* XXX: GwyGrapher has no such thing */
    gmodel->title = g_string_new("FIXME: Mysterious Grapher");
    gmodel->top_label = g_string_new("");
    gmodel->bottom_label = g_string_new("");
    gmodel->left_label = g_string_new("");
    gmodel->right_label = g_string_new("");

    gmodel->label_position = GWY_GRAPHER_LABEL_NORTHEAST;
    gmodel->label_has_frame = 1; /*designed to be removed*/
    gmodel->label_frame_thickness = 1;
    gmodel->label_reverse = 0; /*designed to be added*/
}

/**
 * gwy_grapher_model_new:
 * @grapher: A grapher to represent.
 *
 * Creates a new grapher model.
 *
 * Returns: New grapher model as a #GObject.
 **/
GObject*
gwy_grapher_model_new(GwyGrapher *grapher)
{
    GwyGrapherModel *gmodel;
    GtkWidget *window;

    gwy_debug("");
    gmodel = g_object_new(GWY_TYPE_GRAPHER_MODEL, NULL);

    gmodel->grapher = grapher;
    
    return (GObject*)(gmodel);
}


void       
gwy_grapher_model_add_curve(GwyGrapherModel *gmodel, GwyGrapherCurveModel *curve)
{
    GObject **newcurves;
    gint i;
    
    newcurves = g_new(GObject*, gmodel->ncurves+1);
    
    for (i = 0; i < gmodel->ncurves; i++)
    {
        newcurves[i] = gwy_serializable_duplicate(gmodel->curves[i]);
        g_object_unref(gmodel->curves[i]);
    }
    newcurves[i] = gwy_serializable_duplicate(curve);
 
    gmodel->curves = newcurves;
    
    gmodel->ncurves++;
   
    g_object_notify(gmodel, "n");
}

gint
gwy_grapher_model_get_n_curves(GwyGrapherModel *gmodel)
{
    /*
    g_return_val_if_fail(GWY_IS_GRAPHER_MODEL(gmodel), 0);

    if (gmodel->grapher)
        return gwy_grapher_get_number_of_curves(gmodel->grapher);
    else
        return gmodel->ncurves;
        */
}

static void
gwy_grapher_model_finalize(GObject *object)
{
    GwyGrapherModel *gmodel;
    gint i;

    gwy_debug("");

    gmodel = GWY_GRAPHER_MODEL(object);
    if (gmodel->grapher_destroy_hid) {
        g_assert(GWY_IS_GRAPHER(gmodel->grapher));
        g_signal_handler_disconnect(gmodel->grapher,
                                    gmodel->grapher_destroy_hid);
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
gwy_grapher_model_grapher_destroyed(GwyGrapher *grapher,
                                  GwyGrapherModel *gmodel)
{
    gwy_debug("");
    gwy_grapher_model_save_grapher(gmodel, grapher);
    g_signal_handler_disconnect(gmodel->grapher, gmodel->grapher_destroy_hid);
    gmodel->grapher_destroy_hid = 0;
    gmodel->grapher = NULL;
}

void
gwy_grapher_model_remove_all_curves(GwyGrapherModel *gmodel)
{
    gint i;

    
    for (i = 0; i < gmodel->ncurves; i++)
        g_object_unref(gmodel->curves[i]);
    g_free(gmodel->curves);    

    
    gmodel->ncurves = 0;
    
    g_object_notify(gmodel, "n");
    
}

/* actually copy save from a -- usually just dying -- grapher */
static void
gwy_grapher_model_save_grapher(GwyGrapherModel *gmodel,
                           GwyGrapher *grapher)
{
    /*
    gint i, nacurves;
    GwyGrapherCurveModel *gcmodel;

    gwy_debug("");
    g_assert(grapher && grapher == gmodel->grapher);

     * some their internals anyway. */
    /* grapher */
    /*
    if ((gmodel->has_x_unit = grapher->has_x_unit))
        gwy_si_unit_set_unit_string(GWY_SI_UNIT(gmodel->x_unit),
                                    grapher->x_unit);
    else
        gwy_object_unref(grapher->x_unit);

    if ((gmodel->has_y_unit = grapher->has_y_unit))
        gwy_si_unit_set_unit_string(GWY_SI_UNIT(gmodel->y_unit),
                                    grapher->y_unit);
    else
        gwy_object_unref(grapher->y_unit);

    gmodel->x_reqmin = grapher->x_reqmin;
    gmodel->y_reqmin = grapher->y_reqmin;
    gmodel->x_reqmax = grapher->x_reqmax;
    gmodel->y_reqmax = grapher->y_reqmax;

    g_string_assign(gmodel->top_label,
                    gwy_axiser_get_label(grapher->axis_top)->str);
    g_string_assign(gmodel->bottom_label,
                    gwy_axiser_get_label(grapher->axis_bottom)->str);
    g_string_assign(gmodel->left_label,
                    gwy_axiser_get_label(grapher->axis_left)->str);
    g_string_assign(gmodel->right_label,
                    gwy_axiser_get_label(grapher->axis_right)->str);

    gmodel->label_position = grapher->area->lab->par.position;
    gmodel->label_has_frame = grapher->area->lab->par.is_frame;
    gmodel->label_frame_thickness = grapher->area->lab->par.frame_thickness;

     * 1. clear extra curves that model has and grapher has not
     * 2. realloc curves to the right size
     * 3. replace already existing curves  <-- if lucky, only this happens
     * 4. fill new curves
     */
    /*
    nacurves = grapher->area->curves->len;
    for (i = nacurves; i < gmodel->ncurves; i++)
        gwy_object_unref(gmodel->curves[i]);
    gmodel->curves = g_renew(GObject*, gmodel->curves, nacurves);
    for (i = 0; i < gmodel->ncurves; i++) {
        gcmodel = GWY_GRAPHER_CURVE_MODEL(gmodel->curves[i]);
        gwy_grapher_curve_model_save_curve(gcmodel, grapher, i);
    }*/
    /* 4. fill */
    /*
    for (i = gmodel->ncurves; i < nacurves; i++) {
        gmodel->curves[i] = gwy_grapher_curve_model_new();
        gcmodel = GWY_GRAPHER_CURVE_MODEL(gmodel->curves[i]);
        gwy_grapher_curve_model_save_curve(gcmodel, grapher, i);
    }
    gmodel->ncurves = nacurves;
    */
}

GtkWidget*
gwy_grapher_new_from_model(GwyGrapherModel *gmodel)
{
    GtkWidget *grapher_widget;
    GwyGrapherCurveModel *gcmodel;
    gchar *BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS;
    GwyGrapher *grapher;
    gint i;

    /*
    g_return_val_if_fail(gmodel->grapher == NULL, gwy_grapher_new(gmodel));

    grapher_widget = gwy_grapher_new(gmodel);
    grapher = GWY_GRAPHER(grapher_widget);
*/
    /*
    gmodel->grapher = grapher;
    gmodel->grapher_destroy_hid
        = g_signal_connect(grapher, "destroy",
                           G_CALLBACK(gwy_grapher_model_grapher_destroyed), gmodel);

    grapher->area->lab->par.position = gmodel->label_position;
    grapher->area->lab->par.is_frame = gmodel->label_has_frame;
    grapher->area->lab->par.frame_thickness = gmodel->label_frame_thickness;

    for (i = 0; i < gmodel->ncurves; i++) {
        gcmodel = GWY_GRAPHER_CURVE_MODEL(gmodel->curves[i]);
        gwy_grapher_add_curve_from_model(grapher, gcmodel);
    }

    gwy_axiser_set_label(grapher->axis_top, gmodel->top_label);
    gwy_axiser_set_label(grapher->axis_bottom, gmodel->bottom_label);
    gwy_axiser_set_label(grapher->axis_left, gmodel->left_label);
    gwy_axiser_set_label(grapher->axis_right, gmodel->right_label);
    if (gmodel->has_x_unit) {
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gmodel->x_unit));
        gwy_axiser_set_unit(grapher->axis_top,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = g_strdup(BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        gwy_axiser_set_unit(grapher->axis_bottom,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
    }
    if (gmodel->has_y_unit) {
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = gwy_si_unit_get_unit_string(GWY_SI_UNIT(gmodel->y_unit));
        gwy_axiser_set_unit(grapher->axis_left,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS
            = g_strdup(BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
        gwy_axiser_set_unit(grapher->axis_right,
                          BRAINDEAD_SI_UNIT_CANT_RETURN_CONSTANT_STRINGS);
    }

    gwy_grapher_set_boundaries(grapher,
                             gmodel->x_reqmin, gmodel->x_reqmax,
                             gmodel->y_reqmin, gmodel->y_reqmax);

                             */
    return grapher_widget;
}

static GByteArray*
gwy_grapher_model_serialize(GObject *obj,
                          GByteArray*buffer)
{
    GwyGrapherModel *gmodel;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPHER_MODEL(obj), NULL);

    gmodel = GWY_GRAPHER_MODEL(obj);
    if (gmodel->grapher)
        gwy_grapher_model_save_grapher(gmodel, gmodel->grapher);
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
                                                GWY_GRAPHER_MODEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_grapher_model_deserialize(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    GwyGrapherModel *gmodel;

    g_return_val_if_fail(buffer, NULL);

    gmodel = (GwyGrapherModel*)gwy_grapher_model_new(NULL);
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
                                                GWY_GRAPHER_MODEL_TYPE_NAME,
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
gwy_grapher_model_duplicate(GObject *object)
{
    GwyGrapherModel *gmodel, *duplicate;
    gint i;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPHER_MODEL(object), NULL);

    gmodel = GWY_GRAPHER_MODEL(object);
    if (gmodel->grapher)
        return gwy_grapher_model_new(gmodel->grapher);

    duplicate = (GwyGrapherModel*)gwy_grapher_model_new(NULL);
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
    duplicate->x_unit = gwy_serializable_duplicate(gmodel->x_unit);
    duplicate->y_unit = gwy_serializable_duplicate(gmodel->y_unit);
    duplicate->top_label = g_string_new(gmodel->top_label->str);
    duplicate->bottom_label = g_string_new(gmodel->bottom_label->str);
    duplicate->left_label = g_string_new(gmodel->left_label->str);
    duplicate->right_label = g_string_new(gmodel->right_label->str);
    duplicate->ncurves = gmodel->ncurves;
    duplicate->curves = g_new(GObject*, gmodel->ncurves);
    for (i = 0; i < gmodel->ncurves; i++)
        duplicate->curves[i] = gwy_serializable_duplicate(gmodel->curves[i]);

    return (GObject*)duplicate;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
