/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2016 David Necas (Yeti), Petr Klapetek.
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

/*
 * TODO: Essentially always we should rotate in physical space, not pixel
 * space.  Meaning that gwy_data_field_rotate() does not do the right thing;
 * in pixel coordinates the transform is general affine... */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include "preview.h"

#define ROTATE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
    ROTATE_EXTERIOR_SAME_SIZE = 0,
    ROTATE_EXTERIOR_EXPAND    = 1,
    ROTATE_EXTERIOR_CUT       = 2,
} RotateExteriorType;

typedef struct {
    gdouble angle;
    GwyInterpolationType interp;
    RotateExteriorType exterior;
    gboolean create_mask;
    gboolean show_grid;
} RotateArgs;

typedef struct {
    GtkObject *angle;
    GtkWidget *interp;
    GSList *exterior;
    GtkWidget *show_grid;
    GtkWidget *create_mask;
    GtkWidget *data_view;
    GwyContainer *data;
    GwySelection *selection;
    RotateArgs *args;
} RotateControls;

static gboolean module_register     (void);
static void     rotate              (GwyContainer *data,
                                     GwyRunType run);
static gboolean rotate_dialog       (RotateArgs *args,
                                     GwyContainer *data);
static void     interp_changed      (GtkWidget *combo,
                                     RotateControls *controls);
static void     angle_changed       (GtkObject *angle,
                                     RotateControls *controls);
static void     exterior_changed    (GtkToggleButton *toggle,
                                     RotateControls *controls);
static void     create_mask_changed (GtkToggleButton *toggle,
                                     RotateControls *controls);
static void     show_grid_changed   (GtkToggleButton *toggle,
                                     RotateControls *controls);
static void     rotate_preview_draw (RotateControls *controls,
                                     RotateArgs *args);
static void     rotate_dialog_update(RotateControls *controls,
                                     RotateArgs *args);
static void     rotate_sanitize_args(RotateArgs *args);
static void     rotate_load_args    (GwyContainer *container,
                                     RotateArgs *args);
static void     rotate_save_args    (GwyContainer *container,
                                     RotateArgs *args);

static const RotateArgs rotate_defaults = {
    0.0,
    GWY_INTERPOLATION_LINEAR,
    ROTATE_EXTERIOR_SAME_SIZE,
    FALSE,
    TRUE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Rotates data by arbitrary angle."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("rotate",
                              (GwyProcessFunc)&rotate,
                              N_("/_Basic Operations/Rotate by _Angle..."),
                              GWY_STOCK_ROTATE,
                              ROTATE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Rotate by arbitrary angle"));
    return TRUE;
}

