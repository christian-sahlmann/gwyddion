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

#include <string.h>
#include <glib.h>
#include <gtk/gtkmarshal.h>

#include <libgwyddion/gwymacros.h>
#include "gwycontainer.h"
#include "gwyserializable.h"
#include "gwywatchable.h"

#define GWY_CONTAINER_TYPE_NAME "GwyContainer"

typedef struct {
    guchar *buffer;
    gsize size;
} SerializeData;

typedef struct {
    GwyContainer *container;
    const gchar *prefix;
    gsize prefix_length;
    gsize count;
    GHFunc func;
    gpointer user_data;
} PrefixData;

typedef struct {
    gulong wid;
    GwyContainerNotifyFunc callback;
    gpointer user_data;
} WatchData;

typedef struct {
    GwyContainer *container;
    GQuark key;
    gulong hid;
} ObjectWatch;

static void     gwy_container_serializable_init  (gpointer giface);
static void     gwy_container_class_init         (GwyContainerClass *klass);
static void     gwy_container_init               (GwyContainer *container);
static void     value_destroy_func               (gpointer data);
static void     gwy_container_finalize           (GObject *obj);
static gboolean gwy_container_try_set_one        (GwyContainer *container,
                                                  GQuark key,
                                                  GValue *value,
                                                  gboolean do_replace,
                                                  gboolean do_create);
static void     gwy_container_try_setv           (GwyContainer *container,
                                                  gsize nvalues,
                                                  GwyKeyVal *values,
                                                  gboolean do_replace,
                                                  gboolean do_create);
static void     gwy_container_try_set_valist     (GwyContainer *container,
                                                  va_list ap,
                                                  gboolean do_replace,
                                                  gboolean do_create);
static void     gwy_container_set_by_name_valist (GwyContainer *container,
                                                  va_list ap,
                                                  gboolean do_replace,
                                                  gboolean do_create);
static guchar*  gwy_container_serialize          (GObject *object,
                                                  guchar *buffer,
                                                  gsize *size);
static void     hash_serialize_func              (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);
static GObject* gwy_container_deserialize        (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
static gboolean hash_remove_prefix_func          (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);
static void     hash_foreach_func                (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);
static GObject* gwy_container_duplicate          (GObject *object);
static void     hash_duplicate_func              (gpointer hkey,
                                                  gpointer hvalue,
                                                  gpointer hdata);

static void     value_changed                    (GwyContainer *container,
                                                  GQuark key);
static void     remove_object_callback           (GwyContainer *container,
                                                  GValue *value);
static void     objects_remove_object_callback   (gpointer p,
                                                  ObjectWatch *owatch);
static void     setup_object_callback            (GwyContainer *container,
                                                  GQuark key,
                                                  GValue *value);
static void     watchable_value_changed          (GObject *object,
                                                  ObjectWatch *owatch);


GType
gwy_container_get_type(void)
{
    static GType gwy_container_type = 0;

    if (!gwy_container_type) {
        static const GTypeInfo gwy_container_info = {
            sizeof(GwyContainerClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_container_class_init,
            NULL,
            NULL,
            sizeof(GwyContainer),
            0,
            (GInstanceInitFunc)gwy_container_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_container_serializable_init, NULL, 0
        };

        gwy_debug("");
        gwy_container_type = g_type_register_static(G_TYPE_OBJECT,
                                                    GWY_CONTAINER_TYPE_NAME,
                                                    &gwy_container_info,
                                                    0);
        g_type_add_interface_static(gwy_container_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
    }

    return gwy_container_type;
}

static void
gwy_container_serializable_init(gpointer giface)
{
    GwySerializableClass *iface = giface;

    gwy_debug("");
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_SERIALIZABLE);

    /* initialize stuff */
    iface->serialize = gwy_container_serialize;
    iface->deserialize = gwy_container_deserialize;
    iface->duplicate = gwy_container_duplicate;
}

static void
gwy_container_class_init(GwyContainerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    gobject_class->finalize = gwy_container_finalize;
}

static void
gwy_container_init(GwyContainer *container)
{
    gwy_debug("");
    container->values = NULL;
    container->watching = NULL;
    container->objects = NULL;
    container->watch_freeze = 0;
    container->last_wid = 1;
}

static void
gwy_container_finalize(GObject *obj)
{
    GwyContainer *container = (GwyContainer*)obj;

    gwy_debug("");

    /* FIXME: doesn't free memory? */
    g_hash_table_destroy(container->watching);
    g_hash_table_foreach(container->objects,
                         (GHFunc)objects_remove_object_callback,
                         NULL);
    g_hash_table_destroy(container->objects);
    g_hash_table_destroy(container->values);
}

/**
 * gwy_container_new:
 *
 * Creates a new #GwyContainer.
 *
 * Returns: The container, as a #GObject.
 **/
GObject*
gwy_container_new(void)
{
    GwyContainer *container;

    gwy_debug("");
    container = g_object_new(GWY_TYPE_CONTAINER, NULL);

    /* assume GQuarks are good enough hash keys */
    container->values = g_hash_table_new_full(NULL, NULL,
                                              NULL, value_destroy_func);
    container->watching = g_hash_table_new_full(NULL, NULL, NULL, g_free);
    /* the callback removal has to be done separately */
    container->objects = g_hash_table_new_full(NULL, NULL, NULL, g_free);

    return (GObject*)(container);
}

static void
value_destroy_func(gpointer data)
{
    GValue *val = (GValue*)data;

    g_value_unset(val);
    g_free(val);
}

/**
 * gwy_container_value_type_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Gets the type of value in container @c identified by name @n.
 **/

/**
 * gwy_container_value_type:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Returns the type of value in @container identified by @key.
 *
 * Returns: The value type as #GType.
 **/
GType
gwy_container_value_type(GwyContainer *container, GQuark key)
{
    GValue *p;

    g_return_val_if_fail(key, 0);
    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    p = (GValue*)g_hash_table_lookup(container->values,
                                     GUINT_TO_POINTER(key));
    g_return_val_if_fail(p, 0);
    return G_VALUE_TYPE(p);
}

/**
 * gwy_container_contains_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Expands to %TRUE if container @c contains a value identified by name @n.
 **/

/**
 * gwy_container_contains:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Returns %TRUE if @container contains a value identified by @key.
 *
 * Returns: Whether @container contains something identified by @key.
 **/
gboolean
gwy_container_contains(GwyContainer *container, GQuark key)
{
    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    return key
           && g_hash_table_lookup(container->values,
                                  GUINT_TO_POINTER(key)) != NULL;
}

static void
objects_remove_object_callback(gpointer p, ObjectWatch *owatch)
{
    g_signal_handler_disconnect(p, owatch->hid);
}

static void
remove_object_callback(GwyContainer *container, GValue *value)
{
    gpointer p;
    ObjectWatch *owatch;

    p = g_value_peek_pointer(value);
    owatch = (ObjectWatch*)g_hash_table_lookup(container->objects, p);
    g_assert(owatch);
    g_signal_handler_disconnect(p, owatch->hid);
    g_hash_table_remove(container->objects, p);  /* also frees owatch */
}

static void
watchable_value_changed(GObject *object, ObjectWatch *owatch)
{
    g_assert(g_hash_table_lookup(GWY_CONTAINER(owatch->container)->objects,
                                 object));
    value_changed(owatch->container, owatch->key);
}

static void
setup_object_callback(GwyContainer *container, GQuark key, GValue *value)
{
    GObject *obj;
    ObjectWatch *owatch;

    obj = (GObject*)g_value_peek_pointer(value);
    owatch = g_new(ObjectWatch, 1);
    owatch->container = container;
    owatch->key = key;
    owatch->hid = g_signal_connect_data(obj, "value_changed",
                                        G_CALLBACK(watchable_value_changed),
                                        owatch, NULL, 0);
    g_hash_table_insert(container->objects, obj, owatch);
}

/**
 * gwy_container_remove_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Removes a value identified by name @n from container @c.
 *
 * Expands to %TRUE if there was such a value and was removed.
 **/

/**
 * gwy_container_remove:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Removes a value identified by @key from @container.
 *
 * Returns: %TRUE if there was such a value and was removed.
 **/
gboolean
gwy_container_remove(GwyContainer *container, GQuark key)
{
    GValue *value;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    if (!key)
        return FALSE;
    /* TODO: notify */

    value = g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!value)
        return FALSE;

    if (G_VALUE_HOLDS_OBJECT(value))
        remove_object_callback(container, value);

    return g_hash_table_remove(container->values,
                               GUINT_TO_POINTER(key));
}

/**
 * gwy_container_remove_by_prefix:
 * @container: A #GwyContainer.
 * @prefix: A nul-terminated id prefix.
 *
 * Removes a values whose key start with @prefix from container @container.
 *
 * @prefix can be %NULL, all values are then removed.
 *
 * Returns: The number of values removed.
 **/
gsize
gwy_container_remove_by_prefix(GwyContainer *container, const gchar *prefix)
{
    PrefixData pfdata;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);

    pfdata.container = container;
    pfdata.prefix = prefix;
    pfdata.prefix_length = prefix ? strlen(pfdata.prefix) : 0;
    pfdata.count = 0;
    g_hash_table_foreach_remove(container->values, hash_remove_prefix_func,
                                &pfdata);

    return pfdata.count;
}

