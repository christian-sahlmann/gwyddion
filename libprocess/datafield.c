/* @(#) $Id$ */

#include <stdio.h>
#include <math.h>
#include "datafield.h"

#define swap(t, x, y) \
    do { \
    t safe ## x ## y; \
    safe ## x ## y = x; \
    x = y; \
    y = safe ## x ## y; \
    } while (0)

#define GWY_DATA_FIELD_TYPE_NAME "GwyDataField"

static void  gwy_data_field_class_init        (GwyDataFieldClass *klass);
static void  gwy_data_field_init              (GwyDataField *data_field);
static void  gwy_data_field_finalize              (GwyDataField *data_field);
static void  gwy_data_field_serializable_init (gpointer giface, gpointer iface_data);
static void  gwy_data_field_watchable_init    (gpointer giface, gpointer iface_data);
static guchar* gwy_data_field_serialize       (GObject *obj, guchar *buffer, gsize *size);
static GObject* gwy_data_field_deserialize    (const guchar *buffer, gsize size, gsize *position);
static void  gwy_data_field_value_changed     (GObject *GwyDataField);


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
            gwy_data_field_serializable_init,
            NULL,
            NULL
        };
        GInterfaceInfo gwy_watchable_info = {
            gwy_data_field_watchable_init,
            NULL,
            NULL
        };

        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
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
gwy_data_field_serializable_init(gpointer giface,
                                 gpointer iface_data)
{
    GwySerializableClass *iface = giface;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_SERIALIZABLE);

    /* initialize stuff */
    iface->serialize = gwy_data_field_serialize;
    iface->deserialize = gwy_data_field_deserialize;
}

static void
gwy_data_field_watchable_init(gpointer giface,
                              gpointer iface_data)
{
    GwyWatchableClass *iface = giface;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_WATCHABLE);

    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_data_field_class_init(GwyDataFieldClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    gobject_class->finalize = (GObjectFinalizeFunc)gwy_data_field_finalize;
}

static void
gwy_data_field_init(GwyDataField *data_field)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    data_field->data = NULL;
    data_field->xres = 0;
    data_field->yres = 0;
    data_field->xreal = 0.0;
    data_field->yreal = 0.0;
}

static void
gwy_data_field_finalize(GwyDataField *data_field)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    gwy_data_field_free(data_field);
}

GObject*
gwy_data_field_new(gint xres, gint yres, gdouble xreal, gdouble yreal, gboolean nullme)
{
    GwyDataField *data_field;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_DATA_FIELD_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        return NULL;
    }
    if (fsize != xres*yres) {
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
              "Serialized %s size mismatch %u != %u",
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



static void
gwy_data_field_value_changed(GObject *data_field)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "signal: GwyGwyDataLine changed");
    #endif
    g_signal_emit_by_name(GWY_DATA_FIELD(data_field), "value_changed", NULL);
}


/**
 * gwy_data_field_alloc:
 * @a: pointer fo field to be allocated
 * @xres: X resolution
 * @yres: Y resolution
 *
 * Allocates GwyDataField
 *
 * Returns:0 at succes
 **/
gint
gwy_data_field_alloc(GwyDataField *a, gint xres, gint yres)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    a->xres = xres;
    a->yres = yres;
    if ((a->data = (gdouble *) g_try_malloc(a->xres*a->yres*sizeof(gdouble))) == NULL) return -1;
    return 0;
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
 *
 * Returns: 0 at success
 **/
gint
gwy_data_field_initialize(GwyDataField *a, gint xres, gint yres, gdouble xreal, gdouble yreal, gboolean nullme)
{
    int i;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s (%dx%d)", __FUNCTION__, xres, yres);
    #endif
    if (gwy_data_field_alloc(a, xres, yres) != 0)
        return -1;
    a->xreal = xreal;
    a->yreal = yreal;
    if (nullme) {
        for (i = 0; i < (a->xres*a->yres); i++)
            a->data[i] = 0;
    }
    return 0;
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
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_free(a->data);
}

/**
 * gwy_data_field_copy:
 * @a: source
 * @b: destination
 *
 * Makes a deep copy of the GwyDataField
 *
 * Returns:
 **/
gint
gwy_data_field_copy(GwyDataField *a, GwyDataField *b)
{
    int i;
    if ((a->xres != b->xres) || (a->yres != b->yres)) {
        g_warning("Cannot copy fields of different size.\n");
        return -1;
    }
    b->xreal = a->xreal;
    b->yreal = a->yreal;
    for (i = 0; i < (a->xres*a->yres); i++)
        b->data[i] = a->data[i];
    return 0;
}


