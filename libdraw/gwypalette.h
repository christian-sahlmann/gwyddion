
#ifndef __GWY_PALETTE_H__
#define __GWY_PALETTE_H__
#include <glib.h>
#include "gwypalettedef.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
    
    GwyPaletteEntry *color;
    GwyPaletteDef *def;
    guchar *ints;
    gint nofvals;
} GwyPalette;

typedef struct{
    GObjectClass parent_class;
} GwyPaletteClass;

GType gwy_palette_get_type  (void) G_GNUC_CONST;

GObject* gwy_palette_new(gdouble nofvals);

gint gwy_palette_alloc(GwyPalette *a);

void gwy_palette_free(GwyPalette *a);

/*fill the palette table using given definition*/
gint gwy_palette_setup(GwyPalette *a, GwyPaletteDef *pdef);

/*set palette definition*/
void gwy_palette_set_def(GwyPalette *a, GwyPaletteDef* b);

/*recompute table according to definition*/
gint gwy_palette_recompute_table(GwyPalette *a);

/*recompute definition according to table*/
gint gwy_palette_recompute_palette(GwyPalette *a, gint istep);

/*get palette entry (usually direct access to entries will be preferred)*/
GwyPaletteEntry* gwy_palette_get_color(GwyPalette *a, gint i);

/*set palette entry*/
gint gwy_palette_set_color(GwyPalette *a, GwyPaletteEntry *val, gint i); 

/*render Pixmap like palette for quick acces*/
guchar* gwy_palette_int32_render(GwyPalette *a, guchar *oldpal);


/*output to stdout (for debgging namely)*/
void gwy_palette_print(GwyPalette *a);



    
#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /*__GWY_PALETTE_H__*/
