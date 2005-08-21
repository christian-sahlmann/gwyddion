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

/**
 * gwy_enum_sanitize_value:
 * @enumval: An enum value.
 * @enum_type: #GType of a registered enum type.
 *
 * Makes sure an enum value is valid.
 *
 * Returns: Either @enumval itself if it's valid, or some valid enum value.
 *          When @enumval is invalid and larger than all valid values the
 *          largest valid value is returned. Likewise if it's smaller the
 *          smallest valid value is returned.  If it's in range but invalid,
 *          the first enum value is returned.
 **/
gint
gwy_enum_sanitize_value(gint enumval,
                        GType enum_type)
{
    GEnumClass *klass;

    klass = G_ENUM_CLASS(g_type_class_ref(enum_type));
    g_return_val_if_fail(klass, enumval);
    if (enumval <= klass->minimum)
        enumval = klass->minimum;
    else if (enumval >= klass->maximum)
        enumval = klass->maximum;
    else {
        guint i;

        for (i = 0; i < klass->n_values; i++) {
            if (enumval == klass->values[i].value)
                break;
        }
        if (i == klass->n_values)
            enumval = klass->values[0].value;
    }
    g_type_class_unref(klass);

    return enumval;
}

/**
 * gwy_enum_freev:
 * @enum_table: A %NULL-name-terminated, dynamically allocated enum table.
 *
 * Frees a dynamically allocated enum.
 *
 * More precisely, it frees all names of a #GwyEnum and then frees the enum
 * itself.
 **/
void
gwy_enum_freev(GwyEnum *enum_table)
{
    gsize i = 0;

    while (enum_table[i].name) {
        g_free((gpointer)enum_table[i].name);
        i++;
    }
    g_free(enum_table);
}

static const gchar*
gwy_enum_get_name(gpointer item)
{
    return ((const GwyEnum*)item)->name;
}

static const GType*
gwy_enum_get_traits(gint *ntraits)
{
    static const GType traits[] = { G_TYPE_STRING, G_TYPE_INT };

    if (ntraits)
        *ntraits = G_N_ELEMENTS(traits);

    return traits;
}

static const gchar*
gwy_enum_get_trait_name(gint i)
{
    static const gchar *trait_names[] = { "name", "value" };

    g_return_val_if_fail(i >= 0 && i < G_N_ELEMENTS(trait_names), NULL);
    return trait_names[i];
}

static void
gwy_enum_get_trait_value(gpointer item,
                         gint i,
                         GValue *value)
{
    switch (i) {
        case 0:
        g_value_init(value, G_TYPE_STRING);
        g_value_set_static_string(value, ((const GwyEnum*)item)->name);
        break;

        case 1:
        g_value_init(value, G_TYPE_INT);
        g_value_set_int(value,  ((const GwyEnum*)item)->value);
        break;

        default:
        g_return_if_reached();
        break;
    }
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
    static GwyInventoryItemType gwy_enum_item_type = {
        0,    /* type must be initialized run-time */
        NULL,
        NULL,
        gwy_enum_get_name,
        NULL,
        NULL,
        NULL,
        NULL,
        gwy_enum_get_traits,
        gwy_enum_get_trait_name,
        gwy_enum_get_trait_value,
    };

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
