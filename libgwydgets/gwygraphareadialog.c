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

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwyscitext.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycolorbutton.h>
#include <libgwydgets/gwygraphareadialog.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphcurvemodel.h>

enum {
    COLUMN_VALUE,
    COLUMN_NAME,
    COLUMN_PIXBUF
};

static void       gwy_graph_area_dialog_destroy    (GtkObject *object);
static gboolean   gwy_graph_area_dialog_delete     (GtkWidget *widget,
                                                    GdkEventAny *event);
static void       gwy_graph_area_dialog_response   (GtkDialog *gtkdialog,
                                                    gint response_id);
static GtkWidget* gwy_graph_combo_box_new          (GtkWidget *parent,
                                                    GCallback callback,
                                                    gpointer cbdata,
                                                    gint last,
                                                    const gchar **labels,
                                                    GCallback realize_cb,
                                                    gint current);
static void       gwy_graph_point_combo_box_realize(GtkWidget *parent,
                                                    GtkWidget *combo);
static void       gwy_graph_line_combo_box_realize (GtkWidget *parent,
                                                    GtkWidget *combo);
static void       pointtype_cb                     (GtkWidget *combo,
                                                    GwyGraphAreaDialog *dialog);
static void       linetype_cb                      (GtkWidget *combo,
                                                    GwyGraphAreaDialog *dialog);
static void       color_change_cb                  (GtkWidget *color_button,
                                                    GwyGraphAreaDialog *dialog);
static void       colorsel_response_cb             (GtkWidget *selector,
                                                    gint response,
                                                    GwyGraphAreaDialog *dialog);
static void       colorsel_changed_cb              (GtkColorSelection *colorsel,
                                                    GwyGraphAreaDialog *dialog);
static void       label_change_cb                  (GwySciText *sci_text,
                                                    GwyGraphAreaDialog *dialog);
static void       refresh                          (GwyGraphAreaDialog *dialog);
static void       curvetype_changed_cb             (GtkWidget *combo,
                                                    GwyGraphAreaDialog *dialog);
static void       thickness_changed_cb              (GtkAdjustment *adj,
                                                    GwyGraphAreaDialog *dialog);
static void       pointsize_changed_cb             (GtkAdjustment *adj,
                                                    GwyGraphAreaDialog *dialog);

G_DEFINE_TYPE(GwyGraphAreaDialog, _gwy_graph_area_dialog, GTK_TYPE_DIALOG)

static void
_gwy_graph_area_dialog_class_init(GwyGraphAreaDialogClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GtkDialogClass *dialog_class = GTK_DIALOG_CLASS(klass);

    object_class->destroy = gwy_graph_area_dialog_destroy;
    widget_class->delete_event = gwy_graph_area_dialog_delete;
    dialog_class->response = gwy_graph_area_dialog_response;
}

