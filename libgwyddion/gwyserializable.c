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
#include <glib-object.h>
#include <glib/gutils.h>

#include <libgwyddion/gwymacros.h>
#include "gwyserializable.h"

#define GWY_SERIALIZABLE_TYPE_NAME "GwySerializable"

static GByteArray* gwy_serializable_do_serialize   (GObject *serializable,
                                                    GByteArray *buffer);
static void        gwy_serialize_skip_type         (const guchar *buffer,
                                                    gsize size,
                                                    gsize *position,
                                                    guchar ctype);;
static void        gwy_serializable_base_init          (gpointer g_class);
static GObject*    gwy_serializable_duplicate_hard_way (GObject *object);

static GByteArray* gwy_serialize_spec              (GByteArray *buffer,
                                                    const GwySerializeSpec *sp);
static gsize       gwy_serialize_spec_get_size     (const GwySerializeSpec *sp);
static gboolean    gwy_deserialize_spec_value      (const guchar *buffer,
                                                    gsize size,
                                                    gsize *position,
                                                    GwySerializeSpec *sp);
static GwySerializeItem* gwy_deserialize_hash_items  (const guchar *buffer,
                                                      gsize size,
                                                      gsize *nitems);

static inline gsize ctype_size     (guchar ctype);

GType
gwy_serializable_get_type(void)
{
    static GType gwy_serializable_type = 0;

    if (!gwy_serializable_type) {
        static const GTypeInfo gwy_serializable_info = {
            sizeof(GwySerializableIface),
            (GBaseInitFunc)gwy_serializable_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL,
            NULL,
        };

        gwy_serializable_type
            = g_type_register_static(G_TYPE_INTERFACE,
                                     GWY_SERIALIZABLE_TYPE_NAME,
                                     &gwy_serializable_info,
                                     0);
        g_type_interface_add_prerequisite(gwy_serializable_type, G_TYPE_OBJECT);
    }

    gwy_debug("%lu", gwy_serializable_type);
    return gwy_serializable_type;
}

static void
gwy_serializable_base_init(G_GNUC_UNUSED gpointer g_class)
{
    static gboolean initialized = FALSE;

    gwy_debug("initialized = %d", initialized);
    if (initialized)
        return;
    initialized = TRUE;
}

/**
 * gwy_serializable_serialize:
 * @serializable: A #GObject that implements #GwySerializable interface.
 * @buffer: A buffer to which the serialized object should be appended,
 *          or %NULL to allocate and return a new #GByteArray.
 *
 * Serializes an object to byte buffer.
 *
 * This is a high-level method.  Do not use it for implementation of child
 * object serialization (should you ever need to do it manually), it would
 * lead to repeated required buffer size calculations.  In such a case, use
 * gwy_serializable_do_serialize() FIXME: but that's not public.
 *
 * Returns: @buffer or a newly allocated #GByteArray with serialized
 *          object appended.
 **/
GByteArray*
gwy_serializable_serialize(GObject *serializable,
                           GByteArray *buffer)
{
    GwySerializeFunc serialize_method;
    gsize expected_size;

    g_return_val_if_fail(serializable, NULL);
    g_return_val_if_fail(GWY_IS_SERIALIZABLE(serializable), NULL);
    gwy_debug("serializing a `%s'",
              g_type_name(G_TYPE_FROM_INSTANCE(serializable)));

    serialize_method = GWY_SERIALIZABLE_GET_IFACE(serializable)->serialize;
    if (!serialize_method) {
        g_critical("`%s' doesn't implement serialize()",
                   g_type_name(G_TYPE_FROM_INSTANCE(serializable)));
        return NULL;
    }

    /* Allocate space for all the data in one turn */
    expected_size = gwy_serializable_get_size(serializable);
    if (!buffer)
        buffer = g_byte_array_sized_new(expected_size);
    else {
        /* This trick can make GLib to fill the unused part of array with
         * zeros.  But that's a small price to pay. */
        g_byte_array_set_size(buffer, buffer->len + expected_size);
        g_byte_array_set_size(buffer, buffer->len - expected_size);
    }

    return serialize_method(serializable, buffer);
}

/**
 * gwy_serializable_get_size:
 * @serializable: A #GObject that implements #GwySerializable interface.
 *
 * Calculates the expected size of serialized object.
 *
 * Returns: The expected size of serialized @serializable.
 **/
gsize
gwy_serializable_get_size(GObject *serializable)
{
    gsize (*get_size_method)(GObject*);
    const gchar *type_name;
    gsize size;

    g_return_val_if_fail(serializable, 0);
    g_return_val_if_fail(GWY_IS_SERIALIZABLE(serializable), 0);

    get_size_method = GWY_SERIALIZABLE_GET_IFACE(serializable)->get_size;
    type_name = g_type_name(G_TYPE_FROM_INSTANCE(serializable));
    if (!get_size_method) {
        g_warning("`%s' doesn't implement get_size(), assuming empty",
                  type_name);
        return strlen(type_name) + 1 + sizeof(guint32);
    }

    size = get_size_method(serializable);
    gwy_debug("Expected size of `%s' is %" G_GSIZE_FORMAT, type_name, size);
    return size;
}

/**
 * gwy_serializable_deserialize:
 * @buffer: A block of memory of size @size contaning object representation.
 * @size: The size of @buffer.
 * @position: The position of the object in @buffer, it's updated to
 *            point after it.
 *
 * Restores a serialized object from byte buffer.
 *
 * The newly created object has reference count according to its nature, thus
 * a #GtkObject will have a floating reference, a #GObject will have a
 * refcount of 1, etc.
 *
 * Returns: A newly created object.
 **/
GObject*
gwy_serializable_deserialize(const guchar *buffer,
                             gsize size,
                             gsize *position)
{
    static const gchar *generic_skip_msg =
        "Trying to recover by generic object skip. "
        "This can fail if the class uses some very unusual "
        "serialization practices or we've got out of sync.";

    GType type;
    GwyDeserializeFunc deserialize_method;
    GObject *object;
    gsize typenamesize, oldposition;
    gpointer classref;

    g_return_val_if_fail(buffer, NULL);

    /* Get type name */
    typenamesize = gwy_serialize_check_string(buffer, size, *position, NULL);
    if (!typenamesize) {
        g_warning("Memory contents at %p doesn't look as an serialized object. "
                  "Trying to recover by ignoring rest of buffer.", buffer);
        *position = size;
        return NULL;
    }

    /* Get type from name */
    type = g_type_from_name((gchar*)(buffer + *position));
    if (!type) {
        g_warning("Type `%s' is unknown. %s",
                  buffer + *position, generic_skip_msg);
        gwy_serialize_skip_type(buffer, size, position, 'o');
        return NULL;
    }

    /* Get class from type */
    classref = g_type_class_ref(type);
    g_assert(classref);   /* this really should not fail */
    gwy_debug("deserializing a `%s'", g_type_name(type));
    if (!G_TYPE_IS_INSTANTIATABLE(type)) {
        g_warning("Type `%s' is not instantiable. %s",
                  buffer + *position, generic_skip_msg);
        gwy_serialize_skip_type(buffer, size, position, 'o');
        return NULL;
    }
    if (!g_type_is_a(type, GWY_TYPE_SERIALIZABLE)) {
        g_warning("Type `%s' is not serializable. %s",
                  buffer + *position, generic_skip_msg);
        gwy_serialize_skip_type(buffer, size, position, 'o');
        return NULL;
    }

    /* FIXME: this horrible construct gets interface class from a mere GType;
     * deserialize() is a class method, not an object method, there already
     * has to be some macro for it in gobject... */
    deserialize_method
        = ((GwySerializableIface*)
                g_type_interface_peek(g_type_class_peek(type),
                                      GWY_TYPE_SERIALIZABLE))->deserialize;
    if (!deserialize_method) {
        g_critical("Class `%s' doesn't implement deserialize()", buffer);
        gwy_serialize_skip_type(buffer, size, position, 'o');
        return NULL;
    }
    oldposition = *position;
    object = deserialize_method(buffer, size, position);
    if (object)
        g_type_class_unref(G_OBJECT_GET_CLASS(object));
    else {
        /* If deserialize fails, don't trust it and prefer generic object
         * skip. */
        *position = oldposition;
        g_warning("Object `%s' deserialization failed. %s",
                  buffer + *position, generic_skip_msg);
        gwy_serialize_skip_type(buffer, size, position, 'o');
        g_warning("Cannot safely unref class after failed `%s' deserialization",
                  g_type_name(type));
    }
    return object;
}

