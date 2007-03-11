/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/datafield.h>
#include <libprocess/linestats.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwynullstore.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_PATH_LEVEL            (gwy_tool_path_level_get_type())
#define GWY_TOOL_PATH_LEVEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_PATH_LEVEL, GwyToolPathLevel))
#define GWY_IS_TOOL_PATH_LEVEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_PATH_LEVEL))
#define GWY_TOOL_PATH_LEVEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_PATH_LEVEL, GwyToolPathLevelClass))

enum {
    NLINES = 18
};

enum {
    COLUMN_I, COLUMN_DX, COLUMN_DY, COLUMN_R, COLUMN_DZ, NCOLUMNS
};

typedef struct _GwyToolPathLevel      GwyToolPathLevel;
typedef struct _GwyToolPathLevelClass GwyToolPathLevelClass;

struct _GwyToolPathLevel {
    GwyPlainTool parent_instance;

    GtkTreeView *treeview;
    GtkTreeModel *model;

    GtkWidget *apply;

    /* potential class data */
    GType layer_type_line;
};

struct _GwyToolPathLevelClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType    gwy_tool_path_level_get_type         (void) G_GNUC_CONST;
static void     gwy_tool_path_level_finalize         (GObject *object);
static void     gwy_tool_path_level_init_dialog      (GwyToolPathLevel *tool);
static void     gwy_tool_path_level_data_switched    (GwyTool *gwytool,
                                                      GwyDataView *data_view);
static void     gwy_tool_path_level_data_changed     (GwyPlainTool *plain_tool);
static void     gwy_tool_path_level_response         (GwyTool *gwytool,
                                                      gint response_id);
static void     gwy_tool_path_level_selection_changed(GwyPlainTool *plain_tool,
                                                      gint hint);
static void     gwy_tool_path_level_update_headers   (GwyToolPathLevel *tool);
static void     gwy_tool_path_level_render_cell      (GtkCellLayout *layout,
                                                      GtkCellRenderer *renderer,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *iter,
                                                      gpointer user_data);
static gboolean gwy_tool_path_level_list_key_press   (GwyToolPathLevel *tool,
                                                      GdkEventKey *event);
static void     gwy_tool_path_level_apply            (GwyToolPathLevel *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Path level tool, performs row leveling based on user-set paths."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2007",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolPathLevel, gwy_tool_path_level, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_PATH_LEVEL);

    return TRUE;
}

static void
gwy_tool_path_level_class_init(GwyToolPathLevelClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_path_level_finalize;

    tool_class->stock_id = GWY_STOCK_LINE_LEVEL;
    tool_class->title = _("Path Level");
    tool_class->tooltip = _("Level rows using intersections with given lines");
    tool_class->prefix = "/module/pathlevel";
    tool_class->default_height = 240;
    tool_class->data_switched = gwy_tool_path_level_data_switched;
    tool_class->response = gwy_tool_path_level_response;

    ptool_class->data_changed = gwy_tool_path_level_data_changed;
    ptool_class->selection_changed = gwy_tool_path_level_selection_changed;
}

static void
gwy_tool_path_level_finalize(GObject *object)
{
    GwyToolPathLevel *tool;

    tool = GWY_TOOL_PATH_LEVEL(object);

    if (tool->model) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        gwy_object_unref(tool->model);
    }

    G_OBJECT_CLASS(gwy_tool_path_level_parent_class)->finalize(object);
}

static void
gwy_tool_path_level_init(GwyToolPathLevel *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_line = gwy_plain_tool_check_layer_type(plain_tool,
                                                            "GwyLayerLine");
    if (!tool->layer_type_line)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_MARKUP;
    plain_tool->lazy_updates = TRUE;

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_line,
                                     "line");

    gwy_tool_path_level_init_dialog(tool);
}

static void
gwy_tool_path_level_init_dialog(GwyToolPathLevel *tool)
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkDialog *dialog;
    GtkWidget *scwin, *label;
    GwyNullStore *store;
    guint i;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    store = gwy_null_store_new(0);
    tool->model = GTK_TREE_MODEL(store);
    tool->treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(tool->model));
    g_signal_connect_swapped(tool->treeview, "key-press-event",
                             G_CALLBACK(gwy_tool_path_level_list_key_press),
                             tool);

    for (i = 0; i < NCOLUMNS; i++) {
        column = gtk_tree_view_column_new();
        gtk_tree_view_column_set_expand(column, TRUE);
        gtk_tree_view_column_set_alignment(column, 0.5);
        g_object_set_data(G_OBJECT(column), "id", GUINT_TO_POINTER(i));
        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "xalign", 1.0, NULL);
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
        gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(column), renderer,
                                           gwy_tool_path_level_render_cell, tool,
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

    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);

    gwy_tool_path_level_update_headers(tool);
    /* gtk_widget_set_sensitive(tool->apply, FALSE); */

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_path_level_data_switched(GwyTool *gwytool,
                                  GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolPathLevel *tool;

    GWY_TOOL_CLASS(gwy_tool_path_level_parent_class)->data_switched(gwytool,
                                                                  data_view);

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_PATH_LEVEL(gwytool);
    if (data_view) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_line,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, NLINES);
    }
    gwy_tool_path_level_update_headers(tool);
}

