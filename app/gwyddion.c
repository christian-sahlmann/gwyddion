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

#ifdef _MSC_VER
#include "version.h"
#else
#ifdef HAVE_CONFIG_H
#include "config.h"
#else
/* XXX: Invent some stuff... */
#endif
#endif  /* _MSC_VER */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <libgwyddion/gwywin32unistd.h>

#include <gtk/gtkglinit.h>
#include <libgwymodule/gwymodule.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwyutils.h>
#include <libgwydgets/gwystock.h>
#include "gwyapp.h"
#include "gwyappinternal.h"
#include "gwyddion.h"

#ifdef G_OS_WIN32
#define LOG_TO_FILE 1
#else
#undef LOG_TO_FILE
#endif

#ifdef LOG_TO_FILE
static void setup_logging(void);
static void logger(const gchar *log_domain,
                   GLogLevelFlags log_level,
                   const gchar *message,
                   gpointer user_data);
#endif  /* LOG_TO_FILE */
static void print_help(void);
static void process_preinit_options(int *argc,
                                    char ***argv);
static void warn_broken_settings_file(GtkWidget *parent,
                                      const gchar *settings_file);

gboolean gwy_gl_ok = FALSE;

int
main(int argc, char *argv[])
{
    GtkWidget *toolbox;
    gchar **module_dirs;
    gchar *config_file, *settings_file, *recent_file_file;
    gboolean has_config, has_settings, settings_ok = FALSE;

#ifdef G_OS_WIN32
    gwy_find_self_set_argv0(argv[0]);
#endif  /* G_OS_WIN32 */

    gwy_debug_objects_enable(TRUE);
    process_preinit_options(&argc, &argv);
    gwy_app_settings_create_config_dir();
    /* FIXME: somewhat late, actually even gwy_find_self_set_argv0() which MUST
     * be run first can print things to console when debuggin is enabled. */
#ifdef LOG_TO_FILE
    setup_logging();
#endif  /* LOG_TO_FILE */

    gtk_init(&argc, &argv);
    gwy_gl_ok = gtk_gl_init_check(&argc, &argv);

    config_file = gwy_app_settings_get_config_filename();
    has_config = g_file_test(config_file, G_FILE_TEST_IS_REGULAR);
    gwy_debug("Binary config file is `%s'. Do we have it: %s",
              config_file, has_config ? "TRUE" : "FALSE");
    settings_file = gwy_app_settings_get_settings_filename();
    has_settings = g_file_test(settings_file, G_FILE_TEST_IS_REGULAR);
    gwy_debug("Text settings file is `%s'. Do we have it: %s",
              settings_file, has_settings ? "TRUE" : "FALSE");
    gwy_app_init();

    gwy_app_splash_create();
    gwy_app_splash_set_message(_("Loading settings"));
    if (has_settings)
        settings_ok = gwy_app_settings_load(settings_file);
    gwy_debug("Loading settings was: %s", settings_ok ? "OK" : "Not OK");
    if (!settings_ok && has_config)
        gwy_app_settings_load_bin(config_file);

    /* TODO: remove sometime, but keep the gwy_app_settings_get(); */
    gwy_app_splash_set_message(_("Loading document history"));
    gwy_container_remove_by_prefix(gwy_app_settings_get(), "/app/recent");
    recent_file_file = gwy_app_settings_get_recent_file_list_filename();
    gwy_app_recent_file_list_load(recent_file_file);

    gwy_app_splash_set_message_prefix(_("Registering "));
    gwy_app_splash_set_message(_("stock items"));
    gwy_stock_register_stock_items();

    gwy_module_set_register_callback(gwy_app_splash_set_message);
    module_dirs = gwy_app_settings_get_module_dirs();
    gwy_module_register_modules((const gchar**)module_dirs);
    gwy_module_set_register_callback(NULL);
    gwy_app_splash_set_message_prefix(NULL);

    gwy_app_splash_set_message("Initializing GUI");
    toolbox = gwy_app_toolbox_create();
    gwy_app_recent_file_list_update(NULL, NULL, NULL);
    gwy_app_splash_close();

    gwy_app_file_open_initial(argv + 1, argc - 1);
    if (has_settings && !settings_ok)
        warn_broken_settings_file(toolbox, settings_file);

    gtk_main();
    if ((settings_ok || !has_settings)
        && gwy_app_settings_save(settings_file)) {
        if (has_config) {
            g_warning("Converted settings to human-readable form, "
                      "deleting old one");
            unlink(config_file);
        }
    }
    gwy_app_recent_file_list_save(recent_file_file);
    gwy_app_settings_free();
    g_free(recent_file_file);
    g_free(config_file);
    g_free(settings_file);
    g_strfreev(module_dirs);
    gwy_app_recent_file_list_free();
    /* FIXME: This crashes in Win32.  Do not enable it.
     * I've run it in a debugger, but still don't know why -- stderr becomes
     * unusable somehow? */
#ifndef G_OS_WIN32
    gwy_debug_objects_dump_to_file(stderr, 0);
#endif
    gwy_debug_objects_clear();

    return 0;
}

