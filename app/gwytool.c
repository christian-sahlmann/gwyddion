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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/app.h>
#include <app/gwytool.h>

static void     gwy_tool_finalize      (GObject *object);
static void     gwy_tool_response      (GwyTool *tool,
                                        gint response);
static void     gwy_tool_show_real     (GwyTool *tool);
static void     gwy_tool_hide_real     (GwyTool *tool);

G_DEFINE_ABSTRACT_TYPE(GwyTool, gwy_tool, G_TYPE_OBJECT)

static void
gwy_tool_class_init(GwyToolClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_finalize;

    klass->hide = gwy_tool_hide_real;
    klass->show = gwy_tool_show_real;
}

static void
gwy_tool_finalize(G_GNUC_UNUSED GObject *object)
{
    G_OBJECT_CLASS(gwy_tool_parent_class)->finalize(object);
}

static void
gwy_tool_init(GwyTool *tool)
{
    GwyToolClass *klass;

    klass = GWY_TOOL_GET_CLASS(tool);
    tool->dialog = gtk_dialog_new();
    gtk_dialog_set_has_separator(GTK_DIALOG(tool->dialog), FALSE);
    gtk_window_set_title(GTK_WINDOW(tool->dialog), _(klass->title));
    gwy_app_add_main_accel_group(GTK_WINDOW(tool->dialog));
    /* Prevent too smart window managers from making big mistakes */
    gtk_window_set_position(GTK_WINDOW(tool->dialog), GTK_WIN_POS_NONE);
    gtk_window_set_type_hint(GTK_WINDOW(tool->dialog),
                             GDK_WINDOW_TYPE_HINT_NORMAL);

    g_signal_connect(tool->dialog, "delete-event",
                     G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);
    g_signal_connect_swapped(tool->dialog, "response",
                             G_CALLBACK(gwy_tool_response), tool);

    tool->update_on_show = TRUE;
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
    }
}

static void
gwy_tool_show_real(GwyTool *tool)
{
    tool->is_visible = TRUE;
    gtk_window_present(GTK_WINDOW(tool->dialog));
}

static void
gwy_tool_hide_real(GwyTool *tool)
{
    tool->is_visible = FALSE;
    gtk_widget_hide(tool->dialog);
}

void
gwy_tool_add_hide_button(GwyTool *tool,
                         gboolean set_default)
{
    GtkWidget *button;

    g_return_if_fail(GWY_IS_TOOL(tool));

    button = gtk_dialog_add_button(GTK_DIALOG(tool->dialog), _("_Hide"),
                                   GTK_RESPONSE_DELETE_EVENT);
    if (set_default)
        gtk_dialog_set_default_response(GTK_DIALOG(tool->dialog),
                                        GTK_RESPONSE_DELETE_EVENT);
}

void
gwy_tool_show(GwyTool *tool)
{
    void (*method)(GwyTool*);

    g_return_if_fail(GWY_IS_TOOL(tool));
    method = GWY_TOOL_GET_CLASS(tool)->show;
    if (method)
        method(tool);
}

void
gwy_tool_hide(GwyTool *tool)
{
    void (*method)(GwyTool*);

    g_return_if_fail(GWY_IS_TOOL(tool));
    method = GWY_TOOL_GET_CLASS(tool)->hide;
    if (method)
        method(tool);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwytool
 * @title: GwyTool
 * @short_description: Base class for tools
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
