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
#include <stdlib.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/datafield.h>
#include <libprocess/interpolation.h>
#include <libprocess/stats.h>

#define GWY_DATA_FIELD_TYPE_NAME "GwyDataField"

/* Cache operations */
#define CVAL(datafield, b)  ((datafield)->cache[GWY_DATA_FIELD_CACHE_##b])
#define CBIT(b)             (1 << GWY_DATA_FIELD_CACHE_##b)
#define CTEST(datafield, b) ((datafield)->cached & CBIT(b))

enum {
    DATA_CHANGED,
    LAST_SIGNAL
};

static void       gwy_data_field_finalize         (GObject *object);
static void       gwy_data_field_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_data_field_serialize       (GObject *obj,
                                                   GByteArray *buffer);
static gsize      gwy_data_field_get_size         (GObject *obj);
static GObject*   gwy_data_field_deserialize      (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject*   gwy_data_field_duplicate_real   (GObject *object);
static void       gwy_data_field_clone_real       (GObject *source,
                                                   GObject *copy);

static guint data_field_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_EXTENDED
    (GwyDataField, gwy_data_field, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_data_field_serializable_init))

static void
gwy_data_field_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_data_field_serialize;
    iface->deserialize = gwy_data_field_deserialize;
    iface->get_size = gwy_data_field_get_size;
    iface->duplicate = gwy_data_field_duplicate_real;
    iface->clone = gwy_data_field_clone_real;
}

static void
gwy_data_field_class_init(GwyDataFieldClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_data_field_finalize;

/**
 * GwyDataField::data-changed:
 * @gwydatafield: The #GwyDataField which received the signal.
 *
 * The ::data-changed signal is never emitted by data field itself.  It
 * is intended as a means to notify others data field users they should
 * update themselves.
 */
    data_field_signals[DATA_CHANGED]
        = g_signal_new("data-changed",
                       G_OBJECT_CLASS_TYPE(gobject_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyDataFieldClass, data_changed),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_data_field_init(GwyDataField *data_field)
{
    gwy_debug_objects_creation(G_OBJECT(data_field));
}

static void
gwy_data_field_finalize(GObject *object)
{
    GwyDataField *data_field = (GwyDataField*)object;

    gwy_debug("%p is dying!", data_field);
    gwy_object_unref(data_field->si_unit_xy);
    gwy_object_unref(data_field->si_unit_z);
    g_free(data_field->data);

    G_OBJECT_CLASS(gwy_data_field_parent_class)->finalize(object);
}

/**
 * gwy_data_field_new:
 * @xres: X-resolution, i.e., the number of columns.
 * @yres: Y-resolution, i.e., the number of rows.
 * @xreal: Real horizontal physical dimension.
 * @yreal: Real vertical physical dimension.
 * @nullme: Whether the data field should be initialized to zeroes. If %FALSE,
 *          the data will not be initialized.
 *
 * Creates a new data field.
 *
 * Returns: A newly created data field.
 **/
GwyDataField*
gwy_data_field_new(gint xres, gint yres,
                   gdouble xreal, gdouble yreal,
                   gboolean nullme)
{
    GwyDataField *data_field;

    data_field = g_object_new(GWY_TYPE_DATA_FIELD, NULL);

    data_field->xreal = xreal;
    data_field->yreal = yreal;
    data_field->xres = xres;
    data_field->yres = yres;
    if (nullme) {
        data_field->data = g_new0(gdouble, data_field->xres*data_field->yres);
        /* We can precompute stats */
        data_field->cached = CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                             | CBIT(MED) | CBIT(ARF) | CBIT(ART);
        /* Values cleared implicitely */
    }
    else
        data_field->data = g_new(gdouble, data_field->xres*data_field->yres);

    return data_field;
}

/**
 * gwy_data_field_new_alike:
 * @model: A data field to take resolutions and units from.
 * @nullme: Whether the data field should be initialized to zeroes. If %FALSE,
 *          the data will not be initialized.
 *
 * Creates a new data field similar to an existing one.
 *
 * Use gwy_data_field_duplicate() if you want to copy a data field including
 * data.
 *
 * Returns: A newly created data field.
 **/
GwyDataField*
gwy_data_field_new_alike(GwyDataField *model,
                         gboolean nullme)
{
    GwyDataField *data_field;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(model), NULL);
    data_field = g_object_new(GWY_TYPE_DATA_FIELD, NULL);

    data_field->xreal = model->xreal;
    data_field->yreal = model->yreal;
    data_field->xres = model->xres;
    data_field->yres = model->yres;
    data_field->xoff = model->xoff;
    data_field->yoff = model->yoff;
    if (nullme) {
        data_field->data = g_new0(gdouble, data_field->xres*data_field->yres);
        /* We can precompute stats */
        data_field->cached = CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                             | CBIT(MED) | CBIT(ARF) | CBIT(ART);
        /* Values cleared implicitely */
    }
    else
        data_field->data = g_new(gdouble, data_field->xres*data_field->yres);

    if (model->si_unit_xy)
        data_field->si_unit_xy = gwy_si_unit_duplicate(model->si_unit_xy);
    if (model->si_unit_z)
        data_field->si_unit_z = gwy_si_unit_duplicate(model->si_unit_z);

    return data_field;
}

static GByteArray*
gwy_data_field_serialize(GObject *obj,
                         GByteArray *buffer)
{
    GwyDataField *data_field;
    guint32 datasize, cachesize;
    gdouble *pxoff, *pyoff;
    gdouble *cache;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_DATA_FIELD(obj), NULL);

    data_field = GWY_DATA_FIELD(obj);
    if (!data_field->si_unit_xy)
        data_field->si_unit_xy = gwy_si_unit_new("");
    if (!data_field->si_unit_z)
        data_field->si_unit_z = gwy_si_unit_new("");
    datasize = data_field->xres*data_field->yres;
    cachesize = GWY_DATA_FIELD_CACHE_SIZE;
    cache = data_field->cache;
    pxoff = data_field->xoff ? &data_field->xoff : NULL;
    pyoff = data_field->yoff ? &data_field->yoff : NULL;
    {
        GwySerializeSpec spec[] = {
            { 'i', "xres", &data_field->xres, NULL, },
            { 'i', "yres", &data_field->yres, NULL, },
            { 'd', "xreal", &data_field->xreal, NULL, },
            { 'd', "yreal", &data_field->yreal, NULL, },
            { 'd', "xoff", pxoff, NULL, },
            { 'd', "yoff", pyoff, NULL, },
            { 'o', "si_unit_xy", &data_field->si_unit_xy, NULL, },
            { 'o', "si_unit_z", &data_field->si_unit_z, NULL, },
            { 'D', "data", &data_field->data, &datasize, },
            { 'i', "cache_bits", &data_field->cached, NULL, },
            { 'D', "cache_data", &cache, &cachesize, },
        };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_DATA_FIELD_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_data_field_get_size(GObject *obj)
{
    GwyDataField *data_field;
    guint32 datasize, cachesize;
    gdouble *cache;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_DATA_FIELD(obj), 0);

    data_field = GWY_DATA_FIELD(obj);
    if (!data_field->si_unit_xy)
        data_field->si_unit_xy = gwy_si_unit_new("");
    if (!data_field->si_unit_z)
        data_field->si_unit_z = gwy_si_unit_new("");
    datasize = data_field->xres*data_field->yres;
    cache = data_field->cache;
    cachesize = GWY_DATA_FIELD_CACHE_SIZE;
    {
        GwySerializeSpec spec[] = {
            { 'i', "xres", &data_field->xres, NULL, },
            { 'i', "yres", &data_field->yres, NULL, },
            { 'd', "xreal", &data_field->xreal, NULL, },
            { 'd', "yreal", &data_field->yreal, NULL, },
            { 'd', "xoff", &data_field->xoff, NULL, },
            { 'd', "yoff", &data_field->yoff, NULL, },
            { 'o', "si_unit_xy", &data_field->si_unit_xy, NULL, },
            { 'o', "si_unit_z", &data_field->si_unit_z, NULL, },
            { 'D', "data", &data_field->data, &datasize, },
            { 'i', "cache_bits", &data_field->cached, NULL, },
            { 'D', "cache_data", &cache, &cachesize, },
        };
        return gwy_serialize_get_struct_size(GWY_DATA_FIELD_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_data_field_deserialize(const guchar *buffer,
                           gsize size,
                           gsize *position)
{
    guint32 datasize, cachesize = 0, cachebits = 0;
    gint xres, yres;
    gdouble xreal, yreal, xoff = 0.0, yoff = 0.0, *data = NULL, *cache = NULL;
    GwySIUnit *si_unit_xy = NULL, *si_unit_z = NULL;
    GwyDataField *data_field;
    GwySerializeSpec spec[] = {
        { 'i', "xres", &xres, NULL, },
        { 'i', "yres", &yres, NULL, },
        { 'd', "xreal", &xreal, NULL, },
        { 'd', "yreal", &yreal, NULL, },
        { 'd', "xoff", &xoff, NULL, },
        { 'd', "yoff", &yoff, NULL, },
        { 'o', "si_unit_xy", &si_unit_xy, NULL, },
        { 'o', "si_unit_z", &si_unit_z, NULL, },
        { 'D', "data", &data, &datasize, },
        { 'i', "cache_bits", &cachebits, NULL, },
        { 'D', "cache_data", &cache, &cachesize, },
    };

    gwy_debug("");

    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_DATA_FIELD_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        g_free(cache);
        gwy_object_unref(si_unit_xy);
        gwy_object_unref(si_unit_z);
        return NULL;
    }
    if (datasize != (gsize)(xres*yres)) {
        g_critical("Serialized %s size mismatch %u != %u",
                   GWY_DATA_FIELD_TYPE_NAME, datasize, xres*yres);
        g_free(data);
        g_free(cache);
        gwy_object_unref(si_unit_xy);
        gwy_object_unref(si_unit_z);
        return NULL;
    }

    /* don't allocate large amount of memory just to immediately free it */
    data_field = gwy_data_field_new(1, 1, xreal, yreal, FALSE);
    g_free(data_field->data);
    data_field->data = data;
    data_field->xres = xres;
    data_field->yres = yres;
    data_field->xoff = xoff;
    data_field->yoff = yoff;
    if (si_unit_z) {
        if (data_field->si_unit_z != NULL)
            gwy_object_unref(data_field->si_unit_z);
        data_field->si_unit_z = si_unit_z;
    }
    if (si_unit_xy) {
        if (data_field->si_unit_xy != NULL)
            gwy_object_unref(data_field->si_unit_xy);
        data_field->si_unit_xy = si_unit_xy;
    }

    /* Copy what we can from deserialized cache, it may be longer, it may be
     * shorter, it maybe whatever. */
    cachesize = MIN(cachesize, GWY_DATA_FIELD_CACHE_SIZE);
    if (cache && cachebits && cachesize) {
        gwy_debug("deserialized cache bits: %08x, size = %u",
                  cachebits, cachesize);
        memcpy(data_field->cache, cache, cachesize*sizeof(gdouble));
        data_field->cached = cachebits;
        data_field->cached &= G_MAXUINT32 ^ (G_MAXUINT32 << cachesize);
    }
    g_free(cache);

    return (GObject*)data_field;
}

static GObject*
gwy_data_field_duplicate_real(GObject *object)
{
    GwyDataField *data_field, *duplicate;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(object), NULL);
    data_field = GWY_DATA_FIELD(object);
    duplicate = gwy_data_field_new_alike(data_field, FALSE);
    memcpy(duplicate->data, data_field->data,
           data_field->xres*data_field->yres*sizeof(gdouble));
    duplicate->cached = data_field->cached;
    memcpy(duplicate->cache, data_field->cache,
           GWY_DATA_FIELD_CACHE_SIZE*sizeof(gdouble));

    return (GObject*)duplicate;
}

static void
gwy_data_field_clone_real(GObject *source, GObject *copy)
{
    GwyDataField *data_field, *clone;
    guint n;

    g_return_if_fail(GWY_IS_DATA_FIELD(source));
    g_return_if_fail(GWY_IS_DATA_FIELD(copy));

    data_field = GWY_DATA_FIELD(source);
    clone = GWY_DATA_FIELD(copy);

    n = data_field->xres*data_field->yres;
    if (clone->xres*clone->yres != n)
        clone->data = g_renew(gdouble, clone->data, n);
    clone->xres = data_field->xres;
    clone->yres = data_field->yres;

    gwy_data_field_copy(data_field, clone, TRUE);
}

/**
 * gwy_data_field_data_changed:
 * @data_field: A data field.
 *
 * Emits signal "data-changed" on a data field.
 **/
void
gwy_data_field_data_changed(GwyDataField *data_field)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_signal_emit(data_field, data_field_signals[DATA_CHANGED], 0);
}

/**
 * gwy_data_field_copy:
 * @src: Source data field.
 * @dest: Destination data field.
 * @nondata_too: Whether non-data (units) should be compied too.
 *
 * Copies the contents of an already allocated data field to a data field
 * of the same size.
 **/
void
gwy_data_field_copy(GwyDataField *src,
                    GwyDataField *dest,
                    gboolean nondata_too)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(src));
    g_return_if_fail(GWY_IS_DATA_FIELD(dest));
    g_return_if_fail(src->xres == dest->xres && src->yres == dest->yres);

    memcpy(dest->data, src->data, src->xres*src->yres*sizeof(gdouble));

    dest->xreal = src->xreal;
    dest->yreal = src->yreal;

    dest->cached = src->cached;
    memcpy(dest->cache, src->cache, GWY_DATA_FIELD_CACHE_SIZE*sizeof(gdouble));

    if (!nondata_too)
        return;

    /* SI Units can be NULL */
    if (src->si_unit_xy && dest->si_unit_xy)
        gwy_serializable_clone(G_OBJECT(src->si_unit_xy),
                               G_OBJECT(dest->si_unit_xy));
    else if (src->si_unit_xy && !dest->si_unit_xy)
        dest->si_unit_xy = gwy_si_unit_duplicate(src->si_unit_xy);
    else if (!src->si_unit_xy && dest->si_unit_xy)
        gwy_object_unref(dest->si_unit_xy);

    if (src->si_unit_z && dest->si_unit_z)
        gwy_serializable_clone(G_OBJECT(src->si_unit_z),
                               G_OBJECT(dest->si_unit_z));
    else if (src->si_unit_z && !dest->si_unit_z)
        dest->si_unit_z = gwy_si_unit_duplicate(src->si_unit_z);
    else if (!src->si_unit_z && dest->si_unit_z)
        gwy_object_unref(dest->si_unit_z);
}

