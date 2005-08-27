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
#define DEBUG 1
#include "config.h"
#include <string.h>

#include <gtk/gtk.h>

#include <libgwyddion/gwyddion.h>
#include <libdraw/gwygradient.h>
#include <libdraw/gwyglmaterial.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <libgwydgets/gwyinventorystore.h>

#define BITS_PER_SAMPLE 8

/*
 * Stuff set on objects:
 * selection button:
 *     "active-resource": name of active resource
 *     "treeview": corresponding treeview (if exists)
 *     "resource-info": ResourceInfo for this resource type
 *
 * tree model:
 *     "resource-info": ResourceInfo for this resource type
 *
 * packed item:
 *     "image": image with resource sample
 *     "label": label with resource name
 */

typedef void (*ResourceSamplerFunc)(GwyResource *resource,
                                    GdkPixbuf *pixbuf);

typedef struct {
    GwyInventory *inventory;
    ResourceSamplerFunc sampler;
    const gchar *human_name;
    const gchar *window_title;
    const gchar *key;
} ResourceInfo;

/* FIXME: It would be cleaner to use GClosures, but then all involved funcs
 * would have to use closures instead of callback/cbdata arguments. */
typedef struct {
    GCallback callback;
    gpointer cbdata;
} CallbackInfo;

static const ResourceInfo* gl_material_resource_info(void);
static const ResourceInfo* gradient_resource_info   (void);

static GtkWidget* gwy_menu_resource               (const ResourceInfo *rinfo,
                                                   GCallback callback,
                                                   gpointer cbdata);
static void pack_resource_box                     (GtkContainer *container,
                                                   const ResourceInfo *rinfo,
                                                   const gchar *name,
                                                   gint width,
                                                   gint height);
static GtkWidget* gwy_resource_tree_view_new      (const ResourceInfo *rinfo,
                                                   GCallback callback,
                                                   gpointer cbdata,
                                                   const gchar *active);
static GtkWidget* gwy_resource_selection_new      (const ResourceInfo *rinfo,
                                                   GCallback callback,
                                                   gpointer cbdata,
                                                   const gchar *active);
static void gwy_resource_selection_set_active     (GtkWidget *widget,
                                                   const gchar *active);
static void gwy_resource_selection_cell_pixbuf    (GtkTreeViewColumn *column,
                                                   GtkCellRenderer *renderer,
                                                   GtkTreeModel *model,
                                                   GtkTreeIter *iter,
                                                   gpointer user_data);
static void gwy_resource_selection_cell_name      (GtkTreeViewColumn *column,
                                                   GtkCellRenderer *renderer,
                                                   GtkTreeModel *model,
                                                   GtkTreeIter *iter,
                                                   gpointer user_data);
static void gwy_resource_selection_prefer_toggled (GtkTreeModel *model,
                                                   const gchar *spath);
static void gwy_resource_button_toggled           (GtkWidget *button,
                                                   CallbackInfo *cbinfo);
static void gwy_resource_selection_changed        (GtkTreeSelection *selection,
                                                   GtkWidget *button);
static void gwy_resource_button_update            (GtkWidget *button,
                                                   GwyResource *resource);
static void gwy_resource_selection_default_changed(GwyInventory *inventory,
                                                   GwyInventoryStore *store);
static void gwy_resource_button_treeview_destroy  (GtkWidget *treeview,
                                                   GtkWidget *button);
static void gwy_resource_button_destroy           (GtkWidget *button,
                                                   CallbackInfo *cbinfo);
static void gwy_resource_store_finalized          (gpointer data,
                                                   GObject *exobject);

/************************** Pop-up menus ****************************/

/**
 * gwy_menu_gradient:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 *
 * Creates a pop-up gradient menu.
 *
 * Object data <literal>"gradient-name"</literal> is set to gradient name for
 * each menu item.
 *
 * Returns: The newly created pop-up menu as #GtkWidget.
 **/