/**
 * gwy_serializable_duplicate:
 * @object: An object implementing #GwySerializable interface.
 *
 * Creates a copy of an object.
 *
 * If the object doesn't support duplication natively, it's brute-force
 * serialized and then deserialized, this may be quite inefficient,
 * namely for large objects.
 *
 * You can duplicate a %NULL, too, but you are discouraged from doing it.
 *
 * Returns: The newly created object copy.  However if the object is a
 *          singleton, @object itself (with incremented reference count)
 *          can be returned, too.
 **/
GObject*
gwy_serializable_duplicate(GObject *object)
{
    GObject* (*duplicate_method)(GObject*);

    if (!object)
        return NULL;
    g_return_val_if_fail(GWY_IS_SERIALIZABLE(object), NULL);

    duplicate_method = GWY_SERIALIZABLE_GET_IFACE(object)->duplicate;
    if (duplicate_method)
        return duplicate_method(object);

    return gwy_serializable_duplicate_hard_way(object);
}

static GObject*
gwy_serializable_duplicate_hard_way(GObject *object)
{
    GByteArray *buffer = NULL;
    gsize position = 0;
    GObject *duplicate;

    g_warning("`%s' doesn't have its own duplicate() method, "
              "forced to duplicate it the hard way.",
              g_type_name(G_TYPE_FROM_INSTANCE(object)));

    buffer = gwy_serializable_serialize(object, NULL);
    if (!buffer) {
        g_critical("`%s' serialization failed",
                   g_type_name(G_TYPE_FROM_INSTANCE(object)));
        return NULL;
    }
    duplicate = gwy_serializable_deserialize(buffer->data, buffer->len,
                                             &position);
    g_byte_array_free(buffer, TRUE);

    return duplicate;
}

/**
 * gwy_serializable_clone:
 * @source: An object implementing #GwySerializable interface.
 * @copy: An object of the same type as @source to modify after it.
 *
 * Makes an object identical to another object of the same type.
 *
 * More precisely, @source may be subclass of @copy (the extra information
 * is lost then).
 **/
void
gwy_serializable_clone(GObject *source,
                       GObject *copy)
{
    GType source_type, copy_type;
    void (*clone_method)(GObject*, GObject*);

    g_return_if_fail(GWY_IS_SERIALIZABLE(source));
    g_return_if_fail(GWY_IS_SERIALIZABLE(copy));
    if (source == copy)
        return;

    source_type = G_TYPE_FROM_INSTANCE(source);
    copy_type = G_TYPE_FROM_INSTANCE(copy);
    g_return_if_fail(g_type_is_a(copy_type, source_type));

    clone_method = GWY_SERIALIZABLE_GET_IFACE(copy)->clone;
    if (!clone_method) {
        g_critical("`%s' doesn't implement clone()",
                   g_type_name(G_TYPE_FROM_INSTANCE(copy)));
        return;
    }
    clone_method(source, copy);
}

/**
 * gwy_byteswapped_copy:
 * @source: Pointer to memory to copy.
 * @dest: Pointer where to copy @src to.
 * @size: Size of one item, must be a power of 2.
 * @len: Number of items to copy.
 * @byteswap: Byte swapping pattern -- if a bit is set, blocks of
 *            corresponding size are swapped.  For byte order reversion,
 *            @byteswap must be equal to 2^@size-1.
 *
 * Copies memory, byte swapping meanwhile.
 *
 * This function is not very fast, but neither assumes any memory alignment
 * nor alocates any temporary buffers.
 **/
static inline void
gwy_byteswapped_copy(const guint8 *source,
                     guint8 *dest,
                     gsize size,
                     gsize len,
                     gsize byteswap)
{
    gsize i, k;

    if (!byteswap) {
        memcpy(dest, source, size*len);
        return;
    }

    for (i = 0; i < len; i++) {
        guint8 *b = dest + i*size;

        for (k = 0; k < size; k++)
            b[k ^ byteswap] = *(source++);
    }
}

/**
 * gwy_byteswapped_append:
 * @source: Pointer to memory to copy.
 * @dest: #GByteArray to copy the memory to.
 * @size: Size of one item, must be a power of 2.
 * @len: Number of items to copy.
 * @byteswap: Byte swapping pattern -- if a bit is set, blocks of
 *            corresponding size are swapped.  For byte order reversion,
 *            @byteswap must be equal to 2^@size-1.
 *
 * Appends memory to a byte array, byte swapping meanwhile.
 *
 * This function is not very fast, but neither assumes any memory alignment
 * nor alocates any temporary buffers.
 **/
static inline void
gwy_byteswapped_append(guint8 *source,
                       GByteArray *dest,
                       gsize size,
                       gsize len,
                       gsize byteswap)
{
    gsize i, k;
    guint8 *buffer;

    if (!byteswap) {
        g_byte_array_append(dest, source, size*len);
        return;
    }

    buffer = dest->data + dest->len;
    g_byte_array_set_size(dest, dest->len + size*len);
    for (i = 0; i < len; i++) {
        guint8 *b = buffer + i*size;

        for (k = 0; k < size; k++)
            b[k ^ byteswap] = *(source++);
    }
}

/**
 * ctype_size:
 * @ctype: Component type, as in gwy_serialize_pack_object_struct().
 *
 * Compute type size based on type letter.
 *
 * Returns: Size in bytes, 0 for arrays and other nonatomic types.
 **/
static inline gsize G_GNUC_CONST
ctype_size(guchar ctype)
{
    switch (ctype) {
        case 'c':
        case 'b':
        return sizeof(guchar);
        break;

        case 'i':
        return sizeof(gint32);
        break;

        case 'q':
        return sizeof(gint64);
        break;

        case 'd':
        return sizeof(gdouble);
        break;

        default:
        return 0;
        break;
    }
}

/****************************************************************************
 *
 * Serialization
 *
 ****************************************************************************/

/**
 * gwy_serializable_do_serialize:
 * @serializable: A #GObject that implements #GwySerializable interface.
 * @buffer: A buffer to which the serialized object should be appended,
 *
 * Performs serialization of an object to byte buffer.
 *
 * This is the low-level method that does not attempt to calculate required
 * buffer size and that should be used for child object serialization.
 *
 * Returns: @buffer or a newly allocated #GByteArray with serialized
 *          object appended.
 **/
static GByteArray*
gwy_serializable_do_serialize(GObject *serializable,
                              GByteArray *buffer)
{
    GwySerializeFunc serialize_method;

    g_return_val_if_fail(serializable, NULL);
    g_return_val_if_fail(GWY_IS_SERIALIZABLE(serializable), NULL);
    gwy_debug("serializing a `%s'",
              g_type_name(G_TYPE_FROM_INSTANCE(serializable)));

    serialize_method = GWY_SERIALIZABLE_GET_IFACE(serializable)->serialize;
    if (!serialize_method) {
        g_critical("`%s' doesn't implement serialize()",
                   g_type_name(G_TYPE_FROM_INSTANCE(serializable)));
        return NULL;
    }
    return serialize_method(serializable, buffer);
}

/**
 * gwy_serialize_store_int32:
 * @buffer: A buffer to which the value should be stored.
 * @position: Position in the buffer to store @value to.
 * @value: A 32bit integer.
 *
 * Stores a 32bit integer to a buffer.
 **/
