#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <libgwydgets/gwydgets.h>
#include <libgwydgets/gwyaxis.h>
#include <libgwydgets/gwygraphlabel.h>
#include <libgwydgets/gwygrapharea.h>
#include <libgwydgets/gwygraph.h>



static void destroy( GtkWidget *widget, gpointer data )
{
        gtk_main_quit ();
}

int
main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *axis, *label, *area, *graph, *foo;
    GError *err = NULL;
    gint i;

    double xs[100];
    double ys[100];
    double xr[100];
    double yr[100];
    double xp[100];
    double yp[100];
    double xu[100];
    double yu[100];
    double xv[100];
    double yv[100];
         
    for (i=0; i<100; i++){xs[i]=xr[i]=xp[i]=xu[i]=xv[i]=i; ys[i]=i; yr[i]=(double)i*i/40; 
        yp[i]=20*sin((double)i*15/100); yu[i]=50-(double)i/2; yv[i]=20*sin((double)i*15/100)-15*cos((double)(i-3)*15/100);}
    
    gtk_init(&argc, &argv);
    
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy), NULL);

    gtk_container_set_border_width (GTK_CONTAINER (window), 0);

    /* 
    axis = gwy_axis_new(1, -0.1, 100, "ble");
    gwy_axis_set_logarithmic(axis, 0);
    
    gtk_container_add (GTK_CONTAINER (window), axis);
    gtk_widget_show (axis);
    */

    /*
    label = gwy_graph_label_new();
    gtk_container_add (GTK_CONTAINER (window), label);
    gtk_widget_show (label);
    */

    /* 
    area = gwy_graph_area_new(NULL,NULL);
    gtk_layout_set_size(GTK_LAYOUT(area), 320, 240);
    gtk_container_add (GTK_CONTAINER (window), area);
    */
    /*
    foo = gtk_label_new("Foo!");
    gtk_layout_put(GTK_LAYOUT(area), foo, 10, 20);
    */

    
    graph = gwy_graph_new();
    gwy_graph_add_datavalues(graph, xs, ys, 100);
    gwy_graph_add_datavalues(graph, xr, yr, 100);
    gwy_graph_add_datavalues(graph, xp, yp, 100);
    gwy_graph_add_datavalues(graph, xu, yu, 100);
    gwy_graph_add_datavalues(graph, xv, yv, 100);
    
    
    gtk_container_add (GTK_CONTAINER (window), graph);
    gtk_widget_show (graph);
    
    gtk_widget_show_all(window);
    
    gtk_main();

    return 0;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
