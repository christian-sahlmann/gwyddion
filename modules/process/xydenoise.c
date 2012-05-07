/*
 *  @(#) $Id: xydenoise.c 12576 2011-07-11 14:51:57Z yeti-dn $
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/correlation.h>
#include <libprocess/filters.h>
#include <libprocess/inttrans.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <app/gwyapp.h>

#define XYDENOISE_RUN_MODES GWY_RUN_INTERACTIVE


typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    GwyDataObjectId op1;
    GwyDataObjectId op2;
} XYdenoiseArgs;

typedef struct {
    XYdenoiseArgs *args;
} XYdenoiseControls;

static gboolean module_register       (void);
static void     xydenoise              (GwyContainer *data,
                                       GwyRunType run);
static gboolean xydenoise_dialog       (XYdenoiseArgs *args);
static void     xydenoise_data_cb      (GwyDataChooser *chooser,
                                       GwyDataObjectId *object);
static gboolean xydenoise_data_filter(GwyContainer *data,
                                       gint id,
                                       gpointer user_data);
static gboolean xydenoise_do           (XYdenoiseArgs *args);

static const XYdenoiseArgs xydenoise_defaults = {
    { NULL, -1 }, { NULL, -1 },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Denoises measurement on basis of two orthogonal scans."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("xydenoise",
                              (GwyProcessFunc)&xydenoise,
                              N_("/M_ultidata/_XY denoise..."),
                              NULL,
                              XYDENOISE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Denoises horizontal/vertical measurement."));

    return TRUE;
}

static void
xydenoise(GwyContainer *data, GwyRunType run)
{
    XYdenoiseArgs args;
    GwyContainer *settings;

    g_return_if_fail(run & XYDENOISE_RUN_MODES);

    settings = gwy_app_settings_get();

    args.op1.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &args.op1.id, 0);
    args.op2.data = NULL;

    if (xydenoise_dialog(&args))
        xydenoise_do(&args);
}

static gboolean
xydenoise_dialog(XYdenoiseArgs *args)
{
    XYdenoiseControls controls;
    GtkWidget *dialog, *table, *chooser;
    gint row, response;
    gboolean ok = FALSE;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("XY Denoising"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(9, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /* Correlate with */
    chooser = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(chooser), "dialog", dialog);
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(chooser),
                                xydenoise_data_filter, &args->op1, NULL);
    g_signal_connect(chooser, "changed",
                     G_CALLBACK(xydenoise_data_cb), &args->op2);
    xydenoise_data_cb(GWY_DATA_CHOOSER(chooser), &args->op2);
    gwy_table_attach_hscale(table, row, _("Second direction:"), NULL,
                            GTK_OBJECT(chooser), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(dialog);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            ok = TRUE;
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
xydenoise_data_cb(GwyDataChooser *chooser,
                 GwyDataObjectId *object)
{
    GtkWidget *dialog;

    object->data = gwy_data_chooser_get_active(chooser, &object->id);
    gwy_debug("data: %p %d", object->data, object->id);

    dialog = g_object_get_data(G_OBJECT(chooser), "dialog");
    g_assert(GTK_IS_DIALOG(dialog));
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
                                      object->data != NULL);
}



static gboolean
xydenoise_data_filter(GwyContainer *data,
                     gint id,
                     gpointer user_data)
{
    GwyDataObjectId *object = (GwyDataObjectId*)user_data;
    GwyDataField *op1, *op2;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    op1 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    quark = gwy_app_get_data_key_for_id(object->id);
    op2 = GWY_DATA_FIELD(gwy_container_get_object(object->data, quark));

    /* It does not make sense to xydenoiserelate with itself */
    if (op1 == op2)
        return FALSE;

    return !gwy_data_field_check_compatibility(op1, op2,
                                               GWY_DATA_COMPATIBILITY_RES
                                               | GWY_DATA_COMPATIBILITY_REAL
                                               | GWY_DATA_COMPATIBILITY_LATERAL
                                               | GWY_DATA_COMPATIBILITY_VALUE);
}