static void
_gwy_graph_area_dialog_init(GwyGraphAreaDialog *dialog)
{
    static const gchar *point_types[] = {
        N_("Square"), N_("Cross"), N_("Circle"), N_("Star"),
        N_("Diagonal cross"), N_("Triangle up"), N_("Triangle down"),
        N_("Diamond"), N_("Full square"), N_("Disc"),
        N_("Full triangle up"), N_("Full triangle down"), N_("Full diamond"),
    };
    GtkWidget *label, *table;
    gint row;

    gwy_debug("");

    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

    table = gtk_table_new(7, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    row = 0;

    dialog->curvetype_menu
        = gwy_enum_combo_box_new(gwy_graph_curve_type_get_enum(), -1,
                                 G_CALLBACK(curvetype_changed_cb), dialog,
                                 0, TRUE);
    gwy_table_attach_hscale(table, row, _("Plot _style:"), NULL,
                            GTK_OBJECT(dialog->curvetype_menu),
                            GWY_HSCALE_WIDGET);
    row++;

    dialog->color_button = gwy_color_button_new();
    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(dialog->color_button),
                                   FALSE);
    gwy_table_attach_hscale(table, row, _("Pl_ot color:"), NULL,
                            GTK_OBJECT(dialog->color_button),
                            GWY_HSCALE_WIDGET_NO_EXPAND);
    g_signal_connect(dialog->color_button, "clicked",
                     G_CALLBACK(color_change_cb), dialog);
    row++;

    dialog->pointtype_menu
        = gwy_graph_combo_box_new(GTK_WIDGET(dialog),
                                  G_CALLBACK(pointtype_cb), dialog,
                                  GWY_GRAPH_POINT_FILLED_DIAMOND,
                                  point_types,
                                  G_CALLBACK(gwy_graph_point_combo_box_realize),
                                  0);
    gwy_table_attach_hscale(table, row, _("Point _type:"), NULL,
                            GTK_OBJECT(dialog->pointtype_menu),
                            GWY_HSCALE_WIDGET);
    row++;

    dialog->pointsize = gtk_adjustment_new(6, 1, 50, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("_Point size:"), "px",
                            dialog->pointsize, 0);
    g_signal_connect(dialog->pointsize, "value-changed",
                     G_CALLBACK(pointsize_changed_cb), dialog);
    row++;

    dialog->linetype_menu
        = gwy_graph_combo_box_new(GTK_WIDGET(dialog),
                                  G_CALLBACK(linetype_cb), dialog,
                                  GDK_LINE_DOUBLE_DASH,
                                  NULL,
                                  G_CALLBACK(gwy_graph_line_combo_box_realize),
                                  0);
    gwy_table_attach_hscale(table, row, _("_Line type:"), NULL,
                            GTK_OBJECT(dialog->linetype_menu),
                            GWY_HSCALE_WIDGET);
    row++;

    dialog->thickness = gtk_adjustment_new(6, 1, 50, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("Line t_hickness:"), "px",
                            dialog->thickness, 0);
    g_signal_connect(dialog->thickness, "value-changed",
                     G_CALLBACK(thickness_changed_cb), dialog);

    row++;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Label text:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    dialog->sci_text = gwy_sci_text_new();
    gtk_container_set_border_width(GTK_CONTAINER(dialog->sci_text), 4);
    g_signal_connect(dialog->sci_text, "edited",
                     G_CALLBACK(label_change_cb), dialog);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                      dialog->sci_text);
    gtk_widget_show_all(dialog->sci_text);

    refresh(dialog);

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    dialog->curve_model = NULL;
    gtk_window_set_title(GTK_WINDOW(dialog), _("Curve Properties"));
}

static gboolean
gwy_graph_area_dialog_delete(GtkWidget *widget,
                             G_GNUC_UNUSED GdkEventAny *event)
{
    GwyGraphAreaDialog *dialog;

    gwy_debug("");

    dialog = GWY_GRAPH_AREA_DIALOG(widget);
    if (dialog->color_dialog)
        gtk_widget_hide(dialog->color_dialog);
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_graph_area_dialog_response(GtkDialog *gtkdialog,
                               G_GNUC_UNUSED gint response_id)
{
    GwyGraphAreaDialog *dialog = GWY_GRAPH_AREA_DIALOG(gtkdialog);

    if (dialog->color_dialog)
        gtk_widget_hide(dialog->color_dialog);
}

static void
pointtype_cb(GtkWidget *combo, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint active;

    if (dialog->curve_model == NULL)
        return;
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
        return;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    gtk_tree_model_get(model, &iter, COLUMN_VALUE, &active, -1);
    g_object_set(cmodel, "point-type", active, NULL);
}

static void
linetype_cb(GtkWidget *combo, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint active;

    if (dialog->curve_model == NULL)
        return;
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
        return;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    gtk_tree_model_get(model, &iter, COLUMN_VALUE, &active, -1);
    g_object_set(cmodel, "line-style", active, NULL);
}

GtkWidget*
_gwy_graph_area_dialog_new()
{
    gwy_debug("");
    return GTK_WIDGET(g_object_new(GWY_TYPE_GRAPH_AREA_DIALOG, NULL));
}

static void
gwy_graph_area_dialog_destroy(GtkObject *object)
{
    GwyGraphAreaDialog *dialog;

    gwy_debug("");

    dialog = GWY_GRAPH_AREA_DIALOG(object);
    if (dialog->color_dialog) {
        gtk_widget_destroy(dialog->color_dialog);
        dialog->color_dialog = NULL;
    }

    GTK_OBJECT_CLASS(_gwy_graph_area_dialog_parent_class)->destroy(object);
}

static GtkWidget*
gwy_graph_combo_box_new(GtkWidget *parent,
                        GCallback callback,
                        gpointer cbdata,
                        gint last,
                        const gchar **labels,
                        GCallback realize_cb,
                        gint current)
{
    GtkListStore *store;
    GtkWidget *combo;
    GtkCellRenderer *renderer;
    GtkTreeIter iter;
    gint i;

    store = gtk_list_store_new(3, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_PIXBUF);
    for (i = 0; i <= last; i++) {
        if (labels)
            gtk_list_store_insert_with_values(store, &iter, G_MAXINT,
                                              COLUMN_VALUE, i,
                                              COLUMN_NAME, _(labels[i]),
                                              -1);
        else
            gtk_list_store_insert_with_values(store, &iter, G_MAXINT,
                                              COLUMN_VALUE, i,
                                              -1);
    }

    combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 1);
    g_object_unref(store);

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, FALSE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combo), renderer,
                                  "pixbuf", COLUMN_PIXBUF);
    if (labels) {
        renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, FALSE);
        gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combo), renderer,
                                      "text", COLUMN_NAME);
    }

    if (current <= GWY_GRAPH_POINT_DIAMOND)
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), current);

    if (realize_cb)
        g_signal_connect(parent, "realize", G_CALLBACK(realize_cb), combo);
    if (callback)
        g_signal_connect(combo, "changed", callback, cbdata);

    return combo;
}