static gboolean
hash_remove_prefix_func(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    PrefixData *pfdata = (PrefixData*)hdata;
    const gchar *name;

    if (pfdata->prefix
        && (!(name = g_quark_to_string(key))
            || !g_str_has_prefix(name, pfdata->prefix)
            || (name[pfdata->prefix_length] != '\0'
                && name[pfdata->prefix_length] != GWY_CONTAINER_PATHSEP)))
        return FALSE;

    if (G_VALUE_HOLDS_OBJECT(value))
        remove_object_callback(pfdata->container, value);

    pfdata->count++;
    return TRUE;
}

/**
 * gwy_container_foreach:
 * @container: A #GwyContainer.
 * @prefix: A nul-terminated id prefix.
 * @function: The function called on the items.
 * @user_data: The user data passed to @function.
 *
 * Calls @function on each @container item whose identifier starts with
 * @prefix.
 *
 * The function is called @function(#GQuark key, #GValue *value, user_data).
 *
 * An empty @prefix means @function will be called on all @container items
 * with a name.  A %NULL @prefix means @function will be called on all
 * @container, even those identified only by a stray nameless %GQuark.
 *
 * Returns: The number of items @function was called on.
 **/
gsize
gwy_container_foreach(GwyContainer *container,
                      const gchar *prefix,
                      GHFunc function,
                      gpointer user_data)
{
    PrefixData pfdata;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    g_return_val_if_fail(function, 0);

    pfdata.container = container;
    pfdata.prefix = prefix;
    pfdata.prefix_length = prefix ? strlen(pfdata.prefix) : 0;
    pfdata.count = 0;
    pfdata.func = function;
    pfdata.user_data = user_data;
    g_hash_table_foreach(container->values, hash_foreach_func, &pfdata);

    return pfdata.count;
}

static void
hash_foreach_func(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    PrefixData *pfdata = (PrefixData*)hdata;
    const gchar *name;

    if (pfdata->prefix
        && (!(name = g_quark_to_string(key))
            || !g_str_has_prefix(name, pfdata->prefix)
            || (name[pfdata->prefix_length] != '\0'
                && name[pfdata->prefix_length] != GWY_CONTAINER_PATHSEP)))
        return;

    pfdata->func(hkey, value, pfdata->user_data);
    pfdata->count++;
}

