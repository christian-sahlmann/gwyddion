/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include <libgwyddion/gwymacros.h>

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include "gwydatawindow.h"
#include "gwystatusbar.h"
#include "gwylayer-basic.h"
#include "gwyhruler.h"
#include "gwyvruler.h"
#include "gwycoloraxis.h"
#include "gwyoptionmenus.h"

#define GWY_DATA_WINDOW_TYPE_NAME "GwyDataWindow"

#define CBRT2 1.259921049894873164767210607277

enum {
    TITLE_CHANGED,
    LAST_SIGNAL
};

/* Forward declarations */

static void     gwy_data_window_class_init        (GwyDataWindowClass *klass);
static void     gwy_data_window_init              (GwyDataWindow *data_window);
static void     gwy_data_window_finalize          (GObject *object);
static void     gwy_data_window_measure_changed   (GwyDataWindow *data_window);
static void     gwy_data_window_lame_resize       (GwyDataWindow *data_window);
static void     gwy_data_window_fit_to_screen     (GwyDataWindow *data_window,
                                                   GwyDataView *data_view);
static void     gwy_data_window_update_units      (GwyDataWindow *data_window);
static gboolean gwy_data_window_update_statusbar  (GwyDataView *data_view,
                                                   GdkEventMotion *event,
                                                   GwyDataWindow *data_window);
static void     gwy_data_window_zoom_changed      (GwyDataWindow *data_window);
static gboolean gwy_data_window_key_pressed       (GwyDataWindow *data_window,
                                                   GdkEventKey *event);
static gboolean gwy_data_window_color_axis_clicked(GtkWidget *data_window,
                                                   GdkEventButton *event);
static void     gwy_data_window_gradient_selected (GtkWidget *item,
                                                   GwyDataWindow *data_window);
static void     gwy_data_window_data_view_updated (GwyDataWindow *data_window);

/* Local data */

static GtkWindowClass *parent_class = NULL;

static guint data_window_signals[LAST_SIGNAL] = { 0 };

static const gdouble zoom_factors[] = {
    G_SQRT2,
    CBRT2,
    1.0,
    0.5,
};

