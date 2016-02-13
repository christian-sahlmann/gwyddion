/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/correct.h>
#include <libprocess/interpolation.h>
#include <libprocess/spline.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define EXTR_PATH_RUN_MODES (GWY_RUN_INTERACTIVE | GWY_RUN_IMMEDIATE)

#define PointXY GwyTriangulationPointXY

typedef struct {
    gboolean x;
    gboolean y;
    gboolean vx;
    gboolean vy;
} ExtrPathArgs;

typedef struct {
    ExtrPathArgs *args;
    GwySelection *selection;
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *vx;
    GtkWidget *vy;
} ExtrPathControls;

static gboolean   module_register          (void);
static void       extract_path             (GwyContainer *data,
                                            GwyRunType run);
static gint       extr_path_dialogue       (ExtrPathArgs *args,
                                            GwySelection *selection);
static GtkWidget* create_output_checkbutton(const gchar *label,
                                            gboolean *target,
                                            ExtrPathControls *controls);
static void       output_changed           (GtkToggleButton *toggle,
                                            gboolean *target);
static void       extract_path_do          (GwyContainer *data,
                                            GwyDataField *dfield,
                                            gboolean realsquare,
                                            GwySelection *selection,
                                            const ExtrPathArgs *args);
static void       extr_path_load_args      (GwyContainer *container,
                                            ExtrPathArgs *args);
static void       extr_path_save_args      (GwyContainer *container,
                                            ExtrPathArgs *args);
static void       extr_path_sanitize_args  (ExtrPathArgs *args);

static const ExtrPathArgs extr_path_defaults = {
    FALSE, FALSE,
    TRUE, TRUE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Extracts coordinates and tangents along a path selection."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("extract_path",
                              (GwyProcessFunc)&extract_path,
                              N_("/_Distortion/Extract _Path Selection..."),
                              NULL,
                              EXTR_PATH_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Extract path selection data"));

    return TRUE;
}

static void
extract_path(GwyContainer *data, GwyRunType run)
{
    ExtrPathArgs args;
    GwySelection *selection = NULL;
    GwyDataField *dfield;
    gboolean realsquare;
    gchar key[48];
    gint id;
    gboolean ok;

    g_return_if_fail(run & EXTR_PATH_RUN_MODES);
    g_return_if_fail(g_type_from_name("GwyLayerPath"));
    extr_path_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    g_snprintf(key, sizeof(key), "/%d/select/path", id);
    gwy_container_gis_object_by_name(data, key, &selection);

    ok = extr_path_dialogue(&args, selection);
    extr_path_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return;

    realsquare = FALSE;
    g_snprintf(key, sizeof(key), "/%d/data/realsquare", id);
    gwy_container_gis_boolean_by_name(data, key, &realsquare);
    extract_path_do(data, dfield, realsquare, selection, &args);
}

