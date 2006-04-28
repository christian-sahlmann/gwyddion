/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_READ_VALUE            (gwy_tool_read_value_get_type())
#define GWY_TOOL_READ_VALUE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_READ_VALUE, GwyToolReadValue))
#define GWY_IS_TOOL_READ_VALUE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_READ_VALUE))
#define GWY_TOOL_READ_VALUE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_READ_VALUE, GwyToolReadValueClass))

typedef struct _GwyToolReadValue      GwyToolReadValue;
typedef struct _GwyToolReadValueClass GwyToolReadValueClass;

typedef struct {
    gint radius;
} ToolArgs;

struct _GwyToolReadValue {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkLabel *x;
    GtkLabel *y;
    GtkLabel *xunits;
    GtkLabel *yunits;
    GtkLabel *val;
    GtkLabel *zunits;
    GtkObject *radius;

    gulong selection_id;

    /* potential class data */
    GType layer_type_point;
};

struct _GwyToolReadValueClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType  gwy_tool_read_value_get_type         (void) G_GNUC_CONST;
static void   gwy_tool_read_value_finalize         (GObject *object);
static void   gwy_tool_read_value_init_dialog      (GwyToolReadValue *tool);
static void   gwy_tool_read_value_data_switched    (GwyTool *gwytool,
                                                    GwyDataView *data_view);
static void   gwy_tool_read_value_data_changed     (GwyPlainTool *plain_tool);
static void   gwy_tool_read_value_selection_changed(GwySelection *selection,
                                                    gint hint,
                                                    GwyToolReadValue *tool);
static void   gwy_tool_read_value_radius_changed   (GwyToolReadValue *tool);
static void   gwy_tool_read_value_update_headers   (GwyToolReadValue *tool);
static void   gwy_tool_read_value_update_values    (GwyToolReadValue *tool);

static const gchar radius_key[] = "/module/readvalue/radius";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Pointer tool, reads value under pointer."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static const ToolArgs default_args = {
    1,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolReadValue, gwy_tool_read_value, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_READ_VALUE);

    return TRUE;
}

static void
gwy_tool_read_value_class_init(GwyToolReadValueClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_read_value_finalize;

    tool_class->stock_id = GWY_STOCK_POINTER_MEASURE;
    tool_class->title = _("Read Value");
    tool_class->tooltip = _("Read value under mouse cursor");
    tool_class->prefix = "/module/readvalue";
    tool_class->data_switched = gwy_tool_read_value_data_switched;

    ptool_class->data_changed = gwy_tool_read_value_data_changed;
}

static void
gwy_tool_read_value_finalize(GObject *object)
{
    GwyToolReadValue *tool;
    GwyPlainTool *plain_tool;
    GwySelection *selection;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(object);
    tool = GWY_TOOL_READ_VALUE(object);

    if (plain_tool->layer) {
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        gwy_signal_handler_disconnect(selection, tool->selection_id);
    }

    settings = gwy_app_settings_get();
    gwy_container_set_int32_by_name(settings, radius_key, tool->args.radius);

    G_OBJECT_CLASS(gwy_tool_read_value_parent_class)->finalize(object);
}

static void
gwy_tool_read_value_init(GwyToolReadValue *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_point = gwy_plain_tool_check_layer_type(plain_tool,
                                                             "GwyLayerPoint");
    if (!tool->layer_type_point)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_VFMARKUP;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_int32_by_name(settings, radius_key, &tool->args.radius);

    gwy_tool_read_value_init_dialog(tool);
}