GType
gwy_data_window_get_type(void)
{
    static GType gwy_data_window_type = 0;

    if (!gwy_data_window_type) {
        static const GTypeInfo gwy_data_window_info = {
            sizeof(GwyDataWindowClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_data_window_class_init,
            NULL,
            NULL,
            sizeof(GwyDataWindow),
            0,
            (GInstanceInitFunc)gwy_data_window_init,
            NULL,
        };
        gwy_debug(" ");
        gwy_data_window_type = g_type_register_static(GTK_TYPE_WINDOW,
                                                      GWY_DATA_WINDOW_TYPE_NAME,
                                                      &gwy_data_window_info,
                                                      0);
    }

    return gwy_data_window_type;
}

static void
gwy_data_window_class_init(GwyDataWindowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;

    gwy_debug(" ");

    object_class = (GtkObjectClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_data_window_finalize;

    klass->title_changed = NULL;

/**
 * GwyDataWindow::title-changed:
 * @gwydatawindow: The #GwyDataWindow which received the signal.
 *
 * The ::title-changed signal is emitted when the title of #GwyDataWindow
 * changes.
 */
    data_window_signals[TITLE_CHANGED] =
        g_signal_new("title_changed",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyDataWindowClass, title_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

static void
gwy_data_window_init(GwyDataWindow *data_window)
{
    gwy_debug(" ");

    data_window->zoom_mode = GWY_ZOOM_MODE_HALFPIX;
}

static void
gwy_data_window_finalize(GObject *object)
{
    GwyDataWindow *data_window;

    gwy_debug("finalizing a GwyDataWindow %p (refcount = %u)",
              object, object->ref_count);

    g_return_if_fail(GWY_IS_DATA_WINDOW(object));

    data_window = GWY_DATA_WINDOW(object);
    if (data_window->coord_format)
        gwy_si_unit_value_format_free(data_window->coord_format);
    if (data_window->value_format)
        gwy_si_unit_value_format_free(data_window->value_format);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

#define class_motion_notify_callback(x) \
    G_CALLBACK(GTK_WIDGET_GET_CLASS(x)->motion_notify_event)

/**
 * gwy_data_window_new:
 * @data_view: A #GwyDataView containing the data-displaying widget to show.
 *
 * Creates a new data displaying window.
 *
 * Returns: A newly created widget, as #GtkWidget.
 **/
GtkWidget*
gwy_data_window_new(GwyDataView *data_view)
{
    GdkGeometry geom = { 10, 10, 1000, 1000, 10, 10, 1, 1, 1.0, 1.0, 0 };
    GwyDataWindow *data_window;
    GwyPixmapLayer *layer;
    GtkWidget *vbox, *hbox;

    gwy_debug(" ");

    data_window = (GwyDataWindow*)g_object_new(GWY_TYPE_DATA_WINDOW, NULL);
    gtk_window_set_wmclass(GTK_WINDOW(data_window), "data",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(data_window), TRUE);
    /* FIXME: this affects the window, not data_view [Fvwm] */
    /*
    gtk_window_set_geometry_hints(GTK_WINDOW(data_window),
                                  GTK_WIDGET(data_view),
                                  &geom,
                                  GDK_HINT_MIN_SIZE | GDK_HINT_ASPECT);
    */
    gtk_window_set_geometry_hints(GTK_WINDOW(data_window),
                                  GTK_WIDGET(data_view),
                                  &geom,
                                  GDK_HINT_MIN_SIZE);
    gwy_data_window_fit_to_screen(data_window, data_view);

    /***** data view *****/
    data_window->data_view = (GtkWidget*)data_view;
    g_signal_connect_data(data_view, "size_allocate",
                           G_CALLBACK(gwy_data_window_zoom_changed),
                           data_window,
                           NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
    g_signal_connect_swapped(data_view, "updated",
                             G_CALLBACK(gwy_data_window_data_view_updated),
                             data_window);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(data_window), vbox);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    /***** statusbar *****/
    data_window->statusbar = gwy_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), data_window->statusbar, FALSE, FALSE, 0);
    data_window->statusbar_context_id
        = gtk_statusbar_get_context_id(GTK_STATUSBAR(data_window->statusbar),
                                       "coordinates");
    g_signal_connect(GTK_WIDGET(data_view), "motion_notify_event",
                     G_CALLBACK(gwy_data_window_update_statusbar), data_window);

    /***** main table *****/
    data_window->table = gtk_table_new(2, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), data_window->table, TRUE, TRUE, 0);

    gtk_table_attach(GTK_TABLE(data_window->table), data_window->data_view,
                     1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

    /***** rulers *****/
    data_window->hruler = gwy_hruler_new();
    gwy_ruler_set_units_placement(GWY_RULER(data_window->hruler),
                                  GWY_UNITS_PLACEMENT_AT_ZERO);
    gtk_table_attach(GTK_TABLE(data_window->table), data_window->hruler,
                     1, 2, 0, 1,
                     GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);
    g_signal_connect_swapped(GTK_WIDGET(data_view), "motion_notify_event",
                             class_motion_notify_callback(data_window->hruler),
                             data_window->hruler);

    data_window->vruler = gwy_vruler_new();
    gwy_ruler_set_units_placement(GWY_RULER(data_window->vruler),
                                  GWY_UNITS_PLACEMENT_NONE);
    g_signal_connect_swapped(GTK_WIDGET(data_view), "motion_notify_event",
                             class_motion_notify_callback(data_window->vruler),
                             data_window->vruler);
    gtk_table_attach(GTK_TABLE(data_window->table), data_window->vruler,
                     0, 1, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

    /***** rhs stuff *****/
    layer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(data_window->data_view));
    g_assert(GWY_IS_LAYER_BASIC(layer));
    data_window->coloraxis = gwy_color_axis_new(GTK_ORIENTATION_VERTICAL);
    gwy_color_axis_set_gradient
                         (GWY_COLOR_AXIS(data_window->coloraxis),
                          gwy_layer_basic_get_gradient(GWY_LAYER_BASIC(layer)));
    gwy_data_window_data_view_updated(data_window);
    gtk_box_pack_start(GTK_BOX(hbox), data_window->coloraxis,
                       FALSE, FALSE, 0);
    g_signal_connect_swapped(data_window->coloraxis, "button_press_event",
                             G_CALLBACK(gwy_data_window_color_axis_clicked),
                             data_window);

    /* show everything except the table */
    gwy_data_window_update_units(data_window);
    gwy_data_window_update_title(data_window);
    gwy_data_window_update_statusbar(NULL, NULL, data_window);

    gtk_widget_show_all(vbox);

    g_signal_connect(data_window, "size-allocate",
                     G_CALLBACK(gwy_data_window_measure_changed), NULL);
    g_signal_connect(data_window, "key-press-event",
                     G_CALLBACK(gwy_data_window_key_pressed), NULL);

    return GTK_WIDGET(data_window);
}

/**
 * gwy_data_window_get_data_view:
 * @data_window: A data view window.
 *
 * Returns the data view widget this data window currently shows.
 *
 * Returns: The currently shown #GwyDataView.
 **/
GtkWidget*
gwy_data_window_get_data_view(GwyDataWindow *data_window)
{
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), NULL);

    return data_window->data_view;
}

/**
 * gwy_data_window_get_data:
 * @data_window: A data view window.
 *
 * Returns the data for the data view this data window currently shows.
 *
 * Returns: The data as #GwyContainer.
 **/
GwyContainer*
gwy_data_window_get_data(GwyDataWindow *data_window)
{
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), NULL);
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_window->data_view), NULL);

    return gwy_data_view_get_data(GWY_DATA_VIEW(data_window->data_view));
}

