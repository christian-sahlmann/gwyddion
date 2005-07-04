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
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define ROTATE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

enum {
    PREVIEW_SIZE = 120
};

/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
typedef struct {
    gdouble angle;
    GwyInterpolationType interp;
    gboolean expand;
} RotateArgs;

typedef struct {
    GtkObject *angle;
    GtkWidget *interp;
    GtkWidget *expand;
    GtkWidget *data_view;
    GwyContainer *data;
    RotateArgs *args;
} RotateControls;

static gboolean    module_register            (const gchar *name);
static gboolean    rotate                     (GwyContainer *data,
                                               GwyRunType run);
static gboolean    rotate_dialog              (RotateArgs *args,
                                               GwyContainer *data);
static void        interp_changed_cb          (GObject *item,
                                               RotateControls *controls);
static void        expand_changed_cb          (GtkWidget *toggle,
                                               RotateControls *controls);
static void        angle_changed_cb           (GtkObject *angle,
                                               RotateControls *controls);
static void        rotate_preview_draw        (RotateControls *controls,
                                               RotateArgs *args);
static void        rotate_dialog_update       (RotateControls *controls,
                                               RotateArgs *args);
static void        rotate_sanitize_args       (RotateArgs *args);
static void        rotate_load_args           (GwyContainer *container,
                                               RotateArgs *args);
static void        rotate_save_args           (GwyContainer *container,
                                               RotateArgs *args);

RotateArgs rotate_defaults = {
    0.0,
    GWY_INTERPOLATION_BILINEAR,
    FALSE,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Rotates data by arbitrary angle."),
    "Yeti <yeti@gwyddion.net>",
    "1.7",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo rotate_func_info = {
        "rotate",
        N_("/_Basic Operations/Rotate by _Angle..."),
        (GwyProcessFunc)&rotate,
        ROTATE_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &rotate_func_info);

    return TRUE;
}

static void
rotate_datafield(GwyDataField *dfield,
                 RotateArgs *args)
{
    gint xres, yres, xborder, yborder;
    gdouble xreal, yreal, phi, min;
    GwyDataField *df;

    if (!args->expand) {
        gwy_data_field_rotate(dfield, args->angle, args->interp);
        return;
    }

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    min = gwy_data_field_get_min(dfield);
    phi = args->angle;
    xborder = fabs(xres/2.0 * cos(phi)) + fabs(yres/2.0 * sin(phi));
    xborder -= xres/2;
    yborder = fabs(yres/2.0 * cos(phi)) + fabs(xres/2.0 * sin(phi));
    yborder -= yres/2;
    df = gwy_data_field_new(xres + 2*xborder, yres + 2*yborder, 1.0, 1.0,
                            FALSE);
    gwy_data_field_fill(df, min);
    gwy_data_field_area_copy(dfield, df, 0, 0, xres, yres, xborder, yborder);
    gwy_data_field_rotate(df, args->angle, args->interp);
    gwy_data_field_resample(dfield, xres + 2*xborder, yres + 2*yborder,
                            GWY_INTERPOLATION_NONE);
    gwy_data_field_copy(df, dfield, TRUE);
    gwy_data_field_set_xreal(dfield, xreal*(xres + 2.0*xborder)/xres);
    gwy_data_field_set_yreal(dfield, yreal*(yres + 2.0*yborder)/yres);
    g_object_unref(df);
}

