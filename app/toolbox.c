/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2016 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#include "gwyappinternal.h"
#include "gwyddion.h"
#include "toolbox.h"

#include "mac_integration.h"

enum {
    DND_TARGET_STRING = 1,
};

typedef struct {
    GtkTooltips *tips;
    GtkBox *box;
    GtkWidget *group;
    gint width;
    gint pos;
    GtkRadioButton *first_tool;
    const gchar *first_tool_func;
    GPtrArray *unseen_tools;
    GtkAccelGroup *accel_group;
    gboolean seen_unseen_tools;
} GwyAppToolboxBuilder;

typedef struct {
    GCallback callback;
    GQuark func;
    GQuark stock_id;
    GwyAppActionType type;
    GwyRunType mode;
    GwyMenuSensFlags sens;
    const gchar *tooltip;
} Action;

static GtkWidget* gwy_app_menu_create_info_menu    (GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_file_menu    (GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_edit_menu    (GtkAccelGroup *accel_group);
static gboolean   gwy_toolbox_fill_builtin_action  (Action *action);
static void       gwy_app_toolbox_showhide         (GtkWidget *expander);
static void       show_user_guide                  (void);
static void       show_message_log                 (void);
static GtkWindow* create_message_log_window        (void);
static void       toolbox_dnd_data_received        (GtkWidget *widget,
                                                    GdkDragContext *context,
                                                    gint x,
                                                    gint y,
                                                    GtkSelectionData *data,
                                                    guint info,
                                                    guint time_,
                                                    gpointer user_data);
static void       delete_app_window                (void);
static void       action_zoom_in                   (void);
static void       action_zoom_out                  (void);
static void       action_zoom_1_1                  (void);
static void       action_undo                      (void);
static void       action_redo                      (void);
static void       remove_all_logs                  (void);
static void       toggle_edit_accelerators         (gpointer callback_data,
                                                    gint callback_action,
                                                    GtkCheckMenuItem *item);
static void       toggle_logging_enabled           (gpointer callback_data,
                                                    gint callback_action,
                                                    GtkCheckMenuItem *item);
static void       enable_edit_accelerators         (gboolean enable);
static void       gwy_app_tool_use                 (const gchar *toolname,
                                                    GtkToggleButton *button);
static void       gwy_app_change_default_mask_color(void);
static void       action_display_3d                (void);

/* Translatability hack, intltool seems overkill at this point. */
#define GWY_TOOLBOX_IGNORE(x) /* */
GWY_TOOLBOX_IGNORE((_("View"),
                    _("Data Process"),
                    _("Graph"),
                    _("Tools"),
                    _("Volume")))

/* FIXME: A temporary hack. */
static void
set_sensitivity(GtkItemFactory *item_factory, ...)
{
    GwySensitivityGroup *sensgroup;
    GwyMenuSensFlags mask;
    const gchar *path;
    GtkWidget *widget;
    va_list ap;

    sensgroup = gwy_app_sensitivity_get_group();
    va_start(ap, item_factory);
    while ((path = va_arg(ap, const gchar*))) {
        mask = va_arg(ap, guint);
        widget = gtk_item_factory_get_widget(item_factory, path);
        if (!widget) {
            g_warning("Cannot find menu item %s", path);
            continue;
        }
        gwy_sensitivity_group_add_widget(sensgroup, widget, mask);
    }
    va_end(ap);
}

static void
toolbox_add_menubar(GtkWidget *container,
                    GtkWidget *menu,
                    const gchar *item_label)
{
    GtkWidget *item, *alignment, *menubar;
    GtkTextDirection direction;

    menubar = gtk_menu_bar_new();
    direction = gtk_widget_get_direction(menubar);
    alignment = gtk_alignment_new(direction == GTK_TEXT_DIR_RTL ? 1.0 : 0.0,
                                  0.0, 1.0, 0.0);
    gtk_container_add(GTK_CONTAINER(container), alignment);
    gtk_container_add(GTK_CONTAINER(alignment), menubar);
    gtk_widget_set_name(menubar, "toolboxmenubar");

    item = gtk_menu_item_new_with_mnemonic(item_label);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
}

static void
toolbox_start_group(GwyAppToolboxBuilder *builder,
                    const GwyToolboxGroupSpec *gspec)
{
    GwyContainer *settings;
    GtkWidget *expander;
    gboolean visible = TRUE;
    gchar *s, *key;
    GQuark quark;

    builder->group = gtk_table_new(1, builder->width, TRUE);

    settings = gwy_app_settings_get();
    key = g_strconcat("/app/toolbox/visible/", g_quark_to_string(gspec->id),
                      NULL);
    quark = g_quark_from_string(key);
    g_free(key);
    gwy_container_gis_boolean(settings, quark, &visible);

    s = g_strconcat("<small>", gettext(gspec->name), "</small>", NULL);
    expander = gtk_expander_new(s);
    gtk_expander_set_use_markup(GTK_EXPANDER(expander), TRUE);
    g_free(s);
    g_object_set_data(G_OBJECT(expander), "key", GUINT_TO_POINTER(quark));
    g_object_set_data(G_OBJECT(expander),
                      "gwy-toolbox-ui-constructed", GUINT_TO_POINTER(TRUE));
    gtk_container_add(GTK_CONTAINER(expander), builder->group);
    gtk_expander_set_expanded(GTK_EXPANDER(expander), visible);
    gtk_box_pack_start(builder->box, expander, FALSE, FALSE, 0);
    g_signal_connect_after(expander, "activate",
                           G_CALLBACK(gwy_app_toolbox_showhide), NULL);
    builder->pos = 0;
}

static GtkWidget*
toolbox_make_tool_button(GwyAppToolboxBuilder *builder,
                         GwyToolClass *tool_class,
                         Action *action)
{
    GtkWidget *button;
    const gchar *name, *stock_id;
    gchar *accel_path;

    stock_id = gwy_tool_class_get_stock_id(tool_class);
    action->stock_id = g_quark_from_static_string(stock_id);
    button = gtk_radio_button_new_from_widget(builder->first_tool);
    name = g_type_name(G_TYPE_FROM_CLASS(tool_class));
    action->func = g_quark_from_static_string(name);
    if (!builder->first_tool) {
        builder->first_tool = GTK_RADIO_BUTTON(button);
        builder->first_tool_func = name;
    }
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
    accel_path = g_strconcat("<tool>/", name, NULL);
    gtk_widget_set_accel_path(button, accel_path, builder->accel_group);
    g_free(accel_path);

    action->tooltip = gwy_tool_class_get_tooltip(tool_class);

    return button;
}

static void
toolbox_action_run(Action *action, GtkButton *button)
{
    const gchar *name = g_quark_to_string(action->func);

    if (action->type == GWY_APP_ACTION_TYPE_BUILTIN)
        action->callback();
    else if (action->type == GWY_APP_ACTION_TYPE_TOOL)
        gwy_app_tool_use(name, GTK_TOGGLE_BUTTON(button));
    else if (action->type == GWY_APP_ACTION_TYPE_PROC)
        gwy_app_run_process_func_in_mode(name, action->mode);
    else if (action->type == GWY_APP_ACTION_TYPE_GRAPH)
        gwy_app_run_graph_func(name);
    else if (action->type == GWY_APP_ACTION_TYPE_VOLUME)
        gwy_app_run_volume_func_in_mode(name, action->mode);
    else if (action->type == GWY_APP_ACTION_TYPE_XYZ)
        gwy_app_run_xyz_func_in_mode(name, action->mode);
    else {
        g_assert_not_reached();
    }
}

static void
toolbox_action_free(Action *action)
{
    g_slice_free(Action, action);
}

static const gchar*
action_type_name(GwyAppActionType type)
{
    return gwy_enum_to_string(type, gwy_toolbox_action_types, -1);
}

static void
check_run_mode(GwyAppActionType type, const gchar *name,
               GwyRunType available_modes, GwyRunType *mode)
{
    GwyRunType first_mode = GWY_RUN_INTERACTIVE;

    if (available_modes & GWY_RUN_INTERACTIVE)
        first_mode = GWY_RUN_INTERACTIVE;
    else if (available_modes & GWY_RUN_IMMEDIATE)
        first_mode = GWY_RUN_IMMEDIATE;
    else if (available_modes & GWY_RUN_NONINTERACTIVE)
        first_mode = GWY_RUN_NONINTERACTIVE;

    if (!*mode) {
        *mode = first_mode;
        return;
    }

    if (available_modes & *mode)
        return;

    g_warning("Function %s::%s cannot be run in mode %d",
              action_type_name(type), name, *mode);
    *mode = first_mode;
}

static gboolean
toolbox_start_item(GwyAppToolboxBuilder *builder,
                   const GwyToolboxItemSpec *ispec)
{
    const gchar *func = NULL, *stock_id = NULL;
    GtkWidget *button = NULL;
    GwyToolClass *tool_class;
    GType gtype;
    Action action, *a;
    guint i;

    g_return_val_if_fail(builder->group, FALSE);

    gwy_clear(&action, 1);
    action.type = ispec->type;
    action.func = ispec->function;
    action.mode = ispec->mode;
    action.stock_id = ispec->icon;
    action.sens = -1;

    func = action.func ? g_quark_to_string(action.func) : NULL;

    switch (action.type) {
        case GWY_APP_ACTION_TYPE_PLACEHOLDER:
        builder->pos++;
        return TRUE;
        break;

        case GWY_APP_ACTION_TYPE_BUILTIN:
        if (!gwy_toolbox_fill_builtin_action(&action)) {
            g_warning("Function builtin::%s does not exist", func);
            return FALSE;
        }
        if (action.mode)
            g_warning("Function builtin::%s does not have run modes", func);
        break;

        case GWY_APP_ACTION_TYPE_PROC:
        if (!gwy_process_func_exists(func)) {
            g_warning("Function proc::%s does not exist", func);
            return FALSE;
        }
        stock_id = gwy_process_func_get_stock_id(func);
        action.tooltip = gwy_process_func_get_tooltip(func);
        action.sens = gwy_process_func_get_sensitivity_mask(func);
        check_run_mode(action.type, func,
                       gwy_process_func_get_run_types(func), &action.mode);
        break;

        case GWY_APP_ACTION_TYPE_GRAPH:
        if (!gwy_graph_func_exists(func)) {
            g_warning("Function graph::%s does not exist", func);
            return FALSE;
        }
        stock_id = gwy_graph_func_get_stock_id(func);
        action.tooltip = gwy_graph_func_get_tooltip(func);
        action.sens = gwy_graph_func_get_sensitivity_mask(func);
        if (action.mode)
            g_warning("Function graph::%s does not have run modes", func);
        break;

        case GWY_APP_ACTION_TYPE_VOLUME:
        if (!gwy_volume_func_exists(func)) {
            g_warning("Function volume::%s does not exist", func);
            return FALSE;
        }
        stock_id = gwy_volume_func_get_stock_id(func);
        action.tooltip = gwy_volume_func_get_tooltip(func);
        action.sens = gwy_volume_func_get_sensitivity_mask(func);
        check_run_mode(action.type, func,
                       gwy_volume_func_get_run_types(func), &action.mode);
        break;

        case GWY_APP_ACTION_TYPE_XYZ:
        if (!gwy_xyz_func_exists(func)) {
            g_warning("Function xyz::%s does not exist", func);
            return FALSE;
        }
        stock_id = gwy_xyz_func_get_stock_id(func);
        action.tooltip = gwy_xyz_func_get_tooltip(func);
        action.sens = gwy_xyz_func_get_sensitivity_mask(func);
        check_run_mode(action.type, func,
                       gwy_xyz_func_get_run_types(func), &action.mode);
        break;

        case GWY_APP_ACTION_TYPE_TOOL:
        /* Handle unseen tools */
        if (!func) {
            if (builder->seen_unseen_tools) {
                g_warning("Unseen tools placeholder present multiple times.");
                return FALSE;
            }
            for (i = 0; i < builder->unseen_tools->len; i++) {
                const gchar *name = g_ptr_array_index(builder->unseen_tools, i);
                GwyToolboxItemSpec iispec;

                gwy_clear(&iispec, 1);
                iispec.type = GWY_APP_ACTION_TYPE_TOOL;
                iispec.function = g_quark_from_static_string(name);
                toolbox_start_item(builder, &iispec);
            }
            builder->seen_unseen_tools = TRUE;
            return TRUE;
        }
        if (!(gtype = g_type_from_name(func))) {
            g_warning("Function tool::%s does not exist", func);
            return FALSE;
        }
        tool_class = g_type_class_peek(gtype);
        if (!GWY_IS_TOOL_CLASS(tool_class)) {
            g_warning("Type %s is not a GwyTool", func);
            return FALSE;
        }
        button = toolbox_make_tool_button(builder, tool_class, &action);
        break;

        default:
        g_return_val_if_reached(FALSE);
        break;
    }

    if (!button)
        button = gtk_button_new();

    if (!action.stock_id && stock_id)
        action.stock_id = g_quark_from_string(stock_id);

    if (!action.stock_id) {
        g_warning("Function %s::%s has no icon set",
                  action_type_name(action.type), func);
        stock_id = GTK_STOCK_MISSING_IMAGE;
        action.stock_id = g_quark_from_static_string(stock_id);
    }
    else {
        stock_id = g_quark_to_string(action.stock_id);
        if (!gtk_icon_factory_lookup_default(stock_id)) {
            g_warning("Function %s::%s icon %s not found",
                      action_type_name(action.type), func,
                      g_quark_to_string(action.stock_id));
            stock_id = GTK_STOCK_MISSING_IMAGE;
            action.stock_id = g_quark_from_static_string(stock_id);
        }
    }

    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_table_attach_defaults(GTK_TABLE(builder->group), button,
                              builder->pos % builder->width,
                              builder->pos % builder->width + 1,
                              builder->pos/builder->width,
                              builder->pos/builder->width + 1);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(stock_id,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    if (action.tooltip)
        gtk_tooltips_set_tip(builder->tips, button, _(action.tooltip), NULL);

    a = g_slice_dup(Action, &action);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(toolbox_action_run), a);
    g_signal_connect_swapped(button, "destroy",
                             G_CALLBACK(toolbox_action_free), a);

    if (action.sens != -1)
        gwy_app_sensitivity_add_widget(button, action.sens);

    builder->pos++;

    return TRUE;
}

static void
gather_tools(const gchar *name,
             GPtrArray *tools)
{
    g_ptr_array_add(tools, (gpointer)name);
}

/* XXX: Move to toolbox-spec probably.  It can keep the file name for
 * itself... */
static void
remove_seen_unseen_tools(GwyAppToolboxBuilder *builder,
                         const GwyToolboxSpec *spec)
{
    GPtrArray *unseen_tools = builder->unseen_tools;
    const GwyToolboxGroupSpec *gspec;
    const GwyToolboxItemSpec *ispec;
    GArray *group, *item;
    const gchar *name;
    guint i, j, k;

    group = spec->group;
    for (i = 0; i < group->len; i++) {
        gspec = &g_array_index(group, GwyToolboxGroupSpec, i);
        item = gspec->item;
        for (j = 0; j < item->len; j++) {
            ispec = &g_array_index(item, GwyToolboxItemSpec, j);
            if (!ispec->function || ispec->type != GWY_APP_ACTION_TYPE_TOOL)
                continue;

            name = g_quark_to_string(ispec->function);
            for (k = 0; k < unseen_tools->len; k++) {
                if (gwy_strequal(name, g_ptr_array_index(unseen_tools, k))) {
                    g_ptr_array_remove_index(unseen_tools, k);
                    break;
                }
            }
        }
    }
}

static void
gwy_app_toolbox_build(GwyToolboxSpec *spec,
                      GtkBox *vbox,
                      GtkTooltips *tips,
                      GtkAccelGroup *accel_group)
{
    GwyAppToolboxBuilder builder;
    const GwyToolboxGroupSpec *gspec;
    const GwyToolboxItemSpec *ispec;
    GArray *group, *item;
    guint i, j;

    gwy_clear(&builder, 1);
    builder.width = spec->width ? spec->width : 4;
    builder.box = vbox;
    builder.tips = tips;
    builder.unseen_tools = g_ptr_array_new();
    builder.accel_group = accel_group;

    gwy_tool_func_foreach((GFunc)gather_tools, builder.unseen_tools);
    remove_seen_unseen_tools(&builder, spec);

    group = spec->group;
    for (i = 0; i < group->len; i++) {
        gspec = &g_array_index(group, GwyToolboxGroupSpec, i);
        toolbox_start_group(&builder, gspec);
        item = gspec->item;
        for (j = 0; j < item->len; j++) {
            ispec = &g_array_index(item, GwyToolboxItemSpec, j);
            /* When the construction fails remove the item also from the spec
             * so *if* we edit and save the spec it is corrected. */
            if (!toolbox_start_item(&builder, ispec))
                gwy_app_toolbox_spec_remove_item(spec, i, j);
        }
        builder.group = NULL;
    }

    if (builder.first_tool) {
        gwy_app_switch_tool(builder.first_tool_func);
        gtk_widget_grab_focus(GTK_WIDGET(builder.first_tool));
    }

    g_ptr_array_free(builder.unseen_tools, TRUE);
}

GtkWidget*
gwy_app_toolbox_window_create(void)
{
    static GtkTargetEntry dnd_target_table[] = {
        { "STRING",        0, DND_TARGET_STRING, },
        { "text/plain",    0, DND_TARGET_STRING, },
        { "text/uri-list", 0, DND_TARGET_STRING, },
    };

    GtkWidget *toolbox, *menu, *container;
    GtkBox *vbox;
    GtkAccelGroup *accel_group;
    GwyToolboxSpec *spec;

    toolbox = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(toolbox), g_get_application_name());
    gtk_window_set_role(GTK_WINDOW(toolbox), GWY_TOOLBOX_WM_ROLE);
    gtk_window_set_resizable(GTK_WINDOW(toolbox), FALSE);
    gwy_help_add_to_window(GTK_WINDOW(toolbox), "main-window", NULL,
                           GWY_HELP_DEFAULT);
    gwy_app_main_window_set(toolbox);

    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(toolbox), accel_group);
    g_object_set_data(G_OBJECT(toolbox), "accel_group", accel_group);

    vbox = GTK_BOX(gtk_vbox_new(FALSE, 0));
    container = GTK_WIDGET(vbox);
    gtk_container_add(GTK_CONTAINER(toolbox), container);

    toolbox_add_menubar(container,
                        gwy_app_menu_create_file_menu(accel_group), _("_File"));
    toolbox_add_menubar(container,
                        gwy_app_menu_create_edit_menu(accel_group), _("_Edit"));

    menu = gwy_app_build_process_menu(accel_group);
    gwy_app_process_menu_add_run_last(menu);
    toolbox_add_menubar(container, menu, _("_Data Process"));

    menu = gwy_app_build_graph_menu(accel_group);
    toolbox_add_menubar(container, menu, _("_Graph"));

    menu = gwy_app_build_volume_menu(accel_group);
    toolbox_add_menubar(container, menu, _("_Volume Data"));

    menu = gwy_app_build_xyz_menu(accel_group);
    toolbox_add_menubar(container, menu, _("_XYZ Data"));

    toolbox_add_menubar(container,
                        gwy_app_menu_create_info_menu(accel_group), _("_Info"));

    /***************************************************************/

    spec = gwy_parse_toolbox_ui();
    if (spec) {
        gwy_app_toolbox_build(spec, vbox, gwy_app_get_tooltips(), accel_group);
        g_object_set_data(G_OBJECT(toolbox), "gwy-app-toolbox-spec", spec);
    }

    /***************************************************************/
    gtk_drag_dest_set(toolbox, GTK_DEST_DEFAULT_ALL,
                      dnd_target_table, G_N_ELEMENTS(dnd_target_table),
                      GDK_ACTION_COPY);
    g_signal_connect(toolbox, "drag-data-received",
                     G_CALLBACK(toolbox_dnd_data_received), NULL);

    /***************************************************************/
    /* XXX */
    g_signal_connect(toolbox, "delete-event", G_CALLBACK(gwy_app_quit), NULL);

    gtk_widget_show_all(toolbox);

    gwy_osx_get_menu_from_widget(container);
    while (gtk_events_pending())
        gtk_main_iteration_do(FALSE);

    gwy_remote_setup(toolbox);
    g_signal_connect(toolbox, "destroy", G_CALLBACK(gwy_remote_finalize), NULL);

    return toolbox;
}