static void
gwy_data_window_measure_changed(GwyDataWindow *data_window)
{
    gdouble excess, pos, real;
    GwyDataView *data_view;
    GwyContainer *data;
    GwyDataField *dfield;

    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = GWY_DATA_VIEW(data_window->data_view);
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    data = gwy_data_view_get_data(data_view);
    g_return_if_fail(GWY_IS_CONTAINER(data));

    /* TODO Container */
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    g_return_if_fail(dfield);

    /* horizontal */
    real =  gwy_data_field_get_xreal(dfield);
    excess = real * gwy_data_view_get_hexcess(data_view)/2.0;
    gwy_ruler_get_range(GWY_RULER(data_window->hruler),
                        NULL, NULL, &pos, NULL);
    gwy_ruler_set_range(GWY_RULER(data_window->hruler),
                        -excess, real + excess, pos, real);

    /* vertical */
    real = gwy_data_field_get_yreal(dfield);
    excess = real * gwy_data_view_get_vexcess(data_view)/2.0;
    gwy_ruler_get_range(GWY_RULER(data_window->vruler),
                        NULL, NULL, &pos, NULL);
    gwy_ruler_set_range(GWY_RULER(data_window->vruler),
                        -excess, real + excess, pos, real);
}

static void
gwy_data_window_lame_resize(GwyDataWindow *data_window)
{
    GtkRequisition hruler_req, vruler_req, statusbar_req, coloraxis_req,
                   view_req;
    gint width, height;

    gwy_debug(" ");
    gtk_widget_get_child_requisition(data_window->hruler, &hruler_req);
    gtk_widget_get_child_requisition(data_window->vruler, &vruler_req);
    gtk_widget_get_child_requisition(data_window->statusbar, &statusbar_req);
    gtk_widget_size_request(data_window->coloraxis, &coloraxis_req);
    gtk_widget_size_request(data_window->data_view, &view_req);

    width = vruler_req.width + view_req.width + coloraxis_req.width;
    height = hruler_req.height + view_req.height + statusbar_req.height;
    gtk_window_resize(GTK_WINDOW(data_window), width, height);
}