static gboolean
rotate(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield;
    RotateArgs args;
    gboolean ok;

    g_return_val_if_fail(run & ROTATE_RUN_MODES, FALSE);
    dfield = gwy_container_get_object_by_name(data, "/0/data");
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = rotate_defaults;
    else
        rotate_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || rotate_dialog(&args, data);
    if (run == GWY_RUN_MODAL)
        rotate_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    data = gwy_container_duplicate(data);
    gwy_app_clean_up_data(data);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    rotate_datafield(dfield, &args);
    if (gwy_container_gis_object_by_name(data, "/0/mask", &dfield))
        rotate_datafield(dfield, &args);
    if (gwy_container_gis_object_by_name(data, "/0/show", &dfield))
        rotate_datafield(dfield, &args);
    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    return FALSE;
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
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(preview,
                                                             "/0/data"));
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
rotate_dialog(RotateArgs *args,
              GwyContainer *data)
{
    GtkWidget *dialog, *table, *hbox;
    GwyPixmapLayer *layer;
    RotateControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Rotate"), NULL, 0,
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

    table = gtk_table_new(3, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);

    controls.angle = gtk_adjustment_new(args->angle*180.0/G_PI,
                                        -360, 360, 1, 30, 0);
    gwy_table_attach_hscale(table, 0, _("Rotate by _angle:"), _("deg"),
                            controls.angle, 0);
    g_signal_connect(controls.angle, "value-changed",
                     G_CALLBACK(angle_changed_cb), &controls);

    controls.interp
        = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                        &controls, args->interp);
    gwy_table_attach_hscale(table, 1, _("_Interpolation type:"), NULL,
                            GTK_OBJECT(controls.interp), GWY_HSCALE_WIDGET);

    controls.expand
        = gtk_check_button_new_with_mnemonic(_("E_xpand result to fit "
                                               "complete data"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.expand),
                                 args->expand);
    gtk_table_attach(GTK_TABLE(table), controls.expand, 0, 4, 2, 3,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(controls.expand, "toggled",
                     G_CALLBACK(expand_changed_cb), &controls);

    controls.data = create_preview_data(data);
    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/show");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view), layer);
    gtk_box_pack_start(GTK_BOX(hbox), controls.data_view, FALSE, FALSE, 8);

    rotate_dialog_update(&controls, args);
    rotate_preview_draw(&controls, args);

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
            *args = rotate_defaults;
            rotate_dialog_update(&controls, args);
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
interp_changed_cb(GObject *item,
                  RotateControls *controls)
{
    controls->args->interp
        = GPOINTER_TO_INT(g_object_get_data(item, "interpolation-type"));
}

static void
expand_changed_cb(GtkWidget *toggle,
                  RotateControls *controls)
{
    RotateArgs *args = controls->args;

    args->expand = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));
    rotate_preview_draw(controls, args);

}

static void
angle_changed_cb(GtkObject *adj,
                 RotateControls *controls)
{
    RotateArgs *args = controls->args;

    args->angle = G_PI/180.0*gtk_adjustment_get_value(GTK_ADJUSTMENT(adj));
    rotate_preview_draw(controls, args);
}

static const gchar *angle_key = "/module/rotate/angle";
static const gchar *interp_key = "/module/rotate/interp";
static const gchar *expand_key = "/module/rotate/expand";

static void
rotate_sanitize_args(RotateArgs *args)
{
    args->angle = fmod(args->angle, 2*G_PI);
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->expand = !!args->expand;
}

static void
rotate_load_args(GwyContainer *container,
                 RotateArgs *args)
{
    *args = rotate_defaults;

    gwy_container_gis_double_by_name(container, angle_key, &args->angle);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_boolean_by_name(container, expand_key, &args->expand);
    rotate_sanitize_args(args);
}

static void
rotate_save_args(GwyContainer *container,
                 RotateArgs *args)
{
    gwy_container_set_double_by_name(container, angle_key, args->angle);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_boolean_by_name(container, expand_key, args->expand);
}

static void
rotate_dialog_update(RotateControls *controls,
                     RotateArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->angle),
                             args->angle*180.0/G_PI);
    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->expand),
                                 args->expand);
}

static void
rotate_preview_draw(RotateControls *controls,
                    RotateArgs *args)
{
    GwyDataField *dfield, *rfield;
    GwyContainer *data;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->data_view));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    rfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/show"));
    gwy_data_field_copy(dfield, rfield, FALSE);
    gwy_data_field_rotate(rfield, args->angle, args->interp);
    gwy_data_field_data_changed(rfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