#if 0
static void
reconstruct_toolbox(void)
{
    GtkWidget* toolbox;
    GtkAccelGroup *accel_group;
    GtkBox *vbox;
    GList *children, *l;

    toolbox = gwy_app_main_window_get();
    vbox = GTK_BOX(gtk_bin_get_child(GTK_BIN(toolbox)));
    children = gtk_container_get_children(GTK_CONTAINER(vbox));

    for (l = children; l; l = g_list_next(l)) {
        if (g_object_get_data(G_OBJECT(l->data), "gwy-toolbox-ui-constructed"))
            gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(l);

    accel_group = g_object_get_data(G_OBJECT(toolbox), "accel_group");
    gwy_app_toolbox_build(vbox, gwy_app_get_tooltips(), accel_group);
    gtk_widget_show_all(GTK_WIDGET(vbox));
}
#endif

const GwyToolboxBuiltinSpec*
gwy_toolbox_get_builtins(guint *nspec)
{
    static const GwyToolboxBuiltinSpec spec[] = {
        {
            "display_3d", GWY_STOCK_3D_BASE, &action_display_3d,
            N_("Display a 3D view of data"), N_("Display a 3D view of data"),
        },
        {
            "zoom_in", GWY_STOCK_ZOOM_IN, &action_zoom_in,
            N_("Zoom in"), N_("Zoom in"),
        },
        {
            "zoom_out", GWY_STOCK_ZOOM_OUT, &action_zoom_out,
            N_("Zoom out"), N_("Zoom out"),
        },
        {
            "zoom_1_1", GWY_STOCK_ZOOM_1_1, &action_zoom_1_1,
            N_("Zoom 1:1"), N_("Zoom 1:1"),
        },
    };

    *nspec = G_N_ELEMENTS(spec);
    return spec;
}

const GwyToolboxBuiltinSpec*
gwy_toolbox_find_builtin_spec(const gchar *name)
{
    const GwyToolboxBuiltinSpec* spec;
    guint i, n;

    spec = gwy_toolbox_get_builtins(&n);
    for (i = 0; i < n; i++) {
        if (gwy_strequal(name, spec[i].name))
            return spec + i;
    }
    return NULL;
}

static gboolean
gwy_toolbox_fill_builtin_action(Action *action)
{
    const GwyToolboxBuiltinSpec *spec;
    const gchar *name;

    name = g_quark_to_string(action->func);
    if (!(spec = gwy_toolbox_find_builtin_spec(name)))
        return FALSE;

    action->type = GWY_APP_ACTION_TYPE_BUILTIN;
    action->sens = GWY_MENU_FLAG_DATA;
    action->callback = spec->callback;
    action->tooltip = spec->tooltip;
    action->stock_id = g_quark_from_static_string(spec->stock_id);

    return TRUE;
}

/*************************************************************************/
static GtkWidget*
gwy_app_menu_create_info_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        {
            N_("/Show _Data Browser"),
            NULL,
            gwy_app_data_browser_show,
            0,
            "<Item>",
            NULL
        },
        {
            N_("/Module _Browser"),
            NULL,
            gwy_module_browser,
            0,
            "<Item>",
            NULL
        },
        {
            N_("/Program _Messages"),
            NULL,
            show_message_log,
            0,
            "<Item>",
            NULL
        },
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Separator>",
            NULL },
        {
            N_("/_User Guide"),
            "",
            show_user_guide,
            0,
            "<StockItem>",
            GTK_STOCK_HELP
        },
        {
            N_("/_Tip of the Day"),
            NULL,
            gwy_app_tip_of_the_day,
            0,
            "<StockItem>",
            GTK_STOCK_DIALOG_INFO
        },
        {
            N_("/_About Gwyddion"),
            NULL,
            gwy_app_about,
            0,
            "<StockItem>",
            GTK_STOCK_ABOUT
        },
    };
    GtkItemFactory *item_factory;

    /* The menu used to be called Meta.  Keep acceleteror paths and do not
     * change the name here */
    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<meta>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items), menu_items, NULL);

    /* Don't do this.  We hide all other help access options if help does not
     * seem available.  But if it happens the user should have means to provoke
     * Gwyddion into telling him why it thinks help is not available. */
    /*
    if (!gwy_help_is_available())
        gtk_item_factory_delete_item(item_factory, "<meta>/User Guide");
        */

    return gtk_item_factory_get_widget(item_factory, "<meta>");
}

