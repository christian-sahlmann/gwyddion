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

GdkPixbuf*    gwy_graph_export_pixmap(GwyGraph *graph, 
                                     G_GNUC_UNUSED gboolean export_title, G_GNUC_UNUSED gboolean export_axis,
                                     G_GNUC_UNUSED gboolean export_labels,
                                     GdkPixbuf *pixbuf)
{
    GdkColor color = { 0, 65535, 65535, 65535 };
    GdkColormap *cmap;
    GdkGC *gc;
    GdkVisual *visual;
    GdkPixmap *pixmap;
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
    return pixbuf;

}

GString *       
gwy_graph_export_postscript(GwyGraph *graph, 
                                         G_GNUC_UNUSED gboolean export_title, G_GNUC_UNUSED gboolean export_axis,
                                         G_GNUC_UNUSED gboolean export_labels,
                                         GString* string)
{
    gint width, height, hpt, vpt, areax, areay, areaw, areah, labelx, labely, labelw, labelh;
    GString *psaxis, *psarea, *pslabel;
    GwyGraphModel *gmodel = graph->graph_model;
    gint fontsize = 20;
    gint borderskip = 30;
   
    width = (GTK_WIDGET(graph))->allocation.width;
    height = (GTK_WIDGET(graph))->allocation.height;
    areax = 90;
    areay = 90;
    
    if (gwy_axis_is_visible(graph->axis_left) && gwy_axis_is_visible(graph->axis_right)) 
        areaw = width - 2*areax;
    else if (gwy_axis_is_visible(graph->axis_left) || gwy_axis_is_visible(graph->axis_right))
        areaw = width - areax - borderskip;
    else areaw = width - 2*borderskip;
    
    if (gwy_axis_is_visible(graph->axis_top) && gwy_axis_is_visible(graph->axis_bottom)) 
        areah = height - 2*areay;
    else if (gwy_axis_is_visible(graph->axis_top) || gwy_axis_is_visible(graph->axis_bottom))
        areah = height - areay - borderskip;
    else areah = height - 2*borderskip;
     
    hpt = vpt = 1;
   
    /*TODO remove the empirical part of these relations*/
    labelh = 5 + 15*gwy_graph_model_get_n_curves(gmodel)*fontsize
        /(gdouble)pango_font_description_get_size(graph->area->lab->label_font)*PANGO_SCALE;
    labelw = graph->area->lab->reqwidth*fontsize
        /(gdouble)pango_font_description_get_size(graph->area->lab->label_font)*PANGO_SCALE
        - 0.08*fontsize*fontsize;
    labelx = width - areax - labelw - 5;
    labely = height - areay - labelh - 5;
    

    /*write header*/
    g_string_append_printf(string, "%%!PS-Adobe EPSF-3.0\n");
    g_string_append_printf(string, "%%%%Title: Gwyddion vector graph export\n");
    g_string_append_printf(string, "%%%%Creator: Gwyddion\n");
    g_string_append_printf(string, "%%%%BoundingBox: %d %d %d %d\n", 0, 0, width, height);
    g_string_append_printf(string, "%%%%Orientation: Portrait\n");
    g_string_append_printf(string, "%%%%EndComments\n");
    g_string_append_printf(string, "/hpt %d def\n", hpt);
    g_string_append_printf(string, "/vpt %d def\n", vpt);
    g_string_append_printf(string, "/hpt2 hpt 2 mul def\n");
    g_string_append_printf(string, "/vpt2 vpt 2 mul def\n");
    g_string_append_printf(string, "/M {moveto} bind def\n");
    g_string_append_printf(string, "/L {lineto} bind def\n");
    g_string_append_printf(string, "/R {rmoveto} bind def\n");
    g_string_append_printf(string, "/V {rlineto} bind def\n");
    g_string_append_printf(string, "/N {newpath moveto} bind def\n");
    g_string_append_printf(string, "/R {rmoveto} bind def\n");
    g_string_append_printf(string, "/C {setrgbcolor} bind def\n");
    g_string_append_printf(string, "/Pnt { stroke [] 0 setdash\n"
                "gsave 1 setlinecap M 0 0 V stroke grestore } def\n");
    g_string_append_printf(string, "/Dia { stroke [] 0 setdash 2 copy vpt add M\n"
                "hpt neg vpt neg V hpt vpt neg V\n"
                "hpt vpt V hpt neg vpt V closepath stroke\n"
                " } def\n");
    g_string_append_printf(string, "/Box { stroke [] 0 setdash 2 copy exch hpt sub exch vpt add M\n"
                "0 vpt2 neg V hpt2 0 V 0 vpt2 V\n"
                "hpt2 neg 0 V closepath stroke\n"
                " } def\n");
    g_string_append_printf(string, "/Circle { stroke [] 0 setdash 2 copy\n"
                "hpt 0 360 arc stroke } def\n");
    g_string_append_printf(string, "/Times { stroke [] 0 setdash exch hpt sub exch vpt add M\n"
                "hpt2 vpt2 neg V currentpoint stroke M\n"
                "hpt2 neg 0 R hpt2 vpt2 V stroke } def\n");
    g_string_append_printf(string, "/Cross { stroke [] 0 setdash vpt sub M 0 vpt2 V\n"
                "currentpoint stroke M\n"
                "hpt neg vpt neg R hpt2 0 V stroke\n"
                "} def\n");
    g_string_append_printf(string, "/Star { 2 copy Cross Times } def\n");
    g_string_append_printf(string, "/TriU { stroke [] 0 setdash 2 copy vpt 1.12 mul add M\n"
                "hpt neg vpt -1.62 mul V\n"
                "hpt 2 mul 0 V\n"
                "hpt neg vpt 1.62 mul V closepath stroke\n"
                "} def\n");
    g_string_append_printf(string, "/TriD { stroke [] 0 setdash 2 copy vpt 1.12 mul sub M\n"
                "hpt neg vpt 1.62 mul V\n"
                "hpt 2 mul 0 V\n"
                "hpt neg vpt -1.62 mul V closepath stroke\n"
                "} def\n");
    g_string_append_printf(string, "/Times-Roman findfont\n");
    g_string_append_printf(string, "%%%%EndProlog\n");

    
    /*write axises*/
    if (gwy_axis_is_visible(graph->axis_bottom))
    {
        psaxis = gwy_axis_export_vector(graph->axis_bottom, areax, 0, areaw, areay, fontsize);
        g_string_append_printf(string, "%s", psaxis->str);
        g_string_free(psaxis, TRUE);
    }
    if (gwy_axis_is_visible(graph->axis_top))
    {
        psaxis = gwy_axis_export_vector(graph->axis_top, areax, areay + areah, areaw, areay, fontsize);
        g_string_append_printf(string, "%s", psaxis->str);
        g_string_free(psaxis, TRUE);
    }
    if (gwy_axis_is_visible(graph->axis_right))
    {
        psaxis = gwy_axis_export_vector(graph->axis_right, 0, areay, areax, areah, fontsize);
        g_string_append_printf(string, "%s", psaxis->str);
        g_string_free(psaxis, TRUE);
    }
    if (gwy_axis_is_visible(graph->axis_left))
    {
        psaxis = gwy_axis_export_vector(graph->axis_left, areax + areaw, areay, areax, areah, fontsize);
        g_string_append_printf(string, "%s", psaxis->str);
        g_string_free(psaxis, TRUE);
    }

    /*write area*/
    psarea = gwy_graph_area_export_vector(graph->area, areax, areay, areaw, areah);
    g_string_append_printf(string, "%s", psarea->str);

    /*write label*/
    pslabel = gwy_graph_label_export_vector(graph->area->lab, labelx, labely, labelw, labelh, fontsize);
    g_string_append_printf(string, "%s", pslabel->str);

    /*save stream*/
    return string;
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
