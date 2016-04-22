/*
 *  @(#) $Id: volumize_layers.c 14879 2013-04-15 21:04:16Z yeti-dn $
 *  Copyright (C) 2015-2016 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/brick.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include "preview.h"

#define VOLUMIZE_LAYERS_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)
#define MAXPIX 600

typedef struct {
    gint xres;
    gint yres;
    gint zres;
    gdouble zreal;
    gchar *zunit;
} VolumizeLayersArgs;

typedef struct {
    VolumizeLayersArgs *args;
    GtkObject *xres;
    GtkObject *yres;
    GtkObject *zres;
    GtkObject *zreal;
} VolumizeLayersControls;

static gboolean module_register              (void);
static void     volumize_layers              (GwyContainer *data,
                                              GwyRunType run);
static gboolean volumize_layers_dialog       (VolumizeLayersArgs *args,
                                              gint xres,
                                              gint yres,
                                              gint zres);
static void     change_zunits                (VolumizeLayersControls *controls);
static void     update_unit_label            (VolumizeLayersControls *controls);
static void     volumize_layers_dialog_update(VolumizeLayersControls *controls,
                                              VolumizeLayersArgs *args);
static void     volumize_layers_load_args    (GwyContainer *container,
                                              VolumizeLayersArgs *args);
static void     volumize_layers_save_args    (GwyContainer *container,
                                              VolumizeLayersArgs *args);

static const VolumizeLayersArgs volumize_layers_defaults = {
    100,
    100,
    100,
    1e-6,
    NULL,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Converts all datafields to 3D volume data."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("volumize_layers",
                              (GwyProcessFunc)&volumize_layers,
                              N_("/_Basic Operations/Volumize Layers..."),
                              GWY_STOCK_VOLUMIZE_LAYERS,
                              VOLUMIZE_LAYERS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Convert all datafields to 3D data"));

    return TRUE;
}

static void
volumize_layers(GwyContainer *data, GwyRunType run)
{
    VolumizeLayersArgs args;
    GwyDataField *dfield = NULL;
    GtkWidget *dialog;
    GwyBrick *brick;
    GwySIUnit *siunit;
    gboolean ok = TRUE;
    gint *ids, col, row, i, nids, xres, yres, newid, power10;
    gdouble *ddata, *bdata;

    g_return_if_fail(run & VOLUMIZE_LAYERS_RUN_MODES);

    ids = gwy_app_data_browser_get_data_ids(data);

    volumize_layers_load_args(gwy_app_settings_get(), &args);

    nids = 1;
    dfield = gwy_container_get_object(data,
                                      gwy_app_get_data_key_for_id(ids[0]));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    i = 0;
    while (ids[i] != -1) {
        dfield = gwy_container_get_object(data,
                                          gwy_app_get_data_key_for_id(ids[i]));
        if (xres != gwy_data_field_get_xres(dfield)
            || yres != gwy_data_field_get_yres(dfield)) {
            ok = FALSE;
            break;
        }

        i++;
        nids++;
    }

    if (!ok) {
        dialog = gtk_message_dialog_new(gwy_app_find_window_for_channel(data,
                                                                        ids[0]),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         _("All datafields must have same "
                                           "resolution to make a volume from "
                                           "them."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        goto finish;
    }

    args.xres = xres;
    args.yres = yres;
    args.zres = nids-1;
    if (run == GWY_RUN_INTERACTIVE) {
        ok = volumize_layers_dialog(&args, xres, yres, nids-1);
        volumize_layers_save_args(gwy_app_settings_get(), &args);
    }
    if (!ok)
        goto finish;

    siunit = gwy_si_unit_new_parse(args.zunit, &power10);
    brick = gwy_brick_new(xres, yres, nids-1,
                          gwy_data_field_get_xreal(dfield),
                          gwy_data_field_get_yreal(dfield),
                          args.zreal * pow10(power10),
                          FALSE);
    bdata = gwy_brick_get_data(brick);
    for (i = 0; i < nids-1; i++) {
        dfield = gwy_container_get_object(data,
                                          gwy_app_get_data_key_for_id(ids[i]));
        ddata = gwy_data_field_get_data(dfield);

        for (row = 0; row < yres; row++) {
            for (col = 0; col < xres; col++)
                bdata[col + xres*row + xres*yres*i] = ddata[col + xres*row];
        }
    }

    gwy_brick_resample(brick, args.xres, args.yres, args.zres,
                       GWY_INTERPOLATION_ROUND);
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_xy(dfield)),
                           G_OBJECT(gwy_brick_get_si_unit_x(brick)));
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_xy(dfield)),
                           G_OBJECT(gwy_brick_get_si_unit_y(brick)));
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_z(dfield)),
                           G_OBJECT(gwy_brick_get_si_unit_w(brick)));
    gwy_brick_set_si_unit_z(brick, siunit);
    g_object_unref(siunit);

    newid = gwy_app_data_browser_add_brick(brick, dfield, data, TRUE);
    g_object_unref(brick);
    gwy_app_volume_log_add(data, -1, newid, "proc::volumize_layers", NULL);

finish:
    g_free(ids);
    g_free(args.zunit);
}

static gboolean
volumize_layers_dialog(VolumizeLayersArgs *args,
                       gint xres, gint yres, gint zres)
{
    GtkWidget *dialog, *table, *zunitbutton;
    VolumizeLayersControls controls;
    gint response, row = 0;

    gwy_clear(&controls, 1);
    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Volumize layers"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    table = gtk_table_new(4, 5, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    args->xres = xres;
    args->yres = yres;
    args->zres = zres;

    controls.xres = gtk_adjustment_new(args->xres, 1.0, 1000.0, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_X resolution:"), "pixels",
                            controls.xres, GWY_HSCALE_SQRT);
    row++;

    controls.yres = gtk_adjustment_new(args->yres, 1.0, 1000.0, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Y resolution:"), "pixels",
                            controls.yres, GWY_HSCALE_SQRT);
    row++;

    controls.zres = gtk_adjustment_new(args->zres, 1.0, 1000.0, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Z resolution:"), "pixels",
                            controls.zres, GWY_HSCALE_SQRT);
    row++;

    controls.zreal = gtk_adjustment_new(args->zreal,
                                        0.01, 10000.0, 0.01, 1.0, 0);
    gwy_table_attach_hscale(table, row, _("Z _range:"), args->zunit,
                            controls.zreal, GWY_HSCALE_SQRT);

    zunitbutton = gtk_button_new_with_label(gwy_sgettext("verb|Change"));
    gtk_table_attach(GTK_TABLE(table), zunitbutton,
                     4, 5, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
    g_signal_connect_swapped(zunitbutton, "clicked",
                             G_CALLBACK(change_zunits), &controls);
    row++;

    update_unit_label(&controls);
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
            args->xres = xres;  /* go back to initially detected resolution */
            args->yres = yres;
            args->zres = zres;
            args->zreal = volumize_layers_defaults.zreal;
            g_free(args->zunit);
            args->zunit = g_strdup("");
            volumize_layers_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->xres = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.xres));
    args->yres = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.yres));
    args->zres = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.zres));
    args->zreal = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.zreal));

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
change_zunits(VolumizeLayersControls *controls)
{
    GtkWidget *dialog, *hbox, *label, *entry;
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Change Units"),
                                         NULL,
                                         GTK_DIALOG_MODAL
                                         | GTK_DIALOG_NO_SEPARATOR,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("New _units:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    g_free(controls->args->zunit);
    controls->args->zunit = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    gtk_widget_destroy(dialog);

    update_unit_label(controls);
}

static void
update_unit_label(VolumizeLayersControls *controls)
{
    GtkWidget *label;
    GwySIUnit *siunit;
    gint power10;
    GwySIValueFormat *vf;

    siunit = gwy_si_unit_new_parse(controls->args->zunit, &power10);
    vf = gwy_si_unit_get_format_for_power10(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            power10, NULL);
    label = gwy_table_hscale_get_units(controls->zreal);
    gtk_label_set_markup(GTK_LABEL(label), vf->units);
    gwy_si_unit_value_format_free(vf);
    g_object_unref(siunit);
}

static void
volumize_layers_dialog_update(VolumizeLayersControls *controls,
                              VolumizeLayersArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xres), args->xres);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yres), args->yres);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zres), args->zres);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zreal), args->zreal);
    update_unit_label(controls);
}

static const gchar zreal_key[] = "/module/volumize_layers/zreal";
static const gchar zunit_key[] = "/module/volumize_layers/zunit";

static void
volumize_layers_load_args(GwyContainer *container,
                          VolumizeLayersArgs *args)
{
    const guchar *s;

    *args = volumize_layers_defaults;

    args->zunit = g_strdup("");
    gwy_container_gis_double_by_name(container, zreal_key, &args->zreal);
    if (gwy_container_gis_string_by_name(container, zunit_key, &s)) {
        g_free(args->zunit);
        args->zunit = g_strdup(s);
    }
}

static void
volumize_layers_save_args(GwyContainer *container,
                          VolumizeLayersArgs *args)
{
    gwy_container_set_double_by_name(container, zreal_key, args->zreal);
    gwy_container_set_const_string_by_name(container, zunit_key, args->zunit);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
