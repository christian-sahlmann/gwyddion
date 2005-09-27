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
#include <glib-object.h>
#include <gtk/gtk.h>

#include "gwydgets.h"
#include "gwyoptionmenus.h"
#include "gwygraph.h"
#include "gwygraphwindowmeasuredialog.h"
#include "gwygraphmodel.h"
#include "gwygraphcurvemodel.h"
#include <libgwyddion/gwymacros.h>


#define GWY_GRAPH_MEASURE_DIALOG_TYPE_NAME "GwyGraphWindowMeasureDialog"


static void     gwy_graph_window_measure_dialog_class_init       (GwyGraphWindowMeasureDialogClass *klass);
static void     gwy_graph_window_measure_dialog_init             (GwyGraphWindowMeasureDialog *dialog);
static void     gwy_graph_window_measure_dialog_finalize         (GObject *object);
static gboolean gwy_graph_window_measure_dialog_delete           (GtkWidget *widget,
                                                                    GdkEventAny *event);

static void     selection_updated_cb                               (GwyGraph *graph, 
                                                                    GwyGraphWindowMeasureDialog *dialog);
static void     index_changed_cb                                 (GwyGraphWindowMeasureDialog *dialog);
static void     method_cb                                        (GtkWidget *combo,
                                                                  GwyGraphWindowMeasureDialog *dialog);

GwyEnum method_type[] = {
    {N_("Intersections"),         METHOD_INTERSECTIONS  },
    {N_("Points anywhere"),       METHOD_CROSSES },
};


static gint NMAX = 10;
static gulong selection_id = 0;
static GtkDialogClass *parent_class = NULL;


GType
gwy_graph_window_measure_dialog_get_type(void)
{
    static GType gwy_graph_window_measure_dialog_type = 0;

    if (!gwy_graph_window_measure_dialog_type) {
        static const GTypeInfo gwy_graph_window_measure_dialog_info = {
            sizeof(GwyGraphWindowMeasureDialogClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_window_measure_dialog_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphWindowMeasureDialog),
            0,
            (GInstanceInitFunc)gwy_graph_window_measure_dialog_init,
            NULL,
        };
        gwy_debug("");
        gwy_graph_window_measure_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
                                                      GWY_GRAPH_MEASURE_DIALOG_TYPE_NAME,
                                                      &gwy_graph_window_measure_dialog_info,
                                                      0);

    }

    return gwy_graph_window_measure_dialog_type;
}

static void
gwy_graph_window_measure_dialog_class_init(GwyGraphWindowMeasureDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    gwy_debug("");
    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_graph_window_measure_dialog_finalize;
    widget_class->delete_event = gwy_graph_window_measure_dialog_delete;
}

static gboolean
gwy_graph_window_measure_dialog_delete(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventAny *event)
{
    gwy_debug("");
    gtk_widget_hide(widget);

    return TRUE;
}

static void
value_label(GtkWidget *label, gdouble value, GString *str)
{
    if ((fabs(value) <= 1e5 && fabs(value) > 1e-2) || fabs(value) == 0)
        g_string_printf(str, "%.2f", value);
    else
        g_string_printf(str, "%.2e", value);
                                                                                                                                                                         
    gtk_label_set_text(GTK_LABEL(label), str->str);
}
                                                                                                                                                             
static void
header_label(GtkWidget *table, gint row, gint col,
            const gchar *header, const gchar *unit,
            GString *str)
{
    GtkWidget *label;
                                                                                                                                                                     
    label = gtk_label_new(NULL);
    if (unit)
        g_string_printf(str, "<b>%s</b> [%s]", header, unit);
    else
        g_string_printf(str, "<b>%s</b>", header);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
                                                                                                                                                                                     
    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, col, col+1, row, row+1,
                     GTK_FILL | GTK_EXPAND, 0, 2, 2);
}


static void
gwy_graph_window_measure_dialog_init(G_GNUC_UNUSED GwyGraphWindowMeasureDialog *dialog)
{
   gwy_debug("");
}

