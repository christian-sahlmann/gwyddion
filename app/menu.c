/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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

/* FIXME: most of this file belongs to gwyddion, not libgwyapp.
 * - menu constructors should be in gwyddion
 * - last-run function stuff should be in ???
 */

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwymodule/gwymodulebrowser.h>
#include <libgwydgets/gwydgets.h>
#include "app.h"
#include "menu.h"
#include "file.h"
#include "filelist.h"
#include "gwyappinternal.h"

typedef struct {
    const gchar *name;
    gchar *path;
    gchar *path_translated;
    gchar *item_canonical;
    gchar *item_translated;
    gchar *item_translated_canonical;
    gchar *item_collated;
    GtkWidget *widget;
} MenuNodeData;

static void       gwy_app_file_open_recent_cb       (GObject *item);
static void       gwy_app_rerun_process_func        (gpointer user_data);
static void       gwy_app_update_last_process_func  (const gchar *name);
static gchar*     fix_recent_file_underscores       (gchar *s);

int gwy_app_n_recent_files = 10;

static GQuark repeat_last_quark = 0;
static GQuark reshow_last_quark = 0;
static GQuark last_name_quark   = 0;

static GtkWidget           *recent_files_menu = NULL;
static GtkWidget           *process_menu      = NULL;
static GtkWidget           *graph_menu        = NULL;
static GtkTooltips         *app_tooltips      = NULL;
static GwySensitivityGroup *app_sensgroup     = NULL;

/* FIXME: how can MSVC can get to needing this when we are not DEBUGging? */
#if (defined(DEBUG) || defined(_MSC_VER))
static gchar*
debug_menu_sens_flags(guint flags)
{
    static const GwyEnum menu_enum[] = {
        { "Data",  GWY_MENU_FLAG_DATA,       },
        { "Undo",  GWY_MENU_FLAG_UNDO,       },
        { "Redo",  GWY_MENU_FLAG_REDO,       },
        { "Graph", GWY_MENU_FLAG_GRAPH,      },
        { "Last",  GWY_MENU_FLAG_LAST_PROC,  },
        { "LastG", GWY_MENU_FLAG_LAST_GRAPH, },
        { "Mask",  GWY_MENU_FLAG_DATA_MASK,  },
        { "Show",  GWY_MENU_FLAG_DATA_SHOW,  },
    };

    /* this is going to leak some memory, but no one cares in debugging mode */
    return gwy_flags_to_string(flags, menu_enum, G_N_ELEMENTS(menu_enum), NULL);
}
#endif

/**
 * gwy_app_menu_canonicalize_label:
 * @label: Menu item label to canonicalize.
 *
 * Canonicalized menu item label in place.
 *
 * That is, removes accelerator underscores and trailing ellipsis.
 **/
static void
gwy_app_menu_canonicalize_label(gchar *label)
{
    guint i, j;

    for (i = j = 0; label[i]; i++) {
        label[j] = label[i];
        if (label[i] != '_' || label[i+1] == '_')
            j++;
    }
    /* If the label *ends* with an underscore, just kill it */
    label[j] = '\0';
    if (j >= 3 && label[j-3] == '.' && label[j-2] == '.' && label[j-1] == '.')
        label[j-3] = '\0';
}

/*****************************************************************************
 *
 * Module function menu building
 *
 *****************************************************************************/

/**
 * gwy_app_menu_add_node:
 * @root: Module function menu root.
 * @name: The name of the function to add.
 * @path: Menu path of this function.
 *
 * Inserts a module function to menu tree.
 *
 * This is stage 1, to sort out the information that gwy_foo_func_foreach()
 * gives us to a tree.
 **/