/**
 * gwy_container_rename_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @nn: A nul-terminated name (id).
 * @f: Whether to delete existing value at @newkey.
 *
 * Makes a value in container @c identified by name @n to be identified by
 * new name @nn.
 *
 * See gwy_container_rename() for details.
 **/

/**
 * gwy_container_rename:
 * @container: A #GwyContainer.
 * @key: The current key.
 * @newkey: A new key for the value.
 * @force: Whether to delete existing value at @newkey.
 *
 * Makes a value in @container identified by @key to be identified by @newkey.
 *
 * When @force is %TRUE existing value at @newkey is removed from @container.
 * When it's %FALSE, an existing value @newkey inhibits the rename and %FALSE
 * is returned.
 *
 * Returns: Whether the rename succeeded.
 **/
gboolean
gwy_container_rename(GwyContainer *container,
                     GQuark key,
                     GQuark newkey,
                     gboolean force)
{
    GValue *value, *oldvalue;

    g_return_val_if_fail(key, FALSE);
    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    /* TODO: notify */

    value = g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!value)
        return FALSE;

    oldvalue = g_hash_table_lookup(container->values, GUINT_TO_POINTER(newkey));
    if (oldvalue) {
        if (!force)
            return FALSE;
        g_assert(gwy_container_remove(container, newkey));
    }
    gwy_container_set_value(container, newkey, value, NULL);
    g_assert(gwy_container_remove(container, key));

    return TRUE;
}

/**
 * gwy_container_get_value_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Gets the value in container @c identified by name @n.
 **/

/**
 * gwy_container_get_value:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Returns the value in @container identified by @key.
 *
 * Returns: The value as a #GValue.
 **/
GValue
gwy_container_get_value(GwyContainer *container, GQuark key)
{
    GValue value;
    GValue *p;

    memset(&value, 0, sizeof(value));
    g_return_val_if_fail(key, value);
    g_return_val_if_fail(GWY_IS_CONTAINER(container), value);
    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    g_return_val_if_fail(p, value);

    g_assert(G_IS_VALUE(p));
    g_value_init(&value, G_VALUE_TYPE(p));
    g_value_copy(p, &value);

    return value;
}

/**
 * gwy_container_get_boolean_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Gets the boolean in container @c identified by name @n.
 **/

/**
 * gwy_container_get_boolean:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Returns the boolean in @container identified by @key.
 *
 * Returns: The boolean as #gboolean.
 **/
gboolean
gwy_container_get_boolean(GwyContainer *container, GQuark key)
{
    GValue *p;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    g_return_val_if_fail(key, 0);
    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p) {
        g_warning("%s: no value for key %u", GWY_CONTAINER_TYPE_NAME, key);
        return 0;
    }
    if (!G_VALUE_HOLDS_BOOLEAN(p)) {
        g_warning("%s: trying to get %s as boolean (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return 0;
    }
    return g_value_get_boolean(p);
}

/**
 * gwy_container_gis_boolean_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the boolean to update.
 *
 * Get-if-set a boolean from @c.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such boolean in the container.
 **/

/**
 * gwy_container_gis_boolean:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: Pointer to the boolean to update.
 *
 * Get-if-set a boolean from @container.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such boolean in the container.
 **/
gboolean
gwy_container_gis_boolean(GwyContainer *container,
                          GQuark key,
                          gboolean *value)
{
    GValue *p;

    if (!key)
        return FALSE;
    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    g_return_val_if_fail(value, FALSE);

    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p)
        return FALSE;
    if (!G_VALUE_HOLDS_BOOLEAN(p)) {
        g_warning("%s: trying to get %s as boolean (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return FALSE;
    }

    *value = g_value_get_boolean(p);
    return TRUE;
}

/**
 * gwy_container_get_uchar_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Gets the unsigned character in container @c identified by name @n.
 **/

/**
 * gwy_container_get_uchar:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Returns the unsigned character in @container identified by @key.
 *
 * Returns: The character as #guchar.
 **/
