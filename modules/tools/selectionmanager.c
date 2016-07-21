/*
 *  @(#) $Id$
 *  Copyright (C) 2009,2012 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

/* The GtkTargetEntry for tree model drags.
 * FIXME: Is it Gtk+ private or what? */
#define GTK_TREE_MODEL_ROW \
    { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_APP, 0 }

/* XXX: Copied from data browser. */
#define page_id_key "gwy-app-data-browser-page-id"
#define container_key "gwy-app-data-browser-container"
enum {
    PAGENO_SHIFT = 16,
};

#define GWY_TYPE_TOOL_SELECTION_MANAGER            (gwy_tool_selection_manager_get_type())
#define GWY_TOOL_SELECTION_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_SELECTION_MANAGER, GwyToolSelectionManager))
#define GWY_IS_TOOL_SELECTION_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_SELECTION_MANAGER))
#define GWY_TOOL_SELECTION_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_SELECTION_MANAGER, GwyToolSelectionManagerClass))

enum {
    NLAYERTYPES = 7
};

enum {
    MODEL_ID,
    MODEL_OBJECT,
    MODEL_N_COLUMNS
};

typedef struct _GwyToolSelectionManager      GwyToolSelectionManager;
typedef struct _GwyToolSelectionManagerClass GwyToolSelectionManagerClass;

typedef struct {
    GwySelection *selection;
    const gchar *name;
    GwySIUnit *xyunit;
    gdouble origin[2];
} DistributeData;

typedef struct {
    gboolean allfiles;
} ToolArgs;

struct _GwyToolSelectionManager {
    GwyPlainTool parent_instance;

    ToolArgs args;
    gboolean in_setup;

    GtkListStore *model;
    GtkWidget *treeview;
    GtkWidget *allfiles;
    GtkWidget *distribute;
    GtkWidget *copy;
    GtkWidget *export;
    GtkWidget *delete;

    /* potential class data */
    GType layer_types[NLAYERTYPES];
    GType selection_types[NLAYERTYPES];
};

struct _GwyToolSelectionManagerClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType    gwy_tool_selection_manager_get_type         (void)                           G_GNUC_CONST;
static void     gwy_tool_selection_manager_finalize         (GObject *object);
static void     gwy_tool_selection_manager_init_dialog      (GwyToolSelectionManager *tool);
static void     gwy_tool_selection_manager_data_switched    (GwyTool *gwytool,
                                                             GwyDataView *data_view);
static void     gwy_tool_selection_manager_response         (GwyTool *gwytool,
                                                             gint response_id);
static void     gwy_tool_selection_manager_clear            (GwyToolSelectionManager *tool);
static void     gwy_tool_selection_manager_selection_changed(GwyToolSelectionManager *tool,
                                                             GtkTreeSelection *selection);
static void     gwy_tool_selection_manager_all_files_changed(GwyToolSelectionManager *tool,
                                                             GtkToggleButton *button);
static gboolean gwy_tool_selection_manager_key_press        (GwyToolSelectionManager *tool,
                                                             GdkEventKey *event,
                                                             GtkTreeView *treeview);
static void     gwy_tool_selection_manager_distribute       (GwyToolSelectionManager *tool);
static void     gwy_tool_selection_manager_distribute_one   (GwyContainer *container,
                                                             DistributeData *distdata);
static void     gwy_tool_selection_manager_copy             (GwyToolSelectionManager *tool);
static void     gwy_tool_selection_manager_export           (GwyToolSelectionManager *tool);
static void     gwy_tool_selection_manager_delete           (GwyToolSelectionManager *tool);
static gchar*   gwy_tool_selection_manager_create_report    (GwyToolSelectionManager *tool);
static void     gwy_tool_selection_manager_setup_layer      (GwyToolSelectionManager *tool,
                                                             guint current,
                                                             GQuark quark);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Grain removal tool, removes continuous parts of mask and/or "
       "underlying data."),
    "Yeti <yeti@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti)",
    "2009",
};

static const gchar *const layer_types[NLAYERTYPES] = {
    "GwyLayerAxis",
    "GwyLayerEllipse",
    "GwyLayerLattice",
    "GwyLayerLine",
    "GwyLayerPath",
    "GwyLayerPoint",
    "GwyLayerRectangle",
};

