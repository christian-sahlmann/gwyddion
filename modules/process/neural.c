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
#include <glib/gstdio.h>
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
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwyinventorystore.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

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
    GtkObject *inpowerxy;
    GtkObject *inpowerz;
    GtkWidget *outunits;
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
static void     neural_data      (GwyDataChooser *chooser,
                                     NeuralControls *controls);
static void     neural_do           (NeuralArgs *args);
static void     neural_sanitize_args(NeuralArgs *args);
static void     neural_load_args    (GwyContainer *settings,
                                     NeuralArgs *args);
static void     neural_save_args    (GwyContainer *settings,
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
static void     gwy_nn_train_step   (GwyNN *nn,
                                     gdouble eta,
                                     gdouble momentum,
                                     gdouble *err_o,
                                     gdouble *err_h);
static void     gwy_nn_free         (GwyNN *nn);


enum {
    PREVIEW_SIZE = 360,
};

typedef enum {
    PREVIEW_MODEL,
    PREVIEW_SIGNAL,
    PREVIEW_RESULT,
    PREVIEW_DIFFERENCE,
} PreviewType;

enum {
    NETWORK_NAME = 0,
    NETWORK_SIZE,
    NETWORK_HIDDEN,
    NETWORK_LAST
};

typedef struct {
    GwyNeuralNetwork *nn;
    GwyDataObjectId tmodel;
    GwyDataObjectId tsignal;
    guint trainsteps;
    PreviewType preview_type;
} NeuralTrainArgs;

typedef struct {
    NeuralTrainArgs *args;
    gboolean in_update;
    gboolean calculated;
    gboolean compatible;
    GwyContainer *mydata;
    GtkWidget *dialog;
    GtkWidget *ok;
    GtkWidget *view;
    GwyPixmapLayer *layer;
    GtkWidget *errgraph;
    GwyGraphModel *gmodel;
    /* Training */
    GtkWidget *tmodel;
    GtkWidget *tsignal;
    GtkObject *trainsteps;
    GtkWidget *train;
    GtkWidget *reinit;
    GSList *preview_group;
    GtkWidget *message;
    /* Network props */
    GtkObject *nhidden;
    GtkObject *width;
    GtkObject *height;
    GtkObject *inpowerxy;
    GtkObject *inpowerz;
    GtkWidget *outunits;
    /* Network list */
    GwyInventoryStore *store;
    GtkWidget *networklist;
    GtkWidget *load;
    GtkWidget *save;
    GtkWidget *rename;
    GtkWidget *delete;
    GtkWidget *networkname;
} NeuralTrainControls;

static void     neural_train                (GwyContainer *data,
                                             GwyRunType run);
static gboolean neural_train_dialog         (NeuralTrainArgs *args);
static void     neural_train_update_controls(NeuralTrainControls *controls);
static gboolean network_is_visible          (GtkTreeModel *model,
                                             GtkTreeIter *iter,
                                             gpointer user_data);
static void     preview_type_changed        (GtkToggleButton *button,
                                             NeuralTrainControls *controls);
static void     train_data_changed          (NeuralTrainControls *controls,
                                             GwyDataChooser *chooser);
static void     train_steps_changed         (NeuralTrainControls *controls,
                                             GtkAdjustment *adj);
static void     width_changed               (NeuralTrainControls *controls,
                                             GtkAdjustment *adj);
static void     height_changed              (NeuralTrainControls *controls,
                                             GtkAdjustment *adj);
static void     nhidden_changed             (NeuralTrainControls *controls,
                                             GtkAdjustment *adj);
static void     inpowerxy_changed           (NeuralTrainControls *controls,
                                             GtkAdjustment *adj);
static void     inpowerz_changed            (NeuralTrainControls *controls,
                                             GtkAdjustment *adj);
static void     outunits_changed            (NeuralTrainControls *controls,
                                             GtkEntry *entry);
static void     reinit_network              (NeuralTrainControls *controls);
static void     train_network               (NeuralTrainControls *controls);
static void     set_layer_channel           (GwyPixmapLayer *layer,
                                             gint channel);
static void     network_load                (NeuralTrainControls *controls);
static void     network_store               (NeuralTrainControls *controls);
static void     network_delete              (NeuralTrainControls *controls);
static void     network_rename              (NeuralTrainControls *controls);
static void     network_selected            (NeuralTrainControls *controls);
static gboolean network_validate_name       (NeuralTrainControls *controls,
                                             const gchar *name,
                                             gboolean show_warning);
static void     neural_train_sanitize_args  (NeuralTrainArgs *args);
static void     neural_train_load_args      (GwyContainer *container,
                                             NeuralTrainArgs *args);
static void     neural_train_save_args      (GwyContainer *container,
                                             NeuralTrainArgs *args);
static gboolean gwy_neural_network_save     (GwyNeuralNetwork *nn);

static guint trainsteps_default = 1000;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Neural network SPM data processing"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    static gint types_initialized = 0;
    GwyResourceClass *klass;

    gwy_process_func_register("neural",
                              (GwyProcessFunc)&neural,
                              N_("/M_ultidata/_Neural Network..."),
                              NULL,
                              NEURAL_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Neural network AFM data processing"));

    if (!types_initialized) {
        types_initialized += gwy_neural_network_get_type();
        klass = g_type_class_ref(GWY_TYPE_NEURAL_NETWORK);
        gwy_resource_class_load(klass);
        gwy_resource_class_mkdir(klass);
        g_type_class_unref(klass);
    }

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
                     G_CALLBACK(neural_data), &controls);
    g_signal_connect(controls.tmodel, "changed",
                     G_CALLBACK(neural_data), &controls);
    g_signal_connect(controls.tsignal, "changed",
                     G_CALLBACK(neural_data), &controls);

    neural_data(GWY_DATA_CHOOSER(controls.rmodel), &controls);

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
neural_data(G_GNUC_UNUSED GwyDataChooser *chooser,
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

/* This assumes model and singal are compatible! */
static void
setup_container(GwyContainer *mydata,
                NeuralTrainArgs *args)
{
    GwyDataField *tmodel, *tsignal, *result, *diff;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(args->tmodel.id);
    tmodel = gwy_container_get_object(args->tmodel.data, quark);
    quark = gwy_app_get_data_key_for_id(args->tsignal.id);
    tsignal = gwy_container_get_object(args->tsignal.data, quark);
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

static void
network_cell_renderer(G_GNUC_UNUSED GtkTreeViewColumn *column,
                      GtkCellRenderer *cell,
                      GtkTreeModel *model,
                      GtkTreeIter *piter,
                      gpointer data)
{
    GwyNeuralNetwork *network;
    gulong id;
    gchar *s;

    id = GPOINTER_TO_UINT(data);
    g_assert(id < NETWORK_LAST);
    gtk_tree_model_get(model, piter, 0, &network, -1);
    switch (id) {
        case NETWORK_NAME:
        g_object_set(cell, "text", gwy_resource_get_name(GWY_RESOURCE(network)),
                     NULL);
        break;

        case NETWORK_SIZE:
        s = g_strdup_printf("%u×%u", network->data.width, network->data.height);
        g_object_set(cell, "text", s, NULL);
        g_free(s);
        break;

        case NETWORK_HIDDEN:
        s = g_strdup_printf("%u", network->data.nhidden);
        g_object_set(cell, "text", s, NULL);
        g_free(s);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static gboolean
neural_train_dialog(NeuralTrainArgs *args)
{
    static const GwyEnum columns[] = {
        { N_("Name"),   NETWORK_NAME,   },
        { N_("Size"),   NETWORK_SIZE,   },
        { N_("Hidden"), NETWORK_HIDDEN, },
    };

    NeuralTrainControls controls;
    NeuralNetworkData *nndata;
    GwyContainer *mydata;
    GtkWidget *dialog, *table, *label, *spin, *hbox, *bbox, *vbox, *notebook,
              *scroll, *button;
    GtkTreeModel *filtermodel;
    GtkTreeSelection *tselect;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GSList *group;
    guint row, response, i;

    controls.args = args;
    controls.in_update = TRUE;
    controls.calculated = FALSE;
    controls.compatible = TRUE;

    controls.mydata = mydata = gwy_container_new();
    setup_container(mydata, args);
    nndata = &args->nn->data;

    dialog = gtk_dialog_new_with_buttons(_("Neural Network Training"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
    controls.ok = gtk_dialog_add_button(GTK_DIALOG(dialog),
                                        GTK_STOCK_OK, GTK_RESPONSE_OK);

    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 4);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 4);

    controls.view = gwy_data_view_new(mydata);
    controls.layer = gwy_layer_basic_new();
    set_layer_channel(controls.layer, 0);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), controls.layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);
    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 0);

    controls.gmodel = gwy_graph_model_new();
    g_object_set(controls.gmodel,
                 "title", _("Training error"),
                 "axis-label-left", _("error"),
                 "axis-label-bottom", "n",
                 NULL);
    controls.errgraph = gwy_graph_new(controls.gmodel);
    gtk_widget_set_size_request(controls.errgraph, 0, 200);
    gtk_box_pack_start(GTK_BOX(vbox), controls.errgraph, TRUE, TRUE, 0);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(hbox), notebook, TRUE, TRUE, 4);

    /* Training */
    table = gtk_table_new(10, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new("Training"));
    row = 0;

    controls.tmodel = gwy_data_chooser_new_channels();
    gwy_table_attach_hscale(table, row, _("_Model:"), NULL,
                            GTK_OBJECT(controls.tmodel), GWY_HSCALE_WIDGET);
    gwy_data_chooser_set_active(GWY_DATA_CHOOSER(controls.tmodel),
                                args->tmodel.data, args->tmodel.id);
    g_object_set_data(G_OBJECT(controls.tmodel), "id", (gpointer)"model");
    g_signal_connect_swapped(controls.tmodel, "changed",
                             G_CALLBACK(train_data_changed), &controls);
    row++;

    controls.tsignal = gwy_data_chooser_new_channels();
    gwy_table_attach_hscale(table, row, _("_Signal:"), NULL,
                            GTK_OBJECT(controls.tsignal), GWY_HSCALE_WIDGET);
    gwy_data_chooser_set_active(GWY_DATA_CHOOSER(controls.tsignal),
                                args->tsignal.data, args->tsignal.id);
    g_object_set_data(G_OBJECT(controls.tsignal), "id", (gpointer)"signal");
    g_signal_connect_swapped(controls.tsignal, "changed",
                             G_CALLBACK(train_data_changed), &controls);
    row++;

    controls.trainsteps = gtk_adjustment_new(args->trainsteps,
                                             1, 10000, 1, 100, 0);
    gwy_table_attach_hscale(table, row, _("Training ste_ps:"), NULL,
                            GTK_OBJECT(controls.trainsteps), GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.trainsteps, "value-changed",
                             G_CALLBACK(train_steps_changed), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gtk_label_new(_("Preview:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.preview_group
        = gwy_radio_buttons_createl(G_CALLBACK(preview_type_changed),
                                    &controls,
                                    args->preview_type,
                                    _("Model"), PREVIEW_MODEL,
                                    _("Signal"), PREVIEW_SIGNAL,
                                    _("Result"), PREVIEW_RESULT,
                                    _("Difference"), PREVIEW_DIFFERENCE,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.preview_group,
                                            GTK_TABLE(table), 3, row);
    group = controls.preview_group;
    gtk_widget_set_sensitive(gwy_radio_buttons_find(group, PREVIEW_RESULT),
                             FALSE);
    gtk_widget_set_sensitive(gwy_radio_buttons_find(group, PREVIEW_DIFFERENCE),
                             FALSE);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_START);
    gtk_table_attach(GTK_TABLE(table), bbox,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.train = gtk_button_new_with_mnemonic(_("_Train"));
    gtk_container_add(GTK_CONTAINER(bbox), controls.train);
    g_signal_connect_swapped(controls.train, "clicked",
                             G_CALLBACK(train_network), &controls);

    controls.reinit = gtk_button_new_with_mnemonic(_("Re_initialize"));
    gtk_container_add(GTK_CONTAINER(bbox), controls.reinit);
    g_signal_connect_swapped(controls.reinit, "clicked",
                             G_CALLBACK(reinit_network), &controls);
    row++;

    controls.message = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.message), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.message,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    /* Parameters */
    table = gtk_table_new(8, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table,
                             gtk_label_new("Parameters"));
    row = 0;

    label = gwy_label_new_header(_("Network"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.width = gtk_adjustment_new(nndata->width, 1, 100, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Window _width:"), "px",
                                       controls.width);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    g_signal_connect_swapped(controls.width, "value-changed",
                             G_CALLBACK(width_changed), &controls);
    row++;

    controls.height = gtk_adjustment_new(nndata->height, 1, 100, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Window h_eight:"), "px",
                                       controls.height);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    g_signal_connect_swapped(controls.height, "value-changed",
                             G_CALLBACK(height_changed), &controls);
    row++;

    controls.nhidden = gtk_adjustment_new(nndata->nhidden, 1, 30, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Hidden nodes:"), NULL,
                                       controls.nhidden);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    g_signal_connect_swapped(controls.nhidden, "value-changed",
                             G_CALLBACK(nhidden_changed), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gwy_label_new_header(_("Result Units"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.inpowerxy = gtk_adjustment_new(nndata->inpowerxy,
                                            -12, 12, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Power of source _XY:"),
                                       NULL, controls.inpowerxy);
    g_signal_connect_swapped(controls.inpowerxy, "value-changed",
                             G_CALLBACK(inpowerxy_changed), &controls);
    row++;

    controls.inpowerz = gtk_adjustment_new(nndata->inpowerz,
                                            -12, 12, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Power of source _Z:"),
                                       NULL, controls.inpowerz);
    g_signal_connect_swapped(controls.inpowerz, "value-changed",
                             G_CALLBACK(inpowerz_changed), &controls);
    row++;

    controls.outunits = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(controls.outunits), nndata->outunits);
    gwy_table_attach_row(table, row, _("_Fixed units:"), NULL,
                         controls.outunits);
    g_signal_connect_swapped(controls.outunits, "changed",
                             G_CALLBACK(outunits_changed), &controls);
    row++;

    /* Networks */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox,
                             gtk_label_new(_("Networks")));

    controls.store = gwy_inventory_store_new(gwy_neural_networks());
    filtermodel = gtk_tree_model_filter_new(GTK_TREE_MODEL(controls.store),
                                            NULL);
    g_object_unref(controls.store);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filtermodel),
                                           &network_is_visible,
                                           NULL, NULL);
    controls.networklist = gtk_tree_view_new_with_model(filtermodel);
    g_object_unref(filtermodel);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(controls.networklist), TRUE);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_(columns[i].name),
                                                          renderer,
                                                          NULL);
        gtk_tree_view_column_set_cell_data_func
                                         (column, renderer,
                                          network_cell_renderer,
                                          GUINT_TO_POINTER(columns[i].value),
                                          NULL);  /* destroy notify */
        gtk_tree_view_append_column(GTK_TREE_VIEW(controls.networklist),
                                    column);
    }

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(scroll), controls.networklist);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_START);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    button = gtk_button_new_with_mnemonic(gwy_sgettext("verb|_Load"));
    controls.load = button;
    gtk_container_add(GTK_CONTAINER(bbox), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(network_load), &controls);

    button = gtk_button_new_with_mnemonic(gwy_sgettext("verb|_Store"));
    controls.save = button;
    gtk_container_add(GTK_CONTAINER(bbox), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(network_store), &controls);

    button = gtk_button_new_with_mnemonic(_("_Rename"));
    controls.rename = button;
    gtk_container_add(GTK_CONTAINER(bbox), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(network_rename), &controls);

    button = gtk_button_new_with_mnemonic(_("_Delete"));
    controls.delete = button;
    gtk_container_add(GTK_CONTAINER(bbox), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(network_delete), &controls);

    table = gtk_table_new(1, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 4);
    row = 0;

    controls.networkname = gtk_entry_new();
    gwy_table_attach_row(table, row, _("Preset _name:"), "",
                         controls.networkname);
    gtk_entry_set_max_length(GTK_ENTRY(controls.networkname), 40);
    row++;

    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls.networklist));
    gtk_tree_selection_set_mode(tselect, GTK_SELECTION_SINGLE);
    g_signal_connect_swapped(tselect, "changed",
                             G_CALLBACK(network_selected), &controls);
    neural_train_update_controls(&controls);
    controls.in_update = FALSE;

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.gmodel);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            g_printerr("Create graph if trained?\n");
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    g_object_unref(controls.gmodel);

    return TRUE;
}