static void
gwy_app_menu_add_node(GNode *root,
                      const gchar *name,
                      const gchar *path)
{
    MenuNodeData *data = NULL;
    GNode *node, *child;
    gchar **segments, **segments_canonical;
    gchar *s;
    guint n, i;

    g_return_if_fail(path && path[0] == '/');
    segments = g_strsplit(path, "/", 0);

    /* Canonicalize */
    n = g_strv_length(segments);
    segments_canonical = g_new0(gchar*, n+1);
    for (i = 0; i < n; i++) {
        segments_canonical[i] = g_strdup(segments[i]);
        gwy_app_menu_canonicalize_label(segments_canonical[i]);
    }

    /* Find node in the tree to branch off */
    node = root;
    i = 1;
    while (segments_canonical[i]) {
        gwy_debug("Searching for <%s> in <%s>",
                  segments_canonical[i], ((MenuNodeData*)node->data)->path);
        for (child = node->children; child; child = child->next) {
            data = (MenuNodeData*)child->data;
            if (gwy_strequal(data->item_canonical, segments_canonical[i])) {
                gwy_debug("Found <%s>, descending", segments_canonical[i]);
                break;
            }
        }
        if (child) {
            node = child;
            i++;
        }
        else {
            gwy_debug("Not found <%s>, stopping search", segments_canonical[i]);
            break;
        }
    }
    if (!segments[i]) {
        g_warning("Item with path `%s' already exists", path);
        goto fail;
    }
    if (i > 1 && (!data || ((MenuNodeData*)node->data)->name)) {
        g_warning("Item with path `%s' cannot be both leaf and branch", path);
        goto fail;
    }

    /* Now recursively create new children till segments[] is exhausted */
    gwy_debug("Branching off new child of <%s>",
              ((MenuNodeData*)node->data)->path);
    while (segments[i]) {
        data = g_new0(MenuNodeData, 1);
        s = segments[i+1];
        segments[i+1] = NULL;
        data->path = g_strjoinv("/", segments);
        segments[i+1] = s;

        data->item_canonical = segments_canonical[i];
        segments_canonical[i] = NULL;
        gwy_debug("Created <%s> with full path <%s>",
                  data->item_canonical, data->path);
        node = g_node_prepend_data(node, data);
        i++;
    }
    /* The leaf node is the real item */
    data->name = name;
    s = gettext(path);
    if (!gwy_strequal(s, path)) {
        data->path_translated = g_strdup(s);
    }

fail:
    g_strfreev(segments);
    g_strfreev(segments_canonical);
}

/**
 * gwy_app_menu_resolve_translations:
 * @node: Module function menu tree node to process.
 * @userdata: Unused.
 *
 * Resolves partial translations of menu paths, calculates collation keys.
 *
 * Must be called on nodes in %G_POST_ORDER.
 *
 * This is stage 2, translations of particular items are extracted and
 * translations are propagated from non-leaf nodes up to braches.
 *
 * FIXME: We should better deal with situations like missing accelerators in
 * some translations by prefering those with accelerators.  Or at least print
 * some warning.
 *
 * Returns: Always %FALSE.
 **/
static gboolean
gwy_app_menu_resolve_translations(GNode *node,
                                  G_GNUC_UNUSED gpointer userdata)
{
    MenuNodeData *data = (MenuNodeData*)node->data;
    MenuNodeData *pdata;
    const gchar *p;

    if (G_NODE_IS_ROOT(node))
        return FALSE;

    pdata = (MenuNodeData*)node->parent->data;
    if (!data->path_translated) {
        gwy_debug("Path <%s> is untranslated", data->path);
        data->path_translated = g_strdup(data->path);
    }
    else {
        gwy_debug("Path <%s> is translated", data->path);
    }

    p = strrchr(data->path_translated, '/');
    g_return_val_if_fail(p, FALSE);
    data->item_translated = g_strdup(p+1);
    data->item_translated_canonical = g_strdup(data->item_translated);
    gwy_app_menu_canonicalize_label(data->item_translated_canonical);
    data->item_collated = g_utf8_collate_key(data->item_translated_canonical,
                                             -1);

    if (!pdata->path_translated) {
        pdata->path_translated = g_strndup(data->path_translated,
                                           p - data->path_translated);
        gwy_debug("Deducing partial translation: <%s> from <%s>",
                  pdata->path, data->path);
    }

    return FALSE;
}

