/* @(#) $Id$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgwydgets/gwydgets.h>
#include <libgwyddion/gwycontainer.h>
#include <libdraw/gwypalette.h>
#include <libprocess/datafield.h>

/***** VECTOR SHADE [[[ *****************************************************/
#define N 5

static gulong hid[N];
static GwySphereCoords *coords[N];

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
};

static void
foo_cb(GwySphereCoords *c, gpointer p)
{
    gint n = GPOINTER_TO_INT(p);
    gint i;
    gdouble theta, phi;

    for (i = 0; i < N; i++)
        g_signal_handler_block(G_OBJECT(coords[i]), hid[i]);
    theta = gwy_sphere_coords_get_theta(c);
    phi = gwy_sphere_coords_get_phi(c);
    i = (n + 1)%N;
    gwy_sphere_coords_set_value(coords[i],
                                gwy_sphere_coords_get_theta(coords[i]), phi);
    i = (n + N-1)%N;
    gwy_sphere_coords_set_value(coords[i],
                                theta, gwy_sphere_coords_get_phi(coords[i]));
    for (i = 0; i < N; i++)
        g_signal_handler_unblock(G_OBJECT(coords[i]), hid[i]);
}

static void
vector_shade_test(void)
{
    GtkWidget *win, *widget, *box;
    GObject *pal, *pdef;
    gint i;

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(win), 4);

    box = gtk_vbox_new(4, TRUE);
    gtk_container_add(GTK_CONTAINER(win), box);
    for (i = 0; i < N; i++) {
        widget = gwy_vector_shade_new(NULL);
        pdef = gwy_palette_def_new(palettes[g_random_int()
                                            % G_N_ELEMENTS(palettes)]);
        pal = gwy_palette_new(GWY_PALETTE_DEF(pdef));
        g_object_unref(pdef);
        gwy_grad_sphere_set_palette(
            GWY_GRAD_SPHERE(gwy_vector_shade_get_grad_sphere(GWY_VECTOR_SHADE(widget))),
            GWY_PALETTE(pal));
        gtk_box_pack_start(GTK_BOX(box), widget, TRUE, TRUE, 0);
        coords[i] = gwy_vector_shade_get_sphere_coords(GWY_VECTOR_SHADE(widget));
        hid[i] = g_signal_connect(G_OBJECT(coords[i]), "value_changed",
                                  G_CALLBACK(foo_cb), GINT_TO_POINTER(i));
    }

    gtk_widget_show_all(win);
    g_signal_connect(G_OBJECT(win), "destroy", gtk_main_quit, NULL);
}
/***** ]]] VECTOR SHADE *****************************************************/

/***** DATA VIEW [[[ ********************************************************/
#define FILENAME "data_field.object"

static void
quit_callback(GwyDataWindow *data_window, GObject *data)
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
data_view_test(void)
{
    GwyContainer *data;
    GtkWidget *window, *view;
    GObject *data_field;
    GwyDataViewLayer *layer;
    gchar *buffer = NULL;
    gsize size = 0;
    gsize pos = 0;
    GError *err = NULL;
    GwyPaletteDef *palette_def;
    GwyPalette *palette;

    /* FIXME: this is necessary to initialize the object system */
    g_type_class_ref(gwy_data_field_get_type());

    g_file_get_contents(FILENAME, &buffer, &size, &err);
    g_assert(!err);
    data = GWY_CONTAINER(gwy_serializable_deserialize(buffer, size, &pos));

    data_field = gwy_container_get_object_by_name(data, "/0/data");

    view = gwy_data_view_new(data);
    layer = (GwyDataViewLayer*)gwy_layer_basic_new();
    palette_def = (GwyPaletteDef*)gwy_palette_def_new(GWY_PALETTE_YELLOW);
    palette = (GwyPalette*)gwy_palette_new(palette_def);
    g_object_unref(palette_def);
    gwy_layer_basic_set_palette(layer, palette);
    g_object_unref(palette);
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(view), layer);

    layer = (GwyDataViewLayer*)gwy_layer_select_new();
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(view), layer);

    g_object_unref(data);
    g_object_unref(data_field);

    window = gwy_data_window_new(GWY_DATA_VIEW(view));

    gtk_widget_show_all(window);
    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(quit_callback), data);
}
/***** ]]] DATA VIEW ********************************************************/

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

    vector_shade_test();
    //data_view_test();
    gtk_main();

    return 0;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
