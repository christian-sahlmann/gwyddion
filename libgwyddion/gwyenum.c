/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
#include <stdlib.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyenum.h>

/* The enum and flags stuff duplicates GLib functionality.
 * However the GLib stuff requires enum class registration for each enum and
 * thus is hardly usable for ad-hoc stuff */

GType
gwy_enum_get_type(void)
{
    static GType enum_type = 0;

    if (G_UNLIKELY(!enum_type))
        enum_type = g_pointer_type_register_static("GwyEnum");

    return enum_type;
}

/**
 * gwy_string_to_enum:
 * @str: A string containing one of @enum_table string values.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 *
 * Creates an integer representation of a string enum value @str.
 *
 * Returns: The integer enum value (NOT index in the table), or -1 if @str
 *          was not found.
 **/
gint
gwy_string_to_enum(const gchar *str,
                   const GwyEnum *enum_table,
                   gint n)
{
    gint j;

    for (j = n; j && enum_table->name; j--, enum_table++) {
        if (gwy_strequal(str, enum_table->name))
            return enum_table->value;
    }

    return -1;
}

/**
 * gwy_enum_to_string:
 * @enumval: A one integer value from @enum_table.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 *
 * Creates a string representation of an integer enum value @enumval.
 *
 * Returns: The name as a string from @enum_table, thus it generally should
 *          not be modified or freed, unless @enum_table is supposed to be
 *          modified too. If the value is not found, an empty string is
 *          returned.
 **/
G_CONST_RETURN gchar*
gwy_enum_to_string(gint enumval,
                   const GwyEnum *enum_table,
                   gint n)
{
    gint j;

    for (j = n; j && enum_table->name; j--, enum_table++) {
        if (enumval == enum_table->value)
            return enum_table->name;
    }

    return "";
}

/**
 * gwy_string_to_flags:
 * @str: A string containing one of @enum_table string values.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 * @delimiter: A delimiter to split @str on, when #NULL space is used.
 *
 * Creates an integer flag combination of its string representation @str.
 *
 * Returns: All the flags present in @str, bitwise ORer.
 **/
gint
gwy_string_to_flags(const gchar *str,
                    const GwyEnum *enum_table,
                    gint n,
                    const gchar *delimiter)
{
    gchar **strings;
    gint i, j, enumval;

    strings = g_strsplit(str, delimiter ? delimiter : " ", 0);
    if (!strings)
        return 0;

    enumval = 0;
    for (i = 0; strings[i]; i++) {
        const GwyEnum *e = enum_table;

        for (j = n; j && e->name; j--, e++) {
            if (gwy_strequal(strings[i], e->name)) {
                enumval |= e->value;
                break;
            }
        }
    }
    g_strfreev(strings);

    return enumval;
}

/**
 * gwy_flags_to_string:
 * @enumval: Some ORed integer flags from @enum_table.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 * @glue: A glue to join string values with, when #NULL space is used.
 *
 * Creates a string representation of integer flag combination @enumval.
 *
 * Returns: The string representation as a newly allocated string.  It should
 *          be freed when no longer used.
 **/
gchar*
gwy_flags_to_string(gint enumval,
                    const GwyEnum *enum_table,
                    gint n,
                    const gchar *glue)
{
    gint j;
    GString *str = NULL;
    gchar *result;

    if (!enumval)
        return "";

    if (!glue)
        glue = " ";

    for (j = n; j && enum_table->name; j--, enum_table++) {
        if (enumval & enum_table->value) {
            if (!str)
                str = g_string_new(enum_table->name);
            else {
                str = g_string_append(str, glue);
                str = g_string_append(str, enum_table->name);
            }
        }
    }
    result = str->str;
    g_string_free(str, FALSE);

    return result;
}

static const gchar*
gwy_enum_get_name(gpointer item)
{
    return ((const GwyEnum*)item)->name;
}

static gboolean
gwy_enum_is_const(G_GNUC_UNUSED gconstpointer item)
{
    return TRUE;
}

static const GType*
gwy_enum_get_traits(gint *n)
{
    static const GType traits[] = { G_TYPE_STRING };

    *n = G_N_ELEMENTS(traits);

    return traits;
}

static void
gwy_enum_get_trait_value(gpointer item,
                         gint i,
                         GValue *value)
{
    g_return_if_fail(i != 0);

    g_value_init(value, G_TYPE_STRING);
    g_value_set_static_string(value, gettext(((const GwyEnum*)item)->name));
}

/**
 * gwy_enum_inventory_new:
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a NULL name.
 *
 * Convenience function to create a constant inventory from a #GwyEnum.
 *
 * Returns: The newly created constant inventory.
 **/
GwyInventory*
gwy_enum_inventory_new(const GwyEnum *enum_table,
                       gint n)
{
    GwyInventoryItemType gwy_enum_item_type = {
        GWY_TYPE_ENUM,
        NULL,
        gwy_enum_is_const,
        gwy_enum_get_name,
        NULL,
        NULL,
        gwy_enum_get_traits,
        NULL,
        gwy_enum_get_trait_value,
    };

    gwy_enum_item_type.type = GWY_TYPE_ENUM;
    if (n == -1) {
        for (n = 0; enum_table[n].name; n++)
            ;
    }
    return gwy_inventory_new_from_array(&gwy_enum_item_type, sizeof(GwyEnum),
                                        n, enum_table);
}

/************************** Documentation ****************************/

/**
 * GwyEnum:
 * @name: Value name.
 * @value: The (integer) enum value.
 *
 * Enumerated type with named values.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
