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
#include <errno.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
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
    GwyContainer *original_data;
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
static void        guess                      (Fit2dControls *controls,
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
static void        dialog_update             (Fit2dControls *controls,
                                             Fit2dArgs *args);
static void        create_results_window     (Fit2dArgs *args);
static GString*    create_fit_report         (Fit2dArgs *args);

static gdouble     fit_sphere		(gdouble x,
					   G_GNUC_UNUSED gint n_param,
					   const gdouble *param,
					   gdouble *dimdata,
					   gboolean *fres);

static void        guess_sphere(GwyDataField *dfield,
	                   G_GNUC_UNUSED gint n_param,
	                   gdouble *param);

static GwyNLFitter*	   gwy_math_nlfit_fit_2d(GwyNLFitFunc ff,
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
    args.original_data = data;
    args.fitter = NULL;
    
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
        RESPONSE_RESET = 2,
        RESPONSE_GUESS = 3
    };
    gint response, row, i, j;
    GtkObject *layer;
    GwyDataField *dfield;
    GtkWidget *label;

    dialog = gtk_dialog_new_with_buttons(_("2D fit"), NULL, 0,
                                         _("_Guess"), RESPONSE_GUESS,
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

    dialog_update(&controls, args); 
     
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
            if (args->fitter && args->fitter->covar)
                create_results_window(args);
            break;

            case RESPONSE_RESET:
            reset(&controls, args);
            break;

            case RESPONSE_FIT:
            fit_2d_run(&controls, args);
            break;

            case RESPONSE_GUESS:
            guess(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    fit_2d_dialog_abandon(&controls);
    if (args->fitter) gwy_math_nlfit_free(args->fitter);

    return TRUE;
}

static void
fit_2d_dialog_abandon(Fit2dControls *controls)
{		
}

static void        
dialog_update             
(Fit2dControls *controls, Fit2dArgs *args)
{
    gint i;
    gchar buffer[20];
    
    guess(controls, args);

    gtk_label_set_text(GTK_LABEL(controls->param_des[0]), "radius");
    gtk_label_set_text(GTK_LABEL(controls->param_des[1]), "x center");
    gtk_label_set_text(GTK_LABEL(controls->param_des[2]), "y center");
    gtk_label_set_text(GTK_LABEL(controls->param_des[3]), "z center");

    for (i=0; i<4; i++)
    {
        gtk_widget_set_sensitive(controls->param_init[i], TRUE);
        gtk_widget_set_sensitive(controls->param_fit[i], TRUE);
        g_snprintf(buffer, sizeof(buffer), "%.3g", args->par_init[i]);
        gtk_entry_set_text(GTK_ENTRY(controls->param_init[i]), buffer);
        gtk_label_set_text(GTK_LABEL(controls->param_res[i]), " ");
        gtk_label_set_text(GTK_LABEL(controls->param_err[i]), " ");
    }

}

static void
reset(Fit2dControls *controls, Fit2dArgs *args)
{
    dialog_update(controls, args);
}

static void
guess(Fit2dControls *controls, Fit2dArgs *args)
{
    
    GwyDataField *dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->data,
                                                                "/0/data"));
    guess_sphere(dfield, 4, args->par_init);
}



static void
fit_2d_run(Fit2dControls *controls,
              Fit2dArgs *args)
{
    GwyDataField *dfield, *weight;
    gdouble param[4], err[4];
    gboolean fix[4], fres;
    gdouble dimdata[4];
    gchar buffer[20];
    gint i, j, nparams;
    gdouble max;
    
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(args->data,
							      "/0/data"));

    gwy_app_wait_start(GTK_WIDGET(gwy_app_data_window_get_for_data(args->original_data)),
                        _("Initializing"));
    
    weight = GWY_DATA_FIELD(gwy_data_field_new(dfield->xres, dfield->yres, 10, 10, FALSE));
    gwy_data_field_fill(weight, 1);

    max = gwy_data_field_area_get_avg(dfield, dfield->xres/2-20, dfield->yres/2-20, 40, 40);
    gwy_data_field_add(dfield, -max);
    
    nparams = 4;
    dimdata[0] = (gdouble)dfield->xres;
    dimdata[1] = (gdouble)dfield->yres;
    dimdata[2] = dfield->xreal;
    dimdata[3] = dfield->yreal;

    fix[0] = args->par_fix[0];
    fix[1] = args->par_fix[1];
    fix[2] = args->par_fix[2];
    fix[3] = args->par_fix[3];

    param[0] = args->par_init[0];
    param[1] = args->par_init[1];
    param[2] = args->par_init[2];
    param[3] = args->par_init[3];
    
  
    gwy_app_wait_set_message(_("Fitting"));
    if (args->fitter) gwy_math_nlfit_free(args->fitter);
    args->fitter = gwy_math_nlfit_fit_2d(fit_sphere,
		      NULL,		      
		      dfield,
		      weight,
		      4,
		      param, err,
		      fix,
		      dimdata);
    gwy_app_wait_finish();

    for (i=0; i<(dfield->xres*dfield->yres); i++)
	dfield->data[i] = fit_sphere((gdouble)i, 4, param, dimdata, &fres);

    for (i=0; i<4; i++)
    {
        g_snprintf(buffer, sizeof(buffer), "%.3g", param[i]);
        gtk_label_set_text(GTK_LABEL(controls->param_res[i]), buffer);
        g_snprintf(buffer, sizeof(buffer), "%.3g", err[i]);
        gtk_label_set_text(GTK_LABEL(controls->param_err[i]), buffer);
    }
    if (args->fitter->covar)
    {
        g_snprintf(buffer, sizeof(buffer), "%2.3g",
                   gwy_math_nlfit_get_dispersion(args->fitter));
        gtk_label_set_markup(GTK_LABEL(controls->chisq), buffer);

        for (i = 0; i < nparams; i++) {
            for (j = 0; j <= i; j++) {
                g_snprintf(buffer, sizeof(buffer), "% 0.3f",
                           gwy_math_nlfit_get_correlations(args->fitter, i, j));
                gtk_label_set_markup
                    (GTK_LABEL(controls->covar[i*MAX_PARAMS + j]), buffer);
            }
        }
    }
 
    gwy_data_view_update(GWY_DATA_VIEW(controls->view));

    g_object_unref(weight);
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

