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

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkiconfactory.h>
#include <gdk/gdkkeysyms.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include "gwystock.h"

static void           register_toolbox_icons    (const gchar *pixmap_path,
                                                 GtkIconFactory *icon_factory);
static gchar*         guess_pixmap_path         (void);
static void           slurp_icon_directory      (const gchar *path,
                                                 GHashTable *icons);
static void           register_icon_set_list_cb (const gchar *id,
                                                 GList *list,
                                                 GtkIconFactory *factory);
static GtkIconSource* file_to_icon_source       (const gchar *path,
                                                 const gchar *filename,
                                                 gchar **id);
static void           free_the_icon_factory     (void);

static GtkIconFactory *the_icon_factory = NULL;

/**
 * gwy_stock_register_stock_items:
 *
 * Registers stock items.
 *
 * This function must be called before any stock items are used.
 **/
void
gwy_stock_register_stock_items(void)
{
    gchar *pixmap_path;

    g_return_if_fail(!the_icon_factory);
    gtk_icon_size_register(GWY_ICON_SIZE_ABOUT, 60, 60);
    pixmap_path = guess_pixmap_path();
    if (!pixmap_path) {
        g_warning("Cannot find directory with stock icons.");
        return;
    }
    the_icon_factory = gtk_icon_factory_new();
    register_toolbox_icons(pixmap_path, the_icon_factory);
    gtk_icon_factory_add_default(the_icon_factory);
    g_free(pixmap_path);
    g_atexit(free_the_icon_factory);
}

static void
register_toolbox_icons(const gchar *pixmap_path,
                       GtkIconFactory *icon_factory)
{
    GHashTable *icons;

    icons = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    slurp_icon_directory(pixmap_path, icons);
    g_hash_table_foreach(icons, (GHFunc)register_icon_set_list_cb,
                         icon_factory);
    g_hash_table_destroy(icons);
}

static gchar*
guess_pixmap_path(void)
{
    gchar *p;

    p = gwy_find_self_dir("pixmaps");
    if (g_file_test(p, G_FILE_TEST_IS_DIR)) {
        gwy_debug("Icon path: %s", p);
        return p;
    }

    return NULL;
}

/* XXX: not only registers the icons but also frees the list */
static void
register_icon_set_list_cb(const gchar *id,
                          GList *list,
                          GtkIconFactory *factory)
{
    GtkIconSet *icon_set;
    GtkIconSource *icon_source, *largest;
    GtkIconSize gtksize;
    GList *l;
    gint max, w, h;

    icon_set = gtk_icon_set_new();
    max = 0;
    largest = NULL;
    for (l = list; l; l = g_list_next(l)) {
        icon_source = (GtkIconSource*)l->data;
        gtksize = gtk_icon_source_get_size(icon_source);
        g_assert(gtk_icon_size_lookup(gtksize, &w, &h));
        if (w*h > max)
            largest = icon_source;
    }
    if (!largest) {
        g_warning("No icon of nonzero size in the set");
        return;
    }
    gtk_icon_source_set_size_wildcarded(largest, TRUE);

    for (l = list; l; l = g_list_next(l)) {
        icon_source = (GtkIconSource*)l->data;
        gtk_icon_set_add_source(icon_set, icon_source);
        gtk_icon_source_free(icon_source);
    }
    gtk_icon_factory_add(factory, id, icon_set);
    g_list_free(list);
}

static void
slurp_icon_directory(const gchar *path,
                     GHashTable *icons)
{
    GtkIconSource *icon_source;
    GDir *gdir = NULL;
    GError *err = NULL;
    const gchar *filename;
    GList *list;
    gchar *id;

    gdir = g_dir_open(path, 0, &err);
    if (!gdir) {
        g_warning("Cannot open directory `%s': %s", path, err->message);
        return;
    }

    while ((filename = g_dir_read_name(gdir))) {
        icon_source = file_to_icon_source(path, filename, &id);
        if (!icon_source) {
            g_free(id);
            id = NULL;
            continue;
        }
        list = (GList*)g_hash_table_lookup(icons, id);
        list = g_list_append(list, icon_source);
        g_hash_table_replace(icons, id, list);
    }
    g_dir_close(gdir);
}

/**
 * Filename format: <gwy_foobar>-<size>[.<state>].png
 **/
