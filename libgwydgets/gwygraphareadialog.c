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

#include <math.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <stdio.h>
#include "gwydgets.h"
#include "gwyoptionmenus.h"
#include "gwygraph.h"
#include "gwygraphareadialog.h"
#include "gwygraphmodel.h"
#include "gwygraphcurvemodel.h"
#include <libgwyddion/gwymacros.h>


#define GWY_GRAPH_AREA_DIALOG_TYPE_NAME "GwyGraphAreaDialog"
#define BITS_PER_SAMPLE 24
#define POINT_SAMPLE_HEIGHT 20
#define POINT_SAMPLE_WIDTH 20
#define LINE_SAMPLE_HEIGHT 20
#define LINE_SAMPLE_WIDTH 50


GwyEnum curve_type[] = {
    { N_("Points"),          GWY_GRAPH_CURVE_POINTS      },
    { N_("Line"),            GWY_GRAPH_CURVE_LINE        },
    { N_("Line + points"),   GWY_GRAPH_CURVE_LINE_POINTS },
    { N_("Hidden"),          GWY_GRAPH_CURVE_HIDDEN      },
};




static void     gwy_graph_area_dialog_class_init       (GwyGraphAreaDialogClass *klass);
static void     gwy_graph_area_dialog_init             (GwyGraphAreaDialog *dialog);
static void     gwy_graph_area_dialog_finalize         (GObject *object);
static gboolean gwy_graph_area_dialog_delete           (GtkWidget *widget,
                                                          GdkEventAny *event);

static GtkWidget* gwy_point_menu_create                  (const GwyGraphPointType current, 
                                                          gint *current_idx);
GtkWidget*      gwy_option_menu_point                    (GCallback callback, 
                                                          gpointer cbdata,
                                                          const GwyGraphPointType current);
static GtkWidget* gwy_sample_point_to_gtkimage           (GwyGraphPointType type);
static void     pointtype_cb                             (GObject *item, 
                                                          GwyGraphAreaDialog *dialog);
static GtkWidget* gwy_line_menu_create                   (const GdkLineStyle current, 
                                                          gint *current_idx);
GtkWidget*      gwy_option_menu_line                     (GCallback callback, 
                                                          gpointer cbdata,
                                                          const GdkLineStyle current);
static GtkWidget* gwy_sample_line_to_gtkimage            (GdkLineStyle type);
static void     linetype_cb                              (GObject *item, 
                                                          GwyGraphAreaDialog *dialog);
static void     color_change_cb                          (GtkWidget *color_button,
                                                          GwyGraphAreaDialog *dialog);
static void     label_change_cb                          (GtkWidget *button,
                                                          GwyGraphAreaDialog *dialog);
static void     refresh                                  (GwyGraphAreaDialog *dialog);
static void     curvetype_changed_cb                     (GObject *item, 
                                                          GwyGraphAreaDialog *dialog);
static void     linesize_changed_cb                      (GtkObject *adj,
                                                          GwyGraphAreaDialog *dialog);
static void     pointsize_changed_cb                      (GtkObject *adj,
                                                          GwyGraphAreaDialog *dialog);



static GtkDialogClass *parent_class = NULL;

GType
gwy_graph_area_dialog_get_type(void)
{
    static GType gwy_graph_area_dialog_type = 0;

    if (!gwy_graph_area_dialog_type) {
        static const GTypeInfo gwy_graph_area_dialog_info = {
            sizeof(GwyGraphAreaDialogClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_area_dialog_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphAreaDialog),
            0,
            (GInstanceInitFunc)gwy_graph_area_dialog_init,
            NULL,
        };
        gwy_debug("");
        gwy_graph_area_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
                                                      GWY_GRAPH_AREA_DIALOG_TYPE_NAME,
                                                      &gwy_graph_area_dialog_info,
                                                      0);

    }

    return gwy_graph_area_dialog_type;
}

static void
gwy_graph_area_dialog_class_init(GwyGraphAreaDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    gwy_debug("");
    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_graph_area_dialog_finalize;
    widget_class->delete_event = gwy_graph_area_dialog_delete;
}

