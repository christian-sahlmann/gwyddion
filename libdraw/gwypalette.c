/* @(#) $Id$ */

#include <stdio.h>
#include <math.h>
#include "gwypalette.h"

#define GWY_PALETTE_TYPE_NAME "GwyPalette"

#define GWY_PALETTE_DEFAULT_SIZE 512

static void  gwy_palette_class_init        (GwyPaletteClass *klass);
static void  gwy_palette_init              (GwyPalette *palette);
static void  gwy_palette_finalize          (GwyPalette *palette);
static void  gwy_palette_serializable_init (gpointer giface, gpointer iface_data);
static void  gwy_palette_watchable_init    (gpointer giface, gpointer iface_data);
static guchar* gwy_palette_serialize       (GObject *obj, guchar *buffer, gsize *size);
static GObject* gwy_palette_deserialize    (const guchar *buffer, gsize size, gsize *position);
static void  gwy_palette_value_changed     (GObject *GwyPalette);
static gint  gwy_palette_recompute_table   (GwyPalette *palette);

static gboolean gwy_palette_is_extreme     (GwyPaletteEntry a,
                                            GwyPaletteEntry am,
                                            GwyPaletteEntry ap);
static gboolean gwy_palette_is_bigslopechange(GwyPaletteEntry a,
                                              GwyPaletteEntry am,
                                              GwyPaletteEntry ap);

GType
gwy_palette_get_type(void)
{
    static GType gwy_palette_type = 0;

    if (!gwy_palette_type) {
        static const GTypeInfo gwy_palette_info = {
            sizeof(GwyPaletteClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_palette_class_init,
            NULL,
            NULL,
            sizeof(GwyPalette),
            0,
            (GInstanceInitFunc)gwy_palette_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            gwy_palette_serializable_init,
            NULL,
            NULL
        };
        GInterfaceInfo gwy_watchable_info = {
            gwy_palette_watchable_init,
            NULL,
            NULL
        };

        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        gwy_palette_type = g_type_register_static(G_TYPE_OBJECT,
                                                   GWY_PALETTE_TYPE_NAME,
                                                   &gwy_palette_info,
                                                   0);
        g_type_add_interface_static(gwy_palette_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_palette_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_palette_type;
}

static void
gwy_palette_serializable_init(gpointer giface,
                              gpointer iface_data)
{
    GwySerializableClass *iface = giface;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_SERIALIZABLE);

    /* initialize stuff */
    iface->serialize = gwy_palette_serialize;
    iface->deserialize = gwy_palette_deserialize;
}

static void
gwy_palette_watchable_init(gpointer giface,
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
gwy_palette_class_init(GwyPaletteClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    gobject_class->finalize = (GObjectFinalizeFunc)gwy_palette_finalize;
}

static void
gwy_palette_init(GwyPalette *palette)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    palette->nofvals = 0;
    palette->color = NULL;
    palette->def = NULL;

}

static void
gwy_palette_finalize(GwyPalette *palette)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    g_free(palette->color);
    g_free(palette->samples);
    g_array_free(palette->def->data, 0);
}

GObject*
gwy_palette_new(GwyPalettePreset preset)
{
    GwyPalette *palette;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    palette = g_object_new(GWY_TYPE_PALETTE, NULL);
    palette->nofvals = GWY_PALETTE_DEFAULT_SIZE;
    palette->color = g_new(GwyPaletteEntry, palette->nofvals);
    palette->samples = g_new(guchar, 4*palette->nofvals);
    palette->def = (GwyPaletteDef*)gwy_palette_def_new(palette->nofvals);
    gwy_palette_setup_preset(palette, preset);

    return (GObject*)(palette);
}


static guchar*
gwy_palette_serialize(GObject *obj,
                      guchar *buffer,
                      gsize *size)
{
    GwyPalette *palette;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(GWY_IS_PALETTE(obj), NULL);

    palette = GWY_PALETTE(obj);
    {
        GwySerializeSpec spec[] = {
            { 'o', "pdef", &palette->def, NULL, },
        };
        return gwy_serialize_pack_object_struct(buffer, size,
                                                GWY_PALETTE_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_palette_deserialize(const guchar *buffer,
                        gsize size,
                        gsize *position)
{
    GwyPalette *palette;
    GwyPaletteDef *pdef = NULL;
    GwySerializeSpec spec[] = {
      { 'o', "pdef", &pdef, NULL, },
    };

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_PALETTE_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        if (pdef)
            g_object_unref(pdef);
        return NULL;
    }

    palette = (GwyPalette*)gwy_palette_new(0);
    g_array_free(palette->def->data, FALSE);
    palette->def = pdef;

    gwy_palette_recompute_table(palette);

    return (GObject*)palette;
}



static void
gwy_palette_value_changed(GObject *palette)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "signal: GwyPalette changed");
    #endif
    g_signal_emit_by_name(GWY_PALETTE(palette), "value_changed", NULL);
}

/**
 * gwy_palette_set_def:
 * @palette: palette we want to be set
 * @pdef: palette definition to be used
 *
 * Sets the palette definition.
 *
 * Makes a deep copy of @pdef and puts it into
 * palette structure. This function does not recompute the color tables
 * inside palette. Call gwy_palette_recompute_table() to do this afterwards
 * or use gwy_palette_setup() instead of this function.
 **/
void
gwy_palette_set_def(GwyPalette *palette, GwyPaletteDef* pdef)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    gwy_palette_def_copy(pdef, palette->def);
}

