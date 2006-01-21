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

static void       gwy_app_file_open_recent_cb       (GObject *item);
static void       gwy_app_update_last_process_func  (GtkWidget *menu,
                                                     const gchar *name);
static gchar*     fix_recent_file_underscores       (gchar *s);
static GtkWidget* find_repeat_last_item             (GtkWidget *menu,
                                                     const gchar *key);

int gwy_app_n_recent_files = 10;
static GtkWidget           *recent_files_menu = NULL;
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
 * gwy_app_run_process_func_cb:
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
        GWY_RUN_INTERACTIVE, GWY_RUN_IMMEDIATE,
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
 **/
void
gwy_app_run_process_func_in_mode(gchar *name,
                                 GwyRunType run)
{
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GtkWidget *menu;
    GwyContainer *data;

    gwy_debug("`%s'", name);
    if (!(run & gwy_process_func_get_run_types(name)))
        return;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(data_window);
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(data_view);
    g_return_if_fail(data);
    gwy_process_func_run(name, data, run);

    menu = GTK_WIDGET(g_object_get_data(G_OBJECT(gwy_app_main_window_get()),
                                        "<proc>"));
    gwy_app_update_last_process_func(menu, name);
    gwy_app_sensitivity_set_state(GWY_MENU_FLAG_LAST_PROC,
                                  GWY_MENU_FLAG_LAST_PROC);
}

static void
gwy_app_update_last_process_func(GtkWidget *menu,
                                 const gchar *name)
{
    static GtkWidget *repeat_item = NULL;
    static GtkWidget *reshow_item = NULL;
    GwyMenuSensFlags sens;
    GtkWidget *label;
    const gchar *menu_path;
    gsize len;
    gchar *s, *mp;

    g_object_set_data(G_OBJECT(menu), "last-func", (gpointer)name);
    if (!repeat_item)
        repeat_item = find_repeat_last_item(menu, "run-last-item");
    if (!reshow_item)
        reshow_item = find_repeat_last_item(menu, "show-last-item");

    /* FIXME: at least the `_' removal should not be necessary as libgwymodule
     * knows the right path */
    menu_path = gwy_process_func_get_menu_path(name);
    menu_path = strrchr(menu_path, '/');
    g_assert(menu_path);
    menu_path++;
    len = strlen(menu_path);
    if (g_str_has_suffix(menu_path, "..."))
        len -= 3;
    mp = gwy_strkill(g_strndup(menu_path, len), "_");

    sens = (gwy_process_func_get_sensitivity_flags(name)
            | GWY_MENU_FLAG_LAST_PROC);
    if (repeat_item) {
        label = GTK_BIN(repeat_item)->child;
        s = g_strconcat(_("Repeat"), " (", mp, ")", NULL);
        gtk_label_set_text_with_mnemonic(GTK_LABEL(label), s);
        gwy_sensitivity_group_set_widget_mask(app_sensgroup, repeat_item, sens);
        g_free(s);
    }

    if (reshow_item) {
        label = GTK_BIN(reshow_item)->child;
        s = g_strconcat(_("Re-show"), " (", mp, ")", NULL);
        gtk_label_set_text_with_mnemonic(GTK_LABEL(label), s);
        gwy_sensitivity_group_set_widget_mask(app_sensgroup, reshow_item, sens);
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
    GtkWidget *graph_window;

    gwy_debug("`%s'", name);
    graph_window = gwy_app_graph_window_get_current();
    g_return_if_fail(GWY_IS_GRAPH_WINDOW(graph_window));
    g_return_if_fail(GWY_IS_GRAPH(GWY_GRAPH_WINDOW(graph_window)->graph));
    gwy_graph_func_run(name, GWY_GRAPH(GWY_GRAPH_WINDOW(graph_window)->graph));
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
