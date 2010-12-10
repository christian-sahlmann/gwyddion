/*
 *  @(#) $Id$
 *  Copyright (C) 2008 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>

#ifdef _WIN32
#include <process.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <gtk/gtk.h>
#include <app/gwyapp.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/gwyprocess.h>
#include <libdraw/gwydraw.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>

#define PROGRAM_NAME "gwyddion-thumbnailer"

#define THUMBNAILER_ERROR thumbnailer_error_quark()

/***********************************************************************
 * XXX: Copied from app/filelist.c, should be consolidated to a single place.
 ***********************************************************************/
/* PNG (additional in TMS) */
#define KEY_DESCRIPTION "tEXt::Description"
#define KEY_SOFTWARE    "tEXt::Software"
/* TMS, required */
#define KEY_THUMB_URI   "tEXt::Thumb::URI"
#define KEY_THUMB_MTIME "tEXt::Thumb::MTime"
/* TMS, additional */
#define KEY_THUMB_FILESIZE "tEXt::Thumb::Size"
#define KEY_THUMB_MIMETYPE "tEXt::Thumb::Mimetype"
/* TMS, format specific
 * XXX: we use Image::Width, Image::Height, even tough the data are not images
 * but they are very image-like... */
#define KEY_THUMB_IMAGE_WIDTH "tEXt::Thumb::Image::Width"
#define KEY_THUMB_IMAGE_HEIGHT "tEXt::Thumb::Image::Height"
/* Gwyddion specific */
#define KEY_THUMB_GWY_REAL_SIZE "tEXt::Thumb::X-Gwyddion::RealSize"
/* Gwyddion specific, unimplemented */
#define KEY_THUMB_GWY_IMAGES "tEXt::Thumb::X-Gwyddion::Images"
#define KEY_THUMB_GWY_GRAPHS "tEXt::Thumb::X-Gwyddion::Graphs"

typedef enum {
    /* Auxiliary modes */
    THUMBNAILER_UPDATE  = -100,
    THUMBNAILER_VERSION = -3,
    THUMBNAILER_HELP    = -2,
    THUMBNAILER_UNKNOWN = -1,
    /* Thumbnailing modes */
    THUMBNAILER_GNOME2,
    THUMBNAILER_TMS,
    THUMBNAILER_CHECK,
    THUMBNAILER_KDE4,
} ThumbnailerMode;

typedef enum {
    THUMBNAIL_SIZE_NORMAL = 128,
    THUMBNAIL_SIZE_LARGE  = 256,
} ThumbnailSize;

typedef enum {
    THUMBNAILER_ERROR_NO_DATA,
    THUMBNAILER_ERROR_THUMBNAIL,
    THUMBNAILER_ERROR_CHECK,  /* Not a `real' error */
} ThumbnailerError;

typedef struct {
    ThumbnailerMode mode;
    gboolean update;
} Options;

typedef struct {
    gchar *inputfile;
    gchar *outputfile;
    gchar *uri;
    gulong mtime;
    gulong fsize;
} FileInfo;

typedef struct {
    GwyContainer *container;
    gint present;
    gint visible;
} DataFound;

static const GwyEnum thumbnail_sizes[] = {
    { "normal", THUMBNAIL_SIZE_NORMAL, },
    { "large",  THUMBNAIL_SIZE_LARGE,  },
};

const GwyEnum modes[] = {
    { "gnome2",    THUMBNAILER_GNOME2,  },
    { "tms",       THUMBNAILER_TMS,     },
    { "check",     THUMBNAILER_CHECK,   },
    { "kde4",      THUMBNAILER_KDE4,    },
    { "help",      THUMBNAILER_HELP,    },
    { "--help",    THUMBNAILER_HELP,    },
    { "-h",        THUMBNAILER_HELP,    },
    { "version",   THUMBNAILER_VERSION, },
    { "--version", THUMBNAILER_VERSION, },
    { "-v",        THUMBNAILER_VERSION, },
    { "--update",  THUMBNAILER_UPDATE,  },
};

