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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymath.h>
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
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand1,
                                            GWY_DATA_COMPATIBILITY_RES));
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand2,
                                            GWY_DATA_COMPATIBILITY_RES));

    xres = result->xres;
    yres = result->yres;
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
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand1,
                                            GWY_DATA_COMPATIBILITY_RES));
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand2,
                                            GWY_DATA_COMPATIBILITY_RES));

    xres = result->xres;
    yres = result->yres;
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
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand1,
                                            GWY_DATA_COMPATIBILITY_RES));
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand2,
                                            GWY_DATA_COMPATIBILITY_RES));

    xres = result->xres;
    yres = result->yres;
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
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand1,
                                            GWY_DATA_COMPATIBILITY_RES));
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand2,
                                            GWY_DATA_COMPATIBILITY_RES));

    xres = result->xres;
    yres = result->yres;
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
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand1,
                                            GWY_DATA_COMPATIBILITY_RES));
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand2,
                                            GWY_DATA_COMPATIBILITY_RES));

    xres = result->xres;
    yres = result->yres;
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
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand1,
                                            GWY_DATA_COMPATIBILITY_RES));
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand2,
                                            GWY_DATA_COMPATIBILITY_RES));

    xres = result->xres;
    yres = result->yres;
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
 * gwy_data_field_hypot_of_fields:
 * @result: A data field to put the result to.  May be one of @operand1,
 *          @operand2.
 * @operand1: First data field operand.
 * @operand2: Second data field operand.
 *
 * Finds point-wise hypotenuse of two data fields.
 *
 * Since: 2.31
 **/
void
gwy_data_field_hypot_of_fields(GwyDataField *result,
                               GwyDataField *operand1,
                               GwyDataField *operand2)
{
    gdouble *p, *q, *r;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(result));
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand1,
                                             GWY_DATA_COMPATIBILITY_RES));
    g_return_if_fail
        (!gwy_data_field_check_compatibility(result, operand2,
                                             GWY_DATA_COMPATIBILITY_RES));

    xres = result->xres;
    yres = result->yres;
    r = result->data;
    p = operand1->data;
    q = operand2->data;
    for (i = xres*yres; i; i--, p++, q++, r++)
        *r = hypot(*p, *q);

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

/* code ported from libgwy3 */

#define gwy_assign(dest, source, n) \
    memcpy((dest), (source), (n)*sizeof((dest)[0]))

typedef void (*RowExtendFunc)(const gdouble *in,
                              gdouble *out,
                              guint pos,
                              guint width,
                              guint res,
                              guint extend_left,
                              guint extend_right,
                              gdouble value);

typedef void (*RectExtendFunc)(const gdouble *in,
                               guint inrowstride,
                               gdouble *out,
                               guint outrowstride,
                               guint xpos,
                               guint ypos,
                               guint width,
                               guint height,
                               guint xres,
                               guint yres,
                               guint extend_left,
                               guint extend_right,
                               guint extend_up,
                               guint extend_down,
                               gdouble value);

static inline void
fill_block(gdouble *data, guint len, gdouble value)
{
    while (len--)
        *(data++) = value;
}

static inline void
row_extend_base(const gdouble *in, gdouble *out,
                guint *pos, guint *width, guint res,
                guint *extend_left, guint *extend_right)
{
    guint e2r, e2l;

    // Expand the ROI to the right as far as possible
    e2r = MIN(*extend_right, res - (*pos + *width));
    *width += e2r;
    *extend_right -= e2r;

    // Expand the ROI to the left as far as possible
    e2l = MIN(*extend_left, *pos);
    *width += e2l;
    *extend_left -= e2l;
    *pos -= e2l;

    // Direct copy of the ROI
    gwy_assign(out + *extend_left, in + *pos, *width);
}

static void
row_extend_mirror(const gdouble *in, gdouble *out,
                  guint pos, guint width, guint res,
                  guint extend_left, guint extend_right,
                  G_GNUC_UNUSED gdouble value)
{
    guint res2 = 2*res, k0, j;
    gdouble *out2;
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    out2 = out + extend_left + width;
    for (j = 0; j < extend_right; j++, out2++) {
        guint k = (pos + width + j) % res2;
        *out2 = (k < res) ? in[k] : in[res2-1 - k];
    }
    // Backward-extend
    k0 = (extend_left/res2 + 1)*res2;
    out2 = out + extend_left-1;
    for (j = 1; j <= extend_left; j++, out2--) {
        guint k = (k0 + pos - j) % res2;
        *out2 = (k < res) ? in[k] : in[res2-1 - k];
    }
}

static void
row_extend_periodic(const gdouble *in, gdouble *out,
                    guint pos, guint width, guint res,
                    guint extend_left, guint extend_right,
                    G_GNUC_UNUSED gdouble value)
{
    guint k0, j;
    gdouble *out2;
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    out2 = out + extend_left + width;
    for (j = 0; j < extend_right; j++, out2++) {
        guint k = (pos + width + j) % res;
        *out2 = in[k];
    }
    // Backward-extend
    k0 = (extend_left/res + 1)*res;
    out2 = out + extend_left-1;
    for (j = 1; j <= extend_left; j++, out2--) {
        guint k = (k0 + pos - j) % res;
        *out2 = in[k];
    }
}

