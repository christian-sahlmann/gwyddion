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

/*
 * The remote control code was more or less copied from:
 *
 * The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimp-remote.c
 * Copyright (C) 2000-2004  Sven Neumann <sven@gimp.org>
 *                          Simon Budig <simon@gimp.org>
 *
 * Tells a running gimp to open files by creating a synthetic drop-event.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

/* TODO: Must chech Xmu or what the heck... */
#ifdef GDK_WINDOWING_X11
#define ENABLE_REMOTE_X11
#endif

#ifdef ENABLE_REMOTE_X11
#include <gdk/gdkx.h>
#include <X11/Xmu/WinUtil.h>
#include <X11/Xatom.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyversion.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwymodule/gwymoduleloader.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>
#include "gwyappinternal.h"
#include "gwyddion.h"

#ifdef G_OS_WIN32
#define LOG_TO_FILE 1
#else
#undef LOG_TO_FILE
#endif

typedef enum {
    GWY_APP_REMOTE_NONE = 0,
    GWY_APP_REMOTE_NEW,
    GWY_APP_REMOTE_EXISTING,
    GWY_APP_REMOTE_QUERY
} GwyAppRemoteType;

typedef struct {
    gboolean no_splash;
    gboolean objects;
    gboolean startup;
    GwyAppRemoteType remote;
} GwyAppOptions;

#ifdef LOG_TO_FILE
static void setup_logging(void);
static void logger       (const gchar *log_domain,
                          GLogLevelFlags log_level,
                          const gchar *message,
                          gpointer user_data);
#endif  /* LOG_TO_FILE */

static void       open_command_line_files  (gint n,
                                            gchar **args);
static void       print_help               (void);
static void       process_preinit_options  (int *argc,
                                            char ***argv,
                                            GwyAppOptions *options);
static void       remote                   (int argc,
                                            char **argv);
static GdkWindow* remote_find_toolbox      (GdkDisplay *display,
                                            guint32 *xid);
static void       debug_time               (GTimer *timer,
                                            const gchar *task);
static void       warn_broken_settings_file(GtkWidget *parent,
                                            const gchar *settings_file,
                                            const gchar *reason);
static void       gwy_app_init             (int *argc,
                                            char ***argv);
static void       gwy_app_set_window_icon  (void);
static void       gwy_app_check_version    (void);

static GwyAppOptions app_options = {
    FALSE, FALSE, FALSE, GWY_APP_REMOTE_NONE,
};

int
main(int argc, char *argv[])
{
    GtkWidget *toolbox;
    gchar **module_dirs;
    gchar *settings_file, *recent_file_file;
    gboolean has_settings, settings_ok = FALSE;
    GError *settings_err = NULL;
    GTimer *timer;

    timer = g_timer_new();
    gwy_app_check_version();
    process_preinit_options(&argc, &argv, &app_options);
    gwy_debug_objects_enable(app_options.objects);
    /* TODO: handle failure */
    gwy_app_settings_create_config_dir(NULL);
    /* FIXME: somewhat late, actually even gwy_find_self_set_argv0() which MUST
     * be run first can print things to console when debuggin is enabled. */
#ifdef LOG_TO_FILE
    setup_logging();
#endif  /* LOG_TO_FILE */
    debug_time(timer, "init");

    gtk_init(&argc, &argv);
    debug_time(timer, "gtk_init()");
    remote(argc - 1, argv + 1);
    gwy_app_init(&argc, &argv);
    debug_time(timer, "gwy_app_init()");

    settings_file = gwy_app_settings_get_settings_filename();
    has_settings = g_file_test(settings_file, G_FILE_TEST_IS_REGULAR);
    gwy_debug("Text settings file is `%s'. Do we have it: %s",
              settings_file, has_settings ? "TRUE" : "FALSE");

    gwy_app_splash_start(!app_options.no_splash);
    debug_time(timer, "create splash");

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
    debug_time(timer, "register modules");

    gwy_app_splash_set_message(_("Initializing GUI"));
    toolbox = gwy_app_toolbox_create();
    debug_time(timer, "create toolbox");
    gwy_app_data_browser_restore();
    debug_time(timer, "init data-browser");
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

    gtk_main();

    timer = g_timer_new();
    /* TODO: handle failure */
    if (settings_ok || !has_settings)
        gwy_app_settings_save(settings_file, NULL);
    debug_time(timer, "save settings");
    gwy_app_recent_file_list_save(recent_file_file);
    debug_time(timer, "save document history");
    gwy_app_process_func_save_use();
    debug_time(timer, "save funcuse");
    gwy_app_settings_free();
    gwy_debug_objects_dump_to_file(stderr, 0);
    gwy_debug_objects_clear();
    debug_time(timer, "dump debug-objects");
    gwy_app_recent_file_list_free();
    /* XXX: EXIT-CLEAN-UP */
    /* Finalize all gradients.  Useless, but makes --debug-objects happy.
     * Remove in production version. */
    g_object_unref(gwy_gradients());
    g_object_unref(gwy_gl_materials());
    g_free(recent_file_file);
    g_free(settings_file);
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
                options->objects = TRUE;
                continue;
            }
            if (gwy_strequal((*argv)[i], "--startup-time")) {
                options->startup = TRUE;
                continue;
            }
        }

        j++;
    }
    (*argv)[j] = NULL;
    *argc = j;
}