/**
 * gwy_app_menu_sort_submenus:
 * @node: Module function menu tree node to process.
 * @userdata: Unused.
 *
 * Sorts module function submenus alphabetically.
 *
 * Must be called on nodes in %G_PRE_ORDER.
 *
 * This is stage 3, childrens of each node are sorted according to collation
 * keys (calculated in stage 2).
 *
 * Returns: Always %FALSE.
 **/
static gboolean
gwy_app_menu_sort_submenus(GNode *node,
                           G_GNUC_UNUSED gpointer userdata)
{
    MenuNodeData *data1, *data2;
    GNode *c1, *c2;
    gboolean ok = FALSE;

    if (G_NODE_IS_LEAF(node))
        return FALSE;

    /* This is bubble sort. */
    while (!ok) {
        ok = TRUE;
        for (c1 = node->children, c2 = c1->next; c2; c1 = c2, c2 = c2->next) {
            data1 = (MenuNodeData*)c1->data;
            data2 = (MenuNodeData*)c2->data;
            if (strcmp(data1->item_collated, data2->item_collated) < 0)
                continue;

            c1->next = c2->next;
            c2->prev = c1->prev;
            if (c1->prev)
                c1->prev->next = c2;
            if (c1 == node->children)
                node->children = c2;
            if (c2->next)
                c2->next->prev = c1;
            c1->prev = c2;
            c2->next = c1;
            c1 = c1->prev;
            c2 = c2->next;
            ok = FALSE;
        }
    }

    return FALSE;
}

/**
 * gwy_app_menu_create_widgets:
 * @node: Module function menu tree node to process.
 * @callback: Callback function to connect to "activate" signal of the
 *            created menu items, swapped.  Function name is used as callback
 *            data.
 *
 * Creates widgets from module function tree.
 *
 * Must be called on nodes in %G_POST_ORDER.
 *
 * This is stage 4, menu items are created from leaves and branches, submenus
 * are attached to branches (with tearoffs, titles, and everything).  Each node
 * data gets its @widget field filled with corresponding menu item, except
 * root node that gets it @with field filled with the top-level menu.
 *
 * Returns: Always %FALSE.
 **/
static gboolean
gwy_app_menu_create_widgets(GNode *node,
                            gpointer callback)
{
    MenuNodeData *cdata, *data = (MenuNodeData*)node->data;
    GNode *child;
    GtkWidget *menu, *item;

    if (!G_NODE_IS_ROOT(node))
        data->widget = gtk_menu_item_new_with_mnemonic(data->item_translated);
    if (G_NODE_IS_LEAF(node)) {
        g_signal_connect_swapped(data->widget, "activate",
                                 G_CALLBACK(callback), (gpointer)data->name);
        return FALSE;
    }

    menu = gtk_menu_new();
    gtk_menu_set_title(GTK_MENU(menu), data->item_translated_canonical);
    item = gtk_tearoff_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    for (child = node->children; child; child = child->next) {
        cdata = (MenuNodeData*)child->data;
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), cdata->widget);
    }
    if (G_NODE_IS_ROOT(node))
        data->widget = menu;
    else
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(data->widget), menu);
    gtk_widget_show_all(menu);

    return FALSE;
}

/**
 * gwy_app_menu_setup_sensitivity:
 * @node: Module function menu tree node to process.
 * @userdata: Function to get sensitivity flags for @node.
 *
 * Sets up widget sensitivities.
 *
 * This is stage 5, sensitivity set-up.
 *
 * Returns: Always %FALSE.
 **/
static gboolean
gwy_app_menu_setup_sensitivity(GNode *node,
                               gpointer userdata)
{
    MenuNodeData *data = (MenuNodeData*)node->data;
    guint (*get_flags)(const gchar *name);

    get_flags = userdata;
    gwy_app_sensitivity_add_widget(data->widget, get_flags(data->name));
    return FALSE;
}

