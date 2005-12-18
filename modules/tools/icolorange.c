/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwylayer-basic.h>
#include <app/app.h>
#include <app/settings.h>
#include <app/unitool.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef enum {
    USE_SELECTION = 0,
    USE_HISTOGRAM
} IColorRangeSource;

typedef struct {
    GwyUnitoolRectLabels labels;
    GtkWidget *histogram;
    GtkWidget *cmin;
    GtkWidget *cmax;
    GtkWidget *cdatamin;
    GtkWidget *cdatamax;
    IColorRangeSource range_source;
    gboolean in_update;
    gboolean initial_use;
    GwyDataLine *heightdist;
    GSList *modelist;
    /* storable state */
    GQuark key_min;
    GQuark key_max;
    GwyGraphModel *histogram_model;
} ToolControls;

static gboolean   module_register             (const gchar *name);
static gboolean   use                         (GwyDataWindow *data_window,
                                               GwyToolSwitchEvent reason);
static void       layer_setup                 (GwyUnitoolState *state);
static GtkWidget* dialog_create               (GwyUnitoolState *state);
static void       dialog_update               (GwyUnitoolState *state,
                                               GwyUnitoolUpdateType reason);
static void       dialog_abandon              (GwyUnitoolState *state);
static void       histogram_selection_changed (GwyUnitoolState *state);
static void       update_graph_selection      (GwyUnitoolState *state,
                                               const gdouble *selection);
static GwyLayerBasicRangeType get_range_type  (GwyUnitoolState *state);
static void       set_range_type            (GwyUnitoolState *state,
                                             GwyLayerBasicRangeType range_type);
static void       get_min_max                 (GwyUnitoolState *state,
                                               gdouble *selection);
static void       set_min_max                 (GwyUnitoolState *state,
                                               const gdouble *selection);