static gboolean
toolbox_timeout(G_GNUC_UNUSED gpointer data)
{
    g_printerr("Could not connect to the toolbox.\n");
    gtk_main_quit();

    return TRUE;
}

static void
source_selection_get(G_GNUC_UNUSED GtkWidget *widget,
                     GtkSelectionData *selection_data,
                     G_GNUC_UNUSED guint info,
                     G_GNUC_UNUSED guint time_,
                     const gchar *uri)
{
    gtk_selection_data_set(selection_data, selection_data->target,
                           8, uri, strlen(uri));
    gtk_main_quit();
}

static void
remote(int argc,
       char **argv)
{
    GdkDisplay *display;
    GdkWindow *toolbox;
    GdkDragContext *context;
    GdkDragProtocol protocol;
    GtkWidget *source;
    GdkAtom sel_type, sel_id;
    GString *file_list;
    GList *targetlist;
    guint32 xid = 0;
    gchar *cwd;
    gint i;

    if (app_options.remote == GWY_APP_REMOTE_NONE)
        return;

    /* No args, nothing to do. Silly, but consistent. */
    if (app_options.remote == GWY_APP_REMOTE_EXISTING && !argc)
        exit(EXIT_SUCCESS);

    display = gdk_display_get_default();
    toolbox = remote_find_toolbox(display, &xid);
    gwy_debug("Toolbox: %p", toolbox);

    switch (app_options.remote) {
        case GWY_APP_REMOTE_EXISTING:
        if (!toolbox) {
            g_printerr("No Gwyddion toolbox window found.\n");
            exit(EXIT_FAILURE);
        }
        break;

        case GWY_APP_REMOTE_NEW:
        /* Returning simply continues execution of Gwyddion. */
        if (!toolbox)
            return;
        if (!argc)
            exit(EXIT_SUCCESS);
        break;

        case GWY_APP_REMOTE_QUERY:
        if (toolbox) {
            printf("0x%08x\n", xid);
            exit(EXIT_SUCCESS);
        }
        exit(EXIT_FAILURE);
        break;

        default:
        g_return_if_reached();
        break;
    }

    /* Now we have the toolbox and have some files to send to it. */
    cwd = g_get_current_dir();
    file_list = g_string_new(NULL);
    for (i = 0; i < argc; i++) {
        gchar *s, *t;

        if (i)
            g_string_append_c(file_list, '\n');

        if (g_path_is_absolute(argv[i]))
            s = g_filename_to_uri(argv[i], NULL, NULL);
        else {
            t = g_build_filename(cwd, argv[i], NULL);
            s = g_filename_to_uri(t, NULL, NULL);
            g_free(t);
        }
        g_string_append(file_list, s);
        g_free(s);
    }

    xid = gdk_drag_get_protocol_for_display (display, xid, &protocol);
    if (!xid) {
        g_printerr("Gwyddion window doesn't support DnD.\n");
        exit(EXIT_FAILURE);
    }

    /* This may not be necessary in Gwyddion.  Fixes non-responsive toolbox */
    g_timeout_add(2000, toolbox_timeout, NULL);

    /* Set up an DND-source. */
    source = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(source, "selection_get",
                     G_CALLBACK(source_selection_get), file_list->str);
    gtk_widget_realize (source);

    /* Specify the id and the content-type of the selection used to
     * pass the URIs to Gwyddion toolbox. */
    sel_id = gdk_atom_intern("XdndSelection", FALSE);
    sel_type = gdk_atom_intern("text/plain", FALSE);
    targetlist = g_list_prepend(NULL, GUINT_TO_POINTER(sel_type));

    /* Assign the selection to our DnD-source. */
    gtk_selection_owner_set(source, sel_id, GDK_CURRENT_TIME);
    gtk_selection_add_target(source, sel_id, sel_type, 0);

    /* Drag_begin/motion/drop. */
    context = gdk_drag_begin(source->window, targetlist);

    gdk_drag_motion(context, toolbox, protocol, 0, 0,
                    GDK_ACTION_COPY, GDK_ACTION_COPY, GDK_CURRENT_TIME);
    gdk_drag_drop(context, GDK_CURRENT_TIME);

    /* Finally enter the mainloop to handle the events. */
    gtk_main();

    exit(EXIT_SUCCESS);
}