static gdouble
get_y_for_x(GwyGraph *graph, gdouble x, gint curve, gboolean *ret)
{
    GwyGraphModel *model;
    GwyGraphCurveModel *cmodel;
    gdouble *xdata, *ydata;
    gint ndata, i, pos;
    
    model = GWY_GRAPH_MODEL(gwy_graph_get_model(graph));
    if (model->ncurves <= curve) {
        *ret = FALSE;
        return 0;
    }
    
    cmodel = GWY_GRAPH_CURVE_MODEL(model->curves[curve]);
    xdata = gwy_graph_curve_model_get_xdata(cmodel);
    ydata = gwy_graph_curve_model_get_ydata(cmodel);
    ndata = gwy_graph_curve_model_get_ndata(cmodel);
    
    pos = -1;
    for (i = 0; i < (ndata - 1); i++)
    {
        if (xdata[i] < x && xdata[i+1] >= x) {
            pos = i; 
            break;
        }
    }
    if (pos == -1) {
        *ret = FALSE;
        return 0;
    }
    *ret = TRUE;
    return ydata[pos] + (ydata[pos+1] - ydata[pos])*(x - (xdata[pos]))/(xdata[pos+1]-xdata[pos]);
}

static void
selection_updated_cb(GwyGraph *graph, GwyGraphWindowMeasureDialog *dialog)
{
    GtkWidget *label;
    GString *str;
    gint i, n;
    gdouble *spoints = NULL;
    gdouble x=0, y=0, xp=0, yp=0;
    gboolean ret = TRUE,  prevret = TRUE;

    g_return_if_fail(GWY_IS_GRAPH(graph));
    if (!(gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_POINTS || 
        gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_XLINES)) return;

    if ((n = gwy_graph_get_selection_number(graph))>0)
    {
        if (gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_POINTS)
            spoints = (gdouble *) g_malloc(2*gwy_graph_get_selection_number(graph)*sizeof(gdouble));
        else
            spoints = (gdouble *) g_malloc(gwy_graph_get_selection_number(graph)*sizeof(gdouble));
    }
    
    gwy_graph_get_selection(graph, spoints);
    
    /*update points data */
    str = g_string_new("");
    for (i = 0; i < NMAX; i++) {
        if (i < n) {
            if (i)
            {
                xp = x;
                yp = y;
                prevret = ret;
            }
            
            if (gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_POINTS)
            {
                x = spoints[2*i];
                y = spoints[2*i + 1];
            }
            else if (gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_XLINES)
            {
                x = spoints[i];
                y = get_y_for_x(graph, x, dialog->curve_index - 1, &ret);
            }
    
            label = g_ptr_array_index(dialog->pointx, i);
            value_label(label, x/dialog->x_mag, str);

            label = g_ptr_array_index(dialog->pointy, i);
            if (ret)
                value_label(label, y/dialog->y_mag, str);
            else
                gtk_label_set_text(GTK_LABEL(label), "");                

            if (!i)
                continue;

            label = g_ptr_array_index(dialog->distx, i);
            value_label(label, (x - xp)/dialog->x_mag, str);

            
            label = g_ptr_array_index(dialog->disty, i);
            if (ret && prevret)
                value_label(label, (y - yp)/dialog->y_mag, str);
            else
                gtk_label_set_text(GTK_LABEL(label), "");
            
            label = g_ptr_array_index(dialog->slope, i);
            if (ret && prevret)
                value_label(label,
                        180.0/G_PI*atan2((y - yp),
                                         (x - xp)),
                        str);
            else
                gtk_label_set_text(GTK_LABEL(label), "");
        }
        else {
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(dialog->pointx, i)),
                               "");
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(dialog->pointy, i)),
                               "");
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(dialog->distx, i)),
                               "");
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(dialog->disty, i)),
                               "");
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(dialog->slope, i)),
                               "");
        }
    }

    if (n) g_free(spoints);
    g_string_free(str, TRUE);
}


