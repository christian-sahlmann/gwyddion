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
#include <stdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define FRACTAL_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)


/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
typedef struct {
    gdouble from_cubecounting;
    gdouble to_cubecounting;
    gdouble from_partitioning;
    gdouble to_partitioning;
    gdouble from_triangulation;
    gdouble to_triangulation;
    gdouble from_psdf;
    gdouble to_psdf;
     
    gdouble result_cubecounting;
    gdouble result_partitioning;
    gdouble result_triangulation;
    gdouble result_psdf;
    GwyInterpolationType interp;
    GwyFractalType out;
} FractalArgs;

typedef struct {
    GtkWidget *from;
    GtkWidget *to;
    GtkWidget *result;
    GtkWidget *interp;
    GtkWidget *out;
    GtkWidget *graph;
    GtkWidget *res_cubecounting;
    GtkWidget *res_partitioning;
    GtkWidget *res_triangulation;
    GtkWidget *res_psdf;
} FractalControls;


static gboolean    module_register            (const gchar *name);
static gboolean    fractal                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    fractal_dialog                 (FractalArgs *args,
                                                   GwyContainer *data);
static void        interp_changed_cb          (GObject *item,
                                               FractalArgs *args);
static void        out_changed_cb             (GObject *item,
                                               FractalArgs *args);
static void        fractal_load_args              (GwyContainer *container,
                                               FractalArgs *args);
static void        fractal_save_args              (GwyContainer *container,
                                               FractalArgs *args);
static void        fractal_dialog_update          (FractalControls *controls,
                                               FractalArgs *args,
                                               GwyContainer *data);
static void        fractal_dialog_recompute    (FractalControls *controls,
                                               FractalArgs *args,
                                               GwyContainer *data);
static void        graph_selected             (GwyGraphArea *area,
                                               FractalArgs *args);

static gboolean        remove_datapoints          (GwyDataLine *xline, 
                                               GwyDataLine *yline, 
                                               GwyDataLine *newxline,
                                               GwyDataLine *newyline,
                                               FractalArgs *args);

FractalControls *global_controls = NULL;
GwyContainer *global_data = NULL;

FractalArgs fractal_defaults = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    GWY_INTERPOLATION_BILINEAR,
    GWY_FRACTAL_PARTITIONING,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "fractal",
    "Fractal dimension evaluation",
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo fractal_func_info = {
        "fractal",
        "/_Statistics/_Fractal dimension...",
        (GwyProcessFunc)&fractal,
        FRACTAL_RUN_MODES,
    };

    gwy_process_func_register(name, &fractal_func_info);

    return TRUE;
}

static gboolean
fractal(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window, *dialog;
    GwyDataField *dfield;
    GwyDataField *raout, *ipout, *imin;
    GwyDataLine *xline, *yline;
    GwySIUnit *xyunit, *zunit;
    FractalArgs args;
    gboolean ok;
    gint xsize, ysize, newsize;
    gdouble newreals;
    gint i;

    g_assert(run & FRACTAL_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = fractal_defaults;
    else
        fractal_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || fractal_dialog(&args, data);
    if (ok) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));

        xsize = gwy_data_field_get_xres(dfield);
        ysize = gwy_data_field_get_yres(dfield);

        
    }

    return ok;
}

