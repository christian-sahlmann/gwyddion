/* @(#) $Id$ */

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
        { GWY_STOCK_ZOOM_IN, "Zoom in", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_ZOOM_OUT, "Zoom out", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_ZOOM_1_1, "Zoom 1:1", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_ZOOM_FIT, "Zoom to fit", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_FIT_PLANE, "Fit plane", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_FIT_TRIANGLE, "Fit plane", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_GRAPH, "Graph", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_CROP, "Crop", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_SCALE, "Scale", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_ROTATE, "Rotate", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_SHADER, "Shade", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_BOLD, "Bold", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_ITALIC, "Italic", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_SUBSCRIPT, "Subscript", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_SUPERSCRIPT, "Superscript", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_POINTER, "Pointer", 0, GDK_VoidSymbol, "gwy" },
        { GWY_STOCK_POINTER_MEASURE, "Pointer measure", 0, GDK_VoidSymbol, "gwy" },
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
