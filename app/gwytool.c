/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/app.h>
#include <app/gwytool.h>

static void     gwy_tool_class_init    (GwyToolClass *klass);
static void     gwy_tool_init          (GwyTool *tool,
                                        gpointer g_class);
static void     gwy_tool_finalize      (GObject *object);
static void     gwy_tool_response      (GwyTool *tool,
                                        gint response);
static void     gwy_tool_unmap         (GwyTool *tool);
static void     gwy_tool_show_real     (GwyTool *tool);
static void     gwy_tool_hide_real     (GwyTool *tool);

static gpointer gwy_tool_parent_class = NULL;

/* Note: We cannot use the G_DEFINE_TYPE machinery, because we have a
 * two-argument instance init function which would cause declaration
 * conflict. */
GType
gwy_tool_get_type (void)
{
    static GType gwy_tool_type = 0;

    if (G_UNLIKELY(gwy_tool_type == 0)) {
        static const GTypeInfo gwy_tool_type_info = {
            sizeof(GwyToolClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_tool_class_init,
            NULL,
            NULL,
            sizeof(GwyTool),
            0,
            (GInstanceInitFunc)gwy_tool_init,
            NULL,
        };

        gwy_tool_type = g_type_register_static(G_TYPE_OBJECT, "GwyTool",
                                               &gwy_tool_type_info,
                                               G_TYPE_FLAG_ABSTRACT);
    }

    return gwy_tool_type;
}


static void
gwy_tool_class_init(GwyToolClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_tool_parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_tool_finalize;

    klass->hide = gwy_tool_hide_real;
    klass->show = gwy_tool_show_real;
}

static void
gwy_tool_finalize(GObject *object)
{
    GwyTool *tool;

    tool = GWY_TOOL(object);
    gtk_widget_destroy(tool->dialog);

    G_OBJECT_CLASS(gwy_tool_parent_class)->finalize(object);
}

static void
gwy_tool_init(GwyTool *tool,
              gpointer g_class)
{
    GwyToolClass *klass;
    GtkWindow *window;
    gint width, height;

    klass = GWY_TOOL_CLASS(g_class);
    gwy_debug("%s", klass->title);
    tool->dialog = gtk_dialog_new();
    gtk_dialog_set_has_separator(GTK_DIALOG(tool->dialog), FALSE);

    window = GTK_WINDOW(tool->dialog);
    gtk_window_set_title(window, gettext(klass->title));
    gwy_app_add_main_accel_group(window);
    /* Prevent too smart window managers from making big mistakes */
    gtk_window_set_position(window, GTK_WIN_POS_NONE);
    gtk_window_set_type_hint(window, GDK_WINDOW_TYPE_HINT_NORMAL);
    gtk_window_set_role(window, "tool");

    /* Set the default window size first from class,
     * then let settings override it */
    width = klass->default_width > 0 ? klass->default_width : -1;
    height = klass->default_height > 0 ? klass->default_height : -1;
    gtk_window_set_default_size(window, width, height);
    if (klass->prefix && g_str_has_prefix(klass->prefix, "/module")) {
        gchar *key;
        guint len;

        len = strlen(klass->prefix);
        key = g_newa(gchar, len + sizeof("/dialog"));
        strcpy(key, klass->prefix);
        strcpy(key + len, "/dialog");
        gwy_app_restore_window_position(GTK_WINDOW(tool->dialog), key, TRUE);
    }

    g_signal_connect_swapped(tool->dialog, "unmap",
                             G_CALLBACK(gwy_tool_unmap), tool);
    g_signal_connect(tool->dialog, "delete-event",
                     G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);
    g_signal_connect_swapped(tool->dialog, "response",
                             G_CALLBACK(gwy_tool_response), tool);
}

static void
gwy_tool_response(GwyTool *tool,
                  gint response)
{
    static guint response_id = 0;

    if (!response_id)
        response_id = g_signal_lookup("response", GTK_TYPE_DIALOG);

    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        g_signal_stop_emission(tool->dialog, response_id, 0);
        gwy_tool_hide(tool);
        break;

        case GTK_RESPONSE_NONE:
        g_warning("Tool dialog destroyed.");
        g_signal_stop_emission(tool->dialog, response_id, 0);
        g_object_unref(tool);
        break;

        default:
        {
            GwyToolClass *klass;

            klass = GWY_TOOL_GET_CLASS(tool);
            if (klass->response)
                klass->response(tool, response);
        }
        break;
    }
}

