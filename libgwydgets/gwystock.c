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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
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

static void           register_icons           (const gchar **pixmap_paths,
                                                GtkIconFactory *icon_factory);
static void           slurp_icon_directory     (const gchar *path,
                                                GHashTable *icons);
static void           register_icon_set_list_cb(const gchar *id,
                                                GList *list,
                                                GtkIconFactory *factory);
static GtkIconSource* file_to_icon_source      (const gchar *path,
                                                const gchar *filename,
                                                gchar **id);

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
    gchar *pixmap_paths[3];

    g_return_if_fail(!the_icon_factory);
    gtk_icon_size_register(GWY_ICON_SIZE_ABOUT, 60, 60);
    pixmap_paths[0] = gwy_find_self_dir("pixmaps");
    pixmap_paths[1] = g_build_filename(gwy_get_user_dir(), "pixmaps", NULL);
    if (!g_file_test(pixmap_paths[1], G_FILE_TEST_IS_DIR)) {
        g_free(pixmap_paths[1]);
        pixmap_paths[1] = NULL;
    }
    pixmap_paths[2] = NULL;

    the_icon_factory = gtk_icon_factory_new();
    register_icons((const gchar**)pixmap_paths, the_icon_factory);

    g_free(pixmap_paths[0]);
    g_free(pixmap_paths[1]);
    gtk_icon_factory_add_default(the_icon_factory);
}

