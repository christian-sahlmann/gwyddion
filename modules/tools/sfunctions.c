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

#include <string.h>
#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/unitool.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef struct {
    GwyUnitoolState *state;
    GtkWidget *graph;
    GtkWidget *interpolation;
    GtkWidget *output;
    GtkWidget *direction;
    GtkWidget *xy;
    GtkWidget *wh;
    GwyInterpolationType interp;
    GwySFOutputType out;
    GtkOrientation dir;
} ToolControls;

static gboolean   module_register      (const gchar *name);
static gboolean   use                  (GwyDataWindow *data_window,
                                        GwyToolSwitchEvent reason);
static void       layer_setup          (GwyUnitoolState *state);
static GtkWidget* dialog_create        (GwyUnitoolState *state);
static void       dialog_update        (GwyUnitoolState *state,
                                        GwyUnitoolUpdateType reason);
static void       dialog_abandon       (GwyUnitoolState *state);
static void       apply                (GwyUnitoolState *state);
static void       interp_changed_cb    (GObject *item,
                                        ToolControls *controls);
static void       output_changed_cb    (GObject *item,
                                        ToolControls *controls);
static void       direction_changed_cb (GObject *item,
                                        ToolControls *controls);
static void       load_args            (GwyContainer *container,
                                        ToolControls *controls);
static void       save_args            (GwyContainer *container,
                                        ToolControls *controls);
static void       get_selection_or_all (GwyDataField *dfield,
                                        GwyVectorLayer *layer,
                                        gdouble *xmin, gdouble *ymin,
                                        gdouble *xmax, gdouble *ymax);

static const gchar *interp_key = "/tool/sfunctions/interp";
static const gchar *out_key = "/tool/sfunctions/out";
static const gchar *dir_key = "/tool/sfunctions/dir";

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "sfunctions",
    "Statistical functions.",
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
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

static const GwyEnum sf_types[] =  {
    { "Height distribution", GWY_SF_OUTPUT_DH },
    { "Height distribution", GWY_SF_OUTPUT_CDH },
    { "Slope distribution",  GWY_SF_OUTPUT_DA },
    { "Slope distribution",  GWY_SF_OUTPUT_CDA },
    { "ACF",                 GWY_SF_OUTPUT_ACF },
    { "HHCF",                GWY_SF_OUTPUT_HHCF },
    { "PSDF",                GWY_SF_OUTPUT_PSDF },
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo func_info = {
        "sfunctions",
        "gwy_graph_halfgauss",
        "Compute 1D statistical functions.",
        77,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerSelect";
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
    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer, "is_crop", FALSE, NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *table, *label, *vbox, *frame;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    dialog = gtk_dialog_new_with_buttons(_("Statistical functions"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLEAR,
                                         GWY_UNITOOL_RESPONSE_UNSELECT,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         _("_Hide"), GTK_RESPONSE_CLOSE,
                                         NULL);

    table = gtk_table_new(2, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    vbox = gtk_vbox_new(FALSE, 0);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Area of computation</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Origin: (x, y)"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    controls->xy = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->xy), 1.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), controls->xy, FALSE, FALSE, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Size: (w x h)"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    controls->wh = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->wh), 1.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), controls->wh, FALSE, FALSE, 0);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Module parameters</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 10);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 2);

    controls->output
        = gwy_option_menu_sfunctions_output(G_CALLBACK(output_changed_cb),
                                            controls, controls->out);
    gtk_box_pack_start(GTK_BOX(vbox), controls->output, FALSE, FALSE, 2);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Computation direction:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 2);

    controls->direction
        = gwy_option_menu_direction(G_CALLBACK(direction_changed_cb),
                                    controls, controls->dir);
    gtk_box_pack_start(GTK_BOX(vbox), controls->direction, FALSE, FALSE, 2);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 2);

    controls->interpolation
        = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                        controls, controls->interp);
    gtk_box_pack_start(GTK_BOX(vbox), controls->interpolation, FALSE, FALSE, 2);

    gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 0, 1,
                     GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);

    controls->graph = gwy_graph_new();
    gtk_table_attach(GTK_TABLE(table), controls->graph, 1, 2, 0, 1,
                     GTK_FILL, 0, 2, 2);

    return dialog;
}

