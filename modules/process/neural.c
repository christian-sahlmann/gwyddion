/*
 *  @(#) $Id: neural.c 8929 2008-12-31 13:40:16Z yeti-dn $
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyexpr.h>
#include <libprocess/datafield.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>


#define NEURAL_RUN_MODES GWY_RUN_INTERACTIVE

enum {
    NEURAL_OK   = 0,
    NEURAL_DATA = 1,
    NEURAL_EXPR = 2
};

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    GwyDataObjectId tmodel;
    GwyDataObjectId tsignal;
    GwyDataObjectId rmodel;
    gint hidden;
    gint trainsteps;
    gint width;
    gint height;
} NeuralArgs;

typedef struct {
    gdouble *input;
    gint ninput;
    gdouble *hidden;
    gint nhidden;
    gdouble *output;
    gint noutput;

    gdouble *dhidden;
    gdouble *doutput;
    gdouble *target;

    gdouble **whidden;
    gdouble **winput;
    gdouble **wphidden;
    gdouble **wpinput;

} GwyNN;

typedef struct {
    NeuralArgs *args;
    GtkWidget *dialog;
    GtkWidget *tmodel;
    GtkWidget *tsignal;
    GtkWidget *rmodel;
    GtkObject *hidden;
    GtkObject *trainsteps;
    GtkObject *width;
    GtkObject *height;
    GtkWidget *message;
    GtkWidget *ok;
} NeuralControls;

static const NeuralArgs neural_defaults = {
    { NULL, -1 },
    { NULL, -1 },
    { NULL, -1 },
    7,
    1000,
    20,
    20,
};


static gboolean     module_register           (void);
static void         neural                (GwyContainer *data,
                                               GwyRunType run);
static gboolean     neural_dialog         (NeuralArgs *args);
static void         neural_data_cb        (GwyDataChooser *chooser,
                                               NeuralControls *controls);
static void         neural_do             (NeuralArgs *args);
static void         neural_sanitize_args  (NeuralArgs *args);
static void         neural_load_args      (GwyContainer *container,
                                           NeuralArgs *args);
static void         neural_save_args      (GwyContainer *container,
                                           NeuralArgs *args);
static void         neural_values_update  (NeuralControls *controls,
                                           NeuralArgs *args);
static void         neural_dialog_update  (NeuralControls *controls,
                                           NeuralArgs *args);
static GwyNN*       gwy_nn_alloc          (gint input, 
                                           gint hidden,
                                           gint output);
static void         gwy_nn_feed_forward   (GwyNN* nn, 
                                           gdouble *input, 
                                           gdouble *output);

static void         layer_forward         (gdouble *input, 
                                           gdouble *output, 
                                           gdouble **weight, 
                                           gint nin, 
                                           gint nout);

static void         gwy_nn_train_step     (GwyNN *nn, 
                                           gdouble eta, 
                                           gdouble momentum, 
                                           gdouble *err_o, 
                                           gdouble *err_h, 
                                           gdouble *input, 
                                           gdouble *target);

static void         gwy_nn_free           (GwyNN *nn);

static const gchar default_expression[] = "d1 - d2";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Neural network SPM data processing"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("neural",
                              (GwyProcessFunc)&neural,
                              N_("/M_ultidata/_Neural network..."),
                              NULL,
                              NEURAL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Neural network AFM data processing"));

    return TRUE;
}

void
neural(GwyContainer *data, GwyRunType run)
{
    NeuralArgs args;
    GwyContainer *settings;
    gint id;

    g_return_if_fail(run & NEURAL_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id, 0);

    neural_load_args(gwy_app_settings_get(), &args);
    settings = gwy_app_settings_get();
    args.tmodel.data = data;
    args.tmodel.id = id;
    args.rmodel.data = data;
    args.rmodel.id = id;
    args.tsignal.data = data;
    args.tsignal.id = id;


    if (neural_dialog(&args)) {
        neural_do(&args);
        neural_save_args(gwy_app_settings_get(), &args);
    }
}

static gboolean
neural_dialog(NeuralArgs *args)
{
    NeuralControls controls;
    GtkWidget *dialog, *table, *label, *spin;
    guint row, response;
    enum { RESPONSE_RESET = 1 };

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Neural network"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
    controls.ok = gtk_dialog_add_button(GTK_DIALOG(dialog),
                                      GTK_STOCK_OK, GTK_RESPONSE_OK);

    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(10, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(_("Operands:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.tmodel = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(controls.tmodel), "dialog", dialog);
    gwy_table_attach_hscale(table, row, _("Training model:"), NULL,
                            GTK_OBJECT(controls.tmodel), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.tsignal = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(controls.tsignal), "dialog", dialog);
    gwy_table_attach_hscale(table, row, _("Training signal:"), NULL,
                            GTK_OBJECT(controls.tsignal), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.rmodel = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(controls.rmodel), "dialog", dialog);
    gwy_table_attach_hscale(table, row, _("Result model:"), NULL,
                            GTK_OBJECT(controls.rmodel), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;


    controls.width = gtk_adjustment_new(args->width, 1, 100, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Window width:"), "px",
                                       controls.width);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.height = gtk_adjustment_new(args->height, 1, 100, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Window height:"), "px",
                                       controls.height);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.trainsteps = gtk_adjustment_new(args->trainsteps, 1, 100000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Training steps:"), "",
                                       controls.trainsteps);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.hidden = gtk_adjustment_new(args->hidden, 1, 20, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Hidden nodes:"), "",
                                       controls.hidden);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.message = gtk_label_new((" "));
    gtk_misc_set_alignment(GTK_MISC(controls.message), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.message, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;


    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    g_signal_connect(controls.rmodel, "changed",
                     G_CALLBACK(neural_data_cb), &controls);
    g_signal_connect(controls.tmodel, "changed",
                     G_CALLBACK(neural_data_cb), &controls); 
    g_signal_connect(controls.tsignal, "changed",
                     G_CALLBACK(neural_data_cb), &controls);

    neural_data_cb(GWY_DATA_CHOOSER(controls.rmodel), &controls);


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
            neural_values_update(&controls, args);
            break;

            case RESPONSE_RESET:
            *args = neural_defaults;
            neural_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
neural_data_cb(G_GNUC_UNUSED GwyDataChooser *chooser,
                   NeuralControls *controls)
{
    NeuralArgs *args;
    GwyDataField *tf, *rf, *sf;
    GQuark quark;

    args = controls->args;
    args->tmodel.data = gwy_data_chooser_get_active(GWY_DATA_CHOOSER(controls->tmodel),
                                                        &args->tmodel.id);
    args->rmodel.data = gwy_data_chooser_get_active(GWY_DATA_CHOOSER(controls->rmodel),
                                                        &args->rmodel.id);
    args->tsignal.data = gwy_data_chooser_get_active(GWY_DATA_CHOOSER(controls->tsignal),
                                                     &args->tsignal.id);

    quark = gwy_app_get_data_key_for_id(args->tmodel.id);
    tf = GWY_DATA_FIELD(gwy_container_get_object(args->tmodel.data, quark));
    quark = gwy_app_get_data_key_for_id(args->rmodel.id);
    rf = GWY_DATA_FIELD(gwy_container_get_object(args->rmodel.data, quark));
    quark = gwy_app_get_data_key_for_id(args->tsignal.id);
    sf = GWY_DATA_FIELD(gwy_container_get_object(args->tsignal.data, quark));


    if (gwy_data_field_check_compatibility(tf, rf,
         GWY_DATA_COMPATIBILITY_RES
         | GWY_DATA_COMPATIBILITY_REAL
         | GWY_DATA_COMPATIBILITY_LATERAL)==0 && 
        gwy_data_field_check_compatibility(tf, sf,
         GWY_DATA_COMPATIBILITY_RES
         | GWY_DATA_COMPATIBILITY_REAL
         | GWY_DATA_COMPATIBILITY_LATERAL) == 0)
    {
        gtk_label_set_text(GTK_LABEL(controls->message), " ");
        gtk_widget_set_sensitive(controls->ok, TRUE);
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls->message), _("Data not compatible"));
        gtk_widget_set_sensitive(controls->ok, FALSE);
    }

}

static void
neural_do(NeuralArgs *args)
{
    GwyContainer *data;
    GQuark quark;
    GwyDataField *tmodel, *rmodel, *tsignal, *result;
    GwyDataLine *errors;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    gdouble mfactor, sfactor, mshift, sshift;
    gdouble *dtmodel, *drmodel, *dtsignal, *dresult, *input, *output, eo=0, eh=0;
    gint xres, yres, col, row, newid, n, height, width, irow, icol;
    GwyNN *nn;

    data = args->tmodel.data;
    quark = gwy_app_get_data_key_for_id(args->tmodel.id);
    tmodel = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    dtmodel = gwy_data_field_get_data(tmodel);

    gwy_app_wait_start(gwy_app_find_window_for_channel(data, args->tmodel.id), _("Starting..."));

    data = args->tsignal.data;
    quark = gwy_app_get_data_key_for_id(args->tsignal.id);
    tsignal = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    dtsignal = gwy_data_field_get_data(tsignal);

    data = args->rmodel.data;
    quark = gwy_app_get_data_key_for_id(args->rmodel.id);
    rmodel = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    drmodel = gwy_data_field_get_data(rmodel);

    xres = gwy_data_field_get_xres(tmodel);
    yres = gwy_data_field_get_yres(tmodel);
    width = args->width;
    height = args->height;

    nn = gwy_nn_alloc(height*width, args->hidden, 1); 
    input = g_new(gdouble, height*width);
    output = g_new(gdouble, 1); //preserve for generality
    errors = gwy_data_line_new(args->trainsteps, args->trainsteps, TRUE);
    mshift = gwy_data_field_get_min(tmodel);
    sshift = gwy_data_field_get_min(tsignal);
    mfactor = 1.0/(gwy_data_field_get_max(tmodel)-mshift);
    sfactor = 1.0/(gwy_data_field_get_max(tsignal)-sshift);

    result = gwy_data_field_new_alike(tsignal, FALSE);
    gwy_data_field_fill(result, gwy_data_field_get_avg(tsignal));
    dresult = gwy_data_field_get_data(result);

    /*perform training*/
    gwy_app_wait_set_message("Training...");
    for (n=0; n<args->trainsteps; n++)
    {
        for (row=(height/2); row<(yres-height/2); row++)
        {
            for (col=(width/2); col<(xres-width/2); col++)
            {
                for (irow = 0; irow<height; irow++)
                {
                    for (icol = 0; icol<width; icol++)
                    {
                        input[irow*width + icol] = mfactor*(dtmodel[(row+irow-width/2)*xres + (col+icol-height/2)] - mshift); 
                    }
                }        
                output[0] = sfactor*(dtsignal[row*xres + col] - sshift);
                gwy_nn_train_step(nn, 0.3, 0.3,
                                    &eo, &eh, input, output); 

            }
        }
        if (!gwy_app_wait_set_fraction((gdouble)n/(gdouble)args->trainsteps))
        {
            gwy_nn_free(nn);
            g_free(input);
            g_free(output);
            g_object_unref(result);
            gwy_object_unref(errors);
            return;
        }
        gwy_data_line_set_val(errors, n, eo+eh);
    }

    gwy_app_wait_set_message("Evaluating...");
    gwy_app_wait_set_fraction(0.0);
    for (row=(height/2); row<(yres-height/2); row++)
    {
        for (col=(width/2); col<(xres-width/2); col++)
        {
            for (irow = 0; irow<height; irow++)
            {
                for (icol = 0; icol<width; icol++)
                {
                    input[irow*width + icol] = mfactor*(drmodel[(row+irow-width/2)*xres + (col+icol-height/2)] - mshift); 
                }
            }        

            gwy_nn_feed_forward(nn, input, output);

            for (irow = 0; irow<height; irow++)
            {
                for (icol = 0; icol<width; icol++)
                {
                    dresult[row*xres + col] = output[0]/sfactor + sshift;
                }
            }        
        }
        if (!gwy_app_wait_set_fraction((gdouble)row/(gdouble)yres))
        {
            gwy_nn_free(nn);
            g_free(input);
            g_free(output);
            g_object_unref(result);
            gwy_object_unref(errors);
            return;
        }
     }

    gwy_app_wait_finish();

    newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
    g_object_unref(result);
    gwy_app_set_data_field_title(data, newid, _("Evaluated signal"));
    gwy_app_sync_data_items(data, data, args->tmodel.id, newid, FALSE,
                                               GWY_DATA_ITEM_GRADIENT, 0);

    gmodel = gwy_graph_model_new();
    g_object_set(gmodel,
                 "title", _("Training error"),
                 "axis-label-left", _("error"),
                 "axis-label-bottom", "n",
                 NULL);

    gcmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_data_from_dataline(gcmodel, errors, -1, -1);
    g_object_set(gcmodel, "description", _("NN training error"), NULL);
    gwy_graph_model_add_curve(gmodel, gcmodel);
    gwy_object_unref(gcmodel);

    gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
    gwy_object_unref(gmodel);
    gwy_object_unref(errors);


}

static GwyNN*       
gwy_nn_alloc(gint input, gint hidden, gint output)
{
    gint i, j;
    GwyNN *nn = (GwyNN*)g_malloc(sizeof(GwyNN));
    GRand *rng;

    nn->ninput = input+1;
    nn->nhidden = hidden+1;
    nn->noutput = output+1;

    nn->input = g_new0(gdouble, nn->ninput);
    nn->hidden = g_new0(gdouble, nn->nhidden);
    nn->dhidden = g_new0(gdouble, nn->nhidden);
    nn->output = g_new0(gdouble, nn->noutput);
    nn->doutput = g_new0(gdouble, nn->noutput);
    nn->target = g_new0(gdouble, nn->noutput);

    nn->winput = g_new(gdouble *, nn->ninput);
    nn->wpinput = g_new(gdouble *, nn->ninput);
    for (i=0; i<nn->ninput; i++)
    {
        nn->winput[i] = (gdouble*)g_new(gdouble, nn->nhidden);
        nn->wpinput[i] = (gdouble*)g_new0(gdouble, nn->nhidden);
    }

    nn->whidden = g_new(gdouble *, nn->nhidden);
    nn->wphidden = g_new(gdouble *, nn->nhidden);
    for (i=0; i<nn->nhidden; i++)
    {
        nn->whidden[i] = (gdouble*)g_new(gdouble, nn->noutput);
        nn->wphidden[i] = (gdouble*)g_new0(gdouble, nn->noutput);
    }

    rng = g_rand_new();
    g_rand_set_seed(rng, 1);

    for (i=0; i<nn->ninput; i++)
    {
        for (j=0; j<nn->nhidden; j++) nn->winput[i][j] = (2.0*g_rand_double(rng) - 1)*0.1;
    }
    for (i=0; i<nn->nhidden; i++)
    {
        for (j=0; j<nn->noutput; j++) nn->whidden[i][j] = (2.0*g_rand_double(rng) - 1)*0.1;
    }

    g_rand_free(rng);
    return nn;
}

static inline gdouble 
sigma(gdouble x)
{
    return (1.0/(1.0+exp(-x)));
}

static void 
gwy_nn_feed_forward(GwyNN* nn, gdouble *input, gdouble *output)
{
    gint i;
    for (i=0; i<(nn->ninput-1); i++) nn->input[i+1] = input[i];

    layer_forward(nn->input, nn->hidden, nn->winput, nn->ninput, nn->nhidden);
    layer_forward(nn->hidden, nn->output, nn->whidden, nn->nhidden, nn->noutput);

    for (i=0; i<(nn->noutput-1); i++) output[i] = nn->output[i+1];
}

static void
layer_forward(gdouble *input, gdouble *output, gdouble **weight, gint nin, gint nout)
{
    gint j, k;
    gdouble sum;
    
    input[0] = 1.0;
    for (j=1; j<nout; j++)
    {
        sum=0.0;
        for (k=0; k<nin; k++)
        {
            sum+=weight[k][j]*input[k];
        }
        output[j]=sigma(sum);
    }
}

static void
adjust_weights(gdouble *delta, gint ndelta, gdouble *data, gint ndata,
               gdouble **w, gdouble **oldw, gdouble eta, gdouble momentum)
{
    gdouble new_dw;
    gint j, k;

    data[0]=1.0;
    for (j=1; j<ndelta; j++)
    {
        for (k=0; k<ndata; k++)
        {
            new_dw=((eta*delta[j]*data[k])+(momentum*oldw[k][j]));
            w[k][j]+=new_dw;
            oldw[k][j]=new_dw;
        }
    }
}

static
gdouble output_error(gdouble *output, gint noutput, gdouble *target, gdouble *doutput)
{
    gint j;
    gdouble out, tar, errsum;

    errsum = 0.0;
    for (j=1; j<noutput; j++)
    {
        out=output[j];
        tar=target[j];
        doutput[j]=out*(1.0-out)*(tar-out);
        errsum+=fabs(doutput[j]);
    }
    return errsum;
}

static gdouble
hidden_error(gdouble *hidden, gint nhidden, gdouble *dhidden, gdouble *doutput, gint noutput, gdouble **whidden)
{
    gint j, k;
    gdouble h, sum, errsum;

    errsum=0.0;
    for (j=1; j<nhidden; j++)
    {
        h=hidden[j];
        sum=0.0;
        for (k=1; k<noutput; k++)
            sum += doutput[k]*whidden[j][k];

        dhidden[j]=h*(1.0-h)*sum;
        errsum+=fabs(dhidden[j]);
    }
    return errsum;
}

static void
gwy_nn_train_step(GwyNN *nn, gdouble eta, gdouble momentum, gdouble *err_o, gdouble *err_h, gdouble *input, gdouble *target)
{
    gint i;

    for (i=0; i<(nn->ninput-1); i++) nn->input[i+1]=input[i];
    for (i=0; i<(nn->noutput-1); i++) nn->target[i+1]=target[i];

    layer_forward(nn->input, nn->hidden, nn->winput, nn->ninput, nn->nhidden);
    layer_forward(nn->hidden, nn->output, nn->whidden, nn->nhidden, nn->noutput);

    *err_o=output_error(nn->output, nn->noutput, nn->target, nn->doutput);
    *err_h=hidden_error(nn->hidden, nn->nhidden, nn->dhidden, nn->doutput, nn->noutput, nn->whidden);

    adjust_weights(nn->doutput, nn->noutput, nn->hidden, nn->nhidden, nn->whidden, nn->wphidden, eta, momentum);
    adjust_weights(nn->dhidden, nn->nhidden, nn->input, nn->ninput, nn->winput, nn->wpinput, eta, momentum);

}

static void
gwy_nn_free(GwyNN *nn)
{
    gint i;
    
    g_free(nn->input);
    g_free(nn->hidden);
    g_free(nn->dhidden);
    g_free(nn->output);
    g_free(nn->doutput);
    g_free(nn->target);

    for (i=0; i<nn->ninput; i++)
    {
        g_free(nn->winput[i]);
        g_free(nn->wpinput[i]);
    }
    g_free(nn->winput);
    g_free(nn->wpinput);

    for (i=0; i<nn->nhidden; i++)
    {
        g_free(nn->whidden[i]);
        g_free(nn->wphidden[i]);
    }
    g_free(nn->whidden);
    g_free(nn->wphidden);

}


static void
neural_dialog_update(NeuralControls *controls,
                                    NeuralArgs *args)
{

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->width),
                             args->width);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height),
                             args->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->trainsteps),
                             args->trainsteps);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->hidden),
                             args->hidden);

}

static void
neural_values_update(NeuralControls *controls,
                                    NeuralArgs *args)
{
    args->width
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->width));
    args->height
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->height));
    args->trainsteps
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->trainsteps));
    args->hidden
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->hidden));

}


static const gchar width_key[]    = "/module/neural/width";
static const gchar height_key[]    = "/module/neural/height";
static const gchar trainsteps_key[]    = "/module/neural/trainsteps";
static const gchar hidden_key[]    = "/module/neural/hidden";

static void
neural_sanitize_args(NeuralArgs *args)
{
    args->width = CLAMP(args->width, 1, 1000);
    args->height = CLAMP(args->height, 1, 1000);
    args->trainsteps = CLAMP(args->trainsteps, 1, 100000);
    args->hidden = CLAMP(args->hidden, 1, 1000);
}
static void
neural_load_args(GwyContainer *container,
                 NeuralArgs *args)
{
    *args = neural_defaults;

    gwy_container_gis_int32_by_name(container, width_key, &args->width);
    gwy_container_gis_int32_by_name(container, height_key, &args->height);
    gwy_container_gis_int32_by_name(container, trainsteps_key, &args->trainsteps);
    gwy_container_gis_int32_by_name(container, hidden_key, &args->hidden);

    neural_sanitize_args(args);
}

static void
neural_save_args(GwyContainer *container,
                 NeuralArgs *args)
{
    gwy_container_set_int32_by_name(container, width_key, args->width);
    gwy_container_set_int32_by_name(container, height_key, args->height);
    gwy_container_set_int32_by_name(container, trainsteps_key, args->trainsteps);
    gwy_container_set_int32_by_name(container, hidden_key, args->hidden);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

