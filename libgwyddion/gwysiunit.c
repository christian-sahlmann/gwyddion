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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwysiunit.h>

#define GWY_SI_UNIT_TYPE_NAME "GwySIUnit"

typedef struct {
    gdouble prefix_affinity;
    gint base_power10;
} GwyUnitTraits;

typedef struct {
    GQuark unit;
    gshort power;
    gshort traits;
} GwySimpleUnit;

typedef struct {
    gint power10;
    GArray *units;
} GwySIUnit2;

typedef struct {
    const gchar *power10_prefix;
    const gchar *power_prefix;
    const gchar *power_suffix;
    const gchar *unit_times;
    const gchar *unit_division;
    const gchar *power_unit_separator;
} GwySIFormatStyle;

static GwySIUnit2* gwy_si_unit2_parse         (const gchar *string);
static GwySIUnit2* gwy_si_unit2_power_multiply(GwySIUnit2 *siunit1,
                                               gint power1,
                                               GwySIUnit2 *siunit2,
                                               gint power2);
static GwySIUnit2* gwy_si_unit2_power         (GwySIUnit2 *siunit1,
                                               gint power1);
static void        gwy_si_unit2_free          (GwySIUnit2* siunit);
static GwySIUnit2* gwy_si_unit2_canonicalize  (GwySIUnit2 *siunit);
const gchar*       gwy_si_unit2_prefix        (gint power);
static gchar*  gwy_si_unit2_format_as_plain_string(GwySIUnit2 *siunit,
                                                   const GwySIFormatStyle *fs);
static GString*    gwy_si_unit2_format        (GwySIUnit2 *siunit,
                                               const GwySIFormatStyle *fs);


static void        gwy_si_unit_class_init        (GwySIUnitClass *klass);
static void        gwy_si_unit_init              (GwySIUnit *si_unit);
static void        gwy_si_unit_finalize          (GObject *object);
static void        gwy_si_unit_serializable_init (GwySerializableIface *iface);
static GByteArray* gwy_si_unit_serialize         (GObject *obj,
                                                  GByteArray *buffer);
