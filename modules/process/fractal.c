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
    gdouble from;
    gdouble to;
    gdouble result;
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


FractalControls *global_controls = NULL;
GwyContainer *global_data = NULL;

FractalArgs fractal_defaults = {
    0, 1, 2,
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

        xline = gwy_data_line_new(10, 10, FALSE);
        yline = gwy_data_line_new(10, 10, FALSE);

        if (args.out == GWY_FRACTAL_PARTITIONING)
            gwy_data_field_fractal_partitioning(dfield, xline, yline, args.interp);
        else if (args.out == GWY_FRACTAL_CUBECOUNTING)
            gwy_data_field_fractal_cubecounting(dfield, xline, yline, args.interp);
        else if (args.out == GWY_FRACTAL_TRIANGULATION)
            gwy_data_field_fractal_triangulation(dfield, xline, yline, args.interp);

        for (i=0; i<xline->res; i++)
        {
            printf("%g %g\n", log(xline->data[i]), log(yline->data[i]));
        }
        printf("\n");
        
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
    
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("to:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 6, 7, GTK_FILL, 0, 2, 2);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Result:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 7, 8, GTK_FILL, 0, 2, 2);
     
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("partitioning:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 8, 9, GTK_FILL, 0, 2, 2);
    
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("cube counting:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 9, 10, GTK_FILL, 0, 2, 2);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("triangulation:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 10, 11, GTK_FILL, 0, 2, 2);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), _("power spectrum:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 11, 12, GTK_FILL, 0, 2, 2);


    /*graph*/
    controls.graph = gwy_graph_new();
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, 
                       FALSE, FALSE, 4);

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
            *args = fractal_defaults;
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
    fractal_dialog_update(global_controls, args, global_data);   
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
    GwyDataLine *xline, *yline, *xfit, *yfit;
    GString *label;
    GwyGraphAreaCurveParams *params;
    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);
    gint i;
    gdouble a, b;
    
    
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));

    xline = gwy_data_line_new(10, 10, FALSE);
    yline = gwy_data_line_new(10, 10, FALSE);

    if (args->out == GWY_FRACTAL_PARTITIONING)
    {
        gwy_data_field_fractal_partitioning(dfield, xline, yline, args->interp);
        args->result = gwy_data_field_fractal_partitioning_dim(xline, yline, &a, &b);
        printf("dim_part = %f\n", args->result);
        label = g_string_new("Partitioning");
    }
    else if (args->out == GWY_FRACTAL_CUBECOUNTING)
    {
        gwy_data_field_fractal_cubecounting(dfield, xline, yline, args->interp);
        args->result = gwy_data_field_fractal_cubecounting_dim(xline, yline, &a, &b);
        printf("dim_cube = %f\n", args->result);
        label = g_string_new("Cube counting");
        
    }
    else if (args->out == GWY_FRACTAL_TRIANGULATION)
    {
        gwy_data_field_fractal_triangulation(dfield, xline, yline, args->interp);
        args->result = gwy_data_field_fractal_triangulation_dim(xline, yline, &a, &b);
        printf("dim_tri = %f\n", args->result);
        label = g_string_new("Triangulation");
    }
    else if (args->out == GWY_FRACTAL_PSDF)
    {
/*        gwy_data_field_fractal_triangulation(dfield, xline, yline, args->interp);*/
        label = g_string_new("Power spectrum");
    }
    else return;

    params = g_new(GwyGraphAreaCurveParams, 1);
    params->is_line = 0;
    params->is_point = 1;
    params->point_type = 0;
    params->point_size = 8;
    params->color.pixel = 0x00000000;
    
    gwy_graph_clear(controls->graph);
    gwy_graph_add_datavalues(controls->graph, xline->data, yline->data, xline->res,
                             label, params);    

    xfit = gwy_data_line_new(2, 2, FALSE);
    yfit = gwy_data_line_new(2, 2, FALSE);   
    xfit->data[0] = xline->data[0];
    xfit->data[1] = xline->data[xline->res-1];
    yfit->data[0] = xfit->data[0]*a + b;
    yfit->data[1] = xfit->data[1]*a + b;;
   
    params = g_new(GwyGraphAreaCurveParams, 1);
    params->is_line = 1;
    params->is_point = 0;
    params->color.pixel = 0xff000000; 
/*    gwy_graph_add_datavalues(controls->graph, xfit->data, yfit->data, xfit->res, 
                             label, params);*/
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
