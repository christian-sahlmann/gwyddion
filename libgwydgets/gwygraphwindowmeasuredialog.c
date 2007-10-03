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

/* TODO: Use a treeview instead of the ugly table. */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include "gwygraphwindowmeasuredialog.h"

enum { NMAX = 10 };

static void     gwy_graph_window_measure_dialog_finalize            (GObject *object);
static gboolean gwy_graph_window_measure_dialog_delete              (GtkWidget *widget,
                                                                     GdkEventAny *event);
static void     gwy_graph_window_measure_dialog_show                (GtkWidget *widget);
static void     gwy_graph_window_measure_dialog_hide                (GtkWidget *widget);
static void     gwy_graph_window_measure_dialog_connect_selection   (GwyGraphWindowMeasureDialog *dialog);
static void     gwy_graph_window_measure_dialog_disconnect_selection(GwyGraphWindowMeasureDialog *dialog);
static void     selection_updated_cb                                (GwySelection *selection,
                                                                     gint k,
                                                                     GwyGraphWindowMeasureDialog *dialog);
static void     index_changed_cb                                    (GwyGraphWindowMeasureDialog *dialog);
static void     method_cb                                           (GtkWidget *combo,
                                                                     GwyGraphWindowMeasureDialog *dialog);
static void     status_cb                                           (GwyGraphArea *area,
                                                                     GwyGraphWindowMeasureDialog *dialog);

GwyEnum method_type[] = {
    { N_("Intersections"),   METHOD_INTERSECTIONS, },
    { N_("Points anywhere"), METHOD_CROSSES,       },
};

G_DEFINE_TYPE(GwyGraphWindowMeasureDialog, _gwy_graph_window_measure_dialog,
              GTK_TYPE_DIALOG)

static void
_gwy_graph_window_measure_dialog_class_init(GwyGraphWindowMeasureDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gobject_class->finalize = gwy_graph_window_measure_dialog_finalize;

    widget_class->delete_event = gwy_graph_window_measure_dialog_delete;
    widget_class->show = gwy_graph_window_measure_dialog_show;
    widget_class->hide = gwy_graph_window_measure_dialog_hide;
}

static void
_gwy_graph_window_measure_dialog_init(G_GNUC_UNUSED GwyGraphWindowMeasureDialog *dialog)
{
}

static gboolean
gwy_graph_window_measure_dialog_delete(GtkWidget *widget,
                                       G_GNUC_UNUSED GdkEventAny *event)
{
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_graph_window_measure_dialog_show(GtkWidget *widget)
{
    GwyGraphWindowMeasureDialog *dialog;

    dialog = GWY_GRAPH_WINDOW_MEASURE_DIALOG(widget);
    GTK_WIDGET_CLASS(_gwy_graph_window_measure_dialog_parent_class)->show(widget);
    gwy_graph_window_measure_dialog_connect_selection(dialog);
}

static void
gwy_graph_window_measure_dialog_hide(GtkWidget *widget)
{
    GwyGraphWindowMeasureDialog *dialog;

    dialog = GWY_GRAPH_WINDOW_MEASURE_DIALOG(widget);
    gwy_graph_window_measure_dialog_disconnect_selection(dialog);
    GTK_WIDGET_CLASS(_gwy_graph_window_measure_dialog_parent_class)->hide(widget);
}

static void
gwy_graph_window_measure_dialog_connect_selection(GwyGraphWindowMeasureDialog *dialog)
{
    GwyGraphStatusType status;
    GwyGraphArea *area;

    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(dialog->graph)));
    status = gwy_graph_area_get_status(area);
    g_return_if_fail(status == GWY_GRAPH_STATUS_XLINES
                     || status == GWY_GRAPH_STATUS_POINTS);

    dialog->selection = gwy_graph_area_get_selection(area, status);
    g_object_ref(dialog->selection);
    gwy_selection_set_max_objects(dialog->selection, NMAX);
    dialog->selection_id
        = g_signal_connect(dialog->selection, "changed",
                           G_CALLBACK(selection_updated_cb), dialog);
}