static GObject*    gwy_si_unit_deserialize       (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
static GObject*    gwy_si_unit_duplicate_real     (GObject *object);


static const GwyUnitTraits unit_traits[] = {
    { 1.0,   0 },    /* normal unit */
    { 1.0,   3 },    /* kilogram */
    { 0.0,   0 },    /* does not take prefixes */
};

static const struct {
    const gchar *prefix;
    gint power10;
}
/* Canonical form must be always first, because this table is used for reverse
 * mapping too */
SI_prefixes[] = {
    { "k",     3  },
    { "c",    -2  },
    { "m",    -3  },
    { "M",     6  },
    { "µ",    -6  },
    /* People are extremely creative when it comes to \mu replacements... */
    { "μ",    -6  },
    { "~",    -6  },
    { "u",    -6  },
    { "\265", -6  },
    { "G",     9  },
    { "n",    -9  },
    { "T",     12 },
    { "p",    -12 },
    { "P",     15 },
    { "f",    -15 },
    { "E",     18 },
    { "a",    -18 },
    { "Z",     21 },
    { "z",    -21 },
    { "Y",     24 },
    { "y",    -24 },
};

/* TODO: silly units we should probably support specially: kg */

/* Units that can conflict with prefixes */
static const gchar *known_units[] = {
    "deg", "Pa", "cd", "mol", "cal", "px", "pt",
};

/* Unit formats */
static const GwySIFormatStyle format_style_plain = {
    "10^", "^", NULL, " ", "/", " "
};
static const GwySIFormatStyle format_style_markup = {
    "10<sup>", "<sup>", "</sup>", " ", "/", " "
};
static const GwySIFormatStyle format_style_vfmarkup = {
    "× 10<sup>", "<sup>", "</sup>", " ", "/", " "
};
static const GwySIFormatStyle format_style_backwoods = {
    "1e", NULL, NULL, " ", "/", " "
};
static const GwySIFormatStyle format_style_TeX = {
    "10^{", "^{", "}", "\\,", "/", "\\,"
};

static GObjectClass *parent_class = NULL;

GType
gwy_si_unit_get_type(void)
{
    static GType gwy_si_unit_type = 0;

    if (!gwy_si_unit_type) {
        static const GTypeInfo gwy_si_unit_info = {
            sizeof(GwySIUnitClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_si_unit_class_init,
            NULL,
            NULL,
            sizeof(GwySIUnit),
            0,
            (GInstanceInitFunc)gwy_si_unit_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_si_unit_serializable_init,
            NULL,
            NULL
        };

        gwy_debug("");
        gwy_si_unit_type = g_type_register_static(G_TYPE_OBJECT,
                                                  GWY_SI_UNIT_TYPE_NAME,
                                                  &gwy_si_unit_info,
                                                  0);
        g_type_add_interface_static(gwy_si_unit_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
    }

    return gwy_si_unit_type;
}

static void
gwy_si_unit_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_si_unit_serialize;
    iface->deserialize = gwy_si_unit_deserialize;
    iface->duplicate = gwy_si_unit_duplicate_real;
}


static void
gwy_si_unit_class_init(GwySIUnitClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_si_unit_finalize;
}

static void
gwy_si_unit_init(GwySIUnit *si_unit)
{
    gwy_debug("");
    gwy_debug_objects_creation((GObject*)si_unit);
    si_unit->unitstr = NULL;
}

static void
gwy_si_unit_finalize(GObject *object)
{
    GwySIUnit *si_unit = (GwySIUnit*)object;

    gwy_debug("");
    gwy_si_unit2_free(g_object_get_data(object, "gwy-si-unit2"));
    g_free(si_unit->unitstr);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static GByteArray*
gwy_si_unit_serialize(GObject *obj,
                      GByteArray *buffer)
{
    GwySIUnit *si_unit;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SI_UNIT(obj), NULL);

    si_unit = GWY_SI_UNIT(obj);
    {
        const gchar *unitstr = si_unit->unitstr ? si_unit->unitstr : "";
        GwySerializeSpec spec[] = {
            { 's', "unitstr", &unitstr, NULL, },
        };
        gwy_debug("unitstr = <%s>", unitstr);
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_SI_UNIT_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_si_unit_deserialize(const guchar *buffer,
                        gsize size,
                        gsize *position)
{
    gchar *unitstr = NULL;
    GwySerializeSpec spec[] = {
        { 's', "unitstr", &unitstr, NULL, },
    };
    GObject *si_unit;

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_SI_UNIT_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        return NULL;
    }

    if (unitstr && !*unitstr) {
        g_free(unitstr);
        unitstr = NULL;
    }
    si_unit = gwy_si_unit_new(unitstr);

    return si_unit;
}


static GObject*
gwy_si_unit_duplicate_real(GObject *object)
{
    GwySIUnit *si_unit;
    GObject *duplicate;

    g_return_val_if_fail(GWY_IS_SI_UNIT(object), NULL);
    si_unit = GWY_SI_UNIT(object);
    duplicate = gwy_si_unit_new(si_unit->unitstr);

    return duplicate;
}

/**
 * gwy_si_unit_new:
 * @unit_string: Unit string.
 *
 * Unit string represents unit with no prefixes
 * (e. g. "m", "N", "A", etc.)
 *
 * Returns: A new #GwySiUnit with a given string.
 **/
GObject*
gwy_si_unit_new(const char *unit_string)
{
    return gwy_si_unit_new_parse(unit_string, NULL);
}

/**
 * gwy_si_unit_new_parse:
 * @unit_string: Unit string.
 * @power10: Where power of 10 should be stored (or %NULL).
 *
 * Creates a new SI unit from string representation.
 *
 * This is a more powerful version of gwy_si_unit_new(): @unit_string may
 * be a relatively complex unit, with prefixes, like "pA/s" or "km^2".
 * Beside conversion to a base SI unit like "A/s" or "m^2" it also computes
 * the power of 10 one has to multiply the base unit with to get an equivalent
 * of @unit_string.
 *
 * For example, for "pA/s" it will store -12 to @power10 because 1 pA/s is
 * 1e-9 A/s, for "km^2" it will store 6 to @power10 because 1 km^2 is 1e6
 * m^2.
 *
 * Returns: A new SI unit.
 *
 * Since: 1.8
 **/
GObject*
gwy_si_unit_new_parse(const char *unit_string,
                      gint *power10)
{
    GwySIUnit *siunit;
    GwySIUnit2 *siunit2;

    gwy_debug("");
    siunit = g_object_new(GWY_TYPE_SI_UNIT, NULL);
    if (unit_string == NULL || strcmp(unit_string, "") == 0)
        siunit->unitstr = NULL;
    else
        siunit->unitstr = g_strdup(unit_string);

    siunit2 = gwy_si_unit2_parse(unit_string);
    g_object_set_data((GObject*)siunit, "gwy-si-unit2", siunit2);
    if (power10)
        *power10 = siunit2->power10;

    return (GObject*)siunit;
}

/**
 * gwy_si_unit_set_unit_string:
 * @siunit: An SI unit.
 * @unit_string: Unit string to be set.
 *
 * Sets string that represents unit.
 *
 * It must be base unit with no prefixes (e. g. "m", "N", "A", etc.).
 **/
void
gwy_si_unit_set_unit_string(GwySIUnit *siunit,
                            const gchar *unit_string)
{
    gwy_si_unit_set_unit_string_parse(siunit, unit_string, NULL);
}

/**
 * gwy_si_unit_set_unit_string_parse:
 * @siunit: An SI unit.
 * @unit_string: Unit string to be set.
 * @power10: Where power of 10 should be stored (or %NULL).
 *
 * Changes an SI unit according to string representation.
 *
 * This is a more powerful version of gwy_si_unit_set_unit_string(), please
 * see gwy_si_unit_new_parse() for some discussion.
 *
 * Since: 1.8
 **/
void
gwy_si_unit_set_unit_string_parse(GwySIUnit *siunit,
                                  const gchar *unit_string,
                                  gint *power10)
{
    GwySIUnit2 *siunit2;

    gwy_debug("");

    g_free(siunit->unitstr);
    siunit->unitstr = g_strdup(unit_string);
    gwy_si_unit2_free(g_object_get_data((GObject*)siunit, "gwy-si-unit2"));
    siunit2 = gwy_si_unit2_parse(unit_string);
    g_object_set_data((GObject*)siunit, "gwy-si-unit2", siunit2);
    if (power10)
        *power10 = siunit2->power10;
}

/**
 * gwy_si_unit_get_unit_string:
 * @siunit: An SI unit.
 *
 * Obtains string representing a SI unit.
 *
 * Returns: String that represents base unit (with no prefixes).
 **/
gchar*
gwy_si_unit_get_unit_string(GwySIUnit *siunit)
{
    gwy_debug("");

    return g_strdup(siunit->unitstr);
}

/**
 * gwy_si_unit_multiply:
 * @siunit1: An SI unit.
 * @siunit2: An SI unit.
 * @result:  An SI unit to set to product of @siunit1 and @siunit2.  It can be
 *           one of @siunit1, @siunit2.
 *
 * Multiplies two SI units.
 *
 * Returns: @result, for convenience.
 *
 * Since: 1.8
 **/
GwySIUnit*
gwy_si_unit_multiply(GwySIUnit *siunit1,
                     GwySIUnit *siunit2,
                     GwySIUnit *result)
{
    GwySIUnit2 *siunit12, *siunit22, *result2, *oldresult2;

    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit1), NULL);
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit2), NULL);
    g_return_val_if_fail(GWY_IS_SI_UNIT(result), NULL);

    siunit12 = (GwySIUnit2*)g_object_get_data((GObject*)siunit1,
                                              "gwy-si-unit2");
    siunit22 = (GwySIUnit2*)g_object_get_data((GObject*)siunit2,
                                              "gwy-si-unit2");
    result2 = (GwySIUnit2*)g_object_get_data((GObject*)result,
                                              "gwy-si-unit2");
    oldresult2 = result2;
    g_free(result->unitstr);

    result2 = gwy_si_unit2_power_multiply(siunit12, 1, siunit22, 1);
    gwy_si_unit2_free(oldresult2);
    result2->power10 = 0;
    result->unitstr = gwy_si_unit2_format_as_plain_string(result2,
                                                          &format_style_plain);
    g_object_set_data((GObject*)result, "gwy-si-unit2", result2);

    return result;
}

