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
#include <libprocess/arithmetic.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyshader.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define SHADE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 240
};

typedef struct {
    gdouble theta;
    gdouble phi;
    gdouble mix;
    gboolean do_mix;
} ShadeArgs;

typedef struct {
    ShadeArgs *args;
    GtkWidget *shader;
    GtkObject *theta;
    GtkObject *phi;
    GtkObject *mix;
    GtkWidget *do_mix;
    GtkWidget *data_view;
    GwyContainer *data;
    gboolean in_update;
} ShadeControls;

static gboolean    module_register              (void);
static void        shade                        (GwyContainer *data,
                                                 GwyRunType run);
static gboolean    shade_dialog                 (ShadeArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 gint id);
static void        shade_changed_cb             (GtkWidget *shader,
                                                 ShadeControls *controls);
static void        theta_changed_cb             (GtkObject *adj,
                                                 ShadeControls *controls);
static void        phi_changed_cb               (GtkObject *adj,
                                                 ShadeControls *controls);
static void        mix_changed_cb               (ShadeControls *controls);
static void        shade_dialog_update          (ShadeControls *controls,
                                                 ShadeArgs *args);
static void        shade_mix_with_plane         (GwyDataField *shaded,
                                                 GwyDataField *plane,
                                                 gdouble mixpercent);
static void        load_args                    (GwyContainer *container,
                                                 ShadeArgs *args);
static void        save_args                    (GwyContainer *container,
                                                 ShadeArgs *args);
static void        sanitize_args                (ShadeArgs *args);


static const ShadeArgs shade_defaults = {
    0,
    0,
    0,
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates a shaded presentation of data."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("shade",
                              (GwyProcessFunc)&shade,
                              N_("/_Presentation/_Shading..."),
                              GWY_STOCK_SHADER,
                              SHADE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Shade data"));

    return TRUE;
}

static void
shade(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *shadefield;
    GQuark dquark, squark;
    GwySIUnit *siunit;
    ShadeArgs args;
    gboolean ok;
    gint id;

    g_return_if_fail(run & SHADE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     GWY_APP_SHOW_FIELD, &shadefield,
                                     0);
    g_return_if_fail(dfield && dquark && squark);

    load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = shade_dialog(&args, data, dfield, id);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    gwy_app_undo_qcheckpointv(data, 1, &squark);
    if (!shadefield) {
        shadefield = gwy_data_field_new_alike(dfield, FALSE);
        siunit = gwy_si_unit_new("");
        gwy_data_field_set_si_unit_z(shadefield, siunit);
        g_object_unref(siunit);
        gwy_container_set_object(data, squark, shadefield);
        g_object_unref(shadefield);
    }

    gwy_data_field_shade(dfield, shadefield, args.theta, args.phi);
    if (args.do_mix) {
        shade_mix_with_plane(shadefield, dfield, args.mix);
    }
    gwy_data_field_normalize(shadefield);
    gwy_data_field_data_changed(shadefield);
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
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    return pdata;
}

static gboolean
shade_dialog(ShadeArgs *args,
             GwyContainer *data,
             GwyDataField *dfield,
             gint id)
{
    GtkWidget *dialog, *hbox, *table, *spin;
    GwyPixmapLayer *layer;
    const guchar *pal;
    ShadeControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response, row;

    controls.args = args;
    controls.in_update = TRUE;
    controls.data = create_preview_data(data, dfield, id);

    dialog = gtk_dialog_new_with_buttons(_("Shading"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 4);

    table = gtk_table_new(3, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 0);
    row = 0;

    pal = NULL;
    gwy_container_gis_string_by_name(controls.data, "/0/base/palette", &pal);
    controls.shader = gwy_shader_new(pal);
    gwy_shader_set_angle(GWY_SHADER(controls.shader), args->theta, args->phi);
    gtk_widget_set_size_request(controls.shader, 72, 72);
    g_signal_connect(controls.shader, "angle_changed",
                     G_CALLBACK(shade_changed_cb), &controls);
    gtk_table_attach(GTK_TABLE(table), controls.shader, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.theta = gtk_adjustment_new(args->theta*180.0/G_PI,
                                        0.0, 90.0, 1.0, 15.0, 0.0);
    spin = gwy_table_attach_hscale(table, row, _("θ:"), "deg",
                                   controls.theta, GWY_HSCALE_DEFAULT);
    g_signal_connect(controls.theta, "value-changed",
                     G_CALLBACK(theta_changed_cb), &controls);
    row++;

    controls.phi = gtk_adjustment_new(args->phi*180.0/G_PI,
                                      0.0, 360.0, 1.0, 30.0, 0.0);
    spin = gwy_table_attach_hscale(table, row, _("φ:"), "deg",
                                   controls.phi, GWY_HSCALE_DEFAULT);
    g_signal_connect(controls.phi, "value-changed",
                     G_CALLBACK(phi_changed_cb), &controls);
    row++;

    controls.mix = gtk_adjustment_new(args->mix, 0.0, 100.0, 1, 5, 0);

    spin = gwy_table_attach_hscale(table, row, _("_Mix:"), "%",
                                   controls.mix, GWY_HSCALE_CHECK);

    controls.do_mix = g_object_get_data(G_OBJECT(controls.mix), "check");

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_mix),
                                 !args->do_mix);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_mix),
                                 args->do_mix);
    g_signal_connect_swapped(controls.mix, "value-changed",
                             G_CALLBACK(mix_changed_cb), &controls);

    g_signal_connect_swapped(controls.do_mix, "toggled",
                             G_CALLBACK(mix_changed_cb), &controls);

    row++;
    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/show");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.data_view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view), layer);
    gtk_box_pack_start(GTK_BOX(hbox), controls.data_view, FALSE, FALSE, 8);

    controls.in_update = FALSE;
    shade_dialog_update(&controls, args);

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
            *args = shade_defaults;
            gtk_adjustment_set_value(GTK_ADJUSTMENT(controls.mix), args->mix);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_mix),
                                         !args->do_mix);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_mix),
                                         args->do_mix);
            gwy_shader_set_angle(GWY_SHADER(controls.shader),
                                 args->theta, args->phi);
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
shade_changed_cb(GtkWidget *shader,
                 ShadeControls *controls)
{
    ShadeArgs *args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args = controls->args;
    args->theta = gwy_shader_get_theta(GWY_SHADER(shader));
    args->phi = gwy_shader_get_phi(GWY_SHADER(shader));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->theta),
                             args->theta*180.0/G_PI);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->phi),
                             args->phi*180.0/G_PI);
    shade_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
