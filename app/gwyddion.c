/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libdraw/gwydraw.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>
#include <app/gwyappinternal.h>
#include "gwyddion.h"

#ifdef G_OS_WIN32
#define LOG_TO_FILE 1
#else
#undef LOG_TO_FILE
#endif

#ifdef LOG_TO_FILE
static void setup_logging(void);
static void logger       (const gchar *log_domain,
                          GLogLevelFlags log_level,
                          const gchar *message,
                          gpointer user_data);
#endif  /* LOG_TO_FILE */

static void open_command_line_files  (gchar **args,
                                      gint n);
static void print_help               (void);
static void process_preinit_options  (int *argc,
                                      char ***argv);
static void warn_broken_settings_file(GtkWidget *parent,
                                      const gchar *settings_file,
                                      const gchar *reason);
static void gwy_app_init             (int *argc,
                                      char ***argv);
static void gwy_app_set_window_icon  (void);

static gboolean enable_object_debugging = FALSE;

int
main(int argc, char *argv[])
{
    GtkWidget *toolbox, *data_browser;
    gchar **module_dirs;
    gchar *settings_file, *recent_file_file;
    gboolean has_settings, settings_ok = FALSE;
    GError *settings_err = NULL;

    process_preinit_options(&argc, &argv);
    gwy_debug_objects_enable(enable_object_debugging);
    /* TODO: handle failure */
    gwy_app_settings_create_config_dir(NULL);
    /* FIXME: somewhat late, actually even gwy_find_self_set_argv0() which MUST
     * be run first can print things to console when debuggin is enabled. */
#ifdef LOG_TO_FILE
    setup_logging();
#endif  /* LOG_TO_FILE */

    gtk_init(&argc, &argv);
    gwy_app_init(&argc, &argv);

    settings_file = gwy_app_settings_get_settings_filename();
    has_settings = g_file_test(settings_file, G_FILE_TEST_IS_REGULAR);
    gwy_debug("Text settings file is `%s'. Do we have it: %s",
              settings_file, has_settings ? "TRUE" : "FALSE");

    gwy_app_splash_create();

    gwy_app_splash_set_message(_("Loading document history"));
    recent_file_file = gwy_app_settings_get_recent_file_list_filename();
    gwy_app_recent_file_list_load(recent_file_file);

    gwy_app_splash_set_message_prefix(_("Registering "));
    gwy_app_splash_set_message(_("stock items"));
    gwy_stock_register_stock_items();

    gwy_app_splash_set_message(_("color gradients"));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GRADIENT));
    gwy_app_splash_set_message(_("GL materials"));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GL_MATERIAL));
    gwy_app_splash_set_message_prefix(NULL);

    gwy_app_splash_set_message(_("Loading settings"));
    if (has_settings)
        settings_ok = gwy_app_settings_load(settings_file, &settings_err);
    gwy_debug("Loading settings was: %s", settings_ok ? "OK" : "Not OK");
    gwy_app_settings_get();

    gwy_app_splash_set_message(_("Registering modules"));
    module_dirs = gwy_app_settings_get_module_dirs();
    gwy_module_register_modules((const gchar**)module_dirs);

    gwy_app_splash_set_message(_("Initializing GUI"));
    toolbox = gwy_app_toolbox_create();
    data_browser = gwy_app_data_browser_create();
    gwy_app_recent_file_list_update(NULL, NULL, NULL);
    gwy_app_splash_close();

    open_command_line_files(argv + 1, argc - 1);
    if (has_settings && !settings_ok) {
        warn_broken_settings_file(toolbox,
                                  settings_file, settings_err->message);
        g_clear_error(&settings_err);
    }

    /* Move focus to toolbox */
    gtk_window_present(GTK_WINDOW(toolbox));

    gtk_main();

    /* TODO: handle failure */
    if (settings_ok || !has_settings)
        gwy_app_settings_save(settings_file, NULL);
    gwy_app_recent_file_list_save(recent_file_file);
    gwy_process_func_save_use();
    gwy_app_settings_free();
    gwy_debug_objects_dump_to_file(stderr, 0);
    gwy_debug_objects_clear();
    gwy_app_recent_file_list_free();
    /* XXX: EXIT-CLEAN-UP */
    /* Finalize all gradients.  Useless, but makes --debug-objects happy.
     * Remove in production version. */
    g_object_unref(gwy_gradients());
    g_object_unref(gwy_gl_materials());
    g_free(recent_file_file);
    g_free(settings_file);
    g_strfreev(module_dirs);

    return 0;
}

