/* @(#) $Id$ */

#ifndef __GWY_PALETTE_H__
#define __GWY_PALETTE_H__

#include <glib.h>
#include <libdraw/gwypalettedef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_PALETTE                  (gwy_palette_get_type())
#define GWY_PALETTE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_PALETTE, GwyPalette))
#define GWY_PALETTE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_PALETTE, GwyPalette))
#define GWY_IS_PALETTE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_PALETTE))
#define GWY_IS_PALETTE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_PALETTE))
#define GWY_PALETTE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_PALETTE, GwyPaletteClass))


/*Palette - generated table of (0-N) colors for quick use.
 Generating definition is included for easy reference/change*/
typedef struct {
    GObject parent_instance;

    GwyRGBA *color;         /* generated table of doubles */
    GwyPaletteDef *def;     /* palette definition */
    guchar *samples;        /* generated table of int32s */
    gint nofvals;           /* maximum N (size of tables) */
} GwyPalette;

typedef struct{
    GObjectClass parent_class;

    /* Class data, for making the palettes singletons */
    GHashTable *palettes;
} GwyPaletteClass;

GType            gwy_palette_get_type             (void) G_GNUC_CONST;
GObject*         gwy_palette_new                  (GwyPaletteDef *palette_def);
void             gwy_palette_set_palette_def      (GwyPalette *palette,
                                                   GwyPaletteDef *palette_def);
GwyPaletteDef*   gwy_palette_get_palette_def      (GwyPalette *palette);
gboolean         gwy_palette_set_by_name          (GwyPalette *palette,
                                                   const gchar *name);
GwyRGBA*         gwy_palette_get_color            (GwyPalette *palette,
                                                   gint i);
G_CONST_RETURN
guchar*          gwy_palette_get_samples          (GwyPalette *palette,
                                                   gint *n_of_samples);
G_CONST_RETURN
GwyRGBA*         gwy_palette_get_data             (GwyPalette *palette,
                                                   gint *n_of_data);
guchar*          gwy_palette_sample               (GwyPalette *palette,
                                                   gint size,
                                                   guchar *oldsample);

/*output to stdout (for debgging namely)*/
void gwy_palette_print(GwyPalette *palette);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /*__GWY_PALETTE_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
