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

#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include "dataline.h"
#include "interpolation.h"
#include "simplefft.h"

#define GWY_DATA_LINE_TYPE_NAME "GwyDataLine"

static void     gwy_data_line_class_init        (GwyDataLineClass *klass);
static void     gwy_data_line_init              (GObject *object);
static void     gwy_data_line_finalize          (GObject *object);
static void     gwy_data_line_serializable_init (GwySerializableIface *iface);
static void     gwy_data_line_watchable_init    (GwyWatchableIface *iface);
static GByteArray* gwy_data_line_serialize      (GObject *obj,
                                                 GByteArray *buffer);
static GObject* gwy_data_line_deserialize       (const guchar *buffer,
                                                 gsize size,
                                                 gsize *position);
static GObject* gwy_data_line_duplicate_real    (GObject *object);
/*static void     gwy_data_line_value_changed     (GObject *object);*/

static GObjectClass *parent_class = NULL;

GType
gwy_data_line_get_type(void)
{
    static GType gwy_data_line_type = 0;

    if (!gwy_data_line_type) {
        static const GTypeInfo gwy_data_line_info = {
            sizeof(GwyDataLineClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_data_line_class_init,
            NULL,
            NULL,
            sizeof(GwyDataLine),
            0,
            (GInstanceInitFunc)gwy_data_line_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_data_line_serializable_init, NULL, 0,
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_data_line_watchable_init, NULL, 0,
        };

        gwy_debug("");
        gwy_data_line_type = g_type_register_static(G_TYPE_OBJECT,
                                                   GWY_DATA_LINE_TYPE_NAME,
                                                   &gwy_data_line_info,
                                                   0);
        g_type_add_interface_static(gwy_data_line_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_data_line_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_data_line_type;
}

static void
gwy_data_line_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_data_line_serialize;
    iface->deserialize = gwy_data_line_deserialize;
    iface->duplicate = gwy_data_line_duplicate_real;
}

static void
gwy_data_line_watchable_init(GwyWatchableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_data_line_class_init(GwyDataLineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_data_line_finalize;
}

static void
gwy_data_line_init(GObject *object)
{
    gwy_debug("");
    gwy_debug_objects_creation(object);
}

static void
gwy_data_line_finalize(GObject *object)
{
    GwyDataLine *data_line = (GwyDataLine*)object;

    gwy_debug("");
    g_free(data_line->data);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

GwyDataLine*
gwy_data_line_new(gint res, gdouble real, gboolean nullme)
{
    GwyDataLine *data_line;

    gwy_debug("");
    data_line = g_object_new(GWY_TYPE_DATA_LINE, NULL);

    data_line->res = res;
    data_line->real = real;
    if (nullme)
        data_line->data = g_new0(gdouble, data_line->res);
    else
        data_line->data = g_new(gdouble, data_line->res);

    return data_line;
}

GwyDataLine*
gwy_data_line_new_alike(GwyDataLine *model,
                        gboolean nullme)
{
    GwyDataLine *data_line;

    g_return_val_if_fail(GWY_IS_DATA_LINE(model), NULL);
    data_line = g_object_new(GWY_TYPE_DATA_LINE, NULL);

    data_line->res = model->res;
    data_line->real = model->real;
    if (nullme)
        data_line->data = g_new0(gdouble, data_line->res);
    else
        data_line->data = g_new(gdouble, data_line->res);

    return data_line;
}


static GByteArray*
gwy_data_line_serialize(GObject *obj,
                        GByteArray *buffer)
{
    GwyDataLine *data_line;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_DATA_LINE(obj), NULL);

    data_line = GWY_DATA_LINE(obj);
    {
        GwySerializeSpec spec[] = {
            { 'i', "res", &data_line->res, NULL, },
            { 'd', "real", &data_line->real, NULL, },
            { 'D', "data", &data_line->data, &data_line->res, },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_DATA_LINE_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_data_line_deserialize(const guchar *buffer,
                          gsize size,
                          gsize *position)
{
    guint32 fsize;
    gint res;
    gdouble real, *data = NULL;
    GwyDataLine *data_line;
    GwySerializeSpec spec[] = {
      { 'i', "res", &res, NULL, },
      { 'd', "real", &real, NULL, },
      { 'D', "data", &data, &fsize, },
    };

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_DATA_LINE_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        return NULL;
    }
    if (fsize != (guint)res) {
        g_critical("Serialized %s size mismatch %u != %u",
              GWY_DATA_LINE_TYPE_NAME, fsize, res);
        g_free(data);
        return NULL;
    }

    /* don't allocate large amount of memory just to immediately free it */
    data_line = gwy_data_line_new(1, real, 0);
    g_free(data_line->data);
    data_line->res = res;
    data_line->data = data;

    return (GObject*)data_line;
}

static GObject*
gwy_data_line_duplicate_real(GObject *object)
{
    GwyDataLine *data_line, *duplicate;

    g_return_val_if_fail(GWY_IS_DATA_LINE(object), NULL);
    data_line = GWY_DATA_LINE(object);
    duplicate = gwy_data_line_new_alike(data_line, FALSE);
    memcpy(duplicate->data, data_line->data, data_line->res*sizeof(gdouble));

    return (GObject*)duplicate;
}

/*
static void
gwy_data_line_value_changed(GObject *object)
{
    gwy_debug("signal: GwyDataLine changed");
    g_signal_emit_by_name(object, "value_changed", NULL);
}
*/

/**
 * gwy_data_line_resample:
 * @data_line: A data line.
 * @res: Desired resolution.
 * @interpolation: Interpolation method to use.
 *
 * Resamples a data line.
 *
 * In otwhe words changes the size of one dimensional field related with data
 * line. The original values are used for resampling using a requested
 * interpolation alorithm.
 **/
void
gwy_data_line_resample(GwyDataLine *data_line,
                       gint res,
                       GwyInterpolationType interpolation)
{
    gdouble *bdata;
    gdouble ratio, pos;
    gint i;

    /* XXX: cannot be here because many functions don't create proper objects
     * g_return_if_fail(GWY_IS_DATA_LINE(data_line)); */
    if (res == data_line->res)
        return;
    g_return_if_fail(res > 1);

    if (interpolation == GWY_INTERPOLATION_NONE) {
        data_line->res = res;
        data_line->data = g_renew(gdouble, data_line->data, data_line->res);
        return;
    }

    bdata = g_new(gdouble, res);
    ratio = (data_line->res - 1.0)/(res - 1.0);

    for (i = 0; i < res; i++) {
        pos = i*ratio;
        if (G_UNLIKELY(pos > data_line->res-1))
            pos = data_line->res-1;
        bdata[i] = gwy_data_line_get_dval(data_line, pos, interpolation);
    }
    g_free(data_line->data);
    data_line->data = bdata;
    data_line->res = res;
}


/**
 * gwy_data_line_resize:
 * @data_line: A data line.
 * @from: where to start
 * @to: where to finish
 *
 * Resamples data line to (@from - @to) and fills it
 * by appropriate original data line part.
 **/
void
gwy_data_line_resize(GwyDataLine *a, gint from, gint to)
{
    gwy_debug("");
    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_if_fail(from >= 0 && to <= a->res);

    /* FIXME: probably must fix callers:
       a->real *= (to - from)/a->res; */
    a->res = to - from;
    memmove(a->data, a->data + from, a->res*sizeof(gdouble));
    a->data = g_renew(gdouble, a->data, a->res*sizeof(gdouble));
}

/**
 * gwy_data_line_copy:
 * @data_line: Source data line.
 * @b: Destination data line.
 *
 * Copies the contents of a data line to another already allocated data line
 * of the same size.
 **/
void
gwy_data_line_copy(GwyDataLine *a, GwyDataLine *b)
{
    gwy_debug("");
    g_return_if_fail(a->res == b->res);

    memcpy(b->data, a->data, a->res*sizeof(gdouble));
}

/**
 * gwy_data_line_get_dval:
 * @data_line: A data line.
 * @x: position requested (0 - resolution)
 * @interpolation: interpolation used
 *
 * Using a specified interpolation returns value
 * in any point wihin data line.
 *
 * Returns: value interpolated in the data line.
 **/
gdouble
gwy_data_line_get_dval(GwyDataLine *a, gdouble x, gint interpolation)
{
    gint l = floor(x);
    gdouble rest = x - (gdouble)l;
    gdouble intline[4];

        gwy_debug("");
    g_return_val_if_fail(x >= 0 && x < (a->res), 0.0);

    if (rest == 0) return a->data[l];

    /*simple (and fast) methods*/
    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        return 0.0;

        case GWY_INTERPOLATION_ROUND:
        return a->data[(gint)(x + 0.5)];

        case GWY_INTERPOLATION_BILINEAR:
        return
            (1 - rest)*a->data[l] + rest*a->data[l+1];
    }

    /*other 4point methods are very similar:*/
    if (l < 1 || l >= (a->res - 2))
        return gwy_data_line_get_dval(a, x, GWY_INTERPOLATION_BILINEAR);

    intline[0] = a->data[l-1];
    intline[1] = a->data[l];
    intline[2] = a->data[l+1];
    intline[3] = a->data[l+2];

    return gwy_interpolation_get_dval_of_equidists(rest, intline, interpolation);
}

/**
 * gwy_data_line_get_data:
 * @data_line: A data line.
 *
 * Gets the data of the line.
 *
 * This function invalidates any cached information, use
 * gwy_data_line_get_data_const() if you are not going to change the data.
 *
 * Returns: The data as an array of doubles of length gwy_data_line_get_res().
 **/
gdouble*
gwy_data_line_get_data(GwyDataLine *data_line)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);
    return data_line->data;
}

/**
 * gwy_data_line_get_data_const:
 * @data_line: A data line.
 *
 * Gets the data of the line, read-only.
 *
 * Use gwy_data_line_get_data() if you want to change the data.
 *
 * Returns: The data as an array of doubles of length gwy_data_line_get_res().
 **/
const gdouble*
gwy_data_line_get_data_const(GwyDataLine *data_line)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);
    return (const gdouble*)data_line->data;
}

