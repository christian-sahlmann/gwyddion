/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
#include <glib/gstdio.h>
#include <libgwyddion/gwyddion.h>
#include <app/menu.h>
#include <app/settings.h>
#include <app/resource-editor.h>

#include <libdraw/gwyglmaterial.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwydgets/gwydgetutils.h>

enum {
    WHATEVER,
    LAST_SIGNAL
};

static void     gwy_resource_editor_finalize      (GObject *object);
static void     gwy_resource_editor_set_property  (GObject *object,
                                                   guint prop_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void     gwy_resource_editor_get_property  (GObject *object,
                                                   guint prop_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static void     gwy_resource_editor_cell_name     (GtkTreeViewColumn *column,
                                                   GtkCellRenderer *renderer,
                                                   GtkTreeModel *model,
                                                   GtkTreeIter *iter,
                                                   gpointer data);
static void     gwy_resource_editor_changed       (GtkTreeSelection *selection,
                                                   GwyResourceEditor *editor);
static void     gwy_resource_editor_destroy       (GtkObject *object);
static void     gwy_resource_editor_new           (GwyResourceEditor *editor);
static void     gwy_resource_editor_duplicate     (GwyResourceEditor *editor);
static void     gwy_resource_editor_copy          (GwyResourceEditor *editor,
                                                   const gchar *name,
                                                   const gchar *newname);
static void     gwy_resource_editor_delete        (GwyResourceEditor *editor);
static void     gwy_resource_editor_set_default   (GwyResourceEditor *editor);
static void     gwy_resource_editor_edit          (GwyResourceEditor *editor);
static void     gwy_resource_editor_row_activated (GwyResourceEditor *editor,
                                                   GtkTreePath *path,
                                                   GtkTreeViewColumn *column);
static void     gwy_resource_editor_edit_resource (GwyResourceEditor *editor,
                                                   const gchar *name);
static void     gwy_resource_editor_editor_closed (GwyResourceEditor *editor);
static void     gwy_resource_editor_name_edited   (GwyResourceEditor *editor,
                                                   const gchar *strpath,
                                                   const gchar *text);
static gboolean gwy_resource_editor_save          (GwyResourceEditor *editor,
                                                   const gchar *name);
static void     gwy_resource_editor_update_title  (GwyResourceEditor *editor);
static GwyResource* gwy_resource_editor_get_active(GwyResourceEditor *editor,
                                                   GtkTreeModel **model,
                                                   GtkTreeIter *iter,
                                                   const gchar *warnwhat);

static guint resource_editor_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE(GwyResourceEditor, gwy_resource_editor,
                       GTK_TYPE_WINDOW)

static void
gwy_resource_editor_class_init(GwyResourceEditorClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_resource_editor_finalize;
    gobject_class->get_property = gwy_resource_editor_get_property;
    gobject_class->set_property = gwy_resource_editor_set_property;

    object_class->destroy = gwy_resource_editor_destroy;
}

static void
gwy_resource_editor_finalize(GObject *object)
{
    GwyResourceEditorClass *klass;

    klass = GWY_RESOURCE_EDITOR_GET_CLASS(object);
    klass->instance = NULL;

    G_OBJECT_CLASS(gwy_resource_editor_parent_class)->finalize(object);
}

static void
gwy_resource_editor_init(GwyResourceEditor *editor)
{
    static const struct {
        GwyResourceEditorButton id;
        const gchar *label;
        const gchar *stock_id;
        const gchar *tooltip;
        GCallback callback;
    }
    toolbar_buttons[] = {
        {
            GWY_RESOURCE_EDITOR_BUTTON_EDIT,
            N_("_Edit"), GTK_STOCK_EDIT,
            N_("Edit selected item"),
            G_CALLBACK(gwy_resource_editor_edit),
        },
        {
            GWY_RESOURCE_EDITOR_BUTTON_NEW,
            N_("_New"), GTK_STOCK_NEW,
            N_("Create a new item"),
            G_CALLBACK(gwy_resource_editor_new),
        },
        {
            GWY_RESOURCE_EDITOR_BUTTON_COPY,
            N_("_Copy"), GTK_STOCK_COPY,
            N_("Create a new item based on selected one"),
            G_CALLBACK(gwy_resource_editor_duplicate),
        },
        {
            GWY_RESOURCE_EDITOR_BUTTON_DELETE,
            N_("_Delete"), GTK_STOCK_DELETE,
            N_("Delete selected item"),
            G_CALLBACK(gwy_resource_editor_delete),
        },
        {
            GWY_RESOURCE_EDITOR_BUTTON_SET_DEFAULT,
            N_("De_fault"), "gwy_favourite",
            N_("Set selected item as default"),
            G_CALLBACK(gwy_resource_editor_set_default),
        },
    };

    GwyResourceEditorClass *klass;
    GtkTreeViewColumn *column;
    GwyInventory *inventory;
    GtkTreeModel *model;
    GwyContainer *settings;
    GtkWidget *hbox, *button;
    GtkTooltips *tooltips;
    GtkWidget *scwin;
    const guchar *name;
    GList *rlist;
    guint i;

    klass = GWY_RESOURCE_EDITOR_GET_CLASS(editor);
    if (klass->instance) {
        g_warning("An instance of this editor already exists.  "
                  "This is not going to work.");
    }
    klass->instance = editor;

    settings = gwy_app_settings_get();
    name = klass->base_resource;
    gwy_container_gis_string(settings, klass->current_key, &name);

    /* Window setup */
    gtk_window_set_resizable(GTK_WINDOW(editor), TRUE);
    gtk_window_set_title(GTK_WINDOW(editor), klass->window_title);
    gtk_window_set_default_size(GTK_WINDOW(editor), -1, 420);

    editor->vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(editor), editor->vbox);

    /* Treeview */
    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(editor->vbox), scwin, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    editor->treeview
        = klass->construct_treeview(G_CALLBACK(gwy_resource_editor_changed),
                                    editor, NULL);
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(editor->treeview));
    inventory = gwy_inventory_store_get_inventory(GWY_INVENTORY_STORE(model));
    column = gtk_tree_view_get_column(GTK_TREE_VIEW(editor->treeview), 2);
    rlist = gtk_tree_view_column_get_cell_renderers(column);
    g_assert(rlist && !rlist->next);
    g_object_set(rlist->data, "editable-set", TRUE, NULL);
    gtk_tree_view_column_set_cell_data_func(column,
                                            GTK_CELL_RENDERER(rlist->data),
                                            gwy_resource_editor_cell_name,
                                            inventory, NULL);
    g_signal_connect_swapped(rlist->data, "edited",
                             G_CALLBACK(gwy_resource_editor_name_edited),
                             editor);
    g_signal_connect_swapped(editor->treeview, "row-activated",
                             G_CALLBACK(gwy_resource_editor_row_activated),
                             editor);
    g_list_free(rlist);
    gtk_container_add(GTK_CONTAINER(scwin), editor->treeview);

    /* Controls */
    tooltips = gwy_app_tooltips_get();
    hbox = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(editor->vbox), hbox, FALSE, FALSE, 0);
    for (i = 0; i < G_N_ELEMENTS(toolbar_buttons); i++) {
        button = gwy_tool_like_button_new(toolbar_buttons[i].label,
                                          toolbar_buttons[i].stock_id);
        editor->buttons[toolbar_buttons[i].id] = button;
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_tooltips_set_tip(tooltips, button,
                             toolbar_buttons[i].tooltip, NULL);
        g_signal_connect_swapped(button, "clicked",
                                 toolbar_buttons[i].callback, editor);
    }

    gtk_widget_show_all(editor->vbox);
    gwy_resource_tree_view_set_active(editor->treeview, name);
}

