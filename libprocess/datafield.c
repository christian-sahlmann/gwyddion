/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <libgwyddion/gwymacros.h>
#include "datafield.h"
#include "cwt.h"

#define GWY_DATA_FIELD_TYPE_NAME "GwyDataField"

static void     gwy_data_field_class_init        (GwyDataFieldClass *klass);
static void     gwy_data_field_init              (GwyDataField *data_field);
static void     gwy_data_field_finalize          (GwyDataField *data_field);
static void     gwy_data_field_serializable_init (gpointer giface);
static void     gwy_data_field_watchable_init    (gpointer giface);
static guchar*  gwy_data_field_serialize         (GObject *obj,
                                                  guchar *buffer,
                                                  gsize *size);
static GObject* gwy_data_field_deserialize       (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
static GObject* gwy_data_field_duplicate         (GObject *object);
static void     gwy_data_field_value_changed     (GObject *GwyDataField);

/*local functions*/
static void     gwy_data_field_mult_wav          (GwyDataField *real_field, 
                                                  GwyDataField *imag_field,
                                                  gdouble scale,
                                                  Gwy2DCWTWaveletType wtype);

gdouble         edist                             (gint x1, gint y1, gint x2, gint y2);

#define ROUND(x) ((gint)floor((x) + 0.5))

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

        gwy_debug("%s", __FUNCTION__);
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
gwy_data_field_serializable_init(gpointer giface)
{
    GwySerializableClass *iface = giface;

    gwy_debug("%s", __FUNCTION__);
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_SERIALIZABLE);

    /* initialize stuff */
    iface->serialize = gwy_data_field_serialize;
    iface->deserialize = gwy_data_field_deserialize;
    iface->duplicate = gwy_data_field_duplicate;
}

static void
gwy_data_field_watchable_init(gpointer giface)
{
    GwyWatchableClass *iface = giface;

    gwy_debug("%s", __FUNCTION__);
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_WATCHABLE);

    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_data_field_class_init(GwyDataFieldClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("%s", __FUNCTION__);

    gobject_class->finalize = (GObjectFinalizeFunc)gwy_data_field_finalize;
}

static void
gwy_data_field_init(GwyDataField *data_field)
{
    gwy_debug("%s", __FUNCTION__);
    data_field->data = NULL;
    data_field->xres = 0;
    data_field->yres = 0;
    data_field->xreal = 0.0;
    data_field->yreal = 0.0;
}

static void
gwy_data_field_finalize(GwyDataField *data_field)
{
    gwy_debug("%s", __FUNCTION__);
    gwy_data_field_free(data_field);
}

GObject*
gwy_data_field_new(gint xres, gint yres,
                   gdouble xreal, gdouble yreal,
                   gboolean nullme)
{
    GwyDataField *data_field;

    gwy_debug("%s", __FUNCTION__);
    data_field = g_object_new(GWY_TYPE_DATA_FIELD, NULL);

    gwy_data_field_initialize(data_field, xres, yres, xreal, yreal, nullme);

    return (GObject*)(data_field);
}

static guchar*
gwy_data_field_serialize(GObject *obj,
                         guchar *buffer,
                         gsize *size)
{
    GwyDataField *data_field;
    gsize datasize;

    gwy_debug("%s", __FUNCTION__);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(obj), NULL);

    data_field = GWY_DATA_FIELD(obj);
    datasize = data_field->xres*data_field->yres;
    {
        GwySerializeSpec spec[] = {
            { 'i', "xres", &data_field->xres, NULL, },
            { 'i', "yres", &data_field->yres, NULL, },
            { 'd', "xreal", &data_field->xreal, NULL, },
            { 'd', "yreal", &data_field->yreal, NULL, },
            { 'D', "data", &data_field->data, &datasize, },
        };
        return gwy_serialize_pack_object_struct(buffer, size,
                                                GWY_DATA_FIELD_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_data_field_deserialize(const guchar *buffer,
                           gsize size,
                           gsize *position)
{
    gsize fsize;
    gint xres, yres;
    gdouble xreal, yreal, *data = NULL;
    GwyDataField *data_field;
    GwySerializeSpec spec[] = {
        { 'i', "xres", &xres, NULL, },
        { 'i', "yres", &yres, NULL, },
        { 'd', "xreal", &xreal, NULL, },
        { 'd', "yreal", &yreal, NULL, },
        { 'D', "data", &data, &fsize, },
    };

    gwy_debug("%s", __FUNCTION__);
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_DATA_FIELD_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        return NULL;
    }
    if (fsize != (gsize)(xres*yres)) {
        g_critical("Serialized %s size mismatch %u != %u",
              GWY_DATA_FIELD_TYPE_NAME, fsize, xres*yres);
        g_free(data);
        return NULL;
    }

    /* don't allocate large amount of memory just to immediately free it */
    data_field = (GwyDataField*)gwy_data_field_new(1, 1, xreal, yreal, FALSE);
    g_free(data_field->data);
    data_field->data = data;
    data_field->xres = xres;
    data_field->yres = yres;

    return (GObject*)data_field;
}

static GObject*
gwy_data_field_duplicate(GObject *object)
{
    GwyDataField *data_field;
    GObject *duplicate;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(object), NULL);
    data_field = GWY_DATA_FIELD(object);
    duplicate = gwy_data_field_new(data_field->xres, data_field->yres,
                                   data_field->xreal, data_field->yreal,
                                   FALSE);
    gwy_data_field_copy(data_field, GWY_DATA_FIELD(duplicate));

    return duplicate;
}

static void
gwy_data_field_value_changed(GObject *data_field)
{
    gwy_debug("signal: GwyGwyDataLine changed");
    g_signal_emit_by_name(GWY_DATA_FIELD(data_field), "value_changed", NULL);
}


/**
 * gwy_data_field_alloc:
 * @a: pointer fo field to be allocated
 * @xres: X resolution
 * @yres: Y resolution
 *
 * Allocates GwyDataField
 **/
void
gwy_data_field_alloc(GwyDataField *a, gint xres, gint yres)
{
    gwy_debug("%s", __FUNCTION__);

    a->xres = xres;
    a->yres = yres;
    a->data = g_new(gdouble, a->xres*a->yres);
}

/**
 * gwy_data_field_initialize:
 * @a: A data field to be initialized
 * @xres: X resolution
 * @yres: Y resolution
 * @xreal: X real dimension of the field
 * @yreal: Y real dimension of the field
 * @nullme: true if field should be filled with zeros
 *
 * Allocates and initializes GwyDataField.
 **/