static gboolean
xydenoise_do(XYdenoiseArgs *args)
{
    GwyContainer *data;
    GwyDataField *dfieldx, *rx, *ix, *dfieldy, *ry, *iy, *result, *iresult;
    gint i, newid, xres, yres;
    gdouble *rxdata, *rydata, *ixdata, *iydata, xmodule, xphase, ymodule, yphase;
    GwyWindowingType window = GWY_WINDOWING_NONE;
    GwyInterpolationType interp = GWY_INTERPOLATION_LINEAR;

    GQuark quark;

    gwy_app_wait_start(gwy_app_find_window_for_channel(args->op1.data, args->op1.id),
                       "Starting...");

    quark = gwy_app_get_data_key_for_id(args->op1.id);
    dfieldx = GWY_DATA_FIELD(gwy_container_get_object(args->op1.data, quark));

    quark = gwy_app_get_data_key_for_id(args->op2.id);
    dfieldy = GWY_DATA_FIELD(gwy_container_get_object(args->op2.data, quark));

    xres = gwy_data_field_get_xres(dfieldx);
    yres = gwy_data_field_get_yres(dfieldy);
    result = gwy_data_field_new_alike(dfieldx, TRUE);
    iresult = gwy_data_field_new_alike(dfieldx, TRUE);
    rx = gwy_data_field_new_alike(dfieldx, TRUE);
    ix = gwy_data_field_new_alike(dfieldx, TRUE);
    ry = gwy_data_field_new_alike(dfieldx, TRUE);
    iy = gwy_data_field_new_alike(dfieldx, TRUE);

    gwy_app_wait_set_fraction(0.1);
    gwy_app_wait_set_message("Computing forward FFTs...");

    gwy_data_field_2dfft(dfieldx, NULL, rx, ix,
                         window, GWY_TRANSFORM_DIRECTION_FORWARD, interp,
                         FALSE, 0);

    gwy_data_field_2dfft(dfieldy, NULL, ry, iy,
                         window, GWY_TRANSFORM_DIRECTION_FORWARD, interp,
                         FALSE, 0);

    rxdata = gwy_data_field_get_data(rx);
    rydata = gwy_data_field_get_data(ry);
    ixdata = gwy_data_field_get_data(ix);
    iydata = gwy_data_field_get_data(iy);

    gwy_app_wait_set_fraction(0.3);
    gwy_app_wait_set_message("Computing image...");

    for (i=0; i<(xres*yres); i++) {
        xmodule = sqrt(rxdata[i]*rxdata[i] + ixdata[i]*ixdata[i]);
        xphase = atan2(ixdata[i],rxdata[i]);
        ymodule = sqrt(rydata[i]*rydata[i] + iydata[i]*iydata[i]);
        yphase = atan2(iydata[i],rydata[i]);
        rxdata[i] = MIN(xmodule, ymodule)*cos(xphase);
        ixdata[i] = MIN(xmodule, ymodule)*sin(xphase);
    }

    gwy_app_wait_set_fraction(0.7);
    gwy_app_wait_set_message("Computing backward FFT...");
    gwy_data_field_2dfft(rx, ix, result, iresult,
                         window, GWY_TRANSFORM_DIRECTION_BACKWARD, interp,
                         FALSE, 0);
    
    gwy_app_wait_set_fraction(0.9);

    data = args->op1.data;
    newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
    gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT, 0);

    gwy_app_set_data_field_title(data, newid, _("Denoised"));
    gwy_app_wait_finish();

    g_object_unref(result);
    g_object_unref(iresult);
    g_object_unref(dfieldy);
    g_object_unref(dfieldx);
    g_object_unref(rx);
    g_object_unref(ix);
    g_object_unref(ry);
    g_object_unref(iy);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

