/* @(#) $Id$ */

#include <string.h>
#include <math.h>

#include <libgwyddion/gwymacros.h>
#include "gwypalettedef.h"

#define GWY_PALETTE_DEF_TYPE_NAME "GwyPaletteDef"

static void     gwy_palette_def_class_init        (GwyPaletteDefClass *klass);
static void     gwy_palette_def_init              (GwyPaletteDef *palette_def);
static void     gwy_palette_def_finalize          (GwyPaletteDef *palette_def);
static void     gwy_palette_def_serializable_init (gpointer giface);
static void     gwy_palette_def_watchable_init    (gpointer giface);
static guchar*  gwy_palette_def_serialize         (GObject *obj,
                                                   guchar *buffer,
                                                   gsize *size);
static GObject* gwy_palette_def_deserialize       (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject* gwy_palette_def_duplicate         (GObject *object);
static void     gwy_palette_def_value_changed     (GObject *GwyPaletteDef);
static gint     gwy_palette_def_entry_compare     (GwyPaletteDefEntry *a,
                                                   GwyPaletteDefEntry *b);
static gchar*   gwy_palette_def_invent_name       (GHashTable *palettes,
                                                   const gchar *prefix);
static void     gwy_palette_def_create_preset     (GwyPaletteDefEntry *entries,
                                                   gint n,
                                                   const gchar *name);
static gboolean gwy_palette_is_big_slope_change   (GwyRGBA a,
                                                   GwyRGBA am,
                                                   GwyRGBA ap);
static gboolean gwy_palette_is_extreme            (GwyRGBA a,
                                                   GwyRGBA am,
                                                   GwyRGBA ap);

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
            (GInterfaceInitFunc)gwy_palette_def_serializable_init,
            NULL,
            NULL
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_palette_def_watchable_init,
            NULL,
            NULL
        };

        gwy_debug("%s", __FUNCTION__);
        gwy_palette_def_type = g_type_register_static(G_TYPE_OBJECT,
                                                   GWY_PALETTE_DEF_TYPE_NAME,
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
gwy_palette_def_serializable_init(gpointer giface)
{
    GwySerializableClass *iface = giface;

    gwy_debug("%s", __FUNCTION__);
    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_SERIALIZABLE);

    /* initialize stuff */
    iface->serialize = gwy_palette_def_serialize;
    iface->deserialize = gwy_palette_def_deserialize;
    iface->duplicate = gwy_palette_def_duplicate;
}

static void
gwy_palette_def_watchable_init(gpointer giface)
{
    GwyWatchableClass *iface = giface;

    gwy_debug("%s", __FUNCTION__);

    g_assert(G_TYPE_FROM_INTERFACE(iface) == GWY_TYPE_WATCHABLE);

    /* initialize stuff */
    iface->value_changed = NULL;
}

static void
gwy_palette_def_class_init(GwyPaletteDefClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("%s", __FUNCTION__);

    gobject_class->finalize = (GObjectFinalizeFunc)gwy_palette_def_finalize;
    /* static classes are never finalized, so this is never freed */
    klass->palettes = g_hash_table_new(g_str_hash, g_str_equal);
    /*XXX: too early gwy_palette_def_setup_presets(klass->palettes);*/
}

static void
gwy_palette_def_init(GwyPaletteDef *palette_def)
{
    gwy_debug("%s", __FUNCTION__);
    palette_def->data = NULL;
    palette_def->has_alpha = FALSE;
    palette_def->name = NULL;
}

static void
gwy_palette_def_finalize(GwyPaletteDef *palette_def)
{
    GwyPaletteDefClass *klass;
    gboolean removed;

    gwy_debug("%s (%s)", __FUNCTION__, palette_def->name);

    klass = GWY_PALETTE_DEF_GET_CLASS(palette_def);
    removed = g_hash_table_remove(klass->palettes, palette_def->name);
    g_assert(removed);
    g_array_free(palette_def->data, TRUE);
    g_free(palette_def->name);
}

