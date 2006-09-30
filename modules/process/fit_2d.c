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

#include "config.h"
#include <stdlib.h>
#include <errno.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwynlfit.h>
#include <libprocess/stats.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define FIT_2D_RUN_MODES GWY_RUN_INTERACTIVE

#define MAX_PARAMS 4

enum {
    PREVIEW_SIZE = 320
};

typedef enum {
    GWY_FIT_2D_DISPLAY_DATA = 0,
    GWY_FIT_2D_DISPLAY_RESULT = 1,
    GWY_FIT_2D_DISPLAY_DIFF = 2
} GwyFit2DDisplayType;

typedef enum {
    GWY_FIT_2D_FIT_SPHERE_UP = 0,
    GWY_FIT_2D_FIT_SPHERE_DOWN = 1
} GwyFit2DFunctionType;

typedef struct {
    gdouble par_init[MAX_PARAMS];
    gdouble par_res[MAX_PARAMS];
    gdouble par_err[MAX_PARAMS];
    gboolean par_fix[MAX_PARAMS];
    GwyFit2DDisplayType display_type;
    GwyFit2DFunctionType function_type;
} Fit2DArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *type;
    GtkWidget **param_des;
    GtkWidget **param_init;
    GtkWidget **param_res;
    GtkWidget **param_err;
    GtkWidget **param_fit;
    GtkWidget **covar;
    GtkWidget *chisq;
    GtkWidget *menu_display;
    GtkWidget *menu_function;
    gboolean is_fitted;
    GwyNLFitter *fitter;
    GwyContainer *mydata;
    GwyContainer *data;
    GwyDataField *original_field;
    GwyDataField *fit_field;
    gint original_id;
    Fit2DArgs *args;
} Fit2DControls;

static gboolean    module_register          (void);
static void        fit_2d                   (GwyContainer *data,
                                             GwyRunType run);
static void        fit_2d_dialog            (Fit2DArgs *args,
                                             GwyContainer *data,
                                             GwyDataField *dfield,
                                             gint id);
static void        guess                    (Fit2DControls *controls,
                                             Fit2DArgs *args);
static void        plot_inits               (Fit2DControls *controls,
                                             Fit2DArgs *args);
static void        fit_2d_load_args         (GwyContainer *container,
                                             Fit2DArgs *args);
static void        fit_2d_save_args         (GwyContainer *container,
                                             Fit2DArgs *args);
static void        fit_2d_sanitize_args     (Fit2DArgs *args);
static void        fit_2d_run               (Fit2DControls *controls,
                                             Fit2DArgs *args);
static void        fit_2d_do                (Fit2DControls *controls);
static void        fit_2d_dialog_abandon    (Fit2DControls *controls);
static GtkWidget*  menu_display             (GCallback callback,
                                             gpointer cbdata,
                                             GwyFit2DDisplayType current);
static GtkWidget*  menu_function            (GCallback callback,
                                             gpointer cbdata,
                                             GwyFit2DFunctionType current);
static void        display_changed          (GtkWidget *combo,
                                             Fit2DControls *controls);
static void        function_changed         (GtkWidget *combo,
                                             Fit2DControls *controls);
static void        double_entry_changed_cb  (GtkWidget *entry,
                                             gdouble *value);
static void        toggle_changed_cb        (GtkToggleButton *button,
                                             gboolean *value);
static void        create_results_window    (Fit2DControls *controls,
                                             Fit2DArgs *args);
static GString*    create_fit_report        (Fit2DControls *controls,
                                             Fit2DArgs *args);
static void        update_view              (Fit2DControls *controls,
                                             Fit2DArgs *args);
static gdouble     fit_sphere_up            (gdouble x,
                                             G_GNUC_UNUSED gint n_param,
                                             const gdouble *param,
                                             gdouble *dimdata,
                                             gboolean *fres);
static gdouble     fit_sphere_down          (gdouble x,
                                             G_GNUC_UNUSED gint n_param,
                                             const gdouble *param,
                                             gdouble *dimdata,
                                             gboolean *fres);
static void        guess_sphere_up          (GwyDataField *dfield,
                                             G_GNUC_UNUSED gint n_param,
                                             gdouble *param);
static void        guess_sphere_down        (GwyDataField *dfield,
                                             G_GNUC_UNUSED gint n_param,
                                             gdouble *param);
