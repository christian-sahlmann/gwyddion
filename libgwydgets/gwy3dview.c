
#include <gdk/gdkkeysyms.h>



/*//////////////////////////////////////////////////////////////////////////////////*/
/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, silerm@... .
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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkevents.h>
#include <glib-object.h>
#include <gtk/gtkgl.h>

#include <libgwyddion/gwymacros.h>
#include "gwy3dview.h"

#define DIG_2_RAD (G_PI / 180.0)
#define RAD_2_DIG (180.0 / G_PI)

#define GWY_3D_VIEW_TYPE_NAME "Gwy3DView"

#define BITS_PER_SAMPLE 8
#define GWY_3D_VIEW_DEFAULT_SIZE_X 200
#define GWY_3D_VIEW_DEFAULT_SIZE_Y 200
#define GWY_3D_SHAPE_AFM     0
#define GWY_3D_SHAPE_REDUCED 1

#define  GWY_3D_ORTHO_CORRECTION   2.0
#define  GWY_3D_Z_DEFORMATION     1.01
#define  GWY_3D_Z_TRANSFORMATION  0.5
#define  GWY_3D_Z_DISPLACEMENT    -0.2

typedef struct { GLfloat x, y, z; } Gwy3DVector;

/* Forward declarations */

static void          gwy_3d_view_class_init     (Gwy3DViewClass *klass);
static void          gwy_3d_view_init           (Gwy3DView *gwy3dview);
static void          gwy_3d_view_finalize       (GObject *object);
static void          gwy_3d_view_realize        (GtkWidget *widget);
static void          gwy_3d_view_unrealize      (GtkWidget *widget);
static void          gwy_3d_view_realize_gl     (Gwy3DView *widget);
static void          gwy_3d_view_size_request   (GtkWidget *widget,
                                                 GtkRequisition *requisition);
static gboolean      gwy_3d_view_expose         (GtkWidget *widget,
                                                 GdkEventExpose *event);
static gboolean      gwy_3d_view_configure      (GtkWidget         *widget,
                                                 GdkEventConfigure *event);
static gboolean      gwy_3d_view_button_press   (GtkWidget *widget,
                                                 GdkEventButton *event);
static gboolean      gwy_3d_view_button_release (GtkWidget *widget,
                                                 GdkEventButton *event);
static gboolean      gwy_3d_view_motion_notify  (GtkWidget *widget,
                                                 GdkEventMotion *event);
static Gwy3DVector * gwy_3d_make_normals        (GwyDataField * data,
                                                 Gwy3DVector *normals);
static void          gwy_3d_make_list           (Gwy3DView * gwy3D,
                                                 GwyDataField *data,
                                                 gint shape);
static void          gwy_3d_draw_axes           (Gwy3DView *gwy3dview);
static void          gwy_3d_draw_light_position (Gwy3DView *gwy3dview);
static void          gwy_3d_init_font           (Gwy3DView *gwy3dview);
static void          gwy_3d_set_projection      (Gwy3DView *gwy3dview,
                                                 GLfloat width,
                                                 GLfloat height);


/* Local data */

static GtkDrawingAreaClass *parent_class = NULL;

static float begin_x = 0.0;
static float begin_y = 0.0;

#ifdef WIN32
static const gchar font_string[] = "arial 12";
#else
static const gchar font_string[] = "helvetica 12";
#endif

GType
gwy_3d_view_get_type(void)
{
    static GType gwy_3d_view_type = 0;

    if (!gwy_3d_view_type) {
        static const GTypeInfo gwy_3d_view_info = {
            sizeof(Gwy3DViewClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_3d_view_class_init,
            NULL,
            NULL,
            sizeof(Gwy3DView),
            0,
            (GInstanceInitFunc)gwy_3d_view_init,
            NULL,
        };
        gwy_debug("");
        gwy_3d_view_type = g_type_register_static(GTK_TYPE_DRAWING_AREA,
                                                      GWY_3D_VIEW_TYPE_NAME,
                                                      &gwy_3d_view_info,
                                                      0);
    }

    return gwy_3d_view_type;
}

static void
gwy_3d_view_class_init(Gwy3DViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_3d_view_finalize;
    widget_class->realize = gwy_3d_view_realize;
    widget_class->unrealize = gwy_3d_view_unrealize;
    widget_class->expose_event = gwy_3d_view_expose;
    widget_class->configure_event = gwy_3d_view_configure;
    widget_class->size_request = gwy_3d_view_size_request;
    widget_class->button_press_event = gwy_3d_view_button_press;
    widget_class->button_release_event = gwy_3d_view_button_release;
    widget_class->motion_notify_event = gwy_3d_view_motion_notify;

    /*
     * Configure OpenGL-capable visual.
     */

    /* Try double-buffered visual */
    klass->glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB
                                        | GDK_GL_MODE_DEPTH
                                        | GDK_GL_MODE_DOUBLE);
    if (klass->glconfig == NULL)
    {
        gwy_debug("*** Cannot find the double-buffered visual.\n");
        gwy_debug("*** Trying single-buffered visual.\n");

        /* Try single-buffered visual */
        klass->glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB
                                                     | GDK_GL_MODE_DEPTH);
        if (klass->glconfig == NULL)
        {
            gwy_debug("*** No appropriate OpenGL-capable visual found.\n");
            /*
              TODO: solve the situation without 3D driver in another way
            */
            exit(1);
        }
    }

}

static void
gwy_3d_view_init(Gwy3DView *gwy3dview)
{
    gwy_debug("");

    gwy3dview->container             = NULL;
    gwy3dview->data                  = NULL;
    gwy3dview->downsampled           = NULL;
    gwy3dview->palette               = NULL;
    gwy3dview->reduced_size          = 100;
    gwy3dview->data_min              = 0.0;
    gwy3dview->data_max              = 0.0;
    gwy3dview->data_mean             = 0.0;
    gwy3dview->rot_x                 = 45.0f;
    gwy3dview->rot_y                 = -45.0f;
    gwy3dview->view_scale_max        = 3.0f;
    gwy3dview->view_scale_min        = 0.5f;
    gwy3dview->view_scale            = 1.0f;
    gwy3dview->deformation_z         = 1.0f;
    gwy3dview->light_z               = 0.0f;
    gwy3dview->light_y               = 0.0f;
    gwy3dview->movement_status       = GWY_3D_ROTATION;
    gwy3dview->orthogonal_projection = TRUE;
    gwy3dview->show_axes             = TRUE;
    gwy3dview->show_labels           = TRUE;
    gwy3dview->enable_lights         = FALSE;
    gwy3dview->mat_current           = gwyGL_mat_none;
    gwy3dview->shape_list_base       = -1;
    gwy3dview->font_list_base        = -1;
    gwy3dview->font_height           = 0;
    gwy3dview->shape_current         = 0;
    gwy3dview->reserved1             = NULL;
}