static void
register_icons(const gchar **pixmap_paths,
               GtkIconFactory *icon_factory)
{
    GHashTable *icons;

    icons = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    while (*pixmap_paths) {
        slurp_icon_directory(*pixmap_paths, icons);
        pixmap_paths++;
    }
    g_hash_table_foreach(icons, (GHFunc)register_icon_set_list_cb,
                         icon_factory);
    g_hash_table_destroy(icons);
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

/*
 * Filename format: <gwy_foobar>-<size>[.<state>].png
 */
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

/************************** Documentation ****************************/

/**
 * SECTION:gwystock
 * @title: gwystock
 * @short_description: Stock icons
 *
 * Use gwy_stock_register_stock_items() to register stock icons.
 **/

/**
 * GWY_ICON_SIZE_ABOUT:
 *
 * The icon size name for about dialog icon size.
 *
 * Note: This is the name, you have to use gtk_icon_size_from_name() to get a
 * #GtkIconSize from it.
 **/

/* The following generated part is updated by running utils/stockgen.py */

/* @@@ GENERATED STOCK LIST BEGIN @@@ */
/**
 * GWY_STOCK_3D_BASE:
 *
 * The "3D-Base" stock icon.
 * <inlinegraphic fileref="gwy_3d_base-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ARITHMETIC:
 *
 * The "Arithmetic" stock icon.
 * <inlinegraphic fileref="gwy_arithmetic-24.png" format="PNG"/>
 *
 * Since: 2.3
 **/

/**
 * GWY_STOCK_BOLD:
 *
 * The "Bold" stock icon.
 * <inlinegraphic fileref="gwy_bold-20.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_CANTILEVER:
 *
 * The "Cantilever" stock icon.
 * <inlinegraphic fileref="gwy_cantilever-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_COLOR_RANGE:
 *
 * The "Color-Range" stock icon.
 * <inlinegraphic fileref="gwy_color_range-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_COLOR_RANGE_ADAPTIVE:
 *
 * The "Color-Range-Adaptive" stock icon.
 * <inlinegraphic fileref="gwy_color_range_adaptive-24.png" format="PNG"/>
 *
 * Since: 2.7
 **/

/**
 * GWY_STOCK_COLOR_RANGE_AUTO:
 *
 * The "Color-Range-Auto" stock icon.
 * <inlinegraphic fileref="gwy_color_range_auto-24.png" format="PNG"/>
 *
 * Since: 2.7
 **/

/**
 * GWY_STOCK_COLOR_RANGE_FIXED:
 *
 * The "Color-Range-Fixed" stock icon.
 * <inlinegraphic fileref="gwy_color_range_fixed-24.png" format="PNG"/>
 *
 * Since: 2.7
 **/

/**
 * GWY_STOCK_COLOR_RANGE_FULL:
 *
 * The "Color-Range-Full" stock icon.
 * <inlinegraphic fileref="gwy_color_range_full-24.png" format="PNG"/>
 *
 * Since: 2.7
 **/

/**
 * GWY_STOCK_CONVOLUTION:
 *
 * The "Convolution" stock icon.
 * <inlinegraphic fileref="gwy_convolution-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_CORRECT_AFFINE:
 *
 * The "Correct-Affine" stock icon.
 * <inlinegraphic fileref="gwy_correct_affine-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_CROP:
 *
 * The "Crop" stock icon.
 * <inlinegraphic fileref="gwy_crop-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_CWT:
 *
 * The "CWT" stock icon.
 * <inlinegraphic fileref="gwy_cwt-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_DATA_MEASURE:
 *
 * The "Data-Measure" stock icon.
 * <inlinegraphic fileref="gwy_data_measure-24.png" format="PNG"/>
 *
 * Since: 2.3
 **/

/**
 * GWY_STOCK_DISTANCE:
 *
 * The "Distance" stock icon.
 * <inlinegraphic fileref="gwy_distance-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_DISTRIBUTION_ANGLE:
 *
 * The "Distribution-Angle" stock icon.
 * <inlinegraphic fileref="gwy_distribution_angle-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_DISTRIBUTION_SLOPE:
 *
 * The "Distribution-Slope" stock icon.
 * <inlinegraphic fileref="gwy_distribution_slope-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_DRIFT:
 *
 * The "Drift" stock icon.
 * <inlinegraphic fileref="gwy_drift-24.png" format="PNG"/>
 *
 * Since: 2.3
 **/

/**
 * GWY_STOCK_DWT:
 *
 * The "DWT" stock icon.
 * <inlinegraphic fileref="gwy_dwt-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_EDGE:
 *
 * The "Edge" stock icon.
 * <inlinegraphic fileref="gwy_edge-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ENTROPY:
 *
 * The "Entropy" stock icon.
 * <inlinegraphic fileref="gwy_entropy-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_EXTEND:
 *
 * The "Extend" stock icon.
 * <inlinegraphic fileref="gwy_extend-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_FACET_LEVEL:
 *
 * The "Facet-Level" stock icon.
 * <inlinegraphic fileref="gwy_facet_level-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FAVOURITE:
 *
 * The "Favourite" stock icon.
 * <inlinegraphic fileref="gwy_favourite-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FFT:
 *
 * The "FFT" stock icon.
 * <inlinegraphic fileref="gwy_fft-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FFT_2D:
 *
 * The "FFT-2D" stock icon.
 * <inlinegraphic fileref="gwy_fft_2d-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_FFT_FILTER_2D:
 *
 * The "FFT-Filter-2D" stock icon.
 * <inlinegraphic fileref="gwy_fft_filter_2d-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_FILTER:
 *
 * The "Filter" stock icon.
 * <inlinegraphic fileref="gwy_filter-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FIX_ZERO:
 *
 * The "Fix-Zero" stock icon.
 * <inlinegraphic fileref="gwy_fix_zero-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FLIP_HORIZONTALLY:
 *
 * The "Flip-Horizontally" stock icon.
 * <inlinegraphic fileref="gwy_flip_horizontally-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FLIP_VERTICALLY:
 *
 * The "Flip-Vertically" stock icon.
 * <inlinegraphic fileref="gwy_flip_vertically-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_FRACTAL:
 *
 * The "Fractal" stock icon.
 * <inlinegraphic fileref="gwy_fractal-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GL_MATERIAL:
 *
 * The "GL-Material" stock icon.
 * <inlinegraphic fileref="gwy_gl_material-16.png" format="PNG"/>
 *
 * Since: 2.7
 **/

/**
 * GWY_STOCK_GRADIENT_HORIZONTAL:
 *
 * The "Gradient-Horizontal" stock icon.
 * <inlinegraphic fileref="gwy_gradient_horizontal-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRADIENT_VERTICAL:
 *
 * The "Gradient-Vertical" stock icon.
 * <inlinegraphic fileref="gwy_gradient_vertical-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAINS:
 *
 * The "Grains" stock icon.
 * <inlinegraphic fileref="gwy_grains-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAINS_EDGE:
 *
 * The "Grains-Edge" stock icon.
 * <inlinegraphic fileref="gwy_grains_edge-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAINS_GRAPH:
 *
 * The "Grains-Graph" stock icon.
 * <inlinegraphic fileref="gwy_grains_graph-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAINS_MEASURE:
 *
 * The "Grains-Measure" stock icon.
 * <inlinegraphic fileref="gwy_grains_measure-24.png" format="PNG"/>
 *
 * Since: 2.7
 **/

/**
 * GWY_STOCK_GRAINS_REMOVE:
 *
 * The "Grains-Remove" stock icon.
 * <inlinegraphic fileref="gwy_grains_remove-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAINS_WATER:
 *
 * The "Grains-Water" stock icon.
 * <inlinegraphic fileref="gwy_grains_water-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAIN_CORRELATION:
 *
 * The "Grain-Correlation" stock icon.
 * <inlinegraphic fileref="gwy_grain_correlation-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAIN_EXSCRIBED_CIRCLE:
 *
 * The "Grain-Exscribed-Circle" stock icon.
 * <inlinegraphic fileref="gwy_grain_exscribed_circle-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAIN_INSCRIBED_CIRCLE:
 *
 * The "Grain-Inscribed-Circle" stock icon.
 * <inlinegraphic fileref="gwy_grain_inscribed_circle-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAPH:
 *
 * The "Graph" stock icon.
 * <inlinegraphic fileref="gwy_graph-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_ALIGN:
 *
 * The "Graph-Align" stock icon.
 * <inlinegraphic fileref="gwy_graph_align-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAPH_CUT:
 *
 * The "Graph-Cut" stock icon.
 * <inlinegraphic fileref="gwy_graph_cut-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAPH_DOS:
 *
 * The "Graph-Dos" stock icon.
 * <inlinegraphic fileref="gwy_graph_dos-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAPH_EXPORT_ASCII:
 *
 * The "Graph-Export-Ascii" stock icon.
 * <inlinegraphic fileref="gwy_graph_export_ascii-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAPH_EXPORT_PNG:
 *
 * The "Graph-Export-PNG" stock icon.
 * <inlinegraphic fileref="gwy_graph_export_png-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAPH_EXPORT_VECTOR:
 *
 * The "Graph-Export-Vector" stock icon.
 * <inlinegraphic fileref="gwy_graph_export_vector-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAPH_FD:
 *
 * The "Graph-FD" stock icon.
 * <inlinegraphic fileref="gwy_graph_fd-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAPH_FILTER:
 *
 * The "Graph-Filter" stock icon.
 * <inlinegraphic fileref="gwy_graph_filter-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAPH_FUNCTION:
 *
 * The "Graph-Function" stock icon.
 * <inlinegraphic fileref="gwy_graph_function-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_HALFGAUSS:
 *
 * The "Graph-Halfgauss" stock icon.
 * <inlinegraphic fileref="gwy_graph_halfgauss-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_LEVEL:
 *
 * The "Graph-Level" stock icon.
 * <inlinegraphic fileref="gwy_graph_level-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_GRAPH_MEASURE:
 *
 * The "Graph-Measure" stock icon.
 * <inlinegraphic fileref="gwy_graph_measure-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_PALETTE:
 *
 * The "Graph-Palette" stock icon.
 * <inlinegraphic fileref="gwy_graph_palette-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_POINTER:
 *
 * The "Graph-Pointer" stock icon.
 * <inlinegraphic fileref="gwy_graph_pointer-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_RULER:
 *
 * The "Graph-Ruler" stock icon.
 * <inlinegraphic fileref="gwy_graph_ruler-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_VERTICAL:
 *
 * The "Graph-Vertical" stock icon.
 * <inlinegraphic fileref="gwy_graph_vertical-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_ZOOM_FIT:
 *
 * The "Graph-Zoom-Fit" stock icon.
 * <inlinegraphic fileref="gwy_graph_zoom_fit-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_ZOOM_IN:
 *
 * The "Graph-Zoom-In" stock icon.
 * <inlinegraphic fileref="gwy_graph_zoom_in-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GRAPH_ZOOM_OUT:
 *
 * The "Graph-Zoom-Out" stock icon.
 * <inlinegraphic fileref="gwy_graph_zoom_out-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_GWYDDION:
 *
 * The "Gwyddion" stock icon.
 * <inlinegraphic fileref="gwy_gwyddion-60.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_HOUGH:
 *
 * The "Hough" stock icon.
 * <inlinegraphic fileref="gwy_hough-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_IMMERSE:
 *
 * The "Immerse" stock icon.
 * <inlinegraphic fileref="gwy_immerse-24.png" format="PNG"/>
 *
 * Since: 2.3
 **/

/**
 * GWY_STOCK_ISO_ROUGHNESS:
 *
 * The "Iso-Roughness" stock icon.
 * <inlinegraphic fileref="gwy_iso_roughness-24.png" format="PNG"/>
 *
 * Since: 2.7
 **/

/**
 * GWY_STOCK_ITALIC:
 *
 * The "Italic" stock icon.
 * <inlinegraphic fileref="gwy_italic-20.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LESS:
 *
 * The "Less" stock icon.
 * <inlinegraphic fileref="gwy_less-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LEVEL:
 *
 * The "Level" stock icon.
 * <inlinegraphic fileref="gwy_level-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LEVEL_FLATTEN_BASE:
 *
 * The "Level-Flatten-Base" stock icon.
 * <inlinegraphic fileref="gwy_level_flatten_base-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_LEVEL_MEDIAN:
 *
 * The "Level-Median" stock icon.
 * <inlinegraphic fileref="gwy_level_median-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_LEVEL_TRIANGLE:
 *
 * The "Level-Triangle" stock icon.
 * <inlinegraphic fileref="gwy_level_triangle-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LIGHT_ROTATE:
 *
 * The "Light-Rotate" stock icon.
 * <inlinegraphic fileref="gwy_light_rotate-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LINE_LEVEL:
 *
 * The "Line-Level" stock icon.
 * <inlinegraphic fileref="gwy_line_level-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LOAD_DEBUG:
 *
 * The "Load-Debug" stock icon.
 * <inlinegraphic fileref="gwy_load_debug-20.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_LOAD_INFO:
 *
 * The "Load-Info" stock icon.
 * <inlinegraphic fileref="gwy_load_info-20.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_LOAD_WARNING:
 *
 * The "Load-Warning" stock icon.
 * <inlinegraphic fileref="gwy_load_warning-20.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_LOCAL_SLOPE:
 *
 * The "Local-Slope" stock icon.
 * <inlinegraphic fileref="gwy_local_slope-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_LOGSCALE_HORIZONTAL:
 *
 * The "Logscale-Horizontal" stock icon.
 * <inlinegraphic fileref="gwy_logscale_horizontal-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_LOGSCALE_VERTICAL:
 *
 * The "Logscale-Vertical" stock icon.
 * <inlinegraphic fileref="gwy_logscale_vertical-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MARK_WITH:
 *
 * The "Mark-With" stock icon.
 * <inlinegraphic fileref="gwy_mark_with-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_MASK:
 *
 * The "Mask" stock icon.
 * <inlinegraphic fileref="gwy_mask-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_ADD:
 *
 * The "Mask-Add" stock icon.
 * <inlinegraphic fileref="gwy_mask_add-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_CIRCLE:
 *
 * The "Mask-Circle" stock icon.
 * <inlinegraphic fileref="gwy_mask_circle-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_CIRCLE_EXCLUSIVE:
 *
 * The "Mask-Circle-Exclusive" stock icon.
 * <inlinegraphic fileref="gwy_mask_circle_exclusive-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_CIRCLE_INCLUSIVE:
 *
 * The "Mask-Circle-Inclusive" stock icon.
 * <inlinegraphic fileref="gwy_mask_circle_inclusive-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_DISTRIBUTE:
 *
 * The "Mask-Distribute" stock icon.
 * <inlinegraphic fileref="gwy_mask_distribute-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_MASK_EDITOR:
 *
 * The "Mask-Editor" stock icon.
 * <inlinegraphic fileref="gwy_mask_editor-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_EXCLUDE:
 *
 * The "Mask-Exclude" stock icon.
 * <inlinegraphic fileref="gwy_mask_exclude-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_EXCLUDE_CIRCLE:
 *
 * The "Mask-Exclude-Circle" stock icon.
 * <inlinegraphic fileref="gwy_mask_exclude_circle-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_EXTRACT:
 *
 * The "Mask-Extract" stock icon.
 * <inlinegraphic fileref="gwy_mask_extract-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_MASK_FILL_DRAW:
 *
 * The "Mask-Fill-Draw" stock icon.
 * <inlinegraphic fileref="gwy_mask_fill_draw-24.png" format="PNG"/>
 *
 * Since: 2.22
 **/

/**
 * GWY_STOCK_MASK_FILL_ERASE:
 *
 * The "Mask-Fill-Erase" stock icon.
 * <inlinegraphic fileref="gwy_mask_fill_erase-24.png" format="PNG"/>
 *
 * Since: 2.22
 **/

/**
 * GWY_STOCK_MASK_GROW:
 *
 * The "Mask-Grow" stock icon.
 * <inlinegraphic fileref="gwy_mask_grow-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_INTERSECT:
 *
 * The "Mask-Intersect" stock icon.
 * <inlinegraphic fileref="gwy_mask_intersect-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_INVERT:
 *
 * The "Mask-Invert" stock icon.
 * <inlinegraphic fileref="gwy_mask_invert-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_LINE:
 *
 * The "Mask-Line" stock icon.
 * <inlinegraphic fileref="gwy_mask_line-24.png" format="PNG"/>
 *
 * Since: 2.7
 **/

/**
 * GWY_STOCK_MASK_MORPH:
 *
 * The "Mask-Morph" stock icon.
 * <inlinegraphic fileref="gwy_mask_morph-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_MASK_PAINT_DRAW:
 *
 * The "Mask-Paint-Draw" stock icon.
 * <inlinegraphic fileref="gwy_mask_paint_draw-24.png" format="PNG"/>
 *
 * Since: 2.22
 **/

/**
 * GWY_STOCK_MASK_PAINT_ERASE:
 *
 * The "Mask-Paint-Erase" stock icon.
 * <inlinegraphic fileref="gwy_mask_paint_erase-24.png" format="PNG"/>
 *
 * Since: 2.22
 **/

/**
 * GWY_STOCK_MASK_RECT_EXCLUSIVE:
 *
 * The "Mask-Rect-Exclusive" stock icon.
 * <inlinegraphic fileref="gwy_mask_rect_exclusive-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_RECT_INCLUSIVE:
 *
 * The "Mask-Rect-Inclusive" stock icon.
 * <inlinegraphic fileref="gwy_mask_rect_inclusive-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_REMOVE:
 *
 * The "Mask-Remove" stock icon.
 * <inlinegraphic fileref="gwy_mask_remove-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_SHRINK:
 *
 * The "Mask-Shrink" stock icon.
 * <inlinegraphic fileref="gwy_mask_shrink-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_SUBTRACT:
 *
 * The "Mask-Subtract" stock icon.
 * <inlinegraphic fileref="gwy_mask_subtract-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_MASK_THIN:
 *
 * The "Mask-Thin" stock icon.
 * <inlinegraphic fileref="gwy_mask_thin-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_MEASURE_LATTICE:
 *
 * The "Measure-Lattice" stock icon.
 * <inlinegraphic fileref="gwy_measure_lattice-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_MERGE:
 *
 * The "Merge" stock icon.
 * <inlinegraphic fileref="gwy_merge-24.png" format="PNG"/>
 *
 * Since: 2.3
 **/

/**
 * GWY_STOCK_MORE:
 *
 * The "More" stock icon.
 * <inlinegraphic fileref="gwy_more-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_NEURAL_APPLY:
 *
 * The "Neural-Apply" stock icon.
 * <inlinegraphic fileref="gwy_neural_apply-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_NEURAL_TRAIN:
 *
 * The "Neural-Train" stock icon.
 * <inlinegraphic fileref="gwy_neural_train-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PALETTES:
 *
 * The "Palettes" stock icon.
 * <inlinegraphic fileref="gwy_palettes-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_PATH_LEVEL:
 *
 * The "Path-Level" stock icon.
 * <inlinegraphic fileref="gwy_path_level-24.png" format="PNG"/>
 *
 * Since: 2.7
 **/

/**
 * GWY_STOCK_POINTER_MEASURE:
 *
 * The "Pointer-Measure" stock icon.
 * <inlinegraphic fileref="gwy_pointer_measure-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_POLYNOM:
 *
 * The "Polynom" stock icon.
 * <inlinegraphic fileref="gwy_polynom-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_POLYNOM_LEVEL:
 *
 * The "Polynom-Level" stock icon.
 * <inlinegraphic fileref="gwy_polynom_level-24.png" format="PNG"/>
 *
 * Since: 2.29
 **/

/**
 * GWY_STOCK_PRESENTATION_ATTACH:
 *
 * The "Presentation-Attach" stock icon.
 * <inlinegraphic fileref="gwy_presentation_attach-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_CANNY:
 *
 * The "Presentation-Edge-Canny" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_canny-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_HARRIS_CORNER:
 *
 * The "Presentation-Edge-Harris-Corner" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_harris_corner-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_HOUGH:
 *
 * The "Presentation-Edge-Hough" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_hough-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_INCLINATION:
 *
 * The "Presentation-Edge-Inclination" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_inclination-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_LAPLACE_GAUSS:
 *
 * The "Presentation-Edge-Laplace-Gauss" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_laplace_gauss-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_LOCAL_NON_LINEARITY:
 *
 * The "Presentation-Edge-Local-Non-Linearity" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_local_non_linearity-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_PREWITT:
 *
 * The "Presentation-Edge-Prewitt" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_prewitt-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_RMS:
 *
 * The "Presentation-Edge-RMS" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_rms-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_RMS_EDGE:
 *
 * The "Presentation-Edge-RMS-Edge" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_rms_edge-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_SOBEL:
 *
 * The "Presentation-Edge-Sobel" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_sobel-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_STEP:
 *
 * The "Presentation-Edge-Step" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_step-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EDGE_ZERO_CROSSING:
 *
 * The "Presentation-Edge-Zero-Crossing" stock icon.
 * <inlinegraphic fileref="gwy_presentation_edge_zero_crossing-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_EXTRACT:
 *
 * The "Presentation-Extract" stock icon.
 * <inlinegraphic fileref="gwy_presentation_extract-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_LOCAL_CONTRAST:
 *
 * The "Presentation-Local-Contrast" stock icon.
 * <inlinegraphic fileref="gwy_presentation_local_contrast-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_LOG:
 *
 * The "Presentation-Log" stock icon.
 * <inlinegraphic fileref="gwy_presentation_log-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_RANK:
 *
 * The "Presentation-Rank" stock icon.
 * <inlinegraphic fileref="gwy_presentation_rank-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_REMOVE:
 *
 * The "Presentation-Remove" stock icon.
 * <inlinegraphic fileref="gwy_presentation_remove-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_SEM:
 *
 * The "Presentation-Sem" stock icon.
 * <inlinegraphic fileref="gwy_presentation_sem-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PRESENTATION_SHADING:
 *
 * The "Presentation-Shading" stock icon.
 * <inlinegraphic fileref="gwy_presentation_shading-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PROFILE:
 *
 * The "Profile" stock icon.
 * <inlinegraphic fileref="gwy_profile-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_PSDF_LOG_PHI:
 *
 * The "PSDF-Log-Phi" stock icon.
 * <inlinegraphic fileref="gwy_psdf_log_phi-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PSD_SECTION:
 *
 * The "PSD-Section" stock icon.
 * <inlinegraphic fileref="gwy_psd_section-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_PYGWY:
 *
 * The "Pygwy" stock icon.
 * <inlinegraphic fileref="gwy_pygwy-24.png" format="PNG"/>
 *
 * Since: 2.34
 **/

/**
 * GWY_STOCK_REMOVE_UNDER_MASK:
 *
 * The "Remove-Under-Mask" stock icon.
 * <inlinegraphic fileref="gwy_remove_under_mask-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ROTATE:
 *
 * The "Rotate" stock icon.
 * <inlinegraphic fileref="gwy_rotate-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ROTATE_180:
 *
 * The "Rotate-180" stock icon.
 * <inlinegraphic fileref="gwy_rotate_180-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ROTATE_90_CCW:
 *
 * The "Rotate-90-CCW" stock icon.
 * <inlinegraphic fileref="gwy_rotate_90_ccw-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ROTATE_90_CW:
 *
 * The "Rotate-90-CW" stock icon.
 * <inlinegraphic fileref="gwy_rotate_90_cw-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SCALE:
 *
 * The "Scale" stock icon.
 * <inlinegraphic fileref="gwy_scale-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SCALE_HORIZONTALLY:
 *
 * The "Scale-Horizontally" stock icon.
 * <inlinegraphic fileref="gwy_scale_horizontally-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SCALE_VERTICALLY:
 *
 * The "Scale-Vertically" stock icon.
 * <inlinegraphic fileref="gwy_scale_vertically-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SCARS:
 *
 * The "Scars" stock icon.
 * <inlinegraphic fileref="gwy_scars-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SELECTIONS:
 *
 * The "Selections" stock icon.
 * <inlinegraphic fileref="gwy_selections-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SHADER:
 *
 * The "Shader" stock icon.
 * <inlinegraphic fileref="gwy_shader-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SPECTRUM:
 *
 * The "Spectrum" stock icon.
 * <inlinegraphic fileref="gwy_spectrum-24.png" format="PNG"/>
 *
 * Since: 2.7
 **/

/**
 * GWY_STOCK_SPOT_REMOVE:
 *
 * The "Spot-Remove" stock icon.
 * <inlinegraphic fileref="gwy_spot_remove-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_STAT_QUANTITIES:
 *
 * The "Stat-Quantities" stock icon.
 * <inlinegraphic fileref="gwy_stat_quantities-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SUBSCRIPT:
 *
 * The "Subscript" stock icon.
 * <inlinegraphic fileref="gwy_subscript-20.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SUPERSCRIPT:
 *
 * The "Superscript" stock icon.
 * <inlinegraphic fileref="gwy_superscript-20.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_SYNTHETIC_BALLISTIC_DEPOSITION:
 *
 * The "Synthetic-Ballistic-Deposition" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_ballistic_deposition-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_SYNTHETIC_BROWNIAN_MOTION:
 *
 * The "Synthetic-Brownian-Motion" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_brownian_motion-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_SYNTHETIC_COLUMNAR:
 *
 * The "Synthetic-Columnar" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_columnar-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_SYNTHETIC_DIFFUSION:
 *
 * The "Synthetic-Diffusion" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_diffusion-24.png" format="PNG"/>
 *
 * Since: 2.38
 **/

/**
 * GWY_STOCK_SYNTHETIC_DOMAINS:
 *
 * The "Synthetic-Domains" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_domains-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_SYNTHETIC_LATTICE:
 *
 * The "Synthetic-Lattice" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_lattice-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_SYNTHETIC_LINE_NOISE:
 *
 * The "Synthetic-Line-Noise" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_line_noise-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_SYNTHETIC_NOISE:
 *
 * The "Synthetic-Noise" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_noise-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_SYNTHETIC_OBJECTS:
 *
 * The "Synthetic-Objects" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_objects-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_SYNTHETIC_PARTICLES:
 *
 * The "Synthetic-Particles" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_particles-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_SYNTHETIC_PATTERN:
 *
 * The "Synthetic-Pattern" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_pattern-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_SYNTHETIC_SPECTRAL:
 *
 * The "Synthetic-Spectral" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_spectral-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_SYNTHETIC_WAVES:
 *
 * The "Synthetic-Waves" stock icon.
 * <inlinegraphic fileref="gwy_synthetic_waves-24.png" format="PNG"/>
 *
 * Since: 2.37
 **/

/**
 * GWY_STOCK_TILT:
 *
 * The "Tilt" stock icon.
 * <inlinegraphic fileref="gwy_tilt-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_TIP_DILATION:
 *
 * The "Tip-Dilation" stock icon.
 * <inlinegraphic fileref="gwy_tip_dilation-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_TIP_EROSION:
 *
 * The "Tip-Erosion" stock icon.
 * <inlinegraphic fileref="gwy_tip_erosion-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_TIP_ESTIMATION:
 *
 * The "Tip-Estimation" stock icon.
 * <inlinegraphic fileref="gwy_tip_estimation-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_TIP_MAP:
 *
 * The "Tip-Map" stock icon.
 * <inlinegraphic fileref="gwy_tip_map-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_TIP_MODEL:
 *
 * The "Tip-Model" stock icon.
 * <inlinegraphic fileref="gwy_tip_model-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_TIP_PID:
 *
 * The "Tip-Pid" stock icon.
 * <inlinegraphic fileref="gwy_tip_pid-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_UNROTATE:
 *
 * The "Unrotate" stock icon.
 * <inlinegraphic fileref="gwy_unrotate-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_VALUE_INVERT:
 *
 * The "Value-Invert" stock icon.
 * <inlinegraphic fileref="gwy_value_invert-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_VOLUME:
 *
 * The "Volume" stock icon.
 * <inlinegraphic fileref="gwy_volume-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_VOLUME_CALIBRATE:
 *
 * The "Volume-Calibrate" stock icon.
 * <inlinegraphic fileref="gwy_volume_calibrate-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_VOLUME_DIMENSIONS:
 *
 * The "Volume-Dimensions" stock icon.
 * <inlinegraphic fileref="gwy_volume_dimensions-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_VOLUME_INVERT:
 *
 * The "Volume-Invert" stock icon.
 * <inlinegraphic fileref="gwy_volume_invert-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_VOLUMIZE_LAYERS:
 *
 * The "Volumize-Layers" stock icon.
 * <inlinegraphic fileref="gwy_volumize_layers-24.png" format="PNG"/>
 *
 * Since: 2.45
 **/

/**
 * GWY_STOCK_ZOOM_1_1:
 *
 * The "Zoom-1:1" stock icon.
 * <inlinegraphic fileref="gwy_zoom_1_1-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ZOOM_FIT:
 *
 * The "Zoom-Fit" stock icon.
 * <inlinegraphic fileref="gwy_zoom_fit-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ZOOM_IN:
 *
 * The "Zoom-In" stock icon.
 * <inlinegraphic fileref="gwy_zoom_in-24.png" format="PNG"/>
 **/

/**
 * GWY_STOCK_ZOOM_OUT:
 *
 * The "Zoom-Out" stock icon.
 * <inlinegraphic fileref="gwy_zoom_out-24.png" format="PNG"/>
 **/

/* @@@ GENERATED STOCK LIST END @@@ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
