
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
    buffer = gwy_serialize_pack(buffer, size, "s", GWY_PALETTE_TYPE_NAME);
    buffer = gwy_serializable_serialize((GObject *)palette->def, buffer, size);
    
    return buffer;
}

static GObject*
gwy_palette_deserialize(const guchar *stream,
                         gsize size,
                         gsize *position)
{
    GwyPalette *palette;
    GwyPaletteDef *pdef;
    gsize pos, fsize;
    guint n;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(stream, NULL);

    pos = gwy_serialize_check_string(stream, size, *position,
                                     GWY_PALETTE_TYPE_NAME);
    g_return_val_if_fail(pos, NULL);
    *position += pos;

    pdef = (GwyPaletteDef*) gwy_serializable_deserialize(stream, size, &pos);

    palette = (GwyPalette*)gwy_palette_new((gint)pdef->n);
    g_array_free(palette->def->data, 0);
    palette->def = pdef;

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

void 
gwy_palette_set_def(GwyPalette *a, GwyPaletteDef* b)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
     gwy_palette_def_copy(b, a->def); 
}

gint 
gwy_palette_recompute_table(GwyPalette *a)
{
    gint i;
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
 
    for (i=0; i<a->nofvals; i++)
	a->color[i] = gwy_palette_def_get_color(a->def, i, GWY_INTERPOLATION_BILINEAR);
     
    return 0;
}

void
gwy_palette_print(GwyPalette *a)
{
    gint i;
    printf("### palette (integer output) ##########################################\n");
    for (i=0; i<a->nofvals; i++)
    {
	printf("%d : (%d %d %d)\n", i, (gint)a->color[i].r, (gint)a->color[i].g, (gint)a->color[i].b);
    }
    printf("######################################################################\n");
}
