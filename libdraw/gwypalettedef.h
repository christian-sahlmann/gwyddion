/* @(#) $Id$ */

#ifndef __GWY_PALETTE_DEF_H__
#define __GWY_PALETTE_DEF_H__
#include <glib/ghash.h>
#include <glib-object.h>

#include <libprocess/interpolation.h>
#include <libgwyddion/gwywatchable.h>
#include <libgwyddion/gwyserializable.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_PALETTE_DEF                  (gwy_palette_def_get_type())
#define GWY_PALETTE_DEF(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_PALETTE_DEF, GwyPaletteDef))
#define GWY_PALETTE_DEF_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_PALETTE_DEF, GwyPaletteDef))
#define GWY_IS_PALETTE_DEF(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_PALETTE_DEF))
#define GWY_IS_PALETTE_DEF_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_PALETTE_DEF))
#define GWY_PALETTE_DEF_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_PALETTE_DEF, GwyPaletteDefClass))

#define GWY_PALETTE_GRAY      "Gray"
#define GWY_PALETTE_RED       "Red"
#define GWY_PALETTE_GREEN     "Green"
#define GWY_PALETTE_BLUE      "Blue"
#define GWY_PALETTE_YELLOW    "Yellow"
#define GWY_PALETTE_PINK      "Pink"
#define GWY_PALETTE_OLIVE     "Olive"
#define GWY_PALETTE_BW1       "BW1"
#define GWY_PALETTE_BW2       "BW2"
#define GWY_PALETTE_RAINBOW1  "Rainbow1"
#define GWY_PALETTE_RAINBOW2  "Rainbow2"

/*Palette entry (red/green/blue) in range of 0-255*/
/* XXX: this is wrong, the doubles should be in the range 0.0-1.0, only
 * rendered colors should have output bit depth */
typedef struct {
    gdouble r;
    gdouble g;
    gdouble b;
    gdouble a;
} GwyRGBA;

typedef struct {
    gdouble x;
    GwyRGBA color;
} GwyPaletteDefEntry;

/*Palette definition - x data (0-N) and corresponding colors*/
typedef struct{
    GObject parent_instance;

    gchar *name;
    GArray *data;                  /*color data*/
    gboolean has_alpha;		   /*has alpha chanel?*/
} GwyPaletteDef;

typedef struct{
    GObjectClass parent_class;

    /* Class data, for making the palettes singletons */
    GHashTable *palettes;
} GwyPaletteDefClass;

GType         gwy_palette_def_get_type         (void) G_GNUC_CONST;
GObject*      gwy_palette_def_new              (const gchar *name);
GObject*      gwy_palette_def_new_as_copy      (GwyPaletteDef *src_palette_def);
G_CONST_RETURN
gchar*        gwy_palette_def_get_name         (GwyPaletteDef *palette_def);
gboolean      gwy_palette_def_set_name         (GwyPaletteDef *palette_def,
                                                const gchar *name);
gboolean      gwy_palette_def_exists           (const gchar *name);
GwyRGBA       gwy_palette_def_get_color        (GwyPaletteDef *palette_def,
                                                gdouble x,
                                                gint interpolation);
void          gwy_palette_def_set_color        (GwyPaletteDef *palette_def,
                                                GwyPaletteDefEntry *val);
void          gwy_palette_def_set_from_samples (GwyPaletteDef *palette_def,
                                                GwyRGBA *samples,
                                                gint nsamples,
                                                gint istep);
void          gwy_palette_def_setup_presets    (void);

/*output (for debugging namely)*/
void gwy_palette_def_print(GwyPaletteDef *a);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /*__GWY_PALETTE_H__*/