static void
gwy_graph_point_combo_box_realize(GtkWidget *parent,
                                  GtkWidget *combo)
{
    static const GwyRGBA fg = { 0.0, 0.0, 0.0, 1 };
    GdkColor bg = { -1, 0xffff, 0xffff, 0xffff };
    GdkPixbuf *pixbuf;
    GdkPixmap *pixmap;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GdkGC *gc;
    gint width, height, i, size;

    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
    width |= 1;
    height |= 1;
    gc = gdk_gc_new(parent->window);
    pixmap = gdk_pixmap_new(parent->window, width, height, -1);

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
    gtk_tree_model_get_iter_first(model, &iter);
    size = (3*height/4 - 1) | 1;
    do {
        gtk_tree_model_get(model, &iter, COLUMN_VALUE, &i, -1);

        gdk_gc_set_rgb_fg_color(gc, &bg);
        gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, width, height);
        gwy_graph_draw_point(pixmap, gc, width/2, height/2, i, size, &fg);

        pixbuf = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE(pixmap), NULL,
                                              0, 0, 0, 0, width, height);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COLUMN_PIXBUF, pixbuf, -1);
        g_object_unref(pixbuf);
    } while (gtk_tree_model_iter_next(model, &iter));

    g_object_unref(pixmap);
    g_object_unref(gc);
}

static void
gwy_graph_line_combo_box_realize(GtkWidget *parent,
                                 GtkWidget *combo)
{
    static const GwyRGBA fg = { 0.0, 0.0, 0.0, 1 };
    GdkColor bg = { -1, 0xffff, 0xffff, 0xffff };
    GdkPixbuf *pixbuf;
    GdkPixmap *pixmap;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GdkGC *gc;
    gint width, height, i;

    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
    width = 5*height;
    gc = gdk_gc_new(parent->window);
    pixmap = gdk_pixmap_new(parent->window, width, height, -1);

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
    gtk_tree_model_get_iter_first(model, &iter);
    do {
        gtk_tree_model_get(model, &iter, COLUMN_VALUE, &i, -1);

        gdk_gc_set_rgb_fg_color(gc, &bg);
        gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, width, height);
        gwy_graph_draw_line(pixmap, gc, 1, height/2, width - 1, height/2,
                            i, /*XXX line width*/ 3, &fg);

        pixbuf = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE(pixmap), NULL,
                                              0, 0, 0, 0, width, height);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COLUMN_PIXBUF, pixbuf, -1);
        g_object_unref(pixbuf);
    } while (gtk_tree_model_iter_next(model, &iter));

    g_object_unref(pixmap);
    g_object_unref(gc);
}