static GtkWidget*
gwy_app_menu_create_file_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        {
            N_("/_Open..."),
            "<control>O",
            gwy_app_file_open,
            0,
            "<StockItem>",
            GTK_STOCK_OPEN
        },
        {
            N_("/_Merge..."),
            "<control><shift>M",
            gwy_app_file_merge,
            0,
            NULL,
            NULL
        },
        {
            N_("/Open _Recent"),
            NULL,
            NULL,
            0,
            "<Branch>",
            NULL
        },
        {
            N_("/_Save"),
            "<control>S",
            gwy_app_file_save,
            0,
            "<StockItem>",
            GTK_STOCK_SAVE
        },
        {
            N_("/Save _As..."),
            "<control><shift>S",
            gwy_app_file_save_as,
            0,
            "<StockItem>",
            GTK_STOCK_SAVE_AS
        },
        {
            N_("/_Close"),
            "<control>W",
            gwy_app_file_close,
            0,
            "<StockItem>",
            GTK_STOCK_CLOSE
        },
        {
            N_("/Remo_ve All Logs"),
            NULL,
            remove_all_logs,
            0,
            NULL,
            NULL
        },
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Separator>",
            NULL
        },
        {
            N_("/_Quit"),
            "<control>Q",
            delete_app_window,
            0,
            "<StockItem>",
            GTK_STOCK_QUIT
        },
    };
    GtkItemFactory *item_factory;
    GtkWidget *item, *menu;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<file>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items), menu_items, NULL);

    set_sensitivity(item_factory,
                    "<file>/Save",            GWY_MENU_FLAG_FILE,
                    "<file>/Save As...",      GWY_MENU_FLAG_FILE,
                    "<file>/Merge...",        GWY_MENU_FLAG_FILE,
                    "<file>/Close",           GWY_MENU_FLAG_FILE,
                    "<file>/Remove All Logs", GWY_MENU_FLAG_FILE,
                    NULL);

    item = gtk_item_factory_get_item(item_factory, "<file>/Open Recent");
    menu = gwy_app_menu_recent_files_get();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);

    return gtk_item_factory_get_widget(item_factory, "<file>");
}

