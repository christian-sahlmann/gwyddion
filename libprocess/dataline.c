/* @(#) $Id$ */

#include <stdio.h>
#include <math.h>

#include <libgwyddion/gwymacros.h>
#include "dataline.h"

#define GWY_DATA_LINE_TYPE_NAME "GwyDataLine"

static void  gwy_data_line_class_init        (GwyDataLineClass *klass);
static void  gwy_data_line_init              (GwyDataLine *data_line);
static void  gwy_data_line_finalize          (GwyDataLine *data_line);
static void  gwy_data_line_serializable_init (gpointer giface);
static void  gwy_data_line_watchable_init    (gpointer giface);
static guchar* gwy_data_line_serialize       (GObject *obj,
                                              guchar *buffer,
                                              gsize *size);
static GObject* gwy_data_line_deserialize    (const guchar *buffer,
                                              gsize size,
                                              gsize *position);
static void  gwy_data_line_value_changed     (GObject *GwyDataLine);


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

        gwy_debug("%s", __FUNCTION__);
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
gwy_data_line_serializable_init(gpointer giface)
{
    GwySerializableClass *iface = giface;

    gwy_debug("%s", __FUNCTION__);
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_SERIALIZABLE);

    /* initialize stuff */
    iface->serialize = gwy_data_line_serialize;
    iface->deserialize = gwy_data_line_deserialize;
}

static void
gwy_data_line_watchable_init(gpointer giface)
{
    GwyWatchableClass *iface = giface;

    gwy_debug("%s", __FUNCTION__);
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_WATCHABLE);

    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_data_line_class_init(GwyDataLineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("%s", __FUNCTION__);

     gobject_class->finalize = (GObjectFinalizeFunc)gwy_data_line_finalize;
}

static void
gwy_data_line_init(GwyDataLine *data_line)
{
    gwy_debug("%s", __FUNCTION__);
    data_line->data = NULL;
    data_line->res = 0;
    data_line->real = 0.0;
}

static void
gwy_data_line_finalize(GwyDataLine *data_line)
{
    gwy_debug("%s", __FUNCTION__);
    gwy_data_line_free(data_line);
}

GObject*
gwy_data_line_new(gint res, gdouble real, gboolean nullme)
{
    GwyDataLine *data_line;

    gwy_debug("%s", __FUNCTION__);
    data_line = g_object_new(GWY_TYPE_DATA_LINE, NULL);

    gwy_data_line_initialize(data_line, res, real, nullme);

    return (GObject*)(data_line);
}


