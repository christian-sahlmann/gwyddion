/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* FIXME: we should use a better backend and also add more methods, for the
 * original reason this was implemented they were not necessary */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwystringlist.h>

#define GWY_STRING_LIST_TYPE_NAME "GwyStringList"

enum {
    VALUE_CHANGED,
    LAST_SIGNAL
};

static void        gwy_string_list_finalize       (GObject *object);
static void      gwy_string_list_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_string_list_serialize      (GObject *obj,
                                                   GByteArray *buffer);
static gsize       gwy_string_list_get_size       (GObject *obj);
static GObject*    gwy_string_list_deserialize    (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject*    gwy_string_list_duplicate_real (GObject *object);
static void        gwy_string_list_clone_real     (GObject *source,
                                                   GObject *copy);

static guint string_list_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwyStringList, gwy_string_list, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_string_list_serializable_init))

static void
gwy_string_list_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_string_list_serialize;
    iface->deserialize = gwy_string_list_deserialize;
    iface->get_size = gwy_string_list_get_size;
    iface->duplicate = gwy_string_list_duplicate_real;
    iface->clone = gwy_string_list_clone_real;
}

static void
gwy_string_list_class_init(GwyStringListClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_string_list_finalize;

    /**
     * GwyStringList::value-changed:
     * @gwystrlist: The #GwyStringList which received the signal.
     *
     * The ::value-changed signal is emitted whenever a string list changes.
     */
    string_list_signals[VALUE_CHANGED]
        = g_signal_new("value-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyStringListClass, value_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_string_list_init(GwyStringList *strlist)
{
    gwy_debug_objects_creation((GObject*)strlist);

    strlist->strings = (gpointer)g_ptr_array_new();
}

static void
gwy_string_list_finalize(GObject *object)
{
    GwyStringList *strlist = (GwyStringList*)object;
    GPtrArray *strings;

    strings = (GPtrArray*)strlist->strings;
    if (strings) {
        g_ptr_array_free(strings, TRUE);
        strlist->strings = NULL;
    }

    G_OBJECT_CLASS(gwy_string_list_parent_class)->finalize(object);
}

static GByteArray*
gwy_string_list_serialize(GObject *obj,
                      GByteArray *buffer)
{
    GwyStringList *strlist;
    GByteArray *retval;
    GPtrArray *strings;

    g_return_val_if_fail(GWY_IS_STRING_LIST(obj), NULL);

    strlist = GWY_STRING_LIST(obj);
    strings = (GPtrArray*)strlist->strings;
    {
        guint32 len = strings->len;
        GwySerializeSpec spec[] = {
            { 'S', "strings", &strings->pdata, &len, },
        };
        retval = gwy_serialize_pack_object_struct(buffer,
                                                  GWY_STRING_LIST_TYPE_NAME,
                                                  G_N_ELEMENTS(spec), spec);
        return retval;
    }
}

static gsize
gwy_string_list_get_size(GObject *obj)
{
    GwyStringList *strlist;
    GPtrArray *strings;
    gsize size;

    g_return_val_if_fail(GWY_IS_STRING_LIST(obj), 0);

    strlist = GWY_STRING_LIST(obj);
    strings = (GPtrArray*)strlist->strings;
    {
        guint32 len = strings->len;
        GwySerializeSpec spec[] = {
            { 'S', "strings", &strings->pdata, &len, },
        };
        size = gwy_serialize_get_struct_size(GWY_STRING_LIST_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
    }

    return size;
}

static GObject*
gwy_string_list_deserialize(const guchar *buffer,
                        gsize size,
                        gsize *position)
{
    gchar **pstr = NULL;
    guint32 len = 0;
    GwySerializeSpec spec[] = {
        { 'S', "strings", &pstr, &len, },
    };
    GwyStringList *strlist;
    GPtrArray *strings;
    guint i;

    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_STRING_LIST_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        return NULL;
    }

    strlist = gwy_string_list_new();
    strings = (GPtrArray*)strlist->strings;
    g_ptr_array_set_size(strings, len);
    for (i = 0; i < len; i++)
        g_ptr_array_index(strings, i) = pstr[i];
    g_free(pstr);

    return (GObject*)strlist;
}


static GObject*
gwy_string_list_duplicate_real(GObject *object)
{
    GwyStringList *strlist, *duplicate;
    GPtrArray *strings, *dupstrings;
    guint i;

    g_return_val_if_fail(GWY_IS_STRING_LIST(object), NULL);
    strlist = GWY_STRING_LIST(object);
    duplicate = gwy_string_list_new();

    strings = (GPtrArray*)strlist->strings;
    dupstrings = (GPtrArray*)duplicate->strings;
    g_ptr_array_set_size(dupstrings, strings->len);
    for (i = 0; i < strings->len; i++)
        g_ptr_array_index(dupstrings, i)
            = g_strdup(g_ptr_array_index(strings, i));

    return (GObject*)duplicate;
}

static void
gwy_string_list_clone_real(GObject *source, GObject *copy)
{
    GwyStringList *strlist, *clone;
    GPtrArray *strings, *copystrings;
    guint i;

    g_return_if_fail(GWY_IS_STRING_LIST(source));
    g_return_if_fail(GWY_IS_STRING_LIST(copy));

    strlist = GWY_STRING_LIST(source);
    clone = GWY_STRING_LIST(copy);

    strings = (GPtrArray*)strlist->strings;
    copystrings = (GPtrArray*)clone->strings;
    for (i = 0; i < copystrings->len; i++)
        g_free(g_ptr_array_index(copystrings, i));
    g_ptr_array_set_size(copystrings, strings->len);
    for (i = 0; i < strings->len; i++)
        g_ptr_array_index(copystrings, i)
            = g_strdup(g_ptr_array_index(strings, i));

    g_signal_emit(copy, string_list_signals[VALUE_CHANGED], 0);
}

/**
 * gwy_string_list_new:
 *
 * Creates a new string list.
 *
 * Returns: A new empty string list.
 **/
GwyStringList*
gwy_string_list_new(void)
{
    return g_object_new(GWY_TYPE_STRING_LIST, NULL);
}

/**
 * gwy_string_list_append:
 * @strlist: A string list.
 * @string: A string to add.
 *
 * Appends a string to the end of a string list.
 **/
void
gwy_string_list_append(GwyStringList *strlist,
                       const gchar *string)
{
    GPtrArray *strings;

    g_return_if_fail(GWY_IS_STRING_LIST(strlist));
    g_return_if_fail(string);

    strings = (GPtrArray*)strlist->strings;
    g_ptr_array_add(strings, g_strdup(string));

    g_signal_emit(strlist, string_list_signals[VALUE_CHANGED], 0);
}

/**
 * gwy_string_list_get_length:
 * @strlist: A string list.
 *
 * Gets the number of strings in a string list.
 *
 * Returns: The number of strings in @strlist.
 **/
guint
gwy_string_list_get_length(GwyStringList *strlist)
{
    GPtrArray *strings;

    g_return_val_if_fail(GWY_IS_STRING_LIST(strlist), 0);

    strings = (GPtrArray*)strlist->strings;
    return strings->len;
}

/**
 * gwy_string_list_get:
 * @strlist: A string list.
 * @i: The position of string to get.
 *
 * Gets a string from a string list by position.
 *
 * Returns: The string, owned by @strlist.  It is valid only until @strlist
 *          changes.
 **/
const gchar*
gwy_string_list_get(GwyStringList *strlist,
                    guint i)
{
    GPtrArray *strings;

    g_return_val_if_fail(GWY_IS_STRING_LIST(strlist), NULL);
    strings = (GPtrArray*)strlist->strings;
    g_return_val_if_fail(i < strings->len, NULL);

    return g_ptr_array_index(strings, i);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwystrlist
 * @title: GwyStringList
 * @short_description: A string list object wrapper
 *
 * #GwyStringList object represents a reference-counted, serializable list of
 * strings.  The current interface is very minimal, more methods may be added
 * later as needed.
 **/

/**
 * GwyStringList:
 *
 * The #GwyStringList struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * gwy_string_list_duplicate:
 * @strlist: A string list to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
