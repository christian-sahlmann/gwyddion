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

#include <string.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkimagemenuitem.h>

#include <libgwyddion/gwyddion.h>
#include <libdraw/gwypalette.h>
#include "gwyoptionmenus.h"

#define BITS_PER_SAMPLE 8
#define PALETTE_SAMPLE_HEIGHT 16
#define PALETTE_SAMPLE_WIDTH 80

static GtkWidget* gwy_palette_menu_create        (const gchar *current,
                                                  gint *current_idx);
static GtkWidget* gwy_sample_palette_to_gtkimage (GwyPaletteDef *palette_def);
static gint       palette_def_compare            (GwyPaletteDef *a,
                                                  GwyPaletteDef *b);
static gint       gl_material_compare            (GwyGLMaterial *a,
                                                  GwyGLMaterial *b);
static void       gwy_option_menu_metric_unit_destroyed (GwyEnum *entries);

/************************** Palette menu ****************************/

static GtkWidget*
gwy_palette_menu_create(const gchar *current,
                        gint *current_idx)
{
    GSList *l, *entries = NULL;
    GtkWidget *menu, *image, *item, *hbox, *label;
    gint i, idx;

    gwy_palette_def_foreach((GwyPaletteDefFunc)gwy_hash_table_to_slist_cb,
                            &entries);
    entries = g_slist_sort(entries, (GCompareFunc)palette_def_compare);

    menu = gtk_menu_new();

    idx = -1;
    i = 0;
    for (l = entries; l; l = g_slist_next(l)) {
        GwyPaletteDef *palette_def = (GwyPaletteDef*)l->data;
        const gchar *name = gwy_palette_def_get_name(palette_def);

        image = gwy_sample_palette_to_gtkimage(palette_def);
        item = gtk_menu_item_new();
        hbox = gtk_hbox_new(FALSE, 6);
        label = gtk_label_new(name);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(item), hbox);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_data(G_OBJECT(item), "palette-name", (gpointer)name);
        if (current && strcmp(current, name) == 0)
            idx = i;
        i++;
    }
    gwy_sample_palette_to_gtkimage(NULL);
    g_slist_free(entries);

    if (current_idx && idx != -1)
        *current_idx = idx;

    return menu;
}

/**
 * gwy_menu_palette:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 *
 * Creates a pop-up palette menu.
 *
 * Returns: The newly created pop-up menu as #GtkWidget.
 **/
GtkWidget*
gwy_menu_palette(GCallback callback,
                 gpointer cbdata)
{
    GtkWidget *menu;
    GList *c;

    menu = gwy_palette_menu_create(NULL, NULL);
    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }

    return menu;
}

/**
 * gwy_option_menu_palette:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 * @current: Palette definition name to be shown as currently selected
 *           (or %NULL to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of palettes (more preciesly, palettes definitions),
 * alphabetically sorted, with names and small sample images.
 *
 * It sets object data "palette-name" to palette definition name for each
 * menu item.
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_palette(GCallback callback,
                        gpointer cbdata,
                        const gchar *current)
{
    GtkWidget *omenu, *menu;
    GList *c;
    gint idx;

    idx = -1;
    omenu = gtk_option_menu_new();
    g_object_set_data(G_OBJECT(omenu), "gwy-option-menu",
                      GINT_TO_POINTER(TRUE));
    menu = gwy_palette_menu_create(current, &idx);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (idx != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), idx);

    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }

    return omenu;
}

/* XXX: magic static variables */
static GtkWidget*
gwy_sample_palette_to_gtkimage(GwyPaletteDef *palette_def)
{
    static GwyPalette *palette = NULL;
    static guchar *samples = NULL;
    GdkPixbuf *pixbuf;
    GtkWidget *image;
    guint rowstride;
    guchar *data;
    gint i;

    /* clean up when called with NULL */
    if (!palette_def) {
        g_free(samples);
        samples = NULL;
        gwy_object_unref(palette);
        return NULL;
    }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 0, BITS_PER_SAMPLE,
                            PALETTE_SAMPLE_WIDTH, PALETTE_SAMPLE_HEIGHT);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    data = gdk_pixbuf_get_pixels(pixbuf);

    if (!palette)
        palette = (GwyPalette*)gwy_palette_new(palette_def);
    else
        gwy_palette_set_palette_def(palette, palette_def);

    samples = gwy_palette_sample(palette, 4*PALETTE_SAMPLE_WIDTH, samples);
    for (i = 0; i < PALETTE_SAMPLE_WIDTH; i++) {
        guchar *sam = samples + 4*4*i;

        data[3*i] = ((guint)sam[0] + sam[4] + sam[8] + sam[12])/4;
        data[3*i + 1] = ((guint)sam[1] + sam[5] + sam[9] + sam[13])/4;
        data[3*i + 2] = ((guint)sam[2] + sam[6] + sam[10] + sam[14])/4;
    }
    for (i = 1; i < PALETTE_SAMPLE_HEIGHT; i++)
        memcpy(data + i*rowstride, data, 3*PALETTE_SAMPLE_WIDTH);
    gwy_object_unref(palette);

    image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);

    return image;
}

