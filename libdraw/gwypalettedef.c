/* @(#) $Id$ */

#include <stdio.h>
#include <math.h>
#include "gwypalettedef.h"

#define GWY_PALETTEDEF_TYPE_NAME "GwyPaletteDef"

static void  gwy_palette_def_class_init        (GwyPaletteDefClass *klass);
static void  gwy_palette_def_init              (GwyPaletteDef *palette_def);
static void  gwy_palette_def_finalize          (GwyPaletteDef *palette_def);
static void  gwy_palette_def_serializable_init (gpointer giface, gpointer iface_data);
static void  gwy_palette_def_watchable_init    (gpointer giface, gpointer iface_data);
static guchar* gwy_palette_def_serialize       (GObject *obj, guchar *buffer, gsize *size);
static GObject* gwy_palette_def_deserialize    (const guchar *buffer, gsize size, gsize *position);
static void  gwy_palette_def_value_changed     (GObject *GwyPaletteDef);

gint gwy_palette_def_sort_func(GwyPaletteDefEntry *a, GwyPaletteDefEntry *b);

GType
gwy_palette_def_get_type(void)
{
    static GType gwy_palette_def_type = 0;

    if (!gwy_palette_def_type) {
        static const GTypeInfo gwy_palette_def_info = {
            sizeof(GwyPaletteDefClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_palette_def_class_init,
            NULL,
            NULL,
            sizeof(GwyPaletteDef),
            0,
            (GInstanceInitFunc)gwy_palette_def_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            gwy_palette_def_serializable_init,
            NULL,
            NULL
        };
        GInterfaceInfo gwy_watchable_info = {
            gwy_palette_def_watchable_init,
            NULL,
            NULL
        };

        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        gwy_palette_def_type = g_type_register_static(G_TYPE_OBJECT,
                                                   GWY_PALETTEDEF_TYPE_NAME,
                                                   &gwy_palette_def_info,
                                                   0);
        g_type_add_interface_static(gwy_palette_def_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_palette_def_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_palette_def_type;
}

static void
gwy_palette_def_serializable_init(gpointer giface,
                                  gpointer iface_data)
{
    GwySerializableClass *iface = giface;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_SERIALIZABLE);

    /* initialize stuff */
    iface->serialize = gwy_palette_def_serialize;
    iface->deserialize = gwy_palette_def_deserialize;
}

static void
gwy_palette_def_watchable_init(gpointer giface,
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
gwy_palette_def_class_init(GwyPaletteDefClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    gobject_class->finalize = (GObjectFinalizeFunc)gwy_palette_def_finalize;
}

static void
gwy_palette_def_init(GwyPaletteDef *palette_def)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

}

static void
gwy_palette_def_finalize(GwyPaletteDef *palette_def)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    g_array_free(palette_def->data, 0);
}

GObject*
gwy_palette_def_new(gdouble n)
{
    GwyPaletteDef *palette_def;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    palette_def = g_object_new(GWY_TYPE_PALETTEDEF, NULL);

    palette_def->n = n;
    palette_def->data = g_array_new(0, 0, sizeof(GwyPaletteDefEntry));

    return (GObject*)(palette_def);
}


static guchar*
gwy_palette_def_serialize(GObject *obj,
                          guchar *buffer,
                          gsize *size)
{
    GwyPaletteDef *palette_def;
    GwyPaletteDefEntry *pe;
    GArray *pd;
    gdouble *rdat, *gdat, *bdat, *adat, *xdat;
    gsize ndat, i;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(GWY_IS_PALETTEDEF(obj), NULL);

    palette_def = GWY_PALETTEDEF(obj);
    pd = palette_def->data;

    ndat = pd->len;
    rdat = g_new(gdouble, ndat);
    gdat = g_new(gdouble, ndat);
    bdat = g_new(gdouble, ndat);
    adat = g_new(gdouble, ndat);
    xdat = g_new(gdouble, ndat);

    for (i = 0; i < ndat; i++) {
        pe = &g_array_index(pd, GwyPaletteDefEntry, i);
        rdat[i] = pe->color.r;
        gdat[i] = pe->color.g;
        bdat[i] = pe->color.b;
        adat[i] = pe->color.a;
        xdat[i] = pe->x;
    }

    {
        GwySerializeSpec spec[] = {
            { 'd', "n", &palette_def->n, NULL, },
            { 'b', "has_alpha", &palette_def->has_alpha, NULL, },
            { 'D', "red", &rdat, &ndat, },
            { 'D', "green", &gdat, &ndat, },
            { 'D', "blue", &bdat, &ndat, },
            { 'D', "alpha", &adat, &ndat, },
            { 'D', "x", &xdat, &ndat, },
        };
        buffer = gwy_serialize_pack_object_struct(buffer, size,
                                                  GWY_PALETTEDEF_TYPE_NAME,
                                                  G_N_ELEMENTS(spec), spec);
    }

    g_free(rdat);
    g_free(gdat);
    g_free(bdat);
    g_free(adat);
    g_free(xdat);

    return buffer;
}

static GObject*
gwy_palette_def_deserialize(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    gint ndat, i;
    GwyPaletteDef *palette_def;
    GwyPaletteDefEntry pe;
    gdouble *rdat = NULL, *gdat = NULL, *bdat = NULL, *adat = NULL,
            *xdat = NULL, n;
    gboolean has_alpha = FALSE;
    GwySerializeSpec spec[] = {
      { 'd', "n", &n, NULL, },
      { 'b', "has_alpha", &has_alpha, NULL, },
      { 'D', "red", &rdat, &ndat, },
      { 'D', "green", &gdat, &ndat, },
      { 'D', "blue", &bdat, &ndat, },
      { 'D', "alpha", &adat, &ndat, },
      { 'D', "x", &xdat, &ndat, },
    };

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_PALETTEDEF_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(rdat);
        g_free(gdat);
        g_free(bdat);
        g_free(adat);
        g_free(xdat);
        return NULL;
    }
    /* FIXME: check whether the n's are the same */

    palette_def = (GwyPaletteDef*)gwy_palette_def_new(n);
    for (i = 0; i < ndat; i++) {
        pe.color.r = rdat[i];
        pe.color.g = gdat[i];
        pe.color.b = bdat[i];
        pe.color.a = adat[i];
        pe.x = xdat[i];
        g_array_append_val(palette_def->data, pe);
    }
    palette_def->has_alpha = has_alpha;

    return (GObject*)palette_def;
}