static GtkIconSource*
file_to_icon_source(const gchar *path,
                    const gchar *filename,
                    gchar **id)
{
    static struct { gchar letter; GtkStateType state; }
    const state_letters[] = {
        { 'n', GTK_STATE_NORMAL },
        { 'a', GTK_STATE_ACTIVE },
        { 'p', GTK_STATE_PRELIGHT },
        { 's', GTK_STATE_SELECTED },
        { 'i', GTK_STATE_INSENSITIVE },
    };
    /* FIXME: Of course, this is conceptually wrong.  however some guess is
     * better than nothing when we have more than one size of the same icon */
    static struct { gint size; GtkIconSize gtksize; }
    gtk_sizes[] = {
        { 16, GTK_ICON_SIZE_MENU },
        { 18, GTK_ICON_SIZE_SMALL_TOOLBAR },
        { 20, GTK_ICON_SIZE_BUTTON },
        { 24, GTK_ICON_SIZE_LARGE_TOOLBAR },
        { 32, GTK_ICON_SIZE_DND },
        { 48, GTK_ICON_SIZE_DIALOG },
        { 60, 0 },
    };
    GtkIconSource *icon_source;
    GtkIconSize gtksize = -1;
    GtkStateType state = -1;
    gchar *sz, *st, *p, *fullpath;
    gint size;
    gsize i;

    if (!gtk_sizes[G_N_ELEMENTS(gtk_sizes)-1].gtksize) {
        gtk_sizes[G_N_ELEMENTS(gtk_sizes)-1].gtksize
            = gtk_icon_size_from_name(GWY_ICON_SIZE_ABOUT);
    }
    *id = g_strdup(filename);
    if (!g_str_has_suffix(filename, ".png"))
        return NULL;
    if (!(sz = strchr(*id, '-')))
        return NULL;
    *sz = '\0';
    sz++;
    if (!(st = strchr(sz, '.')))
        return NULL;
    *st = '\0';
    st++;
    if ((p = strchr(st, '.')))
        *p = '\0';
    else
        st = NULL;
    size = atoi(sz);
    if (size < 0)
        return NULL;

    if (st) {
        if (strlen(st) != 1)
            return NULL;
        for (i = 0; i < G_N_ELEMENTS(state_letters); i++) {
            if (st[0] == state_letters[i].letter) {
                state = state_letters[i].state;
                break;
            }
        }
    }

    for (i = 0; i < G_N_ELEMENTS(gtk_sizes); i++) {
        if (gtk_sizes[i].size == size) {
            gtksize = gtk_sizes[i].gtksize;
            break;
        }
        if (gtk_sizes[i].size > size) {
            if (!i)
                gtksize = gtk_sizes[i].gtksize;
            else if (size*size > gtk_sizes[i-1].size*gtk_sizes[i].size)
                gtksize = gtk_sizes[i].gtksize;
            else
                gtksize = gtk_sizes[i-1].gtksize;
            break;
        }
    }
    if (gtksize == (GtkIconSize)-1)
        gtksize = gtk_sizes[G_N_ELEMENTS(gtk_sizes)-1].gtksize;

    icon_source = gtk_icon_source_new();
    fullpath = g_build_filename(path, filename, NULL);
    gtk_icon_source_set_filename(icon_source, fullpath);
    g_free(fullpath);
    gtk_icon_source_set_size(icon_source, gtksize);
    gtk_icon_source_set_direction_wildcarded(icon_source, TRUE);
    gtk_icon_source_set_size_wildcarded(icon_source, FALSE);
    if (state != (GtkStateType)-1) {
        gtk_icon_source_set_state_wildcarded(icon_source, FALSE);
        gtk_icon_source_set_state(icon_source, state);
    }

    return icon_source;
}

