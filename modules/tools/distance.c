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
    GtkTreeModel *model;
    GwySIValueFormat *angle_format;
    GType layer_type_line;
};

struct _GwyToolDistanceClass {
    GwyPlainToolClass parent_class;

};

static gboolean module_register                  (void);
static GType    gwy_tool_distance_get_type       (void) G_GNUC_CONST;

static void gwy_tool_finalize(GObject *object);
static void gwy_tool_distance_data_switched(GwyTool *tool,
                                            GwyDataView *data_view);
static void gwy_tool_distance_selection_changed(GwySelection *selection,
                                                gint hint,
                                                GwyToolDistance *tool);
static void gwy_tool_distance_update_headers(GwyToolDistance *tool);
static void gwy_tool_distance_render_cell(GtkCellLayout *layout,
                                          GtkCellRenderer *renderer,
                                          GtkTreeModel *model,
                                          GtkTreeIter *iter,
                                          gpointer user_data);

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

G_DEFINE_TYPE(GwyToolDistance, gwy_tool_distance, GWY_TYPE_PLAIN_TOOL)

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
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_finalize;

    tool_class->stock_id = GWY_STOCK_DISTANCE;
    tool_class->title = _("Distance");
    tool_class->tooltip = _("Measure distances and directions between points");
    tool_class->data_switched = gwy_tool_distance_data_switched;
}

static void
gwy_tool_finalize(GObject *object)
{
    GwyToolDistance *tool;

    tool = GWY_TOOL_DISTANCE(object);
    gwy_object_unref(tool->model);
    if (tool->angle_format)
        gwy_si_unit_value_format_free(tool->angle_format);
}

static void
gwy_tool_distance_init(GwyToolDistance *tool)
{
    GwyPlainTool *plain_tool;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkDialog *dialog;
    GtkWidget *scwin, *label;
    GtkListStore *store;
    guint i;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_line = gwy_plain_tool_check_layer_type(plain_tool,
                                                            "GwyLayerLine");
    if (!tool->layer_type_line)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_MARKUP;

    tool->angle_format = g_new(GwySIValueFormat, 1);
    tool->angle_format->magnitude = 1.0;
    tool->angle_format->precision = 0.1;
    gwy_si_unit_value_format_set_units(tool->angle_format, "deg");

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);
    /* XXX */
    gtk_window_set_default_size(GTK_WINDOW(dialog), -1, 200);

    store = gtk_list_store_new(1, G_TYPE_INT);
    tool->model = GTK_TREE_MODEL(store);
    tool->treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(tool->model));

    for (i = 0; i < NCOLUMNS; i++) {
        column = gtk_tree_view_column_new();
        g_object_set_data(G_OBJECT(column), "id", GUINT_TO_POINTER(i));
        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "xalign", 1.0, NULL);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
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

    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);
    gwy_tool_distance_update_headers(tool);
}

static void
gwy_tool_distance_data_switched(GwyTool *tool,
                                GwyDataView *data_view)
{
    GwySelection *selection;
    GwyPlainTool *plain_tool;
    GType type;

    GWY_TOOL_CLASS(gwy_tool_distance_parent_class)->data_switched(tool,
                                                                  data_view);
    plain_tool = GWY_PLAIN_TOOL(tool);
    if (plain_tool->init_failed)
        return;

    if (plain_tool->layer) {
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        g_signal_handlers_disconnect_by_func
                                         (selection,
                                          gwy_tool_distance_selection_changed,
                                          tool);
        gwy_object_unref(plain_tool->layer);
    }

    plain_tool->layer = gwy_data_view_get_top_layer(data_view);
    type = GWY_TOOL_DISTANCE(tool)->layer_type_line;
    if (!plain_tool->layer
        || G_TYPE_FROM_INSTANCE(plain_tool->layer) != type) {
        plain_tool->layer = g_object_new(type, NULL);
        gwy_data_view_set_top_layer(data_view, plain_tool->layer);
    }
    g_object_ref(plain_tool->layer);
    gwy_plain_tool_set_selection_key(plain_tool, "line");
    g_object_set(plain_tool->layer,
                 "line-numbers", TRUE,
                 NULL);
    selection = gwy_vector_layer_get_selection(plain_tool->layer);
    gwy_selection_set_max_objects(selection, NLINES);
    g_signal_connect(selection, "changed",
                     G_CALLBACK(gwy_tool_distance_selection_changed), tool);

    gwy_tool_distance_update_headers(GWY_TOOL_DISTANCE(tool));
    gwy_tool_distance_selection_changed(selection, -1, GWY_TOOL_DISTANCE(tool));
}

