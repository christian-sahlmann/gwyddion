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

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libprocess/datafield.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwydgets.h"

#define GWY_3D_WINDOW_TYPE_NAME "Gwy3DWindow"

#define CBRT2 1.259921049894873164767210607277
#define DEFAULT_SIZE 360

enum {
    FOO,
    LAST_SIGNAL
};

/* Forward declarations */

static void     gwy_3d_window_class_init        (Gwy3DWindowClass *klass);
static void     gwy_3d_window_init              (Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_finalize          (GObject *object);
static void     gwy_3d_window_set_mode          (gpointer userdata,
                                                 GtkWidget *button);
/*
static void     gwy_3d_window_measure_changed   (Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_lame_resize       (Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_update_units      (Gwy3DWindow *gwy3dwindow);
static gboolean gwy_data_view_update_statusbar    (GwyDataView *data_view,
                                                   GdkEventMotion *event,
                                                   Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_zoom_changed      (Gwy3DWindow *gwy3dwindow);
static gboolean gwy_3d_window_key_pressed       (Gwy3DWindow *gwy3dwindow,
                                                   GdkEventKey *event);
static gboolean gwy_3d_window_color_axis_clicked(GtkWidget *gwy3dwindow,
                                                   GdkEventButton *event);
static void     gwy_3d_window_palette_selected  (GtkWidget *item,
                                                   Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_data_view_updated (Gwy3DWindow *gwy3dwindow);
*/

/* Local data */

static GtkWidgetClass *parent_class = NULL;

static guint gwy3dwindow_signals[LAST_SIGNAL] = { 0 };

static const gdouble zoom_factors[] = {
    G_SQRT2,
    CBRT2,
    1.0,
    0.5,
};

GType
gwy_3d_window_get_type(void)
{
    static GType gwy_3d_window_type = 0;

    if (!gwy_3d_window_type) {
        static const GTypeInfo gwy_3d_window_info = {
            sizeof(Gwy3DWindowClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_3d_window_class_init,
            NULL,
            NULL,
            sizeof(Gwy3DWindow),
            0,
            (GInstanceInitFunc)gwy_3d_window_init,
            NULL,
        };
        gwy_debug("");
        gwy_3d_window_type = g_type_register_static(GTK_TYPE_WINDOW,
                                                    GWY_3D_WINDOW_TYPE_NAME,
                                                    &gwy_3d_window_info,
                                                    0);
    }

    return gwy_3d_window_type;
}

static void
gwy_3d_window_class_init(Gwy3DWindowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_3d_window_finalize;

    /*
    gwy3dwindow_signals[TITLE_CHANGED] =
        g_signal_new("title_changed",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(Gwy3DWindowClass, title_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
                     */
}

static void
gwy_3d_window_init(Gwy3DWindow *gwy3dwindow)
{
    gwy_debug("");

    gwy3dwindow->gwy3dview = NULL;
    gwy3dwindow->statusbar = NULL;
    gwy3dwindow->zoom_mode = GWY_ZOOM_MODE_HALFPIX;
    gwy3dwindow->statusbar_context_id = 0;
    gwy3dwindow->statusbar_message_id = 0;
}

static void
gwy_3d_window_finalize(GObject *object)
{
    Gwy3DWindow *gwy3dwindow;

    gwy_debug("finalizing a Gwy3DWindow %p (refcount = %u)",
              object, object->ref_count);

    g_return_if_fail(GWY_IS_3D_WINDOW(object));

    gwy3dwindow = GWY_3D_WINDOW(object);
    /*
    g_free(gwy3dwindow->coord_format);
    g_free(gwy3dwindow->value_format);
    */

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_3d_window_new:
 * @data_view: A #GwyDataView containing the data-displaying widget to show.
 *
 * Creates a new data displaying window.
 *
 * Returns: A newly created widget, as #GtkWidget.
 **/
GtkWidget*
gwy_3d_window_new(Gwy3DView *gwy3dview)
{
    Gwy3DWindow *gwy3dwindow;
    GwyPalette *palette;
    GtkWidget *vbox, *hbox, *table, *toolbar, *spin, *button, *omenu, *group,
               *label;
    gint row;
    GtkRequisition size_req;

    gwy_debug("");

    gwy3dwindow = (Gwy3DWindow*)g_object_new(GWY_TYPE_3D_WINDOW, NULL);
    gtk_window_set_wmclass(GTK_WINDOW(gwy3dwindow), "data",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(gwy3dwindow), TRUE);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(gwy3dwindow), hbox);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    gwy3dwindow->gwy3dview = (GtkWidget*)gwy3dview;
    gtk_box_pack_start(GTK_BOX(hbox), gwy3dwindow->gwy3dview, TRUE, TRUE, 0);

    /* Toolbar */
    toolbar = gwy_toolbox_new(4);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                NULL, _("Rotate the data"),
                                NULL, GWY_STOCK_ROTATE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_ROTATION));
    group = button;
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Scale the data"),
                                NULL, GWY_STOCK_SCALE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_SCALE));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Scale value range"),
                                NULL, GWY_STOCK_Z_SCALE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_DEFORMATION));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Scale value range"),
                                NULL, GWY_STOCK_LIGHT_ROTATE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_LIGHT_MOVEMENT));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    /* Parameter table */
    table = gtk_table_new(8, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    row = 0;

    label = gtk_label_new(_("Material:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    omenu = gwy_option_menu_gl_material(NULL, NULL, NULL);
    gtk_table_attach(GTK_TABLE(table), omenu,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    label = gtk_label_new(_("Palette:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    omenu = gwy_option_menu_palette(NULL, NULL, NULL);
    gtk_table_attach(GTK_TABLE(table), omenu,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    /* TODO: meaningful description, don't access 3DView fields directly! */
    spin = gwy_table_attach_spinbutton(table, row++, _("Rot_x"), _("???"),
                                       (GtkObject*)gwy3dview->rot_x);
    spin = gwy_table_attach_spinbutton(table, row++, _("Rot_y"), _("???"),
                                       (GtkObject*)gwy3dview->rot_y);
    spin = gwy_table_attach_spinbutton(table, row++, _("Scale"), "",
                                       (GtkObject*)gwy3dview->view_scale);
    spin = gwy_table_attach_spinbutton(table, row++, _("Value scale"), "",
                                       (GtkObject*)gwy3dview->deformation_z);
    spin = gwy_table_attach_spinbutton(table, row++, _("Light_z"), _("???"),
                                       (GtkObject*)gwy3dview->light_z);
    spin = gwy_table_attach_spinbutton(table, row++, _("Light_y"), _("???"),
                                       (GtkObject*)gwy3dview->light_y);

    gtk_widget_show_all(hbox);

#if 0
    g_signal_connect(gwy3dwindow, "size-allocate",
                     G_CALLBACK(gwy_3d_window_measure_changed), NULL);
    g_signal_connect(gwy3dwindow, "key-press-event",
                     G_CALLBACK(gwy_3d_window_key_pressed), NULL);
#endif

    /* make the 3D view at least DEFAULT_SIZE x DEFAULT_SIZE */
    gtk_widget_size_request(vbox, &size_req);
    size_req.height = MAX(size_req.height, DEFAULT_SIZE);
    gtk_window_set_default_size(GTK_WINDOW(gwy3dwindow),
                                size_req.width + size_req.height,
                                size_req.height);

    return GTK_WIDGET(gwy3dwindow);
}

/**
 * gwy_3d_window_get_data_view:
 * @gwy3dwindow: A data view window.
 *
 * Returns the data view widget this data window currently shows.
 *
 * Returns: The currently shown #GwyDataView.
 **/
GtkWidget*
gwy_3d_window_get_data_view(Gwy3DWindow *gwy3dwindow)
{
    g_return_val_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow), NULL);

    return gwy3dwindow->gwy3dview;
}

static void
gwy_3d_window_set_mode(gpointer userdata, GtkWidget *button)
{
    Gwy3DWindow *gwy3dwindow;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    gwy3dwindow = g_object_get_data(G_OBJECT(button), "gwy3dwindow");
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    gwy_3d_view_set_status(GWY_3D_VIEW(gwy3dwindow->gwy3dview),
                           GPOINTER_TO_INT(userdata));
}

#if 0

/**
 * gwy_3d_window_get_data:
 * @gwy3dwindow: A data view window.
 *
 * Returns the data for the data view this data window currently shows.
 *
 * Returns: The data as #GwyContainer.
 **/
GwyContainer*
gwy_3d_window_get_data(Gwy3DWindow *gwy3dwindow)
{
    g_return_val_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow), NULL);
    g_return_val_if_fail(GWY_IS_DATA_VIEW(gwy3dwindow->data_view), NULL);

    return gwy_data_view_get_data(GWY_DATA_VIEW(gwy3dwindow->data_view));
}

static void
gwy_3d_window_measure_changed(Gwy3DWindow *gwy3dwindow)
{
    gdouble excess, pos, real;
    GwyDataView *data_view;
    GwyContainer *data;
    GwyDataField *dfield;

    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    data_view = GWY_DATA_VIEW(gwy3dwindow->data_view);
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    data = gwy_data_view_get_data(data_view);
    g_return_if_fail(GWY_IS_CONTAINER(data));

    /* TODO Container */
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    g_return_if_fail(dfield);

    /* horizontal */
    real =  gwy_data_field_get_xreal(dfield);
    excess = real * gwy_data_view_get_hexcess(data_view)/2.0;
    gwy_ruler_get_range(GWY_RULER(gwy3dwindow->hruler),
                        NULL, NULL, &pos, NULL);
    gwy_ruler_set_range(GWY_RULER(gwy3dwindow->hruler),
                        -excess, real + excess, pos, real);

    /* vertical */
    real = gwy_data_field_get_yreal(dfield);
    excess = real * gwy_data_view_get_vexcess(data_view)/2.0;
    gwy_ruler_get_range(GWY_RULER(gwy3dwindow->vruler),
                        NULL, NULL, &pos, NULL);
    gwy_ruler_set_range(GWY_RULER(gwy3dwindow->vruler),
                        -excess, real + excess, pos, real);
}

static void
gwy_3d_window_lame_resize(Gwy3DWindow *gwy3dwindow)
{
    GtkRequisition hruler_req, vruler_req, statusbar_req, coloraxis_req,
                   view_req;
    gint width, height;

    gwy_debug("");
    gtk_widget_get_child_requisition(gwy3dwindow->hruler, &hruler_req);
    gtk_widget_get_child_requisition(gwy3dwindow->vruler, &vruler_req);
    gtk_widget_get_child_requisition(gwy3dwindow->statusbar, &statusbar_req);
    gtk_widget_size_request(gwy3dwindow->coloraxis, &coloraxis_req);
    gtk_widget_size_request(gwy3dwindow->data_view, &view_req);

    width = vruler_req.width + view_req.width + coloraxis_req.width;
    height = hruler_req.height + view_req.height + statusbar_req.height;
    gtk_window_resize(GTK_WINDOW(gwy3dwindow), width, height);
}

/**
 * gwy_3d_window_set_zoom:
 * @gwy3dwindow: A data window.
 * @izoom: The new zoom value (as an integer).
 *
 * Sets the zoom of a data window to @izoom.
 *
 * When @izoom is -1 it zooms out; when @izoom is 1 it zooms out.
 * Otherwise the new zoom value is set to @izoom/10000.
 **/
void
gwy_3d_window_set_zoom(Gwy3DWindow *gwy3dwindow,
                         gint izoom)
{
    gdouble rzoom, factor;
    gint curzoom = 0;

    gwy_debug("%d", izoom);
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    g_return_if_fail(izoom == -1 || izoom == 1
                     || (izoom >= 625 && izoom <= 160000));
    g_return_if_fail(gwy3dwindow->zoom_mode <= GWY_ZOOM_MODE_HALFPIX);

    rzoom = gwy_data_view_get_zoom(GWY_DATA_VIEW(gwy3dwindow->data_view));
    factor = zoom_factors[gwy3dwindow->zoom_mode];
    switch (izoom) {
        case -1:
        case 1:
        switch (gwy3dwindow->zoom_mode) {
            case GWY_ZOOM_MODE_SQRT2:
            case GWY_ZOOM_MODE_CBRT2:
            curzoom = floor(log(rzoom)/log(factor) + 0.5);
            break;

            case GWY_ZOOM_MODE_PIX4PIX:
            case GWY_ZOOM_MODE_HALFPIX:
            if (rzoom >= 1)
                curzoom = floor((rzoom - 1.0)/factor + 0.5);
            else
                curzoom = -floor((1.0/rzoom - 1.0)/factor + 0.5);
            break;
        }
        curzoom += izoom;
        switch (gwy3dwindow->zoom_mode) {
            case GWY_ZOOM_MODE_SQRT2:
            case GWY_ZOOM_MODE_CBRT2:
            rzoom = exp(log(factor)*curzoom);
            break;

            case GWY_ZOOM_MODE_PIX4PIX:
            case GWY_ZOOM_MODE_HALFPIX:
            if (curzoom >= 0)
                rzoom = 1.0 + curzoom*factor;
            else
                rzoom = 1.0/(1.0 - curzoom*factor);
            break;
        }
        break;

        default:
        rzoom = izoom/10000.0;
        break;
    }
    rzoom = CLAMP(rzoom, 1/8.0, 8.0);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(gwy3dwindow->data_view), rzoom);
    gwy_3d_window_lame_resize(gwy3dwindow);
}

/**
 * gwy_3d_window_set_zoom_mode:
 * @gwy3dwindow: A data window.
 * @zoom_mode: A zoom mode to use.
 *
 * Sets the zoom mode @gwy3dwindow should use to @zoom_mode.
 *
 * It does not affect the current zoom in any way, only its changes in the
 * future.
 **/
void
gwy_3d_window_set_zoom_mode(Gwy3DWindow *gwy3dwindow,
                              GwyZoomMode zoom_mode)
{
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    g_return_if_fail(gwy3dwindow->zoom_mode <= GWY_ZOOM_MODE_HALFPIX);
    gwy3dwindow->zoom_mode = zoom_mode;
}

/**
 * gwy_3d_window_get_zoom_mode:
 * @gwy3dwindow: A data window.
 *
 * Returns the current zoom mode of a data window @gwy3dwindow.
 *
 * Returns: The current zoom mode.
 **/
GwyZoomMode
gwy_3d_window_get_zoom_mode(Gwy3DWindow *gwy3dwindow)
{
    g_return_val_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow), 0);
    return gwy3dwindow->zoom_mode;
}