static guchar*
gwy_data_line_serialize(GObject *obj,
                       guchar *buffer,
                       gsize *size)
{
    GwyDataLine *data_line;

    gwy_debug("%s", __FUNCTION__);
    g_return_val_if_fail(GWY_IS_DATA_LINE(obj), NULL);

    data_line = GWY_DATA_LINE(obj);
    {
        GwySerializeSpec spec[] = {
            { 'i', "res", &data_line->res, NULL, },
            { 'd', "real", &data_line->real, NULL, },
            { 'D', "data", &data_line->data, &data_line->res, },
        };

        return gwy_serialize_pack_object_struct(buffer, size,
                                                GWY_DATA_LINE_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_data_line_deserialize(const guchar *buffer,
                          gsize size,
                          gsize *position)
{
    gsize fsize;
    gint res;
    gdouble real, *data = NULL;
    GwyDataLine *data_line;
    GwySerializeSpec spec[] = {
      { 'i', "res", &res, NULL, },
      { 'd', "real", &real, NULL, },
      { 'D', "data", &data, &fsize, },
    };

    gwy_debug("%s", __FUNCTION__);
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_DATA_LINE_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        return NULL;
    }
    if (fsize != res) {
        g_critical("Serialized %s size mismatch %u != %u",
              GWY_DATA_LINE_TYPE_NAME, fsize, res);
        g_free(data);
        return NULL;
    }

    /* don't allocate large amount of memory just to immediately free it */
    data_line = (GwyDataLine*)gwy_data_line_new(1, real, 0);
    g_free(data_line->data);
    data_line->res = res;
    data_line->data = data;

    return (GObject*)data_line;
}



static void
gwy_data_line_value_changed(GObject *data_line)
{
    gwy_debug("signal: GwyDataLine changed");
    g_signal_emit_by_name(GWY_DATA_LINE(data_line), "value_changed", NULL);
}


void
gwy_data_line_alloc(GwyDataLine *a, gint res)
{
    gwy_debug("%s", __FUNCTION__);

    a->res = res;
    a->data = g_new(gdouble, a->res);
}

void
gwy_data_line_initialize(GwyDataLine *a,
                         gint res, gdouble real,
                         gboolean nullme)
{
    int i;

    gwy_debug("%s", __FUNCTION__);
    gwy_data_line_alloc(a, res);

    a->real = real;
    if (nullme) {
        for (i = 0; i < a->res; i++)
            a->data[i] = 0;
    }
}

void
gwy_data_line_free(GwyDataLine *a)
{
    gwy_debug("%s", __FUNCTION__);
    g_free(a->data);
}

void
gwy_data_line_resample(GwyDataLine *a, gint res, gint interpolation)
{
    gint i;
    gdouble ratio = ((gdouble)a->res - 1)/((gdouble)res - 1);
    GwyDataLine b;

    gwy_debug("%s", __FUNCTION__);
    if (res == a->res)
        return;

    b.res=a->res;
    b.data = g_new(gdouble, a->res);
    gwy_data_line_copy(a, &b);

    a->res=res;
    a->data = g_renew(gdouble, a->data, a->res);
    for (i = 0; i < res; i++) {
        a->data[i] = gwy_data_line_get_dval(&b, (gdouble)i*ratio,
                                            interpolation);
    }
    gwy_data_line_free(&b);
    gwy_data_line_value_changed(G_OBJECT(a));
}


gboolean
gwy_data_line_resize(GwyDataLine *a, gint from, gint to)
{
    gint i;
    GwyDataLine b;

    gwy_debug("%s", __FUNCTION__);
    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to < a->res, FALSE);

    b.res = a->res;
    b.data = g_new(gdouble, a->res);
    gwy_data_line_copy(a, &b);

    a->res = to-from;
    a->data = g_renew(gdouble, a->data, a->res);

    for (i = from; i < to; i++)
        a->data[i-from] = b.data[i];
    gwy_data_line_free(&b);
    gwy_data_line_value_changed(G_OBJECT(a));

    return TRUE;
}

gboolean
gwy_data_line_copy(GwyDataLine *a, GwyDataLine *b)
{
    gint i;

    gwy_debug("%s", __FUNCTION__);
    g_return_val_if_fail(a->res == b->res, FALSE);

    for (i=0; i<a->res; i++)
        b->data[i] = a->data[i];
    return 0;
}

gdouble
gwy_data_line_get_dval(GwyDataLine *a, gdouble x, gint interpolation)
{
    gint l = floor(x);
    gdouble w1, w2, w3, w4;
    gdouble rest = x - (gdouble)l;

    gwy_debug("%s", __FUNCTION__);
    g_return_val_if_fail(x >= 0 && x < a->res, 0.0);

    /*simple (and fast) methods*/
    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        /* XXX: WTF? why is this different from ROUND? */
        return 0.0;

        case GWY_INTERPOLATION_ROUND:
        return a->data[(gint)(x + 0.5)];

        case GWY_INTERPOLATION_BILINEAR:
        if (rest == 0)
            return a->data[l];
        return
            (1 - rest)*a->data[l] + rest*a->data[l+1];
    }

    /*other 4point methods are very similar:*/
    if (l < 1 || l >= (a->res - 2))
        return gwy_data_line_get_dval(a, x, GWY_INTERPOLATION_BILINEAR);

    w1 = rest + 1;
    w2 = rest;
    w3 = 1 - rest;
    w4 = 2 - rest;
    switch (interpolation) {
        case GWY_INTERPOLATION_KEY:
        w1 = -0.5*w1*w1*w1 + 2.5*w1*w1 - 4*w1 + 2;
        w2 = 1.5*w2*w2*w2 - 2.5*w2*w2 + 1;
        w3 = 1.5*w3*w3*w3 - 2.5*w3*w3 + 1;
        w4 = -0.5*w4*w4*w4 + 2.5*w4*w4 - 4*w4 + 2;
        break;

        case GWY_INTERPOLATION_BSPLINE:
        w1 = (2-w1)*(2-w1)*(2-w1)/6;
        w2 = 0.6666667-0.5*w2*w2*(2-w2);
        w3 = 0.6666667-0.5*w3*w3*(2-w3);
        w4 = (2-w4)*(2-w4)*(2-w4)/6;
        break;

        case GWY_INTERPOLATION_OMOMS:
        w1 = -w1*w1*w1/6+w1*w1-85*w1/42+1.3809523;
        w2 = w2*w2*w2/2-w2*w2+w2/14+0.6190476;
        w3 = w3*w3*w3/2-w3*w3+w3/14+0.6190476;
        w4 = -w4*w4*w4/6+w4*w4-85*w4/42+1.3809523;
        break;

        case GWY_INTERPOLATION_NNA:
        /* XXX: WFT? _ALL_ interpolations should return the point when
         * rest == 0 */
        if (rest == 0)
            return a->data[l];
        w1 = 1/(w1*w1*w1*w1);
        w2 = 1/(w2*w2*w2*w2);
        w3 = 1/(w3*w3*w3*w3);
        w4 = 1/(w4*w4*w4*w4);
        return (w1*a->data[l-1] + w2*a->data[l]
                + w3*a->data[l+1] + w4*a->data[l+2])/(w1 + w2 + w3 + w4);
    }

    return w1*a->data[l-1] + w2*a->data[l] + w3*a->data[l+1] + w4*a->data[l+2];
}

