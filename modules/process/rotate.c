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
#include <app/app.h>

#define ROTATE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
typedef struct {
    gdouble angle;
    GwyInterpolationType interp;
} RotateArgs;

typedef struct {
    GtkObject *angle;
    GtkWidget *interp;
} RotateControls;

static gboolean    module_register            (const gchar *name);
static gboolean    rotate                     (GwyContainer *data,
                                               GwyRunType run);
static gboolean    rotate_dialog              (RotateArgs *args);
static void        interp_changed_cb          (GObject *item,
                                               RotateArgs *args);
static void        rotate_load_args           (GwyContainer *container,
                                               RotateArgs *args);
static void        rotate_save_args           (GwyContainer *container,
                                               RotateArgs *args);
static void        rotate_dialog_update       (RotateControls *controls,
                                               RotateArgs *args);

RotateArgs rotate_defaults = {
    0.0,
    GWY_INTERPOLATION_BILINEAR,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "rotate",
    "Rotation by an arbitrary angle.",
    "Yeti <yeti@gwyddion.net>",
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
    static GwyProcessFuncInfo rotate_func_info = {
        "rotate",
        "/_Basic Operations/Rotate By _Angle...",
        (GwyProcessFunc)&rotate,
        ROTATE_RUN_MODES,
    };

    gwy_process_func_register(name, &rotate_func_info);

    return TRUE;
}

static gboolean
rotate(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield;
    RotateArgs args;
    gboolean ok;

    g_return_val_if_fail(run & ROTATE_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = rotate_defaults;
    else
        rotate_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || rotate_dialog(&args);
    if (ok) {
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
        g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
        gwy_app_clean_up_data(data);
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        gwy_data_field_rotate(dfield, args.angle, args.interp);
        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
        if (run != GWY_RUN_WITH_DEFAULTS)
            rotate_save_args(gwy_app_settings_get(), &args);
    }

    return ok;
}

static gboolean
rotate_dialog(RotateArgs *args)
{
    GtkWidget *dialog, *table;
    RotateControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Rotate"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    table = gtk_table_new(2, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.angle = gtk_adjustment_new(args->angle, -360, 360, 5, 30, 0);
    gwy_table_attach_spinbutton(table, 0, _("Rotate by angle:"), _("deg (CCW)"),
                                controls.angle);

    controls.interp
        = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                        args, args->interp);
    gwy_table_attach_row(table, 1, _("Interpolation type:"), "",
                         controls.interp);

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

    args->angle = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.angle));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
interp_changed_cb(GObject *item,
                  RotateArgs *args)
{
    args->interp = GPOINTER_TO_INT(g_object_get_data(item,
                                                     "interpolation-type"));
}

static const gchar *angle_key = "/module/rotate/angle";
static const gchar *interp_key = "/module/rotate/interp";

static void
rotate_load_args(GwyContainer *container,
                 RotateArgs *args)
{
    *args = rotate_defaults;

    if (gwy_container_contains_by_name(container, angle_key))
        args->angle = gwy_container_get_double_by_name(container, angle_key);
    if (gwy_container_contains_by_name(container, interp_key))
        args->interp = gwy_container_get_int32_by_name(container, interp_key);
}

static void
rotate_save_args(GwyContainer *container,
                 RotateArgs *args)
{
    gwy_container_set_double_by_name(container, angle_key, args->angle);
    gwy_container_set_int32_by_name(container, interp_key, args->interp);
}

static void
rotate_dialog_update(RotateControls *controls,
                     RotateArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->angle),
                             args->angle);
    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