/**
 * gwy_data_line_get_res:
 * @data_line: A data line.
 *
 *
 *
 * Returns: Resolution (number of data points).
 **/
gint
gwy_data_line_get_res(GwyDataLine *data_line)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0);
    return data_line->res;
}

/**
 * gwy_data_line_get_real:
 * @data_line: A data line.
 *
 *
 *
 * Returns: Real size of data line.
 **/
gdouble
gwy_data_line_get_real(GwyDataLine *data_line)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0);
    return data_line->real;
}

/**
 * gwy_data_line_set_real:
 * @data_line: A data line.
 * @real: value to be set
 *
 * Sets the real data line size.
 **/
void
gwy_data_line_set_real(GwyDataLine *data_line, gdouble real)
{
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    data_line->real = real;
}

/**
 * gwy_data_line_itor:
 * @data_line: A data line.
 * @pixval: value in pixel coordinates
 *
 *
 *
 * Returns: value in real coordinates.
 **/
gdouble
gwy_data_line_itor(GwyDataLine *data_line, gdouble pixval)
{
    return pixval*data_line->real/data_line->res;
}

/**
 * gwy_data_line_rtoi:
 * @data_line: A data line.
 * @realval: value in real coordinates
 *
 *
 *
 * Returns: value in pixel coordinates.
 **/
