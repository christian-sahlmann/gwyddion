/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gdk/gdkevents.h>
#include <glib-object.h>
#include <gtk/gtkgl.h>


#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include "gwydgets.h"

#define DIG_2_RAD (G_PI / 180.0)
#define RAD_2_DIG (180.0 / G_PI)

#define GWY_3D_VIEW_TYPE_NAME "Gwy3DView"

#define BITS_PER_SAMPLE 8
#define GWY_3D_VIEW_DEFAULT_SIZE_X 260
#define GWY_3D_VIEW_DEFAULT_SIZE_Y 260
#define GWY_3D_SHAPE_AFM     0
#define GWY_3D_SHAPE_REDUCED 1

#define  GWY_3D_ORTHO_CORRECTION   2.0
#define  GWY_3D_Z_DEFORMATION     1.01
#define  GWY_3D_Z_TRANSFORMATION  0.5
#define  GWY_3D_Z_DISPLACEMENT    -0.2

#define GWY_3D_TIMEOUT_DELAY      1000

typedef struct {
    GLfloat x, y, z;
} Gwy3DVector;

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
static void          gwy_3d_set_projection      (Gwy3DView *gwy3dview);
static GtkAdjustment* gwy_3d_view_create_adjustment (Gwy3DView *gwy3dview,
                                                     const gchar *key,
                                                     gdouble value,
                                                     gdouble lower,
                                                     gdouble upper,
                                                     gdouble step,
                                                     gdouble page);
static void     gwy_3d_adjustment_value_changed (GtkAdjustment* adjustment,
                                                 gpointer user_data);
static void     gwy_3d_labels_value_changed     (Gwy3DLabels* labels,
                                                 gpointer user_data);
static void          gwy_3d_timeout_start       (Gwy3DView * gwy3dview,
                                                 gboolean immediate,
                                                 gboolean invalidate_now);
static gboolean      gwy_3d_timeout_func        (gpointer user_data);

static void          gl_pango_ft2_render_layout (PangoLayout *layout);
static void          gwy_3d_print_text          (Gwy3DView      *gwy3dview,
                                                 char           *text,
                                                 GLfloat        raster_x,
                                                 GLfloat        raster_y,
                                                 GLfloat        raster_z,
                                                 guint          size,
                                                 gint           vjustify,
                                                 gint           hjustify,
                                                 gint           displacement_x,
                                                 gint           displacement_y,
                                                 gfloat         rotation);


/* Local data */

static GtkDrawingAreaClass *parent_class = NULL;

static GQuark container_key_quark = 0;



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
        gwy_debug(" ");
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

    gwy_debug(" ");

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
}

static void
gwy_3d_view_init(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");

    gwy3dview->container             = NULL;
    gwy3dview->data                  = NULL;
    gwy3dview->downsampled           = NULL;
    gwy3dview->palette               = NULL;
    gwy3dview->reduced_size          = 100;
    gwy3dview->data_min              = 0.0;
    gwy3dview->data_max              = 0.0;
    gwy3dview->data_mean             = 0.0;

    gwy3dview->rot_x                 = NULL;
    gwy3dview->rot_y                 = NULL;
    gwy3dview->view_scale            = NULL;
    gwy3dview->deformation_z         = NULL;
    gwy3dview->light_z               = NULL;
    gwy3dview->light_y               = NULL;

    gwy3dview->view_scale_max        = 3.0f;
    gwy3dview->view_scale_min        = 0.5f;
    gwy3dview->movement_status       = GWY_3D_ROTATION;
    gwy3dview->orthogonal_projection = TRUE;
    gwy3dview->show_axes             = TRUE;
    gwy3dview->show_labels           = TRUE;
    gwy3dview->enable_lights         = FALSE;
    gwy3dview->mat_current           = NULL;
    gwy3dview->shape_list_base       = -1;
    gwy3dview->shape_current         = 0;
    gwy3dview->mouse_begin_x         = 0.0;
    gwy3dview->mouse_begin_y         = 0.0;
    gwy3dview->ft2_context           = NULL;
    gwy3dview->ft2_font_map          = NULL;
    gwy3dview->si_unit               = NULL;
    gwy3dview->labels                = NULL;
}

/**
 * gwy_3d_view_new:
 * @data: A #GwyContainer containing the data to display.
 *
 * Creates a new threedimensional OpenGL display of @data.
 * The widget is initialized from container @data.
 *
 * Returns: The new OpenGL 3D widget as a #GtkWidget.
 *
 * Since: 1.5
 **/
GtkWidget*
gwy_3d_view_new(GwyContainer *data)
{
    GdkGLConfig *glconfig;
    GtkWidget *widget;
    Gwy3DView *gwy3dview;
    const guchar *palette_name, *material_name;
    GwyPaletteDef *pdef;
    gdouble x;
    GObject *dfield;

    gwy_debug(" ");

    glconfig = gwy_widgets_get_gl_config();
    g_return_val_if_fail(glconfig, NULL);

    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);
    gwy_container_gis_object_by_name(data, "/0/data", &dfield);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), NULL);
    g_object_ref(data);
    g_object_ref(dfield);

    widget = gtk_widget_new(GWY_TYPE_3D_VIEW, NULL);
    gwy3dview = (Gwy3DView*)widget;

    gwy3dview->container = data;
    gwy3dview->data = (GwyDataField*)dfield;

    gwy3dview->rot_x
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/rot_x",
                                        45.0, -G_MAXDOUBLE, G_MAXDOUBLE,
                                         5.0, 30.0);
    gwy3dview->rot_y
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/rot_y",
                                        -45.0, -270.0, 180.0,
                                        5.0, 15.0);
    gwy3dview->view_scale
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/view_scale",
                                        1.0, 0.0, G_MAXDOUBLE,
                                        0.05, 0.5);
    gwy3dview->deformation_z
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/deformation_z",
                                        1.0, 0.0, G_MAXDOUBLE,
                                        0.05, 0.5);
    gwy3dview->light_z
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/light_z",
                                        0.0, -G_MAXDOUBLE, G_MAXDOUBLE,
                                        1.0, 15.0);
    gwy3dview->light_y
        = gwy_3d_view_create_adjustment(gwy3dview, "/0/3d/light_y",
                                        0.0, -G_MAXDOUBLE, G_MAXDOUBLE,
                                        1.0, 15.0);

    gwy3dview->mat_current
             = gwy_gl_material_get_by_name(GWY_GL_MATERIAL_NONE);

    if (!gwy_container_gis_string_by_name(data, "/0/3d/palette",
                                          &palette_name)) {
        if (!gwy_container_gis_string_by_name(data, "/0/base/palette",
                                              &palette_name))
            palette_name = GWY_PALETTE_GRAY;
        gwy_container_set_string_by_name(data, "/0/3d/palette",
                                         g_strdup(palette_name));
    }
    pdef = GWY_PALETTE_DEF(gwy_palette_def_new(palette_name));
    gwy3dview->palette = GWY_PALETTE(gwy_palette_new(pdef));
    g_object_ref(gwy3dview->palette);

    gwy_container_gis_int32_by_name(data, "/0/3d/reduced_size",
                                    &gwy3dview->reduced_size);
    if (gwy_container_gis_double_by_name(data, "/0/3d/view_scale_max", &x))
        gwy3dview->view_scale_max = x;
    if (gwy_container_gis_double_by_name(data, "/0/3d/view_scale_min", &x))
        gwy3dview->view_scale_min = x;
    gwy_container_gis_boolean_by_name(data, "/0/3d/orthogonal_projection",
                                      &gwy3dview->orthogonal_projection);
    gwy_container_gis_boolean_by_name(data, "/0/3d/show_axes",
                                      &gwy3dview->show_axes);
    gwy_container_gis_boolean_by_name(data, "/0/3d/show_labels",
                                      &gwy3dview->show_labels);
    gwy_container_gis_boolean_by_name(data, "/0/3d/enable_lights",
                                      &gwy3dview->enable_lights);
    if (gwy_container_gis_string_by_name(data, "/0/3d/material",
                                         &material_name))
        gwy3dview->mat_current = gwy_gl_material_get_by_name(material_name);

    /* should be always true */
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
    gtk_widget_set_gl_capability(GTK_WIDGET(gwy3dview),
                                 glconfig,
                                 NULL,
                                 TRUE,
                                 GDK_GL_RGBA_TYPE);

    gwy3dview->si_unit = (GwySIUnit*) gwy_si_unit_new("m");
    gwy3dview->labels = gwy_3d_labels_new(gwy3dview->container);
    g_signal_connect(gwy3dview->labels, "label_changed",
                     G_CALLBACK(gwy_3d_labels_value_changed), gwy3dview);
    gwy_3d_labels_update(gwy3dview->labels, gwy3dview->si_unit);
    gwy_debug("labels:%p",gwy3dview->labels);
    return widget;
}