static gboolean
fractal_dialog(FractalArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *hbox, *label;
   
    FractalControls controls;
    enum { RESPONSE_RESET = 1,
        RESPONSE_RECOMPUTE = 2
         };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Fractal dimension - in construction now"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Recompute"), RESPONSE_RECOMPUTE,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    /*controls*/
    table = gtk_table_new(2, 8, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table,
                       FALSE, FALSE, 4);

    controls.interp
        = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                        args, args->interp);
    gwy_table_attach_row(table, 1, _("Interpolation type:"), "",
                         controls.interp);

    controls.out
        = gwy_option_menu_fractal(G_CALLBACK(out_changed_cb),
                                     args, args->out);
    gwy_table_attach_row(table, 3, _("Output type:"), "",
                         controls.out);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Fit area:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5, GTK_FILL, 0, 2, 2);
    
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("from:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6, GTK_FILL, 0, 2, 2);
    
    controls.from = gtk_label_new("minimum");
    gtk_misc_set_alignment(GTK_MISC(controls.from), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.from, 1, 2, 5, 6, GTK_FILL, 0, 2, 2);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("to:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 6, 7, GTK_FILL, 0, 2, 2);

    controls.to = gtk_label_new("maxium");
    gtk_misc_set_alignment(GTK_MISC(controls.to), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.to, 1, 2, 6, 7, GTK_FILL, 0, 2, 2);
 
    /*results*/
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Result:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 7, 8, GTK_FILL, 0, 2, 2);
     
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("partitioning:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 8, 9, GTK_FILL, 0, 2, 2);
   
    controls.res_partitioning = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.res_partitioning), 0.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(controls.res_partitioning), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.res_partitioning, 1, 2, 8, 9, GTK_FILL, 0, 2, 2);
    
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("cube counting:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 9, 10, GTK_FILL, 0, 2, 2);

    controls.res_cubecounting = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.res_cubecounting), 0.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(controls.res_cubecounting), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.res_cubecounting, 1, 2, 9, 10, GTK_FILL, 0, 2, 2);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("triangulation:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 10, 11, GTK_FILL, 0, 2, 2);

    controls.res_triangulation = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.res_triangulation), 0.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(controls.res_triangulation), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.res_triangulation, 1, 2, 10, 11, GTK_FILL, 0, 2, 2);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("power spectrum:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 11, 12, GTK_FILL, 0, 2, 2);

    controls.res_psdf = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.res_psdf), 0.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(controls.res_psdf), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.res_psdf, 1, 2, 11, 12, GTK_FILL, 0, 2, 2);
 
    /*graph*/
    controls.graph = gwy_graph_new();
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, 
                       FALSE, FALSE, 4);
    gwy_graph_set_status(controls.graph, GWY_GRAPH_STATUS_XSEL);
    
    g_signal_connect(GWY_GRAPH(controls.graph)->area, "selected", G_CALLBACK(graph_selected), args);
    

    global_controls = &controls;
    global_data = data;
    
    gtk_widget_show_all(dialog);
    fractal_dialog_update(&controls, args, data);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            switch (args->out){
                case (GWY_FRACTAL_CUBECOUNTING):
                args->from_cubecounting = 0;
                args->to_cubecounting = 0;
                break;

                case (GWY_FRACTAL_PARTITIONING):
                args->from_partitioning = 0;
                args->to_partitioning = 0;
                break;

                case (GWY_FRACTAL_TRIANGULATION):
                args->from_triangulation = 0;
                args->to_partitioning = 0;
                break;

                case (GWY_FRACTAL_PSDF):
                args->from_psdf = 0;
                args->to_psdf = 0;
                break;             
            }
            gwy_graph_set_status(GWY_GRAPH(controls.graph), GWY_GRAPH_STATUS_XSEL);
            fractal_dialog_update(&controls, args, data);
            break;

            case RESPONSE_RECOMPUTE:
            fractal_dialog_recompute(&controls, args, data);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    /*args->angle = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.angle));*/
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
interp_changed_cb(GObject *item,
                  FractalArgs *args)
{
    args->interp = GPOINTER_TO_INT(g_object_get_data(item,
                                                     "interpolation-type"));
}

static void
out_changed_cb(GObject *item,
                  FractalArgs *args)
{
    args->out = GPOINTER_TO_INT(g_object_get_data(item,
                                                     "fractal-type"));

    if (global_controls != NULL && global_data != NULL)
    {
        
        gwy_graph_set_status(GWY_GRAPH(global_controls->graph), GWY_GRAPH_STATUS_XSEL);
        fractal_dialog_update(global_controls, args, global_data);   
    }
}


static const gchar *interp_key = "/module/fractal/interp";
static const gchar *out_key = "/module/fractal/out";

static void
fractal_load_args(GwyContainer *container,
                 FractalArgs *args)
{
    *args = fractal_defaults;

    if (gwy_container_contains_by_name(container, interp_key))
        args->interp = gwy_container_get_int32_by_name(container, interp_key);
    if (gwy_container_contains_by_name(container, out_key))
        args->out = gwy_container_get_int32_by_name(container, out_key);
}

static void
fractal_save_args(GwyContainer *container,
                 FractalArgs *args)
{
    gwy_container_set_int32_by_name(container, interp_key, args->interp);
    gwy_container_set_int32_by_name(container, out_key, args->out);
}


