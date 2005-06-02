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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgwydgets/gwydgets.h>
#include <libgwydgets/gwyshader.h>
#include <libgwyddion/gwycontainer.h>
#include <libdraw/gwydraw.h>
#include <libprocess/datafield.h>

#define TEST_VECTOR_SHADE 0
#define TEST_DATA_VIEW 1
#define TEST_OPTION_MENUS 2
#define TEST_GTKDOC_INFO 3

#define TEST_WHAT TEST_VECTOR_SHADE

/***** VECTOR SHADE [[[ *****************************************************/
#if (TEST_WHAT == TEST_VECTOR_SHADE)
#define N 5

static gulong hid[N];
static GtkWidget *shade[N];

static const char *palettes[] = {
    GWY_PALETTE_GRAY,
    GWY_PALETTE_RED,
    GWY_PALETTE_GREEN,
    GWY_PALETTE_BLUE,
    GWY_PALETTE_YELLOW,
    GWY_PALETTE_PINK,
    GWY_PALETTE_OLIVE,
    GWY_PALETTE_BW1,
    GWY_PALETTE_BW2,
    GWY_PALETTE_RAINBOW1,
    GWY_PALETTE_RAINBOW2,
    GWY_PALETTE_WARM,
    GWY_PALETTE_COLD,
    "Spring",
    "Body",
};

static void
foo_cb(GwyShader *c, gpointer p)
{
    gint n = GPOINTER_TO_INT(p);
    gint i;

    for (i = 0; i < N; i++)
        g_signal_handler_block(G_OBJECT(shade[i]), hid[i]);
    gwy_shader_set_phi(GWY_SHADER(shade[(n+N-1) % N]), gwy_shader_get_phi(c));
    gwy_shader_set_theta(GWY_SHADER(shade[(n+1) % N]), gwy_shader_get_theta(c));
    for (i = 0; i < N; i++)
        g_signal_handler_unblock(G_OBJECT(shade[i]), hid[i]);
}

static void
test(void)
{
    GtkWidget *win, *box;
    const gchar *pal;
    gint i;

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(win), 4);

    box = gtk_vbox_new(4, TRUE);
    gtk_container_add(GTK_CONTAINER(win), box);
    for (i = 0; i < N; i++) {
        pal = palettes[g_random_int_range(0, G_N_ELEMENTS(palettes))];
        shade[i] = gwy_shader_new(pal);
        gwy_shader_set_update_policy(GWY_SHADER(shade[i]),
                                     GTK_UPDATE_DELAYED);
        gtk_box_pack_start(GTK_BOX(box), shade[i], TRUE, TRUE, 0);
        hid[i] = g_signal_connect(shade[i], "angle_changed",
                                  G_CALLBACK(foo_cb), GINT_TO_POINTER(i));
    }

    gtk_widget_show_all(win);
    g_signal_connect(G_OBJECT(win), "destroy", gtk_main_quit, NULL);
}
#endif
/***** ]]] VECTOR SHADE *****************************************************/

/***** DATA VIEW [[[ ********************************************************/
#if (TEST_WHAT == TEST_DATA_VIEW)
#define FILENAME "test.gwy"

static void
quit_callback(GObject *data)
{
    FILE *fh;
    guchar *buffer = NULL;
    gsize size = 0;

    fh = fopen(FILENAME, "wb");
    buffer = gwy_serializable_serialize(data, buffer, &size);
    fwrite(buffer, 1, size, fh);
    fclose(fh);

    gtk_main_quit();
}

static void
test(void)
{
    GwyContainer *data;
    GtkWidget *window, *view;
    GObject *data_field;
    GwyDataViewLayer *layer;
    gchar *buffer = NULL;
    gsize size = 0;
    gsize pos = 0;
    GError *err = NULL;
    GwyPalette *palette;

    /* FIXME: this is necessary to initialize the object system */
    g_type_class_ref(gwy_data_field_get_type());

    g_file_get_contents(FILENAME, &buffer, &size, &err);
    g_assert(!err);
    data = GWY_CONTAINER(gwy_serializable_deserialize(buffer+4, size-1, &pos));

    data_field = gwy_container_get_object_by_name(data, "/0/data");

    view = gwy_data_view_new(data);
    layer = (GwyDataViewLayer*)gwy_layer_basic_new();
    palette = (GwyPalette*)(gwy_palette_new(NULL));
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(view), GWY_PIXMAP_LAYER(layer));
    gwy_palette_set_by_name(palette, GWY_PALETTE_RAINBOW2);
    gwy_layer_basic_set_palette(GWY_LAYER_BASIC(layer), palette);
    g_object_unref(palette);

    layer = (GwyDataViewLayer*)gwy_layer_axes_new();
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(view), GWY_VECTOR_LAYER(layer));

    g_object_unref(data);

    window = gwy_data_window_new(GWY_DATA_VIEW(view));

    gtk_widget_show_all(window);
    g_signal_connect_swapped(G_OBJECT(window), "destroy",
                             G_CALLBACK(quit_callback), data);
}
#endif
/***** ]]] DATA VIEW ********************************************************/