/**
 * gwy_si_unit_divide:
 * @siunit1: An SI unit.
 * @siunit2: An SI unit.
 * @result:  An SI unit to set to quotient of @siunit1 and @siunit2.  It can be
 *           one of @siunit1, @siunit2.
 *
 * Divides two SI units.
 *
 * Returns: @result, for convenience.
 *
 * Since: 1.8
 **/
GwySIUnit*
gwy_si_unit_divide(GwySIUnit *siunit1,
                   GwySIUnit *siunit2,
                   GwySIUnit *result)
{
    GwySIUnit2 *siunit12, *siunit22, *result2, *oldresult2;

    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit1), NULL);
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit2), NULL);
    g_return_val_if_fail(GWY_IS_SI_UNIT(result), NULL);

    siunit12 = (GwySIUnit2*)g_object_get_data((GObject*)siunit1,
                                              "gwy-si-unit2");
    siunit22 = (GwySIUnit2*)g_object_get_data((GObject*)siunit2,
                                              "gwy-si-unit2");
    result2 = (GwySIUnit2*)g_object_get_data((GObject*)result,
                                              "gwy-si-unit2");
    oldresult2 = result2;
    g_free(result->unitstr);

    result2 = gwy_si_unit2_power_multiply(siunit12, 1, siunit22, -1);
    gwy_si_unit2_free(oldresult2);
    result2->power10 = 0;
    result->unitstr = gwy_si_unit2_format_as_plain_string(result2,
                                                          &format_style_plain);
    g_object_set_data((GObject*)result, "gwy-si-unit2", result2);

    return result;
}