static void
gwy_tool_path_level_data_changed(GwyPlainTool *plain_tool)
{
    gwy_tool_path_level_update_headers(GWY_TOOL_PATH_LEVEL(plain_tool));
}

static void
gwy_tool_path_level_response(GwyTool *gwytool,
                             gint response_id)
{
    GwyToolPathLevel *tool;

    GWY_TOOL_CLASS(gwy_tool_path_level_parent_class)->response(gwytool,
                                                               response_id);

    tool = GWY_TOOL_PATH_LEVEL(gwytool);
    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_path_level_apply(tool);
}

static void
gwy_tool_path_level_selection_changed(GwyPlainTool *plain_tool,
                                      gint hint)
{
    GwyToolPathLevel *tool;
    GwyNullStore *store;
    gint n;

    tool = GWY_TOOL_PATH_LEVEL(plain_tool);
    store = GWY_NULL_STORE(tool->model);
    n = gwy_null_store_get_n_rows(store);
    g_return_if_fail(hint <= n);

    if (hint < 0) {
        gtk_tree_view_set_model(tool->treeview, NULL);
        if (plain_tool->selection)
            n = gwy_selection_get_data(plain_tool->selection, NULL);
        else
            n = 0;
        gwy_null_store_set_n_rows(store, n);
        gtk_tree_view_set_model(tool->treeview, tool->model);
    }
    else {
        if (hint < n)
            gwy_null_store_row_changed(store, hint);
        else
            gwy_null_store_set_n_rows(store, n+1);
    }
}

static void
gwy_tool_path_level_update_header(GwyToolPathLevel *tool,
                                  guint col,
                                  GString *str,
                                  const gchar *title,
                                  GwySIValueFormat *vf)
{
    GtkTreeViewColumn *column;
    GtkLabel *label;

    column = gtk_tree_view_get_column(tool->treeview, col);
    label = GTK_LABEL(gtk_tree_view_column_get_widget(column));

    g_string_assign(str, "<b>");
    g_string_append(str, title);
    g_string_append(str, "</b>");
    if (vf)
        g_string_append_printf(str, " [%s]", vf->units);
    gtk_label_set_markup(label, str->str);
}

static void
gwy_tool_path_level_update_headers(GwyToolPathLevel *tool)
{
    GwyPlainTool *plain_tool;
    GString *str;

    plain_tool = GWY_PLAIN_TOOL(tool);
    str = g_string_new("");

    gwy_tool_path_level_update_header(tool, COLUMN_I, str,
                                    "n", NULL);
    gwy_tool_path_level_update_header(tool, COLUMN_DX, str,
                                    "Δx", plain_tool->coord_format);
    gwy_tool_path_level_update_header(tool, COLUMN_DY, str,
                                    "Δy", plain_tool->coord_format);
    gwy_tool_path_level_update_header(tool, COLUMN_R, str,
                                    "R", plain_tool->coord_format);
    gwy_tool_path_level_update_header(tool, COLUMN_DZ, str,
                                    "Δz", plain_tool->value_format);

    g_string_free(str, TRUE);
}

static void
gwy_tool_path_level_render_cell(GtkCellLayout *layout,
                                GtkCellRenderer *renderer,
                                GtkTreeModel *model,
                                GtkTreeIter *iter,
                                gpointer user_data)
{
    GwyToolPathLevel *tool = (GwyToolPathLevel*)user_data;
    GwyPlainTool *plain_tool;
    const GwySIValueFormat *vf;
    gchar buf[32];
    gdouble line[4];
    gdouble val;
    guint idx, id;

    id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(layout), "id"));
    gtk_tree_model_get(model, iter, 0, &idx, -1);
    if (id == COLUMN_I) {
        g_snprintf(buf, sizeof(buf), "%d", idx + 1);
        g_object_set(renderer, "text", buf, NULL);
        return;
    }

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_selection_get_object(plain_tool->selection, idx, line);

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

        case COLUMN_DZ:
        {
            gint x, y;

            x = gwy_data_field_rtoj(plain_tool->data_field, line[2]);
            y = gwy_data_field_rtoi(plain_tool->data_field, line[3]);
            val = gwy_data_field_get_val(plain_tool->data_field, x, y);
            x = gwy_data_field_rtoj(plain_tool->data_field, line[0]);
            y = gwy_data_field_rtoi(plain_tool->data_field, line[1]);
            val -= gwy_data_field_get_val(plain_tool->data_field, x, y);
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

static gboolean
gwy_tool_path_level_list_key_press(GwyToolPathLevel *tool,
                                   GdkEventKey *event)
{
    GtkTreeSelection *selection;
    GtkTreePath *path;
    GtkTreeIter iter;
    const gint *indices;

    if (event->keyval == GDK_Delete) {
        selection = gtk_tree_view_get_selection(tool->treeview);
        if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
            path = gtk_tree_model_get_path(tool->model, &iter);
            indices = gtk_tree_path_get_indices(path);
            gwy_selection_delete_object(GWY_PLAIN_TOOL(tool)->selection,
                                        indices[0]);
            gtk_tree_path_free(path);
        }

        return TRUE;
    }

    return FALSE;
}