static void
print_help(gboolean succeed)
{
    /* GNOME2 */
    puts("Usage: " PROGRAM_NAME " gnome2 MAX-SIZE INPUT-FILE OUTPUT-FILE");
    puts("Create PNG thumbnails according to GNOME 2 thumbnailer specifications, where:");
    puts("  MAX-SIZE    = %s");
    puts("  INPUT-FILE  = %i");
    puts("  OUTPUT-FILE = %o");
    putchar('\n');
    /* TMS */
    puts("Usage: " PROGRAM_NAME " tms MAX-SIZE INPUT-FILE");
    puts("Create PNG thumbnails according to Thumbnail Managing Standard, where:");
    puts("  MAX-SIZE must be `normal', `large', 128 or 256.");
    puts("  INPUT-FILE is the input file, output file name is according to TMS.");
    putchar('\n');
    /* KDE4 */
    puts("Usage: " PROGRAM_NAME " kde4 MAX-SIZE INPUT-FILE");
    puts("Create PNG thumbnails for gwythumbcreator KDE module, where:");
    puts("  MAX-SIZE is maximum image size.");
    puts("  INPUT-FILE is the input file, output is written to the standard output.");
    putchar('\n');
    /* CHECK */
    puts("Usage: " PROGRAM_NAME " check INPUT-FILE");
    puts("Report status of Thumbnail Managing Standard thumbnails for INPUT-FILE.");
    putchar('\n');
    /* Options */
    puts("Options:");
    puts("  --update                   Only write the thumbnail if it is not up to date.");
    putchar('\n');
    /* General */
    puts("Informative options:");
    puts("  -h, --help or help         Print this help and terminate.");
    puts("  -v, --version or version   Print version and terminate.");
    exit(succeed ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void
print_version(void)
{
    puts(PROGRAM_NAME " " PACKAGE_VERSION);
    exit(EXIT_SUCCESS);
}

static void
parse_mode(Options *options,
           int *argc, char ***argv)
{
    ThumbnailerMode mode;

    while (TRUE) {
        mode = gwy_string_to_enum((*argv)[1] ? (*argv)[1] : "",
                                  modes, G_N_ELEMENTS(modes));
        (*argc)--;
        (*argv)++;

        if (mode == THUMBNAILER_UPDATE) {
            options->update = TRUE;
            continue;
        }
        else if (mode == THUMBNAILER_UNKNOWN)
            print_help(FALSE);
        else if (mode == THUMBNAILER_HELP)
            print_help(TRUE);
        else if (mode == THUMBNAILER_VERSION)
            print_version();

        break;
    }

    options->mode = mode;
}

static void
die(const gchar *reason, ...)
{
    va_list ap;

    va_start(ap, reason);
    fprintf(stderr, "%s: Aborting. %s\n",
            PROGRAM_NAME, g_strdup_vprintf(reason, ap));
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void
die_gerror(GError *error, const gchar *func)
{
    if (error)
        die("%s failed with: %s", func, error->message);
    else
        die("%s failed but did not tell why.", func);
}

static void
die_errno(const gchar *func)
{
    die("%s failed with: %s", func, g_strerror(errno));
}

static GQuark
thumbnailer_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("thumbnailer-error-quark");

    return error_domain;
}

static void
load_modules(void)
{
    static const gchar *const module_types[] = { "file", "layer", NULL };
    GPtrArray *module_dirs;
    const gchar *q;
    gchar *p;
    guint i;

    module_dirs = g_ptr_array_new();

    p = gwy_find_self_dir("modules");
    for (i = 0; module_types[i]; i++) {
        g_ptr_array_add(module_dirs,
                        g_build_filename(p, module_types[i], NULL));
    }
    g_free(p);

    q = gwy_get_user_dir();
    for (i = 0; module_types[i]; i++) {
        g_ptr_array_add(module_dirs,
                        g_build_filename(q, module_types[i], NULL));
    }

    g_ptr_array_add(module_dirs, NULL);
    gwy_module_register_modules((const gchar**)module_dirs->pdata);

    for (i = 0; module_dirs->pdata[i]; i++)
        g_free(module_dirs->pdata[i]);
    g_ptr_array_free(module_dirs, TRUE);
}

/* Be defensive.  On the other hand we do not perform global file validation,
 * if we find something to make thumbnail of, we are happy and the rest can
 * be complete rubbish. */
static void
find_some_data(gpointer pquark, gpointer pvalue, gpointer user_data)
{
    const gchar *key = g_quark_to_string(GPOINTER_TO_UINT(pquark));
    GValue *value = (GValue*)pvalue;
    DataFound *data_found = (DataFound*)user_data;
    gchar *s;
    gboolean visible = FALSE;
    int id;

    if (!G_VALUE_HOLDS_OBJECT(value)
        || !g_type_is_a(G_TYPE_FROM_INSTANCE(g_value_get_object(value)),
                        GWY_TYPE_DATA_FIELD))
        return;

    if (!key || key[0] != '/')
        return;

    id = strtol(key + 1, &s, 10);
    if (s == key + 1 || id < 0 || !gwy_strequal(s, "/data"))
        return;

    if (data_found->present == -1 || id < data_found->present)
        data_found->present = id;

    s = g_strconcat(key, "/visible", NULL);
    gwy_container_gis_boolean_by_name(data_found->container, s, &visible);
    g_free(s);

    if (!visible)
        return;

    if (data_found->visible == -1 || id < data_found->visible)
        data_found->visible = id;
}

static gboolean
check_thumbnail(FileInfo *fileinfo,
                GError **error)
{
    GdkPixbuf *pixbuf;
    struct stat st;
    const gchar *value;
    gboolean ok = FALSE;

    if (stat(fileinfo->inputfile, &st) != 0)
        die_errno("stat");

    fileinfo->mtime = st.st_mtime;
    fileinfo->fsize = st.st_size;

    if (!fileinfo->outputfile) {
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_CHECK,
                    "No thumbnail file name is set.");
        return FALSE;
    }

    if (stat(fileinfo->outputfile, &st) != 0) {
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_CHECK,
                    "Thumbnail does not exist or stat() fails on it.");
        return FALSE;
    }

    /* If the file is newer than the thumbnail, do not bother loading the
     * pixbuf. */
    if (fileinfo->mtime > (gulong)st.st_mtime) {
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_CHECK,
                    "File mtime %lu is newer than thumbnail %lu.",
                    fileinfo->mtime, (gulong)st.st_mtime);
        return FALSE;
    }

    pixbuf = gdk_pixbuf_new_from_file(fileinfo->outputfile, NULL);
    if (!pixbuf) {
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_CHECK,
                    "Cannot read thumbnail into a GdkPixbuf.");
        return FALSE;
    }

    if (!(value = gdk_pixbuf_get_option(pixbuf, KEY_THUMB_URI))) {
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_CHECK,
                    "Thumbnail does not have URI chunk %s.", KEY_THUMB_URI);
        goto finalize;
    }
    if (!gwy_strequal(value, fileinfo->uri)) {
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_CHECK,
                    "Thumbnail URI %s from chunk %s does not match "
                    "input file URI %s.",
                    value, KEY_THUMB_URI, fileinfo->uri);
        goto finalize;
    }

    if (!(value = gdk_pixbuf_get_option(pixbuf, KEY_THUMB_MTIME))) {
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_CHECK,
                    "Thumbnail does not have time chunk %s.", KEY_THUMB_MTIME);
        goto finalize;
    }
    if ((gulong)atol(value) != fileinfo->mtime) {
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_CHECK,
                    "Thumbnail mtime %lu from chunk %s does not match "
                    "file modification time %lu.",
                   (gulong)atol(value), KEY_THUMB_MTIME, fileinfo->mtime);
        goto finalize;
    }

    if (!(value = gdk_pixbuf_get_option(pixbuf, KEY_THUMB_FILESIZE))) {
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_CHECK,
                    "Thumbnail does not have size chunk %s.",
                    KEY_THUMB_FILESIZE);
        goto finalize;
    }
    if ((gulong)atol(value) != fileinfo->fsize) {
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_CHECK,
                    "Thumbnail fsize %lu from chunk %s does not match "
                    "file modification time %lu.",
                   (gulong)atol(value), KEY_THUMB_MTIME, fileinfo->fsize);
        goto finalize;
    }

    ok = TRUE;