static gboolean
gwy_graph_area_dialog_delete(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventAny *event)
{
    gwy_debug("");
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_graph_area_dialog_init(GwyGraphAreaDialog *dialog)
{
    GtkWidget *label, *table, *button;
    gint row = 0;
    gwy_debug("");

    table = gtk_table_new(2, 8, FALSE);
   
    label = gtk_label_new_with_mnemonic(_("_Curve label:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    dialog->curve_label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(dialog->curve_label), 0.0, 0.5);
    button = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button), dialog->curve_label);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NORMAL);
    gtk_table_attach(GTK_TABLE(table), button,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(label_change_cb), dialog);
    
    row++; 
    
    label = gtk_label_new_with_mnemonic(_("_Plot style:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    dialog->curvetype_menu = gwy_option_menu_create(curve_type, G_N_ELEMENTS(curve_type),
                                                    "curve-type",
                                                    G_CALLBACK(curvetype_changed_cb), 
                                                    dialog,
                                                    0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), dialog->curvetype_menu);
    gtk_table_attach(GTK_TABLE(table), dialog->curvetype_menu,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;
    
    label = gtk_label_new_with_mnemonic(_("P_lot color:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    dialog->color_button = gwy_color_button_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), dialog->color_button);
    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(dialog->color_button),
                                   FALSE);
    gtk_table_attach(GTK_TABLE(table), dialog->color_button,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(dialog->color_button, "clicked",
                     G_CALLBACK(color_change_cb), dialog);
    row++;
    
        
    dialog->pointtype_menu = gwy_option_menu_point(G_CALLBACK(pointtype_cb), 
                                                   dialog, 0);

    gwy_table_attach_row(table, row, _("Point type:"), "",
                         dialog->pointtype_menu);
    row++;
   
    dialog->pointsize = gtk_adjustment_new(6, 1, 50, 1, 5, 0);
    gwy_table_attach_spinbutton(table, row, _("Point size:"), NULL,
                                dialog->pointsize);
    g_signal_connect(dialog->pointsize, "value_changed",
                     G_CALLBACK(pointsize_changed_cb), dialog);
    row++;
    
    dialog->linetype_menu = gwy_option_menu_line(G_CALLBACK(linetype_cb), 
                                                   dialog, 0);

    gwy_table_attach_row(table, row, _("Line type:"), "",
                         dialog->linetype_menu);
    row++;
     
    dialog->linesize = gtk_adjustment_new(6, 1, 50, 1, 5, 0);
    gwy_table_attach_spinbutton(table, row, _("Line thickness:"), NULL,
                                dialog->linesize);
    g_signal_connect(dialog->linesize, "value_changed",
                     G_CALLBACK(linesize_changed_cb), dialog);
     
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_APPLY, GTK_RESPONSE_APPLY);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                      table);

    dialog->curve_model = NULL;
}

static void
pointtype_cb(GObject *item, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    if (dialog->curve_model == NULL) return;
    
    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    cmodel->point_type = GPOINTER_TO_INT(g_object_get_data(item, "point-type"));
}

static void
linetype_cb(GObject *item, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    if (dialog->curve_model == NULL) return;
    
    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    cmodel->line_style = GPOINTER_TO_INT(g_object_get_data(item, "line-type"));
}

GtkWidget *
gwy_graph_area_dialog_new()
{
    gwy_debug("");
    return GTK_WIDGET (g_object_new (gwy_graph_area_dialog_get_type (), NULL));
}

static void
gwy_graph_area_dialog_finalize(GObject *object)
{
    gwy_debug("");

    g_return_if_fail(GWY_IS_GRAPH_AREA_DIALOG(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}



static GtkWidget*
gwy_point_menu_create(const GwyGraphPointType current,
                      gint *current_idx)
{
    GtkWidget *menu, *image, *item, *hbox;
    guint l; 
    gint idx;
                                                                                                                                                                 menu = gtk_menu_new();
                                                                                                                                                                 idx = -1;
                                                                                                  
    for (l = 0; l<=GWY_GRAPH_POINT_DIAMOND; l++) {
        image = gwy_sample_point_to_gtkimage(l);
        item = gtk_menu_item_new();
        hbox = gtk_hbox_new(FALSE, 6);
        /*label = gtk_label_new(name);*/
        /*gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);*/
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
        /*gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);*/
        gtk_container_add(GTK_CONTAINER(item), hbox);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_data(G_OBJECT(item), "point-type", GINT_TO_POINTER(l));
        if (current && (current == l))
            idx = l;
    }

    if (current_idx && idx != -1)
        *current_idx = idx;
    return menu;        
}

GtkWidget*
gwy_option_menu_point(GCallback callback,
                        gpointer cbdata,
                        const GwyGraphPointType current)
{
    GtkWidget *omenu, *menu;
    GList *c;
    gint idx;

    idx = -1;
    omenu = gtk_option_menu_new();
    g_object_set_data(G_OBJECT(omenu), "gwy-option-menu",
                      GINT_TO_POINTER(TRUE));
    menu = gwy_point_menu_create(current, &idx);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (idx != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), idx);
    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }
    return omenu;
}

static GtkWidget*
gwy_sample_point_to_gtkimage(GwyGraphPointType type)
{
    GdkPixmap *pixmap;
    GtkWidget *image;
    GdkColor gcl;
    GwyRGBA color;
    GdkGC *gc;

    pixmap = gdk_pixmap_new(NULL,
                            POINT_SAMPLE_WIDTH, POINT_SAMPLE_HEIGHT,
                            BITS_PER_SAMPLE);

    gc = gdk_gc_new(pixmap);
    gcl.pixel = 0x0ffffff;
    gdk_gc_set_foreground(gc, &gcl);
    
    color.r = color.b = color.g = 0;
    color.a = 1;
    
    gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, POINT_SAMPLE_WIDTH, POINT_SAMPLE_HEIGHT);
    
    gwy_graph_draw_point(pixmap, NULL,
                           POINT_SAMPLE_WIDTH/2, POINT_SAMPLE_HEIGHT/2,
                           type,
                           10, &color, FALSE);
       
    image = gtk_image_new_from_pixmap(pixmap, NULL);
    g_object_unref(pixmap);
    g_object_unref(gc);
    return image;
}