static void
refresh(GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;

    if (dialog->curve_model == NULL)
        return;

    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);

    gwy_color_button_set_color(GWY_COLOR_BUTTON(dialog->color_button),
                               &cmodel->color);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(dialog->curvetype_menu),
                                  cmodel->mode);
    gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->pointtype_menu),
                             cmodel->point_type);
    gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->linetype_menu),
                             cmodel->line_style);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(dialog->pointsize),
                             cmodel->point_size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(dialog->thickness),
                             cmodel->line_width);

    gwy_sci_text_set_text(GWY_SCI_TEXT((dialog->sci_text)),
                          cmodel->description->str);
}

static void
color_change_cb(G_GNUC_UNUSED GtkWidget *color_button,
                GwyGraphAreaDialog *dialog)
{
    GdkColor gcl;
    GwyGraphCurveModel *cmodel;
    GtkWidget *selector, *colorsel;

    if (!dialog->curve_model)
        return;

    if (dialog->color_dialog) {
        gtk_window_present(GTK_WINDOW(dialog->color_dialog));
        return;
    }

    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    selector = gtk_color_selection_dialog_new(_("Set Curve Color"));
    dialog->color_dialog = selector;
    dialog->old_color = cmodel->color;

    gtk_dialog_set_has_separator(GTK_DIALOG(selector), FALSE);
    colorsel = GTK_COLOR_SELECTION_DIALOG(selector)->colorsel;
    g_signal_connect(selector, "response",
                     G_CALLBACK(colorsel_response_cb), dialog);
    g_signal_connect(colorsel, "color-changed",
                     G_CALLBACK(colorsel_changed_cb), dialog);
    gwy_rgba_to_gdk_color(&cmodel->color, &gcl);
    gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &gcl);
    gtk_widget_show(selector);
}

static void
colorsel_response_cb(GtkWidget *selector,
                     gint response,
                     GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    GtkWidget *colorsel;

    if (!dialog->curve_model)
        return;

    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    colorsel = GTK_COLOR_SELECTION_DIALOG(selector)->colorsel;
    if (response == GTK_RESPONSE_CANCEL) {
        g_object_set(cmodel, "color", &dialog->old_color, NULL);
        refresh(dialog);
    }
    gtk_widget_destroy(selector);
    dialog->color_dialog = NULL;
}

static void
colorsel_changed_cb(GtkColorSelection *colorsel,
                    GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    GwyRGBA rgba;
    GdkColor gcl;

    if (!dialog->curve_model)
        return;

    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    gtk_color_selection_get_current_color(colorsel, &gcl);
    gwy_rgba_from_gdk_color(&rgba, &gcl);
    rgba.a = 1.0;
    g_object_set(cmodel, "color", &rgba, NULL);
    refresh(dialog);
}

static void
label_change_cb(GwySciText *sci_text, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;

    if (dialog->curve_model == NULL)
        return;
    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    g_object_set(cmodel,
                 "description", gwy_sci_text_get_text(GWY_SCI_TEXT(sci_text)),
                 NULL);
}

void
_gwy_graph_area_dialog_set_curve_data(GtkWidget *dialog,
                                      GwyGraphCurveModel *cmodel)
{
    GwyGraphAreaDialog *gadialog = GWY_GRAPH_AREA_DIALOG(dialog);

    gadialog->curve_model = cmodel;
    if (gadialog->color_dialog) {
        GtkWidget *colorsel;
        GdkColor gcl;

        gadialog->old_color = cmodel->color;
        colorsel = GTK_COLOR_SELECTION_DIALOG(gadialog->color_dialog)->colorsel;
        gwy_rgba_to_gdk_color(&cmodel->color, &gcl);
        gtk_color_selection_set_previous_color(GTK_COLOR_SELECTION(colorsel),
                                               &gcl);
        gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel),
                                              &gcl);
    }
    refresh(gadialog);
}

static void
curvetype_changed_cb(GtkWidget *combo, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    GwyGraphCurveType ctype;

    if (dialog->curve_model == NULL)
        return;

    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    ctype = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    g_object_set(cmodel, "mode", ctype, NULL);
}

static void
thickness_changed_cb(GtkAdjustment *adj, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;

    if (dialog->curve_model == NULL)
        return;

    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    g_object_set(cmodel, "line-width", gwy_adjustment_get_int(adj), NULL);
}

static void
pointsize_changed_cb(GtkAdjustment *adj, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;

    if (dialog->curve_model == NULL)
        return;

    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    g_object_set(cmodel, "point-size", gwy_adjustment_get_int(adj), NULL);
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
