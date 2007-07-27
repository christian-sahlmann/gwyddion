/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
#include <stdarg.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/gwygrainvalue.h>
#include <libgwydgets/gwygrainvaluemenu.h>

static void          render_name                   (GtkTreeViewColumn *column,
                                                    GtkCellRenderer *renderer,
                                                    GtkTreeModel *model,
                                                    GtkTreeIter *iter,
                                                    gpointer user_data);
static void          render_symbol_markup          (GtkTreeViewColumn *column,
                                                    GtkCellRenderer *renderer,
                                                    GtkTreeModel *model,
                                                    GtkTreeIter *iter,
                                                    gpointer user_data);
static void          enabled_activated             (GtkCellRendererToggle *renderer,
                                                    gchar *strpath,
                                                    GtkTreeModel *model);
static gboolean      selection_allowed             (GtkTreeSelection *selection,
                                                    GtkTreeModel *model,
                                                    GtkTreePath *path,
                                                    gboolean path_currently_selected,
                                                    gpointer user_data);
static GtkTreeModel* gwy_grain_value_tree_model_new(gboolean show_id);
static void          inventory_item_updated        (GwyInventory *inventory,
                                                    guint pos,
                                                    GtkTreeStore *store);
static void          inventory_item_inserted       (GwyInventory *inventory,
                                                    guint pos,
                                                    GtkTreeStore *store);
static void          inventory_item_deleted        (GwyInventory *inventory,
                                                    guint pos,
                                                    GtkTreeStore *store);
static gboolean      find_grain_value              (GtkTreeModel *model,
                                                    GwyGrainValue *gvalue,
                                                    GtkTreeIter *iter);
static void          grain_value_store_finalized   (gpointer inventory,
                                                    GObject *store);

enum {
    GWY_GRAIN_VALUE_STORE_COLUMN_ITEM,
    GWY_GRAIN_VALUE_STORE_COLUMN_GROUP,
    GWY_GRAIN_VALUE_STORE_COLUMN_ENABLED
};

typedef struct {
    GtkTreeIter user_group_iter;
    guint user_start_pos;
} GrainValueStorePrivate;

static GQuark priv_quark = 0;

GtkWidget*
gwy_grain_value_tree_view_new(gboolean show_id,
                              const gchar *first_column,
                              ...)
{
    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GtkWidget *widget;
    GtkTreeModel *model;
    va_list ap;

    model = gwy_grain_value_tree_model_new(show_id);
    widget = gtk_tree_view_new_with_model(model);
    treeview = GTK_TREE_VIEW(widget);
    g_object_unref(model);

    /* No data (yet), just a marker */
    g_object_set_qdata(G_OBJECT(treeview), priv_quark, GUINT_TO_POINTER(1));

    va_start(ap, first_column);
    while (first_column) {
        GtkTreeViewColumn *column;
        GtkCellRenderer *renderer;
        gboolean expand;
        const gchar *title;

        column = gtk_tree_view_column_new();
        expand = FALSE;
        if (gwy_strequal(first_column, "name")) {
            renderer = gtk_cell_renderer_text_new();
            gtk_tree_view_column_pack_start(column, renderer, TRUE);
            g_object_set(renderer,
                         "ellipsize-set", TRUE,
                         "weight-set", TRUE,
                         NULL);
            gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                    render_name, NULL,
                                                    NULL);
            title = _("Quantity");
            expand = TRUE;
        }
        else if (gwy_strequal(first_column, "symbol_markup")) {
            renderer = gtk_cell_renderer_text_new();
            gtk_tree_view_column_pack_start(column, renderer, TRUE);
            gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                    render_symbol_markup, NULL,
                                                    NULL);
            title =_("Symbol");
        }
        /*
        else if (gwy_strequal(first_column, "symbol")) {
            renderer = gtk_cell_renderer_text_new();
            gtk_tree_view_column_pack_start(column, renderer, TRUE);
            gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                    render_symbol, NULL,
                                                    NULL);
            title = _("Symbol");
        }
        */
        else if (gwy_strequal(first_column, "enabled")) {
            renderer = gtk_cell_renderer_toggle_new();
            gtk_tree_view_column_pack_start(column, renderer, TRUE);
            g_object_set(renderer, "activatable", TRUE, NULL);
            gtk_tree_view_column_add_attribute
                                       (column, renderer, "active",
                                        GWY_GRAIN_VALUE_STORE_COLUMN_ENABLED);
            g_signal_connect(renderer, "activated",
                             G_CALLBACK(enabled_activated), model);
            title = _("Enabled");
        }
        else {
            g_warning("Unknown column `%s'", first_column);
            title = "Unknonw";
        }

        gtk_tree_view_column_set_title(column, title);
        gtk_tree_view_column_set_expand(column, expand);
        gtk_tree_view_append_column(treeview, column);

        first_column = va_arg(ap, const gchar*);
    }
    va_end(ap);

    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    gtk_tree_selection_set_select_function(selection,
                                           selection_allowed, NULL, NULL);

    return widget;
}