static const gchar mode_key[]   = "/module/selectionmanager/allfiles";

static const ToolArgs default_args = {
    FALSE,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolSelectionManager, gwy_tool_selection_manager, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_SELECTION_MANAGER);

    return TRUE;
}

static void
gwy_tool_selection_manager_class_init(GwyToolSelectionManagerClass *klass)
{
    /*GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);*/
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_selection_manager_finalize;

    tool_class->stock_id = GWY_STOCK_SELECTIONS;
    tool_class->title = _("Selection Manager");
    tool_class->tooltip = _("Display, copy and export selections");
    tool_class->prefix = "/module/selectionmanager";
    tool_class->default_height = 280;
    tool_class->data_switched = gwy_tool_selection_manager_data_switched;
    tool_class->response = gwy_tool_selection_manager_response;
}

static void
gwy_tool_selection_manager_finalize(GObject *object)
{
    GwyToolSelectionManager *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_SELECTION_MANAGER(object);

    settings = gwy_app_settings_get();
    gwy_container_set_boolean_by_name(settings,
                                      mode_key, tool->args.allfiles);
    g_object_unref(tool->model);

    G_OBJECT_CLASS(gwy_tool_selection_manager_parent_class)->finalize(object);
}

static void
gwy_tool_selection_manager_init(GwyToolSelectionManager *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;
    GtkTreeSelection *selection;
    guint i;

    plain_tool = GWY_PLAIN_TOOL(tool);

    for (i = 0; i < NLAYERTYPES; i++) {
        GwyVectorLayerClass *klass;
        GType type;

        type = gwy_plain_tool_check_layer_type(plain_tool, layer_types[i]);
        if (!type)
            return;

        tool->layer_types[i] = type;
        klass = g_type_class_ref(type);
        tool->selection_types[i]
            = gwy_vector_layer_class_get_selection_type(klass);
        g_type_class_unref(klass);
    }
    settings = gwy_app_settings_get();
    tool->in_setup = TRUE;
    tool->args = default_args;
    gwy_container_gis_boolean_by_name(settings, mode_key, &tool->args.allfiles);

    tool->model = gtk_list_store_new(MODEL_N_COLUMNS,
                                     G_TYPE_INT, G_TYPE_OBJECT);
    g_object_set_data(G_OBJECT(tool->model), page_id_key,
                      GUINT_TO_POINTER(GWY_PAGE_NOPAGE + PAGENO_SHIFT));

    gwy_tool_selection_manager_init_dialog(tool);
    tool->in_setup = FALSE;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tool->treeview));
    gwy_tool_selection_manager_selection_changed(tool, selection);
}

static void
render_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED gpointer user_data)
{
    GQuark quark;
    const gchar *s;

    gtk_tree_model_get(model, iter, MODEL_ID, &quark, -1);
    s = g_quark_to_string(quark);
    g_return_if_fail(s);
    s = strrchr(s, GWY_CONTAINER_PATHSEP);
    g_return_if_fail(s);
    g_object_set(renderer, "text", s+1, NULL);
}

static void
render_type(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            G_GNUC_UNUSED gpointer user_data)
{
    static const struct {
        const gchar *typename;
        const gchar *humanname;
    }
    type_names[] = {
        { "GwySelectionAxis",      N_("Horiz./vert. lines"), },
        { "GwySelectionEllipse",   N_("Ellipses"),           },
        { "GwySelectionLattice",   N_("Lattice"),            },
        { "GwySelectionLine",      N_("Lines"),              },
        { "GwySelectionPath",      N_("Spline path"),        },
        { "GwySelectionPoint",     N_("Points"),             },
        { "GwySelectionRectangle", N_("Rectangles"),         },
    };

    GwySelection *sel;
    const gchar *name;
    guint i;

    gtk_tree_model_get(model, iter, MODEL_OBJECT, &sel, -1);
    g_return_if_fail(GWY_IS_SELECTION(sel));
    name = G_OBJECT_TYPE_NAME(sel);
    for (i = 0; i < G_N_ELEMENTS(type_names); i++) {
        if (gwy_strequal(name, type_names[i].typename)) {
            name = _(type_names[i].humanname);
            break;
        }
    }
    g_object_set(renderer, "text", name, NULL);
    g_object_unref(sel);
}