/**
 * gwy_3d_view_new:
 * @container:
 *
 * Creates a new #Gwy3DView.
 *
 * @container cannot be %NULL. If data is %NULL function returns %NULL
 *
 *
 *
 *
 * Returns: The new OpenGL 3D widget as a #GtkWidget.
 **/
GtkWidget*
gwy_3d_view_new(GwyContainer *container)
{
    GtkWidget *widget;
    Gwy3DView *gwy3dview;
    Gwy3DViewClass * klass;
    const guchar * palette_name;

    gwy_debug("");

    if (container)
        g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);
    else
        return NULL;
    g_object_ref(G_OBJECT(container));

    gwy3dview = gtk_type_new(gwy_3d_view_get_type());
    widget = GTK_WIDGET(gwy3dview);

    gwy3dview->container = container;
    if (gwy_container_contains_by_name(container, "/0/data"))
    {
        gwy3dview->data = GWY_DATA_FIELD(
            gwy_container_get_object_by_name(container, "/0/data"));
        g_object_ref(G_OBJECT(gwy3dview->data));
    }

    if (gwy_container_contains_by_name(container, "/0/3d/palette"))
    {    palette_name =
             gwy_container_get_string_by_name(container, "/0/3d/palette");
         gwy_debug("A - %s", palette_name);
    }
    else if (gwy_container_contains_by_name(container, "/0/base/palette"))
    {
        palette_name =
            gwy_container_get_string_by_name(container, "/0/base/palette");
        gwy_debug("B - %s", palette_name);
        gwy_container_set_string_by_name(gwy3dview->container, "/0/3d/palette",
                                         g_strdup(palette_name));
    }
    else
    {
        palette_name =  g_strdup(GWY_PALETTE_YELLOW);
        gwy_debug("C - %s", palette_name);
        gwy_container_set_string_by_name(gwy3dview->container, "/0/3d/palette",
                                         g_strdup(palette_name));
    }
    gwy3dview->palette = gwy_palette_new(gwy_palette_def_new(palette_name));
    g_object_ref(G_OBJECT(gwy3dview->palette));

    if (gwy_container_contains_by_name(container, "/0/3d/reduced_size"))
        gwy3dview->reduced_size = (guint)(
            gwy_container_get_int32_by_name(container,
                                            "/0/3d/reduced_size"));
    if (gwy_container_contains_by_name(container, "/0/3d/rot_x"))
        gwy3dview->rot_x = (GLfloat)(
            gwy_container_get_double_by_name(container,
                                             "/0/3d/rot_x"));
    if (gwy_container_contains_by_name(container, "/0/3d/rot_y"))
        gwy3dview->rot_y = (GLfloat)(
            gwy_container_get_double_by_name(container,
                                             "/0/3d/rot_y"));
    if (gwy_container_contains_by_name(container, "/0/3d/view_scale_max"))
        gwy3dview->view_scale_max = (GLfloat)(
            gwy_container_get_double_by_name(container,
                                             "/0/3d/view_scale_max"));
    if (gwy_container_contains_by_name(container, "/0/3d/view_scale_min"))
        gwy3dview->view_scale_min = (GLfloat)(
            gwy_container_get_double_by_name(container,
                                             "/0/3d/view_scale_min"));
    if (gwy_container_contains_by_name(container, "/0/3d/view_scale"))
        gwy3dview->view_scale = (GLfloat)(
            gwy_container_get_double_by_name(container,
                                             "/0/3d/view_scale"));
    if (gwy_container_contains_by_name(container, "/0/3d/deformation_z"))
        gwy3dview->deformation_z = (GLfloat)(
            gwy_container_get_double_by_name(container,
                                             "/0/3d/deformation_z"));
    if (gwy_container_contains_by_name(container, "/0/3d/light_z"))
        gwy3dview->light_z = (GLfloat)(
            gwy_container_get_double_by_name(container,
                                             "/0/3d/light_z"));
    if (gwy_container_contains_by_name(container, "/0/3d/light_y"))
        gwy3dview->light_y = (GLfloat)(
            gwy_container_get_double_by_name(container,
                                             "/0/3d/light_y"));
    if (gwy_container_contains_by_name(container, "/0/3d/ortho"))
        gwy3dview->orthogonal_projection =
            gwy_container_get_boolean_by_name(container,
                                              "/0/3d/view_scale_max");
    if (gwy_container_contains_by_name(container, "/0/3d/show_axes"))
        gwy3dview->show_axes =
            gwy_container_get_boolean_by_name(container,
                                              "/0/3d/show_axes");
    if (gwy_container_contains_by_name(container, "/0/3d/show_labels"))
        gwy3dview->show_labels =
            gwy_container_get_boolean_by_name(container,
                                              "/0/3d/show_labels");
    if (gwy_container_contains_by_name(container, "/0/3d/enable_lights"))
        gwy3dview->enable_lights =
            gwy_container_get_boolean_by_name(container,
                                              "/0/3d/enable_lights");
    if (gwy_container_contains_by_name(container, "/0/3d/material"))
    {
        /* TODO: get material type, atfter reworking gwyglmat.h */
    }
    /*
    TODO: ?? connect the signals value changed of the pallete and the data_field
    */
    if (gwy3dview->data != NULL)
    {
        guint rx, ry;

        gwy3dview->downsampled =
            (GwyDataField*) gwy_data_field_new(
                                gwy_data_field_get_xres(gwy3dview->data),
                                gwy_data_field_get_yres(gwy3dview->data),
                                gwy_data_field_get_xreal(gwy3dview->data),
                                gwy_data_field_get_yreal(gwy3dview->data),
                                TRUE);
        g_object_ref(G_OBJECT(gwy3dview->downsampled));
        gwy_data_field_copy(gwy3dview->data, gwy3dview->downsampled);
        rx = gwy_data_field_get_xres(gwy3dview->downsampled);
        ry = gwy_data_field_get_yres(gwy3dview->downsampled);
        if (rx > ry)
        {
           ry = (guint)((gdouble)ry / (gdouble)rx
                   * (gdouble)gwy3dview->reduced_size);
           rx = gwy3dview->reduced_size;
        } else {
           rx = (guint)((gdouble)rx / (gdouble)ry
                   * (gdouble)gwy3dview->reduced_size);
           ry = gwy3dview->reduced_size;
        }
        gwy_data_field_resample(gwy3dview->downsampled,
                                rx,
                                ry,
                                GWY_INTERPOLATION_BILINEAR);


        gwy3dview->data_min  = gwy_data_field_get_min(gwy3dview->data);
        gwy3dview->data_max  = gwy_data_field_get_max(gwy3dview->data);
    }
    klass = GWY_3D_VIEW_GET_CLASS(gwy3dview);
    gtk_widget_set_gl_capability(GTK_WIDGET(gwy3dview),
                                 klass->glconfig,
                                 NULL,
                                 TRUE,
                                 GDK_GL_RGBA_TYPE);

    return widget;
}

