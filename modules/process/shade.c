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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
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
    GtkWidget *data_view;
    GwyContainer *data;
    ShadeArgs *args;
} ShadeControls;

static gboolean    module_register              (const gchar *name);
static gboolean    shade                        (GwyContainer *data,
                                                 GwyRunType run);
static gboolean    shade_dialog                 (ShadeArgs *args,
                                                 GwyContainer *data);
static void        shade_changed_cb             (GtkObject *coords,
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
    "shade",
    N_("Shade module"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.2",
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
    GObject *shadefield;
    GwyDataField *dfield;
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
        if (gwy_container_gis_object_by_name(data, "/0/show", &shadefield)) {
            gwy_data_field_resample(GWY_DATA_FIELD(shadefield),
                                    gwy_data_field_get_xres(dfield),
                                    gwy_data_field_get_yres(dfield),
                                    GWY_INTERPOLATION_NONE);
        }
        else {
            shadefield = gwy_serializable_duplicate(G_OBJECT(dfield));
            gwy_container_set_object_by_name(data, "/0/show", shadefield);
            g_object_unref(shadefield);
        }

        gwy_data_field_shade(dfield, GWY_DATA_FIELD(shadefield),
                             args.theta*180/G_PI, args.phi*180/G_PI);
    }

    return ok;
}

/* create a smaller copy of data */
static GwyContainer*
create_preview_data(GwyContainer *data)
{
    GwyContainer *preview;
    GObject *dfield;
    gint xres, yres;
    gdouble zoomval;

    preview = GWY_CONTAINER(gwy_container_new());
    dfield = gwy_container_get_object_by_name(data, "/0/data");
    dfield = gwy_serializable_duplicate(dfield);
    xres = gwy_data_field_get_xres(GWY_DATA_FIELD(dfield));
    yres = gwy_data_field_get_yres(GWY_DATA_FIELD(dfield));
    zoomval = 120.0/MAX(xres, yres);
    gwy_data_field_resample(GWY_DATA_FIELD(dfield), xres*zoomval, yres*zoomval,
                            GWY_INTERPOLATION_BILINEAR);
    gwy_container_set_object_by_name(preview, "/0/data", dfield);
    g_object_unref(dfield);
    dfield = gwy_serializable_duplicate(dfield);
    gwy_container_set_object_by_name(preview, "/0/show", dfield);
    g_object_unref(dfield);

    return preview;
}

static gboolean
shade_dialog(ShadeArgs *args,
             GwyContainer *data)
{
    GtkWidget *dialog, *hbox;
    GtkObject *layer, *coords;
    const gchar *palette;
    GObject *pal, *pdef;
    ShadeControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Shading"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    coords = gwy_sphere_coords_new(args->theta, args->phi);
    controls.gradsphere = gwy_vector_shade_new(GWY_SPHERE_COORDS(coords));

    palette = gwy_container_get_string_by_name(data, "/0/base/palette");
    pdef = gwy_palette_def_new(palette);
    pal = gwy_palette_new(GWY_PALETTE_DEF(pdef));
    gwy_grad_sphere_set_palette(
              GWY_GRAD_SPHERE(gwy_vector_shade_get_grad_sphere
                              (GWY_VECTOR_SHADE(controls.gradsphere))),
              GWY_PALETTE(pal));
    g_object_unref(pdef);
    g_object_unref(pal);
    g_signal_connect(coords, "value_changed",
                     G_CALLBACK(shade_changed_cb), &controls);
    gtk_box_pack_start(GTK_BOX(hbox), controls.gradsphere, FALSE, FALSE, 0);

    controls.data = create_preview_data(data);
    gwy_container_set_string_by_name(controls.data, "/0/base/palette",
                                     g_strdup(palette));
    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view),
                                 GWY_PIXMAP_LAYER(layer));
    gtk_box_pack_start(GTK_BOX(hbox), controls.data_view, FALSE, FALSE, 0);

    shade_changed_cb(coords, &controls);

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
shade_changed_cb(GtkObject *coords,
                 ShadeControls *controls)
{
    ShadeArgs *args;
    GObject *dfield, *shader;

    args = controls->args;
    args->theta = gwy_sphere_coords_get_theta(GWY_SPHERE_COORDS(coords));
    args->phi = gwy_sphere_coords_get_phi(GWY_SPHERE_COORDS(coords));
    dfield = gwy_container_get_object_by_name(controls->data, "/0/data");
    shader = gwy_container_get_object_by_name(controls->data, "/0/show");
    gwy_data_field_shade(GWY_DATA_FIELD(dfield), GWY_DATA_FIELD(shader),
                         args->theta*180.0/G_PI, args->phi*180.0/G_PI);
    gwy_data_view_update(GWY_DATA_VIEW(controls->data_view));
}

static void
shade_dialog_update(ShadeControls *controls,
                    ShadeArgs *args)
{
    GwySphereCoords *sphere_coords;

    sphere_coords = gwy_vector_shade_get_sphere_coords
                        (GWY_VECTOR_SHADE(controls->gradsphere));
    gwy_sphere_coords_set_value(sphere_coords, args->theta, args->phi);
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