static void
gwy_data_window_fit_to_screen(GwyDataWindow *data_window,
                              GwyDataView *data_view)
{
    GtkRequisition request;
    GdkScreen *screen;
    gint scrwidth, scrheight;
    gdouble zoom, z;

    screen = gtk_widget_get_screen(GTK_WIDGET(data_window));
    scrwidth = gdk_screen_get_width(screen);
    scrheight = gdk_screen_get_height(screen);

    zoom = gwy_data_view_get_zoom(data_view);
    gtk_widget_size_request(GTK_WIDGET(data_view), &request);
    z = MAX(request.width/(gdouble)scrwidth, request.height/(gdouble)scrheight);
    if (z > 0.85) {
        zoom *= 0.85/z;
        /* TODO: honour zoom mode */
        gwy_data_view_set_zoom(data_view, zoom);
    }
}

/**
 * gwy_data_window_set_zoom:
 * @data_window: A data window.
 * @izoom: The new zoom value (as an integer).
 *
 * Sets the zoom of a data window to @izoom.
 *
 * When @izoom is -1 it zooms out; when @izoom is 1 it zooms out.
 * Otherwise the new zoom value is set to @izoom/10000.
 **/
void
gwy_data_window_set_zoom(GwyDataWindow *data_window,
                         gint izoom)
{
    gdouble rzoom, factor;
    gint curzoom = 0;

    gwy_debug("%d", izoom);
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    g_return_if_fail(izoom == -1 || izoom == 1
                     || (izoom >= 625 && izoom <= 160000));
    g_return_if_fail(data_window->zoom_mode <= GWY_ZOOM_MODE_HALFPIX);

    rzoom = gwy_data_view_get_zoom(GWY_DATA_VIEW(data_window->data_view));
    factor = zoom_factors[data_window->zoom_mode];
    switch (izoom) {
        case -1:
        case 1:
        switch (data_window->zoom_mode) {
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
        switch (data_window->zoom_mode) {
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
    gwy_data_view_set_zoom(GWY_DATA_VIEW(data_window->data_view), rzoom);
    gwy_data_window_lame_resize(data_window);
}

/**
 * gwy_data_window_set_zoom_mode:
 * @data_window: A data window.
 * @zoom_mode: A zoom mode to use.
 *
 * Sets the zoom mode @data_window should use to @zoom_mode.
 *
 * It does not affect the current zoom in any way, only its changes in the
 * future.
 **/
void
gwy_data_window_set_zoom_mode(GwyDataWindow *data_window,
                              GwyZoomMode zoom_mode)
{
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    g_return_if_fail(data_window->zoom_mode <= GWY_ZOOM_MODE_HALFPIX);
    data_window->zoom_mode = zoom_mode;
}

/**
 * gwy_data_window_get_zoom_mode:
 * @data_window: A data window.
 *
 * Returns the current zoom mode of a data window @data_window.
 *
 * Returns: The current zoom mode.
 **/
GwyZoomMode
gwy_data_window_get_zoom_mode(GwyDataWindow *data_window)
{
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), 0);
    return data_window->zoom_mode;
}

static void
gwy_data_window_update_units(GwyDataWindow *data_window)
{
    GwyDataField *dfield;
    GwyContainer *data;

    gwy_debug(" ");
    data = gwy_data_window_get_data(data_window);
    g_return_if_fail(GWY_IS_CONTAINER(data));

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_debug("before: coord_format = %p, value_format = %p",
              data_window->coord_format, data_window->value_format);
    data_window->coord_format
        = gwy_data_field_get_value_format_xy(dfield, data_window->coord_format);
    data_window->value_format
        = gwy_data_field_get_value_format_z(dfield, data_window->value_format);
    gwy_debug("after: coord_format = %p, value_format = %p",
              data_window->coord_format, data_window->value_format);
    gwy_debug("after: coord_format = {%d, %g, %s}, value_format = {%d, %g, %s}",
              data_window->coord_format->precision,
              data_window->coord_format->magnitude,
              data_window->coord_format->units,
              data_window->value_format->precision,
              data_window->value_format->magnitude,
              data_window->value_format->units);
    gwy_ruler_set_units(GWY_RULER(data_window->hruler),
                        gwy_data_field_get_si_unit_xy(dfield));
    gwy_ruler_set_units(GWY_RULER(data_window->vruler),
                        gwy_data_field_get_si_unit_xy(dfield));
    gwy_color_axis_set_si_unit(GWY_COLOR_AXIS(data_window->coloraxis),
                               gwy_data_field_get_si_unit_z(dfield));
}

static gboolean
gwy_data_window_update_statusbar(GwyDataView *data_view,
                                 GdkEventMotion *event,
                                 GwyDataWindow *data_window)
{
    static gchar label[128];
    GwyContainer *data;
    GwyDataField *dfield;
    GtkStatusbar *sbar = GTK_STATUSBAR(data_window->statusbar);
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
        data = gwy_data_view_get_data(GWY_DATA_VIEW(data_window->data_view));
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

    g_snprintf(label, sizeof(label), "(%.*f%s%s, %.*f%s%s): %.*f%s%s",
               data_window->coord_format->precision,
               xreal/data_window->coord_format->magnitude,
               strlen(data_window->coord_format->units) ? " " : "",
               data_window->coord_format->units,
               data_window->coord_format->precision,
               yreal/data_window->coord_format->magnitude,
               strlen(data_window->coord_format->units) ? " " : "",
               data_window->coord_format->units,
               data_window->value_format->precision,
               value/data_window->value_format->magnitude,
               strlen(data_window->value_format->units) ? " " : "",
               data_window->value_format->units);
    id = gtk_statusbar_push(sbar, data_window->statusbar_context_id, label);
    if (data_window->statusbar_message_id)
        gtk_statusbar_remove(sbar,
                             data_window->statusbar_context_id,
                             data_window->statusbar_message_id);
    data_window->statusbar_message_id = id;

    return FALSE;
}

/**
 * gwy_data_window_update_title:
 * @data_window: A data window.
 *
 * Updates the title of @data_window to reflect current state.
 *
 * FIXME: (a) the window title format should be configurable (b) this
 * should probably happen automatically(?).
 **/
void
gwy_data_window_update_title(GwyDataWindow *data_window)
{
    gchar *window_title, *filename;
    gchar zoomstr[8];
    GwyDataView *data_view;
    GwyContainer *data;
    gdouble zoom;
    gint prec;

    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = GWY_DATA_VIEW(data_window->data_view);
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    data = gwy_data_view_get_data(data_view);

    filename = gwy_data_window_get_base_name(data_window);

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
    gtk_window_set_title(GTK_WINDOW(data_window), window_title);
    g_free(window_title);
    g_free(filename);

    g_signal_emit(data_window, data_window_signals[TITLE_CHANGED], 0);
}

/**
 * gwy_data_window_get_base_name:
 * @data_window: A data window.
 *
 * Creates a string usable as a @data_window window name/title.
 *
 * This is the prefered data window representation in option menus,
 * infoboxes, etc.
 *
 * Returns: The window name as a newly allocated string.  It should be
 *          freed when no longer needed.
 **/
gchar*
gwy_data_window_get_base_name(GwyDataWindow *data_window)
{
    GwyContainer *data;
    const gchar *data_title = NULL;
    const gchar *fnm = "Untitled";
    gchar *s1, *s2;

    data = gwy_data_window_get_data(data_window);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    gwy_container_gis_string_by_name(data, "/filename/title",
                                     (const guchar**)&data_title);
    if (gwy_container_gis_string_by_name(data, "/filename",
                                         (const guchar**)&fnm)) {
        if (!data_title)
            return g_path_get_basename(fnm);
        s1 = g_path_get_basename(fnm);
        s2 = g_strconcat(s1, " [", data_title, "]", NULL);
        g_free(s1);
        return s2;
    }
    else {
        gwy_container_gis_string_by_name(data, "/filename/untitled",
                                         (const guchar**)&fnm);
        if (!data_title)
            return g_strdup(fnm);
        else
            return g_strconcat(fnm, " [", data_title, "]", NULL);
    }
}

/**
 * gwy_data_window_get_ul_corner_widget:
 * @data_window: A data window.
 *
 * Returns the upper left corner widget of @data_window.
 *
 * Returns: The upper left corner widget as a #GtkWidget, %NULL if there is
 *          no such widget.
 **/
GtkWidget*
gwy_data_window_get_ul_corner_widget(GwyDataWindow *data_window)
{
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), NULL);
    return data_window->ul_corner;
}