static void
gwy_serialize_store_int32(GByteArray *buffer,
                          gsize position,
                          guint32 value)
{
    value = GINT32_TO_LE(value);
    memcpy(buffer->data + position, &value, sizeof(guint32));
}

/**
 * gwy_serialize_pack_object_header:
 * @buffer: A buffer to which the serialized object header should be appended.
 * @object_name: Object name.
 *
 * Packs object name and size to a byte buffer.
 *
 * As the size is unknown, placeholder value 0 is stored instead.
 * It has to be filled later with gwy_serialize_store_int32().
 *
 * Returns: @buffer or a newly allocated #GByteArray with serialization of
 *          object header appended.
 **/
static GByteArray*
gwy_serialize_pack_object_header(GByteArray *buffer,
                                 const guchar *object_name)
{
    gint32 value = 0;

    if (!buffer)
        buffer = g_byte_array_new();
    g_byte_array_append(buffer, object_name, strlen(object_name) + 1);
    g_byte_array_append(buffer, (guint8*)&value, sizeof(gint32));

    return buffer;
}

/**
 * gwy_serialize_pack_object_struct:
 * @buffer: A buffer to which the serialized components should be appended.
 * @object_name: The g_type_name() of the object.
 * @nspec: The number of items in @spec.
 * @spec: The components to serialize.
 *
 * Appends serialization of object with g_type_name() @object_name and
 * components described by @spec to @buffer in gwy-file format.
 *
 * Here's how a serialization method of a simple object whose state is
 * described by a single real number foo< could look (without error checking):
 * <informalexample><programlisting>
 * static guchar*
 * my_object_serialize(GObject *obj,
 *                     guchar *buffer,
 *                     gsize *size)
 * {
 *     MyObject *my_object = MY_OBJECT(obj);
 *     GwySerializeSpec spec[] = {
 *         { 'd', "foo", &amp;my_object-&gt;foo, NULL, },
 *     };
 *     <!-- Hello, gtk-doc! -->
 *     return gwy_serialize_pack_object_struct(buffer, size,
 *                                             "MyObject",
 *                                             G_N_ELEMENTS(spec), spec);
 * }
 * </programlisting></informalexample>
 *
 * Returns: The buffer with serialization of @spec components appended.
 **/
GByteArray*
gwy_serialize_pack_object_struct(GByteArray *buffer,
                                 const guchar *object_name,
                                 gsize nspec,
                                 const GwySerializeSpec *spec)
{
    gsize before_obj, i;

    g_return_val_if_fail(spec || !nspec, buffer);
    g_return_val_if_fail(object_name && *object_name, buffer);
    gwy_debug("init size: %u, buffer = %p", buffer ? buffer->len : 0, buffer);

    buffer = gwy_serialize_pack_object_header(buffer, object_name);
    before_obj = buffer->len;
    gwy_debug("+head size: %u", buffer->len);
    for (i = 0; i < nspec; i++) {
        if (!spec[i].value) {
            gwy_debug("ignoring item `%s' with NULL value", spec[i].name);
            continue;
        }
        gwy_serialize_spec(buffer, spec + i);
    }
    gwy_debug("+body size: %u", buffer->len);
    gwy_serialize_store_int32(buffer, before_obj - sizeof(guint32),
                              buffer->len - before_obj);
    return buffer;
}

/**
 * gwy_serialize_object_items:
 * @buffer: A buffer to which the serialized components should be appended,
 *          or %NULL.
 * @object_name: The g_type_name() of the object.
 * @nitems: The number of @items items.
 * @items: The components to serialize.
 *
 * Serializes an object to buffer in gwy-file format.
 *
 * More precisely, it appends serialization of object with g_type_name()
 * @object_name with components described by @items to @buffer.
 *
 * Returns: @buffer or a newly allocated #GByteArray with serialization of
 *          @items components appended.
 **/
GByteArray*
gwy_serialize_object_items(GByteArray *buffer,
                           const guchar *object_name,
                           gsize nitems,
                           const GwySerializeItem *items)
{
    GwySerializeSpec sp;
    gsize before_obj, i;

    g_return_val_if_fail(items || !nitems, buffer);
    g_return_val_if_fail(object_name && *object_name, buffer);
    gwy_debug("init size: %u, buffer = %p", buffer ? buffer->len : 0, buffer);

    buffer = gwy_serialize_pack_object_header(buffer, object_name);
    before_obj = buffer->len;
    gwy_debug("+head size: %u", buffer->len);

    for (i = 0; i < nitems; i++) {
        sp.ctype = items[i].ctype;
        sp.name = items[i].name;
        sp.value = (const gpointer)&items[i].value;
        sp.array_size = (guint32*)&items[i].array_size;
        gwy_serialize_spec(buffer, &sp);
    }

    gwy_debug("+body size: %u", buffer->len);
    gwy_serialize_store_int32(buffer, before_obj - sizeof(guint32),
                              buffer->len - before_obj);

    return buffer;
}

static GByteArray*
gwy_serialize_spec(GByteArray *buffer,
                   const GwySerializeSpec *sp)
{
    guint32 asize = 0, leasize;
    gsize j;
    guint8 *arr = NULL;

    g_assert(sp->value);
    if (g_ascii_isupper(sp->ctype)) {
        g_assert(sp->array_size);
        g_assert(*(gpointer*)sp->value);
        asize = *sp->array_size;
        leasize = GINT32_TO_LE(asize);
        arr = *(guint8**)sp->value;
        if (!asize) {
            g_warning("Ignoring zero-length array <%s>", sp->name);
            return buffer;
        }
    }

    g_byte_array_append(buffer, sp->name, strlen(sp->name) + 1);
    g_byte_array_append(buffer, &sp->ctype, 1);
    gwy_debug("<%s> <%c> %u", sp->name, sp->ctype, buffer->len);
    switch (sp->ctype) {
        case 'b': {
            /* store it as char */
            char value = *(gboolean*)sp->value;

            value = !!value;
            g_byte_array_append(buffer, &value, 1);
        }
        break;

        case 'c': {
            g_byte_array_append(buffer, sp->value, 1);
        }
        break;

        case 'C': {
            g_byte_array_append(buffer, (guint8*)&leasize, sizeof(gint32));
            g_byte_array_append(buffer, arr, asize*sizeof(char));
        }
        break;

        case 'i': {
            gint32 value = *(gint32*)sp->value;

            value = GINT32_TO_LE(value);
            g_byte_array_append(buffer, (guint8*)&value, sizeof(gint32));
        }
        break;

        case 'I': {
            g_byte_array_append(buffer, (guint8*)&leasize, sizeof(gint32));
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
            g_byte_array_append(buffer, arr, asize*sizeof(gint32));
#else
             gwy_byteswapped_append(arr, buffer, sizeof(gint32), asize,
                                       (1 << sizeof(gint32)) - 1);
#endif
        }
        break;

        case 'q': {
            gint64 value = *(gint64*)sp->value;

            value = GINT64_TO_LE(value);
            g_byte_array_append(buffer, (guint8*)&value, sizeof(gint64));
        }
        break;

        case 'Q': {
            g_byte_array_append(buffer, (guint8*)&leasize, sizeof(gint32));
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
            g_byte_array_append(buffer, arr, asize*sizeof(gint64));
#else
            gwy_byteswapped_append(arr, buffer, sizeof(gint64), asize,
                                   (1 << sizeof(gint64)) - 1);
#endif
        }
        break;

        case 'd': {
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
            g_byte_array_append(buffer, sp->value, sizeof(gdouble));
#else
            gwy_byteswapped_append(sp->value, buffer, sizeof(gdouble), 1,
                                   (1 << sizeof(gdouble)) - 1);
#endif
        }
        break;

        case 'D': {
            g_byte_array_append(buffer, (guint8*)&leasize, sizeof(gint32));
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
            g_byte_array_append(buffer, arr, asize*sizeof(gdouble));
#else
            gwy_byteswapped_append(arr, buffer, sizeof(gdouble), asize,
                                   (1 << sizeof(gdouble)) - 1);
#endif
        }
        break;

        case 's': {
            guchar *value = *(guchar**)sp->value;

            if (!value) {
                g_warning("representing NULL as an empty string");
                g_byte_array_append(buffer, "", 1);
            }
            else
                g_byte_array_append(buffer, value, strlen(value) + 1);
        }
        break;

        case 'S': {
            g_byte_array_append(buffer, (guint8*)&leasize, sizeof(gint32));
            for (j = 0; j < asize; j++) {
                guchar *value = ((guchar**)arr)[j];

                if (!value) {
                    g_warning("representing NULL as an empty string");
                    g_byte_array_append(buffer, "", 1);
                }
                else
                    g_byte_array_append(buffer, value, strlen(value) + 1);
            }
        }
        break;

        case 'o': {
            GObject *value = *(GObject**)sp->value;

            if (G_UNLIKELY(!value))
                g_critical("Object cannot be NULL");
            else if (G_UNLIKELY(!GWY_IS_SERIALIZABLE(value)))
                g_critical("Object must be serializable");
            else
                gwy_serializable_do_serialize(value, buffer);
        }
        break;

        case 'O': {
            g_byte_array_append(buffer, (guint8*)&leasize, sizeof(gint32));
            for (j = 0; j < asize; j++) {
                GObject *value = ((GObject**)arr)[j];

                if (G_UNLIKELY(!value))
                    g_critical("Object cannot be NULL");
                else if (G_UNLIKELY(!GWY_IS_SERIALIZABLE(value)))
                    g_critical("Object must be serializable");
                else
                    gwy_serializable_do_serialize(value, buffer);
            }
        }
        break;

        default:
        g_critical("wrong spec <%c>", sp->ctype);
        break;
    }

    gwy_debug("after: %u", buffer->len);
    return buffer;
}

