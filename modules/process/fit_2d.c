/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libprocess/tip.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define FIT_2D_RUN_MODES \
    (GWY_RUN_MODAL)

#define MAX_PARAMS 4
    

/* Data for this function. */
typedef struct {
    gdouble par_init[MAX_PARAMS];
    gdouble par_res[MAX_PARAMS];
    gdouble par_err[MAX_PARAMS];
    gboolean par_fix[MAX_PARAMS];
    GwyNLFitter *fitter;
    gboolean is_fitted;
    GwyContainer *data;
    GwyContainer *vdata;
} Fit2dArgs;

typedef struct {
    gint vxres;
    gint vyres;
    GtkWidget *view;
    GtkWidget *type;
    GtkWidget **param_des;
    GtkObject **param_init;
    GtkWidget **param_res;
    GtkWidget **param_err;
    GtkWidget **param_fit;
    GtkWidget **covar;
    GtkWidget *chisq;
} Fit2dControls;

static gboolean    module_register            (const gchar *name);
static gboolean    fit_2d                  (GwyContainer *data,
                                               GwyRunType run);
static gboolean    fit_2d_dialog           (Fit2dArgs *args,
                                               GwyContainer *data);
static void        reset                      (Fit2dControls *controls,
                                               Fit2dArgs *args);
static void        fit_2d_load_args        (GwyContainer *container,
                                               Fit2dArgs *args);
static void        fit_2d_save_args        (GwyContainer *container,
                                               Fit2dArgs *args);
static void        fit_2d_sanitize_args    (Fit2dArgs *args);

static void        fit_2d_run               (Fit2dControls *controls,
                        	                 Fit2dArgs *args);
static void        fit_2d_do               (Fit2dControls *controls,
                        	                 Fit2dArgs *args);
static void        fit_2d_dialog_abandon   (Fit2dControls *controls);

static void        double_entry_changed_cb   (GtkWidget *entry,
					      gdouble *value);

static void        toggle_changed_cb         (GtkToggleButton *button,
					      gboolean *value);
static gdouble     fit_sphere		(gdouble x,
					   G_GNUC_UNUSED gint n_param,
					   const gdouble *param,
					   gdouble *dimdata,
					   gboolean *fres);

GwyNLFitter*	   gwy_math_nlfit_fit_2d(GwyNLFitFunc ff,
		    		    	      GwyNLFitDerFunc df,		      
					      GwyDataField *dfield,		     
					      GwyDataField *weight,
					      gint n_param,
					      gdouble *param, gdouble *err,
					      const gboolean *fixed_param,
                   		              gpointer user_data);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "fit_2d",
    N_("2D fitting"),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo fit_2d_func_info = {
        "fit_2d",
        N_("/_Background/_2D fit..."),
        (GwyProcessFunc)&fit_2d,
        FIT_2D_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &fit_2d_func_info);

    return TRUE;
}

static gboolean
fit_2d(GwyContainer *data, GwyRunType run)
{
    Fit2dArgs args;
    gboolean ok;

    g_assert(run & FIT_2D_RUN_MODES);

    fit_2d_load_args(gwy_app_settings_get(), &args);
    args.par_fix[0] = FALSE;
    args.par_fix[1] = TRUE;
    args.par_fix[2] = TRUE;
    args.par_fix[3] = FALSE;
    
    if ((ok = fit_2d_dialog(&args, data)))
	    fit_2d_save_args(gwy_app_settings_get(), &args);

    return ok;
}


