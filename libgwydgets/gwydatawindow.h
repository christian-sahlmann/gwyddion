/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_DATAWINDOW_H__
#define __GWY_DATAWINDOW_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>

#include <libgwyddion/gwycontainer.h>
#include <libgwyddion/gwysiunit.h>
#include <libgwydgets/gwydgetenums.h>
#include <libgwydgets/gwydataview.h>

G_BEGIN_DECLS

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
    GtkWidget *coloraxis;

    GwyZoomMode zoom_mode;  /* reserved for future use */

    guint statusbar_context_id;
    guint statusbar_message_id;
    GwySIValueFormat *coord_format;
    GwySIValueFormat *value_format;

    GtkWidget *ul_corner;
    gpointer reserved2;
};

struct _GwyDataWindowClass {
    GtkWindowClass parent_class;

    void (*title_changed)(GwyDataWindow *data_window);

    gpointer reserved1;
    gpointer reserved2;
};

GtkWidget*    gwy_data_window_new                  (GwyDataView *data_view);
GType         gwy_data_window_get_type             (void) G_GNUC_CONST;
GtkWidget*    gwy_data_window_get_data_view        (GwyDataWindow *data_window);
GwyContainer* gwy_data_window_get_data             (GwyDataWindow *data_window);
void          gwy_data_window_set_zoom             (GwyDataWindow *data_window,
                                                    gint izoom);
void          gwy_data_window_set_zoom_mode        (GwyDataWindow *data_window,
                                                    GwyZoomMode zoom_mode);
GwyZoomMode   gwy_data_window_get_zoom_mode        (GwyDataWindow *data_window);
void          gwy_data_window_update_title         (GwyDataWindow *data_window);
gchar*        gwy_data_window_get_base_name        (GwyDataWindow *data_window);
GtkWidget*    gwy_data_window_get_ul_corner_widget (GwyDataWindow *data_window);
void          gwy_data_window_set_ul_corner_widget (GwyDataWindow *data_window,
                                                    GtkWidget *corner);

G_END_DECLS

#endif /* __GWY_DATAWINDOW_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