/**
 * gwy_si_unit_power:
 * @siunit: An SI unit.
 * @power: Power to power @siunit to.
 * @result:  An SI unit to set to power of @siunit.  It can be @siunit itself.
 *
 * Computes a power of an SI unit.
 *
 * Returns: @result, for convenience.
 *
 * Since: 1.8
 **/
GwySIUnit*
gwy_si_unit_power(GwySIUnit *siunit,
                  gint power,
                  GwySIUnit *result)
{
    GwySIUnit2 *siunit2, *result2, *oldresult2;

    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit), NULL);
    g_return_val_if_fail(GWY_IS_SI_UNIT(result), NULL);

    siunit2 = (GwySIUnit2*)g_object_get_data((GObject*)siunit,
                                             "gwy-si-unit2");
    result2 = (GwySIUnit2*)g_object_get_data((GObject*)result,
                                              "gwy-si-unit2");
    oldresult2 = result2;
    g_free(result->unitstr);

    result2 = gwy_si_unit2_power(siunit2, power);
    gwy_si_unit2_free(oldresult2);
    result2->power10 = 0;
    result->unitstr = gwy_si_unit2_format_as_plain_string(result2,
                                                          &format_style_plain);
    g_object_set_data((GObject*)result, "gwy-si-unit2", result2);

    return result;
}

