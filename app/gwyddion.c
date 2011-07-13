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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyversion.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/gwygrainvalue.h>
#include <libprocess/gwycalibration.h>
#include <libgwymodule/gwymoduleloader.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>
#include "gwyappinternal.h"
#include "gwyddion.h"
#include "mac_integration.h"

#ifdef G_OS_WIN32
#define LOG_TO_FILE_DEFAULT TRUE
#include <windows.h>
#include <winreg.h>
#else
#define LOG_TO_FILE_DEFAULT FALSE
#endif

typedef struct {
    gboolean no_splash;
    gboolean debug_objects;
    gboolean startup_time;
    gboolean check;
    gboolean log_to_file;
    gboolean disable_gl;
    GwyAppRemoteType remote;
} GwyAppOptions;

static void open_command_line_files         (gint n,
                                             gchar **args);
static gint check_command_line_files        (gint n,
                                             gchar **args);
static void print_help                      (void);
static void process_preinit_options         (int *argc,
                                             char ***argv,
                                             GwyAppOptions *options);
static void debug_time                      (GTimer *timer,
                                             const gchar *task);
static void setup_logging                   (void);
static void logger                          (const gchar *log_domain,
                                             GLogLevelFlags log_level,
                                             const gchar *message,
                                             gpointer user_data);
static void setup_locale_from_win32_registry(void);
static void warn_broken_settings_file       (GtkWidget *parent,
                                             const gchar *settings_file,
                                             const gchar *reason);
static void gwy_app_init                    (int *argc,
                                             char ***argv);
static void gwy_app_set_window_icon         (void);
static void gwy_app_check_version           (void);
static void sneaking_thread_init            (void);

static GwyAppOptions app_options = {
    FALSE, FALSE, FALSE, FALSE, LOG_TO_FILE_DEFAULT, FALSE,
    GWY_APP_REMOTE_NONE,
};

