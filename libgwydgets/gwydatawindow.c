/* @(#) $Id$ */

#include <math.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <libprocess/datafield.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwydgets.h"

#define GWY_DATA_WINDOW_TYPE_NAME "GwyDataWindow"

#define CBRT2 1.259921049894873164767210607277

/* Forward declarations */

static void     gwy_data_window_class_init     (GwyDataWindowClass *klass);
static void     gwy_data_window_init           (GwyDataWindow *data_window);

static void     measure_changed                (GwyDataWindow *data_window);
static void     lame_window_resize             (GwyDataWindow *data_window);
static void     compute_statusbar_units        (GwyDataWindow *data_window);
static void     gwy_data_view_update_statusbar (GwyDataView *data_view,
                                                GdkEventMotion *event,
                                                GwyDataWindow *data_window);
static void     zoom_changed_cb                (GtkWidget *data_view,
                                                GtkAllocation *allocation,
                                                GwyDataWindow *data_window);
static gboolean color_axis_clicked_cb          (GtkWidget *coloraxis,
                                                GdkEventButton *event,
                                                GtkWidget *data_window);
static void     palette_selected_cb            (GtkWidget *item,
                                                GwyDataWindow *data_window);
static void     data_view_updated_cb           (GwyDataWindow *data_window);

/* Local data */

static const gdouble zoom_factors[] = {
    G_SQRT2,
    CBRT2,
    1.0,
    0.5,
};

static GtkWidgetClass *parent_class = NULL;

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
        gwy_debug("%s", __FUNCTION__);
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
    gwy_debug("%s", __FUNCTION__);

    parent_class = g_type_class_peek_parent(klass);
}

static void
gwy_data_window_init(GwyDataWindow *data_window)
{
    gwy_debug("%s", __FUNCTION__);

    data_window->data_view = NULL;
    data_window->hruler = NULL;
    data_window->vruler = NULL;
    data_window->statusbar = NULL;
    data_window->table = NULL;
    data_window->coloraxis = NULL;
    data_window->zoom_mode = GWY_ZOOM_MODE_HALFPIX;
    data_window->statusbar_context_id = 0;
    data_window->statusbar_message_id = 0;
}

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
    GwyDataWindow *data_window;
    GwyDataViewLayer *layer;
    GwyPalette *palette;
    GtkWidget *vbox, *hbox, *widget;
    GdkGeometry geom = { 10, 10, 1000, 1000, 10, 10, 1, 1, 1.0, 1.0, 0 };

    gwy_debug("%s", __FUNCTION__);

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

    /***** data view *****/
    data_window->data_view = (GtkWidget*)data_view;
    g_signal_connect_after(data_view, "size_allocate",
                           G_CALLBACK(zoom_changed_cb), data_window);
    g_signal_connect_swapped(data_view, "updated",
                             G_CALLBACK(data_view_updated_cb), data_window);

    vbox = gtk_vbox_new(0, FALSE);
    gtk_container_add(GTK_CONTAINER(data_window), vbox);

    hbox = gtk_hbox_new(0, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    /***** statusbar *****/
    data_window->statusbar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), data_window->statusbar, FALSE, FALSE, 0);
    data_window->statusbar_context_id
        = gtk_statusbar_get_context_id(GTK_STATUSBAR(data_window->statusbar),
                                       "coordinates");
    compute_statusbar_units(data_window);
    g_signal_connect(GTK_WIDGET(data_view), "motion_notify_event",
                     G_CALLBACK(gwy_data_view_update_statusbar), data_window);

    /***** main table *****/
    data_window->table = gtk_table_new(2, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), data_window->table, TRUE, TRUE, 0);

    widget = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
    gtk_table_attach(GTK_TABLE(data_window->table), widget,
                     0, 1, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 0);

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
                             G_CALLBACK(GTK_WIDGET_GET_CLASS(data_window->hruler)->motion_notify_event),
                             data_window->hruler);

    data_window->vruler = gwy_vruler_new();
    gwy_ruler_set_units_placement(GWY_RULER(data_window->vruler),
                                  GWY_UNITS_PLACEMENT_NONE);
    g_signal_connect_swapped(GTK_WIDGET(data_view), "motion_notify_event",
                             G_CALLBACK(GTK_WIDGET_GET_CLASS(data_window->vruler)->motion_notify_event),
                             data_window->vruler);
    gtk_table_attach(GTK_TABLE(data_window->table), data_window->vruler,
                     0, 1, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

    /***** rhs stuff *****/
    layer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(data_window->data_view));
    g_assert(GWY_IS_LAYER_BASIC(layer));
    palette = gwy_layer_basic_get_palette(layer);
    data_window->coloraxis = gwy_color_axis_new(GTK_ORIENTATION_VERTICAL,
                                                0, 1, palette);
    data_view_updated_cb(data_window);
    gtk_box_pack_start(GTK_BOX(hbox), data_window->coloraxis,
                       FALSE, FALSE, 0);
    g_signal_connect(data_window->coloraxis, "button_press_event",
                     G_CALLBACK(color_axis_clicked_cb), data_window);

    /* show everything except the table */
    gtk_widget_show_all(vbox);

    g_signal_connect(data_window, "size-allocate",
                     G_CALLBACK(measure_changed), NULL);

    return GTK_WIDGET(data_window);
}