static void
neural_train_update_controls(NeuralTrainControls *controls)
{
    NeuralNetworkData *nndata = &controls->args->nn->data;

    controls->in_update = TRUE;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->width),
                             nndata->width);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->height),
                             nndata->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->nhidden),
                             nndata->nhidden);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->inpowerxy),
                             nndata->inpowerxy);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->inpowerz),
                             nndata->inpowerz);
    gtk_entry_set_text(GTK_ENTRY(controls->outunits),
                       nndata->outunits);
    controls->in_update = FALSE;
}

static gboolean
network_is_visible(GtkTreeModel *model,
                   GtkTreeIter *iter,
                   G_GNUC_UNUSED gpointer user_data)
{
    GwyNeuralNetwork *network;
    gtk_tree_model_get(model, iter, 0, &network, -1);
    return !gwy_strequal(gwy_resource_get_name(GWY_RESOURCE(network)),
                         GWY_NEURAL_NETWORK_UNTITLED);
}

static void
preview_type_changed(G_GNUC_UNUSED GtkToggleButton *button,
                     NeuralTrainControls *controls)
{
    GSList *group;

    group = controls->preview_group;
    set_layer_channel(controls->layer, gwy_radio_buttons_get_current(group));
}

static void
train_data_changed(NeuralTrainControls *controls,
                   GwyDataChooser *chooser)
{
    NeuralTrainArgs *args = controls->args;
    GwyDataChooser *tmodel_chooser = GWY_DATA_CHOOSER(controls->tmodel),
                   *tsignal_chooser = GWY_DATA_CHOOSER(controls->tsignal);
    GwyDataField *model, *signal;
    const gchar *id;
    gboolean ok;
    GSList *group;
    GQuark quark;

    args->tmodel.data
        = gwy_data_chooser_get_active(tmodel_chooser, &args->tmodel.id);
    args->tsignal.data
        = gwy_data_chooser_get_active(tsignal_chooser, &args->tsignal.id);

    quark = gwy_app_get_data_key_for_id(args->tmodel.id);
    model = GWY_DATA_FIELD(gwy_container_get_object(args->tmodel.data, quark));
    quark = gwy_app_get_data_key_for_id(args->tsignal.id);
    signal = GWY_DATA_FIELD(gwy_container_get_object(args->tsignal.data, quark));

    ok = !gwy_data_field_check_compatibility(model, signal,
                                             GWY_DATA_COMPATIBILITY_RES
                                             | GWY_DATA_COMPATIBILITY_REAL
                                             | GWY_DATA_COMPATIBILITY_LATERAL);
    gtk_label_set_text(GTK_LABEL(controls->message),
                       ok ? "" : _("Model and signal are not compatible."));
    gtk_widget_set_sensitive(controls->train, ok);
    gtk_widget_set_sensitive(controls->ok, ok);

    controls->calculated = FALSE;
    controls->compatible = ok;
    setup_container(controls->mydata, controls->args);

    id = (const gchar*)g_object_get_data(G_OBJECT(chooser), "id");
    group = controls->preview_group;
    if (gwy_strequal(id, "model"))
        gwy_radio_buttons_set_current(group, PREVIEW_MODEL);
    else if (gwy_strequal(id, "signal"))
        gwy_radio_buttons_set_current(group, PREVIEW_SIGNAL);
    else {
        g_critical("Chooser lacks id");
    }

    gtk_widget_set_sensitive(gwy_radio_buttons_find(group, PREVIEW_RESULT),
                             FALSE);
    gtk_widget_set_sensitive(gwy_radio_buttons_find(group, PREVIEW_DIFFERENCE),
                             FALSE);
}