static void
gwy_3d_window_update_units(Gwy3DWindow *gwy3dwindow)
{
    GwyDataField *dfield;
    GwyContainer *data;

    gwy_debug("");
    data = gwy_3d_window_get_data(gwy3dwindow);
    g_return_if_fail(GWY_IS_CONTAINER(data));

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_debug("before: coord_format = %p, value_format = %p",
              gwy3dwindow->coord_format, gwy3dwindow->value_format);
    gwy3dwindow->coord_format
        = gwy_data_field_get_value_format_xy(dfield, gwy3dwindow->coord_format);
    gwy3dwindow->value_format
        = gwy_data_field_get_value_format_z(dfield, gwy3dwindow->value_format);
    gwy_debug("after: coord_format = %p, value_format = %p",
              gwy3dwindow->coord_format, gwy3dwindow->value_format);
    gwy_debug("after: coord_format = {%d, %g, %s}, value_format = {%d, %g, %s}",
              gwy3dwindow->coord_format->precision,
              gwy3dwindow->coord_format->magnitude,
              gwy3dwindow->coord_format->units,
              gwy3dwindow->value_format->precision,
              gwy3dwindow->value_format->magnitude,
              gwy3dwindow->value_format->units);
    gwy_ruler_set_units(GWY_RULER(gwy3dwindow->hruler),
                        gwy_data_field_get_si_unit_xy(dfield));
    gwy_ruler_set_units(GWY_RULER(gwy3dwindow->vruler),
                        gwy_data_field_get_si_unit_xy(dfield));
    gwy_color_axis_set_unit(GWY_COLOR_AXIS(gwy3dwindow->coloraxis),
                            gwy_data_field_get_si_unit_z(dfield));
}