static void
gwy_graph_window_measure_dialog_disconnect_selection(GwyGraphWindowMeasureDialog *dialog)
{
    gwy_signal_handler_disconnect(dialog->selection, dialog->selection_id);
    gwy_object_unref(dialog->selection);
}

static void
value_label(GtkWidget *label, gdouble value, gint precision, GString *str)
{
    g_string_printf(str, "%.*f", precision, value);
    gtk_label_set_text(GTK_LABEL(label), str->str);
}

static GtkWidget *
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

    return label;
}

static void
header_label_update(GtkLabel *label,
                    const gchar *header, const gchar *unit, GString *str)
{
    if (unit)
        g_string_printf(str, "<b>%s</b> [%s]", header, unit);
    else
        g_string_printf(str, "<b>%s</b>", header);
    gtk_label_set_markup(label, str->str);
}

static gdouble
get_y_for_x(GwyGraph *graph, gdouble x, gint curve, gboolean *ret)
{
    GwyGraphModel *model;
    GwyGraphCurveModel *cmodel;
    const gdouble *xdata, *ydata;
    gint ndata, i, pos;

    model = gwy_graph_get_model(graph);
    if (gwy_graph_model_get_n_curves(model) <= curve) {
        *ret = FALSE;
        return 0;
    }

    cmodel = gwy_graph_model_get_curve(model, curve);
    xdata = gwy_graph_curve_model_get_xdata(cmodel);
    ydata = gwy_graph_curve_model_get_ydata(cmodel);
    ndata = gwy_graph_curve_model_get_ndata(cmodel);

    pos = -1;
    for (i = 0; i < (ndata - 1); i++) {
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

    return ydata[pos] + (ydata[pos+1] - ydata[pos])*(x - (xdata[pos]))/(xdata[pos+1] - xdata[pos]);
}

static void
selection_updated_cb(GwySelection *selection,
                     G_GNUC_UNUSED gint k,
                     GwyGraphWindowMeasureDialog *dialog)
{
    GtkWidget *label;
    GwyGraph *graph;
    GwyGraphModel *gmodel;
    GwyGraphArea *garea;
    GwyAxis *xaxis, *yaxis;
    GString *str;
    gint i, n;
    gdouble *spoints = NULL;
    gdouble x = 0, y = 0, xp = 0, yp = 0;
    gboolean ret = TRUE, prevret = TRUE;
    GwySIUnit *xunit;
    GwySIUnit *yunit;
    GwySIValueFormat *xformat;
    GwySIValueFormat *yformat;
    gdouble xmin, xmax, xrange, xresolution;
    gdouble ymin, ymax, yrange, yresolution;
    guint width, height;

    graph = GWY_GRAPH(dialog->graph);
    gmodel = GWY_GRAPH_MODEL(gwy_graph_get_model(graph));
    garea = GWY_GRAPH_AREA(gwy_graph_get_area(graph));

    if (!(gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_POINTS
          || gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_XLINES))
        return;

    n = gwy_selection_get_data(selection, NULL);
    if (n > 0) {
        if (gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_POINTS) {
            spoints = g_new(gdouble, 2*n);
            gwy_selection_get_data(selection, spoints);

        }
        else {
            spoints = g_new(gdouble, n);
            gwy_selection_get_data(selection, spoints);
         }
    }

    str = g_string_new("");

    xaxis = gwy_graph_get_axis(graph, GTK_POS_TOP);
    xunit = gwy_si_unit_new(gwy_axis_get_magnification_string(xaxis));
    yaxis = gwy_graph_get_axis(graph, GTK_POS_LEFT);
    yunit = gwy_si_unit_new(gwy_axis_get_magnification_string(yaxis));

    /* set up some nice formatting for the values */
    gtk_layout_get_size(GTK_LAYOUT(garea), &width, &height);
    gwy_graph_model_get_x_range(gmodel, &xmin, &xmax);
    gwy_graph_model_get_y_range(gmodel, &ymin, &ymax);
    xrange = xmax - xmin;
    yrange = ymax - ymin;
    xresolution = xrange / width;
    yresolution = yrange / height;
    xformat = gwy_si_unit_get_format_with_resolution(xunit,
                                                     GWY_SI_UNIT_FORMAT_PLAIN,
                                                     xrange,
                                                     xresolution/6,
                                                     NULL);
    yformat = gwy_si_unit_get_format_with_resolution(yunit,
                                                     GWY_SI_UNIT_FORMAT_PLAIN,
                                                     yrange,
                                                     yresolution/6,
                                                     NULL);

    /* set up header labels */
    header_label_update(GTK_LABEL(dialog->header_x), "X",
                        xformat->units, str);
    header_label_update(GTK_LABEL(dialog->header_distx), _("Length"),
                        xformat->units, str);
    header_label_update(GTK_LABEL(dialog->header_y), "Y",
                        yformat->units, str);
    header_label_update(GTK_LABEL(dialog->header_disty), _("Height"),
                        yformat->units, str);

    /*update points data */
    for (i = 0; i < NMAX; i++) {
        if (i < n) {
            if (i) {
                xp = x;
                yp = y;
                prevret = ret;
            }

            if (gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_POINTS) {
                x = spoints[2*i];
                y = spoints[2*i + 1];
            }
            else if (gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_XLINES) {
                x = spoints[i];
                y = get_y_for_x(graph, x, dialog->curve_index - 1, &ret);
            }
            label = g_ptr_array_index(dialog->pointx, i);
            value_label(label, x/xformat->magnitude, xformat->precision, str);

            label = g_ptr_array_index(dialog->pointy, i);
            if (ret)
                value_label(label, y/yformat->magnitude,
                            yformat->precision, str);
            else
                gtk_label_set_text(GTK_LABEL(label), NULL);

            if (!i)
                continue;

            label = g_ptr_array_index(dialog->distx, i);
            value_label(label, (x - xp)/xformat->magnitude,
                        xformat->precision, str);


            label = g_ptr_array_index(dialog->disty, i);
            if (ret && prevret)
                value_label(label, (y - yp)/yformat->magnitude,
                            yformat->precision, str);
            else
                gtk_label_set_text(GTK_LABEL(label), NULL);

            label = g_ptr_array_index(dialog->slope, i);
            if (ret && prevret) {
                if (gwy_si_unit_equal (xunit, yunit))
                    value_label(label, 180.0/G_PI*atan2((y - yp), (x - xp)),
                            2, str);
                else
                    gtk_label_set_text(GTK_LABEL(label), "N.A.");
            }
            else
                gtk_label_set_text(GTK_LABEL(label), NULL);
        }
        else {
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(dialog->pointx, i)),
                               NULL);
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(dialog->pointy, i)),
                               NULL);
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(dialog->distx, i)),
                               NULL);
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(dialog->disty, i)),
                               NULL);
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(dialog->slope, i)),
                               NULL);
        }
    }

    if (n)
        g_free(spoints);
    g_string_free(str, TRUE);

    gwy_si_unit_value_format_free(xformat);
    gwy_si_unit_value_format_free(yformat);
    g_object_unref(xunit);
    g_object_unref(yunit);
}