static void
train_steps_changed(NeuralTrainControls *controls, GtkAdjustment *adj)
{
    if (controls->in_update)
        return;
    controls->args->trainsteps = gwy_adjustment_get_int(adj);
}

static void
width_changed(NeuralTrainControls *controls, GtkAdjustment *adj)
{
    NeuralNetworkData *nndata = &controls->args->nn->data;
    if (controls->in_update)
        return;
    nndata->width = gwy_adjustment_get_int(adj);
    neural_network_data_resize(nndata);
    GWY_RESOURCE(controls->args->nn)->is_modified = TRUE;
    controls->calculated = FALSE;
}

static void
height_changed(NeuralTrainControls *controls, GtkAdjustment *adj)
{
    NeuralNetworkData *nndata = &controls->args->nn->data;
    if (controls->in_update)
        return;
    nndata->height = gwy_adjustment_get_int(adj);
    neural_network_data_resize(nndata);
    GWY_RESOURCE(controls->args->nn)->is_modified = TRUE;
    controls->calculated = FALSE;
}

static void
nhidden_changed(NeuralTrainControls *controls, GtkAdjustment *adj)
{
    NeuralNetworkData *nndata = &controls->args->nn->data;
    if (controls->in_update)
        return;
    nndata->nhidden = gwy_adjustment_get_int(adj);
    neural_network_data_resize(nndata);
    GWY_RESOURCE(controls->args->nn)->is_modified = TRUE;
    controls->calculated = FALSE;
}

