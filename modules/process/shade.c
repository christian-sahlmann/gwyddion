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

#include <math.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/file.h>
#include <app/gwyapp.h>

#define SHADE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)


/* Data for this function.*/
typedef struct {
    gdouble theta;
    gdouble phi;
} ShadeArgs;

typedef struct {
    GtkWidget *gradsphere;
} ShadeControls;

static GwySphereCoords *coords;

static gboolean    module_register            (const gchar *name);
static gboolean    shade                      (GwyContainer *data,
                                               GwyRunType run);
static gboolean    shade_dialog               (ShadeArgs *args);
static void        shade_changed_cb           (ShadeArgs *args);
static void        shade_load_args            (GwyContainer *container,
                                               ShadeArgs *args);
static void        shade_save_args            (GwyContainer *container,
                                               ShadeArgs *args);
static void        shade_dialog_update        (ShadeControls *controls,
                                               ShadeArgs *args);


ShadeArgs shade_defaults = {
    0,
    0,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "shade",
    "Shade module",
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
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
        "/_Display/_Shading...",
        (GwyProcessFunc)&shade,
        SHADE_RUN_MODES,
    };

    gwy_process_func_register(name, &shade_func_info);

    return TRUE;
}

static gboolean
shade(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *shadefield;
    ShadeArgs args;
    gboolean ok;

    g_assert(run & SHADE_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    if (run == GWY_RUN_WITH_DEFAULTS)
    {
        args = shade_defaults;
    }
    else
    {
        shade_load_args(gwy_app_settings_get(), &args);
    }
    ok = (run != GWY_RUN_MODAL) || shade_dialog(&args);
    if (ok) {

        gwy_app_undo_checkpoint(data, "/0/show", NULL);
        if (gwy_container_contains_by_name(data, "/0/show")) {
            shadefield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                         "/0/show"));
            gwy_data_field_resample(shadefield,
                                    gwy_data_field_get_xres(dfield),
                                    gwy_data_field_get_yres(dfield),
                                    GWY_INTERPOLATION_NONE);
        }
        else
        {
            shadefield = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
            gwy_container_set_object_by_name(data, "/0/show", G_OBJECT(shadefield));
        }

        gwy_data_field_shade(dfield, shadefield,
                             args.theta*180/G_PI, args.phi*180/G_PI);


        if (run != GWY_RUN_WITH_DEFAULTS)
            shade_save_args(gwy_app_settings_get(), &args);
    }

    return ok;
}


static gboolean
shade_dialog(ShadeArgs *args)
{
    GtkWidget *dialog;
    GObject *pal, *pdef;
    ShadeControls controls;
    GwySphereCoords *gscoords;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Shading"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    gscoords = GWY_SPHERE_COORDS(gwy_sphere_coords_new(args->theta, args->phi));
    controls.gradsphere = gwy_vector_shade_new(gscoords);

    pdef = gwy_palette_def_new(GWY_PALETTE_GRAY);
    pal = gwy_palette_new(GWY_PALETTE_DEF(pdef));
    gwy_grad_sphere_set_palette(
              GWY_GRAD_SPHERE(gwy_vector_shade_get_grad_sphere(GWY_VECTOR_SHADE(controls.gradsphere))),
              GWY_PALETTE(pal));


    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), controls.gradsphere,
                                              FALSE, FALSE, 4);

    coords = gwy_vector_shade_get_sphere_coords(GWY_VECTOR_SHADE(controls.gradsphere));
    g_signal_connect_swapped(G_OBJECT(coords), "value_changed",
                             G_CALLBACK(shade_changed_cb), args);

/*    controls.out
        = gwy_fft_output_menu(G_CALLBACK(out_changed_cb),
                                        args, args->out);
    gwy_table_attach_row(table, 3, _("Output type:"), "",
                         controls.out);
*/

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
            shade_dialog_update(&controls, args);
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
shade_changed_cb(ShadeArgs *args)
{
    args->theta = gwy_sphere_coords_get_theta(coords);
    args->phi = gwy_sphere_coords_get_phi(coords);


}

static const gchar *theta_key = "/module/shade/theta";
static const gchar *phi_key = "/module/shade/phi";

static void
shade_load_args(GwyContainer *container,
                ShadeArgs *args)
{
    *args = shade_defaults;

    if (gwy_container_contains_by_name(container, theta_key))
        args->theta = gwy_container_get_double_by_name(container, theta_key);
    if (gwy_container_contains_by_name(container, phi_key))
        args->phi = gwy_container_get_double_by_name(container, phi_key);
}

static void
shade_save_args(GwyContainer *container,
                ShadeArgs *args)
{
    gwy_container_set_double_by_name(container, theta_key, args->theta);
    gwy_container_set_double_by_name(container, phi_key, args->phi);
}

static void
shade_dialog_update(ShadeControls *controls,
                    ShadeArgs *args)
{
    GwySphereCoords *sphere_coords;

    sphere_coords = gwy_vector_shade_get_sphere_coords(GWY_VECTOR_SHADE(controls->gradsphere));
    gwy_sphere_coords_set_value(sphere_coords, args->theta, args->phi);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
