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
#include <stdlib.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkiconfactory.h>
#include <gdk/gdkkeysyms.h>
#include <libgwyddion/gwymacros.h>
#include "gwystock.h"

static void   register_toolbox_icons (const gchar *pixmap_path,
                                      GtkIconFactory *icon_factory);
static gchar* guess_pixmap_path      (void);

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
    GtkIconFactory *icon_factory;
    gchar *pixmap_path;

    pixmap_path = guess_pixmap_path();
    g_return_if_fail(pixmap_path);

    icon_factory = gtk_icon_factory_new();
    register_toolbox_icons(pixmap_path, icon_factory);
    gtk_icon_factory_add_default(icon_factory);
    g_free(pixmap_path);
}

static void
register_toolbox_icons(const gchar *pixmap_path,
                       GtkIconFactory *icon_factory)
{
  /* Textual stock items */
    static const GtkStockItem stock_items[] = {
        { GWY_STOCK_BOLD, "Bold", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_CROP, "Crop", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_FIT_PLANE, "Fit plane", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_FIT_TRIANGLE, "Fit plane", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_GRAPH, "Graph", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_ITALIC, "Italic", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_NONE, "None", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_POINTER, "Pointer", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_POINTER_MEASURE, "Pointer measure", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_ROTATE, "Rotate", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_SCALE, "Scale", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_SHADER, "Shade", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_SUBSCRIPT, "Subscript", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_SUPERSCRIPT, "Superscript", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_ZOOM_1_1, "Zoom 1:1", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_ZOOM_FIT, "Zoom to fit", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_ZOOM_IN, "Zoom in", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_ZOOM_OUT, "Zoom out", 0, GDK_VoidSymbol, "gwy" },
    };
    static const GtkStockItem gwyddion_stock = {
        GWY_STOCK_GWYDDION, "Gwyddion", 0, GDK_VoidSymbol, "gwy"
    };
    guint i;

    gtk_stock_add_static(stock_items, G_N_ELEMENTS(stock_items));
    for (i = 0; i < G_N_ELEMENTS(stock_items); i++) {
        GtkIconSet *icon_set = gtk_icon_set_new();
        const gchar *id = stock_items[i].stock_id;
        GtkIconSource *icon_source = gtk_icon_source_new();
        gchar *filename;

        filename = g_strdup_printf("%s/%s-%u.png", pixmap_path, id, 24);
        gwy_debug("%s: `%s': %d",
                  __FUNCTION__, filename,
                  g_file_test(filename, G_FILE_TEST_EXISTS));
        gtk_icon_source_set_filename(icon_source, filename);
        gtk_icon_set_add_source(icon_set, icon_source);
        g_free(filename);
        gtk_icon_factory_add(icon_factory, id, icon_set);
    }

    gtk_icon_size_register(GWY_ICON_SIZE_ABOUT, 60, 60);
    {
        GtkIconSet *icon_set = gtk_icon_set_new();
        const gchar *id = gwyddion_stock.stock_id;
        GtkIconSource *icon_source = gtk_icon_source_new();
        gchar *filename;

        filename = g_strdup_printf("%s/%s-%u.png", pixmap_path, id, 60);
        gwy_debug("%s: `%s': %d",
                  __FUNCTION__, filename,
                  g_file_test(filename, G_FILE_TEST_EXISTS));
        gtk_icon_source_set_filename(icon_source, filename);
        gtk_icon_set_add_source(icon_set, icon_source);
        g_free(filename);
        gtk_icon_factory_add(icon_factory, id, icon_set);
    }
}

static gchar*
guess_pixmap_path(void)
{
    gchar *b, *p, *q;

    /* try argv[0] */
    p = g_strdup(g_get_prgname());
    if (!g_path_is_absolute(p)) {
        b = g_get_current_dir();
        q = g_build_filename(b, p, NULL);
        g_free(p);
        g_free(b);
        p = q;
    }
    q = g_path_get_dirname(p);
    b = g_path_get_dirname(q);
    g_free(q);
    if (g_path_is_absolute(b)) {
        p = g_build_filename(b, "pixmaps", NULL);
        if (g_file_test(p, G_FILE_TEST_IS_DIR)) {
            g_free(b);
            gwy_debug("Icon path (from argv[0]): %s", p);
            return p;
        }
        g_free(p);
    }
    g_free(b);

    /* try to find gwyddion in path, this is namely for windows */
    p = g_find_program_in_path("gwyddion");
    if (p) {
        if (g_path_is_absolute(p)) {
            b = g_path_get_dirname(p);
            q = g_path_get_dirname(b);
            g_free(b);
            g_free(p);
            p = g_build_filename(q, "pixmaps", NULL);
            g_free(q);
            if (g_file_test(p, G_FILE_TEST_IS_DIR)) {
                g_free(b);
                gwy_debug("Icon path (from $PATH): %s", p);
                return p;
            }
        }
        g_free(p);
    }

    /* try GWY_PIXMAP_DIR, try it after the previous ones, so an uninstalled
     * version gets its own directory, not the system one */
    if (g_file_test(GWY_PIXMAP_DIR, G_FILE_TEST_IS_DIR)) {
        gwy_debug("Icon path (from GWY_PIXMAP_DIR): %s", GWY_PIXMAP_DIR);
        return g_strdup(GWY_PIXMAP_DIR);
    }

    /* as last resort, try current directory */
    p = g_get_current_dir();
    q = g_build_filename(p, b, "pixmaps", NULL);
    g_free(p);
    if (g_file_test(q, G_FILE_TEST_IS_DIR)) {
        gwy_debug("Icon path (from cwd): %s", q);
        return q;
    }

    return NULL;
}

#if 0
static GList*
slurp_icon_directory(const gchar *path)
{
    GList *icons = NULL;
    GDir *gdir = NULL;
    GError *err = NULL;

    gdir = g_dir_open(path, 0, &err);
    if (!gdir) {
        g_warning("Cannot open directory `%s': %s", path, err->message);
        return NULL;
    }

    return icons;
}

/**
 * Filename format: <gwy_foobar>-<size>[.<state>].png
 **/
static GtkIconSource*
file_to_icon_source(const gchar *filename,
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
    const gtk_sizes[] = {
        { 16, GTK_ICON_SIZE_MENU },
        { 18, GTK_ICON_SIZE_SMALL_TOOLBAR },
        { 20, GTK_ICON_SIZE_BUTTON },
        { 24, GTK_ICON_SIZE_LARGE_TOOLBAR },
        { 32, GTK_ICON_SIZE_DND },
        { 48, GTK_ICON_SIZE_DIALOG },
    };
    GtkIconSource *icon_source;
    GtkIconSize gtksize = -1;
    GtkStateType state = -1;
    gchar *sz, *st, *p;
    gint size;
    gsize i;

    *id = g_strdup(filename);
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
    gtk_icon_source_set_filename(icon_source, filename);
    gtk_icon_source_set_size(icon_source, gtksize);
    gtk_icon_source_set_direction_wildcarded(icon_source, TRUE);
    gtk_icon_source_set_size_wildcarded(icon_source, FALSE);
    if (state != (GtkStateType)-1) {
        gtk_icon_source_set_state_wildcarded(icon_source, FALSE);
        gtk_icon_source_set_state(icon_source, state);
    }

    return icon_source;
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