/**
 * gwy_data_window_get_data_view:
 * @data_window: A data view window.
 *
 * Returns the data view widget this data view window currently shows.
 *
 * Returns: The currently shown #GwyDataView.
 **/
GtkWidget*
gwy_data_window_get_data_view(GwyDataWindow *data_window)
{
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), NULL);

    return data_window->data_view;
}

static void
measure_changed(GwyDataWindow *data_window)
{
    gdouble excess, pos, real;
    GwyDataView *data_view;
    GwyContainer *data;
    GwyDataField *dfield;

    data_view = GWY_DATA_VIEW(data_window->data_view);
    data = gwy_data_view_get_data(data_view);
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
lame_window_resize(GwyDataWindow *data_window)
{
    GtkRequisition hruler_req, vruler_req, statusbar_req, coloraxis_req,
                   view_req;
    gint width, height;

    gwy_debug("%s", __FUNCTION__);
    gtk_widget_get_child_requisition(data_window->hruler, &hruler_req);
    gtk_widget_get_child_requisition(data_window->vruler, &vruler_req);
    gtk_widget_get_child_requisition(data_window->statusbar, &statusbar_req);
    gtk_widget_size_request(data_window->coloraxis, &coloraxis_req);
    gtk_widget_size_request(data_window->data_view, &view_req);

    width = vruler_req.width + view_req.width + coloraxis_req.width;
    height = hruler_req.height + view_req.height + statusbar_req.height;
    gtk_window_resize(GTK_WINDOW(data_window), width, height);
}

/**
 * gwy_data_window_set_zoom:
 * @data_window: A data window.
 * @izoom: The new zoom value (as an integer).
 *
 * Sets the zoom of a data window to @izoom.
 *
 * When @izoom is -1 it zooms out; when @izoom is 1 it zooms out.
 * Otherwise the new zoom value is set to @izoom/01000.
 **/
void
gwy_data_window_set_zoom(GwyDataWindow *data_window,
                         gint izoom)
{
    gdouble rzoom, factor;
    gint curzoom = 0;

    gwy_debug("%s: %d", __FUNCTION__, izoom);
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
    lame_window_resize(data_window);
}

/**
 * gwy_data_view_set_zoom_mode:
 * @data_window: A data window.
 * @zoom_mode: A zoom mode to use.
 *
 * Sets the zoom mode @data_window should use to @zoom_mode.
 *
 * It does not affect the current zoom in any way, only its changes in the
 * future.
 **/
void
gwy_data_view_set_zoom_mode(GwyDataWindow *data_window,
                            GwyZoomMode zoom_mode)
{
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    g_return_if_fail(data_window->zoom_mode <= GWY_ZOOM_MODE_HALFPIX);
    data_window->zoom_mode = zoom_mode;
}

/**
 * gwy_data_view_get_zoom_mode:
 * @data_window: A data window.
 *
 * Returns the current zoom mode of a data window @data_window.
 *
 * Returns: The current zoom mode.
 **/
GwyZoomMode
gwy_data_view_get_zoom_mode(GwyDataWindow *data_window)
{
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), 0);
    return data_window->zoom_mode;
}

