/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include <stdarg.h>
#include <string.h>
#include <libgwyddion/gwyddion.h>
#include "app.h"
#include "menu.h"
#include "undo.h"

/* this is not used (yet), the GUI doesn't allow more levels */
enum {
    UNDO_LEVELS = 3
};

typedef struct {
    GQuark key;
    GObject *data;  /* TODO: keep references to the objects */
} GwyAppUndoItem;

typedef struct {
    gulong id;
    gint modif;
    gsize nitems;
    GwyAppUndoItem *items;
} GwyAppUndoLevel;

static void       gwy_app_undo_or_redo        (GwyContainer *data,
                                               GwyAppUndoLevel *level);
static GList*     gwy_app_undo_list_trim      (GList *list,
                                               gsize n);
static void       gwy_app_undo_list_free      (GList *list);
static void       setup_keys                  (void);

static GQuark undo_key = 0;
static GQuark redo_key = 0;
static GQuark modif_key = 0;

/**
 * gwy_app_undo_checkpoint:
 * @data: A data container.
 * @...: %NULL-terminated list of container item names to save.
 *
 * Create a point in the undo history it is possible to return to.
 *
 * XXX: It can only save the state of standard datafields.
 *
 * Returns: Undo level id.  Not useful (yet).
 **/
gulong
gwy_app_undo_checkpoint(GwyContainer *data,
                        ...)
{
    va_list ap;
    const gchar **keys;
    gsize i, n;
    gulong id;

    n = 0;
    va_start(ap, data);
    while (TRUE) {
        if (!va_arg(ap, const gchar*))
            break;
        n++;
    };
    va_end(ap);

    keys = g_new(const gchar*, n);
    va_start(ap, data);
    for (i = 0; i < n; i++) {
        keys[i] = va_arg(ap, const gchar*);
    }
    va_end(ap);

    id = gwy_app_undo_checkpointv(data, n, keys);
    g_free(keys);

    return id;
}

/**
 * gwy_app_undo_checkpointv:
 * @data: A data container.
 * @n: The number of strings in @keys.
 * @keys: An array of container keys to save data.
 *
 * Create a point in the undo history is is possible to return to.
 *
 * XXX: It can only save the state of standard datafields.
 *
 * Returns: Undo level id.  Not useful (yet).
 **/
gulong
gwy_app_undo_checkpointv(GwyContainer *data,
                         gsize n,
                         const gchar **keys)
{
    const char *good_keys[] = {
        "/0/data", "/0/mask", "/0/show", NULL
    };
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        GWY_MENU_FLAG_UNDO
    };
    static gulong undo_level_id = 0;
    GwyAppUndoLevel *level;
    GObject *object;
    GList *undo, *redo;
    const gchar **p, *key;
    gsize i;

    g_return_val_if_fail(GWY_IS_CONTAINER(data), 0UL);
    if (!n) {
        g_warning("Nothing to save for undo, no undo level will be created.");
        return 0UL;
    }

    if (!undo_key)
        setup_keys();

    for (i = 0; i < n; i++) {
        key = keys[i];
        for (p = good_keys; *p && strcmp(key, *p); p++)
            ;
        if (!*p) {
            g_warning("FIXME: Undo works only for standard datafields");
            return 0UL;
        }
        if (gwy_container_contains_by_name(data, key)) {
            object = gwy_container_get_object_by_name(data, key);
            /*g_return_if_fail(GWY_IS_DATA_FIELD(object));*/
        }
    };

    /* create new undo level */
    undo_level_id++;
    gwy_debug("Creating a new undo level #%lu", undo_level_id);
    level = g_new(GwyAppUndoLevel, 1);
    level->modif = 0;  /* TODO */
    level->nitems = n;
    level->items = g_new0(GwyAppUndoItem, n);
    level->id = undo_level_id;

    /* fill the things to save */
    for (i = 0; i < n; i++) {
        GQuark quark;

        key = keys[i];
        quark = g_quark_from_string(key);
        level->items[i].key = quark;
        object = NULL;
        if (gwy_container_gis_object(data, quark, &object))
            object = gwy_serializable_duplicate(object);
        level->items[i].data = object;
    }

    /* add to the undo queue */
    undo = (GList*)g_object_get_qdata(G_OBJECT(data), undo_key);
    g_assert(!undo || !undo->prev);
    redo = (GList*)g_object_get_qdata(G_OBJECT(data), redo_key);
    g_assert(!redo || !redo->prev);

    gwy_app_undo_list_free(redo);
    undo = g_list_prepend(undo, level);
    undo = gwy_app_undo_list_trim(undo, UNDO_LEVELS);
    g_object_set_qdata(G_OBJECT(data), undo_key, undo);
    g_object_set_qdata(G_OBJECT(data), redo_key, NULL);

    /* TODO */
    g_object_set_qdata(G_OBJECT(data), modif_key,
        GINT_TO_POINTER(GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(data),
                                                           modif_key)) + 1));
    gwy_app_toolbox_update_state(&sens_data);

    return level->id;
}