/**
 * gwy_data_field_resample:
 * @a: pointer to field to be resampled
 * @xres: desired X resolution
 * @yres: desired Y resolution
 * @interpolation: interpolation method
 *
 * Resamples GwyDataField using given interpolation method
 *
 * Returns: 0 at success.
 **/
gint
gwy_data_field_resample(GwyDataField *a, gint xres, gint yres, gint interpolation)
{
    GwyDataField b;
    gdouble xratio, yratio, xpos, ypos;
    gint i,j;

    if (a->xres == xres && a->yres == yres)
        return 0;
    if (gwy_data_field_alloc(&b, a->xres, a->yres) != 0)
        return -1;
    gwy_data_field_copy(a, &b);

    a->xres = xres;
    a->yres = yres;
    a->data = g_try_realloc(a->data, a->xres*a->yres*sizeof(gdouble));
    if (!a->data)
        return -1;

    if (interpolation!=GWY_INTERPOLATION_NONE) {
        xratio = (gdouble)(b.xres-1)/(gdouble)(a->xres-1);
        yratio = (gdouble)(b.yres-1)/(gdouble)(a->yres-1);

        for (i = 0; i < a->yres; i++) {
            for (j = 0; j < a->xres; j++) {
                xpos = (gdouble)i*yratio;
                if (xpos>(b.yres-1))
                    xpos=(b.yres-1);
                ypos = (gdouble)j*xratio;
                if (ypos>(b.xres-1))
                    ypos=(b.xres-1);
                /*printf("(%d, %d), -> %f, %f\n",i, j, xpos, ypos);*/
                a->data[i + a->yres*j] = gwy_data_field_get_dval(&b, xpos, ypos,
                                                                 interpolation);
            }
        }
    }

    gwy_data_field_free(&b);
    return 0;
}

gint
gwy_data_field_confirmsize(GwyDataField *a, gint xres, gint yres)
{
    int ret=0;
    if (a->data == NULL)
        ret = gwy_data_field_initialize(a, xres, yres, xres, yres, 0);
    else if (a->xres != xres)
        ret = gwy_data_field_resample(a, xres, yres, GWY_INTERPOLATION_NONE);
    return ret;
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
 * Returns:0 at success
 **/
gint
gwy_data_field_resize(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj)
{
    GwyDataField b;
    gint i, j, xres, yres;

    if (uli < 0 || ulj < 0 || bri < 0 || brj < 0) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > a->yres || ulj > a->xres || bri > a->yres || brj > a->xres) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }

    if (uli>bri)
        swap(gint, uli, bri);
    if (ulj>brj)
        swap(gint, ulj, brj);
    yres = bri-uli;
    xres = brj-ulj;

    if (gwy_data_field_alloc(&b, a->xres, a->yres) != 0)
        return -1;
    gwy_data_field_copy(a, &b);

    a->xres = xres;
    a->yres = yres;
    a->data = (gdouble*)g_try_realloc(a->data, a->xres*a->yres*sizeof(gdouble));
    if (!a->data)
        return -1;

    for (i = uli; i < bri; i++) {
        for (j = ulj; j < brj; j++)
            a->data[i-uli + (j-ulj)*a->yres] = b.data[i + j*b.yres];
    }

    gwy_data_field_free(&b);

    return 0;
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
 * Returns:value at the position (x,y).
 **/