gdouble
gwy_data_line_rtoi(GwyDataLine *data_line, gdouble realval)
{
    return realval*data_line->res/data_line->real;
}

/**
 * gwy_data_line_get_val:
 * @data_line: A data line.
 * @i: index (pixel coordinates)
 *
 *
 *
 * Returns: value at given index.
 **/
gdouble
gwy_data_line_get_val(GwyDataLine *data_line, gint i)
{
    g_return_val_if_fail(i >= 0 && i < data_line->res, 0.0);

    return data_line->data[i];
}

/**
 * gwy_data_line_set_val:
 * @data_line:  data line
 * @i: pixel coordinates
 * @value: value to be set
 *
 * Sets the value at given index.
 *
 * Returns: TRUE it there were no problems.
 **/
gboolean
gwy_data_line_set_val(GwyDataLine *data_line, gint i, gdouble value)
{
    g_return_val_if_fail(i >= 0 && i < data_line->res, FALSE);

    data_line->data[i] = value;

    return TRUE;
}


/**
 * gwy_data_line_get_dval_real:
 * @data_line: A data line.
 * @x: real coordinates position
 * @interpolation: interpolation method used
 *
 * Same as gwy_data_line_get_dval() fucntion, but uses
 * real coordinates input.
 *
 * Returns: Value interpolated in the data line.
 **/
gdouble
gwy_data_line_get_dval_real(GwyDataLine *a, gdouble x, gint interpolation)
{
    return gwy_data_line_get_dval(a, gwy_data_line_rtoi(a, x), interpolation);
}

/**
 * gwy_data_line_invert:
 * @data_line: A data line.
 * @x: invert x axis
 * @z: invert z axis
 *
 * Inverts values. If  @x is TRUE it inverts
 * x-axis values (x1...xn) to (xn...x1), if
 * @z is TRUE inverts z-axis values (peaks to valleys
 * and valleys to peaks).
 **/
void
gwy_data_line_invert(GwyDataLine *a, gboolean x, gboolean z)
{
    gint i;
    gdouble avg;
    GwyDataLine b;

    gwy_debug("");
    if (x) {
        b.res = a->res;
        b.data = g_new(gdouble, a->res);
        gwy_data_line_copy(a, &b);

        for (i = 0; i < a->res; i++)
            a->data[i] = b.data[i - a->res - 1];
    }
    if (z) {
        avg = gwy_data_line_get_avg(a);
        for (i = 0; i < a->res; i++)
            a->data[i] = 2*avg - a->data[i];
    }
    g_free(b.data);
}

/**
 * gwy_data_line_fill:
 * @data_line: A data line.
 * @value: value to be used for filling
 *
 * Fills whole data lien with specified number.
 **/
void
gwy_data_line_fill(GwyDataLine *a, gdouble value)
{
    gint i;

    for (i = 0; i < a->res; i++)
        a->data[i] = value;
}

/**
 * gwy_data_line_add:
 * @data_line: A data line.
 * @value: value to be added.
 *
 * Adds a specified number to whole data line.
 **/
void
gwy_data_line_add(GwyDataLine *a, gdouble value)
{
    gint i;

    for (i = 0; i < a->res; i++)
        a->data[i] += value;
}

/**
 * gwy_data_line_multiply:
 * @data_line: A data line.
 * @value: value to be used for multiplication
 *
 * Multiplies whole data line with a specified number.
 **/
void
gwy_data_line_multiply(GwyDataLine *a, gdouble value)
{
    gint i;

    for (i = 0; i < a->res; i++)
        a->data[i] *= value;
}

