
#include <stdio.h>
#include <math.h>
#include "gwypalette.h"

#define GWY_PALETTE_TYPE_NAME "GwyPalette"

static void  gwy_palette_class_init        (GwyPaletteClass *klass);
static void  gwy_palette_init              (GwyPalette *palette);
static void  gwy_palette_finalize          (GwyPalette *palette);
static void  gwy_palette_serializable_init (gpointer giface, gpointer iface_data);
static void  gwy_palette_watchable_init    (gpointer giface, gpointer iface_data);
static guchar* gwy_palette_serialize       (GObject *obj, guchar *buffer, gsize *size);
static GObject* gwy_palette_deserialize    (const guchar *buffer, gsize size, gsize *position);
static void  gwy_palette_value_changed     (GObject *GwyPalette);

gboolean gwy_palette_is_extreme(GwyPaletteEntry a, GwyPaletteEntry am, GwyPaletteEntry ap);
gboolean gwy_palette_is_bigslopechange(GwyPaletteEntry a, GwyPaletteEntry am, GwyPaletteEntry ap);

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
    g_free(palette->ints);
    g_array_free(palette->def->data, 0);
}

GObject*
gwy_palette_new(gdouble n)
{
    GwyPalette *palette;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    palette = g_object_new(GWY_TYPE_PALETTE, NULL);
    palette->nofvals = (gint)n;
    palette->color = (GwyPaletteEntry *) g_try_malloc(palette->nofvals*sizeof(GwyPaletteEntry));
    palette->ints = (guchar *) g_try_malloc(palette->nofvals*sizeof(guchar)*4);
    palette->def = (GwyPaletteDef*)gwy_palette_def_new(n);

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
    buffer = gwy_serialize_pack(buffer, size, "si",
                                GWY_PALETTE_TYPE_NAME, 0);
    {
        GwySerializeSpec spec[] = {
            { 'o', "pdef", &palette->def, NULL, },
        };
        gsize oldsize = *size;

        buffer = gwy_serialize_pack_struct(buffer, size,
                                           G_N_ELEMENTS(spec), spec);
        gwy_serialize_store_int32(buffer + oldsize - sizeof(guint32),
                                  *size - oldsize);
    }
    return buffer;
}

static GObject*
gwy_palette_deserialize(const guchar *buffer,
                         gsize size,
                         gsize *position)
{
    GwyPalette *palette;
    GwyPaletteDef *pdef;
    gsize pos, mysize;
    GwySerializeSpec spec[] = {
      { 'o', "pdef", &pdef, NULL, },
    };

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(buffer, NULL);

    pos = gwy_serialize_check_string(buffer, size, *position,
                                     GWY_PALETTE_TYPE_NAME);
    g_return_val_if_fail(pos, NULL);
    *position += pos;
    mysize = gwy_serialize_unpack_int32(buffer, size, position);

    gwy_serialize_unpack_struct(buffer + *position, mysize,
                                G_N_ELEMENTS(spec), spec);
    *position += mysize;

    palette = (GwyPalette*)gwy_palette_new((gint)pdef->n);
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
 * @a: palette we want to be set
 * @b: palette definition to be used
 *
 * Sets the palette definition.
 *
 * Makes a deep copy of @b and puts it into
 * palette structure. This function does not recompute the color tables
 * inside palette. Call gwy_palette_recompute_table() to do this afterwards
 * or use gwy_palette_setup() instead of this function.
 **/
void
gwy_palette_set_def(GwyPalette *a, GwyPaletteDef* b)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
     gwy_palette_def_copy(b, a->def);
}

/**
 * gwy_palette_recompute_table:
 * @a: palette to be recomputed
 *
 * Recomputes all the color tables inside palette.
 *
 * Call this function if you changed the palette definition
 * directly or by calling gwy_palette_set_def().
 *
 * Returns: 0 at success
 **/