GtkWidget*
_gwy_graph_window_measure_dialog_new(GwyGraph *graph)
{
    GtkWidget *label, *table;
    GwyGraphWindowMeasureDialog *dialog;
    GwyGraphModel *gmodel;
    GwyAxis *axis;
    gint i;
    GString *str;

    gwy_debug("");

    dialog = GWY_GRAPH_WINDOW_MEASURE_DIALOG(g_object_new(GWY_TYPE_GRAPH_WINDOW_MEASURE_DIALOG, NULL));
    gtk_window_set_title(GTK_WINDOW(dialog), _("Measure Distances"));
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

    dialog->graph = GTK_WIDGET(graph);
    gmodel = gwy_graph_get_model(graph);

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

    dialog->index = gtk_adjustment_new(dialog->curve_index,
                                       1, gwy_graph_model_get_n_curves(gmodel),
                                       1, 5, 0);
    gwy_table_attach_spinbutton(table, 0, _("Curve:"), NULL, dialog->index);
    g_signal_connect_swapped(dialog->index, "value-changed",
                             G_CALLBACK(index_changed_cb), dialog);
    g_signal_connect(GWY_GRAPH_AREA(gwy_graph_get_area(graph)),
                     "notify::status",
                     G_CALLBACK(status_cb), dialog);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1,
                     GTK_FILL | GTK_EXPAND, 0, 2, 2);

    dialog->mmethod = 0;
    dialog->method = gwy_enum_combo_box_new(method_type,
                                            G_N_ELEMENTS(method_type),
                                            G_CALLBACK(method_cb), dialog,
                                            dialog->mmethod, TRUE);
    gtk_table_attach(GTK_TABLE(table), dialog->method, 3, 4, 0, 1,
                     GTK_FILL | GTK_EXPAND, 0, 2, 2);

    /* big table */
    table = gtk_table_new(6, 11, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gwy_label_new_header(_("Points"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, 0, 2, 2);

    axis = gwy_graph_get_axis(graph, GTK_POS_TOP);
    dialog->header_x = header_label(table, 1, 1, "X",
                                    gwy_axis_get_magnification_string(axis),
                                    str);
    dialog->header_distx = header_label(table, 1, 3, _("Length"),
                                        gwy_axis_get_magnification_string(axis),
                                        str);

    axis = gwy_graph_get_axis(graph, GTK_POS_LEFT);
    dialog->header_y = header_label(table, 1, 2, "Y",
                                    gwy_axis_get_magnification_string(axis),
                                    str);
    dialog->header_disty = header_label(table, 1, 4, _("Height"),
                                        gwy_axis_get_magnification_string(axis),
                                        str);

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

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLEAR,
                          GWY_GRAPH_WINDOW_MEASURE_RESPONSE_CLEAR);

    return GTK_WIDGET(dialog);
}