/**
 * gwy_data_line_part_fill:
 * @data_line: A data line.
 * @from: where to start
 * @to: where to finish
 * @value: value to be used for filling
 *
 * Fills specified part of data line with specified number
 **/
void
gwy_data_line_part_fill(GwyDataLine *a, gint from, gint to, gdouble value)
{
    gint i;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_if_fail(from >= 0 && to <= a->res);

    for (i = from; i < to; i++)
        a->data[i] = value;
}

/**
 * gwy_data_line_part_add:
 * @data_line: A data line.
 * @from: where to start
 * @to: where to finish
 * @value: value to be added
 *
 * Adds specified number to a part of data line.
 **/
void
gwy_data_line_part_add(GwyDataLine *a, gint from, gint to, gdouble value)
{
    gint i;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_if_fail(from >= 0 && to <= a->res);

    for (i = from; i < to; i++)
        a->data[i] += value;
}

/**
 * gwy_data_line_part_multiply:
 * @data_line: A data line.
 * @from: where to start
 * @to: where to finish
 * @value: value to be used for multiplication
 *
 * Multiplies specified part of data line by specified number
 **/
void
gwy_data_line_part_multiply(GwyDataLine *a, gint from, gint to, gdouble value)
{
    gint i;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_if_fail(from >= 0 && to <= a->res);

    for (i = from; i < to; i++)
        a->data[i] *= value;
}

/**
 * gwy_data_line_get_max:
 * @data_line: A data line.
 *
 *
 *
 * Returns: Maximum value
 **/
gdouble
gwy_data_line_get_max(GwyDataLine *a)
{
    gint i;
    gdouble max = a->data[0];

    for (i = 1; i < a->res; i++) {
        if (max < a->data[i])
            max = a->data[i];
    }
    return max;
}

/**
 * gwy_data_line_get_min:
 * @data_line: A data line.
 *
 *
 *
 * Returns: Minimum value
 **/
gdouble
gwy_data_line_get_min(GwyDataLine *a)
{
    gint i;
    gdouble min = a->data[0];

    for (i = 1; i < a->res; i++) {
        if (min > a->data[i])
            min = a->data[i];
    }
    return min;
}

/**
 * gwy_data_line_get_avg:
 * @data_line: A data line.
 *
 *
 *
 * Returns: Average value
 **/
gdouble
gwy_data_line_get_avg(GwyDataLine *a)
{
    gint i;
    gdouble avg = 0;

    for (i = 0; i < a->res; i++)
        avg += a->data[i];

    return avg/(gdouble)a->res;
}

/**
 * gwy_data_line_get_rms:
 * @data_line: A data line.
 *
 *
 *
 * Returns: Root mean square deviation of heights
 **/
gdouble
gwy_data_line_get_rms(GwyDataLine *a)
{
    gint i;
    gdouble sum2 = 0;
    gdouble sum = gwy_data_line_get_sum(a);

    for (i = 0; i < a->res; i++)
        sum2 += a->data[i]*a->data[i];

    return sqrt(fabs(sum2 - sum*sum/a->res)/a->res);
}

/**
 * gwy_data_line_get_sum:
 * @data_line: A data line.
 *
 *
 *
 * Returns: sum of all the values
 **/
gdouble
gwy_data_line_get_sum(GwyDataLine *a)
{
    gint i;
    gdouble sum = 0;

    for (i = 0; i < a->res; i++)
        sum += a->data[i];

    return sum;
}


/**
 * gwy_data_line_part_get_max:
 * @data_line: A data line.
 * @from: where to start (in pixels)
 * @to: where to finish (in pixels)
 *
 *
 *
 * Returns: Maximum within given interval
 **/
gdouble
gwy_data_line_part_get_max(GwyDataLine *a, gint from, gint to)
{
    gint i;
    gdouble max = -G_MAXDOUBLE;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to <= a->res, max);

    for (i = from; i < to; i++) {
        if (max < a->data[i])
            max = a->data[i];
    }
    return max;
}

/**
 * gwy_data_line_part_get_min:
 * @data_line: A data line.
 * @from: where to start (in pixels)
 * @to: where to finish (in pixels)
 *
 *
 *
 * Returns: Minimum within given interval
 **/
gdouble
gwy_data_line_part_get_min(GwyDataLine *a, gint from, gint to)
{
    gint i;
    gdouble min = G_MAXDOUBLE;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to <= a->res, min);

    for (i = from; i < to; i++) {
        if (min > a->data[i])
            min = a->data[i];
    }

    return min;
}

/**
 * gwy_data_line_part_get_avg:
 * @data_line: A data line.
 * @from: where to start (in pixels)
 * @to: where to finish (in pixels)
 *
 *
 *
 * Returns: Average within given interval
 **/
gdouble
gwy_data_line_part_get_avg(GwyDataLine *a, gint from, gint to)
{
    return gwy_data_line_part_get_sum(a, from, to)/(gdouble)(to-from);
}

/**
 * gwy_data_line_part_get_rms:
 * @data_line: A data line.
 * @from: where to start (in pixels)
 * @to: where to finish (in pixels)
 *
 *
 *
 * Returns: Root mean square deviation of heights within a given interval
 **/
