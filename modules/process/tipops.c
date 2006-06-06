/*
 *  @(#) $Id$
 *  Copyright (C) 2004-2006 David Necas (Yeti), Petr Klapetek.
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
#include <libprocess/tip.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define TIP_OPS_RUN_MODES GWY_RUN_INTERACTIVE

typedef enum {
    DILATION,
    EROSION,
    CERTAINTY_MAP
} TipOperation;

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    GwyDataObjectId tip;
    GwyDataObjectId target;
} TipOpsArgs;

static gboolean module_register  (void);
static void     tipops           (GwyContainer *data,
                                  GwyRunType run,
                                  const gchar *name);
static gboolean tipops_dialog    (TipOpsArgs *args,
                                  TipOperation op);
static void     tipops_tip_cb    (GwyDataChooser *chooser,
                                  GwyDataObjectId *object);
static gboolean tipops_tip_filter(GwyContainer *data,
                                  gint id,
                                  gpointer user_data);
static gboolean tipops_check     (TipOpsArgs *args,
                                  GtkWidget *tipops_window);
static void     tipops_do        (TipOpsArgs *args,
                                  TipOperation op);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Tip operations: dilation (convolution), erosion (reconstruction) "
       "and certainty map."),
    "Petr Klapetek <klapetek@gwyddion.net>, Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("tip_dilation",
                              &tipops,
                              N_("/_Tip/_Dilation..."),
                              NULL,
                              TIP_OPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Surface dilation by defined tip"));
    gwy_process_func_register("tip_reconstruction",
                              &tipops,
                              N_("/_Tip/_Surface Reconstruction..."),
                              NULL,
                              TIP_OPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Surface reconstruction by defined tip"));
    gwy_process_func_register("tip_map",
                              &tipops,
                              N_("/_Tip/_Certainty Map..."),
                              NULL,
                              TIP_OPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Tip certainty map"));

    return TRUE;
}

static void
tipops(GwyContainer *data,
       GwyRunType run,
       const gchar *name)
{
    static const GwyEnum ops[] = {
        { "tip_dilation",       DILATION,      },
        { "tip_reconstruction", EROSION,       },
        { "tip_map",            CERTAINTY_MAP, },
    };

    TipOperation op;
    TipOpsArgs args;

    g_return_if_fail(run & TIP_OPS_RUN_MODES);
    op = gwy_string_to_enum(name, ops, G_N_ELEMENTS(ops));
    if (op == (TipOperation)-1) {
        g_warning("tipops does not provide function `%s'", name);
        return;
    }

    args.target.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &args.target.id, 0);
    args.tip.data = NULL;

    if (tipops_dialog(&args, op))
        tipops_do(&args, op);
}

static gboolean
tipops_dialog(TipOpsArgs *args,
              TipOperation op)
{
    static const gchar *titles[] = {
        N_("Tip Dilation"),
        N_("Surface Reconstruction"),
        N_("Certainty Map Analysis"),
    };
    GtkWidget *dialog, *table, *chooser, *label;
    gint row, response;
    gboolean ok = FALSE;

    dialog = gtk_dialog_new_with_buttons(_(titles[op]), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(1, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /***** First operand *****/
    label = gtk_label_new_with_mnemonic(_("_Tip morphology:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    chooser = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(chooser), "dialog", dialog);
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(chooser),
                                tipops_tip_filter, &args->target, NULL);
    g_signal_connect(chooser, "changed", G_CALLBACK(tipops_tip_cb), &args->tip);
    tipops_tip_cb(GWY_DATA_CHOOSER(chooser), &args->tip);
    gtk_table_attach_defaults(GTK_TABLE(table), chooser, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), chooser);
    row++;

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
            ok = tipops_check(args, dialog);
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
tipops_tip_cb(GwyDataChooser *chooser,
              GwyDataObjectId *object)
{
    GtkWidget *dialog;

    object->data = gwy_data_chooser_get_active(chooser, &object->id);
    gwy_debug("tip: %p %d", object->data, object->id);

    dialog = g_object_get_data(G_OBJECT(chooser), "dialog");
    g_assert(GTK_IS_DIALOG(dialog));
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
                                      object->data != NULL);
}