static void
gwy_graph_window_measure_dialog_finalize(GObject *object)
{
    GwyGraphWindowMeasureDialog *dialog;

    dialog = GWY_GRAPH_WINDOW_MEASURE_DIALOG(object);

    gwy_graph_window_measure_dialog_disconnect_selection(dialog);
    g_ptr_array_free(dialog->labpoint, TRUE);
    g_ptr_array_free(dialog->pointx, TRUE);
    g_ptr_array_free(dialog->pointy, TRUE);
    g_ptr_array_free(dialog->distx, TRUE);
    g_ptr_array_free(dialog->disty, TRUE);
    g_ptr_array_free(dialog->slope, TRUE);

    G_OBJECT_CLASS(_gwy_graph_window_measure_dialog_parent_class)->finalize(object);
}

static void
index_changed_cb(GwyGraphWindowMeasureDialog *dialog)
{
    GwyGraphArea *area;
    GwySelection *selection;

    dialog->curve_index = gwy_adjustment_get_int(dialog->index);
    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(dialog->graph)));
    selection = gwy_graph_area_get_selection(area, GWY_GRAPH_STATUS_PLAIN);
    selection_updated_cb(selection, 0,  dialog);
}

static void
method_cb(GtkWidget *combo, GwyGraphWindowMeasureDialog *dialog)
{
    GwyGraphStatusType status;

    dialog->mmethod = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    switch (dialog->mmethod) {
        case METHOD_INTERSECTIONS:
        status = GWY_GRAPH_STATUS_XLINES;
        break;

        case METHOD_CROSSES:
        status = GWY_GRAPH_STATUS_POINTS;
        break;

        default:
        g_return_if_reached();
        break;
    }

    gwy_graph_window_measure_dialog_disconnect_selection(dialog);
    gwy_graph_set_status(GWY_GRAPH(dialog->graph), status);
    gwy_graph_window_measure_dialog_connect_selection(dialog);
    selection_updated_cb(dialog->selection, -1, dialog);
}

static void
status_cb(GwyGraphArea *area, GwyGraphWindowMeasureDialog *dialog)
{
    /* FIXME: Who knows. What should happen when *someone else* changes the
     * status while the measure dialog is active? */
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
