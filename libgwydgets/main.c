#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gwyspherecoords.h"
#include "gwyvectorshade.h"
#include <libdraw/gwypalette.h>

#define N 4

static gulong hid[N];
static GwySphereCoords *coords[N];

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

int
main(int argc, char *argv[])
{
    GtkWidget *win, *widget, *box;
    GObject *pal;
    gint i;
    FILE *fh;
    guint32 seed;

    fh = fopen("/dev/urandom", "rb");
    fread(&seed, sizeof(guint32), 1, fh);
    fclose(fh);
    g_random_set_seed(seed);

    gtk_init(&argc, &argv);
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(win), 4);

    box = gtk_vbox_new(4, TRUE);
    gtk_container_add(GTK_CONTAINER(win), box);
    for (i = 0; i < N; i++) {
        widget = gwy_vector_shade_new(NULL);
        pal = gwy_palette_new(512);
        gwy_palette_setup_predef(GWY_PALETTE(pal), g_random_int()%10);
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
    gtk_main();

    return 0;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
