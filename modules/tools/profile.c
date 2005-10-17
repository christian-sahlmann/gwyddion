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
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/unitool.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

#define MAX_WIDTH 100

enum {
    NPROFILE = 4
};

typedef struct {
    GwyUnitoolState *state;
    GtkWidget *graph;
    GwyGraphModel *graphmodel;
    GtkWidget *interpolation;
    GtkWidget *separation;
    GPtrArray *positions;
    GPtrArray *dtl;
    GPtrArray *str;
    GtkObject *linesize;
    GtkObject *nofpoints;
    GtkWidget *isnofpoints;
    gint size;
    gint interp;
    gint npoints;
    gboolean separate;
    gboolean isnpoints;
} ToolControls;

static gboolean   module_register     (const gchar *name);
static gboolean   use                 (GwyDataWindow *data_window,
                                       GwyToolSwitchEvent reason);
static void       layer_setup         (GwyUnitoolState *state);
static GtkWidget* dialog_create       (GwyUnitoolState *state);
static void       dialog_update       (GwyUnitoolState *state,
                                       GwyUnitoolUpdateType reason);
static void       dialog_abandon      (GwyUnitoolState *state);
static void       apply               (GwyUnitoolState *state);
static void       interp_changed_cb   (GtkWidget *combo,
                                       ToolControls *controls);
static void       separate_changed_cb (GtkToggleButton *button,
                                       ToolControls *controls);
static void       size_changed_cb     (ToolControls *controls);
static void       npoints_changed_cb  (ToolControls *controls);
static void       load_args           (GwyContainer *container,
                                       ToolControls *controls);
static void       save_args           (GwyContainer *container,
                                       ToolControls *controls);
static void       isnofpoints_changed_cb (GtkToggleButton *button,
                                       ToolControls *controls);



/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Profile tool, creates profile graphs from selected lines."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
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
    static GwyToolFuncInfo func_info = {
        "profile",
        GWY_STOCK_PROFILE,
        N_("Extract profiles from data."),
        80,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerLine";
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
    ((ToolControls*)state->user_data)->state = state;
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    GwySelection *selection;

    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer,
                 "selection-key", "/0/select/line",
                 "line-numbers", TRUE,
                 NULL);
    selection = gwy_vector_layer_get_selection(state->layer);
    gwy_selection_set_max_objects(selection, NPROFILE);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    const gchar *headers[] = {
        "<b>x<sub>1</sub></b>",
        "<b>y<sub>1</sub></b>",
        "<b>x<sub>2</sub></b>",
        "<b>y<sub>2</sub></b>",
    };
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *table, *label, *hbox, *vbox, *frame;
    GtkSizeGroup *sizegroup;
    GString *str;
    gint i, j, row;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    dialog = gtk_dialog_new_with_buttons(_("Extract Profile"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_clear(dialog);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    table = gtk_table_new(3 + NPROFILE, 5, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_col_spacing(GTK_TABLE(table), 0, 12);
    gtk_table_set_col_spacing(GTK_TABLE(table), 2, 12);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label),
                         _("<b>Profile Positions</b> [pixels]"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 5, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    for (j = 0; j < 4; j++) {
        label = gtk_label_new(NULL);
        gtk_size_group_add_widget(sizegroup, label);
        gtk_label_set_markup(GTK_LABEL(label), headers[j]);
        gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, j+1, j+2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
    }
    row++;

    controls->dtl = g_ptr_array_new();
    controls->str = g_ptr_array_new();
    controls->positions = g_ptr_array_new();
    for (i = 0; i < NPROFILE; i++) {
        /* Don't free, graph eats them */
        str = g_string_new("");

        g_string_printf(str, "<b>%d</b>", i+1);
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), str->str);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, row+i, row+i+1,
                         0, 0, 2, 2);

        for (j = 0; j < 4; j++) {
            label = gtk_label_new(NULL);
            gtk_size_group_add_widget(sizegroup, label);
            gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
            gtk_table_attach(GTK_TABLE(table), label, j+1, j+2, row+i, row+i+1,
                             GTK_EXPAND | GTK_FILL, 0, 2, 2);
            g_ptr_array_add(controls->positions, label);
        }

        g_string_printf(str, _("Profile %d"), i+1);
        g_ptr_array_add(controls->str, str);
        g_ptr_array_add(controls->dtl, gwy_data_line_new(10, 10, 0));
    }
    g_object_unref(sizegroup);
    row += NPROFILE;
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    table = gtk_table_new(5, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_col_spacing(GTK_TABLE(table), 2, 8);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls->linesize = gtk_adjustment_new(controls->size,
                                            1, MAX_WIDTH, 1, 5, 0);
    gwy_table_attach_spinbutton(table, row, _("Thickness:"), "px",
                                controls->linesize);
    g_signal_connect_swapped(controls->linesize, "value-changed",
                             G_CALLBACK(size_changed_cb), controls);
    row++;

    controls->nofpoints = gtk_adjustment_new(controls->npoints, 10, 100, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("Fix res.:"), NULL,
                                controls->nofpoints, GWY_HSCALE_CHECK);
    g_signal_connect_swapped(controls->nofpoints, "value-changed",
                             G_CALLBACK(npoints_changed_cb), controls);
    controls->isnofpoints = g_object_get_data(G_OBJECT(controls->nofpoints),
                                              "check");
    g_signal_connect(controls->isnofpoints, "toggled",
                            G_CALLBACK(isnofpoints_changed_cb), controls);

    gwy_table_hscale_set_sensitive(controls->nofpoints,
                                         controls->isnpoints);

    row++;

    controls->separation
        = gtk_check_button_new_with_mnemonic(_("_Separate profiles"));
    gtk_table_attach(GTK_TABLE(table), controls->separation, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->separation),
                                 controls->separate);
    g_signal_connect(controls->separation, "toggled",
                     G_CALLBACK(separate_changed_cb), controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 4);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls->interpolation
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(interp_changed_cb), controls,
                                 controls->interp, TRUE);
    gtk_table_attach(GTK_TABLE(table), controls->interpolation,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    controls->graphmodel = gwy_graph_model_new();
    controls->graph = gwy_graph_new(controls->graphmodel);
    gtk_widget_set_size_request(controls->graph, 400, 150);
    gwy_graph_enable_user_input(GWY_GRAPH(controls->graph), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), controls->graph, TRUE, TRUE, 0);

    return dialog;
}