static void
gwy_palette_def_value_changed(GObject *palette_def)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "signal: GwyPaletteDef changed");
    #endif
    g_signal_emit_by_name(GWY_PALETTEDEF(palette_def), "value_changed", NULL);
}

/**
 * gwy_palette_def_copy:
 * @a: source
 * @b: destination
 *
 * Copies data from one palette definition to another.
 *
 **/
void
gwy_palette_def_copy(GwyPaletteDef *a, GwyPaletteDef *b)
{
    guint i;
    GwyPaletteDefEntry pe;
    guint ndat = a->data->len;
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
     /*realloc array*/
    g_array_free(b->data, 0);
    b->data = g_array_new(0, 0, sizeof(GwyPaletteDefEntry));

    /*make a deep copy*/
    for (i=0; i<ndat; i++)
    {
        pe = g_array_index (a->data, GwyPaletteDefEntry, i);
        g_array_append_val(b->data, pe);
    }
}

/**
 * gwy_palette_def_get_n:
 * @a: palette definition of interest
 *
 * Returns maximal x range of palette.
 *
 * Returns:
 **/
gint
gwy_palette_def_get_n(GwyPaletteDef *a)
{
    return a->n;
}

/**
 * gwy_palette_def_get_color:
 * @a: palette definition of interest
 * @x: position (0-N)
 * @interpolation: interpolation method
 *
 * Finds the color at position @x between 0 and gwy_palette_def_get_n().
 *
 * Interpolates between palette definition entries closest to @x
 * and returns the resulting color.
 *
 * Returns: color found
 **/