GtkWidget*
gwy_menu_gradient(GCallback callback,
                  gpointer cbdata)
{
    return gwy_menu_resource(gradient_resource_info(), callback, cbdata);
}

/**
 * gwy_menu_gl_material:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 *
 * Creates a pop-up GL material menu.
 *
 * Object data <literal>"gl-material-name"</literal> is set to GL material
 * name for each menu item.
 *
 * Returns: The newly created pop-up menu as #GtkWidget.
 **/
GtkWidget*
gwy_menu_gl_material(GCallback callback,
                     gpointer cbdata)
{
    return gwy_menu_resource(gl_material_resource_info(), callback, cbdata);
}

static GtkWidget*
gwy_menu_resource(const ResourceInfo *rinfo,
                  GCallback callback,
                  gpointer cbdata)
{
    GtkWidget *menu, *item;
    GwyResource *resource;
    const gchar *name;
    gint i, width, height;

    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
    width = 5*height;
    menu = gtk_menu_new();
    for (i = 0;
         (resource = gwy_inventory_get_nth_item(rinfo->inventory, i));
         i++) {
        if (!gwy_resource_get_is_preferred(resource))
            continue;
        item = gtk_menu_item_new();
        name = gwy_resource_get_name(resource);
        gwy_debug("<%s>", name);
        pack_resource_box(GTK_CONTAINER(item), rinfo, name, width, height);
        g_object_set_data(G_OBJECT(item), rinfo->key, (gpointer)name);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        if (callback)
            g_signal_connect(item, "activate", callback, cbdata);
    }

    return menu;
}

/************************** Tree views ****************************/

/**
 * gwy_gradient_tree_view_new:
 * @callback: Callback to connect to "changed" signal of tree view selection
 *            (or %NULL for none).
 * @cbdata: User data passed to @callback.
 * @active: Gradient name to be shown as currently selected
 *          (or %NULL for default).
 *
 * Creates a tree view with gradient list.
 *
 * Returns: The newly created gradient tree view as #GtkWidget.
 **/
GtkWidget*
gwy_gradient_tree_view_new(GCallback callback,
                           gpointer cbdata,
                           const gchar *active)
{
    return gwy_resource_tree_view_new(gradient_resource_info(),
                                      callback, cbdata, active);
}

/**
 * gwy_gl_material_tree_view_new:
 * @callback: Callback to connect to "changed" signal of tree view selection
 *            (or %NULL for none).
 * @cbdata: User data passed to @callback.
 * @active: GL material name to be shown as currently selected
 *          (or %NULL for default).
 *
 * Creates a tree view with GL material list.
 *
 * Returns: The newly created GL material tree view as #GtkWidget.
 **/
GtkWidget*
gwy_gl_material_tree_view_new(GCallback callback,
                              gpointer cbdata,
                              const gchar *active)
{
    return gwy_resource_tree_view_new(gl_material_resource_info(),
                                      callback, cbdata, active);
}

/**
 * gwy_gradient_selection_new:
 * @callback: Callback to connect to "changed" signal of tree view selection
 *            (or %NULL for none).
 * @cbdata: User data passed to @callback.
 * @active: Gradient name to be shown as currently selected
 *          (or %NULL for default).
 *
 * Creates a gradient selection button.
 *
 * Returns: The newly created gradient selection button as #GtkWidget.
 **/
GtkWidget*
gwy_gradient_selection_new(GCallback callback,
                           gpointer cbdata,
                           const gchar *active)
{
    return gwy_resource_selection_new(gradient_resource_info(),
                                      callback, cbdata, active);
}

/**
 * gwy_gradient_selection_get_active:
 * @selection: Gradient selection button.
 *
 * Gets the name of currently selected gradient of a selection button.
 *
 * Returns: Name as a string owned by the selected gradient.
 **/