/****************************************************************************
 *
 * Size calculation
 *
 ****************************************************************************/

/**
 * gwy_serialize_get_struct_object_size:
 * @object_name: The g_type_name() of the object.
 * @nspec: The number of items in @spec.
 * @spec: The components to serialize.
 *
 * Calculates serialized object size for struct-like objects.
 *
 * The component specification is the same as in
 * gwy_serialize_pack_object_struct().
 *
 * Returns: Serialized object size, it included space for object name and size.
 *          The value is exact unless some components are objects that do not
 *          return exact size estimate themselves.
 **/
gsize
gwy_serialize_get_struct_size(const guchar *object_name,
                              gsize nspec,
                              const GwySerializeSpec *spec)
{
    gsize i, size;

    g_return_val_if_fail(spec || !nspec, 0);
    g_return_val_if_fail(object_name && *object_name, 0);

    size = strlen(object_name) + 1 + sizeof(guint32);
    for (i = 0; i < nspec; i++) {
        if (!spec[i].value) {
            gwy_debug("ignoring item `%s' with NULL value", spec[i].name);
            continue;
        }
        size += gwy_serialize_spec_get_size(spec + i);
    }

    return size;
}

/**
 * gwy_serialize_get_items_size:
 * @object_name: The g_type_name() of the object.
 * @nitems: The number of @items items.
 * @items: The components to serialize.
 *
 * Calculates serialized object size for hash-like objects.
 *
 * The component specification is the same as in
 * gwy_serialize_object_items().
 *
 * Returns: Serialized object size, it included space for object name and size.
 *          The value is exact unless some components are objects that do not
 *          return exact size estimate themselves.
 **/
gsize
gwy_serialize_get_items_size(const guchar *object_name,
                             gsize nitems,
                             const GwySerializeItem *items)
{
    GwySerializeSpec sp;
    gsize size, i;

    g_return_val_if_fail(items || !nitems, 0);
    g_return_val_if_fail(object_name && *object_name, 0);

    size = strlen(object_name) + 1 + sizeof(guint32);
    for (i = 0; i < nitems; i++) {
        sp.ctype = items[i].ctype;
        sp.name = items[i].name;
        sp.value = (const gpointer)&items[i].value;
        sp.array_size = (guint32*)&items[i].array_size;
        size += gwy_serialize_spec_get_size(&sp);
    }

    return size;
}

static gsize
gwy_serialize_spec_get_size(const GwySerializeSpec *sp)
{
    guint32 asize = 0;
    gsize size, j, s;
    guint8 *arr = NULL;

    g_assert(sp->value);
    if (g_ascii_isupper(sp->ctype)) {
        g_assert(sp->array_size);
        g_assert(*(gpointer*)sp->value);
        asize = *sp->array_size;
        if (!asize) {
            g_warning("Ignoring zero-length array <%s>", sp->name);
            return 0;
        }
        arr = *(guint8**)sp->value;
    }

    size = strlen(sp->name) + 1;
    size++;
    if ((s = ctype_size(sp->ctype)))
        return size + s;
    else if ((s = ctype_size(g_ascii_tolower(sp->ctype))))
        return size + s*asize;

    switch (sp->ctype) {
        case 's': {
            guchar *value = *(guchar**)sp->value;

            size++;
            if (value)
                size += strlen(value);
        }
        break;

        case 'S': {
            size += sizeof(guint32);
            for (j = 0; j < asize; j++) {
                guchar *value = ((guchar**)arr)[j];

                if (value)
                    size += strlen(value);
            }
            size += asize;
        }
        break;

        case 'o': {
            GObject *value = *(GObject**)sp->value;

            if (G_UNLIKELY(!value))
                g_critical("Object cannot be NULL");
            else if (G_UNLIKELY(!GWY_IS_SERIALIZABLE(value)))
                g_critical("Object must be serializable");
            else
                size += gwy_serializable_get_size(value);
        }
        break;

        case 'O': {
            size += sizeof(guint32);
            for (j = 0; j < asize; j++) {
                GObject *value = ((GObject**)arr)[j];

                if (G_UNLIKELY(!value))
                    g_critical("Object cannot be NULL");
                else if (G_UNLIKELY(!GWY_IS_SERIALIZABLE(value)))
                    g_critical("Object must be serializable");
                else
                    size += gwy_serializable_get_size(value);
            }
        }
        break;

        default:
        g_critical("wrong spec <%c>", sp->ctype);
        break;
    }

    return size;
}

/****************************************************************************
 *
 * Deserialization
 *
 ****************************************************************************/

/**
 * gwy_deserialize_int32:
 * @buffer: A memory location containing a serialized 32bit integer at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the integer in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one 32bit integer.
 *
 * Returns: The integer as gint32.
 **/
static inline gint32
gwy_deserialize_int32(const guchar *buffer,
                           gsize size,
                           gsize *position)
{
    gint32 value;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position + sizeof(gint32) <= size, 0);
    memcpy(&value, buffer + *position, sizeof(gint32));
    value = GINT32_FROM_LE(value);
    *position += sizeof(gint32);

    gwy_debug("value = <%d>", value);
    return value;
}

/**
 * gwy_deserialize_char:
 * @buffer: A memory location containing a serialized character at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the character in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one character.
 *
 * Returns: The character as guchar.
 **/
static inline guchar
gwy_deserialize_char(const guchar *buffer,
                          gsize size,
                          gsize *position)
{
    guchar value;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position + sizeof(guchar) <= size, '\0');
    value = buffer[*position];
    *position += sizeof(guchar);

    gwy_debug("value = <%c>", value);
    return value;
}

/**
 * gwy_serialize_skip_type:
 * @buffer: Serialized data.
 * @size: Size of @buffer.
 * @position: Current position in buffer, will be updated after type end.
 * @ctype: Type to skip.
 *
 * Skips a serialized item of given type in buffer.
 *
 * On failure, skips to the end of buffer.
 **/