static GwyNLFitter* gwy_math_nlfit_fit_2d   (GwyNLFitFunc ff,
                                             GwyNLFitDerFunc df,
                                             GwyDataField *dfield,
                                             GwyDataField *weight,
                                             gint n_param,
                                             gdouble *param,
                                             gdouble *err,
                                             const gboolean *fixed_param,
                                             gpointer user_data);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("2D fitting"),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fit_2d",
                              (GwyProcessFunc)&fit_2d,
                              N_("/_Level/_Fit Sphere..."),
                              NULL,
                              FIT_2D_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Fit by sphere"));

    return TRUE;
}

static void
fit_2d(GwyContainer *data, GwyRunType run)
{
    Fit2DArgs args;
    GwyDataField *dfield;
    gint id;
    
    g_return_if_fail(run & FIT_2D_RUN_MODES);

    fit_2d_load_args(gwy_app_settings_get(), &args);
    args.par_fix[0] = FALSE;
    args.par_fix[1] = TRUE;
    args.par_fix[2] = TRUE;
    args.par_fix[3] = FALSE;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    fit_2d_dialog(&args, data, dfield, id);
    fit_2d_save_args(gwy_app_settings_get(), &args);
}

static void
fit_2d_dialog(Fit2DArgs *args, 
              GwyContainer *data, 
              GwyDataField *dfield,
              gint id)
{
    GtkWidget *dialog, *table, *hbox, *vbox, *hbox2;
    Fit2DControls controls;
    gdouble zoomval;
    enum {
        RESPONSE_FIT = 1,
        RESPONSE_INITS = 2,
        RESPONSE_GUESS = 3
    };
    gint response, i, j;
    GwyPixmapLayer *layer;
    GtkWidget *label;

    controls.fitter = NULL;
    controls.args = args;
    controls.original_field = dfield;
    controls.original_id = id;
    dialog = gtk_dialog_new_with_buttons(_("Fit sphere"), NULL, 0,
                                         _("_Fit"), RESPONSE_FIT,
                                         _("_Guess"), RESPONSE_GUESS,
                                         _("_Plot Inits"), RESPONSE_INITS,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);


    controls.fit_field = gwy_data_field_duplicate(dfield);
    gwy_data_field_clear(controls.fit_field);

    /*set up data of rescaled image of the surface*/
    controls.data = data;
    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", gwy_data_field_duplicate(dfield));
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    /*set up fit controls*/
    vbox = gtk_vbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    label = gwy_label_new_header(_("Fitting Parameters"));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 4);

    table = gtk_table_new(2, 2, FALSE);

    label = gtk_label_new_with_mnemonic(_("Function type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    controls.menu_function = menu_function(G_CALLBACK(function_changed),
                                           &controls,
                                           args->function_type);

    gtk_table_attach(GTK_TABLE(table), controls.menu_function, 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gtk_label_new_with_mnemonic(_("Preview type:"));

    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    controls.menu_display = menu_display(G_CALLBACK(display_changed),
                                         &controls,
                                         args->display_type);

    gtk_table_attach(GTK_TABLE(table), controls.menu_display, 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    gtk_container_add(GTK_CONTAINER(vbox), table);

    table = gtk_table_new(4, 6, FALSE);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), " ");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gwy_label_new_header(_("Initial"));
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gwy_label_new_header(_("Result"));
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);


    label = gwy_label_new_header(_("Error"));
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);

    label = gwy_label_new_header(_("Fix"));
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

    controls.param_init = g_new(GtkWidget*, MAX_PARAMS);
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

    controls.param_res = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_res[i] = gtk_label_new(NULL);
        gtk_table_attach(GTK_TABLE(table), controls.param_res[i],
                         2, 3, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    controls.param_err = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_err[i] = gtk_label_new(NULL);
        gtk_table_attach(GTK_TABLE(table), controls.param_err[i],
                         3, 4, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }

    controls.param_fit = g_new(GtkWidget*, MAX_PARAMS);
    for (i = 0; i < MAX_PARAMS; i++) {
        controls.param_fit[i] = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.param_fit[i]), args->par_fix[i]);
        g_signal_connect(controls.param_fit[i], "toggled",
                         G_CALLBACK(toggle_changed_cb), &args->par_fix[i]);
        gtk_table_attach(GTK_TABLE(table), controls.param_fit[i],
                         4, 5, i+1, i+2,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    }
    gtk_container_add(GTK_CONTAINER(vbox), table);

    label = gwy_label_new_header(_("Correlation Matrix"));
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
    gtk_label_set_markup(GTK_LABEL(label), _("χ<sup>2</sup> result:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(hbox2), label);

    controls.chisq = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.chisq), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(hbox2), controls.chisq);

    gtk_container_add(GTK_CONTAINER(vbox), hbox2);

    guess(&controls, args);
    update_view(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            fit_2d_do(&controls);
            if (controls.is_fitted && controls.fitter && controls.fitter->covar)
                create_results_window(&controls, args);
            break;

            case RESPONSE_FIT:
            fit_2d_run(&controls, args);
            break;

            case RESPONSE_GUESS:
            guess(&controls, args);
            break;

            case RESPONSE_INITS:
            plot_inits(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    fit_2d_dialog_abandon(&controls);

}

static void
fit_2d_dialog_abandon(Fit2DControls *controls)
{
    if (controls->fitter)
        gwy_math_nlfit_free(controls->fitter);
    gwy_object_unref(controls->fit_field);
    gwy_object_unref(controls->mydata);
}

/*update preview depending on user's wishes*/
static void
update_view(Fit2DControls *controls, Fit2DArgs *args)
{
    GwyDataField *outputfield;

    g_return_if_fail(GWY_IS_DATA_FIELD(controls->original_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(controls->fit_field));

    outputfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                                  "/0/data"));
    if (args->display_type == GWY_FIT_2D_DISPLAY_DATA)
        gwy_data_field_copy(controls->original_field, outputfield, FALSE);
    else if (args->display_type == GWY_FIT_2D_DISPLAY_RESULT)
        gwy_data_field_copy(controls->fit_field, outputfield, FALSE);
    else 
        gwy_data_field_subtract_fields(outputfield, controls->original_field,
                                           controls->fit_field);
 

    gwy_data_field_data_changed(outputfield);
}

