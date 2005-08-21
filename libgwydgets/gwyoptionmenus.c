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
#include <libgwyddion/gwymacros.h>

#include <string.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkimagemenuitem.h>

#include <libgwyddion/gwyddion.h>
#include <libdraw/gwygradient.h>
#include <libgwydgets/gwyglmaterial.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <libgwydgets/gwyinventorystore.h>

#define BITS_PER_SAMPLE 8

enum {
    SAMPLE_HEIGHT = 16,
    SAMPLE_WIDTH = 80
};

/* FIXME: It would be cleaner to use GClosures, but then all involved funcs
 * would have to use closures instead of callback/cbdata arguments. */
typedef struct {
    GCallback callback;
    gpointer cbdata;
} CallbackInfo;

static void gwy_gradient_button_toggled(GtkWidget *button,
                                        CallbackInfo *cbinfo);
static void gwy_resource_selection_changed(GtkTreeSelection *selection,
                               GtkWidget *button);

static GtkWidget* gwy_sample_gl_material_to_gtkimage(GwyGLMaterial *material);
static gint       gl_material_compare            (GwyGLMaterial *a,
                                                  GwyGLMaterial *b);

/************************** Gradient menu ****************************/

static void
pack_gradient_box(GtkContainer *container,
                  const gchar *name,
                  gint width,
                  gint height)
{
    GwyGradient *gradient;
    GtkWidget *hbox, *image, *label;
    GdkPixbuf *pixbuf;

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, BITS_PER_SAMPLE,
                            width, height);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    gradient = gwy_gradients_get_gradient(name);
    gwy_gradient_sample_to_pixbuf(gradient, pixbuf);
    image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);

    name = gwy_resource_get_name(GWY_RESOURCE(gradient));
    hbox = gtk_hbox_new(FALSE, 6);
    label = gtk_label_new(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
    gtk_container_add(container, hbox);

    g_object_set_data(G_OBJECT(container), "image", image);
    g_object_set_data(G_OBJECT(container), "label", label);
}

/**
 * gwy_menu_gradient:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 *
 * Creates a pop-up gradient menu.
 *
 * Returns: The newly created pop-up menu as #GtkWidget.
 **/
GtkWidget*
gwy_menu_gradient(GCallback callback,
                  gpointer cbdata)
{
    GtkWidget *menu, *item;
    GwyInventory *gradients;
    GwyGradient *gradient;
    const gchar *name;
    gint i, width, height;

    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
    width = 5*height;
    gradients = gwy_gradients();
    menu = gtk_menu_new();
    for (i = 0; (gradient = gwy_inventory_get_nth_item(gradients, i)); i++) {
        if (!gwy_resource_get_is_preferred(GWY_RESOURCE(gradient)))
            continue;
        item = gtk_menu_item_new();
        name = gwy_resource_get_name(GWY_RESOURCE(gradient));
        gwy_debug("<%s>", name);
        pack_gradient_box(GTK_CONTAINER(item), name, width, height);
        g_object_set_data(G_OBJECT(item), "gradient-name", (gpointer)name);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        if (callback)
            g_signal_connect(item, "activate", callback, cbdata);
    }

    return menu;
}

static void
gwy_gradient_button_treeview_destroy(G_GNUC_UNUSED GtkWidget *treeview,
                                     GtkWidget *button)
{
    gwy_debug(" ");
    g_object_set_data(G_OBJECT(button), "treeview", NULL);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
}

static void
gwy_gradient_button_toggled(GtkWidget *button,
                            CallbackInfo *cbinfo)
{
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

    /* Pop up */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), _("Choose Gradient"));
    gtk_window_set_default_size(GTK_WINDOW(window), -1, 400);
    gtk_window_set_transient_for(GTK_WINDOW(window),
                                 GTK_WINDOW(gtk_widget_get_toplevel(button)));

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(window), scwin);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    resource = g_object_get_data(G_OBJECT(button), "active-resource");
    gwy_debug("active-resource = <%s>", gwy_resource_get_name(resource));
    treeview = gwy_gradient_tree_view_new(cbinfo->callback, cbinfo->cbdata,
                                          gwy_resource_get_name(resource));
    g_object_set_data(G_OBJECT(button), "treeview", treeview);
    gtk_container_add(GTK_CONTAINER(scwin), treeview);
    g_signal_connect(treeview, "destroy",
                     G_CALLBACK(gwy_gradient_button_treeview_destroy), button);
    g_signal_connect_swapped(treeview, "row-activated",
                             G_CALLBACK(gtk_widget_destroy), window);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(gwy_resource_selection_changed), button);

    gtk_widget_show_all(scwin);
    gtk_window_present(GTK_WINDOW(window));
}

