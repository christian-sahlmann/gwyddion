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

typedef struct {
    gboolean is_visible;  /* XXX: GTK_WIDGET_VISIBLE() returns BS? */
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *val;
    gdouble mag;
    gint precision;
    gchar *units;
} PointerControls;

static gboolean   module_register                (const gchar *name);
static void       pointer_use                    (GwyDataWindow *data_window,
                                                  GwyToolSwitchEvent reason);
static GtkWidget* pointer_dialog_create          (GwyDataView *data_view);
static void       pointer_selection_updated_cb   (void);
static void       pointer_dialog_response_cb     (gpointer unused,
                                                  gint response);
static void       pointer_dialog_abandon         (void);
static void       pointer_dialog_set_visible     (gboolean visible);

static GtkWidget *dialog = NULL;
static PointerControls controls;
static gulong updated_id = 0;
static gulong response_id = 0;
static GwyDataViewLayer *pointer_layer = NULL;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "pointer",
    "Pointer tool.",
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
    static GwyToolFuncInfo pointer_func_info = {
        "pointer",
        "gwy_pointer_measure",
        "Read value under mouse cursor.",
        &pointer_use,
    };

    gwy_tool_func_register(name, &pointer_func_info);

    return TRUE;
}

static void
pointer_use(GwyDataWindow *data_window,
            GwyToolSwitchEvent reason)
{
    GwyDataViewLayer *layer;
    GwyDataView *data_view;

    gwy_debug("%p", data_window);

    if (!data_window) {
        pointer_dialog_abandon();
        return;
    }
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = (GwyDataView*)gwy_data_window_get_data_view(data_window);
    layer = gwy_data_view_get_top_layer(data_view);
    if (layer && layer == pointer_layer)
        return;
    if (pointer_layer && updated_id)
        g_signal_handler_disconnect(pointer_layer, updated_id);

    if (layer && GWY_IS_LAYER_POINTER(layer))
        pointer_layer = layer;
    else {
        pointer_layer = (GwyDataViewLayer*)gwy_layer_pointer_new();
        gwy_data_view_set_top_layer(data_view, pointer_layer);
    }
    if (!dialog)
        dialog = pointer_dialog_create(data_view);

    updated_id = g_signal_connect(pointer_layer, "updated",
                                   G_CALLBACK(pointer_selection_updated_cb),
                                   NULL);
    pointer_selection_updated_cb();
}

static void
pointer_dialog_abandon(void)
{
    if (pointer_layer && updated_id)
        g_signal_handler_disconnect(pointer_layer, updated_id);
    updated_id = 0;
    pointer_layer = NULL;
    if (dialog) {
        g_signal_handler_disconnect(dialog, response_id);
        gtk_widget_destroy(dialog);
        dialog = NULL;
        response_id = 0;
        g_free(controls.units);
        controls.is_visible = FALSE;
    }
}

static GtkWidget*
pointer_dialog_create(GwyDataView *data_view)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GtkWidget *dialog, *table, *label;
    gdouble xreal, yreal, max, unit;

    gwy_debug("");
    data = gwy_data_view_get_data(data_view);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(dfield),
               yreal/gwy_data_field_get_yres(dfield));
    controls.mag = gwy_math_humanize_numbers(unit, max, &controls.precision);
    controls.units = g_strconcat(gwy_math_SI_prefix(controls.mag), "m", NULL);

    dialog = gtk_dialog_new_with_buttons(_("Show value"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    g_signal_connect(dialog, "delete_event",
                     G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);
    response_id = g_signal_connect(dialog, "response",
                                   G_CALLBACK(pointer_dialog_response_cb), NULL);
    table = gtk_table_new(5, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Position</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("X"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Y"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Value</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Value"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5, GTK_FILL, 0, 2, 2);

    controls.x = gtk_label_new("");
    controls.y = gtk_label_new("");
    controls.val = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.x), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.y), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.val), 1.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.x, 2, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.y, 2, 3, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.val, 2, 3, 4, 5);
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
pointer_selection_updated_cb(void)
{
    gdouble x, y;
    gboolean is_visible, is_selected;

    gwy_debug("");
    /*XXX: seems broken
     * is_visible = GTK_WIDGET_VISIBLE(dialog);*/
    is_visible = controls.is_visible;
    is_selected = gwy_layer_pointer_get_point(pointer_layer, &x, &y);
    if (!is_visible && !is_selected)
        return;
    if (is_selected) {
        update_label(controls.x, x);
        update_label(controls.y, y);
        {
            gchar buffer[16];
            gdouble value;
            GwyContainer *data;
            GwyDataField *dfield;

            data = gwy_data_view_get_data(GWY_DATA_VIEW(pointer_layer->parent));
            dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                     "/0/data"));
            value = gwy_data_field_get_dval_real(dfield, x, y,
                                                 GWY_INTERPOLATION_ROUND);
            g_snprintf(buffer, sizeof(buffer), "%g", value);
            gtk_label_set_text(GTK_LABEL(controls.val), buffer);
        }
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls.x), "");
        gtk_label_set_text(GTK_LABEL(controls.y), "");
        gtk_label_set_text(GTK_LABEL(controls.val), "");
    }
    if (!is_visible)
        pointer_dialog_set_visible(TRUE);
}

static void
pointer_dialog_response_cb(gpointer unused, gint response)
{
    gwy_debug("response %d", response);
    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        pointer_dialog_set_visible(FALSE);
        break;

        case GTK_RESPONSE_NONE:
        g_warning("Tool dialog destroyed.");
        pointer_use(NULL, 0);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
pointer_dialog_set_visible(gboolean visible)
{
    gwy_debug("now %d, setting to %d",
              controls.is_visible, visible);
    if (controls.is_visible == visible)
        return;

    controls.is_visible = visible;
    if (visible)
        gtk_window_present(GTK_WINDOW(dialog));
    else
        gtk_widget_hide(dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

