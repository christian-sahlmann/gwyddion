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
 * - the menu sensitivity stuff should be in libgwyapp
 * - last-run function stuff should be in ???
 */

#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwydgets/gwytoolbox.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwymodule/gwymodulebrowser.h>
#include "app.h"
#include "menu.h"
#include "filelist.h"
#include "gwyappinternal.h"

#define set_sensitive(item, flags) \
    g_object_set_qdata(G_OBJECT(item), sensitive_key, \
                       GUINT_TO_POINTER(flags))
#define set_sensitive_state(item, state) \
    g_object_set_qdata(G_OBJECT(item), sensitive_state_key, \
                       GUINT_TO_POINTER(state))
#define set_sensitive_both(item, flags, state) \
    do { \
        set_sensitive(item, flags); \
        set_sensitive_state(item, state); \
    } while (0)

static void       gwy_app_update_last_process_func  (GtkWidget *menu,
                                                     const gchar *name);
static void       setup_sensitivity_keys            (void);
static gchar*     fix_recent_file_underscores       (gchar *s);
static GtkWidget* find_repeat_last_item             (GtkWidget *menu,
                                                     const gchar *key);

static GQuark sensitive_key = 0;
static GQuark sensitive_state_key = 0;

int gwy_app_n_recent_files = 10;
static GtkWidget *recent_files_menu = NULL;

/* FIXME: how can MSVC can get to needing this when we are not DEBUGging? */
#if (defined(DEBUG) || defined(_MSC_VER))
static gchar*
debug_menu_sens_flags(guint flags)
{
    static const GwyEnum menu_enum[] = {
        { "Data", GWY_MENU_FLAG_DATA },
        { "Undo", GWY_MENU_FLAG_UNDO },
        { "Redo", GWY_MENU_FLAG_REDO },
        { "Graph", GWY_MENU_FLAG_GRAPH },
        { "Last", GWY_MENU_FLAG_LAST_PROC },
        { "LastG", GWY_MENU_FLAG_LAST_GRAPH },
        { "Mask", GWY_MENU_FLAG_DATA_MASK },
        { "Show", GWY_MENU_FLAG_DATA_SHOW },
    };

    /* this is going to leak some memory, but no one cares in debugging mode */
    return gwy_flags_to_string(flags, menu_enum, G_N_ELEMENTS(menu_enum), NULL);
}
#endif

/**
 * gwy_app_menu_set_sensitive_array:
 * @item_factory: A item factory to obtain menu items from.
 * @root: Menu root, without "<" and ">".
 * @items: %NULL-terminated array of item paths in the menu (without the
 *         root).
 * @flags: Sensitivity bits describing when the item should be sensitive.
 *
 * Sets sensitivity flags for a list of menu items.
 **/
void
gwy_app_menu_set_sensitive_array(GtkItemFactory *item_factory,
                                 const gchar *root,
                                 const gchar **items,
                                 GwyMenuSensFlags flags)
{
    GtkWidget *item;
    gsize i, len, maxlen;
    gchar *path;

    setup_sensitivity_keys();

    g_return_if_fail(GTK_IS_ITEM_FACTORY(item_factory));
    g_return_if_fail(root);
    g_return_if_fail(items);

    maxlen = 0;
    for (i = 0; items[i]; i++) {
        len = strlen(items[i]);
        if (len > maxlen)
            maxlen = len;
    }

    len = strlen(root);
    path = g_new(gchar, maxlen + len + 3);
    strcpy(path + 1, root);
    path[0] = '<';
    path[len+1] = '>';
    for (i = 0; items[i]; i++) {
        strcpy(path + len + 2, items[i]);
        item = gtk_item_factory_get_item(item_factory, path);
        set_sensitive(item, flags);
    }
    g_free(path);
}

/**
 * gwy_app_menu_set_sensitive_recursive:
 * @widget: A menu widget (a menu bar, menu, or an item).
 * @data: Sensitivity data.
 *
 * Sets sensitivity bits and current state of a menu subtree at @widget
 * according @data.
 **/
void
gwy_app_menu_set_sensitive_recursive(GtkWidget *widget,
                                     const GwyMenuSensData *data)
{
    GObject *obj;
    guint i, j;

    setup_sensitivity_keys();

    obj = G_OBJECT(widget);
    /*gwy_debug("%s", g_type_name(G_TYPE_FROM_INSTANCE(obj)));*/
    i = GPOINTER_TO_UINT(g_object_get_qdata(obj, sensitive_key));
    /* if there are any relevant flags */
    if (i & data->flags) {
        j = GPOINTER_TO_UINT(g_object_get_qdata(obj, sensitive_state_key));
        /* clear all data->flags bits in state
         * and set all data->set_to bits in state */
        j = (j & ~data->flags) | (data->set_to & data->flags);
        set_sensitive_state(obj, j);
        /* make widget sensitive if all conditions are met */
        gtk_widget_set_sensitive(widget, (j & i) == i);

    }
    if (GTK_IS_ALIGNMENT(widget)
        || GTK_IS_MENU(widget)
        || GTK_IS_VBOX(widget)
        || GTK_IS_MENU_BAR(widget)
        || GWY_IS_TOOLBOX(widget)) {
        gtk_container_foreach(GTK_CONTAINER(widget),
                              (GtkCallback)gwy_app_menu_set_sensitive_recursive,
                              (gpointer)data);
    }
    else if (GTK_IS_MENU_ITEM(widget)
             && (widget = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget))))
        gtk_container_foreach(GTK_CONTAINER(widget),
                              (GtkCallback)gwy_app_menu_set_sensitive_recursive,
                              (gpointer)data);
}