/**
 * gwy_palette_def_new:
 * @name: Palette name.
 *
 * Returns a palette definition called @name.
 *
 * Palette definitions of the same name are singletons, thus if a palette
 * definitions called @name already exists, it is returned instead (with
 * reference count incremented).
 *
 * @name can be %NULL, a new unique name is invented then.
 *
 * A newly created palette definition is completely empty.
 *
 * Returns: The new palette definition as a #GObject.
 **/
GObject*
gwy_palette_def_new(const gchar *name)
{
    GwyPaletteDef *palette_def;
    GwyPaletteDefClass *klass;

    gwy_debug("%s", __FUNCTION__);

    /* when g_type_class_peek() returns NULL we are constructing the very
     * first palette definition and thus no other can exist yet */
    if ((klass = g_type_class_peek(GWY_TYPE_PALETTE_DEF))
        && name
        && (palette_def = g_hash_table_lookup(klass->palettes, name))) {
        g_object_ref(palette_def);
        return (GObject*)palette_def;
    }

    palette_def = g_object_new(GWY_TYPE_PALETTE_DEF, NULL);
    /* now it has to be defined */
    klass = g_type_class_peek(GWY_TYPE_PALETTE_DEF);
    g_assert(klass);
    palette_def->name = gwy_palette_def_invent_name(klass->palettes, name);
    palette_def->data = g_array_new(0, 0, sizeof(GwyPaletteDefEntry));
    g_hash_table_insert(klass->palettes, palette_def->name, palette_def);

    return (GObject*)(palette_def);
}


/**
 * gwy_palette_def_new_as_copy:
 * @src_palette_def: An existing #GwyPaletteDef.
 *
 * Creates a new palette definition as a copy of an existing one.
 *
 * A new name is invented based on the existing one.
 *
 * Returns: The new palette definition as a #GObject.
 **/
GObject*
gwy_palette_def_new_as_copy(GwyPaletteDef *src_palette_def)
{
    GwyPaletteDef *palette_def;
    guint i, ndat;

    gwy_debug("%s", __FUNCTION__);
    g_return_val_if_fail(GWY_IS_PALETTE_DEF(src_palette_def), NULL);

    /* make a deep copy */
    ndat = src_palette_def->data->len;
    palette_def = (GwyPaletteDef*)gwy_palette_def_new(src_palette_def->name);
    g_array_free(palette_def->data, FALSE);
    palette_def->data = g_array_sized_new(FALSE, FALSE,
                                          sizeof(GwyPaletteDefEntry), ndat);
    for (i = 0; i < ndat; i++) {
        GwyPaletteDefEntry pe = g_array_index(src_palette_def->data,
                                              GwyPaletteDefEntry, i);
        g_array_append_val(palette_def->data, pe);
    }

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

    gwy_debug("%s", __FUNCTION__);
    g_return_val_if_fail(GWY_IS_PALETTE_DEF(obj), NULL);

    palette_def = GWY_PALETTE_DEF(obj);
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
            { 's', "name", &palette_def->name, NULL, },
            { 'b', "has_alpha", &palette_def->has_alpha, NULL, },
            { 'D', "red", &rdat, &ndat, },
            { 'D', "green", &gdat, &ndat, },
            { 'D', "blue", &bdat, &ndat, },
            { 'D', "alpha", &adat, &ndat, },
            { 'D', "x", &xdat, &ndat, },
        };
        buffer = gwy_serialize_pack_object_struct(buffer, size,
                                                  GWY_PALETTE_DEF_TYPE_NAME,
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
    gint nrdat, ngdat, nbdat, nadat, nxdat, i;
    GwyPaletteDef *palette_def;
    GwyPaletteDefEntry pe;
    gdouble *rdat = NULL, *gdat = NULL, *bdat = NULL,
            *adat = NULL, *xdat = NULL;
    gboolean has_alpha = FALSE;
    gchar *name = NULL;
    GwySerializeSpec spec[] = {
      { 's', "name", &name, NULL, },
      { 'b', "has_alpha", &has_alpha, NULL, },
      { 'D', "red", &rdat, &nrdat, },
      { 'D', "green", &gdat, &ngdat, },
      { 'D', "blue", &bdat, &nbdat, },
      { 'D', "alpha", &adat, &nadat, },
      { 'D', "x", &xdat, &nxdat, },
    };
    gboolean exists;

    gwy_debug("%s", __FUNCTION__);
    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_PALETTE_DEF_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(rdat);
        g_free(gdat);
        g_free(bdat);
        g_free(adat);
        g_free(xdat);
        g_free(name);
        return NULL;
    }
    if (nrdat != ngdat || ngdat != nbdat || nbdat != nadat || nadat != nxdat) {
        g_critical("Serialized array component array sizes differ");
        g_free(rdat);
        g_free(gdat);
        g_free(bdat);
        g_free(adat);
        g_free(xdat);
        g_free(name);
        return NULL;
    }

    exists = gwy_palette_def_exists(name);
    palette_def = (GwyPaletteDef*)gwy_palette_def_new(name);
    if (exists) {
        g_warning("Deserializing existing palette %s, "
                  "who knows what will happen...",
                   name);
        g_array_set_size(palette_def->data, 0);
    }
    g_free(name);
    for (i = 0; i < nxdat; i++) {
        pe.x = xdat[i];
        pe.color.r = rdat[i];
        pe.color.g = gdat[i];
        pe.color.b = bdat[i];
        pe.color.a = adat[i];
        g_array_append_val(palette_def->data, pe);
    }
    palette_def->has_alpha = has_alpha;

    return (GObject*)palette_def;
}

