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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_DISTANCE            (gwy_tool_distance_get_type())
#define GWY_TOOL_DISTANCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_DISTANCE, GwyToolDistance))
#define GWY_IS_TOOL_DISTANCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_DISTANCE))
#define GWY_TOOL_DISTANCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_DISTANCE, GwyToolDistanceClass))

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

enum { NLINES = 12 };

enum {
    COLUMN_I,
    COLUMN_DX,
    COLUMN_DY,
    COLUMN_PHI,
    COLUMN_R,
    COLUMN_DZ,
    NCOLUMNS
};

typedef struct _GwyToolDistance          GwyToolDistance;
typedef struct _GwyToolDistanceClass     GwyToolDistanceClass;

struct _GwyToolDistance {
    GwyPlainTool parent_instance;

    GtkTreeView *treeview;
    GtkListStore *store;
} ToolControls;

struct _GwyToolDistanceClass {
    GwyPlainToolClass parent_class;

};

static gboolean module_register                  (void);
static GType    gwy_tool_distance_get_type       (void) G_GNUC_CONST;

static void gwy_tool_distance_update_headers(GwyToolDistance *tool);
static void gwy_tool_distance_render_cell(GtkCellLayout *layout,
                                          GtkCellRenderer *renderer,
                                          GtkTreeModel *model,
                                          GtkTreeIter *iter,
                                          gpointer user_data);

static gboolean   use                 (GwyDataWindow *data_window,
                                       GwyToolSwitchEvent reason);
static void       layer_setup         (GwyUnitoolState *state);
static GtkWidget* dialog_create       (GwyUnitoolState *state);
static void       dialog_update       (GwyUnitoolState *state,
                                       GwyUnitoolUpdateType reason);
static void       dialog_abandon      (GwyUnitoolState *state);



static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Distance measurement tool, measures distances and angles."),
    "Nenad Ocelic <ocelic@biochem.mpg.de>",
    "2.0",
    "Nenad Ocelic & David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolDistance, gwy_tool_distance, GWY_TYPE_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_DISTANCE);

    return TRUE;
}

static void
gwy_tool_distance_class_init(GwyToolDistanceClass *klass)
{
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);

    tool_class->stock_id = GWY_STOCK_DISTANCE;
    tool_class->title = _("Distance");
    tool_class->tooltip = _("Measure distances and directions between points");
}

static void
gwy_tool_distance_init(GwyToolDistance *tool)
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkDialog *dialog;
    GtkWidget *scwin, *label;
    guint i;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);
    /* XXX */
    gtk_window_set_default_size(GTK_WINDOW(dialog), -1, 200);

    tool->store = gtk_list_store_new(1, G_TYPE_INT);
    tool->treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model
                                                (GTK_TREE_MODEL(tool->store)));
    g_object_unref(tool->store);

    for (i = 0; i < NCOLUMNS; i++) {
        column = gtk_tree_view_column_new();
        g_object_set_data(G_OBJECT(column), "id", GUINT_TO_POINTER(i));
        renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, FALSE);
        gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                           gwy_tool_distance_render_cell, tool,
                                           NULL);
        label = gtk_label_new(NULL);
        gtk_tree_view_column_set_widget(column, label);
        gtk_widget_show(label);
        gtk_tree_view_append_column(tool->treeview, column);
    }

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scwin), GTK_WIDGET(tool->treeview));

    gtk_box_pack_start(GTK_BOX(dialog->vbox), scwin, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog->vbox);

    gtk_dialog_add_button(dialog, _("Hide"), 100);

    gwy_tool_distance_update_headers(tool);
}

static void
gwy_tool_distance_update_headers(GwyToolDistance *tool)
{
    GtkTreeViewColumn *column;
    GwyPlainTool *plain_tool;
    GtkLabel *label;
    GString *str;

    plain_tool = GWY_PLAIN_TOOL(tool);
    str = g_string_new("");

    column = gtk_tree_view_get_column(tool->treeview, COLUMN_I);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
    g_string_assign(str, "n");
    gtk_label_set_markup(label, str->str);

    column = gtk_tree_view_get_column(tool->treeview, COLUMN_DX);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
    g_string_assign(str, "Δx");
    if (plain_tool->coord_format)
        g_string_append_printf(str, " [%s]", plain_tool->coord_format->units);
    gtk_label_set_markup(label, str->str);

    column = gtk_tree_view_get_column(tool->treeview, COLUMN_DY);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
    g_string_assign(str, "Δy");
    if (plain_tool->coord_format)
        g_string_append_printf(str, " [%s]", plain_tool->coord_format->units);
    gtk_label_set_markup(label, str->str);

    column = gtk_tree_view_get_column(tool->treeview, COLUMN_PHI);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
    gtk_label_set_markup(label, _("Angle [deg]"));

    column = gtk_tree_view_get_column(tool->treeview, COLUMN_R);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
    g_string_assign(str, "R");
    if (plain_tool->coord_format)
        g_string_append_printf(str, " [%s]", plain_tool->coord_format->units);
    gtk_label_set_markup(label, str->str);

    column = gtk_tree_view_get_column(tool->treeview, COLUMN_DZ);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));
    g_string_assign(str, "Δz");
    if (plain_tool->value_format)
        g_string_append_printf(str, " [%s]", plain_tool->value_format->units);
    gtk_label_set_markup(label, str->str);

    g_string_free(str, TRUE);
}

