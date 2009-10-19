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
#ifdef HAVE_GTKGLEXT
#include <gtk/gtkgl.h>
#endif
#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyversion.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymoduleloader.h>
#include "app.h"
#include "authors.h"
#include "gwyddion.h"

static void about_close  (void);
static void fill_credits (GtkTextBuffer *buffer);
static void fill_features(GtkTextBuffer *buffer);

static GtkWidget *about = NULL;

void
gwy_app_about(void)
{
    GtkWidget *vbox, *hbox, *widget, *credits, *text, *notebook;
    GtkTextBuffer *buff;
    GtkTextIter iter;
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
    g_free(s);
    gtk_dialog_set_default_response(GTK_DIALOG(about), GTK_RESPONSE_CLOSE);
    gtk_container_set_border_width(GTK_CONTAINER(about), 6);
    gtk_window_set_transient_for(GTK_WINDOW(about),
                                 GTK_WINDOW(gwy_app_main_window_get()));
    gtk_window_set_position(GTK_WINDOW(about), GTK_WIN_POS_CENTER);
    gwy_app_add_main_accel_group(GTK_WINDOW(about));

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
    s = g_strconcat(s2, _("An SPM data visualization and analysis tool."),
                    NULL);
    gtk_label_set_markup(GTK_LABEL(widget), s);
    g_free(s);
    g_free(s2);

    widget = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(widget), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(widget), 2, 6);
    gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);
    s = g_strdup_printf(_("<i>%s</i>\nReport bugs to: <i>%s</i>"),
                        PACKAGE_URL, PACKAGE_BUGREPORT);
    gtk_label_set_markup(GTK_LABEL(widget), s);
    g_free(s);
    gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

    notebook = gtk_notebook_new();
    gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(about)->vbox), notebook,
                       TRUE, TRUE, 0);

    /* Credits */
    credits = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(credits),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(credits, 320, 160);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             credits, gtk_label_new(_("Credits")));

    buff = gtk_text_buffer_new(NULL);
    fill_credits(buff);

    text = gtk_text_view_new_with_buffer(buff);
    g_object_unref(buff);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(credits), text);

    /* License */
    credits = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(credits),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(credits, 320, 160);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             credits, gtk_label_new(_("License")));

    buff = gtk_text_buffer_new(NULL);
    gtk_text_buffer_get_end_iter(buff, &iter);
    s = g_strdup_printf(
            _("%s is free software; "
              "you can redistribute it and/or modify it "
              "under the terms of the GNU General Public License "
              "as published by the Free Software Foundation; "
              "either version 2 of the License, or (at your option) "
              "any later version. For full license text see file COPYING "
              "included in the source tarball."),
            g_get_application_name());
    gtk_text_buffer_insert(buff, &iter, s, -1);
    g_free(s);

    text = gtk_text_view_new_with_buffer(buff);
    g_object_unref(buff);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(credits), text);

    /* Features */
    credits = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(credits),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(credits, 320, 160);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             credits, gtk_label_new(_("Features")));

    buff = gtk_text_buffer_new(NULL);
    fill_features(buff);

    text = gtk_text_view_new_with_buffer(buff);
    g_object_unref(buff);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(credits), text);

    gtk_widget_show_all(about);

    g_signal_connect(about, "delete-event",
                     G_CALLBACK(about_close), NULL);
    g_signal_connect(about, "response",
                     G_CALLBACK(about_close), NULL);
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
                  const gchar *body,
                  gboolean italicize)
{
    GtkTextIter iter;

    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, title, -1,
                                             "b", NULL);
    gtk_text_buffer_insert(buffer, &iter, "\n", 1);
    if (italicize)
        gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, body, -1,
                                                 "i", NULL);
    else
        gtk_text_buffer_insert(buffer, &iter, body, -1);
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

    add_credits_block(buffer, _("Developers"), developers, FALSE);
    add_credits_block(buffer, _("Translators"), translators, FALSE);

    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter,
                           _("Development is supported by "
                             "the Czech Metrology Institute: "), -1);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
                                             "http://www.cmi.cz/", -1,
                                             "uri", NULL);
}

static void
fill_features(GtkTextBuffer *buffer)
{
    const GwyModuleInfo *modinfo;
    G_GNUC_UNUSED gchar *s;
    G_GNUC_UNUSED const gchar *cs;
    gboolean b;

    gtk_text_buffer_create_tag(buffer, "b",
                               "weight", PANGO_WEIGHT_BOLD,
                               NULL);
    gtk_text_buffer_create_tag(buffer, "i",
                               "style", PANGO_STYLE_ITALIC,
                               NULL);

#ifdef HAVE_GTKGLEXT
    s = g_strdup_printf("GtkGLExt %d.%d.%d\n",
                        gtkglext_major_version,
                        gtkglext_minor_version,
                        gtkglext_micro_version);
    add_credits_block(buffer, _("OpenGL 3D View"), s, FALSE);
    g_free(s);
#else
    add_credits_block(buffer, _("OpenGL 3D View"),
                      _("not available\n"), TRUE);
#endif

#ifdef HAVE_FFTW3
    s = g_strdup_printf("FFTW %s\n", fftw_version);
    add_credits_block(buffer, _("Fast Fourier Transform"), s, FALSE);
    g_free(s);
#else
    add_credits_block(buffer, _("Fast Fourier Transform"),
                      _("built-in SimpleFFT\n"), TRUE);
#endif

    b = FALSE;
#if (REMOTE_BACKEND == REMOTE_NONE)
    cs = _("not available\n");
    b = TRUE;
#elif (REMOTE_BACKEND == REMOTE_X11)
    cs = _("X11 protocol\n");
#elif (REMOTE_BACKEND == REMOTE_WIN32)
    cs = _("Win32 protocol\n");
#elif (REMOTE_BACKEND == REMOTE_UNIQUE)
    cs = _("LibUnique\n");
#else
#error "An unknown remote control backend."
#endif
    add_credits_block(buffer, _("Remote Control"), cs, b);

    if ((modinfo = gwy_module_lookup("pygwy"))) {
        s = g_strdup_printf("pygwy %s\n", modinfo->version);
        add_credits_block(buffer, _("Python Scripting Interface"), s, FALSE);
        g_free(s);
    }
    else
        add_credits_block(buffer, _("Python Scripting Interface"),
                          _("not available\n"), TRUE);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
