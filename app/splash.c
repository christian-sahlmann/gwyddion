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

#ifdef _MSC_VER
#include "version.h"
#else
#include "config.h"
#endif

#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>

static void splash_map(void);

static GtkWidget *window = NULL;
static GtkWidget *label = NULL;
gchar *message_prefix = NULL;

void
gwy_app_splash_create(void)
{
    GtkWidget *image, *vbox, *frame, *lab;
    char *p, *filename;

    gwy_debug("");
    g_return_if_fail(window == NULL);

    p = gwy_find_self_dir("pixmaps");
    filename = g_build_filename(p, "splash.png", NULL);
    g_free(p);

    window = gtk_window_new(GTK_WINDOW_POPUP);
    p = g_strconcat(_("Starting "), g_get_application_name(), NULL);
    gtk_window_set_title(GTK_WINDOW(window), p);
    gtk_window_set_wmclass(GTK_WINDOW(window), "splash",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(window),
                             GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);

    g_signal_connect_swapped(window, "delete_event",
                             G_CALLBACK(exit), GINT_TO_POINTER(0));
    /* we don't want the splash screen to send the startup notification */
    gtk_window_set_auto_startup_notification(FALSE);
    g_signal_connect(window, "map", G_CALLBACK(splash_map), NULL);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
    gtk_container_add(GTK_CONTAINER(window), frame);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    image = gtk_image_new_from_file(filename);
    gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, FALSE, 0);

    label = gtk_label_new(p);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(label), 6, 4);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    g_free(p);

    lab = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), lab, FALSE, FALSE, 0);

    p = g_strconcat("<small>", PACKAGE_NAME,
                    _(" is free software released under GNU GPL."), "</small>",
                    NULL);
    lab = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lab), p);
    gtk_misc_set_alignment(GTK_MISC(lab), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(lab), 5, 3);
    gtk_box_pack_start(GTK_BOX(vbox), lab, FALSE, FALSE, 0);
    g_free(p);

    gtk_widget_show_all(window);

    g_free(filename);

    while (gtk_events_pending())
        gtk_main_iteration();
}

void
gwy_app_splash_close(void)
{
    g_return_if_fail(window);
    gtk_widget_destroy(window);

    window = NULL;
    label = NULL;
    message_prefix = NULL;

    while (gtk_events_pending())
        gtk_main_iteration();
}

void
gwy_app_splash_set_message_prefix(const gchar *prefix)
{
    g_return_if_fail(window);

    g_free(message_prefix);
    message_prefix = g_strdup(prefix);

    while (gtk_events_pending())
        gtk_main_iteration();
}

void
gwy_app_splash_set_message(const gchar *message)
{
    g_return_if_fail(window);

    while (gtk_events_pending())
        gtk_main_iteration();

    if (message_prefix) {
        gchar *s = g_strconcat(message_prefix, message, NULL);
        gtk_label_set_markup(GTK_LABEL(label), s);
        g_free(s);
    }
    else
        gtk_label_set_markup(GTK_LABEL(label), message);

    while (gtk_events_pending())
        gtk_main_iteration();
}

static void
splash_map(void)
{
  /* Reenable startup notification after the splash has been shown
   * so that the next window that is mapped sends the notification. */
    gtk_window_set_auto_startup_notification(TRUE);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
