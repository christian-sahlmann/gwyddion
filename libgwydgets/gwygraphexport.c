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

#include "config.h"
#include <math.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <pango/pango.h>
#include <libgwydgets/gwydgets.h>
#include <libgwyddion/gwymacros.h>
#include "gwygraph.h"

void       gwy_graph_export_pixmap(GwyGraph *graph, const gchar *filename,
                                     G_GNUC_UNUSED gboolean export_title, G_GNUC_UNUSED gboolean export_axis,
                                     G_GNUC_UNUSED gboolean export_labels)
{
    GdkColor color = { 0, 65535, 65535, 65535 };
    GdkColormap *cmap;
    GdkGC *gc;
    GdkVisual *visual;
    GdkPixmap *pixmap;
    GdkPixbuf *pixbuf;
    PangoLayout *layout;
    PangoContext *context;
    gint width, height, topheight, bottomheight, leftwidth, rightwidth;
    gint labelx, labely, labelw, labelh;

    width = (GTK_WIDGET(graph))->allocation.width;
    height = (GTK_WIDGET(graph))->allocation.height;

    topheight = (GTK_WIDGET(graph->axis_top))->allocation.height;
    bottomheight = (GTK_WIDGET(graph->axis_bottom))->allocation.height;
    rightwidth = (GTK_WIDGET(graph->axis_right))->allocation.width;
    leftwidth = (GTK_WIDGET(graph->axis_left))->allocation.width;

    labelx = (GTK_WIDGET(graph->area->lab))->allocation.x + rightwidth;
    labely = (GTK_WIDGET(graph->area->lab))->allocation.y + topheight;
    labelw = (GWY_GRAPH_LABEL(graph->area->lab))->reqwidth;
    labelh = (GWY_GRAPH_LABEL(graph->area->lab))->reqheight;
    
    visual = gdk_visual_get_best();
    pixmap = gdk_pixmap_new(NULL, width, height, visual->depth);
    gc = gdk_gc_new(pixmap);
    cmap = gdk_colormap_new(visual, FALSE);

    gdk_gc_set_rgb_fg_color(gc, &color);
    gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, width, height);

    gwy_graph_area_draw_area_on_drawable(pixmap, gc,
                                        rightwidth, topheight,
                                        width - rightwidth - leftwidth,
                                        height - topheight - bottomheight,
                                        graph->area);

    /*plot axis*/
    gwy_axis_draw_on_drawable(pixmap, gc,
                              rightwidth, 0,
                              width - rightwidth - leftwidth, topheight,
                              graph->axis_top);
    gwy_axis_draw_on_drawable(pixmap, gc,
                              rightwidth, height - bottomheight,
                              width - rightwidth - leftwidth, bottomheight,
                              graph->axis_bottom);
    gwy_axis_draw_on_drawable(pixmap, gc,
                              0, topheight,
                              rightwidth, height - topheight - bottomheight,
                              graph->axis_right);
    gwy_axis_draw_on_drawable(pixmap, gc,
                              width - leftwidth, topheight, 
                              leftwidth, height - topheight - bottomheight,
                              graph->axis_left);

    context = gdk_pango_context_get_for_screen(gdk_screen_get_default());
    pango_context_set_font_description(context, graph->area->lab->label_font);
    layout = pango_layout_new(context);
    gwy_graph_label_draw_label_on_drawable(pixmap, gc, layout,
                                           labelx, labely, labelw, labelh,
                                           graph->area->lab);

    pixbuf = gdk_pixbuf_get_from_drawable(NULL,
                                          pixmap,
                                          cmap,
                                          0, 0, 0, 0,
                                          -1, -1);
    gdk_pixbuf_save(pixbuf, filename, "png", NULL, NULL);

}

