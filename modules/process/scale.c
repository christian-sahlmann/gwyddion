/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define SCALE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    gdouble ratio;
    GwyInterpolationType interp;
    /* interface only */
    gint org_xres;
    gint org_yres;
    gboolean proportional;
    gint xres;
    gint yres;
    gdouble aspectratio;
} ScaleArgs;

typedef struct {
    GtkObject *ratio;
    GtkWidget *interp;
    /* interface only */
    GtkObject *xres;
    GtkObject *yres;
    GtkWidget *proportional;
    gboolean in_update;
} ScaleControls;

static gboolean    module_register           (void);
static void        scale                     (GwyContainer *data,
                                              GwyRunType run);
static gboolean    scale_dialog              (ScaleArgs *args);
static void        proportional_changed_cb   (GtkToggleButton *check_button,
                                              ScaleArgs *args);
static void        scale_changed_cb          (GtkAdjustment *adj,
                                              ScaleArgs *args);
static void        width_changed_cb          (GtkAdjustment *adj,
                                              ScaleArgs *args);
static void        height_changed_cb         (GtkAdjustment *adj,
                                              ScaleArgs *args);
static void        scale_dialog_update       (ScaleControls *controls,
                                              ScaleArgs *args);
static void        scale_sanitize_args       (ScaleArgs *args);
static void        scale_load_args           (GwyContainer *container,
                                              ScaleArgs *args);
static void        scale_save_args           (GwyContainer *container,
                                              ScaleArgs *args);

static const ScaleArgs scale_defaults = {
    1.0,
    GWY_INTERPOLATION_LINEAR,
    0,
    0,
    TRUE,
    0,
    0,
    1.0
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Scales data by arbitrary factor."),
    "Yeti <yeti@gwyddion.net>",
    "1.4",
    "David Nečas (Yeti) & Petr Klapetek & Dirk Kähler",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("scale",
                              (GwyProcessFunc)&scale,
                              N_("/_Basic Operations/Scale..."),
                              GWY_STOCK_SCALE,
                              SCALE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Scale data"));

    return TRUE;
}

static void
scale(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfields[3];
    GQuark quark;
    gint oldid, newid;
    ScaleArgs args;
    gboolean ok;

    g_return_if_fail(run & SCALE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, dfields + 0,
                                     GWY_APP_MASK_FIELD, dfields + 1,
                                     GWY_APP_SHOW_FIELD, dfields + 2,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfields[0]);

    scale_load_args(gwy_app_settings_get(), &args);
    args.org_xres = gwy_data_field_get_xres(dfields[0]);
    args.org_yres = gwy_data_field_get_yres(dfields[0]);
    args.xres = GWY_ROUND(args.ratio*args.org_xres);
    if (args.proportional)
        args.aspectratio = 1.0;
    args.yres = GWY_ROUND(args.aspectratio*args.ratio*args.org_yres);

    if (run == GWY_RUN_INTERACTIVE) {
        ok = scale_dialog(&args);
        scale_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    dfields[0] = gwy_data_field_new_resampled(dfields[0],
                                              GWY_ROUND(args.xres),
                                              GWY_ROUND(args.yres),
                                              args.interp);
    if (dfields[1]) {
        dfields[1] = gwy_data_field_new_resampled(dfields[1],
                                                  GWY_ROUND(args.xres),
                                                  GWY_ROUND(args.yres),
                                                  args.interp);
    }
    if (dfields[2]) {
        dfields[2] = gwy_data_field_new_resampled(dfields[2],
                                                  GWY_ROUND(args.xres),
                                                  GWY_ROUND(args.yres),
                                                  args.interp);
    }

    newid = gwy_app_data_browser_add_data_field(dfields[0], data, TRUE);
    g_object_unref(dfields[0]);
    gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    if (dfields[1]) {
        quark = gwy_app_get_mask_key_for_id(newid);
        gwy_container_set_object(data, quark, dfields[1]);
        g_object_unref(dfields[1]);
    }
    if (dfields[2]) {
        quark = gwy_app_get_show_key_for_id(newid);
        gwy_container_set_object(data, quark, dfields[2]);
        g_object_unref(dfields[2]);
    }

    gwy_app_set_data_field_title(data, newid, _("Scaled Data"));
}

static gboolean
scale_dialog(ScaleArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    ScaleControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Scale"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 5, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.ratio = gtk_adjustment_new(args->ratio,
                                        2.0/MIN(args->xres, args->yres),
                                        4096.0/MAX(args->xres, args->yres),
                                        0.01, 0.2, 0);
    spin = gwy_table_attach_hscale(table, 0, _("Scale by _ratio:"), NULL,
                                   controls.ratio, GWY_HSCALE_LOG);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    g_object_set_data(G_OBJECT(controls.ratio), "controls", &controls);
    g_signal_connect(controls.ratio, "value-changed",
                     G_CALLBACK(scale_changed_cb), args);

    controls.proportional
        = gtk_check_button_new_with_mnemonic(_("_proportional"));
    gtk_table_attach_defaults(GTK_TABLE(table), controls.proportional,
                              3, 4, 0, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.proportional),
                                 args->proportional);
    g_object_set_data(G_OBJECT(controls.proportional), "controls", &controls);
    g_signal_connect(controls.proportional, "toggled",
                     G_CALLBACK(proportional_changed_cb), args);

    controls.xres = gtk_adjustment_new(args->ratio*args->xres,
                                       2, 4096, 1, 10, 0);
    spin = gwy_table_attach_hscale(table, 1, _("New _width:"), "px",
                                   controls.xres, GWY_HSCALE_LOG);
    g_object_set_data(G_OBJECT(controls.xres), "controls", &controls);
    g_signal_connect(controls.xres, "value-changed",
                     G_CALLBACK(width_changed_cb), args);

    controls.yres = gtk_adjustment_new(args->ratio*args->yres,
                                       2, 4096, 1, 10, 0);
    spin = gwy_table_attach_hscale(table, 2, _("New _height:"), "px",
                                   controls.yres, GWY_HSCALE_LOG);
    g_object_set_data(G_OBJECT(controls.yres), "controls", &controls);
    g_signal_connect(controls.yres, "value-changed",
                     G_CALLBACK(height_changed_cb), args);

    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->interp, args->interp, TRUE);
    gwy_table_attach_hscale(table, 3, _("_Interpolation type:"), NULL,
                            GTK_OBJECT(controls.interp), GWY_HSCALE_WIDGET);

    controls.in_update = FALSE;
    scale_dialog_update(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->ratio
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.ratio));
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            args->ratio = scale_defaults.ratio;
            args->xres = args->org_xres;
            args->yres = args->org_yres;
            args->proportional = scale_defaults.proportional;
            args->aspectratio = scale_defaults.aspectratio;
            args->interp = scale_defaults.interp;
            scale_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->ratio = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.ratio));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