GtkWidget*
gwy_app_menu_create_edit_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        {
            N_("/_Undo"),
            "<control>Z",
            action_undo,
            0,
            "<StockItem>",
            GTK_STOCK_UNDO
        },
        {
            N_("/_Redo"),
            "<control>Y",
            action_redo,
            0,
            "<StockItem>",
            GTK_STOCK_REDO
        },
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Separator>",
            NULL
        },
        {
            N_("/Default Mask _Color..."),
            NULL,
            gwy_app_change_default_mask_color,
            0,
            "<StockItem>",
            GWY_STOCK_MASK
        },
        {
            N_("/Color _Gradients..."),
            NULL,
            gwy_app_gradient_editor,
            0,
            "<StockItem>",
            GWY_STOCK_PALETTES
        },
        {
            N_("/G_L Materials..."),
            NULL,
            gwy_app_gl_material_editor,
            0,
            "<StockItem>",
            GWY_STOCK_GL_MATERIAL
        },
        {
            N_("/_Toolbox.."),
            NULL,
            gwy_toolbox_editor,
            0,
            NULL,
            NULL
        },
        {
            N_("/_Keyboard Shortcuts"),
            NULL,
            toggle_edit_accelerators,
            0,
            "<CheckItem>",
            NULL
        },
        {
            N_("/_Logging Enabled"),
            NULL,
            toggle_logging_enabled,
            0,
            "<CheckItem>",
            NULL
        },
    };
    GtkItemFactory *item_factory;
    GtkWidget *item;
    GwyContainer *settings;
    gboolean enable_edit = FALSE, enable_logging;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<edit>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items), menu_items, NULL);

    set_sensitivity(item_factory,
                    "<edit>/Undo", GWY_MENU_FLAG_UNDO,
                    "<edit>/Redo", GWY_MENU_FLAG_REDO,
                    NULL);

    settings = gwy_app_settings_get();

    gwy_container_gis_boolean_by_name(settings,
                                      "/app/edit-accelerators", &enable_edit);
    item = gtk_item_factory_get_widget(item_factory,
                                       "<edit>/Keyboard Shortcuts");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), enable_edit);
    enable_edit_accelerators(enable_edit);

    enable_logging = gwy_log_get_enabled();
    item = gtk_item_factory_get_widget(item_factory,
                                       "<edit>/Logging Enabled");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), enable_logging);

    return gtk_item_factory_get_widget(item_factory, "<edit>");
}