static gint
extr_path_dialogue(ExtrPathArgs *args, GwySelection *selection)
{
    GtkDialog *dialogue;
    GtkTable *table;
    GtkWidget *check, *label;
    ExtrPathControls controls;
    gint response, row, npts = 0;
    gchar buf[16];

    gwy_clear(&controls, 1);
    controls.args = args;
    controls.selection = selection;
    if (selection)
        npts = gwy_selection_get_data(selection, NULL);

    dialogue = GTK_DIALOG(gtk_dialog_new_with_buttons(_("Extract "
                                                        "Path Selection"),
                                                      NULL, 0, NULL));
    gtk_dialog_add_button(dialogue, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(dialogue, GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(dialogue, GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialogue), GWY_HELP_DEFAULT);

    table = GTK_TABLE(gtk_table_new(5, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialogue)->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 4);
    row = 0;

    if (selection) {
        label = gtk_label_new(_("Number of path points:"));
        gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
        g_snprintf(buf, sizeof(buf), "%d", npts);
        label = gtk_label_new(buf);
        gtk_table_attach(table, label, 1, 3, row, row+1, GTK_FILL, 0, 0, 0);
    }
    else {
        label = gtk_label_new(_("There is no path selection."));
        gtk_table_attach(table, label, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    }
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    check = create_output_checkbutton(_("X position"), &args->x, &controls);
    gtk_table_attach(table, check, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    check = create_output_checkbutton(_("Y position"), &args->y, &controls);
    gtk_table_attach(table, check, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    check = create_output_checkbutton(_("X tangent"), &args->vx, &controls);
    gtk_table_attach(table, check, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    check = create_output_checkbutton(_("Y tangent"), &args->vy, &controls);
    gtk_table_attach(table, check, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    gtk_dialog_set_response_sensitive(dialogue, GTK_RESPONSE_OK, npts > 1);
    gtk_widget_show_all(GTK_WIDGET(dialogue));

    do {
        response = gtk_dialog_run(dialogue);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(GTK_WIDGET(dialogue));
            case GTK_RESPONSE_NONE:
            return FALSE;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(GTK_WIDGET(dialogue));
    return response == GTK_RESPONSE_OK;
}

static GtkWidget*
create_output_checkbutton(const gchar *label, gboolean *target,
                          ExtrPathControls *controls)
{
    GtkWidget *check;

    check = gtk_check_button_new_with_label(label);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), *target);
    gtk_widget_set_sensitive(check, !!controls->selection);
    g_object_set_data(G_OBJECT(check), "target", target);
    g_signal_connect(check, "toggled", G_CALLBACK(output_changed), target);
    return check;
}

static void
output_changed(GtkToggleButton *toggle, gboolean *target)
{
    *target = gtk_toggle_button_get_active(toggle);
}

static GwyGraphModel*
create_graph_model(const PointXY *points,
                   const gdouble *xdata, gdouble *ydata, guint n,
                   gboolean x, gboolean y)
{
    GwyGraphModel *gmodel = gwy_graph_model_new();
    GwyGraphCurveModel *gcmodel;
    guint i;

    if (!x && !y)
        return NULL;

    if (x) {
        gcmodel = gwy_graph_curve_model_new();
        for (i = 0; i < n; i++)
            ydata[i] = points[i].x;
        gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, n);
        g_object_set(gcmodel,
                     "description", "X",
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "color", gwy_graph_get_preset_color(0),
                     NULL);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
    }

    if (y) {
        gcmodel = gwy_graph_curve_model_new();
        for (i = 0; i < n; i++)
            ydata[i] = points[i].y;
        gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, n);
        g_object_set(gcmodel,
                     "description", "Y",
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "color", gwy_graph_get_preset_color(1),
                     NULL);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
    }

    return gmodel;
}

/* XXX: This replicates straighten_path.c */
static PointXY*
rescale_points(GwySelection *selection, GwyDataField *dfield,
               gboolean realsquare,
               gdouble *pdx, gdouble *pdy, gdouble *pqx, gdouble *pqy)
{
    gdouble dx, dy, qx, qy, h;
    PointXY *points;
    guint n, i;

    dx = gwy_data_field_get_xmeasure(dfield);
    dy = gwy_data_field_get_ymeasure(dfield);
    h = MIN(dx, dy);
    if (realsquare) {
        qx = h/dx;
        qy = h/dy;
        dx = dy = h;
    }
    else
        qx = qy = 1.0;

    n = gwy_selection_get_data(selection, NULL);
    points = g_new(PointXY, n);
    for (i = 0; i < n; i++) {
        gdouble xy[2];

        gwy_selection_get_object(selection, i, xy);
        points[i].x = xy[0]/dx;
        points[i].y = xy[1]/dy;
    }

    *pdx = dx;
    *pdy = dy;
    *pqx = qx;
    *pqy = qy;
    return points;
}

static void
extract_path_do(GwyContainer *data,
                GwyDataField *dfield, gboolean realsquare,
                GwySelection *selection,
                const ExtrPathArgs *args)
{
    GwyGraphModel *gmodel;
    GwySpline *spline;
    PointXY *points, *tangents;
    GwySIUnit *xyunit;
    gdouble dx, dy, qx, qy, h, l, length, slackness;
    gdouble *xdata, *ydata;
    guint n, i;
    gboolean closed;

    /* This can only be satisfied in non-interactive use.  Doing nothing is
     * the best option in this case. */
    if (!selection || (n = gwy_selection_get_data(selection, NULL)) < 2)
        return;

    points = rescale_points(selection, dfield, realsquare, &dx, &dy, &qx, &qy);
    h = MIN(dx, dy);
    spline = gwy_spline_new_from_points(points, n);
    g_object_get(selection,
                 "slackness", &slackness,
                 "closed", &closed,
                 NULL);
    gwy_spline_set_closed(spline, closed);
    gwy_spline_set_slackness(spline, slackness);
    g_free(points);

    length = gwy_spline_length(spline);

    /* This would give natural sampling for a straight line along some axis. */
    n = GWY_ROUND(length + 1.0);
    points = g_new(PointXY, n);
    tangents = g_new(PointXY, n);
    xdata = g_new(gdouble, n);
    ydata = g_new(gdouble, n);
    gwy_spline_sample_uniformly(spline, points, tangents, n);
    qx *= dx;
    qy *= dy;
    for (i = 0; i < n; i++) {
        points[i].x *= qx;
        points[i].y *= qy;
        GWY_SWAP(gdouble, tangents[i].x, tangents[i].y);
        tangents[i].x *= qx;
        tangents[i].y *= -qy;
        l = sqrt(tangents[i].x*tangents[i].x + tangents[i].y*tangents[i].y);
        if (h > 0.0) {
            tangents[i].x /= l;
            tangents[i].y /= l;
        }
        xdata[i] = i/(n - 1.0)*length*h;
    }

    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    if ((gmodel = create_graph_model(points, xdata, ydata, n,
                                     args->x, args->y))) {
        g_object_set(gmodel,
                     "axis-label-left", _("Position"),
                     "axis-label-bottom", _("Distance"),
                     "si-unit-x", xyunit,
                     "si-unit-y", xyunit,
                     NULL);
        gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        g_object_unref(gmodel);
    }

    if ((gmodel = create_graph_model(tangents, xdata, ydata, n,
                                     args->vx, args->vy))) {
        g_object_set(gmodel,
                     "axis-label-left", _("Tangent"),
                     "axis-label-bottom", _("Distance"),
                     "si-unit-x", xyunit,
                     NULL);
        gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        g_object_unref(gmodel);
    }

    g_free(ydata);
    g_free(xdata);
    g_free(points);
    g_free(tangents);
}

static const gchar vx_key[] = "/module/extract_path/vx";
static const gchar vy_key[] = "/module/extract_path/vy";
static const gchar x_key[]  = "/module/extract_path/x";
static const gchar y_key[]  = "/module/extract_path/y";

static void
extr_path_sanitize_args(ExtrPathArgs *args)
{
    args->x = !!args->x;
    args->y = !!args->y;
    args->vx = !!args->vx;
    args->vy = !!args->vy;
}

static void
extr_path_load_args(GwyContainer *container,
                     ExtrPathArgs *args)
{
    *args = extr_path_defaults;

    gwy_container_gis_boolean_by_name(container, x_key, &args->x);
    gwy_container_gis_boolean_by_name(container, y_key, &args->y);
    gwy_container_gis_boolean_by_name(container, vx_key, &args->vx);
    gwy_container_gis_boolean_by_name(container, vy_key, &args->vy);

    extr_path_sanitize_args(args);
}

static void
extr_path_save_args(GwyContainer *container,
                     ExtrPathArgs *args)
{
    gwy_container_set_boolean_by_name(container, x_key, args->x);
    gwy_container_set_boolean_by_name(container, y_key, args->y);
    gwy_container_set_boolean_by_name(container, vx_key, args->vx);
    gwy_container_set_boolean_by_name(container, vy_key, args->vy);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