static void
gwy_serialize_skip_type(const guchar *buffer,
                        gsize size,
                        gsize *position,
                        guchar ctype)
{
    static const gchar *too_short_msg =
        "Truncated or corrupted buffer, need %" G_GSIZE_FORMAT " bytes "
        "to skip <%c>, but only %" G_GSIZE_FORMAT " bytes remain.";
    static const gchar *no_string_msg =
        "Expected a string, trying to skip to end of [sub]buffer.";

    gsize tsize, alen;

    if (*position >= size) {
        g_critical("Trying to skip <%c> after end of buffer?!", ctype);
        return;
    }

    /* simple types */
    tsize = ctype_size(ctype);
    if (tsize) {
        if (*position + tsize > size) {
            g_warning(too_short_msg, tsize, ctype, size - *position);
            *position = size;
            return;
        }
        *position += tsize;
        return;
    }

    /* strings */
    if (ctype == 's') {
        tsize = gwy_serialize_check_string(buffer, size, *position, NULL);
        if (!tsize) {
            g_warning(no_string_msg);
            *position = size;
            return;
        }
        *position += tsize;
        return;
    }

    /* objects */
    if (ctype == 'o') {
        /* an object consists of its name... */
        tsize = gwy_serialize_check_string(buffer, size, *position, NULL);
        if (!tsize) {
            g_warning(no_string_msg);
            *position = size;
            return;
        }
        *position += tsize;
        /* ...and length of data... */
        if (*position + ctype_size('i') > size) {
            g_warning(too_short_msg, ctype_size('i'), 'i', size - *position);
            *position = size;
            return;
        }
        tsize = gwy_deserialize_int32(buffer, size, position);
        /* ...and the data */
        if (*position + tsize > size) {
            g_warning(too_short_msg, tsize, ctype, size - *position);
            *position = size;
            return;
        }
        *position += tsize;
        return;
    }

    /* string arrays */
    if (ctype == 'S') {
        if (*position + ctype_size('i') > size) {
            g_warning(too_short_msg, ctype_size('i'), 'i', size - *position);
            *position = size;
            return;
        }
        alen = gwy_deserialize_int32(buffer, size, position);
        while (alen) {
            tsize = gwy_serialize_check_string(buffer, size, *position, NULL);
            if (!tsize) {
                g_warning(no_string_msg);
                *position = size;
                return;
            }
            *position += tsize;
            alen--;
        }
        return;
    }

    /* object arrays */
    if (ctype == 'O') {
        if (*position + ctype_size('i') > size) {
            g_warning(too_short_msg, ctype_size('i'), 'i', size - *position);
            *position = size;
            return;
        }
        alen = gwy_deserialize_int32(buffer, size, position);
        while (alen) {
            /* an object consists of its name... */
            tsize = gwy_serialize_check_string(buffer, size, *position, NULL);
            if (!tsize) {
                g_warning(no_string_msg);
                *position = size;
                return;
            }
            *position += tsize;
            /* ...and length of data... */
            if (*position + ctype_size('i') > size) {
                g_warning(too_short_msg,
                          ctype_size('i'), 'i', size - *position);
                *position = size;
                return;
            }
            tsize = gwy_deserialize_int32(buffer, size, position);
            /* ...and the data */
            if (*position + tsize > size) {
                g_warning(too_short_msg, tsize, ctype, size - *position);
                *position = size;
                return;
            }
            *position += tsize;
            alen--;
        }
        return;
    }

    /* arrays of simple types */
    if (g_ascii_isupper(ctype)) {
        ctype = g_ascii_tolower(ctype);
        tsize = ctype_size(ctype);
        if (*position + ctype_size('i') > size) {
            g_warning(too_short_msg, ctype_size('i'), 'i', size - *position);
            *position = size;
            return;
        }
        alen = gwy_deserialize_int32(buffer, size, position);
        if (*position + alen*tsize > size) {
            g_warning(too_short_msg, alen*tsize, ctype, size - *position);
            *position = size;
            return;
        }
        *position += alen*tsize;
        return;
    }

    g_critical("Trying to skip uknown type `%c'", ctype);
    *position = size;
}

/* FIXME: Merge into gwy_serialize_unpack_object_struct()? */
static gboolean
gwy_serialize_unpack_struct(const guchar *buffer,
                            gsize size,
                            gsize nspec,
                            GwySerializeSpec *spec)
{
    gsize nlen, position;
    const GwySerializeSpec *sp;
    const guchar *name;
    guchar ctype;

    position = 0;
    while (position < size) {
        nlen = gwy_serialize_check_string(buffer, size, position, NULL);
        if (!nlen) {
            g_warning("Expected a component name to deserialize, got garbage");
            return FALSE;
        }

        for (sp = spec; (gsize)(sp - spec) < nspec; sp++) {
            if (gwy_strequal(sp->name, buffer + position))
                break;
        }
        name = buffer + position;
        position += nlen;
        if (position >= size) {
            g_warning("Got past the end of truncated or corrupted buffer");
            return FALSE;
        }
        ctype = gwy_deserialize_char(buffer, size, &position);
        if ((gsize)(sp - spec) == nspec) {
            g_warning("Extra component `%s' of type `%c'", name, ctype);
            gwy_serialize_skip_type(buffer, size, &position, ctype);
            continue;
        }
        if (!sp->value) {
            gwy_debug("ignoring item `%s' with NULL value", sp->name);
            gwy_serialize_skip_type(buffer, size, &position, ctype);
            continue;
        }

        if (ctype != sp->ctype) {
            g_warning("Bad or unknown type `%c' of `%s' (expected `%c')",
                      ctype, name, sp->ctype);
            return FALSE;
        }

        if (position + ctype_size(ctype) > size) {
            g_warning("Got past the end of truncated or corrupted buffer");
            return FALSE;
        }

        if (!gwy_deserialize_spec_value(buffer, size, &position,
                                        (GwySerializeSpec*)sp))
            return FALSE;
    }
    return TRUE;
}

/**
 * gwy_serialize_unpack_object_struct:
 * @buffer: A memory location containing a serialized object at position
 *          @position.
 * @size: Current size of @buffer, new size is returned here.
 * @position: The position of the object in @buffer, it's updated to point
 *            after it.
 * @object_name: The g_type_name() of the object.
 * @nspec: The number of items in @spec.
 * @spec: The components to deserialize.
 *
 * Deserializes an object with named components packed into gwy-file format by
 * gwy_serialize_pack_object_struct().
 *
 * Extra components are ignored (but cause a warning), components of different
 * type than expected cause failure, missing components are not detected.
 *
 * It is safe to pass pointers to existing non-atomic objects (strings, arrays,
 * objects) in @spec values, they will be dereferenced and freed as necessary
 * when an unpacked value is about to replace them.
 * For the same reason it is an error to pass pointers to unintialized memory
 * there, always initialize non-atomic @spec values to %NULL pointers, at
 * least.
 *
 * Caller is responsible for use/clean-up of these values if deserialization
 * succeeds or not.
 *
 * Here's how a deserialization method of a simple object whose state is
 * described by a single real number @foo could look (without error checking):
 * <informalexample><programlisting>
 * static GObject*
 * my_object_deserialize(const guchar *buffer,
 *                       gsize size,
 *                       gsize *position)
 * {
 *     double foo = 1.0;
 *     GwySerializeSpec spec[] = {
 *         { 'd', "foo", &amp;foo, NULL, },
 *     };
 *     MyObject *my_object;
 *     <!-- Hello, gtk-doc! -->
 *     gwy_serialize_unpack_object_struct(buffer, size, position,
 *                                        "MyObject",
 *                                        G_N_ELEMENTS(spec), spec);
 *     return my_object_new(foo);
 * }
 * </programlisting></informalexample>
 *
 * Returns: Whether the unpacking succeeded
 *          (see description body for definition of success and failure).
 **/