/* Changed in rev 1.52: now uses bitwise OR with existing flags.  There is no
 * way to clear existing flags. */
/**
 * gwy_app_menu_set_flags_recursive:
 * @widget: A menu widget (a menu bar, menu, or an item).
 * @data: Sensitivity data.
 *
 * Adds item sensitivity data @data to a menu subtree @widget.
 *
 * Adding means bitwise OR with existing flags, so existing flags are kept.
 **/
void
gwy_app_menu_set_flags_recursive(GtkWidget *widget,
                                 const GwyMenuSensData *data)
{
    setup_sensitivity_keys();

    if (!GTK_IS_TEAROFF_MENU_ITEM(widget)) {
        GwyMenuSensFlags flags, state;
        GObject *obj;

        obj = G_OBJECT(widget);
        flags = GPOINTER_TO_UINT(g_object_get_qdata(obj, sensitive_key));
        state = GPOINTER_TO_UINT(g_object_get_qdata(obj, sensitive_state_key));
        set_sensitive_both(widget, data->flags | flags, data->set_to | state);
    }

    if (GTK_IS_ALIGNMENT(widget)
        || GTK_IS_MENU(widget)
        || GTK_IS_VBOX(widget)
        || GTK_IS_MENU_BAR(widget)
        || GWY_IS_TOOLBOX(widget)) {
        gtk_container_foreach(GTK_CONTAINER(widget),
                              (GtkCallback)gwy_app_menu_set_flags_recursive,
                              (gpointer)data);
    }
    else if (GTK_IS_MENU_ITEM(widget)) {
        if ((widget = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget))))
            gtk_container_foreach(GTK_CONTAINER(widget),
                                  (GtkCallback)gwy_app_menu_set_flags_recursive,
                                  (gpointer)data);
    }
}

/**
 * gwy_app_menu_set_sensitive_both:
 * @item: A menu item.
 * @flags: Sensitivity bits describing when the item should be sensitive.
 * @state: Current state bits determining whether it's actually sensitive
 *         or not.
 *
 * Sets both senstitivity data and current state for a menu item.
 **/
void
gwy_app_menu_set_sensitive_both(GtkWidget *item,
                                GwyMenuSensFlags flags,
                                GwyMenuSensFlags state)
{
    set_sensitive_both(item, flags, state);
}


static void
setup_sensitivity_keys(void)
{
    if (!sensitive_key)
        sensitive_key = g_quark_from_static_string("sensitive");
    if (!sensitive_state_key)
        sensitive_state_key = g_quark_from_static_string("sensitive-state");
}

/**
 * gwy_app_run_process_func_in_mode:
 * @name: A data processing function name.
 *
 * Run a data processing function @name in the best possible mode.
 *
 * Interactive modes are considered `better' than noninteractive for this
 * purpose.  Since 1.2 it returns the actually used mode (nonzero), or 0 on
 * failure.
 **/
guint
gwy_app_run_process_func_cb(gchar *name)
{
    GwyRunType run_types[] = {
        GWY_RUN_INTERACTIVE, GWY_RUN_MODAL,
        GWY_RUN_NONINTERACTIVE, GWY_RUN_WITH_DEFAULTS,
    };
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
    g_critical("Trying to run `%s', but no run mode found", name);
    return 0;
}

/**
 * gwy_app_run_process_func_in_mode:
 * @name: A data processing function name.
 * @run: A run mode.
 *
 * Run a data processing function @name in mode @run.
 *
 * Since: 1.2.
 **/
