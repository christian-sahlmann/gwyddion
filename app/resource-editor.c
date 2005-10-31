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
#include <app/resource-editor.h>

#include <libdraw/gwyglmaterial.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwydgets/gwydgetutils.h>

enum {
    WHATEVER,
    LAST_SIGNAL
};

static void gwy_resource_editor_finalize     (GObject *object);
static void gwy_resource_editor_set_property (GObject *object,
                                              guint prop_id,
                                              const GValue *value,
                                              GParamSpec *pspec);
static void gwy_resource_editor_get_property (GObject *object,
                                              guint prop_id,
                                              GValue *value,
                                              GParamSpec *pspec);
static void gwy_resource_editor_cell_name    (GtkTreeViewColumn *column,
                                              GtkCellRenderer *renderer,
                                              GtkTreeModel *model,
                                              GtkTreeIter *iter,
                                              gpointer data);
static void gwy_resource_editor_changed      (GtkTreeSelection *selection,
                                              GwyResourceEditor *editor);
static void gwy_resource_editor_destroy      (GtkObject *object);
static void gwy_resource_editor_new          (GwyResourceEditor *editor);
static void gwy_resource_editor_duplicate    (GwyResourceEditor *editor);
static void gwy_resource_editor_copy         (GwyResourceEditor *editor,
                                              const gchar *name,
                                              const gchar *newname);
static void gwy_resource_editor_delete       (GwyResourceEditor *editor);
static void gwy_resource_editor_set_default  (GwyResourceEditor *editor);
static void gwy_resource_editor_edit         (GwyResourceEditor *editor);
static void gwy_resource_editor_row_activated(GwyResourceEditor *editor,
                                              GtkTreePath *path,
                                              GtkTreeViewColumn *column);
static void gwy_resource_editor_name_edited  (GwyResourceEditor *editor,
                                              const gchar *strpath,
                                              const gchar *text);

static guint resource_editor_signals[LAST_SIGNAL] = { 0 };

/* XXX: for initial testing we need something instantiable
G_DEFINE_ABSTRACT_TYPE(GwyResourceEditor, gwy_resource_editor,
                       GTK_TYPE_WINDOW)
                       */
G_DEFINE_TYPE(GwyResourceEditor, gwy_resource_editor, GTK_TYPE_WINDOW)

static void
gwy_resource_editor_class_init(GwyResourceEditorClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_resource_editor_finalize;
    gobject_class->get_property = gwy_resource_editor_get_property;
    gobject_class->set_property = gwy_resource_editor_set_property;

    object_class->destroy = gwy_resource_editor_destroy;

    /* XXX XXX XXX */
    klass->resource_type = GWY_TYPE_GL_MATERIAL;
    klass->base_resource = GWY_GL_MATERIAL_DEFAULT;
    klass->window_title = "Window Title";
    klass->editor_title = "Editor Title";
    klass->construct_treeview = gwy_gl_material_tree_view_new;
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
    GtkWidget *hbox, *button;
    GtkTooltips *tooltips;
    GtkWidget *scwin;
    GList *rlist;
    guint i;

    editor->active = g_string_new("");

    klass = GWY_RESOURCE_EDITOR_GET_CLASS(editor);

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

    /* XXX: must get active resource */
    editor->treeview
        = klass->construct_treeview(G_CALLBACK(gwy_resource_editor_changed),
                                    editor, editor->active->str);
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
gwy_resource_editor_finalize(GObject *object)
{
    GwyResourceEditor *editor = (GwyResourceEditor*)object;

    g_string_free(editor->active, TRUE);

    G_OBJECT_CLASS(gwy_resource_editor_parent_class)->finalize(object);
}

static void
gwy_resource_editor_set_property(GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    GwyResourceEditor *editor = GWY_RESOURCE_EDITOR(object);

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
    GwyResourceEditor *editor = GWY_RESOURCE_EDITOR(object);

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
    g_string_assign(editor->active, gwy_resource_get_name(resource));

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
    gwy_debug("");
    /*
    GString *s = editor->active;

    memset(editor, 0, sizeof(GwyResourceEditor));
    editor->active = s;
    */

    GTK_OBJECT_CLASS(gwy_resource_editor_parent_class)->destroy(object);
}

static void
gwy_resource_editor_set_default(GwyResourceEditor *editor)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    GwyResource *resource;
    GwyInventory *inventory;

    gwy_debug("");
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(editor->treeview));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        g_warning("Something should be selected for `Set Default'");
        return;
    }

    gtk_tree_model_get(model, &iter, 0, &resource, -1);
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
    gwy_debug("");
    gwy_resource_editor_copy(editor, editor->active->str, NULL);
}

static void
gwy_resource_editor_copy(GwyResourceEditor *editor,
                         const gchar *name,
                         const gchar *newname)
{
    GtkTreeModel *model;
    GwyInventory *inventory;
    GwyResource *resource;
    FILE *fh;
    gchar *filename;
    GString *str;

    gwy_debug("");
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(editor->treeview));
    inventory = gwy_inventory_store_get_inventory(GWY_INVENTORY_STORE(model));
    resource = gwy_inventory_new_item(inventory, name, newname);
    gwy_resource_tree_view_set_active(editor->treeview,
                                      gwy_resource_get_name(resource));

    /* Save new resource to file */
    filename = gwy_resource_build_filename(resource);
    fh = g_fopen(filename, "w");
    if (!fh) {
        /* FIXME: GUIze this */
        g_warning("Cannot save resource file: %s", filename);
        g_free(filename);
        return;
    }
    g_free(filename);
    str = gwy_resource_dump(resource);
    fwrite(str->str, 1, str->len, fh);
    fclose(fh);
    g_string_free(str, TRUE);

    /* XXX: don't? gwy_resource_editor_edit(editor); */
}

static void
gwy_resource_editor_delete(GwyResourceEditor *editor)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    GwyResource *resource;
    GwyInventory *inventory;
    GtkTreePath *path;
    gchar *filename;
    int result;

    gwy_debug("");
    /* Get selected resource, and the inventory it belongs to: */
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(editor->treeview));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        g_warning("Something should be selected for `Delete'");
        return;
    }
    gtk_tree_model_get(model, &iter, 0, &resource, -1);
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
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_path_free(path);
}

static void
gwy_resource_editor_edit(GwyResourceEditor *editor)
{
    gwy_debug("");
    /*
    GwyGLMaterial *material;

    if (!editor->edit_window) {
        gwy_resource_editor_construct(editor);
    }
    gwy_resource_editor_preview_set(editor);
    material = gwy_inventory_get_item(gwy_gl_materials(), editor->active->str);
    editor->old[GL_MATERIAL_AMBIENT] = *gwy_gl_material_get_ambient(material);
    editor->old[GL_MATERIAL_DIFFUSE] = *gwy_gl_material_get_diffuse(material);
    editor->old[GL_MATERIAL_SPECULAR] = *gwy_gl_material_get_specular(material);
    editor->old[GL_MATERIAL_EMISSION] = *gwy_gl_material_get_emission(material);
    gwy_resource_editor_update(editor);
    gtk_window_present(GTK_WINDOW(editor->edit_window));
    */
}

static void
gwy_resource_editor_row_activated(GwyResourceEditor *editor,
                                  GtkTreePath *path,
                                  GtkTreeViewColumn *column)
{
    gwy_debug("");
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
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