static GtkAdjustment*
gwy_3d_view_create_adjustment(Gwy3DView *gwy3dview,
                              const gchar *key,
                              gdouble value,
                              gdouble lower,
                              gdouble upper,
                              gdouble step,
                              gdouble page)
{
    GtkObject *adj;
    GQuark quark;

    if (!container_key_quark)
        container_key_quark = g_quark_from_string("gwy3dview-container-key");

    quark = g_quark_from_string(key);
    gwy_container_gis_double(gwy3dview->container, quark, &value);
    adj = gtk_adjustment_new(value, lower, upper, step, page, 0.0);
    g_object_ref(adj);
    gtk_object_sink(adj);
    g_object_set_qdata(G_OBJECT(adj), container_key_quark,
                       GUINT_TO_POINTER(quark));
    g_signal_connect(adj, "value-changed",
                     G_CALLBACK(gwy_3d_adjustment_value_changed), gwy3dview);

    return (GtkAdjustment*)adj;
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
    gwy_object_unref(gwy3dview->rot_x);
    gwy_object_unref(gwy3dview->rot_y);
    gwy_object_unref(gwy3dview->view_scale);
    gwy_object_unref(gwy3dview->deformation_z);
    gwy_object_unref(gwy3dview->light_z);
    gwy_object_unref(gwy3dview->light_y);

    gwy_object_unref(gwy3dview->si_unit);
    gwy_object_unref(gwy3dview->labels);

    if (gwy3dview->shape_list_base >= 0) {
        glDeleteLists(gwy3dview->shape_list_base, 2);
        gwy3dview->shape_list_base = -1;
    }


    G_OBJECT_CLASS(parent_class)->finalize(object);

    gwy_debug("    ... (refcount = %u)", object->ref_count);
}

static void
gwy_3d_view_unrealize(GtkWidget *widget)
{
    Gwy3DView *gwy3dview;

    gwy3dview = GWY_3D_VIEW(widget);

    gwy_debug(" ");

    if (gwy3dview->shape_list_base >= 0)
    {
        glDeleteLists(gwy3dview->shape_list_base, 2);
        gwy3dview->shape_list_base = -1;
    }

    g_object_unref( G_OBJECT(gwy3dview->ft2_context));
    g_object_unref( G_OBJECT(gwy3dview->ft2_font_map));

    /* TODO: save the current view settings into the contaier  */
    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}

/**
 * gwy_3d_view_update:
 * @gwy3dview: A 3D data view widget.
 *
 * Instructs a 3D data view to update self and repaint.
 * The values of GwyDataField @data, GwyPalette @palette
 * are updated from container @container. If necessary new
 * @downsampled data are created. Old values of @data and @palette
 * are unreferenced.
 *
 * The display lists are recreated if the widget is realized. This may
 * take a great amount of processor time (seconds).
 *
 * Since: 1.5
 **/
void
gwy_3d_view_update(Gwy3DView *gwy3dview)
{
    gboolean update_data = FALSE;
    gboolean update_palette = FALSE;
    const guchar *palette_name = NULL;

    gwy_debug(" ");
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (gwy_container_contains_by_name(gwy3dview->container, "/0/data")) {
        GwyDataField  *data;

        data = GWY_DATA_FIELD(gwy_container_get_object_by_name
                                    (gwy3dview->container, "/0/data"));
/*        if (data != gwy3dview->data)
*/
        {
            gwy_object_unref(gwy3dview->data);
            gwy3dview->data = data;
            g_object_ref(gwy3dview->data);
            update_data = TRUE;
        }
    }

    if (!gwy_container_gis_string_by_name(gwy3dview->container, "/0/3d/palette",
                                          &palette_name)) {
        if (!gwy_container_gis_string_by_name(gwy3dview->container,
                                              "/0/base/palette",
                                              &palette_name))
            palette_name = GWY_PALETTE_GRAY;
        gwy_container_set_string_by_name(gwy3dview->container, "/0/3d/palette",
                                         g_strdup(palette_name));
    }
    if (strcmp(palette_name,
               gwy_palette_def_get_name(gwy_palette_get_palette_def
                                                      (gwy3dview->palette)))) {
        gwy_palette_set_by_name(gwy3dview->palette, palette_name);
        update_palette = TRUE;
    }

    if (update_data == TRUE) {
        /* make the dowsampled preview */
        guint rx, ry;

        gwy_object_unref(gwy3dview->downsampled);
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

        gwy3dview->data_min  = gwy_data_field_get_min(gwy3dview->data);
        gwy3dview->data_max  = gwy_data_field_get_max(gwy3dview->data);
        gwy_3d_labels_update(gwy3dview->labels, gwy3dview->si_unit);
    }

    if ((update_data == TRUE || update_palette == TRUE)
         && GTK_WIDGET_REALIZED(gwy3dview))
    {
        gwy_3d_make_list(gwy3dview, gwy3dview->downsampled, GWY_3D_SHAPE_REDUCED);
        gwy_3d_make_list(gwy3dview, gwy3dview->data, GWY_3D_SHAPE_AFM);
        gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
    }

}



/**
 * gwy_3d_view_get_palette:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns the palette a 3D data view uses.
 *
 * Returns: The palette @gwy3dview uses.
 *
 * Since: 1.5
 **/
GwyPalette*
gwy_3d_view_get_palette       (Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);

    return gwy3dview->palette;
}


/**
 * gwy_3d_view_set_palette:
 * @gwy3dview: A 3D data view widget.
 * @palette: A palette.
 *
 * Sets the palette of a 3D data view.
 *
 * The display lists are recreated if the widget is realized. This may
 * take a great amount of processor time (seconds).
 * Further, the widget is redrawn.
 *
 * Since: 1.5
 **/