static void
render_objects(G_GNUC_UNUSED GtkTreeViewColumn *column,
               GtkCellRenderer *renderer,
               GtkTreeModel *model,
               GtkTreeIter *iter,
               G_GNUC_UNUSED gpointer user_data)
{
    gchar buffer[16];
    GwySelection *sel;

    gtk_tree_model_get(model, iter, MODEL_OBJECT, &sel, -1);
    g_return_if_fail(GWY_IS_SELECTION(sel));
    g_snprintf(buffer, sizeof(buffer), "%d", gwy_selection_get_data(sel, NULL));
    g_object_set(renderer, "text", buffer, NULL);
    g_object_unref(sel);
}

static void
gwy_tool_selection_manager_init_dialog(GwyToolSelectionManager *tool)
{
    static const GtkTargetEntry dnd_target_table[] = { GTK_TREE_MODEL_ROW };

    GtkDialog *dialog;
    GtkWidget *scwin, *hbox, *label;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), scwin, TRUE, TRUE, 0);

    tool->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tool->model));
    gtk_container_add(GTK_CONTAINER(scwin), tool->treeview);
    gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(tool->treeview),
                                           GDK_BUTTON1_MASK,
                                           dnd_target_table,
                                           G_N_ELEMENTS(dnd_target_table),
                                           GDK_ACTION_COPY);
    g_signal_connect_swapped(tool->treeview, "key-press-event",
                             G_CALLBACK(gwy_tool_selection_manager_key_press),
                             tool);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tool->treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Name"));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tool->treeview), column);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_name, tool, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "Type");
    gtk_tree_view_append_column(GTK_TREE_VIEW(tool->treeview), column);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_type, tool, NULL);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Objects"));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tool->treeview), column);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_objects, tool, NULL);

    label = gtk_label_new(_("Manage chosen selection"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(label), 0, 2);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), label, FALSE, FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), hbox, FALSE, FALSE, 2);

    tool->distribute = gtk_button_new_with_mnemonic(_("_Distribute"));
    gtk_box_pack_start(GTK_BOX(hbox), tool->distribute, FALSE, FALSE, 0);
    g_signal_connect_swapped(tool->distribute, "clicked",
                             G_CALLBACK(gwy_tool_selection_manager_distribute),
                             tool);

    tool->allfiles = gtk_check_button_new_with_mnemonic(_("to _all files"));
    gtk_box_pack_start(GTK_BOX(hbox), tool->allfiles, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->allfiles),
                                 tool->args.allfiles);
    g_signal_connect_swapped(tool->allfiles, "toggled",
                             G_CALLBACK(gwy_tool_selection_manager_all_files_changed),
                             tool);

    hbox = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), hbox, FALSE, FALSE, 2);

    tool->copy = gwy_stock_like_button_new(_("_Copy"), GTK_STOCK_COPY);
    gtk_box_pack_start(GTK_BOX(hbox), tool->copy, FALSE, FALSE, 0);
    g_signal_connect_swapped(tool->copy, "clicked",
                             G_CALLBACK(gwy_tool_selection_manager_copy),
                             tool);

    tool->export = gwy_stock_like_button_new(_("_Export"), GTK_STOCK_SAVE);
    gtk_box_pack_start(GTK_BOX(hbox), tool->export, FALSE, FALSE, 0);
    g_signal_connect_swapped(tool->export, "clicked",
                             G_CALLBACK(gwy_tool_selection_manager_export),
                             tool);

    tool->delete = gwy_stock_like_button_new(_("_Delete"), GTK_STOCK_DELETE);
    gtk_box_pack_start(GTK_BOX(hbox), tool->delete, FALSE, FALSE, 0);
    g_signal_connect_swapped(tool->delete, "clicked",
                             G_CALLBACK(gwy_tool_selection_manager_delete),
                             tool);

    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(gwy_tool_selection_manager_selection_changed),
                             tool);

    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);
    gtk_dialog_add_button(GTK_DIALOG(GWY_TOOL(tool)->dialog),
                          GTK_STOCK_CLEAR, GWY_TOOL_RESPONSE_CLEAR);
    gwy_help_add_to_tool_dialog(dialog, GWY_TOOL(tool), GWY_HELP_NO_BUTTON);

    gtk_widget_show_all(dialog->vbox);
}

