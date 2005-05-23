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

#include <stdarg.h>
#include <string.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/datafield.h>
#include "app.h"
#include "menu.h"
#include "undo.h"

enum {
    UNDO_LEVELS = 3
};

typedef struct {
    GQuark key;
    GObject *object;  /* TODO: keep references to the objects */
} GwyAppUndoItem;

typedef struct {
    gulong id;
    guint nitems;
    GwyAppUndoItem *items;
} GwyAppUndoLevel;

static void       gwy_app_undo_reuse_levels   (GwyAppUndoLevel *level,
                                               GList *available);
static void       gwy_app_undo_or_redo        (GwyContainer *data,
                                               GwyAppUndoLevel *level);
static GList*     gwy_list_split              (GList *list,
                                               guint n,
                                               GList **tail);
static void       gwy_app_undo_list_free      (GList *list);
static void       gwy_app_undo_setup_keys     (void);

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
    guint i, n;
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
                         guint n,
                         const gchar **keys)
{
    const char *good_keys[] = {
        "/0/data", "/0/mask", "/0/show", NULL
    };
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO
            | GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA_SHOW,
        GWY_MENU_FLAG_UNDO
    };
    static gulong undo_level_id = 0;
    GwyAppUndoLevel *level;
    GObject *object;
    GList *undo, *redo, *available;
    const gchar **p, *key;
    guint i;

    if (!UNDO_LEVELS)
        return 0;

    g_return_val_if_fail(GWY_IS_CONTAINER(data), 0UL);
    if (!n) {
        g_warning("Nothing to save for undo, no undo level will be created.");
        return 0UL;
    }

    if (!undo_key)
        gwy_app_undo_setup_keys();

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
            /* XXX: datafield special-casing */
            g_return_val_if_fail(GWY_IS_DATA_FIELD(object), 0UL);
        }
    };

    /* create new undo level */
    undo_level_id++;
    gwy_debug("Creating a new undo level #%lu", undo_level_id);
    level = g_new(GwyAppUndoLevel, 1);
    level->nitems = n;
    level->items = g_new0(GwyAppUndoItem, n);
    level->id = undo_level_id;

    /* fill the things to save, but don't create copies yet */
    for (i = 0; i < n; i++) {
        GQuark quark;

        key = keys[i];
        quark = g_quark_from_string(key);
        level->items[i].key = quark;
        if (gwy_container_gis_object(data, quark, &object))
            level->items[i].object = object;
    }

    /* add to the undo queue */
    undo = (GList*)g_object_get_qdata(G_OBJECT(data), undo_key);
    g_assert(!undo || !undo->prev);
    redo = (GList*)g_object_get_qdata(G_OBJECT(data), redo_key);
    g_assert(!redo || !redo->prev);

    /* gather undo/redo levels we are going to free for potential reuse */
    undo = gwy_list_split(undo, UNDO_LEVELS-1, &available);
    available = g_list_concat(available, redo);
    redo = NULL;

    gwy_app_undo_reuse_levels(level, available);

    undo = g_list_prepend(undo, level);
    g_object_set_qdata(G_OBJECT(data), undo_key, undo);
    g_object_set_qdata(G_OBJECT(data), redo_key, redo);

    /* TODO */
    g_object_set_qdata(G_OBJECT(data), modif_key,
        GINT_TO_POINTER(GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(data),
                                                           modif_key)) + 1));

    if (gwy_container_contains_by_name(data, "/0/mask"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_MASK;
    if (gwy_container_contains_by_name(data, "/0/show"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_SHOW;
    gwy_app_toolbox_update_state(&sens_data);

    return level->id;
}

/**
 * gwy_app_undo_reuse_levels:
 * @level: An undo level with objects that have to be either duplicated or
 *         reused from to-be-discarded levels.
 * @available: A list of to-be-discarded levels to eventually reuse.
 *
 * Actually duplicates data in @level, eventually reusing @available.
 **/
static void
gwy_app_undo_reuse_levels(GwyAppUndoLevel *level,
                          GList *available)
{
    GType type;
    GList *l;
    guint i, j;
    GwyAppUndoItem *item, *jtem;
    GwyAppUndoLevel *lvl;
    gboolean found;

    for (i = 0; i < level->nitems; i++) {
        item = level->items + i;
        if (!item->object)
            continue;

        found = FALSE;
        type = G_TYPE_FROM_INSTANCE(item->object);
        /* scan through all available levels and all objects inside */
        for (l = available; l; l = g_list_next(l)) {
            lvl = (GwyAppUndoLevel*)l->data;
            for (j = 0; j < lvl->nitems; j++) {
                jtem = lvl->items + j;
                if (!jtem->object)
                    continue;
                if (G_TYPE_FROM_INSTANCE(jtem->object) == type) {
                    /* we've found a reusable item
                     * FIXME: for datafields, we normally know they are all
                     * all the same size, but otherwise there should be some
                     * real compatibility check */
                    gwy_serializable_clone(item->object, jtem->object);
                    item->object = jtem->object;
                    jtem->object = NULL;
                    found = TRUE;
                    gwy_debug("Item (%lu,%x) reused from (%lu,%x)",
                              level->id, item->key, lvl->id, jtem->key);

                    l = NULL;    /* break from outer cycle */
                    break;
                }
            }
        }
        if (!found) {
            item->object = gwy_serializable_duplicate(item->object);
            gwy_debug("Item (%lu,%x) created as new",
                      level->id, item->key);
        }
    }

    gwy_app_undo_list_free(available);
}


/**
 * gwy_app_undo_undo:
 *
 * Performs undo for the current data window.
 **/
void
gwy_app_undo_undo(void)
{
    gwy_app_undo_undo_window(gwy_app_data_window_get_current());
}

/**
 * gwy_app_undo_undo_window:
 * @data_window: A data window (with undo available).
 *
 * Performs undo for the data window @data_window.
 **/
void
gwy_app_undo_undo_window(GwyDataWindow *data_window)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO
            | GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA_SHOW,
        GWY_MENU_FLAG_REDO
    };
    GwyDataView *data_view;
    GwyAppUndoLevel *level;
    GwyContainer *data;
    GList *undo, *redo, *l;

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
    gwy_app_data_view_update(GWY_DATA_VIEW(data_view));
    if (undo)
        sens_data.set_to |= GWY_MENU_FLAG_UNDO;
    if (gwy_container_contains_by_name(data, "/0/mask"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_MASK;
    if (gwy_container_contains_by_name(data, "/0/show"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_SHOW;
    gwy_app_toolbox_update_state(&sens_data);
}

/**
 * gwy_app_undo_redo_window:
 * @data_window: A data window (with redo available).
 *
 * Performs redo for the data window @data_window.
 **/
void
gwy_app_undo_redo_window(GwyDataWindow *data_window)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO
            | GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA_SHOW,
        GWY_MENU_FLAG_UNDO
    };
    GwyDataView *data_view;
    GwyAppUndoLevel *level;
    GList *undo, *redo, *l;
    GwyContainer *data;

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
    gwy_app_data_view_update(GWY_DATA_VIEW(data_view));
    if (redo)
        sens_data.set_to |= GWY_MENU_FLAG_REDO;
    if (gwy_container_contains_by_name(data, "/0/mask"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_MASK;
    if (gwy_container_contains_by_name(data, "/0/show"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_SHOW;
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
    gwy_app_undo_redo_window(gwy_app_data_window_get_current());
}

static void
gwy_app_undo_or_redo(GwyContainer *data,
                     GwyAppUndoLevel *level)
{
    GObject *dfapp, *df;
    GQuark quark;
    guint i;

    for (i = 0; i < level->nitems; i++) {
        quark = level->items[i].key;
        df = level->items[i].object;
        dfapp = NULL;
        gwy_container_gis_object(data, quark, &dfapp);
        if (df && dfapp) {
            gwy_debug("Changing object <%s>", g_quark_to_string(quark));
            g_object_ref(dfapp);
            gwy_container_set_object(data, quark, df);
            level->items[i].object = dfapp;
            g_object_unref(df);
        }
        else if (df && !dfapp) {
            gwy_debug("Restoring object <%s>", g_quark_to_string(quark));
            gwy_container_set_object(data, quark, df);
            level->items[i].object = NULL;
            g_object_unref(df);
        }
        else if (!df && dfapp) {
            gwy_debug("Deleting object <%s>", g_quark_to_string(quark));
            level->items[i].object = dfapp;
            g_object_ref(level->items[i].object);
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
    guint i;

    if (!list)
        return;

    for (l = g_list_first(list); l; l = g_list_next(l)) {
        level = (GwyAppUndoLevel*)l->data;
        for (i = 0; i < level->nitems; i++) {
            if (level->items[i].object) {
                gwy_debug("Item (%lu,%x) destroyed",
                          level->id, level->items[i].key);
            }
            gwy_object_unref(level->items[i].object);
        }
    }
    g_list_free(list);
}

/**
 * gwy_list_split:
 * @list: A list.
 * @n: Length to split list at.
 * @tail: Pointer to store list tail to.
 *
 * Splits a list at given position.
 *
 * Returns: New list head.
 **/
static GList*
gwy_list_split(GList *list,
               guint n,
               GList **tail)
{
    GList *l;

    g_return_val_if_fail(tail, list);
    *tail = NULL;
    if (!list)
        return NULL;

    list = g_list_first(list);
    l = g_list_nth(list, n);
    if (!l)
        return list;

    l->prev->next = NULL;
    l->prev = NULL;
    *tail = l;

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
gwy_app_undo_setup_keys(void)
{
    undo_key = g_quark_from_static_string("gwy-app-undo");
    redo_key = g_quark_from_static_string("gwy-app-redo");
    modif_key = g_quark_from_static_string("gwy-app-modified");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