#ifdef ENABLE_REMOTE_X11
#define GWY_REMOTE_FIND_TOOLBOX_DEFINED 1
static GdkWindow*
remote_find_toolbox(GdkDisplay *display,
                    guint32 *xid)
{
    GdkWindow *root, *result = NULL;
    Display *xdisplay;
    Window xroot, xparent, *xchildren;
    Atom role_xatom;
    guint nchildren;
    gint i;

    root = gdk_screen_get_root_window(gdk_screen_get_default());
    xdisplay = gdk_x11_display_get_xdisplay(display);

    if (!XQueryTree(xdisplay, GDK_WINDOW_XID(root),
                    &xroot, &xparent, &xchildren, &nchildren)
        || !nchildren
        || !xchildren)
        return NULL;

    role_xatom = gdk_x11_get_xatom_by_name_for_display(display,
                                                       "WM_WINDOW_ROLE");

    for (i = nchildren-1; !result && i >= 0; i--) {
        Window xwindow;
        Atom ret_type_xatom;
        gint ret_format;
        gulong bytes_after, nitems;
        guchar *data;

        /*
         * The XmuClientWindow() function finds a window at or below the
         * specified window, that has a WM_STATE property. If such a
         * window is found, it is returned; otherwise the argument window
         * is returned.
         */
        xwindow = XmuClientWindow(xdisplay, xchildren[i]);
        if (XGetWindowProperty(xdisplay, xwindow,
                               role_xatom, 0, 32, FALSE, XA_STRING,
                               &ret_type_xatom, &ret_format,
                               &nitems, &bytes_after, &data) == Success
            && ret_type_xatom) {
            if (gwy_strequal(data, GWY_TOOLBOX_WM_ROLE)) {
                *xid = xwindow;
                result = gdk_window_foreign_new_for_display(display, xwindow);
            }

            XFree(data);
        }
    }

    XFree(xchildren);

    return result;
}
#endif

#ifndef GWY_REMOTE_FIND_TOOLBOX_DEFINED
static GdkWindow*
remote_find_toolbox(G_GNUC_UNUSED GdkDisplay *display,
                    G_GNUC_UNUSED guint32 *xid)
{
    g_warning("Remote control not implemented for this windowing system");
    return NULL;
}
#endif

static void
print_help(void)
{
    puts(
"Usage: gwyddion [OPTIONS...] FILES...\n"
"An SPM data analysis framework, written in Gtk+.\n"
        );
    puts(
"Gwyddion options:\n"
" -h, --help                 Print this help and terminate.\n"
" -v, --version              Print version info and terminate.\n"
"     --no-splash            Don't show splash screen.\n"
"     --remote-query         Check if a Gwyddion instance is already running.\n"
"     --remote-new           Load FILES to a running instance or run a new one.\n"
"     --remote-existing      Load FILES to a running instance or fail.\n"
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

    if (!app_options.startup || app_options.remote)
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
    /* g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL); */
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

static void
gwy_app_check_version(void)
{
    if (!gwy_strequal(GWY_VERSION_STRING, gwy_version_string())) {
        g_warning("Application and library versions do not match: %s vs. %s",
                  GWY_VERSION_STRING, gwy_version_string());
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