static gboolean
gwy_data_view_update_statusbar(GwyDataView *data_view,
                               GdkEventMotion *event,
                               Gwy3DWindow *gwy3dwindow)
{
    static gchar label[128];
    GwyContainer *data;
    GwyDataField *dfield;
    GtkStatusbar *sbar = GTK_STATUSBAR(gwy3dwindow->statusbar);
    guint id;
    gdouble xreal, yreal, value;
    gint x, y;

    if (data_view) {
        x = event->x;
        y = event->y;
        gwy_data_view_coords_xy_clamp(data_view, &x, &y);
        if (x != event->x || y != event->y)
            return FALSE;
        gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
        data = gwy_data_view_get_data(GWY_DATA_VIEW(gwy3dwindow->data_view));
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        gwy_debug("xreal = %g, yreal = %g, xr = %g, yr = %g, xi = %d, yi = %d",
                gwy_data_field_get_xreal(dfield),
                gwy_data_field_get_yreal(dfield),
                xreal, yreal, x, y);
        value = gwy_data_field_get_dval_real(dfield, xreal, yreal,
                                            GWY_INTERPOLATION_ROUND);
    }
    else
        xreal = yreal = value = 0.0;
    g_snprintf(label, sizeof(label), "(%.*f %s, %.*f %s): %.*f %s",
               gwy3dwindow->coord_format->precision,
               xreal/gwy3dwindow->coord_format->magnitude,
               gwy3dwindow->coord_format->units,
               gwy3dwindow->coord_format->precision,
               yreal/gwy3dwindow->coord_format->magnitude,
               gwy3dwindow->coord_format->units,
               gwy3dwindow->value_format->precision,
               value/gwy3dwindow->value_format->magnitude,
               gwy3dwindow->value_format->units);
    id = gtk_statusbar_push(sbar, gwy3dwindow->statusbar_context_id, label);
    if (gwy3dwindow->statusbar_message_id)
        gtk_statusbar_remove(sbar,
                             gwy3dwindow->statusbar_context_id,
                             gwy3dwindow->statusbar_message_id);
    gwy3dwindow->statusbar_message_id = id;

    return FALSE;
}