gint
gwy_data_line_get_res(GwyDataLine *a)
{
    return a->res;
}

gdouble gwy_data_line_get_real(GwyDataLine *a)
{
    return a->real;
}

void gwy_data_line_set_real(GwyDataLine *a, gdouble real)
{
    a->real = real;
    gwy_data_line_value_changed(G_OBJECT(a));
}

gdouble
gwy_data_line_itor(GwyDataLine *a, gdouble pixval)
{
    return pixval*a->real/a->res;
}

gdouble
gwy_data_line_rtoi(GwyDataLine *a, gdouble realval)
{
    return realval*a->res/a->real;
}

gdouble
gwy_data_line_get_val(GwyDataLine *a, gint i)
{
    g_return_val_if_fail(i >= 0 && i < a->res, 0.0);

    return a->data[i];
}

gboolean
gwy_data_line_set_val(GwyDataLine *a, gint i, gdouble value)
{
    g_return_val_if_fail(i >= 0 && i < a->res, FALSE);

    a->data[i] = value;
    gwy_data_line_value_changed(G_OBJECT(a));

    return TRUE;
}


gdouble
gwy_data_line_get_dval_real(GwyDataLine *a, gdouble x, gint interpolation)
{
    return gwy_data_line_get_dval(a, gwy_data_line_rtoi(a, x), interpolation);
}

void
gwy_data_line_invert(GwyDataLine *a, gboolean x, gboolean z)
{
    gint i;
    gdouble avg;
    GwyDataLine b;

    gwy_debug("%s", __FUNCTION__);
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

void
gwy_data_line_fill(GwyDataLine *a, gdouble value)
{
    gint i;

    for (i = 0; i < a->res; i++)
        a->data[i] = value;
    gwy_data_line_value_changed(G_OBJECT(a));
}

void
gwy_data_line_add(GwyDataLine *a, gdouble value)
{
    gint i;

    for (i = 0; i < a->res; i++)
        a->data[i] += value;
    gwy_data_line_value_changed(G_OBJECT(a));
}

void
gwy_data_line_multiply(GwyDataLine *a, gdouble value)
{
    gint i;

    for (i = 0; i < a->res; i++)
        a->data[i] *= value;
    gwy_data_line_value_changed(G_OBJECT(a));
}

void
gwy_data_line_part_fill(GwyDataLine *a, gint from, gint to, gdouble value)
{
    gint i;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_if_fail(from >= 0 && to < a->res);

    for (i = from; i < to; i++)
        a->data[i] = value;
    gwy_data_line_value_changed(G_OBJECT(a));
}

void
gwy_data_line_part_add(GwyDataLine *a, gint from, gint to, gdouble value)
{
    gint i;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_if_fail(from >= 0 && to < a->res);

    for (i = from; i < to; i++)
        a->data[i] += value;
    gwy_data_line_value_changed(G_OBJECT(a));
}

void
gwy_data_line_part_multiply(GwyDataLine *a, gint from, gint to, gdouble value)
{
    gint i;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_if_fail(from >= 0 && to < a->res);

    for (i = from; i < to; i++)
        a->data[i] *= value;
    gwy_data_line_value_changed(G_OBJECT(a));
}

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

gdouble
gwy_data_line_get_avg(GwyDataLine *a)
{
    gint i;
    gdouble avg = 0;

    for (i = 0; i < a->res; i++)
        avg += a->data[i];

    return avg/(gdouble)a->res;
}

gdouble
gwy_data_line_get_rms(GwyDataLine *a)
{
    gint i;
    gdouble rms = 0;
    gdouble avg = gwy_data_line_get_avg(a);

    for (i = 0; i < a->res; i++)
        rms += (avg - a->data[i])*(avg - a->data[i]);

    return sqrt(rms)/(gdouble)a->res;
}

gdouble
gwy_data_line_get_sum(GwyDataLine *a)
{
    gint i;
    gdouble sum = 0;

    for (i = 0; i < a->res; i++)
        sum += a->data[i];

    return sum;
}


gdouble
gwy_data_line_part_get_max(GwyDataLine *a, gint from, gint to)
{
    gint i;
    gdouble max = -G_MAXDOUBLE;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to < a->res, max);

    for (i = from; i < to; i++) {
        if (max < a->data[i])
            max = a->data[i];
    }
    return max;
}