static void
gwy_gradient_button_destroy(GtkWidget *button,
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
gwy_gradient_selection_cell_pixbuf(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                   GtkCellRenderer *renderer,
                                   GtkTreeModel *model,
                                   GtkTreeIter *iter,
                                   gpointer user_data)
{
    GwyGradient *gradient;
    GdkPixbuf *pixbuf = (GdkPixbuf*)user_data;

    gtk_tree_model_get(model, iter, 0, &gradient, -1);
    gwy_gradient_sample_to_pixbuf(gradient, pixbuf);
    /* This looks like noop because we use one pixbuf all the time, but it
     * apparently makes the cell rendered to really render it */
    g_object_set(renderer, "pixbuf", pixbuf, NULL);
}

static void
gwy_gradient_selection_cell_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
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
    GtkWidget *image, *label;
    GwyResource *resource;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GdkPixbuf *pixbuf;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;
    gtk_tree_model_get(model, &iter, 0, &resource, -1);

    image = g_object_get_data(G_OBJECT(button), "image");
    pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(image));
    gwy_gradient_sample_to_pixbuf(GWY_GRADIENT(resource), pixbuf);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);

    label = g_object_get_data(G_OBJECT(button), "label");
    gtk_label_set_text(GTK_LABEL(label), gwy_resource_get_name(resource));

    g_object_set_data(G_OBJECT(button), "active-resource", resource);
}

static void
gwy_gradient_selection_prefer_toggled(GtkTreeModel *model,
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
gwy_gradient_selection_default_changed(GwyInventory *inventory,
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

/**
 * gwy_gradient_tree_view_new:
 * @callback: A callback called when tree view selection changes (or %NULL for
 *            none), that is to connect to "changed" signal of corresponding
 *            #GtkTreeSelection.
 * @cbdata: User data passed to the callback.
 * @active: Gradient name to be shown as currently selected
 *          (or %NULL to use what happens to appear first).
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
    GwyInventory *inventory;
    GdkPixbuf *pixbuf;
    GwyInventoryStore *store;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkWidget *treeview;
    GwyGradient *gradient;
    gint width, height, i;

    /* Assure active exists */
    gradient = gwy_gradients_get_gradient(active);
    active = gwy_resource_get_name(GWY_RESOURCE(gradient));

    inventory = gwy_gradients();
    store = gwy_inventory_store_new(inventory);
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    g_signal_connect(inventory, "default-changed",
                     G_CALLBACK(gwy_gradient_selection_default_changed), store);

    /* pixbuf */
    gtk_icon_size_lookup(GTK_ICON_SIZE_SMALL_TOOLBAR, &width, &height);
    width = 6*height;
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, BITS_PER_SAMPLE,
                            width, height);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_renderer_set_fixed_size(renderer, width, height);
    column = gtk_tree_view_column_new_with_attributes(_("Gradient"), renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            gwy_gradient_selection_cell_pixbuf,
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
                             G_CALLBACK(gwy_gradient_selection_prefer_toggled),
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
                                            gwy_gradient_selection_cell_name,
                                            inventory, NULL);
    g_object_set(renderer, "weight-set", TRUE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), i);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview), TRUE);

    /* selection */
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    i = gwy_inventory_get_item_position(inventory, active);
    gwy_debug("active resource position: %d", i);
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, i);
    gtk_tree_selection_select_iter(selection, &iter);

    if (callback)
        g_signal_connect(selection, "changed", callback, cbdata);

    return treeview;
}