static void
gwy_resource_editor_cell_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
                              GtkCellRenderer *renderer,
                              GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gpointer data)
{
    GwyResource *item;
    gpointer defitem;

    defitem = gwy_inventory_get_default_item(GWY_INVENTORY(data));
    gtk_tree_model_get(model, iter, 0, &item, -1);
    g_object_set(renderer,
                 "editable",
                 gwy_resource_get_is_modifiable(item),
                 "weight",
                 (item == defitem) ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
                 NULL);
}

static void
gwy_resource_editor_set_property(GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    /*GwyResourceEditor *editor = GWY_RESOURCE_EDITOR(object);*/

    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_resource_editor_get_property(GObject *object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    /*GwyResourceEditor *editor = GWY_RESOURCE_EDITOR(object);*/

    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_resource_editor_changed(GtkTreeSelection *selection,
                            GwyResourceEditor *editor)
{
    static const GwyResourceEditorButton needs_selection[] = {
        GWY_RESOURCE_EDITOR_BUTTON_COPY,
        GWY_RESOURCE_EDITOR_BUTTON_SET_DEFAULT,
    };
    static const GwyResourceEditorButton needs_modifiable[] = {
        GWY_RESOURCE_EDITOR_BUTTON_EDIT,
        GWY_RESOURCE_EDITOR_BUTTON_DELETE,
    };
    GwyResource *resource;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean is_modifiable;
    guint i;

    gwy_debug("");
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        for (i = 0; i < G_N_ELEMENTS(needs_selection); i++)
            gtk_widget_set_sensitive(editor->buttons[needs_selection[i]],
                                     FALSE);
        for (i = 0; i < G_N_ELEMENTS(needs_modifiable); i++)
            gtk_widget_set_sensitive(editor->buttons[needs_modifiable[i]],
                                     FALSE);
        return;
    }

    gtk_tree_model_get(model, &iter, 0, &resource, -1);

    for (i = 0; i < G_N_ELEMENTS(needs_selection); i++)
        gtk_widget_set_sensitive(editor->buttons[needs_selection[i]],
                                 TRUE);
    is_modifiable = gwy_resource_get_is_modifiable(resource);
    for (i = 0; i < G_N_ELEMENTS(needs_modifiable); i++)
        gtk_widget_set_sensitive(editor->buttons[needs_modifiable[i]],
                                 is_modifiable);
}

static void
gwy_resource_editor_destroy(GtkObject *object)
{
    GwyResourceEditor *editor;
    GwyResourceEditorClass *klass;
    GwyResource *resource;

    gwy_debug("");
    editor = GWY_RESOURCE_EDITOR(object);
    klass = GWY_RESOURCE_EDITOR_GET_CLASS(object);

    gwy_resource_editor_commit(editor);
    if (editor->edited_resource) {
        g_string_free(editor->edited_resource, TRUE);
        editor->edited_resource = NULL;
    }

    if (editor->treeview
        && (resource = gwy_resource_editor_get_active(editor,
                                                      NULL, NULL, NULL))) {
        GwyContainer *settings;

        settings = gwy_app_settings_get();
        gwy_container_set_string(settings, klass->current_key,
                                 g_strdup(gwy_resource_get_name(resource)));
    }
    editor->treeview = NULL;

    GTK_OBJECT_CLASS(gwy_resource_editor_parent_class)->destroy(object);
}

static void
gwy_resource_editor_set_default(GwyResourceEditor *editor)
{
    GtkTreeModel *model;
    GwyResource *resource;
    GwyInventory *inventory;

    gwy_debug("");
    resource = gwy_resource_editor_get_active(editor, &model, NULL,
                                              "Set Default");
    inventory = gwy_inventory_store_get_inventory(GWY_INVENTORY_STORE(model));
    gwy_inventory_set_default_item_name(inventory,
                                        gwy_resource_get_name(resource));
}

static void
gwy_resource_editor_new(GwyResourceEditor *editor)
{
    GwyResourceEditorClass *klass;

    gwy_debug("");
    klass = GWY_RESOURCE_EDITOR_GET_CLASS(editor);
    gwy_resource_editor_copy(editor, klass->base_resource, _("Untitled"));
}

static void
gwy_resource_editor_duplicate(GwyResourceEditor *editor)
{
    GwyResource *resource;

    gwy_debug("");
    resource = gwy_resource_editor_get_active(editor, NULL, NULL, "Copy");
    gwy_resource_editor_copy(editor, gwy_resource_get_name(resource), NULL);
}

static void
gwy_resource_editor_copy(GwyResourceEditor *editor,
                         const gchar *name,
                         const gchar *newname)
{
    GtkTreeModel *model;
    GwyInventory *inventory;
    GwyResource *resource;

    gwy_debug("<%s> -> <%s>", name, newname);
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(editor->treeview));
    inventory = gwy_inventory_store_get_inventory(GWY_INVENTORY_STORE(model));
    resource = gwy_inventory_new_item(inventory, name, newname);
    gwy_resource_tree_view_set_active(editor->treeview,
                                      gwy_resource_get_name(resource));
    gwy_resource_editor_save(editor, gwy_resource_get_name(resource));
    /* XXX: don't? gwy_resource_editor_edit(editor); */
}