void
gwy_3d_view_set_palette       (Gwy3DView *gwy3dview,
                                 GwyPalette *palette)
{
    GwyPalette *old;
    const gchar *name;

    gwy_debug(" ");

    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    g_return_if_fail(GWY_IS_PALETTE(palette));

    old = gwy3dview->palette;
    g_object_ref(palette);
    gwy3dview->palette = palette;
    gwy_object_unref(old);

    name = gwy_palette_def_get_name(gwy_palette_get_palette_def(palette));
    gwy_container_set_string_by_name(gwy3dview->container, "/0/3d/palette",
                                     g_strdup(name));

    if (GTK_WIDGET_REALIZED(gwy3dview))
    {
        gwy_3d_make_list(gwy3dview, gwy3dview->downsampled, GWY_3D_SHAPE_REDUCED);
        gwy_3d_make_list(gwy3dview, gwy3dview->data, GWY_3D_SHAPE_AFM);
        gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
    }

}

/**
 * gwy_3d_view_get_status:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a #Gwy3DMovement describing actual type of response on
 * the mouse motion event.
 *
 * Returns: actual type of response on the mouse motion event
 *
 * Since: 1.5
 **/
Gwy3DMovement
gwy_3d_view_get_status (Gwy3DView * gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), GWY_3D_NONE);

    return gwy3dview->movement_status;
}

/**
 * gwy_3d_view_set_status:
 * @gwy3dview: A 3D data view widget.
 * @mv: A new type of response on the mouse motion event.
 *
 * Sets the type of widget response on the mouse motion event.
 * See #Gwy3DMovement.
 *
 * Since: 1.5
 **/
void
gwy_3d_view_set_status (Gwy3DView * gwy3dview, Gwy3DMovement mv)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    g_return_if_fail(mv <= GWY_3D_LIGHT_MOVEMENT);

    gwy3dview->movement_status = mv;
}

/**
 * gwy_3d_view_get_orthographic:
 * @gwy3dview: A 3D data view widget.
 *
 *
 * Returns: TRUE if projection of the data is orthografic
 *          FALSE if projection of the data is perspective
 *
 * Since: 1.5
 **/
gboolean
gwy_3d_view_get_orthographic(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);

    return gwy3dview->orthogonal_projection;
}

/**
 * gwy_3d_view_set_orthographic:
 * @gwy3dview: A 3D data view widget.
 * @orthographic: Whether the projection of the data is
 *    orthograpic (TRUE) or perspective (FALSE).
 *
 * Sets the type of projection of the 3D data to the screen and
 * redraws widget (if realized).
 *
 * Since: 1.5
 **/
void
gwy_3d_view_set_orthographic(Gwy3DView *gwy3dview,
                             gboolean  orthographic)
{
    gwy_debug(" ");
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (orthographic == gwy3dview->orthogonal_projection) return;
    gwy3dview->orthogonal_projection = orthographic;
    gwy_container_set_boolean_by_name(gwy3dview->container,
                                      "/0/3d/orthogonal_projection",
                                      orthographic);

    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}

/**
 * gwy_3d_view_get_show_axes:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns whether the axes are shown within the widget.
 *
 * Returns: visibility of the axes
 *
 * Since: 1.5
 **/
gboolean
gwy_3d_view_get_show_axes(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);

    return gwy3dview->show_axes;
}

/**
 * gwy_3d_view_set_show_axes:
 * @gwy3dview: A 3D data view widget.
 * @show_axes: Show/hide axes
 *
 * Show/hide axes within @gwy3dview. Widget is invalidated if necessary.
 *
 * Since: 1.5
 **/
void
gwy_3d_view_set_show_axes(Gwy3DView *gwy3dview,
                          gboolean  show_axes)
{
    gwy_debug(" ");
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (show_axes == gwy3dview->show_axes) return;
    gwy3dview->show_axes = show_axes;
    gwy_container_set_boolean_by_name(gwy3dview->container,
                                      "/0/3d/show_axes",
                                      show_axes);

    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);

}

/**
 * gwy_3d_view_get_show_labels:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns whether the axes labels are shown within the @gwy3dview.
 * The labels are visible only if #show_axes is TRUE.
 *
 * Returns: Whwteher the axes labels are visible.
 *
 * Since: 1.5
 **/
gboolean
gwy_3d_view_get_show_labels(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);

    return gwy3dview->show_labels;
}

/**
 * gwy_3d_view_set_show_labels:
 * @gwy3dview: A 3D data view widget.
 * @show_labels: Show/hide axes labels
 *
 * Show/hide lables of the axes within @gwy3dview.
 * Widget is invalidated if necessary.
 * The labels of the axes are visible only if #show_axes is TRUE.
 *
 * Since: 1.5
 **/
void
gwy_3d_view_set_show_labels(Gwy3DView *gwy3dview,
                            gboolean  show_labels)
{
     gwy_debug(" ");
     g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (show_labels == gwy3dview->show_labels) return;
    gwy3dview->show_labels = show_labels;
    gwy_container_set_boolean_by_name(gwy3dview->container,
                                      "/0/3d/show_labels",
                                      show_labels);

    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);

}

/**
 * gwy_3d_view_get_use_lights:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns whether the lighst are on or off within the @gwy3dview.
 *
 * Returns: %TRUE if the lights are on, %FALSE if they are off.
 *
 * Since: 1.5
 **/
gboolean
gwy_3d_view_get_use_lights(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);

    return gwy3dview->enable_lights;
}

/**
 * gwy_3d_view_set_use_lights:
 * @gwy3dview: A 3D data view widget.
 * @use_lights: %TRUE if lights should be used, %FALSE to use palette.
 *
 * Turn lights on/off within @gwy3dview.
 *
 * Widget is invalidated if necessary.
 *
 * Since: 1.5
 **/
void
gwy_3d_view_set_use_lights(Gwy3DView *gwy3dview,
                           gboolean  use_lights)
{
     gwy_debug(" ");
     g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (use_lights == gwy3dview->enable_lights)
        return;
    gwy3dview->enable_lights = use_lights;
    gwy_container_set_boolean_by_name(gwy3dview->container,
                                      "/0/3d/enable_lights",
                                      use_lights);

    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}

/**
 * gwy_3d_view_get_reduced_size:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns the size of the downsampled data. The downampled
 * data are used when fast rendering of surface is needed
 * (in the situations like mouse rotations etc.)
 *
 * Returns: The size of the downsampled data.
 *
 * Since: 1.5
 **/
guint
gwy_3d_view_get_reduced_size(Gwy3DView *gwy3dview)
{
     gwy_debug(" ");
     g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);

    return gwy3dview->reduced_size;
}

/**
 * gwy_3d_view_set_reduced_size:
 * @gwy3dview: A 3D data view widget.
 * @reduced_size: The size of the downsampled data.
 *
 * Sets the size of the downsampled data. In case of the original
 * data are not square, the @reduced_size is the greater size of the
 * downsampled data, the other dimension is proportional to the original
 * size.
 *
 * If necessary a display list is recreated and widget is invalidated
 *
 * Since: 1.5
 **/