/**
 * gwy_si_unit_get_format:
 * @siunit: GwySiUnit
 * @value: input value
 * @format: returned number representation parameters
 *
 * Finds reasonable representation for a number.
 * This means that number @value should
 * be written as @value / @format->magnitude [@format->units].
 *
 * Returns: The value format.  If @format was %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_si_unit_get_format(GwySIUnit *siunit,
                       gdouble value,
                       GwySIValueFormat *format)
{
    GwySIUnit2 *siunit2;

    gwy_debug("");
    if (!format)
        format = (GwySIValueFormat*)g_new0(GwySIValueFormat, 1);
    else
        g_free(format->units);

    siunit2 = (GwySIUnit2*)g_object_get_data((GObject*)siunit, "gwy-si-unit2");
    g_assert(siunit2);

    value = fabs(value);
    if (!value) {
        format->magnitude = 1;
        format->precision = 2;
    }
    else
        format->magnitude = gwy_math_humanize_numbers(value/120, value,
                                                      &format->precision);
    siunit2->power10 = ROUND(log(format->magnitude)/G_LN10);
    format->units = gwy_si_unit2_format_as_plain_string(siunit2,
                                                        &format_style_vfmarkup);

    return format;
}


/**
 * gwy_si_unit_get_format_with_resolution:
 * @siunit: A SI unit.
 * @maximum: The maximum value to be represented.
 * @resolution: The smallest step (approximately) that should make a visible
 *              difference in the representation.
 * @format: A value format to set-up, may be %NULL, a new value format is
 *          allocated then.
 *
 * Finds a good format for representing a range of values with given resolution.
 *
 * The values should be then printed as value/@format->magnitude
 * [@format->units] with @format->precision decimal places.
 *
 * Returns: The value format.  If @format was %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_si_unit_get_format_with_resolution(GwySIUnit *siunit,
                                       gdouble maximum,
                                       gdouble resolution,
                                       GwySIValueFormat *format)
{
    GwySIUnit2 *siunit2;

    gwy_debug("");
    if (!format)
        format = (GwySIValueFormat*)g_new0(GwySIValueFormat, 1);
    else
        g_free(format->units);

    siunit2 = (GwySIUnit2*)g_object_get_data((GObject*)siunit, "gwy-si-unit2");
    g_assert(siunit2);

    maximum = fabs(maximum);
    resolution = fabs(resolution);
    if (!maximum) {
        format->magnitude = 1;
        format->precision = 2;
    }
    else
        format->magnitude = gwy_math_humanize_numbers(resolution, maximum,
                                                      &format->precision);
    siunit2->power10 = ROUND(log(format->magnitude)/G_LN10);
    format->units = gwy_si_unit2_format_as_plain_string(siunit2,
                                                        &format_style_vfmarkup);

    return format;
}

/**
 * gwy_si_unit_get_format_with_digits:
 * @siunit: A SI unit.
 * @maximum: The maximum value to be represented.
 * @sdigits: The number of significant digits the value should have.
 * @format: A value format to set-up, may be %NULL, a new value format is
 *          allocated then.
 *
 * Finds a good format for representing a values with given number of
 * significant digits.
 *
 * The values should be then printed as value/@format->magnitude
 * [@format->units] with @format->precision decimal places.
 *
 * Returns: The value format.  If @format was %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_si_unit_get_format_with_digits(GwySIUnit *siunit,
                                   gdouble maximum,
                                   gint sdigits,
                                   GwySIValueFormat *format)
{
    GwySIUnit2 *siunit2;

    gwy_debug("");
    if (!format)
        format = (GwySIValueFormat*)g_new0(GwySIValueFormat, 1);
    else
        g_free(format->units);

    siunit2 = (GwySIUnit2*)g_object_get_data((GObject*)siunit, "gwy-si-unit2");
    g_assert(siunit2);

    maximum = fabs(maximum);
    if (!maximum) {
        format->magnitude = 1;
        format->precision = sdigits;
    }
    else
        format->magnitude
            = gwy_math_humanize_numbers(maximum/exp(G_LN10*sdigits),
                                        maximum, &format->precision);
    siunit2->power10 = ROUND(log(format->magnitude)/G_LN10);
    format->units = gwy_si_unit2_format_as_plain_string(siunit2,
                                                        &format_style_vfmarkup);

    return format;
}

/**
 * gwy_si_unit_value_format_free:
 * @format: A value format to free.
 *
 * Frees a value format structure.
 **/
void
gwy_si_unit_value_format_free(GwySIValueFormat *format)
{
    g_free(format->units);
    g_free(format);
}


/************************** GwySIUnit2 ***********************************/

static void
gwy_si_unit2_free(GwySIUnit2* siunit)
{
    if (!siunit)
        return;
    g_array_free(siunit->units, TRUE);
    g_free(siunit);
}

