/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek, Martin Siler.
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

#ifndef __GWY_GWY3DVIEW_H__
#define __GWY_GWY3DVIEW_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkdrawingarea.h>

#include <GL/gl.h>
#include <gtk/gtkgl.h>

#include <libprocess/datafield.h>
#include <libdraw/gwypalette.h>

#include "gwyglmat.h"

G_BEGIN_DECLS

#define GWY_TYPE_3D_VIEW              (gwy_3d_view_get_type())
#define GWY_3D_VIEW(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_3D_VIEW, Gwy3DView))
#define GWY_3D_VIEW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_3D_VIEW, Gwy3DViewClass))
#define GWY_IS_3D_VIEW(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_3D_VIEW))
#define GWY_IS_3D_VIEW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_3D_VIEW))
#define GWY_3D_VIEW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_3D_VIEW, Gwy3DViewClass))

typedef struct _Gwy3DView      Gwy3DView;
typedef struct _Gwy3DViewClass Gwy3DViewClass;

typedef enum {
	GWY_3D_ROTATION      = 1,
	GWY_3D_SCALE         = 2,
	GWY_3D_DEFORMATION   = 3,
        GWY_3D_LIGHTMOVEMENT = 4
}  Gwy3DMovement;


struct _Gwy3DView {
    GtkDrawingArea drawing_area;

    GwyContainer * container;       /* Container with data */
    GwyDataField * data;            /* Data to be shown */
    GwyDataField * downsampled;     /* Downsampled data for faster rendering */
    GwyPalette   * palette;         /* Color palette of heights (if lights are off) */

    gdouble data_min;               /* minimal z-value of the heights */
    gdouble data_max;               /* maximal z-value od the heights */
    gdouble data_mean;              /* mean z-value od the heights */

    Gwy3DMovement movement_status;  /* What to do, if mouse is moving */

    GLint shape_list_base;          /* Base index of scene display lists */
    GLint font_list_base;           /* Base index of font display lists */
    gint font_height;               /* Font height in pixels */
    GLuint shape_current;           /* Actually shown shape in the scene (full or reduced data) */


    guint reduced_size;             /* Resolution of the surface while rotations etc. */

    GLfloat rot_x;                  /* First angle of ratation of the scene */
    GLfloat rot_y;                  /* Second angle of ratation of the scene */
    GLfloat view_scale_max;         /* Maximum zoom of the scene */
    GLfloat view_scale_min;         /* Minimum zoom of the scene */
    GLfloat view_scale;             /* Actual zoom*/
    GLfloat deformation_z;          /* Deformation of the z axis within the scene */
    GLfloat light_z;                /* First angle describing position of light */
    GLfloat light_y;                /* Second angle describing position of light */

    gboolean orthogonal_projection; /* Whether use orthographic or perspectine projection */
    gboolean show_axes;             /* Whether show axes wihin the scene */
    gboolean show_labels;           /* Whwther show axes labels, only if axes are shown */
    gboolean enable_lights;         /* Enable lightning */

    GwyGLMaterialProp * mat_current;/* Current material (influences the color of the object, lights must be on) */

    gpointer reserved1;
};

struct _Gwy3DViewClass {
    GtkDrawingAreaClass parent_class;

    GdkGLConfig * glconfig;
    gpointer reserved1;
};

GtkWidget*       gwy_3d_view_new               (GwyContainer * container);
GType            gwy_3d_view_get_type          (void) G_GNUC_CONST;

void             gwy_3d_view_update            (Gwy3DView *gwy3dwiew);

GwyPalette*      gwy_3d_view_get_palette       (Gwy3DView *gwy3dwiew);
void             gwy_3d_view_set_palette       (Gwy3DView *gwy3dwiew,
                                                    GwyPalette *palette);

Gwy3DMovement    gwy_3d_view_get_status        (Gwy3DView * gwy3dwiew);
void             gwy_3d_view_set_status        (Gwy3DView * gwy3dwiew,
                                                    Gwy3DMovement mv);

gboolean         gwy_3d_view_get_orthographic  (Gwy3DView *gwy3dwiew);
void             gwy_3d_view_set_orthographic  (Gwy3DView *gwy3dwiew,
                                                    gboolean  orthographic);
gboolean         gwy_3d_view_get_show_axes     (Gwy3DView *gwy3dwiew);
void             gwy_3d_view_set_show_axes     (Gwy3DView *gwy3dwiew,
                                                    gboolean  show_axes);
gboolean         gwy_3d_view_get_show_labels   (Gwy3DView *gwy3dwiew);
void             gwy_3d_view_set_show_labels   (Gwy3DView *gwy3dwiew,
                                                    gboolean  show_labels);

guint            gwy_3d_view_get_reduced_size  (Gwy3DView *gwy3dwiew);
void             gwy_3d_view_set_reduced_size  (Gwy3DView *gwy3dwiew,
                                                    guint  reduced_size);

GwyGLMaterialProp*   gwy_3d_view_get_material  (Gwy3DView *gwy3dwiew);
void                 gwy_3d_view_set_material  (Gwy3DView *gwy3dwiew,
                                                  GwyGLMaterialProp *material);

GdkPixbuf *      gwy_3d_view_get_pixbuf        (Gwy3DView *gwy3dwiew,
                                                  guint xres,
                                                  guint yres);

void             gwy_3d_view_reset_view        (Gwy3DView *gwy3Dview);

G_END_DECLS

#endif  /* gwy3Dwidget.h */