static GObject*
gwy_palette_def_duplicate(GObject *object)
{
    g_return_val_if_fail(GWY_IS_PALETTE_DEF(object), NULL);
    g_object_ref(object);
    return object;
}

/**
 * gwy_palette_def_get_name:
 * @palette_def: A #GwyPaletteDef.
 *
 * Returns the name of palette definition @palette_def.
 *
 * Returns: The name. It should be considered constant and not modifier or
 *          freed.
 **/
G_CONST_RETURN gchar*
gwy_palette_def_get_name(GwyPaletteDef *palette_def)
{
    g_return_val_if_fail(GWY_IS_PALETTE_DEF(palette_def), NULL);
    return palette_def->name;
}

/**
 * gwy_palette_def_set_name:
 * @palette_def: A #GwyPaletteDef.
 * @name: A new name of the palette definition.
 *
 * Sets the name of a palette definition.
 *
 * This function fails when a palette definition of given name already
 * exists.
 *
 * Returns: Whether the rename was successfull.
 **/
gboolean
gwy_palette_def_set_name(GwyPaletteDef *palette_def,
                         const gchar *name)
{
    GwyPaletteDefClass *klass;
    gchar *oldname;

    g_return_val_if_fail(GWY_IS_PALETTE_DEF(palette_def), FALSE);
    g_return_val_if_fail(name, FALSE);

    klass = GWY_PALETTE_DEF_GET_CLASS(palette_def);
    if (name == palette_def->name)
        return TRUE;
    if (g_hash_table_lookup(klass->palettes, name))
        return FALSE;

    oldname = palette_def->name;
    g_hash_table_steal(klass->palettes, palette_def->name);
    palette_def->name = g_strdup(name);
    g_hash_table_insert(klass->palettes, palette_def->name, palette_def);
    g_free(oldname);
    gwy_palette_def_value_changed(G_OBJECT(palette_def));

    return TRUE;
}


/**
 * gwy_palette_def_value_changed:
 * @palette_def: A #GwyPaletteDef.
 *
 * Emits a "value_changed" signal on a palette definition @palette_def.
 **/
static void
gwy_palette_def_value_changed(GObject *palette_def)
{
    gwy_debug("signal: GwyPaletteDef changed");
    g_signal_emit_by_name(GWY_PALETTE_DEF(palette_def), "value_changed", NULL);
}

/**
 * gwy_palette_def_get_color:
 * @palette_def: palette definition of interest
 * @x: position (0-N)
 * @interpolation: interpolation method
 *
 * Finds the color at position @x between 0 and gwy_palette_def_get_n().
 *
 * Interpolates between palette definition entries closest to @x
 * and returns the resulting color.
 *
 * Returns: the color sample.
 **/