static gint
palette_def_compare(GwyPaletteDef *a,
                    GwyPaletteDef *b)
{
    /* XXX: should use gwy_palette_def_get_name() */
    return strcmp(a->name, b->name);
}


/************************** Material menu ****************************/

static GtkWidget*
gwy_gl_material_menu_create(const gchar *current,
                            gint *current_idx)
{
    GSList *l, *entries = NULL;
    GtkWidget *menu, *item;
    gint i, idx;

    gwy_gl_material_foreach((GwyGLMaterialFunc)gwy_hash_table_to_slist_cb,
                            &entries);
    entries = g_slist_sort(entries, (GCompareFunc)gl_material_compare);

    menu = gtk_menu_new();

    idx = -1;
    i = 0;
    for (l = entries; l; l = g_slist_next(l)) {
        GwyGLMaterial *gl_material = (GwyGLMaterial*)l->data;
        const gchar *name = gwy_gl_material_get_name(gl_material);

        item = gtk_menu_item_new_with_label(_(name));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_data(G_OBJECT(item), "material-name", (gpointer)name);
        if (current && strcmp(current, name) == 0)
            idx = i;
        i++;
    }
    g_slist_free(entries);

    if (current_idx && idx != -1)
        *current_idx = idx;

    return menu;
}

/**
 * gwy_menu_gl_material:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 *
 * Creates a pop-up OpenGL material menu.
 *
 * Returns: The newly created pop-up menu as #GtkWidget.
 *
 * Since: 1.5.
 **/
GtkWidget*
gwy_menu_gl_material(GCallback callback,
                     gpointer cbdata)
{
    GtkWidget *menu;
    GList *c;

    menu = gwy_gl_material_menu_create(NULL, NULL);
    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }

    return menu;
}

/**
 * gwy_option_menu_gl_material:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 * @current: Palette definition name to be shown as currently selected
 *           (or %NULL to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of OpenGL materials.
 *
 * It sets object data "material-name" to material definition name for each
 * menu item.
 *
 * Returns: The newly created option menu as #GtkWidget.
 *
 * Since: 1.5.
 **/
GtkWidget*
gwy_option_menu_gl_material(GCallback callback,
                            gpointer cbdata,
                            const gchar *current)
{
    GtkWidget *omenu, *menu;
    GList *c;
    gint idx;

    idx = -1;
    omenu = gtk_option_menu_new();
    g_object_set_data(G_OBJECT(omenu), "gwy-option-menu",
                      GINT_TO_POINTER(TRUE));
    menu = gwy_gl_material_menu_create(current, &idx);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (idx != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), idx);

    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }

    return omenu;
}

static gint
gl_material_compare(GwyGLMaterial *a,
                    GwyGLMaterial *b)
{
    return strcmp(gwy_gl_material_get_name(a), gwy_gl_material_get_name(b));
}


/************************** Enum option menu ****************************/

