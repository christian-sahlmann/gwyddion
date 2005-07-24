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

#include "config.h"
#include <stdarg.h>
#include <string.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/datafield.h>
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

typedef struct {
    gpointer container;
    GList *undo;
    GList *redo;
    gint modif;
} GwyAppUndo;

static void        gwy_app_undo_reuse_levels        (GwyAppUndoLevel *level,
                                                     GList *available);
static void        gwy_app_undo_or_redo             (GwyContainer *data,
                                                     GwyAppUndoLevel *level);
static GList*      gwy_list_split                   (GList *list,
                                                     guint n,
                                                     GList **tail);
static void        gwy_app_undo_container_finalized (gpointer userdata,
                                                     GObject *deceased_data);
static void        gwy_app_undo_list_free           (GList *list);
static gint        gwy_app_undo_compare_data        (gconstpointer a,
                                                     gconstpointer b);
static GwyAppUndo* gwy_app_undo_get_for_data        (GwyContainer *data,
                                                     gboolean do_create);

static GList *container_list = NULL;

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
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        GWY_MENU_FLAG_UNDO
    };
    static gulong undo_level_id = 0;

    GwyAppUndo *appundo;
    GwyAppUndoLevel *level;
    GObject *object;
    GList *available;
    const gchar **p, *key;
    guint i;

    if (!UNDO_LEVELS)
        return 0;

    g_return_val_if_fail(GWY_IS_CONTAINER(data), 0UL);
    if (!n) {
        g_warning("Nothing to save for undo, no undo level will be created.");
        return 0UL;
    }

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
    gwy_debug("Creating a new appundo->undo level #%lu", undo_level_id);
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
    appundo = gwy_app_undo_get_for_data(data, TRUE);

    /* gather undo/redo levels we are going to free for potential reuse */
    appundo->undo = gwy_list_split(appundo->undo, UNDO_LEVELS-1, &available);
    available = g_list_concat(available, appundo->redo);
    appundo->redo = NULL;

    gwy_app_undo_reuse_levels(level, available);
    appundo->undo = g_list_prepend(appundo->undo, level);
    appundo->modif++;    /* TODO */

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
 * gwy_app_undo_undo_container:
 * @data: A data container.
 *
 * Performs undo on a data container.
 *
 * It must have undo available.
 **/
void
gwy_app_undo_undo_container(GwyContainer *data)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        GWY_MENU_FLAG_REDO
    };
    GwyAppUndo *appundo;
    GwyAppUndoLevel *level;
    GList *l;

    appundo = gwy_app_undo_get_for_data(data, FALSE);
    g_return_if_fail(appundo && appundo->undo);

    level = (GwyAppUndoLevel*)appundo->undo->data;
    gwy_debug("Undoing to undo level id #%lu", level->id);
    gwy_app_undo_or_redo(data, level);

    l = appundo->undo;
    appundo->undo = g_list_remove_link(appundo->undo, l);
    appundo->redo = g_list_concat(l, appundo->redo);
    appundo->modif--;    /* TODO */

    if (appundo->undo)
        sens_data.set_to |= GWY_MENU_FLAG_UNDO;
    gwy_app_toolbox_update_state(&sens_data);
}

/**
 * gwy_app_undo_redo_window:
 * @data: A data container.
 *
 * Performs undo on a data container.
 *
 * It must have redo available.
 **/
void
gwy_app_undo_redo_container(GwyContainer *data)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        GWY_MENU_FLAG_UNDO
    };
    GwyAppUndo *appundo;
    GwyAppUndoLevel *level;
    GList *l;

    appundo = gwy_app_undo_get_for_data(data, FALSE);
    g_return_if_fail(appundo && appundo->redo);

    level = (GwyAppUndoLevel*)appundo->redo->data;
    gwy_debug("Redoing to undo level id #%lu", level->id);
    gwy_app_undo_or_redo(data, level);

    l = appundo->redo;
    appundo->redo = g_list_remove_link(appundo->redo, l);
    appundo->undo = g_list_concat(l, appundo->undo);
    appundo->modif++;    /* TODO */

    if (appundo->redo)
        sens_data.set_to |= GWY_MENU_FLAG_REDO;
    gwy_app_toolbox_update_state(&sens_data);
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
            /* Note: we have to use duplicate to destroy object identity
             * (user data, signals, ...) */
            level->items[i].object = gwy_serializable_duplicate(dfapp);
            gwy_container_set_object(data, quark, df);
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
 * gwy_app_undo_container_has_undo:
 * @data: Data container to get undo infomation of.
 *
 * Returns whether there is any undo available for a container.
 *
 * Returns: %TRUE if there is undo, %FALSE otherwise.
 **/