static void
gwy_tool_distance_selection_changed(GwySelection *selection,
                                    gint hint,
                                    GwyToolDistance *tool)
{
    GtkTreeIter iter;
    GtkListStore *store;
    gint n, nsel;

    n = gtk_tree_model_iter_n_children(tool->model, NULL);
    store = GTK_LIST_STORE(tool->model);

    /* One row has changed, emit signal */
    if (hint > 0) {
        g_return_if_fail(hint <= n);
        if (hint < n) {
            gwy_list_store_row_changed(store, NULL, NULL, hint);
            return;
        }
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, n, -1);
        return;
    }

    /* No specific hint, disconnect model, possibly updated the number of rows,
     * rebuilt it and reconnect.  This causes full redraw in any case. */
    gtk_tree_view_set_model(tool->treeview, NULL);
    nsel = gwy_selection_get_data(selection, NULL);
    while (nsel > n) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, n, -1);
        n++;
    }
    if (nsel < n) {
        gtk_tree_model_iter_nth_child(tool->model, &iter, NULL, nsel);
        while (nsel < n) {
            gtk_list_store_remove(store, &iter);
            n--;
        }
    }
    gtk_tree_view_set_model(tool->treeview, tool->model);
}

static void
gwy_tool_distance_update_header(GwyToolDistance *tool,
                                guint col,
                                GString *str,
                                const gchar *title,
                                GwySIValueFormat *vf)
{
    GtkTreeViewColumn *column;
    GtkLabel *label;

    column = gtk_tree_view_get_column(tool->treeview, col);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));

    g_string_assign(str, title);
    if (vf)
        g_string_append_printf(str, " [%s]", vf->units);
    gtk_label_set_markup(label, str->str);
}

static void
gwy_tool_distance_update_headers(GwyToolDistance *tool)
{
    GwyPlainTool *plain_tool;
    GString *str;

    plain_tool = GWY_PLAIN_TOOL(tool);
    str = g_string_new("");

    gwy_tool_distance_update_header(tool, COLUMN_I, str,
                                    "n", NULL);
    gwy_tool_distance_update_header(tool, COLUMN_DX, str,
                                    "Δx", plain_tool->coord_format);
    gwy_tool_distance_update_header(tool, COLUMN_DY, str,
                                    "Δy", plain_tool->coord_format);
    gwy_tool_distance_update_header(tool, COLUMN_PHI, str,
                                    "φ", tool->angle_format);
    gwy_tool_distance_update_header(tool, COLUMN_R, str,
                                    "R", plain_tool->coord_format);
    gwy_tool_distance_update_header(tool, COLUMN_DZ, str,
                                    "Δz", plain_tool->value_format);

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
    GwyPlainTool *plain_tool;
    GwySelection *selection;
    const GwySIValueFormat *vf;
    gchar buf[32];
    gdouble line[4];
    gdouble val;
    gint idx, id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(layout), "id"));
    gtk_tree_model_get(model, iter, 0, &idx, -1);
    if (id == COLUMN_I) {
        g_snprintf(buf, sizeof(buf), "%d", idx + 1);
        g_object_set(renderer, "text", buf, NULL);
        return;
    }

    plain_tool = GWY_PLAIN_TOOL(tool);
    selection = gwy_vector_layer_get_selection(plain_tool->layer);
    gwy_selection_get_object(selection, idx, line);

    switch (id) {
        case COLUMN_DX:
        vf = plain_tool->coord_format;
        val = line[2] - line[0];
        break;

        case COLUMN_DY:
        vf = plain_tool->coord_format;
        val = line[3] - line[1];
        break;

        case COLUMN_R:
        vf = plain_tool->coord_format;
        val = hypot(line[2] - line[0], line[3] - line[1]);
        break;

        case COLUMN_PHI:
        vf = tool->angle_format;
        val = atan2(line[3] - line[1], line[2] - line[0]) * 180.0/G_PI;
        break;

        case COLUMN_DZ:
        {
            GwyDataField *dfield;
            gint x, y;

            dfield = gwy_plain_tool_get_data_field(plain_tool);
            x = gwy_data_field_rtoj(dfield, line[2]);
            y = gwy_data_field_rtoi(dfield, line[3]);
            val = gwy_data_field_get_val(dfield, x, y);
            x = gwy_data_field_rtoj(dfield, line[0]);
            y = gwy_data_field_rtoi(dfield, line[1]);
            val -= gwy_data_field_get_val(dfield, x, y);
            vf = plain_tool->value_format;
        }
        break;

        default:
        g_return_if_reached();
        break;
    }

    if (vf)
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, val/vf->magnitude);
    else
        g_snprintf(buf, sizeof(buf), "%.3g", val);

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