/**
 * gwy_option_menu_create:
 * @entries: Option menu items.
 * @nentries: The number of items.
 * @key: Value object data key.
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            no callback).
 * @cbdata: User data passed to the callback.
 * @current: Value to be shown as currently selected (-1 to use what happens
 *           to be first).
 *
 * Creates an option menu for an enum.
 *
 * It sets object data identified by @key for each menu item to its value.
 * Try to avoid -1 as an enum value.
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_create(const GwyEnum *entries,
                       gint nentries,
                       const gchar *key,
                       GCallback callback,
                       gpointer cbdata,
                       gint current)
{
    GtkWidget *omenu, *menu, *item;
    GList *c;
    GQuark quark;
    gint i, idx;

    quark = g_quark_from_string(key);
    omenu = gtk_option_menu_new();
    g_object_set_data(G_OBJECT(omenu), "gwy-option-menu",
                      GINT_TO_POINTER(TRUE));
    menu = gtk_menu_new();

    idx = -1;
    for (i = 0; i < nentries; i++) {
        item = gtk_menu_item_new_with_label(_(entries[i].name));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_qdata(G_OBJECT(item), quark,
                           GINT_TO_POINTER(entries[i].value));
        if (entries[i].value == current)
            idx = i;
    }
    gwy_debug("current: %d", idx);

    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (idx != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), idx);

    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }

    return omenu;
}

/**
 * gwy_option_menu_set_history:
 * @option_menu: An option menu created by gwy_option_menu_create().
 * @key: Value object data key.  Either the key you specified when called
 *       gwy_option_menu_create(), or the key listed in description of
 *       particular option menu constructor.
 * @current: Value to be shown as currently selected.
 *
 * Sets option menu history based on integer item object data (as set by
 * gwy_option_menu_create()).
 *
 * Returns: %TRUE if the history was set, %FALSE if @current was not found.
 **/
gboolean
gwy_option_menu_set_history(GtkWidget *option_menu,
                            const gchar *key,
                            gint current)
{
    GQuark quark;
    GtkWidget *menu;
    GList *c;
    gint i;

    g_return_val_if_fail(GTK_IS_OPTION_MENU(option_menu), FALSE);
    g_return_val_if_fail(g_object_get_data(G_OBJECT(option_menu),
                                           "gwy-option-menu"), FALSE);
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));
    quark = g_quark_from_string(key);
    i = 0;
    for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c)) {
        if (GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(c->data), quark))
            == current) {
            gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), i);
            return TRUE;
        }
        i++;
    }
    return FALSE;
}

/**
 * gwy_option_menu_get_history:
 * @option_menu: An option menu created by gwy_option_menu_create().
 * @key: Value object data key.  Either the key you specified when called
 *       gwy_option_menu_create(), or the key listed in description of
 *       particular option menu constructor.
 *
 * Gets the integer enum value corresponding to currently selected item.
 *
 * Returns: The enum value corresponding to currently selected item.  In
 *          case of failure -1 is returned.
 **/
gint
gwy_option_menu_get_history(GtkWidget *option_menu,
                            const gchar *key)
{
    GList *c;
    GQuark quark;
    GtkWidget *menu, *item;
    gint idx;

    g_return_val_if_fail(GTK_IS_OPTION_MENU(option_menu), -1);
    g_return_val_if_fail(g_object_get_data(G_OBJECT(option_menu),
                                           "gwy-option-menu"), -1);

    idx = gtk_option_menu_get_history(GTK_OPTION_MENU(option_menu));
    if (idx < 0)
        return -1;
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));
    quark = g_quark_from_string(key);
    c = g_list_nth(GTK_MENU_SHELL(menu)->children, (guint)idx);
    g_return_val_if_fail(c, FALSE);
    item = GTK_WIDGET(c->data);

    return GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(item), quark));
}

/************************** Particular menus ****************************/

/**
 * gwy_option_menu_interpolation:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 * @current: Interpolation type to be shown as currently selected
 *           (or -1 to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of interpolation types i.e., values of
 * #GwyInterpolationType.
 *
 * It sets object data "interpolation-type" to interpolation type for each
 * menu item (use GPOINTER_TO_INT() when retrieving it)..
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_interpolation(GCallback callback,
                              gpointer cbdata,
                              GwyInterpolationType current)
{
    static const GwyEnum entries[] = {
      /*{ "None",     GWY_INTERPOLATION_NONE,     },*/
        { "Round",    GWY_INTERPOLATION_ROUND,    },
        { "Bilinear", GWY_INTERPOLATION_BILINEAR, },
        { "Key",      GWY_INTERPOLATION_KEY,      },
        { "BSpline",  GWY_INTERPOLATION_BSPLINE,  },
        { "OMOMS",    GWY_INTERPOLATION_OMOMS,    },
        { "NNA",      GWY_INTERPOLATION_NNA,      },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "interpolation-type", callback, cbdata,
                                  current);
}

