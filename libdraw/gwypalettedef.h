
#ifndef __GWY_PALETTEDEF_H__
#define __GWY_PALETTEDEF_H__
#include <glib-object.h>

#include <libprocess/interpolation.h>
#include <libgwyddion/gwywatchable.h>
#include <libgwyddion/gwyserializable.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_PALETTEDEF                  (gwy_palette_def_get_type())
#define GWY_PALETTEDEF(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_PALETTEDEF, GwyPaletteDef))
#define GWY_PALETTEDEF_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_PALETTEDEF, GwyPaletteDef))
#define GWY_IS_PALETTEDEF(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_PALETTEDEF))
#define GWY_IS_PALETTEDEF_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_PALETTEDEF))
#define GWY_PALETTEDEF_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_PALETTEDEF, GwyPaletteDef))
   
    
/*Palette entry (red/green/blue) in range of 0-255*/
typedef struct{
    gdouble r;
    gdouble g;
    gdouble b;
    gdouble a;
} GwyPaletteEntry;

typedef struct{
    gdouble x;
    GwyPaletteEntry color;
} GwyPaletteDefEntry;

/*Palette definition - x data (0-N) and corresponding colors*/
typedef struct{
    GObject parent_instance;

    GArray *data;                  /*color data*/
    gdouble n;                     /*maximum possible n*/
    gboolean has_alpha;		   /*has alpha chanel?*/
} GwyPaletteDef;

typedef struct{
    GObjectClass parent_class;
} GwyPaletteDefClass;

GType gwy_palette_def_get_type  (void) G_GNUC_CONST;

GObject* gwy_palette_def_new(gdouble n);

void gwy_palette_def_copy(GwyPaletteDef *a, GwyPaletteDef *b);

gint gwy_palette_def_get_n(GwyPaletteDef *a);

/*get color at given point between (0-n)*/
GwyPaletteEntry gwy_palette_def_get_color(GwyPaletteDef *a, gdouble x, gint interpolation);

/*set color at given point between (0-n) (add or change palette entry) and resort if necessary*/
gint gwy_palette_def_set_color(GwyPaletteDef *a, GwyPaletteDefEntry *val);

/*sort palette entries*/
void gwy_palette_def_sort(GwyPaletteDef *a);

/*output (for debugging namely)*/
void gwy_palette_def_print(GwyPaletteDef *a);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /*__GWY_PALETTE_H__*/