/*call appropriate guess function and reset all result fields*/
static void
guess
(Fit2DControls *controls, Fit2DArgs *args)
{
    gint i, j;
    gchar buffer[20];

    if (args->function_type == GWY_FIT_2D_FIT_SPHERE_UP)
        guess_sphere_up(controls->original_field, 4, args->par_init);
    else
        guess_sphere_down(controls->original_field, 4, args->par_init);

    gtk_label_set_text(GTK_LABEL(controls->param_des[0]), "radius");
    gtk_label_set_text(GTK_LABEL(controls->param_des[1]), "x center");
    gtk_label_set_text(GTK_LABEL(controls->param_des[2]), "y center");
    gtk_label_set_text(GTK_LABEL(controls->param_des[3]), "z center");

    gtk_label_set_text(GTK_LABEL(controls->chisq), " ");
    for (i = 0; i < 4; i++) {
        gtk_widget_set_sensitive(controls->param_init[i], TRUE);
        gtk_widget_set_sensitive(controls->param_fit[i], TRUE);
        g_snprintf(buffer, sizeof(buffer), "%.3g", args->par_init[i]);
        gtk_entry_set_text(GTK_ENTRY(controls->param_init[i]), buffer);
        gtk_label_set_text(GTK_LABEL(controls->param_res[i]), " ");
        gtk_label_set_text(GTK_LABEL(controls->param_err[i]), " ");

        for (j = 0; j <= i; j++) {
            gtk_label_set_text
                   (GTK_LABEL(controls->covar[i*MAX_PARAMS + j]), " ");
        }
     }
     controls->is_fitted = 0;
}

/*plot guessed (or user) initial parameters*/
static void
plot_inits(Fit2DControls *controls, Fit2DArgs *args)
{
    gint i, xres, yres;
    gdouble dimdata[4];
    gboolean fres;
    gdouble *data;

    xres = gwy_data_field_get_xres(controls->fit_field);
    yres = gwy_data_field_get_yres(controls->fit_field);
    data = gwy_data_field_get_data(controls->fit_field);

    dimdata[0] = (gdouble)xres;
    dimdata[1] = (gdouble)yres;
    dimdata[2] = gwy_data_field_get_xreal(controls->fit_field);
    dimdata[3] = gwy_data_field_get_yreal(controls->fit_field);

    if (args->function_type == GWY_FIT_2D_FIT_SPHERE_UP)
        for (i = 0; i < (xres*yres); i++)
            data[i] = fit_sphere_up((gdouble)i, 4, args->par_init, dimdata, &fres);
    else
        for (i = 0; i < (xres*yres); i++)
            data[i] = fit_sphere_down((gdouble)i, 4, args->par_init, dimdata, &fres);

    controls->is_fitted = TRUE;

    update_view(controls, args);
}