static void
rotate_datafield(GwyDataField *dfield,
                 RotateArgs *args)
{
    gint xres, yres, xborder, yborder;
    gdouble xreal, yreal, phi, min, sphi, cphi;
    GwyDataField *df;

    phi = args->angle;
    if (args->exterior == ROTATE_EXTERIOR_SAME_SIZE) {
        gwy_data_field_rotate(dfield, phi, args->interp);
        return;
    }

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);

    /*
     * To cut the data to maximum container rectangle.
     * We have rectangle width 2a, height 2b, centered at origin, rotated by
     * angle 0 ≤ φ ≤ π/2.
     * If b ≤ a*sin(2φ) then the rectangle is `flat' and we have corner
     * coordinates of the cut x = b/2sin(φ), y = b/2cos(φ).
     * If a ≤ b*sin(2φ) then the rectangle is `tall' and we have corner
     * coordinates of the cut x = a/2cos(φ), y = a/2sin(φ).
     * Otherwise the maximum rectangle is attained when its corners touch
     * both edges of the rotated rectangle, resulting in coordinates of the cut
     * x = (a*cos(φ) - b*sin(φ))/cos(2*φ),
     * y = (b*cos(φ) - a*sin(φ))/cos(2*φ).
     */
    if (args->exterior == ROTATE_EXTERIOR_CUT) {
        gdouble xr, yr, s2phi, c2phi;
        gint xfrom, xto, yfrom, yto;

        gwy_data_field_rotate(dfield, phi, args->interp);
        /* Make 0 ≤ φ ≤ π. */
        phi = fmod(phi + 4.0*G_PI, G_PI);
        /* Make 0 ≤ φ ≤ π/2. */
        if (phi > 0.5*G_PI)
            phi = G_PI - phi;

        sphi = sin(phi);
        cphi = cos(phi);
        s2phi = sin(2.0*phi);
        c2phi = cos(2.0*phi);

        if (yres <= xres*s2phi) {
            xr = 0.5*yres/sphi;
            yr = 0.5*yres/cphi;
        }
        else if (xres <= yres*s2phi) {
            xr = 0.5*xres/cphi;
            yr = 0.5*xres/sphi;
        }
        else {
            xr = (xres*cphi - yres*sphi)/c2phi;
            yr = (yres*cphi - xres*sphi)/c2phi;
        }

        xfrom = (gint)ceil(0.5*(xres - xr));
        yfrom = (gint)ceil(0.5*(yres - yr));
        xto = (gint)floor(0.5*(xres + xr));
        yto = (gint)floor(0.5*(yres + yr));
        xfrom = CLAMP(xfrom, 0, xres/2 - 1);
        yfrom = CLAMP(yfrom, 0, yres/2 - 1);
        xto = CLAMP(xto, (xres + 1)/2 + 1, xres-1);
        yto = CLAMP(yto, (yres + 1)/2 + 1, yres-1);
        gwy_data_field_resize(dfield, xfrom, yfrom, xto+1, yto+1);
        return;
    }

    /* To expand the rectangle we just calculate x and y of the corners of
     * the rotated rectangle.  But we have to transform them to expanded
     * widths. */

    sphi = sin(phi);
    cphi = cos(phi);
    min = gwy_data_field_get_min(dfield);
    xborder = fabs(xres/2.0 * cphi) + fabs(yres/2.0 * sphi);
    xborder -= xres/2;
    yborder = fabs(yres/2.0 * cphi) + fabs(xres/2.0 * sphi);
    yborder -= yres/2;
    df = gwy_data_field_new(xres + fabs(2*xborder), yres + fabs(2*yborder),
                            1.0, 1.0,
                            FALSE);
    gwy_data_field_fill(df, min);
    gwy_data_field_area_copy(dfield, df, 0, 0, xres, yres,
                             fabs(xborder), fabs(yborder));
    gwy_data_field_rotate(df, args->angle, args->interp);
    gwy_data_field_resample(dfield, xres + 2*xborder, yres + 2*yborder,
                            GWY_INTERPOLATION_NONE);
    if (xborder <= 0) {
        gwy_data_field_area_copy(df, dfield,
                                 fabs(2*xborder), 0,
                                 xres + 2*xborder, yres + 2*yborder,
                                 0, 0);
    }
    else {
          if (yborder <= 0) {
              gwy_data_field_area_copy(df, dfield,
                                       0, fabs(2*yborder),
                                       xres + 2*xborder, yres + 2*yborder,
                                       0, 0);
          }
          else {
            gwy_data_field_area_copy(df, dfield,
                                     0, 0,
                                     xres + 2*xborder, yres + 2*yborder,
                                     0, 0);
          }
    }
    gwy_data_field_set_xreal(dfield, xreal*(xres + 2.0*xborder)/xres);
    gwy_data_field_set_yreal(dfield, yreal*(yres + 2.0*yborder)/yres);
    g_object_unref(df);
}