int
main(int argc, char *argv[])
{
    GtkWidget *toolbox;
    gchar **module_dirs;
    gchar *settings_file, *recent_file_file, *accel_file;
    gboolean has_settings, settings_ok = FALSE;
    GError *settings_err = NULL;
    GTimer *timer;

    sneaking_thread_init();
    timer = g_timer_new();
    gwy_app_check_version();

    gwy_osx_init_handler(&argc);
    gwy_osx_set_locale();

    process_preinit_options(&argc, &argv, &app_options);
    if (app_options.log_to_file)
        setup_logging();
    gwy_debug_objects_enable(app_options.debug_objects);
    /* TODO: handle failure */
    gwy_app_settings_create_config_dir(NULL);
    debug_time(timer, "init");
    setup_locale_from_win32_registry();
    gtk_init(&argc, &argv);
    debug_time(timer, "gtk_init()");
    gwy_remote_do(app_options.remote, argc - 1, argv + 1);
    gwy_app_init(&argc, &argv);
    debug_time(timer, "gwy_app_init()");

    settings_file = gwy_app_settings_get_settings_filename();
    has_settings = g_file_test(settings_file, G_FILE_TEST_IS_REGULAR);
    gwy_debug("Text settings file is `%s'. Do we have it: %s",
              settings_file, has_settings ? "TRUE" : "FALSE");

    gwy_app_splash_start(!app_options.no_splash && !app_options.check);
    debug_time(timer, "create splash");

    accel_file = g_build_filename(gwy_get_user_dir(), "accel_map", NULL);
    gtk_accel_map_load(accel_file);
    debug_time(timer, "load accel map");

    gwy_app_splash_set_message(_("Loading document history"));
    recent_file_file = gwy_app_settings_get_recent_file_list_filename();
    gwy_app_recent_file_list_load(recent_file_file);
    debug_time(timer, "load document history");

    gwy_app_splash_set_message_prefix(_("Registering "));
    gwy_app_splash_set_message(_("stock items"));
    gwy_stock_register_stock_items();
    debug_time(timer, "register stock items");

    gwy_app_splash_set_message(_("color gradients"));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GRADIENT));
    gwy_app_splash_set_message(_("GL materials"));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GL_MATERIAL));
    gwy_app_splash_set_message(_("grain quantities"));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GRAIN_VALUE));
    gwy_app_splash_set_message(_("calibrations"));
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_CALIBRATION));
    gwy_app_splash_set_message_prefix(NULL);
    debug_time(timer, "load resources");

    gwy_app_splash_set_message(_("Loading settings"));
    if (has_settings)
        settings_ok = gwy_app_settings_load(settings_file, &settings_err);
    gwy_debug("Loading settings was: %s", settings_ok ? "OK" : "Not OK");
    gwy_app_settings_get();
    debug_time(timer, "load settings");

    gwy_app_splash_set_message(_("Registering modules"));
    module_dirs = gwy_app_settings_get_module_dirs();
    gwy_module_register_modules((const gchar**)module_dirs);
    /* The Python initialisation somehow overrides SIGINT and Gwyddion can no
     * longer be terminated with Ctrl-C.  Fix it. */
    signal(SIGINT, SIG_DFL);
    /* TODO: The Python initialisation also overrides where the warnings go.
     * Restore the handlers. */
    debug_time(timer, "register modules");

    if (app_options.check) {
        gint nfailures;

        gwy_app_splash_finish();
        debug_time(timer, "destroy splash");

        nfailures = check_command_line_files(argc - 1, argv + 1);
        debug_time(timer, "check files");

        return !nfailures;
    }

    gwy_app_splash_set_message(_("Initializing GUI"));
    toolbox = gwy_app_toolbox_create();
    debug_time(timer, "create toolbox");
    gwy_app_data_browser_restore();
    debug_time(timer, "init data-browser");
    /* A dirty trick, it constructs the recent files menu as a side effect. */
    gwy_app_recent_file_list_update(NULL, NULL, NULL, 0);
    debug_time(timer, "create recent files menu");
    gwy_app_splash_finish();
    debug_time(timer, "destroy splash");

    open_command_line_files(argc - 1, argv + 1);
    if (has_settings && !settings_ok) {
        warn_broken_settings_file(toolbox,
                                  settings_file, settings_err->message);
        g_clear_error(&settings_err);
    }
    debug_time(timer, "open commandline files");

    /* Move focus to toolbox */
    gtk_window_present(GTK_WINDOW(toolbox));
    debug_time(timer, "show toolbox");
    g_timer_destroy(timer);
    debug_time(NULL, "STARTUP");

    gwy_osx_open_files();

    gtk_main();

    gwy_osx_remove_handler();

    timer = g_timer_new();
    /* TODO: handle failure */
    if (settings_ok || !has_settings)
        gwy_app_settings_save(settings_file, NULL);
    gtk_accel_map_save(accel_file);
    debug_time(timer, "save settings");
    gwy_app_recent_file_list_save(recent_file_file);
    debug_time(timer, "save document history");
    gwy_app_process_func_save_use();
    debug_time(timer, "save funcuse");
    gwy_app_settings_free();
    /*gwy_resource_classes_finalize();*/
    gwy_debug_objects_dump_to_file(stderr, 0);
    gwy_debug_objects_clear();
    debug_time(timer, "dump debug-objects");
    gwy_app_recent_file_list_free();
    /* XXX: EXIT-CLEAN-UP */
    /* Finalize all gradients.  Useless, but makes --debug-objects happy.
     * Remove in production version. */
    g_free(recent_file_file);
    g_free(settings_file);
    g_free(accel_file);
    g_strfreev(module_dirs);
    debug_time(timer, "destroy resources");
    g_timer_destroy(timer);
    debug_time(NULL, "SHUTDOWN");

    return 0;
}