static void
gwy_tool_distance_render_cell(GtkCellLayout *layout,
                              GtkCellRenderer *renderer,
                              GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gpointer user_data)
{
    GwyToolDistance *tool = (GwyToolDistance*)user_data;
    gchar buf[32];
    gint idx, id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(layout), "id"));
    gtk_tree_model_get(model, iter, 0, &idx, -1);
    switch (id) {
        case COLUMN_I:
        g_snprintf(buf, sizeof(buf), "<b>%d</b>", id);
        g_object_set(renderer, "markup", buf, NULL);
        return;
        break;

        default:
        strcpy(buf, "FIXME");
        break;
    }
    g_object_set(renderer, "text", buf, NULL);
}

#if 0
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
    gwy_selection_set_max_objects(selection, NLINES);
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

    table = gtk_table_new(NLINES+1, 6, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);
    str = g_string_new("");

    controls->units[0] = label= gtk_label_new(NULL);
    g_string_printf(str, "<b>Δx</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_FILL, 0, 2, 2);

    controls->units[1] = label = gtk_label_new(NULL);
    g_string_printf(str, "<b>Δy</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 2, 2);

    controls->units[2] = label = gtk_label_new(NULL);
    g_string_printf(str, _("<b>Angle</b> [deg]"));
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1, GTK_FILL, 0, 2, 2);

    controls->units[3] = label = gtk_label_new(NULL);
    g_string_printf(str, "<b>R</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 0, 1, GTK_FILL, 0, 2, 2);

    controls->units[4] = label = gtk_label_new(NULL);
    g_string_printf(str, "<b>Δz</b> [%s]", state->value_hformat->units);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 5, 6, 0, 1, GTK_FILL, 0, 2, 2);


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
        label = controls->diffs[i] = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 5, 6, i+1, i+2,
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
    gdouble line[4];
    gboolean is_visible;
    gint nselected, i;
    GString *str;

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    selection = gwy_vector_layer_get_selection(state->layer);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    is_visible = state->is_visible;
    nselected = gwy_selection_get_data(selection, NULL);
    if (!is_visible && !nselected)
        return;

    str = g_string_new("");

    g_string_printf(str, "<b>Δx</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->units[0]), str->str);

    g_string_printf(str, "<b>Δy</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->units[1]), str->str);

    g_string_printf(str, "<b>R</b> [%s]", state->coord_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->units[3]), str->str);

    g_string_printf(str, "<b>Δz</b> [%s]", state->value_hformat->units);
    gtk_label_set_markup(GTK_LABEL(controls->units[4]), str->str);

    for (i = 0; i < NLINES; i++) {
        if (i < nselected) {
            gint x, y;
            gdouble dx, dy, z1, z2, r, a;

            gwy_selection_get_object(selection, i, line);
            dx = line[2] - line[0];
            dy = line[3] - line[1];
            r = sqrt(dx*dx + dy*dy);
            a = atan2(dy, dx) * 180.0/G_PI;

            x = gwy_data_field_rtoj(dfield, line[0]);
            y = gwy_data_field_rtoi(dfield, line[1]);
            z1 = gwy_data_field_get_val(dfield, x, y);
            x = gwy_data_field_rtoj(dfield, line[2]);
            y = gwy_data_field_rtoi(dfield, line[3]);
            z2 = gwy_data_field_get_val(dfield, x, y);

            gwy_unitool_update_label_no_units(state->coord_hformat,
                                              controls->positions[2*i + 0], dx);
            gwy_unitool_update_label_no_units(state->coord_hformat,
                                              controls->positions[2*i + 1], dy);

            g_string_printf(str, "%#6.2f", a);
            gtk_label_set_markup(GTK_LABEL(controls->vectors[2*i + 0]),
                                 str->str);
            gwy_unitool_update_label_no_units(state->coord_hformat,
                                              controls->vectors[2*i + 1], r);

            gwy_unitool_update_label_no_units(state->value_hformat,
                                              controls->diffs[i], z2 - z1);
        }
        else {
            gtk_label_set_text(GTK_LABEL(controls->positions[2*i + 0]), "");
            gtk_label_set_text(GTK_LABEL(controls->positions[2*i + 1]), "");
            gtk_label_set_text(GTK_LABEL(controls->vectors[2*i + 0]), "");
            gtk_label_set_text(GTK_LABEL(controls->vectors[2*i + 1]), "");
            gtk_label_set_text(GTK_LABEL(controls->diffs[i]), "");
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
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

