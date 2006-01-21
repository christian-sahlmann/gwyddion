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

/* FIXME: non-object code paths are untested and may leak memory or even
 * crash */

#include "config.h"
#include <stdarg.h>
#include <string.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/datafield.h>
#include <app/menu.h>
#include <app/undo.h>

enum {
    UNDO_LEVELS = 3
};

typedef struct {
    GQuark key;
    GValue value;
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

static void        gwy_app_undo_reuse_levels       (GwyAppUndoLevel *level,
                                                    GList *available);
static void        gwy_app_undo_or_redo            (GwyContainer *data,
                                                    GwyAppUndoLevel *level);
static GList*      gwy_list_split                  (GList *list,
                                                    guint n,
                                                    GList **tail);
static void        gwy_app_undo_container_finalized(gpointer userdata,
                                                    GObject *deceased_data);
static void        gwy_app_undo_list_free          (GList *list);
static gint        gwy_app_undo_compare_data       (gconstpointer a,
                                                    gconstpointer b);
static GwyAppUndo* gwy_undo_get_for_data           (GwyContainer *data,
                                                    gboolean do_create);

static GList *container_list = NULL;

/**
 * gwy_app_undo_checkpoint:
 * @data: A data container.
 * @...: %NULL-terminated list of container item names to save.
 *
 * Create a point in the undo history it is possible to return to.
 *
 * In addition to what gwy_undo_checkpoint() does, this function takes care
 * of updating application controls state.
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

    keys = g_newa(const gchar*, n);
    va_start(ap, data);
    for (i = 0; i < n; i++) {
        keys[i] = va_arg(ap, const gchar*);
    }
    va_end(ap);

    id = gwy_app_undo_checkpointv(data, n, keys);

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
 * In addition to what gwy_undo_checkpointv() does, this function takes care
 * of updating application controls state.
 *
 * Returns: Undo level id.  Not useful (yet).
 **/
gulong
gwy_app_undo_checkpointv(GwyContainer *data,
                         guint n,
                         const gchar **keys)
{
    gulong id;


    id = gwy_undo_checkpointv(data, n, keys);
    if (id)
        gwy_app_sensitivity_set_state(GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
                                      GWY_MENU_FLAG_UNDO);

    return id;
}

/**
 * gwy_app_undo_undo_container:
 * @data: A data container.
 *
 * Performs undo on a data container.
 *
 * It must have undo available.
 *
 * In addition to what gwy_undo_undo_container() does, this function takes care
 * of updating application controls state.
 **/
void
gwy_app_undo_undo_container(GwyContainer *data)
{
    GwyAppUndo *appundo;

    gwy_undo_undo_container(data);
    appundo = gwy_undo_get_for_data(data, FALSE);
    gwy_app_sensitivity_set_state(GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
                                  GWY_MENU_FLAG_REDO
                                  | (appundo->undo ? GWY_MENU_FLAG_UNDO : 0));
}

/**
 * gwy_app_undo_redo_container:
 * @data: A data container.
 *
 * Performs undo on a data container.
 *
 * It must have redo available.
 *
 * In addition to what gwy_undo_redo_container() does, this function takes care
 * of updating application controls state.
 **/
void
gwy_app_undo_redo_container(GwyContainer *data)
{
    GwyAppUndo *appundo;

    gwy_undo_redo_container(data);
    appundo = gwy_undo_get_for_data(data, FALSE);
    gwy_app_sensitivity_set_state(GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
                                  GWY_MENU_FLAG_UNDO
                                  | (appundo->redo ? GWY_MENU_FLAG_REDO : 0));
}

/**
 * gwy_undo_checkpoint:
 * @data: A data container.
 * @...: %NULL-terminated list of container item names to save.
 *
 * Create a point in the undo history it is possible to return to.
 *
 * Returns: Undo level id.  Not useful (yet).
 **/
gulong
gwy_undo_checkpoint(GwyContainer *data,
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

    keys = g_newa(const gchar*, n);
    va_start(ap, data);
    for (i = 0; i < n; i++) {
        keys[i] = va_arg(ap, const gchar*);
    }
    va_end(ap);

    id = gwy_undo_checkpointv(data, n, keys);

    return id;
}

/**
 * gwy_undo_checkpointv:
 * @data: A data container.
 * @n: The number of strings in @keys.
 * @keys: An array of container keys to save data.
 *
 * Create a point in the undo history is is possible to return to.
 *
 * Returns: Undo level id.  Not useful (yet).
 **/
gulong
gwy_undo_checkpointv(GwyContainer *data,
                     guint n,
                     const gchar **keys)
{
    static gulong undo_level_id = 0;

    GwyAppUndo *appundo;
    GwyAppUndoLevel *level;
    GList *available;
    const gchar *key;
    guint i;

    if (!UNDO_LEVELS)
        return 0;

    g_return_val_if_fail(GWY_IS_CONTAINER(data), 0UL);
    if (!n) {
        g_warning("Nothing to save for undo, no undo level will be created.");
        return 0UL;
    }

    /* create new undo level */
    undo_level_id++;
    gwy_debug("Creating a new appundo->undo level #%lu", undo_level_id);
    level = g_new(GwyAppUndoLevel, 1);
    level->nitems = n;
    level->items = g_new0(GwyAppUndoItem, n);
    level->id = undo_level_id;

    /* fill the things to save, but don't duplicate objects yet */
    for (i = 0; i < n; i++) {
        GwyAppUndoItem *item = level->items + i;
        GQuark quark;

        key = keys[i];
        quark = g_quark_from_string(key);
        item->key = quark;
        memset(&item->value, 0, sizeof(GValue));
        /* note this call itself creates a copy for non-objects; for objects
         * it increases reference count */
        gwy_container_gis_value(data, quark, &item->value);
    }

    /* add to the undo queue */
    appundo = gwy_undo_get_for_data(data, TRUE);

    /* gather undo/redo levels we are going to free for potential reuse */
    appundo->undo = gwy_list_split(appundo->undo, UNDO_LEVELS-1, &available);
    available = g_list_concat(available, appundo->redo);
    appundo->redo = NULL;

    gwy_app_undo_reuse_levels(level, available);
    appundo->undo = g_list_prepend(appundo->undo, level);
    appundo->modif++;    /* TODO */

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
    GObject *iobject, *jobject;
    GwyAppUndoLevel *lvl;
    gboolean found;

    for (i = 0; i < level->nitems; i++) {
        item = level->items + i;
        if (!G_VALUE_HOLDS_OBJECT(&item->value))
            continue;

        found = FALSE;
        iobject = g_value_get_object(&item->value);
        type = G_TYPE_FROM_INSTANCE(iobject);
        /* scan through all available levels and all objects inside */
        for (l = available; l; l = g_list_next(l)) {
            lvl = (GwyAppUndoLevel*)l->data;
            for (j = 0; j < lvl->nitems; j++) {
                jtem = lvl->items + j;
                if (!G_VALUE_HOLDS_OBJECT(&jtem->value))
                    continue;
                jobject = g_value_get_object(&jtem->value);
                if (!G_TYPE_FROM_INSTANCE(jobject) == type)
                    continue;

                /* we've found a reusable item
                 * FIXME: for datafields, we normally know they are all
                 * all the same size, but otherwise there should be some
                 * real compatibility check */
                gwy_serializable_clone(iobject, jobject);
                g_value_copy(&jtem->value, &item->value);
                g_value_unset(&jtem->value);
                found = TRUE;
                gwy_debug("Item (%lu,%x) reused from (%lu,%x)",
                          level->id, item->key, lvl->id, jtem->key);

                l = NULL;    /* break from outer cycle */
                break;
            }
        }
        if (!found) {
            if (G_VALUE_HOLDS_OBJECT(&item->value)) {
                iobject = g_value_get_object(&item->value);
                g_value_take_object(&item->value,
                                    gwy_serializable_duplicate(iobject));
            }
            gwy_debug("Item (%lu,%x) created as new",
                      level->id, item->key);
        }
    }

    gwy_app_undo_list_free(available);
}

/**
 * gwy_undo_undo_container:
 * @data: A data container.
 *
 * Performs undo on a data container.
 *
 * It must have undo available.
 **/
void
gwy_undo_undo_container(GwyContainer *data)
{
    GwyAppUndo *appundo;
    GwyAppUndoLevel *level;
    GList *l;

    appundo = gwy_undo_get_for_data(data, FALSE);
    g_return_if_fail(appundo && appundo->undo);

    level = (GwyAppUndoLevel*)appundo->undo->data;
    gwy_debug("Undoing to undo level id #%lu", level->id);
    gwy_app_undo_or_redo(data, level);

    l = appundo->undo;
    appundo->undo = g_list_remove_link(appundo->undo, l);
    appundo->redo = g_list_concat(l, appundo->redo);
    appundo->modif--;    /* TODO */
}

/**
 * gwy_undo_redo_container:
 * @data: A data container.
 *
 * Performs undo on a data container.
 *
 * It must have redo available.
 **/
void
gwy_undo_redo_container(GwyContainer *data)
{
    GwyAppUndo *appundo;
    GwyAppUndoLevel *level;
    GList *l;

    appundo = gwy_undo_get_for_data(data, FALSE);
    g_return_if_fail(appundo && appundo->redo);

    level = (GwyAppUndoLevel*)appundo->redo->data;
    gwy_debug("Redoing to undo level id #%lu", level->id);
    gwy_app_undo_or_redo(data, level);

    l = appundo->redo;
    appundo->redo = g_list_remove_link(appundo->redo, l);
    appundo->undo = g_list_concat(l, appundo->undo);
    appundo->modif++;    /* TODO */
}

static void
gwy_app_undo_or_redo(GwyContainer *data,
                     GwyAppUndoLevel *level)
{
    GValue value;
    gboolean app, undo;
    guint i;

    memset(&value, 0, sizeof(GValue));
    for (i = 0; i < level->nitems; i++) {
        GwyAppUndoItem *item = level->items + i;

        /* note this call itself creates a copy for non-objects; for objects
         * it increases reference count */
        app = gwy_container_gis_value(data, item->key, &value);
        undo = G_VALUE_TYPE(&item->value) != 0;
        if (undo && app) {
            if (G_VALUE_TYPE(&item->value) != G_VALUE_TYPE(&value)) {
                g_critical("Types of undone/redone and current object "
                           "don't match");
                continue;
            }
            gwy_debug("Changing item <%s>", g_quark_to_string(item->key));
            /* Note: we have to use duplicate to destroy object identity
             * (user data, signals, ...) */
            if (G_VALUE_HOLDS_OBJECT(&item->value)) {
                GObject *object;

                object = g_value_get_object(&item->value);
                gwy_debug("saved object: %p", object);
                object = gwy_serializable_duplicate(object);
                gwy_container_set_object(data, item->key, object);
                g_object_unref(object);

                g_value_copy(&value, &item->value);
                g_value_unset(&value);
            }
            else {
                gwy_container_set_value(data, item->key, &item->value, 0);
                g_value_copy(&item->value, &value);
                g_value_unset(&value);
            }
        }
        else if (undo && !app) {
            gwy_debug("Restoring item <%s>", g_quark_to_string(item->key));
            gwy_container_set_value(data, item->key, &item->value, 0);
            g_value_unset(&item->value);
        }
        else if (!undo && app) {
            gwy_debug("Deleting item <%s>", g_quark_to_string(item->key));
            g_value_init(&item->value, G_VALUE_TYPE(&value));
            g_value_copy(&value, &item->value);
            gwy_container_remove(data, item->key);
            g_value_unset(&value);
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
 * gwy_undo_container_has_undo:
 * @data: Data container to get undo infomation of.
 *
 * Returns whether there is any undo available for a container.
 *
 * Returns: %TRUE if there is undo, %FALSE otherwise.
 **/
gboolean
gwy_undo_container_has_undo(GwyContainer *data)
{
    GwyAppUndo *appundo;

    appundo = gwy_undo_get_for_data(data, FALSE);
    return appundo && appundo->undo;
}

/**
 * gwy_undo_container_has_redo:
 * @data: Data container to get redo infomation of.
 *
 * Returns whether there is any redo available for a container.
 *
 * Returns: %TRUE if there is redo, %FALSE otherwise.
 **/
gboolean
gwy_undo_container_has_redo(GwyContainer *data)
{
    GwyAppUndo *appundo;

    appundo = gwy_undo_get_for_data(data, FALSE);
    return appundo && appundo->redo;
}

/**
 * gwy_undo_container_get_modified:
 * @data: Data container to get modification infomation of.
 *
 * Tests whether a container was modified.
 *
 * FIXME: it may not work.
 *
 * Returns: %TRUE if container was modified, %FALSE otherwise.
 **/
gint
gwy_undo_container_get_modified(GwyContainer *data)
{
    GwyAppUndo *appundo;

    appundo = gwy_undo_get_for_data(data, FALSE);
    return appundo ? appundo->modif : 0;
}

/**
 * gwy_undo_container_set_unmodified:
 * @data: Data container to set modification infomation of.
 *
 * Marks a data container as umodified (that is, saved).
 **/
void
gwy_undo_container_set_unmodified(GwyContainer *data)
{
    GwyAppUndo *appundo;

    appundo = gwy_undo_get_for_data(data, FALSE);
    if (appundo)
        appundo->modif = 0;
}

/**
 * gwy_app_undo_container_finalized:
 * @deceased_data: A #GwyContainer pointer (the object may not longer exits).
 *
 * Removes all undo and redo information for a container.
 **/
static void
gwy_app_undo_container_finalized(G_GNUC_UNUSED gpointer userdata,
                                 GObject *deceased_data)
{
    GwyAppUndo *appundo;

    gwy_debug("Freeing undo for Container %p", deceased_data);
    /* must not typecast with GWY_CONTAINER(), it doesn't exist any more */
    appundo = gwy_undo_get_for_data((GwyContainer*)deceased_data, FALSE);
    g_return_if_fail(appundo);
    /* gwy_undo_get_for_data() moves the item to list head */
    g_assert(appundo == container_list->data);
    container_list = g_list_delete_link(container_list, container_list);
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
            GwyAppUndoItem *item = level->items + i;

            if (G_VALUE_TYPE(&item->value)) {
                gwy_debug("Item (%lu,%x) destroyed", level->id, item->key);
                g_value_unset(&item->value);
            }
            /* FIXME: gwy_object_unref(level->items[i].object); */
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
gwy_undo_get_for_data(GwyContainer *data,
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

/************************** Documentation ****************************/

/**
 * SECTION:undo
 * @title: undo
 * @short_description: Undo and redo facility
 * 
 * For a module developer, the only two useful undo/redo functions are probably
 * gwy_app_undo_checkpoint() and gwy_app_undo_checkpointv() creating a
 * checkpoint in the undo history to which is possible to return to later. You
 * should use these functions before commiting changes to data.
 *
 * The other functions actually perform undo or redo, and has little use
 * outside main application, unless it wishes to implement its local undo.
 *
 * There are two types of functions app-level and container-level.  App-level
 * functions (have `app' in their names) should be generally used only by the
 * main application as they update menus and toolboxes according to undo and
 * redo state.  Container-level functions only perform the actual undo or redo
 * and can be used for local undo implementation.
 *
 * Undo information for a #GwyContainer is automatically destroyed when the
 * container is finalized.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