/**
 * gwy_option_menu_windowing:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 * @current: Windowing type to be shown as currently selected
 *           (or -1 to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of windowing types i.e., values of
 * #GwyWindowingType.
 *
 * It sets object data "windowing-type" to windowing type for each
 * menu item (use GPOINTER_TO_INT() when retrieving it)..
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_windowing(GCallback callback,
                              gpointer cbdata,
                              GwyWindowingType current)
{
    static const GwyEnum entries[] = {
        { "None",     GWY_WINDOWING_NONE      },
        { "Hann",     GWY_WINDOWING_HANN      },
        { "Hamming",  GWY_WINDOWING_HAMMING   },
        { "Blackman", GWY_WINDOWING_BLACKMANN },
        { "Lanzcos",  GWY_WINDOWING_LANCZOS   },
        { "Welch",    GWY_WINDOWING_WELCH     },
        { "Rect",     GWY_WINDOWING_RECT      },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "windowing-type", callback, cbdata,
                                  current);
}

/**
 * gwy_option_menu_zoom_mode:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 * @current: Zoom mode type to be shown as currently selected
 *           (or -1 to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of zoom modes i.e., values of
 * #GwyZoomMode.
 *
 * It sets object data "zoom-mode" to zoom mode for each
 * menu item (use GPOINTER_TO_INT() when retrieving it)..
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_zoom_mode(GCallback callback,
                          gpointer cbdata,
                          GwyZoomMode current)
{
    static const GwyEnum entries[] = {
        { "By square root of 2",     GWY_ZOOM_MODE_SQRT2      },
        { "By cubic root of 2",      GWY_ZOOM_MODE_CBRT2      },
        { "Integer zooms",           GWY_ZOOM_MODE_PIX4PIX    },
        { "Half-integer zooms",      GWY_ZOOM_MODE_HALFPIX    },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "zoom-mode", callback, cbdata,
                                  current);
}

/**
 * gwy_option_menu_2dcwt:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 * @current: 2D CWT wavelet type to be shown as currently selected
 *           (or -1 to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of available wavelet types.
 *
 * It sets object data "2dcwt-wavelet-type" to 2D CWT wavelet type for each
 * menu item (use GPOINTER_TO_INT() when retrieving it)..
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_2dcwt(GCallback callback,
                      gpointer cbdata,
                      Gwy2DCWTWaveletType current)
{
    static const GwyEnum entries[] = {
        { "Gaussian",          GWY_2DCWT_GAUSS      },
        { "Hat",               GWY_2DCWT_HAT        },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "2dcwt-wavelet-type", callback, cbdata,
                                  current);
}

/**
 * gwy_option_menu_sfunctions_output:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 * @current: Statistical function output type to be shown as currently selected
 *           (or -1 to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of available one-dimensional statistical functions.
 *
 * It sets object data "sf-output-type" to statistical functions output type for each
 * menu item (use GPOINTER_TO_INT() when retrieving it).
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_sfunctions_output(GCallback callback,
                           gpointer cbdata,
                           GwySFOutputType current)
{
    static const GwyEnum entries[] = {
        { "Dist. of heights",  GWY_SF_OUTPUT_DH,  },
        { "Cum. dist. of heights",  GWY_SF_OUTPUT_CDH, },
        { "Dist. of angles",  GWY_SF_OUTPUT_DA,      },
        { "Cum. dist. of angles",  GWY_SF_OUTPUT_CDA,       },
        { "Autocorrelation",  GWY_SF_OUTPUT_ACF,       },
        { "Height-height cor.",  GWY_SF_OUTPUT_HHCF,     },
        { "Power spectral density",  GWY_SF_OUTPUT_PSDF,     },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "sf-output-type", callback, cbdata,
                                  current);
}

/**
 * gwy_option_menu_direction:
 * @callback: A callback called when a menu item is activated (or %NULL for
 * @cbdata: User data passed to the callback.
 * @current: Direction selected
 *           (or -1 to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of datafield computation directions available.
 *
 * It sets object data "direction-type" to statistical functions output type
 * for each menu item (use GPOINTER_TO_INT() when retrieving it).
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_direction(GCallback callback,
                           gpointer cbdata,
                           GtkOrientation current)
{
    static const GwyEnum entries[] = {
        { "Horizontal",  GTK_ORIENTATION_HORIZONTAL,  },
        { "Vertical",  GTK_ORIENTATION_VERTICAL, },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "direction-type", callback, cbdata,
                                  current);
}


/**
 * gwy_option_menu_filter:
 * @callback: A callback called when a menu item is activated (or %NULL for
 * @cbdata: User data passed to the callback.
 * @current: Filter selected
 *           (or -1 to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of available 2D filters
 *
 * It sets object data "filter-type" to filter type
 * for each menu item (use GPOINTER_TO_INT() when retrieving it).
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_filter(GCallback callback,
                       gpointer cbdata,
                       GwyFilterType current)
{
    static const GwyEnum entries[] = {
        { "Mean value",            GWY_FILTER_MEAN,  },
        { "Median value",          GWY_FILTER_MEDIAN, },
        { "Conservative denoise",  GWY_FILTER_CONSERVATIVE, },
        { "Laplacian",             GWY_FILTER_LAPLACIAN, },
        { "Sobel gradient",        GWY_FILTER_SOBEL, },
        { "Prewitt gradient",      GWY_FILTER_PREWITT, },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "filter-type", callback, cbdata,
                                  current);
}

/**
 * gwy_option_menu_mergegrain:
 * @callback: A callback called when a menu item is activated (or %NULL for
 * @cbdata: User data passed to the callback.
 * @current: Grain merging selected
 *           (or -1 to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of available grain merging modes
 *
 * It sets object data "mergegrain-type" to grain merge type
 * for each menu item (use GPOINTER_TO_INT() when retrieving it).
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_mergegrain(GCallback callback,
                       gpointer cbdata,
                       GwyFilterType current)
{
    static const GwyEnum entries[] = {
        { "Union",            GWY_MERGE_UNION,  },
        { "Intersection",     GWY_MERGE_INTERSECTION, },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "mergegrain-type", callback, cbdata,
                                  current);
}

/**
 * gwy_option_menu_fit_line:
 * @callback: A callback called when a menu item is activated (or %NULL for
 * @cbdata: User data passed to the callback.
 * @current: Line fit mode selected
 *           (or -1 to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of available line fit modes
 *
 * It sets object data "fit-type" to line fit
 * for each menu item (use GPOINTER_TO_INT() when retrieving it).
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_fit_line(GCallback callback,
                       gpointer cbdata,
                       GwyFitLineType current)
{
    static const GwyEnum entries[] = {
        { "Fit height",    GWY_FIT_POLY_0, },
        { "Fit linear",    GWY_FIT_POLY_1, },
        { "Fit quadratic", GWY_FIT_POLY_2, },
        { "Fit cubic",     GWY_FIT_POLY_3, },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "fit-type", callback, cbdata,
                                  current);
}

/**
 * gwy_option_menu_metric_unit:
 * @callback: A callback called when a menu item is activated (or %NULL for
 * @cbdata: User data passed to the callback.
 * @from: The exponent of 10 the menu should start at (a multiple of 3, will
 *        be rounded towards zero if isn't).
 * @to: The exponent of 10 the menu should end at (a multiple of 3, will be
 *      rounded towards zero if isn't).
 * @unit: The unit to be prefixed.
 * @current: Exponent of 10 selected (a multiple of 3)
 *           (or -1 to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of units with SI prefixes in given range.
 *
 * It sets object data "metric-unit" to the exponents of 10
 * for each menu item (use GPOINTER_TO_INT() when retrieving it).
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_metric_unit(GCallback callback,
                            gpointer cbdata,
                            gint from,
                            gint to,
                            const gchar *unit,
                            gint current)
{
    static const gint min = -8;
    static const gint max = 8;

    GtkWidget *omenu;
    GwyEnum *entries;
    gchar *s;
    gint i, n;

    from = CLAMP(from/3, min, max);
    to = CLAMP(to/3, min, max);
    if (to < from)
        GWY_SWAP(gint, from, to);

    n = (to - from) + 1;
    entries = g_new(GwyEnum, n + 1);
    for (i = from; i <= to; i++) {
        s = g_strconcat(gwy_math_SI_prefix(exp(G_LN10*3.0*i)), unit, NULL);
        entries[i - from].name = s;
        entries[i - from].value = 3*i;
    }
    entries[n].name = NULL;

    omenu = gwy_option_menu_create(entries, n, "metric-unit", callback, cbdata,
                                   current);
    g_signal_connect_swapped(omenu, "destroy",
                             G_CALLBACK(gwy_option_menu_metric_unit_destroyed),
                             entries);
    return omenu;
}

static void
gwy_option_menu_metric_unit_destroyed(GwyEnum *entries)
{
    gsize i = 0;

    while (entries[i].name) {
        g_free((void*)entries[i].name);
        i++;
    }
    g_free(entries);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