void
gwy_data_field_initialize(GwyDataField *a,
                          gint xres, gint yres,
                          gdouble xreal, gdouble yreal,
                          gboolean nullme)
{
    int i;

    gwy_debug("%s (%dx%d)", __FUNCTION__, xres, yres);

    gwy_data_field_alloc(a, xres, yres);
    a->xreal = xreal;
    a->yreal = yreal;
    if (nullme) {
        for (i = 0; i < (a->xres*a->yres); i++)
            a->data[i] = 0;
    }
}

/**
 * gwy_data_field_free:
 * @a: A data field to be freed
 *
 * Frees GwyDataField
 **/
void
gwy_data_field_free(GwyDataField *a)
{
    gwy_debug("%s", __FUNCTION__);
    g_free(a->data);
}

/**
 * gwy_data_field_copy:
 * @a: source data field.
 * @b: destination data field.
 *
 * Copies the contents of an already allocated data field to a data field
 * of the same size.
 *
 * Returns:
 **/
gboolean
gwy_data_field_copy(GwyDataField *a, GwyDataField *b)
{
    g_return_val_if_fail(a->xres == b->xres && a->yres == b->yres, FALSE);

    b->xreal = a->xreal;
    b->yreal = a->yreal;
    memcpy(b->data, a->data, a->xres*a->yres*sizeof(gdouble));

    return TRUE;
}


/**
 * gwy_data_field_resample:
 * @a: A data field to be resampled
 * @xres: desired X resolution
 * @yres: desired Y resolution
 * @interpolation: interpolation method
 *
 * Resamples GwyDataField using given interpolation method
 **/
/* XY: yeti */
void
gwy_data_field_resample(GwyDataField *a,
                        gint xres, gint yres,
                        GwyInterpolationType interpolation)
{
    GwyDataField b;
    gdouble xratio, yratio, xpos, ypos;
    gint i,j;

    if (a->xres == xres && a->yres == yres)
        return;

    if (interpolation != GWY_INTERPOLATION_NONE) {
        gwy_data_field_alloc(&b, a->xres, a->yres);
        gwy_data_field_copy(a, &b);
    }

    a->xres = xres;
    a->yres = yres;
    a->data = g_renew(gdouble, a->data, a->xres*a->yres);

    if (interpolation == GWY_INTERPOLATION_NONE)
        return;

    xratio = (gdouble)(b.xres-1)/(gdouble)(a->xres-1);
    yratio = (gdouble)(b.yres-1)/(gdouble)(a->yres-1);

    for (i = 0; i < a->yres; i++) {
        gdouble *row = a->data + i*a->xres;

        ypos = (gdouble)i*yratio;
        if (ypos > (b.yres-1))
            ypos = (b.yres-1);

        for (j = 0; j < a->xres; j++, row++) {
            xpos = (gdouble)j*xratio;
            if (xpos > (b.xres-1))
                xpos = (b.xres-1);
            /*printf("(%d, %d), -> %f, %f\n",i, j, xpos, ypos);*/
            *row = gwy_data_field_get_dval(&b, xpos, ypos, interpolation);
        }
    }
    gwy_data_field_free(&b);
}

void
gwy_data_field_confirmsize(GwyDataField *a, gint xres, gint yres)
{
    if (a->data == NULL)
        gwy_data_field_initialize(a, xres, yres, xres, yres, FALSE);
    else if (a->xres != xres)
        gwy_data_field_resample(a, xres, yres, GWY_INTERPOLATION_NONE);
}

/**
 * gwy_data_field_resize:
 * @a: A data field to be resized
 * @ulcol: upper-left column coordinate
 * @ulrow: upper-left row coordinate
 * @brcol: bottom-right column coordinate
 * @brrow: bottom-right row coordinate
 *
 * Resizes (crops) the GwyDataField.
 *
 * Extracts part of the GwyDataField.between
 * upper-left and bottom-right points.
 *
 * Returns:%TRUE at success
 **/
/* XY: yeti */
gboolean
gwy_data_field_resize(GwyDataField *a, gint ulcol, gint ulrow, gint brcol, gint brrow)
{
    GwyDataField b;
    gint i, xres, yres;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0 && brcol < a->xres && brrow < a->yres,
                         FALSE);

    yres = brrow - ulrow;
    xres = brcol - ulcol;
    gwy_data_field_alloc(&b, xres, yres);

    for (i = ulrow; i < brrow; i++) {
        memcpy(b.data + (i-ulrow)*xres,
               a->data + i*a->xres + ulcol,
               xres*sizeof(gdouble));
    }
    a->xres = xres;
    a->yres = yres;
    GWY_SWAP(gdouble*, a->data, b.data);

    gwy_data_field_free(&b);
    return TRUE;
}

/**
 * gwy_data_field_get_dval:
 * @a: A data field
 * @x: x position
 * @y: y postition
 * @interpolation: interpolation method to be used
 *
 * Interpolates to extract a value of the field in arbitrary position.
 *
 * Returns: value at the position (x,y).
 **/
/* XY: yeti */
gdouble
gwy_data_field_get_dval(GwyDataField *a, gdouble x, gdouble y,
                        GwyInterpolationType interpolation)
{
    gint ix, iy;
    gint floorx, floory;
    gdouble restx, resty, valpx, valxp, valpp;

    if (x<0 && x>-0.1) x = 0;
    if (y<0 && x>-0.1) y = 0;
    
    if (!(x >= 0 && y >= 0 && y < a->yres && x < a->xres))
        printf("GRRRRRRRRRRRR: %f %f\n", x, y);
    g_return_val_if_fail(x >= 0 && y >= 0 && y < a->yres && x < a->xres,
                         0.0);

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
        break;

        default:
        g_warning("Not supported interpolation type.\n");
        return 0.0;
    }
}


/**
 * gwy_data_field_get_data:
 * @a: A data field
 *
 * Get the data of the field.
 *
 * Returns: The data field as a pointer to an array of
 *          gwy_data_field_get_xres()*gwy_data_field_get_yres() #gdouble's,
 *          ordered by lines.  I.e., they are to be accessed as
 *          data[row*xres + column].
 **/
gdouble*
gwy_data_field_get_data(GwyDataField *a)
{
    return a->data;
}

/**
 * gwy_data_field_get_xres:
 * @a: A data field
 *
 * Get X resolution of the field.
 *
 * Returns:X resolution
 **/
gint
gwy_data_field_get_xres(GwyDataField *a)
{
    return a->xres;
}

/**
 * gwy_data_field_get_yres:
 * @a: A data field
 *
 * Get Y resolution of the field.
 *
 * Returns: Y resolution
 **/
gint
gwy_data_field_get_yres(GwyDataField *a)
{
    return a->yres;
}

/**
 * gwy_data_field_get_xreal:
 * @a: A data field
 *
 * Get the X real size value
 *
 * Returns:X real size value
 **/
gdouble
gwy_data_field_get_xreal(GwyDataField *a)
{
    return a->xreal;
}