/**
 * gwy_app_menu_free_node_data:
 * @node: Module function menu tree node to process.
 * @userdata: Unused.
 *
 * Frees module function menu tree auxiliary data.
 *
 * This is stage 6, clean-up.
 *
 * Returns: Always %FALSE.
 **/
static gboolean
gwy_app_menu_free_node_data(GNode *node,
                            G_GNUC_UNUSED gpointer userdata)
{
    MenuNodeData *data = (MenuNodeData*)node->data;

    g_free(data->path);
    g_free(data->path_translated);
    g_free(data->item_canonical);
    g_free(data->item_collated);
    g_free(data->item_translated);
    g_free(data->item_translated_canonical);
    g_free(data);

    return FALSE;
}

/**
 * gwy_app_build_module_func_menu:
 * @root: Module function menu root.
 * @callback: The callback function to connect to leaves.
 *
 * Executes stages 2-4 of module function menu construction and destroys the
 * node tree.
 *
 * Returns: The top-level menu widget.
 **/
static GtkWidget*
gwy_app_build_module_func_menu(GNode *root,
                               GCallback callback,
                               guint (*get_flags)(const gchar *name))
{
    GtkWidget *menu;

    g_node_traverse(root, G_POST_ORDER, G_TRAVERSE_ALL, -1,
                    &gwy_app_menu_resolve_translations, NULL);
    g_node_traverse(root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
                    &gwy_app_menu_sort_submenus, NULL);
    g_node_traverse(root, G_POST_ORDER, G_TRAVERSE_ALL, -1,
                    &gwy_app_menu_create_widgets, callback);
    menu = ((MenuNodeData*)root->data)->widget;
    if (get_flags)
        g_node_traverse(root, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1,
                        &gwy_app_menu_setup_sensitivity, get_flags);
    g_node_traverse(root, G_POST_ORDER, G_TRAVERSE_ALL, -1,
                    &gwy_app_menu_free_node_data, NULL);
    g_node_destroy(root);

    return menu;
}

static void
gwy_app_menu_add_proc_func(const gchar *name,
                           GNode *root)
{
    gwy_app_menu_add_node(root, name, gwy_process_func_get_menu_path(name));
}

GtkWidget*
gwy_app_build_process_menu(GtkAccelGroup *accel_group)
{
    MenuNodeData *data;
    GtkWidget *menu;
    GNode *root;

    data = g_new0(MenuNodeData, 1);
    data->path = g_strdup("");
    data->item_translated = g_strdup(_("_Data Process"));
    root = g_node_new(data);
    gwy_process_func_foreach((GFunc)&gwy_app_menu_add_proc_func, root);
    menu = gwy_app_build_module_func_menu(root,
                                          G_CALLBACK(gwy_app_run_process_func),
                                          gwy_process_func_get_sensitivity_flags);
    gtk_menu_set_accel_group(GTK_MENU(menu), accel_group);
    process_menu = menu;

    return menu;
}

static void
gwy_app_menu_add_graph_func(const gchar *name,
                            GNode *root)
{
    gwy_app_menu_add_node(root, name, gwy_graph_func_get_menu_path(name));
}

GtkWidget*
gwy_app_build_graph_menu(GtkAccelGroup *accel_group)
{
    MenuNodeData *data;
    GtkWidget *menu;
    GNode *root;

    data = g_new0(MenuNodeData, 1);
    data->path = g_strdup("");
    data->item_translated = g_strdup(_("_Graph"));
    root = g_node_new(data);
    gwy_graph_func_foreach((GFunc)&gwy_app_menu_add_graph_func, root);
    menu = gwy_app_build_module_func_menu(root,
                                          G_CALLBACK(gwy_app_run_graph_func),
                                          gwy_graph_func_get_sensitivity_flags);
    gtk_menu_set_accel_group(GTK_MENU(menu), accel_group);
    graph_menu = menu;

    return menu;
}

