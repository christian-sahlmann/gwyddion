#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gwyspherecoords.h"
#include "gwyvectorshade.h"

#define N 3

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
  gint i;

  gtk_init(&argc, &argv);
  win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width(GTK_CONTAINER(win), 4);

  box = gtk_vbox_new(4, TRUE);
  gtk_container_add(GTK_CONTAINER(win), box);
  for (i = 0; i < N; i++) {
    widget = gwy_vector_shade_new(NULL);
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
