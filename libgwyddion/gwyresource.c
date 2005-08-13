/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwyinventory.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwyresource.h>

#define MAGIC_HEADER "Gwyddion Resource "

enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

static void         gwy_resource_serializable_init(GwySerializableIface *iface);
static void         gwy_resource_finalize         (GObject *object);
static gboolean     gwy_resource_get_is_const     (gconstpointer item);
static const gchar* gwy_resource_get_item_name    (gpointer item);
static gboolean     gwy_resource_compare          (gconstpointer item1,
                                                   gconstpointer item2);
static void         gwy_resource_rename           (gpointer item,
                                                   const gchar *new_name);
static void         gwy_resource_rename           (gpointer item,
                                                   const gchar *new_name);

static guint resource_signals[LAST_SIGNAL] = { 0 };

static const GwyInventoryItemType gwy_resource_item_type = {
    0,
    "data-changed",
    &gwy_resource_get_is_const,
    &gwy_resource_get_item_name,
    &gwy_resource_compare,
    &gwy_resource_rename,
    NULL,  /* needs particular class */
    NULL,  /* needs particular class */
    NULL,  /* needs particular class */
};

G_DEFINE_TYPE_EXTENDED
    (GwyResource, gwy_resource, G_TYPE_OBJECT, G_TYPE_FLAG_ABSTRACT,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_resource_serializable_init))

static void
gwy_resource_serializable_init(G_GNUC_UNUSED GwySerializableIface *iface)
{
}

static void
gwy_resource_class_init(GwyResourceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_resource_finalize;

    klass->item_type = gwy_resource_item_type;

    resource_signals[DATA_CHANGED]
        = g_signal_new("data-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyResourceClass, data_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_resource_init(GwyResource *resource)
{
    gwy_debug_objects_creation(G_OBJECT(resource));
}

static void
gwy_resource_finalize(GObject *object)
{
    GwyResource *resource = (GwyResource*)object;

    gwy_debug("%s", resource->name);
    if (resource->use_count)
        g_critical("Resource %p with nonzero use_count is finalized.", object);
    g_free(resource->name);

    G_OBJECT_CLASS(gwy_resource_parent_class)->finalize(object);
}

static const gchar*
gwy_resource_get_item_name(gpointer item)
{
    GwyResource *resource = (GwyResource*)item;
    return resource->name;
}

static gboolean
gwy_resource_get_is_const(gconstpointer item)
{
    GwyResource *resource = (GwyResource*)item;
    return resource->is_const;
}

static gboolean
gwy_resource_compare(gconstpointer item1,
                     gconstpointer item2)
{
    GwyResource *resource1 = (GwyResource*)item1;
    GwyResource *resource2 = (GwyResource*)item2;

    return strcmp(resource1->name, resource2->name);
}

static void
gwy_resource_rename(gpointer item,
                    const gchar *new_name)
{
    GwyResource *resource = (GwyResource*)item;

    g_return_if_fail(!resource->is_const);
    g_free(resource->name);
    resource->name = g_strdup(new_name);
}

/**
 * gwy_resource_get_name:
 * @resource: A resource.
 *
 * Returns resource name.
 *
 * Returns: Name of @resource.  The string is owned by @resource and must not
 *          be modfied or freed.
 **/
const gchar*
gwy_resource_get_name(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), NULL);
    return resource->name;
}

/**
 * gwy_resource_get_is_modifiable:
 * @resource: A resource.
 *
 * Returns whether a resource is modifiable.
 *
 * Returns: %TRUE if resource is modifiable, %FALSE if it's fixed (system)
 *          resource.
 **/
gboolean
gwy_resource_get_is_modifiable(GwyResource *resource)
{
    g_return_val_if_fail(GWY_IS_RESOURCE(resource), FALSE);
    return !resource->is_const;
}

/**
 * gwy_resource_class_get_name:
 * @klass: Resource class.
 *
 * Gets the name of resource class.
 *
 * This is an simple identifier usable for example as directory name.
 *
 * Returns: Resource class name, as a constant string that must not be modified
 *          nor freed.
 **/
const gchar*
gwy_resource_class_get_name(GwyResourceClass *klass)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    return klass->name;
}

/**
 * gwy_resource_class_get_traits:
 * @klass: Resource class.
 * @ntraits: Location to store the number of traits.
 *
 * Gets the traits of a resource class.
 *
 * Returns: An array of trait types of length *@ntraits.  It is owned by class
 *          and must not be modified.
 **/
const GType*
gwy_resource_class_get_traits(GwyResourceClass *klass,
                              gint *ntraits)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    if (ntraits)
        *ntraits = klass->n_traits;
    return klass->traits;
}