static GwySIUnit2*
gwy_si_unit2_parse(const gchar *string)
{
    GwySIUnit2 *siunit;
    GwySimpleUnit unit;
    gdouble q;
    const gchar *end;
    gchar *p, *e;
    gint n, i, pfpower;
    GString *buf;
    gboolean dividing = FALSE;

    if (!string)
        string = "";

    siunit = g_new0(GwySIUnit2, 1);
    siunit->units = g_array_new(FALSE, FALSE, sizeof(GwySimpleUnit));
    siunit->power10 = 0;

    /* give up when it looks too wild */
    end = strpbrk(string,
                  "\177\001\002\003\004\005\006\007"
                  "\010\011\012\013\014\015\016\017"
                  "\020\021\022\023\024\025\026\027"
                  "\030\031\032\033\034\035\036\037"
                  "!#$&()*,:;=?@\\[]_`|{}");
    if (end) {
        g_warning("Invalid character 0x%02x", *end);
        return siunit;
    }

    /* may start with a multiplier, but it must be a power of 10 */
    q = g_ascii_strtod(string, (gchar**)&end);
    if (end != string) {
        string = end;
        siunit->power10 = ROUND(log(q)/G_LN10);
        if (q <= 0 || fabs(log(q/exp(G_LN10*siunit->power10))) > 1e-14) {
            g_warning("Bad multiplier %g", q);
            siunit->power10 = 0;
        }
        else if (g_str_has_prefix(string, "<sup>")) {
            string += strlen("<sup>");
            n = strtol(string, (gchar**)&end, 10);
            if (end == string)
                g_warning("Bad exponent %s", string);
            else if (!g_str_has_prefix(end, "</sup>"))
                g_warning("Expected </sup> after exponent");
            else
                siunit->power10 *= n;
            string = end;
        }
        else if (string[0] == '^') {
            string++;
            n = strtol(string, (gchar**)&end, 10);
            if (end == string)
                g_warning("Bad exponent %s", string);
            else
                siunit->power10 *= n;
            string = end;
        }
    }
    while (g_ascii_isspace(*string))
        string++;

    buf = g_string_new("");

    /* the rest are units */
    while (*string) {
        /* units are separated with whitespace and maybe a division sign */
        end = string;
        do {
            end = strpbrk(end, " /");
            if (!end || end == string || *end != '/' || *(end-1) != '<')
                break;
            end++;
        } while (TRUE);
        if (!end)
            end = string + strlen(string);

        g_string_set_size(buf, 0);
        g_string_append_len(buf, string, end - string);

        /* fix sloppy notations */
        if (buf->str[0] == '\272') {
            if (!buf->str[1])
                g_string_assign(buf, "deg");
            else {
                g_string_erase(buf, 0, 1);
                g_string_prepend(buf, "°");
            }
        }
        else if (!strcmp(buf->str, "°"))
            g_string_assign(buf, "deg");
        else if (buf->str[0] == '\305' && !buf->str[1])
            g_string_assign(buf, "Å");
        else if (!strcmp(buf->str, "Å"))
            g_string_assign(buf, "Å");

        /* get prefix, but be careful not to split mol to mili-ol */
        pfpower = 0;
        for (i = 0; i < G_N_ELEMENTS(known_units); i++) {
            if (g_str_has_prefix(buf->str, known_units[i])
                && !g_ascii_isalpha(buf->str[strlen(known_units[i])]))
                break;
        }
        if (i == G_N_ELEMENTS(known_units) && strlen(buf->str) > 1) {
            for (i = 0; i < G_N_ELEMENTS(SI_prefixes); i++) {
                const gchar *pfx = SI_prefixes[i].prefix;

                if (g_str_has_prefix(buf->str, pfx)
                    && g_ascii_isalpha(buf->str[strlen(pfx)])) {
                    pfpower = SI_prefixes[i].power10;
                    g_string_erase(buf, 0, strlen(pfx));
                    break;
                }
            }
        }

        /* get unit power */
        unit.power = 1;
        if ((p = strstr(buf->str + 1, "<sup>"))) {
            unit.power = strtol(p + strlen("<sup>"), &e, 10);
            if (e == p + strlen("<sup>")
                || !g_str_has_prefix(e, "</sup>")) {
                g_warning("Bad power %s", p);
                unit.power = 1;
            }
            else if (!unit.power || abs(unit.power) > 12) {
                g_warning("Bad power %d", unit.power);
                unit.power = 1;
            }
            g_string_truncate(buf, p - buf->str);
        }
        else if ((p = strchr(buf->str + 1, '^'))) {
            unit.power = strtol(p + 1, &e, 10);
            if (e == p + 1 || *e) {
                g_warning("Bad power %s", p);
                unit.power = 1;
            }
            else if (!unit.power || abs(unit.power) > 12) {
                g_warning("Bad power %d", unit.power);
                unit.power = 1;
            }
            g_string_truncate(buf, p - buf->str);
        }
        else if (buf->len) {
            /* Are we really desperate?  Yes, we are! */
            i = buf->len;
            while (i && (g_ascii_isdigit(buf->str[i-1])
                         || buf->str[i-1] == '-'))
                i--;
            if (i != buf->len) {
                unit.power = strtol(buf->str + i, NULL, 10);
                if (!unit.power || abs(unit.power) > 12) {
                    g_warning("Bad power %d", unit.power);
                    unit.power = 1;
                }
                g_string_truncate(buf, i);
            }
        }

        /* handle some ugly, but quite common units */
        if (!strcmp(buf->str, "Å")) {
            pfpower -= 10;
            g_string_assign(buf, "m");
        }
        else if (!strcmp(buf->str, "%")) {
            pfpower -= 2;
            g_string_assign(buf, "");
        }

        /* elementary sanity */
        if (!g_utf8_validate(buf->str, -1, (const gchar**)&p)) {
            g_warning("Unit string is not valid UTF-8");
            g_string_truncate(buf, p - buf->str);
        }
        if (!buf->len)
            g_warning("Base unit cannot be empty: %s", string);
        if (!g_ascii_isalpha(buf->str[0]) && (guchar)buf->str[0] < 128)
            g_warning("Invalid base unit: %s", buf->str);
        else {
            /* append it */
            unit.unit = g_quark_from_string(buf->str);
            if (dividing)
                unit.power = -unit.power;
            siunit->power10 += unit.power * pfpower;
            g_array_append_val(siunit->units, unit);
        }

        /* TODO: scan known obscure units */
        unit.traits = 0;

        /* get to the next token, looking for division */
        while (g_ascii_isspace(*end))
            end++;
        if (*end == '/') {
            if (dividing)
                g_warning("Cannot group multiple divisions");
            dividing = TRUE;
            end++;
            while (g_ascii_isspace(*end))
                end++;
        }
        string = end;
    }

    gwy_si_unit2_canonicalize(siunit);

    return siunit;
}