static void
fractal_dialog_update(FractalControls *controls,
                     FractalArgs *args, GwyContainer *data)
{
    GwyDataField *dfield;
    GwyDataLine *xline, *yline, *xfit, *yfit, *xnline, *ynline;
    GString *label;
    GwyGraphAreaCurveParams *params;
    GwyGraphAutoProperties prop;
    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);
    gint i;
    gboolean is_line;
    gdouble a, b;
    gchar buffer[16];

    
    
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));

    xline = gwy_data_line_new(10, 10, FALSE);
    yline = gwy_data_line_new(10, 10, FALSE);
    xnline = gwy_data_line_new(10, 10, FALSE);
    ynline = gwy_data_line_new(10, 10, FALSE);

    if (args->out == GWY_FRACTAL_PARTITIONING)
    {
        gwy_data_field_fractal_partitioning(dfield, xline, yline, args->interp);
        if ((is_line = remove_datapoints(xline, yline, xnline, ynline, args)) == TRUE)
        {
            args->result_partitioning = gwy_data_field_fractal_partitioning_dim(xnline, ynline, &a, &b);
        
            g_snprintf(buffer, sizeof(buffer), "%2.3g", args->result_partitioning);
            gtk_label_set_text(GTK_LABEL(controls->res_partitioning), buffer);
        }
        label = g_string_new("Partitioning");
    }
    else if (args->out == GWY_FRACTAL_CUBECOUNTING)
    {
        gwy_data_field_fractal_cubecounting(dfield, xline, yline, args->interp);
        if ((is_line = remove_datapoints(xline, yline, xnline, ynline, args)) == TRUE)
        {
            args->result_cubecounting = gwy_data_field_fractal_cubecounting_dim(xnline, ynline, &a, &b);
        
            g_snprintf(buffer, sizeof(buffer), "%2.3g", args->result_cubecounting);
            gtk_label_set_text(GTK_LABEL(controls->res_cubecounting), buffer);
        }
        label = g_string_new("Cube counting");
    }
    else if (args->out == GWY_FRACTAL_TRIANGULATION)
    {
        gwy_data_field_fractal_triangulation(dfield, xline, yline, args->interp);
        if ((is_line = remove_datapoints(xline, yline, xnline, ynline, args)) == TRUE)
        {
            args->result_triangulation = gwy_data_field_fractal_triangulation_dim(xnline, ynline, &a, &b);
        
            g_snprintf(buffer, sizeof(buffer), "%2.3g", args->result_triangulation);
            gtk_label_set_text(GTK_LABEL(controls->res_triangulation), buffer);
        }
        label = g_string_new("Triangulation");
    }
    else if (args->out == GWY_FRACTAL_PSDF)
    {
        gwy_data_field_fractal_psdf(dfield, xline, yline, args->interp);
        if ((is_line = remove_datapoints(xline, yline, xnline, ynline, args)) == TRUE)
        {
            args->result_psdf = gwy_data_field_fractal_psdf_dim(xnline, ynline, &a, &b);
        
            g_snprintf(buffer, sizeof(buffer), "%2.3g", args->result_psdf);
            gtk_label_set_text(GTK_LABEL(controls->res_psdf), buffer);
        }
        label = g_string_new("Power spectrum");
    }
    else return;

    params = g_new(GwyGraphAreaCurveParams, 1);
    params->is_line = 0;
    params->is_point = 1;
    params->point_type = 0;
    params->point_size = 8;
    params->color.pixel = 0xff000000;
    
    gwy_graph_clear(controls->graph);
    gwy_graph_add_datavalues(controls->graph, xline->data, yline->data, xline->res,
                             label, params);    

    if (is_line)
    {
        xfit = gwy_data_line_new(xnline->res, xnline->res, FALSE);
        yfit = gwy_data_line_new(xnline->res, xnline->res, FALSE);   
        for (i=0; i<xnline->res; i++)
        {
            xfit->data[i] = xnline->data[i];
            yfit->data[i] = xfit->data[i]*a + b;
        }
   
        label = g_string_new("linear fit");
        gwy_graph_get_autoproperties(controls->graph, &prop);
        prop.is_point = 0;
        gwy_graph_set_autoproperties(controls->graph, &prop);
    
        gwy_graph_add_datavalues(controls->graph, xfit->data, yfit->data, xnline->res, 
                                 label, NULL);
    }
}