static void
inpowerxy_changed(NeuralTrainControls *controls, GtkAdjustment *adj)
{
    NeuralNetworkData *nndata = &controls->args->nn->data;
    if (controls->in_update)
        return;
    nndata->inpowerxy = gwy_adjustment_get_int(adj);
    GWY_RESOURCE(controls->args->nn)->is_modified = TRUE;
}

static void
inpowerz_changed(NeuralTrainControls *controls, GtkAdjustment *adj)
{
    NeuralNetworkData *nndata = &controls->args->nn->data;
    if (controls->in_update)
        return;
    nndata->inpowerz = gwy_adjustment_get_int(adj);
    GWY_RESOURCE(controls->args->nn)->is_modified = TRUE;
}

static void
outunits_changed(NeuralTrainControls *controls, GtkEntry *entry)
{
    NeuralNetworkData *nndata = &controls->args->nn->data;
    if (controls->in_update)
        return;
    g_free(nndata->outunits);
    nndata->outunits = g_strdup(gtk_entry_get_text(entry));
    GWY_RESOURCE(controls->args->nn)->is_modified = TRUE;
}

static void
reinit_network(NeuralTrainControls *controls)
{
    NeuralNetworkData *nndata = &controls->args->nn->data;
    neural_network_data_init(nndata, NULL);
    controls->calculated = FALSE;
    GWY_RESOURCE(controls->args->nn)->is_modified = TRUE;
}

