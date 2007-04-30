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
#include <math.h>
#include <libprocess/arithmetic.h>
#include "gwyprocessinternal.h"

/* for compatibility checks */
#define EPSILON 1e-6

/**
 * gwy_data_field_sum_fields:
 * @result: A data field to put the result to.  May be one of @operand1,
 *          @operand2.
 * @operand1: First data field operand.
 * @operand2: Second data field operand.
 *
 * Sums two data fields.
 **/
void
gwy_data_field_sum_fields(GwyDataField *result,
                          GwyDataField *operand1,
                          GwyDataField *operand2)
{
    gdouble *p, *q, *r;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(result));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand1));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand2));
    xres = gwy_data_field_get_xres(result);
    yres = gwy_data_field_get_yres(result);
    g_return_if_fail(xres == gwy_data_field_get_xres(operand1));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand1));
    g_return_if_fail(xres == gwy_data_field_get_xres(operand2));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand2));

    r = result->data;
    p = operand1->data;
    q = operand2->data;
    for (i = xres*yres; i; i--, p++, q++, r++)
        *r = *p + *q;

    if (CTEST(operand1, SUM) && CTEST(operand2, SUM)) {
        result->cached = CBIT(SUM);
        CVAL(result, SUM) = CVAL(operand1, SUM) + CVAL(operand2, SUM);
    }
    else
        gwy_data_field_invalidate(result);
}

/**
 * gwy_data_field_subtract_fields:
 * @result: A data field to put the result to.  May be one of @operand1,
 *          @operand2.
 * @operand1: First data field operand.
 * @operand2: Second data field operand.
 *
 * Subtracts one data field from another.
 **/
void
gwy_data_field_subtract_fields(GwyDataField *result,
                               GwyDataField *operand1,
                               GwyDataField *operand2)
{
    gdouble *p, *q, *r;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(result));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand1));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand2));
    xres = gwy_data_field_get_xres(result);
    yres = gwy_data_field_get_yres(result);
    g_return_if_fail(xres == gwy_data_field_get_xres(operand1));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand1));
    g_return_if_fail(xres == gwy_data_field_get_xres(operand2));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand2));

    r = result->data;
    p = operand1->data;
    q = operand2->data;
    for (i = xres*yres; i; i--, p++, q++, r++)
        *r = *p - *q;

    if (CTEST(operand1, SUM) && CTEST(operand2, SUM)) {
        result->cached = CBIT(SUM);
        CVAL(result, SUM) = CVAL(operand1, SUM) - CVAL(operand2, SUM);
    }
    else
        gwy_data_field_invalidate(result);
}

/**
 * gwy_data_field_multiply_fields:
 * @result: A data field to put the result to.  May be one of @operand1,
 *          @operand2.
 * @operand1: First data field operand.
 * @operand2: Second data field operand.
 *
 * Multiplies two data fields.
 **/
void
gwy_data_field_multiply_fields(GwyDataField *result,
                               GwyDataField *operand1,
                               GwyDataField *operand2)
{
    gdouble *p, *q, *r;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(result));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand1));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand2));
    xres = gwy_data_field_get_xres(result);
    yres = gwy_data_field_get_yres(result);
    g_return_if_fail(xres == gwy_data_field_get_xres(operand1));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand1));
    g_return_if_fail(xres == gwy_data_field_get_xres(operand2));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand2));

    r = result->data;
    p = operand1->data;
    q = operand2->data;
    for (i = xres*yres; i; i--, p++, q++, r++)
        *r = *p * *q;

    gwy_data_field_invalidate(result);
}

/**
 * gwy_data_field_divide_fields:
 * @result: A data field to put the result to.  May be one of @operand1,
 *          @operand2.
 * @operand1: First data field operand.
 * @operand2: Second data field operand.
 *
 * Divides one data field with another.
 **/
void
gwy_data_field_divide_fields(GwyDataField *result,
                             GwyDataField *operand1,
                             GwyDataField *operand2)
{
    gdouble *p, *q, *r;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(result));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand1));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand2));
    xres = gwy_data_field_get_xres(result);
    yres = gwy_data_field_get_yres(result);
    g_return_if_fail(xres == gwy_data_field_get_xres(operand1));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand1));
    g_return_if_fail(xres == gwy_data_field_get_xres(operand2));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand2));

    r = result->data;
    p = operand1->data;
    q = operand2->data;
    for (i = xres*yres; i; i--, p++, q++, r++)
        *r = *p / *q;

    gwy_data_field_invalidate(result);
}