GwyRGBA
gwy_palette_def_get_color(GwyPaletteDef *palette_def,
                          gdouble x,
                          GwyInterpolationType interpolation)
{
    GwyRGBA ret;
    GwyPaletteDefEntry pe, pf;
    guint i;
    gdouble rlow, rhigh, blow, bhigh, glow, ghigh, alow, ahigh, xlow, xhigh;

    rlow = rhigh = blow = bhigh = glow = ghigh = alow = ahigh = xlow = xhigh = 0;

    if (x < 0.0 || x > 1.0) {
        g_warning("Trying to reach value outside of palette.");
        return ret;
    }

    /*find the closest color index*/
    for (i = 0; i < palette_def->data->len-1; i++) {
        pe = g_array_index(palette_def->data, GwyPaletteDefEntry, i);
        pf = g_array_index(palette_def->data, GwyPaletteDefEntry, i+1);
        if (pe.x == x) {
            ret.r = pe.color.r;
            ret.g = pe.color.g;
            ret.b = pe.color.b;
            ret.a = pe.color.a;
            return ret;
        }
        else if (pf.x == x) {
            ret.r = pf.color.r;
            ret.g = pf.color.g;
            ret.b = pf.color.b;
            ret.a = pf.color.a;
            return ret;
        }
        else if (pe.x < x && pf.x > x) {
            rlow = pe.color.r; rhigh = pf.color.r;
            glow = pe.color.g; ghigh = pf.color.g;
            blow = pe.color.b; bhigh = pf.color.b;
            alow = pe.color.a; ahigh = pf.color.a;
            xlow = pe.x; xhigh = pf.x;
            break;
        }
    }

    /*interpolate the result*/
    ret.r = gwy_interpolation_get_dval(x, xlow, rlow, xhigh, rhigh,
                                       interpolation);
    ret.g = gwy_interpolation_get_dval(x, xlow, glow, xhigh, ghigh,
                                       interpolation);
    ret.b = gwy_interpolation_get_dval(x, xlow, blow, xhigh, bhigh,
                                       interpolation);
    ret.a = gwy_interpolation_get_dval(x, xlow, alow, xhigh, ahigh,
                                       interpolation);

    return ret;
}

/**
 * gwy_palette_def_set_color:
 * @palette_def: A palette definition to be changed.
 * @val: An entry to be added.
 *
 * Adds an entry to palette definition.
 **/
void
gwy_palette_def_set_color(GwyPaletteDef *palette_def,
                          GwyPaletteDefEntry *val)
{
    gwy_debug("%s", __FUNCTION__);

    g_return_if_fail(val->x < 0.0 || val->x > 1.0);
    palette_def->data = g_array_append_val(palette_def->data, *val);
    g_array_sort(palette_def->data,
                 (GCompareFunc)gwy_palette_def_entry_compare);
    gwy_palette_def_value_changed(G_OBJECT(palette_def));
}

static gint
gwy_palette_def_entry_compare(GwyPaletteDefEntry *a,
                              GwyPaletteDefEntry *b)
{
    if (a->x < b->x)
        return -1;
    if (a->x == b->x)
        return 0;
    return 1;
}

static gchar*
gwy_palette_def_invent_name(GHashTable *palettes,
                            const gchar *prefix)
{
    gchar *str;
    gint n, i;

    if (!prefix)
        prefix = _("Untitled");
    n = strlen(prefix);
    str = g_new(gchar, n + 9);
    strcpy(str, prefix);
    if (!g_hash_table_lookup(palettes, str))
        return str;
    for (i = 1; i < 100000; i++) {
        g_snprintf(str + n, 9, "%d", i);
        if (!g_hash_table_lookup(palettes, str))
            return str;
    }
    g_assert_not_reached();
    return NULL;
}