const gchar*
gwy_gradient_selection_get_active(GtkWidget *selection)
{
    return g_object_get_data(G_OBJECT(selection), "active-resource");
}

/**
 * gwy_gradient_selection_set_active:
 * @selection: Gradient selection button.
 * @active: Gradient name to be shown as currently selected.
 *
 * Sets the currently selected gradient of a selection button.
 **/
void
gwy_gradient_selection_set_active(GtkWidget *selection,
                                  const gchar *active)
{
    gwy_resource_selection_set_active(selection, active);
}

/**
 * gwy_gl_material_selection_new:
 * @callback: Callback to connect to "changed" signal of tree view selection
 *            (or %NULL for none).
 * @cbdata: User data passed to @callback.
 * @active: GL material name to be shown as currently selected
 *          (or %NULL for default).
 *
 * Creates a GL material selection button.
 *
 * Returns: The newly created GL material selection button as #GtkWidget.
 **/
GtkWidget*
gwy_gl_material_selection_new(GCallback callback,
                              gpointer cbdata,
                              const gchar *active)
{
    return gwy_resource_selection_new(gl_material_resource_info(),
                                      callback, cbdata, active);
}

/**
 * gwy_gl_material_selection_get_active:
 * @selection: GL material selection button.
 *
 * Gets the name of currently selected GL material of a selection button.
 *
 * Returns: Name as a string owned by the selected GL material.
 **/
const gchar*
gwy_gl_material_selection_get_active(GtkWidget *selection)
{
    return g_object_get_data(G_OBJECT(selection), "active-resource");
}

/**
 * gwy_gl_material_selection_set_active:
 * @selection: GL material selection button.
 * @active: GL material name to be shown as currently selected.
 *
 * Sets the currently selected GL material of a selection button.
 **/
void
gwy_gl_material_selection_set_active(GtkWidget *selection,
                                     const gchar *active)
{
    gwy_resource_selection_set_active(selection, active);
}

/************************** Private methods ****************************/

static GtkWidget*
gwy_resource_tree_view_new(const ResourceInfo *rinfo,
                           GCallback callback,
                           gpointer cbdata,
                           const gchar *active)
{
    GdkPixbuf *pixbuf;
    GwyInventoryStore *store;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkWidget *treeview;
    GwyResource *resource;
    gint width, height, i;

    /* Assure active exists */
    resource = gwy_inventory_get_item_or_default(rinfo->inventory, active);
    active = gwy_resource_get_name(resource);

    store = gwy_inventory_store_new(rinfo->inventory);
    g_object_set_data(G_OBJECT(store), "resource-info", (gpointer)rinfo);
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    g_signal_connect(rinfo->inventory, "default-changed",
                     G_CALLBACK(gwy_resource_selection_default_changed), store);
    g_object_weak_ref(G_OBJECT(store), gwy_resource_store_finalized,
                      rinfo->inventory);

    /* pixbuf */
    gtk_icon_size_lookup(GTK_ICON_SIZE_SMALL_TOOLBAR, &width, &height);
    width = 6*height;
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, BITS_PER_SAMPLE,
                            width, height);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_renderer_set_fixed_size(renderer, width, height);
    column = gtk_tree_view_column_new_with_attributes(rinfo->human_name,
                                                      renderer, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            gwy_resource_selection_cell_pixbuf,
                                            pixbuf, g_object_unref);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* preferred */
    renderer = gtk_cell_renderer_toggle_new();
    gtk_cell_renderer_set_fixed_size(renderer, -1, height);
    i = gwy_inventory_store_get_column_by_name(store, "is-preferred");
    g_assert(i > 0);
    column = gtk_tree_view_column_new_with_attributes(_("Preferred"), renderer,
                                                      "active", i,
                                                      NULL);
    g_object_set(renderer, "activatable", TRUE, NULL);
    g_signal_connect_swapped(renderer, "toggled",
                             G_CALLBACK(gwy_resource_selection_prefer_toggled),
                             store);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* name */
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(renderer, -1, height);
    i = gwy_inventory_store_get_column_by_name(store, "name");
    g_assert(i > 0);
    column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer,
                                                      "text", i,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            gwy_resource_selection_cell_name,
                                            rinfo->inventory, NULL);
    g_object_set(renderer, "weight-set", TRUE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), i);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview), TRUE);

    /* selection */
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    i = gwy_inventory_get_item_position(rinfo->inventory, active);
    gwy_debug("active resource position: %d", i);
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, i);
    gtk_tree_selection_select_iter(selection, &iter);

    if (callback)
        g_signal_connect(selection, "changed", callback, cbdata);

    return treeview;
}

