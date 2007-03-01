/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwygraphcurves.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwydgetutils.h>
#include "gwygraphareadialog.h"

/* The GtkTargetEntry for tree model drags.
 * FIXME: Is it Gtk+ private or what? */
#define GTK_TREE_MODEL_ROW \
    { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_APP, 0 }

enum {
    HANDLER_NOTIFY_N_CURVES,
    HANDLER_CURVE_NOTIFY,
    NHANDLERS
};

/* XXX: Hardcoded gwygraphareadialog model columns */
enum {
    COMBO_COLUMN_VALUE,
    COMBO_COLUMN_NAME,
    COMBO_COLUMN_PIXBUF
};

enum {
    COLUMN_DESCRIPTION,
    COLUMN_MODE,
    COLUMN_COLOR,
    COLUMN_POINT_TYPE,
    COLUMN_LINE_STYLE,
    NVIEW_COLUMNS
};

enum {
    COLUMN_ID,
    COLUMN_MODEL,
    NMODEL_COLUMNS
};

static void     gwy_graph_curves_destroy         (GtkObject *object);
static void     gwy_graph_curves_finalize        (GObject *object);
static void     gwy_graph_curves_realize         (GtkWidget *widget);
static gboolean gwy_graph_curves_key_press       (GtkWidget *widget,
                                                  GdkEventKey *event);
static void     gwy_graph_curves_n_curves_changed(GwyGraphCurves *graph_curves);
static void     gwy_graph_curves_curve_notify    (GwyGraphCurves *graph_curves,
                                                  gint id,
                                                  const GParamSpec *pspec);
static void     gwy_graph_curves_setup           (GwyGraphCurves *graph_curves);

static GQuark quark_id = 0;
static GQuark quark_model = 0;

G_DEFINE_TYPE(GwyGraphCurves, gwy_graph_curves, GTK_TYPE_TREE_VIEW)

static void
gwy_graph_curves_class_init(GwyGraphCurvesClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_graph_curves_finalize;

    object_class->destroy = gwy_graph_curves_destroy;

    widget_class->realize = gwy_graph_curves_realize;
    widget_class->key_press_event = gwy_graph_curves_key_press;
}

static void
gwy_graph_curves_init(GwyGraphCurves *graph_curves)
{
    GtkTreeView *treeview;

    if (!quark_id)
        quark_id = g_quark_from_static_string("gwy-graph-curves-column-id");
    if (!quark_model)
        quark_model = g_quark_from_static_string("gwy-graph-curves-combomodel");

    graph_curves->curves = gtk_list_store_new(NMODEL_COLUMNS,
                                              G_TYPE_INT,
                                              GWY_TYPE_GRAPH_CURVE_MODEL);
    graph_curves->handler_ids = g_new0(gulong, NHANDLERS);

    treeview = GTK_TREE_VIEW(graph_curves);
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(graph_curves->curves));
    gwy_graph_curves_setup(graph_curves);
}

static void
gwy_graph_curves_finalize(GObject *object)
{
    GwyGraphCurves *graph_curves;

    graph_curves = GWY_GRAPH_CURVES(object);

    g_free(graph_curves->handler_ids);
    g_object_unref(graph_curves->curves);

    G_OBJECT_CLASS(gwy_graph_curves_parent_class)->finalize(object);
}

static void
gwy_graph_curves_destroy(GtkObject *object)
{
    GwyGraphCurves *graph_curves;

    graph_curves = GWY_GRAPH_CURVES(object);

    gwy_graph_curves_set_model(graph_curves, NULL);
    gwy_object_unref(graph_curves->pixbuf);

    GTK_OBJECT_CLASS(gwy_graph_curves_parent_class)->destroy(object);
}

static void
gwy_graph_curves_realize(GtkWidget *widget)
{
    GtkTreeViewColumn *column;
    GtkTreeModel *model;

    GTK_WIDGET_CLASS(gwy_graph_curves_parent_class)->realize(widget);

    column = gtk_tree_view_get_column(GTK_TREE_VIEW(widget), COLUMN_POINT_TYPE);
    model = _gwy_graph_get_point_type_store(widget);
    g_object_set_qdata(G_OBJECT(column), quark_model, model);

    column = gtk_tree_view_get_column(GTK_TREE_VIEW(widget), COLUMN_LINE_STYLE);
    model = _gwy_graph_get_line_style_store(widget);
    g_object_set_qdata(G_OBJECT(column), quark_model, model);
}