void
gwy_3d_view_set_reduced_size(Gwy3DView *gwy3dview,
                             guint  reduced_size)
{
    guint rx, ry;

    gwy_debug(" ");
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (reduced_size == gwy3dview->reduced_size) return;
    gwy3dview->reduced_size = reduced_size;
    gwy_object_unref(gwy3dview->downsampled);
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

        gwy_3d_make_list(gwy3dview, gwy3dview->downsampled,
                         GWY_3D_SHAPE_REDUCED);

        gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
    }

}

/**
 * gwy_3d_view_get_material:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a #GwyGLMaterial used to draw data with lights on.
 *
 * Returns: A #GwyGLMaterial used to draw data with lights on.
 *
 * Since: 1.5
 **/
GwyGLMaterial*
gwy_3d_view_get_material(Gwy3DView *gwy3dview)
{
     gwy_debug(" ");
     g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);

    return gwy3dview->mat_current;
}

/**
 * gwy_3d_view_set_material:
 * @gwy3dview: A 3D data view widget.
 * @material: A #GwyGLMaterial used to draw data with lights on.
 *
 * Sets the material of the surface. If the material is #GWY_GL_MATERIAL_NONE
 * the surface is drawn using the colors obtained from the #palette.
 * If any other material is the the lights are turned on and the surface
 * is rendered using this material.
 *
 * Since: 1.5
 **/
void
gwy_3d_view_set_material(Gwy3DView *gwy3dview,
                         GwyGLMaterial *material)
{
    const gchar *name;

    gwy_debug(" ");
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (material == gwy3dview->mat_current)
        return;
    gwy3dview->mat_current = material;
    name = gwy_gl_material_get_name(material);
    gwy_container_set_string_by_name(gwy3dview->container,
                                      "/0/3d/material",
                                      g_strdup(name));

    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}


/**
 * gwy_3d_view_get_pixbuf:
 * @gwy3dview: A 3D data view widget.
 * @xres: Requested pixbuf x-resolution.  This parameters is currently
 *        unimplemented.
 * @yres: Requested pixbuf y-resolution.  This parameters is currently
 *        unimplemented.
 *
 * Copies the contents of the framebuffer to the GdkPixbuf.
 *
 * The size of the pixbuf is currently indentical with the size of the widget.
 * @xres and @yres will be implemented to set the resolution of the pixbuf.
 *
 * Returns: A newly allocated GdkPixbuf with copy of the framebuffer.
 *
 * Since: 1.5
 **/
GdkPixbuf*
gwy_3d_view_get_pixbuf(Gwy3DView *gwy3dview,
                       G_GNUC_UNUSED guint xres,
                       G_GNUC_UNUSED guint yres)
{
    int width, height, rowstride, n_channels, i, j;
    guchar *pixels, *a, *b, z;
    GdkPixbuf * pixbuf;

    gwy_debug(" ");

    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    g_return_val_if_fail(GTK_WIDGET_REALIZED(gwy3dview), NULL);

    width  = GTK_WIDGET(gwy3dview)->allocation.width;
    height = GTK_WIDGET(gwy3dview)->allocation.height;

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
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

/**
 * gwy_3d_view_get_data:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns the data container this 3D view displays.
 *
 * Returns: The container as a #GwyContainer.
 *
 * Since: 1.5
 **/
GwyContainer*
gwy_3d_view_get_data(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GTK_WIDGET_REALIZED(gwy3dview), NULL);

    return gwy3dview->container;
}

/**
 * gwy_3d_view_reset_view:
 * @gwy3dview: A 3D data view widget.
 *
 * Resets angle of the view, scale, deformation ant the position
 * of the light to the "default" values. Invalidates the widget.
 *
 * Since: 1.5
 **/
void
gwy_3d_view_reset_view(Gwy3DView * gwy3dview)
{
   g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
   gtk_adjustment_set_value(gwy3dview->rot_x, 45.0);
   gtk_adjustment_set_value(gwy3dview->rot_y, -45.0);
   gtk_adjustment_set_value(gwy3dview->view_scale, 1.0);
   gtk_adjustment_set_value(gwy3dview->deformation_z, 1.0);
   gtk_adjustment_set_value(gwy3dview->light_z, 0.0);
   gtk_adjustment_set_value(gwy3dview->light_y, 0.0f);

   gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);

}

/**
 * gwy_3d_view_get_rot_x_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the Phi angle of rotation.
 *
 * Returns: a GtkAdjustment with settings of the Phi angle of rotation
 *
 * Since: 1.5
 **/
GtkAdjustment *
gwy_3d_view_get_rot_x_adjustment(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->rot_x;
}

/**
 * gwy_3d_view_get_rot_y_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the Theta angle of rotation
 *
 * Returns: a GtkAdjustment with settings of the Theta angle of rotation
 *
 * Since: 1.5
 **/
GtkAdjustment *
gwy_3d_view_get_rot_y_adjustment(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->rot_y;
}

/**
 * gwy_3d_view_get_view_scale_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the view zoom
 *
 * Returns: a GtkAdjustment with settings of the view zoom
 *
 * Since: 1.5
 **/
GtkAdjustment *
gwy_3d_view_get_view_scale_adjustment(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->view_scale;
}

/**
 * gwy_3d_view_get_z_deformation_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the zoom of the z-axis
 *
 * Returns: a GtkAdjustment with settings of the zoom of the z-axis
 *
 * Since: 1.5
 **/
GtkAdjustment *
gwy_3d_view_get_z_deformation_adjustment(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->deformation_z;
}

/**
 * gwy_3d_view_get_light_z_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the Phi angle of light position.
 *
 * Returns: a GtkAdjustment with settings of the Phi angle of light position
 *
 * Since: 1.5
 **/
GtkAdjustment *
gwy_3d_view_get_light_z_adjustment(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->light_z;
}

/**
 * gwy_3d_view_get_light_y_adjustment:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a GtkAdjustment with settings of the Theta angle of light position.
 *
 * Returns: a GtkAdjustment with settings of the Theta angle of light position
 *
 * Since: 1.5
 **/
GtkAdjustment *
gwy_3d_view_get_light_y_adjustment(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->light_y;
}

/**
 * gwy_3d_view_get_max_view_scale:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns the maximal zoom of the 3D data view
 *
 * Returns: the maximal zoom of the 3D data view
 *
 * Since: 1.5
 **/
gdouble
gwy_3d_view_get_max_view_scale(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), G_MAXDOUBLE);
    return gwy3dview->view_scale_max;
}

/**
 * gwy_3d_view_get_min_view_scale:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns the minimal zoom of the 3D data view
 *
 * Returns: the minimal zoom of the 3D data view
 *
 * Since: 1.5
 **/
gdouble
gwy_3d_view_get_min_view_scale(Gwy3DView *gwy3dview)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), G_MAXDOUBLE);
    return gwy3dview->view_scale_min;
}

/**
 * gwy_3d_view_set_max_view_scale:
 * @gwy3dview: A 3D data view widget.
 * @new_max_scale: maximal zoom of the 3D data view
 *
 * Sets the new maximal zoom of 3D data view. Recommended values are 0.5 - 5.0.
 *
 * Returns: whether the function finished succesfully (allways)
 *
 * Since: 1.5
 **/
