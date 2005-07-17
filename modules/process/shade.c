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
#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/gwyapp.h>

#define SHADE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

enum {
    PREVIEW_SIZE = 120
};

/* Data for this function.*/
typedef struct {
    gdouble theta;
    gdouble phi;
} ShadeArgs;

typedef struct {
    GtkWidget *shader;
    GtkObject *theta;
    GtkObject *phi;
    GtkWidget *data_view;
    GwyContainer *data;
    ShadeArgs *args;
    gboolean in_update;
} ShadeControls;

static gboolean    module_register              (const gchar *name);
static gboolean    shade                        (GwyContainer *data,
                                                 GwyRunType run);
static gboolean    shade_dialog                 (ShadeArgs *args,
                                                 GwyContainer *data);
static void        shade_changed_cb             (GtkWidget *shader,
                                                 ShadeControls *controls);
static void        theta_changed_cb             (GtkObject *adj,
                                                 ShadeControls *controls);
static void        phi_changed_cb               (GtkObject *adj,
                                                 ShadeControls *controls);
static void        shade_dialog_update          (ShadeControls *controls,
                                                 ShadeArgs *args);
static void        shade_load_args              (GwyContainer *container,
                                                 ShadeArgs *args);
static void        shade_save_args              (GwyContainer *container,
                                                 ShadeArgs *args);
static void        shade_sanitize_args          (ShadeArgs *args);


ShadeArgs shade_defaults = {
    0,
    0,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates a shaded presentation of data."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo shade_func_info = {
        "shade",
        N_("/_Display/_Shading..."),
        (GwyProcessFunc)&shade,
        SHADE_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &shade_func_info);

    return TRUE;
}

static gboolean
shade(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *shadefield;
    GwySIUnit *siunit;
    ShadeArgs args;
    gboolean ok;

    g_assert(run & SHADE_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    if (run == GWY_RUN_WITH_DEFAULTS)
        args = shade_defaults;
    else
        shade_load_args(gwy_app_settings_get(), &args);

    ok = (run != GWY_RUN_MODAL) || shade_dialog(&args, data);
    if (run == GWY_RUN_MODAL)
        shade_save_args(gwy_app_settings_get(), &args);
    if (ok) {
        gwy_app_undo_checkpoint(data, "/0/show", NULL);
        siunit = gwy_si_unit_new("");
        if (gwy_container_gis_object_by_name(data, "/0/show", &shadefield)) {
            gwy_data_field_resample(shadefield,
                                    gwy_data_field_get_xres(dfield),
                                    gwy_data_field_get_yres(dfield),
                                    GWY_INTERPOLATION_NONE);
            gwy_data_field_set_si_unit_z(shadefield, siunit);
        }
        else {
            shadefield = gwy_data_field_new_alike(dfield, FALSE);
            gwy_data_field_set_si_unit_z(shadefield, siunit);
            gwy_container_set_object_by_name(data, "/0/show", shadefield);
            g_object_unref(shadefield);
        }
        g_object_unref(siunit);

        gwy_data_field_shade(dfield, shadefield, args.theta, args.phi);
        gwy_data_field_normalize(shadefield);
        gwy_data_field_data_changed(shadefield);
    }

    return ok;
}

/* create a smaller copy of data */
static GwyContainer*
create_preview_data(GwyContainer *data)
{
    GwyContainer *preview;
    GwyDataField *dfield;
    gint xres, yres;
    gdouble zoomval;

    preview = gwy_container_duplicate_by_prefix(data,
                                                "/0/data",
                                                "/0/base/palette",
                                                NULL);
    dfield = gwy_container_get_object_by_name(preview, "/0/data");
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    zoomval = (gdouble)PREVIEW_SIZE/MAX(xres, yres);
    gwy_data_field_resample(dfield, xres*zoomval, yres*zoomval,
                            GWY_INTERPOLATION_BILINEAR);
    dfield = gwy_data_field_duplicate(dfield);
    gwy_container_set_object_by_name(preview, "/0/show", dfield);
    g_object_unref(dfield);

    return preview;
}

static gboolean
shade_dialog(ShadeArgs *args,
             GwyContainer *data)
{
    GtkWidget *dialog, *hbox, *table, *spin;
    GwyPixmapLayer *layer;
    const guchar *palette;
    ShadeControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response, row;

    controls.args = args;
    controls.in_update = TRUE;

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

    palette = GWY_GRADIENT_DEFAULT;
    gwy_container_gis_string_by_name(data, "/0/base/palette", &palette);
    controls.shader = gwy_shader_new(palette);
    gwy_shader_set_angle(GWY_SHADER(controls.shader), args->theta, args->phi);
    gtk_widget_set_size_request(controls.shader, 72, 72);
    g_signal_connect(controls.shader, "angle_changed",
                     G_CALLBACK(shade_changed_cb), &controls);
    gtk_table_attach(GTK_TABLE(table), controls.shader, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.theta = gtk_adjustment_new(args->theta*180.0/G_PI,
                                        0.0, 90.0, 5.0, 15.0, 0.0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Theta:"), "deg",
                                       controls.theta);
    g_signal_connect(controls.theta, "value-changed",
                     G_CALLBACK(theta_changed_cb), &controls);
    row++;

    controls.phi = gtk_adjustment_new(args->phi*180.0/G_PI,
                                      0.0, 360.0, 5.0, 30.0, 0.0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Phi:"), "deg",
                                       controls.phi);
    g_signal_connect(controls.phi, "value-changed",
                     G_CALLBACK(phi_changed_cb), &controls);
    row++;

    controls.data = create_preview_data(data);
    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/show");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
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
shade_dialog_update(ShadeControls *controls,
                    ShadeArgs *args)
{
    GwyDataField *dfield, *shader;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->data,
                                                             "/0/data"));
    shader = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->data,
                                                             "/0/show"));
    gwy_data_field_shade(dfield, shader, args->theta, args->phi);
    gwy_data_field_data_changed(shader);
}

static const gchar *theta_key = "/module/shade/theta";
static const gchar *phi_key = "/module/shade/phi";

static void
shade_sanitize_args(ShadeArgs *args)
{
    args->theta = CLAMP(args->theta, 0.0, G_PI/2.0);
    args->phi = CLAMP(args->phi, 0.0, 2.0*G_PI);
}

static void
shade_load_args(GwyContainer *container,
                ShadeArgs *args)
{
    *args = shade_defaults;

    gwy_container_gis_double_by_name(container, theta_key, &args->theta);
    gwy_container_gis_double_by_name(container, phi_key, &args->phi);
    shade_sanitize_args(args);
}

static void
shade_save_args(GwyContainer *container,
                ShadeArgs *args)
{
    gwy_container_set_double_by_name(container, theta_key, args->theta);
    gwy_container_set_double_by_name(container, phi_key, args->phi);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
