/* @(#) $Id$ */

#include <math.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <libprocess/datafield.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwydgets.h"

#define GWY_DATA_WINDOW_TYPE_NAME "GwyDataWindow"

/*#define ZOOM_FACTOR G_SQRT2*/
#define ZOOM_FACTOR 1.259921049894873164767210607277

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

/* Local data */

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
    data_window->notebook = NULL;
    data_window->table = NULL;
    data_window->sidebox = NULL;
    data_window->zoom_mode = 0;
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
    GtkWidget *vbox, *hbox, *widget;
    GdkGeometry geom = { 10, 10, 1000, 1000, 10, 10, 1, 1, 1.0, 1.0, 0 };

    gwy_debug("%s", __FUNCTION__);

    data_window = (GwyDataWindow*)g_object_new(GWY_TYPE_DATA_WINDOW, NULL);
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
    g_signal_connect_after(data_view, "size-allocate",
                           G_CALLBACK(zoom_changed_cb), data_window);

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
                                  GWY_UNITS_PLACEMENT_AT_ZERO);
    g_signal_connect_swapped(GTK_WIDGET(data_view), "motion_notify_event",
                             G_CALLBACK(GTK_WIDGET_GET_CLASS(data_window->vruler)->motion_notify_event),
                             data_window->vruler);
    gtk_table_attach(GTK_TABLE(data_window->table), data_window->vruler,
                     0, 1, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

    /***** rhs stuff TODO *****/
    data_window->sidebox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), data_window->sidebox, FALSE, FALSE, 0);

    data_window->notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(data_window->sidebox), data_window->notebook,
                       TRUE, TRUE, 0);
    widget = gtk_label_new("Crash me!");
    gtk_notebook_append_page(GTK_NOTEBOOK(data_window->notebook),
                             widget, NULL);
    widget = gtk_label_new("Crash me too!");
    gtk_notebook_append_page(GTK_NOTEBOOK(data_window->notebook),
                             widget, NULL);

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
    GtkRequisition hruler_req, vruler_req, statusbar_req, sidebox_req,
                   view_req;
    gint width, height;

    gtk_widget_get_child_requisition(data_window->hruler, &hruler_req);
    gtk_widget_get_child_requisition(data_window->vruler, &vruler_req);
    gtk_widget_get_child_requisition(data_window->statusbar, &statusbar_req);
    gtk_widget_size_request(data_window->sidebox, &sidebox_req);
    gtk_widget_size_request(data_window->data_view, &view_req);

    width = vruler_req.width + view_req.width + sidebox_req.width;
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
    gdouble rzoom;

    gwy_debug("%s: %d", __FUNCTION__, izoom);
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    g_return_if_fail(izoom == -1 || izoom == 1
                     || (izoom >= 625 && izoom <= 160000));

    switch (izoom) {
        case -1:
        rzoom = gwy_data_view_get_zoom(GWY_DATA_VIEW(data_window->data_view))
                / ZOOM_FACTOR;
        break;

        case 1:
        rzoom = gwy_data_view_get_zoom(GWY_DATA_VIEW(data_window->data_view))
                * ZOOM_FACTOR;
        break;

        default:
        rzoom = izoom/10000.0;
        break;
    }
    rzoom = CLAMP(rzoom, 1/8.0, 8.0);
    rzoom = exp(log(ZOOM_FACTOR)*floor(log(rzoom)/log(ZOOM_FACTOR) + 0.5));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(data_window->data_view), rzoom);
    lame_window_resize(data_window);
}

static void
compute_statusbar_units(GwyDataWindow *data_window)
{
    GwyDataView *data_view;
    GwyDataField *dfield;
    GwyContainer *data;
    gdouble mag;

    data_view = GWY_DATA_VIEW(data_window->data_view);
    data = gwy_data_view_get_data(data_view);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    mag = MIN(gwy_data_field_get_xreal(dfield)/gwy_data_field_get_xres(dfield),
              gwy_data_field_get_yreal(dfield)/gwy_data_field_get_yres(dfield));
    mag = exp(floor(log(mag)/G_LN10)*G_LN10);
    data_window->statusbar_mag = mag;
    gwy_debug("%s: %g", __FUNCTION__, data_window->statusbar_mag);
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
    g_snprintf(label, sizeof(label), "%.1f %sm, %.1f %sm",
               xreal/data_window->statusbar_mag,
               data_window->statusbar_SI_prefix,
               yreal/data_window->statusbar_mag,
               data_window->statusbar_SI_prefix);
    id = gtk_statusbar_push(sbar, data_window->statusbar_context_id, label);
    if (data_window->statusbar_message_id)
        gtk_statusbar_remove(sbar,
                             data_window->statusbar_context_id,
                             data_window->statusbar_message_id);
    data_window->statusbar_message_id = id;
}

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

    if (gwy_container_contains_by_name(data, "/filename"))
        filename = g_path_get_basename(
                       gwy_container_get_string_by_name(data, "/filename"));
    else
        filename = g_strdup(_("Untitled"));

    zoom = gwy_data_view_get_zoom(data_view);
    gwy_debug("%s: %g", __FUNCTION__, zoom);
    prec = (zoom == floor(zoom)) ? 0 : 1;
    g_snprintf(zoomstr, sizeof(zoomstr), "%.*f",
               prec, zoom > 1.0 ? zoom : 1.0/zoom);

    window_title = g_strdup_printf("%s %s:%s (Gwyddion)",
                                   filename,
                                   zoom > 1.0 ? zoomstr : "1",
                                   zoom > 1.0 ? "1" : zoomstr);
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


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