static gboolean
gwy_graph_curves_key_press(GtkWidget *widget,
                           GdkEventKey *event)
{
    GwyGraphCurves *graph_curves;
    GtkTreeSelection *selection;
    GtkTreePath *path;
    GtkTreeIter iter;
    const gint *indices;

    if (event->keyval == GDK_Delete) {
        graph_curves = GWY_GRAPH_CURVES(widget);
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
        if (graph_curves->graph_model
            && gtk_tree_selection_get_selected(selection, NULL, &iter)) {
            path = gtk_tree_model_get_path(GTK_TREE_MODEL(graph_curves->curves),
                                           &iter);
            indices = gtk_tree_path_get_indices(path);
            gwy_graph_model_remove_curve(graph_curves->graph_model, indices[0]);
            gtk_tree_path_free(path);
        }

        return TRUE;
    }

    return GTK_WIDGET_CLASS(gwy_graph_curves_parent_class)->key_press_event
                                                                 (widget,event);
}

/**
 * gwy_graph_curves_new:
 * @gmodel: A graph model.  It can be %NULL.
 *
 * Creates graph curve list widget based on information in graph model.
 *
 * The #GtkTreeModel and the columns follow the graph model and must not be
 * changed manually.
 *
 * Returns: A new graph curve list.
 *
 * Since: 2.5
 **/
GtkWidget*
gwy_graph_curves_new(GwyGraphModel *gmodel)
{
    GwyGraphCurves *graph_curves;

    graph_curves = g_object_new(GWY_TYPE_GRAPH_CURVES, NULL);
    g_return_val_if_fail(!gmodel || GWY_IS_GRAPH_MODEL(gmodel),
                         (GtkWidget*)graph_curves);

    if (gmodel)
       gwy_graph_curves_set_model(graph_curves, gmodel);

    return (GtkWidget*)graph_curves;
}

/**
 * gwy_graph_curves_set_model:
 * @graph_curves: A graph curve list.
 * @gmodel: New graph model.
 *
 * Changes the graph model a graph curve list.
 *
 * Since: 2.5
 **/
void
gwy_graph_curves_set_model(GwyGraphCurves *graph_curves,
                           GwyGraphModel *gmodel)
{
    guint i;

    g_return_if_fail(GWY_IS_GRAPH_CURVES(graph_curves));
    g_return_if_fail(!gmodel || GWY_IS_GRAPH_MODEL(gmodel));

    if (gmodel == graph_curves->graph_model)
        return;

    for (i = 0; i < NHANDLERS; i++)
        gwy_signal_handler_disconnect(graph_curves->graph_model,
                                      graph_curves->handler_ids[i]);

    gwy_object_unref(graph_curves->graph_model);
    gwy_debug("setting model to: %p", gmodel);
    graph_curves->graph_model = gmodel;
    if (gmodel) {
        g_object_ref(gmodel);
        graph_curves->handler_ids[HANDLER_NOTIFY_N_CURVES]
            = g_signal_connect_swapped
                            (gmodel, "notify::n-curves",
                             G_CALLBACK(gwy_graph_curves_n_curves_changed),
                             graph_curves);
        graph_curves->handler_ids[HANDLER_CURVE_NOTIFY]
            = g_signal_connect_swapped
                            (gmodel, "curve-notify",
                             G_CALLBACK(gwy_graph_curves_curve_notify),
                             graph_curves);
    }
    gwy_graph_curves_n_curves_changed(graph_curves);
}

/**
 * gwy_graph_curves_get_model:
 * @graph_curves: A graph curve list.
 *
 * Gets the graph model a graph curve list displays.
 *
 * Returns: The graph model associated with this #GwyGraphCurves widget.
 *
 * Since: 2.5
 **/
GwyGraphModel*
gwy_graph_curves_get_model(GwyGraphCurves *graph_curves)
{
    g_return_val_if_fail(GWY_IS_GRAPH_CURVES(graph_curves), NULL);

    return graph_curves->graph_model;
}

