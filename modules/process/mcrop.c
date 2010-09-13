/*
 *  @(#) $Id$
 *  Copyright (C) 2010, David Necas (Yeti), Petr Klapetek, Daniil Bratashov
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, dn2010@gmail.com
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
#include <libprocess/arithmetic.h>
#include <libprocess/correlation.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define MCROP_RUN_MODES GWY_RUN_INTERACTIVE

typedef struct {
    gint x;
    gint y;
    gint width;
    gint height;
} GwyRectangle;

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

static gboolean module_register      (void);
static void     mcrop                (GwyContainer *data,
                                      GwyRunType run);
static void     mcrop_data_cb        (GwyDataChooser *chooser,
                                      GwyDataObjectId *object);
static gboolean mcrop_data_filter    (GwyContainer *data, gint id,
                                      gpointer user_data);
static gboolean mcrop_dialog         (GwyDataObjectId *op1,
                                      GwyDataObjectId *op2);
static gboolean mcrop_do             (GwyDataObjectId *op1,
                                      GwyDataObjectId *op2);
static gboolean get_score_iteratively(GwyDataField *data_field,
                                      GwyDataField *kernel_field,
                                      GwyDataField *score,
                                      GwyDataObjectId *op1);
static void     find_score_maximum   (GwyDataField *correlation_score,
                                      gint *max_col,
                                      gint *max_row);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Crops non-intersecting regions of two images."),
    "Daniil Bratashov <dn2010@gmail.com>",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek & Daniil Bratashov",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("mcrop",
                              (GwyProcessFunc)&mcrop,
                              N_("/M_ultidata/Mutual C_rop..."),
                              GWY_STOCK_CROP,
                              MCROP_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Crop non-intersecting regions of two images"));

    return TRUE;
}

static void
mcrop(GwyContainer *data, GwyRunType run)
{
    GwyDataObjectId op1, op2;
    GQuark quark1, quark2;

    g_return_if_fail(run & MCROP_RUN_MODES);

    op1.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &op1.id, 0);
    op2 = op1;

    if (mcrop_dialog(&op1, &op2)) {
		if (op1.id != op2.id) {
			quark1 = gwy_app_get_data_key_for_id(op1.id);
			quark2 = gwy_app_get_data_key_for_id(op2.id);
			gwy_app_undo_qcheckpoint(data, quark1, quark2, NULL);
			mcrop_do(&op1, &op2);
		}
    }
}

static void
mcrop_data_cb(GwyDataChooser *chooser,
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
mcrop_data_filter(GwyContainer *data, gint id, gpointer user_data)
{
    GwyDataObjectId *object = (GwyDataObjectId*)user_data;
    GwyDataField *op1, *op2;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    op1 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    quark = gwy_app_get_data_key_for_id(object->id);
    op2 = GWY_DATA_FIELD(gwy_container_get_object(object->data, quark));

    return !gwy_data_field_check_compatibility(op1, op2,
                                               GWY_DATA_COMPATIBILITY_MEASURE
                                               | GWY_DATA_COMPATIBILITY_LATERAL
                                               | GWY_DATA_COMPATIBILITY_VALUE);
}

static gboolean mcrop_dialog (GwyDataObjectId *op1, GwyDataObjectId *op2)
{
    GtkWidget *dialog, *chooser, *table;

    gint response;
    gboolean ok;

    dialog = gtk_dialog_new_with_buttons(_("Mutual Crop"),
                                         NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(2, 1, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);

    chooser = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(chooser), "dialog", dialog);
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(chooser),
                                mcrop_data_filter, op1, NULL);
    g_signal_connect(chooser, "changed",
                     G_CALLBACK(mcrop_data_cb), op2);
    mcrop_data_cb(GWY_DATA_CHOOSER(chooser), op2);
    gwy_table_attach_hscale(table, 1, _("_Select second argument:"), NULL,
                            GTK_OBJECT(chooser), GWY_HSCALE_WIDGET);

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

static gboolean
mcrop_do(GwyDataObjectId *op1, GwyDataObjectId *op2)
{
    GwyDataField *dfield1, *dfield2;
    GwyDataField *correlation_data, *correlation_kernel, *correlation_score;
    GwyRectangle cdata, kdata;
    gint max_col, max_row;
    gint x1l, x1r, y1t, y1b, x2l, x2r, y2t, y2b;
    gint xres1, xres2, yres1, yres2;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(op1->id);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object(op1->data,
                             quark));

    quark = gwy_app_get_data_key_for_id(op2->id);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object(op2->data,
                             quark));

    if ((dfield1->xres*dfield1->yres) < (dfield2->xres*dfield2->yres)) {
        GWY_SWAP(GwyDataField*, dfield1, dfield2);
    }

    xres1 = gwy_data_field_get_xres(dfield1);
    xres2 = gwy_data_field_get_xres(dfield2);
    yres1 = gwy_data_field_get_yres(dfield1);
    yres2 = gwy_data_field_get_yres(dfield2);

    cdata.x = 0;
    cdata.y = 0;
    cdata.width = xres1;
    cdata.height = yres1;
    kdata.width = MIN(xres2, cdata.width/3);
    kdata.height = MIN(yres2, cdata.height/3);
    kdata.x = MAX(0, xres2/2 - kdata.width/2);
    kdata.y = MAX(0, yres2/2 - kdata.height/2);

    correlation_data = gwy_data_field_area_extract(dfield1,
                                                   cdata.x,
                                                   cdata.y,
                                                   cdata.width,
                                                   cdata.height);
    correlation_kernel = gwy_data_field_area_extract(dfield2,
                                                     kdata.x,
                                                     kdata.y,
                                                     kdata.width,
                                                     kdata.height);
    correlation_score = gwy_data_field_new_alike(correlation_data, FALSE);

    /* get appropriate correlation score */
    if (!get_score_iteratively(correlation_data, correlation_kernel,
                               correlation_score, op1)) {
        g_object_unref(correlation_score);
        g_object_unref(correlation_data);
        g_object_unref(correlation_kernel);

        return FALSE;
    }

    find_score_maximum(correlation_score, &max_col, &max_row);
    gwy_debug("c: %d %d %dx%d  k: %d %d %dx%d res: %d %d\n",
           cdata.x,
           cdata.y,
           cdata.width,
           cdata.height,
           kdata.x,
           kdata.y,
           kdata.width,
           kdata.height,
           max_col, max_row
            );

    x1l = MAX(0, MAX(max_col-xres1/2, max_col-xres2/2));
    y1b = MAX(0, MAX(max_row-yres1/2, max_row-yres2/2));
    x1r = MIN(xres1, MIN(max_col+xres1/2, max_col+xres2/2));
    y1t = MIN(yres1, MIN(max_row+yres1/2, max_row+yres2/2));

    x2l = MAX(0, xres2/2-max_col);
    x2r = x2l+x1r-x1l;
    y2b = MAX(0, yres2/2-max_row);
    y2t = y2b+y1t-y1b;

    gwy_debug("%d %d %d %d\n", x1l, y1b, x1r, y1t);
    gwy_debug("%d %d %d %d\n", x2l, y2b, x2r, y2t);

    gwy_data_field_resize(dfield1, x1l, y1b, x1r, y1t);
    gwy_data_field_data_changed(dfield1);
    gwy_data_field_resize(dfield2, x2l, y2b, x2r, y2t);
    gwy_data_field_data_changed(dfield2);

    g_object_unref(correlation_data);
    g_object_unref(correlation_kernel);
    g_object_unref(correlation_score);

    return TRUE;
}