static void
process_preinit_options(int *argc,
                        char ***argv,
                        GwyAppOptions *options)
{
    int i, j;
    gboolean ignore = FALSE;

    if (*argc == 1)
        return;

    if (gwy_strequal((*argv)[1], "--help")
        || gwy_strequal((*argv)[1], "-h")) {
        print_help();
        exit(0);
    }

    if (gwy_strequal((*argv)[1], "--version")
        || gwy_strequal((*argv)[1], "-v")) {
        printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
        exit(0);
    }

    for (i = j = 1; i < *argc; i++) {
        if (gwy_strequal((*argv)[i], "--"))
            ignore = TRUE;

        (*argv)[j] = (*argv)[i];

        if (!ignore) {
            if (gwy_strequal((*argv)[i], "--no-splash")) {
                options->no_splash = TRUE;
                continue;
            }
            if (gwy_strequal((*argv)[i], "--remote-existing")) {
                options->remote = GWY_APP_REMOTE_EXISTING;
                continue;
            }
            if (gwy_strequal((*argv)[i], "--remote-new")) {
                options->remote = GWY_APP_REMOTE_NEW;
                continue;
            }
            if (gwy_strequal((*argv)[i], "--remote-query")) {
                options->remote = GWY_APP_REMOTE_QUERY;
                continue;
            }
            if (gwy_strequal((*argv)[i], "--debug-objects")) {
                options->debug_objects = TRUE;
                continue;
            }
            if (gwy_strequal((*argv)[i], "--startup-time")) {
                options->startup_time = TRUE;
                continue;
            }
            if (gwy_strequal((*argv)[i], "--log-to-file")) {
                options->log_to_file = TRUE;
                continue;
            }
            if (gwy_strequal((*argv)[i], "--no-log-to-file")) {
                options->log_to_file = FALSE;
                continue;
            }
            if (gwy_strequal((*argv)[i], "--disable-gl")) {
                options->disable_gl = TRUE;
                continue;
            }
            if (gwy_strequal((*argv)[i], "--check")) {
                options->check = TRUE;
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
"Usage: gwyddion [OPTIONS...] FILES...\n"
"An SPM data visualization and analysis tool, written with Gtk+.\n"
        );
    puts(
"Gwyddion options:\n"
" -h, --help                 Print this help and terminate.\n"
" -v, --version              Print version info and terminate.\n"
"     --no-splash            Don't show splash screen.\n"
"     --remote-query         Check if a Gwyddion instance is already running.\n"
"     --remote-new           Load FILES to a running instance or run a new one.\n"
"     --remote-existing      Load FILES to a running instance or fail.\n"
"     --check                Check FILES, print problems and terminate.\n"
"     --disable-gl           Disable OpenGL, including any availability checks.\n"
"     --log-to-file          Redirect messages file set in GWYDDION_LOGFILE.\n"
"     --no-log-to-file       Print messages to console.\n"
"     --debug-objects        Catch leaking objects (devel only).\n"
"     --startup-time         Measure time of startup tasks.\n"
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
debug_time(GTimer *timer,
           const gchar *task)
{
    static gdouble total = 0.0;

    gdouble t;

    if (!app_options.startup_time || app_options.remote)
        return;

    if (timer) {
        total += t = g_timer_elapsed(timer, NULL);
        printf("%24s: %5.1f ms\n", task, 1000.0*t);
        g_timer_start(timer);
    }
    else {
        printf("%24s: %5.1f ms\n", task, 1000.0*total);
        total = 0.0;
    }
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

/* Redirect messages from all libraries we use to a file.  This (a) creates
 * a possibly useful log if we don't crash totally (b) prevents the mesages
 * to go to a DOS console thus creating it. */
static void
setup_logging(void)
{
    const gchar *domains[] = {
        "GLib", "GLib-GObject", "GLib-GIO", "GModule", "GThread",
        "GdkPixbuf", "Gdk", "Gtk",
        "GdkGLExt", "GtkGLExt",
        "Pango",
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

static void
setup_locale_from_win32_registry(void)
{
#ifdef G_OS_WIN32
    gchar locale[64];
    DWORD size = sizeof(locale);
    HKEY reg_key;

    if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Gwyddion\\2.0"),
                     0, KEY_READ, &reg_key) == ERROR_SUCCESS) {
        if (RegQueryValueEx(reg_key, TEXT("Locale"), NULL, NULL, locale, &size) == ERROR_SUCCESS){
            g_setenv("LANG", locale, TRUE);
            RegCloseKey(reg_key);
            return;
        }
        RegCloseKey(reg_key);
    }
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\Gwyddion\\2.0"),
                     0, KEY_READ, &reg_key) == ERROR_SUCCESS) {
        if (RegQueryValueEx(reg_key, TEXT("Locale"), NULL, NULL, locale, &size) == ERROR_SUCCESS)
            g_setenv("LANG", locale, TRUE);
        RegCloseKey(reg_key);
    }
#endif
}

static void
open_command_line_files(gint n, gchar **args)
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

static gint
check_command_line_files(gint n,
                         gchar **args)
{
    gint i, nfailures;

    /* FIXME: cwd is in GLib encoding. And args? */
    for (i = nfailures = 0; i < n; i++) {
        const gchar *filename = args[i];
        const gchar *name = NULL;
        GwyContainer *data;
        GError *error = NULL;
        GSList *failures, *f;

        if (!(data = gwy_file_load(filename, GWY_RUN_NONINTERACTIVE, &error))) {
            if (!error)
                g_printerr("%s: Loader failed to report error properly!\n",
                           filename);
            else {
                g_printerr("%s: %s\n", filename, error->message);
                g_clear_error(&error);
            }
            continue;
        }

        failures = gwy_data_validate(data, GWY_DATA_VALIDATE_ALL);
        gwy_file_get_data_info(data, &name, NULL);
        g_assert(name);
        for (f = failures; f; f = g_slist_next(f)) {
            GwyDataValidationFailure *failure;

            failure = (GwyDataValidationFailure*)f->data;
            g_printerr("%s: %s, %s: %s",
                       filename, name,
                       g_quark_to_string(failure->key),
                       gwy_data_error_desrcibe(failure->error));
            if (failure->details)
                g_printerr(" (%s)", failure->details);
            g_printerr("\n");

            nfailures++;
        }
        gwy_data_validation_failure_list_free(failures);
    }

    return nfailures;
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
    if (sizeof(GWY_VERSION_STRING) > 9)
        g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL);
    g_set_application_name(PACKAGE_NAME);
    if (!gwy_app_gl_disabled())
        gwy_app_gl_init(argc, argv);
    /* XXX: These references are never released. */
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

static void
gwy_app_check_version(void)
{
    if (!gwy_strequal(GWY_VERSION_STRING, gwy_version_string())) {
        g_warning("Application and library versions do not match: %s vs. %s",
                  GWY_VERSION_STRING, gwy_version_string());
    }
}

/* This is (a) to ensure threads are initialised as the very first thing if
 * it's neceesary (b) get timers right. */
static void
sneaking_thread_init(void)
{
    GModule *main_module;
    const gchar *version_mismatch;
    GDestroyNotify thread_init_func = NULL;

    if ((version_mismatch = glib_check_version(2, 24, 0))) {
        gwy_debug("GLib is not >= 2.24.0; says %s", version_mismatch);
        return;
    }

    main_module = g_module_open(NULL, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
    if (!main_module) {
        gwy_debug("Canno dlopen() self.");
        return;
    }

    if (g_module_symbol(main_module, "g_thread_init",
                        (gpointer)&thread_init_func)) {
        thread_init_func(NULL);
        gwy_debug("Threads initialised.");
    }
    else {
        gwy_debug("Cannot find symbol g_thread_init.");
    }
    g_module_close(main_module);
}

gboolean
gwy_app_gl_disabled(void)
{
    return app_options.disable_gl;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