static gboolean
train_do(GwyNeuralNetwork *nn, GwyDataLine *errors,
         GwyDataField *model, GwyDataField *signal,
         gdouble sfactor, gdouble sshift,
         guint trainsteps)
{
    NeuralNetworkData *nndata = &nn->data;
    GwyDataField *scaled;
    guint width = nndata->width, height = nndata->height, xres, yres;
    guint n, k, irow, col, row;
    guint *indices;
    const gdouble *dtmodel;
    gdouble *dtsignal;
    gdouble eo = 0.0, eh = 0.0;
    gboolean ok = FALSE;

    gwy_app_wait_set_message(_("Training..."));
    gwy_app_wait_set_fraction(0.0);

    scaled = gwy_data_field_duplicate(model);
    gwy_data_field_normalize(scaled);
    xres = gwy_data_field_get_xres(scaled);
    yres = gwy_data_field_get_yres(scaled);
    dtmodel = gwy_data_field_get_data_const(scaled);
    dtsignal = gwy_data_field_get_data(signal);

    indices = g_new(guint, (xres - width)*(yres - height));
    k = 0;
    for (row = height/2; row < yres + height/2 - height; row++) {
        for (col = width/2; col < xres + width/2 - width; col++)
            indices[k++] = (row - height/2)*xres + col - width/2;
    }
    g_assert(k == (xres - width)*(yres - height));

    for (n = 0; n < trainsteps; n++) {
        /* FIXME: Randomisation leads to weird spiky NN error curves. */
        /* shuffle(indices, (xres - width)*(yres - height), rng); even though
         * it may improve convergence. */
        for (k = 0; k < (xres - width)*(yres - height); k++) {
            for (irow = 0; irow < height; irow++) {
                memcpy(nn->input + irow*width,
                       dtmodel + indices[k] + irow*xres,
                       width*sizeof(gdouble));
            }
            nn->target[0] = sfactor*(dtsignal[indices[k]
                                              + height/2*xres + width/2]
                                              - sshift);
            gwy_neural_network_train_step(nn, 0.3, 0.3, &eo, &eh);
        }
        if (!gwy_app_wait_set_fraction((gdouble)n/trainsteps))
            goto fail;
        gwy_data_line_set_val(errors, n, eo + eh);
    }
    ok = TRUE;

fail:
    g_object_unref(scaled);
    g_free(indices);

    return ok;
}