static void
process_preinit_options(int *argc,
                        char ***argv)
{
    int i;

    if (*argc == 1)
        return;

    if (!strcmp((*argv)[1], "--help") || !strcmp((*argv)[1], "-h")) {
        print_help();
        exit(0);
    }

    if (!strcmp((*argv)[1], "--version") || !strcmp((*argv)[1], "-v")) {
        printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
        exit(0);
    }

    for (i = 1; i < *argc; i++) {
        if (strncmp((*argv)[i], "--", 2) || !strcmp((*argv)[i], "--"))
            break;

        if (!strcmp((*argv)[i], "--no-splash"))
            gwy_app_splash_enable(FALSE);
    }

    (*argv)[i-1] = (*argv)[0];
    *argv += i-1;
    *argc -= i-1;
}

static void
print_help(void)
{
    puts(
"Usage: gwyddion [OPTIONS...] FILE...\n"
"An SPM data analysis framework, written in Gtk+.\n"
        );
    puts(
"Gwyddion options:\n"
" -h, --help                 Print this help and terminate.\n"
" -v, --version              Print version info and terminate.\n"
"     --no-splash            Don't show splash screen.\n"
        );
    puts(
"Gtk+ and Gdk options:\n"
"     --display=DISPLAY      Set X display to use.\n"
"     --screen=SCREEN        Set X screen to use.\n"
"     --sync                 Make X calls synchronous.\n"
"     --name=NAME            Set program name as used by the window manager.\n"
"     --class=CLASS          Set program class as used by the window manager.\n"
"     --gtk-module=MODULE    Load an additional Gtk module MODULE.\n"
"They may be other Gtk+, Gdk, and GtkGLExt options, depending on platform, on\n"
"how it was compiled, and on loaded modules.  Please see Gtk+ documentation.\n"
        );
    puts("Please report bugs in Gwyddion bugzilla "
         "http://trific.ath.cx/bugzilla/");
}

static void
warn_broken_settings_file(GtkWidget *parent,
                          const gchar *settings_file)
{
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new
                 (GTK_WINDOW(parent),
                  GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_WARNING,
                  GTK_BUTTONS_OK,
                  _("Could not read settings file `%s'.\n\n"
                    "Settings will not be saved "
                    "until it is repaired or removed."),
                  settings_file);
    /* parent is usually in a screen corner, centering on it looks ugly */
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_widget_show_all(dialog);
}

#ifdef LOG_TO_FILE
/* Redirect messages from all libraries we use to a file.  This (a) creates
 * a possibly useful log if we don't crash totally (b) prevents the mesages
 * to go to a DOS console thus creating it. */
static void
setup_logging(void)
{
    const gchar *domains[] = {
        "GLib", "GLib-GObject", "GModule", "GThread",
        "GdkPixbuf", "Gdk", "Gtk",
        "GdkGLExt", "GtkGLExt",
        "Gwyddion", "GwyProcess", "GwyDraw", "Gwydgets", "GwyModule", "GwyApp",
        "Module", NULL
    };
    gchar *log_filename;
    gsize i;
    FILE *logfile;

    log_filename = gwy_app_settings_get_log_filename();
    logfile = fopen(log_filename, "w");
    for (i = 0; i < G_N_ELEMENTS(domains); i++)
        g_log_set_handler(domains[i],
                          G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_MESSAGE
                          | G_LOG_LEVEL_INFO | G_LOG_LEVEL_WARNING,
                          logger, logfile);
}

static void
logger(const gchar *log_domain,
       G_GNUC_UNUSED GLogLevelFlags log_level,
       const gchar *message,
       gpointer user_data)
{
    FILE *logfile = (FILE*)user_data;

    if (!logfile)
        return;
    fprintf(logfile, "%s: %s\n", log_domain, message);
    fflush(logfile);
}
#endif  /* LOG_TO_FILE */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