static void
gwy_3d_view_finalize(GObject *object)
{
    Gwy3DView *gwy3dview;

    gwy_debug("finalizing a Gwy3DView (refcount = %u)",
              object->ref_count);

    g_return_if_fail(GWY_IS_3D_VIEW(object));

    gwy3dview = GWY_3D_VIEW(object);

    gwy_object_unref(gwy3dview->data);
    gwy_object_unref(gwy3dview->downsampled);
    gwy_object_unref(gwy3dview->container);

    if (gwy3dview->shape_list_base >= 0)
    {
        glDeleteLists(gwy3dview->shape_list_base, 2);
        gwy3dview->shape_list_base = -1;
    }
    if (gwy3dview->font_list_base >= 0)
    {
        glDeleteLists(gwy3dview->font_list_base, 128);
        gwy3dview->font_list_base = -1;
    }


    G_OBJECT_CLASS(parent_class)->finalize(object);

    gwy_debug("    ... (refcount = %u)", object->ref_count);
}

static void
gwy_3d_view_unrealize(GtkWidget *widget)
{
    Gwy3DView *gwy3dview;

    gwy3dview = GWY_3D_VIEW(widget);

    gwy_debug("");

    if (gwy3dview->shape_list_base >= 0)
    {
        glDeleteLists(gwy3dview->shape_list_base, 2);
        gwy3dview->shape_list_base = -1;
    }
    if (gwy3dview->font_list_base >= 0)
    {
        glDeleteLists(gwy3dview->font_list_base, 128);
        gwy3dview->font_list_base = -1;
    }

    /* TODO: save the current view settings into the contaier  */
    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}

void
gwy_3d_view_update(Gwy3DView *gwy3dview)
{
    gboolean update_data = FALSE;
    gboolean update_palette = FALSE;
    const gchar * palette_name = NULL;

    gwy_debug("");
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (gwy_container_contains_by_name(gwy3dview->container, "/0/data"))
    {
        GwyDataField  * data;

        data = GWY_DATA_FIELD(gwy_container_get_object_by_name(
                    gwy3dview->container, "/0/data"));
        if (data != gwy3dview->data)
        {
            g_object_unref(G_OBJECT(gwy3dview->data));
            gwy3dview->data = data;
            g_object_ref(G_OBJECT(gwy3dview->data));
            update_data = TRUE;
        }
    }

    if (gwy_container_contains_by_name(gwy3dview->container, "/0/3d/palette"))
        palette_name = gwy_container_get_string_by_name(
                            gwy3dview->container, "/0/3d/palette");
    else if (gwy_container_contains_by_name(gwy3dview->container,
                                            "/0/base/palette"))
    {
        palette_name = gwy_container_get_string_by_name(
                            gwy3dview->container, "/0/base/palette");
        gwy_container_set_string_by_name(gwy3dview->container, "/0/3d/palette",
                                         g_strdup(palette_name));
    }
    else
    {
        palette_name =  g_strdup(GWY_PALETTE_YELLOW);
        gwy_container_set_string_by_name(gwy3dview->container, "/0/3d/palette",
                                         g_strdup(palette_name));
    }
    if (strcmp(palette_name,
               gwy_palette_def_get_name(gwy_palette_get_palette_def(gwy3dview->palette)))
         == 0)
    {
        gwy_palette_set_by_name(gwy3dview->palette, palette_name);
        update_palette = TRUE;
    }

    if (update_data == TRUE)
    {   /* make the dowsampled preview */
        guint rx, ry;

        g_object_unref(G_OBJECT(gwy3dview->downsampled));
        gwy3dview->downsampled =
            (GwyDataField*) gwy_data_field_new(
                                gwy_data_field_get_xres(gwy3dview->data),
                                gwy_data_field_get_yres(gwy3dview->data),
                                gwy_data_field_get_xreal(gwy3dview->data),
                                gwy_data_field_get_yreal(gwy3dview->data),
                                TRUE);
        gwy_data_field_copy(gwy3dview->data, gwy3dview->downsampled);
        rx = gwy_data_field_get_xres(gwy3dview->downsampled);
        ry = gwy_data_field_get_yres(gwy3dview->downsampled);
        if (rx > ry)
        {
            ry = (guint)((gdouble)ry / (gdouble)rx * (gdouble)gwy3dview->reduced_size);
            rx = gwy3dview->reduced_size;
        } else {
            rx = (guint)((gdouble)rx / (gdouble)ry * (gdouble)gwy3dview->reduced_size);
            ry = gwy3dview->reduced_size;
        }
        gwy_data_field_resample(gwy3dview->downsampled,
                                rx,
                                ry,
                                GWY_INTERPOLATION_BILINEAR);

    }

    if ((update_data == TRUE || update_palette == TRUE)
         && GTK_WIDGET_REALIZED(gwy3dview))
    {
        /* FIXME: ?is it necessary to delete old dislpay lists? */
        gwy_3d_make_list(gwy3dview, gwy3dview->data, GWY_3D_SHAPE_AFM);
        gwy_3d_make_list(gwy3dview, gwy3dview->downsampled, GWY_3D_SHAPE_REDUCED);

        gdk_window_invalidate_rect(
            GTK_WIDGET(gwy3dview)->window,
                       &GTK_WIDGET(gwy3dview)->allocation, FALSE);
    }

}



GwyPalette*
gwy_3d_view_get_palette       (Gwy3DView *gwy3dview)
{
    gwy_debug("");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);

    return gwy3dview->palette;
}

/*
void
gwy_3d_view_set_palette(Gwy3DView *gwy3dview,
                            GwyPalette *palette)
{
}
*/
void
gwy_3d_view_set_palette       (Gwy3DView *gwy3dview,
                                 GwyPalette *palette)
{
    GwyPalette *old;

    gwy_debug("");

    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    g_return_if_fail(GWY_IS_PALETTE(palette));

    old = gwy3dview->palette;
/*    g_signal_handlers_disconnect_matched(old, G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, gwy3dview);
*/
    g_object_ref(palette);
    gwy3dview->palette = palette;
/*    g_signal_connect_swapped(3D_widget->palette, "value_changed",
                             G_CALLBACK(gwy_3d_view_update), 3D_widget);
*/
    g_object_unref(old);

    if (GTK_WIDGET_REALIZED(gwy3dview))
    {
        /* FIXME: ?is it necessary to delete old dislpay lists? */
        gwy_3d_make_list(gwy3dview, gwy3dview->data, GWY_3D_SHAPE_AFM);
        gwy_3d_make_list(gwy3dview, gwy3dview->downsampled, GWY_3D_SHAPE_REDUCED);

        gdk_window_invalidate_rect(
            GTK_WIDGET(gwy3dview)->window,
                       &GTK_WIDGET(gwy3dview)->allocation, FALSE);
    }

}

Gwy3DMovement
gwy_3d_view_get_status (Gwy3DView * gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), GWY_3D_ROTATION);

    return gwy3dview->movement_status;
}

void
gwy_3d_view_set_status (Gwy3DView * gwy3dview, Gwy3DMovement mv)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    gwy3dview->movement_status = mv;
}