static void
gwy_graph_curves_n_curves_changed(GwyGraphCurves *graph_curves)
{
    GwyGraphCurveModel *gcmodel;
    GtkTreeModel *model;
    GtkListStore *store;
    GtkTreeIter iter;
    gint i, nrows, ncurves = 0;

    store = graph_curves->curves;
    model = GTK_TREE_MODEL(store);
    nrows = gtk_tree_model_iter_n_children(model, NULL);
    if (graph_curves->graph_model)
        ncurves = gwy_graph_model_get_n_curves(graph_curves->graph_model);
    gwy_debug("old ncurves %u, new ncurves: %u", nrows, ncurves);

    /* Rebuild the model from scratch */
    gtk_tree_model_get_iter_first(model, &iter);
    for (i = 0; i < MAX(ncurves, nrows); i++) {
        if (i < MIN(ncurves, nrows)) {
            gcmodel = gwy_graph_model_get_curve(graph_curves->graph_model, i);
            gtk_list_store_set(store, &iter, COLUMN_MODEL, gcmodel, -1);
            gtk_tree_model_iter_next(model, &iter);
        }
        else if (i < ncurves) {
            gcmodel = gwy_graph_model_get_curve(graph_curves->graph_model, i);
            gtk_list_store_insert_with_values(store, &iter, G_MAXINT,
                                              COLUMN_ID, i,
                                              COLUMN_MODEL, gcmodel,
                                              -1);
        }
        else if (i < nrows)
            gtk_list_store_remove(store, &iter);
        else
            g_assert_not_reached();
    }
}

static void
gwy_graph_curves_curve_notify(GwyGraphCurves *graph_curves,
                              gint id,
                              G_GNUC_UNUSED const GParamSpec *pspec)
{
    gwy_list_store_row_changed(graph_curves->curves, NULL, NULL, id);
}

/* XXX: O(n) */
static gboolean
find_enum_row(GtkTreeModel *model,
              GtkTreeIter *iter,
              gint value)
{
    gint v;

    if (!model || !gtk_tree_model_get_iter_first(model, iter))
        return FALSE;

    do {
        gtk_tree_model_get(model, iter, COMBO_COLUMN_VALUE, &v, -1);
        if (v == value)
            return TRUE;
    } while (gtk_tree_model_iter_next(model, iter));

    return FALSE;
}

