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
#include <libgwyddion/gwymacros.h>

#include <string.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkimagemenuitem.h>

#include <libgwyddion/gwyddion.h>
#include <libdraw/gwygradient.h>
#include "gwyglmaterial.h"
#include "gwyoptionmenus.h"

#define BITS_PER_SAMPLE 8

enum {
    SAMPLE_HEIGHT = 16,
    SAMPLE_WIDTH = 80
};

/* TODO: when rewriting with GtkComboBox, factor out common stuff of
 * gradient and material list constructors -- they differ only in sorting
 * and rendering functions;
 * Actually, something like GwyResource would be useful, with generic
 * interface and namely with signals emitted not only when a particular
 * resource changes, but when some is created or deleted */

static GtkWidget* gwy_gradient_menu_create       (const gchar *current,
                                                  gint *current_idx);
static GtkWidget* gwy_sample_gradient_to_gtkimage(GwyGradient *gradient);
static GtkWidget* gwy_sample_gl_material_to_gtkimage(GwyGLMaterial *material);
static gint       gl_material_compare            (GwyGLMaterial *a,
                                                  GwyGLMaterial *b);
static GtkWidget* gwy_option_menu_create_real    (const GwyEnum *entries,
                                                  gint nentries,
                                                  const gchar *key,
                                                  GCallback callback,
                                                  gpointer cbdata,
                                                  gint current,
                                                  gboolean do_translate);
static void       gwy_option_menu_metric_unit_destroyed (GwyEnum *entries);

/************************** Gradient menu ****************************/
/* XXX: deprecated */

static GtkWidget*
gwy_gradient_menu_create(const gchar *current,
                         gint *current_idx)
{
    GSList *l, *entries = NULL;
    GtkWidget *menu, *image, *item, *hbox, *label;
    gint i, idx;

    gwy_inventory_foreach(gwy_gradients(),
                          gwy_hash_table_to_slist_cb, &entries);
    entries = g_slist_reverse(entries);

    menu = gtk_menu_new();

    idx = -1;
    i = 0;
    for (l = entries; l; l = g_slist_next(l)) {
        GwyGradient *gradient = (GwyGradient*)l->data;
        const gchar *name = gwy_resource_get_name(GWY_RESOURCE(gradient));

        image = gwy_sample_gradient_to_gtkimage(gradient);
        item = gtk_menu_item_new();
        hbox = gtk_hbox_new(FALSE, 6);
        label = gtk_label_new(name);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(item), hbox);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_data(G_OBJECT(item), "gradient-name", (gpointer)name);
        if (current && gwy_strequal(current, name))
            idx = i;
        i++;
    }
    g_slist_free(entries);

    if (current_idx && idx != -1)
        *current_idx = idx;

    return menu;
}

/**
 * gwy_menu_gradient:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 *
 * Creates a pop-up gradient menu.
 *
 * Returns: The newly created pop-up menu as #GtkWidget.
 **/
GtkWidget*
gwy_menu_gradient(GCallback callback,
                  gpointer cbdata)
{
    GtkWidget *menu;
    GList *c;

    menu = gwy_gradient_menu_create(NULL, NULL);
    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }

    return menu;
}

/**
 * gwy_option_menu_gradient:
 * @callback: A callback called when a menu item is activated (or %NULL for
 *            none).
 * @cbdata: User data passed to the callback.
 * @current: Gradient name to be shown as currently selected
 *           (or %NULL to use what happens to appear first).
 *
 * Creates a #GtkOptionMenu of gradients,
 * alphabetically sorted, with names and small sample images.
 *
 * It sets object data "gradient-name" to gradient definition name for each
 * menu item.
 *
 * Returns: The newly created option menu as #GtkWidget.
 **/
GtkWidget*
gwy_option_menu_gradient(GCallback callback,
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
    menu = gwy_gradient_menu_create(current, &idx);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (idx != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), idx);

    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }

    return omenu;
}

static GtkWidget*
gwy_sample_gradient_to_gtkimage(GwyGradient *gradient)
{
    GdkPixbuf *pixbuf;
    const guchar *samples;
    GtkWidget *image;
    guint rowstride;
    guchar *data;
    guint tmp[4];
    gint i, j, k, n;

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, BITS_PER_SAMPLE,
                            SAMPLE_WIDTH, SAMPLE_HEIGHT);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    gwy_gradient_sample_to_pixbuf(gradient, pixbuf);
    image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);

    return image;
}

/************************** Material menu ****************************/

static GtkWidget*
gwy_gl_material_menu_create(const gchar *current,
                            gint *current_idx)
{
    GSList *l, *entries = NULL;
    GtkWidget *menu, *image, *item, *hbox, *label;
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

        if (gwy_strequal(name, GWY_GL_MATERIAL_NONE))
            continue;

        image = gwy_sample_gl_material_to_gtkimage(gl_material);
        item = gtk_menu_item_new();
        hbox = gtk_hbox_new(FALSE, 6);
        label = gtk_label_new(name);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(item), hbox);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_data(G_OBJECT(item), "material-name", (gpointer)name);
        if (current && gwy_strequal(current, name))
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

/* XXX: magic static variables */
static GtkWidget*
gwy_sample_gl_material_to_gtkimage(GwyGLMaterial *material)
{
    static guchar *samples = NULL;
    GdkPixbuf *pixbuf;
    GtkWidget *image;
    guint rowstride;
    guchar *data;
    gint i;

    /* clean up when called with NULL */
    if (!material) {
        g_free(samples);
        samples = NULL;
        return NULL;
    }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, BITS_PER_SAMPLE,
                            SAMPLE_WIDTH, SAMPLE_HEIGHT);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    data = gdk_pixbuf_get_pixels(pixbuf);

    samples = gwy_gl_material_sample(material, SAMPLE_WIDTH, samples);
    for (i = 0; i < SAMPLE_HEIGHT; i++)
        memcpy(data + i*rowstride, samples, 4*SAMPLE_WIDTH);

    image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);

    return image;
}

static gint
gl_material_compare(GwyGLMaterial *a,
                    GwyGLMaterial *b)
{
    return strcmp(gwy_gl_material_get_name(a), gwy_gl_material_get_name(b));
}


/************************** Enum option menu ****************************/

static GtkWidget*
gwy_option_menu_create_real(const GwyEnum *entries,
                            gint nentries,
                            const gchar *key,
                            GCallback callback,
                            gpointer cbdata,
                            gint current,
                            gboolean do_translate)
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
        if (do_translate)
            item = gtk_menu_item_new_with_label(_(entries[i].name));
        else
            item = gtk_menu_item_new_with_label(entries[i].name);
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
                            GwySIUnit *unit,
                            gint current)
{
    static const gint min = -18;
    static const gint max = 18;

    GtkWidget *omenu;
    GwyEnum *entries;
    GwySIValueFormat *format = NULL;
    gint i, n;

    from = CLAMP(from/3, min, max);
    to = CLAMP(to/3, min, max);
    if (to < from)
        GWY_SWAP(gint, from, to);

    n = (to - from) + 1;
    entries = g_new(GwyEnum, n + 1);
    for (i = from; i <= to; i++) {
        format = gwy_si_unit_get_format_for_power10(unit,
                                                    GWY_SI_UNIT_FORMAT_MARKUP,
                                                    3*i, format);
        entries[i - from].name = g_strdup(format->units);
        entries[i - from].value = 3*i;
    }
    entries[n].name = NULL;
    gwy_si_unit_value_format_free(format);

    omenu = gwy_option_menu_create_real(entries, n, "metric-unit",
                                        callback, cbdata,
                                        current, FALSE);
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
