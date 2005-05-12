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

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/gwyprocess.h>
#include <libgwydgets/gwydgets.h>
#include <string.h>
#include "gwyddion.h"

static GSList *palettes = NULL;

static void gwy_app_init_set_window_icon (void);

/**
 * gwy_app_init:
 *
 * Initializes all Gwyddion data types, i.e. types that may appear in
 * serialized data. GObject has to know about them when g_type_from_name()
 * is called.
 *
 * XXX: This function does much more. It registeres stock items, setups
 * palette presets, and similar things.
 **/
void
gwy_app_init(void)
{
    g_assert(palettes == NULL);

    gwy_widgets_type_init();
    if (gwy_gl_ok)
        gwy_gl_ok = gwy_widgets_gl_init();
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL);
    g_set_application_name(PACKAGE_NAME);
    gwy_gl_material_setup_presets();

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
gwy_app_set_find_self_style(const gchar *argv0)
{
    const gchar *ext ="/app/.libs/lt-gwyddion";
    gchar *p;

    if (g_str_has_suffix(argv0, ext)) {
        g_printerr("Warning: Running uninstalled, "
                   "but may be still using *installed* libraries.\n");
        p = g_strdup(argv0);
        strcpy(p + strlen(p) - strlen(ext) + 1, "gwyddion");
        gwy_find_self_set_argv0(p);
        g_free(p);
    }
}
#endif  /* G_OS_UNIX */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