proportional_changed_cb(GtkToggleButton *check_button,
                        ScaleArgs *args)
{
    ScaleControls *controls;

    controls = g_object_get_data(G_OBJECT(check_button), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->proportional = gtk_toggle_button_get_active(check_button);
    if (args->proportional) {
        args->xres = GWY_ROUND(args->ratio*args->org_xres);
        args->yres = GWY_ROUND(args->ratio*args->org_yres);
    }
    scale_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
scale_changed_cb(GtkAdjustment *adj,
                 ScaleArgs *args)
{
    ScaleControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->ratio = gtk_adjustment_get_value(adj);
    /* occurs only if args->proportional */
    args->xres = GWY_ROUND(args->ratio*args->org_xres);
    args->yres = GWY_ROUND(args->ratio*args->org_yres);
    scale_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
width_changed_cb(GtkAdjustment *adj,
                 ScaleArgs *args)
{
    ScaleControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;

    args->xres = gtk_adjustment_get_value(adj);
    if (args->proportional) {
        args->ratio = (gdouble)args->xres/args->org_xres;
        args->yres = GWY_ROUND(args->ratio*args->org_yres);
    }
    scale_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
height_changed_cb(GtkAdjustment *adj,
                  ScaleArgs *args)
{
    ScaleControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->yres = gtk_adjustment_get_value(adj);
    if (args->proportional) {
        args->ratio = (gdouble)args->yres/args->org_yres;
        args->xres = GWY_ROUND(args->ratio*args->org_xres);
    }
    scale_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
scale_dialog_update(ScaleControls *controls,
                    ScaleArgs *args)
{
    args->aspectratio = (gdouble)args->yres/args->org_yres
                        *(gdouble)args->org_xres/args->xres;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xres),
                             GWY_ROUND(args->xres));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yres),
                             GWY_ROUND(args->yres));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ratio),
                             args->ratio);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->proportional),
                                 args->proportional);
    /* deactivate Ratio */
    gwy_table_hscale_set_sensitive(controls->ratio,args->proportional);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
}

static const gchar ratio_key[]        = "/module/scale/ratio";
static const gchar interp_key[]       = "/module/scale/interp";
static const gchar proportional_key[] = "/module/scale/proportional";
static const gchar aspectratio_key[]  = "/module/scale/aspectratio";

static void
scale_sanitize_args(ScaleArgs *args)
{
    args->ratio = CLAMP(args->ratio, 0.001, 100.0);
    args->interp = gwy_enum_sanitize_value(args->interp,
                                           GWY_TYPE_INTERPOLATION_TYPE);
    args->proportional = !!args->proportional;
    if (args->aspectratio <= 0.0)
        args->aspectratio = 1.0;
}

static void
scale_load_args(GwyContainer *container,
                ScaleArgs *args)
{
    *args = scale_defaults;

    gwy_container_gis_double_by_name(container, ratio_key, &args->ratio);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, proportional_key,
                                   &args->proportional);
    gwy_container_gis_double_by_name(container, aspectratio_key,
                                     &args->aspectratio);
    scale_sanitize_args(args);
}

static void
scale_save_args(GwyContainer *container,
                ScaleArgs *args)
{
    gwy_container_set_enum_by_name(container, proportional_key,
                                   args->proportional);
    gwy_container_set_double_by_name(container, ratio_key, args->ratio);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_double_by_name(container, aspectratio_key,
                                     args->aspectratio);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