/**
 * gwy_3d_window_update_title:
 * @gwy3dwindow: A data window.
 *
 * Updates the title of @gwy3dwindow to reflect current state.
 *
 * FIXME: (a) the window title format should be configurable (b) this
 * should probably happen automatically(?).
 **/
void
gwy_3d_window_update_title(Gwy3DWindow *gwy3dwindow)
{
    gchar *window_title, *filename, zoomstr[8];
    GwyDataView *data_view;
    gdouble zoom;
    gint prec;

    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    data_view = GWY_DATA_VIEW(gwy3dwindow->data_view);
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    filename = gwy_3d_window_get_base_name(gwy3dwindow);

    zoom = gwy_data_view_get_zoom(data_view);
    gwy_debug("%g", zoom);
    prec = (zoom == floor(zoom)) ? 0 : 1;
    g_snprintf(zoomstr, sizeof(zoomstr), "%.*f",
               prec, zoom > 1.0 ? zoom : 1.0/zoom);

    window_title = g_strdup_printf("%s %s:%s (%s)",
                                   filename,
                                   zoom > 1.0 ? zoomstr : "1",
                                   zoom > 1.0 ? "1" : zoomstr,
                                   g_get_application_name());
    gtk_window_set_title(GTK_WINDOW(gwy3dwindow), window_title);
    g_free(window_title);
    g_free(filename);

    g_signal_emit(gwy3dwindow, gwy3dwindow_signals[TITLE_CHANGED], 0);
}