static gboolean
evaluate_do(GwyNeuralNetwork *nn,
            GwyDataField *model, GwyDataField *result,
            gdouble sfactor, gdouble sshift)
{
    NeuralNetworkData *nndata = &nn->data;
    GwyDataField *scaled;
    GwySIUnit *unit;
    guint width = nndata->width, height = nndata->height, xres, yres;
    guint col, row, irow;
    const gdouble *drmodel;
    gdouble *dresult;
    gdouble avg;
    gboolean ok = FALSE;

    gwy_app_wait_set_message(_("Evaluating..."));
    gwy_app_wait_set_fraction(0.0);

    scaled = gwy_data_field_duplicate(model);
    gwy_data_field_normalize(scaled);
    xres = gwy_data_field_get_xres(scaled);
    yres = gwy_data_field_get_yres(scaled);
    drmodel = gwy_data_field_get_data_const(scaled);
    dresult = gwy_data_field_get_data(result);

    for (row = height/2; row < yres + height/2 - height; row++) {
        for (col = width/2; col < xres + width/2 - width; col++) {
            for (irow = 0; irow < height; irow++) {
                memcpy(nn->input + irow*width,
                       drmodel + ((row + irow - height/2)*xres
                                  + col - width/2),
                       width*sizeof(gdouble));
            }
            gwy_neural_network_forward_feed(nn);
            dresult[row*xres + col] = nn->output[0]/sfactor + sshift;
        }
        if (row % 32 == 31 && !gwy_app_wait_set_fraction((gdouble)row/yres))
            goto fail;
    }
    ok = TRUE;
    unit = gwy_data_field_get_si_unit_z(result);
    gwy_si_unit_set_from_string(unit, nndata->outunits);
    gwy_si_unit_power_multiply(unit, 1,
                               gwy_data_field_get_si_unit_xy(model),
                               nndata->inpowerxy,
                               unit);
    gwy_si_unit_power_multiply(unit, 1,
                               gwy_data_field_get_si_unit_z(model),
                               nndata->inpowerz,
                               unit);

    /* Fill the borders with the average of result. */
    avg = gwy_data_field_area_get_avg_mask(result, NULL, GWY_MASK_IGNORE,
                                           width/2, height/2,
                                           xres - width, yres - height);
    gwy_data_field_area_fill(result, 0, 0, xres, height/2, avg);
    gwy_data_field_area_fill(result, 0, height/2, width/2, yres - height, avg);
    gwy_data_field_area_fill(result,
                             xres + width/2 - width, height/2,
                             width - width/2, yres - height,
                             avg);
    gwy_data_field_area_fill(result,
                             0, yres + height/2 - height,
                             xres, height - height/2,
                             avg);

fail:
    g_object_unref(scaled);
    return ok;
}

static void
train_network(NeuralTrainControls *controls)
{
    NeuralTrainArgs *args = controls->args;
    GwyNeuralNetwork *nn = args->nn;
    GwyDataField *tmodel, *tsignal, *result, *diff;
    GwyDataLine *errors;
    GwyGraphCurveModel *gcmodel;
    gdouble sfactor, sshift;
    GSList *group;
    gboolean ok;

    gwy_app_wait_start(GTK_WINDOW(controls->dialog), _("Starting..."));

    tmodel = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    tsignal = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                              "/1/data"));
    result = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/2/data"));
    diff = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                           "/3/data"));

    errors = gwy_data_line_new(args->trainsteps, args->trainsteps, TRUE);
    sshift = gwy_data_field_get_min(tsignal);
    sfactor = 1.0/(gwy_data_field_get_max(tsignal) - sshift);

    gwy_neural_network_use(GWY_RESOURCE(nn));
    ok = (train_do(nn, errors, tmodel, tsignal, sfactor, sshift,
                   args->trainsteps)
          && evaluate_do(nn, tmodel, result, sfactor, sshift));
    gwy_neural_network_release(GWY_RESOURCE(nn));
    GWY_RESOURCE(nn)->is_modified = TRUE;

    gwy_app_wait_finish();

    /* FIXME: Should we return the neural network to the original state when
     * the training is cancelled? */
    if (!ok) {
        g_object_unref(errors);
        return;
    }

    gwy_data_field_min_of_fields(diff, result, tsignal);
    gwy_data_field_data_changed(result);
    gwy_data_field_data_changed(diff);

    if (gwy_graph_model_get_n_curves(controls->gmodel))
        gcmodel = gwy_graph_model_get_curve(controls->gmodel, 0);
    else {
        gcmodel = gwy_graph_curve_model_new();
        g_object_set(gcmodel, "description", _("NN training error"), NULL);
        gwy_graph_model_add_curve(controls->gmodel, gcmodel);
        g_object_unref(gcmodel);
    }
    gwy_graph_curve_model_set_data_from_dataline(gcmodel, errors, -1, -1);
    gwy_object_unref(errors);

    controls->calculated = TRUE;
    group = controls->preview_group;
    gtk_widget_set_sensitive(gwy_radio_buttons_find(group, PREVIEW_RESULT),
                             TRUE);
    gtk_widget_set_sensitive(gwy_radio_buttons_find(group, PREVIEW_DIFFERENCE),
                             TRUE);
}

