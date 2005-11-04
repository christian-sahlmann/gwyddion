/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
static gboolean silent_waiting = FALSE;

/**
 * gwy_app_wait_start:
 * @window: A window.
 * @message: A message to show in the wait dialog.
 *
 * Starts waiting for a window @window, creating a dialog with a progress bar.
 *
 * Waiting is global, there can be only one at a time.
 **/
void
gwy_app_wait_start(GtkWidget *window,
                   const gchar *message)
{
    if (window && !GTK_IS_WINDOW(window))
        g_warning("Widget is not a window");

    if (wait_widget || silent_waiting) {
        g_warning("Already waiting on a widget, switching to the new one");
        gwy_app_wait_switch_widget(window, message);
        return;
    }

    canceled = FALSE;
    if (!window)
        silent_waiting = TRUE;
    else
        gwy_app_wait_create_dialog(window, message);
    wait_widget = window;
}

/**
 * gwy_app_wait_finish:
 *
 * Finishes waiting, closing the dialog.
 *
 * No function like gwy_app_wait_set_message() should be call after that.
 *
 * This function must be called even if user cancelled the operation.
 **/
void
gwy_app_wait_finish(void)
{
    if (canceled) {
        canceled = FALSE;
        return;
    }

    if (!silent_waiting) {
        g_return_if_fail(dialog != NULL);
        gtk_widget_destroy(dialog);
        g_free(message_prefix);
    }
    dialog = NULL;
    progress = NULL;
    label = NULL;
    message_prefix = NULL;
    wait_widget = NULL;
    silent_waiting = FALSE;
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
    gtk_window_present(GTK_WINDOW(dialog));
    while (gtk_events_pending())
        gtk_main_iteration();
}

/**
 * gwy_app_wait_switch_widget:
 * @window: A window.
 * @message: A mesage to show now (%NULL for keep the present one).
 *
 * Switches the waiting window.
 *
 * FIXME: This is probably both broken and nonsense.
 *
 * Returns: %TRUE if the operation can continue, %FALSE if user cancelled it
 *          meanwhile.
 **/
gboolean
gwy_app_wait_switch_widget(GtkWidget *window,
                           const gchar *message)
{
    if (!window || !silent_waiting) {
        g_warning("Cannot switch between normal and silent waiting.");
        return TRUE;
    }

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
    return !canceled;
}

/**
 * gwy_app_wait_set_message:
 * @message: A mesage to show in the progress dialog.
 *
 * Sets the message shown on the progress dialog.
 *
 * See also gwy_app_wait_set_message_prefix() which makes this function more
 * usable directly as a callback.
 *
 * Returns: %TRUE if the operation can continue, %FALSE if user cancelled it
 *          meanwhile.
 **/
gboolean
gwy_app_wait_set_message(const gchar *message)
{
    if (silent_waiting)
        return TRUE;

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

/**
 * gwy_app_wait_set_message_prefix:
 * @prefix: The prefix for new messages.
 *
 * Sets prefix for the messages shown in the progress dialog.
 *
 * The prefix will take effect in the next gwy_app_wait_set_message() call.
 *
 * Returns: %TRUE if the operation can continue, %FALSE if user cancelled it
 *          meanwhile.
 **/
gboolean
gwy_app_wait_set_message_prefix(const gchar *prefix)
{
    if (silent_waiting)
        return TRUE;
    if (canceled)
        return FALSE;

    g_return_val_if_fail(dialog, FALSE);
    g_free(message_prefix);
    message_prefix = g_strdup(prefix);

    while (gtk_events_pending())
        gtk_main_iteration();

    return !canceled;
}

/**
 * gwy_app_wait_set_fraction:
 * @fraction: The progress of the operation, as a number from 0 to 1.
 *
 * Sets the amount of progress the progress bar on the dialog displays.
 *
 * Returns: %TRUE if the operation can continue, %FALSE if user cancelled it
 *          meanwhile.
 **/
gboolean
gwy_app_wait_set_fraction(gdouble fraction)
{
    gchar buf[8];

    if (silent_waiting)
        return TRUE;

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

/************************** Documentation ****************************/

/**
 * SECTION:wait
 * @title: wait
 * @short_description: Informing the world we are busy
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