gboolean
gwy_serialize_unpack_object_struct(const guchar *buffer,
                                   gsize size,
                                   gsize *position,
                                   const guchar *object_name,
                                   gsize nspec,
                                   GwySerializeSpec *spec)
{
    gsize mysize;
    gboolean ok;

    mysize = gwy_serialize_check_string(buffer, size, *position, object_name);
    g_return_val_if_fail(mysize, FALSE);
    *position += mysize;

    mysize = gwy_deserialize_int32(buffer, size, position);
    g_return_val_if_fail(mysize <= size - *position, FALSE);
    ok = gwy_serialize_unpack_struct(buffer + *position, mysize, nspec, spec);
    *position += mysize;

    return ok;
}

/**
 * gwy_deserialize_object_hash:
 * @buffer: A block of memory of size @size contaning object representation.
 * @size: The size of @buffer.
 * @position: Current position in buffer, will be updated to point after
 *            object.
 * @object_name: The g_type_name() of the object.
 * @nitems: Where the number of deserialized components should be stored.
 *
 * Deserializes an object with arbitrary components from gwy-file format.
 *
 * This function works like gwy_serialize_unpack_object_struct(), except that
 * it does not use any a priori knowledge of what the object contains.  So
 * instead of filling values in supplied #GwySerializeSpec's, it constructs
 * #GwySerializeItem's completely from what is found in @buffer.  It does
 * considerably less sanity checks and even allows several components of the
 * same name.
 *
 * Returns: A newly allocated array of deserialized components.  Note the
 *          @name fields of #GwySerializeSpec's point to @buffer and thus are
 *          valid only as long as @buffer is; any arrays or strings are newly
 *          allocated and must be reused or freed by caller.
 **/
GwySerializeItem*
gwy_deserialize_object_hash(const guchar *buffer,
                            gsize size,
                            gsize *position,
                            const guchar *object_name,
                            gsize *nitems)
{
    gsize mysize;
    GwySerializeItem *items;

    mysize = gwy_serialize_check_string(buffer, size, *position, object_name);
    g_return_val_if_fail(mysize, NULL);
    *position += mysize;

    mysize = gwy_deserialize_int32(buffer, size, position);
    g_return_val_if_fail(mysize <= size - *position, NULL);
    items = gwy_deserialize_hash_items(buffer + *position, mysize, nitems);
    *position += mysize;

    return items;
}

static GwySerializeItem*
gwy_deserialize_hash_items(const guchar *buffer,
                           gsize size,
                           gsize *nitems)
{
    gsize nlen, position;
    GArray *items;
    GwySerializeItem it, *pit;
    GwySerializeSpec sp;

    if (!size) {
        *nitems = 0;
        /* Return something non-NULL since we didn't fail. */
        return g_new(GwySerializeItem, 1);
    }

    items = g_array_new(FALSE, FALSE, sizeof(GwySerializeItem));
    position = 0;
    sp.array_size = &it.array_size;
    sp.value = &it.value;
    while (position < size) {
        memset(&it, 0, sizeof(it));
        nlen = gwy_serialize_check_string(buffer, size, position, NULL);
        if (!nlen) {
            g_warning("Expected a component name to deserialize, got garbage");
            break;
        }
        it.name = buffer + position;
        position += nlen;
        if (position >= size) {
            g_warning("Got past the end of truncated or corrupted buffer");
            break;
        }
        it.ctype = gwy_deserialize_char(buffer, size, &position);
        if (position + ctype_size(it.ctype) > size) {
            g_warning("Got past the end of truncated or corrupted buffer");
            break;
        }
        sp.name = it.name;
        sp.ctype = it.ctype;
        if (!gwy_deserialize_spec_value(buffer, size, &position, &sp))
            break;
        g_array_append_val(items, it);
        gwy_debug("appended value #%u: <%s> of <%c>",
                  items->len - 1, sp.name, sp.ctype);
    }

    *nitems = items->len;
    pit = (GwySerializeItem*)items->data;
    g_array_free(items, FALSE);

    return pit;
}

/**
 * gwy_deserialize_boolean:
 * @buffer: A memory location containing a serialized boolean at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the character in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one boolean.
 *
 * Returns: The boolean as gboolean.
 **/
static inline gboolean
gwy_deserialize_boolean(const guchar *buffer,
                             gsize size,
                             gsize *position)
{
    gboolean value;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position + sizeof(guchar) <= size, FALSE);
    value = !!buffer[*position];
    *position += sizeof(guchar);

    gwy_debug("value = <%s>", value ? "TRUE" : "FALSE");
    return value;
}

/**
 * gwy_deserialize_char_array:
 * @buffer: A memory location containing a serialized character array at
 *          position @position.
 * @size: The size of @buffer.
 * @position: The position of the array in @buffer, it's updated to
 *            point after it.
 * @asize: Where the size of the array is to be returned on success.
 *
 * Deserializes a character array.
 *
 * Returns: The unpacked character array (newly allocated).
 **/
static inline guchar*
gwy_deserialize_char_array(const guchar *buffer,
                                gsize size,
                                gsize *position,
                                gsize *asize)
{
    guchar *value;
    gsize newasize;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);

    g_return_val_if_fail(*position + sizeof(gint32) <= size, NULL);
    if (!(newasize = gwy_deserialize_int32(buffer, size, position)))
        return NULL;
    g_return_val_if_fail(*position + newasize*sizeof(guchar) <= size, NULL);
    value = g_memdup(buffer + *position, newasize*sizeof(guchar));
    *position += newasize*sizeof(guchar);
    *asize = newasize;

    gwy_debug("|value| = %" G_GSIZE_FORMAT, newasize);
    return value;
}

/**
 * gwy_deserialize_int32_array:
 * @buffer: A memory location containing a serialized int32 array at
 *          position @position.
 * @size: The size of @buffer.
 * @position: The position of the array in @buffer, it's updated to
 *            point after it.
 * @asize: Where the size of the array is to be returned on success.
 *
 * Deserializes an int32 array.
 *
 * Returns: The unpacked 32bit integer array (newly allocated).
 **/
static inline gint32*
gwy_deserialize_int32_array(const guchar *buffer,
                                 gsize size,
                                 gsize *position,
                                 gsize *asize)
{
    gint32 *value;
    gsize newasize;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);

    g_return_val_if_fail(*position + sizeof(gint32) <= size, NULL);
    if (!(newasize = gwy_deserialize_int32(buffer, size, position)))
        return NULL;
    g_return_val_if_fail(*position + newasize*sizeof(gint32) <= size, NULL);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    value = g_memdup(buffer + *position, newasize*sizeof(gint32));
#else
    value = g_new(gint32, newasize);
    gwy_byteswapped_copy(buffer + *position, (guint8*)value,
                         sizeof(gint32), newasize, (1 << sizeof(gint32)) - 1);
#endif
    *position += newasize*sizeof(gint32);
    *asize = newasize;

    gwy_debug("|value| = %" G_GSIZE_FORMAT, newasize);
    return value;
}

/**
 * gwy_deserialize_int64:
 * @buffer: A memory location containing a serialized 64bit integer at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the integer in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one 64bit integer.
 *
 * Returns: The integer as gint64.
 **/
static inline gint64
gwy_deserialize_int64(const guchar *buffer,
                           gsize size,
                           gsize *position)
{
    gint64 value;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position + sizeof(gint64) <= size, 0);
    memcpy(&value, buffer + *position, sizeof(gint64));
    value = GINT64_FROM_LE(value);
    *position += sizeof(gint64);

    gwy_debug("value = <%lld>", value);
    return value;
}

/**
 * gwy_deserialize_int64_array:
 * @buffer: A memory location containing a serialized int64 array at
 *          position @position.
 * @size: The size of @buffer.
 * @position: The position of the array in @buffer, it's updated to
 *            point after it.
 * @asize: Where the size of the array is to be returned on success.
 *
 * Deserializes an int64 array.
 *
 * Returns: The unpacked 64bit integer array (newly allocated).
 **/