/**
 * gwy_data_field_min_of_fields:
 * @result: A data field to put the result to.  May be one of @operand1,
 *          @operand2.
 * @operand1: First data field operand.
 * @operand2: Second data field operand.
 *
 * Finds point-wise maxima of two data fields.
 **/
void
gwy_data_field_min_of_fields(GwyDataField *result,
                             GwyDataField *operand1,
                             GwyDataField *operand2)
{
    gdouble *p, *q, *r;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(result));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand1));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand2));
    xres = gwy_data_field_get_xres(result);
    yres = gwy_data_field_get_yres(result);
    g_return_if_fail(xres == gwy_data_field_get_xres(operand1));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand1));
    g_return_if_fail(xres == gwy_data_field_get_xres(operand2));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand2));

    r = result->data;
    p = operand1->data;
    q = operand2->data;
    for (i = xres*yres; i; i--, p++, q++, r++)
        *r = MIN(*p, *q);

    if (CTEST(operand1, MIN) && CTEST(operand2, MIN)) {
        result->cached = CBIT(MIN);
        CVAL(result, MIN) = MIN(CVAL(operand1, MIN), CVAL(operand2, MIN));
    }
    else
        gwy_data_field_invalidate(result);
}

/**
 * gwy_data_field_max_of_fields:
 * @result: A data field to put the result to.  May be one of @operand1,
 *          @operand2.
 * @operand1: First data field operand.
 * @operand2: Second data field operand.
 *
 * Finds point-wise minima of two data fields.
 **/
void
gwy_data_field_max_of_fields(GwyDataField *result,
                             GwyDataField *operand1,
                             GwyDataField *operand2)
{
    gdouble *p, *q, *r;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(result));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand1));
    g_return_if_fail(GWY_IS_DATA_FIELD(operand2));
    xres = gwy_data_field_get_xres(result);
    yres = gwy_data_field_get_yres(result);
    g_return_if_fail(xres == gwy_data_field_get_xres(operand1));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand1));
    g_return_if_fail(xres == gwy_data_field_get_xres(operand2));
    g_return_if_fail(yres == gwy_data_field_get_yres(operand2));

    r = result->data;
    p = operand1->data;
    q = operand2->data;
    for (i = xres*yres; i; i--, p++, q++, r++)
        *r = MAX(*p, *q);

    if (CTEST(operand1, MAX) && CTEST(operand2, MAX)) {
        result->cached = CBIT(MAX);
        CVAL(result, MAX) = MAX(CVAL(operand1, MAX), CVAL(operand2, MAX));
    }
    else
        gwy_data_field_invalidate(result);
}

/**
 * gwy_data_field_check_compatibility:
 * @data_field1: A data field.
 * @data_field2: Another data field.
 * @check: The compatibility tests to perform.
 *
 * Checks whether two data fields are compatible.
 *
 * Returns: Zero if all tested properties are compatible.  Flags corresponding
 *          to failed tests if data fields are not compatible.
 **/