gboolean
gwy_app_undo_container_has_undo(GwyContainer *data)
{
    GwyAppUndo *appundo;

    appundo = gwy_app_undo_get_for_data(data, FALSE);
    return appundo && appundo->undo;
}

/**
 * gwy_app_undo_container_has_redo:
 * @data: Data container to get redo infomation of.
 *
 * Returns whether there is any redo available for a container.
 *
 * Returns: %TRUE if there is redo, %FALSE otherwise.
 **/
gboolean
gwy_app_undo_container_has_redo(GwyContainer *data)
{
    GwyAppUndo *appundo;

    appundo = gwy_app_undo_get_for_data(data, FALSE);
    return appundo && appundo->redo;
}

/**
 * gwy_app_undo_container_get_modified:
 * @data: Data container to get modification infomation of.
 *
 * Tests whether a container was modified.
 *
 * FIXME: it may not work.
 *
 * Returns: %TRUE if container was modified, %FALSE otherwise.
 **/
gint
gwy_app_undo_container_get_modified(GwyContainer *data)
{
    GwyAppUndo *appundo;

    appundo = gwy_app_undo_get_for_data(data, FALSE);
    return appundo ? appundo->modif : 0;
}

/**
 * gwy_app_undo_container_set_unmodified:
 * @data: Data container to set modification infomation of.
 *
 * Marks a data container as umodified (that is, saved).
 **/
void
gwy_app_undo_container_set_unmodified(GwyContainer *data)
{
    GwyAppUndo *appundo;

    appundo = gwy_app_undo_get_for_data(data, FALSE);
    if (appundo)
        appundo->modif = 0;
}

/**
 * gwy_app_undo_clear:
 * @data_window: A data window.
 *
 * Removes all undo and redo information for a data window.
 **/
static void
gwy_app_undo_container_finalized(G_GNUC_UNUSED gpointer userdata,
                                 GObject *deceased_data)
{
    GwyAppUndo *appundo;

    gwy_debug("Freeing undo for Container %p", deceased_data);
    /* must not typecast with GWY_CONTAINER(), it doesn't exist any more */
    appundo = gwy_app_undo_get_for_data((GwyContainer*)deceased_data, FALSE);
    g_return_if_fail(appundo);
    gwy_app_undo_list_free(appundo->redo);
    gwy_app_undo_list_free(appundo->undo);
    g_free(appundo);
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

static gint
gwy_app_undo_compare_data(gconstpointer a,
                          gconstpointer b)
{
    GwyAppUndo *ua = (GwyAppUndo*)a;

    /* sign does not matter, only used for equality test */
    return (guchar*)ua->container - (guchar*)b;
}

static GwyAppUndo*
gwy_app_undo_get_for_data(GwyContainer *data,
                          gboolean do_create)
{
    GwyAppUndo *appundo;
    GList *l;

    l = g_list_find_custom(container_list, data, &gwy_app_undo_compare_data);
    if (!l) {
        if (!do_create)
            return NULL;

        gwy_debug("Creating undo for Container %p", data);
        appundo = g_new0(GwyAppUndo, 1);
        appundo->container = data;
        container_list = g_list_prepend(container_list, appundo);
        g_object_weak_ref(G_OBJECT(data), gwy_app_undo_container_finalized,
                          NULL);

        return appundo;
    }

    /* move container to head */
    if (l != container_list) {
        container_list = g_list_remove_link(container_list, l);
        container_list = g_list_concat(l, container_list);
    }

    return (GwyAppUndo*)l->data;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