/**
 * gwy_3d_window_get_base_name:
 * @gwy3dwindow: A data window.
 *
 * Creates a string usable as a @gwy3dwindow window name/title.
 *
 * This is the prefered data window representation in option menus,
 * infoboxes, etc.
 *
 * Returns: The window name as a newly allocated string.  It should be
 *          freed when no longer needed.
 **/
gchar*
gwy_3d_window_get_base_name(Gwy3DWindow *gwy3dwindow)
{
    GwyContainer *data;
    const gchar *fnm = "Untitled";

    data = gwy_3d_window_get_data(gwy3dwindow);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    if (gwy_container_contains_by_name(data, "/filename")) {
        fnm = gwy_container_get_string_by_name(data, "/filename");
        return g_path_get_basename(fnm);
    }
    else {
        gwy_container_gis_string_by_name(data, "/filename/untitled",
                                         (const guchar**)&fnm);
        return g_strdup(fnm);
    }
}

/**
 * gwy_3d_window_get_ul_corner_widget:
 * @gwy3dwindow: A data window.
 *
 * Returns the upper left corner widget of @gwy3dwindow.
 *
 * Returns: The upper left corner widget as a #GtkWidget, %NULL if there is
 *          no such widget.
 *
 * Since: 1.5.
 **/
GtkWidget*
gwy_3d_window_get_ul_corner_widget(Gwy3DWindow *gwy3dwindow)
{
    g_return_val_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow), NULL);
    return gwy3dwindow->ul_corner;
}