static inline gint64*
gwy_deserialize_int64_array(const guchar *buffer,
                                 gsize size,
                                 gsize *position,
                                 gsize *asize)
{
    gint64 *value;
    gsize newasize;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);

    g_return_val_if_fail(*position + sizeof(gint32) <= size, NULL);
    if (!(newasize = gwy_deserialize_int32(buffer, size, position)))
        return NULL;
    g_return_val_if_fail(*position + newasize*sizeof(gint64) <= size, NULL);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    value = g_memdup(buffer + *position, newasize*sizeof(gint64));
#else
    value = g_new(gint64, newasize);
    gwy_byteswapped_copy(buffer + *position, (guint8*)value,
                         sizeof(gint64), newasize, (1 << sizeof(gint64)) - 1);
#endif
    *position += newasize*sizeof(gint64);
    *asize = newasize;

    gwy_debug("|value| = %" G_GSIZE_FORMAT, newasize);
    return value;
}

/**
 * gwy_deserialize_double:
 * @buffer: A memory location containing a serialized gdouble at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the integer in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one gdouble.
 *
 * Returns: The integer as gdouble.
 **/
static inline gdouble
gwy_deserialize_double(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    gdouble value;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position + sizeof(gdouble) <= size, 0.0);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    memcpy(&value, buffer + *position, sizeof(gdouble));
#else
    gwy_byteswapped_copy(buffer + *position, (guint8*)&value,
                         sizeof(gdouble), 1, (1 << sizeof(gdouble)) - 1);
#endif
    *position += sizeof(gdouble);

    gwy_debug("value = <%g>", value);
    return value;
}

/**
 * gwy_deserialize_double_array:
 * @buffer: A memory location containing a serialized gdouble array at
 *          position @position.
 * @size: The size of @buffer.
 * @position: The position of the array in @buffer, it's updated to
 *            point after it.
 * @asize: Where the size of the array is to be returned on success.
 *
 * Deserializes a gdouble array.
 *
 * Returns: The unpacked gdouble array (newly allocated).
 **/
static inline gdouble*
gwy_deserialize_double_array(const guchar *buffer,
                                  gsize size,
                                  gsize *position,
                                  gsize *asize)
{
    gdouble *value;
    gsize newasize;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);

    g_return_val_if_fail(*position + sizeof(gint32) <= size, NULL);
    if (!(newasize = gwy_deserialize_int32(buffer, size, position)))
        return NULL;
    g_return_val_if_fail(*position + newasize*sizeof(gdouble) <= size, NULL);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    value = g_memdup(buffer + *position, newasize*sizeof(gdouble));
#else
    value = g_new(gdouble, newasize);
    gwy_byteswapped_copy(buffer + *position, (guint8*)value,
                         sizeof(gdouble), newasize, (1 << sizeof(gdouble)) - 1);
#endif
    *position += newasize*sizeof(gdouble);
    *asize = newasize;

    gwy_debug("|value| = %" G_GSIZE_FORMAT, newasize);
    return value;
}

/**
 * gwy_deserialize_string:
 * @buffer: A memory location containing a serialized nul-terminated string at
 *          position @position.
 * @size: The size of @buffer.
 * @position: The position of the string in @buffer, it's updated to
 *            point after it.
 *
 * Deserializes a one nul-terminated string.
 *
 * Returns: A newly allocated, nul-terminated string.
 **/
static inline guchar*
gwy_deserialize_string(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    guchar *value;
    const guchar *p;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position < size, NULL);
    p = memchr(buffer + *position, 0, size - *position);
    g_return_val_if_fail(p, NULL);
    value = g_strdup(buffer + *position);
    *position += (p - buffer) - *position + 1;

    gwy_debug("value = <%s>", value);
    return value;
}

/**
 * gwy_deserialize_string_array:
 * @buffer: A memory location containing an array of serialized nul-terminated
 *          string at position @position.
 * @size: The size of @buffer.
 * @position: The position of the string array in @buffer, it's updated to
 *            after it.
 * @asize: Where the size of the array is to be returned on success.
 *
 * Deserializes a string array.
 *
 * Returns: A newly allocated array of nul-terminated strings.
 **/
static inline guchar**
gwy_deserialize_string_array(const guchar *buffer,
                             gsize size,
                             gsize *position,
                             gsize *asize)
{
    guchar **value;
    gsize newasize, j;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);

    g_return_val_if_fail(*position + sizeof(gint32) <= size, NULL);
    if (!(newasize = gwy_deserialize_int32(buffer, size, position)))
        return NULL;
    g_return_val_if_fail(*position + newasize*sizeof(guchar) <= size, NULL);
    value = g_new(guchar*, newasize);
    for (j = 0; j < newasize; j++) {
        value[j] = gwy_deserialize_string(buffer, size, position);
        if (!value[j]) {
            while (j) {
                j--;
                g_free(value[j]);
            }
            g_free(value);
            return NULL;
        }
    }
    *asize = newasize;

    gwy_debug("|value| = %" G_GSIZE_FORMAT, newasize);
    return value;
}

/**
 * gwy_deserialize_object_array:
 * @buffer: A memory location containing an array of serialized object at
 *          position @position.
 * @size: The size of @buffer.
 * @position: The position of the object array in @buffer, it's updated to
 *            after it.
 * @asize: Where the size of the array is to be returned on success.
 *
 * Deserializes an object array.
 *
 * Returns: A newly allocated array of objects.
 **/
static inline GObject**
gwy_deserialize_object_array(const guchar *buffer,
                             gsize size,
                             gsize *position,
                             gsize *asize)
{
    GObject **value;
    gsize j, newasize, minsize;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);

    g_return_val_if_fail(*position + sizeof(gint32) <= size, NULL);
    minsize = 2*sizeof(guchar) + sizeof(gint32);  /* Size of empty object */
    if (!(newasize = gwy_deserialize_int32(buffer, size, position)))
        return NULL;
    g_return_val_if_fail(*position + newasize*minsize <= size, NULL);
    value = g_new(GObject*, newasize);
    for (j = 0; j < newasize; j++) {
        value[j] = gwy_serializable_deserialize(buffer, size, position);
        if (!value[j]) {
            while (j) {
                j--;
                g_object_unref(value[j]);
            }
            g_free(value);
            return NULL;
        }
    }
    *asize = newasize;

    gwy_debug("|value| = %" G_GSIZE_FORMAT, newasize);
    return value;
}