/**
 * gwy_data_window_set_ul_corner_widget:
 * @data_window: A data window.
 * @corner: A widget to set as upper left corner widget, many be %NULL to
 *          just remove any eventual existing one.
 *
 * Sets the widget in upper left corner of a data window to @corner.
 **/
void
gwy_data_window_set_ul_corner_widget(GwyDataWindow *data_window,
                                     GtkWidget *corner)
{
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    g_return_if_fail(!corner || GTK_IS_WIDGET(corner));

    if (data_window->ul_corner)
        gtk_widget_unparent(data_window->ul_corner);

    if (corner)
        gtk_table_attach(GTK_TABLE(data_window->table), corner,
                         0, 1, 0, 1,
                         GTK_FILL, GTK_FILL, 0, 0);
}

static void
gwy_data_window_zoom_changed(GwyDataWindow *data_window)
{
    gwy_debug(" ");
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    gwy_data_window_update_title(data_window);
}

static gboolean
gwy_data_window_key_pressed(GwyDataWindow *data_window,
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
        gwy_data_window_set_zoom(data_window, -1);
    else if (!state && (key == GDK_equal || key == GDK_KP_Equal
                        || key == GDK_plus || key == GDK_KP_Add))
        gwy_data_window_set_zoom(data_window, 1);
    else if (!state && (key == GDK_Z || key == GDK_z || key == GDK_KP_Divide))
        gwy_data_window_set_zoom(data_window, 10000);

    return FALSE;
}