/**
 * gwy_3d_window_set_ul_corner_widget:
 * @gwy3dwindow: A data window.
 * @corner: A widget to set as upper left corner widget, many be %NULL to
 *          just remove any eventual existing one.
 *
 * Sets the widget in upper left corner of a data window to @corner.
 *
 * Since: 1.5.
 **/
void
gwy_3d_window_set_ul_corner_widget(Gwy3DWindow *gwy3dwindow,
                                     GtkWidget *corner)
{
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    g_return_if_fail(!corner || GTK_IS_WIDGET(corner));

    if (gwy3dwindow->ul_corner)
        gtk_widget_unparent(gwy3dwindow->ul_corner);

    if (corner)
        gtk_table_attach(GTK_TABLE(gwy3dwindow->table), corner,
                         0, 1, 0, 1,
                         GTK_FILL, GTK_FILL, 0, 0);
}

static void
gwy_3d_window_zoom_changed(Gwy3DWindow *gwy3dwindow)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    gwy_3d_window_update_title(gwy3dwindow);
}

static gboolean
gwy_3d_window_key_pressed(Gwy3DWindow *gwy3dwindow,
                            GdkEventKey *event)
{
    enum {
        important_mods = GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_RELEASE_MASK
    };
    guint state, key;

    gwy_debug("state = %u, keyval = %u", event->state, event->keyval);
    state = event->state & important_mods;
    key = event->keyval;
    if (!state && (key == GDK_minus || key == GDK_KP_Subtract))
        gwy_3d_window_set_zoom(gwy3dwindow, -1);
    else if (!state && (key == GDK_equal || key == GDK_KP_Equal
                        || key == GDK_plus || key == GDK_KP_Add))
        gwy_3d_window_set_zoom(gwy3dwindow, 1);
    else if (!state && (key == GDK_Z || key == GDK_z || key == GDK_KP_Divide))
        gwy_3d_window_set_zoom(gwy3dwindow, 10000);

    return FALSE;
}

static gboolean
gwy_3d_window_color_axis_clicked(GtkWidget *gwy3dwindow,
                                   GdkEventButton *event)
{
    GtkWidget *menu;

    if (event->button != 3)
        return FALSE;

    menu = gwy_menu_palette(G_CALLBACK(gwy_3d_window_palette_selected),
                            gwy3dwindow);
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);
    return FALSE;
}

static void
gwy_3d_window_palette_selected(GtkWidget *item,
                                 Gwy3DWindow *gwy3dwindow)
{
    GwyPixmapLayer *layer;
    GwyPalette *palette;
    const gchar *name;

    name = g_object_get_data(G_OBJECT(item), "palette-name");
    gwy_debug("%s", name);
    g_return_if_fail(gwy_palette_def_exists(name));

    layer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(gwy3dwindow->data_view));
    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
    palette = gwy_layer_basic_get_palette(GWY_LAYER_BASIC(layer));
    g_return_if_fail(GWY_IS_PALETTE(palette));
    gwy_palette_set_by_name(palette, name);
    gwy_data_view_update(GWY_DATA_VIEW(gwy3dwindow->data_view));
}

static void
gwy_3d_window_data_view_updated(Gwy3DWindow *gwy3dwindow)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble min, max;

    data = gwy_3d_window_get_data(gwy3dwindow);
    g_return_if_fail(GWY_IS_CONTAINER(data));

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    min = gwy_data_field_get_min(dfield);
    max = gwy_data_field_get_max(dfield);
    gwy_color_axis_set_range(GWY_COLOR_AXIS(gwy3dwindow->coloraxis), min, max);
    gwy_3d_window_update_units(gwy3dwindow);
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
