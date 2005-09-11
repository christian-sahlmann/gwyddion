/*
 *  Copyright (C) 2003,2004 Nenad Ocelic, David Necas (Yeti), Petr Klapetek.
 *  E-mail: ocelic@biochem.mpg.de, yeti@gwyddion.net, klapetek@gwyddion.net.
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
#include <math.h>
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

#define NLINES 4

typedef struct {
    GwyUnitoolState *state;
    GtkWidget *units[4];
    GtkWidget *positions[NLINES * 2];
    GtkWidget *vectors[NLINES * 2];
    GPtrArray *str;
} ToolControls;

static gboolean   module_register     (const gchar *name);
static gboolean   use                 (GwyDataWindow *data_window,
                                       GwyToolSwitchEvent reason);
static void       layer_setup         (GwyUnitoolState *state);
static GtkWidget* dialog_create       (GwyUnitoolState *state);
static void       dialog_update       (GwyUnitoolState *state,
                                       GwyUnitoolUpdateType reason);
static void       dialog_abandon      (GwyUnitoolState *state);



/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Distance measurement tool, measures distances and angles."),
    "Nenad Ocelic <ocelic _at_ biochem.mpg.de>",
    "1.2",
    "Nenad Ocelic & David Nečas (Yeti) & Petr Klapetek",
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
    static GwyToolFuncInfo func_info = {
        "distance",
        GWY_STOCK_DISTANCE,
        N_("Measure distances between points."),
        6,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerLines";
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
    g_object_set(state->layer,
                 "max_lines", NLINES,
                 "line_numbers", TRUE,
                 NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GtkWidget *dialog, *table, *label, *frame;
    GString *str;
    gint i;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;

    dialog = gtk_dialog_new_with_buttons(_("Distances"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_clear(dialog);
    gwy_unitool_dialog_add_button_hide(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(NLINES+1, 5, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);
    str = g_string_new("");

    controls->units[0] = label= gtk_label_new(NULL);
    g_string_printf(str, "<b>&#916;x</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_FILL, 0, 2, 2);

    controls->units[1] = label = gtk_label_new(NULL);
    g_string_printf(str, "<b>&#916;y</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 2, 2);

    controls->units[2] = label = gtk_label_new(NULL);
    /*g_string_printf(str, _("<b>   &#8736;</b> [ &#176; ]"));*/
    g_string_printf(str, _("<b>Angle</b> [deg]"));
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1, GTK_FILL, 0, 2, 2);

    controls->units[3] = label = gtk_label_new(NULL);
    g_string_printf(str, "<b>R</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 0, 1, GTK_FILL, 0, 2, 2);


    controls->str = g_ptr_array_new();

    for (i = 0; i < NLINES; i++) {
        label = gtk_label_new(NULL);
        g_string_printf(str, "<b>%d</b>", i+1);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_label_set_markup(GTK_LABEL(label), str->str);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, i+1, i+2, 0, 0, 2, 2);
        label = controls->positions[2*i] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 1, 2, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
        label = controls->positions[2*i + 1] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 2, 3, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
        label = controls->vectors[2*i] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 3, 4, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
        label = controls->vectors[2*i+1] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 4, 5, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
    }
    g_string_free(str, TRUE);

    table = gtk_table_new(1, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    return dialog;
}

static void
update_labels(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwySelection *selection;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gdouble lines[4*NLINES];
    gboolean is_visible;
    gint nselected, i;
    GString *str;


    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    selection = gwy_vector_layer_get_selection(state->layer);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    is_visible = state->is_visible;
    nselected = gwy_selection_get_data(selection, lines);
    if (!is_visible && !nselected)
        return;

    str = g_string_new("");

    g_string_printf(str, "<b>&#916;x</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->units[0]), str->str);

    g_string_printf(str, "<b>&#916;y</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->units[1]), str->str);

    g_string_printf(str, "<b>R</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->units[3]), str->str);

    for (i = 0; i < NLINES; i++) {
        if (i < nselected) {
            gdouble dx, dy, r, a;

            dx = lines[4*i+0] - lines[4*i+2];
            dy = lines[4*i+3] - lines[4*i+1];
            r = sqrt(dx*dx + dy*dy);
            a = atan2(dy, dx) * 180.0/G_PI;

            gwy_unitool_update_label_no_units(state->coord_hformat,
                                              controls->positions[2*i+ 0], dx);
            gwy_unitool_update_label_no_units(state->coord_hformat,
                                              controls->positions[2*i+ 1], dy);

            g_string_printf(str, "%#6.2f", a);
            gtk_label_set_markup(GTK_LABEL(controls->vectors[2*i+0]), str->str);
            gwy_unitool_update_label_no_units(state->coord_hformat,
                                              controls->vectors[2*i+ 1], r);

        }
        else {
            gtk_label_set_text(GTK_LABEL(controls->positions[2*i+0]), "");
            gtk_label_set_text(GTK_LABEL(controls->positions[2*i+1]), "");
            gtk_label_set_text(GTK_LABEL(controls->vectors[2*i+0]), "");
            gtk_label_set_text(GTK_LABEL(controls->vectors[2*i+1]), "");

        }
    }

    g_string_free(str, TRUE);

}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    ToolControls *controls;
    GwySelection *selection;
    gboolean is_visible;
    gint nselected;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    is_visible = state->is_visible;
    selection = gwy_vector_layer_get_selection(state->layer);
    nselected = gwy_selection_get_data(selection, NULL);
    if (!is_visible && !nselected)
        return;

    update_labels(state);
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    ToolControls *controls;

    controls = (ToolControls*)state->user_data;
    g_ptr_array_free(controls->str, TRUE);
    memset(state->user_data, 0, sizeof(ToolControls));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

