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
#define DEBUG 1
#include <libgwyddion/gwymacros.h>
#include <string.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwygraph.h>
#include <app/app.h>
#include <app/settings.h>
#include <app/unitool.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef enum {
    USE_SELECTION = 1,
    USE_HISTOGRAM
} IColorRangeSource;

typedef struct {
    GwyUnitoolRectLabels labels;
    GtkWidget *cdo_preview;
    GtkWidget *histogram;
    GtkWidget *cmin;
    GtkWidget *cmax;
    GtkWidget *cdatamin;
    GtkWidget *cdatamax;
    gboolean in_update;
    gboolean initial_use;
    gdouble min;
    gdouble max;
    gdouble datamin;
    gdouble datamax;
    /* storable state */
    GQuark key_min;
    GQuark key_max;
    gboolean do_preview;
    IColorRangeSource range_source;
    gdouble rel_min;
    gdouble rel_max;
} ToolControls;

static gboolean   module_register             (const gchar *name);
static gboolean   use                         (GwyDataWindow *data_window,
                                               GwyToolSwitchEvent reason);
static void       layer_setup                 (GwyUnitoolState *state);
static GtkWidget* dialog_create               (GwyUnitoolState *state);
static void       dialog_update               (GwyUnitoolState *state,
                                               GwyUnitoolUpdateType reason);
static void       dialog_abandon              (GwyUnitoolState *state);
static void       apply                       (GwyUnitoolState *state);
static void       do_preview_updated          (GtkWidget *toggle,
                                               GwyUnitoolState *state);
static void       histogram_selection_changed (GwyUnitoolState *state);
static void       update_percentages          (ToolControls *controls);
static void       load_args                   (GwyContainer *container,
                                               ToolControls *controls);
