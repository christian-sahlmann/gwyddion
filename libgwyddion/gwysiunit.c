/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include "gwymath.h"
#include "gwysiunit.h"

#define ROUND(x) ((gint)floor((x) + 0.5))

#define GWY_SI_UNIT_TYPE_NAME "GwySiUnit"

static void     gwy_si_unit_class_init        (GwySIUnitClass *klass);
static void     gwy_si_unit_init              (GwySIUnit *si_unit);
static void     gwy_si_unit_finalize          (GwySIUnit *si_unit);
static void     gwy_si_unit_serializable_init (gpointer giface);
static guchar*  gwy_si_unit_serialize         (GObject *obj,
                                              guchar *buffer,
                                              gsize *size);
static GObject* gwy_si_unit_deserialize       (const guchar *buffer,
                                              gsize size,
                                              gsize *position);
static GObject* gwy_si_unit_duplicate         (GObject *object);

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
gwy_si_unit_serializable_init(gpointer giface)
{
    GwySerializableClass *iface = giface;

    gwy_debug("");
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_SERIALIZABLE);

    /* initialize stuff */
    iface->serialize = gwy_si_unit_serialize;
    iface->deserialize = gwy_si_unit_deserialize;
    iface->duplicate = gwy_si_unit_duplicate;
}


static void
gwy_si_unit_class_init(GwySIUnitClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    gobject_class->finalize = (GObjectFinalizeFunc)gwy_si_unit_finalize;
}

static void
gwy_si_unit_init(GwySIUnit *si_unit)
{
    gwy_debug("");
    si_unit->unitstr = NULL;
}

static void
gwy_si_unit_finalize(GwySIUnit *si_unit)
{
    gwy_debug("");
    g_free(si_unit->unitstr);
}

static guchar*
gwy_si_unit_serialize(GObject *obj,
                         guchar *buffer,
                         gsize *size)
{
    GwySIUnit *si_unit;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_SI_UNIT(obj), NULL);

    si_unit = GWY_SI_UNIT(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "unitstr", &si_unit->unitstr, NULL, },
        };
        return gwy_serialize_pack_object_struct(buffer, size,
                                                GWY_SI_UNIT_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_si_unit_deserialize(const guchar *buffer,
                           gsize size,
                           gsize *position)
{
    gchar *unitstr=NULL;

    GwySIUnit *si_unit;
    GwySerializeSpec spec[] = {
        { 's', "unitstr", &unitstr, NULL, },
    };

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_SI_UNIT_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        return NULL;
    }

    /* don't allocate large amount of memory just to immediately free it */
    si_unit = (GwySIUnit*)gwy_si_unit_new(unitstr);

    return (GObject*)si_unit;
}


static
GObject* gwy_si_unit_duplicate (GObject *object)
{
    GwySIUnit *si_unit;
    GObject *duplicate;

    g_return_val_if_fail(GWY_IS_SI_UNIT(object), NULL);
    si_unit = GWY_SI_UNIT(object);
    duplicate = gwy_si_unit_new(si_unit->unitstr);

    return duplicate;
}

GObject*
gwy_si_unit_new(char *unit_string)
{
    GwySIUnit *siunit;

    gwy_debug("");
    siunit = g_object_new(GWY_TYPE_SI_UNIT, NULL);
    siunit->unitstr = g_strdup(unit_string);

    return (GObject*)siunit;
}

void
gwy_si_unit_set_unit_string(GwySIUnit *siunit, char *unit_string)
{
    gwy_debug("");

    g_free(siunit->unitstr);
    siunit->unitstr = g_strdup(unit_string);
}

void
gwy_si_unit_copy(GwySIUnit *target, GwySIUnit *example)
{
    gwy_si_unit_set_unit_string(target, example->unitstr);
}

gchar*
gwy_si_unit_get_unit_string(GwySIUnit *siunit)
{
    gwy_debug("");
    return siunit->unitstr;
}

void
gwy_si_unit_get_prefix(GwySIUnit *siunit,
                       double value,
                       gint precision,
                       char *prefix,
                       double *power)
{
    gwy_debug("");
    *power = pow(10, 3*ROUND(((gint)(log10(fabs(value))))/3.0) - 3);
    strcpy(prefix, gwy_math_SI_prefix(*power));
}

void
gwy_si_unit_get_prefixed(GwySIUnit *siunit,
                         double value,
                         gint precision,
                         char *prefix,
                         double *power)
{
    gwy_debug("");
    *power = pow(10, 3*ROUND(((gint)(log10(fabs(value))))/3.0) - 3);
    strcpy(prefix, gwy_math_SI_prefix(*power));
    strcat(prefix, siunit->unitstr);
}



/**
 * gwy_math_SI_prefix:
 * @magnitude: A power of 1000.
 *
 * Finds SI prefix corresponding to a given power of 1000.
 *
 * In fact, @magnitude doesn't have to be power of 1000, but then the result
 * is mostly meaningless.
 *
 * Returns: The SI unit prefix corresponding to @magnitude, "?" if @magnitude
 *          is outside of the SI prefix range.  The returned value must be
 *          considered constant and never modified or freed.
 **/
/*const gchar*
gwy_math_SI_prefix(gdouble magnitude)
{
    static const gchar *positive[] = {
        "", "k", "M", "G", "T", "P", "E", "Z", "Y"
    };
    static const gchar *negative[] = {
        "", "m", "µ", "n", "p", "f", "a", "z", "y"
    };
    static const gchar *unknown = "?";
    gint i;

    i = ROUND(log10(magnitude)/3.0);
    if (i >= 0 && i < (gint)G_N_ELEMENTS(positive))
        return positive[i];
    if (i <= 0 && -i < (gint)G_N_ELEMENTS(negative))
        return negative[-i];
*/    /* FIXME: the vertical ruler text placing routine can't reasonably
     * break things like 10<sup>-36</sup> to lines */
 /*   g_warning("magnitude %g outside of prefix range.  FIXME!", magnitude);

    return unknown;
}
*/
/**
 * gwy_math_humanize_numbers:
 * @unit: The smallest possible step.
 * @maximum: The maximum possible value.
 * @precision: A location to store printf() precession, if not %NULL.
 *
 * Find a human readable representation for a range of numbers.
 *
 * Returns: The magnitude i.e., a power of 1000.
 **/
/*
gdouble
gwy_math_humanize_numbers(gdouble unit,
                          gdouble maximum,
                          gint *precision)
{
    gdouble lm, lu, mag, range, min;

    lm = log10(maximum);
    lu = log10(unit);
    mag = 3.0*floor((lm + lu)/6.0);
    if (precision) {
        range = lm - lu;
        if (range > 3.0)
            range = (range + 3.0)/2;
        min = lm - range;
        *precision = (min < mag) ? (gint)ceil(mag - min) : 0;
    }

    return exp(G_LN10*mag);
}
*/


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
