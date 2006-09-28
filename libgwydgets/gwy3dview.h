/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
#include <libgwydgets/gwy3dsetup.h>

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

    GwyContainer *data;           /* Container with data */
    guint reduced_size;
    GwyDataField *downsampled;    /* Downsampled data for faster rendering */
    GwyDataField *downsampled2;

    Gwy3DSetup *setup;
    GQuark setup_key;
    gulong setup_id;
    gulong setup_item_id;

    GwyDataField *data_field;
    GQuark data_key;
    gulong data_id;
    gulong data_item_id;

    /* Unused, intended for future color !~ height visualization */
    GwyDataField *data2_field;
    GQuark data2_key;
    gulong data2_id;
    gulong data2_item_id;

    GwyGradient *gradient;
    GQuark gradient_key;
    gulong gradient_id;
    gulong gradient_item_id;

    GwyGLMaterial *material;
    GQuark material_key;
    gulong material_id;
    gulong material_item_id;

    Gwy3DLabel **labels;          /* labels text, displacement etc */
    gulong *label_ids;
    gulong *label_item_ids;
    GHashTable *variables;        /* Label substitution variables */

    gint changed;                 /* What has changed, XXX: not used */

    gint shape_list_base;         /* Base index of scene display lists */
    guint shape_current;          /* Actually shown shape in the scene
                                     (full or reduced data) */

    gdouble view_scale_max;       /* Maximum zoom of the scene */
    gdouble view_scale_min;       /* Minimum zoom of the scene */

    gdouble mouse_begin_x;        /* Start x-coordinate of mouse */
    gdouble mouse_begin_y;        /* Start y-coordinate of mouse */

    guint timeout_id;             /* Full size redraw timeout id */

    PangoContext *ft2_context;    /* For text rendering */
    PangoFontMap *ft2_font_map;   /* Font map for text rendering */

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

    void (*reserved1)(void);
    void (*reserved2)(void);
    void (*reserved3)(void);
    void (*reserved4)(void);
    void (*reserved5)(void);
    void (*reserved6)(void);
};

GtkWidget*        gwy_3d_view_new               (GwyContainer *data);
GType             gwy_3d_view_get_type          (void) G_GNUC_CONST;

const gchar*      gwy_3d_view_get_setup_prefix  (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_setup_prefix  (Gwy3DView *gwy3dview,
                                                 const gchar *key);
const gchar*      gwy_3d_view_get_data_key      (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_data_key      (Gwy3DView *gwy3dview,
                                                 const gchar *key);
const gchar*      gwy_3d_view_get_gradient_key  (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_gradient_key  (Gwy3DView *gwy3dview,
                                                 const gchar *key);
const gchar*      gwy_3d_view_get_material_key  (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_material_key  (Gwy3DView *gwy3dview,
                                                 const gchar *key);

guint             gwy_3d_view_get_reduced_size  (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_reduced_size  (Gwy3DView *gwy3dview,
                                                 guint reduced_size);
Gwy3DMovement     gwy_3d_view_get_movement_type (Gwy3DView *gwy3dview);
void              gwy_3d_view_set_movement_type (Gwy3DView *gwy3dview,
                                                 Gwy3DMovement movement);

GdkPixbuf*        gwy_3d_view_get_pixbuf        (Gwy3DView *gwy3dview);
Gwy3DLabel*       gwy_3d_view_get_label         (Gwy3DView *gwy3dview,
                                                 Gwy3DViewLabel label);
Gwy3DSetup*       gwy_3d_view_get_setup         (Gwy3DView *gwy3dview);
GwyContainer*     gwy_3d_view_get_data          (Gwy3DView *gwy3dview);

void              gwy_3d_view_get_scale_range   (Gwy3DView *gwy3dview,
                                                 gdouble *min_scale,
                                                 gdouble *max_scale);
void              gwy_3d_view_set_scale_range   (Gwy3DView *gwy3dview,
                                                 gdouble min_scale,
                                                 gdouble max_scale);

G_END_DECLS

#endif  /* __GWY_3D_VIEW_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