/*fit data*/
static void
fit_2d_run(Fit2DControls *controls,
              Fit2DArgs *args)
{
    GtkWidget *dialog;
    GwyDataField *weight;
    gdouble param[4], err[4];
    gboolean fres;
    gdouble dimdata[4], *data;
    gchar buffer[20];
    gint i, j, nparams, xres, yres;

    param[0] = args->par_init[0];
    param[1] = args->par_init[1];
    param[2] = args->par_init[2];
    param[3] = args->par_init[3];

     if (param[0] <= 0) {
       dialog = gtk_message_dialog_new
            (gwy_app_find_window_for_channel(controls->data, controls->original_id),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_ERROR,
             GTK_BUTTONS_OK,
             _("%s: Radius cannot be null or negative."), "Fit");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }


    gwy_app_wait_start(GTK_WINDOW(controls->dialog), _("Initializing"));

    xres = gwy_data_field_get_xres(controls->original_field);
    yres = gwy_data_field_get_yres(controls->original_field);
    weight = gwy_data_field_new_alike(controls->original_field, FALSE);
    gwy_data_field_fill(weight, 1);

    nparams = 4;
    dimdata[0] = (gdouble)xres;
    dimdata[1] = (gdouble)yres;
    dimdata[2] = gwy_data_field_get_xreal(controls->original_field);
    dimdata[3] = gwy_data_field_get_yreal(controls->original_field);

    gwy_app_wait_set_message(_("Fitting"));
    if (controls->fitter) gwy_math_nlfit_free(controls->fitter);
    if (args->function_type == GWY_FIT_2D_FIT_SPHERE_UP)
        controls->fitter = gwy_math_nlfit_fit_2d((GwyNLFitFunc)fit_sphere_up,
                                             NULL,
                                             controls->original_field,
                                             weight,
                                             4,
                                             param, err,
                                             args->par_fix,
                                             dimdata);
    else
        controls->fitter = gwy_math_nlfit_fit_2d((GwyNLFitFunc)fit_sphere_down,
                                             NULL,
                                             controls->original_field,
                                             weight,
                                             4,
                                             param, err,
                                             args->par_fix,
                                             dimdata);

    gwy_app_wait_finish();

    data = gwy_data_field_get_data(controls->fit_field);
    if (args->function_type == GWY_FIT_2D_FIT_SPHERE_UP)
        for (i = 0; i < (xres*yres); i++)
            data[i] = fit_sphere_up((gdouble)i, 4, param, dimdata, &fres);
    else
        for (i = 0; i < (xres*yres); i++)
            data[i] = fit_sphere_down((gdouble)i, 4, param, dimdata, &fres);

    controls->is_fitted = 1;
    for (i = 0; i < 4; i++)
    {
        args->par_res[i] = param[i];
        args->par_err[i] = err[i];
        g_snprintf(buffer, sizeof(buffer), "%.3g", param[i]);
        gtk_label_set_text(GTK_LABEL(controls->param_res[i]), buffer);
        g_snprintf(buffer, sizeof(buffer), "%.3g", err[i]);
        gtk_label_set_text(GTK_LABEL(controls->param_err[i]), buffer);
    }
    if (controls->fitter->covar)
    {
        g_snprintf(buffer, sizeof(buffer), "%2.3g",
                   gwy_math_nlfit_get_dispersion(controls->fitter));
        gtk_label_set_markup(GTK_LABEL(controls->chisq), buffer);

        for (i = 0; i < nparams; i++) {
            for (j = 0; j <= i; j++) {
                g_snprintf(buffer, sizeof(buffer), "% 0.3f",
                           gwy_math_nlfit_get_correlations(controls->fitter, i, j));
                gtk_label_set_markup
                    (GTK_LABEL(controls->covar[i*MAX_PARAMS + j]), buffer);
            }
        }
    }
    update_view(controls, args);
    g_object_unref(weight);
}

