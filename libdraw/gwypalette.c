/* @(#) $Id$ */

#include <libgwyddion/gwymacros.h>
#include "gwypalette.h"

/* TODO: hook on "value_changed" of GwyPaletteDef */

#define GWY_PALETTE_TYPE_NAME "GwyPalette"

#define GWY_PALETTE_DEFAULT_SIZE 512
#define BITS_PER_SAMPLE 8
#define MAX_CVAL (0.999999*(1 << (BITS_PER_SAMPLE)))

static void     gwy_palette_class_init            (GwyPaletteClass *klass);
static void     gwy_palette_init                  (GwyPalette *palette);
static void     gwy_palette_finalize              (GwyPalette *palette);
static void     gwy_palette_serializable_init     (gpointer giface);
static void     gwy_palette_watchable_init        (gpointer giface);
static guchar*  gwy_palette_serialize             (GObject *obj,
                                                   guchar *buffer,
                                                   gsize *size);
static GObject* gwy_palette_deserialize           (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static void     gwy_palette_value_changed         (GObject *GwyPalette);
static gint     gwy_palette_recompute_table       (GwyPalette *palette);


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
            (GInterfaceInitFunc)gwy_palette_serializable_init,
            NULL,
            NULL
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_palette_watchable_init,
            NULL,
            NULL
        };

        gwy_debug("%s", __FUNCTION__);
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
gwy_palette_serializable_init(gpointer giface)
{
    GwySerializableClass *iface = giface;

    gwy_debug("%s", __FUNCTION__);
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_SERIALIZABLE);

    /* initialize stuff */
    iface->serialize = gwy_palette_serialize;
    iface->deserialize = gwy_palette_deserialize;
}

static void
gwy_palette_watchable_init(gpointer giface)
{
    GwyWatchableClass *iface = giface;

    gwy_debug("%s", __FUNCTION__);

    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_WATCHABLE);

    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_palette_class_init(GwyPaletteClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("%s", __FUNCTION__);

    gobject_class->finalize = (GObjectFinalizeFunc)gwy_palette_finalize;
    klass->palettes = g_hash_table_new(g_direct_hash, g_direct_equal);
}

static void
gwy_palette_init(GwyPalette *palette)
{
    gwy_debug("%s", __FUNCTION__);

    palette->nofvals = 0;
    palette->color = NULL;
    palette->def = NULL;

}

static void
gwy_palette_finalize(GwyPalette *palette)
{
    GwyPaletteClass *klass;

    gwy_debug("%s (%s)", __FUNCTION__, palette->def->name);

    klass = GWY_PALETTE_GET_CLASS(palette);
    g_hash_table_remove(klass->palettes, palette->def);
    gwy_debug("%s child GwyPaletteDef ref_count = %d",
              __FUNCTION__, G_OBJECT(palette->def)->ref_count);
    g_object_unref(palette->def);
    g_free(palette->color);
    g_free(palette->samples);
}

GObject*
gwy_palette_new(GwyPaletteDef *palette_def)
{
    GwyPalette *palette;
    GwyPaletteClass *klass;

    gwy_debug("%s", __FUNCTION__);

    g_return_val_if_fail(GWY_IS_PALETTE_DEF(palette_def), NULL);

    /* when g_type_class_peek() returns NULL we are constructing the very
     * first palette and thus no other can exist yet */
    if ((klass = g_type_class_peek(GWY_TYPE_PALETTE))
        && (palette = g_hash_table_lookup(klass->palettes, palette_def))) {
        g_object_ref(palette);
        return (GObject*)palette;
    }

    g_object_ref(palette_def);
    palette = g_object_new(GWY_TYPE_PALETTE, NULL);
    palette->nofvals = GWY_PALETTE_DEFAULT_SIZE;
    palette->color = g_new(GwyRGBA, palette->nofvals);
    palette->samples = g_new(guchar, 4*palette->nofvals);
    palette->def = palette_def;
    gwy_palette_recompute_table(palette);

    return (GObject*)(palette);
}

/**
 * gwy_palette_set_by_name:
 * @palette: A #GwyPalette.
 * @name: A palette definition name.
 *
 * Defines palette using palette definition of given name.
 *
 * Returns: %TURE if palette definition of given name existed, %FALSE
 *          on failure.
 **/
gboolean
gwy_palette_set_by_name(GwyPalette *palette,
                        const gchar *name)
{
    GwyPaletteDef *palette_def;

    g_return_val_if_fail(GWY_IS_PALETTE(palette), FALSE);

    if (!gwy_palette_def_exists(name))
        return FALSE;

    palette_def = (GwyPaletteDef*)gwy_palette_def_new(name);
    gwy_palette_set_palette_def(palette, palette_def);
    g_object_unref(palette_def);

    return TRUE;
}