/**
 * gwy_data_field_area_copy:
 * @src: Source data field.
 * @dest: Destination data field.
 * @ulcol: Starting column.
 * @ulrow: Starting row.
 * @brcol: Ending column (noninclusive).
 * @brrow: Ending row (noninclusive).
 * @destcol: Destination column.
 * @destrow: Destination row.
 *
 * Copies a rectangular area from @src to @dest.
 *
 * The area starts at (@ulcol, @ulrow) in @src and ends at (@brcol, @brrow)
 * (noninclusive).  It is copied to @dest starting from (@destcol, @destrow).
 *
 * There must be enough room in the destination data field, areas sticking
 * out are rejected.  If @src is equal to @dest, the areas may not overlap.
 *
 * Returns: Whether it succeeded (area sizes OK).
 **/
gboolean
gwy_data_field_area_copy(GwyDataField *src,
                         GwyDataField *dest,
                         gint ulcol, gint ulrow,
                         gint brcol, gint brrow,
                         gint destcol, gint destrow)
{
    gint i;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(src), FALSE);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dest), FALSE);

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    if (ulcol < 0
        || ulrow < 0
        || brcol > src->xres
        || brrow > src->yres
        || destcol < 0
        || destrow < 0
        || destcol + brcol - ulcol > dest->xres
        || destrow + brrow - ulrow > dest->yres)
        return FALSE;

    /* make it as fast as gwy_data_field_copy() whenever possible (and maybe
     * faster, as we don't play with units */
    gwy_data_field_invalidate(dest);
    if (brrow - ulrow == 1
        || (ulcol == 0 && brcol == src->xres && src->xres == dest->xres)) {
        memcpy(dest->data + dest->xres*destrow + destcol,
               src->data + src->xres*ulrow + ulcol,
               (brcol - ulcol)*(brrow - ulrow)*sizeof(gdouble));
        return TRUE;
    }

    for (i = 0; i < brrow - ulrow; i++)
        memcpy(dest->data + dest->xres*(destrow + i) + destcol,
               src->data + src->xres*(ulrow + i) + ulcol,
               (brcol - ulcol)*sizeof(gdouble));

    return TRUE;
}

/**
 * gwy_data_field_resample:
 * @data_field: A data field to be resampled.
 * @xres: Desired X resolution.
 * @yres: Desired Y resolution.
 * @interpolation: Interpolation method to use.
 *
 * Resamples a data field using given interpolation method
 *
 * This method may invalidate raw data buffer returned by
 * gwy_data_field_get_data().
 **/
void
gwy_data_field_resample(GwyDataField *data_field,
                        gint xres, gint yres,
                        GwyInterpolationType interpolation)
{
    gdouble *bdata, *p;
    gdouble xratio, yratio;
    gint i, j;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    if (data_field->xres == xres && data_field->yres == yres)
        return;
    g_return_if_fail(xres > 1 && yres > 1);

    gwy_data_field_invalidate(data_field);

    if (interpolation == GWY_INTERPOLATION_NONE) {
        data_field->xres = xres;
        data_field->yres = yres;
        data_field->data = g_renew(gdouble, data_field->data,
                                   data_field->xres*data_field->yres);
        return;
    }

    bdata = g_new(gdouble, xres*yres);

    xratio = data_field->xres/(gdouble)xres;
    yratio = data_field->yres/(gdouble)yres;

    p = bdata;
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++, p++) {
            *p = gwy_data_field_get_dval(data_field,
                                         (j + 0.5)*xratio, (i + 0.5)*yratio,
                                         interpolation);
        }
    }
    g_free(data_field->data);
    data_field->data = bdata;
    data_field->xres = xres;
    data_field->yres = yres;
}