/*dialog finished, export result data*/
static void
fit_2d_do(Fit2DControls *controls)
{
    gint newid;

    newid = gwy_app_data_browser_add_data_field(controls->fit_field, controls->data, TRUE);
    gwy_app_set_data_field_title(controls->data, newid, _("Fitted sphere"));
}

/*display mode menu*/
static GtkWidget*
menu_display(GCallback callback, gpointer cbdata,
             GwyFit2DDisplayType current)
{
    static const GwyEnum entries[] = {
        { N_("Data"),        GWY_FIT_2D_DISPLAY_DATA,   },
        { N_("Fit result"),  GWY_FIT_2D_DISPLAY_RESULT, },
        { N_("Difference"),  GWY_FIT_2D_DISPLAY_DIFF,   },
    };
    return gwy_enum_combo_box_new(entries, G_N_ELEMENTS(entries),
                                  callback, cbdata, current, TRUE);
}

/*function type menu*/
static GtkWidget*
menu_function(GCallback callback, gpointer cbdata,
              GwyFit2DFunctionType current)
{
    static const GwyEnum entries[] = {
        { N_("Sphere (up)"),    GWY_FIT_2D_FIT_SPHERE_UP,    },
        { N_("Sphere (down)"),  GWY_FIT_2D_FIT_SPHERE_DOWN,  },
    };
    return gwy_enum_combo_box_new(entries, G_N_ELEMENTS(entries),
                                  callback, cbdata, current, TRUE);
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
display_changed(GtkWidget *combo, Fit2DControls *controls)
{
    controls->args->display_type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    update_view(controls, controls->args);
}

static void
function_changed(GtkWidget *combo, Fit2DControls *controls)
{
    controls->args->function_type = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    guess(controls, controls->args);
    update_view(controls, controls->args);
}

/*extract radius and center from upper section of sphere*/
static void
guess_sphere_up(GwyDataField *dfield,
                G_GNUC_UNUSED gint n_param,
                gdouble *param)
{
    gint xres, yres;
    gdouble t, v, avgcorner, avgtop, xreal, yreal;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);


    avgcorner = gwy_data_field_area_get_avg(dfield, NULL, 0, 0, 10, 10);
    avgcorner += gwy_data_field_area_get_avg(dfield, NULL, xres-10, 0, 10, 10);
    avgcorner += gwy_data_field_area_get_avg(dfield, NULL, 0, yres-10, 10, 10);
    avgcorner += gwy_data_field_area_get_avg(dfield, NULL, xres-10, yres-10, 10, 10);
    avgcorner/=4;

    avgtop = gwy_data_field_area_get_avg(dfield, NULL, xres/2-5, yres/2-5,
                                         10, 10);

    v = avgtop - avgcorner;
    t = hypot(xreal, yreal);
    param[0] = fabs((t*t - 4*v*v)/8/v);
    param[1] = xreal/2;
    param[2] = yreal/2;
    param[3] = avgtop-param[0];
}

/*extract radius and center from lower section of sphere*/
static void
guess_sphere_down(GwyDataField *dfield,
       G_GNUC_UNUSED gint n_param,
       gdouble *param)
{
    gint xres, yres;
    gdouble t, v, avgcorner, avgtop, xreal, yreal;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);

    avgcorner = gwy_data_field_area_get_avg(dfield, NULL, 0, 0, 10, 10);
    avgcorner += gwy_data_field_area_get_avg(dfield, NULL, xres-10, 0, 10, 10);
    avgcorner += gwy_data_field_area_get_avg(dfield, NULL, 0, yres-10, 10, 10);
    avgcorner += gwy_data_field_area_get_avg(dfield, NULL, xres-10, yres-10, 10, 10);
    avgcorner/=4;

    avgtop = gwy_data_field_area_get_avg(dfield, NULL, xres/2-5, yres/2-5,
                                         10, 10);

    v = avgtop - avgcorner;
    t = sqrt(xreal*xreal + yreal*yreal);
    param[0] = fabs((t*t - 4*v*v)/8/v);
    param[1] = xreal/2;
    param[2] = yreal/2;
    param[3] = avgtop+param[0];
}

