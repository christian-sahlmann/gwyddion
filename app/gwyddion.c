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
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#else
#ifdef _MSC_VER
#define unlink(name) _unlink(name)
#else
int unlink(const char *name);
#endif
#endif

#include <libgwymodule/gwymodule.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwydgets/gwystock.h>
#include "app.h"
#include "settings.h"
#include "gwyappinternal.h"
#include "gwyddion.h"

#ifdef G_OS_WIN32
static void setup_logging(void);
static void logger(const gchar *log_domain,
                   GLogLevelFlags log_level,
                   const gchar *message,
                   gpointer user_data);
#endif
static void print_help(void);
static void process_preinit_options(int *argc,
                                    char ***argv);

int
main(int argc, char *argv[])
{
    gchar **module_dirs;
    gchar *config_file, *settings_file;
    gboolean has_config, has_settings, ok = FALSE;

#ifdef G_OS_WIN32
    gwy_find_self_set_argv0(argv[0]);
#endif

    process_preinit_options(&argc, &argv);
    gwy_app_settings_create_config_dir();
#ifdef G_OS_WIN32
    setup_logging();
#endif

    gtk_init(&argc, &argv);
    config_file = gwy_app_settings_get_config_filename();
    has_config = g_file_test(config_file, G_FILE_TEST_IS_REGULAR);
    settings_file = gwy_app_settings_get_settings_filename();
    has_settings = g_file_test(settings_file, G_FILE_TEST_IS_REGULAR);
    gwy_app_init();

    gwy_app_splash_create();
    gwy_app_splash_set_message(_("Loading settings"));
    if (has_settings)
        ok = gwy_app_settings_load(settings_file);
    if (!ok && has_config)
        gwy_app_settings_load_bin(config_file);
    gwy_app_settings_get();

    gwy_app_splash_set_message_prefix(_("Registering "));
    gwy_app_splash_set_message(_("stock items"));
    gwy_stock_register_stock_items();

    gwy_module_set_register_callback(gwy_app_splash_set_message);
    module_dirs = gwy_app_settings_get_module_dirs();
    gwy_module_register_modules((const gchar**)module_dirs);
    gwy_module_set_register_callback(NULL);
    gwy_app_splash_set_message_prefix(NULL);

    gwy_app_splash_set_message("Initializing GUI");
    gwy_app_toolbox_create();
    gwy_app_splash_close();

    gwy_app_file_open_initial(argv + 1, argc - 1);

    gtk_main();
    if (gwy_app_settings_save(settings_file)) {
        if (has_config) {
            g_warning("Converted settings to human-readable form, "
                      "deleting old one");
            unlink(config_file);
        }
    }
    gwy_app_settings_free();
    g_free(config_file);

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
"They may be other Gtk+ and Gdk options, depending on platform, how it was\n"
"compiled, and loaded modules.  Please see Gtk+ documentation.\n"
        );
    puts("Please report bugs in Gwyddion bugzilla "
         "http://trific.ath.cx/bugzilla/");
}

#ifdef WIN32
#define _X86_
#include <windef.h>

int
APIENTRY WinMain(HINSTANCE hInstance,
                 HINSTANCE hPrevInstance,
                 LPSTR     lpCmdLine,
                 int       nCmdShow)
{
    /* FIXME: file_real_open() now expects filenames in system encoding, not
     * UTF-8, how this works on MS Windows? */
    return main(_argc, _argv);

}

#endif /* WIN32 */

#ifdef G_OS_WIN32
/* Redirect messages from all libraries we use to a file.  This (a) creates
 * a possibly useful log if we don't crash totally (b) prevents the mesages
 * to go to a DOS console thus creating it. */
static void
setup_logging(void)
{
    const gchar *domains[] = {
        "GLib", "GLib-GObject", "GModule", "GThread",
        "GdkPixbuf", "Gdk", "Gtk",
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
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