/**
 * gwy_app_undo_undo:
 *
 * Performs undo for the current data window.
 **/
void
gwy_app_undo_undo(void)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        GWY_MENU_FLAG_REDO
    };
    GwyDataWindow *data_window;
    GtkWidget *data_view;
    GwyAppUndoLevel *level;
    GwyContainer *data;
    GList *undo, *redo, *l;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));

    undo = (GList*)g_object_get_qdata(G_OBJECT(data), undo_key);
    g_return_if_fail(undo);
    g_assert(!undo->prev);
    redo = (GList*)g_object_get_qdata(G_OBJECT(data), redo_key);
    g_assert(!redo || !redo->prev);

    level = (GwyAppUndoLevel*)undo->data;
    gwy_debug("Undoing to undo level id #%lu", level->id);
    gwy_app_undo_or_redo(data, level);

    l = undo;
    undo = g_list_remove_link(undo, l);
    redo = g_list_concat(l, redo);
    g_object_set_qdata(G_OBJECT(data), undo_key, undo);
    g_object_set_qdata(G_OBJECT(data), redo_key, redo);
    /* TODO */
    g_object_set_qdata(G_OBJECT(data), modif_key,
        GINT_TO_POINTER(GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(data),
                                                           modif_key)) - 1));
    gwy_app_data_view_update(data_view);
    if (undo)
        sens_data.set_to |= GWY_MENU_FLAG_UNDO;
    gwy_app_toolbox_update_state(&sens_data);
}

/**
 * gwy_app_undo_redo:
 *
 * Performs redo for the current data window.
 **/
void
gwy_app_undo_redo(void)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        GWY_MENU_FLAG_UNDO
    };
    GwyDataWindow *data_window;
    GtkWidget *data_view;
    GwyAppUndoLevel *level;
    GList *undo, *redo, *l;
    GwyContainer *data;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));

    redo = (GList*)g_object_get_qdata(G_OBJECT(data), redo_key);
    g_return_if_fail(redo);
    g_assert(!redo->prev);
    undo = (GList*)g_object_get_qdata(G_OBJECT(data), undo_key);
    g_assert(!undo || !undo->prev);

    level = (GwyAppUndoLevel*)redo->data;
    gwy_debug("Redoing to undo level id #%lu", level->id);
    gwy_app_undo_or_redo(data, level);

    l = redo;
    redo = g_list_remove_link(redo, l);
    undo = g_list_concat(l, undo);
    g_object_set_qdata(G_OBJECT(data), undo_key, undo);
    g_object_set_qdata(G_OBJECT(data), redo_key, redo);
    /* TODO */
    g_object_set_qdata(G_OBJECT(data), modif_key,
        GINT_TO_POINTER(GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(data),
                                                           modif_key)) + 1));
    gwy_app_data_view_update(data_view);
    if (redo)
        sens_data.set_to |= GWY_MENU_FLAG_REDO;
    gwy_app_toolbox_update_state(&sens_data);
}