/**
 * gwy_palette_recompute_table:
 * @palette: palette to be recomputed
 *
 * Recomputes all the color tables inside palette.
 *
 * Call this function if you changed the palette definition
 * directly or by calling gwy_palette_set_def().
 *
 * Returns: 0 at success
 **/
static gint
gwy_palette_recompute_table(GwyPalette *palette)
{
    gint i;
    GwyPaletteEntry pe;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    for (i = 0; i < palette->nofvals; i++) {
        pe = (gwy_palette_def_get_color(palette->def,
                                        i, GWY_INTERPOLATION_BILINEAR));
        palette->color[i] = pe;
    }
    palette->samples = gwy_palette_sample(palette,
                                          palette->nofvals,
                                          palette->samples);

    return 0;
}

/**
 * gwy_palette_setup:
 * @a: palette we want to set up
 * @pdef: palette definition
 *
 * Sets the palette definition and recomputes all the color tables inside palette
 *
 * Use this function to set up the palette if you have allready prepared
 * the palette definition.
 *
 * Returns: 0 at success
 **/
gint
gwy_palette_setup(GwyPalette *a, GwyPaletteDef *pdef)
{
    gwy_palette_set_def(a, pdef);
    return gwy_palette_recompute_table(a);
}

/**
 * gwy_palette_set_color:
 * @palette: palette we want to change
 * @val: palette entry to add
 * @i: position inside palette to put the entry
 *
 * Puts the color entry inside palette color table and recomputes palette definition
 *
 * Note: this is not the best way of changing palette. Better choice is
 * to change the palette definition and recompute the color table.
 *
 * Returns: 0 at success
 **/
gint
gwy_palette_set_color(GwyPalette *palette, GwyPaletteEntry *val, gint i)
{
    if (i < 0 || i >= palette->nofvals) {
        g_warning("Trying to reach value outside of palette.\n");
        return 1;
    }
    palette->color[i] = *val;
    gwy_palette_recompute_palette(palette, 20);
    return 0;
}

static gboolean
gwy_palette_is_extreme(GwyPaletteEntry a,
                       GwyPaletteEntry am,
                       GwyPaletteEntry ap)
{
    if ((a.r > am.r && a.r > ap.r) || (a.r < am.r && a.r < ap.r))
        return 1;
    else if ((a.g > am.g && a.g > ap.g) || (a.g < am.g && a.g < ap.g))
        return 1;
    else if ((a.b > am.b && a.b > ap.b) || (a.b < am.b && a.b < ap.b))
        return 1;
    else if ((a.a > am.a && a.a > ap.a) || (a.a < am.a && a.a < ap.a))
        return 1;
    else
        return 0;
}

static gboolean
gwy_palette_is_bigslopechange(GwyPaletteEntry a,
                              GwyPaletteEntry am,
                              GwyPaletteEntry ap)
{
    gdouble tresh = 10; /*treshold for large slope change*/

    if (fabs(fabs(ap.r - a.r) - fabs(am.r - a.r)) > tresh)
        return 1;
    else if (fabs(fabs(ap.g - a.g)-fabs(am.g - a.g)) > tresh)
        return 1;
    else if (fabs(fabs(ap.b - a.b)-fabs(am.b - a.b)) > tresh)
        return 1;
    else if (fabs(fabs(ap.a - a.a)-fabs(am.a - a.a)) > tresh)
        return 1;
    else
        return 0;
}