guchar
gwy_container_get_uchar(GwyContainer *container, GQuark key)
{
    GValue *p;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    g_return_val_if_fail(key, 0);
    p = (GValue*)g_hash_table_lookup(container->values,
                                     GUINT_TO_POINTER(key));
    if (!p) {
        g_warning("%s: no value for key %u", GWY_CONTAINER_TYPE_NAME, key);
        return 0;
    }
    if (!G_VALUE_HOLDS_UCHAR(p)) {
        g_warning("%s: trying to get %s as uchar (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return 0;
    }
    return g_value_get_uchar(p);
}

/**
 * gwy_container_gis_uchar_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the unsigned char to update.
 *
 * Get-if-set an unsigned char from @c.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such unsigned char in the container.
 **/

/**
 * gwy_container_gis_uchar:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: Pointer to the unsigned char to update.
 *
 * Get-if-set an unsigned char from @container.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such unsigned char in the container.
 **/
gboolean
gwy_container_gis_uchar(GwyContainer *container,
                        GQuark key,
                        guchar *value)
{
    GValue *p;

    if (!key)
        return FALSE;
    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    g_return_val_if_fail(value, FALSE);

    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p)
        return FALSE;
    if (!G_VALUE_HOLDS_UCHAR(p)) {
        g_warning("%s: trying to get %s as uchar (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return FALSE;
    }

    *value = g_value_get_uchar(p);
    return TRUE;
}

/**
 * gwy_container_get_int32_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Gets the 32bit integer in container @c identified by name @n.
 **/

/**
 * gwy_container_get_int32:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Returns the 32bit integer in @container identified by @key.
 *
 * Returns: The integer as #guint32.
 **/
gint32
gwy_container_get_int32(GwyContainer *container, GQuark key)
{
    GValue *p;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    g_return_val_if_fail(key, 0);
    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p) {
        g_warning("%s: no value for key %u", GWY_CONTAINER_TYPE_NAME, key);
        return 0;
    }
    if (!G_VALUE_HOLDS_INT(p)) {
        g_warning("%s: trying to get %s as int32 (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return 0;
    }
    return g_value_get_int(p);
}

/**
 * gwy_container_gis_int32_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the 32bit integer to update.
 *
 * Get-if-set a 32bit integer from @c.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such 32bit integer in the container.
 **/

/**
 * gwy_container_gis_int32:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: Pointer to the 32bit integer to update.
 *
 * Get-if-set a 32bit integer from @container.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such 32bit integer in the container.
 **/
gboolean
gwy_container_gis_int32(GwyContainer *container,
                        GQuark key,
                        gint32 *value)
{
    GValue *p;

    if (!key)
        return FALSE;
    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    g_return_val_if_fail(value, FALSE);

    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p)
        return FALSE;
    if (!G_VALUE_HOLDS_INT(p)) {
        g_warning("%s: trying to get %s as int32 (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return FALSE;
    }

    *value = g_value_get_int(p);
    return TRUE;
}

/**
 * gwy_container_get_enum_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Gets the enum in container @c identified by name @n.
 *
 * Note enums are treated as 32bit integers.
 *
 * Since: 1.1.
 **/

/**
 * gwy_container_get_enum:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Returns the enum in @container identified by @key.
 *
 * Note enums are treated as 32bit integers.
 *
 * Returns: The enum as #gint.
 *
 * Since: 1.1.
 **/
guint
gwy_container_get_enum(GwyContainer *container, GQuark key)
{
    return gwy_container_get_int32(container, key);
}

/**
 * gwy_container_gis_enum_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the enum to update.
 *
 * Get-if-set an enum from @c.
 *
 * Note enums are treated as 32bit integers.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such enum in the container.
 *
 * Since: 1.1.
 **/

/**
 * gwy_container_gis_enum:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: Pointer to the enum to update.
 *
 * Get-if-set an enum from @container.
 *
 * Note enums are treated as 32bit integers.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such enum in the container.
 *
 * Since: 1.1.
 **/
/* FIXME: this is probably wrong.  It's here to localize the problem with
 * enum/int/int32 exchanging in a one place. */
gboolean
gwy_container_gis_enum(GwyContainer *container,
                       GQuark key,
                       guint *value)
{
    gint32 value32;

    if (gwy_container_gis_int32(container, key, &value32)) {
        *value = value32;
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_container_get_int64_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Gets the 64bit integer in container @c identified by name @n.
 **/

/**
 * gwy_container_get_int64:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Returns the 64bit integer in @container identified by @key.
 *
 * Returns: The 64bit integer as #guint64.
 **/
gint64
gwy_container_get_int64(GwyContainer *container, GQuark key)
{
    GValue *p;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    g_return_val_if_fail(key, 0);
    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p) {
        g_warning("%s: no value for key %u", GWY_CONTAINER_TYPE_NAME, key);
        return 0;
    }
    if (!G_VALUE_HOLDS_INT64(p)) {
        g_warning("%s: trying to get %s as int64 (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return 0;
    }
    return g_value_get_int64(p);
}

/**
 * gwy_container_gis_int64_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the 64bit integer to update.
 *
 * Get-if-set a 64bit integer from @c.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such 64bit integer in the container.
 **/

/**
 * gwy_container_gis_int64:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: Pointer to the 64bit integer to update.
 *
 * Get-if-set a 64bit integer from @container.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such 64bit integer in the container.
 **/
gboolean
gwy_container_gis_int64(GwyContainer *container,
                        GQuark key,
                        gint64 *value)
{
    GValue *p;

    if (!key)
        return FALSE;
    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    g_return_val_if_fail(value, FALSE);

    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p)
        return FALSE;
    if (!G_VALUE_HOLDS_INT64(p)) {
        g_warning("%s: trying to get %s as int64 (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return FALSE;
    }

    *value = g_value_get_int64(p);
    return TRUE;
}

/**
 * gwy_container_get_double_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Gets the double in container @c identified by name @n.
 **/

/**
 * gwy_container_get_double:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Returns the double in @container identified by @key.
 *
 * Returns: The double as #gdouble.
 **/
gdouble
gwy_container_get_double(GwyContainer *container, GQuark key)
{
    GValue *p;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    g_return_val_if_fail(key, 0);
    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p) {
        g_warning("%s: no value for key %u", GWY_CONTAINER_TYPE_NAME, key);
        return 0;
    }
    if (!G_VALUE_HOLDS_DOUBLE(p)) {
        g_warning("%s: trying to get %s as double (key %u)",
              GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return 0;
    }
    return g_value_get_double(p);
}

/**
 * gwy_container_gis_double_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the double to update.
 *
 * Get-if-set a double from @c.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such double in the container.
 **/

/**
 * gwy_container_gis_double:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: Pointer to the double to update.
 *
 * Get-if-set a double from @container.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such double in the container.
 **/
gboolean
gwy_container_gis_double(GwyContainer *container,
                        GQuark key,
                        gdouble *value)
{
    GValue *p;

    if (!key)
        return FALSE;
    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    g_return_val_if_fail(value, FALSE);

    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p)
        return FALSE;
    if (!G_VALUE_HOLDS_DOUBLE(p)) {
        g_warning("%s: trying to get %s as double (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return FALSE;
    }

    *value = g_value_get_double(p);
    return TRUE;
}

/**
 * gwy_container_get_string_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Gets the string in container @c identified by name @n.
 *
 * The returned string must be treated as constant and never freed or modified.
 **/

/**
 * gwy_container_get_string:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Returns the string in @container identified by @key.
 *
 * The returned string must be treated as constant and never freed or modified.
 *
 * Returns: The string.
 **/
G_CONST_RETURN guchar*
gwy_container_get_string(GwyContainer *container, GQuark key)
{
    GValue *p;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);
    g_return_val_if_fail(key, NULL);
    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p) {
        g_warning("%s: no value for key %u", GWY_CONTAINER_TYPE_NAME, key);
        return NULL;
    }
    if (!G_VALUE_HOLDS_STRING(p)) {
        g_warning("%s: trying to get %s as string (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return NULL;
    }
    return g_value_get_string(p);
}

/**
 * gwy_container_gis_string_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the string pointer to update.
 *
 * Get-if-set a string from @c.
 *
 * The string eventually stored in @v must be treated as constant and
 * never freed or modified.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such string in the container.
 **/

/**
 * gwy_container_gis_string:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: Pointer to the string pointer to update.
 *
 * Get-if-set a string from @container.
 *
 * The string eventually stored in @value must be treated as constant and
 * never freed or modified.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such string in the container.
 **/
gboolean
gwy_container_gis_string(GwyContainer *container,
                         GQuark key,
                         const guchar **value)
{
    GValue *p;

    if (!key)
        return FALSE;
    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    g_return_val_if_fail(value, FALSE);

    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p)
        return FALSE;
    if (!G_VALUE_HOLDS_STRING(p)) {
        g_warning("%s: trying to get %s as string (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return FALSE;
    }

    *value = g_value_get_string(p);
    return TRUE;
}

/**
 * gwy_container_get_object_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 *
 * Gets the object in container @c identified by name @n.
 *
 * The returned object doesn't have its reference count increased, use
 * g_object_ref() if you want to access it even when @container may cease
 * to exist.
 **/

/**
 * gwy_container_get_object:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 *
 * Returns the object in @container identified by @key.
 *
 * The returned object doesn't have its reference count increased, use
 * g_object_ref() if you want to access it even when @container may cease
 * to exist.
 *
 * Returns: The object as #GObject.
 **/
GObject*
gwy_container_get_object(GwyContainer *container, GQuark key)
{
    GValue *p;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), NULL);
    g_return_val_if_fail(key, NULL);
    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p) {
        g_warning("%s: no value for key %u", GWY_CONTAINER_TYPE_NAME, key);
        return NULL;
    }
    if (!G_VALUE_HOLDS_OBJECT(p)) {
        g_warning("%s: trying to get %s as object (key %u)",
              GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return NULL;
    }
    return g_value_get_object(p);
}