const GwyInventoryItemType*
gwy_resource_class_get_item_type(GwyResourceClass *klass)
{
    g_return_val_if_fail(GWY_IS_RESOURCE_CLASS(klass), NULL);
    return &klass->item_type;
}

void
gwy_resource_get_trait(GwyResource *resource,
                       gint n,
                       GValue *value)
{
    GwyResourceClass *klass;
    void (*method)(gpointer, gint, GValue*);

    g_return_if_fail(GWY_IS_RESOURCE(resource));
    klass = GWY_RESOURCE_GET_CLASS(resource);
    g_return_if_fail(n < 0 || n >= klass->n_traits);
    method = klass->item_type.get_trait_value;
    g_return_if_fail(method);
    method(resource, n, value);
}

/**
 * gwy_resource_ref:
 * @resource: A resource.
 *
 * References a resource, indicating intent to use it.
 *
 * Call to this function is necessary to use a resource properly.
 * It makes the resource to create any auxiliary structures that consume
 * considerable amount of memory and perform other initialization to
 * ready-to-use form.
 *
 * Resources usually exist through almose whole program lifetime from
 * #GObject perspective, but from usage perspective this method is the
 * constructor and gwy_resource_unref() is the destructor.
 *
 * When a resource is no longer used, it should be released with
 * gwy_resource_unref().
 **/
void
gwy_resource_ref(GwyResource *resource)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));

    if (!resource->use_count++) {
        void (*method)(GwyResource*);

        method = GWY_RESOURCE_GET_CLASS(resource)->use;
        if (method)
            method(resource);
    }
}

/**
 * gwy_resource_unref:
 * @resource: A resource.
 *
 * Unreferences a resource, indicating intent to release it.
 *
 * When the number of resource references drops to zero, it frees all
 * auxiliary data and returns back to `latent' form.  See gwy_resource_ref()
 * for more.
 **/
void
gwy_resource_unref(GwyResource *resource)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    g_return_if_fail(resource->use_count);

    if (!--resource->use_count) {
        void (*method)(GwyResource*);

        method = GWY_RESOURCE_GET_CLASS(resource)->unuse;
        if (method)
            method(resource);
    }
}

/**
 * gwy_resource_dump:
 * @resource: A resource.
 *
 * Dumps a resource to a textual (human readable) form.
 *
 * Returns: Textual resource representation.
 **/
GString*
gwy_resource_dump(GwyResource *resource)
{
    GString* (*method)(GwyResource*);
    GString *str;
    gchar *s;

    g_return_val_if_fail(GWY_IS_RESOURCE(resource), NULL);
    method = GWY_RESOURCE_GET_CLASS(resource)->dump;
    g_return_val_if_fail(method, NULL);

    str = method(resource);
    g_return_val_if_fail(str, NULL);
    s = g_strconcat(MAGIC_HEADER,
                    g_type_name(G_TYPE_FROM_INSTANCE(resource)),
                    "\n",
                    NULL);
    g_string_prepend(str, s);
    g_free(s);

    return str;
}

/**
 * gwy_resource_parse:
 * @text: Textual resource representation.
 *
 * Reconstructs a resource from human readable form.
 *
 * Returns: Newly created resource (or %NULL).
 **/
GwyResource*
gwy_resource_parse(const gchar *text)
{
    GwyResourceClass *klass;
    GType type;
    gchar *name;
    guint len;

    if (!g_str_has_prefix(text, MAGIC_HEADER)) {
        g_warning("Wrong resource magic header");
        return NULL;
    }

    text += sizeof(MAGIC_HEADER) - 1;
    len = strspn(text, G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS);
    name = g_strndup(text, len);
    text = strchr(text + len, '\n');
    if (!text) {
        g_warning("Empty resource body");
        return NULL;
    }
    type = g_type_from_name(name);
    if (!type
        || !g_type_is_a(type, GWY_TYPE_RESOURCE)
        || !G_TYPE_IS_INSTANTIATABLE(type)) {
        g_warning("Wrong resource type `%s'", name);
        g_free(name);
        return NULL;
    }
    g_free(name);
    klass = GWY_RESOURCE_CLASS(g_type_class_peek_static(type));
    g_return_val_if_fail(klass && klass->parse, NULL);

    return klass->parse(text);
}

/**
 * gwy_data_field_data_changed:
 * @resource: A resource.
 *
 * Emits signal "data-changed" on a resource.
 **/
void
gwy_resource_data_changed(GwyResource *resource)
{
    g_return_if_fail(GWY_IS_RESOURCE(resource));
    g_signal_emit(resource, resource_signals[DATA_CHANGED], 0);
}

/************************** Documentation ****************************/

/**
 * GwyResource:
 *
 * The #GwyResource struct contains private data only and should be accessed
 * using the functions below.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