gboolean
gwy_3d_view_get_orthographic   (Gwy3DView *gwy3dview)
{
    gwy_debug("");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);

    return gwy3dview->orthogonal_projection;
}

void
gwy_3d_view_set_orthographic   (Gwy3DView *gwy3dview,
                                 gboolean  orthographic)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (orthographic == gwy3dview->orthogonal_projection) return;
    gwy3dview->orthogonal_projection = orthographic;

    if (GTK_WIDGET_REALIZED(gwy3dview))
        gdk_window_invalidate_rect(
            GTK_WIDGET(gwy3dview)->window,
                       &GTK_WIDGET(gwy3dview)->allocation, FALSE);
}

gboolean
gwy_3d_view_get_show_axes     (Gwy3DView *gwy3dview)
{
    gwy_debug("");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);

    return gwy3dview->show_axes;
}

void
gwy_3d_view_set_show_axes     (Gwy3DView *gwy3dview,
                                 gboolean  show_axes)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (show_axes == gwy3dview->show_axes) return;
    gwy3dview->show_axes = show_axes;

    if (GTK_WIDGET_REALIZED(gwy3dview))
        gdk_window_invalidate_rect(
            GTK_WIDGET(gwy3dview)->window,
                       &GTK_WIDGET(gwy3dview)->allocation, FALSE);
}

gboolean
gwy_3d_view_get_show_labels   (Gwy3DView *gwy3dview)
{
    gwy_debug("");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);

    return gwy3dview->show_labels;
}

void
gwy_3d_view_set_show_labels   (Gwy3DView *gwy3dview,
                                 gboolean  show_labels)
{
     gwy_debug("");
     g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (show_labels == gwy3dview->show_labels) return;
    gwy3dview->show_labels = show_labels;

    if (GTK_WIDGET_REALIZED(gwy3dview))
        gdk_window_invalidate_rect(
            GTK_WIDGET(gwy3dview)->window,
                       &GTK_WIDGET(gwy3dview)->allocation, FALSE);
}

guint
gwy_3d_view_get_reduced_size  (Gwy3DView *gwy3dview)
{
     gwy_debug("");
     g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);

    return gwy3dview->reduced_size;
}

void
gwy_3d_view_set_reduced_size  (Gwy3DView *gwy3dview,
                                 guint  reduced_size)
{
    guint rx, ry;

    gwy_debug("");
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (reduced_size == gwy3dview->reduced_size) return;
    gwy3dview->reduced_size = reduced_size;
    g_object_unref(G_OBJECT(gwy3dview->downsampled));
    gwy3dview->downsampled =
        (GwyDataField*) gwy_data_field_new(
                            gwy_data_field_get_xres(gwy3dview->data),
                            gwy_data_field_get_yres(gwy3dview->data),
                            gwy_data_field_get_xreal(gwy3dview->data),
                            gwy_data_field_get_yreal(gwy3dview->data),
                            TRUE);
    gwy_data_field_copy(gwy3dview->data, gwy3dview->downsampled);
    rx = gwy_data_field_get_xres(gwy3dview->downsampled);
    ry = gwy_data_field_get_yres(gwy3dview->downsampled);
    if (rx > ry)
    {
        ry = (guint)((gdouble)ry / (gdouble)rx * (gdouble)gwy3dview->reduced_size);
        rx = gwy3dview->reduced_size;
    } else {
        rx = (guint) ((gdouble)rx / (gdouble)ry * (gdouble)gwy3dview->reduced_size);
        ry = gwy3dview->reduced_size;
    }
    gwy_data_field_resample(gwy3dview->downsampled,
                            rx,
                            ry,
                            GWY_INTERPOLATION_BILINEAR);

    if (GTK_WIDGET_REALIZED(gwy3dview))
    {

        /* FIXME: ?is it necessary to delete old dislpay lists? */
        gwy_3d_make_list(gwy3dview, gwy3dview->downsampled,
                         GWY_3D_SHAPE_REDUCED);

        gdk_window_invalidate_rect(
            GTK_WIDGET(gwy3dview)->window,
            &GTK_WIDGET(gwy3dview)->allocation, FALSE);
    }

}

GwyGLMaterialProp*
gwy_3d_view_get_material      (Gwy3DView *gwy3dview)
{
     gwy_debug("");
     g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);

    return gwy3dview->mat_current;
}

void
gwy_3d_view_set_material      (Gwy3DView *gwy3dview,
                                 GwyGLMaterialProp *material)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (material == gwy3dview->mat_current) return;
    gwy3dview->mat_current = material;

    if (GTK_WIDGET_REALIZED(gwy3dview))
        gdk_window_invalidate_rect(GTK_WIDGET(gwy3dview)->window,
                                   &GTK_WIDGET(gwy3dview)->allocation,
                                   FALSE);
}


GdkPixbuf*
gwy_3d_view_get_pixbuf(Gwy3DView *gwy3dview, guint xres, guint yres)
{
    int width, height, rowstride, n_channels, i, j;
    guchar *pixels, *a, *b, z;
    GdkPixbuf * pixbuf;

    gwy_debug("");

    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    g_return_val_if_fail(GTK_WIDGET_REALIZED(gwy3dview), NULL);

    width  = GTK_WIDGET(gwy3dview)->allocation.width;
    height = GTK_WIDGET(gwy3dview)->allocation.height;

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);

    glReadPixels(0, 0, width, height , GL_RGB, GL_UNSIGNED_BYTE,  pixels);

    a = pixels;
    b = pixels + (height -1)  * rowstride;
    for (i = 0; i < height / 2; i ++, b = pixels + (height - 1 - i) * rowstride)
        for (j = 0; j < rowstride; j++, a++, b++)
        {
             z = *a;
            *a = *b;
            *b =  z;
        }
    return pixbuf;
}

void
gwy_3d_view_reset_view(Gwy3DView * widget)
{
   g_return_if_fail(GWY_IS_3D_VIEW(widget));
   widget->rot_x = 45.0;
   widget->rot_y = -45.0;
   widget->view_scale = 1.0;
   widget->deformation_z = 1.0;
   widget->light_z = 0.0f;
   widget->light_y = 0.0f;
   if (GTK_WIDGET_REALIZED(widget))
        gdk_window_invalidate_rect(GTK_WIDGET(widget)->window,
                                   &GTK_WIDGET(widget)->allocation, FALSE);

}

static void
gwy_3d_view_realize(GtkWidget *widget)
{
    Gwy3DView *gwy3dview;


    gwy_debug("realizing a Gwy3DView (%ux%u)",
          widget->allocation.width, widget->allocation.height);

    g_return_if_fail(GWY_IS_3D_VIEW(widget));

    gwy3dview = GWY_3D_VIEW(widget);
    gwy_debug("parent realize");
    GTK_WIDGET_CLASS(parent_class)->realize(widget);
    gwy_debug("after parent realize");
    gtk_widget_add_events(widget,
                              GDK_BUTTON1_MOTION_MASK
                            | GDK_BUTTON2_MOTION_MASK
                            | GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_VISIBILITY_NOTIFY_MASK);


    gwy_3d_view_realize_gl(gwy3dview);


}