/**
 * gwy_deserialize_spec_value:
 * @buffer: A memory location containing a serialized item value.
 * @size: The size of @buffer.
 * @position: Current position in buffer, will be updated to point after item.
 * @sp: A single serialize spec with @ctype and @name already deserialized,
 *      its @value and @array_size will be filled.
 *
 * Unpacks one serialized item value.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
static gboolean
gwy_deserialize_spec_value(const guchar *buffer,
                           gsize size,
                           gsize *position,
                           GwySerializeSpec *sp)
{
    gpointer p;
    guint32 *a;

    p = sp->value;
    a = sp->array_size;

    switch (sp->ctype) {
        case 'o': {
            GObject *val, *old = *(GObject**)p;

            val = gwy_serializable_deserialize(buffer, size, position);
            if (val) {
                *(GObject**)p = val;
                gwy_object_unref(old);
            }
        }
        break;

        case 'b':
        *(gboolean*)p = gwy_deserialize_boolean(buffer, size, position);
        break;

        case 'c':
        *(guchar*)p = gwy_deserialize_char(buffer, size, position);
        break;

        case 'i':
        *(gint32*)p = gwy_deserialize_int32(buffer, size, position);
        break;

        case 'q':
        *(gint64*)p = gwy_deserialize_int64(buffer, size, position);
        break;

        case 'd':
        *(gdouble*)p = gwy_deserialize_double(buffer, size, position);
        break;

        case 's': {
            guchar *val, *old = *(guchar**)p;

            val = gwy_deserialize_string(buffer, size, position);
            if (val) {
                *(guchar**)p = val;
                g_free(old);
            }
        }
        break;

        case 'C': {
            guchar *val, *old = *(guchar**)p;
            gsize len;

            val = gwy_deserialize_char_array(buffer, size,
                                                  position, &len);
            if (val) {
                *a = len;
                *(guchar**)p = val;
                g_free(old);
            }
        }
        break;

        case 'I': {
            guint32 *val, *old = *(guint32**)p;
            gsize len;

            val = gwy_deserialize_int32_array(buffer, size,
                                                   position, &len);
            if (val) {
                *a = len;
                *(guint32**)p = val;
                g_free(old);
            }
        }
        break;

        case 'Q': {
            guint64 *val, *old = *(guint64**)p;
            gsize len;

            val = gwy_deserialize_int64_array(buffer, size,
                                                   position, &len);
            if (val) {
                *a = len;
                *(guint64**)p = val;
                g_free(old);
            }
        }
        break;

        case 'D': {
            gdouble *val, *old = *(gdouble**)p;
            gsize len;

            val = gwy_deserialize_double_array(buffer, size,
                                                    position, &len);
            if (val) {
                *a = len;
                *(gdouble**)p = val;
                g_free(old);
            }
        }
        break;

        case 'S': {
            guchar **val, **old = *(guchar***)p;
            gsize len, j, oldlen = *a;

            val = gwy_deserialize_string_array(buffer, size, position, &len);
            if (val) {
                *a = len;
                *(guchar***)p = val;
                if (old) {
                    for (j = 0; j < oldlen; j++)
                        g_free(old[j]);
                    g_free(old);
                }
            }
        }
        break;

        case 'O': {
            GObject **val, **old = *(GObject***)p;
            gsize len, j, oldlen = *a;

            val = gwy_deserialize_object_array(buffer, size, position, &len);
            if (val) {
                *a = len;
                *(GObject***)p = val;
                if (old) {
                    for (j = 0; j < oldlen; j++)
                        gwy_object_unref(old[j]);
                    g_free(old);
                }
            }
        }
        break;

        default:
        g_critical("Type <%c> of <%s> is unknown (though known to caller?!)",
                   sp->ctype, sp->name);
        return FALSE;
        break;
    }

    return TRUE;
}

/**
 * gwy_serialize_check_string:
 * @buffer: A memory location containing a nul-terminated string at position
 *          @position.
 * @size: The size of @buffer.
 * @position: The position of the string in @buffer.
 * @compare_to: String to compare @buffer to, or %NULL.
 *
 * Check whether @size bytes of memory in @buffer can be interpreted as a
 * nul-terminated string, and eventually whether it's equal to @compare_to.
 *
 * When @compare_to is %NULL, the comparsion is not performed.
 *
 * Returns: The length of the nul-terminated string including the nul
 * character; zero otherwise.
 **/
gsize
gwy_serialize_check_string(const guchar *buffer,
                           gsize size,
                           gsize position,
                           const guchar *compare_to)
{
    const guchar *p;

    gwy_debug("<%s> buf = %p, size = %" G_GSIZE_FORMAT ", pos = %"
              G_GSIZE_FORMAT,
              compare_to ? compare_to : (const guchar*)"(null)",
              buffer, size, position);
    g_assert(buffer);
    g_assert(size > 0);
    g_return_val_if_fail(position < size, 0);
    p = (guchar*)memchr(buffer + position, 0, size - position);
    if (!p || (compare_to && !gwy_strequal(buffer + position, compare_to)))
        return 0;

    return (p - buffer) + 1 - position;
}

/************************** Documentation ****************************/

/**
 * GwySerializeFunc:
 * @serializable: An object to serialize.
 * @buffer: A buffer to append the representation to, may be %NULL indicating
 *          a new one should be allocated.
 *
 * The type of serialization method, see gwy_serializable_serialize() for
 * description.
 *
 * Returns: @buffer with serialized object appended.
 */

/**
 * GwyDeserializeFunc:
 * @buffer: A buffer containing a serialized object.
 * @size: The size of @buffer.
 * @position: The current position in @buffer.
 *
 * The type of deserialization method, see gwy_serializable_deserialize() for
 * description.
 *
 * Returns: A newly created (restored) object.
 */

/**
 * GwySerializeSpec:
 * @ctype: Component type, see description body for possible values.
 * @name: Component name as a nul-terminated string.
 * @value: Pointer to component (always add one level of indirection; for
 *         an object, a #GObject** pointer should be stored).  If it is
 *         %NULL, the component is ignored.  For serialization it means it
 *         is not serialized, for deserialization it means its existence
 *         is acknowledged (no unknown component warning) but it's skipped
 *         instead of unpacking.
 * @array_size: Pointer to array size if component is an array, %NULL
 *              otherwise.
 *
 * A structure containing information for one object/struct component
 * serialization or deserialization.
 *
 * This component information is used in gwy_serialize_pack_object_struct()
 * and gwy_serialize_unpack_object_struct() suitable for (de)serialization
 * of struct-like objects.
 *
 * Following atomic component types (@ctype's) exist:
 * <literal>'b'</literal> for a boolean,
 * <literal>'c'</literal> for a character,
 * <literal>'i'</literal> for a 32bit integer,
 * <literal>'q'</literal> for a 64bit integer,
 * <literal>'d'</literal> for a gdouble,
 * <literal>'s'</literal> for a nul-terminated string,
 * <literal>'o'</literal> for a serializable object.
 *
 * And array component types:
 * <literal>'C'</literal> for a character array,
 * <literal>'I'</literal> for a 32bit integer array,
 * <literal>'Q'</literal> for a 64bit integer array,
 * <literal>'D'</literal> for a gdouble array,
 * <literal>'S'</literal> for an array of nul-terminated strings,
 * <literal>'O'</literal> for an array of objects.
 **/

/**
 * GwySerializeItem:
 * @ctype: Component type, see #GwySerializeSpec for details.
 * @name: Component name as a nul-terminated string.
 * @value: Component value.
 * @array_size: Array size if component is an array, unused otherwise.
 *
 * A structure containing information for one object/struct component
 * serialization or deserialization.
 *
 * This component information is used in gwy_serialize_object_items() and
 * gwy_deserialize_object_hash() suitable for (de)serialization of hash-like
 * objects.
 **/

/**
 * GwySerializableIface:
 * @serialize: Serialization method (obligatory), see #GwySerializeFunc for
 *             description.
 * @deserialize: Restore method (obligatory), see #GwyDeserializeFunc for
 *               description.
 * @get_size: Serialized size calculation method (optional).
 *            Calculates expected serialized object size (including object
 *            name and size header).
 *            Its purpose is to avoid frequent memory reallocations during
 *            serialization of large objects.
 *            The returned value may not be exact, it can be an upper bound.
 * @clone: Clone method (obligatory).  Copies complete object `value' to an
 *         existing object of the same type.  This method is called from
 *         copy's class if source and copy classes differ.
 * @duplicate: Duplication method (optional).  Creates a duplicate of an
 *             object.
 *
 * The methods a serializable objects has to implement.
 **/

/**
 * GWY_IMPLEMENT_SERIALIZABLE:
 * @iface_init: The interface init function.
 *
 * Specialization of G_IMPLEMENT_INTERFACE() for #GwySerializableIface.
 *
 * To be used in last G_DEFINE_TYPE_EXTENDED() argument:
 * <informalexample><programlisting>
 * G_DEFINE_TYPE_EXTENDED
 *    (GwyFoo, gwy_foo, G_TYPE_OBJECT, 0,
 *     GWY_IMPLEMENT_SERIALIZABLE(gwy_foo_serializable_init))
 * </programlisting></informalexample>
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