/* compute corelation */
static gboolean
get_score_iteratively(GwyDataField *data_field, GwyDataField *kernel_field,
                      GwyDataField *score, GwyDataObjectId *op1)
{
    enum { WORK_PER_UPDATE = 50000000 };
    GwyComputationState *state;
    gboolean ok = FALSE;
    int work, wpi;

    work = 0;
    wpi = gwy_data_field_get_xres(kernel_field)
          *gwy_data_field_get_yres(kernel_field);
    wpi = MIN(wpi, WORK_PER_UPDATE);
    state = gwy_data_field_correlate_init(data_field, kernel_field, score);

    /* FIXME */
    gwy_app_wait_start(gwy_app_find_window_for_channel(op1->data,
                                                       op1->id),
                       _("Initializing"));
    gwy_data_field_correlate_iteration(state);
    if (!gwy_app_wait_set_message(_("Correlating")))
        goto get_score_fail;
    do {
        gwy_data_field_correlate_iteration(state);
        work += wpi;
        if (work > WORK_PER_UPDATE) {
            work -= WORK_PER_UPDATE;
            if (!gwy_app_wait_set_fraction(state->fraction))
                goto get_score_fail;
        }
    } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
    ok = TRUE;

get_score_fail:
    gwy_data_field_correlate_finalize(state);
    gwy_app_wait_finish();

    return ok;
}

static void
find_score_maximum(GwyDataField *correlation_score,
                   gint *max_col,
                   gint *max_row)
{
    gint i, n, maxi = 0;
    gdouble max = -G_MAXDOUBLE;
    const gdouble *data;

    n = gwy_data_field_get_xres(correlation_score)
        *gwy_data_field_get_yres(correlation_score);
    data = gwy_data_field_get_data_const(correlation_score);

    for (i = 0; i < n; i++) {
        if (max < data[i]) {
            max = data[i];
            maxi = i;
        }
    }

    *max_row = (gint)floor(maxi/gwy_data_field_get_xres(correlation_score));
    *max_col = maxi - (*max_row)*gwy_data_field_get_xres(correlation_score);
}

