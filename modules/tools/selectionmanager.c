/*
 *  @(#) $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/grains.h>
#include <libprocess/fractals.h>
#include <libprocess/correct.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_SELECTION_MANAGER            (gwy_tool_selection_manager_get_type())
#define GWY_TOOL_SELECTION_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_SELECTION_MANAGER, GwyToolSelectionManager))
#define GWY_IS_TOOL_SELECTION_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_SELECTION_MANAGER))
#define GWY_TOOL_SELECTION_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_SELECTION_MANAGER, GwyToolSelectionManagerClass))

typedef struct _GwyToolSelectionManager      GwyToolSelectionManager;
typedef struct _GwyToolSelectionManagerClass GwyToolSelectionManagerClass;

typedef struct {
} ToolArgs;

struct _GwyToolSelectionManager {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkWidget *list;
    gint row;

    /* potential class data */
    GType layer_type_point;
};

struct _GwyToolSelectionManagerClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_selection_manager_get_type        (void) G_GNUC_CONST;
static void gwy_tool_selection_manager_finalize         (GObject *object);
static void gwy_tool_selection_manager_init_dialog      (GwyToolSelectionManager *tool);
static void gwy_tool_selection_manager_data_switched    (GwyTool *gwytool,
                                                     GwyDataView *data_view);

//static const gchar mode_key[]   = "/module/grainremover/mode";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Grain removal tool, removes continuous parts of mask and/or "
       "underlying data."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2009",
};

static const ToolArgs default_args = {
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
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_selection_manager_finalize;

    tool_class->stock_id = GWY_STOCK_ARITHMETIC;
    tool_class->title = _("Selection Manager");
    tool_class->tooltip = _("Display and copy selections");
    tool_class->prefix = "/module/selmanager";
    tool_class->data_switched = gwy_tool_selection_manager_data_switched;
}

static void
gwy_tool_selection_manager_finalize(GObject *object)
{
    GwyToolSelectionManager *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_SELECTION_MANAGER(object);

    settings = gwy_app_settings_get();
    /*
    gwy_container_set_int32_by_name(settings,
                                    mode_key, tool->args.mode);
                                    */

    G_OBJECT_CLASS(gwy_tool_selection_manager_parent_class)->finalize(object);
}

static void
gwy_tool_selection_manager_init(GwyToolSelectionManager *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_point = gwy_plain_tool_check_layer_type(plain_tool,
                                                             "GwyLayerPoint");
    if (!tool->layer_type_point)
        return;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    /*
    gwy_container_gis_enum_by_name(settings, mode_key, &tool->args.mode);
    */

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_point,
                                     "pointer");

    gwy_tool_selection_manager_init_dialog(tool);
}

static void
gwy_tool_selection_manager_init_dialog(GwyToolSelectionManager *tool)
{
    GtkWidget *label, *combo;
    GtkDialog *dialog;
    GtkTable *table;
    GSList *group;
    gboolean sensitive;
    gint row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    table = GTK_TABLE(gtk_table_new(10, 3, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table), TRUE, TRUE, 0);
    row = 0;

    tool->list = table;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Name</b>");
    gtk_table_attach(table, label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Objects</b>");
    gtk_table_attach(table, label,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Max. Objects</b>");
    gtk_table_attach(table, label,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);

    gtk_widget_show_all(dialog->vbox);
}

static void
add_selection(gpointer hkey, gpointer hvalue, gpointer data)
{
    GQuark quark = GPOINTER_TO_UINT(hkey);
    GValue *value = (GValue*)hvalue;
    GwyToolSelectionManager *tool = (GwyToolSelectionManager*)data;
    GtkTable *table = GTK_TABLE(tool->list);
    GwySelection *sel = g_value_get_object(value);
    GtkWidget *label;

    label = gtk_label_new(G_OBJECT_TYPE_NAME(sel));
    gtk_table_attach_defaults(table, label, 0, 1, tool->row, tool->row+1);
    label = gtk_label_new(g_strdup_printf("%u", gwy_selection_get_data(sel,
                                                                       NULL)));
    gtk_table_attach_defaults(table, label, 1, 2, tool->row, tool->row+1);
    tool->row++;
}

static void
gwy_tool_selection_manager_data_switched(GwyTool *gwytool,
                                     GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolSelectionManager *tool;
    GtkWidget *label;
    GtkTable *table;
    GtkDialog *dialog;
    gboolean ignore;
    gint row;

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    ignore = (data_view == plain_tool->data_view);

    GWY_TOOL_CLASS(gwy_tool_selection_manager_parent_class)->data_switched(gwytool,
                                                                     data_view);

    if (ignore || plain_tool->init_failed)
        return;

    tool = GWY_TOOL_SELECTION_MANAGER(gwytool);
    gtk_widget_destroy(tool->list);
    dialog = GTK_DIALOG(gwytool->dialog);

    table = GTK_TABLE(gtk_table_new(10, 3, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table), TRUE, TRUE, 0);
    row = 0;

    tool->list = table;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Name</b>");
    gtk_table_attach(table, label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Objects</b>");
    gtk_table_attach(table, label,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Max. Objects</b>");
    gtk_table_attach(table, label,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    if (data_view) {
        tool->row = row;
        gwy_container_foreach(plain_tool->container,
                              g_strdup_printf("/%d/select", plain_tool->id),
                              (GHFunc)&add_selection,
                              tool);
        /*
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_point,
                                "draw-marker", FALSE,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
        */
    }

    gtk_widget_show_all(GTK_WIDGET(table));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