gdouble
gwy_data_field_get_dval(GwyDataField *a, gdouble x, gdouble y, gint interpolation)
{
    gint ix, iy;
    gint floorx, floory;
    gdouble restx, resty, valpx, valxp, valpp;

    if (x < 0 || y < 0 || x > (a->yres-1) || y > (a->xres-1)) {
        g_warning("Trying to reach value outside data_field.\n");
        /*printf("(%f, %f) <> (%d, %d))\n", x, y, a->yres-1, a->xres-1);*/
        return -1;
    }

    if (interpolation == GWY_INTERPOLATION_NONE) return 0;
    else if (interpolation == GWY_INTERPOLATION_ROUND)
    {
        ix = (gint)(x + 0.5);
        iy = (gint)(y + 0.5);
        return a->data[ix + a->yres*iy];
    }
    else if (interpolation == GWY_INTERPOLATION_BILINEAR)
    {
        floorx = (gint)floor(x);
        floory = (gint)floor(y);
        restx = x - (gdouble)floorx;
        resty = y - (gdouble)floory;

        if (restx != 0)
            valpx = restx*(1 - resty)*a->data[floorx + 1 + a->yres*floory];
        else
            valpx = 0;
        if (resty != 0)
            valxp = resty*(1 - restx)*a->data[floorx + a->yres*(floory + 1)];
        else
            valxp = 0;
        if (restx != 0 && resty != 0)
            valpp = restx*resty*a->data[floorx + 1 + a->yres*(floory + 1)];
        else
            valpp = 0;

        return valpx + valxp + valpp
               + (1 - restx)*(1 - resty)*a->data[floorx + a->yres*floory];
    }

    g_warning("Not supported interpolation type.\n");
    return 0;
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
    a->xreal=xreal;
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
    a->yreal=yreal;
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
 * gwy_data_field_rtoi:
 * @a: pointer to field
 * @realval:  value at real coordinates
 *
 * recomputes row real coordinate to pixel coordinate
 *
 * Returns: recomputed value
 **/
gdouble
gwy_data_field_rtoj(GwyDataField *a, gdouble realval)
{
    return realval*a->xres/a->xreal;
}

gboolean
gwy_data_field_outside(GwyDataField *a, gint i, gint j)
{
    if (i<0 || j<0 || i>=a->yres || j>=a->xres) return 1;
    else return 0;
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
gdouble
gwy_data_field_get_val(GwyDataField *a, gint i, gint j)
{
    if (gwy_data_field_outside(a, i, j)) {g_warning("Trying to reach value outside of data_field.\n"); return 1;}
    return a->data[i + a->yres*j];
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
void
gwy_data_field_set_val(GwyDataField *a, gint i, gint j, gdouble value)
{
    if (gwy_data_field_outside(a, i, j)) {
        g_warning("Trying to reach value outside of data_field.\n");
        return;
    }
    a->data[i + a->yres*j] = value;
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
gwy_data_field_get_dval_real(GwyDataField *a, gdouble x, gdouble y, gint interpolation)
{
    return  gwy_data_field_get_dval(a, gwy_data_field_rtoi(a, x), gwy_data_field_rtoj(a, y), interpolation);
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
 *
 * Returns: 0 at success
 **/
gint
gwy_data_field_rotate(GwyDataField *a, gdouble angle, gint interpolation)
{
    GwyDataField b;
    gdouble inew, jnew, ir, jr, ang, icor, jcor, sn, cs, val;
    gint i,j;

    if (((gint)angle%360) == 0) return 0;
    if (gwy_data_field_alloc(&b, a->xres, a->yres) != 0)
        return -1;
    gwy_data_field_copy(a, &b);

    val = gwy_data_field_get_min(a);
    ang = 3*G_PI/4 + angle*G_PI/180;
    sn = sin(angle*G_PI/180);
    cs = cos(angle*G_PI/180);
    icor = (gdouble)a->yres/2 + G_SQRT2*(gdouble)a->yres/2*sin(ang) - sn*(a->xres-a->yres)/2;
    jcor = (gdouble)a->xres/2 + G_SQRT2*(gdouble)a->xres/2*cos(ang) + sn*(a->xres-a->yres)/2;;
    if (angle == 90) {
        sn = 1.0;
        cs = 0.0;
        icor = 1.0;
        jcor = 0.0;
    }
    if (angle == 180) {
        sn = 0.0;
        cs = -1.0;
        icor = 1.0;
        jcor = a->xres-1;
    }
    if (angle == 270) {sn = -1.0;
        cs = 0.0;
        icor = a->yres;
        jcor = a->xres-1;
    }

    for (i = 0; i < a->yres; i++) {
        for (j = 0; j < a->xres; j++) {
            ir = a->yres-i-icor; jr = j-jcor;
            inew = -ir*cs + jr*sn;
            jnew = ir*sn + jr*cs;
            if (inew > a->yres || jnew > a->xres || inew < -1 || jnew < -1) {
                a->data[i + a->yres*j] = val;
            }
            else {
                if (inew > (a->yres - 1)) inew = a->yres-1;
                if (jnew > (a->xres - 1)) jnew = a->xres-1;
                if (inew < 0) inew = 0;
                if (jnew < 0) jnew = 0;
                a->data[i + a->yres*j] = gwy_data_field_get_dval(&b, inew, jnew,
                                                                 interpolation);
            }
        }
    }

    gwy_data_field_free(&b);
    return 0;
}


/**
 * gwy_data_field_invert:
 * @a: pointer fo field
 * @x: invert in X direction?
 * @y: invert in Y direction?
 * @z: invert in Z direction?
 *
 * Make requested inversion(s).
 *
 * Returns:
 **/
gint
gwy_data_field_invert(GwyDataField *a, gboolean x, gboolean y, gboolean z)
{
    gint i,j;
    gdouble avg;
    GwyDataField b;

    if (x == 0 && y ==0 && z==0) return 0;

    if (gwy_data_field_alloc(&b, a->xres, a->yres)!=0) return -1;
    if (y)
    {
        gwy_data_field_copy(a, &b);
        for (i=0; i<a->yres; i++)
        {
            for (j=0; j<a->xres; j++)
                a->data[i + a->yres*j] = b.data[i + a->yres*(a->xres-j-1)];
        }
    }
    if (x)
    {
        gwy_data_field_copy(a, &b);
        for (i=0; i<a->yres; i++)
        {
            for (j=0; j<a->xres; j++)
                a->data[i + a->yres*j] = b.data[a->yres - i - 1 + a->yres*j];
        }
    }
    if (z)
    {
        avg=gwy_data_field_get_avg(a);
        for (i=0; i<(a->yres*a->xres); i++)
                a->data[i] = 2*avg - a->data[i];
    }
    gwy_data_field_free(&b);
    return 0;
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
    for (i = 0; i < (a->xres * a->yres); i++)
        a->data[i] = value;
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
    for (i = 0; i < (a->xres * a->yres); i++)
        a->data[i] *= value;
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
    for (i = 0; i < (a->xres * a->yres); i++)
        a->data[i] += value;
}

/**
 * gwy_data_field_area_fill:
 * @a: pointer to field
 * @uli: upper-left row coordinate
 * @ulj: upper-left column coordinate
 * @bri: bottom-right row coordinate
 * @brj: bottom-right column coordinate
 * @value: value to be entered
 *
 * Fill a specified part of the field witha given  value
 *
 * Returns: 0 at success
 **/
gint
gwy_data_field_area_fill(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj, gdouble value)
{
    gint i, j;

    if (uli < 0 || ulj < 0 || bri < 0 || brj < 0) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > a->yres || ulj > a->xres || bri > a->yres || brj > a->xres) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }

    if (uli > bri)
        swap(gint, uli, bri);
    if (ulj > brj)
        swap(gint, ulj, brj);

    for (i = uli; i < bri; i++) {
        for (j = ulj; j < brj; j++)
            a->data[i + j*a->yres] = value;
    }
    return 0;

}

/**
 * gwy_data_field_area_add:
 * @a: pointer to field
 * @uli: upper-left row coordinate
 * @ulj: upper-left column coordinate
 * @bri: bottom-right row coordinate
 * @brj: bottom-right column coordinate
 * @value: value to be used
 *
 * Add the given value to a specified part of the field
 *
 * Returns: 0 at success
 **/
gint
gwy_data_field_area_add(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj, gdouble value)
{
    gint i, j;

    if (uli < 0 || ulj < 0 || bri < 0 || brj < 0) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > a->yres || ulj > a->xres || bri > a->yres || brj > a->xres) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }

    if (uli>bri) swap(gint, uli, bri);
    if (ulj>brj) swap(gint, ulj, brj);

    for (i = uli; i < bri; i++) {
        for (j = ulj; j < brj; j++)
            a->data[i + j*a->yres] += value;
    }
    return 0;

}

/**
 * gwy_data_field_area_multiply:
 * @a: pointer to field
 * @uli: upper-left row coordinate
 * @ulj: upper-left column coordinate
 * @bri: bottom-right row coordinate
 * @brj: bottom-right column coordinate
 * @value: value to be used
 *
 * Multiply a specified part of the field by the given value
 *
 * Returns:0 at success
 **/
gint
gwy_data_field_area_multiply(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj, gdouble value)
{
    gint i, j;

    if (uli < 0 || ulj < 0 || bri < 0 || brj < 0) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > a->yres || ulj > a->xres || bri > a->yres || brj > a->xres) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }

    if (uli > bri)
        swap(gint, uli, bri);
    if (ulj > brj)
        swap(gint, ulj, brj);

    for (i = uli; i < bri; i++) {
        for (j = ulj; j < brj; j++)
            a->data[i + j*a->yres] *= value;
    }
    return 0;

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
    gdouble max=a->data[0];

    for (i = 1; i < (a->xres * a->yres); i++) {
        if (max < a->data[i])
            max = a->data[i];
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
    gdouble min=a->data[0];

    for (i = 1; i < (a->xres * a->yres); i++) {
        if (min > a->data[i])
            min = a->data[i];
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
    gint i;
    gdouble avg=0;

    for (i = 1; i < (a->xres * a->yres); i++) {
        avg += a->data[i];
    }
    return avg/((gdouble)(a->xres * a->yres));
}

gdouble
gwy_data_field_get_rms(GwyDataField *a)
{
    gint i;
    gdouble rms=0;
    gdouble avg=gwy_data_field_get_avg(a);

    for (i = 1; i < (a->xres * a->yres); i++) {
        rms += (a->data[i]-avg)*(a->data[i]-avg);
    }
    return sqrt(rms)/((gdouble)(a->xres * a->yres));
}

gdouble
gwy_data_field_get_sum(GwyDataField *a)
{
    gint i;
    gdouble sum=0;

    for (i = 1; i < (a->xres * a->yres); i++) {
        sum += a->data[i];
    }
    return sum;
}


gdouble
gwy_data_field_get_area_max(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj)
{
    gint i, j;
    gdouble max=G_MINDOUBLE;

    if (uli < 0 || ulj < 0 || bri < 0 || brj < 0) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > a->yres || ulj > a->xres || bri > a->yres || brj > a->xres) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }

    if (uli > bri)
        swap(gint, uli, bri);
    if (ulj > brj)
        swap(gint, ulj, brj);

    for (i = uli; i < bri; i++) {
        for (j = ulj; j < brj; j++) {
            if (max < a->data[i + j*a->yres])
                max = a->data[i + j*a->yres];
        }
    }
    return max;
}

