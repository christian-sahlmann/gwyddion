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

#include "datafield.h"

/**
 * gwy_data_field_sum_fields:
 * @result: A data field to put the result to.  May be one of @operand1,
 *          @operand2.
 * @operand1: First data field operand.
 * @operand2: Second data field operand.
 *
 * Sums two data fields.
 *
 * Since: 1.7
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
 *
 * Since: 1.7
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
 *
 * Since: 1.7
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
 *
 * Since: 1.7
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
 *
 * Since: 1.7
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
 *
 * Since: 1.7
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

    gwy_data_field_invalidate(result);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