static void
network_load(NeuralTrainControls *controls)
{
    GwyNeuralNetwork *network;
    NeuralNetworkData *nndata;
    GtkTreeModel *store;
    GtkTreeSelection *tselect;
    GtkTreeIter iter;

    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->networklist));
    if (!gtk_tree_selection_get_selected(tselect, &store, &iter))
        return;

    gtk_tree_model_get(store, &iter, 0, &network, -1);
    nndata = &controls->args->nn->data;
    neural_network_data_copy(&network->data, nndata);
    neural_train_update_controls(controls);
    controls->calculated = FALSE;
}

static void
network_store(NeuralTrainControls *controls)
{
    GwyNeuralNetwork *network;
    NeuralNetworkData *nndata;
    GtkTreeModelFilter *filter;
    GtkTreeModel *model, *imodel;
    GtkTreeSelection *tselect;
    GtkTreeIter iter, iiter;
    const gchar *name;

    nndata = &controls->args->nn->data;
    name = gtk_entry_get_text(GTK_ENTRY(controls->networkname));
    if (!network_validate_name(controls, name, TRUE))
        return;
    gwy_debug("Now I'm saving `%s'", name);
    network = gwy_inventory_get_item(gwy_neural_networks(), name);
    if (!network) {
        gwy_debug("Appending `%s'", name);
        network = gwy_neural_network_new(name, nndata, FALSE);
        gwy_inventory_insert_item(gwy_neural_networks(), network);
        g_object_unref(network);
    }
    else {
        gwy_debug("Setting `%s'", name);
        neural_network_data_copy(nndata, &network->data);
        gwy_resource_data_changed(GWY_RESOURCE(network));
    }
    gwy_neural_network_save(network);

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->networklist));
    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->networklist));
    filter = GTK_TREE_MODEL_FILTER(model);
    imodel = gtk_tree_model_filter_get_model(filter);
    gwy_inventory_store_get_iter(GWY_INVENTORY_STORE(imodel), name, &iiter);
    gtk_tree_model_filter_convert_child_iter_to_iter(filter, &iter, &iiter);
    gtk_tree_selection_select_iter(tselect, &iter);
}

static void
network_delete(NeuralTrainControls *controls)
{
    GwyNeuralNetwork *network;
    GtkTreeModel *model;
    GtkTreeSelection *tselect;
    GtkTreeIter iter;
    gchar *filename;
    const gchar *name;

    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->networklist));
    if (!gtk_tree_selection_get_selected(tselect, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, 0, &network, -1);
    name = gwy_resource_get_name(GWY_RESOURCE(network));
    filename = gwy_resource_build_filename(GWY_RESOURCE(network));
    if (g_remove(filename))
        g_warning("Cannot remove preset %s", filename);
    g_free(filename);
    gwy_inventory_delete_item(gwy_neural_networks(), name);
}

static void
network_rename(NeuralTrainControls *controls)
{
    GwyNeuralNetwork *network;
    GtkTreeModelFilter *filter;
    GwyInventory *inventory;
    GtkTreeModel *model, *imodel;
    GtkTreeSelection *tselect;
    GtkTreeIter iter, iiter;
    const gchar *newname, *oldname;
    gchar *oldfilename, *newfilename;

    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->networklist));
    if (!gtk_tree_selection_get_selected(tselect, &model, &iter))
        return;

    inventory = gwy_neural_networks();
    gtk_tree_model_get(model, &iter, 0, &network, -1);
    oldname = gwy_resource_get_name(GWY_RESOURCE(network));
    newname = gtk_entry_get_text(GTK_ENTRY(controls->networkname));
    if (gwy_strequal(newname, oldname)
        || !network_validate_name(controls, newname, TRUE)
        || gwy_inventory_get_item(inventory, newname))
        return;

    gwy_debug("Now I will rename `%s' to `%s'", oldname, newname);

    oldfilename = gwy_resource_build_filename(GWY_RESOURCE(network));
    gwy_inventory_rename_item(inventory, oldname, newname);
    newfilename = gwy_resource_build_filename(GWY_RESOURCE(network));
    if (g_rename(oldfilename, newfilename) != 0) {
        g_warning("Cannot rename network %s to %s", oldfilename, newfilename);
        gwy_inventory_rename_item(inventory, newname, oldname);
    }
    g_free(oldfilename);
    g_free(newfilename);

    filter = GTK_TREE_MODEL_FILTER(model);
    imodel = gtk_tree_model_filter_get_model(filter);
    gwy_inventory_store_get_iter(GWY_INVENTORY_STORE(imodel), newname, &iiter);
    gtk_tree_model_filter_convert_child_iter_to_iter(filter, &iter, &iiter);
    gtk_tree_selection_select_iter(tselect, &iter);
}

