#include <math.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "gwydatawindow.h"

#define GWY_DATA_WINDOW_TYPE_NAME "GwyDataWindow"

#define _(x) x

/* Forward declarations */

static void     gwy_data_window_class_init     (GwyDataWindowClass *klass);
static void     gwy_data_window_init           (GwyDataWindow *data_window);
GtkWidget*      gwy_data_window_new            (GwyDataView *data_view);

/* Local data */

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
        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
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
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
}

static void
gwy_data_window_init(GwyDataWindow *data_window)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    data_window->data_view = NULL;
    data_window->hruler = NULL;
    data_window->vruler = NULL;
    data_window->statusbar = NULL;
    data_window->notebook = NULL;
}

GtkWidget*
gwy_data_window_new(GwyDataView *data_view)
{
    GwyDataWindow *data_window;
    GtkWidget *vbox, *hbox, *table, *widget;
    GdkGeometry geom = { 10, 10, 1000, 1000, 10, 10, 1, 1, 1.0, 1.0, 0 };

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

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

    data_window->data_view = (GtkWidget*)data_view;

    vbox = gtk_vbox_new(0, FALSE);
    gtk_container_add(GTK_CONTAINER(data_window), vbox);

    hbox = gtk_hbox_new(0, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    data_window->statusbar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), data_window->statusbar, FALSE, FALSE, 0);

    table = gtk_table_new(2, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 0);

    widget = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
    gtk_table_attach(GTK_TABLE(table), widget, 0, 1, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 0);

    gtk_table_attach(GTK_TABLE(table), data_window->data_view, 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

    data_window->hruler = gtk_hruler_new();
    gtk_ruler_set_range(GTK_RULER(data_window->hruler),
                        0.0, 100.0, 0.0, 100.0);
    gtk_table_attach(GTK_TABLE(table), data_window->hruler, 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    g_signal_connect_swapped(GTK_WIDGET(data_view), "motion_notify_event",
                             G_CALLBACK(GTK_WIDGET_GET_CLASS(data_window->hruler)->motion_notify_event),
                             data_window->hruler);

    data_window->vruler = gtk_vruler_new();
    gtk_ruler_set_range(GTK_RULER(data_window->vruler),
                        0.0, 100.0, 0.0, 100.0);
    g_signal_connect_swapped(GTK_WIDGET(data_view), "motion_notify_event",
                             G_CALLBACK(GTK_WIDGET_GET_CLASS(data_window->vruler)->motion_notify_event),
                             data_window->vruler);
    gtk_table_attach(GTK_TABLE(table), data_window->vruler, 0, 1, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

    data_window->notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(hbox), data_window->notebook, FALSE, FALSE, 0);
    widget = gtk_label_new("Crash me!");
    gtk_notebook_append_page(GTK_NOTEBOOK(data_window->notebook),
                             widget, NULL);
    widget = gtk_label_new("Crash me too!");
    gtk_notebook_append_page(GTK_NOTEBOOK(data_window->notebook),
                             widget, NULL);

    /* show everything except the table */
    gtk_widget_show_all(vbox);

    return GTK_WIDGET(data_window);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
