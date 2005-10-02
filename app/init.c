/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/gwyprocess.h>
#include <libgwydgets/gwydgets.h>
#include <app/menu.h>
#include "gwyddion.h"

#ifdef HAVE_GTKGLEXT
#include <gtk/gtkglinit.h>
#endif

static GSList *palettes = NULL;

static void gwy_app_init_set_window_icon (void);

/**
 * gwy_app_init:
 * @argc: Address of the argc parameter of main(). Passed to
 *        gtk_gl_init_check().
 * @argv: Address of the argv parameter of main(). Passed to
 *        gtk_gl_init_check().
 *
 * Initializes all Gwyddion data types, i.e. types that may appear in
 * serialized data. GObject has to know about them when g_type_from_name()
 * is called.
 *
 * It registeres stock items, initializes tooltip class resources, sets
 * application icon, sets Gwyddion specific widget resources.
 *
 * If NLS is compiled in, it sets it up and binds text domains.
 *
 * If OpenGL is compiled in, it checks whether it's really available (calling
 * gtk_gl_init_check() and gwy_widgets_gl_init()).
 **/
void
gwy_app_init(int *argc,
             char ***argv)
{
    g_assert(palettes == NULL);

    gwy_widgets_type_init();
    gwy_gl_ok = gtk_gl_init_check(argc, argv) && gwy_widgets_gl_init();
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL);
    g_set_application_name(PACKAGE_NAME);
    /* XXX: These reference are never released. */
    gwy_data_window_class_set_tooltips(gwy_app_tooltips_get());
    gwy_3d_window_class_set_tooltips(gwy_app_tooltips_get());
    gwy_graph_window_class_set_tooltips(gwy_app_tooltips_get());

    gwy_app_init_set_window_icon();

    gtk_rc_parse_string(/* data window corner buttons */
                        "style \"cornerbutton\" {\n"
                        "GtkButton::focus_line_width = 0\n"
                        "GtkButton::focus_padding = 0\n"
                        "}\n"
                        "widget \"*.cornerbutton\" style \"cornerbutton\"\n"
                        "\n"
                        /* toolbox group header buttons */
                        "style \"toolboxheader\" {\n"
                        "GtkButton::focus_line_width = 0\n"
                        "GtkButton::focus_padding = 0\n"
                        "}\n"
                        "widget \"*.toolboxheader\" style \"toolboxheader\"\n"
                        "\n"
                        /* toolbox single-item menubars */
                        "style \"toolboxmenubar\" {\n"
                        "GtkMenuBar::shadow_type = 0\n"
                        "}\n"
                        "widget \"*.toolboxmenubar\" style \"toolboxmenubar\"\n"
                        "\n");
#ifdef ENABLE_NLS
#ifdef G_OS_WIN32
    bindtextdomain(PACKAGE, gwy_find_self_dir("locale"));
#else
    bindtextdomain(PACKAGE, LOCALEDIR);
#endif  /* G_OS_WIN32 */
    textdomain(PACKAGE);
    if (!bind_textdomain_codeset(PACKAGE, "UTF-8"))
        g_critical("Cannot bind gettext `%s' codeset to UTF-8", PACKAGE);
#endif  /* ENABLE_NLS */
}

static void
gwy_app_init_set_window_icon(void)
{
    gchar *filename, *p;
    GError *err = NULL;

    p = gwy_find_self_dir("pixmaps");
    filename = g_build_filename(p, "gwyddion.ico", NULL);
    gtk_window_set_default_icon_from_file(filename, &err);
    if (err) {
        g_warning("Cannot load window icon: %s", err->message);
        g_clear_error(&err);
    }
    g_free(filename);
    g_free(p);
}

#ifdef G_OS_WIN32
void
gwy_app_set_find_self_style(const gchar *argv0)
{
    gwy_find_self_set_argv0(argv0);
}
#endif  /* G_OS_WIN32 */

#ifdef G_OS_UNIX
void
gwy_app_set_find_self_style(G_GNUC_UNUSED const gchar *argv0)
{
}
#endif  /* G_OS_UNIX */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