static void
render_column(GtkCellLayout *column,
              GtkCellRenderer *renderer,
              GtkTreeModel *model,
              GtkTreeIter *iter,
              gpointer data)
{
    GwyGraphCurves *graph_curves = GWY_GRAPH_CURVES(data);
    GwyGraphCurveModel *gcmodel;
    gint id, v;

    gtk_tree_model_get(model, iter, COLUMN_MODEL, &gcmodel, -1);
    /* Be fault-tolerant */
    if (!gcmodel)
        return;

    id = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(column), quark_id));
    switch (id) {
        case COLUMN_DESCRIPTION:
        {
            gchar *s;

            g_object_get(gcmodel, "description", &s, NULL);
            g_object_set(renderer, "text", s, NULL);
            g_free(s);
        }
        break;

        case COLUMN_COLOR:
        {
            GwyRGBA *color;
            guint32 pixel;

            g_object_get(gcmodel, "color", &color, NULL);
            pixel = 0xff
                   | ((guint32)(guchar)floor(255.99999*color->b) << 8)
                    | ((guint32)(guchar)floor(255.99999*color->g) << 16)
                    | ((guint32)(guchar)floor(255.99999*color->r) << 24);
            gwy_rgba_free(color);
            gdk_pixbuf_fill(graph_curves->pixbuf, pixel);
        }
        break;

        case COLUMN_MODE:
        {
            const GwyEnum *modes;
            const gchar *s;

            g_object_get(gcmodel, "mode", &v, NULL);
            modes = gwy_graph_curve_type_get_enum();
            s = gwy_enum_to_string(v, modes, -1);
            if (s && *s)
                g_object_set(renderer, "text", s, NULL);
        }
        break;

        case COLUMN_POINT_TYPE:
        case COLUMN_LINE_STYLE:
        {
            GtkTreeModel *combomodel;
            GtkTreeIter comboiter;
            GdkPixbuf *pixbuf;
            gchar *s;

            if (id == COLUMN_POINT_TYPE)
                g_object_get(gcmodel, "point-type", &v, NULL);
            else if (id == COLUMN_LINE_STYLE)
                g_object_get(gcmodel, "line-style", &v, NULL);

            combomodel = g_object_get_qdata(G_OBJECT(column), quark_model);
            if (find_enum_row(combomodel, &comboiter, v)) {
                if (GTK_IS_CELL_RENDERER_TEXT(renderer)) {
                    gtk_tree_model_get(combomodel, &comboiter,
                                       COMBO_COLUMN_NAME, &s,
                                       -1);
                    g_object_set(renderer, "text", s, NULL);
                    g_free(s);
                }
                else if (GTK_IS_CELL_RENDERER_PIXBUF(renderer)) {
                    gtk_tree_model_get(combomodel, &comboiter,
                                       COMBO_COLUMN_PIXBUF, &pixbuf,
                                       -1);
                    g_object_set(renderer, "pixbuf", pixbuf, NULL);
                    g_object_unref(pixbuf);
                }
            }
        }
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
gwy_graph_curves_setup(GwyGraphCurves *graph_curves)
{
    static const GtkTargetEntry dnd_target_table[] = { GTK_TREE_MODEL_ROW };

    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeView *treeview;
    GdkPixbuf *pixbuf;
    gint width, height;

    treeview = GTK_TREE_VIEW(graph_curves);

    /* Description */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Description"));
    g_object_set_qdata(G_OBJECT(column), quark_id,
                       GINT_TO_POINTER(COLUMN_DESCRIPTION));
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, FALSE);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                       render_column, graph_curves, NULL);

    /* Mode */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Mode"));
    g_object_set_qdata(G_OBJECT(column), quark_id,
                       GINT_TO_POINTER(COLUMN_MODE));
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, FALSE);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                       render_column, graph_curves, NULL);

    /* Color */
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
    width |= 1;
    height |= 1;
    graph_curves->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                                          GWY_ROUND(1.618*height), height);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Color"));
    g_object_set_qdata(G_OBJECT(column), quark_id,
                       GINT_TO_POINTER(COLUMN_COLOR));
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_pixbuf_new();
    g_object_set(renderer, "pixbuf", graph_curves->pixbuf, NULL);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, FALSE);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                       render_column, graph_curves, NULL);

    /* Point type */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Point Type"));
    g_object_set_qdata(G_OBJECT(column), quark_id,
                       GINT_TO_POINTER(COLUMN_POINT_TYPE));
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_pixbuf_new();
    /* This pixbuf is needed to calculate the column size request correctly,
     * because we set the combo model in realize() and that's too late. */
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    g_object_set(renderer, "pixbuf", pixbuf, NULL);
    g_object_unref(pixbuf);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, FALSE);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                       render_column, graph_curves, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, FALSE);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                       render_column, graph_curves, NULL);

    /* Point type */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Line Style"));
    g_object_set_qdata(G_OBJECT(column), quark_id,
                       GINT_TO_POINTER(COLUMN_LINE_STYLE));
    gtk_tree_view_append_column(treeview, column);

    renderer = gtk_cell_renderer_pixbuf_new();
    /* This pixbuf is needed to calculate the column size request correctly,
     * because we set the combo model in realize() and that's too late. */
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 5*width, height);
    g_object_set(renderer, "pixbuf", pixbuf, NULL);
    g_object_unref(pixbuf);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, FALSE);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                       render_column, graph_curves, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, FALSE);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                       render_column, graph_curves, NULL);

    /* DnD */
    /* XXX: Requires support from the model.  This is unlikely to be
     * implemented in GwyNullStore.  */
    gtk_tree_view_enable_model_drag_source(treeview,
                                           GDK_BUTTON1_MASK,
                                           dnd_target_table,
                                           G_N_ELEMENTS(dnd_target_table),
                                           GDK_ACTION_COPY);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygraphcurves
 * @title: GwyGraphCurves
 * @short_description: Graph curve list
 *
 * #GwyGraphCurves displays the list of #GwyGraphModel curve properties in a
 * table.  While it is a #GtkTreeView, it uses a simplistic tree model
 * and its content is determined by the graph model.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