finalize:
    g_object_unref(pixbuf);
    return ok;
}

static gboolean
write_thumbnail(const FileInfo *fileinfo,
                gint maxsize,
                GError **error)
{
    GwyContainer *container;
    GwyDataField *dfield;
    DataFound data_found = { NULL, -1, -1 };
    GdkPixbuf *pixbuf;
    GwySIUnit *siunit;
    GwySIValueFormat *vf;
    gdouble xreal, yreal;
    gchar *str_mtime, *str_fsize, *str_width, *str_height, *str_real_size;
    gboolean ok;
    gint id;

    if (!(container = gwy_file_load(fileinfo->inputfile,
                                    GWY_RUN_NONINTERACTIVE, error)))
        return FALSE;

    data_found.container = container;
    gwy_container_foreach(container, NULL, find_some_data, &data_found);
    id = data_found.visible > -1 ? data_found.visible : data_found.present;
    if (id < 0) {
        g_object_unref(container);
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_NO_DATA,
                    "File contains no previewable data.");
        return FALSE;
    }

    pixbuf = gwy_app_get_channel_thumbnail(container, id, maxsize, maxsize);
    if (!pixbuf) {
        g_object_unref(container);
        g_set_error(error, THUMBNAILER_ERROR, THUMBNAILER_ERROR_THUMBNAIL,
                    "Cannot preview channel %d.", id);
        return FALSE;
    }

    dfield = gwy_container_get_object(container,
                                      gwy_app_get_data_key_for_id(id));
    siunit = gwy_data_field_get_si_unit_xy(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    vf = gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                sqrt(xreal*yreal), NULL);
    str_real_size = g_strdup_printf("%.*f×%.*f%s%s",
                                    vf->precision, xreal/vf->magnitude,
                                    vf->precision, yreal/vf->magnitude,
                                    (vf->units && *vf->units) ? " " : "",
                                    vf->units);
    gwy_si_unit_value_format_free(vf);
    str_mtime = g_strdup_printf("%lu", (gulong)fileinfo->mtime);
    str_fsize = g_strdup_printf("%lu", (gulong)fileinfo->fsize);
    str_width = g_strdup_printf("%d", gwy_data_field_get_xres(dfield));
    str_height = g_strdup_printf("%d", gwy_data_field_get_yres(dfield));
    g_object_unref(container);

    if (fileinfo->outputfile) {
        ok = gdk_pixbuf_save(pixbuf, fileinfo->outputfile,
                             "png", error,
                             "compression", "9",
                             KEY_SOFTWARE, PACKAGE_NAME,
                             KEY_THUMB_URI, fileinfo->uri,
                             KEY_THUMB_MTIME, str_mtime,
                             KEY_THUMB_FILESIZE, str_fsize,
                             KEY_THUMB_IMAGE_WIDTH, str_width,
                             KEY_THUMB_IMAGE_HEIGHT, str_height,
                             KEY_THUMB_GWY_REAL_SIZE, str_real_size,
                             NULL);
    }
    else {
        gchar *buffer;
        gsize buffer_size;

        ok = gdk_pixbuf_save_to_buffer(pixbuf, &buffer, &buffer_size,
                                       "png", error,
                                       "compression", "9",
                                       KEY_SOFTWARE, PACKAGE_NAME,
                                       KEY_THUMB_URI, fileinfo->uri,
                                       KEY_THUMB_MTIME, str_mtime,
                                       KEY_THUMB_FILESIZE, str_fsize,
                                       KEY_THUMB_IMAGE_WIDTH, str_width,
                                       KEY_THUMB_IMAGE_HEIGHT, str_height,
                                       KEY_THUMB_GWY_REAL_SIZE, str_real_size,
                                       NULL);

        if (ok)
            fwrite(buffer, 1, buffer_size, stdout);

        g_free(buffer);
    }

    g_free(str_real_size);
    g_free(str_mtime);
    g_free(str_fsize);
    g_free(str_width);
    g_free(str_height);
    g_object_unref(pixbuf);

    return ok;
}