static void
gwy_palette_def_create_preset(GwyPaletteDefEntry *entries,
                              gint n,
                              const gchar *name)
{
    GwyPaletteDef *palette_def;
    gint i;

    palette_def = (GwyPaletteDef*)gwy_palette_def_new(name);
    for (i = 0; i < n; i++)
        g_array_append_val(palette_def->data, entries[i]);
}

/**
 * gwy_palette_def_setup_presets:
 *
 * Set up built-in palette definitions.  To be used in Gwyddion initialization
 * and eventually replaced by loading palette definitions from external files.
 **/
void
gwy_palette_def_setup_presets(void)
{
    GwyPaletteDefEntry gray[] = {
        { 0.0, { 0, 0, 0, 1 } },
        { 1.0, { 1, 1, 1, 1 } },
    };
    GwyRGBA black = { 0, 0, 0, 1 };
    GwyRGBA white = { 1, 1, 1, 1 };
    GwyRGBA red = { 1, 0, 0, 1 };
    GwyRGBA green = { 0, 1, 0, 1 };
    GwyRGBA blue = { 0, 0, 1, 1 };
    GwyRGBA yellow = { 0.8314, 0.71765, 0.16471, 1 };
    GwyRGBA pink = { 1, 0.07843, 0.62745, 1 };
    GwyRGBA olive = { 0.36863, 0.69020, 0.45882, 1 };
    GwyPaletteDefEntry rainbow1[] = {
        { 0.0,   { 0, 0, 0, 1 } },
        { 0.125, { 1, 0, 0, 1 } },
        { 0.25,  { 1, 1, 0, 1 } },
        { 0.375, { 0, 1, 1, 1 } },
        { 0.5,   { 1, 0, 1, 1 } },
        { 0.625, { 0, 1, 0, 1 } },
        { 0.75,  { 0, 0, 1, 1 } },
        { 0.875, { 0.5, 0.5, 0.5, 1 } },
        { 1.0,   { 1, 1, 1, 1 } },
    };
    GwyPaletteDefEntry rainbow2[] = {
        { 0.0,  { 0, 0, 0, 1 } },
        { 0.25, { 1, 0, 0, 1 } },
        { 0.5,  { 0, 1, 0, 1 } },
        { 0.75, { 0, 0, 1, 1 } },
        { 1.0,  { 1, 1, 1, 1 } },
    };

    GwyPaletteDefEntry pd[] = {
        { 0.0, { 0, 0, 0, 1 } },
        { 0.5, { 0, 0, 0, 0 } },
        { 1.0, { 1, 1, 1, 1 } },
    };
    GwyPaletteDefEntry *pd2;
    gsize i;

    gwy_palette_def_create_preset(gray, G_N_ELEMENTS(gray), GWY_PALETTE_GRAY);
    gwy_palette_def_create_preset(rainbow1, G_N_ELEMENTS(rainbow1),
                                  GWY_PALETTE_RAINBOW1);
    gwy_palette_def_create_preset(rainbow2, G_N_ELEMENTS(rainbow2),
                                  GWY_PALETTE_RAINBOW2);

    pd[1].color = red;
    gwy_palette_def_create_preset(pd, G_N_ELEMENTS(pd), GWY_PALETTE_RED);

    pd[1].color = green;
    gwy_palette_def_create_preset(pd, G_N_ELEMENTS(pd), GWY_PALETTE_GREEN);

    pd[1].color = blue;
    gwy_palette_def_create_preset(pd, G_N_ELEMENTS(pd), GWY_PALETTE_BLUE);

    pd[1].color = yellow;
    gwy_palette_def_create_preset(pd, G_N_ELEMENTS(pd), GWY_PALETTE_YELLOW);

    pd[1].color = pink;
    gwy_palette_def_create_preset(pd, G_N_ELEMENTS(pd), GWY_PALETTE_PINK);

    pd[1].color = olive;
    gwy_palette_def_create_preset(pd, G_N_ELEMENTS(pd), GWY_PALETTE_OLIVE);

    pd2 = g_new(GwyPaletteDefEntry, 10);
    for (i = 0; i < 10; i++) {
        pd2[i].x = i/9.0;
        pd2[i].color = i%2 ? black : white;
    }
    gwy_palette_def_create_preset(pd2, 10, GWY_PALETTE_BW1);
    g_free(pd2);

    pd2 = g_new(GwyPaletteDefEntry, 20);
    pd2[0].x = 0.0;
    pd2[0].color = black;
    for (i = 1; i < 19; i++) {
        pd2[i].x = (i/2 + i%2)/10.0 + (i%2 ? -0.01 : 0.01);
        pd2[i].color = i/2%2 ? white : black;
    }
    pd2[19].x = 1.0;
    pd2[19].color = white;
    gwy_palette_def_create_preset(pd2, 20, GWY_PALETTE_BW2);
    g_free(pd2);
}