gdouble
gwy_data_line_part_get_rms(GwyDataLine *a, gint from, gint to)
{
    gint i;
    gdouble rms = 0;
    gdouble avg;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to <= a->res, rms);

    avg = gwy_data_line_part_get_avg(a, from, to);
    for (i = from; i < to; i++)
        rms += (avg - a->data[i])*(avg - a->data[i]);

    return sqrt(rms)/(gdouble)(to-from);
}

/**
 * gwy_data_line_part_get_sum:
 * @data_line: A data line.
 * @from: where to start (in pixels)
 * @to: where to finish (in pixels)
 *
 *
 *
 * Returns: Sum of all values within the interval
 **/
gdouble
gwy_data_line_part_get_sum(GwyDataLine *a, gint from, gint to)
{
    gint i;
    gdouble sum = 0;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to <= a->res, sum);

    for (i = from; i < to; i++)
        sum += a->data[i];

    return sum;
}

/**
 * gwy_data_line_threshold:
 * @data_line: A data line.
 * @threshval: value used for thresholding
 * @bottom: lower value
 * @top: upper value
 *
 * Sets all the values to @bottom or @top value
 * depending on whether the original values are
 * below or above @threshold value
 *
 * Returns: total number of values above threshold
 **/
gint
gwy_data_line_threshold(GwyDataLine *a,
                        gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, tot=0;

    for (i = 0; i < a->res; i++) {
        if (a->data[i] < threshval)
            a->data[i] = bottom;
        else {
            a->data[i] = top;
            tot++;
        }
    }
    return tot;
}

/**
 * gwy_data_line_part_threshold:
 * @data_line: A data line.
 * @from: where to start
 * @to: where to finish
 * @threshval: value used for thresholding
 * @bottom: lower value
 * @top: upper value
 *
 *  Sets all the values within interval to @bottom or @top value
 * depending on whether the original values are
 * below or above @threshold value.
 *
 * Returns: total number of values above threshold within interval
 **/
gint
gwy_data_line_part_threshold(GwyDataLine *a,
                             gint from, gint to,
                             gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, tot=0;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to <= a->res, 0);

    for (i = from; i < to; i++) {
        if (a->data[i] < threshval)
            a->data[i] = bottom;
        else {
            a->data[i] = top;
            tot++;
        }
    }
    return tot;
}

/**
 * gwy_data_line_line_coeffs:
 * @data_line: A data line.
 * @av: height coefficient
 * @bv: slope coeficient
 *
 * Finds coefficients that can be used for line
 * leveling using relation data[i] -= av + bv*real_index;
 **/
void
gwy_data_line_line_coeffs(GwyDataLine *a, gdouble *av, gdouble *bv)
{
    gdouble sumxi, sumxixi;
    gdouble sumsixi = 0.0;
    gdouble sumsi = 0.0;
    gdouble n = a->res;
    gdouble *pdata;
    gint i;

    sumxi = (n-1)/2;
    sumxixi = (2*n-1)*(n-1)/6;

    pdata = a->data;
    for (i = a->res; i; i--) {
        sumsi += *pdata;
        sumsixi += *pdata * i;
        pdata++;
    }
    sumsi /= n;
    sumsixi /= n;

    if (bv) {
        *bv = (sumsixi - sumsi*sumxi) / (sumxixi - sumxi*sumxi);
        *bv *= n/a->real;
    }
    if (av)
        *av = (sumsi*sumxixi - sumxi*sumsixi) / (sumxixi - sumxi*sumxi);
}

/**
 * gwy_data_line_line_level:
 * @data_line: A data line.
 * @av: height coefficient
 * @bv: slope coefficient
 *
 * Performs line leveling using relation data[i] -= av + bv*real_index.
 **/
void
gwy_data_line_line_level(GwyDataLine *a, gdouble av, gdouble bv)
{
    gint i;
    gdouble bpix = bv/a->res*a->real;

        gwy_debug("");
    for (i = 0; i < a->res; i++)
        a->data[i] -= av + bpix*i;
}

/**
 * gwy_data_line_line_rotate:
 * @data_line: A data line.
 * @angle: angle of rotation (in radians)
 * @interpolation: interpolation mode used
 *
 * Performs line rotation. This is operation similar
 * to leveling, but not changing the angles between
 * line segments.
 **/
