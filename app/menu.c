/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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
#include <libgwydgets/gwytoolbox.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwymodule/gwymodulebrowser.h>
#include "app.h"
#include "file.h"
#include "menu.h"

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

static void   gwy_app_update_last_process_func (GtkWidget *menu,
                                                const gchar *name);
static void   setup_sensitivity_keys           (void);
static gchar* fix_recent_file_underscores      (gchar *s);

static GQuark sensitive_key = 0;
static GQuark sensitive_state_key = 0;

static GtkWidget *recent_files_menu = NULL;

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
                                     GwyMenuSensData *data)
{
    GObject *obj;
    guint i, j;

    setup_sensitivity_keys();

    obj = G_OBJECT(widget);
    gwy_debug("%s", g_type_name(G_TYPE_FROM_INSTANCE(obj)));
    i = GPOINTER_TO_UINT(g_object_get_qdata(obj, sensitive_key));
    if (i & data->flags) {
        j = GPOINTER_TO_UINT(g_object_get_qdata(obj, sensitive_state_key));
        j = (j & ~data->flags) | (data->set_to & data->flags);
        set_sensitive_state(obj, j);
        gtk_widget_set_sensitive(widget, (j & i) == i);

    }
    if (GTK_IS_ALIGNMENT(widget)
        || GTK_IS_MENU_BAR(widget)
        || GWY_IS_TOOLBOX(widget)) {
        gtk_container_foreach(GTK_CONTAINER(widget),
                              (GtkCallback)gwy_app_menu_set_sensitive_recursive,
                              data);
    }
    else if (GTK_IS_MENU_ITEM(widget)
             && (widget = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget))))
        gtk_container_foreach(GTK_CONTAINER(widget),
                              (GtkCallback)gwy_app_menu_set_sensitive_recursive,
                              data);
}

void
gwy_app_menu_set_flags_recursive(GtkWidget *widget,
                                 GwyMenuSensData *data)
{
    setup_sensitivity_keys();

    if (!GTK_IS_TEAROFF_MENU_ITEM(widget))
        set_sensitive_both(widget, data->flags, data->set_to);

    if (GTK_IS_ALIGNMENT(widget)
        || GTK_IS_MENU_BAR(widget)
        || GWY_IS_TOOLBOX(widget)) {
        gtk_container_foreach(GTK_CONTAINER(widget),
                              (GtkCallback)gwy_app_menu_set_flags_recursive,
                              data);
    }
    else if (GTK_IS_MENU_ITEM(widget)) {
        if ((widget = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget))))
            gtk_container_foreach(GTK_CONTAINER(widget),
                                  (GtkCallback)gwy_app_menu_set_flags_recursive,
                                  data);
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

void
gwy_app_run_process_func_cb(gchar *name)
{
    GwyRunType run_types[] = {
        GWY_RUN_INTERACTIVE, GWY_RUN_MODAL,
        GWY_RUN_NONINTERACTIVE, GWY_RUN_WITH_DEFAULTS,
    };
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_LAST_PROC,
        GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_LAST_PROC
    };
    GwyRunType run;
    GwyDataWindow *data_window;
    GtkWidget *data_view, *menu;
    GwyContainer *data;
    gsize i;

    gwy_debug("`%s'", name);
    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(data_window);
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    g_return_if_fail(data);
    run = gwy_process_func_get_run_types(name);
    for (i = 0; i < G_N_ELEMENTS(run_types); i++) {
        if (run & run_types[i]) {
            gwy_process_func_run(name, data, run_types[i]);
            /* FIXME: the ugliest hack! */
            gwy_app_data_view_update(data_view);
            menu = GTK_WIDGET(g_object_get_data(G_OBJECT(
                                                    gwy_app_main_window_get()),
                                                "<proc>"));
            gwy_app_update_last_process_func(menu, name);
            gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

            return;
        }
    }
    g_critical("Trying to run `%s', but no run mode found (%d)", name, run);
}