static void
gwy_app_undo_or_redo(GwyContainer *data,
                     GwyAppUndoLevel *level)
{
    GObject *dfapp, *df;
    GQuark quark;
    gsize i;

    for (i = 0; i < level->nitems; i++) {
        quark = level->items[i].key;
        df = level->items[i].data;
        dfapp = NULL;
        gwy_container_gis_object(data, quark, &dfapp);
        if (df && dfapp) {
            dfapp = gwy_container_get_object(data, quark);
            g_object_ref(dfapp);
            gwy_container_set_object(data, quark, df);
            level->items[i].data = dfapp;
            g_object_unref(df);
        }
        else if (df && !dfapp) {
            gwy_container_set_object(data, quark, df);
            level->items[i].data = NULL;
        }
        else if (!df && dfapp) {
            level->items[i].data = gwy_container_get_object(data, quark);
            g_object_ref(level->items[i].data);
            gwy_container_remove(data, quark);
        }
        else
            g_warning("Undoing/redoing NULL to another NULL");
    }
}

static void
gwy_app_undo_list_free(GList *list)
{
    GwyAppUndoLevel *level;
    GList *l;
    gsize i;

    if (!list)
        return;

    for (l = g_list_first(list); l; l = g_list_next(l)) {
        level = (GwyAppUndoLevel*)l->data;
        for (i = 0; i < level->nitems; i++)
            gwy_object_unref(level->items[i].data);
    }
    g_list_free(list);
}

/*
 * Trim undo list to @n levels.
 * Return the new list head.
 **/
static GList*
gwy_app_undo_list_trim(GList *list,
                       gsize n)
{
    GList *l;

    if (!list || !n) {
        gwy_app_undo_list_free(list);
        return NULL;
    }

    list = g_list_first(list);
    l = g_list_nth(list, n);
    if (!l)
        return list;

    l->prev->next = NULL;
    l->prev = NULL;
    gwy_app_undo_list_free(l);

    return list;
}

/**
 * gwy_app_data_window_has_undo:
 * @data_window: A data window.
 *
 * Returns wheter there is any undo available for @data_window.
 *
 * Returns: %TRUE if there is undo, %FALSE otherwise.
 **/
gboolean
gwy_app_data_window_has_undo(GwyDataWindow *data_window)
{
    GObject *data;

    data = G_OBJECT(gwy_data_window_get_data(data_window));
    return g_object_get_qdata(data, undo_key) != NULL;
}

/**
 * gwy_app_data_window_has_redo:
 * @data_window: A data window.
 *
 * Returns wheter there is any redo available for @data_window.
 *
 * Returns: %TRUE if there is redo, %FALSE otherwise.
 **/
gboolean
gwy_app_data_window_has_redo(GwyDataWindow *data_window)
{
    GObject *data;

    data = G_OBJECT(gwy_data_window_get_data(data_window));
    return g_object_get_qdata(data, redo_key) != NULL;
}

/**
 * gwy_app_undo_clear:
 * @data_window: A data window.
 *
 * Removes all undo and redo information for a data window.
 **/
void
gwy_app_undo_clear(GwyDataWindow *data_window)
{
    GObject *data;

    data = G_OBJECT(gwy_data_window_get_data(data_window));
    gwy_app_undo_list_free((GList*)g_object_get_qdata(data, undo_key));
    gwy_app_undo_list_free((GList*)g_object_get_qdata(data, redo_key));
}

static void
setup_keys(void)
{
    undo_key = g_quark_from_static_string("gwy-app-undo");
    redo_key = g_quark_from_static_string("gwy-app-redo");
    modif_key = g_quark_from_static_string("gwy-app-modified");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