/**
 * gwy_data_field_get_yreal:
 * @a: A data field
 *
 * Get the Y real size value
 *
 * Returns: Y real size value
 **/
gdouble
gwy_data_field_get_yreal(GwyDataField *a)
{
    return a->yreal;
}

/**
 * gwy_data_field_set_xreal:
 * @a: A data field
 * @xreal: new X real size value
 *
 * Set the X real size value
 **/
void
gwy_data_field_set_xreal(GwyDataField *a, gdouble xreal)
{
    a->xreal = xreal;
}

/**
 * gwy_data_field_set_yreal:
 * @a: A data field
 * @yreal: new Y real size value
 *
 * Set the Y real size value
 **/
void
gwy_data_field_set_yreal(GwyDataField *a, gdouble yreal)
{
    a->yreal = yreal;
}

/**
 * gwy_data_field_itor:
 * @a: A data field
 * @pixval: value at data (pixel) coordinates
 *
 * recomputes row pixel coordinate to real coordinate
 *
 * Returns: recomputed value
 **/
gdouble
gwy_data_field_itor(GwyDataField *a, gdouble pixval)
{
    return (gdouble)pixval*a->yreal/a->yres;
}

/**
 * gwy_data_field_jtor:
 * @a: A data field
 * @pixval:  value at real coordinates
 *
 * recomputes column real coordinate to pixel coordinate
 *
 * Note: for field represented by square grid (same distance
 * between adjacent pixels in X and Y dimension, the
 * functions gwy_data_field_itor() and gwy_data_field_jtor()
 * are  identical.
 * Returns: recomputed value
 **/
gdouble
gwy_data_field_jtor(GwyDataField *a, gdouble pixval)
{
    return (gdouble)pixval*a->xreal/a->xres;
}


/**
 * gwy_data_field_rtoi:
 * @a: A data field
 * @realval:  value at real coordinates
 *
 * recomputes row real coordinate to pixel coordinate
 *
 * Returns: recomputed value
 **/
gdouble
gwy_data_field_rtoi(GwyDataField *a, gdouble realval)
{
    return realval*a->yres/a->yreal;
}


/**
 * gwy_data_field_rtoj:
 * @a: A data field
 * @realval:  value at real coordinates
 *
 * recomputes column real coordinate to pixel coordinate
 *
 * Returns: recomputed value
 **/
gdouble
gwy_data_field_rtoj(GwyDataField *a, gdouble realval)
{
    return realval*a->xres/a->xreal;
}

gboolean
gwy_data_field_inside(GwyDataField *a, gint i, gint j)
{
    if (i >= 0 && j >= 0 && i < a->xres && j < a->yres)
        return TRUE;
    else
        return FALSE;
}


/**
 * gwy_data_field_get_val:
 * @a: A data field
 * @col: column position
 * @row: row position
 *
 * Get value at given pixel
 *
 * Returns: value at (i, j)
 **/
gdouble
gwy_data_field_get_val(GwyDataField *a, gint col, gint row)
{
    g_return_val_if_fail(gwy_data_field_inside(a, col, row), 0.0);
    return a->data[col + a->xres*row];
}

/**
 * gwy_data_field_set_val:
 * @a: A data field
 * @col: column position
 * @row: row position
 * @value: value to set
 *
 * Set @value at given pixel
 **/
void
gwy_data_field_set_val(GwyDataField *a, gint col, gint row, gdouble value)
{
    g_return_if_fail(gwy_data_field_inside(a, col, row));
    a->data[col + a->xres*row] = value;
}

/**
 * gwy_data_field_get_dval_real:
 * @a: A data field
 * @x: x postion in real coordinates
 * @y: y postition in real coordinates
 * @interpolation: interpolation method
 *
 * Get value at arbitrary point given by real values.
 *
 * See also gwy_data_field_get_dval() that does the same for arbitrary point
 * given by data (pixel) coordinate values.
 *
 * Returns: value at point x, y
 **/
gdouble
gwy_data_field_get_dval_real(GwyDataField *a, gdouble x, gdouble y,
                             GwyInterpolationType interpolation)
{
    return  gwy_data_field_get_dval(a,
                                    gwy_data_field_rtoi(a, x),
                                    gwy_data_field_rtoj(a, y),
                                    interpolation);
}

/**
 * gwy_data_field_rotate:
 * @a: A data field
 * @angle: angle (in degrees)
 * @interpolation: interpolation method
 *
 * Rotates field by a given angle.
 *
 * The values that will be outside of square after rotation will
 * be lost. The new unknown values will be set to field minimum value.
 * */
void
gwy_data_field_rotate(GwyDataField *a, gdouble angle,
                      GwyInterpolationType interpolation)
{
    GwyDataField b;
    gdouble inew, jnew, ir, jr, ang, icor, jcor, sn, cs, val;
    gint i,j;

    angle = fmod(angle, 360.0);
    if (angle < 0.0)
        angle += 360.0;

    if (angle == 0.0)
        return;

    gwy_data_field_alloc(&b, a->xres, a->yres);
    gwy_data_field_copy(a, &b);

    val = gwy_data_field_get_min(a);
    ang = 3*G_PI/4 + angle*G_PI/180;
    sn = sin(angle*G_PI/180);
    cs = cos(angle*G_PI/180);
    icor = (gdouble)a->yres/2
            + G_SQRT2*(gdouble)a->yres/2*sin(ang)
            - sn*(a->xres-a->yres)/2;
    jcor = (gdouble)a->xres/2
           + G_SQRT2*(gdouble)a->xres/2*cos(ang)
           + sn*(a->xres-a->yres)/2;;
    if (angle == 90.0) {
        sn = 1.0;
        cs = 0.0;
        icor = 1.0;
        jcor = 0.0;
    }
    if (angle == 180.0) {
        sn = 0.0;
        cs = -1.0;
        icor = 1.0;
        jcor = a->xres-1;
    }
    if (angle == 270.0) {
        sn = -1.0;
        cs = 0.0;
        icor = a->yres;
        jcor = a->xres-1;
    }

    for (i = 0; i < a->yres; i++) { /*row*/
        for (j = 0; j < a->xres; j++) { /*column*/
            ir = a->yres-i-icor;
            jr = j-jcor;
            /* XXX: cosinus should have no minus!? */
            inew = -ir*cs + jr*sn;
            jnew = ir*sn + jr*cs;
            if (inew > a->yres || jnew > a->xres || inew < -1 || jnew < -1)
                a->data[j + a->xres*i] = val;
            else {
                if (inew > (a->yres - 1))
                    inew = a->yres-1;
                if (jnew > (a->xres - 1))
                    jnew = a->xres-1;
                if (inew < 0) inew = 0;
                if (jnew < 0) jnew = 0;
                a->data[j + a->xres*i] = gwy_data_field_get_dval(&b, jnew, inew,
                                                                 interpolation);
            }
        }
    }

    gwy_data_field_free(&b);
}