theta_changed_cb(GtkObject *adj,
                 ShadeControls *controls)
{
    ShadeArgs *args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args = controls->args;
    args->theta = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj))/180.0*G_PI;
    gwy_shader_set_theta(GWY_SHADER(controls->shader), args->theta);
    shade_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
phi_changed_cb(GtkObject *adj,
               ShadeControls *controls)
{
    ShadeArgs *args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args = controls->args;
    args->phi = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj))/180.0*G_PI;
    gwy_shader_set_phi(GWY_SHADER(controls->shader), args->phi);
    shade_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
mix_changed_cb(ShadeControls *controls)
{
    ShadeArgs *args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args = controls->args;
    args->mix = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->mix));
    args->do_mix = gtk_toggle_button_get_active(
                                         GTK_TOGGLE_BUTTON(controls->do_mix));
    shade_dialog_update(controls, args);
    controls->in_update = FALSE;
}


static void
shade_dialog_update(ShadeControls *controls,
                    ShadeArgs *args)
{
    GwyDataField *dfield, *shader;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->data,
                                                             "/0/data"));
    shader = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->data,
                                                             "/0/show"));
    gwy_data_field_shade(dfield, shader, args->theta, args->phi);

    if (args->do_mix) {
        shade_mix_with_plane(shader, dfield, args->mix);
    }

    gwy_data_field_data_changed(shader);
}

static void
shade_mix_with_plane(GwyDataField *shaded,
                     GwyDataField *plane,
                     gdouble mixpercent)
{
    GwyDataField *mix;
    gdouble plane_min, plane_max, shade_min, shade_max;
    gdouble plane_range, shade_range;

    mixpercent /= 100;

    gwy_data_field_get_min_max(plane, &plane_min, &plane_max);
    gwy_data_field_get_min_max(shaded, &shade_min, &shade_max);
    plane_range = plane_max - plane_min;
    shade_range = shade_max - shade_min;

    gwy_data_field_multiply(shaded, (1 - mixpercent)*(plane_range/shade_range));

    mix = gwy_data_field_duplicate(plane);
    gwy_data_field_multiply(mix, mixpercent);
    gwy_data_field_sum_fields(shaded, shaded, mix);

    g_object_unref(mix);

}

static const gchar theta_key[]  = "/module/shade/theta";
static const gchar phi_key[]    = "/module/shade/phi";
static const gchar mix_key[]    = "/module/shade/mix";
static const gchar do_mix_key[] = "/module/shade/do_mix";

static void
sanitize_args(ShadeArgs *args)
{
    args->theta = CLAMP(args->theta, 0.0, G_PI/2.0);
    args->phi = CLAMP(args->phi, 0.0, 2.0*G_PI);
    args->mix = CLAMP(args->mix, 0.0, 100.0);
}

static void
load_args(GwyContainer *container,
          ShadeArgs *args)
{
    *args = shade_defaults;

    gwy_container_gis_double_by_name(container, theta_key, &args->theta);
    gwy_container_gis_double_by_name(container, phi_key, &args->phi);
    gwy_container_gis_double_by_name(container, mix_key, &args->mix);
    gwy_container_gis_boolean_by_name(container, do_mix_key, &args->do_mix);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          ShadeArgs *args)
{
    gwy_container_set_double_by_name(container, theta_key, args->theta);
    gwy_container_set_double_by_name(container, phi_key, args->phi);
    gwy_container_set_double_by_name(container, mix_key, args->mix);
    gwy_container_set_boolean_by_name(container, do_mix_key, args->do_mix);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