gdouble
gwy_data_field_get_area_min(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj)
{
    gint i, j;
    gdouble min=G_MAXDOUBLE;


    if (uli < 0 || ulj < 0 || bri < 0 || brj < 0) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > a->yres || ulj > a->xres || bri > a->yres || brj > a->xres) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > bri)
        swap(gint, uli, bri);
    if (ulj > brj)
        swap(gint, ulj, brj);


    for (i = uli; i < bri; i++) {
        for (j = ulj; j < brj; j++) {
            if (min > a->data[i + j*a->yres])
                min = a->data[i + j*a->yres];
        }
    }
    return min;
}

gdouble
gwy_data_field_get_area_avg(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj)
{
    gint i, j;
    gdouble avg=0;

    if (uli < 0 || ulj < 0 || bri < 0 || brj < 0) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > a->yres || ulj > a->xres || bri > a->yres || brj > a->xres) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > bri)
        swap(gint, uli, bri);
    if (ulj > brj)
        swap(gint, ulj, brj);

    for (i = uli; i < bri; i++) {
        for (j = ulj; j < brj; j++) {
            avg += a->data[i + j*a->yres];
        }
    }
    return avg/((gdouble)(bri-uli)*(brj-ulj));
}

gdouble
gwy_data_field_get_area_sum(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj)
{
    gint i, j;
    gdouble sum = 0;


    if (uli < 0 || ulj < 0 || bri < 0 || brj < 0) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > a->yres || ulj > a->xres || bri > a->yres || brj > a->xres) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }

    if (uli > bri)
        swap(gint, uli, bri);
    if (ulj > brj)
        swap(gint, ulj, brj);

    for (i = uli; i < bri; i++) {
        for (j = ulj; j < brj; j++) {
            sum += a->data[i + j*a->yres];
        }
    }
    return sum;
}

