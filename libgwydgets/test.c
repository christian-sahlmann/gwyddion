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
#include <libgwydgets/gwycoloraxis.h>
#include <libgwydgets/gwystock.h>



static void destroy( GtkWidget *widget, gpointer data )
{
        gtk_main_quit ();
}

int
main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *axis, *label, *area, *graph, *foo;
    GwyPalette *pal;
    GError *err = NULL;
    gint i;
    GString *str1, *str2, *str3, *str4, *str5;
    GwyGraphAreaCurveParams par;
    GwyGraphAutoProperties prop;
    GwyDataLine *dln;

    double xs[100];
    double ys[100];
    double xr[10];
    double yr[10];
    double xp[100];
    double yp[100];
    double xu[10];
    double yu[10];
    double xv[20];
    double yv[20];
         
    for (i=0; i<100; i++){xs[i]=i-7; xp[i]=i; ys[i]=(double)i*i/40; 
        yp[i]=20*sin((double)i*15/100);
        
        if (i<20) {
            xv[i]=5.0*i + 12;
            yv[i]=20*sin((double)i*5.0*15/100)-15*cos((double)(i*5.0-3)*15/100) - 30;
           }
        if (i<10) {
            xr[i]=20+i*3;
            yr[i]=150+4*i;
            xu[i]=20+i*7;
            yu[i]=50 - (double)i*4;
           }
        }
    
    gtk_init(&argc, &argv);
    
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy), NULL);

    gtk_container_set_border_width (GTK_CONTAINER (window), 0);


    gwy_palette_def_setup_presets();
    gwy_stock_register_stock_items();
    /*
    pal = (GwyPalette*) gwy_palette_new(NULL);
    gwy_palette_set_by_name(pal, GWY_PALETTE_OLIVE);
    
    axis = gwy_color_axis_new(GTK_ORIENTATION_VERTICAL, 12e-9, 125e-3, pal);
    
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

    foo = gtk_label_new("Foo!");
    gtk_layout_put(GTK_LAYOUT(area), foo, 10, 20);
    */

  
    dln = (GwyDataLine *) gwy_data_line_new(200, 200, 1);
    
    str1 = g_string_new("parabola");
    str2 = g_string_new("kousek");
    str3 = g_string_new("sinus");
    str4 = g_string_new("cosi");
    str5 = g_string_new("jiny sinus");

    par.is_line = 1;
    par.is_point = 1;
    par.line_style = GDK_LINE_SOLID;
    par.line_size = 1;
    par.point_type = 0;
    par.point_size = 8;
    par.color.pixel = 0x00000000;
    
    graph = gwy_graph_new();
    gwy_graph_get_autoproperties(graph, &prop);
    prop.is_point = 0; 
    gwy_graph_set_autoproperties(graph, &prop);

    gwy_graph_add_datavalues(graph, xs, ys, 100, str1, NULL);
    gwy_graph_add_datavalues(graph, xp, yp, 100, str3, NULL);
    prop.is_point = 1;
    gwy_graph_set_autoproperties(graph, &prop);
    gwy_graph_add_datavalues(graph, xr, yr, 10, str2, NULL);
    gwy_graph_add_datavalues(graph, xu, yu, 10, str4, &par);
    prop.is_line = 0;
    gwy_graph_set_autoproperties(graph, &prop);
    gwy_graph_add_dataline(graph, dln, 0, str5, NULL);
    
    
    gtk_container_add (GTK_CONTAINER (window), graph);
    gtk_widget_show (graph);

    gtk_widget_show_all(window);
    
    gtk_main();

    return 0;
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