static gboolean
fit_2d_dialog(Fit2dArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *hbox, *vbox, *hbox2;
    Fit2dControls controls;
    enum {
	RESPONSE_FIT = 1,
        RESPONSE_RESET = 2
    };
    gint response, row, i, j;
    GtkObject *layer;
    GwyDataField *dfield;
    GtkWidget *label;

    dialog = gtk_dialog_new_with_buttons(_("2D fit"), NULL, 0,
                                         _("_Fit"), RESPONSE_FIT,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);


    hbox = gtk_hbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.vxres = 200;
    controls.vyres = 200;

    /*set initial tip properties*/
    args->data = gwy_container_duplicate_by_prefix(data,
                                                     "/0/data",
                                                     "/0/base/palette",
                                                     NULL);

    /*set up data of rescaled image of the surface*/
    args->vdata
        = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(args->data)));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->vdata,
                                                             "/0/data"));
    gwy_data_field_resample(dfield, controls.vxres, controls.vyres,
                            GWY_INTERPOLATION_ROUND);

    /*set up rescaled image of the surface*/
    controls.view = gwy_data_view_new(args->vdata);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));

    /*set up fit controls*/
    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);
    


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Fitting parameters:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 4);


    label = gtk_label_new_with_mnemonic(_("Function: sphere"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 4);

    table = gtk_table_new(4, 6, FALSE);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), " ");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Initial</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
		     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Result</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1,
		     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Error</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1,
		     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Fix</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 0, 1,
		     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    
    
    
    controls.param_des = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
	controls.param_des[i] = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(controls.param_des[i]), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), controls.param_des[i],
			 0, 1, i+1, i+2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    controls.param_init = (GtkWidget **)g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
	controls.param_init[i] = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(controls.param_init[i]), 12);
	gtk_entry_set_width_chars(GTK_ENTRY(controls.param_init[i]), 12);
	g_signal_connect(controls.param_init[i], "changed",
			 G_CALLBACK(double_entry_changed_cb),
			 &args->par_init[i]);
	gtk_table_attach(GTK_TABLE(table), controls.param_init[i],
			 1, 2, i+1, i+2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    controls.param_res = (GtkWidget **)g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
	controls.param_res[i] = gtk_label_new(NULL);
	gtk_table_attach(GTK_TABLE(table), controls.param_res[i],
			 2, 3, i+1, i+2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    controls.param_err = (GtkWidget **)g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
	controls.param_err[i] = gtk_label_new(NULL);
	gtk_table_attach(GTK_TABLE(table), controls.param_err[i],
			 3, 4, i+1, i+2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    controls.param_fit = (GtkWidget **)g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
	controls.param_fit[i] = gtk_check_button_new();
	gtk_toggle_button_set_active(controls.param_fit[i], args->par_fix[i]);
	g_signal_connect(controls.param_fit[i], "toggled",
			 G_CALLBACK(toggle_changed_cb), &args->par_fix[i]);
	gtk_table_attach(GTK_TABLE(table), controls.param_fit[i],
			 4, 5, i+1, i+2,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }
    gtk_container_add(GTK_CONTAINER(vbox), table);
    

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Correlation matrix:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(vbox), label);


    controls.covar = g_new0(GtkWidget*, MAX_PARAMS*MAX_PARAMS);
    table = gtk_table_new(MAX_PARAMS, MAX_PARAMS, TRUE);
    for (i = 0; i < MAX_PARAMS; i++) {
	for (j = 0; j <= i; j++) {
	    label = controls.covar[i*MAX_PARAMS + j] = gtk_label_new(NULL);
	    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	    gtk_table_attach(GTK_TABLE(table), label,
			     j, j+1, i, i+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
	}
    }
    gtk_container_add(GTK_CONTAINER(vbox), table);

    hbox2 = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Chi-square result:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(hbox2), label);

    controls.chisq = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.chisq), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(hbox2), controls.chisq);

    gtk_container_add(GTK_CONTAINER(vbox), hbox2);

    
    
    
    
    gtk_widget_show_all(dialog);
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
            fit_2d_do(&controls, args);
            break;

            case RESPONSE_RESET:
            reset(&controls, args);
            break;

            case RESPONSE_FIT:
            fit_2d_run(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    fit_2d_dialog_abandon(&controls);

    return TRUE;
}

static void
fit_2d_dialog_abandon(Fit2dControls *controls)
{		      
}

static void
reset(Fit2dControls *controls, Fit2dArgs *args)
{
}


static void
fit_2d_run(Fit2dControls *controls,
              Fit2dArgs *args)
{
    GwyNLFitter *fitter;
    GwyDataField *dfield, *weight;
    gdouble param[4], err[4];
    gboolean fix[4], fres;
    gdouble dimdata[4];
    gint i;
    gdouble max;
    
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->data,
							      "/0/data"));

    weight = GWY_DATA_FIELD(gwy_data_field_new(dfield->xres, dfield->yres, 10, 10, FALSE));
    gwy_data_field_fill(weight, 1);

    max = gwy_data_field_area_get_avg(dfield, dfield->xres/2-20, dfield->yres/2-20, 40, 40);
    gwy_data_field_add(dfield, -max);
    printf("maximum (%g) subtracted\n", max);
    
    dimdata[0] = (gdouble)dfield->xres;
    dimdata[1] = (gdouble)dfield->yres;
    dimdata[2] = dfield->xreal;
    dimdata[3] = dfield->yreal;

    fix[0] = args->par_fix[0];
    fix[1] = args->par_fix[1];
    fix[2] = args->par_fix[2];
    fix[3] = args->par_fix[3];
    
    param[0] = args->par_init[0];
    param[1] = dfield->xreal/2;/*args->par_init[1];*/
    param[2] = dfield->yreal/2;/*args->par_init[2];*/
    param[3] = args->par_init[3];
 
    fitter = gwy_math_nlfit_fit_2d(fit_sphere,
		      NULL,		      
		      dfield,
		      weight,
		      4,
		      param, err,
		      fix,
		      dimdata);

    for (i=0; i<4; i++)
    {
	printf("param %d: %g +- %g\n", i, param[i], err[i]);
    }
    for (i=0; i<(dfield->xres*dfield->yres); i++)
	dfield->data[i] = fit_sphere((gdouble)i, 4, param, dimdata, &fres);


    printf("maximum (%g) added\n", max);
    gwy_data_field_add(dfield, max);
    
    
    gwy_data_view_update(GWY_DATA_VIEW(controls->view));

    g_object_unref(weight);
    gwy_math_nlfit_free(fitter);
}