gdouble
gwy_data_field_get_area_rms(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj)
{
    gint i, j;
    gdouble rms = 0;
    gdouble avg = gwy_data_field_get_area_avg(a, uli, ulj, bri, brj);

    if (uli < 0 || ulj < 0 || bri < 0 || brj < 0) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > a->yres || ulj > a->xres || bri > a->yres || brj > a->xres) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }

    if (uli > bri)
        swap(gint, uli, bri);
    if (ulj > brj)
        swap(gint, ulj, brj);

    for (i = uli; i < bri; i++) {
        for (j = ulj; j < brj; j++) {
            rms += (avg - a->data[i + j*a->yres])*(avg - a->data[i + j*a->yres]);
        }
    }
    return sqrt(rms)/((gdouble)(bri-uli)*(brj-ulj));
}

gint
gwy_data_field_threshold(GwyDataField *a, gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, tot = 0;
    for (i = 0; i < (a->xres * a->yres); i++)
    {
        if (a->data[i] < threshval)
            a->data[i] = bottom;
        else {
            a->data[i] = top;
            tot++;
        }
    }
    return tot;
}

gint
gwy_data_field_area_threshold(GwyDataField *a, gint uli, gint ulj, gint bri, gint brj, gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, j, tot = 0;

    if (uli < 0 || ulj < 0 || bri < 0 || brj < 0) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > a->yres || ulj > a->xres || bri > a->yres || brj > a->xres) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }

    if (uli > bri)
        swap(gint, uli, bri);
    if (ulj > brj)
        swap(gint, ulj, brj);

    for (i = uli; i < bri; i++)
    {
        for (j = ulj; j < brj; j++)
        {
            if (a->data[i + j*a->yres] < threshval)
                a->data[i + j*a->yres] = bottom;
            else {
                a->data[i + j*a->yres] = top;
                tot++;
            }
        }
    }
    return tot;
}