gint
gwy_palette_recompute_table(GwyPalette *a)
{
    gint i;
    GwyPaletteEntry pe;
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    for (i=0; i<a->nofvals; i++)
    {
        pe = (gwy_palette_def_get_color(a->def, i, GWY_INTERPOLATION_BILINEAR));
        a->color[i] = pe;
    }
    a->ints = gwy_palette_int32_render(a, a->nofvals, a->ints);

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
 * @a: palette we want to change
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
gwy_palette_set_color(GwyPalette *a, GwyPaletteEntry *val, gint i)
{
    if (i<0 || i>=a->nofvals) { g_warning("Trying to reach value outside of palette.\n"); return 1;}
    a->color[i] = *val;
    gwy_palette_recompute_palette(a, 20);
    return 0;
}

gboolean
gwy_palette_is_extreme(GwyPaletteEntry a, GwyPaletteEntry am, GwyPaletteEntry ap)
{
    if ((a.r > am.r && a.r > ap.r) || (a.r < am.r && a.r < ap.r)) return 1;
    else if ((a.g > am.g && a.g > ap.g) || (a.g < am.g && a.g < ap.g)) return 1;
    else if ((a.b > am.b && a.b > ap.b) || (a.b < am.b && a.b < ap.b)) return 1;
    else if ((a.a > am.a && a.a > ap.a) || (a.a < am.a && a.a < ap.a)) return 1;
    else return 0;
}
gboolean
gwy_palette_is_bigslopechange(GwyPaletteEntry a, GwyPaletteEntry am, GwyPaletteEntry ap)
{
    gdouble tresh=10; /*treshold for large slope change*/
    if (fabs(fabs(ap.r - a.r)-fabs(am.r - a.r))>tresh) return 1;
    else if (fabs(fabs(ap.g - a.g)-fabs(am.g - a.g))>tresh) return 1;
    else if (fabs(fabs(ap.b - a.b)-fabs(am.b - a.b))>tresh) return 1;
    else if (fabs(fabs(ap.a - a.a)-fabs(am.a - a.a))>tresh) return 1;
    else return 0;
}

/**
 * gwy_palette_recompute_palette:
 * @a: palette which definition is to be recomputed
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
gwy_palette_recompute_palette(GwyPalette *a, gint istep)
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

    g_array_free(a->def->data, 0);
    a->def->data = g_array_new(0, 0, sizeof(GwyPaletteDefEntry));

    pd.x = 0; pd.color = a->color[0];
    gwy_palette_def_set_color(a->def, &pd);
    icount = 1;
    for (i=1; i<(a->nofvals-1); i++)
    {
        if (gwy_palette_is_extreme(a->color[i], a->color[i-1], a->color[i+1]) ||
            gwy_palette_is_bigslopechange(a->color[i], a->color[i-1], a->color[i+1]))
        {
            pd.x = (gdouble)i; pd.color = a->color[i];
            gwy_palette_def_set_color(a->def, &pd);
            icount = 1; continue;
        }
        else if (icount >= istep)
        {
            pd.x = (gdouble)i; pd.color = a->color[i];
            gwy_palette_def_set_color(a->def, &pd);
            icount = 1; continue;
        }
        icount++;
    }
    pd.x = a->nofvals-1; pd.color = a->color[a->nofvals-1];
    gwy_palette_def_set_color(a->def, &pd);

    return 0;
}

/**
 * gwy_palette_int32_render:
 * @a: palette to be rendered
 * @oldpal: pointer to field to be filled
 *
 * Fills the GPixmap-like field of integer values representing palette.
 *
 * Returns:
 **/
guchar*
gwy_palette_int32_render(GwyPalette *a, gint size, guchar *oldpal)
{
    gint i, k;
    gdouble cor;
    GwyPaletteEntry pe;

    if (oldpal==NULL)
    {
        oldpal = (guchar *) g_try_malloc(size*sizeof(guchar)*4);
    }
    else oldpal = (guchar *) g_try_realloc(oldpal, size*sizeof(guchar)*4);

    k = 0;
    cor = a->def->n/(gdouble)size;
    for (i=0; i<size; i++)
    {
        pe = gwy_palette_def_get_color(a->def, i*cor, GWY_INTERPOLATION_BILINEAR);

        oldpal[k++] = (guchar)((gint32) pe.r);
        oldpal[k++] = (guchar)((gint32) pe.g);
        oldpal[k++] = (guchar)((gint32) pe.b);
        oldpal[k++] = (guchar)((gint32) pe.a);
    }
    return oldpal;
}

/**
 * gwy_palette_setup_predef:
 * @a: palette to be filled
 * @pal: palette index
 *
 * Fills palette with some predefined colors
 *
 * Returns: 0 at success
 **/
gint
gwy_palette_setup_predef(GwyPalette *a, gint pal)
{
    GwyPaletteDefEntry cval;
    gint i;

    /*remove every previous items in the palette definition*/
    g_array_free(a->def->data, 0);
    a->def->data = g_array_new(0, 0, sizeof(GwyPaletteDefEntry));

    cval.color.r = 0; cval.color.g = 0; cval.color.b = 0; cval.color.a = 255; cval.x = 0;
    gwy_palette_def_set_color(a->def, &cval);
    cval.color.r = 255; cval.color.g = 255; cval.color.b = 255; cval.color.a = 255; cval.x = 511;
    gwy_palette_def_set_color(a->def, &cval);

    if (pal == GWY_PALETTE_GRAY)
    {

    }
    else if (pal == GWY_PALETTE_RED)
    {
        cval.color.r = 255; cval.color.g = 0; cval.color.b = 0; cval.color.a = 255; cval.x = 256;
        gwy_palette_def_set_color(a->def, &cval);
    }
    else if (pal == GWY_PALETTE_GREEN)
    {
        cval.color.r = 0; cval.color.g = 255; cval.color.b = 0; cval.color.a = 255; cval.x = 256;
        gwy_palette_def_set_color(a->def, &cval);
    }

    else if (pal == GWY_PALETTE_BLUE)
    {
        cval.color.r = 0; cval.color.g = 0; cval.color.b = 255; cval.color.a = 255; cval.x = 256;
        gwy_palette_def_set_color(a->def, &cval);
    }

    else if (pal == GWY_PALETTE_YELLOW)
    {
        cval.color.r = 212; cval.color.g = 183; cval.color.b = 42; cval.color.a = 255; cval.x = 256;
        gwy_palette_def_set_color(a->def, &cval);
    }

    else if (pal == GWY_PALETTE_PINK)
    {
        cval.color.r = 255; cval.color.g = 20; cval.color.b = 160; cval.color.a = 255; cval.x = 256;
        gwy_palette_def_set_color(a->def, &cval);
    }

    else if (pal == GWY_PALETTE_OLIVE)
    {
        cval.color.r = 94; cval.color.g = 176; cval.color.b = 117; cval.color.a = 255; cval.x = 256;
        gwy_palette_def_set_color(a->def, &cval);
    }

    else if (pal == GWY_PALETTE_BW1)
    {
        for (i=0; i<10; i++)
        {
            if (i%2==0) {cval.color.r = 255; cval.color.g = 255; cval.color.b = 255; cval.color.a = 255; }
            else {cval.color.r = 0; cval.color.g = 0; cval.color.b = 0; cval.color.a = 255;}
            cval.x = (gdouble)i*(256/10.0);
            gwy_palette_def_set_color(a->def, &cval);
        }
    }
    else if (pal == GWY_PALETTE_BW2)
    {
        for (i=0; i<10; i++)
        {
            if (i%2==0)
            {
                cval.color.r = 255; cval.color.g = 255; cval.color.b = 255; cval.color.a = 255;
                cval.x = (gdouble)i*(256/10.0);
                gwy_palette_def_set_color(a->def, &cval);
                cval.color.r = 0; cval.color.g = 0; cval.color.b = 0; cval.color.a = 255;
                cval.x = (gdouble)i*(256/10.0) + 0.1;
                gwy_palette_def_set_color(a->def, &cval);
            }
            else
            {
                cval.color.r = 0; cval.color.g = 0; cval.color.b = 0; cval.color.a = 255;
                cval.x = (gdouble)i*(256/10.0);
                gwy_palette_def_set_color(a->def, &cval);
                cval.color.r = 255; cval.color.g = 255; cval.color.b = 255; cval.color.a = 255;
                cval.x = (gdouble)i*(256/10.0) + 0.1;
                gwy_palette_def_set_color(a->def, &cval);
            }
        }
    }
    else if (pal == GWY_PALETTE_RAINBOW1)
    {
        cval.color.r = 255; cval.color.g = 0; cval.color.b = 0; cval.color.a = 255; cval.x = 64;
        gwy_palette_def_set_color(a->def, &cval);
        cval.color.r = 255; cval.color.g = 255; cval.color.b = 0; cval.color.a = 255; cval.x = 128;
        gwy_palette_def_set_color(a->def, &cval);
        cval.color.r = 0; cval.color.g = 255; cval.color.b = 255; cval.color.a = 255; cval.x = 192;
        gwy_palette_def_set_color(a->def, &cval);
        cval.color.r = 255; cval.color.g = 0; cval.color.b = 255; cval.color.a = 255; cval.x = 256;
        gwy_palette_def_set_color(a->def, &cval);
        cval.color.r = 0; cval.color.g = 255; cval.color.b = 0; cval.color.a = 255; cval.x = 320;
        gwy_palette_def_set_color(a->def, &cval);
        cval.color.r = 0; cval.color.g = 0; cval.color.b = 255; cval.color.a = 255; cval.x = 384;
        gwy_palette_def_set_color(a->def, &cval);
        cval.color.r = 128; cval.color.g = 128; cval.color.b = 128; cval.color.a = 255; cval.x = 448;
        gwy_palette_def_set_color(a->def, &cval);

    }
    else if (pal == GWY_PALETTE_RAINBOW2)
    {
        cval.color.r = 255; cval.color.g = 0; cval.color.b = 0; cval.color.a = 255; cval.x = 128;
        gwy_palette_def_set_color(a->def, &cval);
        cval.color.r = 0; cval.color.g = 255; cval.color.b = 0; cval.color.a = 255; cval.x = 256;
        gwy_palette_def_set_color(a->def, &cval);
        cval.color.r = 0; cval.color.g = 0; cval.color.b = 255; cval.color.a = 255; cval.x = 384;
        gwy_palette_def_set_color(a->def, &cval);

    }
    else {g_warning("Palette not implemented yet."); return 1;}

    gwy_palette_def_sort(a->def);
    return gwy_palette_recompute_table(a);;
}

/**
 * gwy_palette_print:
 * @a: palette to be outputted
 *
 * Debugging function that prints full palette color tables to stdout.
 *
 **/
void
gwy_palette_print(GwyPalette *a)
{
    gint i;
    printf("### palette (integer output) ##########################################\n");
    for (i=0; i<a->nofvals; i++)
    {
        printf("%d : (%d %d %d %d)\n", i, (gint)a->color[i].r, (gint)a->color[i].g, (gint)a->color[i].b, (gint)a->color[i].a);
    }
    printf("######################################################################\n");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