void
gwy_app_process_menu_add_run_last(GtkWidget *menu)
{
    static const gchar *reshow_accel_path = "<proc>/Re-show Last";
    static const gchar *repeat_accel_path = "<proc>/Repeat Last";
    GtkWidget *item;

    if (!reshow_last_quark)
        reshow_last_quark = g_quark_from_static_string("gwy-app-menu-"
                                                       "reshow-last");
    item = gtk_menu_item_new_with_mnemonic(_("Re-show Last"));
    gtk_menu_item_set_accel_path(GTK_MENU_ITEM(item), reshow_accel_path);
    gtk_accel_map_add_entry(reshow_accel_path, GDK_f,
                            GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    g_object_set_qdata(G_OBJECT(menu), reshow_last_quark, item);
    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, 1);
    gwy_app_sensitivity_add_widget(item, (GWY_MENU_FLAG_DATA
                                          | GWY_MENU_FLAG_LAST_PROC));
    g_signal_connect_swapped(item, "activate",
                             G_CALLBACK(gwy_app_rerun_process_func),
                             GUINT_TO_POINTER(GWY_RUN_INTERACTIVE));

    if (!repeat_last_quark)
        repeat_last_quark = g_quark_from_static_string("gwy-app-menu-"
                                                       "repeat-last");
    item = gtk_menu_item_new_with_mnemonic(_("Repeat Last"));
    gtk_menu_item_set_accel_path(GTK_MENU_ITEM(item), repeat_accel_path);
    gtk_accel_map_add_entry(repeat_accel_path, GDK_f, GDK_CONTROL_MASK);
    g_object_set_qdata(G_OBJECT(menu), repeat_last_quark, item);
    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, 1);
    gwy_app_sensitivity_add_widget(item, (GWY_MENU_FLAG_DATA
                                          | GWY_MENU_FLAG_LAST_PROC));
    g_signal_connect_swapped(item, "activate",
                             G_CALLBACK(gwy_app_rerun_process_func),
                             GUINT_TO_POINTER(GWY_RUN_INTERACTIVE));
}

static void
gwy_app_rerun_process_func(gpointer user_data)
{
    GwyRunType run, available_run_modes;
    const gchar *name;

    g_return_if_fail(process_menu);
    g_return_if_fail(last_name_quark);

    name = (const gchar*)g_object_get_qdata(G_OBJECT(process_menu),
                                            last_name_quark);
    g_return_if_fail(name);
    run = GPOINTER_TO_UINT(user_data);
    available_run_modes = gwy_process_func_get_run_types(name);
    g_return_if_fail(available_run_modes);
    gwy_debug("run mode = %u, available = %u", run, available_run_modes);

    /* try to find some mode `near' to requested one, otherwise just use any */
    run &= available_run_modes;
    if (run)
        gwy_app_run_process_func_in_mode(name, run);
    else
        gwy_app_run_process_func(name);
}

/**
 * gwy_app_run_process_func:
 * @name: A data processing function name.
 *
 * Runs a data processing function on current data.
 *
 * From the run modes function @name supports, the most interactive one is
 * selected.
 *
 * Returns: The actually used mode (nonzero), or 0 on failure.
 **/
GwyRunType
gwy_app_run_process_func(const gchar *name)
{
    GwyRunType run_types[] = { GWY_RUN_INTERACTIVE, GWY_RUN_IMMEDIATE, };
    GwyRunType available_run_modes;
    gsize i;

    gwy_debug("`%s'", name);
    available_run_modes = gwy_process_func_get_run_types(name);
    g_return_val_if_fail(available_run_modes, 0);
    for (i = 0; i < G_N_ELEMENTS(run_types); i++) {
        if (run_types[i] & available_run_modes) {
            gwy_app_run_process_func_in_mode(name, run_types[i]);
            return run_types[i];
        }
    }
    return 0;
}

/**
 * gwy_app_run_process_func_in_mode:
 * @name: A data processing function name.
 * @run: A run mode.
 *
 * Runs a data processing function @on current data in specified mode.
 **/