static void
fit_2d_do(Fit2dControls *controls,
             Fit2dArgs *args)
{
    GtkWidget *data_window;

    data_window = gwy_app_data_window_create(GWY_CONTAINER(args->data));
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
}

static void
double_entry_changed_cb(GtkWidget *entry, gdouble *value)
{
    *value = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
}

static void
toggle_changed_cb(GtkToggleButton *button, gboolean *value)
{
    *value = gtk_toggle_button_get_active(button);
}


static gdouble
fit_sphere(gdouble x,
	   G_GNUC_UNUSED gint n_param,
	   const gdouble *param,
	   gdouble *dimdata,
	   gboolean *fres)
/*dimdata[0]:xres, dimdata[1]:yres, dimdata[2]:xreal, dimdata[3]:yreal*/
/*param[0]: radius, param[1]: x0, param[2]: y0, param[3]: z0*/
{
    gdouble val;
    gint col, row;
    gint xres, yres;
    gdouble xreal, yreal;
    gdouble fcol, frow;

    xres = (gint)dimdata[0];
    yres = (gint)dimdata[1];
    xreal = dimdata[2];
    yreal = dimdata[3];
    
    col = (gint)floor(x/xres);
    row = (gint)(x - col*xres);
    fcol = col*xreal/xres;
    frow = row*yreal/yres;
    
    val = sqrt(param[0]*param[0] - (fcol - param[1])*(fcol - param[1])
	       - (frow - param[2])*(frow - param[2])) - param[0] - param[3];
   
/*    printf("x=%g, co, row = ( %d, %d), fcol, frow= (%g, %g)  params: %g, %g, %g, %g,  res: %g\n", 
	   x, col, row, fcol, frow, param[0], param[1], param[2], param[3],
	   val);*/
    
    *fres = TRUE;
    return val;
    
}

GwyNLFitter*
gwy_math_nlfit_fit_2d(GwyNLFitFunc ff,
		      GwyNLFitDerFunc df,		      
		      GwyDataField *dfield,		     
		      GwyDataField *weight,
		      gint n_param,
		      gdouble *param, gdouble *err,
		      const gboolean *fixed_param,
		      gpointer user_data)
{
    GwyNLFitter *fitter;
    GwyDataField *xsc;
    gint i;

    xsc = gwy_data_field_new(dfield->xres, dfield->yres, dfield->xreal, dfield->yreal, FALSE);
    for (i=0; i<(dfield->xres*dfield->yres); i++) xsc->data[i] = i;
    
    
    if (df == NULL)
	fitter = gwy_math_nlfit_new(ff,
				    gwy_math_nlfit_derive);
    else
	fitter = gwy_math_nlfit_new(ff, df);

    gwy_math_nlfit_fit_with_fixed(fitter, dfield->xres*dfield->yres, 
				  xsc->data, dfield->data, weight->data,
				  n_param, param, fixed_param, user_data);

   
    if (fitter->covar)
    {
	for (i = 0; i < n_param; i++)
	    err[i] = gwy_math_nlfit_get_sigma(fitter, i);
    }

    g_object_unref(xsc);
    return fitter;
}



static const gchar *thresh_key = "/module/fit_2d/threshold";

static void
fit_2d_sanitize_args(Fit2dArgs *args)
{
}

static void
fit_2d_load_args(GwyContainer *container,
                    Fit2dArgs *args)
{
/*    *args = fit_2d_defaults;

    gwy_container_gis_double_by_name(container, thresh_key, &args->thresh);
    fit_2d_sanitize_args(args);
    */
}

static void
fit_2d_save_args(GwyContainer *container,
                    Fit2dArgs *args)
{
    /*
    gwy_container_set_double_by_name(container, thresh_key, args->thresh);
    */
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