static void
gwy_resource_editor_delete(GwyResourceEditor *editor)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GwyResource *resource;
    GwyInventory *inventory;
    GtkTreePath *path;
    gchar *filename;
    int result;

    gwy_debug("");
    gwy_resource_editor_commit(editor);

    /* Get selected resource, and the inventory it belongs to: */
    resource = gwy_resource_editor_get_active(editor, &model, &iter, "Delete");
    inventory = gwy_inventory_store_get_inventory(GWY_INVENTORY_STORE(model));

    /* Delete the resource file */
    filename = gwy_resource_build_filename(resource);
    result = g_remove(filename);
    if (result) {
        /* FIXME: GUIze this */
        g_warning("Resource (%s) could not be deleted.",
                  gwy_resource_get_name(resource));

        g_free(filename);
        return;
    }
    g_free(filename);

    /* Delete the resource from the inventory */
    path = gtk_tree_model_get_path(model, &iter);
    gwy_inventory_delete_item(inventory, gwy_resource_get_name(resource));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(editor->treeview));
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_path_free(path);
}

static void
gwy_resource_editor_edit(GwyResourceEditor *editor)
{
    GwyResource *resource;

    gwy_debug("");
    resource = gwy_resource_editor_get_active(editor, NULL, NULL, "Edit");
    gwy_resource_editor_edit_resource(editor, gwy_resource_get_name(resource));
}