/**
 * gwy_gradient_selection_new:
 * @callback: A callback called when tree view selection changes (or %NULL for
 *            none), that is to connect to "changed" signal of corresponding
 *            #GtkTreeSelection.
 * @cbdata: User data passed to the callback.
 * @active: Gradient name to be shown as currently selected
 *          (or %NULL to use what happens to appear first).
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
    GtkWidget *button;
    CallbackInfo *cbinfo;
    GwyGradient *gradient;
    gint width, height;

    /* Assure active exists */
    gradient = gwy_gradients_get_gradient(active);
    active = gwy_resource_get_name(GWY_RESOURCE(gradient));

    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
    width = 5*height;

    button = gtk_toggle_button_new();
    pack_gradient_box(GTK_CONTAINER(button), active, width, height);
    g_object_set_data(G_OBJECT(button), "active-resource", gradient);

    cbinfo = g_new(CallbackInfo, 1);
    cbinfo->callback = callback;
    cbinfo->cbdata = cbdata;
    g_signal_connect(button, "toggled",
                     G_CALLBACK(gwy_gradient_button_toggled), cbinfo);
    g_signal_connect(button, "destroy",
                     G_CALLBACK(gwy_gradient_button_destroy), cbinfo);

    return button;
}

/************************** Material menu ****************************/

static GtkWidget*
gwy_gl_material_menu_create(const gchar *current,
                            gint *current_idx)
{
    GSList *l, *entries = NULL;
    GtkWidget *menu, *image, *item, *hbox, *label;
    gint i, idx;

    gwy_gl_material_foreach((GwyGLMaterialFunc)gwy_hash_table_to_slist_cb,
                            &entries);
    entries = g_slist_sort(entries, (GCompareFunc)gl_material_compare);

    menu = gtk_menu_new();

    idx = -1;
    i = 0;
    for (l = entries; l; l = g_slist_next(l)) {
        GwyGLMaterial *gl_material = (GwyGLMaterial*)l->data;
        const gchar *name = gwy_gl_material_get_name(gl_material);

        if (gwy_strequal(name, GWY_GL_MATERIAL_NONE))
            continue;

        image = gwy_sample_gl_material_to_gtkimage(gl_material);
        item = gtk_menu_item_new();
        hbox = gtk_hbox_new(FALSE, 6);
        label = gtk_label_new(name);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(item), hbox);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_data(G_OBJECT(item), "material-name", (gpointer)name);
        if (current && gwy_strequal(current, name))
            idx = i;
        i++;
    }
    g_slist_free(entries);

    if (current_idx && idx != -1)
        *current_idx = idx;

    return menu;
}

/**
 * gwy_menu_gl_material:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 *
 * Creates a pop-up OpenGL material menu.
 *
 * Returns: The newly created pop-up menu as #GtkWidget.
 **/
GtkWidget*
gwy_menu_gl_material(GCallback callback,
                     gpointer cbdata)
{
    GtkWidget *menu;
    GList *c;

    menu = gwy_gl_material_menu_create(NULL, NULL);
    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }

    return menu;
}

/**
 * gwy_option_menu_gl_material:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 * @current: Palette definition name to be shown as currently selected
 *           (or %NULL to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of OpenGL materials.
 *
 * It sets object data "material-name" to material definition name for each
 * menu item.
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_gl_material(GCallback callback,
                            gpointer cbdata,
                            const gchar *current)
{
    GtkWidget *omenu, *menu;
    GList *c;
    gint idx;

    idx = -1;
    omenu = gtk_option_menu_new();
    g_object_set_data(G_OBJECT(omenu), "gwy-option-menu",
                      GINT_TO_POINTER(TRUE));
    menu = gwy_gl_material_menu_create(current, &idx);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (idx != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), idx);

    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }

    return omenu;
}

/* XXX: magic static variables */
static GtkWidget*
gwy_sample_gl_material_to_gtkimage(GwyGLMaterial *material)
{
    static guchar *samples = NULL;
    GdkPixbuf *pixbuf;
    GtkWidget *image;
    guint rowstride;
    guchar *data;
    gint i;

    /* clean up when called with NULL */
    if (!material) {
        g_free(samples);
        samples = NULL;
        return NULL;
    }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, BITS_PER_SAMPLE,
                            SAMPLE_WIDTH, SAMPLE_HEIGHT);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    data = gdk_pixbuf_get_pixels(pixbuf);

    samples = gwy_gl_material_sample(material, SAMPLE_WIDTH, samples);
    for (i = 0; i < SAMPLE_HEIGHT; i++)
        memcpy(data + i*rowstride, samples, 4*SAMPLE_WIDTH);

    image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);

    return image;
}

static gint
gl_material_compare(GwyGLMaterial *a,
                    GwyGLMaterial *b)
{
    return strcmp(gwy_gl_material_get_name(a), gwy_gl_material_get_name(b));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