static void
update_labels(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    GwySelection *selection;
    gdouble lines[4*NPROFILE];
    GPtrArray *positions;
    GString *str;
    gint i, nselected;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    selection = gwy_vector_layer_get_selection(state->layer);
    nselected = gwy_selection_get_data(selection, lines);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    positions = controls->positions;

    gwy_debug("%d lines", nselected);
    str = g_string_new("");
    for (i = 0; i < 4*NPROFILE; i++) {
        if (i < 4*nselected) {
            if (i % 2)
                g_string_printf(str, "%d",
                                (gint)gwy_data_field_rtoj(dfield, lines[i]));
            else
                g_string_printf(str, "%d",
                                (gint)gwy_data_field_rtoi(dfield, lines[i]));
            gtk_label_set_markup(GTK_LABEL(positions->pdata[i]), str->str);
        }
        else
            gtk_label_set_markup(GTK_LABEL(positions->pdata[i]), "");
    }
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
    gdouble line[4];
    gboolean is_visible;
    gint nselected, i, lineres;
    gint xl1, xl2, yl1, yl2;
    GwyGraphCurveModel *gcmodel;
    GwyRGBA color;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    is_visible = state->is_visible;
    selection = gwy_vector_layer_get_selection(state->layer);
    nselected = gwy_selection_get_data(selection, NULL);
    if (!is_visible && !nselected)
        return;

    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    g_object_set(G_OBJECT(controls->nofpoints),
                 "upper",
                 (gdouble)(MAX(5*gwy_data_field_get_xres(dfield),
                     5*gwy_data_field_get_yres(dfield))),
                 NULL);

    gwy_graph_model_remove_all_curves(controls->graphmodel);
    gwy_graph_model_set_x_siunit(controls->graphmodel, dfield->si_unit_xy);
    gwy_graph_model_set_y_siunit(controls->graphmodel, dfield->si_unit_z);
    gwy_graph_model_set_title(controls->graphmodel, _("Profiles"));

    controls->isnpoints = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->isnofpoints));

    if (nselected) {
        for (i = 0; i < nselected; i++) {
            gwy_selection_get_object(selection, i, line);
            xl1 = gwy_data_field_rtoj(dfield, line[0]);
            yl1 = gwy_data_field_rtoi(dfield, line[1]);
            xl2 = gwy_data_field_rtoj(dfield, line[2]);
            yl2 = gwy_data_field_rtoi(dfield, line[3]);

            if (!controls->isnpoints) {
                lineres = ROUND(hypot(xl1 - xl2, yl1 - yl2));
                lineres = MAX(lineres, 10);
            }
            else
                lineres = controls->npoints;

            gwy_data_field_get_data_line_averaged(dfield,
                                                  controls->dtl->pdata[i],
                                                  xl1, yl1, xl2, yl2,
                                                  lineres, controls->size,
                                                  controls->interp);
            gcmodel = gwy_graph_curve_model_new();
            gwy_graph_curve_model_set_curve_type(gcmodel, GWY_GRAPH_CURVE_LINE);
            gwy_graph_curve_model_set_data_from_dataline(gcmodel,
                                                         controls->dtl->pdata[i],
                                                         0, 0);
            gwy_graph_curve_model_set_description(gcmodel,
                                                  ((GString*)(controls->str->pdata[i]))->str);

            color.r = 0;
            color.g = 0;
            color.b = 0;
            color.a = 1;

            if (i==1) color.r = 1;
            else if (i==2) color.g = 1;
            else if (i==3) color.b = 1;

            gwy_graph_curve_model_set_curve_color(gcmodel, &color);
            gwy_graph_model_add_curve(controls->graphmodel, gcmodel);
            g_object_unref(gcmodel);
        }
    }
    update_labels(state);
    gwy_unitool_apply_set_sensitive(state, nselected);
}