static void
process_preinit_options(int *argc,
                        char ***argv)
{
    int i, j;
    gboolean ignore = FALSE;

    if (*argc == 1)
        return;

    if (gwy_strequal((*argv)[1], "--help") || gwy_strequal((*argv)[1], "-h")) {
        print_help();
        exit(0);
    }

    if (gwy_strequal((*argv)[1], "--version") || gwy_strequal((*argv)[1], "-v")) {
        printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
        exit(0);
    }

    for (i = j = 1; i < *argc; i++) {
        if (gwy_strequal((*argv)[i], "--"))
            ignore = TRUE;

        (*argv)[j] = (*argv)[i];

        if (!ignore) {
            if (gwy_strequal((*argv)[i], "--no-splash")) {
                gwy_app_splash_enable(FALSE);
                continue;
            }

            if (gwy_strequal((*argv)[i], "--debug-objects")) {
                enable_object_debugging = TRUE;
                continue;
            }
        }

        j++;
    }
    (*argv)[j] = NULL;
    *argc = j;
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
"     --debug-objects        Catch leaking objects (devel only).\n"
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
    puts("Please report bugs to <" PACKAGE_BUGREPORT ">.");
}

static void
warn_broken_settings_file(GtkWidget *parent,
                          const gchar *settings_file,
                          const gchar *reason)
{
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new
                 (GTK_WINDOW(parent),
                  GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_WARNING,
                  GTK_BUTTONS_OK,
                  _("Could not read settings."));
    gtk_message_dialog_format_secondary_text
        (GTK_MESSAGE_DIALOG(dialog),
         _("Settings file `%s' cannot be read: %s\n\n"
           "To prevent loss of saved settings no attempt to update it will "
           "be made until it is repaired or removed."),
         settings_file, reason);
    /* parent is usually in a screen corner, centering on it looks ugly */
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_window_present(GTK_WINDOW(dialog));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
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
    logfile = g_fopen(log_filename, "w");
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
    static GString *last = NULL;
    static guint count = 0;
    FILE *logfile = (FILE*)user_data;

    if (!logfile)
        return;

    if (!last)
        last = g_string_new("");

    if (gwy_strequal(message, last->str)) {
        count++;
        return;
    }

    if (count)
        fprintf(logfile, "Last message repeated %u times\n", count);
    g_string_assign(last, message);
    count = 0;

    fprintf(logfile, "%s%s%s\n",
            log_domain ? log_domain : "",
            log_domain ? ": " : "",
            message);
    fflush(logfile);
}
#endif  /* LOG_TO_FILE */

static void
open_command_line_files(gchar **args, gint n)
{
    gchar **p;
    gchar *cwd, *filename;

    /* FIXME: cwd is in GLib encoding. And args? */
    cwd = g_get_current_dir();
    for (p = args; n; p++, n--) {
        if (g_path_is_absolute(*p))
            filename = g_strdup(*p);
        else
            filename = g_build_filename(cwd, *p, NULL);
        if (g_file_test(filename, G_FILE_TEST_IS_DIR)) {
            gwy_app_set_current_directory(filename);
            gwy_app_file_open();
        }
        else
            gwy_app_file_load(NULL, filename, NULL);
        g_free(filename);
    }
    g_free(cwd);
}

/**
 * gwy_app_init:
 * @argc: Address of the argc parameter of main(). Passed to gwy_app_gl_init().
 * @argv: Address of the argv parameter of main(). Passed to gwy_app_gl_init().
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
static void
gwy_app_init(int *argc,
             char ***argv)
{
    gwy_widgets_type_init();
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL);
    g_set_application_name(PACKAGE_NAME);
    gwy_app_gl_init(argc, argv);
    /* XXX: These reference are never released. */
    gwy_data_window_class_set_tooltips(gwy_app_get_tooltips());
    gwy_3d_window_class_set_tooltips(gwy_app_get_tooltips());
    gwy_graph_window_class_set_tooltips(gwy_app_get_tooltips());

    gwy_app_set_window_icon();
    gwy_app_init_widget_styles();
    gwy_app_init_i18n();
}

static void
gwy_app_set_window_icon(void)
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