static GtkWidget*
gwy_line_menu_create(const GdkLineStyle current,
                      gint *current_idx)
{
    GtkWidget *menu, *image, *item, *hbox;
    guint l; 
    gint idx;
                                                                                                                                                                 menu = gtk_menu_new();
                                                                                                                                                                 idx = -1;
                                                                                                  
    for (l = 0; l<=GDK_LINE_DOUBLE_DASH; l++) {
        image = gwy_sample_line_to_gtkimage(l);
        item = gtk_menu_item_new();
        hbox = gtk_hbox_new(FALSE, 6);
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(item), hbox);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_object_set_data(G_OBJECT(item), "line-type", GINT_TO_POINTER(l));
        if (current && (current == l))
            idx = l;
    }

    if (current_idx && idx != -1)
        *current_idx = idx;
    return menu;        
}

GtkWidget*
gwy_option_menu_line(GCallback callback,
                        gpointer cbdata,
                        const GdkLineStyle current)
{
    GtkWidget *omenu, *menu;
    GList *c;
    gint idx;

    idx = -1;
    omenu = gtk_option_menu_new();
    g_object_set_data(G_OBJECT(omenu), "gwy-option-menu",
                      GINT_TO_POINTER(TRUE));
    menu = gwy_line_menu_create(current, &idx);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    if (idx != -1)
        gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), idx);
    if (callback) {
        for (c = GTK_MENU_SHELL(menu)->children; c; c = g_list_next(c))
            g_signal_connect(c->data, "activate", callback, cbdata);
    }
    return omenu;
}

static GtkWidget*
gwy_sample_line_to_gtkimage(GdkLineStyle type)
{
    GdkPixmap *pixmap;
    GtkWidget *image;
    GdkColor gcl;
    GwyRGBA color;
    GdkGC *gc;

    pixmap = gdk_pixmap_new(NULL,
                            LINE_SAMPLE_WIDTH, LINE_SAMPLE_HEIGHT,
                            BITS_PER_SAMPLE);

    gc = gdk_gc_new(pixmap);
    gcl.pixel = 0x0ffffff;
    gdk_gc_set_foreground(gc, &gcl);
    
    color.r = color.b = color.g = 0;
    color.a = 1;
    
    gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, LINE_SAMPLE_WIDTH, LINE_SAMPLE_HEIGHT);
    
    gwy_graph_draw_line(pixmap, NULL,
                           1, LINE_SAMPLE_HEIGHT/2,
                           LINE_SAMPLE_WIDTH - 1, LINE_SAMPLE_HEIGHT/2,
                           type,
                           3, &color);
       
    image = gtk_image_new_from_pixmap(pixmap, NULL);
    g_object_unref(pixmap);
    g_object_unref(gc);
    return image;
}