static guchar*
gwy_palette_serialize(GObject *obj,
                      guchar *buffer,
                      gsize *size)
{
    GwyPalette *palette;

    gwy_debug("%s", __FUNCTION__);
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

    gwy_debug("%s", __FUNCTION__);
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_PALETTE_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        if (pdef)
            g_object_unref(pdef);
        return NULL;
    }

    palette = (GwyPalette*)gwy_palette_new(pdef);
    g_object_unref(pdef);

    return (GObject*)palette;
}



static void
gwy_palette_value_changed(GObject *palette)
{
    gwy_debug("signal: GwyPalette changed");
    g_signal_emit_by_name(GWY_PALETTE(palette), "value_changed", NULL);
}

/**
 * gwy_palette_set_palette_def:
 * @palette: palette we want to be set.
 * @palette_def: palette definition to be used.
 *
 * Sets the palette definition to @palette_def.
 **/
void
gwy_palette_set_palette_def(GwyPalette *palette,
                            GwyPaletteDef* palette_def)
{
    GwyPaletteDef *olddef;

    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_PALETTE(palette));
    g_return_if_fail(GWY_IS_PALETTE_DEF(palette_def));

    if (palette->def == palette_def)
        return;

    olddef = palette->def;
    g_object_ref(palette_def);
    palette->def = palette_def;
    gwy_palette_recompute_table(palette);
    g_object_unref(olddef);
    gwy_palette_value_changed(G_OBJECT(palette));
}

/**
 * gwy_palette_get_palette_def:
 * @palette: A #GwyPalette.
 *
 * Returns the palette definition this palette was created from.
 *
 * Returns: The palette definition.
 **/
GwyPaletteDef*
gwy_palette_get_palette_def(GwyPalette *palette)
{
    g_return_val_if_fail(GWY_IS_PALETTE(palette), NULL);
    return palette->def;
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
    GwyRGBA pe;

    gwy_debug("%s", __FUNCTION__);

    for (i = 0; i < palette->nofvals; i++) {
        pe = (gwy_palette_def_get_color(palette->def,
                                        i/(palette->nofvals - 1.0),
                                        GWY_INTERPOLATION_BILINEAR));
        palette->color[i] = pe;
    }
    palette->samples = gwy_palette_sample(palette,
                                          palette->nofvals,
                                          palette->samples);

    return 0;
}

/**
 * gwy_palette_sample:
 * @palette: palette to be sampled.
 * @size: Required sample size.
 * @oldsample: pointer to field to be filled.
 *
 * Fills the GdkPixbuf-like field of RRGGBBAA integer values representing
 * the palette.
 *
 * If @oldsample is not %NULL, it's resized to 4*@size bytes, otherwise it's
 * newly allocated.
 *
 * If you don't have a reason for specific sample size (or are not going
 * to modify the samples), think about using gwy_palette_get_samples() instead.
 *
 * Returns: The sampled palette.
 **/
guchar*
gwy_palette_sample(GwyPalette *palette, gint size, guchar *oldsample)
{
    gint i, k;
    gdouble cor;
    GwyRGBA pe;

    g_return_val_if_fail(GWY_IS_PALETTE(palette), NULL);
    g_return_val_if_fail(size > 1, NULL);

    oldsample = g_renew(guchar, oldsample, 4*size);

    k = 0;
    cor = 1.0/(size - 1.0);
    for (i = 0; i < size; i++) {
        pe = gwy_palette_def_get_color(palette->def, i*cor,
                                       GWY_INTERPOLATION_BILINEAR);

        oldsample[k++] = (guchar)(gint32)(MAX_CVAL*pe.r);
        oldsample[k++] = (guchar)(gint32)(MAX_CVAL*pe.g);
        oldsample[k++] = (guchar)(gint32)(MAX_CVAL*pe.b);
        oldsample[k++] = (guchar)(gint32)(MAX_CVAL*pe.a);
    }
    return oldsample;
}

const guchar*
gwy_palette_get_samples(GwyPalette *palette, gint *n_of_samples)
{
    *n_of_samples = palette->nofvals;
    return palette->samples;
}

const GwyRGBA*
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

    g_print("### palette #################################################\n");
    for (i = 0; i < palette->nofvals; i++) {
        g_print("%d : (%.3g %.3g %.3g %.3g)\n",
               i,
               palette->color[i].r, palette->color[i].g, palette->color[i].b,
               palette->color[i].a);
    }
    g_print("##############################################################\n");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
