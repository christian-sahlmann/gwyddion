/*
 *  @(#) $Id: supres.c 6601 2006-09-18 15:38:58Z yeti-dn $
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyexpr.h>
#include <libprocess/datafield.h>
#include <libprocess/stats.h>
#include <libprocess/arithmetic.h>
#include <libprocess/correlation.h>
#include <libprocess/hough.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>


#define SUPRES_RUN_MODES GWY_RUN_INTERACTIVE

enum {
    NARGS = 4
};

enum {
    SUPRES_OK   = 0,
    SUPRES_DATA = 1,
};

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    guint err;
    GwyDataObjectId objects[NARGS];
    gchar *name[NARGS];
    guint pos[NARGS];
} SupresArgs;

typedef struct {
    SupresArgs *args;
    GtkWidget *dialog;
    GtkWidget *result;
    GtkWidget *data[NARGS];
} SupresControls;

static gboolean     module_register           (void);
static void         supres                (GwyContainer *data,
                                               GwyRunType run);
static void         supres_load_args      (GwyContainer *settings,
                                               SupresArgs *args);
static void         supres_save_args      (GwyContainer *settings,
                                               SupresArgs *args);
static gboolean     supres_dialog         (SupresArgs *args);
static void         supres_data_cb        (GwyDataChooser *chooser,
                                               SupresControls *controls);
static void         supres_maybe_preview  (SupresControls *controls);
static const gchar* supres_check          (SupresArgs *args);
static void         supres_do             (SupresArgs *args);
static GwyDataField*       make_superresolution  (GwyDataField *result, 
                                           GPtrArray *fields,
                                           SupresArgs *args);



static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Simple supres operations with two data fields "
       "(or a data field and a scalar)."),
    "Yeti <yeti@gwyddion.net>",
    "2.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("superresolution",
                              (GwyProcessFunc)&supres,
                              N_("/M_ultidata/_Super-resolution..."),
                              NULL,
                              SUPRES_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Super resolution of multiple images of same object"));

    return TRUE;
}

void
supres(GwyContainer *data, GwyRunType run)
{
    SupresArgs args;
    guint i;
    GwyContainer *settings;
    gint id;

    g_return_if_fail(run & SUPRES_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id, 0);

    settings = gwy_app_settings_get();
    for (i = 0; i < NARGS; i++) {
        args.objects[i].data = data;
        args.objects[i].id = id;
    }
    supres_load_args(settings, &args);
    if (supres_dialog(&args)) {
        supres_do(&args);
    }
    supres_save_args(settings, &args);
}

static gboolean
supres_dialog(SupresArgs *args)
{
    SupresControls controls;
    GtkWidget *dialog, *table, *chooser, *entry, *label;
    guint i, row, response;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Supres"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4 + NARGS, 2, FALSE);
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

    for (i = 0; i < NARGS; i++) {
        args->name[i] = g_strdup_printf("d_%d", i+1);
        label = gtk_label_new_with_mnemonic(args->name[i]);
        gwy_strkill(args->name[i], "_");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);

        chooser = gwy_data_chooser_new_channels();
        gwy_data_chooser_set_active(GWY_DATA_CHOOSER(chooser),
                                    args->objects[i].data, args->objects[i].id);
        g_signal_connect(chooser, "changed",
                         G_CALLBACK(supres_data_cb), &controls);
        g_object_set_data(G_OBJECT(chooser), "index", GUINT_TO_POINTER(i));
        gtk_table_attach(GTK_TABLE(table), chooser, 1, 2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), chooser);
        controls.data[i] = chooser;

        row++;
    }
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    controls.result = label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);


    args->err = SUPRES_OK;
    supres_data_cb(controls.data[0], &controls);
    

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
supres_data_cb(GwyDataChooser *chooser,
                   SupresControls *controls)
{
    SupresArgs *args;
    guint i;

    args = controls->args;
    i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(chooser), "index"));
    args->objects[i].data = gwy_data_chooser_get_active(chooser,
                                                        &args->objects[i].id);
    if (!(args->err))
        supres_maybe_preview(controls);
}


static void
supres_maybe_preview(SupresControls *controls)
{
    SupresArgs *args;
    const gchar *message;

    args = controls->args;
    message = supres_check(args);
    if (args->err) {
        gtk_label_set_text(GTK_LABEL(controls->result), message);
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                          GTK_RESPONSE_OK, FALSE);
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls->result), "");
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                          GTK_RESPONSE_OK, TRUE);
    }
}

static const gchar*
supres_check(SupresArgs *args)
{
    guint first = 0, i;
    GwyContainer *data;
    GQuark quark;
    GwyDataField *dfirst, *dfield;
    GwyDataCompatibilityFlags diff;

    if (args->err)
        return NULL;

    for (i = 0; i < NARGS; i++) {
        if (args->pos[i]) {
            first = i;
            break;
        }
    }
    if (i == NARGS) {
        /* no variables */
        args->err &= ~SUPRES_DATA;
        return NULL;
    }

    /* each window must match with first, this is transitive */
    data = args->objects[first].data;
    quark = gwy_app_get_data_key_for_id(args->objects[first].id);
    dfirst = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    for (i = first+1; i < NARGS; i++) {
        if (!args->pos[i])
            continue;

        data = args->objects[i].data;
        quark = gwy_app_get_data_key_for_id(args->objects[i].id);
        dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

        /* FIXME: what about value units? */
        diff = gwy_data_field_check_compatibility
                                            (dfirst, dfield,
                                             GWY_DATA_COMPATIBILITY_RES
                                             | GWY_DATA_COMPATIBILITY_REAL
                                             | GWY_DATA_COMPATIBILITY_LATERAL);
        if (diff) {
            args->err |= SUPRES_DATA;
            if (diff & GWY_DATA_COMPATIBILITY_RES)
                return _("Pixel dimensions differ");
            if (diff & GWY_DATA_COMPATIBILITY_LATERAL)
                return _("Lateral dimensions are different physical "
                         "quantities");
            if (diff & GWY_DATA_COMPATIBILITY_REAL)
                return _("Physical dimensions differ");
        }
    }

    args->err &= ~SUPRES_DATA;

    return NULL;
}

