/* @(#) $Id$ */

#ifndef __GWY_DATAWINDOW_H__
#define __GWY_DATAWINDOW_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

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

typedef struct _GwyDataWindow      GwyDataWindow;
typedef struct _GwyDataWindowClass GwyDataWindowClass;

struct _GwyDataWindow {
    GtkWindow parent_instance;

    GtkWidget *table;
    GtkWidget *data_view;
    GtkWidget *hruler;
    GtkWidget *vruler;
    GtkWidget *statusbar;
    GtkWidget *notebook;
    GtkWidget *sidebox;
    GtkWidget *sidebuttons;
};

struct _GwyDataWindowClass {
    GtkWindowClass parent_class;
};

GtkWidget*       gwy_data_window_new               (GwyDataView *data_view);
GType            gwy_data_window_get_type          (void) G_GNUC_CONST;
GtkWidget*       gwy_data_window_get_data_view     (GwyDataWindow *window);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_DATAWINDOW_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
