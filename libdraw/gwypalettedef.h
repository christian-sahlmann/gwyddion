/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_PALETTE_DEF_H__
#define __GWY_PALETTE_DEF_H__

#include <glib-object.h>

#include <libprocess/interpolation.h>
#include <libgwyddion/gwywatchable.h>
#include <libgwyddion/gwyserializable.h>

G_BEGIN_DECLS

#define GWY_TYPE_PALETTE_DEF                  (gwy_palette_def_get_type())
#define GWY_PALETTE_DEF(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_PALETTE_DEF, GwyPaletteDef))
#define GWY_PALETTE_DEF_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_PALETTE_DEF, GwyPaletteDef))
#define GWY_IS_PALETTE_DEF(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_PALETTE_DEF))
#define GWY_IS_PALETTE_DEF_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_PALETTE_DEF))
#define GWY_PALETTE_DEF_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_PALETTE_DEF, GwyPaletteDefClass))

#define GWY_PALETTE_BLUE         "Blue"
#define GWY_PALETTE_BLUE_CYAN    "Blue-Cyan"
#define GWY_PALETTE_BLUE_VIOLET  "Blue-Violet"
#define GWY_PALETTE_BLUE_YELLOW  "Blue-Yellow"
#define GWY_PALETTE_BW1          "BW1"
#define GWY_PALETTE_BW2          "BW2"
#define GWY_PALETTE_COLD         "Cold"
#define GWY_PALETTE_DFIT         "DFit"
#define GWY_PALETTE_GOLD         "Gold"
#define GWY_PALETTE_GRAY         "Gray"
#define GWY_PALETTE_GREEN        "Green"
#define GWY_PALETTE_GREEN_CYAN   "Green-Cyan"
#define GWY_PALETTE_GREEN_VIOLET "Green-Violet"
#define GWY_PALETTE_GREEN_YELLOW "Green-Yellow"
#define GWY_PALETTE_OLIVE        "Olive"
#define GWY_PALETTE_PINK         "Pink"
#define GWY_PALETTE_PM3D         "Pm3d"
#define GWY_PALETTE_RAINBOW1     "Rainbow1"
#define GWY_PALETTE_RAINBOW2     "Rainbow2"
#define GWY_PALETTE_RED          "Red"
#define GWY_PALETTE_RED_CYAN     "Red-Cyan"
#define GWY_PALETTE_RED_VIOLET   "Red-Violet"
#define GWY_PALETTE_RED_YELLOW   "Red-Yellow"
#define GWY_PALETTE_SPECTRAL     "Spectral"
#define GWY_PALETTE_WARM         "Warm"
#define GWY_PALETTE_YELLOW       "Yellow"

typedef struct _GwyPaletteDef      GwyPaletteDef;
typedef struct _GwyPaletteDefClass GwyPaletteDefClass;

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

struct _GwyPaletteDef {
    GObject parent_instance;

    gchar *name;
    GArray *data;                  /*color data*/
    gboolean has_alpha;      /*has alpha chanel?*/

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyPaletteDefClass {
    GObjectClass parent_class;

    GHashTable *palettes;
};

typedef void (*GwyPaletteDefFunc)(const gchar *name,
                                  GwyPaletteDef *palette_def,
                                  gpointer user_data);

GType         gwy_palette_def_get_type         (void) G_GNUC_CONST;
GObject*      gwy_palette_def_new              (const gchar *name);
GObject*      gwy_palette_def_new_as_copy      (GwyPaletteDef *src_palette_def);
G_CONST_RETURN
gchar*        gwy_palette_def_get_name         (GwyPaletteDef *palette_def);
gboolean      gwy_palette_def_set_name         (GwyPaletteDef *palette_def,
                                                const gchar *name);
gboolean      gwy_palette_def_exists           (const gchar *name);
void          gwy_palette_def_foreach          (GwyPaletteDefFunc callback,
                                                gpointer user_data);
GwyRGBA       gwy_palette_def_get_color        (GwyPaletteDef *palette_def,
                                                gdouble x,
                                                GwyInterpolationType interpolation);
void          gwy_palette_def_set_color        (GwyPaletteDef *palette_def,
                                                GwyPaletteDefEntry *val);
void          gwy_palette_def_set_from_samples (GwyPaletteDef *palette_def,
                                                GwyRGBA *samples,
                                                gint nsamples,
                                                gint istep);
void          gwy_palette_def_setup_presets    (void);

#define gwy_palette_def_is_set(pd) (GWY_PALETTE_DEF(pd)->data->len)

G_END_DECLS


#endif /*__GWY_PALETTE_H__*/