static void       save_args                   (GwyContainer *container,
                                               ToolControls *controls);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "icolorange",
    N_("Interactive color range tool."),
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    layer_setup,                   /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                 /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    apply,                         /* apply action */
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
        "gwy_color_range",
        N_("Stretch color range to part of data."),
        130,
        &use,
    };

    gwy_tool_func_register(name, &icolorange_func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerSelect";
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
        controls = (ToolControls*)state->user_data;
        controls->key_min = g_quark_from_string("/0/base/min");
        controls->key_max = g_quark_from_string("/0/base/max");
    }
    ((ToolControls*)state->user_data)->initial_use = TRUE;
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer, "is_crop", FALSE, NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *table, *frame, *label;
    gint row;

    gwy_debug(" ");

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    dialog = gtk_dialog_new_with_buttons(_("Color Range"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(9, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    row = 0;

    controls->histogram = gwy_graph_new();
    /* XXX */
    gtk_widget_set_size_request(controls->histogram, 240, 160);
    gtk_table_attach(GTK_TABLE(table), controls->histogram, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gwy_graph_set_status(GWY_GRAPH(controls->histogram),
                         GWY_GRAPH_STATUS_XSEL);
    g_signal_connect_swapped(GWY_GRAPH(controls->histogram)->area, "selected",
                             G_CALLBACK(histogram_selection_changed), state);
    row++;

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
    gwy_unitool_update_label(state->value_format, controls->cdatamin,
                             controls->datamin);

    controls->cdatamax = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->cdatamax), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls->cdatamax, 3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gwy_unitool_update_label(state->value_format, controls->cdatamax,
                             controls->datamax);

    label = gtk_label_new(_("Full"));
    gtk_table_attach(GTK_TABLE(table), label, 1, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    row += gwy_unitool_rect_info_table_setup(&controls->labels,
                                             GTK_TABLE(table), 0, row);
    controls->labels.unselected_is_full = TRUE;

    controls->cdo_preview
        = gtk_check_button_new_with_mnemonic(_("_Instant apply"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->cdo_preview),
                                 controls->do_preview);
    g_signal_connect(controls->cdo_preview, "toggled",
                     G_CALLBACK(do_preview_updated), state);
    gtk_table_attach(GTK_TABLE(table), controls->cdo_preview, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

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
    gint isel[4];

    gwy_debug("%d", reason);

    controls = (ToolControls*)state->user_data;
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    is_visible = state->is_visible;

    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    graph = GWY_GRAPH(controls->histogram);

    /* XXX */
    if (controls->initial_use) {
        gtk_widget_hide(GTK_WIDGET(graph->axis_top));
        gtk_widget_hide(GTK_WIDGET(graph->axis_bottom));
        gtk_widget_hide(GTK_WIDGET(graph->axis_left));
        gtk_widget_hide(GTK_WIDGET(graph->axis_right));
        gtk_widget_hide(GTK_WIDGET(graph->corner_tl));
        gtk_widget_hide(GTK_WIDGET(graph->corner_bl));
        gtk_widget_hide(GTK_WIDGET(graph->corner_tr));
        gtk_widget_hide(GTK_WIDGET(graph->corner_br));
        gtk_widget_hide(GTK_WIDGET(graph->area->lab));
    }

    if (reason == GWY_UNITOOL_UPDATED_DATA || controls->initial_use) {
        GwyGraphAutoProperties prop;
        GwyDataLine *dataline;
        GString *graph_title;

        controls->datamin = gwy_data_field_get_min(dfield);
        controls->datamax = gwy_data_field_get_max(dfield);

        /* Update the curve even if not visible to get graph ranges right */
        gwy_graph_clear(graph);
        gwy_graph_get_autoproperties(GWY_GRAPH(graph), &prop);
        prop.is_point = 0;
        prop.is_line = 1;
        gwy_graph_set_autoproperties(GWY_GRAPH(graph), &prop);

        dataline = GWY_DATA_LINE(gwy_data_line_new(1, 1.0, FALSE));
        if (gwy_data_field_get_line_stat_function(dfield, dataline,
                                                0, 0,
                                                gwy_data_field_get_xres(dfield),
                                                gwy_data_field_get_yres(dfield),
                                                GWY_SF_OUTPUT_DH,
                                                0, 0,
                                                GWY_WINDOWING_HANN,
                                                220)) {
            /* XXX */
            graph_title = g_string_new("");
            gwy_graph_add_dataline(GWY_GRAPH(controls->histogram), dataline,
                                0, graph_title, NULL);
            g_string_free(graph_title, TRUE);
        }
        g_object_unref(dataline);
    }

    switch (reason) {
        case GWY_UNITOOL_UPDATED_SELECTION:
        if (!controls->initial_use)
            controls->range_source = USE_SELECTION;
        break;

        case GWY_UNITOOL_UPDATED_CONTROLS:
        controls->range_source = USE_HISTOGRAM;
        break;

        default:
        /* data was updated, so keep whatever source is set */
        break;
    }

    if (controls->range_source == USE_SELECTION) {
        if (gwy_unitool_rect_info_table_fill(state, &controls->labels,
                                             NULL, isel)) {
            controls->min = gwy_data_field_area_get_min(dfield,
                                                        isel[0], isel[1],
                                                        isel[2] - isel[0],
                                                        isel[3] - isel[1]);
            controls->max = gwy_data_field_area_get_max(dfield,
                                                        isel[0], isel[1],
                                                        isel[2] - isel[0],
                                                        isel[3] - isel[1]);
        }
        else {
            controls->min = controls->datamin;
            controls->max = controls->datamax;
        }
        update_percentages(controls);

        /* XXX */
        if (controls->rel_min == 0.0 && controls->rel_max == 1.0)
            gwy_graph_area_set_selection(graph->area,
                                         graph->area->x_min,
                                         graph->area->x_min);
        else {
            gdouble graph_min, graph_max, graph_range;

            graph_range = graph->area->x_max - graph->area->x_min;
            graph_min = controls->rel_min*graph_range + graph->area->x_min;
            graph_max = controls->rel_max*graph_range + graph->area->x_min;
            gwy_graph_area_set_selection(graph->area,
                                         graph_min, graph_max);
        }
    }

    if (controls->range_source == USE_HISTOGRAM) {
        GwyGraphStatus_SelData *fuck;

        fuck = (GwyGraphStatus_SelData*)gwy_graph_get_status_data(graph);
        gwy_debug("graph selection: [%d, %d]", fuck->scr_start, fuck->scr_end);
        if (fuck->scr_start == fuck->scr_end) {
            controls->min = controls->datamin;
            controls->max = controls->datamax;
            controls->rel_min = 0.0;
            controls->rel_max = 1.0;
        }
        else {
            controls->min = MIN(fuck->data_start, fuck->data_end);
            controls->max = MAX(fuck->data_end, fuck->data_start);
            controls->min += controls->datamin;
            controls->max += controls->datamin;
        }
        update_percentages(controls);
    }

    if (is_visible) {
        gwy_unitool_update_label(state->value_format, controls->cmin,
                                 controls->min);
        gwy_unitool_update_label(state->value_format, controls->cmax,
                                 controls->max);
        gwy_unitool_update_label(state->value_format, controls->cdatamin,
                                 controls->datamin);
        gwy_unitool_update_label(state->value_format, controls->cdatamax,
                                 controls->datamax);
    }

    controls->initial_use = FALSE;
    controls->in_update = FALSE;

    if (controls->do_preview)
        apply(state);
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    GwyContainer *settings;
    ToolControls *controls;

    settings = gwy_app_settings_get();
    controls = (ToolControls*)state->user_data;
    save_args(settings, controls);

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
apply(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;

    controls = (ToolControls*)state->user_data;
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    data = gwy_data_window_get_data(state->data_window);
    gwy_container_set_double(data, controls->key_min, controls->min);
    gwy_container_set_double(data, controls->key_max, controls->max);

    gwy_data_view_update
        (GWY_DATA_VIEW(gwy_data_window_get_data_view
                           (GWY_DATA_WINDOW(state->data_window))));
    controls->in_update = FALSE;
}

static void
do_preview_updated(GtkWidget *toggle, GwyUnitoolState *state)
{
    ToolControls *controls;

    controls = (ToolControls*)state->user_data;
    controls->do_preview
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));
    if (controls->do_preview)
        apply(state);
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
update_percentages(ToolControls *controls)
{
    gdouble range;

    range = controls->datamax - controls->datamin;
    if (range == 0.0
        || (controls->min == controls->datamin
            && controls->max == controls->datamax)) {
        controls->rel_min = 0.0;
        controls->rel_max = 1.0;
    }
    else {
        controls->rel_min = (controls->min - controls->datamin)/range;
        controls->rel_max = (controls->max - controls->datamin)/range;
    }
    gwy_debug("%f %f", controls->rel_min, controls->rel_max);
}

static const gchar *range_source_key = "/tool/icolorange/range_source";
static const gchar *do_preview_key = "/tool/icolorange/do_preview";
static const gchar *rel_min_key = "/tool/icolorange/rel_min";
static const gchar *rel_max_key = "/tool/icolorange/rel_max";

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_boolean_by_name(container, do_preview_key,
                                      controls->do_preview);
    gwy_container_set_enum_by_name(container, range_source_key,
                                   controls->range_source);
    gwy_container_set_double_by_name(container, rel_min_key,
                                     controls->rel_min);
    gwy_container_set_double_by_name(container, rel_max_key,
                                     controls->rel_max);
}

static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->do_preview = TRUE;
    controls->range_source = USE_SELECTION;
    controls->rel_min = 0.0;
    controls->rel_max = 1.0;

    gwy_container_gis_enum_by_name(container, range_source_key,
                                   &controls->range_source);
    gwy_container_gis_boolean_by_name(container, do_preview_key,
                                      &controls->do_preview);
    gwy_container_gis_double_by_name(container, rel_min_key,
                                     &controls->rel_min);
    gwy_container_gis_double_by_name(container, rel_max_key,
                                     &controls->rel_max);

    /* sanitize */
    controls->do_preview = !!controls->do_preview;
    controls->range_source = CLAMP(controls->range_source,
                                   USE_SELECTION, USE_HISTOGRAM);
    controls->rel_min = CLAMP(controls->rel_min, 0.0, 1.0);
    controls->rel_max = CLAMP(controls->rel_max, controls->rel_min, 1.0);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