static void
gwy_app_toolbox_showhide(GtkWidget *expander)
{
    GwyContainer *settings;
    gboolean visible;
    GQuark quark;

    settings = gwy_app_settings_get();
    quark = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(expander), "key"));
    visible = gtk_expander_get_expanded(GTK_EXPANDER(expander));
    gwy_container_set_boolean(settings, quark, visible);
}

static void
show_user_guide(void)
{
    gwy_help_show("index", NULL);
}

static void
show_message_log(void)
{
    static GtkWindow *window = NULL;

    if (!window)
        window = create_message_log_window();

    gtk_window_present(window);
}

static void
message_log_updated(GtkTextBuffer *textbuf, GtkTextView *textview)
{
    GtkTextIter iter;

    gtk_text_buffer_get_end_iter(textbuf, &iter);
    gtk_text_view_scroll_to_iter(textview, &iter, 0.0, FALSE, 0.0, 1.0);
}

static gboolean
message_log_deleted(GtkWidget *window)
{
    gtk_widget_hide(window);
    return TRUE;
}

static gboolean
message_log_key_pressed(GtkWidget *window, GdkEventKey *event)
{
    if (event->keyval != GDK_Escape
        || (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)))
        return FALSE;

    gtk_widget_hide(window);
    return TRUE;
}

