/* @(#) $Id$ */

#ifndef __GWY_PALETTE_H__
#define __GWY_PALETTE_H__
#include <glib.h>
#include <libdraw/gwypalettedef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* FIXME: either change these to enum, or to strings and refernece palettes
 * by name instead of number */
typedef enum {
    GWY_PALETTE_GRAY,
    GWY_PALETTE_RED,
    GWY_PALETTE_GREEN,
    GWY_PALETTE_BLUE,
    GWY_PALETTE_YELLOW,
    GWY_PALETTE_PINK,
    GWY_PALETTE_OLIVE,
    GWY_PALETTE_BW1,
    GWY_PALETTE_BW2,
    GWY_PALETTE_RAINBOW1,
    GWY_PALETTE_RAINBOW2
} GwyPalettePreset;

#define GWY_TYPE_PALETTE                  (gwy_palette_get_type())
#define GWY_PALETTE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_PALETTE, GwyPalette))
#define GWY_PALETTE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_PALETTE, GwyPalette))
#define GWY_IS_PALETTE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_PALETTE))
#define GWY_IS_PALETTE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_PALETTE))
#define GWY_PALETTE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_PALETTE, GwyPalette))


/*Palette - generated table of (0-N) colors for quick use.
 Generating definition is included for easy reference/change*/
typedef struct{
    GObject parent_instance;

    GwyPaletteEntry *color; /*generated table of doubles*/
    GwyPaletteDef *def;     /*palette definition*/
    guchar *samples;        /*generated table of int32s*/
    gint nofvals;           /*maximum N (size of tables)*/
} GwyPalette;

typedef struct{
    GObjectClass parent_class;
} GwyPaletteClass;

GType gwy_palette_get_type  (void) G_GNUC_CONST;

GObject* gwy_palette_new(GwyPalettePreset preset);

/*fill the palette table using given definition*/
gint gwy_palette_setup(GwyPalette *palette, GwyPaletteDef *pdef);

/*fill the palette with predefined values*/
gint gwy_palette_setup_preset(GwyPalette *palette, GwyPalettePreset preset);

/*set palette definition*/
/* XXX: causes incosistency, don't export
 * void gwy_palette_set_def(GwyPalette *palette, GwyPaletteDef* b);*/

/*recompute table according to definition*/
/* XXX: expect incosistency, don't export
 * gint gwy_palette_recompute_table(GwyPalette *palette);*/

/*recompute definition according to table*/
gint gwy_palette_recompute_palette(GwyPalette *palette, gint istep);

/*get palette entry (usually direct access to entries will be preferred)*/
GwyPaletteEntry* gwy_palette_get_color(GwyPalette *palette, gint i);

/*set palette entry*/
gint gwy_palette_set_color(GwyPalette *palette, GwyPaletteEntry *val, gint i);

/*render Pixmap like palette for quick acces*/
guchar*                gwy_palette_sample         (GwyPalette *palette,
                                                   gint size,
                                                   guchar *oldpalette);

const guchar*          gwy_palette_get_samples    (GwyPalette *palette,
                                                   gint *n_of_samples);
const GwyPaletteEntry* gwy_palette_get_data       (GwyPalette *palette,
                                                   gint *n_of_data);

/*output to stdout (for debgging namely)*/
void gwy_palette_print(GwyPalette *palette);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /*__GWY_PALETTE_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