GwyDataCompatibilityFlags
gwy_data_field_check_compatibility(GwyDataField *data_field1,
                                   GwyDataField *data_field2,
                                   GwyDataCompatibilityFlags check)
{
    GwyDataCompatibilityFlags result = 0;
    gint xres1, xres2, yres1, yres2;
    gdouble xreal1, xreal2, yreal1, yreal2;
    GwySIUnit *unit1, *unit2;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field1), check);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field2), check);

    xres1 = data_field1->xres;
    xres2 = data_field2->xres;
    yres1 = data_field1->yres;
    yres2 = data_field2->yres;
    xreal1 = data_field1->xreal;
    xreal2 = data_field2->xreal;
    yreal1 = data_field1->yreal;
    yreal2 = data_field2->yreal;

    /* Resolution */
    if (check & GWY_DATA_COMPATIBILITY_RES) {
        if (xres1 != xres2 || yres1 != yres2)
            result |= GWY_DATA_COMPATIBILITY_RES;
    }

    /* Real size */
    if (check & GWY_DATA_COMPATIBILITY_REAL) {
        /* Keeps the condition in negative form to catch NaNs and odd values
         * as incompatible. */
        if (!(fabs(log(xreal1/xreal2)) <= EPSILON)
            || !(fabs(log(yreal1/yreal2)) <= EPSILON))
            result |= GWY_DATA_COMPATIBILITY_REAL;
    }

    /* Measure */
    if (check & GWY_DATA_COMPATIBILITY_MEASURE) {
        if (!(fabs(log(xreal1/xres1*xres2/xreal2)) <= EPSILON)
            || !(fabs(log(yreal1/yres1*yres2/yreal2)) <= EPSILON))
            result |= GWY_DATA_COMPATIBILITY_MEASURE;
    }

    /* Lateral units */
    if (check & GWY_DATA_COMPATIBILITY_LATERAL) {
        /* This can cause instantiation of data_field units as a side effect */
        unit1 = gwy_data_field_get_si_unit_xy(data_field1);
        unit2 = gwy_data_field_get_si_unit_xy(data_field2);
        if (!gwy_si_unit_equal(unit1, unit2))
            result |= GWY_DATA_COMPATIBILITY_LATERAL;
    }

    /* Value units */
    if (check & GWY_DATA_COMPATIBILITY_VALUE) {
        /* This can cause instantiation of data_field units as a side effect */
        unit1 = gwy_data_field_get_si_unit_z(data_field1);
        unit2 = gwy_data_field_get_si_unit_z(data_field2);
        if (!gwy_si_unit_equal(unit1, unit2))
            result |= GWY_DATA_COMPATIBILITY_VALUE;
    }

    return result;
}

/**
 * gwy_data_line_check_compatibility:
 * @data_line1: A data line.
 * @data_line2: Another data line.
 * @check: The compatibility tests to perform.
 *
 * Checks whether two data lines are compatible.
 *
 * Returns: Zero if all tested properties are compatible.  Flags corresponding
 *          to failed tests if data lines are not compatible.
 **/
GwyDataCompatibilityFlags
gwy_data_line_check_compatibility(GwyDataLine *data_line1,
                                  GwyDataLine *data_line2,
                                  GwyDataCompatibilityFlags check)
{
    GwyDataCompatibilityFlags result = 0;
    gint res1, res2;
    gdouble real1, real2;
    GwySIUnit *unit1, *unit2;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line1), check);
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line2), check);

    res1 = data_line1->res;
    res2 = data_line2->res;
    real1 = data_line1->real;
    real2 = data_line2->real;

    /* Resolution */
    if (check & GWY_DATA_COMPATIBILITY_RES) {
        if (res1 != res2)
            result |= GWY_DATA_COMPATIBILITY_RES;
    }

    /* Real size */
    if (check & GWY_DATA_COMPATIBILITY_REAL) {
        if (!(fabs(log(real1/real2)) <= EPSILON))
            result |= GWY_DATA_COMPATIBILITY_REAL;
    }

    /* Measure */
    if (check & GWY_DATA_COMPATIBILITY_MEASURE) {
        if (!(fabs(log(real1/res1*res2/real2)) <= EPSILON))
            result |= GWY_DATA_COMPATIBILITY_MEASURE;
    }

    /* Lateral units */
    if (check & GWY_DATA_COMPATIBILITY_LATERAL) {
        /* This can cause instantiation of data_line units as a side effect */
        unit1 = gwy_data_line_get_si_unit_x(data_line1);
        unit2 = gwy_data_line_get_si_unit_x(data_line2);
        if (!gwy_si_unit_equal(unit1, unit2))
            result |= GWY_DATA_COMPATIBILITY_LATERAL;
    }

    /* Value units */
    if (check & GWY_DATA_COMPATIBILITY_VALUE) {
        /* This can cause instantiation of data_line units as a side effect */
        unit1 = gwy_data_line_get_si_unit_y(data_line1);
        unit2 = gwy_data_line_get_si_unit_y(data_line2);
        if (!gwy_si_unit_equal(unit1, unit2))
            result |= GWY_DATA_COMPATIBILITY_VALUE;
    }

    return result;
}

/************************** Documentation ****************************/

/**
 * SECTION:arithmetic
 * @title: arithmetic
 * @short_description: Arithmetic opetations on data fields
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