static gboolean
gwy_3d_view_configure (GtkWidget *widget, GdkEventConfigure *event)
{
    Gwy3DView *gwy3dview;


    GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

    GLfloat w = widget->allocation.width;
    GLfloat h = widget->allocation.height;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    gwy3dview = GWY_3D_VIEW(widget);

    /*** OpenGL BEGIN ***/
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return FALSE;

    glViewport(0, 0, w, h);
    gwy_3d_set_projection(gwy3dview, w, h);

    glMatrixMode(GL_MODELVIEW);

    gdk_gl_drawable_gl_end(gldrawable);
    /*** OpenGL END ***/

    return FALSE;
}

static void
gwy_3d_view_size_request(GtkWidget *widget,
                           GtkRequisition *requisition)
{
    gwy_debug("");
    GTK_WIDGET_CLASS(parent_class)->size_request(widget, requisition);
    requisition->width = GWY_3D_VIEW_DEFAULT_SIZE_X;
    requisition->height = GWY_3D_VIEW_DEFAULT_SIZE_Y;
}


static gboolean
gwy_3d_view_expose(GtkWidget *widget,
                       GdkEventExpose *event)
{
    GdkGLContext  *glcontext;
    GdkGLDrawable *gldrawable;
    Gwy3DView * gwy3D;

    GLfloat position[] = { 0.0, 3.0, 3.0, 1.0 };

    gwy_debug("");

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    gwy3D = GWY_3D_VIEW(widget);

    glcontext  = gtk_widget_get_gl_context(widget);
    gldrawable = gtk_widget_get_gl_drawable(widget);

    /*** OpenGL BEGIN ***/
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
      return FALSE;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();

    /* View transformation. */
    gwy_3d_set_projection(gwy3D, -1.0, -1.0);
    glTranslatef(0.0, 0.0, -10.0);
    glScalef(gwy3D->view_scale, gwy3D->view_scale, gwy3D->view_scale);

    glRotatef(gwy3D->rot_y, 1.0, 0.0, 0.0);
    glRotatef(gwy3D->rot_x, 0.0,  0.0, 1.0);
    glScalef(1.0f, 1.0f, gwy3D->deformation_z);

    /* Render shape */
    if (gwy3D->mat_current != gwyGL_mat_none)
    {
        glEnable(GL_LIGHTING);
        glMaterialfv(GL_FRONT, GL_AMBIENT,   gwy3D->mat_current->ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE,   gwy3D->mat_current->diffuse);
        glMaterialfv(GL_FRONT, GL_SPECULAR,  gwy3D->mat_current->specular);
        glMaterialf(GL_FRONT, GL_SHININESS, gwy3D->mat_current->shininess * 128.0);
        glPushMatrix();
        glRotatef(gwy3D->light_z, 0.0f, 0.0f, 1.0f);
        glRotatef(gwy3D->light_y, 1.0f, 0.0f, 0.0f);
        glLightfv(GL_LIGHT0, GL_POSITION, position);
        glPopMatrix();
    } else {
        glDisable(GL_LIGHTING);
    }


    glCallList(gwy3D->shape_list_base + gwy3D->shape_current);
    gwy_3d_draw_axes(gwy3D);

    if (gwy3D->movement_status == GWY_3D_LIGHTMOVEMENT
          && gwy3D->shape_current == GWY_3D_SHAPE_REDUCED)
        gwy_3d_draw_light_position(gwy3D);

    /* Swap buffers */
    if (gdk_gl_drawable_is_double_buffered(gldrawable))
      gdk_gl_drawable_swap_buffers(gldrawable);
    else
      glFlush();

    gdk_gl_drawable_gl_end(gldrawable);
    /*** OpenGL END ***/
    return FALSE;
}

static gboolean
gwy_3d_view_button_press(GtkWidget *widget,
                             GdkEventButton *event)
{
    Gwy3DView *gwy3dview;

    gwy_debug("");

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    gwy3dview = GWY_3D_VIEW(widget);

    begin_x = event->x;
    begin_y = event->y;

    if ((event->button == 1 || event->button == 2)
        && gwy3dview->shape_current == GWY_3D_SHAPE_AFM)
        gwy3dview->shape_current = GWY_3D_SHAPE_REDUCED;

    return FALSE;
}

static gboolean
gwy_3d_view_button_release(GtkWidget *widget,
                               GdkEventButton *event)
{
    Gwy3DView *gwy3dview;

    gwy_debug("");

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    gwy3dview = GWY_3D_VIEW(widget);

    if ((event->button == 1 || event->button == 2)
        && (gwy3dview->shape_current == GWY_3D_SHAPE_REDUCED))
    {
        gwy3dview->shape_current = GWY_3D_SHAPE_AFM;
        gdk_window_invalidate_rect(widget->window, &widget->allocation, FALSE);
    }

    return FALSE;
}

static gboolean
gwy_3d_view_motion_notify(GtkWidget *widget,
                              GdkEventMotion *event)
{
    Gwy3DView *gwy3dview;
    float w = widget->allocation.width;
    float h = widget->allocation.height;
    float x = event->x;
    float y = event->y;
    gboolean redraw = FALSE;

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    g_return_val_if_fail(event, FALSE);

    gwy3dview = GWY_3D_VIEW(widget);
    gwy_debug("motion event: (%f, %f), shape=%d",
              event->x, event->y, gwy3dview->shape_current);

    /* Rotation. */
    if (event->state & GDK_BUTTON1_MASK)
        switch (gwy3dview->movement_status)
        {
            case GWY_3D_ROTATION:
                gwy3dview->rot_x += x - begin_x;
                gwy3dview->rot_y += y - begin_y;
                redraw = TRUE;
                break;

            case GWY_3D_SCALE:
                gwy3dview->view_scale = gwy3dview->view_scale
                                        * (1.0 + (y - begin_y) / h);
                if (gwy3dview->view_scale > gwy3dview->view_scale_max)
                    gwy3dview->view_scale = gwy3dview->view_scale_max;
                else if (gwy3dview->view_scale < gwy3dview->view_scale_min)
                    gwy3dview->view_scale = gwy3dview->view_scale_min;
                redraw = TRUE;
                break;

            case GWY_3D_DEFORMATION:
            {
                register int i;
                if (y - begin_y > 0)
                    for (i = 0; i < y - begin_y; i++)
                        gwy3dview->deformation_z /= GWY_3D_Z_DEFORMATION;
                else
                    for (i = 0; i < begin_y - y; i++)
                        gwy3dview->deformation_z *= GWY_3D_Z_DEFORMATION;
                redraw = TRUE;
                break;
            }
            case GWY_3D_LIGHTMOVEMENT:
                gwy3dview->light_z += x - begin_x;
                gwy3dview->light_y += y - begin_y;
                redraw = TRUE;
                break;
        }

    begin_x = x;
    begin_y = y;
    if (redraw)
        gdk_window_invalidate_rect(widget->window, &widget->allocation, FALSE);

    return FALSE;
}