GtkWidget *
gwy_graph_window_measure_dialog_new(GwyGraph *graph)
{
    GtkWidget *label, *table;
    GwyGraphWindowMeasureDialog *dialog;
    GwyGraphModel *gmodel;
    gint i;
    GString *str;
    
    gwy_debug("");
    dialog = GWY_GRAPH_WINDOW_MEASURE_DIALOG (g_object_new (gwy_graph_window_measure_dialog_get_type (), NULL));

    dialog->graph = GTK_WIDGET(graph);
    gmodel = gwy_graph_get_model(GWY_GRAPH(dialog->graph));

    dialog->labpoint = g_ptr_array_new();
    dialog->pointx = g_ptr_array_new();
    dialog->pointy = g_ptr_array_new();
    dialog->distx = g_ptr_array_new();
    dialog->disty = g_ptr_array_new();
    dialog->slope = g_ptr_array_new();
    dialog->curve_index = 1;
    str = g_string_new("");

    table = gtk_table_new(1, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    dialog->index = GTK_OBJECT(gtk_adjustment_new(dialog->curve_index, 1, gmodel->ncurves, 1, 5, 0));
    gwy_table_attach_spinbutton(table, 0, "Curve:", "", dialog->index);
    g_signal_connect_swapped(dialog->index, "value-changed",
                             G_CALLBACK(index_changed_cb), dialog);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1,
                     GTK_FILL | GTK_EXPAND, 0, 2, 2);

    dialog->mmethod = 0;
    dialog->method = gwy_enum_combo_box_new(method_type, G_N_ELEMENTS(method_type),
                                              G_CALLBACK(method_cb), dialog,
                                              dialog->mmethod, TRUE);
    gtk_table_attach(GTK_TABLE(table), dialog->method, 3, 4, 0, 1,
                     GTK_FILL | GTK_EXPAND, 0, 2, 2);


    
    /* big table */
    table = gtk_table_new(6, 11, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Points</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, 0, 2, 2);


    dialog->x_mag = gwy_axis_get_magnification(GWY_GRAPH(dialog->graph)->axis_top);
    dialog->y_mag = gwy_axis_get_magnification(GWY_GRAPH(dialog->graph)->axis_left);
    printf("%g %g\n", dialog->x_mag, dialog->y_mag);

    header_label(table, 1, 1, "X", gwy_axis_get_magnification_string(GWY_GRAPH(dialog->graph)->axis_top)->str, str);
    header_label(table, 1, 2, "Y", gwy_axis_get_magnification_string(GWY_GRAPH(dialog->graph)->axis_left)->str, str);
    header_label(table, 1, 3, _("Length"), gwy_axis_get_magnification_string(GWY_GRAPH(dialog->graph)->axis_top)->str, str);
    header_label(table, 1, 4, _("Height"), gwy_axis_get_magnification_string(GWY_GRAPH(dialog->graph)->axis_left)->str, str);
    header_label(table, 1, 5, _("Angle"), "deg", str);

    for (i = 0; i < NMAX; i++) {
        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(dialog->labpoint, label);

        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 1, 2, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(dialog->pointx, label);

        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 2, 3, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(dialog->pointy, label);

        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 3, 4, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(dialog->distx, label);

        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 4, 5, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(dialog->disty, label);

        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 5, 6, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(dialog->slope, label);
    }
    g_string_free(str, TRUE);

    selection_id = g_signal_connect(dialog->graph, "selected",
                                            G_CALLBACK(selection_updated_cb),
                                            dialog);

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                                GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
   
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                                "Clear", GWY_GRAPH_WINDOW_MEASURE_RESPONSE_CLEAR);
    
    return GTK_WIDGET(dialog);
}

static void
gwy_graph_window_measure_dialog_finalize(GObject *object)
{
    gwy_debug("");

    g_return_if_fail(GWY_IS_GRAPH_MEASURE_DIALOG(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void     
index_changed_cb(GwyGraphWindowMeasureDialog *dialog)
{
   dialog->curve_index = 
        gtk_adjustment_get_value(GTK_ADJUSTMENT(dialog->index));
   selection_updated_cb(GWY_GRAPH(dialog->graph), dialog);    
}

static void     
method_cb(GtkWidget *combo, GwyGraphWindowMeasureDialog *dialog)
{
    dialog->mmethod = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));

    gwy_graph_clear_selection(GWY_GRAPH(dialog->graph));
    if (dialog->mmethod == METHOD_INTERSECTIONS)
        gwy_graph_set_status(GWY_GRAPH(dialog->graph), GWY_GRAPH_STATUS_XLINES);
    else
        gwy_graph_set_status(GWY_GRAPH(dialog->graph), GWY_GRAPH_STATUS_POINTS);

}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