static void
add_selection(gpointer hkey, gpointer hvalue, gpointer data)
{
    GQuark quark = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    GwyToolSelectionManager *tool = (GwyToolSelectionManager*)data;
    GwyPlainTool *plain_tool = GWY_PLAIN_TOOL(tool);
    GwySelection *sel = g_value_get_object(value);
    GtkTreeIter iter;

    g_object_set_data(G_OBJECT(sel), container_key, plain_tool->container);
    gtk_list_store_insert_with_values(tool->model, &iter, G_MAXINT,
                                      MODEL_ID, quark,
                                      MODEL_OBJECT, sel,
                                      -1);
}

static void
gwy_tool_selection_manager_data_switched(GwyTool *gwytool,
                                         GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolSelectionManager *tool;
    GtkTreeSelection *selection;
    gboolean ignore;

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    ignore = (data_view == plain_tool->data_view);

    GWY_TOOL_CLASS(gwy_tool_selection_manager_parent_class)->data_switched(gwytool,
                                                                           data_view);

    if (ignore || plain_tool->init_failed)
        return;

    tool = GWY_TOOL_SELECTION_MANAGER(gwytool);

    /* FIXME: This is very naive because the tool cannot react to selections
     * changed by something else.  Hopefully only other tools do such things
     * -- and then we get a chance to re-read the selection list.  */
    gtk_list_store_clear(tool->model);
    if (data_view) {
        gchar *basekey = g_strdup_printf("/%d/select", plain_tool->id);
        gwy_container_foreach(plain_tool->container, basekey,
                              (GHFunc)&add_selection,
                              tool);
        g_free(basekey);
    }

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tool->treeview));
    gwy_tool_selection_manager_selection_changed(tool, selection);
}

static void
gwy_tool_selection_manager_response(GwyTool *gwytool,
                                    gint response_id)
{
    GwyToolSelectionManager *tool;

    GWY_TOOL_CLASS(gwy_tool_selection_manager_parent_class)->response(gwytool,
                                                                      response_id);

    tool = GWY_TOOL_SELECTION_MANAGER(gwytool);
    if (response_id == GWY_TOOL_RESPONSE_CLEAR)
        gwy_tool_selection_manager_clear(tool);
}

static void
gwy_tool_selection_manager_clear(GwyToolSelectionManager *tool)
{
    GtkTreeModel *model = GTK_TREE_MODEL(tool->model);
    GwyPlainTool *plain_tool;
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    plain_tool = GWY_PLAIN_TOOL(tool);
    do {
        GQuark quark;
        gtk_tree_model_get(model, &iter, MODEL_ID, &quark, -1);
        gwy_container_remove(plain_tool->container, quark);
    } while (gtk_tree_model_iter_next(model, &iter));

    /* XXX: Since the model is not auto-updated, clear it manually. */
    gtk_list_store_clear(tool->model);
}

static void
gwy_tool_selection_manager_selection_changed(GwyToolSelectionManager *tool,
                                             GtkTreeSelection *selection)
{
    GtkTreeIter iter;
    gboolean is_selected = gtk_tree_selection_get_selected(selection,
                                                           NULL, &iter);
    guint current = G_MAXUINT;
    GQuark quark = 0;

    gtk_widget_set_sensitive(tool->distribute, is_selected);
    gtk_widget_set_sensitive(tool->copy, is_selected);
    gtk_widget_set_sensitive(tool->export, is_selected);
    gtk_widget_set_sensitive(tool->delete, is_selected);

    if (tool->in_setup)
        return;

    if (is_selected) {
        GwySelection *sel;
        GType type;
        guint i, n;

        gtk_tree_model_get(GTK_TREE_MODEL(tool->model), &iter,
                           MODEL_OBJECT, &sel,
                           MODEL_ID, &quark,
                           -1);
        n = gwy_selection_get_data(sel, NULL);
        if (!n) {
            gtk_widget_set_sensitive(tool->distribute, FALSE);
            gtk_widget_set_sensitive(tool->copy, FALSE);
            gtk_widget_set_sensitive(tool->export, FALSE);
        }

        type = G_OBJECT_TYPE(sel);
        for (i = 0; i < NLAYERTYPES; i++) {
            if (type == tool->selection_types[i]) {
                current = i;
                break;
            }
        }
        g_object_unref(sel);
    }

    gwy_tool_selection_manager_setup_layer(tool, current, quark);
}