GtkWidget*
gwy_resource_selection_new(const ResourceInfo *rinfo,
                           GCallback callback,
                           gpointer cbdata,
                           const gchar *active)
{
    GtkWidget *button;
    CallbackInfo *cbinfo;
    GwyResource *resource;
    gint width, height;

    /* Assure active exists */
    resource = gwy_inventory_get_item_or_default(rinfo->inventory, active);
    active = gwy_resource_get_name(resource);

    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
    width = 5*height;

    button = gtk_toggle_button_new();
    pack_resource_box(GTK_CONTAINER(button), rinfo, active, width, height);
    g_object_set_data(G_OBJECT(button), "active-resource", resource);
    g_object_set_data(G_OBJECT(button), "resource-info", (gpointer)rinfo);

    cbinfo = g_new(CallbackInfo, 1);
    cbinfo->callback = callback;
    cbinfo->cbdata = cbdata;
    g_signal_connect(button, "toggled",
                     G_CALLBACK(gwy_resource_button_toggled), cbinfo);
    g_signal_connect(button, "destroy",
                     G_CALLBACK(gwy_resource_button_destroy), cbinfo);

    return button;
}

/* FIXME: If there is no treeview, no signal is emitted.  Must define a proper
 * class for the buttons. */
static void
gwy_resource_selection_set_active(GtkWidget *widget,
                                  const gchar *active)
{
    const ResourceInfo *rinfo;
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreePath *path;
    GtkTreeIter iter;
    GtkWidget *treeview;
    const gchar *current;
    gint i;

    rinfo = g_object_get_data(G_OBJECT(widget), "resource-info");
    g_return_if_fail(rinfo);

    current = g_object_get_data(G_OBJECT(widget), "active-resource");
    if (current && active && gwy_strequal(active, current))
        return;

    treeview = g_object_get_data(G_OBJECT(widget), "treeview");
    if (!treeview) {
        GwyResource *resource;

        resource = gwy_inventory_get_item_or_default(rinfo->inventory,
                                                     active);
        gwy_resource_button_update(widget, resource);
        return;
    }

    g_return_if_fail(GTK_IS_TREE_VIEW(treeview));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    i = gwy_inventory_get_item_position(rinfo->inventory, active);
    gtk_tree_model_iter_nth_child(model, &iter, NULL, i);
    path = gtk_tree_model_get_path(model, &iter);
    gtk_tree_selection_select_iter(selection, &iter);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL,
                                 TRUE, 0.5, 0.0);
    gtk_tree_path_free(path);
}

