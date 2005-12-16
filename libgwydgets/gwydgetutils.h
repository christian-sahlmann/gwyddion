/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2005 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_GWYDGET_UTILS_H__
#define __GWY_GWYDGET_UTILS_H__

#include <gtk/gtkwidget.h>
#include <libgwydgets/gwydgetenums.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwycolorbutton.h>

G_BEGIN_DECLS

#define gwy_adjustment_get_int(adj) \
    ((gint)(gtk_adjustment_get_value(GTK_ADJUSTMENT(adj)) + 0.5))

GtkWidget*   gwy_table_attach_spinbutton    (GtkWidget *table,
                                             gint row,
                                             const gchar *name,
                                             const gchar *units,
                                             GtkObject *adj);
void         gwy_table_attach_row           (GtkWidget *table,
                                             gint row,
                                             const gchar *name,
                                             const gchar *units,
                                             GtkWidget *middle_widget);
GtkWidget*   gwy_table_attach_hscale        (GtkWidget *table,
                                             gint row,
                                             const gchar *name,
                                             const gchar *units,
                                             GtkObject *pivot,
                                             GwyHScaleStyle style);
void         gwy_table_hscale_set_sensitive (GtkObject *pivot,
                                             gboolean sensitive);

#define      gwy_table_hscale_get_scale     (pivot) \
    ((GtkWidget*)(g_object_get_data(G_OBJECT(pivot), "scale")))
#define      gwy_table_hscale_get_check     (pivot) \
    ((GtkWidget*)(g_object_get_data(G_OBJECT(pivot), "check")))
#define      gwy_table_hscale_get_label     (pivot) \
    ((GtkWidget*)(g_object_get_data(G_OBJECT(pivot), "label")))
#define      gwy_table_hscale_get_units     (pivot) \
    ((GtkWidget*)(g_object_get_data(G_OBJECT(pivot), "units")))
#define      gwy_table_hscale_get_middle_widget(pivot) \
    ((GtkWidget*)(g_object_get_data(G_OBJECT(pivot), "middle_widget")))

GtkWidget*   gwy_table_get_child_widget     (GtkWidget *table,
                                             gint row,
                                             gint col);
void         gwy_color_selector_for_mask    (const gchar *dialog_title,
                                             GwyColorButton *color_button,
                                             GwyContainer *container,
                                             const gchar *prefix);
gboolean     gwy_dialog_prevent_delete_cb   (void);
GtkWidget*   gwy_stock_like_button_new      (const gchar *label_text,
                                             const gchar *stock_id);
GtkWidget*   gwy_tool_like_button_new       (const gchar *label_text,
                                             const gchar *stock_id);
PangoFontMap* gwy_get_pango_ft2_font_map    (gboolean unref);
void         gwy_gdk_cursor_new_or_ref      (GdkCursor **cursor,
                                             GdkCursorType type);
void         gwy_gdk_cursor_free_or_unref   (GdkCursor **cursor);

G_END_DECLS

#endif /* __GWY_GWYDGET_UTILS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