void
gwy_grain_value_tree_view_set_expanded_groups(GtkTreeView *treeview,
                                              guint expanded_bits)
{
    GtkTreeModel *model;
    GtkTreeIter siter;

    g_return_if_fail(GTK_IS_TREE_VIEW(treeview));
    g_return_if_fail(priv_quark
                     && g_object_get_qdata(G_OBJECT(treeview), priv_quark));

    model = gtk_tree_view_get_model(treeview);
    if (!gtk_tree_model_get_iter_first(model, &siter)) {
        g_warning("Grain value tree view is empty?!");
        return;
    }

    do {
        GtkTreePath *path;
        GwyGrainValueGroup group;

        gtk_tree_model_get(model, &siter,
                           GWY_GRAIN_VALUE_STORE_COLUMN_GROUP, &group,
                           -1);
        path = gtk_tree_model_get_path(model, &siter);
        if (expanded_bits & (1 << group))
            gtk_tree_view_expand_row(treeview, path, TRUE);
        else
            gtk_tree_view_collapse_row(treeview, path);
        gtk_tree_path_free(path);
    } while (gtk_tree_model_iter_next(model, &siter));
}

guint
gwy_grain_value_tree_view_get_expanded_groups(GtkTreeView *treeview)
{
    GtkTreeModel *model;
    GtkTreeIter siter;
    guint expanded_bits = 0;

    g_return_val_if_fail(GTK_IS_TREE_VIEW(treeview), 0);
    g_return_val_if_fail(priv_quark
                         && g_object_get_qdata(G_OBJECT(treeview), priv_quark),
                         0);

    model = gtk_tree_view_get_model(treeview);
    if (!gtk_tree_model_get_iter_first(model, &siter)) {
        g_warning("Grain value tree view is empty?!");
        return 0;
    }

    do {
        GwyGrainValueGroup group;
        GtkTreePath *path;

        gtk_tree_model_get(model, &siter,
                           GWY_GRAIN_VALUE_STORE_COLUMN_GROUP, &group,
                           -1);
        path = gtk_tree_model_get_path(model, &siter);
        if (gtk_tree_view_row_expanded(treeview, path))
            expanded_bits |= (1 << group);
        gtk_tree_path_free(path);
    } while (gtk_tree_model_iter_next(model, &siter));

    return expanded_bits;
}

void
gwy_grain_value_tree_view_select(GtkTreeView *treeview,
                                 GwyGrainValue *gvalue)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;

    g_return_if_fail(GTK_IS_TREE_VIEW(treeview));
    g_return_if_fail(GWY_IS_GRAIN_VALUE(gvalue));
    g_return_if_fail(priv_quark
                     && g_object_get_qdata(G_OBJECT(treeview), priv_quark));

    model = gtk_tree_view_get_model(treeview);
    if (!find_grain_value(model, gvalue, &iter)) {
        g_warning("Grain value not in tree model.");
        return;
    }

    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_select_iter(selection, &iter);
    path = gtk_tree_model_get_path(model, &iter);
    gtk_tree_view_scroll_to_cell(treeview, path, NULL, FALSE, 0.0, 0.0);
    gtk_tree_path_free(path);
}

static void
render_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED gpointer user_data)
{
    PangoEllipsizeMode ellipsize;
    PangoWeight weight;
    GwyGrainValue *gvalue;
    GwyGrainValueGroup group;
    const gchar *name;

    gtk_tree_model_get(model, iter,
                       GWY_GRAIN_VALUE_STORE_COLUMN_ITEM, &gvalue,
                       GWY_GRAIN_VALUE_STORE_COLUMN_GROUP, &group,
                       -1);
    ellipsize = gvalue ? PANGO_ELLIPSIZE_END : PANGO_ELLIPSIZE_NONE;
    weight = gvalue ? PANGO_WEIGHT_NORMAL : PANGO_WEIGHT_BOLD;
    if (gvalue) {
        name = gwy_resource_get_name(GWY_RESOURCE(gvalue));
        if (group != GWY_GRAIN_VALUE_GROUP_USER)
            name = gettext(name);
        g_object_unref(gvalue);
    }
    else
        name = gettext(gwy_grain_value_group_name(group));

    g_object_set(renderer,
                 "ellipsize", ellipsize,
                 "weight", weight,
                 "markup", name,
                 NULL);
}