gboolean
gwy_3d_view_set_max_view_scale(Gwy3DView *gwy3dview, gdouble new_max_scale)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);
    new_max_scale = fabs(new_max_scale);
    if (new_max_scale != gwy3dview->view_scale_max)
    {
        gwy3dview->view_scale_max = new_max_scale;
        if (gwy3dview->view_scale_max < gwy3dview->view_scale_min)
        {
            gdouble _tmp = gwy3dview->view_scale_max;
            gwy3dview->view_scale_max = gwy3dview->view_scale_min;
            gwy3dview->view_scale_min = _tmp;
        }
        if (gwy3dview->view_scale->value > gwy3dview->view_scale_max)
           gtk_adjustment_set_value(gwy3dview->view_scale,
                                    gwy3dview->view_scale_max);
        if (gwy3dview->view_scale->value < gwy3dview->view_scale_min)
           gtk_adjustment_set_value(gwy3dview->view_scale,
                                    gwy3dview->view_scale_min);
    }
    return TRUE;
}

/**
 * gwy_3d_view_set_min_view_scale:
 * @gwy3dview: A 3D data view widget.
 * @new_min_scale: minimal zoom of the 3D data view
 *
 * Sets the new manimal zoom of 3D data view. Recommended values are 0.5 - 5.0.
 *
 * Returns: whether the function finished succesfully (allways)
 *
 * Since: 1.5
 **/
gboolean
gwy_3d_view_set_min_view_scale(Gwy3DView *gwy3dview, gdouble new_min_scale)
{
    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), FALSE);
    new_min_scale = fabs(new_min_scale);
    if (new_min_scale != gwy3dview->view_scale_min)
    {
        gwy3dview->view_scale_min = new_min_scale;
        if (gwy3dview->view_scale_min < gwy3dview->view_scale_min)
        {
            gdouble _tmp = gwy3dview->view_scale_min;
            gwy3dview->view_scale_min = gwy3dview->view_scale_max;
            gwy3dview->view_scale_max = _tmp;
        }
        if (gwy3dview->view_scale->value < gwy3dview->view_scale_min)
           gtk_adjustment_set_value(gwy3dview->view_scale,
                                    gwy3dview->view_scale_min);
        if (gwy3dview->view_scale->value > gwy3dview->view_scale_max)
           gtk_adjustment_set_value(gwy3dview->view_scale,
                                    gwy3dview->view_scale_max);
    }
    return TRUE;

}

/**
 * gwy_3d_view_get_label_description:
 * @gwy3dview: A 3D data view widget.
 * @label_name: identifier of the label
 *
 * Returns a #Gwy3DLabelDescription structure containing informations about
 * labels shown within 3D data view. 
 *
 * Returns: a #Gwy3DLabelDescription structure containing informations about
 *          labels shown within 3D data view. 
 *
 * Since: 1.5
 **/
Gwy3DLabelDescription *
gwy_3d_view_get_label_description(Gwy3DView * gwy3dview, Gwy3DLabelName label_name)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy_3d_labels_get_description(gwy3dview->labels, label_name);
}

/******************************************************************************/
static void
gwy_3d_timeout_start(Gwy3DView * gwy3dview, gboolean immediate, gboolean invalidate_now)
{
    gwy_debug(" ");

    if (gwy3dview->timeout == TRUE)
         g_source_remove(gwy3dview->timeout_id);

    if (!GTK_WIDGET_REALIZED(gwy3dview))
        return;

    if (immediate == TRUE)
        gwy3dview->shape_current = GWY_3D_SHAPE_AFM;
    else
        gwy3dview->shape_current = GWY_3D_SHAPE_REDUCED;

    if (invalidate_now == TRUE || immediate == TRUE)
        gdk_window_invalidate_rect(GTK_WIDGET(gwy3dview)->window,
                                   &GTK_WIDGET(gwy3dview)->allocation, FALSE);

    if (!immediate)
    {
        gwy3dview->timeout_id = g_timeout_add(GWY_3D_TIMEOUT_DELAY,
                                              (GSourceFunc)gwy_3d_timeout_func,
                                              (gpointer)gwy3dview);
        gwy3dview->timeout = TRUE;
    }
}

static gboolean
gwy_3d_timeout_func(gpointer user_data)
{
    Gwy3DView * gwy3dview;

    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_3D_VIEW(user_data), FALSE);

    gwy3dview = (Gwy3DView *) user_data;
    gwy3dview->shape_current = GWY_3D_SHAPE_AFM;
    gwy3dview->timeout = FALSE;
    if (GTK_WIDGET_REALIZED(gwy3dview))
        gdk_window_invalidate_rect(GTK_WIDGET(gwy3dview)->window,
                                   &GTK_WIDGET(gwy3dview)->allocation, TRUE);

    return FALSE;
}


static void
gwy_3d_adjustment_value_changed(GtkAdjustment* adjustment, gpointer user_data)
{
    Gwy3DView *gwy3dview = (Gwy3DView*)user_data;
    GQuark quark;

    gwy_debug(" ");
    g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if ((quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(adjustment),
                                                     container_key_quark))))
        gwy_container_set_double(gwy3dview->container, quark,
                                 gtk_adjustment_get_value(adjustment));
    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
}

static void
gwy_3d_labels_value_changed(Gwy3DLabels* labels, gpointer user_data)
{
    Gwy3DView *gwy3dview = (Gwy3DView*)user_data;

    gwy_debug(" ");
    g_return_if_fail(GWY_IS_3D_LABELS(labels));
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    gwy_3d_timeout_start(gwy3dview, FALSE, TRUE);
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

   /* Get PangoFT2 context. */
   gwy3dview->ft2_font_map = (PangoFT2FontMap*) pango_ft2_font_map_new();
   pango_ft2_font_map_set_resolution(gwy3dview->ft2_font_map, 72, 72);
   gwy3dview->ft2_context = pango_ft2_font_map_create_context(gwy3dview->ft2_font_map);

}

static gboolean
gwy_3d_view_configure(GtkWidget *widget,
                      G_GNUC_UNUSED GdkEventConfigure *event)
{
    Gwy3DView *gwy3dview;


    GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

    GLfloat w = widget->allocation.width;
    GLfloat h = widget->allocation.height;

    gwy_debug("width: %f, height: %f", w, h);
    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    gwy3dview = GWY_3D_VIEW(widget);

    /*** OpenGL BEGIN ***/
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return FALSE;

    glViewport(0, 0, w, h);
    gwy_3d_set_projection(gwy3dview);

    glMatrixMode(GL_MODELVIEW);

    gdk_gl_drawable_gl_end(gldrawable);
    /*** OpenGL END ***/

    return FALSE;
}

static void
gwy_3d_view_size_request(GtkWidget *widget,
                           GtkRequisition *requisition)
{
    gwy_debug(" ");
    GTK_WIDGET_CLASS(parent_class)->size_request(widget, requisition);
    requisition->width = GWY_3D_VIEW_DEFAULT_SIZE_X;
    requisition->height = GWY_3D_VIEW_DEFAULT_SIZE_Y;
}