void
gwy_app_run_process_func_in_mode(const gchar *name,
                                 GwyRunType run)
{
    GwyContainer *data;

    gwy_debug("`%s'", name);
    if (!(run & gwy_process_func_get_run_types(name)))
        return;

    data = gwy_app_get_current_data();
    g_return_if_fail(data);
    gwy_process_func_run(name, data, run);
    gwy_app_update_last_process_func(name);
    gwy_app_sensitivity_set_state(GWY_MENU_FLAG_LAST_PROC,
                                  GWY_MENU_FLAG_LAST_PROC);
}

static void
gwy_app_update_last_process_func(const gchar *name)
{
    GtkWidget *repeat_item, *reshow_item, *label;
    GwyMenuSensFlags sens;
    const gchar *menu_path;
    gchar *s, *lab;

    if (!last_name_quark)
        last_name_quark = g_quark_from_static_string("gwy-app-menu-last"
                                                     "-func-name");

    g_return_if_fail(GTK_IS_MENU(process_menu));
    g_object_set_qdata(G_OBJECT(process_menu), last_name_quark, (gpointer)name);
    repeat_item = g_object_get_qdata(G_OBJECT(process_menu), repeat_last_quark);
    reshow_item = g_object_get_qdata(G_OBJECT(process_menu), reshow_last_quark);
    g_return_if_fail(repeat_item && reshow_item);

    /* FIXME: at least the `_' removal should not be necessary as libgwymodule
     * knows the right path */
    menu_path = gwy_process_func_get_menu_path(name);
    menu_path = strrchr(menu_path, '/');
    g_assert(menu_path);
    menu_path++;
    lab = g_strdup(menu_path);
    gwy_app_menu_canonicalize_label(lab);
    sens = (gwy_process_func_get_sensitivity_flags(name)
            | GWY_MENU_FLAG_LAST_PROC);
    gwy_debug("Repeat sens: %s", debug_menu_sens_flags(sens));

    label = GTK_BIN(repeat_item)->child;
    s = g_strconcat(_("Repeat"), " (", lab, ")", NULL);
    gtk_label_set_text_with_mnemonic(GTK_LABEL(label), s);
    gwy_sensitivity_group_set_widget_mask(app_sensgroup, repeat_item, sens);
    g_free(s);

    label = GTK_BIN(reshow_item)->child;
    s = g_strconcat(_("Re-show"), " (", lab, ")", NULL);
    gtk_label_set_text_with_mnemonic(GTK_LABEL(label), s);
    gwy_sensitivity_group_set_widget_mask(app_sensgroup, reshow_item, sens);
    g_free(s);

    g_free(lab);
}

void
gwy_app_run_graph_func(const gchar *name)
{
    GtkWidget *graph_window;

    gwy_debug("`%s'", name);
    graph_window = gwy_app_graph_window_get_current();
    g_return_if_fail(GWY_IS_GRAPH_WINDOW(graph_window));
    g_return_if_fail(GWY_IS_GRAPH(GWY_GRAPH_WINDOW(graph_window)->graph));
    gwy_graph_func_run(name, GWY_GRAPH(GWY_GRAPH_WINDOW(graph_window)->graph));
}

static void
gwy_app_recent_file_list_cb(void)
{
    static GtkWidget *recent_file_list;

    if (recent_file_list) {
        gtk_window_present(GTK_WINDOW(recent_file_list));
        return;
    }

    recent_file_list = gwy_app_recent_file_list_new();
    g_object_add_weak_pointer(G_OBJECT(recent_file_list),
                              (gpointer*)&recent_file_list);
    gtk_widget_show(recent_file_list);
}

/**
 * gwy_app_menu_recent_files_update:
 * @recent_files: A list of recent file names, in UTF-8.
 *
 * Updates recent file menu to show @recent_files.
 **/
