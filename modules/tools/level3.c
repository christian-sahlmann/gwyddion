/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

typedef struct {
    gboolean is_visible;  /* XXX: GTK_WIDGET_VISIBLE() returns BS? */
    GtkWidget *coords[6];
    GtkObject *radius;
    gdouble mag;
    gint precision;
    gchar *units;
} Level3Controls;

static gboolean   module_register               (const gchar *name);
static void       level3_use                    (GwyDataWindow *data_window,
                                                 GwyToolSwitchEvent reason);
static GtkWidget* level3_dialog_create          (GwyDataView *data_view);
static void       level3_do                     (void);
static gdouble    level3_get_z_average          (GwyDataField *dfield,
                                                 gdouble xreal,
                                                 gdouble yreal,
                                                 gint radius);
static void       level3_selection_finished_cb  (void);
static void       level3_dialog_response_cb     (gpointer unused,
                                                 gint response);
static void       level3_dialog_abandon         (void);
static void       level3_dialog_set_visible     (gboolean visible);

static const gchar *radius_key = "/tool/level3/radius";

static GtkWidget *dialog;
static Level3Controls controls;
static gulong finished_id = 0;
static gulong response_id = 0;
static GwyDataViewLayer *points_layer = NULL;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "level3",
    "Level tool.  Allows to level data by fitting a plane through three "
        "selected points.",
    "Yeti <yeti@physics.muni.cz>",
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
    static GwyToolFuncInfo level3_func_info = {
        "level3",
        "gwy_fit_triangle",
        "Level data using three points.",
        level3_use,
    };

    gwy_tool_func_register(name, &level3_func_info);

    return TRUE;
}

static void
level3_use(GwyDataWindow *data_window,
           GwyToolSwitchEvent reason)
{
    GwyDataViewLayer *layer;
    GwyDataView *data_view;

    gwy_debug("%s: %p", __FUNCTION__, data_window);

    if (!data_window) {
        level3_dialog_abandon();
        return;
    }
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = (GwyDataView*)gwy_data_window_get_data_view(data_window);
    layer = gwy_data_view_get_top_layer(data_view);
    if (layer && layer == points_layer)
        return;
    if (points_layer && finished_id)
        g_signal_handler_disconnect(points_layer, finished_id);

    if (layer && GWY_IS_LAYER_POINTS(layer)) {
        points_layer = layer;
        gwy_layer_points_set_max_points(layer, 3);
    }
    else {
        points_layer = (GwyDataViewLayer*)gwy_layer_points_new();
        gwy_layer_points_set_max_points(points_layer, 3);
        gwy_data_view_set_top_layer(data_view, points_layer);
    }
    if (!dialog)
        dialog = level3_dialog_create(data_view);

    finished_id = g_signal_connect(points_layer, "finished",
                                   G_CALLBACK(level3_selection_finished_cb),
                                   NULL);
    level3_selection_finished_cb();
}

static void
level3_do(void)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble points[6], z[3];
    gdouble bx, by, c, det;
    gint i, radius;

    if (gwy_layer_points_get_points(points_layer, points) < 3)
        return;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(points_layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.radius));

    det = bx = by = c = 0;
    for (i = 0; i < 3; i++)
        z[i] = level3_get_z_average(dfield, points[2*i], points[2*i+1], radius);
    det = points[0]*(points[3] - points[5])
          + points[2]*(points[5] - points[1])
          + points[4]*(points[1] - points[3]);
    bx = z[0]*(points[3] - points[5])
         + z[1]*(points[5] - points[1])
         + z[2]*(points[1] - points[3]);
    by = z[0]*(points[4] - points[2])
         + z[1]*(points[0] - points[4])
         + z[2]*(points[2] - points[0]);
    c = z[0]*(points[2]*points[5] - points[3]*points[4])
         + z[1]*(points[1]*points[4] - points[5]*points[0])
         + z[2]*(points[0]*points[3] - points[1]*points[2]);
    gwy_debug("%s: bx = %g, by = %g, c = %g, det = %g", __FUNCTION__,
              bx, by, c, det);
    bx /= det;
    by /= det;
    c /= det;
    gwy_debug("%s: bx = %g, by = %g, c = %g", __FUNCTION__, bx, by, c);
    gwy_debug("%s: z[0] = %g, z[1] = %g, z[2] = %g", __FUNCTION__,
              z[0], z[1], z[2]);
    gwy_debug("%s: zn[0] = %g, zn[1] = %g, zn[2] = %g", __FUNCTION__,
              z[0] - c - bx*points[0] - by*points[1],
              z[1] - c - bx*points[2] - by*points[3],
              z[2] - c - bx*points[4] - by*points[5]);
    gwy_app_undo_checkpoint(data, "/0/data");
    gwy_data_field_plane_level(dfield, c, bx, by);
    gwy_debug("%s: zN[0] = %g, zN[1] = %g, zN[2] = %g", __FUNCTION__,
              level3_get_z_average(dfield, points[0], points[1], radius),
              level3_get_z_average(dfield, points[2], points[3], radius),
              level3_get_z_average(dfield, points[4], points[5], radius));
    gwy_data_view_update(GWY_DATA_VIEW(points_layer->parent));
}