static gboolean
gwy_3d_view_expose(GtkWidget *widget,
                   G_GNUC_UNUSED GdkEventExpose *event)
{
    GdkGLContext  *glcontext;
    GdkGLDrawable *gldrawable;
    Gwy3DView * gwy3D;

    GLfloat light_position[] = { 0.0, 0.0, 4.0, 1.0 };

    gwy_debug(" ");

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
    gwy_3d_set_projection(gwy3D);
    glTranslatef(0.0, 0.0, -10.0);
    glScalef(gwy3D->view_scale->value,
             gwy3D->view_scale->value,
             gwy3D->view_scale->value);

    glRotatef(gwy3D->rot_y->value, 1.0, 0.0, 0.0);
    glRotatef(gwy3D->rot_x->value, 0.0,  0.0, 1.0);
    glScalef(1.0f, 1.0f, gwy3D->deformation_z->value);

    /* Render shape */
    if (gwy3D->enable_lights)
    {
        glEnable(GL_LIGHTING);
        glMaterialfv(GL_FRONT, GL_AMBIENT,   gwy3D->mat_current->ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE,   gwy3D->mat_current->diffuse);
        glMaterialfv(GL_FRONT, GL_SPECULAR,  gwy3D->mat_current->specular);
        glMaterialf(GL_FRONT, GL_SHININESS, gwy3D->mat_current->shininess*128);
        glPushMatrix();
        glRotatef(gwy3D->light_z->value, 0.0f, 0.0f, 1.0f);
        glRotatef(gwy3D->light_y->value, 0.0f, 1.0f, 0.0f);
        glLightfv(GL_LIGHT0, GL_POSITION, light_position);
        glPopMatrix();
    }
    else {
        glDisable(GL_LIGHTING);
    }


    glCallList(gwy3D->shape_list_base + gwy3D->shape_current);
    gwy_3d_draw_axes(gwy3D);

    if (gwy3D->movement_status == GWY_3D_LIGHT_MOVEMENT
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

    gwy_debug(" ");

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    gwy3dview = GWY_3D_VIEW(widget);

    gwy3dview->mouse_begin_x = event->x;
    gwy3dview->mouse_begin_y = event->y;

/*    if ((event->button == 1 || event->button == 2)
        && gwy3dview->shape_current == GWY_3D_SHAPE_AFM)
        gwy3dview->shape_current = GWY_3D_SHAPE_REDUCED;
*/
    return FALSE;
}

static gboolean
gwy_3d_view_button_release(GtkWidget *widget,
                               GdkEventButton *event)
{
    Gwy3DView *gwy3dview;

    gwy_debug(" ");

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    gwy3dview = GWY_3D_VIEW(widget);

    return FALSE;
}

static gboolean
gwy_3d_view_motion_notify(GtkWidget *widget,
                              GdkEventMotion *event)
{
    Gwy3DView *gwy3dview;
    float h = widget->allocation.height;
    float x = event->x;
    float y = event->y;

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    g_return_val_if_fail(event, FALSE);

    gwy3dview = GWY_3D_VIEW(widget);
    gwy_debug("motion event: (%f, %f), shape=%d",
              event->x, event->y, gwy3dview->shape_current);

    /* Rotation. */
    if (event->state & GDK_BUTTON1_MASK)
        switch (gwy3dview->movement_status)
        {
            case GWY_3D_NONE:
            break;

            case GWY_3D_ROTATION:
                gtk_adjustment_set_value(gwy3dview->rot_x,
                                         gwy3dview->rot_x->value
                                         + x - gwy3dview->mouse_begin_x);
                gtk_adjustment_set_value(gwy3dview->rot_y,
                                         gwy3dview->rot_y->value
                                         + y - gwy3dview->mouse_begin_y);
                break;

            case GWY_3D_SCALE:
                gtk_adjustment_set_value(gwy3dview->view_scale,
                                        gwy3dview->view_scale->value
                                        * (1.0 + (y - gwy3dview->mouse_begin_y) / h));
                if (gwy3dview->view_scale->value > gwy3dview->view_scale_max)
                    gtk_adjustment_set_value(gwy3dview->view_scale, gwy3dview->view_scale_max);
                else if (gwy3dview->view_scale->value < gwy3dview->view_scale_min)
                    gtk_adjustment_set_value(gwy3dview->view_scale, gwy3dview->view_scale_min);
                break;

            case GWY_3D_DEFORMATION:
            {
                register int i;
                double dz = gwy3dview->deformation_z->value;
                if (y - gwy3dview->mouse_begin_y > 0)
                    for (i = 0; i < y - gwy3dview->mouse_begin_y; i++)
                        dz /= GWY_3D_Z_DEFORMATION;
                else
                    for (i = 0; i < gwy3dview->mouse_begin_y - y; i++)
                        dz *= GWY_3D_Z_DEFORMATION;
                gtk_adjustment_set_value(gwy3dview->deformation_z, dz);
                break;
            }
            case GWY_3D_LIGHT_MOVEMENT:
                gtk_adjustment_set_value(gwy3dview->light_z,
                                         gwy3dview->light_z->value
                                         + x - gwy3dview->mouse_begin_x);
                gtk_adjustment_set_value(gwy3dview->light_y,
                                         gwy3dview->light_y->value
                                         + y - gwy3dview->mouse_begin_y);
                break;
        }

    gwy3dview->mouse_begin_x = x;
    gwy3dview->mouse_begin_y = y;
    return FALSE;
}



static Gwy3DVector *
gwy_3d_make_normals (GwyDataField * data, Gwy3DVector *normals)
{
   typedef struct { Gwy3DVector A, B; } RectangleNorm;
   gint i, j, xres, yres;
   RectangleNorm * norms;

   gwy_debug(" ");

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

   gwy_debug(" ");

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
           /*TODO solve not enough momory problem*/
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
    GLfloat rx = widget->rot_x->value - ((int)(widget->rot_x->value / 360.0)) * 360.0;
    GLfloat Ax, Ay, Bx, By, Cx, Cy;
    gboolean yfirst = TRUE;
/*    gchar text[50];*/
    gint xres = gwy_data_field_get_xres(widget->data);
    gint yres = gwy_data_field_get_yres(widget->data);
    GwyGLMaterial * mat_none = gwy_gl_material_get_by_name(GWY_GL_MATERIAL_NONE);

    gwy_debug(" ");

    if (rx < 0.0)
        rx += 360.0;
    Ax = Ay = Bx = By = Cx = Cy = 0.0f;

    glPushMatrix();
    glTranslatef(-1.0, -1.0, GWY_3D_Z_DISPLACEMENT);
    glScalef(2.0/ xres,
             2.0/ yres,
             GWY_3D_Z_TRANSFORMATION / (widget->data_max - widget->data_min));
    glMaterialfv(GL_FRONT, GL_AMBIENT,  mat_none->ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE,  mat_none->diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_none->specular);
    glMaterialf(GL_FRONT, GL_SHININESS, mat_none->shininess * 128.0);

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
            glTranslatef(Cx*cos(widget->rot_x->value * DIG_2_RAD)
                         - Cy*sin(widget->rot_x->value * DIG_2_RAD),
                         Cx*sin(widget->rot_x->value * DIG_2_RAD)
                         + Cy*cos(widget->rot_x->value * DIG_2_RAD), 0.0f);
            glRotatef(-widget->rot_x->value, 0.0f, 0.0f, 1.0f);
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
        TODO: create bitmaps with labels in the beginning (possibly in init_gl)
              into display lists and draw here
        */
        if (widget->show_labels == TRUE)
        {
#if 0
            gdouble xreal = gwy_data_field_get_xreal(widget->data);
            gdouble yreal = gwy_data_field_get_yreal(widget->data);
            gdouble range, maximum;
            GwySIValueFormat * format;
            guint size;

            size = GTK_WIDGET(widget)->allocation.width
                   < GTK_WIDGET(widget)->allocation.height
                   ? GTK_WIDGET(widget)->allocation.width
                   : GTK_WIDGET(widget)->allocation.height;
            range = fabs(widget->data_max - widget->data_min);
            maximum = MAX(fabs(widget->data_min), fabs(widget->data_max));
            /*FIXME are the parameters all right?*/
            format = gwy_si_unit_get_format_with_resolution(
                         widget->si_unit,
                         yfirst ? yreal : xreal,
                         yfirst ? yreal/3 : xreal/3,
                         NULL);
            g_snprintf(text, sizeof(text), "%c: %1.1f %s",
                       yfirst ? 'y' : 'x',
                       (yfirst ? yreal : xreal)/format->magnitude,
                       format->units);
            gwy_debug("label 1: %s", text);
            gwy_3d_print_text(widget, text, (Ax+2*Bx)/3 - (Cx-Bx)*0.1,
                              (Ay+2*By)/3 - (Cy-By)*0.1, -0.0f, (int) (sqrt(size)*0.8),
                              1, 1, 0, 0, 0.0f);
            gwy_si_unit_get_format_with_resolution(
                widget->si_unit,
                !yfirst ? yreal : xreal,
                !yfirst ? yreal/3 : xreal/3,
                format);
            g_snprintf(text, sizeof(text), "%c: %1.1f %s",
                       !yfirst ? 'y' : 'x',
                       (!yfirst ? yreal : xreal)/format->magnitude,
                       format->units);
            gwy_3d_print_text(widget, text, (2*Bx+Cx)/3 - (Ax-Bx)*0.1,
                          (2*By+Cy)/3 - (Ay-By)*0.1, -0.0f, (int) (sqrt(size)*0.8),
                          1, -1, 0,0, 0.0f);
            gwy_si_unit_get_format_with_resolution(widget->si_unit,
                                                   maximum, range/3, format);
            g_snprintf(text, sizeof(text), "%1.0f %s",
                       widget->data_max / format->magnitude,
                       format->units);
            gwy_3d_print_text(widget, text, Cx - (Ax-Bx)*0.1, Cy - (Ay-By)*0.1,
                          (widget->data_max - widget->data_min), (int) (sqrt(size)*0.8),
                          0, -1, 0, 0, 0.0f);
            g_snprintf(text, sizeof(text), "%1.0f %s",
                       widget->data_min / format->magnitude,
                       format->units);
            gwy_3d_print_text(widget, text, Cx - (Ax-Bx)*0.1, Cy - (Ay-By)*0.1,
                              -0.0f, (int) (sqrt(size)*0.8),
                               0, -1, 0, 0, 0.0f);


            gwy_si_unit_value_format_free(format);
#else
            guint view_size;
            gint size;
            guint label;

            view_size = GTK_WIDGET(widget)->allocation.width
                   < GTK_WIDGET(widget)->allocation.height
                   ? GTK_WIDGET(widget)->allocation.width
                   : GTK_WIDGET(widget)->allocation.height;
            size = (gint) (sqrt(view_size)*0.8);
            if (yfirst)
                label = GWY_3D_VIEW_LABEL_Y;
            else
                label = GWY_3D_VIEW_LABEL_X;
            glColor3f(0.0, 0.0, 0.0);
            gwy_3d_print_text(widget,
                              gwy_3d_labels_expand_text(widget->labels,label),
                              (Ax+2*Bx)/3 - (Cx-Bx)*0.1,
                              (Ay+2*By)/3 - (Cy-By)*0.1, -0.0f,
                              gwy_3d_labels_user_size(widget->labels,
                                                      label,
                                                      size),
                              1, 1,
                              gwy_3d_labels_get_delta_x(widget->labels, label),
                              gwy_3d_labels_get_delta_y(widget->labels, label),
                              gwy_3d_labels_get_rotation(widget->labels, label));
            if (yfirst)
               label = GWY_3D_VIEW_LABEL_X;
            else
               label = GWY_3D_VIEW_LABEL_Y;
            gwy_3d_print_text(widget,
                              gwy_3d_labels_expand_text(widget->labels,label),
                              (2*Bx+Cx)/3 - (Ax-Bx)*0.1,
                              (2*By+Cy)/3 - (Ay-By)*0.1, -0.0f,
                              gwy_3d_labels_user_size(widget->labels,
                                                      label,
                                                      size),
                              1, -1,
                              gwy_3d_labels_get_delta_x(widget->labels, label),
                              gwy_3d_labels_get_delta_y(widget->labels, label),
                              gwy_3d_labels_get_rotation(widget->labels, label));
            gwy_3d_print_text(widget,
                              gwy_3d_labels_expand_text(widget->labels,
                                                     GWY_3D_VIEW_LABEL_MAX),
                              Cx - (Ax-Bx)*0.1, Cy - (Ay-By)*0.1,
                              (widget->data_max - widget->data_min),
                              gwy_3d_labels_user_size(widget->labels,
                                                      GWY_3D_VIEW_LABEL_MAX,
                                                      size),
                              0, -1,
                              gwy_3d_labels_get_delta_x(widget->labels, GWY_3D_VIEW_LABEL_MAX),
                              gwy_3d_labels_get_delta_y(widget->labels, GWY_3D_VIEW_LABEL_MAX),
                              gwy_3d_labels_get_rotation(widget->labels, GWY_3D_VIEW_LABEL_MAX));
            gwy_3d_print_text(widget,
                              gwy_3d_labels_expand_text(widget->labels,
                                                     GWY_3D_VIEW_LABEL_MIN),
                              Cx - (Ax-Bx)*0.1, Cy - (Ay-By)*0.1, 0.0f,
                              gwy_3d_labels_user_size(widget->labels,
                                                      GWY_3D_VIEW_LABEL_MIN,
                                                      size),
                              0, -1,
                              gwy_3d_labels_get_delta_x(widget->labels, GWY_3D_VIEW_LABEL_MIN),
                              gwy_3d_labels_get_delta_y(widget->labels, GWY_3D_VIEW_LABEL_MIN),
                              gwy_3d_labels_get_rotation(widget->labels, GWY_3D_VIEW_LABEL_MIN));
#endif
        }
    }

   glPopMatrix();
}