gint
gwy_data_field_get_row(GwyDataField *a, GwyDataLine* b, gint i)
{
    gint k;

    if (i < 0 || i >= a->yres) {
        g_warning("Trying to reach row outside of data_field\n");
        return 1;
    }

    if (gwy_data_line_resample(b, a->xres, GWY_INTERPOLATION_NONE)!=0)
        return 1;
    for (k = 0; k < a->xres; k++)
        b->data[k] = a->data[i + k*a->yres];

    return 0;
}

gint
gwy_data_field_get_column(GwyDataField *a, GwyDataLine* b, gint i)
{
    gint k;

    if (i < 0 || i >= a->xres) {
        g_warning("Trying to reach column outside of data_field\n");
        return 1;
    }

    if (gwy_data_line_resample(b, a->yres, GWY_INTERPOLATION_NONE) != 0)
        return 0;
    for (k = 0; k < a->yres; k++)
        b->data[k] = a->data[k + i*a->yres];

    return 0;
}

gint
gwy_data_field_set_row(GwyDataField *a, GwyDataLine* b, gint i)
{
    gint k;
    if (i < 0 || i >= a->yres) {
        g_warning("Trying to reach row outside of data_field\n");
        return 1;
    }

    for (k = 0; k < a->xres; k++)
        a->data[i + k*a->yres] = b->data[k];
    return 0;
}

gint
gwy_data_field_set_column(GwyDataField *a, GwyDataLine* b, gint i)
{
    gint k;
    if (i < 0 || i >= a->xres) {
        g_warning("Trying to reach row outside of data_field\n");
        return 1;
    }

    for (k = 0; k < a->yres; k++)
        a->data[k + i*a->yres] = b->data[k];
    return 0;
}

gint
gwy_data_field_get_data_line(GwyDataField *a, GwyDataLine* b, gint uli, gint ulj, gint bri, gint brj, gint res, gint interpolation)
{
    gint k;
    gdouble length, alpha, cosa, sina;

    if (uli < 0 || ulj < 0 || bri < 0 || brj < 0) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }
    if (uli > a->yres || ulj > a->xres || bri > a->yres || brj > a->xres) {
        g_warning("Coordinates outside data_field.\n");
        return -1;
    }

    if (uli > bri)
        swap(gint, uli, bri);
    if (ulj > brj)
        swap(gint, ulj, brj);

    length = sqrt((bri - uli)*(bri - uli) + (brj - ulj)*(brj - ulj));
    alpha = atan((brj - ulj)/(bri - uli));
    cosa = cos(alpha)*length/res;
    sina = sin(alpha)*length/res;

    if (gwy_data_line_resample(b, res, GWY_INTERPOLATION_NONE) !=0 )
        return 1;
    for (k = 0; k < res; k++)
        b->data[k] = gwy_data_field_get_dval(a, k*cosa, k*sina, interpolation);

    return 0;
}

gint
gwy_data_field_plane_coefs(GwyDataField *a, gdouble *ap, gdouble *bp, gdouble *cp)
{
    gint k;
    GwyDataLine l;
    gdouble val, buff;

    bp = cp = 0;

    gwy_data_line_alloc(&l, a->xres);
    for (k = 0; k<a->yres; k++) {
        gwy_data_field_get_row(a, &l, k);
        gwy_data_line_line_coefs(&l, &buff, &val);
        *bp += val;
    }
    for (k = 0; k<a->xres; k++) {
        gwy_data_field_get_column(a, &l, k);
        gwy_data_line_line_coefs(&l, &buff, &val);
        *cp += val;
    }
    *cp /= a->xres;
    *bp /= a->yres;

    *ap = gwy_data_field_get_avg(a);

    gwy_data_line_free(&l);
    return 0;
}


gint
gwy_data_field_plane_level(GwyDataField *a, gdouble ap, gdouble bp, gdouble cp)
{
    gint i, j;

    for (i = 0; i < a->yres; i++) {
        for (j = 0; j < a->xres; j++) {
            a->data[i + j*a->yres] -= ap + bp*i + cp*j;
        }
    }
    return 0;
}

gint
gwy_data_field_plane_rotate(GwyDataField *a, gdouble xangle, gdouble yangle, gint interpolation)
{
    int k;
    GwyDataLine l;
    gwy_data_line_alloc(&l, a->xres);

    if (xangle != 0) {
        for (k=0; k<a->yres; k++) {
            gwy_data_field_get_row(a, &l, k);
            gwy_data_line_line_rotate(&l, xangle, interpolation);
            gwy_data_field_set_row(a, &l, k);
        }
    }
    if (yangle!=0) {
        for (k=0; k<a->xres; k++) {
            gwy_data_field_get_column(a, &l, k);
            gwy_data_line_line_rotate(&l, yangle, interpolation);
            gwy_data_field_set_column(a, &l, k);
        }
    }
    gwy_data_line_free(&l);

    return 0;
}