static gdouble
level3_get_z_average(GwyDataField *dfield,
                     gdouble xreal,
                     gdouble yreal,
                     gint radius)
{
    gint x, y, xres, yres, uli, ulj, bri, brj;

    if (radius < 1)
        g_warning("Bad averaging radius %d, fixing to 1", radius);
    x = gwy_data_field_rtoj(dfield, xreal);
    y = gwy_data_field_rtoi(dfield, yreal);
    if (radius < 2)
        return gwy_data_field_get_val(dfield, y, x);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    ulj = CLAMP(x - radius, 0, xres - 1);
    uli = CLAMP(y - radius, 0, yres - 1);
    brj = CLAMP(x + radius, 0, xres - 1);
    bri = CLAMP(y + radius, 0, yres - 1);
    return gwy_data_field_get_area_avg(dfield, uli, ulj, bri, brj);
}

static void
level3_dialog_abandon(void)
{
    GwyContainer *settings;
    gint radius;

    if (points_layer && finished_id)
        g_signal_handler_disconnect(points_layer, finished_id);
    finished_id = 0;
    points_layer = NULL;
    if (dialog) {
        radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.radius));
        radius = CLAMP(radius, 1, 16);
        settings = gwy_app_settings_get();
        gwy_container_set_int32_by_name(settings, radius_key, radius);
        g_signal_handler_disconnect(dialog, response_id);
        gtk_widget_destroy(dialog);
        dialog = NULL;
        response_id = 0;
        g_free(controls.units);
        controls.is_visible = FALSE;
    }
}

static GtkWidget*
level3_dialog_create(GwyDataView *data_view)
{
    GwyContainer *data, *settings;
    GwyDataField *dfield;
    GtkWidget *dialog, *table, *label;
    gdouble xreal, yreal, max, unit;
    gint radius;
    guchar *buffer;
    gint i;

    gwy_debug("%s", __FUNCTION__);
    data = gwy_data_view_get_data(data_view);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(dfield),
               yreal/gwy_data_field_get_yres(dfield));
    controls.mag = gwy_math_humanize_numbers(unit, max, &controls.precision);
    controls.units = g_strconcat(gwy_math_SI_prefix(controls.mag), "m", NULL);

    dialog = gtk_dialog_new_with_buttons(_("Level"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    g_signal_connect(dialog, "delete_event",
                     G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);
    response_id = g_signal_connect(dialog, "response",
                                   G_CALLBACK(level3_dialog_response_cb), NULL);
    table = gtk_table_new(10, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    for (i = 0; i < 3; i++) {
        label = gtk_label_new(NULL);
        buffer = g_strdup_printf(_("<b>Point %d</b>"), i+1);
        gtk_label_set_markup(GTK_LABEL(label), buffer);
        g_free(buffer);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3*i, 3*i+1,
                         GTK_FILL, 0, 2, 2);
        label = gtk_label_new(_("X"));
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 1, 2, 3*i + 1, 3*i + 2,
                         GTK_FILL, 0, 2, 2);
        label = gtk_label_new(_("Y"));
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 1, 2, 3*i + 2, 3*i + 3,
                         GTK_FILL, 0, 2, 2);
        label = controls.coords[2*i] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach_defaults(GTK_TABLE(table), label,
                                  2, 3, 3*i + 1, 3*i + 2);
        label = controls.coords[2*i + 1] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach_defaults(GTK_TABLE(table), label,
                                  2, 3, 3*i + 2, 3*i + 3);
    }
    settings = gwy_app_settings_get();
    if (gwy_container_contains_by_name(settings, radius_key))
        radius = gwy_container_get_int32_by_name(settings, radius_key);
    else
        radius = 1;
    controls.radius = gtk_adjustment_new((gdouble)radius, 1, 16, 1, 5, 16);
    gwy_table_attach_spinbutton(table, 9, "Averaging radius", "px",
                                controls.radius);
    gtk_widget_show_all(table);
    controls.is_visible = FALSE;

    return dialog;
}

static void
update_label(GtkWidget *label, gdouble value)
{
    gchar buffer[16];

    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
               controls.precision, value/controls.mag, controls.units);
    gtk_label_set_text(GTK_LABEL(label), buffer);
}

static void
level3_selection_finished_cb(void)
{
    gdouble points[6];
    gboolean is_visible;
    gint nselected, i;

    gwy_debug("%s", __FUNCTION__);
    /*XXX: seems broken
     * is_visible = GTK_WIDGET_VISIBLE(dialog);*/
    is_visible = controls.is_visible;
    nselected = gwy_layer_points_get_points(points_layer, points);
    if (!is_visible && !nselected)
        return;
    for (i = 0; i < 6; i++) {
        if (i < 2*nselected)
            update_label(controls.coords[i], points[i]);
        else
            gtk_label_set_text(GTK_LABEL(controls.coords[i]), "");
    }
    if (!is_visible)
        level3_dialog_set_visible(TRUE);
}

static void
level3_dialog_response_cb(gpointer unused, gint response)
{
    gwy_debug("%s: response %d", __FUNCTION__, response);
    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        level3_dialog_set_visible(FALSE);
        break;

        case GTK_RESPONSE_NONE:
        g_warning("Tool dialog destroyed.");
        level3_use(NULL, 0);
        break;

        case GTK_RESPONSE_APPLY:
        level3_do();
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
level3_dialog_set_visible(gboolean visible)
{
    gwy_debug("%s: now %d, setting to %d",
              __FUNCTION__, controls.is_visible, visible);
    if (controls.is_visible == visible)
        return;

    controls.is_visible = visible;
    if (visible)
        gtk_window_present(GTK_WINDOW(dialog));
    else
        gtk_widget_hide(dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