void       
gwy_graph_export_postscript(GwyGraph *graph, const gchar *filename,
                                         G_GNUC_UNUSED gboolean export_title, G_GNUC_UNUSED gboolean export_axis,
                                         G_GNUC_UNUSED gboolean export_labels)
{
    FILE *fw;
    gint width, height, hpt, vpt, areax, areay, areaw, areah, labelx, labely, labelw, labelh;
    GString *psaxis, *psarea, *pslabel;
    gint fontsize = 20;
   
    width = 600;
    height = 450;
    areax = 90;
    areay = 90;
    areaw = width - 2*areax;
    areah = height - 2*areay;
    hpt = vpt = 8;
   
    /*TODO remove the empirical quadratic part of these relations*/
    labelh = graph->area->lab->reqheight*fontsize
        /(gdouble)pango_font_description_get_size(graph->area->lab->label_font)*PANGO_SCALE
        - 0.07*fontsize*fontsize;
    labelw = graph->area->lab->reqwidth*fontsize
        /(gdouble)pango_font_description_get_size(graph->area->lab->label_font)*PANGO_SCALE
        - 0.08*fontsize*fontsize;
    labelx = width - areax - labelw - 5;
    labely = height - areay - labelh - 5;
    
    /*create stream*/
    fw = g_fopen(filename, "w");

    /*write header*/
    fprintf(fw, "%%!PS-Adobe EPSF-3.0\n");
    fprintf(fw, "%%%%Title: %s\n", filename);
    fprintf(fw, "%%%%Creator: Gwyddion\n");
    fprintf(fw, "%%%%BoundingBox: %d %d %d %d\n", 0, 0, width, height);
    fprintf(fw, "%%%%Orientation: Portrait\n");
    fprintf(fw, "%%%%EndComments\n");
    fprintf(fw, "/hpt %d def\n", hpt);
    fprintf(fw, "/vpt %d def\n", vpt);
    fprintf(fw, "/hpt2 hpt 2 mul def\n");
    fprintf(fw, "/vpt2 vpt 2 mul def\n");
    fprintf(fw, "/M {moveto} bind def\n");
    fprintf(fw, "/L {lineto} bind def\n");
    fprintf(fw, "/R {rmoveto} bind def\n");
    fprintf(fw, "/V {rlineto} bind def\n");
    fprintf(fw, "/N {newpath moveto} bind def\n");
    fprintf(fw, "/R {rmoveto} bind def\n");
    fprintf(fw, "/C {setrgbcolor} bind def\n");
    fprintf(fw, "/Pnt { stroke [] 0 setdash\n"
                "gsave 1 setlinecap M 0 0 V stroke grestore } def\n");
    fprintf(fw, "/Dia { stroke [] 0 setdash 2 copy vpt add M\n"
                "hpt neg vpt neg V hpt vpt neg V\n"
                "hpt vpt V hpt neg vpt V closepath stroke\n"
                " } def\n");
    fprintf(fw, "/Box { stroke [] 0 setdash 2 copy exch hpt sub exch vpt add M\n"
                "0 vpt2 neg V hpt2 0 V 0 vpt2 V\n"
                "hpt2 neg 0 V closepath stroke\n"
                " } def\n");
    fprintf(fw, "/Circle { stroke [] 0 setdash 2 copy\n"
                "hpt 0 360 arc stroke } def\n");
    fprintf(fw, "/Times { stroke [] 0 setdash exch hpt sub exch vpt add M\n"
                "hpt2 vpt2 neg V currentpoint stroke M\n"
                "hpt2 neg 0 R hpt2 vpt2 V stroke } def\n");
    fprintf(fw, "/Cross { stroke [] 0 setdash vpt sub M 0 vpt2 V\n"
                "currentpoint stroke M\n"
                "hpt neg vpt neg R hpt2 0 V stroke\n"
                "} def\n");
    fprintf(fw, "/Star { 2 copy Cross Times } def\n");
    fprintf(fw, "/TriU { stroke [] 0 setdash 2 copy vpt 1.12 mul add M\n"
                "hpt neg vpt -1.62 mul V\n"
                "hpt 2 mul 0 V\n"
                "hpt neg vpt 1.62 mul V closepath stroke\n"
                "} def\n");
    fprintf(fw, "/TriD { stroke [] 0 setdash 2 copy vpt 1.12 mul sub M\n"
                "hpt neg vpt 1.62 mul V\n"
                "hpt 2 mul 0 V\n"
                "hpt neg vpt -1.62 mul V closepath stroke\n"
                "} def\n");
    fprintf(fw, "/Times-Roman findfont\n");
    fprintf(fw, "%%%%EndProlog\n");

    
    /*write axises*/
    psaxis = gwy_axis_export_vector(graph->axis_bottom, areax, 0, areaw, areay, fontsize);
    fprintf(fw, "%s", psaxis->str);
    g_string_free(psaxis, TRUE);
    psaxis = gwy_axis_export_vector(graph->axis_top, areax, areay + areah, areaw, areay, fontsize);
    fprintf(fw, "%s", psaxis->str);
    g_string_free(psaxis, TRUE);
    psaxis = gwy_axis_export_vector(graph->axis_left, 0, areay, areax, areah, fontsize);
    fprintf(fw, "%s", psaxis->str);
    g_string_free(psaxis, TRUE);
    psaxis = gwy_axis_export_vector(graph->axis_right, areax + areaw, areay, areax, areah, fontsize);
    fprintf(fw, "%s", psaxis->str);
    g_string_free(psaxis, TRUE);


    /*write area*/
    psarea = gwy_graph_area_export_vector(graph->area, areax, areay, areaw, areah);
    fprintf(fw, "%s", psarea->str);

    /*write label*/
    pslabel = gwy_graph_label_export_vector(graph->area->lab, labelx, labely, labelw, labelh, fontsize);
    fprintf(fw, "%s", pslabel->str);

    /*save stream*/
    fclose(fw);
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