static void
gwy_resource_editor_row_activated(GwyResourceEditor *editor,
                                  GtkTreePath *path,
                                  G_GNUC_UNUSED GtkTreeViewColumn *column)
{
    GwyResource *resource;
    GtkTreeModel *model;
    GtkTreeIter iter;

    gwy_debug("");
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(editor->treeview));
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, 0, &resource, -1);
    gwy_resource_editor_edit_resource(editor, gwy_resource_get_name(resource));
}

static void
gwy_resource_editor_edit_resource(GwyResourceEditor *editor,
                                  const gchar *name)
{
    GwyResourceEditorClass *klass;

    klass = GWY_RESOURCE_EDITOR_GET_CLASS(editor);
    gwy_resource_editor_commit(editor);

    if (!editor->edit_window) {
        editor->edited_resource = g_string_new(name);
        editor->edit_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gwy_resource_editor_update_title(editor);
        klass->construct_editor(editor);
        g_signal_connect_swapped(editor->edit_window, "destroy",
                                 G_CALLBACK(gwy_resource_editor_editor_closed),
                                 editor);
        gtk_widget_show_all(editor->edit_window);
    }
    else {
        g_string_assign(editor->edited_resource, name);
        gwy_resource_editor_update_title(editor);
        klass->switch_resource(editor);
        gtk_window_present(GTK_WINDOW(editor->edit_window));
    }
}