/**
 * gwy_container_gis_object_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: Pointer to the object pointer to update.
 *
 * Get-if-set an object from @c.
 *
 * The object eventually stored in @value doesn't have its reference count
 * increased, use g_object_ref() if you want to access it even when
 * @container may cease to exist.
 *
 * Expands to %TRUE if @value was actually updated, %FALSE when there is no
 * such object in the container.
 **/

/**
 * gwy_container_gis_object:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: Pointer to the object pointer to update.
 *
 * Get-if-set an object from @container.
 *
 * The object eventually stored in @value doesn't have its reference count
 * increased, use g_object_ref() if you want to access it even when
 * @container may cease to exist.
 *
 * Returns: %TRUE if @v was actually updated, %FALSE when there is no
 *          such object in the container.
 **/
gboolean
gwy_container_gis_object(GwyContainer *container,
                         GQuark key,
                         GObject **value)
{
    GValue *p;

    if (!key)
        return FALSE;
    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    g_return_val_if_fail(value, FALSE);

    p = (GValue*)g_hash_table_lookup(container->values, GUINT_TO_POINTER(key));
    if (!p)
        return FALSE;
    if (!G_VALUE_HOLDS_OBJECT(p)) {
        g_warning("%s: trying to get %s as object (key %u)",
                  GWY_CONTAINER_TYPE_NAME, G_VALUE_TYPE_NAME(p), key);
        return FALSE;
    }

    *value = g_value_get_object(p);
    return TRUE;
}

static gboolean
gwy_container_try_set_one(GwyContainer *container,
                          GQuark key,
                          GValue *value,
                          gboolean do_replace,
                          gboolean do_create)
{
    GValue *old;

    g_return_val_if_fail(GWY_IS_CONTAINER(container), FALSE);
    g_return_val_if_fail(key, FALSE);
    g_return_val_if_fail(G_IS_VALUE(value), FALSE);

    /* Allow only some sane types to be stored, at least for now */
    if (G_VALUE_HOLDS_OBJECT(value)) {
        GObject *obj = g_value_peek_pointer(value);

        g_return_val_if_fail(GWY_IS_SERIALIZABLE(obj)
                             && GWY_IS_WATCHABLE(obj),
                             FALSE);
    }
    else {
        GType type = G_VALUE_TYPE(value);

        g_return_val_if_fail(G_TYPE_FUNDAMENTAL(type)
                             && type != G_TYPE_BOXED
                             && type != G_TYPE_POINTER
                             && type != G_TYPE_PARAM,
                             FALSE);
    }

    old = (GValue*)g_hash_table_lookup(container->values, GINT_TO_POINTER(key));
    if (old) {
        if (!do_replace)
            return FALSE;
        g_assert(G_IS_VALUE(old));
        if (G_VALUE_HOLDS_OBJECT(old))
            remove_object_callback(container, old);
        g_value_unset(old);
    }
    else {
        if (!do_create)
            return FALSE;
        old = g_new0(GValue, 1);
        g_hash_table_insert(container->values, GINT_TO_POINTER(key), old);
    }
    g_value_init(old, G_VALUE_TYPE(value));
    if (G_VALUE_HOLDS_STRING(value))
        g_value_set_string_take_ownership(old, g_value_peek_pointer(value));
    else
        g_value_copy(value, old);

    /* set up a watch for "value_changed" for objects */
    if (G_VALUE_HOLDS_OBJECT(value))
        setup_object_callback(container, key, value);

    value_changed(container, key);

    return TRUE;
}