static GtkWindow*
create_message_log_window(void)
{
    GtkWindow *window;
    GtkTextBuffer *textbuf;
    GtkWidget *logview, *scwin;

    window = (GtkWindow*)gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(window, _("Program Messages"));
    gtk_window_set_default_size(window, 480, 320);

    textbuf = gwy_app_get_log_text_buffer();
    logview = gtk_text_view_new_with_buffer(textbuf);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(logview), FALSE);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(scwin), logview);
    gtk_widget_show_all(scwin);

    gtk_container_add(GTK_CONTAINER(window), scwin);

    gwy_app_add_main_accel_group(window);
    g_signal_connect(textbuf, "changed",
                     G_CALLBACK(message_log_updated), logview);
    g_signal_connect(window, "delete-event",
                     G_CALLBACK(message_log_deleted), NULL);
    g_signal_connect(window, "key-press-event",
                     G_CALLBACK(message_log_key_pressed), NULL);

    return window;
}

static gboolean
toolbox_dnd_open_files(gpointer user_data)
{
    GPtrArray *files = (GPtrArray*)user_data;
    gchar *filename;
    guint i;

    for (i = 0; i < files->len; i++) {
        filename = (gchar*)g_ptr_array_index(files, i);
        gwy_app_file_load(NULL, filename, NULL);
        g_free(filename);
    }
    g_ptr_array_free(files, TRUE);

    return FALSE;
}