static Gwy3DVector *
gwy_3d_make_normals (GwyDataField * data, Gwy3DVector *normals)
{
   typedef struct { Gwy3DVector A, B; } RectangleNorm;
   gint i, j, xres, yres;
   RectangleNorm * norms;

   gwy_debug("");

   g_return_val_if_fail(GWY_IS_DATA_FIELD(data), NULL);
   g_return_val_if_fail(normals, NULL);

   xres = gwy_data_field_get_xres(data);
   yres = gwy_data_field_get_yres(data);

   /* memory for normals of triangles
    * total count of rectangels is (xres-1)*(yres-1), each one has 2n triangles
    * 3 componernts per vector
    */
   norms = g_new(RectangleNorm, (xres - 1 ) * (yres - 1));
   if (norms == NULL)
       return NULL;
   /* Calculation of nornals of triangles */
   for (j = 0; j < yres-1; j++)
      for (i = 0; i < xres-1; i++)
      {
         GLfloat a, b, c, n;
         a = gwy_data_field_get_val(data, i, j);
         b = gwy_data_field_get_val(data, i, j+1);
         c = gwy_data_field_get_val(data, i+1, j);
         n = 1.0 / sqrt((a-c)*(a-c) + (b-a)*(b-a) + 1.0);
         norms[j*(data->xres-1) + i].A.x = (a-c)*n;
         norms[j*(data->xres-1) + i].A.y = (b-a)*n;
         norms[j*(data->xres-1)+ i].A.z = n;

         a = b;
         b = c;
         c = gwy_data_field_get_val(data, i+1, j+1);
         n = 1.0 / sqrt((a-c)*(a-c) + (c-b)*(c-b) + 1.0);
         norms[j*(data->xres-1) + i].B.x = (a-c)*n;
         norms[j*(data->xres-1) + i].B.y = (c-b)*n;
         norms[j*(data->xres-1) + i].B.z = n;
      }
   /*two of corner vertecies have only one triangle adjacent (first and last vertex)*/
   normals[0] = norms[0].A;
   normals[xres * yres - 1] = norms[(xres-1) * (yres-1) - 1].B;
   /*the other two corner vertecies have two triangles adjacent*/
   normals[xres - 1].x = (norms[xres - 2].A.x + norms[xres - 2].B.x)/2.0f;
   normals[xres - 1].y = (norms[xres - 2].A.y + norms[xres - 2].B.y)/2.0f;
   normals[xres - 1].z = (norms[xres - 2].A.z + norms[xres - 2].B.z)/2.0f;
   normals[xres * (yres -1)].x =
      (norms[(xres - 1)*(yres -2)].A.x + norms[(xres - 1)*(yres -2)].B.x)/2.0f;
   normals[xres * (yres -1)].y =
      (norms[xres - 2].A.y + norms[(xres - 1)*(yres -2)].B.y)/2.0f;
   normals[xres * (yres -1)].z =
      (norms[xres - 2].A.z + norms[(xres - 1)*(yres -2)].B.z)/2.0f;
   /*the vertexies on the edge og the matrix have three adjacent triangles*/
   for ( i = 1; i < xres - 1; i ++)
   {
      const Gwy3DVector *a = &(norms[i-1].A);
      const Gwy3DVector *b = &(norms[i-1].B);
      const Gwy3DVector *c = &(norms[i].A);
      normals[i].x = (a->x + b->x + c->x)/3.0;
      normals[i].y = (a->y + b->y + c->y)/3.0;
      normals[i].z = (a->z + b->z + c->z)/3.0;
   }

  for ( i = 1; i < xres - 1; i ++)
  {
     const Gwy3DVector *a = &(norms[(xres - 1)*(yres -2) + i-1].B);
     const Gwy3DVector *b = &(norms[(xres - 1)*(yres -2) + i-1].A);
     const Gwy3DVector *c = &(norms[(xres - 1)*(yres -2) + i].B);
     normals[xres * (yres -1) + i].x = (a->x + b->x + c->x)/3.0;
     normals[xres * (yres -1) + i].y = (a->y + b->y + c->y)/3.0;
     normals[xres * (yres -1) + i].z = (a->z + b->z + c->z)/3.0;
  }
  for ( i = 1; i < yres - 1; i ++)
  {
     const Gwy3DVector *a = &(norms[(i-1) * (xres-1)].A);
     const Gwy3DVector *b = &(norms[(i-1) * (xres-1)].B);
     const Gwy3DVector *c = &(norms[i *     (xres-1)].A);
     normals[i * xres].x = (a->x + b->x + c->x)/3.0;
     normals[i * xres].y = (a->y + b->y + c->y)/3.0;
     normals[i * xres].z = (a->z + b->z + c->z)/3.0;
  }
  for ( i = 1; i < yres - 1; i ++)
  {
     const Gwy3DVector *a = &(norms[i     * (xres-1) - 1].B);
     const Gwy3DVector *b = &(norms[(i+1) * (xres-1) - 1].A);
     const Gwy3DVector *c = &(norms[(i+1) * (xres-1) - 1].B);
     normals[i * xres - 1].x = (a->x + b->x + c->x)/3.0;
     normals[i * xres - 1].y = (a->y + b->y + c->y)/3.0;
     normals[i * xres - 1].z = (a->z + b->z + c->z)/3.0;
  }

  /* The vertecies inside of matrix have six adjacent triangles*/
  for ( j = 1; j < yres - 1; j++)
     for ( i = 1; i < xres - 1; i ++)
     {
         Gwy3DVector * adj_tri[6];
         int k;
         adj_tri[0] = &(norms[(j-1)*(xres-1) + i - 1].B);
         adj_tri[1] = &(norms[(j-1)*(xres-1) + i    ].A);
         adj_tri[2] = &(norms[(j-1)*(xres-1) + i    ].B);
         adj_tri[3] = &(norms[j    *(xres-1) + i    ].A);
         adj_tri[4] = &(norms[j    *(xres-1) + i - 1].A);
         adj_tri[5] = &(norms[j    *(xres-1) + i - 1].B);
         normals[j*xres + i].x = 0.0f;
         normals[j*xres + i].y = 0.0f;
         normals[j*xres + i].z = 0.0f;
         for ( k = 0; k < 6; k++)
         {
            normals[j*xres + i].x += adj_tri[k]->x;
            normals[j*xres + i].y += adj_tri[k]->y;
            normals[j*xres + i].z += adj_tri[k]->z;
         }
         normals[j*xres + i].x /= 6.0f;
         normals[j*xres + i].y /= 6.0f;
         normals[j*xres + i].z /= 6.0f;
     }

  g_free(norms);
  return normals;
}