static void
supres_do(SupresArgs *args)
{
    GwyContainer *data, *firstdata = NULL;
    GQuark quark;
    GwyDataField *dfield, *result = NULL;
    GPtrArray *fields;
    gboolean first = TRUE;
    guint i;
    gint firstid = -1, newid;

    g_return_if_fail(!args->err);
    fields = g_ptr_array_new();

    for (i = 0; i < NARGS; i++) {
        if (!args->pos[i])
            continue;

        data = args->objects[i].data;
        quark = gwy_app_get_data_key_for_id(args->objects[i].id);
        dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
        g_ptr_array_add(fields, dfield);
        if (first) {
            first = FALSE;
            result = gwy_data_field_new_alike(dfield, FALSE);
            firstdata = data;
            firstid = args->objects[i].id;
        }
    }
    g_return_if_fail(firstdata);

    result = make_superresolution(result, fields, args);

    newid = gwy_app_data_browser_add_data_field(result, firstdata, TRUE);
    g_object_unref(result);
    gwy_app_set_data_field_title(firstdata, newid, _("Calculated"));
    gwy_app_sync_data_items(firstdata, firstdata, firstid, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT, 0);

    g_ptr_array_free(fields, FALSE);
}


static void
supres_load_args(GwyContainer *settings,
                     SupresArgs *args)
{

}

static void
supres_save_args(GwyContainer *settings,
                     SupresArgs *args)
{
}

typedef struct {
    gdouble xval;
    gdouble yval;
    gdouble zval;
    gdouble score;
} SDataPoint;

