/* @(#) $Id$ */

#ifndef __GWY_DATAWINDOW_H__
#define __GWY_DATAWINDOW_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>

#ifndef GWY_TYPE_SPHERE_COORDS
#  include <libgwydgets/gwydataview.h>
#endif /* no GWY_TYPE_SPHERE_COORDS */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_DATA_WINDOW            (gwy_data_window_get_type())
#define GWY_DATA_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_WINDOW, GwyDataWindow))
#define GWY_DATA_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_WINDOW, GwyDataWindowClass))
#define GWY_IS_DATA_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_WINDOW))
#define GWY_IS_DATA_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_WINDOW))
#define GWY_DATA_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_WINDOW, GwyDataWindowClass))

typedef enum {
    GWY_ZOOM_MODE_SQRT2,
    GWY_ZOOM_MODE_CBRT2,
    GWY_ZOOM_MODE_PIX4PIX,
    GWY_ZOOM_MODE_HALFPIX
} GwyZoomMode;

typedef struct _GwyDataWindow      GwyDataWindow;
typedef struct _GwyDataWindowClass GwyDataWindowClass;

struct _GwyDataWindow {
    GtkWindow parent_instance;

    GtkWidget *table;
    GtkWidget *data_view;
    GtkWidget *hruler;
    GtkWidget *vruler;
    GtkWidget *statusbar;
    GtkWidget *coloraxis;

    GwyZoomMode zoom_mode;  /* reserved for future use */

    guint statusbar_context_id;
    guint statusbar_message_id;
    gdouble statusbar_mag;
    gint statusbar_prec;
    const gchar *statusbar_SI_prefix;
};

struct _GwyDataWindowClass {
    GtkWindowClass parent_class;
};

GtkWidget*       gwy_data_window_new              (GwyDataView *data_view);
GType            gwy_data_window_get_type         (void) G_GNUC_CONST;
GtkWidget*       gwy_data_window_get_data_view    (GwyDataWindow *data_window);
void             gwy_data_window_set_zoom         (GwyDataWindow *data_window,
                                                   gint izoom);
void             gwy_data_view_set_zoom_mode      (GwyDataWindow *data_window,
                                                   GwyZoomMode zoom_mode);
GwyZoomMode      gwy_data_view_get_zoom_mode      (GwyDataWindow *data_window);
void             gwy_data_window_update_title     (GwyDataWindow *data_window);
void             gwy_data_window_set_units        (GwyDataWindow *data_window,
                                                   const gchar *units);
G_CONST_RETURN
gchar*           gwy_data_window_get_units        (GwyDataWindow *data_window);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_DATAWINDOW_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