static void
row_extend_border(const gdouble *in, gdouble *out,
                  guint pos, guint width, guint res,
                  guint extend_left, guint extend_right,
                  G_GNUC_UNUSED gdouble value)
{
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    fill_block(out + extend_left + width, extend_right, in[res-1]);
    // Backward-extend
    fill_block(out, extend_left, in[0]);
}

static void
row_extend_fill(const gdouble *in, gdouble *out,
                guint pos, guint width, guint res,
                guint extend_left, guint extend_right,
                gdouble value)
{
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    fill_block(out + extend_left + width, extend_right, value);
    // Backward-extend
    fill_block(out, extend_left, value);
}
static inline void
rect_extend_base(const gdouble *in, guint inrowstride,
                 gdouble *out, guint outrowstride,
                 guint xpos, guint *ypos,
                 guint width, guint *height,
                 guint xres, guint yres,
                 guint extend_left, guint extend_right,
                 guint *extend_up, guint *extend_down,
                 RowExtendFunc extend_row, gdouble fill_value)
{
    guint e2r, e2l, i;
    // Expand the ROI down as far as possible
    e2r = MIN(*extend_down, yres - (*ypos + *height));
    *height += e2r;
    *extend_down -= e2r;

    // Expand the ROI up as far as possible
    e2l = MIN(*extend_up, *ypos);
    *height += e2l;
    *extend_up -= e2l;
    *ypos -= e2l;

    // Row-wise extension within the vertical range of the ROI
    for (i = 0; i < *height; i++)
        extend_row(in + (*ypos + i)*inrowstride,
                   out + (*extend_up + i)*outrowstride,
                   xpos, width, xres, extend_left, extend_right, fill_value);
}

static void
rect_extend_mirror(const gdouble *in, guint inrowstride,
                   gdouble *out, guint outrowstride,
                   guint xpos, guint ypos,
                   guint width, guint height,
                   guint xres, guint yres,
                   guint extend_left, guint extend_right,
                   guint extend_up, guint extend_down,
                   G_GNUC_UNUSED gdouble value)
{
    guint yres2, i, k0;
    gdouble *out2;
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_mirror, value);
    // Forward-extend
    yres2 = 2*yres;
    out2 = out + outrowstride*(extend_up + height);
    for (i = 0; i < extend_down; i++, out2 += outrowstride) {
        guint k = (ypos + height + i) % yres2;
        if (k >= yres)
            k = yres2-1 - k;
        row_extend_mirror(in + k*inrowstride, out2,
                          xpos, width, xres, extend_left, extend_right, value);
    }
    // Backward-extend
    k0 = (extend_up/yres2 + 1)*yres2;
    out2 = out + outrowstride*(extend_up - 1);
    for (i = 1; i <= extend_up; i++, out2 -= outrowstride) {
        guint k = (k0 + ypos - i) % yres2;
        if (k >= yres)
            k = yres2-1 - k;
        row_extend_mirror(in + k*inrowstride, out2,
                          xpos, width, xres, extend_left, extend_right, value);
    }
}

static void
rect_extend_periodic(const gdouble *in, guint inrowstride,
                     gdouble *out, guint outrowstride,
                     guint xpos, guint ypos,
                     guint width, guint height,
                     guint xres, guint yres,
                     guint extend_left, guint extend_right,
                     guint extend_up, guint extend_down,
                     G_GNUC_UNUSED gdouble value)
{
    guint i, k0;
    gdouble *out2;
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_periodic, value);
    // Forward-extend
    out2 = out + outrowstride*(extend_up + height);
    for (i = 0; i < extend_down; i++, out2 += outrowstride) {
        guint k = (ypos + height + i) % yres;
        row_extend_periodic(in + k*inrowstride, out2,
                            xpos, width, xres, extend_left, extend_right, value);
    }
    // Backward-extend
    k0 = (extend_up/yres + 1)*yres;
    out2 = out + outrowstride*(extend_up - 1);
    for (i = 1; i <= extend_up; i++, out2 -= outrowstride) {
        guint k = (k0 + ypos - i) % yres;
        row_extend_periodic(in + k*inrowstride, out2,
                            xpos, width, xres, extend_left, extend_right, value);
    }
}

