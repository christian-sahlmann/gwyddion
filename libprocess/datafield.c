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
#include <stdlib.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include "datafield.h"
#include "interpolation.h"
#include "stats.h"

#define GWY_DATA_FIELD_TYPE_NAME "GwyDataField"

/* Cache operations */
#define CVAL(datafield, b)  ((datafield)->cache[GWY_DATA_FIELD_CACHE_##b])
#define CBIT(b)             (1 << GWY_DATA_FIELD_CACHE_##b)
#define CTEST(datafield, b) ((datafield)->cached & CBIT(b))

enum {
    PROP_0,
    PROP_XREAL,
    PROP_YREAL,
    PROP_UNIT_XY,
    PROP_UNIT_Z
};

static void     gwy_data_field_class_init        (GwyDataFieldClass *klass);
static void     gwy_data_field_init              (GObject *object);
static void     gwy_data_field_finalize          (GObject *object);
static void     gwy_data_field_set_property      (GObject*object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_data_field_get_property      (GObject*object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_data_field_serializable_init (GwySerializableIface *iface);
static void     gwy_data_field_watchable_init    (GwyWatchableIface *iface);
static GByteArray* gwy_data_field_serialize      (GObject *obj,
                                                  GByteArray *buffer);
static GObject* gwy_data_field_deserialize       (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
static GObject* gwy_data_field_duplicate_real    (GObject *object);
static void     gwy_data_field_clone_real        (GObject *source,
                                                  GObject *copy);
/*static void     gwy_data_field_value_changed     (GObject *object);*/

static GObjectClass *parent_class = NULL;

GType
gwy_data_field_get_type(void)
{
    static GType gwy_data_field_type = 0;

    if (!gwy_data_field_type) {
        static const GTypeInfo gwy_data_field_info = {
            sizeof(GwyDataFieldClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_data_field_class_init,
            NULL,
            NULL,
            sizeof(GwyDataField),
            0,
            (GInstanceInitFunc)gwy_data_field_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_data_field_serializable_init,
            NULL,
            NULL
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_data_field_watchable_init,
            NULL,
            NULL
        };

        gwy_debug("");
        gwy_data_field_type = g_type_register_static(G_TYPE_OBJECT,
                                                     GWY_DATA_FIELD_TYPE_NAME,
                                                     &gwy_data_field_info,
                                                     0);
        g_type_add_interface_static(gwy_data_field_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_data_field_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_data_field_type;
}

static void
gwy_data_field_serializable_init(GwySerializableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->serialize = gwy_data_field_serialize;
    iface->deserialize = gwy_data_field_deserialize;
    iface->duplicate = gwy_data_field_duplicate_real;
    iface->clone = gwy_data_field_clone_real;
}

static void
gwy_data_field_watchable_init(GwyWatchableIface *iface)
{
    gwy_debug("");
    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_data_field_class_init(GwyDataFieldClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_data_field_finalize;
    gobject_class->set_property = gwy_data_field_set_property;
    gobject_class->get_property = gwy_data_field_get_property;

    g_object_class_install_property
        (gobject_class,
         PROP_XREAL,
         g_param_spec_double("xreal",
                             "Real x dimension",
                             "X dimension in physical units",
                             G_MINDOUBLE, G_MAXDOUBLE, 1.0, G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_YREAL,
         g_param_spec_double("yreal",
                             "Real y dimension",
                             "Y dimension in physical units",
                             G_MINDOUBLE, G_MAXDOUBLE, 1.0, G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_UNIT_XY,
         g_param_spec_object("unit_xy",
                             "Units of x, y",
                             "SI unit of lateral dimensions (x, y)",
                             GWY_TYPE_SI_UNIT, G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_UNIT_Z,
         g_param_spec_object("unit_z",
                             "Units of z",
                             "SI unit of values (z)",
                             GWY_TYPE_SI_UNIT, G_PARAM_READWRITE));
}

static void
gwy_data_field_init(GObject *object)
{
    gwy_debug("");
    gwy_debug_objects_creation(object);
}

static void
gwy_data_field_finalize(GObject *object)
{
    GwyDataField *data_field = (GwyDataField*)object;

    gwy_debug("%p is dying!", data_field);
    gwy_object_unref(data_field->si_unit_xy);
    gwy_object_unref(data_field->si_unit_z);
    g_free(data_field->data);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_data_field_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyDataField *data_field = GWY_DATA_FIELD(object);

    switch (prop_id) {
        case PROP_XREAL:
        gwy_data_field_set_xreal(data_field, g_value_get_double(value));
        break;

        case PROP_YREAL:
        gwy_data_field_set_yreal(data_field, g_value_get_double(value));
        break;

        case PROP_UNIT_XY:
        gwy_data_field_set_si_unit_xy(data_field, g_value_get_object(value));
        break;

        case PROP_UNIT_Z:
        gwy_data_field_set_si_unit_z(data_field, g_value_get_object(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_data_field_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    GwyDataField *data_field = GWY_DATA_FIELD(object);

    switch (prop_id) {
        case PROP_XREAL:
        g_value_set_double(value, data_field->xreal);
        break;

        case PROP_YREAL:
        g_value_set_double(value, data_field->yreal);
        break;

        case PROP_UNIT_XY:
        g_value_set_object(value, gwy_data_field_get_si_unit_xy(data_field));
        break;

        case PROP_UNIT_Z:
        g_value_set_object(value, gwy_data_field_get_si_unit_z(data_field));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
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
                             | CBIT(MED);
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
    if (nullme) {
        data_field->data = g_new0(gdouble, data_field->xres*data_field->yres);
        /* We can precompute stats */
        data_field->cached = CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                             | CBIT(MED);
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
    guint32 datasize;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_DATA_FIELD(obj), NULL);

    data_field = GWY_DATA_FIELD(obj);
    datasize = data_field->xres*data_field->yres;
    {
        GwySerializeSpec spec[] = {
            { 'i', "xres", &data_field->xres, NULL, },
            { 'i', "yres", &data_field->yres, NULL, },
            { 'd', "xreal", &data_field->xreal, NULL, },
            { 'd', "yreal", &data_field->yreal, NULL, },
            { 'o', "si_unit_xy", &data_field->si_unit_xy, NULL, },
            { 'o', "si_unit_z", &data_field->si_unit_z, NULL, },
            { 'D', "data", &data_field->data, &datasize, },
        };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_DATA_FIELD_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_data_field_deserialize(const guchar *buffer,
                           gsize size,
                           gsize *position)
{
    guint32 fsize;
    gint xres, yres;
    gdouble xreal, yreal, *data = NULL;
    GwySIUnit *si_unit_xy = NULL, *si_unit_z = NULL;
    GwyDataField *data_field;
    GwySerializeSpec spec[] = {
        { 'i', "xres", &xres, NULL, },
        { 'i', "yres", &yres, NULL, },
        { 'd', "xreal", &xreal, NULL, },
        { 'd', "yreal", &yreal, NULL, },
        { 'o', "si_unit_xy", &si_unit_xy, NULL, },
        { 'o', "si_unit_z", &si_unit_z, NULL, },
        { 'D', "data", &data, &fsize, },
    };

    gwy_debug("");

    si_unit_z = NULL;
    si_unit_xy = NULL;

    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_DATA_FIELD_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        gwy_object_unref(si_unit_xy);
        gwy_object_unref(si_unit_z);
        return NULL;
    }
    if (fsize != (gsize)(xres*yres)) {
        g_critical("Serialized %s size mismatch %u != %u",
              GWY_DATA_FIELD_TYPE_NAME, fsize, xres*yres);
        g_free(data);
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
    if (clone->xres*clone->yres != data_field->xres*data_field->yres)
        clone->data = g_renew(gdouble, clone->data, n);
    clone->xres = data_field->xres;
    clone->yres = data_field->yres;
    memcpy(clone->data, data_field->data, n*sizeof(gdouble));

    clone->cached = data_field->cached;
    memcpy(clone->cache, data_field->cache,
           GWY_DATA_FIELD_CACHE_SIZE*sizeof(gdouble));

    g_object_freeze_notify(G_OBJECT(clone));
    if (clone->xreal != data_field->xreal) {
        clone->xreal = data_field->xreal;
        g_object_notify(G_OBJECT(clone), "xreal");
    }
    if (clone->yreal != data_field->yreal) {
        clone->yreal = data_field->yreal;
        g_object_notify(G_OBJECT(clone), "yreal");
    }

    gwy_serializable_clone(G_OBJECT(data_field->si_unit_xy),
                           G_OBJECT(clone->si_unit_xy));
    g_object_notify(G_OBJECT(clone), "unit_xy");

    gwy_serializable_clone(G_OBJECT(data_field->si_unit_z),
                           G_OBJECT(clone->si_unit_z));
    g_object_notify(G_OBJECT(clone), "unit_z");

    g_object_thaw_notify(G_OBJECT(clone));
}

/*
static void
gwy_data_field_value_changed(GObject *object)
{
    gwy_debug("signal: GwyGwyDataLine changed");
    g_signal_emit_by_name(object, "value_changed", NULL);
}
*/

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

    g_object_freeze_notify(G_OBJECT(dest));
    if (dest->xreal != src->xreal) {
        dest->xreal = src->xreal;
        g_object_notify(G_OBJECT(dest), "xreal");
    }
    if (dest->yreal != src->yreal) {
        dest->yreal = src->yreal;
        g_object_notify(G_OBJECT(dest), "yreal");
    }

    dest->cached = src->cached;
    memcpy(dest->cache, src->cache, GWY_DATA_FIELD_CACHE_SIZE*sizeof(gdouble));

    if (!nondata_too) {
        g_object_thaw_notify(G_OBJECT(dest));
        return;
    }

    gwy_serializable_clone(G_OBJECT(src->si_unit_xy),
                           G_OBJECT(dest->si_unit_xy));
    g_object_notify(G_OBJECT(dest), "unit_xy");

    gwy_serializable_clone(G_OBJECT(src->si_unit_z),
                           G_OBJECT(dest->si_unit_z));
    g_object_notify(G_OBJECT(dest), "unit_z");

    g_object_thaw_notify(G_OBJECT(dest));
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
 **/
void
gwy_data_field_resample(GwyDataField *data_field,
                        gint xres, gint yres,
                        GwyInterpolationType interpolation)
{
    gdouble *bdata;
    gdouble xratio, yratio, xpos, ypos;
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

    xratio = (data_field->xres - 1.0)/(xres - 1.0);
    yratio = (data_field->yres - 1.0)/(yres - 1.0);

    for (i = 0; i < yres; i++) {
        gdouble *row = bdata + i*xres;

        ypos = i*yratio;
        if (G_UNLIKELY(ypos > data_field->yres-1))
            ypos = data_field->yres-1;

        for (j = 0; j < xres; j++, row++) {
            xpos = j*xratio;
            if (G_UNLIKELY(xpos > data_field->xres-1))
                xpos = data_field->xres-1;
            *row = gwy_data_field_get_dval(data_field, xpos, ypos,
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
 * Extracts rectangular part of the a data field.between upper-left and
 * bottom-right points, recomputing real size.
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
    data_field->xres = xres;
    data_field->yres = yres;
    GWY_SWAP(gdouble*, data_field->data, b->data);
    g_object_unref(b);
    data_field->xreal *= xres/data_field->xres;
    data_field->yreal *= yres/data_field->yres;

    gwy_data_field_invalidate(data_field);

    g_object_freeze_notify(G_OBJECT(data_field));
    g_object_notify(G_OBJECT(data_field), "xreal");
    g_object_notify(G_OBJECT(data_field), "yreal");
    g_object_thaw_notify(G_OBJECT(data_field));
}

/**
 * gwy_data_field_get_dval:
 * @data_field: A data field
 * @x: X position in pixel units.
 * @y: Y postition in pixel units.
 * @interpolation: Interpolation method to be used.
 *
 * Gets interpolated value at arbitrary data field point indexed by pixel
 * coordinates.
 *
 * See also gwy_data_field_get_dval_real() that does the same, but takes
 * real coordinates.
 *
 * Returns: Value at position (@x,@y).
 **/
gdouble
gwy_data_field_get_dval(GwyDataField *a, gdouble x, gdouble y,
                        GwyInterpolationType interpolation)
{
    gint ix, iy, i;
    gint floorx, floory;
    gdouble restx, resty, valpx, valxp, valpp, va, vb, vc, vd;
    gdouble intline[4];

    g_return_val_if_fail(GWY_IS_DATA_FIELD(a), 0.0);
    if (x < 0 && x > -0.01)
        x = 0;
    if (y < 0 && x > -0.01)
        y = 0;
    g_return_val_if_fail(x >= 0 && y >= 0 && y < a->yres && x < a->xres, 0.0);

    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        return 0.0;

        case GWY_INTERPOLATION_ROUND:
        ix = (gint)(x + 0.5);
        iy = (gint)(y + 0.5);
        return a->data[ix + a->xres*iy];

        case GWY_INTERPOLATION_BILINEAR:
        floorx = (gint)floor(x);
        floory = (gint)floor(y);
        restx = x - (gdouble)floorx;
        resty = y - (gdouble)floory;

        if (restx != 0)
            valpx = restx*(1 - resty)*a->data[floorx + 1 + a->xres*floory];
        else
            valpx = 0;

        if (resty != 0)
            valxp = resty*(1 - restx)*a->data[floorx + a->xres*(floory + 1)];
        else
            valxp = 0;

        if (restx != 0 && resty != 0)
            valpp = restx*resty*a->data[floorx + 1 + a->xres*(floory + 1)];
        else
            valpp = 0;

        return valpx + valxp + valpp
               + (1 - restx)*(1 - resty)*a->data[floorx + a->xres*floory];


        default:
        floorx = (gint)floor(x);
        floory = (gint)floor(y);
        restx = x - (gdouble)floorx;
        resty = y - (gdouble)floory;

        /*return ROUND result if we have no space for interpolations*/
        if (floorx < 1 || floory < 1
            || floorx >= (a->xres-2) || floory >= (a->yres-2)) {
            ix = (gint)(x + 0.5);
            iy = (gint)(y + 0.5);
            return a->data[ix + a->xres*iy];
        }

        /*interpolation in x direction*/
        for (i = 0; i < 4; i++)
            intline[i] = a->data[floorx - 1 + i + a->xres * (floory - 1)];
        va = gwy_interpolation_get_dval_of_equidists(restx, intline,
                                                     interpolation);
        for (i = 0; i < 4; i++)
            intline[i] = a->data[floorx - 1 + i + a->xres * (floory)];
        vb = gwy_interpolation_get_dval_of_equidists(restx, intline,
                                                     interpolation);
        for (i = 0; i < 4; i++)
            intline[i] = a->data[floorx - 1 + i + a->xres * (floory + 1)];
        vc = gwy_interpolation_get_dval_of_equidists(restx, intline,
                                                     interpolation);
        for (i = 0; i < 4; i++)
            intline[i] = a->data[floorx - 1 + i + a->xres * (floory + 2)];
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
 * Gets the data of a data field.
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
 * Gets the data of a data field, read-only.
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
    if (data_field->xreal == xreal)
        return;

    data_field->xreal = xreal;
    g_object_notify(G_OBJECT(data_field), "xreal");
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
    if (data_field->yreal == yreal)
        return;

    data_field->yreal = yreal;
    g_object_notify(G_OBJECT(data_field), "yreal");
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
        data_field->si_unit_xy = gwy_si_unit_new("m");

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
        data_field->si_unit_z = gwy_si_unit_new("m");

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
    g_object_notify(G_OBJECT(data_field), "unit_xy");
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
    g_object_notify(G_OBJECT(data_field), "unit_z");
}

/**
 * gwy_data_field_get_value_format_xy:
 * @data_field: A data field.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying coordinates of a data field.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_data_field_get_value_format_xy(GwyDataField *data_field,
                                   GwySIValueFormat *format)
{
    gdouble max, unit;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);

    max = MAX(data_field->xreal, data_field->yreal);
    unit = MIN(data_field->xreal/data_field->xres,
               data_field->yreal/data_field->yres);
    return gwy_si_unit_get_format_with_resolution
                                   (gwy_data_field_get_si_unit_xy(data_field),
                                    max, unit, format);
}

/**
 * gwy_data_field_get_value_format_z:
 * @data_field: A data field.
 * @format: A SI value format to modify, or %NULL to allocate a new one.
 *
 * Finds value format good for displaying values of a data field.
 *
 * Returns: The value format.  If @format is %NULL, a newly allocated format
 *          is returned, otherwise (modified) @format itself is returned.
 **/
GwySIValueFormat*
gwy_data_field_get_value_format_z(GwyDataField *data_field,
                                  GwySIValueFormat *format)
{
    gdouble max, min;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);

    max = fabs(gwy_data_field_get_max(data_field));
    min = fabs(gwy_data_field_get_min(data_field));
    max = MAX(min, max);

    return gwy_si_unit_get_format(gwy_data_field_get_si_unit_z(data_field),
                                  max, format);
}

/**
 * gwy_data_field_itor:
 * @data_field: A data field.
 * @row: Row (pixel) coordinate.
 *
 * Transforms row pixel coordinate to real (physical) Y coordinate.
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
 * @col: Column (pixel) coordinate.
 *
 * Transforms column pixel coordinate to real (physical) X coordinate.
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
 * Returns: Row pixel coodinate.
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
 * Returns: Column pixel coordinate.
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
 * @angle: Angle (in radians).
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
        jcor = a->xres-1;
    }
    if (fabs(angle - 3*G_PI/4) < 3e-15) {
        sn = -1.0;
        cs = 0.0;
        icor = a->yres;
        jcor = a->xres-1;
    }
    else {
        sn = sin(angle);
        cs = cos(angle);
        icor = (gdouble)a->yres/2
                + G_SQRT2*(gdouble)a->yres/2*sin(ang)
                - sn*(a->xres-a->yres)/2;
        jcor = (gdouble)a->xres/2
               + G_SQRT2*(gdouble)a->xres/2*cos(ang)
               + sn*(a->xres-a->yres)/2;
    }

    for (i = 0; i < a->yres; i++) { /*row*/
        for (j = 0; j < a->xres; j++) { /*column*/
            ir = a->yres-i-icor;
            jr = j-jcor;
            inew = -ir*cs + jr*sn;
            jnew = ir*sn + jr*cs;
            if (inew > a->yres || jnew > a->xres || inew < -1 || jnew < -1)
                a->data[j + a->xres*i] = val;
            else {
                inew = CLAMP(inew, 0, a->yres - 1);
                jnew = CLAMP(jnew, 0, a->xres - 1);
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

    /* Nothing changes in cache */
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
                         | CBIT(MED);
    CVAL(data_field, MIN) = value;
    CVAL(data_field, MAX) = value;
    CVAL(data_field, SUM) = data_field->xres * data_field->yres * value;
    CVAL(data_field, RMS) = 0.0;
    CVAL(data_field, MED) = value;
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
                         | CBIT(MED);
    CVAL(data_field, MIN) = 0.0;
    CVAL(data_field, MAX) = 0.0;
    CVAL(data_field, SUM) = 0.0;
    CVAL(data_field, RMS) = 0.0;
    CVAL(data_field, MED) = 0.0;
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
                          | CBIT(MED);
    CVAL(data_field, MIN) *= value;
    CVAL(data_field, MAX) *= value;
    CVAL(data_field, SUM) *= value;
    CVAL(data_field, RMS) *= value;
    CVAL(data_field, MED) *= value;
    if (value < 0)
        GWY_SWAP(gdouble, CVAL(data_field, MIN), CVAL(data_field, MAX));
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
                          | CBIT(MED);
    CVAL(data_field, MIN) += value;
    CVAL(data_field, MAX) += value;
    CVAL(data_field, SUM) += data_field->xres * data_field->yres * value;
    /* RMS doesn't change */
    CVAL(data_field, MED) += value;
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
 * @res: Requested resolution of data line.
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

    size = sqrt((ulcol - brcol)*(ulcol - brcol)
                + (ulrow - brrow)*(ulrow - brrow));
    if (res <= 0)
        res = (gint)size;

    cosa = (brcol - ulcol)/(res - 1.0);
    sina = (brrow - ulrow)/(res - 1.0);

    gwy_data_line_resample(data_line, res, GWY_INTERPOLATION_NONE);
    for (k = 0; k < res; k++)
        data_line->data[k] = gwy_data_field_get_dval(data_field,
                                                     ulcol + k*cosa,
                                                     ulrow + k*sina,
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
 * @res: Requested resolution of data line.
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

    size = sqrt((ulcol - brcol)*(ulcol - brcol)
                + (ulrow - brrow)*(ulrow - brrow));
    if (res <= 0)
        res = (gint)size;

    cosa = (gdouble)(brcol - ulcol)/(res - 1);
    sina = (gdouble)(brrow - ulrow)/(res - 1);

    /*extract regular one-pixel line*/
    gwy_data_line_resample(data_line, res, GWY_INTERPOLATION_NONE);
    for (k = 0; k < res; k++)
        data_line->data[k] = gwy_data_field_get_dval(data_field,
                                                     ulcol + k*cosa,
                                                     ulrow + k*sina,
                                                     interpolation);
    data_line->real = size*data_field->xreal/data_field->xres;

    if (thickness <= 1)
        return;

    /*add neighbour values to the line*/
    for (k = 0; k < res; k++) {
        mid = data_line->data[k];
        sum = 0;
        for (j = -thickness/2; j < thickness - thickness/2; j++) {
            srcol = ulcol + k*cosa;
            srrow = ulrow + k*sina;
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
 *
 * Note, no stats are actually cached in Gwyddion 1.x, but they will be cached
 * in 2.x.
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