static void
update_labels(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyUnitoolUnits *units;
    GwyDataViewLayer *layer;
    gdouble xmin, xmax, ymin, ymax;
    gchar buffer[64];

    controls = (ToolControls*)state->user_data;
    units = &state->coord_units;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    get_selection_or_all(dfield, state->layer, &xmin, &ymin, &xmax, &ymax);

    g_snprintf(buffer, sizeof(buffer), "%.*f, %.*f %s",
               units->precision, xmin/units->mag,
               units->precision, ymin/units->mag,
               units->units);
    gtk_label_set_text(GTK_LABEL(controls->xy), buffer);

    g_snprintf(buffer, sizeof(buffer), "%.*f × %.*f %s",
               units->precision, fabs(xmax-xmin)/units->mag,
               units->precision, fabs(ymax-ymin)/units->mag,
               units->units);
    gtk_label_set_text(GTK_LABEL(controls->wh), buffer);
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataLine *dataline;
    GwyDataViewLayer *layer;
    gint xm1, xm2, ym1, ym2;
    GwyGraphAutoProperties prop;
    GString *lab;
    gdouble xmin, ymin, xmax, ymax;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    get_selection_or_all(dfield, state->layer, &xmin, &ymin, &xmax, &ymax);

    /* XXX */
    if (!state->is_visible)
        return;

    gwy_graph_get_autoproperties(GWY_GRAPH(controls->graph), &prop);
    prop.is_point = FALSE;
    prop.is_line = TRUE;
    gwy_graph_set_autoproperties(GWY_GRAPH(controls->graph), &prop);
    gwy_graph_clear(GWY_GRAPH(controls->graph));

    xm1 = ROUND(gwy_data_field_rtoj(dfield, xmin));
    ym1 = ROUND(gwy_data_field_rtoj(dfield, ymin));
    xm2 = ROUND(gwy_data_field_rtoj(dfield, xmax));
    ym2 = ROUND(gwy_data_field_rtoj(dfield, ymax));

    dataline = GWY_DATA_LINE(gwy_data_line_new(10, 10, FALSE));
    lab = g_string_new(gwy_enum_to_string(controls->out,
                                          sf_types, G_N_ELEMENTS(sf_types)));

    if (gwy_data_field_get_line_stat_function(dfield, dataline,
                                              xm1, ym1,
                                              xm2, ym2,
                                              controls->out,
                                              controls->dir,
                                              controls->interp,
                                              GWY_WINDOWING_HANN,
                                              100)) {
        gwy_graph_add_dataline(GWY_GRAPH(controls->graph), dataline,
                               0, lab, NULL);
        update_labels(state);
    }
    g_string_free(lab, TRUE);
    g_object_unref(dataline);
}

static void
apply(GwyUnitoolState *state)
{
    ToolControls *controls;
    GtkWidget *window, *graph;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyGraphAutoProperties prop;
    GwyDataLine *dataline;
    GwyDataViewLayer *layer;
    gint xm1, xm2, ym1, ym2;
    GString *lab;
    gdouble xmin, ymin, xmax, ymax;

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    get_selection_or_all(dfield, state->layer, &xmin, &ymin, &xmax, &ymax);

    graph = gwy_graph_new();

    gwy_graph_get_autoproperties(GWY_GRAPH(graph), &prop);
    prop.is_point = 0;
    prop.is_line = 1;
    gwy_graph_set_autoproperties(GWY_GRAPH(graph), &prop);

    xm1 = ROUND(gwy_data_field_rtoj(dfield, xmin));
    ym1 = ROUND(gwy_data_field_rtoj(dfield, ymin));
    xm2 = ROUND(gwy_data_field_rtoj(dfield, xmax));
    ym2 = ROUND(gwy_data_field_rtoj(dfield, ymax));

    dataline = (GwyDataLine *)gwy_data_line_new(10, 10, 0);
    lab = g_string_new(gwy_enum_to_string(controls->out,
                                          sf_types, G_N_ELEMENTS(sf_types)));

    if (gwy_data_field_get_line_stat_function(dfield, dataline,
                                              xm1, ym1,
                                              xm2, ym2,
                                              controls->out,
                                              controls->dir,
                                              controls->interp,
                                              GWY_WINDOWING_HANN,
                                              100))
        gwy_graph_add_dataline(GWY_GRAPH(graph), dataline, 0, lab, NULL);

    window = gwy_app_graph_window_create(graph);
    g_string_free(lab, TRUE);
    g_object_unref(dataline);
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    save_args(settings, controls);

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
interp_changed_cb(GObject *item, ToolControls *controls)
{
    gwy_debug("");
    controls->interp
        = GPOINTER_TO_INT(g_object_get_data(item, "interpolation-type"));
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
output_changed_cb(GObject *item, ToolControls *controls)
{
    gwy_debug("");
    controls->out = GPOINTER_TO_INT(g_object_get_data(item, "sf-output-type"));
    printf("controls->out = %d\n", controls->out);
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
direction_changed_cb(GObject *item, ToolControls *controls)
{
    gwy_debug("");
    controls->dir = GPOINTER_TO_INT(g_object_get_data(item, "direction-type"));
    dialog_update(controls->state, GWY_UNITOOL_UPDATED_CONTROLS);
}


static void
load_args(GwyContainer *container, ToolControls *controls)
{
    gwy_debug("");
    if (gwy_container_contains_by_name(container, dir_key))
        controls->dir = gwy_container_get_int32_by_name(container, dir_key);
    else
        controls->dir = GTK_ORIENTATION_HORIZONTAL;

    if (gwy_container_contains_by_name(container, out_key))
        controls->out = gwy_container_get_int32_by_name(container, out_key);
    else
        controls->out = GWY_SF_OUTPUT_DH;

    if (gwy_container_contains_by_name(container, interp_key))
        controls->interp = gwy_container_get_int32_by_name(container,
                                                           interp_key);
    else
        controls->interp = GWY_INTERPOLATION_BILINEAR;
}

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_int32_by_name(container, interp_key, controls->interp);
    gwy_container_set_int32_by_name(container, dir_key, controls->dir);
    gwy_container_set_int32_by_name(container, out_key, controls->out);
}

static void
get_selection_or_all(GwyDataField *dfield,
                     GwyVectorLayer *layer,
                     gdouble *xmin, gdouble *ymin,
                     gdouble *xmax, gdouble *ymax)
{
    gdouble xy[4];
    gboolean is_selected;

    is_selected = gwy_vector_layer_get_selection(layer, xy);

    if (is_selected) {
        *xmin = xy[0];
        *ymin = xy[1];
        *xmax = xy[2];
        *ymax = xy[3];
    }
    else {
        *xmin = 0;
        *ymin = 0;
        *xmax = gwy_data_field_get_xreal(dfield);
        *ymax = gwy_data_field_get_yreal(dfield);
    }
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

