/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include "settings.h"

static GdkCursor *wait_cursor = NULL;
static GdkCursor *old_cursor = NULL;

static GtkWidget *wait_widget  = NULL;
static GwyWaitType *wait_type;
static GtkWidget *dialog = NULL;
static GtkWidget *progress = NULL;
static GtkWidget *label = NULL;
gchar *message_prefix = NULL;

void
gwy_wait_start_fow_window(GtkWidget *window,
                          GwyWaitType type,
                          const gchar *message)
{
    g_return_if_fail(GTK_IS_WIDGET(window));
    if (!GTK_IS_WINDOW(window))
        g_warning("widget is not a window");

    if (wait_widget) {
        g_warning("Already waiting on a widget, switching to the new one");
        gwy_wait_switch_widget(window, message);
        return;
    }

    switch (type) {
        case GWY_WAIT_PROGRESS:
        gwy_wait_create_dialog(window, message);
        break;

        case GWY_WAIT_CURSOR:
        gwy_wait_set_cursor(window);
    g_free(message_prefix);
    message_prefix = NULL;

    wait_widget = window;
}

static void
gwy_wait_set_cursor(GtkWidget *window)
{
    if (!wait_cursor)
        wait_cursor = gdk_cursor_new(GDK_WATCH);

    gdk_window_set_cursor(window->window, wait_cursor);
    gdk_window_get_cursor(window->window, wait_cursor);
    
}

static void
gwy_wait_create_dialog(GtkWidget *window,
                       const gchar *message)
{
    dialog = gtk_dialog_new_with_buttons(_("Please wait"),
                                         window,
                                         GTK_DIALOG_DESTROY_WITH_PARENT
                                         | GTK_DIALOG_NO_SEPARATOR
                                         | GTK_DIALOG_MODAL,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label), message);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label,
                       FALSE, FALSE, 4);

    progress = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 0.0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), progress,
                       FALSE, FALSE, 4);

    gtk_widget_show_all(dialog);
    while (gtk_events_pending())
        gtk_main_iteration();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