static void
apply(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwySelection *selection;
    GwyContainer *data;
    GtkWidget *graph;
    GwyGraphModel *model;
    gint i, j, nselected;

    controls = (ToolControls*)state->user_data;
    selection = gwy_vector_layer_get_selection(state->layer);
    nselected = gwy_selection_get_data(selection, NULL);
    if (!nselected)
        return;

    j = 0;
    data = gwy_data_window_get_data(state->data_window);
    if (controls->separate) {
        for (i = 0; i < nselected; i++) {
            model = gwy_graph_model_new_alike(controls->graphmodel);
            gwy_graph_model_remove_all_curves(model);
            gwy_graph_model_add_curve(model, gwy_graph_model_get_curve_by_index(controls->graphmodel, i));
            gwy_graph_model_set_title(model, ((GString*)controls->str->pdata[i])->str);
             
            graph = gwy_graph_new(model);
            gwy_object_unref(model);
            gwy_app_graph_window_create(GWY_GRAPH(graph), data);
            
        }
    }
    else {
        model = gwy_graph_model_duplicate(controls->graphmodel);
        graph = gwy_graph_new(model);
        gwy_object_unref(model);
        gwy_app_graph_window_create(GWY_GRAPH(graph), data);
    }
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    gint i;
    ToolControls *controls;
    GwyContainer *settings;

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    save_args(settings, controls);

    for (i = 0; i < NPROFILE; i++)
        g_object_unref(controls->dtl->pdata[i]);

    g_ptr_array_free(controls->dtl, TRUE);
    g_ptr_array_free(controls->str, TRUE);
    g_ptr_array_free(controls->positions, TRUE);

    gwy_object_unref(controls->graphmodel);
    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
interp_changed_cb(GtkWidget *combo,
                  ToolControls *controls)
{
    controls->interp = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));

    gwy_debug("Interpolation set to %d\n", controls->interp);
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
separate_changed_cb(GtkToggleButton *button, ToolControls *controls)
{
    controls->separate = gtk_toggle_button_get_active(button);
}


static void
size_changed_cb(ToolControls *controls)
{
    controls->size
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->linesize));
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
npoints_changed_cb(ToolControls *controls)
{
    controls->npoints
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->nofpoints));
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
isnofpoints_changed_cb(GtkToggleButton *button, ToolControls *controls)
{
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}


static const gchar *separate_key = "/tool/profile/separate";
static const gchar *interp_key = "/tool/profile/interp";
static const gchar *size_key = "/tool/profile/size";
static const gchar *npoints_key = "/tool/profile/npoints";
static const gchar *isnpoints_key = "/tool/profile/isnpoints";


static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->separate = FALSE;
    controls->interp = GWY_INTERPOLATION_BILINEAR;
    controls->size = 1;
    controls->npoints = 500;
    controls->isnpoints = FALSE;

    gwy_container_gis_boolean_by_name(container, separate_key,
                                      &controls->separate);
    gwy_container_gis_boolean_by_name(container, isnpoints_key,
                                      &controls->isnpoints);
    gwy_container_gis_enum_by_name(container, interp_key, &controls->interp);
    gwy_container_gis_int32_by_name(container, size_key, &controls->size);
    gwy_container_gis_int32_by_name(container, npoints_key, &controls->npoints);
    /* sanitize */
    controls->separate = !!controls->separate;
    controls->interp = CLAMP(controls->interp,
                             GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    controls->size = CLAMP(controls->size, 1, MAX_WIDTH);

}

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_boolean_by_name(container, separate_key,
                                      controls->separate);
    gwy_container_set_boolean_by_name(container, isnpoints_key,
                                      controls->isnpoints);
    gwy_container_set_enum_by_name(container, interp_key, controls->interp);
    gwy_container_set_int32_by_name(container, size_key, controls->size);
    gwy_container_set_int32_by_name(container, npoints_key, controls->npoints);


}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