void
gwy_data_line_line_rotate(GwyDataLine *a, gdouble angle, gint interpolation)
{
    gint i, k, maxi, res;
    gdouble ratio, x, as, radius, xl1, xl2, yl1, yl2;
    GwyDataLine dx, dy;

        gwy_debug("");
    if (angle == 0)
        return;

    res = dx.res = dy.res = a->res;
    dx.data = g_new(gdouble, a->res);
    dy.data = g_new(gdouble, a->res);

    ratio = a->real/(double)a->res;
    dx.data[0] = 0;
    dy.data[0] = a->data[0];
    for (i = 1; i < a->res; i++) {
        as = atan(a->data[i]/((double)i*ratio));
        radius = sqrt(((double)i*ratio)*((double)i*ratio)
                      + a->data[i]*a->data[i]);
        dx.data[i] = radius*cos((as+angle));
        dy.data[i] = radius*sin((as+angle));
    }

    k = 0;
    maxi = 0;
    for (i = 1; i < a->res; i++) {
        x = i*ratio;
        k = 0;
        do {
            k++;
        } while (dx.data[k] < x && k < a->res);

        if (k >= (a->res-1)) {
            maxi=i;
            break;
        }

        xl1 = dx.data[k-1];
        xl2 = dx.data[k];
        yl1 = dy.data[k-1];
        yl2 = dy.data[k];


        if (interpolation == GWY_INTERPOLATION_ROUND
            || interpolation == GWY_INTERPOLATION_BILINEAR)
            a->data[i] = gwy_interpolation_get_dval(x, xl1, yl1, xl2, yl2,
                                                    interpolation);
        else
            g_warning("Interpolation not implemented yet.\n");
    }
    if (maxi != 0)
        gwy_data_line_resize(a, 0, maxi);

    if (a->res != res) gwy_data_line_resample(a, res, interpolation);

    /* XXX: where was this freed? */
    g_free(dx.data);
    g_free(dy.data);
}

/**
 * gwy_data_line_get_der:
 * @data_line: A data line.
 * @i: pixel coordinate
 *
 *
 *
 * Returns: derivation at given pixel
 **/
gdouble
gwy_data_line_get_der(GwyDataLine *a, gint i)
{
    g_return_val_if_fail(i >= 0 && i < a->res, 0.0);

    if (i == 0)
        return (a->data[1] - a->data[0])*a->res/a->real;
    if (i == (a->res-1))
        return (a->data[i] - a->data[i-1])*a->res/a->real;
    return (a->data[i+1] - a->data[i-1])*a->res/a->real/2;
}


/**
 * gwy_data_line_fft_hum:
 * @direction: FFT direction (1 or -1)
 * @ra: real input
 * @ia: imaginary input
 * @rb: real output
 * @ib: imaginary output
 * @interpolation: interpolation used
 *
 * Performs 1D FFT using the alogrithm ffthum (see simplefft.h).
 * Resamples data to closest 2^N and then resamples result back.
 * Resample data by yourself if you want further FFT processing as
 * resampling of the FFT spectrum can destroy some information in it.
 **/
void
gwy_data_line_fft_hum(gint direction,
                      GwyDataLine *ra, GwyDataLine *ia,
                      GwyDataLine *rb, GwyDataLine *ib,
                      gint interpolation)
{
    gint order, newres, oldres;

    /*this should never happen - the function should be called from gwy_data_line_fft()*/
    if (ia->res != ra->res)
        gwy_data_line_resample(ia, ra->res, GWY_INTERPOLATION_NONE);
    if (rb->res != ra->res)
        gwy_data_line_resample(rb, ra->res, GWY_INTERPOLATION_NONE);
    if (ib->res != ra->res)
        gwy_data_line_resample(ib, ra->res, GWY_INTERPOLATION_NONE);

    /*find the next power of two*/
    order = (gint) floor(log ((gdouble)ra->res)/log (2.0)+0.5);
    newres = (gint) pow(2,order);
    oldres = ra->res;

    /*resample if this is not the resolution*/
    if (newres != oldres) {
        gwy_data_line_resample(ra, newres, interpolation);
        gwy_data_line_resample(ia, newres, interpolation);
        gwy_data_line_resample(rb, newres, GWY_INTERPOLATION_NONE);
        gwy_data_line_resample(ib, newres, GWY_INTERPOLATION_NONE);
    }
    gwy_data_line_fill(rb, 0);
    gwy_data_line_fill(ib, 0);

    gwy_fft_hum(direction, ra->data, ia->data, rb->data, ib->data, newres);


    /*FIXME interpolation can dramatically alter the spectrum. Do it preferably
     after all the processings*/
    if (newres != oldres) {
        gwy_data_line_resample(ra, oldres, interpolation);
        gwy_data_line_resample(ia, oldres, interpolation);
        gwy_data_line_resample(rb, oldres, interpolation);
        gwy_data_line_resample(ib, oldres, interpolation);
    }

}


/**
 * gwy_data_line_fft:
 * @ra: real input
 * @ia: imaginary input
 * @rb: real output
 * @ib: imaginary output
 * @fft: fft alorithm
 * @windowing: windowing mode
 * @direction: FFT direction (1 or -1)
 * @interpolation: interpolation mode
 * @preserverms: preserve RMS value while windowing
 * @level: level line before computation
 *
 * Performs Fast Fourier transform using a given algorithm.
 * A windowing or data leveling can be applied if requested.
 **/
