/* @(#) $Id$ */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <libgwyddion/gwymacros.h>
#include "datafield.h"

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
 * @a: pointer to field to be initialized
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
 * @a: pointer to field to be freed
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
 * @a: pointer to field to be resampled
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
 * @a: pointer to field to be resized
 * @uli: upper-left row coordinate
 * @ulj: upper-left column coordinate
 * @bri: bottom-right row coordinate
 * @brj: bottom-right column coordinate
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
gwy_data_field_resize(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj)
{
    GwyDataField b;
    gint i, xres, yres;

    if (uli > bri)
        GWY_SWAP(gint, uli, bri);
    if (ulj > brj)
        GWY_SWAP(gint, ulj, brj);

    g_return_val_if_fail(uli >= 0 && ulj >= 0 && bri < a->yres && brj < a->xres,
                         FALSE);

    yres = bri - uli;
    xres = brj - ulj;
    gwy_data_field_alloc(&b, xres, yres);

    for (i = uli; i < bri; i++) {
        memcpy(b.data + (i-uli)*xres,
               a->data + i*a->xres + ulj,
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
 * @a: pointer to field
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
 * gwy_data_field_get_xres:
 * @a: pointer to field
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
 * @a: pointer to field
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
 * @a: pointer to field
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
 * @a: pointer to field
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
 * @a: pointer to field
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
 * @a: pointer to field
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
 * @a: pointer to field
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
 * @a: pointer to field
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
 * @a: pointer to field
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
 * @a: pointer to field
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
    if (i >= 0 && j >= 0 && i < a->yres && j < a->xres)
        return TRUE;
    else
        return FALSE;
}


/**
 * gwy_data_field_get_val:
 * @a: pointer to field
 * @i: row position
 * @j: column position
 *
 * Get value at given pixel
 *
 * Returns: value at (i, j)
 **/
/* XY: yeti */
gdouble
gwy_data_field_get_val(GwyDataField *a, gint i, gint j)
{
    g_return_val_if_fail(gwy_data_field_inside(a, i, j), 0.0);
    return a->data[j + a->xres*i];
}

/**
 * gwy_data_field_set_val:
 * @a: pointer to field
 * @i: row position
 * @j: column position
 * @value: value to set
 *
 * Set @value at given pixel
 **/
/* XY: yeti */
void
gwy_data_field_set_val(GwyDataField *a, gint i, gint j, gdouble value)
{
    g_return_if_fail(gwy_data_field_inside(a, i, j));
    a->data[j + a->xres*i] = value;
}

/**
 * gwy_data_field_get_dval_real:
 * @a: pointer to field
 * @x: row postion in real coordinates
 * @y: column postition in real coordinates
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
 * @a: pointer to field
 * @angle: angle (in degrees)
 * @interpolation: interpolation method
 *
 * Rotates field by a given angle.
 *
 * The values that will be outside of square after rotation will
 * be lost. The new unknown values will be set to field minimum value.
 **/
/* XY: wrong XXX */
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

    for (i = 0; i < a->yres; i++) {
        for (j = 0; j < a->xres; j++) {
            ir = a->yres-i-icor;
            jr = j-jcor;
            /* XXX: cosinus should not have minus!? */
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
                a->data[j + a->xres*i] = gwy_data_field_get_dval(&b, inew, jnew,
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
/* XY: yeti */
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
 * @a: pointer to field
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
 * gwy_data_field_multiply:
 * @a: pointer to field
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
 * gwy_data_field_add:
 * @a: pointer to field
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
 * gwy_data_field_area_fill:
 * @a: pointer to field
 * @uli: upper-left row coordinate
 * @ulj: upper-left column coordinate
 * @bri: bottom-right row coordinate + 1
 * @brj: bottom-right column coordinate + 1
 * @value: value to be entered
 *
 * Fill a specified part of the field witha given  value
 **/
/* XY: yeti */
void
gwy_data_field_area_fill(GwyDataField *a,
                         gint uli, gint ulj, gint bri, gint brj,
                         gdouble value)
{
    gint i, j;

    if (uli > bri)
        GWY_SWAP(gint, uli, bri);
    if (ulj > brj)
        GWY_SWAP(gint, ulj, brj);

    g_return_if_fail(uli >= 0 && ulj >= 0 && bri < a->yres && brj < a->xres);

    for (i = uli; i < bri; i++) {
        gdouble *row = a->data + i*a->xres + ulj;

        for (j = 0; j < brj - ulj; j++)
            *(row++) = value;
    }
}

/**
 * gwy_data_field_area_add:
 * @a: pointer to field
 * @uli: upper-left row coordinate
 * @ulj: upper-left column coordinate
 * @bri: bottom-right row coordinate + 1
 * @brj: bottom-right column coordinate + 1
 * @value: value to be used
 *
 * Add the given value to a specified part of the field
 **/
/* XY: yeti */
void
gwy_data_field_area_add(GwyDataField *a,
                        gint uli, gint ulj, gint bri, gint brj,
                        gdouble value)
{
    gint i, j;

    if (uli > bri)
        GWY_SWAP(gint, uli, bri);
    if (ulj > brj)
        GWY_SWAP(gint, ulj, brj);

    g_return_if_fail(uli >= 0 && ulj >= 0 && bri < a->yres && brj < a->xres);

    for (i = uli; i < bri; i++) {
        gdouble *row = a->data + i*a->xres + ulj;

        for (j = ulj; j < brj; j++)
            *(row++) += value;
    }
}

/**
 * gwy_data_field_area_multiply:
 * @a: pointer to field
 * @uli: upper-left row coordinate
 * @ulj: upper-left column coordinate
 * @bri: bottom-right row coordinate + 1
 * @brj: bottom-right column coordinate + 1
 * @value: value to be used
 *
 * Multiply a specified part of the field by the given value
 **/
/* XY: yeti */
void
gwy_data_field_area_multiply(GwyDataField *a,
                             gint uli, gint ulj, gint bri, gint brj,
                             gdouble value)
{
    gint i, j;

    if (uli > bri)
        GWY_SWAP(gint, uli, bri);
    if (ulj > brj)
        GWY_SWAP(gint, ulj, brj);

    g_return_if_fail(uli >= 0 && ulj >= 0 && bri < a->yres && brj < a->xres);

    for (i = uli; i < bri; i++) {
        gdouble *row = a->data + i*a->xres + ulj;

        for (j = ulj; j < brj; j++)
            *(row++) *= value;
    }
}

/**
 * gwy_data_field_get_max:
 * @a: pointer to field
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

/**
 * gwy_data_field_get_min:
 * @a: pointer to field
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

/**
 * gwy_data_field_get_avg:
 * @a:
 *
 *
 *
 * Returns:
 **/
gdouble
gwy_data_field_get_avg(GwyDataField *a)
{
    return gwy_data_field_get_sum(a)/((gdouble)(a->xres * a->yres));
}

/**
 * gwy_data_field_get_rms:
 * @a: 
 *
 * 
 *
 * Returns:
 **/
/* XY: yeti */
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
    rms = sqrt(fabs(sum2 - sum*sum/n))/n;

    return rms;
}

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


/* XY: yeti */
gdouble
gwy_data_field_get_area_max(GwyDataField *a,
                            gint uli, gint ulj, gint bri, gint brj)
{
    gint i, j;
    gdouble max = -G_MAXDOUBLE;

    if (uli > bri)
        GWY_SWAP(gint, uli, bri);
    if (ulj > brj)
        GWY_SWAP(gint, ulj, brj);

    g_return_val_if_fail(uli >= 0 && ulj >= 0 && bri < a->yres && brj < a->xres,
                         max);

    for (i = uli; i < bri; i++) {
        gdouble *row = a->data + i*a->xres + ulj;

        for (j = ulj; j < brj; j++, row++) {
            if (max < *row)
                max = *row;
        }
    }
    return max;
}

/* XY: yeti */
gdouble
gwy_data_field_get_area_min(GwyDataField *a,
                            gint uli, gint ulj, gint bri, gint brj)
{
    gint i, j;
    gdouble min = G_MAXDOUBLE;

    if (uli > bri)
        GWY_SWAP(gint, uli, bri);
    if (ulj > brj)
        GWY_SWAP(gint, ulj, brj);

    g_return_val_if_fail(uli >= 0 && ulj >= 0 && bri < a->yres && brj < a->xres,
                         min);

    for (i = uli; i < bri; i++) {
        gdouble *row = a->data + i*a->xres + ulj;

        for (j = ulj; j < brj; j++, row++) {
            if (min > *row)
                min = *row;
        }
    }
    return min;
}

gdouble
gwy_data_field_get_area_avg(GwyDataField *a,
                            gint uli, gint ulj, gint bri, gint brj)
{
    return gwy_data_field_get_area_sum(a, uli, ulj, bri, brj)
           /((gdouble)(bri-uli)*(brj-ulj));
}

/* XY: yeti */
gdouble
gwy_data_field_get_area_sum(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj)
{
    gint i, j;
    gdouble sum = 0;

    if (uli > bri)
        GWY_SWAP(gint, uli, bri);
    if (ulj > brj)
        GWY_SWAP(gint, ulj, brj);

    g_return_val_if_fail(uli >= 0 && ulj >= 0 && bri < a->yres && brj < a->xres,
                         sum);

    for (i = uli; i < bri; i++) {
        gdouble *row = a->data + i*a->xres + ulj;

        for (j = ulj; j < brj; j++)
            sum += *(row++);
    }
    return sum;
}

/* XY: yeti */
gdouble
gwy_data_field_get_area_rms(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj)
{
    gint i, j, n;
    gdouble rms = 0, sum2 = 0;
    gdouble sum = gwy_data_field_get_area_sum(a, uli, ulj, bri, brj);

    if (uli > bri)
        GWY_SWAP(gint, uli, bri);
    if (ulj > brj)
        GWY_SWAP(gint, ulj, brj);

    g_return_val_if_fail(uli >= 0 && ulj >= 0 && bri < a->yres && brj < a->xres,
                         rms);

    for (i = uli; i < bri; i++) {
        gdouble *row = a->data + i*a->xres + ulj;

        for (j = ulj; j < brj; j++, row++)
            sum2 += (*row)*(*row);
    }

    n = (bri-uli)*(brj-ulj);
    rms = sqrt(fabs(sum2 - sum*sum/n))/n;

    return rms;
}

/* XY: yeti */
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

/* XY: yeti */
gint
gwy_data_field_area_threshold(GwyDataField *a,
                              gint uli, gint ulj, gint bri, gint brj,
                              gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, j, tot = 0;

    if (uli > bri)
        GWY_SWAP(gint, uli, bri);
    if (ulj > brj)
        GWY_SWAP(gint, ulj, brj);

    g_return_val_if_fail(uli >= 0 && ulj >= 0 && bri < a->yres && brj < a->xres,
                         -1);

    for (i = uli; i < bri; i++) {
        gdouble *row = a->data + i*a->xres + ulj;

        for (j = ulj; j < brj; j++, row++) {
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


/* XY: yeti */
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

/* XY: yeti */
gint
gwy_data_field_area_clamp(GwyDataField *a,
                          gint uli, gint ulj, gint bri, gint brj,
                          gdouble bottom, gdouble top)
{
    gint i, j, tot = 0;

    if (uli > bri)
        GWY_SWAP(gint, uli, bri);
    if (ulj > brj)
        GWY_SWAP(gint, ulj, brj);

    g_return_val_if_fail(uli >= 0 && ulj >= 0 && bri < a->yres && brj < a->xres,
                         -1);

    for (i = uli; i < bri; i++) {
        gdouble *row = a->data + i*a->xres + ulj;

        for (j = ulj; j < brj; j++, row++) {
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

/* XY: yeti */
void
gwy_data_field_get_row(GwyDataField *a, GwyDataLine* b, gint i)
{
    g_return_if_fail(i >= 0 && i < a->yres);

    gwy_data_line_resample(b, a->xres, GWY_INTERPOLATION_NONE);
    memcpy(b->data, a->data + i*a->xres, a->xres*sizeof(gdouble));
}

/* XY: yeti */
void
gwy_data_field_get_column(GwyDataField *a, GwyDataLine* b, gint j)
{
    gint k;
    gdouble *p;

    g_return_if_fail(j >= 0 && j < a->xres);

    gwy_data_line_resample(b, a->yres, GWY_INTERPOLATION_NONE);
    p = a->data + j;
    for (k = 0; k < a->yres; k++)
        b->data[k] = p[k*a->xres];
}

/* XY: yeti */
void
gwy_data_field_set_row(GwyDataField *a, GwyDataLine* b, gint i)
{
    g_return_if_fail(i >= 0 && i < a->yres);
    g_return_if_fail(a->xres == b->res);

    memcpy(a->data + i*a->xres, b->data, a->xres*sizeof(gdouble));
}

/* XY: yeti */
void
gwy_data_field_set_column(GwyDataField *a, GwyDataLine* b, gint j)
{
    gint k;
    gdouble *p;

    g_return_if_fail(j >= 0 && j < a->xres);
    g_return_if_fail(a->yres == b->res);

    p = a->data + j;
    for (k = 0; k < a->yres; k++)
        p[k*a->xres] = b->data[k];
}

/* XY: yeti */
gboolean
gwy_data_field_get_data_line(GwyDataField *a, GwyDataLine* b,
                             gint uli, gint ulj, gint bri, gint brj,
                             gint res, GwyInterpolationType interpolation)
{
    gint k;
    gdouble cosa, sina;

    if (uli > bri)
        GWY_SWAP(gint, uli, bri);
    if (ulj > brj)
        GWY_SWAP(gint, ulj, brj);

    g_return_val_if_fail(uli >= 0 && ulj >= 0 && bri < a->yres && brj < a->xres,
                         FALSE);

    cosa = (gdouble)(brj - ulj)/(res - 1);
    sina = (gdouble)(bri - uli)/(res - 1);

    gwy_data_line_resample(b, res, GWY_INTERPOLATION_NONE);
    for (k = 0; k < res; k++)
        b->data[k] = gwy_data_field_get_dval(a, ulj + k*cosa, uli + k*sina,
                                             interpolation);

    return TRUE;
}

/* XY: yeti */
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


/* XY: yeti */
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

/* XY: yeti */
gdouble
gwy_data_field_get_xder(GwyDataField *a, gint i, gint j)
{
    gdouble *p;

    g_return_val_if_fail(gwy_data_field_inside(a, i, j), 0.0);

    p = a->data + i*a->xres + j;
    if (j == 0)
        return (*(p+1) - *p) * a->xres/a->xreal;
    if (j == a->xres-1)
        return (*p - *(p-1)) * a->xres/a->xreal;
    return (*(p+1) - *(p-1)) * a->xres/a->xreal/2;
}

/* XY: yeti */
gdouble
gwy_data_field_get_yder(GwyDataField *a, gint i, gint j)
{
    gdouble *p;
    gint xres;

    g_return_val_if_fail(gwy_data_field_inside(a, i, j), 0.0);

    xres = a->xres;
    p = a->data + i*xres + j;
    if (i == 0)
        return (*(p+xres) - *p) * a->yres/a->yreal;
    if (i == a->yres-1)
        return (*p - *(p-xres)) * a->yres/a->yreal;
    return (*(p+xres) - *(p-xres)) * a->yres/a->yreal/2;
}

gdouble
gwy_data_field_get_angder(GwyDataField *a, gint i, gint j, gdouble theta)
{
    g_return_val_if_fail(gwy_data_field_inside(a, i, j), 0.0);

    return gwy_data_field_get_xder(a, i, j)*cos(theta*G_PI/180)
           + gwy_data_field_get_yder(a, i, j)*sin(theta*G_PI/180);
}

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


/* XY: yeti (I hope) */
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




/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