static gboolean
tipops_tip_filter(GwyContainer *data,
                  gint id,
                  gpointer user_data)
{
    GwyDataObjectId *object = (GwyDataObjectId*)user_data;
    GwyDataField *tip, *target;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    tip = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    quark = gwy_app_get_data_key_for_id(object->id);
    target = GWY_DATA_FIELD(gwy_container_get_object(object->data, quark));

    if (gwy_data_field_get_xreal(tip) <= gwy_data_field_get_xreal(target)/4
        && gwy_data_field_get_yreal(tip) <= gwy_data_field_get_yreal(target)/4
        && gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(tip),
                             gwy_data_field_get_si_unit_xy(target))
        && gwy_si_unit_equal(gwy_data_field_get_si_unit_z(tip),
                             gwy_data_field_get_si_unit_z(target)))
        return TRUE;

    return FALSE;
}

static gboolean
tipops_check(TipOpsArgs *args,
             GtkWidget *tipops_window)
{
    GtkWidget *dialog;
    GwyDataField *tip, *target;
    gdouble tipxm, tipym, targetxm, targetym;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(args->tip.id);
    tip = GWY_DATA_FIELD(gwy_container_get_object(args->tip.data, quark));

    quark = gwy_app_get_data_key_for_id(args->target.id);
    target = GWY_DATA_FIELD(gwy_container_get_object(args->target.data, quark));

    tipxm = gwy_data_field_get_xmeasure(tip);
    tipym = gwy_data_field_get_ymeasure(tip);
    targetxm = gwy_data_field_get_xmeasure(target);
    targetym = gwy_data_field_get_ymeasure(target);
    if (fabs(log(tipxm/targetxm)) <= 0.001
        && fabs(log(tipym/targetym)) <= 0.001)
        return TRUE;

    dialog = gtk_message_dialog_new(GTK_WINDOW(tipops_window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_OK,
                                    _("Tip has different size/resolution "
                                      "ratio than image. Tip will be "
                                      "resampled."));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
tipops_do(TipOpsArgs *args,
          TipOperation op)
{
    static const gchar *data_titles[] = {
        N_("Dilated data"),
        N_("Surface reconstruction"),
    };
    GwyDataField *dfield, *tip, *target;
    GwyDataWindow *window;
    GQuark quark;
    gboolean ok;
    gint newid;

    quark = gwy_app_get_data_key_for_id(args->tip.id);
    tip = GWY_DATA_FIELD(gwy_container_get_object(args->tip.data, quark));

    quark = gwy_app_get_data_key_for_id(args->target.id);
    target = GWY_DATA_FIELD(gwy_container_get_object(args->target.data, quark));

    /* result fields - after computation result should be in dfield */
    dfield = gwy_data_field_duplicate(target);

    /* FIXME */
    window = gwy_app_data_window_get_for_data(args->target.data);
    gwy_app_wait_start(GTK_WIDGET(window), _("Initializing..."));

    if (op == DILATION || op == EROSION) {
        if (op == DILATION)
            ok = gwy_tip_dilation(tip, target, dfield,
                                  gwy_app_wait_set_fraction,
                                  gwy_app_wait_set_message) != NULL;
        else
            ok = gwy_tip_erosion(tip, target, dfield,
                                 gwy_app_wait_set_fraction,
                                 gwy_app_wait_set_message) != NULL;
        gwy_app_wait_finish();

        if (ok) {
            newid = gwy_app_data_browser_add_data_field(dfield,
                                                        args->target.data,
                                                        TRUE);
            gwy_app_copy_data_items(args->target.data, args->target.data,
                                    args->target.id, newid,
                                    GWY_DATA_ITEM_PALETTE, 0);
            gwy_app_set_data_field_title(args->target.data, newid,
                                         data_titles[op]);
        }
    }
    else {
        ok = gwy_tip_cmap(tip, target, dfield,
                          gwy_app_wait_set_fraction,
                          gwy_app_wait_set_message) != NULL;
        gwy_app_wait_finish();

        if (ok) {
            quark = gwy_app_get_mask_key_for_id(args->target.id);
            gwy_app_undo_qcheckpointv(args->target.data, 1, &quark);
            gwy_container_set_object(args->target.data, quark, dfield);
        }
    }
    g_object_unref(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