static void
toolbox_dnd_data_received(G_GNUC_UNUSED GtkWidget *widget,
                          GdkDragContext *context,
                          G_GNUC_UNUSED gint x,
                          G_GNUC_UNUSED gint y,
                          GtkSelectionData *data,
                          G_GNUC_UNUSED guint info,
                          guint time_,
                          G_GNUC_UNUSED gpointer user_data)
{
    gchar *uri, *filename, *text;
    gchar **file_list;
    gboolean ok = FALSE;
    GPtrArray *files;
    guint i;

    if (data->length <= 0 || data->format != 8) {
        gtk_drag_finish(context, FALSE, FALSE, time_);
        return;
    }

    text = g_strdelimit(g_strdup((gchar*)data->data), "\r\n", '\n');
    file_list = g_strsplit(text, "\n", 0);
    g_free(text);
    if (!file_list) {
        gtk_drag_finish(context, FALSE, FALSE, time_);
        return;
    }

    files = g_ptr_array_new();
    for (i = 0; file_list[i]; i++) {
        uri = g_strstrip(file_list[i]);
        if (!*uri)
            continue;
        filename = g_filename_from_uri(uri, NULL, NULL);
        if (!filename)
            continue;
        gwy_debug("filename = %s", filename);
        if (gwy_file_detect(filename, FALSE, GWY_FILE_OPERATION_LOAD)) {
            /* FIXME: what about charset conversion? */
            g_ptr_array_add(files, filename);
            ok = TRUE;    /* FIXME: what if we accept only some? */
        }
        else
            g_free(filename);
    }
    g_strfreev(file_list);
    gtk_drag_finish(context, ok, FALSE, time_);

    if (files->len)
        g_idle_add(toolbox_dnd_open_files, files);
    else
        g_ptr_array_free(files, TRUE);
}

static void
delete_app_window(void)
{
    gboolean boo;

    g_signal_emit_by_name(gwy_app_main_window_get(), "delete-event",
                          NULL, &boo);
}