/**
 * gwy_data_field_resize:
 * @data_field: A data field to be resized
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
 *
 * Resizes (crops) a data field.
 *
 * Crops a data field to a rectangle between upper-left and bottom-right
 * points, recomputing real size.
 *
 * This method may invalidate raw data buffer returned by
 * gwy_data_field_get_data().
 **/
void
gwy_data_field_resize(GwyDataField *data_field,
                      gint ulcol, gint ulrow,
                      gint brcol, gint brrow)
{
    GwyDataField *b;
    gint i, xres, yres;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_if_fail(ulcol >= 0 && ulrow >= 0
                     && brcol <= data_field->xres && brrow <= data_field->yres);

    yres = brrow - ulrow;
    xres = brcol - ulcol;
    if (xres == data_field->xres && yres == data_field->yres)
        return;

    /* FIXME: don't allocate second field, use memmove */
    b = gwy_data_field_new(xres, yres, 1.0, 1.0, FALSE);

    for (i = ulrow; i < brrow; i++) {
        memcpy(b->data + (i-ulrow)*xres,
               data_field->data + i*data_field->xres + ulcol,
               xres*sizeof(gdouble));
    }
    data_field->xreal *= (gdouble)xres/data_field->xres;
    data_field->yreal *= (gdouble)yres/data_field->yres;
    data_field->xres = xres;
    data_field->yres = yres;
    GWY_SWAP(gdouble*, data_field->data, b->data);
    g_object_unref(b);

    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_area_extract:
 * @data_field: A data field to be resized
 * @row: Upper-left row coordinate.
 * @col: Upper-left column coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Extracts a rectangular part of a data field to a new data field.
 **/
GwyDataField*
gwy_data_field_area_extract(GwyDataField *data_field,
                            gint col, gint row,
                            gint width, gint height)
{
    GwyDataField *result;
    gint i;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    g_return_val_if_fail(col >= 0
                         && row >= 0
                         && width > 0
                         && height > 0
                         && col + width <= data_field->xres
                         && row + height <= data_field->yres,
                         NULL);

    if (col == 0
        && row == 0
        && width == data_field->xres
        && height == data_field->yres)
        return gwy_data_field_duplicate(data_field);

    result = gwy_data_field_new(width, height,
                                data_field->xreal*width/data_field->xres,
                                data_field->yreal*height/data_field->yres,
                                FALSE);
    for (i = 0; i < height; i++) {
        memcpy(result->data + i*width,
               data_field->data + (i + row)*data_field->xres + col,
               width*sizeof(gdouble));
    }
    if (data_field->si_unit_xy)
        result->si_unit_xy = gwy_si_unit_duplicate(data_field->si_unit_xy);
    if (data_field->si_unit_z)
        result->si_unit_z = gwy_si_unit_duplicate(data_field->si_unit_z);

    return result;
}

/**
 * gwy_data_field_get_dval:
 * @data_field: A data field
 * @x: Horizontal position in pixel units, in range [0, x-resolution].
 * @y: Vertical postition in pixel units, in range [0, y-resolution].
 * @interpolation: Interpolation method to be used.
 *
 * Gets interpolated value at arbitrary data field point indexed by pixel
 * coordinates.
 *
 * Note pixel values are centered in pixels, so to get the same
 * value as gwy_data_field_get_val(@data_field, @j, @i) returns,
 * it's necessary to add 0.5:
 * gwy_data_field_get_dval(@data_field, @j+0.5, @i+0.5, @interpolation).
 *
 * See also gwy_data_field_get_dval_real() that does the same, but takes
 * real coordinates.
 *
 * Returns: Interpolated value at position (@x,@y).
 **/
gdouble
gwy_data_field_get_dval(GwyDataField *a,
                        gdouble x, gdouble y,
                        GwyInterpolationType interpolation)
{
    gint ix, iy, ixp, iyp;
    gint floorx, floory;
    gdouble restx, resty, valxy, valpy, valxp, valpp, va, vb, vc, vd;
    gdouble *data;
    gdouble intline[4];

    g_return_val_if_fail(GWY_IS_DATA_FIELD(a), 0.0);

    if (G_UNLIKELY(interpolation == GWY_INTERPOLATION_NONE))
        return 0.0;

    switch (interpolation) {
        case GWY_INTERPOLATION_ROUND:
        /* floor() centers pixel value */
        floorx = floor(x);
        floory = floor(y);
        ix = CLAMP(floorx, 0, a->xres - 1);
        iy = CLAMP(floory, 0, a->yres - 1);
        return a->data[ix + a->xres*iy];

        case GWY_INTERPOLATION_BILINEAR:
        /* To centered pixel value */
        x -= 0.5;
        y -= 0.5;
        floorx = floor(x);
        floory = floor(y);
        restx = x - floorx;
        resty = y - floory;
        ix = CLAMP(floorx, 0, a->xres - 1);
        iy = CLAMP(floory, 0, a->yres - 1);
        ixp = CLAMP(floorx + 1, 0, a->xres - 1);
        iyp = CLAMP(floory + 1, 0, a->yres - 1);

        valxy = (1.0 - restx)*(1.0 - resty)*a->data[ix + a->xres*iy];
        valxp = (1.0 - restx)*resty*a->data[ix + a->xres*iyp];
        valpy = restx*(1.0 - resty)*a->data[ixp + a->xres*iy];
        valpp = restx*resty*a->data[ixp + a->xres*iyp];
        return valxy + valpy + valxp + valpp;

        default:
        /* To centered pixel value */
        x -= 0.5;
        y -= 0.5;
        floorx = floor(x);
        floory = floor(y);
        restx = x - floorx;
        resty = y - floory;

        /* fall back to bilinear for border pixels. */
        if (floorx < 1 || floory < 1
            || floorx >= a->xres-2 || floory >= a->yres-2) {
            ix = CLAMP(floorx, 0, a->xres - 1);
            iy = CLAMP(floory, 0, a->yres - 1);
            ixp = CLAMP(floorx + 1, 0, a->xres - 1);
            iyp = CLAMP(floory + 1, 0, a->yres - 1);

            valxy = (1.0 - restx)*(1.0 - resty)*a->data[ix + a->xres*iy];
            valxp = (1.0 - restx)*resty*a->data[ix + a->xres*iyp];
            valpy = restx*(1.0 - resty)*a->data[ixp + a->xres*iy];
            valpp = restx*resty*a->data[ixp + a->xres*iyp];
            return valxy + valpy + valxp + valpp;
        }

        /* interpolation in x direction */
        data = a->data + floorx-1 + a->xres*(floory-1);
        memcpy(intline, data, 4*sizeof(gdouble));
        va = gwy_interpolation_get_dval_of_equidists(restx, intline,
                                                     interpolation);
        memcpy(intline, data + a->xres, 4*sizeof(gdouble));
        vb = gwy_interpolation_get_dval_of_equidists(restx, intline,
                                                     interpolation);
        memcpy(intline, data + 2*a->xres, 4*sizeof(gdouble));
        vc = gwy_interpolation_get_dval_of_equidists(restx, intline,
                                                     interpolation);
        memcpy(intline, data + 3*a->xres, 4*sizeof(gdouble));
        vd = gwy_interpolation_get_dval_of_equidists(restx, intline,
                                                     interpolation);

        /*interpolation in y direction*/
        intline[0] = va;
        intline[1] = vb;
        intline[2] = vc;
        intline[3] = vd;
        return gwy_interpolation_get_dval_of_equidists(resty, intline,
                                                       interpolation);
    }
}

/**
 * gwy_data_field_get_data:
 * @data_field: A data field
 *
 * Gets the raw data buffer of a data field.
 *
 * The returned buffer is not quaranteed to be valid through whole data
 * field life time.  Some function may change it, most notably
 * gwy_data_field_resize() and gwy_data_field_resample().
 *
 * This function invalidates any cached information, use
 * gwy_data_field_get_data_const() if you are not going to change the data.
 *
 * See gwy_data_field_invalidate() for some discussion.
 *
 * Returns: The data field as a pointer to an array of
 *          gwy_data_field_get_xres()*gwy_data_field_get_yres() #gdouble's,
 *          ordered by lines.  I.e., they are to be accessed as
 *          data[row*xres + column].
 **/
gdouble*
gwy_data_field_get_data(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    gwy_data_field_invalidate(data_field);
    return data_field->data;
}

/**
 * gwy_data_field_get_data_const:
 * @data_field: A data field.
 *
 * Gets the raw data buffer of a data field, read-only.
 *
 * The returned buffer is not quaranteed to be valid through whole data
 * field life time.  Some function may change it, most notably
 * gwy_data_field_resize() and gwy_data_field_resample().
 *
 * Use gwy_data_field_get_data() if you want to change the data.
 *
 * See gwy_data_field_invalidate() for some discussion.
 *
 * Returns: The data field as a pointer to an array of
 *          gwy_data_field_get_xres()*gwy_data_field_get_yres() #gdouble's,
 *          ordered by lines.  I.e., they are to be accessed as
 *          data[row*xres + column].
 **/
const gdouble*
gwy_data_field_get_data_const(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    return (const gdouble*)data_field->data;
}

/**
 * gwy_data_field_get_xres:
 * @data_field: A data field.
 *
 * Gets X resolution (number of columns) of a data field.
 *
 * Returns: X resolution.
 **/
gint
gwy_data_field_get_xres(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    return data_field->xres;
}

/**
 * gwy_data_field_get_yres:
 * @data_field: A data field.
 *
 * Gets Y resolution (number of rows) of the field.
 *
 * Returns: Y resolution.
 **/
gint
gwy_data_field_get_yres(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    return data_field->yres;
}

/**
 * gwy_data_field_get_xreal:
 * @data_field: A data field.
 *
 * Gets the X real (physical) size of a data field.
 *
 * Returns: X real size value.
 **/
gdouble
gwy_data_field_get_xreal(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0.0);
    return data_field->xreal;
}