static void
render_symbol_markup(G_GNUC_UNUSED GtkTreeViewColumn *column,
                     GtkCellRenderer *renderer,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer user_data)
{
    GwyGrainValue *gvalue;

    gtk_tree_model_get(model, iter,
                       GWY_GRAIN_VALUE_STORE_COLUMN_ITEM, &gvalue,
                       -1);
    if (gvalue) {
        g_object_set(renderer,
                     "markup", gwy_grain_value_get_symbol_markup(gvalue),
                     NULL);
        g_object_unref(gvalue);
    }
    else
        g_object_set(renderer, "text", "", NULL);
}

static void
enabled_activated(GtkCellRendererToggle *renderer,
                  gchar *strpath,
                  GtkTreeModel *model)
{
    GtkTreePath *path;
    GtkTreeIter iter;
    gboolean enabled;

    path = gtk_tree_path_new_from_string(strpath);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);

    g_object_get(renderer, "active", &enabled, NULL);
    gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
                       GWY_GRAIN_VALUE_STORE_COLUMN_ENABLED, !enabled,
                       -1);
}

static gboolean
selection_allowed(G_GNUC_UNUSED GtkTreeSelection *selection,
                  GtkTreeModel *model,
                  GtkTreePath *path,
                  G_GNUC_UNUSED gboolean path_currently_selected,
                  G_GNUC_UNUSED gpointer user_data)
{
    GtkTreeIter iter;
    GwyGrainValue *gvalue;

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter,
                       GWY_GRAIN_VALUE_STORE_COLUMN_ITEM, &gvalue,
                       -1);
    if (!gvalue)
        return FALSE;

    g_object_unref(gvalue);
    return TRUE;
}

static GtkTreeModel*
gwy_grain_value_tree_model_new(gboolean show_id)
{
    GrainValueStorePrivate *priv;
    GwyInventory *inventory;
    GtkTreeStore *store;
    GtkTreeIter siter, iter;
    GwyGrainValue *gvalue;
    GwyGrainValueGroup group, lastgroup;
    guint i, j, n;

    if (!priv_quark)
        priv_quark = g_quark_from_static_string("gwy-grain-value-chooser-data");

    priv = g_new0(GrainValueStorePrivate, 1);
    store = gtk_tree_store_new(3,
                               GWY_TYPE_GRAIN_VALUE,
                               GWY_TYPE_GRAIN_VALUE_GROUP,
                               G_TYPE_BOOLEAN);
    g_object_set_qdata(G_OBJECT(store), priv_quark, priv);

    inventory = gwy_grain_values();
    n = gwy_inventory_get_n_items(inventory);
    lastgroup = -1;
    for (i = j = 0; i < n; i++) {
        gvalue = gwy_inventory_get_nth_item(inventory, i);
        group = gwy_grain_value_get_group(gvalue);
        if (!show_id && group == GWY_GRAIN_VALUE_GROUP_ID)
            continue;

        if (group != lastgroup) {
            gtk_tree_store_insert_after(store, &siter, NULL, i ? &siter : NULL);
            gtk_tree_store_set(store, &siter,
                               GWY_GRAIN_VALUE_STORE_COLUMN_GROUP, group,
                               -1);
            if (group == GWY_GRAIN_VALUE_GROUP_USER) {
                priv->user_group_iter = siter;
                priv->user_start_pos = i;
            }
            lastgroup = group;
            j = 0;
        }
        gtk_tree_store_insert_after(store, &iter, &siter, j ? &iter : NULL);
        gtk_tree_store_set(store, &iter,
                           GWY_GRAIN_VALUE_STORE_COLUMN_ITEM, gvalue,
                           GWY_GRAIN_VALUE_STORE_COLUMN_GROUP, group,
                           -1);
        j++;
    }

    /* Ensure User branch is present, even if empty */
    if (lastgroup != GWY_GRAIN_VALUE_GROUP_USER) {
        group = GWY_GRAIN_VALUE_GROUP_USER;
        gtk_tree_store_insert_after(store, &siter, NULL, i ? &siter : NULL);
        gtk_tree_store_set(store, &siter,
                           GWY_GRAIN_VALUE_STORE_COLUMN_GROUP, group,
                           -1);
        priv->user_group_iter = siter;
        priv->user_start_pos = i;
    }

    g_signal_connect(inventory, "item-updated",
                     G_CALLBACK(inventory_item_updated), store);
    g_signal_connect(inventory, "item-inserted",
                     G_CALLBACK(inventory_item_inserted), store);
    g_signal_connect(inventory, "item-deleted",
                     G_CALLBACK(inventory_item_deleted), store);
    g_object_weak_ref(G_OBJECT(store), grain_value_store_finalized, inventory);

    return GTK_TREE_MODEL(store);
}