/**
 * gwy_data_field_invert:
 * @a: pointer fo field
 * @x: invert in X direction?
 * @y: invert in Y direction?
 * @z: invert in Z direction?
 *
 * Make requested inversion(s).
 **/
void
gwy_data_field_invert(GwyDataField *a,
                      gboolean x,
                      gboolean y,
                      gboolean z)
{
    gint i,j;
    gdouble avg;
    gdouble *line, *ap, *ap2;
    gsize linelen;
    gdouble *data;

    g_return_if_fail(GWY_IS_DATA_FIELD(a));
    data = a->data;

    if (z) {
        avg = gwy_data_field_get_avg(a);
        ap = data;
        for (i = a->yres*a->xres; i; i--) {
            *ap = 2*avg - *ap;
            ap++;
        }
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
    g_free(line);
}

/**
 * gwy_data_field_fill:
 * @a: A data field
 * @value: value to be entered
 *
 * Fill GwyDataField with given value
 **/
void
gwy_data_field_fill(GwyDataField *a, gdouble value)
{
    gint i;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        *p = value;
}

/**
 * gwy_data_field_area_fill:
 * @a: A data field
 * @ulcol: upper-left column coordinate
 * @ulrow: upper-left row coordinate
 * @brcol: bottom-right column coordinate + 1
 * @brrow: bottom-right row coordinate + 1
 * @value: value to be entered
 *
 * Fill a specified part of the field witha given  value
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

    g_return_if_fail(ulcol >= 0 && ulrow >= 0 && brcol < a->xres && brrow < a->yres);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            *(row++) = value;
    }
}

/**
 * gwy_data_field_multiply:
 * @a: A data field
 * @value: value to be used for multiplication
 *
 * Multiply GwyDataField by given value.
 **/
void
gwy_data_field_multiply(GwyDataField *a, gdouble value)
{
    gint i;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        *p *= value;
}

/**
 * gwy_data_field_area_multiply:
 * @a: A data field
 * @ulcol: upper-left column coordinate
 * @ulrow: upper-left row coordinate
 * @brcol: bottom-right column coordinate + 1
 * @brrow: bottom-right row coordinate + 1
 * @value: value to be used
 *
 * Multiply a specified part of the field by the given value
 **/
void
gwy_data_field_area_multiply(GwyDataField *a,
                             gint ulcol, gint ulrow, gint brcol, gint brrow,
                             gdouble value)
{
    gint i, j;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_if_fail(ulcol >= 0 && ulrow >= 0 && brcol < a->xres && brrow < a->yres);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            *(row++) *= value;
    }
}

/**
 * gwy_data_field_add:
 * @a: A data field
 * @value: value to be added
 *
 * Add given value to GwyDataField
 **/
void
gwy_data_field_add(GwyDataField *a, gdouble value)
{
    gint i;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        *p += value;
}

/**
 * gwy_data_field_area_add:
 * @a: A data field
 * @ulcol: upper-left column coordinate
 * @ulrow: upper-left row coordinate
 * @brcol: bottom-right column coordinate + 1
 * @brrow: bottom-right row coordinate + 1
 * @value: value to be used
 *
 * Add the given value to a specified part of the field
 **/
void
gwy_data_field_area_add(GwyDataField *a,
                        gint ulcol, gint ulrow, gint brcol, gint brrow,
                        gdouble value)
{
    gint i, j;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_if_fail(ulcol >= 0 && ulrow >= 0 && brcol < a->xres && brrow < a->yres);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            *(row++) += value;
    }
}

/**
 * gwy_data_field_get_max:
 * @a: A data field
 *
 * Get maximum value of the GwyDataField.
 *
 * Returns:maximum value of the GwyDataField
 **/
gdouble
gwy_data_field_get_max(GwyDataField *a)
{
    gint i;
    gdouble max = a->data[0];
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++) {
        if (max < *p)
            max = *p;
    }
    return max;
}


gdouble
gwy_data_field_get_area_max(GwyDataField *a,
                            gint ulcol, gint ulrow, gint brcol, gint brrow)
{
    gint i, j;
    gdouble max = -G_MAXDOUBLE;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0 && brcol < a->xres && brrow < a->yres, 0);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            if (max < *row)
                max = *(row++);
    }

    return max;
}

/**
 * gwy_data_field_get_min:
 * @a: A data field
 *
 * Get minimum value of the GwyDataField
 *
 * Returns: minimum value of the GwyDataField
 **/
gdouble
gwy_data_field_get_min(GwyDataField *a)
{
    gint i;
    gdouble min = a->data[0];
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++) {
        if (min > *p)
            min = *p;
    }
    return min;
}


gdouble
gwy_data_field_get_area_min(GwyDataField *a,
                            gint ulcol, gint ulrow, gint brcol, gint brrow)
{
    gint i, j;
    gdouble min = G_MAXDOUBLE;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0 && brcol < a->xres && brrow < a->yres, 0);
    

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            if (min > *row)
                min = *(row++);
    }

    return min;
}

/**
 * gwy_data_field_get_sum:
 * @a: A data field
 *
 * Sum all the values in GwyDataField
 *
 * Returns: sum of GwyDataField.
 **/
gdouble
gwy_data_field_get_sum(GwyDataField *a)
{
    gint i;
    gdouble sum = 0;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        sum += *p;

    return sum;
}


gdouble
gwy_data_field_get_area_sum(GwyDataField *a, gint ulcol, gint ulrow, gint brcol, gint brrow)
{
    gint i, j;
    gdouble sum = 0;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0 && brcol < a->xres && brrow < a->yres, 0);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            sum += *(row++);
    }

    return sum;
}

/**
 * gwy_data_field_get_avg:
 * @a: A data field
 *
 * Averages values of GwyDataField
 *
 * Returns: Average value of GwyDataField
 **/
gdouble
gwy_data_field_get_avg(GwyDataField *a)
{
    return gwy_data_field_get_sum(a)/((gdouble)(a->xres * a->yres));
}

gdouble
gwy_data_field_get_area_avg(GwyDataField *a,
                            gint ulcol, gint ulrow, gint brcol, gint brrow)
{
    return gwy_data_field_get_area_sum(a, ulcol, ulrow, brcol, brrow)
           /((gdouble)(brcol-ulcol)*(brrow-ulrow));
}

/**
 * gwy_data_field_get_rms:
 * @a: A data field
 *
 * Evaluates Root mean square value of GwyDataField
 *
 * Returns: RMS of GwyDataField
 **/

gdouble
gwy_data_field_get_rms(GwyDataField *a)
{
    gint i, n;
    gdouble rms, sum2 = 0;
    gdouble sum = gwy_data_field_get_sum(a);
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        sum2 += (*p)*(*p);

    n = a->xres * a->yres;
    rms = sqrt(fabs(sum2 - sum*sum/n)/n);

    return rms;
}