/**
 * gwy_data_field_get_yreal:
 * @data_field: A data field
 *
 * Gets the Y real (physical) size of a data field.
 *
 * Returns: Y real size value.
 **/
gdouble
gwy_data_field_get_yreal(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0.0);
    return data_field->yreal;
}

/**
 * gwy_data_field_set_xreal:
 * @data_field: A data field.
 * @xreal: New X real size value.
 *
 * Sets X real (physical) size value of a data field.
 **/
void
gwy_data_field_set_xreal(GwyDataField *data_field, gdouble xreal)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    data_field->xreal = xreal;
}

/**
 * gwy_data_field_set_yreal:
 * @data_field: A data field.
 * @yreal: New Y real size value.
 *
 * Sets Y real (physical) size value of a data field.
 **/
void
gwy_data_field_set_yreal(GwyDataField *data_field, gdouble yreal)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    data_field->yreal = yreal;
}

/**
 * gwy_data_field_get_xoffset:
 * @data_field: A data field.
 *
 * Gets the X offset of data field origin.
 *
 * Returns: X offset value.
 **/
gdouble
gwy_data_field_get_xoffset(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0.0);
    return data_field->xoff;
}

/**
 * gwy_data_field_get_yoffset:
 * @data_field: A data field
 *
 * Gets the Y offset of data field origin.
 *
 * Returns: Y offset value.
 **/
gdouble
gwy_data_field_get_yoffset(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0.0);
    return data_field->yoff;
}

/**
 * gwy_data_field_set_xoffset:
 * @data_field: A data field.
 * @xoff: New X offset value.
 *
 * Sets the X offset of a data field origin.
 *
 * Note offsets don't affect any calculation, nor functions like
 * gwy_data_field_rotj().
 **/
void
gwy_data_field_set_xoffset(GwyDataField *data_field, gdouble xoff)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    data_field->xoff = xoff;
}

/**
 * gwy_data_field_set_yoffset:
 * @data_field: A data field.
 * @yoff: New Y offset value.
 *
 * Sets the Y offset of a data field origin.
 *
 * Note offsets don't affect any calculation, nor functions like
 * gwy_data_field_rtoi().
 **/
void
gwy_data_field_set_yoffset(GwyDataField *data_field, gdouble yoff)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    data_field->yoff = yoff;
}

/**
 * gwy_data_field_get_si_unit_xy:
 * @data_field: A data field.
 *
 * Returns lateral SI unit of a data field.
 *
 * Returns: SI unit corresponding to the lateral (XY) dimensions of the data
 *          field.  Its reference count is not incremented.
 **/
GwySIUnit*
gwy_data_field_get_si_unit_xy(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);

    if (!data_field->si_unit_xy)
        data_field->si_unit_xy = gwy_si_unit_new("");

    return data_field->si_unit_xy;
}

/**
 * gwy_data_field_get_si_unit_z:
 * @data_field: A data field.
 *
 * Returns value SI unit of a data field.
 *
 * Returns: SI unit corresponding to the "height" (Z) dimension of the data
 *          field.  Its reference count is not incremented.
 **/
GwySIUnit*
gwy_data_field_get_si_unit_z(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);

    if (!data_field->si_unit_z)
        data_field->si_unit_z = gwy_si_unit_new("");

    return data_field->si_unit_z;
}

/**
 * gwy_data_field_set_si_unit_xy:
 * @data_field: A data field.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the lateral (XY) dimensions of a data
 * field.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 **/
void
gwy_data_field_set_si_unit_xy(GwyDataField *data_field,
                              GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (data_field->si_unit_xy == si_unit)
        return;

    gwy_object_unref(data_field->si_unit_xy);
    g_object_ref(si_unit);
    data_field->si_unit_xy = si_unit;
}

/**
 * gwy_data_field_set_si_unit_z:
 * @data_field: A data field.
 * @si_unit: SI unit to be set.
 *
 * Sets the SI unit corresponding to the "height" (Z) dimension of a data
 * field.
 *
 * It does not assume a reference on @si_unit, instead it adds its own
 * reference.
 **/
void
gwy_data_field_set_si_unit_z(GwyDataField *data_field,
                             GwySIUnit *si_unit)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_SI_UNIT(si_unit));
    if (data_field->si_unit_z == si_unit)
        return;

    gwy_object_unref(data_field->si_unit_z);
    g_object_ref(si_unit);
    data_field->si_unit_z = si_unit;
}

/**
 * gwy_data_field_get_value_format_xy:
 * @data_field: A data field.
 * @style: Unit format style.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying coordinates of a data field.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_data_field_get_value_format_xy(GwyDataField *data_field,
                                   GwySIUnitFormatStyle style,
                                   GwySIValueFormat *format)
{
    gdouble max, unit;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);

    max = MAX(data_field->xreal, data_field->yreal);
    unit = MIN(data_field->xreal/data_field->xres,
               data_field->yreal/data_field->yres);
    return gwy_si_unit_get_format_with_resolution
                                   (gwy_data_field_get_si_unit_xy(data_field),
                                    style, max, unit, format);
}

/**
 * gwy_data_field_get_value_format_z:
 * @data_field: A data field.
 * @style: Unit format style.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying values of a data field.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_data_field_get_value_format_z(GwyDataField *data_field,
                                  GwySIUnitFormatStyle style,
                                  GwySIValueFormat *format)
{
    gdouble max, min;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);

    max = gwy_data_field_get_max(data_field);
    min = gwy_data_field_get_min(data_field);
    if (max == min) {
        max = ABS(max);
        min = 0.0;
    }

    return gwy_si_unit_get_format(gwy_data_field_get_si_unit_z(data_field),
                                  style, max - min, format);
}

/**
 * gwy_data_field_itor:
 * @data_field: A data field.
 * @row: Vertical pixel coordinate.
 *
 * Transforms vertical pixel coordinate to real (physical) Y coordinate.
 *
 * That is it maps range [0..y-resolution] to range [0..real-y-size].
 * It is not suitable for conversion of matrix indices to physical coordinates,
 * you have to use gwy_data_field_itor(@data_field, @row + 0.5) for that.
 *
 * Returns: Real Y coordinate.
 **/
gdouble
gwy_data_field_itor(GwyDataField *data_field, gdouble row)
{
    return row * data_field->yreal/data_field->yres;
}

/**
 * gwy_data_field_jtor:
 * @data_field: A data field.
 * @col: Horizontal pixel coordinate.
 *
 * Transforms horizontal pixel coordinate to real (physical) X coordinate.
 *
 * That is it maps range [0..x-resolution] to range [0..real-x-size].
 * It is not suitable for conversion of matrix indices to physical coordinates,
 * you have to use gwy_data_field_jtor(@data_field, @col + 0.5) for that.
 *
 * Returns: Real X coordinate.
 **/
gdouble
gwy_data_field_jtor(GwyDataField *data_field, gdouble col)
{
    return col * data_field->xreal/data_field->xres;
}


/**
 * gwy_data_field_rtoi:
 * @data_field: A data field.
 * @realy: Real (physical) Y coordinate.
 *
 * Transforms real (physical) Y coordinate to row.
 *
 * That is it maps range [0..real-y-size] to range [0..y-resolution].
 *
 * Returns: Vertical pixel coodinate.
 **/
gdouble
gwy_data_field_rtoi(GwyDataField *data_field, gdouble realy)
{
    return realy * data_field->yres/data_field->yreal;
}


/**
 * gwy_data_field_rtoj:
 * @data_field: A data field.
 * @realx: Real (physical) X coodinate.
 *
 * Transforms real (physical) X coordinate to column.
 *
 * That is it maps range [0..real-x-size] to range [0..x-resolution].
 *
 * Returns: Horizontal pixel coordinate.
 **/