static void
gwy_tool_unmap(GwyTool *tool)
{
    GwyToolClass *klass;
    gchar *key;
    guint len;

    klass = GWY_TOOL_GET_CLASS(tool);
    if (!klass->prefix || !g_str_has_prefix(klass->prefix, "/module/"))
        return;

    gwy_debug("Saving %s dialog size", klass->title);
    len = strlen(klass->prefix);
    key = g_newa(gchar, len + sizeof("/dialog"));
    strcpy(key, klass->prefix);
    strcpy(key + len, "/dialog");
    gwy_app_save_window_position(GTK_WINDOW(tool->dialog), key, FALSE, TRUE);
}

static void
gwy_tool_show_real(GwyTool *tool)
{
    gwy_debug("");
    tool->is_visible = TRUE;
    gtk_window_present(GTK_WINDOW(tool->dialog));
}

static void
gwy_tool_hide_real(GwyTool *tool)
{
    tool->is_visible = FALSE;
    gtk_widget_hide(tool->dialog);
}

/**
 * gwy_tool_add_hide_button:
 * @tool: A tool.
 * @set_default: Whether hide should become the default tool dialog response.
 *
 * Adds a Hide button to a tool dialog.
 *
 * All tools should have a Hide button added by this method.  The reason why
 * it is not added automatically is because the usual placement of the Hide
 * button is next to the execution button (Apply), which is not present
 * for informational-only tools.  In that case Hide should become the default
 * dialog button.
 **/
void
gwy_tool_add_hide_button(GwyTool *tool,
                         gboolean set_default)
{
    GtkAccelGroup *accelgroup;
    GtkWidget *button;

    g_return_if_fail(GWY_IS_TOOL(tool));

    button = gtk_dialog_add_button(GTK_DIALOG(tool->dialog), _("Hide"),
                                   GTK_RESPONSE_DELETE_EVENT);
    if (set_default)
        gtk_dialog_set_default_response(GTK_DIALOG(tool->dialog),
                                        GTK_RESPONSE_DELETE_EVENT);

    accelgroup = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(tool->dialog), accelgroup);
    gtk_widget_add_accelerator(button, "activate", accelgroup,
                               GDK_Escape, 0, 0);
    g_object_unref(accelgroup);
}

/**
 * gwy_tool_show:
 * @tool: A tool.
 *
 * Shows a tool's dialog.
 **/
void
gwy_tool_show(GwyTool *tool)
{
    GwyToolClass *klass;

    g_return_if_fail(GWY_IS_TOOL(tool));
    klass = GWY_TOOL_GET_CLASS(tool);
    gwy_debug("%s", klass->title);
    if (klass->show)
        klass->show(tool);
}

/**
 * gwy_tool_hide:
 * @tool: A tool.
 *
 * Hides a tool's dialog.
 **/
void
gwy_tool_hide(GwyTool *tool)
{
    GwyToolClass *klass;

    g_return_if_fail(GWY_IS_TOOL(tool));
    klass = GWY_TOOL_GET_CLASS(tool);
    gwy_debug("%s", klass->title);
    if (klass->hide)
        klass->hide(tool);
}

/**
 * gwy_tool_is_visible:
 * @tool: A tool.
 *
 * Checks whether a tool dialog is visible.
 *
 * Returns: %TRUE if tool dialog is visible, %FALSE if it is hidden.
 **/
gboolean
gwy_tool_is_visible(GwyTool *tool)
{
    g_return_val_if_fail(GWY_IS_TOOL(tool), FALSE);
    return tool->is_visible;
}

/**
 * gwy_tool_data_switched:
 * @tool: A tool.
 * @data_view: A data view.  It can be %NULL, too.
 *
 * Instructs a tool to switch to another data view.
 *
 * This involves set up of the top layer of @data_view and/or its selection
 * to the mode appropriate for @tool.
 **/