static void
rotate(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfields[3];
    GQuark quark;
    gint oldid, newid;
    RotateArgs args;
    gboolean ok;

    g_return_if_fail(run & ROTATE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, dfields + 0,
                                     GWY_APP_MASK_FIELD, dfields + 1,
                                     GWY_APP_SHOW_FIELD, dfields + 2,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfields[0]);

    rotate_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = rotate_dialog(&args, data);
        rotate_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    dfields[0] = gwy_data_field_duplicate(dfields[0]);
    rotate_datafield(dfields[0], &args);
    if (dfields[1]) {
        GwyInterpolationType interp = args.interp;

        dfields[1] = gwy_data_field_duplicate(dfields[1]);
        args.interp = GWY_INTERPOLATION_ROUND;
        rotate_datafield(dfields[1], &args);
        args.interp = interp;
    }
    if (dfields[2]) {
        dfields[2] = gwy_data_field_duplicate(dfields[2]);
        rotate_datafield(dfields[2], &args);
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

    gwy_app_set_data_field_title(data, newid, _("Rotated Data"));
    gwy_app_channel_log_add_proc(data, oldid, newid);
}

/* create a smaller copy of data */
static GwyContainer*
create_preview_data(GwyContainer *data)
{
    GwyContainer *preview;
    GwyDataField *dfield, *dfield_show;
    gint oldid;
    gint xres, yres;
    gdouble zoomval;

    preview = gwy_container_new();

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    zoomval = (gdouble)PREVIEW_SIZE/MAX(xres, yres);
    dfield = gwy_data_field_new_resampled(dfield, xres*zoomval, yres*zoomval,
                                          GWY_INTERPOLATION_LINEAR);
    dfield_show = gwy_data_field_duplicate(dfield);

    gwy_container_set_object_by_name(preview, "/1/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_object_by_name(preview, "/0/data", dfield_show);
    g_object_unref(dfield_show);

    gwy_app_sync_data_items(data, preview, oldid, 0, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    return preview;
}

static gboolean
rotate_dialog(RotateArgs *args,
              GwyContainer *data)
{
    GtkWidget *dialog, *table, *hbox, *label;
    RotateControls controls;
    gint response, row;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Rotate"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    table = gtk_table_new(8, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 0);
    row = 0;

    controls.angle = gtk_adjustment_new(args->angle*180.0/G_PI,
                                        -360, 360, 0.01, 5, 0);
    gwy_table_attach_hscale(table, row, _("Rotate by _angle:"), _("deg"),
                            controls.angle, 0);
    g_signal_connect(controls.angle, "value-changed",
                     G_CALLBACK(angle_changed), &controls);
    row++;

    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(interp_changed), &controls,
                                 args->interp, TRUE);
    gwy_table_attach_hscale(table, row, _("_Interpolation type:"), NULL,
                            GTK_OBJECT(controls.interp),
                            GWY_HSCALE_WIDGET_NO_EXPAND);
    row++;

    controls.show_grid
        = gtk_check_button_new_with_mnemonic(_("Show _grid"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.show_grid),
                                 args->show_grid);
    gtk_table_attach(GTK_TABLE(table), controls.show_grid,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.show_grid, "toggled",
                     G_CALLBACK(show_grid_changed), &controls);
    row++;

    controls.create_mask
        = gtk_check_button_new_with_mnemonic(_("Create _mask over exterior"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.create_mask),
                                 args->create_mask);
    gtk_table_attach(GTK_TABLE(table), controls.create_mask,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.create_mask, "toggled",
                     G_CALLBACK(create_mask_changed), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gtk_label_new(_("Result size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.exterior
        = gwy_radio_buttons_createl(G_CALLBACK(exterior_changed), &controls,
                                    args->exterior,
                                    _("_Same as original"),
                                    ROTATE_EXTERIOR_SAME_SIZE,
                                    _("_Expanded to complete data"),
                                    ROTATE_EXTERIOR_EXPAND,
                                    _("C_ut to valid data"),
                                    ROTATE_EXTERIOR_CUT,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.exterior,
                                            GTK_TABLE(table), 4, row);

    controls.data = create_preview_data(data);
    controls.data_view = create_preview(controls.data, 0, PREVIEW_SIZE, FALSE);
    g_object_unref(controls.data);
    controls.selection = create_vector_layer(GWY_DATA_VIEW(controls.data_view),
                                             0, "Lattice", TRUE);
    gwy_selection_set_max_objects(controls.selection, 1);
    gtk_box_pack_start(GTK_BOX(hbox), controls.data_view, FALSE, FALSE, 8);

    rotate_dialog_update(&controls, args);
    show_grid_changed(GTK_TOGGLE_BUTTON(controls.show_grid), &controls);
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
interp_changed(GtkWidget *combo, RotateControls *controls)
{
    controls->args->interp
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void
angle_changed(GtkObject *adj, RotateControls *controls)
{
    RotateArgs *args = controls->args;

    args->angle = G_PI/180.0*gtk_adjustment_get_value(GTK_ADJUSTMENT(adj));
    rotate_preview_draw(controls, args);
}

static void
exterior_changed(GtkToggleButton *toggle, RotateControls *controls)
{
    if (!gtk_toggle_button_get_active(toggle))
        return;

    controls->args->exterior
        = gwy_radio_buttons_get_current(controls->exterior);
    rotate_preview_draw(controls, controls->args);
}

static void
create_mask_changed(GtkToggleButton *toggle, RotateControls *controls)
{
    controls->args->create_mask = gtk_toggle_button_get_active(toggle);
}

static void
show_grid_changed(GtkToggleButton *toggle, RotateControls *controls)
{
    RotateArgs *args = controls->args;
    GwySelection *selection = controls->selection;
    GwyDataField *dfield;
    gdouble xy[4];

    args->show_grid = gtk_toggle_button_get_active(toggle);
    if (!args->show_grid) {
        gwy_selection_clear(selection);
        return;
    }

    dfield = gwy_container_get_object_by_name(controls->data, "/0/data");
    xy[0] = gwy_data_field_get_xreal(dfield)/12.0;
    xy[1] = xy[2] = 0.0;
    xy[3] = gwy_data_field_get_yreal(dfield)/12.0;
    gwy_selection_set_data(selection, 1, xy);
}

static void
rotate_dialog_update(RotateControls *controls,
                     RotateArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->angle),
                             args->angle*180.0/G_PI);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
    gwy_radio_buttons_set_current(controls->exterior, args->exterior);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->show_grid),
                                 args->show_grid);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->create_mask),
                                 args->create_mask);
}

static void
rotate_preview_draw(RotateControls *controls,
                    RotateArgs *args)
{
    GwyDataField *dfield, *rfield;
    GwyContainer *data;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->data_view));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/1/data"));
    rfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_serializable_clone(G_OBJECT(dfield), G_OBJECT(rfield));
    rotate_datafield(rfield, args);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls->data_view), PREVIEW_SIZE);
    gwy_data_field_data_changed(rfield);
}