gdouble
gwy_data_field_get_area_rms(GwyDataField *a, gint ulcol, gint ulrow, gint brcol, gint brrow)
{
    gint i, j, n;
    gdouble rms = 0, sum2 = 0;
    gdouble sum = gwy_data_field_get_area_sum(a, ulcol, ulrow, brcol, brrow);
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0 && brcol < a->xres && brrow < a->yres, 0);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
            sum2 += (*row)*(*row);
    }

    n = (brcol-ulcol)*(brrow-ulrow);
    rms = sqrt(fabs(sum2 - sum*sum/n)/n);

    return rms;
}



/**
 * gwy_data_field_threshold:
 * @a: A data field
 * @threshval: threshold value
 * @bottom: lower value
 * @top: upper value
 *
 * Tresholds values of GwyDataField. Values
 * smaller than @threshold will be set to value
 * @bottom, values higher than @threshold will be set to value @top
 *
 * Returns: total number of values above threshold.
 **/
gint
gwy_data_field_threshold(GwyDataField *a,
                         gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, tot = 0;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++) {
        if (*p < threshval)
            *p = bottom;
        else {
            *p = top;
            tot++;
        }
    }

    return tot;
}


gint
gwy_data_field_area_threshold(GwyDataField *a,
                              gint ulcol, gint ulrow, gint brcol, gint brrow,
                              gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, j, tot = 0;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0 && brcol < a->xres && brrow < a->yres, 0);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
        {
            if (*row < threshval)
                *row = bottom;
            else {
                *row = top;
                tot++;
            }
        }
    }

    return tot;
}



gint
gwy_data_field_clamp(GwyDataField *a,
                     gdouble bottom, gdouble top)
{
    gint i, tot = 0;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++) {
        if (*p < bottom) {
            *p = bottom;
            tot++;
        }
        else if (*p > top) {
            *p = top;
            tot++;
        }
    }

    return tot;
}