static gdouble
get_mean_correlation(GwyDataField *dfield, 
                     GwyDataField *last, 
                     gdouble *xmean, 
                     gdouble *ymean,
                     SupresArgs *args
                     )
{
    gint newxres, newyres;
    gdouble mean_score;
    GwyDataField *sm_dfield;
    GwyDataField *sm_last;
    GwyDataField *dfield_kernel;
    GwyDataField *score;
    GwyComputationState *state;

    newxres = gwy_data_field_get_xres(dfield)/2;
    newyres = gwy_data_field_get_yres(dfield)/2;
    sm_dfield = gwy_data_field_new_resampled(dfield, 
                                             newxres,
                                             newyres,
                                             GWY_INTERPOLATION_LINEAR);
    sm_last = gwy_data_field_new_resampled(last, 
                                             newxres,
                                             newyres,
                                             GWY_INTERPOLATION_LINEAR);

    dfield_kernel = gwy_data_field_area_extract(sm_dfield, 
                                                newxres/2 - newxres/6, 
                                                newyres/2 - newyres/6,
                                                newxres/3,
                                                newyres/3);

    score = gwy_data_field_new(newxres, newyres, newxres, newyres, FALSE);

    gwy_app_wait_start(gwy_app_find_window_for_channel(args->objects[0].data,
                                                       args->objects[0].id),
                                                      _("Initializing..."));


    state = gwy_data_field_correlate_init(sm_last, dfield_kernel,
                                               score);
    gwy_app_wait_set_message(_("Correlating to determine mean shift..."));
    do {
        gwy_data_field_correlate_iteration(state);
        if (!gwy_app_wait_set_fraction(state->fraction)) {
            gwy_data_field_correlate_finalize(state);
            gwy_app_wait_finish();
            g_object_unref(score);
            g_object_unref(sm_dfield);
            g_object_unref(sm_last);
            return -2;
        }
    } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
    gwy_data_field_correlate_finalize(state);
    gwy_app_wait_finish();

    if (gwy_data_field_get_local_maxima_list(score,
                                     xmean,
                                     ymean,
                                     &mean_score,
                                     1,
                                     1,
                                     -1,
                                     TRUE));

    
    *xmean -= newxres/2 - 1;
    *ymean -= newyres/2 - 1;
    g_object_unref(score);
    g_object_unref(sm_dfield);
    g_object_unref(sm_last);

    return mean_score;
}