gdouble
gwy_data_line_part_get_min(GwyDataLine *a, gint from, gint to)
{
    gint i;
    gdouble min = G_MAXDOUBLE;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to < a->res, min);

    for (i = from; i < to; i++) {
        if (min > a->data[i])
            min = a->data[i];
    }

    return min;
}

gdouble
gwy_data_line_part_get_avg(GwyDataLine *a, gint from, gint to)
{
    return gwy_data_line_part_get_sum(a, from, to)/(gdouble)(to-from);
}

gdouble
gwy_data_line_part_get_rms(GwyDataLine *a, gint from, gint to)
{
    gint i;
    gdouble rms = 0;
    gdouble avg;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to < a->res, rms);

    avg = gwy_data_line_part_get_avg(a, from, to);
    for (i = from; i < to; i++)
        rms += (avg - a->data[i])*(avg - a->data[i]);

    return sqrt(rms)/(gdouble)(to-from);
}

gdouble
gwy_data_line_part_get_sum(GwyDataLine *a, gint from, gint to)
{
    gint i;
    gdouble sum = 0;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to < a->res, sum);

    for (i = from; i < to; i++)
        sum += a->data[i];

    return sum;
}

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
    gwy_data_line_value_changed(G_OBJECT(a));
    return tot;
}

gint
gwy_data_line_part_threshold(GwyDataLine *a,
                             gint from, gint to,
                             gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, tot=0;

    if (to < from)
        GWY_SWAP(gint, from, to);

    g_return_val_if_fail(from >= 0 && to < a->res, 0);

    for (i = from; i < to; i++) {
        if (a->data[i] < threshval)
            a->data[i] = bottom;
        else {
            a->data[i] = top;
            tot++;
        }
    }
    gwy_data_line_value_changed(G_OBJECT(a));
    return tot;
}

void
gwy_data_line_line_coefs(GwyDataLine *a, gdouble *av, gdouble *bv)
{
    gint i;
    gdouble sumxixi=0;
    gdouble sumxi=0;
    gdouble sumsixi=0;
    gdouble sumsi=0;
    gdouble dkap;

    gwy_debug("%s", __FUNCTION__);
    for (i = 0; i < a->res; i++) {
        dkap = (i+1);
        sumsi += a->data[i];
        sumxi += dkap;
        sumxixi += dkap*dkap;
        sumsixi += dkap*a->data[i];
    }
    *bv = (sumsixi*a->res - sumsi*sumxi) / (sumxixi*a->res - sumxi*sumxi);
    *av = (sumsi - (*bv)*sumxi)/a->res;
}

void
gwy_data_line_line_level(GwyDataLine *a, gdouble av, gdouble bv)
{
    gint i;

    gwy_debug("%s", __FUNCTION__);
    for (i = 0; i < a->res; i++)
        a->data[i] -= av + bv*(i+1);
    gwy_data_line_value_changed(G_OBJECT(a));
}