static void
gwy_tool_read_value_init_dialog(GwyToolReadValue *tool)
{
    GtkDialog *dialog;
    GtkTable *table;
    GtkWidget *table2;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    table = GTK_TABLE(gtk_table_new(2, 3, FALSE));
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table), TRUE, TRUE, 0);

    tool->xunits = GTK_LABEL(gtk_label_new(NULL));
    gtk_table_attach(table, GTK_WIDGET(tool->xunits),
                     0, 1, 0, 1, GTK_FILL, 0, 2, 2);

    tool->yunits = GTK_LABEL(gtk_label_new(NULL));
    gtk_table_attach(table, GTK_WIDGET(tool->yunits),
                     1, 2, 0, 1, GTK_FILL, 0, 2, 2);

    tool->zunits = GTK_LABEL(gtk_label_new(NULL));
    gtk_table_attach(table, GTK_WIDGET(tool->zunits),
                     2, 3, 0, 1, GTK_FILL, 0, 2, 2);

    tool->x = GTK_LABEL(gtk_label_new(NULL));
    gtk_misc_set_alignment(GTK_MISC(tool->x), 1.0, 0.5);
    gtk_table_attach(table, GTK_WIDGET(tool->x),
                     0, 1, 1, 2, 0, 0, 2, 2);

    tool->y = GTK_LABEL(gtk_label_new(NULL));
    gtk_misc_set_alignment(GTK_MISC(tool->y), 1.0, 0.5);
    gtk_table_attach(table, GTK_WIDGET(tool->y),
                     1, 2, 1, 2, 0, 0, 2, 2);

    tool->val = GTK_LABEL(gtk_label_new(NULL));
    gtk_misc_set_alignment(GTK_MISC(tool->val), 1.0, 0.5);
    gtk_table_attach(table, GTK_WIDGET(tool->val), 2, 3, 1, 2,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    table2 = gtk_table_new(1, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table2), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), table2, TRUE, TRUE, 0);

    tool->radius = gtk_adjustment_new(tool->args.radius, 1, 16, 1, 5, 16);
    gwy_table_attach_spinbutton(table2, 9, _("_Averaging radius:"), "px",
                                tool->radius);
    g_signal_connect_swapped(tool->radius, "value-changed",
                             G_CALLBACK(gwy_tool_read_value_radius_changed),
                             tool);

    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);
    gwy_tool_read_value_update_headers(tool);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_read_value_data_switched(GwyTool *gwytool,
                                GwyDataView *data_view)
{
    GwyToolReadValue *tool;
    GwySelection *selection;
    GwyPlainTool *plain_tool;

    GWY_TOOL_CLASS(gwy_tool_read_value_parent_class)->data_switched(gwytool,
                                                                  data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_READ_VALUE(gwytool);
    if (plain_tool->layer) {
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        gwy_signal_handler_disconnect(selection, tool->selection_id);
    }
    if (!data_view) {
        gwy_tool_read_value_update_headers(tool);
        return;
    }

    gwy_plain_tool_assure_layer(plain_tool, tool->layer_type_point);
    gwy_plain_tool_set_selection_key(plain_tool, "pointer");
    g_object_set(plain_tool->layer, "draw-marker", FALSE, NULL);
    selection = gwy_vector_layer_get_selection(plain_tool->layer);
    gwy_selection_set_max_objects(selection, 1);
    tool->selection_id
        = g_signal_connect(selection, "changed",
                           G_CALLBACK(gwy_tool_read_value_selection_changed),
                           tool);

    gwy_tool_read_value_data_changed(plain_tool);
}

static void
gwy_tool_read_value_data_changed(GwyPlainTool *plain_tool)
{
    GwySelection *selection;
    GwyToolReadValue *tool;

    tool = GWY_TOOL_READ_VALUE(plain_tool);
    selection = gwy_vector_layer_get_selection(plain_tool->layer);

    gwy_tool_read_value_update_headers(tool);
    gwy_tool_read_value_selection_changed(selection, -1, tool);
}

static void
gwy_tool_read_value_selection_changed(GwySelection *selection,
                                      gint hint,
                                      GwyToolReadValue *tool)
{
    g_return_if_fail(hint <= 0);
    gwy_tool_read_value_update_values(tool);
}

static void
gwy_tool_read_value_radius_changed(G_GNUC_UNUSED GwyToolReadValue *tool)
{
}

static void
gwy_tool_read_value_update_headers(GwyToolReadValue *tool)
{
    GwyPlainTool *plain_tool;
    GString *str;

    plain_tool = GWY_PLAIN_TOOL(tool);
    str = g_string_new("");

    g_string_assign(str, "X");
    if (plain_tool->coord_format)
        g_string_append_printf(str, " [%s]", plain_tool->coord_format->units);
    gtk_label_set_markup(tool->xunits, str->str);

    g_string_assign(str, "Y");
    if (plain_tool->coord_format)
        g_string_append_printf(str, " [%s]", plain_tool->coord_format->units);
    gtk_label_set_markup(tool->yunits, str->str);

    g_string_assign(str, _("Value"));
    if (plain_tool->value_format)
        g_string_append_printf(str, " [%s]", plain_tool->value_format->units);
    gtk_label_set_markup(tool->zunits, str->str);

    g_string_free(str, TRUE);
}

static void
gwy_tool_read_value_update_value(GtkLabel *label,
                                 GwySIValueFormat *vf,
                                 gdouble val)
{
    gchar buf[32];

    if (vf)
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, val/vf->magnitude);
    else
        g_snprintf(buf, sizeof(buf), "%.3g", val);

    gtk_label_set_markup(label, buf);
}