/*fit upper section of sphere*/
static gdouble
fit_sphere_up(gdouble x,
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
           - (frow - param[2])*(frow - param[2])) + param[3];

    *fres = TRUE;
    return val;

}

/*fit lower section of sphere*/
static gdouble
fit_sphere_down(gdouble x,
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

    val = -sqrt(param[0]*param[0] - (fcol - param[1])*(fcol - param[1])
          - (frow - param[2])*(frow - param[2])) + param[3];

    *fres = TRUE;
    return val;

}

/*fitter construction*/
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
    gdouble *data;
    gint i, xres, yres;

    xsc = gwy_data_field_new_alike(dfield, FALSE);

    xres = gwy_data_field_get_xres(xsc);
    yres = gwy_data_field_get_yres(xsc);
    data = gwy_data_field_get_data(xsc);
    
    for (i = 0; i < (xres*yres); i++) data[i] = i;
    if (df == NULL)
        fitter = gwy_math_nlfit_new(ff,
                                    gwy_math_nlfit_derive);
    else
        fitter = gwy_math_nlfit_new(ff, df);

    gwy_math_nlfit_fit_full(fitter, xres*yres,
                            data,
                            gwy_data_field_get_data(dfield),
                            gwy_data_field_get_data(weight),
                            n_param, param, fixed_param, NULL, user_data);


    if (fitter->covar)
    {
        for (i = 0; i < n_param; i++)
            err[i] = gwy_math_nlfit_get_sigma(fitter, i);
    }

    g_object_unref(xsc);
    return fitter;
}


static const gchar function_key[] = "/module/fit_2d/function";
static const gchar display_key[]  = "/module/fit_2d/display";

static void
fit_2d_sanitize_args(Fit2DArgs *args)
{
    args->display_type = MIN(args->display_type, GWY_FIT_2D_DISPLAY_DIFF);
    args->function_type = MIN(args->display_type, GWY_FIT_2D_FIT_SPHERE_DOWN);
}

static void
fit_2d_load_args(GwyContainer *container,
                    Fit2DArgs *args)
{
    gwy_container_gis_enum_by_name(container, display_key, &args->display_type);
    gwy_container_gis_enum_by_name(container, function_key, &args->function_type);
    fit_2d_sanitize_args(args);
}

static void
fit_2d_save_args(GwyContainer *container,
                    Fit2DArgs *args)
{
    gwy_container_set_enum_by_name(container, display_key, args->display_type);
    gwy_container_set_enum_by_name(container, function_key, args->function_type);

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

    fh = g_fopen(filename_sys, "a");
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
create_results_window(Fit2DControls *controls, Fit2DArgs *args)
{
    enum { RESPONSE_SAVE = 1 };
    GwyNLFitter *fitter = controls->fitter;
    GtkWidget *window, *tab, *table;
    gdouble mag, value, sigma;
    gint row, n, i, j;
    gint precision;
    GString *str, *su;

    g_return_if_fail(controls->is_fitted);
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
    
    str = g_string_new(gwy_app_get_data_field_title(controls->data, controls->original_id));
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

    attach_label(table, _("<b>Correlation Matrix</b>"), row, 0, 0.0);
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

    str = create_fit_report(controls, args);

    g_signal_connect(window, "response",
                     G_CALLBACK(results_window_response_cb), str);
    gtk_widget_show_all(window);
}

static GString*
create_fit_report(Fit2DControls *controls, Fit2DArgs *args)
{
    GString *report, *str;
    gchar *s, *s2;
    gint i, j, n;

    s = NULL;
    g_assert(controls->fitter->covar);
    report = g_string_new("");

    g_string_append_printf(report, _("\n===== Fit Results =====\n"));

    str = g_string_new(gwy_app_get_data_field_title(controls->data, controls->original_id));
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
                           gwy_math_nlfit_get_dispersion(controls->fitter));
    g_string_append_printf(report, _("\nCorrelation matrix\n"));
    for (i = 0; i < n; i++) {
        for (j = 0; j <= i; j++) {
            g_string_append_printf
                (report, "% .03f",
                 gwy_math_nlfit_get_correlations(controls->fitter, i, j));
            if (j != i)
                g_string_append_c(report, ' ');
        }
        g_string_append_c(report, '\n');
    }

    g_string_free(str, TRUE);

    return report;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */




