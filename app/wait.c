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
#include <gtk/gtk.h>
#include "wait.h"

static void gwy_app_wait_create_dialog (GtkWidget *window,
                                        const gchar *message);
static void gwy_app_wait_canceled      (void);

static GtkWidget *wait_widget  = NULL;
static GtkWidget *dialog = NULL;
static GtkWidget *progress = NULL;
static GtkWidget *label = NULL;
static gchar *message_prefix = NULL;
static gboolean canceled = FALSE;

void
gwy_app_wait_start(GtkWidget *window,
                   const gchar *message)
{
    g_return_if_fail(GTK_IS_WIDGET(window));
    if (!GTK_IS_WINDOW(window))
        g_warning("widget is not a window");

    if (wait_widget) {
        g_warning("Already waiting on a widget, switching to the new one");
        gwy_app_wait_switch_widget(window, message);
        return;
    }

    canceled = FALSE;
    gwy_app_wait_create_dialog(window, message);
    wait_widget = window;
}

void
gwy_app_wait_finish(void)
{
    if (canceled) {
        canceled = FALSE;
        return;
    }

    g_return_if_fail(dialog != NULL);

    gtk_widget_destroy(dialog);
    g_free(message_prefix);
    dialog = NULL;
    progress = NULL;
    label = NULL;
    message_prefix = NULL;
}

static void
gwy_app_wait_create_dialog(GtkWidget *window,
                           const gchar *message)
{
    dialog = gtk_dialog_new_with_buttons(_("Please wait"),
                                         GTK_WINDOW(window),
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

    g_signal_connect(dialog, "response",
                     G_CALLBACK(gwy_app_wait_canceled), NULL);

    gtk_widget_show_all(dialog);
    while (gtk_events_pending())
        gtk_main_iteration();
}

gboolean
gwy_app_wait_switch_widget(GtkWidget *window,
                           const gchar *message)
{
    while (gtk_events_pending())
        gtk_main_iteration();
    if (canceled)
        return FALSE;

    wait_widget = window;
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
    while (gtk_events_pending())
        gtk_main_iteration();

    if (message)
        gwy_app_wait_set_message(message);
    return canceled;
}

gboolean
gwy_app_wait_set_message(const gchar *message)
{
    while (gtk_events_pending())
        gtk_main_iteration();
    if (canceled)
        return FALSE;

    g_return_val_if_fail(dialog, FALSE);
    if (message_prefix) {
        gchar *s = g_strconcat(message_prefix, message, NULL);
        gtk_label_set_markup(GTK_LABEL(label), s);
        g_free(s);
    }
    else
        gtk_label_set_markup(GTK_LABEL(label), message);

    while (gtk_events_pending())
        gtk_main_iteration();

    return !canceled;
}

gboolean
gwy_app_wait_set_message_prefix(const gchar *prefix)
{
    if (canceled)
        return FALSE;

    g_return_val_if_fail(dialog, FALSE);
    g_free(message_prefix);
    message_prefix = g_strdup(prefix);

    while (gtk_events_pending())
        gtk_main_iteration();

    return !canceled;
}

gboolean
gwy_app_wait_set_fraction(gdouble fraction)
{
    gchar buf[8];

    while (gtk_events_pending())
        gtk_main_iteration();
    if (canceled)
        return FALSE;

    g_return_val_if_fail(dialog, FALSE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), fraction);
    if (fraction < 0.0 || fraction > 1.0) {
        g_warning("Fraction outside [0, 1] range");
        fraction = CLAMP(fraction, 0.0, 1.0);
    }
    g_snprintf(buf, sizeof(buf), "%d %%", (gint)(100*fraction + 0.4));
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), buf);

    while (gtk_events_pending())
        gtk_main_iteration();

    return !canceled;
}

static void
gwy_app_wait_canceled(void)
{
    gwy_app_wait_finish();
    canceled = TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