void
gwy_app_menu_recent_files_update(GList *recent_files)
{
    GtkWidget *item;
    GQuark quark;
    GList *l, *child;
    gchar *s, *label, *filename;
    gint i;

    if (!recent_files_menu)
        return;

    child = GTK_MENU_SHELL(recent_files_menu)->children;
    if (GTK_IS_TEAROFF_MENU_ITEM(child->data))
        child = g_list_next(child);

    quark = g_quark_from_string("filename");
    for (i = 0, l = recent_files;
         l && i < gwy_app_n_recent_files;
         l = g_list_next(l), i++) {
        filename = (gchar*)l->data;
        s = fix_recent_file_underscores(g_path_get_basename(filename));
        label = g_strdup_printf("%s%d. %s", i < 10 ? "_" : "", i, s);
        if (child) {
            item = GTK_BIN(child->data)->child;
            gwy_debug("reusing item %p for <%s> [#%d]", item, s, i);
            gtk_label_set_text_with_mnemonic(GTK_LABEL(item), label);
            g_object_set_qdata_full(G_OBJECT(child->data), quark,
                                    g_strdup(filename), g_free);
            gtk_widget_show(GTK_WIDGET(child->data));
            child = g_list_next(child);
        }
        else {
            item = gtk_menu_item_new_with_mnemonic(label);
            gwy_debug("creating item %p for <%s> [#%d]", item, s, i);
            g_object_set_qdata_full(G_OBJECT(item), quark, g_strdup(filename),
                                    g_free);
            gtk_menu_shell_append(GTK_MENU_SHELL(recent_files_menu), item);
            g_signal_connect(item, "activate",
                             G_CALLBACK(gwy_app_file_open_recent_cb), NULL);
            gtk_widget_show(item);
        }
        g_free(label);
        g_free(s);
    }

    /* keep konstant number of entries, otherwise it's just too hard to
     * manage the separated stuff at the end */
    while (i < gwy_app_n_recent_files) {
        if (child) {
            item = GTK_BIN(child->data)->child;
            gwy_debug("hiding item %p [#%d]", item, i);
            gtk_widget_hide(child->data);
            child = g_list_next(child);
        }
        else {
            item = gtk_menu_item_new_with_mnemonic("Thou Canst See This");
            gwy_debug("adding hidden item %p [#%d]", item, i);
            gtk_menu_shell_append(GTK_MENU_SHELL(recent_files_menu), item);
            g_signal_connect(item, "activate",
                             G_CALLBACK(gwy_app_file_open_recent_cb), NULL);
        }
        i++;
    }

    /* FIXME: if the menu is in tear-off state and entries were added, it
     * doesn't grow but an ugly scrollbar appears. How to make it grow? */

    /* if there are still some entries, the separated entires already exist,
     * so we are done */
    if (child) {
        g_return_if_fail(GTK_IS_SEPARATOR_MENU_ITEM(child->data));
        return;
    }
    /* separator */
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(recent_files_menu), item);
    gtk_widget_show(item);
    /* doc history */
    item = gtk_image_menu_item_new_with_mnemonic(_("_Document history"));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
                                  gtk_image_new_from_stock(GTK_STOCK_OPEN,
                                                           GTK_ICON_SIZE_MENU));
    gtk_menu_shell_append(GTK_MENU_SHELL(recent_files_menu), item);
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_app_recent_file_list_cb), NULL);
    gtk_widget_show(item);
}

static gchar*
fix_recent_file_underscores(gchar *s)
{
    gchar *s2;

    s2 = gwy_strreplace(s, "_", "__", (gsize)-1);
    g_free(s);

    return s2;
}

void
gwy_app_menu_set_recent_files_menu(GtkWidget *menu)
{
    g_return_if_fail(GTK_IS_MENU(menu));
    g_return_if_fail(!recent_files_menu);

    recent_files_menu = menu;
    g_object_add_weak_pointer(G_OBJECT(menu), (gpointer*)&recent_files_menu);
}

static void
gwy_app_file_open_recent_cb(GObject *item)
{
    const gchar *filename_utf8;  /* in UTF-8 */

    filename_utf8 = g_object_get_data(G_OBJECT(item), "filename");
    g_return_if_fail(filename_utf8);
    gwy_app_file_load(filename_utf8, NULL, NULL);
}