gdouble
gwy_data_field_rtoj(GwyDataField *data_field, gdouble realx)
{
    return realx * data_field->xres/data_field->xreal;
}

static inline gboolean
gwy_data_field_inside(GwyDataField *data_field, gint i, gint j)
{
    if (i >= 0 && j >= 0 && i < data_field->xres && j < data_field->yres)
        return TRUE;
    else
        return FALSE;
}

/**
 * gwy_data_field_get_val:
 * @data_field: A data field.
 * @col: Column index.
 * @row: Row index.
 *
 * Gets value at given position in a data field.
 *
 * Do not access data with this function inside inner loops, it's slow.
 * Get raw data buffer with gwy_data_field_get_data_const() and access it
 * directly instead.
 *
 * Returns: Value at (@col, @row).
 **/
gdouble
gwy_data_field_get_val(GwyDataField *data_field, gint col, gint row)
{
    g_return_val_if_fail(gwy_data_field_inside(data_field, col, row), 0.0);
    return data_field->data[col + data_field->xres*row];
}

/**
 * gwy_data_field_set_val:
 * @data_field: A data field.
 * @col: Column index.
 * @row: Row index.
 * @value: Value to set.
 *
 * Sets value at given position in a data field.
 *
 * Do not set data with this function inside inner loops, it's slow.  Get raw
 * data buffer with gwy_data_field_get_data() and write to it directly instead.
 **/
void
gwy_data_field_set_val(GwyDataField *data_field,
                       gint col, gint row,
                       gdouble value)
{
    g_return_if_fail(gwy_data_field_inside(data_field, col, row));
    gwy_data_field_invalidate(data_field);
    data_field->data[col + data_field->xres*row] = value;
}

/**
 * gwy_data_field_get_dval_real:
 * @data_field: A data field.
 * @x: X postion in real coordinates.
 * @y: Y postition in real coordinates.
 * @interpolation: Interpolation method to use.
 *
 * Gets interpolated value at arbitrary data field point indexed by real
 * coordinates.
 *
 * See also gwy_data_field_get_dval() that does the same, but takes pixel
 * coordinates.
 *
 * Returns: Value at position (@x,@y).
 **/
gdouble
gwy_data_field_get_dval_real(GwyDataField *data_field, gdouble x, gdouble y,
                             GwyInterpolationType interpolation)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0.0);
    return gwy_data_field_get_dval(data_field,
                                   gwy_data_field_rtoj(data_field, x),
                                   gwy_data_field_rtoi(data_field, y),
                                   interpolation);
}

/**
 * gwy_data_field_rotate:
 * @data_field: A data field.
 * @angle: Rotation angle (in radians).
 * @interpolation: Interpolation method to use.
 *
 * Rotates a data field by a given angle.
 *
 * Values that get outside of data field by the rotation are lost.
 * Undefined values from outside of data field that get inside are set to
 * data field minimum value.
 **/
void
gwy_data_field_rotate(GwyDataField *a,
                      gdouble angle,
                      GwyInterpolationType interpolation)
{
    GwyDataField *b;
    gdouble inew, jnew, ir, jr, ang, icor, jcor, sn, cs, val;
    gint i, j;

    angle = fmod(angle, 2*G_PI);
    if (angle < 0.0)
        angle += 2*G_PI;

    if (angle == 0.0)
        return;

    b = gwy_data_field_duplicate(a);

    val = gwy_data_field_get_min(a);
    ang = 3*G_PI/4 + angle;
    if (fabs(angle - G_PI/2) < 1e-15) {
        sn = 1.0;
        cs = 0.0;
        icor = 1.0;
        jcor = 0.0;
    }
    if (fabs(angle - G_PI) < 2e-15) {
        sn = 0.0;
        cs = -1.0;
        icor = 1.0;
        jcor = a->xres;
    }
    if (fabs(angle - 3*G_PI/4) < 3e-15) {
        sn = -1.0;
        cs = 0.0;
        icor = a->yres;
        jcor = a->xres;
    }
    else {
        sn = sin(angle);
        cs = cos(angle);
        icor = a->yres*(1.0 + cs)/2.0 - sn*a->xres/2.0;
        jcor = a->xres*(1.0 - cs)/2.0 - sn*a->yres/2.0;
    }

    for (i = 0; i < a->yres; i++) { /*row*/
        for (j = 0; j < a->xres; j++) { /*column*/
            ir = a->yres - (i + 0.5) - icor;
            jr = (j + 0.5) - jcor;
            inew = -ir*cs + jr*sn;
            jnew = ir*sn + jr*cs;
            if (inew > a->yres+1 || jnew > a->xres+1 || inew < -1 || jnew < -1)
                a->data[j + a->xres*i] = val;
            else {
                inew = CLAMP(inew, 0, a->yres);
                jnew = CLAMP(jnew, 0, a->xres);
                a->data[j + a->xres*i] = gwy_data_field_get_dval(b, jnew, inew,
                                                                 interpolation);
            }
        }
    }

    g_object_unref(b);
    gwy_data_field_invalidate(a);
}


/**
 * gwy_data_field_invert:
 * @data_field: A data field.
 * @x: Whether reflect about X axis.
 * @y: Whether reflect about Y axis.
 * @z: Whether to invert in Z direction (i.e., invert values).
 *
 * Reflects amd/or inverts a data field.
 *
 * In the case of value reflection, it's inverted about mean value.
 **/
void
gwy_data_field_invert(GwyDataField *a,
                      gboolean x,
                      gboolean y,
                      gboolean z)
{
    gint i, j;
    gdouble avg;
    gdouble *line, *ap, *ap2;
    gsize linelen;
    gdouble *data;

    g_return_if_fail(GWY_IS_DATA_FIELD(a));
    data = a->data;

    if (z) {
        avg = gwy_data_field_get_avg(a);
        gwy_data_field_multiply(a, -1.0);
        gwy_data_field_add(a, 2*avg);
    }

    if (!x && !y)
        return;

    line = g_new(gdouble, a->xres);
    linelen = a->xres*sizeof(gdouble);
    if (y) {
        for (i = 0; i < a->yres; i++) {
            ap = data + i*a->xres;
            memcpy(line, ap, linelen);
            for (j = 0; j < a->xres; j++)
                ap[j] = line[a->xres-j-1];
        }
    }
    if (x) {
        /* What is lesser evil?
         * allocating one extra datafield or doing 50% extra memcpy()s */
        for (i = 0; i < a->yres/2; i++) {
            ap = data + i*a->xres;
            ap2 = data + (a->yres-i-1)*a->xres;
            memcpy(line, ap, linelen);
            memcpy(ap, ap2, linelen);
            memcpy(ap2, line, linelen);
        }
    }

    /* FIXME: we can recompute ARF and ART too */
    a->cached &= CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS) | CBIT(MED);
    g_free(line);
}

/**
 * gwy_data_field_fill:
 * @data_field: A data field.
 * @value: Value to be entered.
 *
 * Fills a data field with given value.
 **/
void
gwy_data_field_fill(GwyDataField *data_field, gdouble value)
{
    gint i;
    gdouble *p = data_field->data;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    for (i = data_field->xres * data_field->yres; i; i--, p++)
        *p = value;

    /* We can precompute stats */
    data_field->cached = CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                         | CBIT(MED) | CBIT(ARF) | CBIT(ART);
    CVAL(data_field, MIN) = value;
    CVAL(data_field, MAX) = value;
    CVAL(data_field, SUM) = data_field->xres * data_field->yres * value;
    CVAL(data_field, RMS) = 0.0;
    CVAL(data_field, MED) = value;
    CVAL(data_field, ARF) = value;
    CVAL(data_field, ART) = value;
}

/**
 * gwy_data_field_area_fill:
 * @data_field: A data field.
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
 * @value: Value to be entered
 *
 * Fills a rectangular part of a data field with given value.
 **/
void
gwy_data_field_area_fill(GwyDataField *a,
                         gint ulcol, gint ulrow, gint brcol, gint brrow,
                         gdouble value)
{
    gint i, j;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_if_fail(ulcol >= 0 && ulrow >= 0
                     && brcol <= a->xres && brrow <= a->yres);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            *(row++) = value;
    }
    gwy_data_field_invalidate(a);
}

/**
 * gwy_data_field_clear:
 * @data_field: A data field.
 *
 * Fills a data field with zeroes.
 **/
void
gwy_data_field_clear(GwyDataField *data_field)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    memset(data_field->data, 0,
           data_field->xres*data_field->yres*sizeof(gdouble));

    /* We can precompute stats */
    data_field->cached = CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                         | CBIT(MED) | CBIT(ARF) | CBIT(ART);
    CVAL(data_field, MIN) = 0.0;
    CVAL(data_field, MAX) = 0.0;
    CVAL(data_field, SUM) = 0.0;
    CVAL(data_field, RMS) = 0.0;
    CVAL(data_field, MED) = 0.0;
    CVAL(data_field, ARF) = 0.0;
    CVAL(data_field, ART) = 0.0;
}