static void
fractal_dialog_recompute(FractalControls *controls,
                     FractalArgs *args, GwyContainer *data)
{
    GwyDataField *dfield;
    GwyDataLine *xline, *yline;
    GString *label;
    GwyGraphAreaCurveParams *params;
    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);

    
    
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));

    fractal_dialog_update(controls, args, data);
}

static void        
graph_selected(GwyGraphArea *area, FractalArgs *args)
{
    gchar buffer[20];

    if (area->seldata->data_start == area->seldata->data_end)
    {
        g_snprintf(buffer, sizeof(buffer), "minimum");
        gtk_label_set_text(GTK_LABEL(global_controls->from), buffer);
        g_snprintf(buffer, sizeof(buffer), "maximum");
        gtk_label_set_text(GTK_LABEL(global_controls->to), buffer);
        switch (args->out){
            case (GWY_FRACTAL_CUBECOUNTING):
            args->from_cubecounting = 0;
            args->to_cubecounting = 0;
            break;

            case (GWY_FRACTAL_PARTITIONING):
            args->from_partitioning = 0;
            args->to_partitioning = 0;
            break;

            case (GWY_FRACTAL_TRIANGULATION):
            args->from_triangulation = 0;
            args->to_partitioning = 0;
            break;

            case (GWY_FRACTAL_PSDF):
            args->from_psdf = 0;
            args->to_psdf = 0;
            break;
         }
            
    }
    else
    {
        g_snprintf(buffer, sizeof(buffer), "%2.3g", area->seldata->data_start);
        gtk_label_set_text(GTK_LABEL(global_controls->from), buffer);
        g_snprintf(buffer, sizeof(buffer), "%2.3g", area->seldata->data_end);
        gtk_label_set_text(GTK_LABEL(global_controls->to), buffer);
        
        if (area->seldata->data_start > area->seldata->data_end)
        {
            GWY_SWAP(gdouble, area->seldata->data_start, area->seldata->data_end);
        }
        switch (args->out){
            case (GWY_FRACTAL_CUBECOUNTING):
            args->from_cubecounting = area->seldata->data_start;
            args->to_cubecounting = area->seldata->data_end;
            break;

            case (GWY_FRACTAL_PARTITIONING):
            args->from_partitioning = area->seldata->data_start;
            args->to_partitioning = area->seldata->data_end;
            break;
                
            case (GWY_FRACTAL_TRIANGULATION):
            args->from_triangulation = area->seldata->data_start;
            args->to_triangulation = area->seldata->data_end;
            break;

            case (GWY_FRACTAL_PSDF):
            args->from_psdf = area->seldata->data_start;
            args->to_psdf = area->seldata->data_end;
            break;
        }
    }
      
}

static gboolean        
remove_datapoints(GwyDataLine *xline, GwyDataLine *yline, 
                  GwyDataLine *newxline, GwyDataLine *newyline, FractalArgs *args)
{
    gint i, j;
    gdouble from, to;
    
    switch (args->out){
        case (GWY_FRACTAL_CUBECOUNTING):
        from = args->from_cubecounting;
        to = args->to_cubecounting;
        break;

        case (GWY_FRACTAL_PARTITIONING):
        from = args->from_partitioning;
        to = args->to_partitioning;
        break;
                
        case (GWY_FRACTAL_TRIANGULATION):
        from = args->from_triangulation;
        to = args->to_triangulation;
        break;

        case (GWY_FRACTAL_PSDF):
        from = args->from_psdf;
        to = args->to_psdf;
        break;
    }
    gwy_data_line_resample(newxline, xline->res, GWY_INTERPOLATION_NONE);
    gwy_data_line_resample(newyline, yline->res, GWY_INTERPOLATION_NONE);
    
    if (from == to)
    {
        gwy_data_line_copy(xline, newxline);
        gwy_data_line_copy(yline, newyline);
        return 1;
    }
    
    j=0;
    for (i=0; i<xline->res; i++)
    {
        if (xline->data[i] >= from && xline->data[i] <= to)
        {
            newxline->data[j] = xline->data[i];
            newyline->data[j] = yline->data[i];
            j++;
        }
    }
    if (j<2) return 0;
    
    gwy_data_line_resize(newxline, 0, j);
    gwy_data_line_resize(newyline, 0, j);

    return 1;
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