static void
inventory_item_updated(G_GNUC_UNUSED GwyInventory *inventory,
                       guint pos,
                       GtkTreeStore *store)
{
    GrainValueStorePrivate *priv;
    GtkTreeModel *model;
    GtkTreeIter siter, iter;
    GtkTreePath *path;

    priv = g_object_get_qdata(G_OBJECT(store), priv_quark);
    g_return_if_fail(pos >= priv->user_start_pos);
    siter = priv->user_group_iter;

    model = GTK_TREE_MODEL(store);
    gtk_tree_model_iter_nth_child(model, &iter, &siter,
                                  pos - priv->user_start_pos);
    path = gtk_tree_model_get_path(model, &iter);
    gtk_tree_model_row_changed(model, path, &iter);
    gtk_tree_path_free(path);
}

static void
inventory_item_inserted(GwyInventory *inventory,
                        guint pos,
                        GtkTreeStore *store)
{
    GrainValueStorePrivate *priv;
    GwyGrainValue *gvalue;
    GwyGrainValueGroup group;
    GtkTreeIter siter, iter;

    priv = g_object_get_qdata(G_OBJECT(store), priv_quark);
    g_return_if_fail(pos >= priv->user_start_pos);
    siter = priv->user_group_iter;

    gvalue = gwy_inventory_get_nth_item(inventory, pos);
    g_return_if_fail(GWY_IS_GRAIN_VALUE(gvalue));
    group = gwy_grain_value_get_group(gvalue);
    g_return_if_fail(group == GWY_GRAIN_VALUE_GROUP_USER);

    gtk_tree_store_insert(store, &iter, &siter, pos - priv->user_start_pos);
    gtk_tree_store_set(store, &iter,
                       GWY_GRAIN_VALUE_STORE_COLUMN_ITEM, gvalue,
                       GWY_GRAIN_VALUE_STORE_COLUMN_GROUP, group,
                       -1);
}

static void
inventory_item_deleted(G_GNUC_UNUSED GwyInventory *inventory,
                       guint pos,
                       GtkTreeStore *store)
{
    GrainValueStorePrivate *priv;
    GtkTreeIter siter, iter;

    priv = g_object_get_qdata(G_OBJECT(store), priv_quark);
    g_return_if_fail(pos >= priv->user_start_pos);
    siter = priv->user_group_iter;

    gtk_tree_store_insert(store, &iter, &siter, pos - priv->user_start_pos);
    gtk_tree_store_remove(store, &iter);
}

static gboolean
find_grain_value(GtkTreeModel *model,
                 GwyGrainValue *gvalue,
                 GtkTreeIter *iter)
{
    GwyGrainValueGroup group, igroup;
    GwyGrainValue *igvalue;
    GtkTreeIter siter;

    if (!gtk_tree_model_get_iter_first(model, &siter))
        return FALSE;

    group = gwy_grain_value_get_group(gvalue);
    do {
        gtk_tree_model_get(model, &siter,
                           GWY_GRAIN_VALUE_STORE_COLUMN_GROUP, &igroup,
                           -1);
        if (igroup == group)
            break;
    } while (gtk_tree_model_iter_next(model, &siter));

    if (igroup != group
        || !gtk_tree_model_iter_children(model, iter, &siter))
        return FALSE;

    do {
        gtk_tree_model_get(model, iter,
                           GWY_GRAIN_VALUE_STORE_COLUMN_ITEM, &igvalue,
                           -1);
        g_object_unref(gvalue);
        if (gvalue == igvalue)
            return TRUE;

    } while (gtk_tree_model_iter_next(model, iter));

    return FALSE;
}

static void
grain_value_store_finalized(gpointer inventory,
                            GObject *store)
{
    g_signal_handlers_disconnect_by_func(inventory,
                                         inventory_item_updated, store);
    g_signal_handlers_disconnect_by_func(inventory,
                                         inventory_item_inserted, store);
    g_signal_handlers_disconnect_by_func(inventory,
                                         inventory_item_deleted, store);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