gdouble
gwy_data_field_get_xder(GwyDataField *a, gint i, gint j)
{
   if (gwy_data_field_outside(a, i, j)) {g_warning("Trying to reach value outside of data_field.\n"); return 1;}

   if (j==0) return (a->data[i + a->yres] - a->data[i])*a->yres/a->yreal;
   else if (j==(a->xres-1))
       return (a->data[i + a->yres*j] - a->data[i + a->yres*(j-1)])*a->yres/a->yreal;
   else
       return (a->data[i + a->yres*(j+1)] - a->data[i + a->yres*(j-1)])*a->yres/a->yreal/2;
}

gdouble
gwy_data_field_get_yder(GwyDataField *a, gint i, gint j)
{
   if (gwy_data_field_outside(a, i, j)) {g_warning("Trying to reach value outside of data_field.\n"); return 1;}

   if (i==0) return (a->data[1 + a->yres*j] - a->data[a->yres*j])*a->xres/a->xreal;
   else if (i==(a->yres-1))
       return (a->data[i + a->yres*j] - a->data[i - 1 + a->yres*j])*a->xres/a->xreal;
   else
       return (a->data[i + 1 + a->yres*j] - a->data[i - 1 + a->yres*j])*a->xres/a->xreal/2;
}

gdouble
gwy_data_field_get_angder(GwyDataField *a, gint i, gint j, gdouble theta)
{
    if (gwy_data_field_outside(a, i, j)) {g_warning("Trying to reach value outside of data_field.\n"); return 1;}

    return gwy_data_field_get_xder(a, i, j)*cos(theta*G_PI/180) + gwy_data_field_get_yder(a, i, j)*sin(theta*G_PI/180);
}

gint
gwy_data_field_2dfft(GwyDataField *ra, GwyDataField *ia, GwyDataField *rb, GwyDataField *ib, gint (*fft)(),
                 gint windowing, gint direction, gint interpolation, gboolean preserverms, gboolean level)
{
    GwyDataField rh, ih;
    if (gwy_data_field_initialize(&rh, ra->xres, ra->yres, ra->xreal, ra->yreal, 0)) return 1;
    if (gwy_data_field_initialize(&ih, ra->xres, ra->yres, ra->xreal, ra->yreal, 0)) return 1;

    if (gwy_data_field_xfft(ra, ia, &rh, &ih, fft, windowing, direction, interpolation, preserverms, level))
        return 1;
    if (gwy_data_field_yfft(&rh, &ih, rb, ib, fft, windowing, direction, interpolation, preserverms, level))
        return 1;

    gwy_data_field_free(&rh);
    gwy_data_field_free(&ih);
    return 0;
}

gint
gwy_data_field_2dfft_real(GwyDataField *ra, GwyDataField *rb, GwyDataField *ib, gint (*fft)(),
                 gint windowing, gint direction, gint interpolation, gboolean preserverms, gboolean level)
{
    GwyDataField rh, ih;
    if (gwy_data_field_initialize(&rh, ra->xres, ra->yres, ra->xreal, ra->yreal, 0)) return 1;
    if (gwy_data_field_initialize(&ih, ra->xres, ra->yres, ra->xreal, ra->yreal, 0)) return 1;

    if (gwy_data_field_xfft_real(ra, &rh, &ih, fft, windowing, direction, interpolation, preserverms, level))
        return 1;
    if (gwy_data_field_yfft(&rh, &ih, rb, ib, fft, windowing, direction, interpolation, preserverms, level))
        return 1;

    gwy_data_field_free(&rh);
    gwy_data_field_free(&ih);
    return 0;
}



gint
gwy_data_field_2dffthumanize(GwyDataField *a)
{
    gint i, j, im, jm;
    GwyDataField b;

    if (gwy_data_field_initialize(&b, a->xres, a->yres, a->xreal, a->yreal, 0)) return 1;
    gwy_data_field_copy(a, &b);

    im=a->yres/2;
    jm=a->xres/2;
    for (i=0; i<im; i++)
    {
        for (j=0; j<jm; j++)
        {
            a->data[(i + im) + (j + jm)*a->yres] = b.data[i + j*a->yres];
            a->data[(i + im) + j*a->yres] = b.data[i + (j + jm)*a->yres];
            a->data[i + (j + jm)*a->yres] = b.data[(i + im) + j*a->yres];
            a->data[i + j*a->yres] = b.data[(i + im) + (j + jm)*a->yres];
        }
    }
    gwy_data_field_free(&b);
    return 0;
}