/* FIXME: we should zoom whatever is currently active: datawindow, 3dwindow,
 * graph */
static void
gwy_app_zoom_set(gint izoom)
{
    GtkWidget *window, *view;

    gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &view, 0);
    if (!view)
        return;

    window = gtk_widget_get_ancestor(view, GWY_TYPE_DATA_WINDOW);
    g_return_if_fail(window);
    gwy_data_window_set_zoom(GWY_DATA_WINDOW(window), izoom);
}

static void
action_zoom_in(void)
{
    gwy_app_zoom_set(1);
}

static void
action_zoom_out(void)
{
    gwy_app_zoom_set(-1);
}

static void
action_zoom_1_1(void)
{
    gwy_app_zoom_set(10000);
}

static void
action_undo(void)
{
    GwyContainer *data;

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
    if (data)
        gwy_app_undo_undo_container(data);
}

static void
action_redo(void)
{
    GwyContainer *data;

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
    if (data)
        gwy_app_undo_redo_container(data);
}

static void
remove_all_logs(void)
{
    GwyContainer *data;
    gchar buf[32];
    gint *ids;
    gint i;

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
    g_return_if_fail(data);

    g_object_ref(data);

    ids = gwy_app_data_browser_get_data_ids(data);
    for (i = 0; ids[i] != -1; i++) {
        g_snprintf(buf, sizeof(buf), "/%d/data/log", ids[i]);
        gwy_container_remove_by_name(data, buf);
    }
    g_free(ids);

    ids = gwy_app_data_browser_get_volume_ids(data);
    for (i = 0; ids[i] != -1; i++) {
        g_snprintf(buf, sizeof(buf), "/brick/%d/log", ids[i]);
        gwy_container_remove_by_name(data, buf);
    }
    g_free(ids);

    g_object_unref(data);
}

static void
toggle_edit_accelerators(G_GNUC_UNUSED gpointer callback_data,
                         G_GNUC_UNUSED gint callback_action,
                         GtkCheckMenuItem *item)
{
    gboolean active = gtk_check_menu_item_get_active(item);

    gwy_container_set_boolean_by_name(gwy_app_settings_get(),
                                      "/app/edit-accelerators", active);
    enable_edit_accelerators(active);
}

static void
toggle_logging_enabled(G_GNUC_UNUSED gpointer callback_data,
                       G_GNUC_UNUSED gint callback_action,
                       GtkCheckMenuItem *item)
{
    gboolean active = gtk_check_menu_item_get_active(item);

    gwy_container_set_boolean_by_name(gwy_app_settings_get(),
                                      "/app/log/disable", !active);
    gwy_log_set_enabled(active);
}

static void
enable_edit_accelerators(gboolean enable)
{
    g_object_set(gtk_settings_get_default(),
                 "gtk-can-change-accels", enable,
                 NULL);
}

static void
gwy_app_tool_use(const gchar *toolname, GtkToggleButton *button)
{
    /* don't catch deactivations */
    if (button && !gtk_toggle_button_get_active(button)) {
        gwy_debug("deactivation");
    }
    else
        gwy_app_switch_tool(toolname);
}

static void
gwy_app_change_default_mask_color(void)
{
    gwy_color_selector_for_mask(_("Change Default Mask Color"),
                                NULL, gwy_app_settings_get(), "/mask");
}

static void
action_display_3d(void)
{
    static GtkWidget *dialog = NULL;

    if (gwy_app_gl_is_ok()) {
        GwyContainer *data;
        gint id;

        gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data,
                                         GWY_APP_DATA_FIELD_ID, &id,
                                         0);
        g_return_if_fail(data);
        gwy_app_data_browser_show_3d(data, id);
        return;
    }

    if (dialog) {
        gtk_window_present(GTK_WINDOW(dialog));
        return;
    }

    dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_CLOSE,
                                    _("OpenGL 3D graphics not available"));
#ifdef GWYDDION_HAS_OPENGL
    if (gwy_app_gl_disabled()) {
        gtk_message_dialog_format_secondary_markup
            (GTK_MESSAGE_DIALOG(dialog),
             _("OpenGL was disabled with a command-line option."));
    }
    else {
        gtk_message_dialog_format_secondary_markup
            (GTK_MESSAGE_DIALOG(dialog),
             /* FIXME: Makes sense only on Unix */
             /* FIXME: It would be nice to give a more helpful message, but the
              * trouble is we don't know why the silly thing failed either. */
             _("Initialization of OpenGL failed.  Check output of "
               "<tt>glxinfo</tt> and warning messages printed to console "
               "during Gwyddion startup."));
    }
#else
    gtk_message_dialog_format_secondary_markup
        (GTK_MESSAGE_DIALOG(dialog),
         _("This version of Gwyddion was built without OpenGL support."));
#endif
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    g_object_add_weak_pointer(G_OBJECT(dialog), (gpointer*)&dialog);
    gtk_widget_show(dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