void
gwy_tool_data_switched(GwyTool *tool,
                       GwyDataView *data_view)
{
    GwyToolClass *klass;

    g_return_if_fail(GWY_IS_TOOL(tool));
    klass = GWY_TOOL_GET_CLASS(tool);
    gwy_debug("%s", klass->title);
    if (klass->data_switched)
        klass->data_switched(tool, data_view);
}

/**
 * gwy_tool_spectra_switched:
 * @tool: A tool.
 * @data_view: A spectra object.  It can be %NULL, too.
 *
 * Instructs a tool to switch to another spectra object.
 *
 * Bad things may happen when the spectra does not belong to the same container
 * as the currently active channel.
 *
 * Since: 2.6
 **/
void
gwy_tool_spectra_switched(GwyTool *tool,
                          GwySpectra *spectra)
{
    GwyToolClass *klass;

    g_return_if_fail(GWY_IS_TOOL(tool));
    klass = GWY_TOOL_GET_CLASS(tool);
    gwy_debug("%s", klass->title);
    if (klass->spectra_switched)
        klass->spectra_switched(tool, spectra);
}

/**
 * gwy_tool_class_get_title:
 * @klass: A tool class.
 *
 * Gets the title of a tool class (this is a class method).
 *
 * The title is normally used as a tool dialog title.
 *
 * Returns: The title as a string owned by the tool class, untranslated.
 **/
const gchar*
gwy_tool_class_get_title(GwyToolClass *klass)
{
    g_return_val_if_fail(GWY_IS_TOOL_CLASS(klass), NULL);
    return klass->title;
}

/**
 * gwy_tool_class_get_stock_id:
 * @klass: A tool class.
 *
 * Gets the icon stock id of a tool class (this is a class method).
 *
 * Returns: The stock id as a string owned by the tool class.
 **/
const gchar*
gwy_tool_class_get_stock_id(GwyToolClass *klass)
{
    g_return_val_if_fail(GWY_IS_TOOL_CLASS(klass), NULL);
    return klass->stock_id;
}

/**
 * gwy_tool_class_get_tooltip:
 * @klass: A tool class.
 *
 * Gets the title of a tool class (this is a class method).
 *
 * Returns: The tooltip as a string owned by the tool class, untranslated.
 **/
const gchar*
gwy_tool_class_get_tooltip(GwyToolClass *klass)
{
    g_return_val_if_fail(GWY_IS_TOOL_CLASS(klass), NULL);
    return klass->tooltip;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwytool
 * @title: GwyTool
 * @short_description: Base class for tools
 **/

/**
 * GwyToolResponseType:
 * @GWY_TOOL_RESPONSE_CLEAR: Clear selection response.
 * @GWY_TOOL_RESPONSE_UPDATE: Update calculated values (if not instant)
 *                            response.
 *
 * Common tool dialog responses.
 *
 * They do not have any special meaning for #GwyTool (yet?), nonetheless you
 * are encouraged to use them for consistency.
 **/

/**
 * GwyToolClass:
 * @stock_id: Tool icon stock id.
 * @tooltip: Tooltip.
 * @title: Tool dialog title.
 * @prefix: Prefix in settings to store automatically remembered tool state
 *          under, should be of the form <literal>"/module/mytool"</literal>.
 * @default_width: Default initial window width, normally unset.
 * @default_height: Default initial window height, normally unset.
 * @show: Tool show virtual method.  Most tools do not need to override it,
 *        unless they wish to handle lazy updates themselves.
 * @hide: Tool hide virtual method.  Most tools do not need to override it.
 * @data_switched: Data switched virtual method.
 * @response: Dialog response virtual method.  Hiding an closing is normally
 *            handled in the base class, particular tools can handle only
 *            responses from their specific buttons.
 *
 * Tool class.
 *
 * The fields @default_width and @default_height should be set only if a tool
 * dialog requires a different initial size from it would request (due to
 * shrinkable widgets).  Since #GwyTool keeps dialog sizes stored in settings
 * and restores them automatically, these values essentially apply only to the
 * first use of a tool.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