static GwyDataField *
make_superresolution(GwyDataField *result, GPtrArray *fields, SupresArgs *args)
{
    gint col, row, m;
    gint xres, yres;
    gdouble xmean, ymean;
    gdouble mean_threshold, local_threshold;
    gdouble xmax, ymax, scmax, distance, idistance;
    gdouble *xdata, *ydata, *scdata;
    GArray *values, *locals;
    gdouble min, ndist;
    SDataPoint sdp, *pdp;
    GwyDataField *last, *dfield, *last_shifted, *dfield_shifted;
    GwyDataField *dfieldx, *dfieldy, *score;
    GwyComputationState *state;
    gdouble weight, sum, rdist;

    values = g_array_new(FALSE, FALSE, sizeof(SDataPoint));
    locals = g_array_new(FALSE, FALSE, sizeof(int));
    
    last = GWY_DATA_FIELD(g_ptr_array_index(fields, fields->len - 1));
    xres = gwy_data_field_get_xres(last);
    yres = gwy_data_field_get_yres(last);

    dfieldx = gwy_data_field_new_alike(last, TRUE);
    dfieldy = gwy_data_field_new_alike(last, TRUE);
    score = gwy_data_field_new_alike(last, TRUE);

    local_threshold = 0.9;
    mean_threshold = 0.9;
    ndist = 6;
    min = gwy_data_field_get_min(last);

    /*compare locally each image with the last one.*/
    for (m=0; m<(fields->len - 1); m++)
    {
        dfield = GWY_DATA_FIELD(g_ptr_array_index(fields, m));
        
        /*compute overall correlation for downsampled data center to save time*/
/*        if (get_mean_correlation(dfield, last, &xmean, &ymean, args) < mean_threshold)
            continue;
TODO: use this when the rest works well.
            */
        
        /*compute detailed correlation of each point*/
        /*compute crosscorelation */
        
        gwy_app_wait_start(gwy_app_find_window_for_channel(args->objects[0].data,
                                                       args->objects[0].id),
                                                      _("Initializing..."));

        state = gwy_data_field_crosscorrelate_init(dfield, last,
                                               dfieldx, dfieldy, score,
                                               14, 14,
                                               10, 10);
        gwy_app_wait_set_message(_("Cross-correlation..."));
        do {
            gwy_data_field_crosscorrelate_iteration(state);
            if (!gwy_app_wait_set_fraction(state->fraction)) {
                gwy_data_field_crosscorrelate_finalize(state);
                gwy_app_wait_finish();
                g_object_unref(dfieldx);
                g_object_unref(dfieldy);
                g_object_unref(score);
                return FALSE;
            }
        } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
        gwy_data_field_crosscorrelate_finalize(state);
        gwy_app_wait_finish();

        xdata = gwy_data_field_get_data(dfieldx);
        ydata = gwy_data_field_get_data(dfieldy);
        scdata = gwy_data_field_get_data(score);
        for (col=5; col<(xres-5); col++) {
            for (row=5; row<(yres-5); row++) {
               
                if (scdata[row*xres + col] > local_threshold) 
                { 
                    sdp.xval = col + gwy_data_field_rtoi(dfieldx, xdata[row*xres + col]);
                    sdp.yval = row + gwy_data_field_rtoj(dfieldy, ydata[row*xres + col]);
                    sdp.score = scdata[row*xres + col];
                    sdp.zval = gwy_data_field_get_val(dfield, col, row);
                    g_array_append_val(values, sdp); 
                }
            }
        }
        

    }

    /*now, resample the original data twice*/
    gwy_data_field_copy(last, result, FALSE);
    gwy_data_field_resample(result, xres*2, yres*2, GWY_INTERPOLATION_LINEAR);

    /*merge values from array into the field*/
    gwy_app_wait_start(gwy_app_find_window_for_channel(args->objects[0].data,
                                                       args->objects[0].id),
                                                      _("Interpolating..."));

    idistance = 1;
    for (col=1; col<(2*xres - 1); col++) {
        if (!gwy_app_wait_set_fraction(((gdouble)col/(2*(gdouble)xres)))) {
                gwy_app_wait_finish();
                break;
        }
        for (row=1; row<(2*yres - 1); row++) {

            /*get all neighbours within one pixel*/
            g_array_set_size(locals, 0);
            for (m=0; m<values->len; m++)
            {
                pdp = &g_array_index(values, SDataPoint, m);
                if (fabs(2*(pdp->xval) - col)<ndist && fabs(2*(pdp->yval) - row)<ndist) {
                    g_array_append_val(locals, m);
                }

            }
            /*compute weighted average TODO in future recalculate also points from grid*/
            if (((col%2) || (row%2)) && locals->len){
               
                /*add last image data*/ 
                distance = 1.41421356;
                rdist = 4.0/(distance*distance + 1e-18);
                weight = rdist;
                sum = rdist*gwy_data_field_get_val(result, col, row);

                /*add other images data*/
                for (m=0; m<locals->len; m++)
                {
                    pdp = &g_array_index(values, SDataPoint, g_array_index(locals, int, m));
                    distance = sqrt((2*(pdp->xval) - col)*(2*(pdp->xval) - col) 
                                 + (2*(pdp->yval) - row)*(2*(pdp->yval) - row));
                    rdist = 1.0/(distance*distance + 1e-18);
                    weight += rdist;
                    sum += rdist*pdp->zval;
                }
                gwy_data_field_set_val(result, col, row, sum/weight);

            }
            
        } 
    }
    gwy_app_wait_finish();

    g_object_unref(dfieldx);
    g_object_unref(dfieldy);
    g_object_unref(score);
    g_array_free(values, TRUE);
    g_array_free(locals, TRUE);
    return result;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