/**
 * gwy_app_sensitivity_get_group:
 *
 * Gets the application-wide widget sensitvity group.
 *
 * The flags to be used with this sensitvity group are defined in
 * #GwyMenuSensFlags.
 *
 * Returns: The global sensitvity group instead.  No reference is added, you
 *          can add yours, but the returned object will exist to the end of
 *          program anyway.
 **/
GwySensitivityGroup*
gwy_app_sensitivity_get_group(void)
{
    /* This reference is never released. */
    if (!app_sensgroup)
        app_sensgroup = gwy_sensitivity_group_new();

    return app_sensgroup;
}

/**
 * gwy_app_sensitivity_add_widget:
 * @widget: Widget to add.
 * @mask: Which flags the widget is sensitive to.
 *
 * Adds a widget to the application-wide widget sensitvity group.
 *
 * The semantics of this function is the same as
 * gwy_sensitivity_group_add_widget() (in fact, it's a simple wrapper around
 * it).
 **/
void
gwy_app_sensitivity_add_widget(GtkWidget *widget,
                               GwyMenuSensFlags mask)
{
    /* This reference is never released. */
    if (!app_sensgroup)
        app_sensgroup = gwy_sensitivity_group_new();

    gwy_sensitivity_group_add_widget(app_sensgroup, widget, mask);
}

/**
 * gwy_app_sensitivity_set_state:
 * @affected_mask: Which bits in @state to copy to state.
 * @state: The new state (masked with @affected_mask).
 *
 * Sets the state of application-wide widget sensitvity group.
 *
 * The semantics of this function is the same as
 * gwy_sensitivity_group_set_state() (in fact, it's a simple wrapper around
 * it).
 **/
void
gwy_app_sensitivity_set_state(GwyMenuSensFlags affected_mask,
                              GwyMenuSensFlags state)
{
    /* This reference is never released. */
    if (!app_sensgroup)
        app_sensgroup = gwy_sensitivity_group_new();

    gwy_sensitivity_group_set_state(app_sensgroup, affected_mask, state);
}

/**
 * gwy_app_get_tooltips:
 *
 * Gets the application-wide tooltips instance.
 *
 * Returns: The global tooltips instance.  No reference is added, you can
 *          add yours, but the returned object will exist to the end of program
 *          anyway.
 **/
GtkTooltips*
gwy_app_get_tooltips(void)
{
    if (!app_tooltips) {
        app_tooltips = gtk_tooltips_new();
        /* This reference is never released. */
        g_object_ref(app_tooltips);
        gtk_object_sink(GTK_OBJECT(app_tooltips));
    }

    return app_tooltips;
}

/************************** Documentation ****************************/

/**
 * SECTION:menu
 * @title: menu
 * @short_description: Menu and sensitivity functions
 *
 * Menu and toolbox item sensitivity is updated by main application whenever
 * its state changes.  Possible states that may affect widget sesitivity are
 * defined in #GwyMenuSensFlags.
 **/

/**
 * GwyMenuSensFlags:
 * @GWY_MENU_FLAG_DATA: There's at least a one data window present.
 * @GWY_MENU_FLAG_UNDO: There's something to undo (for current data window).
 * @GWY_MENU_FLAG_REDO: There's something to redo (for current data window).
 * @GWY_MENU_FLAG_GRAPH: There's at least a one graph window present.
 * @GWY_MENU_FLAG_LAST_PROC: There is a last-run data processing function
 *                           to rerun.
 * @GWY_MENU_FLAG_LAST_GRAPH: There is a last-run graph function to rerun.
 * @GWY_MENU_FLAG_DATA_MASK: There is a mask on the data.
 * @GWY_MENU_FLAG_DATA_SHOW: There is a presentation on the data.
 * @GWY_MENU_FLAG_3D: A 3D view is present.
 * @GWY_MENU_FLAG_MASK: All the bits combined.
 *
 * Global application sensitivity flags.
 *
 * They represent various application states that may be preconditions for
 * widgets to become sensitive.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