gint
gwy_data_field_area_clamp(GwyDataField *a,
                          gint ulcol, gint ulrow, gint brcol, gint brrow,
                          gdouble bottom, gdouble top)
{
    gint i, j, tot = 0;
    gdouble *row;

    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0 && brcol < a->xres && brrow < a->yres, 0);

    for (i = ulrow; i < brrow; i++) {
        row = a->data + i*a->xres + ulcol;

        for (j = 0; j < brcol - ulcol; j++)
        {
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
    return tot;
}


/**
 * gwy_data_field_get_row:
 * @a: A data field
 * @b: A data line
 * @row: index of row
 *
 * Extracts row into data line. Data line should be allocated allready.
 **/
void
gwy_data_field_get_row(GwyDataField *a, GwyDataLine* b, gint row)
{
    g_return_if_fail(row >= 0 && row < a->yres);

    gwy_data_line_resample(b, a->xres, GWY_INTERPOLATION_NONE);
    memcpy(b->data, a->data + row*a->xres, a->xres*sizeof(gdouble));
}


/**
 * gwy_data_field_get_column:
 * @a: A data field
 * @b: A data line
 * @col: index of column
 *
 *  Extracts column into data line. Data line should be allocated allready.
 **/
void
gwy_data_field_get_column(GwyDataField *a, GwyDataLine* b, gint col)
{
    gint k;
    gdouble *p;

    g_return_if_fail(col >= 0 && col < a->xres);

    gwy_data_line_resample(b, a->yres, GWY_INTERPOLATION_NONE);
    p = a->data + col;
    for (k = 0; k < a->yres; k++)
        b->data[k] = p[k*a->xres];
}


/**
 * gwy_data_field_set_row:
 * @a: A data field
 * @b: A data line
 * @row: index of row
 *
 * Sets the row in the data field to values of data line.
 **/
void
gwy_data_field_set_row(GwyDataField *a, GwyDataLine* b, gint row)
{
    g_return_if_fail(row >= 0 && row < a->yres);
    g_return_if_fail(a->xres == b->res);

    memcpy(a->data + row*a->xres, b->data, a->xres*sizeof(gdouble));
}


/**
 * gwy_data_field_set_column:
 * @a: A data field
 * @b: A data line
 * @col: index of column
 *
 * Sets the column in the data field to values of data line.
 **/
void
gwy_data_field_set_column(GwyDataField *a, GwyDataLine* b, gint col)
{
    gint k;
    gdouble *p;

    g_return_if_fail(col >= 0 && col < a->xres);
    g_return_if_fail(a->yres == b->res);

    p = a->data + col;
    for (k = 0; k < a->yres; k++)
        p[k*a->xres] = b->data[k];
}

/**
 * gwy_data_field_get_data_line:
 * @a: A data field
 * @b: A data line
 * @ulcol: upper-left column coordinate
 * @ulrow: upper-left row coordinate
 * @brcol: bottom-right column coordinate + 1
 * @brrow: bottom-right row coordinate + 1
 * @res: requested resolution of data line
 * @interpolation: interpolation type
 *
 * Extracts a profile from data field and
 * puts it into data line. It is expected that the data
 * line is allready allocated.
 *
 * Returns: true at success
 **/
gboolean
gwy_data_field_get_data_line(GwyDataField *a, GwyDataLine* b,
                             gint ulcol, gint ulrow, gint brcol, gint brrow,
                             gint res, GwyInterpolationType interpolation)
{
    gint k;
    gdouble cosa, sina, size;

    g_return_val_if_fail(ulcol >= 0 && ulrow >= 0 && brcol >= 0 && brrow >= 0
                         && ulrow < a->yres && ulcol < a->xres && brrow < a->yres && brcol < a->xres,
                         FALSE);

    size = sqrt((ulcol - brcol)*(ulcol - brcol) + (ulrow - brrow)*(ulrow - brrow));
    if (res<=0) res = (gint)size;
    
    cosa = (gdouble)(brcol - ulcol)/(res - 1);
    sina = (gdouble)(brrow - ulrow)/(res - 1);

    gwy_data_line_resample(b, res, GWY_INTERPOLATION_NONE);
    for (k = 0; k < res; k++)
        b->data[k] = gwy_data_field_get_dval(a, ulcol + k*cosa, ulrow + k*sina,
                                             interpolation);

    b->real = size*a->xreal/a->xres;
    
    return TRUE;
}

/**
 * gwy_data_field_plane_coeffs:
 * @a: A data field
 * @ap: coefficient
 * @bp: coefficient
 * @cp: coefficient
 *
 * Evaluates coefficients of plane fit of data field.
 **/
void
gwy_data_field_plane_coeffs(GwyDataField *a,
                           gdouble *ap, gdouble *bp, gdouble *cp)
{
    gdouble sumxi, sumxixi, sumyi, sumyiyi;
    gdouble sumsi = 0.0;
    gdouble sumsixi = 0.0;
    gdouble sumsiyi = 0.0;
    gdouble nx = a->xres;
    gdouble ny = a->yres;
    gdouble bx, by;
    gdouble *pdata;
    gint i;

    g_return_if_fail(GWY_IS_DATA_FIELD(a));

    sumxi = (nx-1)/2;
    sumxixi = (2*nx-1)*(nx-1)/6;
    sumyi = (ny-1)/2;
    sumyiyi = (2*ny-1)*(ny-1)/6;

    pdata = a->data;
    for (i = 0; i < a->xres*a->yres; i++) {
        sumsi += *pdata;
        sumsixi += *pdata * (i%a->xres);
        sumsiyi += *pdata * (i/a->xres);
        *pdata++;
    }
    sumsi /= nx*ny;
    sumsixi /= nx*ny;
    sumsiyi /= nx*ny;

    bx = (sumsixi - sumsi*sumxi) / (sumxixi - sumxi*sumxi);
    by = (sumsiyi - sumsi*sumyi) / (sumyiyi - sumyi*sumyi);
    if (bp)
        *bp = bx*nx/a->xreal;
    if (cp)
        *cp = by*ny/a->yreal;
    if (ap)
        *ap = sumsi - bx*sumxi - by*sumyi;
}


/**
 * gwy_data_field_plane_level:
 * @a: A data field
 * @ap: coefficient
 * @bp: coefficient
 * @cp: coefficient
 *
 * Plane leveling.
 **/
void
gwy_data_field_plane_level(GwyDataField *a, gdouble ap, gdouble bp, gdouble cp)
{
    gint i, j;
    gdouble bpix = bp/a->xres*a->xreal;
    gdouble cpix = cp/a->yres*a->yreal;

    for (i = 0; i < a->yres; i++) {
        gdouble *row = a->data + i*a->xres;
        gdouble rb = ap + cpix*i;

        for (j = 0; j < a->xres; j++, row++)
            *row -= rb + bpix*j;
    }
}

/**
 * gwy_data_field_plane_rotate:
 * @a: A data field
 * @xangle: rotation angle in x direction (rotation along y axis)
 * @yangle: rotation angle in y direction (rotation along x axis)
 * @interpolation: interpolation type
 *
 * Performs rotation of plane along x and y axis.
 **/
void
gwy_data_field_plane_rotate(GwyDataField *a, gdouble xangle, gdouble yangle,
                            GwyInterpolationType interpolation)
{
    int k;
    GwyDataLine l;

    gwy_data_line_alloc(&l, a->xres);

    if (xangle != 0) {
        for (k = 0; k < a->yres; k++) {
            gwy_data_field_get_row(a, &l, k);
            gwy_data_line_line_rotate(&l, xangle, interpolation);
            gwy_data_field_set_row(a, &l, k);
        }
    }
    if (yangle != 0) {
        for (k = 0; k < a->xres; k++) {
            gwy_data_field_get_column(a, &l, k);
            gwy_data_line_line_rotate(&l, yangle, interpolation);
            gwy_data_field_set_column(a, &l, k);
        }
    }
    gwy_data_line_free(&l);
}


/**
 * gwy_data_field_get_xder:
 * @a: A data field
 * @col: column coordinate
 * @row: row coordinate
 *
 * Computes derivative in x-direction.
 *
 * Returns: Derivative in x-direction
 **/
gdouble
gwy_data_field_get_xder(GwyDataField *a, gint col, gint row)
{
    gdouble *p;

    g_return_val_if_fail(gwy_data_field_inside(a, col, row), 0.0);

    p = a->data + row*a->xres + col;
    if (col == 0)
        return (*(p+1) - *p) * a->xres/a->xreal;
    if (col == a->xres-1)
        return (*p - *(p-1)) * a->xres/a->xreal;
    return (*(p+1) - *(p-1)) * a->xres/a->xreal/2;
}

/**
 * gwy_data_field_get_yder:
 * @a: A data field
 * @col: column coordinate
 * @row: row coordinate
 *
 *  Computes derivative in y-direction.
 *
 * Returns: Derivative in y-direction
 **/
gdouble
gwy_data_field_get_yder(GwyDataField *a, gint col, gint row)
{
    gdouble *p;
    gint xres;

    g_return_val_if_fail(gwy_data_field_inside(a, col, row), 0.0);

    xres = a->xres;
    p = a->data + row*xres + col;
    if (row == 0)
        return (*(p+xres) - *p) * a->yres/a->yreal;
    if (row == a->yres-1)
        return (*p - *(p-xres)) * a->yres/a->yreal;
    return (*(p+xres) - *(p-xres)) * a->yres/a->yreal/2;
}

/**
 * gwy_data_field_get_angder:
 * @a: A data field
 * @col: column coordinate
 * @row: row coordinate
 * @theta: angle specifying direction
 *
 * Computes derivative in direction specified by given angle.
 * Angle is given in degrees.
 *
 * Returns: Derivative in direction given by angle @theta
 **/
gdouble
gwy_data_field_get_angder(GwyDataField *a, gint col, gint row, gdouble theta)
{
    g_return_val_if_fail(gwy_data_field_inside(a, col, row), 0.0);

    return gwy_data_field_get_xder(a, col, row)*cos(theta*G_PI/180)
           + gwy_data_field_get_yder(a, col, row)*sin(theta*G_PI/180);
}

/**
 * gwy_data_field_2dfft:
 * @ra: Real input data field
 * @ia: Imaginary input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @gint (*fft)(): 1D FFT algorithm
 * @windowing: windowing type
 * @direction: FFT direction (1 or -1)
 * @interpolation: interpolation type
 * @preserverms: preserve RMS while windowing
 * @level: level data before computation
 *
 * Computes 2D FFT using a specified 1D alogrithm. This can
 * be for example "gwy_data_line_fft_hum", which is the
 * simplest algoritm avalilable. If requested a windowing
 * and/or leveling is applied to preprocess data to obtain
 * reasonable results.
 **/
void
gwy_data_field_2dfft(GwyDataField *ra, GwyDataField *ia,
                     GwyDataField *rb, GwyDataField *ib,
                     gint (*fft)(),
                     GwyWindowingType windowing, gint direction,
                     GwyInterpolationType interpolation,
                     gboolean preserverms, gboolean level)
{
    GwyDataField rh, ih;

    gwy_data_field_initialize(&rh, ra->xres, ra->yres,
                              ra->xreal, ra->yreal, FALSE);
    gwy_data_field_initialize(&ih, ra->xres, ra->yres,
                              ra->xreal, ra->yreal, FALSE);
    gwy_data_field_xfft(ra, ia, &rh, &ih, fft,
                        windowing, direction, interpolation,
                        preserverms, level);
    gwy_data_field_yfft(&rh, &ih, rb, ib, fft,
                        windowing, direction, interpolation,
                        preserverms, level);

    gwy_data_field_free(&rh);
    gwy_data_field_free(&ih);
}

/**
 * gwy_data_field_2dfft_real:
 * @ra: Real input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @gint (*fft)(): 1D FFT algorithm 
 * @windowing: windowing type
 * @direction: FFT direction (1 or -1)
 * @interpolation: interpolation type
 * @preserverms: preserve RMS while windowing
 * @level: level data before computation
 *
 * Computes 2D FFT using a specified 1D algorithm. As
 * the input is only real, the computation can be a little bit
 * faster.
 **/
void
gwy_data_field_2dfft_real(GwyDataField *ra, GwyDataField *rb,
                          GwyDataField *ib, gint (*fft)(),
                          GwyWindowingType windowing, gint direction,
                          GwyInterpolationType interpolation,
                          gboolean preserverms, gboolean level)
{
    GwyDataField rh, ih;

    gwy_data_field_initialize(&rh, ra->xres, ra->yres,
                              ra->xreal, ra->yreal, FALSE);
    gwy_data_field_initialize(&ih, ra->xres, ra->yres,
                              ra->xreal, ra->yreal, FALSE);
    gwy_data_field_xfft_real(ra, &rh, &ih, fft,
                             windowing, direction, interpolation,
                             preserverms, level);
    gwy_data_field_yfft(&rh, &ih, rb, ib, fft,
                        windowing, direction, interpolation,
                        preserverms, level);

    gwy_data_field_free(&rh);
    gwy_data_field_free(&ih);
}


/**
 * gwy_data_field_get_fft_res:
 * @data_res: data resolution
 *
 * Finds the closest 2^N value.
 *
 * Returns: 2^N good for FFT.
 **/
gint 
gwy_data_field_get_fft_res(gint data_res)
{
    return (gint) pow (2,((gint) floor(log ((gdouble)data_res)/log (2.0)+0.5)));
}


/**
 * gwy_data_field_2dffthumanize:
 * @a: A data field
 *
 * Swap top-left, top-right, bottom-left and bottom-right
 * squares to obtain a humanized 2D FFT output with 0,0 in the center.
 **/
void
gwy_data_field_2dffthumanize(GwyDataField *a)
{
    gint i, j, im, jm, xres;
    GwyDataField b;

    gwy_data_field_initialize(&b, a->xres, a->yres, a->xreal, a->yreal, FALSE);
    gwy_data_field_copy(a, &b);

    im = a->yres/2;
    jm = a->xres/2;
    xres = a->xres;
    for (i = 0; i < im; i++) {
        for (j = 0; j < jm; j++) {
            a->data[(j + jm) + (i + im)*xres] = b.data[j + i*xres];
            a->data[(j + jm) + i*xres] = b.data[j + (i + im)*xres];
            a->data[j + (i + im)*xres] = b.data[(j + jm) + i*xres];
            a->data[j + i*xres] = b.data[(j + jm) + (i + im)*xres];
        }
    }
    gwy_data_field_free(&b);
}

/**
 * gwy_data_field_xfft:
 * @ra: Real input data field
 * @ia: Imaginary input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @gint (*fft)(): 1D FFT algorithm
 * @windowing: windowing type
 * @direction: FFT direction (1 or -1)
 * @interpolation: interpolation type
 * @preserverms: preserve RMS while windowing
 * @level: level data before computation
 *
 * Transform all rows in the data field using 1D algorithm
 * and other parameters specified.
 * 
 **/
void
gwy_data_field_xfft(GwyDataField *ra, GwyDataField *ia,
                    GwyDataField *rb, GwyDataField *ib,
                    gint (*fft)(), GwyWindowingType windowing,
                    gint direction, GwyInterpolationType interpolation,
                    gboolean preserverms, gboolean level)
{
    gint k;
    GwyDataLine rin, iin, rout, iout;

    gwy_data_line_initialize(&rin, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&rout, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&iin, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&iout, ra->xres, ra->yreal, FALSE);

    gwy_data_field_resample(ia, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);

    for (k = 0; k < ra->yres; k++) {
        gwy_data_field_get_row(ra, &rin, k);  
        gwy_data_field_get_row(ia, &iin, k); 
        
        gwy_data_line_fft(&rin, &iin, &rout, &iout, fft,
                          windowing, direction, interpolation,
                          preserverms, level);
        
        gwy_data_field_set_row(rb, &rout, k); 
        gwy_data_field_set_row(ib, &iout, k);
    }

    gwy_data_line_free(&rin);
    gwy_data_line_free(&rout);
    gwy_data_line_free(&iin);
    gwy_data_line_free(&iout);
}

/**
 * gwy_data_field_yfft:
 * @ra: Real input data field
 * @ia: Imaginary input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @gint (*fft)(): 1D FFT algorithm
 * @windowing: windowing type
 * @direction: FFT direction (1 or -1)
 * @interpolation: interpolation type
 * @preserverms: preserve RMS while windowing
 * @level: level data before computation
 *
 * Transform all columns in the data field using 1D algorithm
 * and other parameters specified.
 * 
 **/
void
gwy_data_field_yfft(GwyDataField *ra, GwyDataField *ia,
                    GwyDataField *rb, GwyDataField *ib,
                    gint (*fft)(), GwyWindowingType windowing,
                    gint direction, GwyInterpolationType interpolation,
                    gboolean preserverms, gboolean level)
{
    gint k;
    GwyDataLine rin, iin, rout, iout;

    gwy_data_line_initialize(&rin, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&rout, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&iin, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&iout, ra->xres, ra->yreal, FALSE);
    gwy_data_field_resample(ia, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);

    /*we compute each two FFTs simultaneously*/
    for (k = 0; k < ra->xres; k++) {
        gwy_data_field_get_column(ra, &rin, k);
        gwy_data_field_get_column(ia, &iin, k);
        gwy_data_line_fft(&rin, &iin, &rout, &iout, fft,
                          windowing, direction, interpolation,
                          preserverms, level);
        gwy_data_field_set_column(rb, &rout, k);
        gwy_data_field_set_column(ib, &iout, k);
    }

    gwy_data_line_free(&rin);
    gwy_data_line_free(&rout);
    gwy_data_line_free(&iin);
    gwy_data_line_free(&iout);
}

/**
 * gwy_data_field_xfft_real:
 * @ra: Real input data field
 * @rb: Real output data field
 * @ib: Imaginary output data field
 * @gint (*fft)(): 1D FFT algorithm
 * @windowing: windowing type
 * @direction: FFT direction (1 or -1)
 * @interpolation: interpolation type
 * @preserverms: preserve RMS while windowing
 * @level: level data before computation
 *
 * Transform all rows in the data field using 1D algorithm
 * and other parameters specified. Only real input field
 * is used, so computation can be faster.
 * 
 **/
void
gwy_data_field_xfft_real(GwyDataField *ra, GwyDataField *rb,
                         GwyDataField *ib, gint (*fft)(),
                         GwyWindowingType windowing, gint direction,
                         GwyInterpolationType interpolation,
                         gboolean preserverms, gboolean level)
{
    gint k, j;
    GwyDataLine rin, iin, rout, iout, rft1, ift1, rft2, ift2;

    gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);

    gwy_data_line_initialize(&rin, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&rout, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&iin, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&iout, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&rft1, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&ift1, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&rft2, ra->xres, ra->yreal, FALSE);
    gwy_data_line_initialize(&ift2, ra->xres, ra->yreal, FALSE);

    /*we compute allways two FFTs simultaneously*/
    for (k = 0; k < ra->yres; k++) {
        gwy_data_field_get_row(ra, &rin, k);
        if (k < (ra->yres-1))
            gwy_data_field_get_row(ra, &iin, k+1);
        else {
            gwy_data_field_get_row(ra, &iin, k);
            gwy_data_line_fill(&iin, 0);
        }

        gwy_data_line_fft(&rin, &iin, &rout, &iout, fft,
                          windowing, direction, interpolation,
                          preserverms, level);

        /*extract back the two profiles FFTs*/
        rft1.data[0] = rout.data[0];
        ift1.data[0] = iout.data[0];
        rft2.data[0] = iout.data[0];
        ift2.data[0] = -rout.data[0];
        for (j = 1; j < ra->xres; j++) {
            rft1.data[j] = (rout.data[j] + rout.data[ra->xres - j])/2;
            ift1.data[j] = (iout.data[j] - iout.data[ra->xres - j])/2;
            rft2.data[j] = (iout.data[j] + iout.data[ra->xres - j])/2;
            ift2.data[j] = -(rout.data[j] - rout.data[ra->xres - j])/2;
        }

        gwy_data_field_set_row(rb, &rft1, k);
        gwy_data_field_set_row(ib, &ift1, k);

        if (k < (ra->yres-1)) {
            gwy_data_field_set_row(rb, &rft2, k+1);
            gwy_data_field_set_row(ib, &ift2, k+1);
            k++;
        }
    }

    gwy_data_line_free(&rft1);
    gwy_data_line_free(&rft2);
    gwy_data_line_free(&ift1);
    gwy_data_line_free(&ift2);
    gwy_data_line_free(&rin);
    gwy_data_line_free(&rout);
    gwy_data_line_free(&iin);
    gwy_data_line_free(&iout);
}