/**
 * gwy_data_field_area_clear:
 * @data_field: A data field.
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
 *
 * Fills a rectangular part of a data field with zeroes.
 **/
void
gwy_data_field_area_clear(GwyDataField *data_field,
                          gint ulcol, gint ulrow, gint brcol, gint brrow)
{
    gint i;
    gdouble *row;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_if_fail(ulcol >= 0 && ulrow >= 0
                     && brcol <= data_field->xres && brrow <= data_field->yres);

    gwy_data_field_invalidate(data_field);
    if (brrow - ulrow == 1 || (ulcol == 0 && brcol == data_field->xres)) {
        memset(data_field->data + data_field->xres*ulrow + ulcol, 0,
               (brcol - ulcol)*(brrow - ulrow)*sizeof(gdouble));
        return;
    }

    for (i = ulrow; i < brrow; i++) {
        row = data_field->data + i*data_field->xres + ulcol;
        memset(row, 0, (brcol - ulcol)*sizeof(gdouble));
    }
}

/**
 * gwy_data_field_multiply:
 * @data_field: A data field.
 * @value: Value to multiply @data_field with.
 *
 * Multiplies all values in a data field by given value.
 **/
void
gwy_data_field_multiply(GwyDataField *data_field, gdouble value)
{
    gint i;
    gdouble *p;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    p = data_field->data;
    for (i = data_field->xres * data_field->yres; i; i--, p++)
        *p *= value;

    /* We can transform stats */
    data_field->cached &= CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                          | CBIT(MED) | CBIT(ARF) | CBIT(ART);
    CVAL(data_field, MIN) *= value;
    CVAL(data_field, MAX) *= value;
    CVAL(data_field, SUM) *= value;
    CVAL(data_field, RMS) *= value;
    CVAL(data_field, MED) *= value;
    CVAL(data_field, ARF) *= value;
    CVAL(data_field, ART) *= value;
    if (value < 0) {
        GWY_SWAP(gdouble, CVAL(data_field, MIN), CVAL(data_field, MAX));
        GWY_SWAP(gdouble, CVAL(data_field, ARF), CVAL(data_field, ART));
    }
}

/**
 * gwy_data_field_area_multiply:
 * @data_field: A data field.
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
 * @value: Value to multiply area with.
 *
 * Multiplies values in a rectangular part of a data field by given value
 **/
void
gwy_data_field_area_multiply(GwyDataField *data_field,
                             gint ulcol, gint ulrow, gint brcol, gint brrow,
                             gdouble value)
{
    gint i, j;
    gdouble *row;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_if_fail(ulcol >= 0 && ulrow >= 0
                     && brcol <= data_field->xres && brrow <= data_field->yres);

    for (i = ulrow; i < brrow; i++) {
        row = data_field->data + i*data_field->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            *(row++) *= value;
    }
    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_add:
 * @data_field: A data field.
 * @value: Value to be added to data field values.
 *
 * Adds given value to all values in a data field.
 **/
void
gwy_data_field_add(GwyDataField *data_field, gdouble value)
{
    gint i;
    gdouble *p;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    p = data_field->data;
    for (i = data_field->xres * data_field->yres; i; i--, p++)
        *p += value;

    /* We can transform stats */
    data_field->cached &= CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                          | CBIT(MED) | CBIT(ARF) | CBIT(ART);
    CVAL(data_field, MIN) += value;
    CVAL(data_field, MAX) += value;
    CVAL(data_field, SUM) += data_field->xres * data_field->yres * value;
    /* RMS doesn't change */
    CVAL(data_field, MED) += value;
    CVAL(data_field, ARF) += value;
    CVAL(data_field, ART) += value;
}

/**
 * gwy_data_field_area_add:
 * @data_field: A data field.
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
 * @value: Value to be added to area values.
 *
 * Adds given value to all values in a rectangular part of a data field.
 **/
void
gwy_data_field_area_add(GwyDataField *data_field,
                        gint ulcol, gint ulrow, gint brcol, gint brrow,
                        gdouble value)
{
    gint i, j;
    gdouble *row;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_if_fail(ulcol >= 0 && ulrow >= 0
                     && brcol <= data_field->xres && brrow <= data_field->yres);

    for (i = ulrow; i < brrow; i++) {
        row = data_field->data + i*data_field->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            *(row++) += value;
    }
    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_threshold:
 * @data_field: A data field.
 * @threshval: Threshold value.
 * @bottom: Lower replacement value.
 * @top: Upper replacement value.
 *
 * Tresholds values of a data field.
 *
 * Values smaller than @threshold are set to value @bottom, values higher
 * than @threshold or equal to it are set to value @top
 *
 * Returns: The total number of values above threshold.
 **/
gint
gwy_data_field_threshold(GwyDataField *data_field,
                         gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, n, tot = 0;
    gdouble *p = data_field->data;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);

    n = data_field->xres * data_field->yres;
    for (i = n; i; i--, p++) {
        if (*p < threshval)
            *p = bottom;
        else {
            *p = top;
            tot++;
        }
    }

    /* We can precompute stats */
    data_field->cached = CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                         | CBIT(MED);
    CVAL(data_field, MIN) = MIN(top, bottom);
    CVAL(data_field, MAX) = MAX(top, bottom);
    CVAL(data_field, SUM) = tot*top + (n - tot)*bottom;
    CVAL(data_field, RMS) = (top - bottom)*(top - bottom)
                            * tot/(gdouble)n * (n - tot)/(gdouble)n;
    /* FIXME: may be incorrect for tot == n/2(?) */
    CVAL(data_field, MED) = tot > n/2 ? top : bottom;

    return tot;
}


/**
 * gwy_data_field_area_threshold:
 * @data_field: A data field.
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
 * @threshval: Threshold value.
 * @bottom: Lower replacement value.
 * @top: Upper replacement value.
 *
 * Tresholds values of a rectangular part of a data field.
 *
 * Values smaller than @threshold are set to value @bottom, values higher
 * than @threshold or equal to it are set to value @top
 *
 * Returns: The total number of values above threshold.
 **/
gint
gwy_data_field_area_threshold(GwyDataField *data_field,
                              gint ulcol, gint ulrow, gint brcol, gint brrow,
                              gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, j, tot = 0;
    gdouble *row;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0
                         && brcol <= data_field->xres
                         && brrow <= data_field->yres,
                         0);

    for (i = ulrow; i < brrow; i++) {
        row = data_field->data + i*data_field->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++) {
            if (*row < threshval)
                *row = bottom;
            else {
                *row = top;
                tot++;
            }
        }
    }
    gwy_data_field_invalidate(data_field);

    return tot;
}

/**
 * gwy_data_field_clamp:
 * @data_field: A data field.
 * @bottom: Lower limit value.
 * @top: Upper limit value.
 *
 * Limits data field values to a range.
 *
 * Returns: The number of changed values, i.e., values that were outside
 *          [@bottom, @top].
 **/
gint
gwy_data_field_clamp(GwyDataField *data_field,
                     gdouble bottom, gdouble top)
{
    gint i, tot = 0;
    gdouble *p = data_field->data;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    g_return_val_if_fail(bottom <= top, 0);

    for (i = data_field->xres * data_field->yres; i; i--, p++) {
        if (*p < bottom) {
            *p = bottom;
            tot++;
        }
        else if (*p > top) {
            *p = top;
            tot++;
        }
    }
    if (tot) {
        /* We can precompute stats */
        data_field->cached &= CBIT(MIN) | CBIT(MAX);
        CVAL(data_field, MIN) = bottom;
        CVAL(data_field, MAX) = top;
    }

    return tot;
}


/**
 * gwy_data_field_area_clamp:
 * @data_field: A data field.
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
 * @bottom: Lower limit value.
 * @top: Upper limit value.
 *
 * Limits values in a rectangular part of a data field to a range.
 *
 * Returns: The number of changed values, i.e., values that were outside
 *          [@bottom, @top].
 **/
gint
gwy_data_field_area_clamp(GwyDataField *data_field,
                          gint ulcol,
                          gint ulrow,
                          gint brcol,
                          gint brrow,
                          gdouble bottom,
                          gdouble top)
{
    gint i, j, tot = 0;
    gdouble *row;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0
                         && brcol < data_field->xres
                         && brrow < data_field->yres,
                         0);

    for (i = ulrow; i < brrow; i++) {
        row = data_field->data + i*data_field->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++) {
            if (*row < bottom) {
                *row = bottom;
                tot++;
            }
            else if (*row > top) {
                *row = top;
                tot++;
            }
        }

    }
    if (tot)
        gwy_data_field_invalidate(data_field);

    return tot;
}


/**
 * gwy_data_field_get_row:
 * @data_field: A data field.
 * @data_line: A data line.  It will be resized to width ot @data_field.
 * @row: Row index.
 *
 * Extracts a data field row into a data line.
 **/
