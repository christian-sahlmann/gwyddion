
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
    GwyPaletteEntry pe;
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
 
    for (i=0; i<a->nofvals; i++)
    {
	pe = (gwy_palette_def_get_color(a->def, i, GWY_INTERPOLATION_BILINEAR));
	a->color[i] = pe;
    }
    a->ints = gwy_palette_int32_render(a, a->ints);
	
    return 0;
}

gint gwy_palette_setup(GwyPalette *a, GwyPaletteDef *pdef)
{
    gwy_palette_set_def(a, pdef);
    return gwy_palette_recompute_table(a);
}

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
   
guchar* gwy_palette_int32_render(GwyPalette *a, guchar *oldpal)
{
    gint i, k;
    gint32 rval, gval, bval, aval;

    if (oldpal==NULL)
    {
    	oldpal = (guchar *) g_try_malloc(a->nofvals*sizeof(guchar)*4);
    }
    else oldpal = (guchar *) g_try_realloc(oldpal, a->nofvals*sizeof(guchar)*4); 
	
    k=0;
    for (i=0; i<a->nofvals; i++)
    {
	rval = (gint32) a->color[i].r;
	gval = (gint32) a->color[i].g;
	bval = (gint32) a->color[i].b;
	aval = (gint32) a->color[i].a;
	
	oldpal[k++] = (guchar)rval; 
	oldpal[k++] = (guchar)gval; 
	oldpal[k++] = (guchar)bval; 
	oldpal[k++] = (guchar)aval; 
    }
    return oldpal;
}

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