static const gchar angle_key[]       = "/module/rotate/angle";
static const gchar create_mask_key[] = "/module/rotate/create_mask";
static const gchar exterior_key[]    = "/module/rotate/exterior";
static const gchar interp_key[]      = "/module/rotate/interp";
static const gchar show_grid_key[]   = "/module/rotate/show_grid";

static void
rotate_sanitize_args(RotateArgs *args)
{
    args->angle = fmod(args->angle, 2*G_PI);
    args->interp = gwy_enum_sanitize_value(args->interp,
                                           GWY_TYPE_INTERPOLATION_TYPE);
    args->exterior = CLAMP(args->exterior,
                           ROTATE_EXTERIOR_SAME_SIZE, ROTATE_EXTERIOR_CUT);
    args->create_mask = !!args->create_mask;
    args->show_grid = !!args->show_grid;
}

static void
rotate_load_args(GwyContainer *container,
                 RotateArgs *args)
{
    *args = rotate_defaults;

    gwy_container_gis_double_by_name(container, angle_key, &args->angle);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, exterior_key, &args->exterior);
    gwy_container_gis_boolean_by_name(container, show_grid_key,
                                      &args->show_grid);
    gwy_container_gis_boolean_by_name(container, create_mask_key,
                                      &args->create_mask);
    rotate_sanitize_args(args);
}

static void
rotate_save_args(GwyContainer *container,
                 RotateArgs *args)
{
    gwy_container_set_double_by_name(container, angle_key, args->angle);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, exterior_key, args->exterior);
    gwy_container_set_boolean_by_name(container, show_grid_key,
                                      args->show_grid);
    gwy_container_set_boolean_by_name(container, create_mask_key,
                                      args->create_mask);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