void
gwy_data_field_get_row(GwyDataField *data_field,
                       GwyDataLine* data_line,
                       gint row)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(row >= 0 && row < data_field->yres);

    gwy_data_line_resample(data_line, data_field->xres, GWY_INTERPOLATION_NONE);
    data_line->real = data_field->xreal;
    memcpy(data_line->data,
           data_field->data + row*data_field->xres,
           data_field->xres*sizeof(gdouble));
}


/**
 * gwy_data_field_get_column:
 * @data_field: A data field
 * @data_line: A data line.  It will be resized to height of @data_field.
 * @col: Column index.
 *
 * Extracts a data field column into a data line.
 **/
void
gwy_data_field_get_column(GwyDataField *data_field,
                          GwyDataLine* data_line,
                          gint col)
{
    gint k;
    gdouble *p;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(col >= 0 && col < data_field->xres);

    gwy_data_line_resample(data_line, data_field->yres, GWY_INTERPOLATION_NONE);
    data_line->real = data_field->yreal;
    p = data_field->data + col;
    for (k = 0; k < data_field->yres; k++)
        data_line->data[k] = p[k*data_field->xres];
}

/**
 * gwy_data_field_get_row_part:
 * @data_field: A data field.
 * @data_line: A data line.  It will be resized to the row part width.
 * @row: Row index.
 * @from: Start column index.
 * @to: End column index + 1.
 *
 * Extracts part of a data field row into a data line.
 **/
void
gwy_data_field_get_row_part(GwyDataField *data_field,
                            GwyDataLine *data_line,
                            gint row,
                            gint from,
                            gint to)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(row >= 0 && row < data_field->yres);
    if (to < from)
        GWY_SWAP(gint, from, to);

    if (data_line->res != (to - from))
        gwy_data_line_resample(data_line, to - from, GWY_INTERPOLATION_NONE);

    data_line->real = data_field->xreal*(to - from)/data_field->xres;
    memcpy(data_line->data,
           data_field->data + row*data_field->xres + from,
           (to - from)*sizeof(gdouble));
}

/**
 * gwy_data_field_get_column_part:
 * @data_field: A data field.
 * @data_line: A data line.  It will be resized to the column part height.
 * @col: Column index.
 * @from: Start row index.
 * @to: End row index + 1.
 *
 * Extracts part of a data field column into a data line.
 **/
void
gwy_data_field_get_column_part(GwyDataField *data_field,
                               GwyDataLine *data_line,
                               gint col,
                               gint from,
                               gint to)
{
    gint k;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(col >= 0 && col < data_field->xres);
    if (to < from)
        GWY_SWAP(gint, from, to);

    if (data_line->res != (to - from))
        gwy_data_line_resample(data_line, to-from, GWY_INTERPOLATION_NONE);

    data_line->real = data_field->yreal*(to - from)/data_field->yres;
    for (k = 0; k < to - from; k++)
        data_line->data[k] = data_field->data[(k+from)*data_field->xres + col];
}

/**
 * gwy_data_field_set_row_part:
 * @data_field: A data field.
 * @data_line: A data line.
 * @row: Row index.
 * @from: Start row index.
 * @to: End row index + 1.
 *
 * Puts a data line into a data field row.
 *
 * If data line length differs from @to-@from, it is resampled to this length.
 **/
void
gwy_data_field_set_row_part(GwyDataField *data_field,
                            GwyDataLine *data_line,
                            gint row,
                            gint from,
                            gint to)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(row >= 0 && row < data_field->yres);
    if (to < from)
        GWY_SWAP(gint, from, to);

    if (data_line->res != (to - from))
        gwy_data_line_resample(data_line, to-from, GWY_INTERPOLATION_BILINEAR);

    memcpy(data_field->data + row*data_field->xres + from,
           data_line->data,
           (to-from)*sizeof(gdouble));
    gwy_data_field_invalidate(data_field);
}


/**
 * gwy_data_field_set_column_part:
 * @data_field: A data field.
 * @data_line: A data line.
 * @col: Column index.
 * @from: Start row index.
 * @to: End row index + 1.
 *
 * Puts a data line into data field column.
 *
 * If data line length differs from @to-@from, it is resampled to this length.
 **/
void
gwy_data_field_set_column_part(GwyDataField *data_field,
                               GwyDataLine* data_line,
                               gint col,
                               gint from,
                               gint to)
{
    gint k;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(col >= 0 && col < data_field->xres);
    if (to < from)
        GWY_SWAP(gint, from, to);

    if (data_line->res != (to-from))
        gwy_data_line_resample(data_line, to-from, GWY_INTERPOLATION_BILINEAR);

    for (k = 0; k < to-from; k++)
        data_field->data[(k+from)*data_field->xres + col] = data_line->data[k];
    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_set_row:
 * @data_field: A data field.
 * @data_line: A data line.
 * @row: Row index.
 *
 * Sets a row in the data field to values of a data line.
 *
 * Data line length must be equal to width of data field.
 **/
void
gwy_data_field_set_row(GwyDataField *data_field,
                       GwyDataLine* data_line,
                       gint row)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(row >= 0 && row < data_field->yres);
    g_return_if_fail(data_field->xres == data_line->res);

    memcpy(data_field->data + row*data_field->xres,
           data_line->data,
           data_field->xres*sizeof(gdouble));
    gwy_data_field_invalidate(data_field);
}


/**
 * gwy_data_field_set_column:
 * @data_field: A data field.
 * @data_line: A data line.
 * @col: Column index.
 *
 * Sets a column in the data field to values of a data line.
 *
 * Data line length must be equal to height of data field.
 **/
void
gwy_data_field_set_column(GwyDataField *data_field,
                          GwyDataLine* data_line,
                          gint col)
{
    gint k;
    gdouble *p;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(col >= 0 && col < data_field->xres);
    g_return_if_fail(data_field->yres == data_line->res);

    p = data_field->data + col;
    for (k = 0; k < data_field->yres; k++)
        p[k*data_field->xres] = data_line->data[k];
    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_get_data_line:
 * @data_field: A data field.
 * @data_line: A data line.  It will be resized to @res samples.
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
 * @res: Requested resolution of data line.  If nonpositive, data line
 *       resolution is chosen to match @data_field's.
 * @interpolation: Interpolation type to use.
 *
 * Extracts a profile from a data field to a data line.
 **/
void
gwy_data_field_get_data_line(GwyDataField *data_field,
                             GwyDataLine* data_line,
                             gint ulcol, gint ulrow,
                             gint brcol, gint brrow,
                             gint res,
                             GwyInterpolationType interpolation)
{
    gint k;
    gdouble cosa, sina, size;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(ulcol >= 0 && ulrow >= 0
                     && brcol >= 0 && brrow >= 0
                     && ulrow <= data_field->yres && ulcol <= data_field->xres
                     && brrow <= data_field->yres && brcol <= data_field->xres);

    size = hypot(ulcol - brcol, ulrow - brrow);
    if (res <= 0)
        res = ROUND(size);

    cosa = (brcol - ulcol)/(res - 1.0);
    sina = (brrow - ulrow)/(res - 1.0);

    gwy_data_line_resample(data_line, res, GWY_INTERPOLATION_NONE);
    for (k = 0; k < res; k++)
        data_line->data[k] = gwy_data_field_get_dval(data_field,
                                                     ulcol + 0.5 + k*cosa,
                                                     ulrow + 0.5 + k*sina,
                                                     interpolation);

    data_line->real = size*data_field->xreal/data_field->xres;
}

/**
 * gwy_data_field_get_data_line_averaged:
 * @data_field: A data field.
 * @data_line: A data line.  It will be resized to @res samples.
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
 * @res: Requested resolution of data line.  If nonpositive, data line
 *       resolution is chosen to match @data_field's.
 * @thickness: Thickness of line to be averaged.
 * @interpolation: Interpolation type to use.
 *
 * Extracts an averaged profile from data field to a data line.
 **/
void
gwy_data_field_get_data_line_averaged(GwyDataField *data_field,
                                      GwyDataLine* data_line,
                                      gint ulcol, gint ulrow,
                                      gint brcol, gint brrow,
                                      gint res, gint thickness,
                                      GwyInterpolationType interpolation)
{
    gint k, j;
    gdouble cosa, sina, size, mid, sum;
    gdouble col, row, srcol, srrow;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(data_line));
    g_return_if_fail(ulcol >= 0 && ulrow >= 0
                     && brcol >= 0 && brrow >= 0
                     && ulrow <= data_field->yres && ulcol <= data_field->xres
                     && brrow <= data_field->yres && brcol <= data_field->xres);

    size = hypot(ulcol - brcol, ulrow - brrow);
    if (res <= 0)
        res = ROUND(size);

    cosa = (brcol - ulcol)/(res - 1.0);
    sina = (brrow - ulrow)/(res - 1.0);

    /*extract regular one-pixel line*/
    gwy_data_line_resample(data_line, res, GWY_INTERPOLATION_NONE);
    for (k = 0; k < res; k++)
        data_line->data[k] = gwy_data_field_get_dval(data_field,
                                                     ulcol + 0.5 + k*cosa,
                                                     ulrow + 0.5 + k*sina,
                                                     interpolation);
    data_line->real = size*data_field->xreal/data_field->xres;

    if (thickness <= 1)
        return;

    /*add neighbour values to the line*/
    for (k = 0; k < res; k++) {
        mid = data_line->data[k];
        sum = 0;
        for (j = -thickness/2; j < thickness - thickness/2; j++) {
            srcol = ulcol + 0.5 + k*cosa;
            srrow = ulrow + 0.5 + k*sina;
            col = (srcol + j*sina);
            row = (srrow + j*cosa);
            if (col >= 0 && col < (data_field->xres-1)
                && row >= 0 && row < (data_field->yres-1)) {
                sum += gwy_data_field_get_dval(data_field,
                                               col, row,
                                               interpolation);
            }
            else
                sum += mid;
        }
        data_line->data[k] = sum/(gdouble)thickness;
    }
}