gdouble
edist(gint x1, gint y1, gint x2, gint y2)
{
    return sqrt(((gdouble)x1-x2)*((gdouble)x1-x2)+((gdouble)y1-y2)*((gdouble)y1-y2));
}

/**
 * gwy_data_field_mult_wav:
 * @real_field: A data field of real values
 * @imag_field: A data field of imaginary values
 * @scale: wavelet scale
 * @wtype: waveelt type
 *
 * multiply a complex data field (real and imaginary)
 * with complex FT of spoecified wavelet at given scale.
 **/
static void  
gwy_data_field_mult_wav(GwyDataField *real_field, 
                        GwyDataField *imag_field,
                        gdouble scale,
                        Gwy2DCWTWaveletType wtype)
{
    gint xres, yres, xresh, yresh;
    gint i, j;
    
    xres = real_field->xres;
    yres = real_field->yres;
    xresh = xres/2;
    yresh = yres/2;
   
    gdouble mval, val;

    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            val = 1;
            if (i<xresh)
            {
                if (j<yresh) mval = edist(0,0,i,j);
                else mval = edist(0,yres,i,j);
            }
            else
            {
                if (j<yresh) mval = edist(xres,0,i,j);
                else mval = edist(xres, yres, i, j);
            }
            val = wfunc_2d(scale, mval, xres, wtype);
        

            real_field->data[i + j*xres] *= val;
            imag_field->data[i + j*xres] *= val;
        }
    }
}