static void
network_selected(NeuralTrainControls *controls)
{
    GwyNeuralNetwork *network;
    GtkTreeModel *store;
    GtkTreeSelection *tselect;
    GtkTreeIter iter;
    const gchar *name;

    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->networklist));
    g_return_if_fail(tselect);
    if (!gtk_tree_selection_get_selected(tselect, &store, &iter)) {
        gtk_widget_set_sensitive(controls->load, FALSE);
        gtk_widget_set_sensitive(controls->delete, FALSE);
        gtk_widget_set_sensitive(controls->rename, FALSE);
        gtk_entry_set_text(GTK_ENTRY(controls->networkname), "");
        gwy_debug("Nothing is selected");
        return;
    }

    gtk_tree_model_get(store, &iter, 0, &network, -1);
    name = gwy_resource_get_name(GWY_RESOURCE(network));
    gtk_entry_set_text(GTK_ENTRY(controls->networkname), name);

    gtk_widget_set_sensitive(controls->load, TRUE);
    gtk_widget_set_sensitive(controls->delete, TRUE);
    gtk_widget_set_sensitive(controls->rename, TRUE);
}

static gboolean
network_validate_name(NeuralTrainControls *controls,
                      const gchar *name,
                      gboolean show_warning)
{
    GtkWidget *dialog, *parent;

    if (*name && !strchr(name, '/'))
        return TRUE;
    if (!show_warning)
        return FALSE;

    parent = controls->dialog;
    dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                    GTK_DIALOG_MODAL
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_CLOSE,
                                    _("The name `%s' is invalid."),
                                    name);
    gtk_window_set_modal(GTK_WINDOW(parent), FALSE);  /* Bug #66 workaround. */
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    gtk_window_set_modal(GTK_WINDOW(parent), TRUE);  /* Bug #66 workaround. */

    return FALSE;
}

static void
set_layer_channel(GwyPixmapLayer *layer, gint channel)
{
    gchar data_key[30];
    gchar grad_key[30];
    gchar mm_key[30];
    gchar range_key[30];

    g_snprintf(data_key, sizeof(data_key), "/%i/data", channel);
    g_snprintf(grad_key, sizeof(grad_key), "/%i/base/palette", channel);
    g_snprintf(mm_key, sizeof(mm_key), "/%i/base", channel);
    g_snprintf(range_key, sizeof(range_key), "/%i/base/range-type", channel);

    gwy_pixmap_layer_set_data_key(layer, data_key);
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), grad_key);
    gwy_layer_basic_set_min_max_key(GWY_LAYER_BASIC(layer), mm_key);
    gwy_layer_basic_set_range_type_key(GWY_LAYER_BASIC(layer), range_key);
}

//static const gchar trainsteps_key[] = "/module/neural/trainsteps";

static void
neural_train_sanitize_args(NeuralTrainArgs *args)
{
    args->trainsteps = MIN(args->trainsteps, 10000);
}

static void
neural_train_load_args(GwyContainer *settings,
                       NeuralTrainArgs *args)
{
    GwyInventory *inventory;
    const guchar *name = GWY_NEURAL_NETWORK_UNTITLED;

    gwy_clear(args, 1);
    args->preview_type = PREVIEW_MODEL;
    args->trainsteps = trainsteps_default;

    gwy_container_gis_int32_by_name(settings, trainsteps_key,
                                    &args->trainsteps);
    neural_train_sanitize_args(args);

    inventory = gwy_neural_networks();
    if ((args->nn = gwy_inventory_get_item(inventory, name)))
        return;

    args->nn = gwy_neural_networks_create_untitled();
    gwy_neural_network_save(args->nn);
}

static void
neural_train_save_args(GwyContainer *settings,
                       NeuralTrainArgs *args)
{
    gwy_container_set_int32_by_name(settings, trainsteps_key,
                                    args->trainsteps);
    gwy_neural_network_save(args->nn);
}

static gboolean
gwy_neural_network_save(GwyNeuralNetwork *nn)
{
    GwyResource *resource;
    GString *str;
    gchar *filename;
    FILE *fh;

    resource = GWY_RESOURCE(nn);
    if (!resource->is_modified)
        return TRUE;

    if (!gwy_resource_get_is_modifiable(resource)) {
        g_warning("Non-modifiable resource was modified and is about to be "
                  "saved");
        return FALSE;
    }

    filename = gwy_resource_build_filename(resource);
    fh = g_fopen(filename, "w");
    if (!fh) {
        /* FIXME: GUIze this */
        g_warning("Cannot save resource file: %s", filename);
        g_free(filename);
        return FALSE;
    }
    g_free(filename);

    str = gwy_resource_dump(resource);
    fwrite(str->str, 1, str->len, fh);
    fclose(fh);
    g_string_free(str, TRUE);

    gwy_resource_data_saved(resource);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