/***********************************************************************
 * XXX: Copied from app/filelist.c
 ***********************************************************************/
static const gchar*
gwy_recent_file_thumbnail_dir(void)
{
    const gchar *thumbdir =
#ifdef G_OS_WIN32
        "thumbnails";
#else
        ".thumbnails";
#endif
    static gchar *thumbnail_dir = NULL;

    if (thumbnail_dir)
        return thumbnail_dir;

    thumbnail_dir = g_build_filename(gwy_get_home_dir(), thumbdir, NULL);
    return thumbnail_dir;
}

static gchar*
gwy_recent_file_thumbnail_name(const gchar *uri,
                               const gchar *sizename,
                               ThumbnailSize size)
{
    static const gchar *hex2digit = "0123456789abcdef";
    guchar md5sum[16];
    gchar buffer[37], *p;
    gsize i;

    gwy_md5_get_digest(uri, -1, md5sum);
    p = buffer;
    for (i = 0; i < 16; i++) {
        *p++ = hex2digit[(guint)md5sum[i] >> 4];
        *p++ = hex2digit[(guint)md5sum[i] & 0x0f];
    }
    *p++ = '.';
    *p++ = 'p';
    *p++ = 'n';
    *p++ = 'g';
    *p = '\0';

    if (!sizename) {
        sizename = gwy_enum_to_string(size, thumbnail_sizes,
                                      G_N_ELEMENTS(thumbnail_sizes));
        g_assert(sizename && *sizename);
    }

    return g_build_filename(gwy_recent_file_thumbnail_dir(), sizename, buffer,
                            NULL);
}