static void
gwy_tool_read_value_update_values(GwyToolReadValue *tool)
{
    GwyPlainTool *plain_tool;
    GwySelection *selection;
    gboolean is_selected = FALSE;
    gdouble xy[2];
    gdouble xoff, yoff, val;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (plain_tool->data_field && plain_tool->layer) {
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        if (selection)
            is_selected = gwy_selection_get_object(selection, 0, xy);
    }

    if (is_selected) {
        xoff = gwy_data_field_get_xoffset(plain_tool->data_field);
        yoff = gwy_data_field_get_yoffset(plain_tool->data_field);

        gwy_tool_read_value_update_value(tool->x, plain_tool->coord_format,
                                         xy[0] + xoff);
        gwy_tool_read_value_update_value(tool->y, plain_tool->coord_format,
                                         xy[1] + yoff);
        /* FIXME
        value = gwy_unitool_get_z_average(dfield, xy[0], xy[1], radius);
        */
        val = gwy_data_field_get_dval_real(plain_tool->data_field, xy[0], xy[1],
                                           GWY_INTERPOLATION_ROUND);
        gwy_tool_read_value_update_value(tool->val, plain_tool->value_format,
                                         val);
    }
    else {
        gtk_label_set_text(tool->x, "");
        gtk_label_set_text(tool->y, "");
        gtk_label_set_text(tool->val, "");
    }
}

#if 0
static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerPoint";
    static GwyUnitoolState *state = NULL;

    if (!state) {
        func_slots.layer_type = g_type_from_name(layer_name);
        if (!func_slots.layer_type) {
            g_warning("Layer type `%s' not available", layer_name);
            return FALSE;
        }
        state = g_new0(GwyUnitoolState, 1);
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
    }
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    GwySelection *selection;

    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer,
                 "selection-key", "/0/select/pointer",
                 "draw-marker", FALSE,
                 NULL);
    selection = gwy_vector_layer_get_selection(state->layer);
    gwy_selection_set_max_objects(selection, 1);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *table, *label, *frame;
    GString *str;
    gint radius;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;

    dialog = gtk_dialog_new_with_buttons(_("Read Value"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(2, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);
    str = g_string_new("");

    controls->xunits = label = gtk_label_new(NULL);
    g_string_printf(str, "<b>X</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);

    controls->yunits = label = gtk_label_new(NULL);
    g_string_printf(str, "<b>Y</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_FILL, 0, 2, 2);

    controls->zunits = label = gtk_label_new(NULL);
    g_string_printf(str, "<b>%s</b> [%s]", _("Value"),
                    state->value_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 2, 2);

    g_string_free(str, TRUE);

    label = controls->x = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, 0, 0, 2, 2);
    label = controls->y = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, 0, 0, 2, 2);
    label = controls->val = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2,
                        GTK_EXPAND | GTK_FILL, 0, 2, 2);

    table = gtk_table_new(1, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    settings = gwy_app_settings_get();
    if (gwy_container_contains_by_name(settings, radius_key))
        radius = gwy_container_get_int32_by_name(settings, radius_key);
    else
        radius = 1;
    controls->radius = gtk_adjustment_new((gdouble)radius, 1, 16, 1, 5, 16);
    gwy_table_attach_spinbutton(table, 9, _("_Averaging radius:"), _("px"),
                                controls->radius);
    g_signal_connect_swapped(controls->radius, "value-changed",
                             G_CALLBACK(dialog_update), state);

    return dialog;
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    GwySelection *selection;
    GString *str;
    gdouble value, xoff, yoff, xy[2];
    gboolean is_visible, is_selected;
    gint radius;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;

    layer = GWY_DATA_VIEW_LAYER(state->layer);
    selection = gwy_vector_layer_get_selection(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->radius));

    is_visible = state->is_visible;
    is_selected = gwy_selection_get_object(selection, 0, xy);
    if (!is_visible && !is_selected)
        return;

    str = g_string_new("");
    g_string_printf(str, "<b>X</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->xunits), str->str);

    g_string_printf(str, "<b>Y</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->yunits), str->str);

    g_string_printf(str, "<b>%s</b> [%s]", _("Value"),
                    state->value_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->zunits), str->str);
    g_string_free(str, TRUE);

    if (is_selected) {
        xoff = gwy_data_field_get_xoffset(dfield);
        yoff = gwy_data_field_get_yoffset(dfield);
        gwy_unitool_update_label_no_units(state->coord_hformat,
                                          controls->x, xy[0] + xoff);
        gwy_unitool_update_label_no_units(state->coord_hformat,
                                          controls->y, xy[1] + yoff);
        value = gwy_unitool_get_z_average(dfield, xy[0], xy[1], radius);
        gwy_unitool_update_label_no_units(state->value_hformat,
                                          controls->val, value);
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls->x), "");
        gtk_label_set_text(GTK_LABEL(controls->y), "");
        gtk_label_set_text(GTK_LABEL(controls->val), "");
    }
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    GwyContainer *settings;
    ToolControls *controls;
    gint radius;

    controls = (ToolControls*)state->user_data;
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->radius));
    radius = CLAMP(radius, 1, 16);
    settings = gwy_app_settings_get();
    gwy_container_set_int32_by_name(settings, radius_key, radius);

    memset(state->user_data, 0, sizeof(ToolControls));
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