static GwySIUnit2*
gwy_si_unit2_power_multiply(GwySIUnit2 *siunit1,
                            gint power1,
                            GwySIUnit2 *siunit2,
                            gint power2)
{
    GwySimpleUnit *unit1, *unit2;
    GwySIUnit2 *siunit;
    gint i, j;

    if (siunit1->units->len < siunit2->units->len) {
        GWY_SWAP(GwySIUnit2*, siunit1, siunit2);
        GWY_SWAP(gint, power1, power2);
    }
    siunit = gwy_si_unit2_power(siunit1, power1);
    siunit->power10 += power2*siunit2->power10;

    for (i = 0; i < siunit2->units->len; i++) {
        unit2 = &g_array_index(siunit2->units, GwySimpleUnit, i);

        for (j = 0; j < siunit1->units->len; j++) {
            unit1 = &g_array_index(siunit1->units, GwySimpleUnit, j);
            if (unit2->unit == unit1->unit) {
                unit1->power += power2*unit2->power;
                break;
            }
        }
        if (j == siunit1->units->len)
            g_array_append_val(siunit1->units, *unit2);
    }
    gwy_si_unit2_canonicalize(siunit);

    return siunit;
}

static GwySIUnit2*
gwy_si_unit2_power(GwySIUnit2 *siunit1,
                   gint power1)
{
    GwySimpleUnit *unit1;
    GwySIUnit2 *siunit;
    gint j;

    siunit = g_new0(GwySIUnit2, 1);
    siunit->units = g_array_new(FALSE, FALSE, sizeof(GwySimpleUnit));
    siunit->power10 = power1*siunit1->power10;

    if (!power1)
        return siunit;

    g_array_append_vals(siunit->units, siunit1->units, siunit1->units->len);
    for (j = 0; j < siunit1->units->len; j++) {
        unit1 = &g_array_index(siunit1->units, GwySimpleUnit, j);
        unit1->power *= power1;
    }

    return siunit;
}

static GwySIUnit2*
gwy_si_unit2_canonicalize(GwySIUnit2 *siunit)
{
    GwySimpleUnit *dst, *src;
    gint i, j;

    /* consolidate multiple occurences of the same unit */
    i = 0;
    while (i < siunit->units->len) {
        src = &g_array_index(siunit->units, GwySimpleUnit, i);

        for (j = 0; j < i; j++) {
            dst = &g_array_index(siunit->units, GwySimpleUnit, j);
            if (src->unit == dst->unit) {
                dst->power += src->power;
                g_array_remove_index(siunit->units, i);
                break;
            }
        }

        if (j == i)
            i++;
    }

    /* remove units with zero power */
    i = 0;
    while (i < siunit->units->len) {
        if (g_array_index(siunit->units, GwySimpleUnit, i).power)
            i++;
        else {
            g_array_remove_index(siunit->units, i);
        }
    }

    return siunit;
}

static gchar*
gwy_si_unit2_format_as_plain_string(GwySIUnit2 *siunit,
                                    const GwySIFormatStyle *fs)
{
    GString *string;
    gchar *s;

    string = gwy_si_unit2_format(siunit, fs);
    s = string->str;
    g_string_free(string, FALSE);

    return s;
}

