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

/* XXX: really NEVER access these fields directly. They don't have to reflect
 * the graph values! */
struct _GwyGraphModel {
    GObject parent_instance;

    gint ncurves;
    GObject **curves;

    GString *title;   

    /*these values reflect reasonable bounding values of all the curves. They can
     be set for example during curve adding to graph. They can differ sligthly
     depending on axis mode (eg. logarithmic)*/
    gdouble x_max;
    gdouble x_min;
    gdouble y_max;
    gdouble y_min;

    gboolean has_x_unit;
    gboolean has_y_unit;
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

    /*logarithmic axis*/
    gboolean x_is_logarithmic;
    gboolean y_is_logarithmic;
            
    
    /* reserved stuff */
    gint int1;
    gint int2;
    GwyGraphPointType enum1;
    GwyGraphPointType enum2;
    gboolean label_reverse;
    gboolean label_visible;
    gpointer reserved3;
    gpointer reserved4;
};

struct _GwyGraphModelClass {
    GObjectClass parent_class;

    void (*layout_updated)(GwyGraphModel *gmodel);

    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
    gpointer reserved5;
};

#define gwy_graph_model_duplicate(gmodel) \
        (GWY_GRAPH_MODEL(gwy_serializable_duplicate(G_OBJECT(gmodel))))

GType          gwy_graph_model_get_type                 (void) G_GNUC_CONST;
GwyGraphModel* gwy_graph_model_new                      (void);
GwyGraphModel* gwy_graph_model_new_alike                (GwyGraphModel *gmodel);
gint           gwy_graph_model_get_n_curves             (GwyGraphModel *gmodel);
void           gwy_graph_model_add_curve                (GwyGraphModel *gmodel,
                                                         GwyGraphCurveModel *curve);
void         gwy_graph_model_remove_curve_by_description(GwyGraphModel *gmodel,
                                                         gchar *description);
void           gwy_graph_model_remove_curve_by_index    (GwyGraphModel *gmodel,
                                                         gint cindex);
GwyGraphCurveModel* gwy_graph_model_get_curve_by_description(GwyGraphModel *gmodel,
                                                             gchar *description);
GwyGraphCurveModel* gwy_graph_model_get_curve_by_index  (GwyGraphModel *gmodel,
                                                         gint cindex);
void           gwy_graph_model_remove_all_curves        (GwyGraphModel *gmodel);
void           gwy_graph_model_set_title                (GwyGraphModel *model,
                                                         gchar *title);
void           gwy_graph_model_set_label_position       (GwyGraphModel *model,
                                                         GwyGraphLabelPosition position);
void           gwy_graph_model_set_label_has_frame      (GwyGraphModel *model,
                                                         gboolean label_has_frame);
void           gwy_graph_model_set_label_frame_thickness(GwyGraphModel *model,
                                                         gint thickness);
void           gwy_graph_model_set_label_reverse        (GwyGraphModel *model,
                                                         gboolean reverse);
void           gwy_graph_model_set_label_visible        (GwyGraphModel *model,
                                                         gboolean visible);
void           gwy_graph_model_set_x_siunit             (GwyGraphModel *model,
                                                         GwySIUnit *siunit);
void           gwy_graph_model_set_y_siunit             (GwyGraphModel *model,
                                                         GwySIUnit *siunit);
gchar*         gwy_graph_model_get_title                (GwyGraphModel *model);
GwyGraphLabelPosition gwy_graph_model_get_label_position(GwyGraphModel *model);
gboolean       gwy_graph_model_get_label_has_frame      (GwyGraphModel *model);
gint           gwy_graph_model_get_label_frame_thickness(GwyGraphModel *model);
gboolean       gwy_graph_model_get_label_reverse        (GwyGraphModel *model);
gboolean       gwy_graph_model_get_label_visible        (GwyGraphModel *model);
GwySIUnit*     gwy_graph_model_get_x_siunit             (GwyGraphModel *model);
GwySIUnit*     gwy_graph_model_get_y_siunit             (GwyGraphModel *model);
void           gwy_graph_model_export_ascii             (GwyGraphModel *model,
                                                         const gchar *filename,
                                                         gboolean export_units,
                                                         gboolean export_labels,
                                                         gboolean export_metadata,
                                                         GwyGraphModelExportStyle export_style);

void           gwy_graph_model_signal_layout_changed    (GwyGraphModel *model);

void           gwy_graph_model_set_direction_logarithmic(GwyGraphModel *model,
                                                         GtkOrientation direction,
                                                         gboolean is_logarithmic);
gboolean       gwy_graph_model_get_direction_logarithmic(GwyGraphModel *model,
                                                         GtkOrientation direction);
gboolean       gwy_graph_model_x_data_can_be_logarithmed(GwyGraphModel *model);
gboolean       gwy_graph_model_y_data_can_be_logarithmed(GwyGraphModel *model);

G_END_DECLS

#endif /* __GWY_GRAPH_MODEL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
