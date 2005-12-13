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
#include <time.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygraphdata.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphcurvemodel.h>
#include <libgwydgets/gwygraphwindow.h>


static void
destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

int
main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *gwindow, *axis, *label, *data, *area, *graph, *foo;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *model;
    gint i, k;
    GString *str1, *str2, *str3, *str4, *str5;
    GwyDataLine *dln;

    double xp[100];
    double yp[100];
    double ppow;

    srand(time(0));

    ppow = 1000*pow(10,10000*rand()/RAND_MAX)*pow(10,100000*rand()/RAND_MAX);
    for (i = 0; i < 10; i++) {
        xp[i] =  -1*(rand()-0.5)/rand()*pow(10,10000*rand()/RAND_MAX)*pow(10,100000*rand()/RAND_MAX);
        xp[i]*=xp[i]*xp[i]*pow(10, xp[i]*pow(10000000, xp[i]))*ppow;
        xp[i] -= rand()/RAND_MAX*xp[i];
        yp[i] =  -1*(rand()-0.5)/rand()*pow(10,10000*rand()/RAND_MAX)*pow(10,100000*rand()/RAND_MAX);
        yp[i]*=yp[i]*yp[i]*pow(10, yp[i]*pow(10000000, xp[i]))*ppow;
        yp[i] -= rand()/RAND_MAX*xp[i];
        printf("%g %g\n", xp[i], yp[i]);
    }
    gtk_init(&argc, &argv);

   /*window = gtk_window_new(GTK_WINDOW_TOPLEVEL);*/

   /*g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);*/

   /*gtk_container_set_border_width(GTK_CONTAINER(window), 0);*/

    /*
       label = gwy_graph_label_new();
       gtk_container_add (GTK_CONTAINER (window), label);
       gtk_widget_show (label);
     */

    /*
       area = gwy_graph_area_new(NULL,NULL);
       gtk_layout_set_size(GTK_LAYOUT(area), 320, 240);
       gtk_container_add (GTK_CONTAINER (window), area);

       foo = gtk_label_new("Foo!");
       gtk_layout_put(GTK_LAYOUT(area), foo, 10, 20);
     */



    dln = (GwyDataLine *)gwy_data_line_new(200, 200, 1);

    gmodel = gwy_graph_model_new();
    graph = gwy_graph_new(gmodel);
    
    model = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_data(model, xp, yp, 10);
    gwy_graph_curve_model_set_description(model, "parabola");

    gwy_graph_model_add_curve(gmodel, model);
   
    gwy_graph_model_set_label_visible(gmodel, TRUE);
    
    gwindow = gwy_graph_window_new(graph);
    
    //gtk_container_add (GTK_CONTAINER (window), gwindow);

   /*gwy_graph_enable_user_input(graph, TRUE);*/
   /*printf("show!\n");*/
    gtk_widget_set_size_request(gwindow, 600, 400);
    gtk_widget_show (gwindow);

    g_signal_connect(G_OBJECT(gwindow), "destroy", G_CALLBACK(destroy), NULL);

    printf("show all!\n");
    gtk_widget_show_all(gwindow);

    gtk_main();

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
