/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

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

typedef struct _GwyPalette      GwyPalette;
typedef struct _GwyPaletteClass GwyPaletteClass;

struct _GwyPalette {
    GObject parent_instance;

    GwyPaletteDef *def;     /* palette definition */
};

struct _GwyPaletteClass {
    GObjectClass parent_class;

    GHashTable *palettes;
};

GType            gwy_palette_get_type             (void) G_GNUC_CONST;
GObject*         gwy_palette_new                  (GwyPaletteDef *palette_def);
void             gwy_palette_set_palette_def      (GwyPalette *palette,
                                                   GwyPaletteDef *palette_def);
GwyPaletteDef*   gwy_palette_get_palette_def      (GwyPalette *palette);
gboolean         gwy_palette_set_by_name          (GwyPalette *palette,
                                                   const gchar *name);
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