static gboolean
gwy_palette_is_extreme(GwyRGBA a,
                       GwyRGBA am,
                       GwyRGBA ap)
{
    if ((a.r > am.r && a.r > ap.r)
        || (a.r < am.r && a.r < ap.r)
        || (a.g > am.g && a.g > ap.g)
        || (a.g < am.g && a.g < ap.g)
        || (a.b > am.b && a.b > ap.b)
        || (a.b < am.b && a.b < ap.b)
        || (a.a > am.a && a.a > ap.a)
        || (a.a < am.a && a.a < ap.a))
        return 1;
    return 0;
}

static gboolean
gwy_palette_is_big_slope_change(GwyRGBA a,
                                GwyRGBA am,
                                GwyRGBA ap)
{
    gdouble tresh = 10; /*treshold for large slope change*/

    if (fabs(fabs(ap.r - a.r) - fabs(am.r - a.r)) > tresh
        || fabs(fabs(ap.g - a.g) - fabs(am.g - a.g)) > tresh
        || fabs(fabs(ap.b - a.b) - fabs(am.b - a.b)) > tresh
        || fabs(fabs(ap.a - a.a) - fabs(am.a - a.a)) > tresh)
        return 1;
    return 0;
}

/**
 * gwy_palette_def_set_from_samples:
 * @palette_def: A #GwyPaletteDef to be computed.
 * @samples: Color data to ctreate the definition from.
 * @nsamples: The number of samples in @samples.
 * @istep: Maximum distance of definition entries, 0 means unlimited.
 *
 * Fits the palette definition to given color samples.
 *
 * It finds the extrema points inside palette color tables and
 * tries to setup the palette definition to fit the color tables.
 * This fucntion should be used only when there is a reason
 * for doing this, for example after some graphical entry
 * of palette color table values. Parameter @istep controls
 * the precision of the linear fit.
 *
 * However, the precise algorithm may change in the future.
 **/
void
gwy_palette_def_set_from_samples(GwyPaletteDef *palette_def,
                                 GwyRGBA *samples,
                                 gint nsamples,
                                 gint istep)
{
    /*local extremes will be used as palette definition entries*/
    /*if there is no local extrema within a given number of points,
     an entry will be added anyway to make sure that convex/concave
     palettes will be fitted with enough precision*/
    gint i, icount, n;
    GwyPaletteDefEntry pd;

    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_PALETTE_DEF(palette_def));
    g_return_if_fail(samples);
    g_return_if_fail(istep >= 0);
    g_return_if_fail(nsamples >= 2);

    if (!istep)
        istep = G_MAXINT;
    palette_def->data = g_array_set_size(palette_def->data, 0);

    n = 0;
    pd.x = 0.0;
    pd.color = samples[0];
    palette_def->data = g_array_append_val(palette_def->data, pd);
    icount = 1;
    for (i = 1; i < nsamples-1; i++) {
        if (gwy_palette_is_extreme(samples[i], samples[i-1], samples[i+1])
            || gwy_palette_is_big_slope_change(samples[i], samples[i-1],
                                               samples[i+1])) {
            pd.x = i/(nsamples - 1.0);
            pd.color = samples[i];
            palette_def->data = g_array_append_val(palette_def->data, pd);
            icount = 1;
        }
        else if (icount >= istep) {
            pd.x = i/(nsamples - 1.0);
            pd.color = samples[i];
            palette_def->data = g_array_append_val(palette_def->data, pd);
            icount = 1;
        }
        else
            icount++;
    }
    if (icount > 1) {
        pd.x = 1.0;
        pd.color = samples[nsamples-1];
        palette_def->data = g_array_append_val(palette_def->data, pd);
    }
    gwy_palette_def_value_changed(G_OBJECT(palette_def));
}