static void
free_the_icon_factory(void)
{
    gwy_object_unref(the_icon_factory);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwystock
 * @title: gwystock
 * @short_description: Stock icons
 *
 * Use gwy_stock_register_stock_items() to register stock icons.
 **/

/**
 * GWY_ICON_SIZE_ABOUT
 *
 * The icon size name for about dialog icon size.
 *
 * Note: This is the name, you have to use gtk_icon_size_from_name() to get a
 * #GtkIconSize from it.
 **/

/* The following generated part is updated by running utils/stockgen.py */

/* @@@ GENERATED STOCK LIST BEGIN @@@ */
/**
 * GWY_STOCK_3D_BASE
 *
 * The "3D-Base" stock icon.
 * <inlinegraphic fileref="gwy_3d_base-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ARITHMETIC
 *
 * The "Arithmetic" stock icon.
 * <inlinegraphic fileref="gwy_arithmetic-24.png" format="PNG"/>

* Since: 2.3
 **/

/**
 * GWY_STOCK_BOLD
 *
 * The "Bold" stock icon.
 * <inlinegraphic fileref="gwy_bold-20.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_CANTILEVER
 *
 * The "Cantilever" stock icon.
 * <inlinegraphic fileref="gwy_cantilever-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_COLOR_RANGE
 *
 * The "Color-Range" stock icon.
 * <inlinegraphic fileref="gwy_color_range-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_CROP
 *
 * The "Crop" stock icon.
 * <inlinegraphic fileref="gwy_crop-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_CWT
 *
 * The "CWT" stock icon.
 * <inlinegraphic fileref="gwy_cwt-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_DATA_MEASURE
 *
 * The "Data-Measure" stock icon.
 * <inlinegraphic fileref="gwy_data_measure-24.png" format="PNG"/>

* Since: 2.3
 **/

/**
 * GWY_STOCK_DISTANCE
 *
 * The "Distance" stock icon.
 * <inlinegraphic fileref="gwy_distance-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_DRIFT
 *
 * The "Drift" stock icon.
 * <inlinegraphic fileref="gwy_drift-24.png" format="PNG"/>

* Since: 2.3
 **/

/**
 * GWY_STOCK_DWT
 *
 * The "DWT" stock icon.
 * <inlinegraphic fileref="gwy_dwt-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_EDGE
 *
 * The "Edge" stock icon.
 * <inlinegraphic fileref="gwy_edge-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FACET_LEVEL
 *
 * The "Facet-Level" stock icon.
 * <inlinegraphic fileref="gwy_facet_level-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FAVOURITE
 *
 * The "Favourite" stock icon.
 * <inlinegraphic fileref="gwy_favourite-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FFT
 *
 * The "FFT" stock icon.
 * <inlinegraphic fileref="gwy_fft-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FILTER
 *
 * The "Filter" stock icon.
 * <inlinegraphic fileref="gwy_filter-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FIX_ZERO
 *
 * The "Fix-Zero" stock icon.
 * <inlinegraphic fileref="gwy_fix_zero-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FLIP_HORIZONTALLY
 *
 * The "Flip-Horizontally" stock icon.
 * <inlinegraphic fileref="gwy_flip_horizontally-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FLIP_VERTICALLY
 *
 * The "Flip-Vertically" stock icon.
 * <inlinegraphic fileref="gwy_flip_vertically-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FRACTAL
 *
 * The "Fractal" stock icon.
 * <inlinegraphic fileref="gwy_fractal-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRADIENT_HORIZONTAL
 *
 * The "Gradient-Horizontal" stock icon.
 * <inlinegraphic fileref="gwy_gradient_horizontal-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRADIENT_VERTICAL
 *
 * The "Gradient-Vertical" stock icon.
 * <inlinegraphic fileref="gwy_gradient_vertical-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAINS
 *
 * The "Grains" stock icon.
 * <inlinegraphic fileref="gwy_grains-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAINS_GRAPH
 *
 * The "Grains-Graph" stock icon.
 * <inlinegraphic fileref="gwy_grains_graph-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAINS_REMOVE
 *
 * The "Grains-Remove" stock icon.
 * <inlinegraphic fileref="gwy_grains_remove-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAINS_WATER
 *
 * The "Grains-Water" stock icon.
 * <inlinegraphic fileref="gwy_grains_water-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH
 *
 * The "Graph" stock icon.
 * <inlinegraphic fileref="gwy_graph-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_FUNCTION
 *
 * The "Graph-Function" stock icon.
 * <inlinegraphic fileref="gwy_graph_function-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_HALFGAUSS
 *
 * The "Graph-Halfgauss" stock icon.
 * <inlinegraphic fileref="gwy_graph_halfgauss-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_MEASURE
 *
 * The "Graph-Measure" stock icon.
 * <inlinegraphic fileref="gwy_graph_measure-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_PALETTE
 *
 * The "Graph-Palette" stock icon.
 * <inlinegraphic fileref="gwy_graph_palette-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_POINTER
 *
 * The "Graph-Pointer" stock icon.
 * <inlinegraphic fileref="gwy_graph_pointer-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_RULER
 *
 * The "Graph-Ruler" stock icon.
 * <inlinegraphic fileref="gwy_graph_ruler-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_VERTICAL
 *
 * The "Graph-Vertical" stock icon.
 * <inlinegraphic fileref="gwy_graph_vertical-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_ZOOM_FIT
 *
 * The "Graph-Zoom-Fit" stock icon.
 * <inlinegraphic fileref="gwy_graph_zoom_fit-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_ZOOM_IN
 *
 * The "Graph-Zoom-In" stock icon.
 * <inlinegraphic fileref="gwy_graph_zoom_in-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_ZOOM_OUT
 *
 * The "Graph-Zoom-Out" stock icon.
 * <inlinegraphic fileref="gwy_graph_zoom_out-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GWYDDION
 *
 * The "Gwyddion" stock icon.
 * <inlinegraphic fileref="gwy_gwyddion-60.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_IMMERSE
 *
 * The "Immerse" stock icon.
 * <inlinegraphic fileref="gwy_immerse-24.png" format="PNG"/>

* Since: 2.3
 **/

/**
 * GWY_STOCK_ITALIC
 *
 * The "Italic" stock icon.
 * <inlinegraphic fileref="gwy_italic-20.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LESS
 *
 * The "Less" stock icon.
 * <inlinegraphic fileref="gwy_less-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LEVEL
 *
 * The "Level" stock icon.
 * <inlinegraphic fileref="gwy_level-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LEVEL_TRIANGLE
 *
 * The "Level-Triangle" stock icon.
 * <inlinegraphic fileref="gwy_level_triangle-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LIGHT_ROTATE
 *
 * The "Light-Rotate" stock icon.
 * <inlinegraphic fileref="gwy_light_rotate-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LINE_LEVEL
 *
 * The "Line-Level" stock icon.
 * <inlinegraphic fileref="gwy_line_level-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LOGSCALE_HORIZONTAL
 *
 * The "Logscale-Horizontal" stock icon.
 * <inlinegraphic fileref="gwy_logscale_horizontal-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LOGSCALE_VERTICAL
 *
 * The "Logscale-Vertical" stock icon.
 * <inlinegraphic fileref="gwy_logscale_vertical-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK
 *
 * The "Mask" stock icon.
 * <inlinegraphic fileref="gwy_mask-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_ADD
 *
 * The "Mask-Add" stock icon.
 * <inlinegraphic fileref="gwy_mask_add-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_CIRCLE
 *
 * The "Mask-Circle" stock icon.
 * <inlinegraphic fileref="gwy_mask_circle-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_CIRCLE_EXCLUSIVE
 *
 * The "Mask-Circle-Exclusive" stock icon.
 * <inlinegraphic fileref="gwy_mask_circle_exclusive-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_CIRCLE_INCLUSIVE
 *
 * The "Mask-Circle-Inclusive" stock icon.
 * <inlinegraphic fileref="gwy_mask_circle_inclusive-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_EDITOR
 *
 * The "Mask-Editor" stock icon.
 * <inlinegraphic fileref="gwy_mask_editor-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_EXCLUDE
 *
 * The "Mask-Exclude" stock icon.
 * <inlinegraphic fileref="gwy_mask_exclude-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_EXCLUDE_CIRCLE
 *
 * The "Mask-Exclude-Circle" stock icon.
 * <inlinegraphic fileref="gwy_mask_exclude_circle-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_GROW
 *
 * The "Mask-Grow" stock icon.
 * <inlinegraphic fileref="gwy_mask_grow-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_INTERSECT
 *
 * The "Mask-Intersect" stock icon.
 * <inlinegraphic fileref="gwy_mask_intersect-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_INVERT
 *
 * The "Mask-Invert" stock icon.
 * <inlinegraphic fileref="gwy_mask_invert-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_RECT_EXCLUSIVE
 *
 * The "Mask-Rect-Exclusive" stock icon.
 * <inlinegraphic fileref="gwy_mask_rect_exclusive-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_RECT_INCLUSIVE
 *
 * The "Mask-Rect-Inclusive" stock icon.
 * <inlinegraphic fileref="gwy_mask_rect_inclusive-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_REMOVE
 *
 * The "Mask-Remove" stock icon.
 * <inlinegraphic fileref="gwy_mask_remove-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_SHRINK
 *
 * The "Mask-Shrink" stock icon.
 * <inlinegraphic fileref="gwy_mask_shrink-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_SUBTRACT
 *
 * The "Mask-Subtract" stock icon.
 * <inlinegraphic fileref="gwy_mask_subtract-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MERGE
 *
 * The "Merge" stock icon.
 * <inlinegraphic fileref="gwy_merge-24.png" format="PNG"/>

* Since: 2.3
 **/

/**
 * GWY_STOCK_MORE
 *
 * The "More" stock icon.
 * <inlinegraphic fileref="gwy_more-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_PALETTES
 *
 * The "Palettes" stock icon.
 * <inlinegraphic fileref="gwy_palettes-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_PATH_LEVEL
 *
 * The "Path-Level" stock icon.
 * <inlinegraphic fileref="gwy_path_level-24.png" format="PNG"/>

* Since: 2.6
 **/

/**
 * GWY_STOCK_POINTER_MEASURE
 *
 * The "Pointer-Measure" stock icon.
 * <inlinegraphic fileref="gwy_pointer_measure-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_POLYNOM
 *
 * The "Polynom" stock icon.
 * <inlinegraphic fileref="gwy_polynom-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_PROFILE
 *
 * The "Profile" stock icon.
 * <inlinegraphic fileref="gwy_profile-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_REMOVE_UNDER_MASK
 *
 * The "Remove-Under-Mask" stock icon.
 * <inlinegraphic fileref="gwy_remove_under_mask-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ROTATE
 *
 * The "Rotate" stock icon.
 * <inlinegraphic fileref="gwy_rotate-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ROTATE_180
 *
 * The "Rotate-180" stock icon.
 * <inlinegraphic fileref="gwy_rotate_180-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ROTATE_90_CCW
 *
 * The "Rotate-90-CCW" stock icon.
 * <inlinegraphic fileref="gwy_rotate_90_ccw-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ROTATE_90_CW
 *
 * The "Rotate-90-CW" stock icon.
 * <inlinegraphic fileref="gwy_rotate_90_cw-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SCALE
 *
 * The "Scale" stock icon.
 * <inlinegraphic fileref="gwy_scale-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SCALE_HORIZONTALLY
 *
 * The "Scale-Horizontally" stock icon.
 * <inlinegraphic fileref="gwy_scale_horizontally-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SCALE_VERTICALLY
 *
 * The "Scale-Vertically" stock icon.
 * <inlinegraphic fileref="gwy_scale_vertically-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SCARS
 *
 * The "Scars" stock icon.
 * <inlinegraphic fileref="gwy_scars-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SHADER
 *
 * The "Shader" stock icon.
 * <inlinegraphic fileref="gwy_shader-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SPECTRUM
 *
 * The "Spectrum" stock icon.
 * <inlinegraphic fileref="gwy_spectrum-24.png" format="PNG"/>

* Since: 2.6
 **/

/**
 * GWY_STOCK_SPOT_REMOVE
 *
 * The "Spot-Remove" stock icon.
 * <inlinegraphic fileref="gwy_spot_remove-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_STAT_QUANTITIES
 *
 * The "Stat-Quantities" stock icon.
 * <inlinegraphic fileref="gwy_stat_quantities-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SUBSCRIPT
 *
 * The "Subscript" stock icon.
 * <inlinegraphic fileref="gwy_subscript-20.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SUPERSCRIPT
 *
 * The "Superscript" stock icon.
 * <inlinegraphic fileref="gwy_superscript-20.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_UNROTATE
 *
 * The "Unrotate" stock icon.
 * <inlinegraphic fileref="gwy_unrotate-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_VALUE_INVERT
 *
 * The "Value-Invert" stock icon.
 * <inlinegraphic fileref="gwy_value_invert-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ZOOM_1_1
 *
 * The "Zoom-1:1" stock icon.
 * <inlinegraphic fileref="gwy_zoom_1_1-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ZOOM_FIT
 *
 * The "Zoom-Fit" stock icon.
 * <inlinegraphic fileref="gwy_zoom_fit-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ZOOM_IN
 *
 * The "Zoom-In" stock icon.
 * <inlinegraphic fileref="gwy_zoom_in-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ZOOM_OUT
 *
 * The "Zoom-Out" stock icon.
 * <inlinegraphic fileref="gwy_zoom_out-24.png" format="PNG"/>
 **/

/* @@@ GENERATED STOCK LIST END @@@ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