static void
gwy_resource_button_toggled(GtkWidget *button,
                            CallbackInfo *cbinfo)
{
    ResourceInfo *rinfo;
    GwyResource *resource;
    GtkTreeSelection *selection;
    GtkWidget *window, *scwin, *treeview;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
        /* Pop down */
        treeview = g_object_get_data(G_OBJECT(button), "treeview");
        if (treeview)
            gtk_widget_destroy(gtk_widget_get_toplevel(treeview));
        return;
    }

    rinfo = g_object_get_data(G_OBJECT(button), "resource-info");

    /* Pop up */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), rinfo->window_title);
    gtk_window_set_default_size(GTK_WINDOW(window), -1, 400);
    gtk_window_set_transient_for(GTK_WINDOW(window),
                                 GTK_WINDOW(gtk_widget_get_toplevel(button)));

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(window), scwin);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    resource = g_object_get_data(G_OBJECT(button), "active-resource");
    gwy_debug("active-resource = <%s>", gwy_resource_get_name(resource));
    treeview = gwy_resource_tree_view_new(rinfo,
                                          cbinfo->callback, cbinfo->cbdata,
                                          gwy_resource_get_name(resource));
    g_object_set_data(G_OBJECT(button), "treeview", treeview);
    gtk_container_add(GTK_CONTAINER(scwin), treeview);
    g_signal_connect(treeview, "destroy",
                     G_CALLBACK(gwy_resource_button_treeview_destroy), button);
    g_signal_connect_swapped(treeview, "row-activated",
                             G_CALLBACK(gtk_widget_destroy), window);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(gwy_resource_selection_changed), button);

    gtk_widget_show_all(scwin);
    gtk_window_present(GTK_WINDOW(window));
}

static void
gwy_resource_selection_cell_pixbuf(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                   GtkCellRenderer *renderer,
                                   GtkTreeModel *model,
                                   GtkTreeIter *iter,
                                   gpointer user_data)
{
    const ResourceInfo *rinfo;
    GwyResource *resource;
    GdkPixbuf *pixbuf = (GdkPixbuf*)user_data;

    gtk_tree_model_get(model, iter, 0, &resource, -1);
    rinfo = g_object_get_data(G_OBJECT(model), "resource-info");
    rinfo->sampler(resource, pixbuf);
    /* This looks like noop because we use one pixbuf all the time, but it
     * apparently makes the cell rendered to really render it */
    g_object_set(renderer, "pixbuf", pixbuf, NULL);
}

static void
gwy_resource_selection_cell_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                 GtkCellRenderer *renderer,
                                 GtkTreeModel *model,
                                 GtkTreeIter *iter,
                                 gpointer user_data)
{
    GwyInventory *inventory = (GwyInventory*)user_data;
    gpointer item, defitem;

    gtk_tree_model_get(model, iter, 0, &item, -1);
    defitem = gwy_inventory_get_default_item(inventory);
    g_object_set(renderer,
                 "weight",
                 (item == defitem) ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
                 NULL);
}

static void
gwy_resource_selection_changed(GtkTreeSelection *selection,
                               GtkWidget *button)
{
    GwyResource *resource;
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;
    gtk_tree_model_get(model, &iter, 0, &resource, -1);
    gwy_resource_button_update(button, resource);
}

static void
gwy_resource_button_update(GtkWidget *button,
                           GwyResource *resource)
{
    const ResourceInfo *rinfo;
    GtkWidget *image, *label;
    GdkPixbuf *pixbuf;

    gwy_debug("updating button to: %s", gwy_resource_get_name(resource));
    image = g_object_get_data(G_OBJECT(button), "image");
    pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(image));
    rinfo = g_object_get_data(G_OBJECT(button), "resource-info");
    rinfo->sampler(resource, pixbuf);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);

    label = g_object_get_data(G_OBJECT(button), "label");
    gtk_label_set_text(GTK_LABEL(label), gwy_resource_get_name(resource));

    g_object_set_data(G_OBJECT(button), "active-resource", resource);
}

static void
gwy_resource_selection_prefer_toggled(GtkTreeModel *model,
                                      const gchar *spath)
{
    GwyInventory *inventory;
    GwyResource *resource;
    GtkTreePath *path;
    GtkTreeIter iter;
    gboolean is_preferred;
    gint i;

    inventory = gwy_inventory_store_get_inventory(GWY_INVENTORY_STORE(model));

    path = gtk_tree_path_new_from_string(spath);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, 0, &resource, -1);
    is_preferred = gwy_resource_get_is_preferred(resource);
    gwy_debug("res: %d", is_preferred);
    gwy_resource_set_is_preferred(resource, !is_preferred);
    i = gtk_tree_path_get_indices(path)[0];
    gtk_tree_path_free(path);

    gwy_inventory_nth_item_updated(inventory, i);
}

