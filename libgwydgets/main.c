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
#include <string.h>
#include <math.h>

#include <glib/gstdio.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgwydgets/gwydgets.h>
#include <libgwydgets/gwyshader.h>
#include <libgwyddion/gwycontainer.h>
#include <libdraw/gwydraw.h>
#include <libprocess/level.h>

#define TEST_VECTOR_SHADE 0
#define TEST_DATA_VIEW 1
#define TEST_OPTION_MENUS 2
#define TEST_GTKDOC_INFO 3
#define TEST_GWY3DVIEW 4

#define TEST_WHAT TEST_GWY3DVIEW

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
        hid[i] = g_signal_connect(shade[i], "angle-changed",
                                  G_CALLBACK(foo_cb), GINT_TO_POINTER(i));
    }

    gtk_widget_show_all(win);
    g_signal_connect(win, "destroy", gtk_main_quit, NULL);
}
#endif
/***** ]]] VECTOR SHADE *****************************************************/

/***** DATA VIEW [[[ ********************************************************/
#if (TEST_WHAT == TEST_DATA_VIEW)

static GwyDataField*
make_data_field(void)
{
    GwyDataField *data_field;
    gdouble *data;
    gint r, i, j;

    r = random() % 400 + 40;
    data_field = gwy_data_field_new(r, r, 1, 1, FALSE);
    data = gwy_data_field_get_data(data_field);
    for (i = 0; i < r; i++) {
        for (j = 0; j < r; j++)
            data[i*r + j] = sin(i/3.0) + sin(j/7.0);
    }

    return data_field;
}

static gboolean
button_press(GwyContainer *data)
{
    GwyDataField *data_field;

    data_field = make_data_field();
    gwy_container_set_object_by_name(data, "data", data_field);

    return FALSE;
}

static void
test(void)
{
    GwyContainer *data;
    GtkWidget *window, *view;
    GwyDataField *data_field;
    GwyPixmapLayer *layer;

    data = gwy_container_new();
    data_field = make_data_field();
    gwy_container_set_object_by_name(data, "data", data_field);
    g_object_unref(data_field);

    view = gwy_data_view_new(data);
    g_object_unref(data);
    gtk_widget_set_size_request(view, 200, 200);
    g_signal_connect_swapped(view, "button-press-event",
                             G_CALLBACK(button_press), data);

    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(view), layer);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_add(GTK_CONTAINER(window), view);

    gtk_widget_show_all(window);
    g_signal_connect_swapped(window, "destroy",
                             G_CALLBACK(gtk_main_quit), data);
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
    g_signal_connect(window, "destroy",
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

/***** GWY3DVIEW [[[ ********************************************************/
#if (TEST_WHAT == TEST_GWY3DVIEW)
static void
test(void)
{
    GtkWidget *window, *view, *vbox, *notebook, *entry;
    GwyContainer *data;
    GwyDataField *dfield;

    data = gwy_container_new();
    dfield = gwy_data_field_new(200, 200, 1.0, 1.0, TRUE);
    gwy_data_field_plane_level(dfield, 0.0, 0.01, 0.03);
    gwy_container_set_object_by_name(data, "/0/data", dfield);
    gwy_container_set_string_by_name(data, "/0/base/palette",
                                     g_strdup("Spring"));
    g_object_unref(dfield);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);
    gtk_container_add(GTK_CONTAINER(window), gtk_label_new("Label"));
    gtk_widget_show_all(window);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);
    vbox = gtk_vbox_new(FALSE, 4);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    view = gwy_3d_view_new(data);
    gwy_3d_view_set_movement_type(GWY_3D_VIEW(view), GWY_3D_MOVEMENT_ROTATION);
    gtk_box_pack_start(GTK_BOX(vbox), view, TRUE, TRUE, 0);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), entry,
                             gtk_label_new("Foo"));

    entry = gtk_entry_new();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), entry,
                             gtk_label_new("Bar"));

    gtk_widget_show_all(window);
}
#endif
/***** ]]] GWY3DVIEW ********************************************************/

int
main(int argc, char *argv[])
{
    FILE *fh;
    guint32 seed;

    fh = g_fopen("/dev/urandom", "rb");
    fread(&seed, sizeof(guint32), 1, fh);
    fclose(fh);
    g_random_set_seed(seed);

    gtk_init(&argc, &argv);
    gwy_widgets_type_init();
    gwy_widgets_gl_init();
    gwy_stock_register_stock_items();
    gwy_gl_material_setup_presets();
    test();
    gtk_main();

    return 0;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