static void
gwy_resource_editor_editor_closed(GwyResourceEditor *editor)
{
    gwy_resource_editor_commit(editor);
    if (editor->edited_resource) {
        g_string_free(editor->edited_resource, TRUE);
        editor->edited_resource = NULL;
    }
    editor->edit_window = NULL;
}

static void
gwy_resource_editor_name_edited(GwyResourceEditor *editor,
                                const gchar *strpath,
                                const gchar *text)
{
    GwyResource *resource, *item;
    GwyInventory *inventory;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    const gchar *s;
    gchar *oldname, *newname, *oldfilename, *newfilename;

    gwy_debug("path: <%s>, text: <%s>", strpath, text);
    newname = g_newa(gchar, strlen(text)+1);
    strcpy(newname, text);
    g_strstrip(newname);
    gwy_debug("newname: <%s>", newname);

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(editor->treeview));
    path = gtk_tree_path_new_from_string(strpath);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);
    gtk_tree_model_get(model, &iter, 0, &resource, -1);
    s = gwy_resource_get_name(resource);
    oldname = g_newa(gchar, strlen(s)+1);
    strcpy(oldname, s);
    gwy_debug("oldname: <%s>", oldname);
    if (gwy_strequal(newname, oldname))
        return;

    inventory = gwy_inventory_store_get_inventory(GWY_INVENTORY_STORE(model));
    item = gwy_inventory_get_item(inventory, newname);
    if (item)
        return;

    gwy_resource_editor_commit(editor);
    oldfilename = gwy_resource_build_filename(resource);
    gwy_inventory_rename_item(inventory, oldname, newname);
    newfilename = gwy_resource_build_filename(resource);
    if (g_rename(oldfilename, newfilename) != 0) {
        /* FIXME: GUIze this */
        g_warning("Cannot rename resource file: %s to %s",
                  oldfilename, newfilename);
        gwy_inventory_rename_item(inventory, newname, oldname);
    }
    g_free(oldfilename);
    g_free(newfilename);

    if (editor->edited_resource
        && gwy_strequal(oldname, editor->edited_resource->str)) {
        g_string_assign(editor->edited_resource, newname);
        gwy_resource_editor_update_title(editor);
    }
}

/**
 * gwy_resource_editor_queue_save:
 * @editor: A resource editor.
 *
 * Queues commit of resource changes, marking the currently edited resource
 * `dirty'.
 *
 * Call this method in particular resource editor subclass whenever user
 * changes some editor property.
 **/
void
gwy_resource_editor_queue_commit(GwyResourceEditor *editor)
{
    g_return_if_fail(GWY_IS_RESOURCE_EDITOR(editor));

    if (editor->commit_id)
        g_source_remove(editor->commit_id);

    editor->commit_id
        = g_timeout_add_full(G_PRIORITY_LOW, 200,
                             (GSourceFunc)&gwy_resource_editor_commit, editor,
                             NULL);
}

/**
 * gwy_resource_editor_commit:
 * @editor: A resource editor.
 *
 * Commits pending resource changes, if there are any.
 *
 * It calls @apply_changes method first (if it exists), then saves resource to
 * disk.
 *
 * Changes are always immediately committed (if there are any pending): before
 * the editor is destroyed, when a resource stops being edited, before a
 * resource is deleted, before a resource is renamed. When a resource is newly
 * created, it is immediately created on disk too.
 **/
void
gwy_resource_editor_commit(GwyResourceEditor *editor)
{
    GwyResourceEditorClass *klass;

    g_return_if_fail(GWY_IS_RESOURCE_EDITOR(editor));
    gwy_debug("%u", editor->commit_id);

    if (!editor->commit_id)
        return;

    g_return_if_fail(editor->edited_resource
                     && *editor->edited_resource->str);

    klass = GWY_RESOURCE_EDITOR_GET_CLASS(editor);
    if (klass->apply_changes)
        klass->apply_changes(editor);

    gwy_resource_editor_save(editor, editor->edited_resource->str);
    editor->commit_id = 0;
}