static void
gwy_resource_selection_default_changed(GwyInventory *inventory,
                                       GwyInventoryStore *store)
{
    GtkTreeModel *model;
    GwyResource *resource;
    GtkTreePath *path;
    GtkTreeIter iter;
    gint i;

    resource = gwy_inventory_get_default_item(inventory);
    if (!resource)
        return;

    /* FIXME: This is somewhat crude */
    model = GTK_TREE_MODEL(store);
    i = gwy_inventory_get_item_position(inventory,
                                        gwy_resource_get_name(resource));
    gtk_tree_model_iter_nth_child(model, &iter, NULL, i);
    path = gtk_tree_model_get_path(model, &iter);
    gtk_tree_model_row_changed(model, path, &iter);
    gtk_tree_path_free(path);
}

static void
gwy_resource_button_treeview_destroy(G_GNUC_UNUSED GtkWidget *treeview,
                                     GtkWidget *button)
{
    gwy_debug(" ");
    g_object_set_data(G_OBJECT(button), "treeview", NULL);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
}

static void
gwy_resource_button_destroy(GtkWidget *button,
                            CallbackInfo *cbinfo)
{
    GtkWidget *widget;

    widget = g_object_get_data(G_OBJECT(button), "treeview");
    gwy_debug("widget: %p", widget);
    if (widget) {
        g_object_set_data(G_OBJECT(button), "treeview", NULL);
        gtk_widget_destroy(gtk_widget_get_toplevel(widget));
    }

    g_free(cbinfo);
}

static void
gwy_resource_store_finalized(gpointer data,
                             GObject *exobject)
{
    g_signal_handlers_disconnect_by_func(data,
                                         gwy_resource_selection_default_changed,
                                         exobject);
}

/************************** Common subroutines ****************************/

static void
pack_resource_box(GtkContainer *container,
                  const ResourceInfo *rinfo,
                  const gchar *name,
                  gint width,
                  gint height)
{
    GwyResource *resource;
    GtkWidget *hbox, *image, *label;
    GdkPixbuf *pixbuf;

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, BITS_PER_SAMPLE,
                            width, height);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    resource = gwy_inventory_get_item_or_default(rinfo->inventory, name);
    rinfo->sampler(resource, pixbuf);
    image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);

    name = gwy_resource_get_name(resource);
    hbox = gtk_hbox_new(FALSE, 6);
    label = gtk_label_new(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
    gtk_container_add(container, hbox);

    g_object_set_data(G_OBJECT(container), "image", image);
    g_object_set_data(G_OBJECT(container), "label", label);
}

static const ResourceInfo*
gradient_resource_info(void)
{
    static ResourceInfo rinfo = { NULL, NULL, NULL, NULL, NULL };

    if (!rinfo.inventory) {
        rinfo.inventory = gwy_gradients();
        rinfo.sampler = (ResourceSamplerFunc)&gwy_gradient_sample_to_pixbuf;
        rinfo.human_name = _("Gradient");
        rinfo.window_title = _("Choose Gradient");
        rinfo.key = "gradient-name";
    }
    return &rinfo;
}

static const ResourceInfo*
gl_material_resource_info(void)
{
    static ResourceInfo rinfo = { NULL, NULL, NULL, NULL, NULL };

    if (!rinfo.inventory) {
        rinfo.inventory = gwy_gl_materials();
        rinfo.sampler = (ResourceSamplerFunc)&gwy_gl_material_sample_to_pixbuf;
        rinfo.human_name = _("GL Material");
        rinfo.window_title = _("Choose GL Material");
        rinfo.key = "gl-material-name";
    }
    return &rinfo;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
