/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <libgwyddion/gwymacros.h>
#include <libdraw/gwypalette.h>
#include "gwydgets.h"

#define BITS_PER_SAMPLE 8
#define PALETTE_SAMPLE_HEIGHT 16
#define PALETTE_SAMPLE_WIDTH 80

static GtkWidget* gwy_palette_menu_create        (GCallback callback,
                                                  gpointer cbdata,
                                                  const gchar *current,
                                                  gint *current_index);
static GtkWidget* gwy_sample_palette_to_gtkimage (GwyPaletteDef *palette_def);
static void       gwy_hash_table_to_slist_cb     (gpointer key,
                                                  gpointer value,
                                                  gpointer user_data);
static gint       palette_def_compare            (GwyPaletteDef *a,
                                                  GwyPaletteDef *b);

/************************** Palette menu ****************************/

static GtkWidget*
gwy_palette_menu_create(GCallback callback,
                        gpointer cbdata,
                        const gchar *current,
                        gint *current_index)
{
    GSList *l, *entries = NULL;
    GtkWidget *menu, *image, *item;
    gint i, index;

    gwy_palette_def_foreach((GwyPaletteDefFunc)gwy_hash_table_to_slist_cb,
                            &entries);
    entries = g_slist_sort(entries, (GCompareFunc)palette_def_compare);

    menu = gtk_menu_new();

    index = -1;
    i = 0;
    for (l = entries; l; l = g_slist_next(l)) {
        GwyPaletteDef *palette_def = (GwyPaletteDef*)l->data;
        const gchar *name = gwy_palette_def_get_name(palette_def);

        image = gwy_sample_palette_to_gtkimage(palette_def);
        item = gtk_image_menu_item_new_with_label(name);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_data(G_OBJECT(item), "palette-name", (gpointer)name);
        if (callback)
            g_signal_connect(item, "activate", callback, cbdata);
        if (current && strcmp(current, name) == 0)
            index = i;
        i++;
    }
    gwy_sample_palette_to_gtkimage(NULL);
    g_slist_free(entries);

    if (current_index && index != -1)
        *current_index = index;

    return menu;
}

/**
 * gwy_palette_menu:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 *
 * Creates a pop-up palette menu.
 *
 * Returns: The newly created pop-up menu as #GtkWidget.
 **/
GtkWidget*
gwy_palette_menu(GCallback callback,
                 gpointer cbdata)
{
    return gwy_palette_menu_create(callback, cbdata, NULL, NULL);
}

/**
 * gwy_palette_option_menu:
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
gwy_palette_option_menu(GCallback callback,
                        gpointer cbdata,
                        const gchar *current)
{
    GtkWidget *omenu, *menu;
    gint index;

    index = -1;
    omenu = gtk_option_menu_new();
    menu = gwy_palette_menu_create(callback, cbdata, current, &index);

    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (index != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), index);

    return omenu;
}

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

    if (!palette_def) {
        g_free(samples);
        samples = NULL;
        gwy_object_unref(palette);
        return NULL;
    }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 0, BITS_PER_SAMPLE,
                            PALETTE_SAMPLE_WIDTH, PALETTE_SAMPLE_HEIGHT);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    data = gdk_pixbuf_get_pixels(pixbuf);

    if (!palette)
        palette = (GwyPalette*)gwy_palette_new(palette_def);
    else
        gwy_palette_set_palette_def(palette, palette_def);

    samples = gwy_palette_sample(palette, PALETTE_SAMPLE_WIDTH, samples);
    for (i = 0; i < PALETTE_SAMPLE_WIDTH; i++)
        memcpy(data + 3*i, samples + 4*i, 3);
    for (i = 1; i < PALETTE_SAMPLE_HEIGHT; i++)
        memcpy(data + i*rowstride, data, 3*PALETTE_SAMPLE_WIDTH);
    gwy_object_unref(palette);

    image = gtk_image_new_from_pixbuf(pixbuf);
    return image;
}

static void
gwy_hash_table_to_slist_cb(gpointer key,
                           gpointer value,
                           gpointer user_data)
{
    GSList **list = (GSList**)user_data;

    *list = g_slist_prepend(*list, value);
}

static gint
palette_def_compare(GwyPaletteDef *a,
                    GwyPaletteDef *b)
{
    /* XXX: should use gwy_palette_def_get_name() */
    return strcmp(a->name, b->name);
}


/************************** Enum menus ****************************/