static gboolean
examine_thumbnails(FileInfo *fileinfo)
{
    GError *err = NULL;
    GDir *gdir;
    gboolean okn, okl;
    const gchar *subdir;
    gchar *s, *t, *p;

    g_print("File:   %s\n", fileinfo->inputfile);
    g_print("URI:    %s\n", fileinfo->uri);

    fileinfo->outputfile
        = gwy_recent_file_thumbnail_name(fileinfo->uri,
                                         NULL, THUMBNAIL_SIZE_NORMAL);
    okn = check_thumbnail(fileinfo, &err);
    g_print("Normal: %s\n", fileinfo->outputfile);
    g_print("        status: %s\n", okn ? "OK" : err->message);
    if (!okn)
        g_clear_error(&err);

    fileinfo->outputfile
        = gwy_recent_file_thumbnail_name(fileinfo->uri,
                                         NULL, THUMBNAIL_SIZE_LARGE);
    okl = check_thumbnail(fileinfo, &err);
    g_print("Large:  %s\n", fileinfo->outputfile);
    g_print("        status: %s\n", okl ? "OK" : err->message);
    if (!okl)
        g_clear_error(&err);

    p = g_build_filename(gwy_recent_file_thumbnail_dir(), "fail", NULL);
    if ((gdir = g_dir_open(p, 0, NULL))) {
        while ((subdir = g_dir_read_name(gdir))) {
            t = g_strconcat("fail", G_DIR_SEPARATOR_S, subdir, NULL);
            s = gwy_recent_file_thumbnail_name(fileinfo->uri, t, 0);
            if (g_file_test(s, G_FILE_TEST_EXISTS))
                g_print("Failed: %s\n", s);
            g_free(s);
            g_free(t);
        }
        g_dir_close(gdir);
    }
    g_free(p);

    /* Succeed if at least one good thumbnail exists. */
    return okn || okl;
}

static void
check_mode_nargs(ThumbnailerMode mode, int argc, int expected_nargs)
{
    if (argc != expected_nargs)
        die("Wrong number of arguments for mode %s (expected %d).\n",
            gwy_enum_to_string(mode, modes, G_N_ELEMENTS(modes)),
            expected_nargs);
}

int
main(int argc,
     char *argv[])
{
    Options options = {
        THUMBNAILER_UNKNOWN,
        FALSE,
    };
    FileInfo fileinfo = { NULL, NULL, NULL, 0, 0 };
    gint maxsize;
    gchar *canonpath;
    GError *err = NULL;

    /* Parse arguments. */
    gtk_parse_args(&argc, &argv);
    parse_mode(&options, &argc, &argv);

    if (options.mode == THUMBNAILER_GNOME2) {
        check_mode_nargs(options.mode, argc, 4);
        maxsize = strtol(argv[1], NULL, 0);
        fileinfo.inputfile = argv[2];
        fileinfo.outputfile = argv[3];
    }
    else if (options.mode == THUMBNAILER_TMS) {
        check_mode_nargs(options.mode, argc, 3);
        if (!(maxsize = strtol(argv[1], NULL, 0))
            && ((maxsize = gwy_string_to_enum(argv[1], thumbnail_sizes,
                                              G_N_ELEMENTS(thumbnail_sizes)))
                < 0))
            die("Maximum size must be normal, large, 128 or 256.");
        fileinfo.inputfile = argv[2];
    }
    else if (options.mode == THUMBNAILER_CHECK) {
        check_mode_nargs(options.mode, argc, 2);
        fileinfo.inputfile = argv[1];
        maxsize = 0;
    }
    else if (options.mode == THUMBNAILER_KDE4) {
        check_mode_nargs(options.mode, argc, 3);
        maxsize = strtol(argv[1], NULL, 0);
        fileinfo.inputfile = argv[2];
    }
    else
        g_assert_not_reached();

    canonpath = gwy_canonicalize_path(fileinfo.inputfile);
    if (!(fileinfo.uri = g_filename_to_uri(canonpath, NULL, &err)))
        die_gerror(err, "g_filename_to_uri");
    g_free(canonpath);

    if (options.mode == THUMBNAILER_CHECK)
        return !examine_thumbnails(&fileinfo);

    if (options.mode == THUMBNAILER_TMS)
        fileinfo.outputfile = gwy_recent_file_thumbnail_name(fileinfo.uri,
                                                             NULL, maxsize);

    /* Perform a sanity check before we load modules. */
    if (maxsize < 2)
        die("Invalid maximum size %d.\n", maxsize);

    /* check_thumbnail() fills mtime and fsize, must go first! */
    if (check_thumbnail(&fileinfo, NULL) && options.update)
        return 0;

    /* Initialize Gwyddion */
    gwy_widgets_type_init();
    gwy_app_settings_load(gwy_app_settings_get_settings_filename(), NULL);
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GRADIENT));
    load_modules();

    /* Go... */
    if (!write_thumbnail(&fileinfo, maxsize, &err))
        die_gerror(err, "write_thumbnail");

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