void
gwy_data_line_line_rotate(GwyDataLine *a, gdouble angle, gint interpolation)
{
    gint i, k, maxi;
    gdouble ratio, x, as, radius, x1, x2, y1, y2;
    GwyDataLine dx, dy;

    gwy_debug("%s", __FUNCTION__);
    if (angle == 0)
        return;

    dx.res = dy.res = a->res;
    dx.data = g_new(gdouble, a->res);
    dy.data = g_new(gdouble, a->res);

    angle *= G_PI/180;
    ratio = a->real/(double)a->res;
    dx.data[0] = 0;
    dy.data[0] = a->data[0];
    for (i = 1; i < a->res; i++) {
        as = atan(a->data[i]/((double)i*ratio));
        radius = sqrt(((double)i*ratio)*((double)i*ratio)
                      + a->data[i]*a->data[i]);
        /*printf("i=%f, radius=%f\n", i*ratio, radius);*/
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

        x1 = dx.data[k-1];
        x2 = dx.data[k];
        y1 = dy.data[k-1];
        y2 = dy.data[k];


        if (interpolation == GWY_INTERPOLATION_ROUND
            || interpolation == GWY_INTERPOLATION_BILINEAR)
            a->data[i] = gwy_interpolation_get_dval(x, x1, y1, x2, y2,
                                                    interpolation);
        else
            g_warning("Interpolation not implemented yet.\n");
    }
    if (maxi != 0)
        gwy_data_line_resize(a, 0, maxi);

    /* XXX: where was this freed? */
    g_free(dx.data);
    g_free(dy.data);
    gwy_data_line_value_changed(G_OBJECT(a));
}

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


void
gwy_data_line_fft_hum(gint direction, GwyDataLine *ra, GwyDataLine *ia, GwyDataLine *rb, GwyDataLine *ib, gint interpolation)
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

    gwy_fft_hum(direction, ra->data, ia->data, rb->data, ib->data, newres);

    if (newres != oldres) {
        gwy_data_line_resample(ra, oldres, interpolation);
        gwy_data_line_resample(ia, oldres, interpolation);
        gwy_data_line_resample(rb, oldres, interpolation);
        gwy_data_line_resample(ib, oldres, interpolation);
    }
}

void
gwy_data_line_fft(GwyDataLine *ra, GwyDataLine *ia,
                  GwyDataLine *rb, GwyDataLine *ib,
                  gint (*fft)(), GwyWindowingType windowing, gint direction,
                  GwyInterpolationType interpolation, gboolean preserverms,
                  gboolean level)
{
    gdouble rmsa, rmsb, av, bv;
    GwyDataLine multra, multia;

    gwy_debug("%s", __FUNCTION__);
    if (ia->res != ra->res)
        gwy_data_line_resample(ia, ra->res, GWY_INTERPOLATION_NONE);
    if (rb->res != ra->res)
        gwy_data_line_resample(rb, ra->res, GWY_INTERPOLATION_NONE);
    if (ib->res != ra->res)
        gwy_data_line_resample(ib, ra->res, GWY_INTERPOLATION_NONE);

    if (level == TRUE) {
        gwy_data_line_line_coefs(ra, &av, &bv);
        gwy_data_line_line_level(ra, av, bv);
        gwy_data_line_line_coefs(ia, &av, &bv);
        gwy_data_line_line_level(ia, av, bv);
    }

    if (preserverms == TRUE
        && windowing != GWY_WINDOWING_NONE
        && windowing != GWY_WINDOWING_RECT) {
        gwy_data_line_initialize(&multra, ra->res, ra->real, 0);
        gwy_data_line_initialize(&multia, ra->res, ra->real, 0);
        gwy_data_line_copy(ra, &multra);
        gwy_data_line_copy(ia, &multia);

        rmsa = gwy_data_line_get_rms(&multra);

        gwy_fft_window(multra.data, multra.res, windowing);
        gwy_fft_window(multia.data, multia.res, windowing);

        rmsb = gwy_data_line_get_rms(&multra);

        (*fft)(direction, &multra, &multia, rb, ib, multra.res, interpolation);

        gwy_data_line_multiply(rb, rmsa/rmsb);
        gwy_data_line_multiply(ib, rmsa/rmsb);
        gwy_data_line_free(&multra);
        gwy_data_line_free(&multia);
    }
    else {
        gwy_fft_window(ra->data, ra->res, windowing);
        gwy_fft_window(ia->data, ra->res, windowing);

        (*fft)(direction, ra, ia, rb, ib, ra->res, interpolation);
    }
    gwy_data_line_value_changed(G_OBJECT(ra));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