void
gwy_data_line_fft(GwyDataLine *ra, GwyDataLine *ia,
                  GwyDataLine *rb, GwyDataLine *ib,
                  void (*fft)(), GwyWindowingType windowing, gint direction,
                  GwyInterpolationType interpolation, gboolean preserverms,
                  gboolean level)
{
    gint i, n;
    gdouble rmsa, rmsb;
    GwyDataLine *multra, *multia;
    gdouble coefs[4];

    g_return_if_fail(GWY_IS_DATA_LINE(ra));
    g_return_if_fail(GWY_IS_DATA_LINE(ia));
    g_return_if_fail(GWY_IS_DATA_LINE(rb));
    g_return_if_fail(GWY_IS_DATA_LINE(ib));

    gwy_debug("");
    if (ia->res != ra->res)
    {
        gwy_data_line_resample(ia, ra->res, GWY_INTERPOLATION_NONE);
        gwy_data_line_fill(ia, 0);
    }
    if (rb->res != ra->res)
        gwy_data_line_resample(rb, ra->res, GWY_INTERPOLATION_NONE);
    if (ib->res != ra->res)
        gwy_data_line_resample(ib, ra->res, GWY_INTERPOLATION_NONE);

    if (level == TRUE) {
        n = 1;
        gwy_data_line_fit_polynom(ra, n, coefs);
        gwy_data_line_subtract_polynom(ra, n, coefs);
        gwy_data_line_fit_polynom(ia, n, coefs);
        gwy_data_line_subtract_polynom(ia, n, coefs);
    }

    gwy_data_line_fill(rb, 0);
    gwy_data_line_fill(ib, 0);


    if (preserverms == TRUE) {
        multra = gwy_data_line_duplicate(ra);
        multia = gwy_data_line_duplicate(ia);

        rmsa = gwy_data_line_get_rms(multra);

        gwy_fft_window(multra->data, multra->res, windowing);
        gwy_fft_window(multia->data, multia->res, windowing);

        (*fft)(direction, multra, multia, rb, ib, interpolation);

        rmsb = 0;
        for (i = 0; i < multra->res/2; i++)
            rmsb += (rb->data[i]*rb->data[i] + ib->data[i]*ib->data[i])*2
                    /(ra->res*ra->res);
        rmsb = sqrt(rmsb);

        gwy_data_line_multiply(rb, rmsa/rmsb);
        gwy_data_line_multiply(ib, rmsa/rmsb);
        g_object_unref(multra);
        g_object_unref(&multia);
    }
    else {
        gwy_fft_window(ra->data, ra->res, windowing);
        gwy_fft_window(ia->data, ra->res, windowing);

        (*fft)(direction, ra, ia, rb, ib, interpolation);
    }
}

/**
 * gwy_data_line_part_fit_polynom:
 * @data_line: A data line.
 * @n: Polynom degree.
 * @coeffs: An array of size @n+1 to store the coefficients to, or %NULL
 *          (a fresh array is allocated then).
 * @from: The index in @data_line to start from (inclusive).
 * @to: The index in @data_line to stop (noninclusive).
 *
 * Fits a polynom through a part of a data line.
 *
 * Please see gwy_data_line_fit_polynom() for more details.
 *
 * Returns: The coefficients of the polynom (@coeffs when it was not %NULL,
 *          otherwise a newly allocated array).
 **/
gdouble*
gwy_data_line_part_fit_polynom(GwyDataLine *data_line,
                               gint n, gdouble *coeffs,
                               gint from, gint to)
{
    gdouble *sumx, *m;
    gint i, j;
    gdouble *data;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), NULL);
    g_return_val_if_fail(n >= 0, NULL);
    data = data_line->data;

    if (to < from)
        GWY_SWAP(gint, from, to);

    sumx = g_new0(gdouble, 2*n+1);
    if (!coeffs)
        coeffs = g_new0(gdouble, n+1);
    else
        memset(coeffs, 0, (n+1)*sizeof(gdouble));

    for (i = from; i < to; i++) {
        gdouble x = i;
        gdouble y = data[i];
        gdouble xp;

        xp = 1.0;
        for (j = 0; j <= n; j++) {
            sumx[j] += xp;
            coeffs[j] += xp*y;
            xp *= x;
        }
        for (j = n+1; j <= 2*n; j++) {
            sumx[j] += xp;
            xp *= x;
        }
    }

    m = g_new(gdouble, (n+1)*(n+2)/2);
    for (i = 0; i <= n; i++) {
        gdouble *row = m + i*(i+1)/2;

        for (j = 0; j <= i; j++)
            row[j] = sumx[i+j];
    }
    if (!gwy_math_choleski_decompose(n+1, m))
        memset(coeffs, 0, (n+1)*sizeof(gdouble));
    else
        gwy_math_choleski_solve(n+1, m, coeffs);

    g_free(m);
    g_free(sumx);

    return coeffs;
}

/**
 * gwy_data_line_fit_polynom:
 * @data_line: A data line.
 * @n: Polynom degree.
 * @coeffs: An array of size @n+1 to store the coefficients to, or %NULL
 *          (a fresh array is allocated then).
 *
 * Fits a polynom through a data line.
 *
 * Note @n is polynom degree, so the size of @coeffs is @n+1.  X-values
 * are indices in the data line.
 *
 * For polynoms of degree 0 and 1 it's better to use gwy_data_line_get_avg()
 * and gwy_data_line_line_coeffs() because they are faster.
 *
 * Returns: The coefficients of the polynom (@coeffs when it was not %NULL,
 *          otherwise a newly allocated array).
 **/