static void
gwy_tool_selection_manager_all_files_changed(GwyToolSelectionManager *tool,
                                             GtkToggleButton *button)
{
    tool->args.allfiles = gtk_toggle_button_get_active(button);
}

static gboolean
gwy_tool_selection_manager_key_press(GwyToolSelectionManager *tool,
                                     GdkEventKey *event,
                                     G_GNUC_UNUSED GtkTreeView *treeview)
{
    if (event->keyval != GDK_Delete)
        return FALSE;

    gwy_tool_selection_manager_delete(tool);
    return TRUE;
}

static void
gwy_tool_selection_manager_distribute(GwyToolSelectionManager *tool)
{
    GwyPlainTool *plain_tool;
    GtkTreeSelection *treesel;
    GwyDataField *dfield;
    DistributeData distdata;
    GtkTreeIter iter;
    GQuark quark;
    const gchar *s;

    treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tool->treeview));
    if (!gtk_tree_selection_get_selected(treesel, NULL, &iter))
        return;

    gtk_tree_model_get(GTK_TREE_MODEL(tool->model), &iter,
                       MODEL_ID, &quark,
                       MODEL_OBJECT, &distdata.selection,
                       -1);
    s = g_quark_to_string(quark);
    g_return_if_fail(s);
    distdata.name = strrchr(s, GWY_CONTAINER_PATHSEP);
    g_return_if_fail(distdata.name);

    plain_tool = GWY_PLAIN_TOOL(tool);
    dfield = plain_tool->data_field;
    distdata.xyunit = gwy_data_field_get_si_unit_xy(dfield);
    distdata.origin[0] = gwy_data_field_get_xoffset(dfield);
    distdata.origin[1] = gwy_data_field_get_yoffset(dfield);
    gwy_debug("source: %p %s", dfield, s);

    if (tool->args.allfiles)
        gwy_app_data_browser_foreach((GwyAppDataForeachFunc)gwy_tool_selection_manager_distribute_one,
                                     &distdata);
    else
        gwy_tool_selection_manager_distribute_one(plain_tool->container,
                                                  &distdata);
}

static void
gwy_tool_selection_manager_distribute_one(GwyContainer *container,
                                          DistributeData *distdata)
{
    GObject *object, *selobject;
    GwySelection *selection;
    GwyDataField *dfield;
    GString *str;
    GQuark quark;
    gint *ids;
    gint i;

    gwy_debug("dest: %p", container);
    ids = gwy_app_data_browser_get_data_ids(container);
    str = g_string_new(NULL);
    selobject = G_OBJECT(distdata->selection);
    for (i = 0; ids[i] >= 0; i++) {
        gdouble xoff, yoff, xreal, yreal;

        g_string_printf(str, "/%d/select%s", ids[i], distdata->name);
        gwy_debug("%p %s", container, str->str);
        quark = g_quark_from_string(str->str);

        /* Avoid copying to self */
        if (gwy_container_gis_object(container, quark, &object)
            && object == selobject) {
            gwy_debug("avoiding copy-to-self");
            continue;
        }

        /* Check units */
        g_string_printf(str, "/%d/data", ids[i]);
        if (!gwy_container_gis_object_by_name(container, str->str, &object)
            || !GWY_IS_DATA_FIELD(object)) {
            gwy_debug("data field not found?!");
            continue;
        }
        dfield = GWY_DATA_FIELD(object);
        if (!gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(dfield),
                               distdata->xyunit)) {
            gwy_debug("units differ");
            continue;
        }

        xoff = gwy_data_field_get_xoffset(dfield);
        yoff = gwy_data_field_get_yoffset(dfield);
        xreal = gwy_data_field_get_xreal(dfield);
        yreal = gwy_data_field_get_yreal(dfield);
        object = gwy_serializable_duplicate(selobject);
        selection = GWY_SELECTION(object);

        /* Crop the selection, taking into account that the coordinates do not
         * include field offset, and move it relative to the new origin.
         * But for Lattice, which is origin-free, just limit it so that it
         * fits inside. */
        if (gwy_strequal(G_OBJECT_TYPE_NAME(object), "GwySelectionLattice")) {
            gwy_selection_crop(selection,
                               -0.5*xreal, -0.5*yreal, 0.5*xreal, 0.5*yreal);
        }
        else {
            gwy_selection_move(selection,
                               distdata->origin[0], distdata->origin[1]);
            gwy_selection_crop(selection,
                               xoff, yoff, xoff + xreal, yoff + yreal);
            gwy_selection_move(selection, -xoff, -yoff);
        }

        if (gwy_selection_get_data(selection, NULL))
            gwy_container_set_object(container, quark, object);
        else {
            gwy_debug("selection empty after cropping");
        }
        g_object_unref(object);
    }
    g_string_free(str, TRUE);
    g_free(ids);
}