static void gwy_3d_draw_light_position(Gwy3DView * widget)
{
    int i;
    GLfloat plane_z;
    GwyGLMaterial * mat_none = gwy_gl_material_get_by_name(GWY_GL_MATERIAL_NONE);

    gwy_debug(" ");

    glMaterialfv(GL_FRONT, GL_AMBIENT,  mat_none->ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE,  mat_none->diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_none->specular);
    glMaterialf(GL_FRONT, GL_SHININESS, mat_none->shininess * 128.0);
    glPushMatrix();
    plane_z =   GWY_3D_Z_TRANSFORMATION
              * (widget->data_mean - widget->data_min)
              / (widget->data_max  - widget->data_min)
              + GWY_3D_Z_DISPLACEMENT;
    /*
    FIXME: show light position does not show the real position of the light
    */
    glTranslatef(0.0f, 0.0f, plane_z);
    glRotatef(widget->light_z->value, 0.0f, 0.0f, 1.0f);
    glRotatef(widget->light_y->value, 0.0f, 1.0f, 0.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_QUAD_STRIP);
        for (i = -180; i <= 180; i += 5)
        {
            GLfloat x = cos(i * DIG_2_RAD) * sqrt(2);
            GLfloat z = sin(i * DIG_2_RAD) * sqrt(2);
            glVertex3f( x, 0.05f, z);
            glVertex3f(x, -0.05f, z);
        }
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_LINE_STRIP);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glVertex3f( 0.0, 0.05f, sqrt(2.0));
        glVertex3f(0.0, -0.05f, sqrt(2.0));
        glVertex3f(0.0f, 0.0f, 0.0f);
    glEnd();
    glPopMatrix();
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

    gdk_gl_drawable_gl_end(gldrawable);
    /*** OpenGL END ***/

  return;
}