gdouble*
gwy_data_line_fit_polynom(GwyDataLine *data_line,
                          gint n, gdouble *coeffs)
{
    return gwy_data_line_part_fit_polynom(data_line, n, coeffs,
                                          0, gwy_data_line_get_res(data_line));
}


void gwy_data_line_part_subtract_polynom(GwyDataLine *data_line,
                                         gint n, gdouble *coeffs, gint from, gint to)
{
    gint i, j;
    gdouble val;

    if (to < from)
        GWY_SWAP(gint, from, to);

    for (i = from; i < to; i++) {
        val = 0.0;
        for (j = n; j; j--) {
            val += coeffs[j];
            val *= i;
        }
        val += coeffs[0];

        data_line->data[i] -= val;
    }

}

void
gwy_data_line_subtract_polynom(GwyDataLine *data_line,
                               gint n, gdouble *coeffs)
{
    gwy_data_line_part_subtract_polynom(data_line, n, coeffs,
                                        0, gwy_data_line_get_res(data_line));
}

/**
 * gwy_data_line_part_get_modus:
 * @data_line: A data line.
 * @from: The index in @data_line to start from (inclusive).
 * @to: The index in @data_line to stop (noninclusive).
 * @histogram_steps: Number of histogram steps used for modus searching,
 *                   pass a nonpositive number to autosize.
 *
 * Finds approximate modus of a data line part.
 *
 * Returns: The modus.
 **/
gdouble
gwy_data_line_part_get_modus(GwyDataLine *data_line,
                             gint from, gint to,
                             gint histogram_steps)
{
    gint *histogram;
    gint n, i, j, m;
    gdouble min, max, sum;

    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0);
    g_return_val_if_fail(from >= 0 && to <= data_line->res, 0);
    g_return_val_if_fail(from != to, 0);

    if (from > to)
        GWY_SWAP(gint, from, to);
    n = to - from;

    if (n == 1)
        return data_line->data[from];

    if (histogram_steps < 1) {
        /*
        gdouble sigma = gwy_data_line_part_get_rms(data_line, from, to);
        histogram_steps = floor(0.49*sigma*pow(n, 1.0/3.0) + 0.5);
        */
        histogram_steps = floor(3.49*pow(n, 1.0/3.0) + 0.5);
        gwy_debug("histogram_steps = %d", histogram_steps);
    }

    min = gwy_data_line_part_get_min(data_line, from, to);
    max = gwy_data_line_part_get_max(data_line, from, to);
    if (min == max)
        return min;

    histogram = g_new0(gint, histogram_steps);
    for (i = from; i < to; i++) {
        j = (data_line->data[i] - min)/(max - min)*histogram_steps;
        j = CLAMP(j, 0, histogram_steps-1);
        histogram[j]++;
    }

    m = 0;
    for (i = 1; i < histogram_steps; i++) {
        if (histogram[i] > histogram[m])
            m = i;
    }

    n = 0;
    sum = 0.0;
    for (i = from; i < to; i++) {
        j = (data_line->data[i] - min)/(max - min)*histogram_steps;
        j = CLAMP(j, 0, histogram_steps-1);
        if (j == m) {
            sum += data_line->data[i];
            n++;
        }
    }

    g_free(histogram);
    gwy_debug("modus = %g", sum/n);

    return sum/n;
}

/**
 * gwy_data_line_get_modus:
 * @data_line: A data line.
 * @histogram_steps: Number of histogram steps used for modus searching,
 *                   pass a nonpositive number to autosize.
 *
 * Finds approximate modus of a data line.
 *
 * As each number in the data line is usually unique, this function does not
 * return modus of the data itself, but modus of a histogram.
 *
 * Returns: The modus.
 **/
gdouble
gwy_data_line_get_modus(GwyDataLine *data_line,
                        gint histogram_steps)
{
    g_return_val_if_fail(GWY_IS_DATA_LINE(data_line), 0);

    return gwy_data_line_part_get_modus(data_line, 0, data_line->res,
                                        histogram_steps);
}

/************************** Documentation ****************************/

/**
 * GwyDataLine:
 *
 * The #GwyDataLine struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * GwySFOutputType:
 * @GWY_SF_OUTPUT_DH: Distribution of heights.
 * @GWY_SF_OUTPUT_CDH: Cumulative distribution of heights.
 * @GWY_SF_OUTPUT_DA: Distribution of angles (slopes).
 * @GWY_SF_OUTPUT_CDA: Cumulative distribution of angles (slopes).
 * @GWY_SF_OUTPUT_ACF: Autocorrelation fucntions.
 * @GWY_SF_OUTPUT_HHCF: Height-height correlation function.
 * @GWY_SF_OUTPUT_PSDF: Power spectral density fucntion.
 *
 * Statistical function type.
 **/

/**
 * gwy_data_line_duplicate:
 * @data_line: A data line to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