/**
 * gwy_data_field_cwt:
 * @data_field: A data field
 * @interpolation: interpolation type
 * @scale: wavelet scale
 * @wtype: wavelet type
 *
 * Compute a continuous wavelet transform at given
 * scale and using given wavelet.
 **/
void 
gwy_data_field_cwt(GwyDataField *data_field,
                        GwyInterpolationType interpolation,
                        gdouble scale,
                        Gwy2DCWTWaveletType wtype)
{
   GwyDataField hlp_r;
   GwyDataField hlp_i;
   GwyDataField imag_field;

   gwy_data_field_initialize(&hlp_r, data_field->xres, data_field->yres,
                        data_field->xreal, data_field->yreal, FALSE);
   gwy_data_field_initialize(&hlp_i, data_field->xres, data_field->yres,
                        data_field->xreal, data_field->yreal, FALSE);
   gwy_data_field_initialize(&imag_field, data_field->xres, data_field->yres,
                        data_field->xreal, data_field->yreal, TRUE);

   gwy_data_field_2dfft(data_field,
                        &imag_field,
                        &hlp_r,
                        &hlp_i,
                        gwy_data_line_fft_hum,
                        GWY_WINDOWING_RECT,
                        1,
                        interpolation,
                        1,
                        0);
   gwy_data_field_mult_wav(&hlp_r, &hlp_i, scale, wtype);
   
   gwy_data_field_2dfft(&hlp_r,
                        &hlp_i,
                        data_field,
                        &imag_field,
                        gwy_data_line_fft_hum,
                        GWY_WINDOWING_RECT,
                        -1,
                        interpolation,
                        1,
                        0);
 
                       
   gwy_data_field_free(&hlp_r);
   gwy_data_field_free(&hlp_i);
   gwy_data_field_free(&imag_field); 
}

/**
 * gwy_data_field_shade:
 * @data_field: A data field
 * @target_field: A shaded data field
 * @theta: shading angle
 * @phi: shading angle
 *
 * Creates a shaded data field. Target field should
 * be allready allocated.
 **/
void 
gwy_data_field_shade(GwyDataField *data_field, GwyDataField *target_field,
                                                    gdouble theta, gdouble phi)
{
    gint i, j; 
    gwy_data_field_resample(target_field, data_field->xres, data_field->yres, GWY_INTERPOLATION_NONE);
    for (i = 0; i < data_field->yres; i++) 
    {
        
        for (j = 0; j < data_field->xres; j++) 
        {
            target_field->data[j + data_field->xres*i] = gwy_data_field_get_angder(data_field, j, i, theta);
        }
    }     
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