typedef struct {
    gint row;
    gint id;
    gboolean end;
} ChangePoint;

static gint
change_point_compare(gconstpointer a, gconstpointer b)
{
    const ChangePoint *pa = (const ChangePoint*)a;
    const ChangePoint *pb = (const ChangePoint*)b;

    if (pa->row < pb->row)
        return -1;
    if (pa->row > pb->row)
        return 1;

    if (pa->end < pb->end)
        return -1;
    if (pa->end > pb->end)
        return 1;

    if (pa->id < pb->id)
        return -1;
    if (pa->id > pb->id)
        return 1;

    g_return_val_if_reached(0);
}

static void
gwy_tool_path_level_apply(GwyToolPathLevel *tool)
{
    ChangePoint *cpts;
    GwyPlainTool *plain_tool;
    GwyDataLine *corr;
    gint n, xres, yres, row, i, j, nw;
    gboolean *wset;
    gdouble sel[4];
    gdouble *cd, *d;
    gdouble s;
    gint *isel;

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_app_undo_qcheckpoint(plain_tool->container,
                             gwy_app_get_data_key_for_id(plain_tool->id), 0);

    n = gwy_selection_get_data(plain_tool->selection, NULL);
    cpts = g_new(ChangePoint, 2*n);
    isel = g_new(gint, 4*n);
    xres = gwy_data_field_get_xres(plain_tool->data_field);
    yres = gwy_data_field_get_yres(plain_tool->data_field);
    for (i = 0; i < n; i++) {
        gwy_selection_get_object(plain_tool->selection, i, sel);
        sel[0] = gwy_data_field_rtoj(plain_tool->data_field, sel[0]);
        sel[1] = gwy_data_field_rtoi(plain_tool->data_field, sel[1]);
        sel[2] = gwy_data_field_rtoj(plain_tool->data_field, sel[2]);
        sel[3] = gwy_data_field_rtoi(plain_tool->data_field, sel[3]);
        if (sel[1] > sel[3]) {
            GWY_SWAP(gdouble, sel[0], sel[2]);
            GWY_SWAP(gdouble, sel[1], sel[3]);
        }
        isel[4*i + 0] = CLAMP((gint)sel[0], 0, xres-1);
        isel[4*i + 1] = CLAMP(floor(sel[1]), 0, yres-1);
        isel[4*i + 2] = CLAMP((gint)sel[2], 0, xres-1);
        isel[4*i + 3] = CLAMP(ceil(sel[3]), 0, yres-1);

        cpts[2*i].row = isel[4*i + 1];
        cpts[2*i].id = i;
        cpts[2*i].end = FALSE;
        cpts[2*i + 1].row = isel[4*i + 3];
        cpts[2*i + 1].id = i;
        cpts[2*i + 1].end = TRUE;
    }

    qsort(cpts, 2*n, sizeof(ChangePoint), change_point_compare);
    wset = g_new0(gboolean, n);
    corr = gwy_data_line_new(yres, 1.0, TRUE);
    cd = gwy_data_line_get_data(corr);
    d = gwy_data_field_get_data(plain_tool->data_field);
    i = 0;
    for (row = 1; row < yres; row++) {
        /* Lines participating on this row leveling are in wset now: they
         * intersect this and the previous row */
        if (row) {
            s = 0.0;
            nw = 0;
            for (j = 0; j < n; j++) {
                if (wset[j]) {
                    gint p = isel[4*j + 2] - isel[4*j + 0];
                    gint q = isel[4*j + 3] - isel[4*j + 1];
                    gint sg = q > 0 ? 1 : -1;
                    gint x = ((2*(row - isel[4*j + 1]) + 1)*p + sg*q)/(2*sg*q)
                             + isel[4*j + 0];

                    s += d[xres*row + x] - d[xres*(row - 1) + x];
                    /* d[xres*(row - 1) + x] = 0.0; */
                    nw++;
                }
            }
            if (nw) {
                s /= nw;
                cd[row] = s;
            }
        }

        /* Update working set.  Sort puts starts before ends, therefore
         * horizontal lines are effectively ignored -- which is the correct
         * behaviour */
        while (i < 2*n && row == cpts[i].row) {
            if (cpts[i].end) {
                g_assert(wset[cpts[i].id]);
                wset[cpts[i].id] = FALSE;
            }
            else {
                g_assert(!wset[cpts[i].id]);
                wset[cpts[i].id] = TRUE;
            }
            i++;
        }
    }
    g_free(wset);
    g_free(cpts);
    g_free(isel);

    gwy_data_line_cumulate(corr);
    for (row = 0; row < yres; row++) {
        s = cd[row];
        for (j = 0; j < xres; j++)
            d[row*xres + j] -= s;
    }
    g_object_unref(corr);

    gwy_data_field_data_changed(plain_tool->data_field);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