gint
gwy_data_field_xfft(GwyDataField *ra, GwyDataField *ia, GwyDataField *rb, GwyDataField *ib, gint (*fft)(),
                gint windowing, gint direction, gint interpolation, gboolean preserverms, gboolean level)
{
    gint k;
    GwyDataLine rin, iin, rout, iout;

    if (gwy_data_line_initialize(&rin, ra->xres, ra->yreal, 0)) return 1;
    if (gwy_data_line_initialize(&rout, ra->xres, ra->yreal, 0)) return 1;
    if (gwy_data_line_initialize(&iin, ra->xres, ra->yreal, 0)) return 1;
    if (gwy_data_line_initialize(&iout, ra->xres, ra->yreal, 0)) return 1;
    if (gwy_data_field_resample(ia, ra->xres, ra->yres, GWY_INTERPOLATION_NONE)) return 1;
    if (gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_NONE)) return 1;
    if (gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_NONE)) return 1;
    for (k=0; k<ra->yres; k++)
    {
        gwy_data_field_get_row(ra, &rin, k);
        gwy_data_field_get_row(ia, &iin, k);
        gwy_data_line_fft(&rin, &iin, &rout, &iout, fft, windowing, direction, interpolation, preserverms, level);
        gwy_data_field_set_row(rb, &rout, k);
        gwy_data_field_set_row(ib, &iout, k);
    }
    gwy_data_line_free(&rin);
    gwy_data_line_free(&rout);
    gwy_data_line_free(&iin);
    gwy_data_line_free(&iout);
    return 0;
}

gint
gwy_data_field_yfft(GwyDataField *ra, GwyDataField *ia, GwyDataField *rb, GwyDataField *ib, gint (*fft)(),
                gint windowing, gint direction, gint interpolation, gboolean preserverms, gboolean level)
{
    gint k;
    GwyDataLine rin, iin, rout, iout;

    gwy_data_line_initialize(&rin, ra->xres, ra->yreal, 0);
    gwy_data_line_initialize(&rout, ra->xres, ra->yreal, 0);
    gwy_data_line_initialize(&iin, ra->xres, ra->yreal, 0);
    gwy_data_line_initialize(&iout, ra->xres, ra->yreal, 0);
    gwy_data_field_resample(ia, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_NONE);


    /*we compute each two FFTs simultaneously*/
    for (k=0; k<ra->xres; k++)
    {
        gwy_data_field_get_column(ra, &rin, k);
        gwy_data_field_get_column(ia, &iin, k);
        gwy_data_line_fft(&rin, &iin, &rout, &iout, fft, windowing, direction, interpolation, preserverms, level);
        gwy_data_field_set_column(rb, &rout, k);
        gwy_data_field_set_column(ib, &iout, k);
    }
    gwy_data_line_free(&rin);
    gwy_data_line_free(&rout);
    gwy_data_line_free(&iin);
    gwy_data_line_free(&iout);
     return 0;
}

gint
gwy_data_field_xfft_real(GwyDataField *ra, GwyDataField *rb, GwyDataField *ib, gint (*fft)(),
                gint windowing, gint direction, gint interpolation, gboolean preserverms, gboolean level)
{
    gint k, j, ret=0;
    GwyDataLine rin, iin, rout, iout, rft1, ift1, rft2, ift2;

    if (gwy_data_field_resample(rb, ra->xres, ra->yres, GWY_INTERPOLATION_NONE))
        return 1;
    if (gwy_data_field_resample(ib, ra->xres, ra->yres, GWY_INTERPOLATION_NONE))
        return 1;

    gwy_data_line_initialize(&rin, ra->xres, ra->yreal, 0);
    gwy_data_line_initialize(&rout, ra->xres, ra->yreal, 0);
    gwy_data_line_initialize(&iin, ra->xres, ra->yreal, 0);
    gwy_data_line_initialize(&iout, ra->xres, ra->yreal, 0);
    gwy_data_line_initialize(&rft1, ra->xres, ra->yreal, 0);
    gwy_data_line_initialize(&ift1, ra->xres, ra->yreal, 0);
    gwy_data_line_initialize(&rft2, ra->xres, ra->yreal, 0);
    gwy_data_line_initialize(&ift2, ra->xres, ra->yreal, 0);

    if (ret != 0){g_warning("Could not allocate fields for FFT.\n"); return 0;}

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
    return 0;
}




/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