static void
color_changed_cb(GtkColorSelectionDialog *selector, gint arg1, gpointer user_data)
{
    GdkColor gcl;
    GwyGraphAreaDialog *dialog;
    GwyGraphCurveModel *cmodel;

    dialog = GWY_GRAPH_AREA_DIALOG(user_data);
    if (dialog->curve_model == NULL) return;
    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
     

    if (arg1 == GTK_RESPONSE_OK) {
        gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(selector->colorsel), &gcl);
        gwy_rgba_from_gdk_color(&cmodel->color, &gcl);  
        refresh(dialog);
        
    }
    gtk_widget_destroy(GTK_WIDGET(selector));
}

static void
refresh(GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    if (dialog->curve_model == NULL) return;
    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    
    gwy_color_button_set_color(GWY_COLOR_BUTTON(dialog->color_button), &cmodel->color);
    gtk_label_set_markup(GTK_LABEL(dialog->curve_label), cmodel->description->str);

    gwy_option_menu_set_history(dialog->curvetype_menu, "curve-type",
                                cmodel->type);
    gwy_option_menu_set_history(dialog->pointtype_menu, "point-type",
                                cmodel->point_type);
    gwy_option_menu_set_history(dialog->linetype_menu, "line-type",
                                cmodel->line_style);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(dialog->pointsize), cmodel->point_size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(dialog->linesize), cmodel->line_size);
     
}

static void     
color_change_cb(GtkWidget *color_button, GwyGraphAreaDialog *dialog)
{
    GdkColor gcl;
    GwyGraphCurveModel *cmodel;
    GtkColorSelectionDialog* selector;
                                                                  
    if (dialog->curve_model == NULL) return;
    
    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    
    selector = GTK_COLOR_SELECTION_DIALOG(
                                        gtk_color_selection_dialog_new("Select curve color"));
    g_signal_connect(GTK_WIDGET(selector), "response",
                     G_CALLBACK(color_changed_cb), dialog);
                     
    
    gwy_rgba_to_gdk_color(&cmodel->color, &gcl);  
    gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(selector->colorsel), &gcl);
    gtk_widget_show(GTK_WIDGET(selector));
    
}

static void     
label_change_cb(GtkWidget *button, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    GwyAxisDialog* selector;
    gint response;
                                                                  
    if (dialog->curve_model == NULL) return;
    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
   
    selector = GWY_AXIS_DIALOG(gwy_axis_dialog_new());
    gwy_sci_text_set_text(GWY_SCI_TEXT(selector->sci_text), cmodel->description->str);
    
    response = gtk_dialog_run(GTK_DIALOG(selector));
    if (response == GTK_RESPONSE_APPLY)
    {
       /*g_string_assign(cmodel->description, gwy_sci_text_get_text(GWY_SCI_TEXT(selector->sci_text)));*/
        gwy_graph_curve_model_set_description(cmodel, gwy_sci_text_get_text(GWY_SCI_TEXT(selector->sci_text)));
       /*refresh(dialog);*/
    }    
    
    gtk_widget_destroy(GTK_WIDGET(selector));
}


void        
gwy_graph_area_dialog_set_curve_data(GtkWidget *dialog, GObject *cmodel)
{
    GwyGraphAreaDialog *gadialog = GWY_GRAPH_AREA_DIALOG(dialog);

    gadialog->curve_model = cmodel;
    refresh(GWY_GRAPH_AREA_DIALOG(dialog));
}

static void     
curvetype_changed_cb(GObject *item, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    if (dialog->curve_model == NULL) return;
    
    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    gwy_graph_curve_model_set_curve_type(cmodel,GPOINTER_TO_INT(g_object_get_data(item, "curve-type")));
}
 
static void     
linesize_changed_cb(GtkObject *adj, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    if (dialog->curve_model == NULL) return;

    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    gwy_graph_curve_model_set_curve_line_size(cmodel, gtk_adjustment_get_value(GTK_ADJUSTMENT(adj)));
}

static void     
pointsize_changed_cb(GtkObject *adj, GwyGraphAreaDialog *dialog)
{
    GwyGraphCurveModel *cmodel;
    if (dialog->curve_model == NULL) return;

    cmodel = GWY_GRAPH_CURVE_MODEL(dialog->curve_model);
    gwy_graph_curve_model_set_curve_point_size(cmodel, gtk_adjustment_get_value(GTK_ADJUSTMENT(adj)));
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