static void
gwy_container_try_setv(GwyContainer *container,
                       gsize nvalues,
                       GwyKeyVal *values,
                       gboolean do_replace,
                       gboolean do_create)
{
    gsize i;

    for (i = 0; i < nvalues; i++)
        values[i].changed = gwy_container_try_set_one(container,
                                                      values[i].key,
                                                      values[i].value,
                                                      do_replace,
                                                      do_create);
}

static void
gwy_container_try_set_valist(GwyContainer *container,
                             va_list ap,
                             gboolean do_replace,
                             gboolean do_create)
{
    GwyKeyVal *values;
    gsize n, i;
    GQuark key;

    n = 16;
    values = g_new(GwyKeyVal, n);
    i = 0;
    key = va_arg(ap, GQuark);
    while (key) {
        if (i == n) {
            n += 16;
            values = g_renew(GwyKeyVal, values, n);
        }
        values[i].value = va_arg(ap, GValue*);
        values[i].key = key;
        values[i].changed = FALSE;
        i++;
        key = va_arg(ap, GQuark);
    }
    gwy_container_try_setv(container, i, values, do_replace, do_create);
    g_free(values);
}

/**
 * gwy_container_set_value:
 * @container: A #GwyContainer.
 * @...: A %NULL-terminated list of #GQuark keys and #GValue values.
 *
 * Inserts or updates several values in @container.
 **/
void
gwy_container_set_value(GwyContainer *container,
                        ...)
{
    va_list ap;

    va_start(ap, container);
    gwy_container_try_set_valist(container, ap, TRUE, TRUE);
    va_end(ap);
}

static void
gwy_container_set_by_name_valist(GwyContainer *container,
                                 va_list ap,
                                 gboolean do_replace,
                                 gboolean do_create)
{
    GwyKeyVal *values;
    gsize n, i;
    GQuark key;
    guchar *name;

    n = 16;
    values = g_new(GwyKeyVal, n);
    i = 0;
    name = va_arg(ap, guchar*);
    while (name) {
        key = g_quark_from_string(name);
        if (i == n) {
            n += 16;
            values = g_renew(GwyKeyVal, values, n);
        }
        values[i].value = va_arg(ap, GValue*);
        values[i].key = key;
        values[i].changed = FALSE;
        i++;
        name = va_arg(ap, guchar*);
    }
    gwy_container_try_setv(container, i, values, do_replace, do_create);
    g_free(values);
}

/**
 * gwy_container_set_value_by_name:
 * @container: A #GwyContainer.
 * @...: A %NULL-terminated list of string keys and #GValue values.
 *
 * Inserts or updates several values in @container.
 **/
void
gwy_container_set_value_by_name(GwyContainer *container,
                                ...)
{
    va_list ap;

    va_start(ap, container);
    gwy_container_set_by_name_valist(container, ap, TRUE, TRUE);
    va_end(ap);
}

/**
 * gwy_container_set_boolean_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: A boolean.
 *
 * Stores a boolean into container @c, identified by name @n.
 **/

/**
 * gwy_container_set_boolean:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: A boolean.
 *
 * Stores a boolean into @container, identified by @key.
 **/
void
gwy_container_set_boolean(GwyContainer *container,
                          GQuark key,
                          gboolean value)
{
    GValue gvalue;

    memset(&gvalue, 0, sizeof(GValue));
    g_value_init(&gvalue, G_TYPE_BOOLEAN);
    g_value_set_boolean(&gvalue, value);
    gwy_container_try_set_one(container, key, &gvalue, TRUE, TRUE);
}

/**
 * gwy_container_set_uchar_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: An unsigned character.
 *
 * Stores an unsigned character into container @c, identified by name @n.
 **/

/**
 * gwy_container_set_uchar:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: An unsigned character.
 *
 * Stores an unsigned character into @container, identified by @key.
 **/
void
gwy_container_set_uchar(GwyContainer *container,
                        GQuark key,
                        guchar value)
{
    GValue gvalue;

    memset(&gvalue, 0, sizeof(GValue));
    g_value_init(&gvalue, G_TYPE_UCHAR);
    g_value_set_uchar(&gvalue, value);
    gwy_container_try_set_one(container, key, &gvalue, TRUE, TRUE);
}

/**
 * gwy_container_set_int32_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: A 32bit integer.
 *
 * Stores a 32bit integer into container @c, identified by name @n.
 **/

/**
 * gwy_container_set_int32:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: A 32bit integer.
 *
 * Stores a 32bit integer into @container, identified by @key.
 **/
void
gwy_container_set_int32(GwyContainer *container,
                        GQuark key,
                        gint32 value)
{
    GValue gvalue;

    memset(&gvalue, 0, sizeof(GValue));
    g_value_init(&gvalue, G_TYPE_INT);
    g_value_set_int(&gvalue, value);
    gwy_container_try_set_one(container, key, &gvalue, TRUE, TRUE);
}

/**
 * gwy_container_set_enum_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: An enum.
 *
 * Stores an enum into container @c, identified by name @n.
 *
 * Note enums are treated as 32bit integers.
 *
 * Since: 1.1.
 **/

/**
 * gwy_container_set_enum:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: An enum integer.
 *
 * Stores an enum into @container, identified by @key.
 *
 * Note enums are treated as 32bit integers.
 *
 * Since: 1.1.
 **/
void
gwy_container_set_enum(GwyContainer *container,
                       GQuark key,
                       guint value)
{
    gint32 value32 = value;

    gwy_container_set_int32(container, key, value32);
}

/**
 * gwy_container_set_int64_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: A 64bit integer.
 *
 * Stores a 64bit integer into container @c, identified by name @n.
 **/

/**
 * gwy_container_set_int64:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: A 64bit integer.
 *
 * Stores a 64bit integer into @container, identified by @key.
 **/
void
gwy_container_set_int64(GwyContainer *container,
                        GQuark key,
                        gint64 value)
{
    GValue gvalue;

    memset(&gvalue, 0, sizeof(GValue));
    g_value_init(&gvalue, G_TYPE_INT64);
    g_value_set_int64(&gvalue, value);
    gwy_container_try_set_one(container, key, &gvalue, TRUE, TRUE);
}