static void
gwy_app_update_last_process_func(GtkWidget *menu,
                                 const gchar *name)
{
    static GtkWidget *label = NULL;
    GtkWidget *item;
    const gchar *menu_path;
    gsize len, i, j;
    gchar *s, *mp;
    GList *l;

    g_object_set_data(G_OBJECT(menu), "last-func", (gpointer)name);
    /* Find the "run-last-item" menu item
     * FIXME: this is very fragile */
    if (!label) {
        while (GTK_IS_BIN(menu))
            menu = GTK_BIN(menu)->child;
        item = GTK_WIDGET(GTK_MENU_SHELL(menu)->children->data);
        menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
        for (l = GTK_MENU_SHELL(menu)->children; l; l = g_list_next(l)) {
            if (g_object_get_data(G_OBJECT(l->data), "run-last-item"))
                break;
        }
        if (!l) {
            g_warning("Cannot find `Last Used' menu item");
            return;
        }
        item = GTK_WIDGET(l->data);
        label = GTK_BIN(item)->child;
    }

    menu_path = gwy_process_func_get_menu_path(name);
    menu_path = strrchr(menu_path, '/');
    g_assert(menu_path);
    menu_path++;
    len = strlen(menu_path);
    if (g_str_has_suffix(menu_path, "..."))
        len -= 3;
    mp = g_new(gchar, len+1);
    for (i = j = 0; i < len; i++) {
        if (menu_path[i] != '_') {
            mp[j++] = menu_path[i];
        }
    }
    mp[j] = '\0';
    s = g_strconcat(_("_Last Used"), " (", mp, ")", NULL);
    gtk_label_set_text_with_mnemonic(GTK_LABEL(label), s);
    g_free(mp);
    g_free(s);
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

void
gwy_app_menu_recent_files_update(GList *recent_files)
{
    GtkWidget *item;
    GQuark quark;
    GList *l, *child;
    gchar *s, *label, *filename;
    gint i;

    g_return_if_fail(GTK_IS_MENU(recent_files_menu));
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
}

static gchar*
fix_recent_file_underscores(gchar *s)
{
    gsize i, j;
    gchar *s2;

    for (i = j = 0; s[i]; i++, j++) {
        if (s[i] == '_')
            j++;
    }
    s2 = g_new(gchar, j + 1);
    for (i = j = 0; s[i]; i++, j++) {
        if (s[i] == '_')
            s2[j++] = '_';
        s2[j] = s[i];
    }
    s2[j] = '\0';

    g_free(s);
    return s2;
}

void
gwy_app_menu_set_recent_files_menu(GtkWidget *menu)
{
    g_return_if_fail(GTK_IS_MENU(menu));
    g_return_if_fail(!recent_files_menu);

    recent_files_menu = menu;
}

/**
 * gwy_app_toolbox_update_state:
 * @sens_data: Menu sensitivity data.
 *
 * Updates menus and toolbox sensititivity to reflect @sens_data.
 **/
void
gwy_app_toolbox_update_state(GwyMenuSensData *sens_data)
{
    GSList *l;
    GObject *obj;

    gwy_debug("{%d, %d}", sens_data->flags, sens_data->set_to);

    /* FIXME: this actually belongs to toolbox.c; however
    * gwy_app_toolbox_update_state() is called from gwy_app_data_view_update()
    * so libgwyapp would depend on gwyddion instead the other way around */
    obj = G_OBJECT(gwy_app_main_window_get());

    for (l = g_object_get_data(obj, "menus"); l; l = g_slist_next(l))
        gtk_container_foreach(GTK_CONTAINER(l->data),
                              (GtkCallback)gwy_app_menu_set_sensitive_recursive,
                              sens_data);

    for (l = g_object_get_data(obj, "toolbars"); l; l = g_slist_next(l))
        gwy_app_menu_set_sensitive_recursive(GTK_WIDGET(l->data),
                                             sens_data);
}

/***** Documentation *******************************************************/

/**
 * GwyMenuSensitivityFlags:
 * @GWY_MENU_FLAG_DATA: There's at least a one data window present.
 * @GWY_MENU_FLAG_UNDO: There's something to undo (for current data window).
 * @GWY_MENU_FLAG_REDO: There's something to redo (for current data window).
 * @GWY_MENU_FLAG_GRAPH: There's at least a one graph window present.
 * @GWY_MENU_FLAG_LAST_PROC: There is a last-run data processing function
 *                           to rerun.
 * @GWY_MENU_FLAG_LAST_GRAPH: There is a last-run graph function to rerun.
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
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