static void
gwy_tool_selection_manager_copy(GwyToolSelectionManager *tool)
{
    GtkClipboard *clipboard;
    GdkDisplay *display;
    gchar *text;

    text = gwy_tool_selection_manager_create_report(tool);
    if (!text)
        return;

    display = gtk_widget_get_display(GTK_WIDGET(GWY_TOOL(tool)->dialog));
    clipboard = gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, text, -1);
    g_free(text);
}

static void
gwy_tool_selection_manager_export(GwyToolSelectionManager *tool)
{
    gchar *text;

    text = gwy_tool_selection_manager_create_report(tool);
    if (!text)
        return;

    gwy_save_auxiliary_data(_("Save Table"),
                            GTK_WINDOW(GWY_TOOL(tool)->dialog), -1, text);
    g_free(text);
}

static gchar*
gwy_tool_selection_manager_create_report(GwyToolSelectionManager *tool)
{
    GwyPlainTool *plain_tool;
    GtkTreeSelection *treesel;
    GwySelection *selection;
    GwyDataField *dfield;
    GwySIUnit *xyunit;
    gdouble xoff, yoff;
    GtkTreeIter iter;
    GString *text;
    guint n;

    treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tool->treeview));
    if (!gtk_tree_selection_get_selected(treesel, NULL, &iter))
        return NULL;

    gtk_tree_model_get(GTK_TREE_MODEL(tool->model), &iter,
                       MODEL_OBJECT, &selection,
                       -1);
    n = gwy_selection_get_data(selection, NULL);
    if (!n)
        return NULL;

    plain_tool = GWY_PLAIN_TOOL(tool);
    dfield = plain_tool->data_field;
    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    xoff = gwy_data_field_get_xoffset(dfield);
    yoff = gwy_data_field_get_yoffset(dfield);
    text = g_string_new("FIXME");

    return g_string_free(text, FALSE);
}

static void
gwy_tool_selection_manager_delete(GwyToolSelectionManager *tool)
{
    GtkTreeSelection *treesel;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GQuark quark;

    treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tool->treeview));
    if (gtk_tree_selection_get_selected(treesel, &model, &iter)) {
        gtk_tree_model_get(model, &iter, MODEL_ID, &quark, -1);
        gwy_container_remove(GWY_PLAIN_TOOL(tool)->container, quark);
        /* XXX: Since the model is not auto-updated, clear it manually. */
        gtk_list_store_remove(tool->model, &iter);
    }
}

static void
gwy_tool_selection_manager_setup_layer(GwyToolSelectionManager *tool,
                                       guint current,
                                       GQuark quark)
{
    GwyPlainTool *plain_tool;
    const gchar *s;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (!plain_tool->data_view || current >= NLAYERTYPES || !quark)
        return;

    s = g_quark_to_string(quark);
    g_return_if_fail(s);
    s = strrchr(s, GWY_CONTAINER_PATHSEP);
    g_return_if_fail(s);

    gwy_plain_tool_connect_selection(plain_tool,
                                     tool->layer_types[current], s+1);
    gwy_object_set_or_reset(plain_tool->layer,
                            tool->layer_types[current],
                            "editable", TRUE,
                            "focus", -1,
                            NULL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