static void gwy_3d_make_list(Gwy3DView * gwy3D, GwyDataField * data, gint shape)
{
   gint i, j, xres, yres;
   GLdouble zdifr;
   Gwy3DVector * normals;
   GwyPaletteDef * pal_def;
   GwyRGBA color;

   gwy_debug("");

   g_return_if_fail(GWY_IS_DATA_FIELD(data));

   xres = gwy_data_field_get_xres(data);
   yres = gwy_data_field_get_yres(data);
   pal_def = gwy_palette_get_palette_def(gwy3D->palette);

   glNewList(gwy3D->shape_list_base + shape, GL_COMPILE);
      glPushMatrix();
      glTranslatef(-1.0, -1.0, GWY_3D_Z_DISPLACEMENT);
      glScalef(2.0/xres, 2.0/yres,
               GWY_3D_Z_TRANSFORMATION / (gwy3D->data_max - gwy3D->data_min) );
      glTranslatef(0.0, 0.0, -gwy3D->data_min);
      zdifr = 1.0/(gwy3D->data_max - gwy3D->data_min);
      normals = g_new(Gwy3DVector, xres * yres);
      if (gwy_3d_make_normals(data, normals) == NULL)
      {
           /*TODO udelat chybovy stav*/
      }
      for (j = 0; j < yres-1; j++)
      {
         glBegin(GL_TRIANGLE_STRIP);
         for (i = 0; i < xres-1; i++)
         {
            double a, b;
            a = gwy_data_field_get_val(data, i, j);
            b = gwy_data_field_get_val(data, i, j+1);
            glNormal3d(normals[j*xres+i].x, normals[j*xres+i].y, normals[j*xres+i].z);
            color = gwy_palette_def_get_color(pal_def,
                         (a - gwy3D->data_min) * zdifr, GWY_INTERPOLATION_BILINEAR);
            glColor3d(color.r , color.g, color.b);
            glVertex3d((double)i, (double)j, a);
            glNormal3d(normals[(j+1)*xres+i].x, normals[(j+1)*xres+i].y,
                       normals[(j+1)*xres+i].z);
            color = gwy_palette_def_get_color(pal_def,
                         (b - gwy3D->data_min) * zdifr, GWY_INTERPOLATION_BILINEAR);
            glColor3d(color.r , color.g, color.b);
            glVertex3d((double)i, (double)(j+1), b);
         }
         glEnd();
      }
      g_free(normals);
      glPopMatrix();
   glEndList();

}

static void gwy_3d_draw_axes(Gwy3DView * widget)
{
   GLfloat rx = widget->rot_x - ((int)(widget->rot_x / 360.0)) * 360.0;
   GLfloat Ax, Ay, Bx, By, Cx, Cy;
   gboolean yfirst = TRUE;
   gchar text[30];
   gint xres = gwy_data_field_get_xres(widget->data);
   gint yres = gwy_data_field_get_yres(widget->data);

   gwy_debug("");

   if (rx < 0.0)
       rx += 360.0;
   Ax = Ay = Bx = By = Cx = Cy = 0.0f;

   glPushMatrix();
   glTranslatef(-1.0, -1.0, GWY_3D_Z_DISPLACEMENT);
   glScalef(2.0/ xres,
            2.0/ yres,
            GWY_3D_Z_TRANSFORMATION / (widget->data_max - widget->data_min));
   glMaterialfv(GL_FRONT, GL_AMBIENT, gwyGL_mat_none->ambient);
   glMaterialfv(GL_FRONT, GL_DIFFUSE, gwyGL_mat_none->diffuse);
   glMaterialfv(GL_FRONT, GL_SPECULAR, gwyGL_mat_none->specular);
   glMaterialf(GL_FRONT, GL_SHININESS, gwyGL_mat_none->shininess * 128.0);

   if (widget->show_axes == TRUE)
   {
      if (rx >= 0.0 && rx <= 90.0)
      {
         Ay = yres;
         Cx = xres;
         yfirst = TRUE;
      } else if (rx > 90.0 && rx <= 180.0)
      {
         Ax = xres; Ay = yres;
         By = xres;
         yfirst = FALSE;
      } else if (rx > 180.0 && rx <= 270.0)
      {
         Ax = xres;
         Bx = xres; By = yres;
         Cy = yres;
         yfirst = TRUE;
      } else if (rx >= 270.0 && rx <= 360.0)
      {
         Bx = xres;
         Cx = xres; Cy = yres;
         yfirst = FALSE;
      }
      glBegin(GL_LINE_STRIP);
         glColor3f(0.0, 0.0, 0.0);
         glVertex3f(Ax, Ay, 0.0f);
         glVertex3f(Bx, By, 0.0f);
         glVertex3f(Cx, Cy, 0.0f);
         glVertex3f(Cx, Cy, widget->data_max - widget->data_min);
      glEnd();
      glBegin(GL_LINES);
         glVertex3f(Ax, Ay, 0.0f);
         glVertex3f(Ax - (Cx-Bx)*0.02, Ay - (Cy-By)*0.02, 0.0f );
         glVertex3f((Ax+Bx) / 2, (Ay+By) / 2, 0.0f);
         glVertex3f((Ax+Bx) / 2 - (Cx-Bx)*0.02,
                    (Ay+By) / 2 - (Cy-By)*0.02, 0.0f );
         glVertex3f(Bx , By, 0.0f);
         glVertex3f(Bx - (Cx-Bx)*0.02, By - (Cy-By)*0.02, 0.0f );
         glVertex3f(Bx, By, 0.0f);
         glVertex3f(Bx - (Ax-Bx)*0.02, By - (Ay-By)*0.02, 0.0f );
         glVertex3f((Cx+Bx) / 2, (Cy+By) / 2, 0.0f);
         glVertex3f((Cx+Bx) / 2 - (Ax-Bx)*0.02,
                    (Cy+By) / 2 - (Ay-By)*0.02, 0.0f );
         glVertex3f(Cx , Cy, 0.0f);
         glVertex3f(Cx - (Ax-Bx)*0.02, Cy - (Ay-By)*0.02, 0.0f );

         glPushMatrix();
         glTranslatef(Cx*cos(widget->rot_x) - Cy*sin(widget->rot_x),
                      Cx*sin(widget->rot_x) + Cy*cos(widget->rot_x), 0.0f);
         glRotatef(-widget->rot_x, 0.0f, 0.0f, 1.0f);
         glTranslatef(-Cx, -Cy, 0.0f);
         glVertex3f(Cx , Cy, widget->data_max - widget->data_min);
         glVertex3f(Cx - (Ax-Bx)*0.02, Cy - (Ay-By)*0.02,
                     widget->data_max - widget->data_min);
         glVertex3f(Cx , Cy, (widget->data_max - widget->data_min)/2);
         glVertex3f(Cx - (Ax-Bx)*0.02, Cy - (Ay-By)*0.02,
                    (widget->data_max-widget->data_min)/2);
         glPopMatrix();
      glEnd();

      /*
      xTODO: dodelat aby s prehazovanim os taky prehazovaly ciselne hodnoty
      xTODO: vyresit nejak zarovnani leveho pisma, chtelo by to zarovnavat doprava
      TODO: vytvorit symbol mikro
      TODO: vyresit zmenu velikosti pisma
      TODO: predelat pomoci GwySIUnit, vytvorit bitmapy hned ze zacatku
            pomoci Panga, ty ulozit a potom je vykreskovat,
            mozna tyto bitmapy do display listu
      */
      if (widget->show_labels == TRUE)
      {
         guint pom = 0;
         gdouble xreal = gwy_data_field_get_xreal(widget->data) * 1e6;
         gdouble yreal = gwy_data_field_get_yreal(widget->data) * 1e6;

         glListBase (widget->font_list_base);
         if (yfirst)
            sprintf(text, "y:%1.1f um", yreal);
         else
            sprintf(text, "x:%1.1f um", xreal);
         glRasterPos3f((Ax+2*Bx)/3 - (Cx-Bx)*0.1,
                       (Ay+2*By)/3 - (Cy-By)*0.1, -0.0f );
         glBitmap(0, 0, 0, 0, -100, 0, (GLubyte *)&pom);
         glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);
         if (! yfirst)
            sprintf(text, "y:%1.1f um", yreal);
         else
            sprintf(text, "x:%1.1f um", xreal);
         glRasterPos3f((2*Bx+Cx)/3 - (Ax-Bx)*0.1,
                       (2*By+Cy)/3 - (Ay-By)*0.1, -0.0f );
         glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);

         sprintf(text, "%1.1f nm", widget->data_max*1e9);
         glRasterPos3f(Cx - (Ax-Bx)*0.1, Cy - (Ay-By)*0.1,
                       (widget->data_max - widget->data_min));
         glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);

         sprintf(text, "%1.1f nm", widget->data_min*1e9);
         glRasterPos3f(Cx - (Ax-Bx)*0.1, Cy - (Ay-By)*0.1, -0.0f);
         glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);

      }
   }

   glPopMatrix();
}

