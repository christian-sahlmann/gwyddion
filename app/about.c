/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwystock.h>
#include "app.h"

static void about_close (void);

static GtkWidget *about = NULL;

void
gwy_app_about(void)
{
    GtkWidget *vbox, *hbox, *widget;
    gchar *s;

    if (about) {
        gtk_window_present(GTK_WINDOW(about));
        return;
    }
    s = g_strdup_printf(_("About %s"), g_get_application_name());
    about = gtk_dialog_new_with_buttons(s,
                                        GTK_WINDOW(gwy_app_main_window_get()),
                                        GTK_DIALOG_NO_SEPARATOR
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                        NULL);
    gtk_container_set_border_width(GTK_CONTAINER(about), 6);
    gtk_window_set_resizable(GTK_WINDOW(about), FALSE);
    gtk_window_set_transient_for(GTK_WINDOW(about),
                                 GTK_WINDOW(gwy_app_main_window_get()));
    gtk_window_set_position(GTK_WINDOW(about), GTK_WIN_POS_CENTER_ON_PARENT);
    g_free(s);

    vbox = GTK_DIALOG(about)->vbox;
    gtk_box_set_spacing(GTK_BOX(vbox), 12);

    hbox = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    widget = gtk_image_new_from_stock(GWY_STOCK_GWYDDION,
                                      gtk_icon_size_from_name(
                                                         GWY_ICON_SIZE_ABOUT));

    gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
    gtk_misc_set_alignment(GTK_MISC(widget), 0.5, 0.0);

    widget = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
    s = g_strdup_printf(
            _("<big><b>%s</b> %s</big>\n"
              "A SPM data analysis framework.\n"
              "\n"
              "Copyright © 2003 David Nečas (Yeti) and Petr Klapetek.\n"
              "E-mail: <i>%s</i>\n"
              "Web: <i>%s</i>\n"
              "\n"
              "%s development was funded by the Czech Metrology Insitute "
              "(<i>http://www.cmi.cz/</i>).\n"
              "\n"
              "<small>%s is free software; "
              "you can redistribute it and/or modify it "
              "under the terms of the GNU General Public License "
              "as published by the Free Software Foundation; "
              "either version 2 of the License, or (at your option) "
              "any later version. For full license text see file COPYING "
              "included in the source tarball.</small>"),
            g_get_application_name(),
            PACKAGE_VERSION,
            PACKAGE_BUGREPORT,
            PACKAGE_URL,
            g_get_application_name(),
            g_get_application_name());
    gtk_label_set_markup(GTK_LABEL(widget), s);
    g_free(s);
    gtk_label_set_line_wrap(GTK_LABEL(widget), TRUE);
    gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

    g_signal_connect(about, "delete_event",
                     G_CALLBACK(about_close), NULL);
    g_signal_connect(about, "response",
                     G_CALLBACK(about_close), NULL);
    gtk_widget_show_all(about);
}

static void
about_close(void)
{
    gtk_widget_destroy(about);
    about = NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
