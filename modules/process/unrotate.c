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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/correct.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define UNROTATE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 160
};

typedef struct {
    GwyInterpolationType interp;
    GwyPlaneSymmetry symmetry;
} UnrotateArgs;

typedef struct {
    GtkWidget *interp;
    GtkWidget *symmetry;
    GtkWidget *symmlabel;
    GtkWidget *corrlabel;
    GtkWidget *data_view;
    GwyContainer *data;
    UnrotateArgs *args;
    GwyPlaneSymmetry guess;
    gdouble *correction;
} UnrotateControls;

static gboolean         module_register          (void);
static void             unrotate                 (GwyContainer *data,
                                                  GwyRunType run);
static gboolean         unrotate_dialog          (UnrotateArgs *args,
                                                  GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  gint id,
                                                  gdouble *correction,
                                                  GwyPlaneSymmetry guess);
static void             unrotate_dialog_update   (UnrotateControls *controls,
                                                  UnrotateArgs *args);
static void             unrotate_symmetry_cb     (GtkWidget *combo,
                                                  UnrotateControls *controls);
static void             unrotate_interp_cb       (GtkWidget *combo,
                                                  UnrotateControls *controls);
static void             load_args                (GwyContainer *container,
                                                  UnrotateArgs *args);
static void             save_args                (GwyContainer *container,
                                                  UnrotateArgs *args);
static void             sanitize_args            (UnrotateArgs *args);

static const UnrotateArgs unrotate_defaults = {
    GWY_INTERPOLATION_LINEAR,
    GWY_SYMMETRY_AUTO,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Rotates data to make characteristic directions parallel "
       "with x or y axis."),
    "Yeti <yeti@gwyddion.net>",
    "2.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("unrotate",
                              (GwyProcessFunc)&unrotate,
                              N_("/_Correct Data/_Unrotate..."),
                              GWY_STOCK_UNROTATE,
                              UNROTATE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Automatically correct rotation in "
                                 "horizontal plane"));

    return TRUE;
}