GwyPaletteEntry
gwy_palette_def_get_color(GwyPaletteDef *a, gdouble x, gint interpolation)
{
    GwyPaletteEntry ret;
    GwyPaletteDefEntry pe, pf;
    guint i;
    gdouble rlow, rhigh, blow, bhigh, glow, ghigh, alow, ahigh, xlow, xhigh;

    rlow =rhigh = blow = bhigh = glow = ghigh = alow = ahigh = xlow = xhigh = 0;

    if (x<0 || x>a->n) {g_warning("Trying t oreach value outside of palette."); return ret;}

    /*find the closest color index*/
    for (i=0; i<(a->data->len-1); i++)
    {
        pe = g_array_index (a->data, GwyPaletteDefEntry, i);
        pf = g_array_index (a->data, GwyPaletteDefEntry, i+1);
        if (pe.x == x)
        {
            ret.r = pe.color.r;
            ret.g = pe.color.g;
            ret.b = pe.color.b;
            ret.a = pe.color.a;
            return ret;
        }
        else if (pf.x == x)
        {
            ret.r = pf.color.r;
            ret.g = pf.color.g;
            ret.b = pf.color.b;
            ret.a = pf.color.a;
            return ret;
        }
        else if (pe.x < x && pf.x > x)
        {
            rlow = pe.color.r; rhigh = pf.color.r;
            glow = pe.color.g; ghigh = pf.color.g;
            blow = pe.color.b; bhigh = pf.color.b;
            alow = pe.color.a; ahigh = pf.color.a;
            xlow = pe.x; xhigh = pf.x;
            break;
        }
    }

    /*interpolate the result*/
    ret.r = gwy_interpolation_get_dval(x, xlow, rlow, xhigh, rhigh, interpolation);
    ret.g = gwy_interpolation_get_dval(x, xlow, glow, xhigh, ghigh, interpolation);
    ret.b = gwy_interpolation_get_dval(x, xlow, blow, xhigh, bhigh, interpolation);
    ret.a = gwy_interpolation_get_dval(x, xlow, alow, xhigh, ahigh, interpolation);

    return ret;
}

/**
 * gwy_palette_def_set_color:
 * @a: palette definition to be changed
 * @val: entry to be added
 *
 * Adds entry to palette definition and resorts this definiton.
 *
 * Returns: 0 at success
 **/
gint
gwy_palette_def_set_color(GwyPaletteDef *a, GwyPaletteDefEntry *val)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    if (val->x<0 || val->x > a->n) {g_warning("Trying to reach value behind palette boundary."); return 1;}

    g_array_append_val(a->data, *val);
    g_array_sort(a->data, (GCompareFunc) gwy_palette_def_sort_func);

    return 0;
}

gint
gwy_palette_def_sort_func(GwyPaletteDefEntry *a, GwyPaletteDefEntry *b)
{
    if (a->x < b->x) return -1;
    else if (a->x == b->x) return 0;
    else return 1;
}

/**
 * gwy_palette_def_sort:
 * @a: palette definition to be resorted
 *
 * Resorts the palette definition.
 **/
void
gwy_palette_def_sort(GwyPaletteDef *a)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    g_array_sort(a->data, (GCompareFunc) gwy_palette_def_sort_func);
}



/**
 * gwy_palette_def_print:
 * @a: palette definition to be outputted
 *
 * Outputs the debugging information about palette definition.
 *
 **/
void
gwy_palette_def_print(GwyPaletteDef *a)
{
    guint i, ndat;
    GArray *pd;
    GwyPaletteDefEntry *pe;

     pd = a->data;
    ndat = pd->len;

    printf("#### palette: ##############################\n");
    printf("%d palette entries in range (0-%f).\n", ndat, a->n);
    for (i=0; i<ndat; i++)
    {
        pe = &g_array_index (pd, GwyPaletteDefEntry, i);
        printf("Palette entry %d: (%f %f %f %f) at %f\n", i, pe->color.r, pe->color.g, pe->color.b, pe->color.a, pe->x);
    }
    printf("############################################\n");

}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