/***** MENUS [[[ ************************************************************/
#if (TEST_WHAT == TEST_OPTION_MENUS)
static void
menu_callback(GObject *menu_item, const gchar *which_menu)
{
    if (strcmp(which_menu, "palette") == 0) {
        g_message("Palette: %s",
                  (gchar*)g_object_get_data(menu_item, "palette-name"));
        return;
    }
    if (strcmp(which_menu, "interpolation") == 0) {
        g_message("Interpolation: %d",
                  GPOINTER_TO_INT(g_object_get_data(menu_item,
                                                    "interpolation-type")));
        return;
    }
    if (strcmp(which_menu, "windowing") == 0) {
        g_message("Windowing: %d",
                  GPOINTER_TO_INT(g_object_get_data(menu_item,
                                                    "windowing-type")));
        return;
    }
    if (strcmp(which_menu, "zoom_mode") == 0) {
        g_message("Zoom mode: %d",
                  GPOINTER_TO_INT(g_object_get_data(menu_item,
                                                    "zoom-mode")));
        return;
    }
    if (strcmp(which_menu, "metric_unit") == 0) {
        g_message("Metric unit: %d",
                  GPOINTER_TO_INT(g_object_get_data(menu_item,
                                                    "metric-unit")));
        return;
    }
    g_assert_not_reached();
}

static void
test(void)
{
    GtkWidget *window, *table, *omenu, *widget;
    GwySIUnit *unit;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(window), 4);
    table = gtk_table_new(5, 2, FALSE);
    gtk_container_add(GTK_CONTAINER(window), table);

    widget = gtk_label_new("Palettes: ");
    gtk_table_attach_defaults(GTK_TABLE(table), widget, 0, 1, 0, 1);
    omenu = gwy_option_menu_palette(G_CALLBACK(menu_callback),
                                    "palette",
                                    GWY_PALETTE_OLIVE);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 0, 1);

    widget = gtk_label_new("Interpolation types: ");
    gtk_table_attach_defaults(GTK_TABLE(table), widget, 0, 1, 1, 2);
    omenu = gwy_option_menu_interpolation(G_CALLBACK(menu_callback),
                                          "interpolation",
                                          GWY_INTERPOLATION_BILINEAR);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 1, 2);

    widget = gtk_label_new("Windowing types: ");
    gtk_table_attach_defaults(GTK_TABLE(table), widget, 0, 1, 2, 3);
    omenu = gwy_option_menu_windowing(G_CALLBACK(menu_callback),
                                      "windowing",
                                      GWY_WINDOWING_HAMMING);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 2, 3);

    widget = gtk_label_new("Zoom modes: ");
    gtk_table_attach_defaults(GTK_TABLE(table), widget, 0, 1, 3, 4);
    omenu = gwy_option_menu_zoom_mode(G_CALLBACK(menu_callback),
                                      "zoom_mode",
                                      GWY_ZOOM_MODE_CBRT2);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 3, 4);

    unit = gwy_si_unit_new("Hz");
    widget = gtk_label_new("Metric units: ");
    gtk_table_attach_defaults(GTK_TABLE(table), widget, 0, 1, 4, 5);
    omenu = gwy_option_menu_metric_unit(G_CALLBACK(menu_callback),
                                        "metric_unit",
                                        -12, 3, unit, -6);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 4, 5);
    g_object_unref(unit);

    gtk_widget_show_all(window);
    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);
}
#endif
/***** ]]] MENUS ************************************************************/

/***** GTKDOC INFO [[[ ******************************************************/
#if (TEST_WHAT == TEST_GTKDOC_INFO)
static void
test(void)
{
    GType type;
    guint n;
    guint *signals;

    type = GWY_TYPE_DATA_VIEW_LAYER;
    g_message("G_TYPE_IS_CLASSED(type) = %d", G_TYPE_IS_CLASSED(type));
    g_type_class_ref(type);
    signals = g_signal_list_ids(type, &n);
    g_message("n of signals = %u", n);
}
#endif
/***** ]]] GTKDOC INFO ******************************************************/

int
main(int argc, char *argv[])
{
    FILE *fh;
    guint32 seed;

    fh = fopen("/dev/urandom", "rb");
    fread(&seed, sizeof(guint32), 1, fh);
    fclose(fh);
    g_random_set_seed(seed);

    gtk_init(&argc, &argv);
    gwy_palette_def_setup_presets();
    gwy_draw_type_init();
    gwy_stock_register_stock_items();
    test();
    gtk_main();

    return 0;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