static void gwy_3d_set_projection(Gwy3DView *widget)
{
    GLfloat w, h;
    GLfloat aspect;

    gwy_debug(" ");

    w = GTK_WIDGET(widget)->allocation.width;
    h = GTK_WIDGET(widget)->allocation.height;

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

static void
gl_pango_ft2_render_layout (PangoLayout *layout)
{
  PangoRectangle logical_rect;
  FT_Bitmap bitmap;
  GLvoid *pixels;
  guint32 *p;
  GLfloat color[4];
  guint32 rgb;
  GLfloat a;
  guint8 *row, *row_end;
  int i;

  pango_layout_get_extents (layout, NULL, &logical_rect);
  if (logical_rect.width == 0 || logical_rect.height == 0)
    return;

  bitmap.rows = PANGO_PIXELS (logical_rect.height);
  bitmap.width = PANGO_PIXELS (logical_rect.width);
  bitmap.pitch = bitmap.width;
  bitmap.buffer = g_malloc (bitmap.rows * bitmap.width);
  bitmap.num_grays = 256;
  bitmap.pixel_mode = ft_pixel_mode_grays;

  memset (bitmap.buffer, 0, bitmap.rows * bitmap.width);
  pango_ft2_render_layout (&bitmap, layout,
                           PANGO_PIXELS (-logical_rect.x), 0);

  pixels = g_malloc (bitmap.rows * bitmap.width * 4);
  p = (guint32 *) pixels;

  glGetFloatv (GL_CURRENT_COLOR, color);
#if !defined(GL_VERSION_1_2) && G_BYTE_ORDER == G_LITTLE_ENDIAN
  rgb =  ((guint32) (color[0] * 255.0))        |
        (((guint32) (color[1] * 255.0)) << 8)  |
        (((guint32) (color[2] * 255.0)) << 16);
#else
  rgb = (((guint32) (color[0] * 255.0)) << 24) |
        (((guint32) (color[1] * 255.0)) << 16) |
        (((guint32) (color[2] * 255.0)) << 8);
#endif
  a = color[3];

  row = bitmap.buffer + bitmap.rows * bitmap.width; /* past-the-end */
  row_end = bitmap.buffer;      /* beginning */

  if (a == 1.0)
    {
      do
        {
          row -= bitmap.width;
          for (i = 0; i < bitmap.width; i++)
#if !defined(GL_VERSION_1_2) && G_BYTE_ORDER == G_LITTLE_ENDIAN
            *p++ = rgb | (((guint32) row[i]) << 24);
#else
            *p++ = rgb | ((guint32) row[i]);
#endif
        }
      while (row != row_end);
    }
  else
    {
      do
        {
          row -= bitmap.width;
          for (i = 0; i < bitmap.width; i++)
#if !defined(GL_VERSION_1_2) && G_BYTE_ORDER == G_LITTLE_ENDIAN
            *p++ = rgb | (((guint32) (a * row[i])) << 24);
#else
            *p++ = rgb | ((guint32) (a * row[i]));
#endif
        }
      while (row != row_end);
    }

  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#if !defined(GL_VERSION_1_2)
  glDrawPixels (bitmap.width, bitmap.rows,
                GL_RGBA, GL_UNSIGNED_BYTE,
                pixels);
#else
  glDrawPixels (bitmap.width, bitmap.rows,
                GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
                pixels);
#endif

  glDisable (GL_BLEND);

  g_free (bitmap.buffer);
  g_free (pixels);
}

static void
gwy_3d_print_text(Gwy3DView      *gwy3dview,
                  char           *text,
                  GLfloat        raster_x,
                  GLfloat        raster_y,
                  GLfloat        raster_z,
                  guint          size,
                  gint           vjustify,
                  gint           hjustify,
                  gint           displacement_x,
                  gint           displacement_y,
                  G_GNUC_UNUSED gfloat         rotation)
{
    PangoContext *widget_context;
    PangoFontDescription *font_desc;
    PangoLayout *layout;
    PangoRectangle logical_rect;
    GLfloat text_w, text_h;
    guint hlp = 0;

    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    /* Font */
    /* FIXME: is it possible for pango to write on trasnparent background? */
    widget_context = gtk_widget_get_pango_context(GTK_WIDGET(gwy3dview));
    font_desc = pango_context_get_font_description(widget_context);
    pango_font_description_set_size(font_desc, size * PANGO_SCALE);
    pango_context_set_font_description(gwy3dview->ft2_context, font_desc);

    /* Text layout */
    layout = pango_layout_new(gwy3dview->ft2_context);
    pango_layout_set_width(layout, -1);
    pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
    pango_layout_set_text(layout, text, -1);
    /* TODO: use Pango to rotate text, after Pango is capable doing it */

    /* Text position */
    pango_layout_get_extents(layout, NULL, &logical_rect);
    text_w = PANGO_PIXELS(logical_rect.width);
    text_h = PANGO_PIXELS(logical_rect.height);


    glRasterPos3f(raster_x, raster_y, raster_z);
    glBitmap(0, 0, 0, 0, displacement_x, displacement_y, (GLubyte *)&hlp);
    if (vjustify < 0)
    {
       /* vertically justified to the bottom */
    }
    else if (vjustify == 0)
    {
       /* vertically justified to the middle */
       glBitmap(0, 0, 0, 0, 0, - text_h / 2, (GLubyte *)&hlp);
    }
    else
    {
       /* vertically justified to the top */
       glBitmap(0, 0, 0, 0, 0, - text_h , (GLubyte *)&hlp);
    }
    if (hjustify < 0)
    {
       /* horizontally justified to the left */
    }
    else if (hjustify == 0)
    {
       /* horizontally justified to the middle */
       glBitmap(0, 0, 0, 0, - text_w / 2, 0, (GLubyte *)&hlp);
    }
    else
    {
       /* horizontally justified to the right */
       glBitmap(0, 0, 0, 0, - text_w , 0, (GLubyte *)&hlp);
    }

    /* Render text */
    gl_pango_ft2_render_layout (layout);



    g_object_unref (G_OBJECT (layout));

    return ;
}

/************************** Documentation ****************************/
/**
 * Gwy3DMovement:
 * @GWY_3D_NONE: View cannot be changed by user.
 * @GWY_3D_ROTATION: View can be rotated.
 * @GWY_3D_SCALE: View can be scaled.
 * @GWY_3D_DEFORMATION: View can be scaled.
 * @GWY_3D_LIGHT_MOVEMENT: Light position can be changed.
 *
 * The type of 3D view change that happens when user drags it with mouse.
 */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