/**
 * gwy_palette_recompute_palette:
 * @palette: palette which definition is to be recomputed
 * @istep: maximum distance of definition entries
 *
 * Fits the palette color tables with to obtain palette definition.
 *
 * The function does the reverse of gwy_palette_recompute_table().
 * It finds the extrema points inside palette color tables and
 * tries to setup the palette definition to fit the color tables.
 * This fucntion should be used only when there is a reason
 * for doing this, for example after some graphical entry
 * of palette color table values. Parameter @istep controls
 * the precision of the linear fit.
 *
 * Returns: 0 at success
 **/
gint
gwy_palette_recompute_palette(GwyPalette *palette, gint istep)
{
    /*local extremes will be used as palette definition entries*/
    /*if there is no local extrema within a given number of points,
     an entry will be added anyway to make sure that convex/concave
     palettes will be fitted with enough precision*/
    gint i, icount;
    GwyPaletteDefEntry pd;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    g_array_free(palette->def->data, FALSE);
    palette->def->data = g_array_new(FALSE, FALSE, sizeof(GwyPaletteDefEntry));

    pd.x = 0;
    pd.color = palette->color[0];
    gwy_palette_def_set_color(palette->def, &pd);
    icount = 1;
    for (i = 1; i < (palette->nofvals-1); i++) {
        if (gwy_palette_is_extreme(palette->color[i],
                                   palette->color[i-1],
                                   palette->color[i+1])
            || gwy_palette_is_bigslopechange(palette->color[i],
                                             palette->color[i-1],
                                             palette->color[i+1])) {
            pd.x = (gdouble)i;
            pd.color = palette->color[i];
            gwy_palette_def_set_color(palette->def, &pd);
            icount = 1;
            continue;
        }
        else if (icount >= istep) {
            pd.x = (gdouble)i;
            pd.color = palette->color[i];
            gwy_palette_def_set_color(palette->def, &pd);
            icount = 1;
            continue;
        }
        icount++;
    }
    pd.x = palette->nofvals-1;
    pd.color = palette->color[palette->nofvals-1];
    gwy_palette_def_set_color(palette->def, &pd);

    return 0;
}

/**
 * gwy_palette_sample:
 * @palette: palette to be sampled
 * @oldpalette: pointer to field to be filled
 *
 * Fills the GPixmap-like field of integer values representing palette.
 *
 * Returns:
 **/
guchar*
gwy_palette_sample(GwyPalette *palette, gint size, guchar *oldpalette)
{
    gint i, k;
    gdouble cor;
    GwyPaletteEntry pe;

    oldpalette = g_renew(guchar, oldpalette, 4*size);

    k = 0;
    cor = palette->def->n/(gdouble)size;
    for (i = 0; i < size; i++) {
        pe = gwy_palette_def_get_color(palette->def,
                                       i*cor,
                                       GWY_INTERPOLATION_BILINEAR);

        oldpalette[k++] = (guchar)((gint32) pe.r);
        oldpalette[k++] = (guchar)((gint32) pe.g);
        oldpalette[k++] = (guchar)((gint32) pe.b);
        oldpalette[k++] = (guchar)((gint32) pe.a);
    }
    return oldpalette;
}

/**
 * gwy_palette_setup_predef:
 * @a: palette to be filled
 * @preset: which preset palette
 *
 * Fills palette with some predefined colors
 *
 * Returns: 0 at success
 **/