/**
 * gwy_option_menu_create:
 * @entries: Option menu entries.
 * @nentries: The number of entries.
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
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_create(const GwyOptionMenuEntry *entries,
                       gint nentries,
                       const gchar *key,
                       GCallback callback,
                       gpointer cbdata,
                       gint current)
{
    GtkWidget *omenu, *menu, *item;
    GQuark quark;
    gint i, index;

    quark = g_quark_from_static_string(key);
    omenu = gtk_option_menu_new();
    menu = gtk_menu_new();

    index = -1;
    for (i = 0; i < nentries; i++) {
        item = gtk_menu_item_new_with_label(_(entries[i].name));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_qdata(G_OBJECT(item), quark,
                          GINT_TO_POINTER(entries[i].value));
        if (callback)
            g_signal_connect(item, "activate", callback, cbdata);
        if (entries[i].value == current)
            index = i;
    }

    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (index != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), index);

    return omenu;
}

/**
 * gwy_option_menu_set_history:
 * @option_menu: An option menu created by gwy_option_menu_create().
 * @key: Value object data key.
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
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));
    quark = g_quark_from_static_string(key);
    i = 0;
    for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c)) {
        if (GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(c->data), quark))
            == current) {
            gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), i);
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * gwy_interpolation_option_menu:
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
gwy_interpolation_option_menu(GCallback callback,
                              gpointer cbdata,
                              GwyInterpolationType current)
{
    static const GwyOptionMenuEntry entries[] = {
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
 * gwy_windowing_option_menu:
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
gwy_windowing_option_menu(GCallback callback,
                              gpointer cbdata,
                              GwyWindowingType current)
{
    static const GwyOptionMenuEntry entries[] = {
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
 * gwy_zoom_mode_option_menu:
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
gwy_zoom_mode_option_menu(GCallback callback,
                          gpointer cbdata,
                          GwyZoomMode current)
{
    static const GwyOptionMenuEntry entries[] = {
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
 * gwy_2dcwt_option_menu:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 * @current: 2D CWT wavelet type to be shown as currently selected
 *           (or -1 to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of available wavelet types.
 *
 * It sets object data "2dcwt_wavelet_type" to 2D CWT wavelet type for each
 * menu item (use GPOINTER_TO_INT() when retrieving it)..
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_2dcwt_option_menu(GCallback callback,
                      gpointer cbdata,
                      Gwy2DCWTWaveletType current)
{
    static const GwyOptionMenuEntry entries[] = {
        { "Gaussian",          GWY_2DCWT_GAUSS      },
        { "Hat",               GWY_2DCWT_HAT        },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "2dcwt_wavelet_type", callback, cbdata,
                                  current);
}


/************************** Table attaching ****************************/

/**
 * gwy_table_attach_spinbutton:
 * @table: A #GtkTable.
 * @row: Table row to attach to.
 * @name: The label before @adj.
 * @units: The label after @adj.
 * @adj: An adjustment to create spinbutton from.
 *
 * Attaches a spinbutton with two labels to a table.
 *
 * Returns: The spinbutton as a #GtkWidget.
 **/
GtkWidget*
gwy_table_attach_spinbutton(GtkWidget *table,
                            gint row,
                            const gchar *name,
                            const gchar *units,
                            GtkObject *adj)
{
    GtkWidget *spin;

    g_return_val_if_fail(GTK_IS_TABLE(table), NULL);
    if (adj)
        g_return_val_if_fail(GTK_IS_ADJUSTMENT(adj), NULL);
    else
        adj = gtk_adjustment_new(0, 0, 0, 0, 0, 0);

    spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gwy_table_attach_row(table, row, name, units, spin);

    return spin;
}

/**
 * gwy_table_attach_row:
 * @table: A #GtkTable.
 * @row: Table row to attach to.
 * @name: The label before @middle_widget.
 * @units: The label after @adj.
 * @middle_widget: A widget.
 *
 * Attaches a widget with two labels to a table.
 **/
void
gwy_table_attach_row(GtkWidget *table,
                     gint row,
                     const gchar *name,
                     const gchar *units,
                     GtkWidget *middle_widget)
{
    GtkWidget *label;

    g_return_if_fail(GTK_IS_TABLE(table));
    g_return_if_fail(GTK_IS_WIDGET(middle_widget));

    label = gtk_label_new(units);
    gtk_table_attach(GTK_TABLE(table), label,
                     2, 3, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    label = gtk_label_new_with_mnemonic(name);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    gtk_table_attach(GTK_TABLE(table), middle_widget,
                     1, 2, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), middle_widget);
}


GtkWidget*
gwy_fft_output_menu(GCallback callback,
                              gpointer cbdata,
                              GwyFFTOutputType current)
{
    static const GwyOptionMenuEntry entries[] = {
        { "Real + Imaginary",  GWY_FFT_OUTPUT_REAL_IMG,  },
        { "Module + Phase",    GWY_FFT_OUTPUT_MOD_PHASE, },
        { "Real",              GWY_FFT_OUTPUT_REAL,      },
        { "Imaginary",         GWY_FFT_OUTPUT_IMG,       },
        { "Module",            GWY_FFT_OUTPUT_MOD,       },
        { "Phase",             GWY_FFT_OUTPUT_PHASE,     },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "fft-output-type", callback, cbdata,
                                  current);
}


/************************** Utils ****************************/

/**
 * gwy_dialog_prevent_delete_cb:
 *
 * Returns %TRUE.
 *
 * The purpose of this function is to be used as a callback connected to the
 * "delete_event" of non-modal dialogs so that they can hide instead of
 * being destroyed.  This is achieved by returning %TRUE from the
 * "delete_event" callback.
 *
 * See #GtkDialog source code for the gory details...
 *
 * Returns: %TRUE.
 **/
gboolean
gwy_dialog_prevent_delete_cb(void)
{
    return TRUE;
}

/************************** Documentation ****************************/
/**
 * GwyOptionMenuEntry:
 * @value: Item (enum) value.
 * @entry: Item labels.
 *
 * Item specification for gwy_option_menu_create().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