/**
 * gwy_container_set_double_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: A double integer.
 *
 * Stores a double into container @c, identified by name @n.
 **/

/**
 * gwy_container_set_double:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: A double.
 *
 * Stores a double into @container, identified by @key.
 **/
void
gwy_container_set_double(GwyContainer *container,
                         GQuark key,
                         gdouble value)
{
    GValue gvalue;

    memset(&gvalue, 0, sizeof(GValue));
    g_value_init(&gvalue, G_TYPE_DOUBLE);
    g_value_set_double(&gvalue, value);
    gwy_container_try_set_one(container, key, &gvalue, TRUE, TRUE);
}

/**
 * gwy_container_set_string_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: A nul-terminated string.
 *
 * Stores a string into container @c, identified by name @n.
 *
 * The container takes ownership of the string, so it can't be used on
 * static strings, use g_strdup() to duplicate them first.
 **/

/**
 * gwy_container_set_string:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: A nul-terminated string.
 *
 * Stores a string into @container, identified by @key.
 *
 * The container takes ownership of the string, so it can't be used on
 * static strings, use g_strdup() to duplicate them first.
 **/
void
gwy_container_set_string(GwyContainer *container,
                         GQuark key,
                         const guchar *value)
{
    GValue gvalue;

    memset(&gvalue, 0, sizeof(GValue));
    g_value_init(&gvalue, G_TYPE_STRING);
    g_value_set_string_take_ownership(&gvalue, (gchar*)value);
    gwy_container_try_set_one(container, key, &gvalue, TRUE, TRUE);
}

/**
 * gwy_container_set_object_by_name:
 * @c: A #GwyContainer.
 * @n: A nul-terminated name (id).
 * @v: An object as #GObject.
 *
 * Stores an object into container @c, identified by name @n.
 *
 * See gwy_container_set_object() for details.
 **/

/**
 * gwy_container_set_object:
 * @container: A #GwyContainer.
 * @key: A #GQuark key.
 * @value: An object as #GObject.
 *
 * Stores an object into @container, identified by @key.
 *
 * The container claims ownership on the object, i.e. its reference count is
 * incremented.
 *
 * The object must implement #GwySerializable interface to allow serialization
 * of the container.  It also has to implement #GwyWatchable interface to
 * allow watching of value changes.
 **/
void
gwy_container_set_object(GwyContainer *container,
                         GQuark key,
                         GObject *value)
{
    GValue gvalue;

    memset(&gvalue, 0, sizeof(GValue));
    g_value_init(&gvalue, G_TYPE_OBJECT);
    g_value_set_object(&gvalue, value);  /* this increases refcount too */
    gwy_container_try_set_one(container, key, &gvalue, TRUE, TRUE);
    g_object_unref(value);
}

static guchar*
gwy_container_serialize(GObject *object,
                        guchar *buffer,
                        gsize *size)
{
    GwyContainer *container;
    SerializeData sdata;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_CONTAINER(object), NULL);

    container = GWY_CONTAINER(object);
    buffer = gwy_serialize_pack(buffer, size, "si",
                                GWY_CONTAINER_TYPE_NAME, 0);
    sdata.buffer = buffer;
    sdata.size = *size;
    g_hash_table_foreach(container->values, hash_serialize_func, &sdata);
    gwy_serialize_store_int32(sdata.buffer + *size - sizeof(guint32),
                              sdata.size - *size);
    *size = sdata.size;

    return sdata.buffer;
}

static void
hash_serialize_func(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    SerializeData *sdata = (SerializeData*)hdata;
    GType type = G_VALUE_TYPE(value);

    sdata->buffer = gwy_serialize_pack(sdata->buffer, &sdata->size, "is",
                                       type, g_quark_to_string(key));
    switch (type) {
        case G_TYPE_OBJECT:
        sdata->buffer = gwy_serializable_serialize(g_value_get_object(value),
                                                   sdata->buffer, &sdata->size);
        break;

        case G_TYPE_BOOLEAN:
        sdata->buffer = gwy_serialize_pack(sdata->buffer, &sdata->size, "b",
                                           g_value_get_boolean(value));
        break;

        case G_TYPE_UCHAR:
        sdata->buffer = gwy_serialize_pack(sdata->buffer, &sdata->size, "c",
                                           g_value_get_uchar(value));
        break;

        case G_TYPE_INT:
        sdata->buffer = gwy_serialize_pack(sdata->buffer, &sdata->size, "i",
                                           g_value_get_int(value));
        break;

        case G_TYPE_INT64:
        sdata->buffer = gwy_serialize_pack(sdata->buffer, &sdata->size, "q",
                                           g_value_get_int64(value));
        break;

        case G_TYPE_DOUBLE:
        sdata->buffer = gwy_serialize_pack(sdata->buffer, &sdata->size, "d",
                                           g_value_get_double(value));
        break;

        case G_TYPE_STRING:
        sdata->buffer = gwy_serialize_pack(sdata->buffer, &sdata->size, "s",
                                           g_value_get_string(value));
        break;

        default:
        g_error("Cannot pack GValue holding %s", g_type_name(type));
        break;
    }
}