static GString*
gwy_si_unit2_format(GwySIUnit2 *siunit,
                    const GwySIFormatStyle *fs)
{
    GString *string;
    const gchar *prefix = "No GCC, this can't be used uninitialized";
    GwySimpleUnit *unit;
    gint i, prefix_bearer, move_me_to_end;

    string = g_string_new("");

    /* if there is a single unit with negative exponent, move it to the end
     * TODO: we may want more sophistication here */
    move_me_to_end = -1;
    if (siunit->units->len > 1) {
        for (i = 0; i < siunit->units->len; i++) {
            unit = &g_array_index(siunit->units, GwySimpleUnit, i);
            if (unit->power < 0) {
                if (move_me_to_end >= 0) {
                    move_me_to_end = -1;
                    break;
                }
                move_me_to_end = i;
            }
        }
    }

    /* find a victim to prepend a prefix to.  mwhahaha */
    prefix_bearer = -1;
    if (siunit->power10) {
        for (i = 0; i < siunit->units->len; i++) {
            if (i == move_me_to_end)
                continue;
            unit = &g_array_index(siunit->units, GwySimpleUnit, i);
            if (siunit->power10 % (3*abs(unit->power)) == 0) {
                prefix_bearer = i;
                break;
            }
        }
    }
    if (siunit->power10 && prefix_bearer < 0 && move_me_to_end >= 0) {
        unit = &g_array_index(siunit->units, GwySimpleUnit, move_me_to_end);
        if (siunit->power10 % (3*abs(unit->power)) == 0)
            prefix_bearer = move_me_to_end;
    }
    /* check whether we are not out of prefix range */
    if (prefix_bearer >= 0) {
        unit = &g_array_index(siunit->units, GwySimpleUnit, prefix_bearer);
        prefix = gwy_si_unit2_prefix(siunit->power10/unit->power);
        if (!prefix)
            prefix_bearer = -1;
    }

    /* if we were unable to place the prefix, we must add a power of 10 */
    if (siunit->power10 && prefix_bearer < 0) {
        if (fs->power10_prefix)
            g_string_append(string, fs->power10_prefix);
        g_string_append_printf(string, "%d", siunit->power10);
        if (fs->power_suffix)
            g_string_append(string, fs->power_suffix);
        if (fs->power_unit_separator)
            g_string_append(string, fs->power_unit_separator);
    }

    /* append units */
    for (i = 0; i < siunit->units->len; i++) {
        if (i == move_me_to_end)
            continue;
        if (i > 1 || (i && move_me_to_end)) {
            g_string_append(string, fs->unit_times);
        }
        unit = &g_array_index(siunit->units, GwySimpleUnit, i);
        if (i == prefix_bearer)
            g_string_append(string, prefix);
        g_string_append(string, g_quark_to_string(unit->unit));
        if (unit->power != 1) {
            if (fs->power_prefix)
                g_string_append(string, fs->power_prefix);
            g_string_append_printf(string, "%d", unit->power);
            if (fs->power_suffix)
                g_string_append(string, fs->power_suffix);
        }
    }
    if (move_me_to_end >= 0) {
        g_string_append(string, fs->unit_division);
        unit = &g_array_index(siunit->units, GwySimpleUnit, move_me_to_end);
        if (move_me_to_end == prefix_bearer)
            g_string_append(string, prefix);
        g_string_append(string, g_quark_to_string(unit->unit));
        if (unit->power != -1) {
            if (fs->power_prefix)
                g_string_append(string, fs->power_prefix);
            g_string_append_printf(string, "%d", -unit->power);
            if (fs->power_suffix)
                g_string_append(string, fs->power_suffix);
        }
    }

    return string;
}

const gchar*
gwy_si_unit2_prefix(gint power)
{
    gint i;

    for (i = 0; i < G_N_ELEMENTS(SI_prefixes); i++) {
        if (SI_prefixes[i].power10 == power)
            return SI_prefixes[i].prefix;
    }
    return NULL;
}

/************************** Documentation ****************************/

/**
 * GwySIUnit:
 *
 * The #GwySIUnit struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwySIValueFormat:
 * @magnitude: Number to divide a quantity by (a power of 1000).
 * @precision: Number of decimal places to format a quantity to.
 * @units: Units to put after quantity divided by @magnitude.
 *
 * A physical quantity formatting information.
 */

/**
 * gwy_si_unit_duplicate:
 * @siunit: An SI unit to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 *
 * Since: 1.8
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