gint
gwy_palette_setup_preset(GwyPalette *palette, GwyPalettePreset preset)
{
    GwyPaletteDefEntry cval;
    gsize i;
    static const struct { GwyPalettePreset preset; GwyPaletteEntry entry; }
    presets[] = {
        { GWY_PALETTE_RED, { 255, 0, 0, 255 } },
        { GWY_PALETTE_GREEN, { 0, 255, 0, 255 } },
        { GWY_PALETTE_BLUE, { 0, 0, 255, 255 } },
        { GWY_PALETTE_YELLOW, { 212, 183, 42, 255 } },
        { GWY_PALETTE_PINK, { 255, 20, 160, 255 } },
        { GWY_PALETTE_OLIVE, { 94, 176, 117, 255 } },
    };
    static const GwyPaletteEntry black = { 0, 0, 0, 255 };
    static const GwyPaletteEntry white = { 255, 255, 255, 255 };
    static const GwyPaletteEntry rainbow1[] = {
        { 255, 0, 0, 255 },
        { 255, 255, 0, 255 },
        { 0, 255, 255, 255 },
        { 255, 0, 255, 255 },
        { 0, 255, 0, 255 },
        { 0, 0, 255, 255 },
        { 128, 128, 128, 255 },
    };
    static const GwyPaletteEntry rainbow2[] = {
        { 255, 0, 0, 255 },
        { 0, 255, 0, 255 },
        { 0, 0, 255, 255 },
    };

    /*remove every previous items in the palette definition*/
    g_array_free(palette->def->data, FALSE);
    palette->def->data = g_array_new(FALSE, FALSE, sizeof(GwyPaletteDefEntry));

    cval.color = black;
    cval.x = 0;
    gwy_palette_def_set_color(palette->def, &cval);

    cval.color = white;
    cval.x = 511;
    gwy_palette_def_set_color(palette->def, &cval);


    switch (preset) {
        case GWY_PALETTE_GRAY:
        break;

        case GWY_PALETTE_RED:
        case GWY_PALETTE_GREEN:
        case GWY_PALETTE_BLUE:
        case GWY_PALETTE_YELLOW:
        case GWY_PALETTE_PINK:
        case GWY_PALETTE_OLIVE:
        for (i = 0; i < G_N_ELEMENTS(presets); i++) {
            if (preset == presets[i].preset)
                break;
        }
        g_assert(i < G_N_ELEMENTS(presets));
        cval.color = presets[i].entry;
        cval.x = 256;
        gwy_palette_def_set_color(palette->def, &cval);
        break;

        case GWY_PALETTE_BW1:
        for (i = 0; i < 10; i++) {
            cval.color = i%2 ? black : white;
            cval.x = (gdouble)i*(256/10.0);
            gwy_palette_def_set_color(palette->def, &cval);
        }
        break;

        case GWY_PALETTE_BW2:
        for (i = 0; i < 10; i++) {
            cval.color = i%2 ? black : white;
            cval.x = (gdouble)i*(256/10.0);
            gwy_palette_def_set_color(palette->def, &cval);

            cval.color = i%2 ? white : black;
            cval.x = (gdouble)i*(256/10.0) + 0.1;
            gwy_palette_def_set_color(palette->def, &cval);
        }
        break;

        case GWY_PALETTE_RAINBOW1:
        for (i = 0; i < G_N_ELEMENTS(rainbow1); i++) {
            cval.color = rainbow1[i];
            cval.x = 64*(i + 1);
            gwy_palette_def_set_color(palette->def, &cval);
        }
        break;

        case GWY_PALETTE_RAINBOW2:
        for (i = 0; i < G_N_ELEMENTS(rainbow1); i++) {
            cval.color = rainbow2[i];
            cval.x = 128*(i + 1);
            gwy_palette_def_set_color(palette->def, &cval);
        }
        break;

        default:
        /* nothing means gray here */
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
              "Palette %d not implemented.", preset);
        return 1;
        break;
    }

    gwy_palette_def_sort(palette->def);

    return gwy_palette_recompute_table(palette);
}

const guchar*
gwy_palette_get_samples(GwyPalette *palette, gint *n_of_samples)
{
    *n_of_samples = palette->nofvals;
    return palette->samples;
}

const GwyPaletteEntry*
gwy_palette_get_data(GwyPalette *palette, gint *n_of_data)
{
    *n_of_data = palette->nofvals;
    return palette->color;
}

/**
 * gwy_palette_print:
 * @palette: palette to be outputted
 *
 * Debugging function that prints full palette color tables to stdout.
 *
 **/
void
gwy_palette_print(GwyPalette *palette)
{
    gint i;
    printf("### palette (integer output) ##########################################\n");
    for (i=0; i<palette->nofvals; i++)
    {
        printf("%d : (%d %d %d %d)\n", i, (gint)palette->color[i].r, (gint)palette->color[i].g, (gint)palette->color[i].b, (gint)palette->color[i].a);
    }
    printf("######################################################################\n");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