/**
 * gwy_data_field_get_xder:
 * @data_field: A data field.
 * @col: Column index.
 * @row: Row index.
 *
 * Computes central derivative in X direction.
 *
 * On border points, one-side derivative is returned.
 *
 * Returns: Derivative in X direction.
 **/
gdouble
gwy_data_field_get_xder(GwyDataField *data_field,
                        gint col, gint row)
{
    gdouble *p;

    g_return_val_if_fail(gwy_data_field_inside(data_field, col, row), 0.0);

    p = data_field->data + row*data_field->xres + col;
    if (col == 0)
        return (*(p+1) - *p) * data_field->xres/data_field->xreal;
    if (col == data_field->xres-1)
        return (*p - *(p-1)) * data_field->xres/data_field->xreal;
    return (*(p+1) - *(p-1)) * data_field->xres/data_field->xreal/2;
}

/**
 * gwy_data_field_get_yder:
 * @data_field: A data field.
 * @col: Column index.
 * @row: Row index.
 *
 * Computes central derivative in Y direction.
 *
 * On border points, one-side derivative is returned.
 *
 * Returns: Derivative in Y direction
 **/
gdouble
gwy_data_field_get_yder(GwyDataField *data_field,
                        gint col, gint row)
{
    gdouble *p;
    gint xres;

    g_return_val_if_fail(gwy_data_field_inside(data_field, col, row), 0.0);

    xres = data_field->xres;
    p = data_field->data + row*xres + col;
    if (row == 0)
        return (*p - *(p+xres)) * data_field->yres/data_field->yreal;
    if (row == data_field->yres-1)
        return (*(p-xres) - *p) * data_field->yres/data_field->yreal;
    return (*(p-xres) - *(p+xres)) * data_field->yres/data_field->yreal/2;
}

/**
 * gwy_data_field_get_angder:
 * @data_field: A data field.
 * @col: Column index.
 * @row: Row index.
 * @theta: Angle defining the direction (in radians, counterclockwise).
 *
 * Computes derivative in direction specified by given angle.
 *
 * Returns: Derivative in direction given by angle @theta.
 **/
gdouble
gwy_data_field_get_angder(GwyDataField *data_field,
                          gint col, gint row,
                          gdouble theta)
{
    g_return_val_if_fail(gwy_data_field_inside(data_field, col, row), 0.0);
    return gwy_data_field_get_xder(data_field, col, row)*cos(theta)
           + gwy_data_field_get_yder(data_field, col, row)*sin(theta);
}

/**
 * gwy_data_field_fit_lines:
 * @data_field: A data field.
 * @ulcol: Upper-left column coordinate.
 * @ulrow: Upper-left row coordinate.
 * @brcol: Bottom-right column coordinate + 1.
 * @brrow: Bottom-right row coordinate + 1.
 * @degree: Fitted polynom degree.
 * @exclude: If %TRUE, outside of area selected by @ulcol, @ulrow, @brcol,
 *           @brrow will be used for polynom coefficients computation, instead
 *           of inside.
 * @orientation: Line orientation.
 *
 * Independently levels profiles on each row/column in a data field.
 *
 * Lines that have no intersection with area selected by @ulcol, @ulrow,
 * @brcol, @brrow are always leveled as a whole.  Lines that have intersection
 * with selected area, are leveled using polynom coefficients computed only
 * from data inside (or outside for @exclude = %TRUE) the area.
 **/
void
gwy_data_field_fit_lines(GwyDataField *data_field,
                         gint ulcol, gint ulrow,
                         gint brcol, gint brrow,
                         gint degree,
                         gboolean exclude,
                         GwyOrientation orientation)
{

    gint i, j, xres, yres, res;
    gdouble real, coefs[4];
    GwyDataLine *hlp, *xdata = NULL, *ydata = NULL;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    xres = data_field->xres;
    yres = data_field->yres;
    res = (orientation == GWY_ORIENTATION_HORIZONTAL) ? xres : yres;
    real = (orientation == GWY_ORIENTATION_HORIZONTAL)
           ? data_field->xreal : data_field->yreal;
    hlp = gwy_data_line_new(res, real, FALSE);
    if (exclude) {
        xdata = gwy_data_line_new(res, real, FALSE);
        ydata = gwy_data_line_new(res, real, FALSE);
    }

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    if (orientation == GWY_ORIENTATION_HORIZONTAL) {
        if (exclude) {
            for (i = j = 0; i < xres; i++) {
                if (i < ulcol || i >= brcol)
                    xdata->data[j++] = i;
            }
        }

        for (i = 0; i < yres; i++) {
            gwy_data_field_get_row(data_field, hlp, i);
            if (i >= ulrow && i < brrow) {
                if (exclude) {
                    memcpy(ydata->data, hlp->data, ulcol*sizeof(gdouble));
                    memcpy(ydata->data + ulcol, hlp->data + brcol,
                           (xres - brcol)*sizeof(gdouble));
                    gwy_math_fit_polynom(xres - (brcol - ulcol),
                                         xdata->data, ydata->data, degree,
                                         coefs);
                }
                else
                    gwy_data_line_part_fit_polynom(hlp, degree, coefs,
                                                   ulcol, brcol);
            }
            else
                gwy_data_line_fit_polynom(hlp, degree, coefs);
            gwy_data_line_subtract_polynom(hlp, degree, coefs);
            gwy_data_field_set_row(data_field, hlp, i);
        }
    }
    else if (orientation == GWY_ORIENTATION_VERTICAL) {
        if (exclude) {
            for (i = j = 0; i < yres; i++) {
                if (i < ulrow || i >= brrow)
                    xdata->data[j++] = i;
            }
        }

        for (i = 0; i < xres; i++) {
            gwy_data_field_get_column(data_field, hlp, i);
            if (i >= ulcol && i < brcol) {
                if (exclude) {
                    memcpy(ydata->data, hlp->data, ulrow*sizeof(gdouble));
                    memcpy(ydata->data + ulrow, hlp->data + brrow,
                           (yres - brrow)*sizeof(gdouble));
                    gwy_math_fit_polynom(yres - (brrow - ulrow),
                                         xdata->data, ydata->data, degree,
                                         coefs);
                }
                else
                    gwy_data_line_part_fit_polynom(hlp, degree, coefs,
                                                   ulrow, brrow);
            }
            else
                gwy_data_line_fit_polynom(hlp, degree, coefs);
            gwy_data_line_subtract_polynom(hlp, degree, coefs);
            gwy_data_field_set_column(data_field, hlp, i);
        }
    }
    g_object_unref(hlp);
    gwy_object_unref(xdata);
    gwy_object_unref(ydata);
}

/************************** Documentation ****************************/

/**
 * SECTION:datafield
 * @title: GwyDataField
 * @short_description: Two-dimensional data representation
 *
 * #GwyDataField is an object that is used for representation of all
 * two-dimensional data matrices. Most of the basic data handling and
 * processing functions in Gwyddion are declared here as they are connected
 * with #GwyDataField.
 **/

/**
 * GwyDataField:
 *
 * The #GwyDataField struct contains private data only and should be accessed
 * using the functions below.
 **/

/**
 * gwy_data_field_invalidate:
 * @data_field: A data field to invalidate.
 *
 * Invalidates cached data field stats.
 *
 * User code should rarely need this macro, as all #GwyDataField methods do
 * proper invalidation when they change data, as well as
 * gwy_data_field_get_data() does.
 *
 * However, if you get raw data with gwy_data_field_get_data() and then mix
 * direct changes to it with calls to methods like gwy_data_field_get_max(),
 * you may need to explicitely invalidate cached values to let
 * gwy_data_field_get_max() know it has to recompute the maximum.
 **/

/**
 * gwy_data_field_duplicate:
 * @data_field: A data field to duplicate.
 *
 * Convenience macro doing gwy_serializable_duplicate() with all the necessary
 * typecasting.
 *
 * Use gwy_data_field_new_alike() if you don't want to copy data, only
 * resolutions and units.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