static void       range_mode_changed          (GtkWidget *button,
                                               GwyUnitoolState *state);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Interactive color range tool, allows to select data range false "
       "color scale should map to, either on data or on height distribution "
       "histogram."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    layer_setup,                   /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                 /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    NULL,                          /* apply action */
    NULL,                          /* nonstandard response handler */
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo icolorange_func_info = {
        "icolorange",
        GWY_STOCK_COLOR_RANGE,
        N_("Stretch color range to part of data."),
        &use,
    };

    gwy_tool_func_register(name, &icolorange_func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerRectangle";
    static GwyUnitoolState *state = NULL;
    ToolControls *controls;

    if (!state) {
        func_slots.layer_type = g_type_from_name(layer_name);
        if (!func_slots.layer_type) {
            g_warning("Layer type `%s' not available", layer_name);
            return FALSE;
        }
        state = g_new0(GwyUnitoolState, 1);
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
        state->apply_doesnt_close = TRUE;
    }
    controls = (ToolControls*)state->user_data;
    controls->initial_use = TRUE;
    controls->range_source = USE_SELECTION;

    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwySelection *selection;
    GwyDataView *view;
    GwyPixmapLayer *layer;
    const gchar *prefix;
    gchar *key;
    guint len;

    g_assert(CHECK_LAYER_TYPE(state->layer));
    controls = (ToolControls*)state->user_data;
    g_object_set(state->layer,
                 "selection-key", "/0/select/rectangle",
                 "is-crop", FALSE,
                 NULL);
    selection = gwy_vector_layer_get_selection(state->layer);

    view = gwy_data_window_get_data_view(state->data_window);
    layer = gwy_data_view_get_base_layer(view);
    prefix = gwy_layer_basic_get_min_max_key(GWY_LAYER_BASIC(layer));
    if (!prefix) {
        /* TODO: Container */
        prefix = "/0/base";
        gwy_layer_basic_set_min_max_key(GWY_LAYER_BASIC(layer), prefix);
    }
    len = strlen(prefix);
    key = g_newa(gchar, len + sizeof("/min"));
    g_stpcpy(g_stpcpy(key, prefix), "/min");
    controls->key_min = g_quark_from_string(key);
    key[len + 2] = 'a';
    key[len + 3] = 'x';
    controls->key_max = g_quark_from_string(key);

    if (controls->modelist)
        gwy_radio_buttons_set_current(controls->modelist, "range-type",
                                      get_range_type(state));
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    static const GwyEnum range_modes[] = {
        { N_("Full"),     GWY_LAYER_BASIC_RANGE_FULL,  },
        { N_("Fixed"),    GWY_LAYER_BASIC_RANGE_FIXED, },
        { N_("Auto"),     GWY_LAYER_BASIC_RANGE_AUTO,  },
        { N_("Adaptive"), GWY_LAYER_BASIC_RANGE_ADAPT, },
    };
    ToolControls *controls;
    GtkWidget *dialog, *table, *frame, *label, *hbox, *button;
    GwyGraph *graph;
    GwyGraphCurveModel *cmodel;
    GSList *modelist, *l;
    gint row;

    controls = (ToolControls*)state->user_data;

    dialog = gtk_dialog_new_with_buttons(_("Color Range"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    hbox = gtk_hbox_new(TRUE, 0);
    modelist = gwy_radio_buttons_create(range_modes, G_N_ELEMENTS(range_modes),
                                        "range-type",
                                        G_CALLBACK(range_mode_changed), state,
                                        get_range_type(state));
    for (l = modelist; l; l = g_slist_next(l)) {
        button = GTK_WIDGET(l->data);
        gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);
    }
    controls->modelist = modelist;
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    controls->heightdist = gwy_data_line_new(1.0, 1.0, TRUE);
    cmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_description(cmodel,
                                          _("Height histogram"));
    gwy_graph_curve_model_set_curve_type(cmodel, GWY_GRAPH_CURVE_LINE);

    controls->histogram_model = gwy_graph_model_new();
    gwy_graph_model_add_curve(controls->histogram_model, cmodel);
    controls->histogram = gwy_graph_new(controls->histogram_model);
    graph = GWY_GRAPH(controls->histogram);
    gwy_graph_model_set_label_visible(controls->histogram_model, FALSE);
    gwy_graph_set_axis_visible(graph, GTK_POS_TOP, FALSE);
    gwy_graph_set_axis_visible(graph, GTK_POS_BOTTOM, FALSE);
    gwy_graph_set_axis_visible(graph, GTK_POS_LEFT, FALSE);
    gwy_graph_set_axis_visible(graph, GTK_POS_RIGHT, FALSE);
    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_XSEL);
    gwy_graph_area_set_selection_limit(GWY_GRAPH_AREA(gwy_graph_get_area(graph)), 1);
    gwy_graph_enable_user_input(graph, FALSE);
    gtk_widget_set_size_request(controls->histogram, 240, 160);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), controls->histogram,
                       TRUE, TRUE, 2);
    g_signal_connect_swapped(controls->histogram, "selected",
                             G_CALLBACK(histogram_selection_changed), state);

    table = gtk_table_new(2, 3, TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 0);
    row = 0;

    controls->cmin = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->cmin), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls->cmin, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    controls->cmax = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->cmax), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls->cmax, 3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("Range"));
    gtk_table_attach(GTK_TABLE(table), label, 1, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls->cdatamin = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->cdatamin), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls->cdatamin, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    controls->cdatamax = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->cdatamax), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls->cdatamax, 3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("Full"));
    gtk_table_attach(GTK_TABLE(table), label, 1, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    table = gtk_table_new(8, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;
    row += gwy_unitool_rect_info_table_setup(&controls->labels,
                                             GTK_TABLE(table), 0, row);
    controls->labels.unselected_is_full = TRUE;

    return dialog;
}

static void
dialog_update(GwyUnitoolState *state,
              GwyUnitoolUpdateType reason)
{
    GwyGraph *graph;
    gboolean is_visible;
    GwyContainer *data;
    GwyDataField *dfield;
    ToolControls *controls;
    GwyGraphCurveModel *cmodel;
    GwyLayerBasicRangeType range_type;
    gboolean has_selection = FALSE;
    gint isel[4];
    gdouble min, max, selection[2];

    controls = (ToolControls*)state->user_data;
    if (controls->in_update)
        return;

    gwy_debug("%d (initial: %d)", reason, controls->initial_use);
    controls->in_update = TRUE;
    is_visible = state->is_visible;
    if (controls->initial_use) {
        reason = GWY_UNITOOL_UPDATED_DATA;
        controls->initial_use = FALSE;
    }

    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    graph = GWY_GRAPH(controls->histogram);
    range_type = get_range_type(state);

    if (reason == GWY_UNITOOL_UPDATED_DATA) {
        cmodel = gwy_graph_model_get_curve_by_index(controls->histogram_model,
                                                    0);
        gwy_data_field_dh(dfield, controls->heightdist, 0);
        gwy_graph_curve_model_set_data_from_dataline(cmodel,
                                                     controls->heightdist,
                                                     0, 0);
    }
    else if (reason == GWY_UNITOOL_UPDATED_SELECTION)
        controls->range_source = USE_SELECTION;
    else if (reason == GWY_UNITOOL_UPDATED_CONTROLS)
        controls->range_source = USE_HISTOGRAM;

    min = selection[0] = gwy_data_field_get_min(dfield);
    max = selection[1] = gwy_data_field_get_max(dfield);

    if (range_type == GWY_LAYER_BASIC_RANGE_FIXED) {
        if (gwy_graph_area_get_selection_number(GWY_GRAPH_AREA(gwy_graph_get_area(graph)))) {
            gwy_graph_area_get_selection(GWY_GRAPH_AREA(gwy_graph_get_area(graph)), selection);
            has_selection = TRUE;
        }

        if (controls->range_source == USE_SELECTION) {
            if (gwy_unitool_rect_info_table_fill(state, &controls->labels,
                                                 NULL, isel))
                has_selection = TRUE;
            selection[0] = gwy_data_field_area_get_min(dfield,
                                                       isel[0], isel[1],
                                                       isel[2] - isel[0],
                                                       isel[3] - isel[1]);
            selection[1] = gwy_data_field_area_get_max(dfield,
                                                       isel[0], isel[1],
                                                       isel[2] - isel[0],
                                                       isel[3] - isel[1]);
            update_graph_selection(state, selection);
        }
        if (has_selection)
            set_min_max(state, selection);
        else
            set_min_max(state, NULL);
    }
    else {
        get_min_max(state, selection);
        update_graph_selection(state, selection);
    }

    gwy_unitool_update_label(state->value_format, controls->cmin, selection[0]);
    gwy_unitool_update_label(state->value_format, controls->cmax, selection[1]);
    gwy_unitool_update_label(state->value_format, controls->cdatamin, min);
    gwy_unitool_update_label(state->value_format, controls->cdatamax, max);

    controls->in_update = FALSE;
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    static const gchar *do_preview_key = "/tool/icolorange/do_preview";
    static const gchar *rel_min_key = "/tool/icolorange/rel_min";
    static const gchar *rel_max_key = "/tool/icolorange/rel_max";

    GwyContainer *settings;
    ToolControls *controls;

    settings = gwy_app_settings_get();
    controls = (ToolControls*)state->user_data;
    gwy_object_unref(controls->heightdist);

    /* TODO: remove someday */
    gwy_container_remove_by_name(settings, do_preview_key);
    gwy_container_remove_by_name(settings, rel_min_key);
    gwy_container_remove_by_name(settings, rel_max_key);

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
histogram_selection_changed(GwyUnitoolState *state)
{
    ToolControls *controls;

    controls = (ToolControls*)state->user_data;
    if (controls->in_update)
        return;

    dialog_update(state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
update_graph_selection(GwyUnitoolState *state,
                       const gdouble *selection)
{
    ToolControls *controls;
    GwyGraph *graph;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble min, max;

    controls = (ToolControls*)state->user_data;
    graph = GWY_GRAPH(controls->histogram);
    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    min = gwy_data_field_get_min(dfield);
    max = gwy_data_field_get_max(dfield);

    if (min == selection[0] && max == selection[1])
        gwy_graph_area_clear_selection(GWY_GRAPH_AREA(gwy_graph_get_area(graph)));
    else
        gwy_graph_area_set_selection(GWY_GRAPH_AREA(gwy_graph_get_area(graph)),
                                     GWY_GRAPH_STATUS_XSEL, selection, 1);
}

static GwyLayerBasicRangeType
get_range_type(GwyUnitoolState *state)
{
    GwyDataView *view;
    GwyPixmapLayer *layer;
    const gchar *key;
    GwyContainer *container;
    GwyLayerBasicRangeType range_type = GWY_LAYER_BASIC_RANGE_FULL;

    view = gwy_data_window_get_data_view(state->data_window);
    container = gwy_data_view_get_data(view);
    layer = gwy_data_view_get_base_layer(view);
    key = gwy_layer_basic_get_range_type_key(GWY_LAYER_BASIC(layer));
    if (key)
        gwy_container_gis_enum_by_name(container, key, &range_type);

    return range_type;
}

static void
set_range_type(GwyUnitoolState *state,
               GwyLayerBasicRangeType range_type)
{
    GwyDataView *view;
    GwyPixmapLayer *layer;
    const gchar *key;
    GwyContainer *container;
    GwyLayerBasicRangeType old_range_type = GWY_LAYER_BASIC_RANGE_FULL;

    view = gwy_data_window_get_data_view(state->data_window);
    container = gwy_data_view_get_data(view);
    layer = gwy_data_view_get_base_layer(view);
    key = gwy_layer_basic_get_range_type_key(GWY_LAYER_BASIC(layer));
    if (key)
        gwy_container_gis_enum_by_name(container, key, &old_range_type);
    if (range_type == old_range_type)
        return;

    if (!key) {
        /* TODO: Container */
        key = "/0/data/range-type";
        gwy_layer_basic_set_range_type_key(GWY_LAYER_BASIC(layer), key);
    }
    gwy_container_set_enum_by_name(container, key, range_type);
}

static void
get_min_max(GwyUnitoolState *state,
            gdouble *selection)
{
    ToolControls *controls;
    GwyDataView *view;
    GwyContainer *container;
    GwyDataField *dfield;

    controls = (ToolControls*)state->user_data;
    view = gwy_data_window_get_data_view(state->data_window);
    container = gwy_data_view_get_data(view);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(container,
                                                             "/0/data"));
    if (!gwy_container_gis_double(container, controls->key_min, &selection[0]))
        selection[0] = gwy_data_field_get_min(dfield);
    if (!gwy_container_gis_double(container, controls->key_max, &selection[1]))
        selection[1] = gwy_data_field_get_max(dfield);
}

static void
set_min_max(GwyUnitoolState *state,
            const gdouble *selection)
{
    ToolControls *controls;
    GwyDataView *view;
    GwyContainer *container;

    controls = (ToolControls*)state->user_data;
    view = gwy_data_window_get_data_view(state->data_window);
    container = gwy_data_view_get_data(view);
    if (selection) {
        gwy_container_set_double(container, controls->key_min, selection[0]);
        gwy_container_set_double(container, controls->key_max, selection[1]);
    }
    else {
        gwy_container_remove(container, controls->key_min);
        gwy_container_remove(container, controls->key_max);
    }
}

static void
range_mode_changed(G_GNUC_UNUSED GtkWidget *button,
                   GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyLayerBasicRangeType range_type;

    controls = (ToolControls*)state->user_data;
    range_type = gwy_radio_buttons_get_current(controls->modelist,
                                               "range-type");
    gtk_widget_set_sensitive(controls->histogram,
                             range_type == GWY_LAYER_BASIC_RANGE_FIXED);
    set_range_type(state, range_type);
    dialog_update(state, GWY_UNITOOL_UPDATED_DATA);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