static GObject*
gwy_container_deserialize(const guchar *buffer,
                          gsize size,
                          gsize *position)
{
    GwyContainer *container;
    gsize mysize, pos;
    const guchar *buf;

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    pos = gwy_serialize_check_string(buffer, size, *position,
                                     GWY_CONTAINER_TYPE_NAME);
    g_return_val_if_fail(pos, NULL);
    *position += pos;
    mysize = gwy_serialize_unpack_int32(buffer, size, position);
    buf = buffer + *position;
    pos = 0;

    container = (GwyContainer*)gwy_container_new();
    while (pos < mysize) {
        GType type;
        guchar *name;
        GQuark key;
        GObject *object;

        type = gwy_serialize_unpack_int32(buf, mysize, &pos);
        name = gwy_serialize_unpack_string(buf, mysize, &pos);
            gwy_debug("deserializing %s => %s", name, g_type_name(type));
            key = g_quark_from_string(name);
        g_free(name);

        switch (type) {
            case G_TYPE_OBJECT:
            object = gwy_serializable_deserialize(buf, mysize, &pos);
            gwy_container_set_object(container, key, object);
            g_object_unref(object);
            break;

            case G_TYPE_BOOLEAN:
            gwy_container_set_boolean(container, key,
                                      gwy_serialize_unpack_boolean(buf, mysize,
                                                                   &pos));
            break;

            case G_TYPE_UCHAR:
            gwy_container_set_uchar(container, key,
                                    gwy_serialize_unpack_char(buf, mysize,
                                                              &pos));
            break;

            case G_TYPE_INT:
            gwy_container_set_int32(container, key,
                                    gwy_serialize_unpack_int32(buf, mysize,
                                                               &pos));
            break;

            case G_TYPE_INT64:
            gwy_container_set_int64(container, key,
                                    gwy_serialize_unpack_int64(buf, mysize,
                                                               &pos));
            break;

            case G_TYPE_DOUBLE:
            gwy_container_set_double(container, key,
                                     gwy_serialize_unpack_double(buf, mysize,
                                                                 &pos));
            break;

            case G_TYPE_STRING:
            gwy_container_set_string(container, key,
                                     gwy_serialize_unpack_string(buf, mysize,
                                                                 &pos));
            break;

            default:
            g_warning("Cannot unpack GValue holding type #%d", (gint)type);
            break;
        }
    }
    *position += mysize;

    return (GObject*)container;
}

static GObject*
gwy_container_duplicate(GObject *object)
{
    GObject *duplicate;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_CONTAINER(object), NULL);

    duplicate = gwy_container_new();
    g_hash_table_foreach(GWY_CONTAINER(object)->values,
                         hash_duplicate_func, duplicate);

    return duplicate;
}

static void
hash_duplicate_func(gpointer hkey, gpointer hvalue, gpointer hdata)
{
    GQuark key = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    GwyContainer *duplicate = (GwyContainer*)hdata;
    GType type = G_VALUE_TYPE(value);
    GObject *object;

    switch (type) {
        case G_TYPE_OBJECT:
        /* objects have to be handled separately since we want a deep copy */
        object = gwy_serializable_duplicate(g_value_get_object(value));
        gwy_container_set_object(duplicate, key, object);
        g_object_unref(object);
        break;

        case G_TYPE_STRING:
        gwy_container_set_string(duplicate, key, g_value_dup_string(value));
        break;

        case G_TYPE_BOOLEAN:
        case G_TYPE_UCHAR:
        case G_TYPE_INT:
        case G_TYPE_INT64:
        case G_TYPE_DOUBLE:
        gwy_container_set_value(duplicate, key, value, NULL);
        break;

        default:
        g_warning("Cannot properly duplicate %s", g_type_name(type));
        gwy_container_set_value(duplicate, key, value, NULL);
        break;
    }
}

/**
 * gwy_container_freeze_watch:
 * @container: A #GwyContainer.
 *
 * Freezes value update notifications for @container.
 *
 * They are collected until gwy_container_thaw_watch() is called (it has to be
 * called the same number of times as gwy_container_freeze_watch()).
 **/
void
gwy_container_freeze_watch(GwyContainer *container)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_CONTAINER(container));
    g_assert(container->watch_freeze >= 0);
    container->watch_freeze++;
}

/**
 * gwy_container_thaw_watch:
 * @container: A #GwyContainer.
 *
 * Reverts the effect of gwy_container_freeze_watch() causing collected
 * value change notifications to be performed.
 *
 * Due to grouping the number of notifications can be lower than the number of
 * actual value changes.  E.g., when you are watching "foo", and both
 * "foo/bar" and "foo/baz" changes in the freezed state, only one notification
 * is performed after thawing @container.
 **/
void
gwy_container_thaw_watch(GwyContainer *container)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_CONTAINER(container));
    g_assert(container->watch_freeze > 0);
    if (container->watch_freeze) {
        container->watch_freeze--;
        if (!container->watch_freeze) {
            /* TODO: do the notifications */
        }
    }
}

gulong
gwy_container_watch(GwyContainer *container,
                    const guchar *path,
                    GwyContainerNotifyFunc callback,
                    gpointer user_data)
{
    GList *callbacks;
    GQuark key;
    WatchData *wdata;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_CONTAINER(container), 0);
    g_return_val_if_fail(path, 0);
    g_return_val_if_fail(callback, 0);

    wdata = g_new(WatchData, 1);
    wdata->wid = container->last_wid++;
    wdata->callback = callback;
    wdata->user_data = user_data;

    key = g_quark_from_string(path);
    /* FIXME: we may need _steal(), if we set up a value freeing function
     * for container->watching */
    callbacks = g_hash_table_lookup(container->watching, GUINT_TO_POINTER(key));
    callbacks = g_list_append(callbacks, wdata);
    g_hash_table_insert(container->watching, GUINT_TO_POINTER(key), callbacks);

    return wdata->wid;
}

static void
value_changed(G_GNUC_UNUSED GwyContainer *container,
              G_GNUC_UNUSED GQuark key)
{
    gwy_debug("[%p] %s", container, g_quark_to_string(key));
}

/************************** Documentation ****************************/

/**
 * GWY_CONTAINER_PATHSEP:
 *
 * Path separator to be used for hierarchical structures in the container.
 **/

/**
 * GWY_CONTAINER_PATHSEP_STR:
 *
 * Path separator to be used for hierarchical structures in the container,
 * as a string.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