/**
 * gwy_palette_def_exists:
 * @name: A palette definition name.
 *
 * Tests whether a palette definition of given name exists.
 *
 * Returns: %TRUE if such a palette definition exists, %FALSE otherwise.
 **/
gboolean
gwy_palette_def_exists(const gchar *name)
{
    GwyPaletteDefClass *klass;

    g_return_val_if_fail(name, FALSE);
    klass = g_type_class_peek(GWY_TYPE_PALETTE_DEF);
    g_return_val_if_fail(klass, FALSE);
    return g_hash_table_lookup(klass->palettes, name) != 0;
}

/**
 * GwyPaletteDefFunc:
 * @name: Palette definition name.
 * @palette_def: Palette definition.
 * @user_data: A user-specified pointer.
 *
 * Callback function type for gwy_palette_def_foreach().
 **/

/**
 * gwy_palette_def_foreach:
 * @callback: A callback.
 * @user_data: User data passed to the callback.
 *
 * Runs @callback for each existing palette definition.
 **/
void
gwy_palette_def_foreach(GwyPaletteDefFunc callback,
                        gpointer user_data)
{
    GwyPaletteDefClass *klass;

    klass = g_type_class_peek(GWY_TYPE_PALETTE_DEF);
    g_hash_table_foreach(klass->palettes, (GHFunc)callback, user_data);
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

    g_print("#### palette def ##############################\n");
    g_print("%d palette entries in range (0.0-1.0).\n", ndat);
    for (i = 0; i < ndat; i++) {
        pe = &g_array_index(pd, GwyPaletteDefEntry, i);
        g_print("Palette entry %d: (%f %f %f %f) at %f\n",
                i, pe->color.r, pe->color.g, pe->color.b, pe->color.a, pe->x);
    }
    g_print("###############################################\n");

}

/************************** Documentation ****************************/

/**
 * GwyRGBA:
 * @r: The red component.
 * @g: The green component.
 * @b: The blue component.
 * @a: The alpha (opacity) value.
 *
 * RGB[A] color specification type.
 *
 * All values are from the range [0,1].
 **/

/**
 * GwyPaletteDefEntry:
 * @x: Position of @color in the palette definition.
 * @color: The color at position @x.
 *
 * Palette definition entry type.
 *
 * The @x coordinate is from the range [0,1].
 **/

/**
 * GWY_PALETTE_GRAY:
 *
 * A gray palette.
 *
 * This is the default palette.
 **/

/**
 * GWY_PALETTE_RED:
 *
 * A red palette.
 **/

/**
 * GWY_PALETTE_GREEN:
 *
 * A green palette.
 **/

/**
 * GWY_PALETTE_BLUE:
 *
 * A blue palette.
 **/

/**
 * GWY_PALETTE_YELLOW:
 *
 * A yellowi palette.
 **/

/**
 * GWY_PALETTE_PINK:
 *
 * A pink palette.
 **/

/**
 * GWY_PALETTE_OLIVE:
 *
 * A plive palette.
 **/

/**
 * GWY_PALETTE_BW1:
 *
 * A striped black-and-white palette with blended stripes.
 **/

/**
 * GWY_PALETTE_BW2:
 *
 * A striped black-and-white palette with sharp stripes.
 **/

/**
 * GWY_PALETTE_RAINBOW1:
 *
 * A colorful rainbow palette.
 **/

/**
 * GWY_PALETTE_RAINBOW2:
 *
 * A black-red-green-blue-white palette.
 **/

/**
 * gwy_palette_def_is_set:
 * @pd: A palette definition.
 *
 * Expands to %TRUE if the palette definition contains any colors.
 *
 * XXX: This is flawed. Unset palette definitions should not exist, though
 * currently gwy_palette_def_new() creates them.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
