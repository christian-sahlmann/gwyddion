/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyversion.h>
#include <libgwydgets/gwystock.h>
#include "app.h"
#include "authors.h"

static void about_close   (void);
static void about_allocate(GtkWidget *vbox,
                           GtkAllocation *allocation,
                           GtkLabel *label);
static void fill_credits  (GtkTextBuffer *buffer);

static GtkWidget *about = NULL;

void
gwy_app_about(void)
{
    GtkWidget *vbox, *hbox, *widget, *credits, *text;
    GtkTextBuffer *buff;
    gchar *s, *s2;
    gint size;

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
    gtk_dialog_set_default_response(GTK_DIALOG(about), GTK_RESPONSE_CLOSE);
    gtk_container_set_border_width(GTK_CONTAINER(about), 6);
    gtk_window_set_transient_for(GTK_WINDOW(about),
                                 GTK_WINDOW(gwy_app_main_window_get()));
    gtk_window_set_position(GTK_WINDOW(about), GTK_WIN_POS_CENTER);
    g_free(s);

    vbox = GTK_DIALOG(about)->vbox;
    gtk_box_set_spacing(GTK_BOX(vbox), 8);

    hbox = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    size = gtk_icon_size_from_name(GWY_ICON_SIZE_ABOUT);
    widget = gtk_image_new_from_stock(GWY_STOCK_GWYDDION, size);

    gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
    gtk_misc_set_alignment(GTK_MISC(widget), 0.5, 0.0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    widget = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);
    s2 = g_strdup_printf("<big><b>%s</b> %s</big>\n",
                         g_get_application_name(), GWY_VERSION_STRING);
    s = g_strconcat(s2, _("An SPM data analysis framework."), NULL);
    gtk_label_set_markup(GTK_LABEL(widget), s);
    g_free(s);
    g_free(s2);

    widget = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(widget), 2, 6);
    gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);
    s = g_strdup_printf("<i>%s</i>", PACKAGE_URL);
    gtk_label_set_markup(GTK_LABEL(widget), s);
    g_free(s);
    gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(about)->vbox), vbox, TRUE, TRUE, 0);

    widget = gtk_label_new(_("Credits"));
    gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);

    credits = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(credits),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(credits, 320, 160);
    gtk_box_pack_start(GTK_BOX(vbox), credits, TRUE, TRUE, 0);

    buff = gtk_text_buffer_new(NULL);
    fill_credits(buff);

    text = gtk_text_view_new_with_buffer(buff);
    g_object_unref(buff);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(credits), text);

    widget = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
    s = g_strdup_printf(
            _("\n"
              "<small>%s is free software; "
              "you can redistribute it and/or modify it "
              "under the terms of the GNU General Public License "
              "as published by the Free Software Foundation; "
              "either version 2 of the License, or (at your option) "
              "any later version. For full license text see file COPYING "
              "included in the source tarball.</small>"),
            g_get_application_name());
    gtk_label_set_markup(GTK_LABEL(widget), s);
    g_free(s);
    gtk_label_set_line_wrap(GTK_LABEL(widget), TRUE);
    gtk_label_set_selectable(GTK_LABEL(widget), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);
    gtk_widget_set_size_request(widget, 320, -1);

    gtk_widget_show_all(about);

    g_signal_connect(about, "delete-event",
                     G_CALLBACK(about_close), NULL);
    g_signal_connect(about, "response",
                     G_CALLBACK(about_close), NULL);
    g_signal_connect(GTK_DIALOG(about)->vbox, "size-allocate",
                     G_CALLBACK(about_allocate), widget);
}

static void
about_allocate(G_GNUC_UNUSED GtkWidget *vbox,
               GtkAllocation *allocation,
               GtkLabel *label)
{
    pango_layout_set_width(gtk_label_get_layout(label),
                           PANGO_SCALE*allocation->width);
}

static void
about_close(void)
{
    gtk_widget_destroy(about);
    about = NULL;
}

static void
add_credits_block(GtkTextBuffer *buffer,
                  const gchar *title,
                  guint n,
                  const gchar **list)
{
    GtkTextIter iter;
    guint i;

    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, title, -1,
                                             "b", NULL);
    gtk_text_buffer_insert(buffer, &iter, "\n", 1);

    for (i = 0; i < n; i++) {
        gtk_text_buffer_insert(buffer, &iter, list[i], -1);
        gtk_text_buffer_insert(buffer, &iter, "\n", 1);
    }
    gtk_text_buffer_insert(buffer, &iter, "\n", 1);
}

static void
fill_credits(GtkTextBuffer *buffer)
{
    GtkTextIter iter;

    gtk_text_buffer_create_tag(buffer, "uri",
                               "style", PANGO_STYLE_ITALIC,
                               "wrap-mode", GTK_WRAP_NONE,
                               NULL);
    gtk_text_buffer_create_tag(buffer, "b",
                               "weight", PANGO_WEIGHT_BOLD,
                               NULL);

    add_credits_block(buffer, _("Core Developers"),
                      G_N_ELEMENTS(core_developers), core_developers);
    add_credits_block(buffer, _("Developers"),
                      G_N_ELEMENTS(developers), developers);
    add_credits_block(buffer, _("Translators"),
                      G_N_ELEMENTS(translators), translators);

    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter,
                           _("Development is supported by "
                             "the Czech Metrology Institute: "), -1);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
                                             "http://www.cmi.cz/", -1,
                                             "uri", NULL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