static gboolean
gwy_data_window_color_axis_clicked(GtkWidget *data_window,
                                   GdkEventButton *event)
{
    GtkWidget *menu;

    if (event->button != 3)
        return FALSE;

    menu = gwy_menu_gradient(G_CALLBACK(gwy_data_window_gradient_selected),
                             data_window);
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);
    return FALSE;
}

static void
gwy_data_window_gradient_selected(GtkWidget *item,
                                  GwyDataWindow *data_window)
{
    GwyPixmapLayer *layer;
    const gchar *name;

    name = g_object_get_data(G_OBJECT(item), "gradient-name");
    gwy_debug("%s", name);

    layer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(data_window->data_view));
    gwy_layer_basic_set_gradient(GWY_LAYER_BASIC(layer), name);
    gwy_color_axis_set_gradient(GWY_COLOR_AXIS(data_window->coloraxis), name);
    /* FIXME: this should happen automatically */
    gwy_data_view_update(GWY_DATA_VIEW(data_window->data_view));
}

static void
gwy_data_window_data_view_updated(GwyDataWindow *data_window)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble min, max;

    gwy_debug(" ");
    data = gwy_data_window_get_data(data_window);
    g_return_if_fail(GWY_IS_CONTAINER(data));

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (!gwy_container_gis_double_by_name(data, "/0/base/min", &min))
        min = gwy_data_field_get_min(dfield);
    if (!gwy_container_gis_double_by_name(data, "/0/base/max", &max))
        max = gwy_data_field_get_max(dfield);
    gwy_color_axis_set_range(GWY_COLOR_AXIS(data_window->coloraxis), min, max);
    gwy_data_window_update_units(data_window);
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