void
gwy_app_run_process_func_in_mode(gchar *name,
                                  GwyRunType run)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_LAST_PROC
            | GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA_SHOW,
        GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_LAST_PROC
    };
    GwyDataWindow *data_window;
    GtkWidget *data_view, *menu;
    GwyContainer *data;
    gboolean ok;

    gwy_debug("`%s'", name);
    if (!(run & gwy_process_func_get_run_types(name)))
        return;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(data_window);
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    g_return_if_fail(data);
    ok = gwy_process_func_run(name, data, run);

    /* update the menus regardless the function returns TRUE or not.
     * functions changing nothing would never appear in the last-used
     * menu and/or it would never become sensitive */
    menu = GTK_WIDGET(g_object_get_data(G_OBJECT(gwy_app_main_window_get()),
                                        "<proc>"));
    gwy_app_update_last_process_func(menu, name);
    /* FIXME: the ugliest hack! */
    if (ok)
        gwy_app_data_view_update(data_view);

    /* re-get current data window, it may have changed */
    data_window = gwy_app_data_window_get_current();
    data = gwy_data_window_get_data(GWY_DATA_WINDOW(data_window));
    if (gwy_container_contains_by_name(data, "/0/mask"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_MASK;
    if (gwy_container_contains_by_name(data, "/0/show"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_SHOW;
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);
}

static void
gwy_app_update_last_process_func(GtkWidget *menu,
                                 const gchar *name)
{
    static GtkWidget *repeat_item = NULL;
    static GtkWidget *reshow_item = NULL;
    GtkWidget *label;
    const gchar *menu_path;
    gsize len;
    guint sens;
    gchar *s, *mp;

    g_object_set_data(G_OBJECT(menu), "last-func", (gpointer)name);
    if (!repeat_item)
        repeat_item = find_repeat_last_item(menu, "run-last-item");
    if (!reshow_item)
        reshow_item = find_repeat_last_item(menu, "show-last-item");

    sens = gwy_process_func_get_sensitivity_flags(name)
           | GWY_MENU_FLAG_DATA;
    menu_path = gwy_process_func_get_menu_path(name);
    menu_path = strrchr(menu_path, '/');
    g_assert(menu_path);
    menu_path++;
    len = strlen(menu_path);
    if (g_str_has_suffix(menu_path, "..."))
        len -= 3;
    mp = gwy_strkill(g_strndup(menu_path, len), "_");

    if (repeat_item) {
        label = GTK_BIN(repeat_item)->child;
        s = g_strconcat(_("Repeat"), " (", mp, ")", NULL);
        gtk_label_set_text_with_mnemonic(GTK_LABEL(label), s);
        set_sensitive(repeat_item, sens);
        g_free(s);
    }

    if (reshow_item) {
        label = GTK_BIN(reshow_item)->child;
        s = g_strconcat(_("Re-show"), " (", mp, ")", NULL);
        gtk_label_set_text_with_mnemonic(GTK_LABEL(label), s);
        set_sensitive(reshow_item, sens);
        g_free(s);
    }
    gwy_debug("Repeat sens: %s", debug_menu_sens_flags(sens));

    g_free(mp);
}

/* Find the "run-last-item" menu item
 * FIXME: this is fragile */
static GtkWidget*
find_repeat_last_item(GtkWidget *menu,
                      const gchar *key)
{
    GQuark quark;
    GList *l;

    quark = g_quark_from_string(key);
    for (l = GTK_MENU_SHELL(menu)->children; l; l = g_list_next(l)) {
        if (g_object_get_qdata(G_OBJECT(l->data), quark))
            break;
    }
    if (!l) {
        g_warning("Cannot find `%s' menu item", key);
        return NULL;
    }

    return GTK_WIDGET(l->data);
}

void
gwy_app_run_graph_func_cb(gchar *name)
{
    GtkWidget *graph_window, *graph;

    gwy_debug("`%s'", name);
    graph_window = gwy_app_graph_window_get_current();
    g_return_if_fail(graph_window);
    graph = GTK_BIN(graph_window)->child;
    g_return_if_fail(GWY_IS_GRAPH(graph));
    gwy_graph_func_run(name, GWY_GRAPH(graph));
    /* FIXME TODO: some equivalent of this:
    gwy_app_data_view_update(data_view);
    */
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

/**
 * gwy_app_toolbox_update_state:
 * @sens_data: Menu sensitivity data.
 *
 * Updates menus and toolbox sensititivity to reflect @sens_data.
 **/
void
gwy_app_toolbox_update_state(const GwyMenuSensData *sens_data)
{
    GSList *l;
    GObject *obj;

    gwy_debug("{%s, %s}",
              debug_menu_sens_flags(sens_data->flags),
              debug_menu_sens_flags(sens_data->set_to));

    /* FIXME: this actually belongs to toolbox.c; however
    * gwy_app_toolbox_update_state() is called from gwy_app_data_view_update()
    * so libgwyapp would depend on gwyddion instead the other way around */
    obj = G_OBJECT(gwy_app_main_window_get());

    for (l = g_object_get_data(obj, "menus"); l; l = g_slist_next(l))
        gtk_container_foreach(GTK_CONTAINER(l->data),
                              (GtkCallback)gwy_app_menu_set_sensitive_recursive,
                              (gpointer)sens_data);

    for (l = g_object_get_data(obj, "toolbars"); l; l = g_slist_next(l))
        gwy_app_menu_set_sensitive_recursive(GTK_WIDGET(l->data),
                                             sens_data);
}

/***** Documentation *******************************************************/

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
 * @GWY_MENU_FLAG_GL_OK: OpenGL is available.
 * @GWY_MENU_FLAG_MASK: All the bits combined.
 *
 * Menu sensitivity flags.
 *
 * They represent various application states that may be preconditions for
 * some menu item (or other widget) to become sensitive.
 **/

/**
 * GwyMenuSensData:
 * @flags: The flags that have to be set for a widget to become sensitive.
 * @set_to: The actually set flags.
 *
 * Sensitivity flags and their current state in one struct.
 *
 * All widget bits have to be set to make it sensitive.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
