/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2005 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *  Copyright (C) 2004 Martin Siler.
 *  E-mail: silerm@physics.muni.cz.
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

#ifndef __GWY_3D_VIEW_H__
#define __GWY_3D_VIEW_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>

#include <libprocess/datafield.h>
#include <libdraw/gwygradient.h>
#include <libdraw/gwyglmaterial.h>

#include <libgwydgets/gwydgetenums.h>
#include <libgwydgets/gwy3dlabel.h>

G_BEGIN_DECLS

#define GWY_TYPE_3D_VIEW              (gwy_3d_view_get_type())
#define GWY_3D_VIEW(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_3D_VIEW, Gwy3DView))
#define GWY_3D_VIEW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_3D_VIEW, Gwy3DViewClass))
#define GWY_IS_3D_VIEW(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_3D_VIEW))
#define GWY_IS_3D_VIEW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_3D_VIEW))
#define GWY_3D_VIEW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_3D_VIEW, Gwy3DViewClass))

typedef struct _Gwy3DView      Gwy3DView;
typedef struct _Gwy3DViewClass Gwy3DViewClass;

struct _Gwy3DView {
    GtkWidget parent_instance;

    Gwy3DMovement movement;
    Gwy3DProjection projection;
    Gwy3DVisualization visual;
    gboolean show_axes;
    gboolean show_labels;
    guint reduced_size;

    GwyContainer *data;           /* Container with data */
    GwyDataField *data_field;     /* Data to be shown */
    GwyDataField *downsampled;    /* Downsampled data for faster rendering */

    GwyGradient *gradient;
    GQuark gradient_key;
    gulong gradient_id;
    gulong gradient_item_id;

    GwyGLMaterial *material;
    GQuark material_key;
    gulong material_id;
    gulong material_item_id;

    gint changed;

    gint shape_list_base;         /* Base index of scene display lists */
    guint shape_current;          /* Actually shown shape in the scene
                                     (full or reduced data) */

    GtkAdjustment *rot_x;         /* First angle of ratation of the scene */
    GtkAdjustment *rot_y;         /* Second angle of ratation of the scene */
    GtkAdjustment *view_scale;    /* Actual zoom*/
    GtkAdjustment *deformation_z; /* Deformation of the z axis within the
                                     scene */
    GtkAdjustment *light_z;       /* First angle describing position of light */
    GtkAdjustment *light_y;       /* Second angle describing position of
                                     light */
    gdouble view_scale_max;       /* Maximum zoom of the scene */
    gdouble view_scale_min;       /* Minimum zoom of the scene */

    gdouble mouse_begin_x;        /* Start x-coordinate of mouse */
    gdouble mouse_begin_y;        /* Start y-coordinate of mouse */

    gboolean timeout;             /* Is running timeout for redrawing in full
                                     scale */
    guint timeout_id;             /* Timeout id */

    PangoContext *ft2_context;    /* For text rendering */
    PangoFontMap *ft2_font_map;   /* Font map for text rendering */
    Gwy3DLabel **labels;          /* labels text, displacement etc */
    gulong      *label_signal_ids;
    GHashTable *variables;        /* Label substitution variables */

    gboolean b_reserved1;
    gboolean b_reserved2;
    gboolean b_reserved3;
    gboolean b_reserved4;

    gint     i_reserved1;
    gint     i_reserved2;

    gpointer p_reserved2;
    gpointer p_reserved3;
    gpointer p_reserved4;
};

struct _Gwy3DViewClass {
    GtkWidgetClass parent_class;

    gpointer list_pool;

    gpointer reserved1;             /* reserved for future use (signals) */
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

GtkWidget*        gwy_3d_view_new               (GwyContainer *data);
GType             gwy_3d_view_get_type          (void) G_GNUC_CONST;

const gchar*      gwy_3d_view_get_gradient_key  (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_gradient_key  (Gwy3DView *gwy3dview,
                                                 const gchar *key);
const gchar*      gwy_3d_view_get_material_key  (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_material_key  (Gwy3DView *gwy3dview,
                                                 const gchar *key);

Gwy3DMovement     gwy_3d_view_get_movement_type (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_movement_type (Gwy3DView *gwy3dview,
                                                 Gwy3DMovement movement);

Gwy3DProjection   gwy_3d_view_get_projection    (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_projection    (Gwy3DView *gwy3dview,
                                                 Gwy3DProjection projection);
gboolean          gwy_3d_view_get_show_axes     (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_show_axes     (Gwy3DView *gwy3dview,
                                                 gboolean show_axes);
gboolean          gwy_3d_view_get_show_labels   (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_show_labels   (Gwy3DView *gwy3dview,
                                                 gboolean show_labels);

Gwy3DVisualization gwy_3d_view_get_visualization(Gwy3DView *gwy3dview);
void              gwy_3d_view_set_visualization (Gwy3DView *gwy3dview,
                                                 Gwy3DVisualization visual);

guint             gwy_3d_view_get_reduced_size  (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_reduced_size  (Gwy3DView *gwy3dview,
                                                 guint reduced_size);

GdkPixbuf*        gwy_3d_view_get_pixbuf        (Gwy3DView *gwy3dview);
Gwy3DLabel*       gwy_3d_view_get_label         (Gwy3DView *gwy3dview,
                                                 Gwy3DViewLabel label);
GwyContainer*     gwy_3d_view_get_data          (Gwy3DView *gwy3dview);
void              gwy_3d_view_reset_view        (Gwy3DView *gwy3dview);

GtkAdjustment* gwy_3d_view_get_rot_x_adjustment         (Gwy3DView *gwy3dview);
GtkAdjustment* gwy_3d_view_get_rot_y_adjustment         (Gwy3DView *gwy3dview);
GtkAdjustment* gwy_3d_view_get_view_scale_adjustment    (Gwy3DView *gwy3dview);
GtkAdjustment* gwy_3d_view_get_z_deformation_adjustment (Gwy3DView *gwy3dview);
GtkAdjustment* gwy_3d_view_get_light_z_adjustment       (Gwy3DView *gwy3dview);
GtkAdjustment* gwy_3d_view_get_light_y_adjustment       (Gwy3DView *gwy3dview);

gdouble           gwy_3d_view_get_max_view_scale(Gwy3DView *gwy3dview);
gdouble           gwy_3d_view_get_min_view_scale(Gwy3DView *gwy3dview);
void              gwy_3d_view_set_max_view_scale(Gwy3DView *gwy3dview,
                                                 gdouble new_max_scale);
void              gwy_3d_view_set_min_view_scale(Gwy3DView *gwy3dview,
                                                 gdouble new_min_scale);

G_END_DECLS

#endif  /* __GWY_3D_VIEW_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
