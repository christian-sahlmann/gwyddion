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

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define SPHREV_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function. */
typedef struct {
    GtkOrientation direction;
    gdouble radius;
    /* interface only */
    GwySIValueFormat valform;
    gdouble pixelsize;
} SphrevArgs;

typedef struct {
    GSList *direction;
    GtkObject *radius;
    GtkObject *size;
    gboolean in_update;
} SphrevControls;

static gboolean    module_register           (const gchar *name);
static gboolean    sphrev                    (GwyContainer *data,
                                              GwyRunType run);
static gboolean    sphrev_dialog             (SphrevArgs *args);
static void        direction_changed_cb      (GObject *item,
                                              SphrevArgs *args);
static void        radius_changed_cb         (GtkAdjustment *adj,
                                              SphrevArgs *args);
static void        size_changed_cb           (GtkAdjustment *adj,
                                              SphrevArgs *args);
static void        sphrev_dialog_update      (SphrevControls *controls,
                                              SphrevArgs *args);
static void        sphrev_sanitize_args      (SphrevArgs *args);
static void        sphrev_load_args          (GwyContainer *container,
                                              SphrevArgs *args);
static void        sphrev_save_args          (GwyContainer *container,
                                              SphrevArgs *args);

SphrevArgs sphrev_defaults = {
    GTK_ORIENTATION_HORIZONTAL,
    1e-9,
    { 1.0, 0, NULL },
    0,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "sphere_revolve",
    N_("Level data by revolving a sphere."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo sphrev_func_info = {
        "sphere_revolve",
        N_("/_Level/Revolve _Sphere (1D)..."),
        (GwyProcessFunc)&sphrev,
        SPHREV_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &sphrev_func_info);

    return TRUE;
}

static gboolean
sphrev(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield;
    SphrevArgs args;
    gdouble xr, yr;
    gboolean ok;

    g_return_val_if_fail(run & SPHREV_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = sphrev_defaults;
    else
        sphrev_load_args(gwy_app_settings_get(), &args);

    /* FIXME: this is bogus for non-square pixels anyway */
    xr = gwy_data_field_get_xreal(dfield)/gwy_data_field_get_xres(dfield);
    yr = gwy_data_field_get_yreal(dfield)/gwy_data_field_get_yres(dfield);
    args.pixelsize = sqrt(xr*xr + yr*yr);
    gwy_data_field_get_value_format_xy(dfield, &args.valform);
    gwy_debug("pixelsize = %g, vf = (%g, %d, %s)",
              args.pixelsize, args.valform.magnitude, args.valform.precision,
              args.valform.units);

    ok = (run != GWY_RUN_MODAL) || sphrev_dialog(&args);
    if (run == GWY_RUN_MODAL)
        sphrev_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    return FALSE;
}

static gboolean
sphrev_dialog(SphrevArgs *args)
{
    const GwyEnum directions[] = {
        { N_("_Horizontal direction"), GTK_ORIENTATION_HORIZONTAL, },
        { N_("_Vertical direction"),   GTK_ORIENTATION_VERTICAL,   },
    };
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *spin;
    SphrevControls controls;
    gint response, row;
    GSList *radio;
    gdouble q;

    dialog = gtk_dialog_new_with_buttons(_("Revolve Sphere (1D)"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;
    controls.in_update = TRUE;

    /* Set it here, to preserve any silly value when GUI it not involved */
    args->radius = CLAMP(args->radius, args->pixelsize, 16384*args->pixelsize);
    q = args->pixelsize/args->valform.magnitude;
    gwy_debug("q = %f", q);
    controls.radius = gtk_adjustment_new(args->radius/args->valform.magnitude,
                                         q, 16384*q, q, 10*q, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Real _radius:"),
                                       args->valform.units,
                                       controls.radius);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), args->valform.precision);
    g_object_set_data(G_OBJECT(controls.radius), "controls", &controls);
    g_signal_connect(controls.radius, "value_changed",
                     G_CALLBACK(radius_changed_cb), args);
    row++;

    controls.size = gtk_adjustment_new(args->radius/args->pixelsize,
                                       1, 16384, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Pixel radius:"), _("px"),
                                       controls.size);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
    g_object_set_data(G_OBJECT(controls.size), "controls", &controls);
    g_signal_connect(controls.size, "value_changed",
                     G_CALLBACK(size_changed_cb), args);
    row++;

    radio = gwy_radio_buttons_create(directions, G_N_ELEMENTS(directions),
                                     "direction-type",
                                     G_CALLBACK(direction_changed_cb), args,
                                     args->direction);
    controls.direction = radio;
    while (radio) {
        gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(radio->data),
                         0, 3, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
        row++;
        radio = g_slist_next(radio);
    }

    controls.in_update = FALSE;

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
            args->radius = sphrev_defaults.radius;
            args->direction = sphrev_defaults.direction;
            sphrev_dialog_update(&controls, args);
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
direction_changed_cb(GObject *item,
                     SphrevArgs *args)
{
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item)))
        return;

    args->direction
        = GPOINTER_TO_INT(g_object_get_data(item, "direction-type"));
}

static void
radius_changed_cb(GtkAdjustment *adj,
                  SphrevArgs *args)
{
    SphrevControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->radius = gtk_adjustment_get_value(adj)*args->valform.magnitude;
    gwy_debug("radius: %.*f %s",
              args->valform.precision,
              args->radius/args->valform.magnitude,
              args->valform.units);
    sphrev_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
size_changed_cb(GtkAdjustment *adj,
                SphrevArgs *args)
{
    SphrevControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->radius = gtk_adjustment_get_value(adj)*args->pixelsize;
    gwy_debug("radius: %.*f %s",
              args->valform.precision,
              args->radius/args->valform.magnitude,
              args->valform.units);
    sphrev_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
sphrev_dialog_update(SphrevControls *controls,
                     SphrevArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->radius),
                             args->radius/args->valform.magnitude);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size),
                             ROUND(args->radius/args->pixelsize));
    gwy_radio_buttons_set_current(controls->direction, "interpolation-type",
                                  args->direction);
}

static const gchar *radius_key = "/module/sphere_revolve/radius";
static const gchar *direction_key = "/module/sphere_revolve/direction";

static void
sphrev_sanitize_args(SphrevArgs *args)
{
    if (args->radius <= 0)
        args->radius = sphrev_defaults.radius;
    args->direction = MIN(args->direction, GTK_ORIENTATION_VERTICAL);
}

static void
sphrev_load_args(GwyContainer *container,
                 SphrevArgs *args)
{
    *args = sphrev_defaults;

    gwy_container_gis_double_by_name(container, radius_key, &args->radius);
    gwy_container_gis_enum_by_name(container, direction_key, &args->direction);
    sphrev_sanitize_args(args);
}

static void
sphrev_save_args(GwyContainer *container,
                 SphrevArgs *args)
{
    gwy_container_set_double_by_name(container, radius_key, args->radius);
    gwy_container_set_enum_by_name(container, direction_key, args->direction);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