static void gwy_3d_draw_light_position(Gwy3DView * widget)
{
    int i;
    GLfloat plane_z;

    gwy_debug("");

    glMaterialfv(GL_FRONT, GL_AMBIENT, gwyGL_mat_none->ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, gwyGL_mat_none->diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, gwyGL_mat_none->specular);
    glMaterialf(GL_FRONT, GL_SHININESS, gwyGL_mat_none->shininess * 128.0);
    glPushMatrix();
    plane_z =   GWY_3D_Z_TRANSFORMATION
              * (widget->data_mean - widget->data_min)
              / (widget->data_max  - widget->data_min)
              + GWY_3D_Z_DISPLACEMENT;
    /*
    FIXME: show light position does not show the real position of the light
    */
    glTranslatef(0.0f, 0.0f, plane_z);
    glRotatef(widget->light_z, 0.0f, 0.0f, 1.0f);
    glRotatef(widget->light_y, 1.0f, 0.0f, 0.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_QUAD_STRIP);
        for (i = -180; i <= 180; i += 5)
        {
            GLfloat y = cos(i * DIG_2_RAD) * sqrt(2);
            GLfloat z = sin(i * DIG_2_RAD) * sqrt(2);
            glVertex3f( 0.05f, y, z);
            glVertex3f(-0.05f, y, z);
        }
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_LINE_STRIP);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f( 0.05f, 1.0f, 1.0f);
        glVertex3f(-0.05f, 1.0f, 1.0f);
        glVertex3f(0.0f, 0.0f, 0.0f);
    glEnd();
    glPopMatrix();
}

static void gwy_3d_init_font(Gwy3DView * widget)
{
    PangoFontDescription *font_desc;
    PangoFont *font;
    PangoFontMetrics *font_metrics;
    /*
     * Generate font display lists.
     */

    gwy_debug("");

    widget->font_list_base = glGenLists(128);

    font_desc = pango_font_description_from_string(font_string);

    font = gdk_gl_font_use_pango_font(font_desc, 0, 128, widget->font_list_base);
    if (font == NULL)
    {
        gwy_debug("*** Can't load font '%s'\n", font_string);
        exit(1);
    }

    font_metrics = pango_font_get_metrics(font, NULL);

    widget->font_height = pango_font_metrics_get_ascent(font_metrics)
                + pango_font_metrics_get_descent(font_metrics);
    widget->font_height = PANGO_PIXELS(widget->font_height);

    pango_font_description_free(font_desc);
    pango_font_metrics_unref(font_metrics);
}

static void
gwy_3d_view_realize_gl (Gwy3DView *widget)
{
    GdkGLContext * glcontext;
    GdkGLDrawable * gldrawable;

    GLfloat ambient[] = { 0.1, 0.1, 0.1, 1.0 };
    GLfloat diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat position[] = { 0.0, 3.0, 3.0, 1.0 };

    GLfloat lmodel_ambient[] = { 0.2, 0.2, 0.2, 1.0 };
    GLfloat local_view[] = { 0.0 } ;

    gwy_debug("GL capable %d", gtk_widget_is_gl_capable(GTK_WIDGET(widget)));

    glcontext = gtk_widget_get_gl_context(GTK_WIDGET(widget));
    gldrawable = gtk_widget_get_gl_drawable(GTK_WIDGET(widget));
    /*** OpenGL BEGIN ***/
    gwy_debug("drawable %p, context %p", gldrawable, glcontext);

    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return;

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClearDepth(1.0);

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
    glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, local_view);

    glFrontFace(GL_CW);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_AUTO_NORMAL);
    glEnable(GL_NORMALIZE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    /* Shape display lists */
    widget->shape_list_base = glGenLists(2);
    gwy_3d_make_list(widget, widget->data, GWY_3D_SHAPE_AFM);
    gwy_3d_make_list(widget, widget->downsampled, GWY_3D_SHAPE_REDUCED);
    gwy_3d_init_font(widget);

    gdk_gl_drawable_gl_end(gldrawable);
    /*** OpenGL END ***/

  return;
}

static void gwy_3d_set_projection(Gwy3DView *widget, GLfloat width, GLfloat height)
{
    static GLfloat w, h;
    GLfloat aspect;

    gwy_debug("");

    if (width > 0.0)
        w = width;
    if (height > 0.0)
        h = height;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (w > h)
    {
        aspect = w / h;
        if (widget->orthogonal_projection == TRUE)
            glOrtho(-aspect * GWY_3D_ORTHO_CORRECTION,
                     aspect * GWY_3D_ORTHO_CORRECTION,
                     -1.0   * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                      1.0   * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                      5.0,
                      60.0);
        else
            glFrustum(-aspect, aspect, -1.0 , 1.0, 5.0, 60.0);
    }
    else
    {
        aspect = h / w;
        if (widget->orthogonal_projection == TRUE)
            glOrtho( -1.0   * GWY_3D_ORTHO_CORRECTION,
                      1.0   * GWY_3D_ORTHO_CORRECTION,
                    -aspect * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                     aspect * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                      5.0,
                      60.0);
        else
            glFrustum(-1.0, 1.0, -aspect, aspect, 5.0, 60.0);
    }
    glMatrixMode(GL_MODELVIEW);
}