static void
guess_sphere(GwyDataField *dfield,
	   G_GNUC_UNUSED gint n_param,
	   gdouble *param)
{
    gdouble t, v, avgcorner, avgtop;

    avgcorner = gwy_data_field_area_get_avg(dfield, 0, 0, 10, 10);
    avgcorner += gwy_data_field_area_get_avg(dfield, dfield->xres-10, 0, 10, 10);
    avgcorner += gwy_data_field_area_get_avg(dfield, 0, dfield->yres-10, 10, 10);
    avgcorner += gwy_data_field_area_get_avg(dfield, dfield->xres-10, dfield->yres-10, 10, 10);

    avgtop = gwy_data_field_area_get_avg(dfield, dfield->xres/2-5, dfield->yres/2-5, 
                                         10, 10);
    
    v = avgtop - avgcorner;
    t = sqrt(dfield->xreal*dfield->xreal + dfield->yreal*dfield->yreal);
    
    param[0] = fabs(t*t - 2*v*v)/4/v;
    param[1] = dfield->xreal/2;
    param[2] = dfield->yreal/2;
    if (avgtop<avgcorner) param[3] = param[0];
    else param[3] = -param[0];
    
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

static GwyNLFitter*
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









/************************* fit report *****************************/
static void
attach_label(GtkWidget *table, const gchar *text,
             gint row, gint col, gdouble halign)
{
    GtkWidget *label;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_misc_set_alignment(GTK_MISC(label), halign, 0.5);

    gtk_table_attach(GTK_TABLE(table), label,
                     col, col+1, row, row+1, GTK_FILL, 0, 2, 2);
}

static void
save_report_cb(GtkWidget *button, GString *report)
{
    const gchar *filename;
    gchar *filename_sys, *filename_utf8;
    GtkWidget *dialog;
    FILE *fh;

    dialog = gtk_widget_get_toplevel(button);
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
    filename_sys = g_strdup(filename);
    gtk_widget_destroy(dialog);

    fh = fopen(filename_sys, "a");
    if (fh) {
        fputs(report->str, fh);
        fclose(fh);
        return;
    }

    filename_utf8 = g_filename_to_utf8(filename_sys, -1, 0, 0, NULL);
    dialog = gtk_message_dialog_new(NULL,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    _("Cannot save report to %s.\n%s\n"),
                                    filename_utf8,
                                    g_strerror(errno));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void
results_window_response_cb(GtkWidget *window,
                           gint response,
                           GString *report)
{
    GtkWidget *dialog;

    if (response == GTK_RESPONSE_CLOSE
        || response == GTK_RESPONSE_DELETE_EVENT
        || response == GTK_RESPONSE_NONE) {
        if (report)
            g_string_free(report, TRUE);
        gtk_widget_destroy(window);
        return;
    }

    g_assert(report);
    dialog = gtk_file_selection_new(_("Save Fit Report"));

    g_signal_connect(GTK_FILE_SELECTION(dialog)->ok_button, "clicked",
                     G_CALLBACK(save_report_cb), report);
    g_signal_connect_swapped(GTK_FILE_SELECTION(dialog)->cancel_button,
                             "clicked",
                             G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_widget_show_all(dialog);
}

static gchar*
format_magnitude(GString *str,
                 gdouble magnitude)
{
    if (magnitude)
        g_string_printf(str, "× 10<sup>%d</sup>",
                        (gint)floor(log10(magnitude) + 0.5));
    else
        g_string_assign(str, "");

    return str->str;
}


static void
create_results_window(Fit2dArgs *args)
{
    enum { RESPONSE_SAVE = 1 };
    GwyNLFitter *fitter = args->fitter;
    GtkWidget *window, *tab, *table, *label;
    gdouble mag, value, sigma;
    gint row, curve, n, i, j;
    gint precision;
    GString *str, *su;
    const gchar *s;

    g_return_if_fail(args->is_fitted);
    g_return_if_fail(fitter->covar);

    window = gtk_dialog_new_with_buttons(_("Fit results"), NULL, 0,
                                         GTK_STOCK_SAVE, RESPONSE_SAVE,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_CLOSE);

    table = gtk_table_new(9, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), table,
                       FALSE, FALSE, 0);
    row = 0;

    attach_label(table, _(_("<b>Data:</b>")), row, 0, 0.0);
    str = g_string_new(gwy_data_window_get_base_name(gwy_app_data_window_get_for_data(args->original_data)));
    su = g_string_new("");
    attach_label(table, str->str, row, 1, 0.0);
    row++;

    attach_label(table, _("<b>Function:</b>"), row, 0, 0.0);
    attach_label(table, "sphere",
                 row, 1, 0.0);
    row++;

    attach_label(table, _("<b>Results</b>"), row, 0, 0.0);
    row++;

    n = 4;
    tab = gtk_table_new(n, 6, FALSE);
    gtk_table_attach(GTK_TABLE(table), tab, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    for (i = 0; i < n; i++) {
        attach_label(tab, "=", i, 1, 0.5);
        attach_label(tab, "±", i, 3, 0.5);
        if (i==0) attach_label(tab, "radius", i, 0, 0.0);
        if (i==1) attach_label(tab, "x center", i, 0, 0.0);
        if (i==2) attach_label(tab, "y center", i, 0, 0.0);
        if (i==3) attach_label(tab, "z center", i, 0, 0.0);
        value = args->par_res[i];
        sigma = args->par_err[i];
        mag = gwy_math_humanize_numbers(sigma/12, fabs(value), &precision);
        g_string_printf(str, "%.*f", precision, value/mag);
        attach_label(tab, str->str, i, 2, 1.0);
        g_string_printf(str, "%.*f", precision, sigma/mag);
        attach_label(tab, str->str, i, 4, 1.0);
        attach_label(tab, format_magnitude(su, mag), i, 5, 0.0);
    }
    row++;

    attach_label(table, _("Residual sum:"), row, 0, 0.0);
    sigma = gwy_math_nlfit_get_dispersion(fitter);
    mag = gwy_math_humanize_numbers(sigma/120, sigma, &precision);
    g_string_printf(str, "%.*f %s",
                    precision, sigma/mag, format_magnitude(su, mag));
    attach_label(table, str->str, row, 1, 0.0);
    row++;

    attach_label(table, _("<b>Correlation matrix</b>"), row, 0, 0.0);
    row++;

    tab = gtk_table_new(n, n, TRUE);
    for (i = 0; i < n; i++) {
        for (j = 0; j <= i; j++) {
            g_string_printf(str, "% .03f",
                            gwy_math_nlfit_get_correlations(fitter, i, j));
            attach_label(tab, str->str, i, j, 1.0);
        }
    }
    gtk_table_attach(GTK_TABLE(table), tab, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    g_string_free(str, TRUE);

    str = create_fit_report(args);

    g_signal_connect(window, "response",
                     G_CALLBACK(results_window_response_cb), str);
    gtk_widget_show_all(window);
}

static GString*
create_fit_report(Fit2dArgs *args)
{
    GString *report, *str;
    gchar *s, *s2;
    gint i, j, curve, n;

    
    g_assert(args->fitter->covar);
    report = g_string_new("");

    g_string_append_printf(report, _("\n===== Fit Results =====\n"));

    str = g_string_new(gwy_data_window_get_base_name(gwy_app_data_window_get_for_data(args->original_data)));
    g_string_append_printf(report, _("Data: %s\n"), str->str);
    str = g_string_new("");
    g_string_append_printf(report, _("Fitted function: sphere\n"));
    g_string_append_printf(report, _("\nResults\n"));
    n = 4;
    for (i = 0; i < n; i++) {
        /* FIXME: how to do this better? use pango_parse_markup()? */
        if (i==0) s = gwy_strreplace("radius", "<sub>", "", (gsize)-1);
        if (i==1) s = gwy_strreplace("x center", "<sub>", "", (gsize)-1);
        if (i==2) s = gwy_strreplace("y center", "<sub>", "", (gsize)-1);
        if (i==3) s = gwy_strreplace("z center", "<sub>", "", (gsize)-1);
        s2 = gwy_strreplace(s, "</sub>", "", (gsize)-1);
        g_string_append_printf(report, "%s = %g ± %g\n",
                               s2, args->par_res[i], args->par_err[i]);
        g_free(s2);
        g_free(s);
    }
    g_string_append_printf(report, _("\nResidual sum:   %g\n"),
                           gwy_math_nlfit_get_dispersion(args->fitter));
    g_string_append_printf(report, _("\nCorrelation matrix\n"));
    for (i = 0; i < n; i++) {
        for (j = 0; j <= i; j++) {
            g_string_append_printf
                (report, "% .03f",
                 gwy_math_nlfit_get_correlations(args->fitter, i, j));
            if (j != i)
                g_string_append_c(report, ' ');
        }
        g_string_append_c(report, '\n');
    }

    g_string_free(str, TRUE);

    return report;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */




