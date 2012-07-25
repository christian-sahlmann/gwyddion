/*
 *  @(#) $Id$
 *  Copyright (C) 2012 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyexpr.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/datafield.h>
#include <libprocess/filters.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>

#include "neuraldata.h"

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

    gdouble *whidden;
    gdouble *winput;
    gdouble *wphidden;
    gdouble *wpinput;
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


static gboolean module_register     (void);
static void     neural              (GwyContainer *data,
                                     GwyRunType run);
static gboolean neural_dialog       (NeuralArgs *args);
static void     neural_data_cb      (GwyDataChooser *chooser,
                                     NeuralControls *controls);
static void     neural_do           (NeuralArgs *args);
static void     neural_sanitize_args(NeuralArgs *args);
static void     neural_load_args    (GwyContainer *container,
                                     NeuralArgs *args);
static void     neural_save_args    (GwyContainer *container,
                                     NeuralArgs *args);
static void     neural_values_update(NeuralControls *controls,
                                     NeuralArgs *args);
static void     neural_dialog_update(NeuralControls *controls,
                                     NeuralArgs *args);
static void     shuffle             (guint *a,
                                     guint n,
                                     GRand *rng);
static GwyNN*   gwy_nn_alloc        (gint input,
                                     gint hidden,
                                     gint output);
static void     gwy_nn_randomize    (GwyNN *nn,
                                     GRand *rng);
static void     gwy_nn_feed_forward (GwyNN* nn);
static void     layer_forward       (const gdouble *input,
                                     gdouble *output,
                                     const gdouble *weight,
                                     guint nin,
                                     guint nout);
static void     gwy_nn_train_step   (GwyNN *nn,
                                     gdouble eta,
                                     gdouble momentum,
                                     gdouble *err_o,
                                     gdouble *err_h);
static void     gwy_nn_free         (GwyNN *nn);


enum {
    PREVIEW_SIZE = 400,
};

typedef struct {
    GwyDataObjectId tmodel;
    GwyDataObjectId tsignal;
} NeuralTrainArgs;

typedef struct {
    NeuralTrainArgs *args;
    GwyContainer *mydata;
    GtkWidget *dialog;
    GtkWidget *ok;
    /* Training */
    GtkWidget *tmodel;
    GtkWidget *tsignal;
    GtkObject *trainsteps;
    /* Network props */
    GtkObject *hidden;
    GtkObject *width;
    GtkObject *height;
    GtkWidget *message;
    /* Network list */
} NeuralTrainControls;

static void     neural_train              (GwyContainer *data,
                                           GwyRunType run);
static gboolean neural_train_dialog       (NeuralTrainArgs *args);
static void     neural_train_sanitize_args(NeuralTrainArgs *args);
static void     neural_train_load_args    (GwyContainer *container,
                                           NeuralTrainArgs *args);
static void     neural_train_save_args    (GwyContainer *container,
                                           NeuralTrainArgs *args);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Neural network SPM data processing"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("neural",
                              (GwyProcessFunc)&neural,
                              N_("/M_ultidata/_Neural Network..."),
                              NULL,
                              NEURAL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Neural network AFM data processing"));

    gwy_process_func_register("neural_train",
                              (GwyProcessFunc)&neural_train,
                              N_("/M_ultidata/Neural Network _Training..."),
                              NULL,
                              NEURAL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Train a neural network for image "
                                 "processing"));

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

    settings = gwy_app_settings_get();
    neural_load_args(settings, &args);
    args.tmodel.data = data;
    args.tmodel.id = id;
    args.rmodel.data = data;
    args.rmodel.id = id;
    args.tsignal.data = data;
    args.tsignal.id = id;

    if (neural_dialog(&args))
        neural_do(&args);
    neural_save_args(settings, &args);
}

static gboolean
neural_dialog(NeuralArgs *args)
{
    NeuralControls controls;
    GtkWidget *dialog, *table, *label, *spin;
    guint row, response;
    enum { RESPONSE_RESET = 1 };

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Neural Network"), NULL, 0,
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

    label = gwy_label_new_header(_("Operands"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.tmodel = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(controls.tmodel), "dialog", dialog);
    gwy_table_attach_hscale(table, row, _("Training _model:"), NULL,
                            GTK_OBJECT(controls.tmodel), GWY_HSCALE_WIDGET);
    row++;

    controls.tsignal = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(controls.tsignal), "dialog", dialog);
    gwy_table_attach_hscale(table, row, _("Training _signal:"), NULL,
                            GTK_OBJECT(controls.tsignal), GWY_HSCALE_WIDGET);
    row++;

    controls.rmodel = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(controls.rmodel), "dialog", dialog);
    gwy_table_attach_hscale(table, row, _("Res_ult model:"), NULL,
                            GTK_OBJECT(controls.rmodel), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gwy_label_new_header(_("Parameters"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.width = gtk_adjustment_new(args->width, 1, 100, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Window _width:"), "px",
                                       controls.width);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.height = gtk_adjustment_new(args->height, 1, 100, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Window h_eight:"), "px",
                                       controls.height);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.trainsteps = gtk_adjustment_new(args->trainsteps, 1, 100000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Training steps:"), "",
                                       controls.trainsteps);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.hidden = gtk_adjustment_new(args->hidden, 1, 20, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Hidden nodes:"), "",
                                       controls.hidden);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.message = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.message), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.message, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

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
    args->tmodel.data
        = gwy_data_chooser_get_active(GWY_DATA_CHOOSER(controls->tmodel),
                                      &args->tmodel.id);
    args->rmodel.data
        = gwy_data_chooser_get_active(GWY_DATA_CHOOSER(controls->rmodel),
                                      &args->rmodel.id);
    args->tsignal.data
        = gwy_data_chooser_get_active(GWY_DATA_CHOOSER(controls->tsignal),
                                      &args->tsignal.id);

    quark = gwy_app_get_data_key_for_id(args->tmodel.id);
    tf = GWY_DATA_FIELD(gwy_container_get_object(args->tmodel.data, quark));
    quark = gwy_app_get_data_key_for_id(args->rmodel.id);
    rf = GWY_DATA_FIELD(gwy_container_get_object(args->rmodel.data, quark));
    quark = gwy_app_get_data_key_for_id(args->tsignal.id);
    sf = GWY_DATA_FIELD(gwy_container_get_object(args->tsignal.data, quark));

    if (!gwy_data_field_check_compatibility
                                (tf, sf,
                                 GWY_DATA_COMPATIBILITY_RES
                                 | GWY_DATA_COMPATIBILITY_REAL
                                 | GWY_DATA_COMPATIBILITY_LATERAL)
        && !gwy_data_field_check_compatibility
                                (tf, rf,
                                 GWY_DATA_COMPATIBILITY_MEASURE
                                 | GWY_DATA_COMPATIBILITY_LATERAL
                                 | GWY_DATA_COMPATIBILITY_VALUE)) {
        gtk_label_set_text(GTK_LABEL(controls->message), "");
        gtk_widget_set_sensitive(controls->ok, TRUE);
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls->message),
                           _("Data not compatible"));
        gtk_widget_set_sensitive(controls->ok, FALSE);
    }
}

static void
neural_do(NeuralArgs *args)
{
    GwyContainer *data;
    GQuark quark;
    GwyDataField *tmodel, *rmodel, *tsignal, *result, *scaled;
    GwyDataLine *errors;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    gdouble sfactor, sshift, eo = 0.0, eh = 0.0;
    gdouble *dresult;
    const gdouble *dtmodel, *drmodel, *dtsignal;
    gint xres, yres, col, row, newid, n, height, width, irow, k;
    guint *indices;
    GRand *rng;
    GwyNN *nn;

    data = args->tmodel.data;
    quark = gwy_app_get_data_key_for_id(args->tmodel.id);
    tmodel = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    rng = g_rand_new();

    gwy_app_wait_start(gwy_app_find_window_for_channel(data, args->tmodel.id),
                       _("Starting..."));

    data = args->tsignal.data;
    quark = gwy_app_get_data_key_for_id(args->tsignal.id);
    tsignal = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    dtsignal = gwy_data_field_get_data_const(tsignal);

    data = args->rmodel.data;
    quark = gwy_app_get_data_key_for_id(args->rmodel.id);
    rmodel = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    width = args->width;
    height = args->height;

    nn = gwy_nn_alloc(height*width, args->hidden, 1);
    gwy_nn_randomize(nn, rng);
    errors = gwy_data_line_new(args->trainsteps, args->trainsteps, TRUE);
    sshift = gwy_data_field_get_min(tsignal);
    sfactor = 1.0/(gwy_data_field_get_max(tsignal)-sshift);

    /*perform training*/
    gwy_app_wait_set_message(_("Training..."));
    scaled = gwy_data_field_duplicate(tmodel);
    gwy_data_field_normalize(scaled);
    xres = gwy_data_field_get_xres(scaled);
    yres = gwy_data_field_get_yres(scaled);
    dtmodel = gwy_data_field_get_data_const(scaled);

    indices = g_new(guint, (xres - width)*(yres - height));
    k = 0;
    for (row = height/2; row < yres + height/2 - height; row++) {
        for (col = width/2; col < xres + width/2 - width; col++)
            indices[k++] = (row - height/2)*xres + col - width/2;
    }
    g_assert(k == (xres - width)*(yres - height));

    for (n = 0; n < args->trainsteps; n++) {
        /* FIXME: Randomisation leads to weird spiky NN error curves. */
        /* shuffle(indices, (xres - width)*(yres - height), rng); */
        for (k = 0; k < (xres - width)*(yres - height); k++) {
            for (irow = 0; irow < height; irow++) {
                memcpy(nn->input + irow*width,
                       dtmodel + indices[k] + irow*xres,
                       width*sizeof(gdouble));
            }
            nn->target[0] = sfactor*(dtsignal[indices[k]
                                              + height/2*xres + width/2]
                                              - sshift);
            gwy_nn_train_step(nn, 0.3, 0.3, &eo, &eh);
        }
        if (!gwy_app_wait_set_fraction((gdouble)n/(gdouble)args->trainsteps)) {
            gwy_nn_free(nn);
            g_rand_free(rng);
            g_object_unref(scaled);
            g_free(indices);
            gwy_object_unref(errors);
            return;
        }
        gwy_data_line_set_val(errors, n, eo + eh);
    }
    g_free(indices);
    g_object_unref(scaled);

    gwy_app_wait_set_message(_("Evaluating..."));
    gwy_app_wait_set_fraction(0.0);
    scaled = gwy_data_field_duplicate(rmodel);
    gwy_data_field_normalize(scaled);
    xres = gwy_data_field_get_xres(scaled);
    yres = gwy_data_field_get_yres(scaled);
    drmodel = gwy_data_field_get_data_const(scaled);

    result = gwy_data_field_new_alike(rmodel, FALSE);
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_z(tsignal)),
                           G_OBJECT(gwy_data_field_get_si_unit_z(result)));
    gwy_data_field_fill(result, gwy_data_field_get_avg(tsignal));
    dresult = gwy_data_field_get_data(result);

    for (row = height/2; row < yres + height/2 - height; row++) {
        for (col = width/2; col < xres + width/2 - width; col++) {
            for (irow = 0; irow < height; irow++) {
                memcpy(nn->input + irow*width,
                       drmodel + ((row + irow - height/2)*xres
                                  + col - width/2),
                       width*sizeof(gdouble));
            }
            gwy_nn_feed_forward(nn);
            dresult[row*xres + col] = nn->output[0]/sfactor + sshift;
        }
        if (row % 32 == 31
            && !gwy_app_wait_set_fraction((gdouble)row/(gdouble)yres)) {
            gwy_nn_free(nn);
            g_rand_free(rng);
            g_object_unref(result);
            g_object_unref(scaled);
            gwy_object_unref(errors);
            return;
        }
    }
    g_object_unref(scaled);
    g_rand_free(rng);

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

static void
shuffle(guint *a, guint n, GRand *rng)
{
    guint i;

    for (i = 0; i < n; i++) {
        guint j = g_rand_int_range(rng, i, n);
        GWY_SWAP(guint, a[i], a[j]);
    }
}

static inline gdouble
sigma(gdouble x)
{
    return (1.0/(1.0 + exp(-x)));
}

static GwyNN*
gwy_nn_alloc(gint ninput, gint nhidden, gint noutput)
{
    GwyNN *nn = (GwyNN*)g_malloc(sizeof(GwyNN));

    nn->ninput = ninput;
    nn->nhidden = nhidden;
    nn->noutput = noutput;

    nn->input = g_new0(gdouble, nn->ninput);
    nn->hidden = g_new0(gdouble, nn->nhidden);
    nn->dhidden = g_new0(gdouble, nn->nhidden);
    nn->output = g_new0(gdouble, nn->noutput);
    nn->doutput = g_new0(gdouble, nn->noutput);
    nn->target = g_new0(gdouble, nn->noutput);

    /* Add weights for the constant input signal neurons. */
    nn->winput = g_new0(gdouble, (nn->ninput + 1)*nn->nhidden);
    nn->wpinput = g_new0(gdouble, (nn->ninput + 1)*nn->nhidden);

    nn->whidden = g_new0(gdouble, (nn->nhidden + 1)*nn->noutput);
    nn->wphidden = g_new0(gdouble, (nn->nhidden + 1)*nn->noutput);

    return nn;
}

static void
gwy_nn_randomize(GwyNN *nn, GRand *rng)
{
    gint i;
    gdouble *p;

    for (i = (nn->ninput + 1)*nn->nhidden, p = nn->winput; i; i--, p++)
        *p = (2.0*g_rand_double(rng) - 1)*0.1;
    for (i = (nn->nhidden + 1)*nn->noutput, p = nn->whidden; i; i--, p++)
        *p = (2.0*g_rand_double(rng) - 1)*0.1;
}

static void
gwy_nn_feed_forward(GwyNN* nn)
{
    layer_forward(nn->input, nn->hidden, nn->winput,
                  nn->ninput, nn->nhidden);
    layer_forward(nn->hidden, nn->output, nn->whidden,
                  nn->nhidden, nn->noutput);
}

static void
layer_forward(const gdouble *input, gdouble *output, const gdouble *weight,
              guint nin, guint nout)
{
    guint j, k;

    for (j = nout; j; j--, output++) {
        const gdouble *p = input;
        /* Initialise with the constant signal neuron. */
        gdouble sum = *weight;
        weight++;
        for (k = nin; k; k--, p++, weight++)
            sum += (*weight)*(*p);
        *output = sigma(sum);
    }
}

static void
adjust_weights(gdouble *delta, guint ndelta, const gdouble *data, guint ndata,
               gdouble *weight, gdouble *oldw, gdouble eta, gdouble momentum)
{
    guint j, k;

    for (j = ndelta; j; j--, delta++) {
        gdouble edeltaj = eta*(*delta);
        const gdouble *p = data;
        /* The constant signal neuron first. */
        gdouble new_dw = edeltaj + momentum*(*oldw);
        *weight += new_dw;
        *oldw = new_dw;
        weight++;
        oldw++;

        for (k = ndata; k; k--, p++, oldw++, weight++) {
            new_dw = edeltaj*(*p) + momentum*(*oldw);
            *weight += new_dw;
            *oldw = new_dw;
        }
    }
}

static gdouble
output_error(const gdouble *output, guint noutput, const gdouble *target,
             gdouble *doutput)
{
    guint j;
    gdouble errsum = 0.0;

    for (j = 0; j < noutput; j++) {
        gdouble out = output[j];
        gdouble tgt = target[j];

        doutput[j] = out*(1.0 - out)*(tgt - out);
        errsum += fabs(doutput[j]);
    }

    return errsum;
}

static gdouble
hidden_error(const gdouble *hidden, guint nhidden, gdouble *dhidden,
             const gdouble *doutput, guint noutput, const gdouble *whidden)
{
    guint j, k;
    gdouble errsum = 0.0;

    for (j = 0; j < nhidden; j++) {
        const gdouble *p = doutput;
        const gdouble *q = whidden + (j + 1);
        gdouble h = hidden[j];
        gdouble sum = 0.0;

        for (k = noutput; k; k--, p++, q += nhidden+1)
            sum += (*p)*(*q);

        dhidden[j] = h*(1.0 - h)*sum;
        errsum += fabs(dhidden[j]);
    }
    return errsum;
}

static void
gwy_nn_train_step(GwyNN *nn, gdouble eta, gdouble momentum,
                  gdouble *err_o, gdouble *err_h)
{
    layer_forward(nn->input, nn->hidden, nn->winput, nn->ninput, nn->nhidden);
    layer_forward(nn->hidden, nn->output, nn->whidden, nn->nhidden, nn->noutput);

    *err_o = output_error(nn->output, nn->noutput, nn->target, nn->doutput);
    *err_h = hidden_error(nn->hidden, nn->nhidden, nn->dhidden,
                          nn->doutput, nn->noutput, nn->whidden);

    adjust_weights(nn->doutput, nn->noutput, nn->hidden, nn->nhidden,
                   nn->whidden, nn->wphidden, eta, momentum);
    adjust_weights(nn->dhidden, nn->nhidden, nn->input, nn->ninput,
                   nn->winput, nn->wpinput, eta, momentum);
}

static void
gwy_nn_free(GwyNN *nn)
{
    g_free(nn->input);
    g_free(nn->hidden);
    g_free(nn->dhidden);
    g_free(nn->output);
    g_free(nn->doutput);
    g_free(nn->target);
    g_free(nn->winput);
    g_free(nn->wpinput);
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
    args->width = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->width));
    args->height = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->height));
    args->trainsteps
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->trainsteps));
    args->hidden = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->hidden));
}

static const gchar width_key[]      = "/module/neural/width";
static const gchar height_key[]     = "/module/neural/height";
static const gchar trainsteps_key[] = "/module/neural/trainsteps";
static const gchar hidden_key[]     = "/module/neural/hidden";

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