static void
compute_statusbar_units(GwyDataWindow *data_window)
{
    GwyDataView *data_view;
    GwyDataField *dfield;
    GwyContainer *data;
    gdouble max, unit, mag, xreal, yreal;

    data_view = GWY_DATA_VIEW(data_window->data_view);
    data = gwy_data_view_get_data(data_view);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(dfield),
               yreal/gwy_data_field_get_yres(dfield));
    mag = gwy_math_humanize_numbers(unit, max, &data_window->statusbar_prec);
    gwy_debug("%s: mag = %g", __FUNCTION__, mag);
    data_window->statusbar_mag = mag;
    data_window->statusbar_SI_prefix = gwy_math_SI_prefix(mag);
}

static void
gwy_data_view_update_statusbar(GwyDataView *data_view,
                               GdkEventMotion *event,
                               GwyDataWindow *data_window)
{
    GtkStatusbar *sbar = GTK_STATUSBAR(data_window->statusbar);
    guint id;
    gdouble xreal, yreal;
    gint x, y;
    gchar label[100];

    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    g_snprintf(label, sizeof(label), "%.*f %sm, %.*f %sm",
               data_window->statusbar_prec,
               xreal/data_window->statusbar_mag,
               data_window->statusbar_SI_prefix,
               data_window->statusbar_prec,
               yreal/data_window->statusbar_mag,
               data_window->statusbar_SI_prefix);
    id = gtk_statusbar_push(sbar, data_window->statusbar_context_id, label);
    if (data_window->statusbar_message_id)
        gtk_statusbar_remove(sbar,
                             data_window->statusbar_context_id,
                             data_window->statusbar_message_id);
    data_window->statusbar_message_id = id;
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
    gchar *window_title, *filename, zoomstr[8];
    GwyDataView *data_view;
    GwyContainer *data;
    gdouble zoom;
    gint prec;

    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = GWY_DATA_VIEW(gwy_data_window_get_data_view(data_window));
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    data = gwy_data_view_get_data(data_view);
    g_return_if_fail(GWY_IS_CONTAINER(data));

    if (gwy_container_contains_by_name(data, "/filename")) {
        const gchar *fnm = gwy_container_get_string_by_name(data, "/filename");

        filename = g_path_get_basename(fnm);
    }
    else {
        gint u = gwy_container_get_int32_by_name(data, "/filename/untitled");

        filename = g_strdup_printf(_("Untitled-%d"), u);
    }

    zoom = gwy_data_view_get_zoom(data_view);
    gwy_debug("%s: %g", __FUNCTION__, zoom);
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
}

static void
zoom_changed_cb(GtkWidget *data_view,
                GtkAllocation *allocation,
                GwyDataWindow *data_window)
{
    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    g_return_if_fail(data_window->data_view == data_view);
    gwy_data_window_update_title(data_window);
}

static gboolean
color_axis_clicked_cb(GtkWidget *coloraxis,
                      GdkEventButton *event,
                      GtkWidget *data_window)
{
    GtkWidget *menu;

    if (event->button != 3)
        return FALSE;

    menu = gwy_palette_menu(G_CALLBACK(palette_selected_cb), data_window);
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);
    return FALSE;
}

static void
palette_selected_cb(GtkWidget *item,
                    GwyDataWindow *data_window)
{
    GwyDataViewLayer *layer;
    GwyPalette *palette;
    const gchar *name;

    name = g_object_get_data(G_OBJECT(item), "palette-name");
    gwy_debug("%s: %s", __FUNCTION__, name);
    g_return_if_fail(gwy_palette_def_exists(name));

    layer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(data_window->data_view));
    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
    palette = gwy_layer_basic_get_palette(layer);
    g_return_if_fail(GWY_IS_PALETTE(palette));
    gwy_palette_set_by_name(palette, name);
    gwy_data_view_update(GWY_DATA_VIEW(data_window->data_view));
}

static void
data_view_updated_cb(GwyDataWindow *data_window)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble min, max;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_window->data_view));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    min = gwy_data_field_get_min(dfield);
    max = gwy_data_field_get_max(dfield);
    gwy_color_axis_set_range(GWY_COLOR_AXIS(data_window->coloraxis), min, max);
}

void
gwy_data_window_set_units(GwyDataWindow *data_window,
                          const gchar *units)
{
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    gwy_ruler_set_units(GWY_RULER(data_window->hruler), units);
}

G_CONST_RETURN gchar*
gwy_data_window_get_units(GwyDataWindow *data_window)
{
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), NULL);
    return gwy_ruler_get_units(GWY_RULER(data_window->hruler));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