static void
rect_extend_border(const gdouble *in, guint inrowstride,
                   gdouble *out, guint outrowstride,
                   guint xpos, guint ypos,
                   guint width, guint height,
                   guint xres, guint yres,
                   guint extend_left, guint extend_right,
                   guint extend_up, guint extend_down,
                   G_GNUC_UNUSED gdouble value)
{
    guint i;
    gdouble *out2;
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_border, value);
    // Forward-extend
    out2 = out + outrowstride*(extend_up + height);
    for (i = 0; i < extend_down; i++, out2 += outrowstride)
        row_extend_border(in + (yres-1)*inrowstride, out2,
                          xpos, width, xres, extend_left, extend_right, value);
    // Backward-extend
    out2 = out + outrowstride*(extend_up - 1);
    for (i = 1; i <= extend_up; i++, out2 -= outrowstride)
        row_extend_border(in, out2,
                          xpos, width, xres, extend_left, extend_right, value);
}

static void
rect_extend_fill(const gdouble *in, guint inrowstride,
                 gdouble *out, guint outrowstride,
                 guint xpos, guint ypos,
                 guint width, guint height,
                 guint xres, guint yres,
                 guint extend_left, guint extend_right,
                 guint extend_up, guint extend_down,
                 gdouble value)
{
    guint i;
    gdouble *out2;
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_fill, value);
    // Forward-extend
    out2 = out + outrowstride*(extend_up + height);
    for (i = 0; i < extend_down; i++, out2 += outrowstride)
        fill_block(out2, extend_left + width + extend_right, value);
    // Backward-extend
    out2 = out + outrowstride*(extend_up - 1);
    for (i = 1; i <= extend_up; i++, out2 -= outrowstride)
        fill_block(out2, extend_left + width + extend_right, value);
}

static RectExtendFunc
get_rect_extend_func(GwyExteriorType exterior)
{
    if (exterior == GWY_EXTERIOR_FIXED_VALUE)
        return &rect_extend_fill;
    if (exterior == GWY_EXTERIOR_BORDER_EXTEND)
        return &rect_extend_border;
    if (exterior == GWY_EXTERIOR_MIRROR_EXTEND)
        return &rect_extend_mirror;
    if (exterior == GWY_EXTERIOR_PERIODIC)
        return &rect_extend_periodic;
    g_return_val_if_reached(NULL);
}

/**
 * gwy_data_field_extend:
 * @field: A two-dimensional data field.
 * @left: Number of pixels to extend to the left (towards lower column indices).
 * @right: Number of pixels to extend to the right (towards higher column
 *         indices).
 * @up: Number of pixels to extend up (towards lower row indices).
 * @down: Number of pixels to extend down (towards higher row indices).
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with %GWY_EXTERIOR_FIXED_VALUE exterior.
 * @keep_offsets: %TRUE to set the X and Y offsets of the new field
 *                using @field offsets.  %FALSE to set offsets
 *                of the new field to zeroes.
 *
 * Creates a new data field by extending another data field using the specified
 * method of exterior handling.
 *
 * Returns: A newly created data field.
 *
 * Since: 2.36
 **/
GwyDataField*
gwy_data_field_extend(GwyDataField *data_field,
                      guint left, guint right,
                      guint up, guint down,
                      GwyExteriorType exterior,
                      gdouble fill_value,
                      gboolean keep_offsets)
{
    GwyDataField *target;
    RectExtendFunc extend_rect;
    guint col = 0, row = 0, width, height;
    gdouble dx, dy;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);

    extend_rect = get_rect_extend_func(exterior);
    g_return_val_if_fail(extend_rect, NULL);

    width = data_field->xres;
    height = data_field->yres;
    target = gwy_data_field_new(width + left + right, height + up + down,
                                1.0, 1.0, FALSE);
    extend_rect(data_field->data, data_field->xres, target->data, target->xres,
                col, row, width, height, data_field->xres, data_field->yres,
                left, right, up, down, fill_value);

    dx = data_field->xreal/data_field->xres;
    dy = data_field->yreal/data_field->yres;
    gwy_data_field_set_xreal(target, (width + left + right)*dx);
    gwy_data_field_set_yreal(target, (height + up + down)*dy);
    if (keep_offsets) {
        gwy_data_field_set_xoffset(target, data_field->xoff + col*dx - left*dx);
        gwy_data_field_set_yoffset(target, data_field->yoff + row*dy - up*dy);
    }
    else {
        gwy_data_field_set_xoffset(target, 0.0);
        gwy_data_field_set_yoffset(target, 0.0);
    }
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_xy(data_field)),
                           G_OBJECT(gwy_data_field_get_si_unit_xy(target)));
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_z(data_field)),
                           G_OBJECT(gwy_data_field_get_si_unit_z(target)));

    return target;
}

/************************** Documentation ****************************/

/**
 * SECTION:arithmetic
 * @title: arithmetic
 * @short_description: Arithmetic opetations on data fields
 *
 * Data arithmetic functions perform simple operations combining several data
 * fields.  Their sizes have to be size-compatible, i.e.
 * gwy_data_field_check_compatibility(operand1, operand2,
 * GWY_DATA_COMPATIBILITY_RES)
 * must pass and the same must hold for the data field to store the result to.
 *
 * Functions gwy_data_field_check_compatibility() and
 * gwy_data_line_check_compatibility() simplify testing compatibility of data
 * fields and lines, respectively.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
