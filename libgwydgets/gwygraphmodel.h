/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_GRAPH_MODEL_H__
#define __GWY_GRAPH_MODEL_H__

#include <glib-object.h>
#include <gtk/gtkenums.h>

#include <libgwydgets/gwygraphcurvemodel.h>
#include <libgwyddion/gwysiunit.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH_MODEL                  (gwy_graph_model_get_type())
#define GWY_GRAPH_MODEL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_MODEL, GwyGraphModel))
#define GWY_GRAPH_MODEL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_MODEL, GwyGraphModelClass))
#define GWY_IS_GRAPH_MODEL(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_MODEL))
#define GWY_IS_GRAPH_MODEL_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_MODEL))
#define GWY_GRAPH_MODEL_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_MODEL, GwyGraphModelClass))


typedef struct _GwyGraphModel GwyGraphModel;
typedef struct _GwyGraphModelClass GwyGraphModelClass;

struct _GwyGraphModel {
    GObject parent_instance;

    GPtrArray *curves;
    GArray *curveaux;

    GString *title;
    GwyGraphGridType grid_type;

    gdouble x_min;
    gdouble x_max;
    gdouble y_min;
    gdouble y_max;

    gboolean x_min_set;
    gboolean x_max_set;
    gboolean y_min_set;
    gboolean y_max_set;

    GwySIUnit *x_unit;
    GwySIUnit *y_unit;

    GString *top_label;
    GString *bottom_label;
    GString *left_label;
    GString *right_label;

    /* like GwyGraphLabelParams */
    GwyGraphLabelPosition label_position;
    gboolean label_has_frame;
    gint label_frame_thickness;
    gboolean label_reverse;
    gboolean label_visible;

    /* logarithmic axes */
    gboolean x_is_logarithmic;
    gboolean y_is_logarithmic;

    /* reserved stuff */
    gint int1;
    gint int2;
    GwyGraphPointType enum1;
    GwyGraphPointType enum2;
    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

struct _GwyGraphModelClass {
    GObjectClass parent_class;

    void (*curve_data_changed)(GwyGraphModel *model,
                               gint i);
    void (*curve_notify)(GwyGraphModel *model,
                         gint i,
                         GParamSpec *pspec);

    void (*reserved1)(void);
    void (*reserved2)(void);
    void (*reserved3)(void);
    void (*reserved4)(void);
};

#define gwy_graph_model_duplicate(gmodel) \
        (GWY_GRAPH_MODEL(gwy_serializable_duplicate(G_OBJECT(gmodel))))

GType          gwy_graph_model_get_type                 (void) G_GNUC_CONST;
GwyGraphModel* gwy_graph_model_new                      (void);
GwyGraphModel* gwy_graph_model_new_alike                (GwyGraphModel *gmodel);
gint           gwy_graph_model_get_n_curves             (GwyGraphModel *gmodel);
gint           gwy_graph_model_add_curve                (GwyGraphModel *gmodel,
                                                         GwyGraphCurveModel *curve);
gint           gwy_graph_model_remove_curve_by_description(GwyGraphModel *gmodel,
                                                         const gchar *description);
void           gwy_graph_model_remove_curve             (GwyGraphModel *gmodel,
                                                         gint cindex);
GwyGraphCurveModel* gwy_graph_model_get_curve_by_description(GwyGraphModel *gmodel,
                                                             const gchar *description);
GwyGraphCurveModel* gwy_graph_model_get_curve           (GwyGraphModel *gmodel,
                                                         gint cindex);
gint           gwy_graph_model_get_curve_index          (GwyGraphModel *gmodel,
                                                         GwyGraphCurveModel *curve);
void           gwy_graph_model_remove_all_curves        (GwyGraphModel *gmodel);
void           gwy_graph_model_set_units_from_data_line(GwyGraphModel *model,
                                                        GwyDataLine *data_line);
gboolean       gwy_graph_model_x_data_can_be_logarithmed(GwyGraphModel *model);
gboolean       gwy_graph_model_y_data_can_be_logarithmed(GwyGraphModel *model);
void           gwy_graph_model_set_axis_label           (GwyGraphModel *model,
                                                         GtkPositionType pos,
                                                         const gchar *label);
const gchar*   gwy_graph_model_get_axis_label           (GwyGraphModel *model,
                                                         GtkPositionType pos);
gboolean       gwy_graph_model_get_x_range              (GwyGraphModel *gmodel,
                                                         gdouble *x_min,
                                                         gdouble *x_max);
gboolean       gwy_graph_model_get_y_range              (GwyGraphModel *gmodel,
                                                         gdouble *y_min,
                                                         gdouble *y_max);
GString*       gwy_graph_model_export_ascii             (GwyGraphModel *model,
                                                         gboolean export_units,
                                                         gboolean export_labels,
                                                         gboolean export_metadata,
                                                         GwyGraphModelExportStyle export_style,
                                                         GString *string);

G_END_DECLS

#endif /* __GWY_GRAPH_MODEL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