static void
unrotate(GwyContainer *data, GwyRunType run)
{
    enum { nder = 4800 };
    GwyDataField *dfield, *mfield, *sfield;
    UnrotateArgs args;
    gdouble correction[GWY_SYMMETRY_LAST];
    GwyPlaneSymmetry symm;
    GwyDataLine *derdist;
    GQuark dquark, mquark, squark;
    gdouble phi;
    gboolean ok = TRUE;
    gint id;

    g_return_if_fail(run & UNROTATE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     GWY_APP_SHOW_FIELD, &sfield,
                                     0);
    g_return_if_fail(dfield && dquark && mquark && squark);

    load_args(gwy_app_settings_get(), &args);
    derdist = GWY_DATA_LINE(gwy_data_line_new(nder, 2*G_PI, FALSE));
    gwy_data_field_slope_distribution(dfield, derdist, 5);
    symm = gwy_data_field_unrotate_find_corrections(derdist, correction);
    g_object_unref(derdist);

    if (run == GWY_RUN_INTERACTIVE) {
        ok = unrotate_dialog(&args, data, dfield, id, correction, symm);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    if (args.symmetry)
        symm = args.symmetry;
    phi = correction[symm];

    if (!mfield)
        mquark = 0;
    if (!sfield)
        squark = 0;
    if (!mfield && sfield)
        GWY_SWAP(GQuark, mquark, squark);
    gwy_app_undo_qcheckpoint(data, dquark, mquark, squark, 0);
    gwy_data_field_rotate(dfield, phi, args.interp);
    gwy_data_field_data_changed(dfield);
    if (mfield) {
        gwy_data_field_rotate(mfield, phi, args.interp);
        gwy_data_field_data_changed(mfield);
    }
    if (sfield) {
        gwy_data_field_rotate(sfield, phi, args.interp);
        gwy_data_field_data_changed(sfield);
    }
}

/* create a smaller copy of data */
static GwyContainer*
create_preview_data(GwyContainer *data,
                    GwyDataField *dfield,
                    gint id)
{
    GwyContainer *pdata;
    GwyDataField *pfield;
    gint xres, yres;
    gdouble zoomval;

    pdata = gwy_container_new();
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    zoomval = (gdouble)PREVIEW_SIZE/MAX(xres, yres);
    xres = MAX(xres*zoomval, 3);
    yres = MAX(yres*zoomval, 3);
    pfield = gwy_data_field_new_resampled(dfield, xres, yres,
                                          GWY_INTERPOLATION_ROUND);
    gwy_container_set_object_by_name(pdata, "/0/data", pfield);
    g_object_unref(pfield);
    pfield = gwy_data_field_new_alike(pfield, FALSE);
    gwy_container_set_object_by_name(pdata, "/0/show", pfield);
    g_object_unref(pfield);
    gwy_app_sync_data_items(data, pdata, id, 0, FALSE,
                            GWY_DATA_ITEM_GRADIENT, 0);

    return pdata;
}

static gboolean
unrotate_dialog(UnrotateArgs *args,
                GwyContainer *data,
                GwyDataField *dfield,
                gint id,
                gdouble *correction,
                GwyPlaneSymmetry guess)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *label, *hbox;
    GwyPixmapLayer *layer;
    UnrotateControls controls;
    const gchar *s;
    gint response;
    gint row;

    controls.correction = correction;
    controls.guess = guess;
    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Correct Rotation"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    table = gtk_table_new(4, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Structure")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new(_("Detected:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    s = gwy_enum_to_string(guess, gwy_plane_symmetry_get_enum(), -1);
    label = gtk_label_new(_(s));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.symmetry
        = gwy_enum_combo_box_new(gwy_plane_symmetry_get_enum(), -1,
                                 G_CALLBACK(unrotate_symmetry_cb), &controls,
                                 args->symmetry, TRUE);
    gwy_table_attach_row(table, row, _("_Assume:"), NULL, controls.symmetry);
    row++;

    label = gtk_label_new(_("Correction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.corrlabel = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.corrlabel), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.corrlabel,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(unrotate_interp_cb), &controls,
                                 args->interp, TRUE);
    gwy_table_attach_row(table, row, _("_Interpolation type:"), "",
                         controls.interp);

    controls.data = create_preview_data(data, dfield, id);
    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/show");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view), layer);
    gtk_box_pack_start(GTK_BOX(hbox), controls.data_view, FALSE, FALSE, 8);

    unrotate_dialog_update(&controls, args);

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

            case RESPONSE_RESET:
            *args = unrotate_defaults;
            unrotate_dialog_update(&controls, args);
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
unrotate_dialog_update(UnrotateControls *controls,
                       UnrotateArgs *args)
{
    gchar *lab;
    GwyPlaneSymmetry symm;
    GwyDataField *dfield, *rfield;
    GwyContainer *data;
    gdouble phi;

    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->symmetry),
                                  args->symmetry);

    symm = args->symmetry ? args->symmetry : controls->guess;
    phi = controls->correction[symm];
    lab = g_strdup_printf("%.2f %s", 180.0/G_PI*phi, _("deg"));
    gtk_label_set_text(GTK_LABEL(controls->corrlabel), lab);
    g_free(lab);

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->data_view));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    rfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/show"));
    gwy_data_field_copy(dfield, rfield, FALSE);
    gwy_data_field_rotate(rfield, phi, args->interp);
    gwy_data_field_data_changed(rfield);
}

static void
unrotate_symmetry_cb(GtkWidget *combo,
                     UnrotateControls *controls)
{
    controls->args->symmetry
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    unrotate_dialog_update(controls, controls->args);

}

static void
unrotate_interp_cb(GtkWidget *combo,
                   UnrotateControls *controls)
{
    controls->args->interp
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    unrotate_dialog_update(controls, controls->args);
}

static const gchar interp_key[]   = "/module/unrotate/interp";
static const gchar symmetry_key[] = "/module/unrotate/symmetry";

static void
sanitize_args(UnrotateArgs *args)
{
    args->interp = gwy_enum_sanitize_value(args->interp,
                                           GWY_TYPE_INTERPOLATION_TYPE);
    args->symmetry = gwy_enum_sanitize_value(args->symmetry,
                                             GWY_TYPE_PLANE_SYMMETRY);
}

static void
load_args(GwyContainer *container,
          UnrotateArgs *args)
{
    *args = unrotate_defaults;

    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, symmetry_key, &args->symmetry);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          UnrotateArgs *args)
{
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, symmetry_key, args->symmetry);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