static gboolean
gwy_resource_editor_save(GwyResourceEditor *editor,
                         const gchar *name)
{
    GwyInventory *inventory;
    GwyResource *resource;
    GtkTreeModel *model;
    gchar *filename;
    GString *str;
    FILE *fh;

    gwy_debug("<%s>", name);
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(editor->treeview));
    inventory = gwy_inventory_store_get_inventory(GWY_INVENTORY_STORE(model));
    resource = gwy_inventory_get_item(inventory, name);
    if (!resource) {
        g_warning("Trying to save `%s' that isn't in inventory", name);
        return FALSE;
    }

    filename = gwy_resource_build_filename(resource);
    fh = g_fopen(filename, "w");
    if (!fh) {
        /* FIXME: GUIze this */
        g_warning("Cannot save resource file: %s", filename);
        g_free(filename);
        return FALSE;
    }
    g_free(filename);

    str = gwy_resource_dump(resource);
    fwrite(str->str, 1, str->len, fh);
    fclose(fh);
    g_string_free(str, TRUE);

    return TRUE;
}

static void
gwy_resource_editor_update_title(GwyResourceEditor *editor)
{
    GwyResourceEditorClass *klass;
    gchar *title;

    gwy_debug("");
    klass = GWY_RESOURCE_EDITOR_GET_CLASS(editor);
    title = g_strdup_printf(klass->editor_title, editor->edited_resource->str);
    gtk_window_set_title(GTK_WINDOW(editor->edit_window), title);
    g_free(title);
}

static GwyResource*
gwy_resource_editor_get_active(GwyResourceEditor *editor,
                               GtkTreeModel **model,
                               GtkTreeIter *iter,
                               const gchar *warnwhat)
{
    GtkTreeSelection *selection;
    GwyResource *resource;
    GtkTreeModel *treemodel;
    GtkTreeIter treeiter;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(editor->treeview));
    if (!gtk_tree_selection_get_selected(selection, &treemodel, &treeiter)) {
        if (warnwhat)
            g_critical("Something must be selected for `%s'!", warnwhat);
        return NULL;
    }

    gtk_tree_model_get(treemodel, &treeiter, 0, &resource, -1);
    if (model)
        *model = treemodel;
    if (iter)
        *iter = treeiter;

    return resource;
}

/**
 * gwy_resource_class_setup_keys:
 * @klass: A resource editor class.
 *
 * Sets up particular resource editor class.
 *
 * To be called in particular class initialization methods.
 **/
void
gwy_resource_class_setup(GwyResourceEditorClass *klass)
{
    GwyResourceClass *rklass;
    const gchar *name;
    GString *str;

    gwy_debug("");
    g_return_if_fail(GWY_IS_RESOURCE_EDITOR_CLASS(klass));

    rklass = g_type_class_ref(klass->resource_type);
    name = gwy_resource_class_get_name(rklass);
    g_type_class_unref(rklass);

    str = g_string_new("");
    g_string_printf(str, "/app/%s/editor/current", name);
    klass->current_key = g_quark_from_string(str->str);
    g_string_printf(str, "/app/%s/editor/position/width", name);
    klass->width_key = g_quark_from_string(str->str);
    g_string_printf(str, "/app/%s/editor/position/height", name);
    klass->height_key = g_quark_from_string(str->str);
}

/**
 * gwy_resource_editor_get_edited:
 * @editor: A resource editor.
 *
 * Gets the currently edited resource.
 *
 * It is an error to call this method when no resource is being edited.
 *
 * Returns: The currently edited resource.
 **/
GwyResource*
gwy_resource_editor_get_edited(GwyResourceEditor *editor)
{
    GwyInventory *inventory;
    GtkTreeModel *model;

    g_return_val_if_fail(GWY_IS_RESOURCE_EDITOR(editor), NULL);
    g_return_val_if_fail(editor->edited_resource, NULL);

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(editor->treeview));
    inventory = gwy_inventory_store_get_inventory(GWY_INVENTORY_STORE(model));

    return (GwyResource*)gwy_inventory_get_item(inventory,
                                                editor->edited_resource->str);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