/**************************************************************************
 *
 * New module
 *
 **************************************************************************/


void
neural_train(GwyContainer *data, GwyRunType run)
{
    NeuralTrainArgs args;
    GwyContainer *settings;
    gint id;

    g_return_if_fail(run & NEURAL_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id, 0);

    settings = gwy_app_settings_get();
    neural_train_load_args(settings, &args);
    args.tmodel.data = data;
    args.tmodel.id = id;
    args.tsignal.data = data;
    args.tsignal.id = id;

    neural_train_dialog(&args);
    neural_train_save_args(settings, &args);
}

static void
setup_container(GwyContainer *mydata,
                NeuralTrainArgs *args)
{
    GwyDataField *tmodel, *tsignal, *result, *diff;

    tmodel = gwy_container_get_object(args->tmodel.data,
                                      gwy_app_get_data_key_for_id(args->tmodel.id));
    tsignal = gwy_container_get_object(args->tsignal.data,
                                       gwy_app_get_data_key_for_id(args->tsignal.id));
    result = gwy_data_field_new_alike(tsignal, TRUE);
    diff = gwy_data_field_new_alike(tsignal, TRUE);

    gwy_container_set_object_by_name(mydata, "/0/data", tmodel);
    gwy_app_sync_data_items(args->tmodel.data, mydata, args->tmodel.id, 0,
                            FALSE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            0);

    gwy_container_set_object_by_name(mydata, "/1/data", tsignal);
    gwy_app_sync_data_items(args->tsignal.data, mydata, args->tsignal.id, 1,
                            FALSE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            0);

    gwy_container_set_object_by_name(mydata, "/2/data", result);
    g_object_unref(result);
    gwy_app_sync_data_items(args->tsignal.data, mydata, args->tsignal.id, 2,
                            FALSE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            0);

    gwy_container_set_object_by_name(mydata, "/3/data", diff);
    g_object_unref(diff);
    gwy_app_sync_data_items(args->tsignal.data, mydata, args->tsignal.id, 3,
                            FALSE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            GWY_DATA_ITEM_GRADIENT,
                            0);
}

static gboolean
neural_train_dialog(NeuralTrainArgs *args)
{
    NeuralTrainControls controls;
    GwyContainer *mydata;
    GwyDataField *tmodel, *tsignal, *result, *diff;
    GtkWidget *dialog, *table, *label, *spin, *hbox;
    guint row, response;
    enum { RESPONSE_RESET = 1 };

    controls.args = args;

    controls.mydata = gwy_container_new();
    setup_container(mydata, args);

    dialog = gtk_dialog_new_with_buttons(_("Neural Network Training"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
    controls.ok = gtk_dialog_add_button(GTK_DIALOG(dialog),
                                        GTK_STOCK_OK, GTK_RESPONSE_OK);

    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);


    /*
    table = gtk_table_new(10, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gwy_label_new_header(_("Operands"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.tmodel = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(controls.tmodel), "dialog", dialog);
    gwy_table_attach_hscale(table, row, _("Training _model:"), NULL,
                            GTK_OBJECT(controls.tmodel), GWY_HSCALE_WIDGET);
    row++;

    controls.tsignal = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(controls.tsignal), "dialog", dialog);
    gwy_table_attach_hscale(table, row, _("Training _signal:"), NULL,
                            GTK_OBJECT(controls.tsignal), GWY_HSCALE_WIDGET);
    row++;

    controls.rmodel = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(controls.rmodel), "dialog", dialog);
    gwy_table_attach_hscale(table, row, _("Res_ult model:"), NULL,
                            GTK_OBJECT(controls.rmodel), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gwy_label_new_header(_("Parameters"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.width = gtk_adjustment_new(args->width, 1, 100, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Window _width:"), "px",
                                       controls.width);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.height = gtk_adjustment_new(args->height, 1, 100, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Window h_eight:"), "px",
                                       controls.height);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.trainsteps = gtk_adjustment_new(args->trainsteps, 1, 100000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Training steps:"), "",
                                       controls.trainsteps);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.hidden = gtk_adjustment_new(args->hidden, 1, 20, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Hidden nodes:"), "",
                                       controls.hidden);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.message = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.message), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.message, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

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
    */

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
neural_train_sanitize_args(NeuralTrainArgs *args)
{
}

static void
neural_train_load_args(GwyContainer *container,
                       NeuralTrainArgs *args)
{
    gwy_clear(args, 1);
    neural_train_sanitize_args(args);
}

static void
neural_train_save_args(GwyContainer *container,
                       NeuralTrainArgs *args)
{
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
