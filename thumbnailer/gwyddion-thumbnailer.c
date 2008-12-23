/*
 *  @(#) $Id: gwyddion.c 8566 2007-09-23 09:46:47Z yeti-dn $
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <process.h>
#endif

#if (defined(HAVE_SYS_STAT_H) || defined(_WIN32))
#include <sys/stat.h>
/* And now we are in a deep s... */
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <gtk/gtk.h>
#include <app/gwyapp.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/gwyprocess.h>
#include <libdraw/gwydraw.h>
#include <libgwymodule/gwymodule.h>

#define PROGRAM_NAME "gwyddion-thumbnailer"

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
    THUMBNAILER_VERSION = -3,
    THUMBNAILER_HELP    = -2,
    THUMBNAILER_UNKNOWN = -1,
    /* Thumbnailing modes */
    THUMBNAILER_GNOME2,
    THUMBNAILER_TMS,
} ThumbnailerMode;

typedef struct {
    GwyContainer *container;
    gint present;
    gint visible;
} DataFound;

static void
print_help(gboolean succeed)
{
    puts("Usage: " PROGRAM_NAME " gnome2 MAX-SIZE INPUT-FILE OUTPUT-FILE");
    puts("Create thumbnails according to GNOME 2 thumbnailer specifications, where:");
    puts("  MAX-SIZE    = %s");
    puts("  INPUT-FILE  = %i");
    puts("  OUTPUT-FILE = %o");
    putchar('\n');
    puts("Usage: " PROGRAM_NAME " tms MAX-SIZE INPUT-FILE");
    puts("Create thumbnails according to Thumbnail Managing Standard, where:");
    puts("  MAX-SIZE must be `normal' or 128 (other values are not implemented)");
    puts("  INPUT-FILE is the input file, output file name is according to TMS.");
    putchar('\n');
    /* Add more usages if more possibilites come... */
    puts("Informative options:");
    puts("  -h, --help or help          Print this help and terminate.");
    puts("  -v, --version or version    Print version and terminate.");
    exit(succeed ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void
print_version(void)
{
    puts(PROGRAM_NAME " " PACKAGE_VERSION);
    exit(EXIT_SUCCESS);
}

static ThumbnailerMode
parse_mode(const gchar *strmode)
{
    const GwyEnum modes[] = {
        { "gnome2",    THUMBNAILER_GNOME2,  },
        { "tms",       THUMBNAILER_TMS,     },
        { "help",      THUMBNAILER_HELP,    },
        { "--help",    THUMBNAILER_HELP,    },
        { "-h",        THUMBNAILER_HELP,    },
        { "version",   THUMBNAILER_VERSION, },
        { "--version", THUMBNAILER_VERSION, },
        { "-v",        THUMBNAILER_VERSION, },
    };
    ThumbnailerMode mode;

    mode = gwy_string_to_enum(strmode ? strmode : "",
                              modes, G_N_ELEMENTS(modes));

    if (mode == THUMBNAILER_UNKNOWN)
        print_help(FALSE);
    if (mode == THUMBNAILER_HELP)
        print_help(TRUE);
    if (mode == THUMBNAILER_VERSION)
        print_version();

    return mode;
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

static void
write_thumbnail(const gchar *inputfile,
                const gchar *outputfile,
                const gchar *uri,
                gint maxsize)
{
    GwyContainer *container;
    GwyDataField *dfield;
    DataFound data_found = { NULL, -1, -1 };
    GError *err = NULL;
    GdkPixbuf *pixbuf;
    struct stat st;
    GwySIUnit *siunit;
    GwySIValueFormat *vf;
    gdouble xreal, yreal;
    gchar *str_mtime, *str_size, *str_width, *str_height, *str_real_size;
    gint id;

    if (!(container = gwy_file_load(inputfile, GWY_RUN_NONINTERACTIVE, &err)))
        die_gerror(err, "gwy_file_load");

    data_found.container = container;
    gwy_container_foreach(container, NULL, find_some_data, &data_found);
    id = data_found.visible > -1 ? data_found.visible : data_found.present;
    if (id < 0)
        die("File contains no data field.");

    pixbuf = gwy_app_get_channel_thumbnail(container, id, maxsize, maxsize);
    if (!pixbuf)
        die("Cannot create pixbuf.");

    if (stat(inputfile, &st) != 0)
        die_errno("stat");

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
    str_mtime = g_strdup_printf("%lu", (gulong)st.st_mtime);
    str_size = g_strdup_printf("%lu", (gulong)st.st_size);
    str_width = g_strdup_printf("%d", gwy_data_field_get_xres(dfield));
    str_height = g_strdup_printf("%d", gwy_data_field_get_yres(dfield));

    if (!gdk_pixbuf_save(pixbuf, outputfile, "png", &err,
                         "compression", "9",
                         KEY_SOFTWARE, PACKAGE_NAME,
                         KEY_THUMB_URI, uri,
                         KEY_THUMB_MTIME, str_mtime,
                         KEY_THUMB_FILESIZE, str_size,
                         KEY_THUMB_IMAGE_WIDTH, str_width,
                         KEY_THUMB_IMAGE_HEIGHT, str_height,
                         KEY_THUMB_GWY_REAL_SIZE, str_real_size,
                         NULL))
        die_gerror(err, "gdk_pixbuf_save");
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
gwy_recent_file_thumbnail_name(const gchar *uri)
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

    return g_build_filename(gwy_recent_file_thumbnail_dir(), "normal", buffer,
                            NULL);
}

int
main(int argc,
     char *argv[])
{
    ThumbnailerMode mode;
    const gchar *inputfile, *outputfile;
    char *uri;
    gint maxsize = 0;

    /* Parse arguments. */
    gtk_parse_args(&argc, &argv);
    mode = parse_mode(argv[1]);

    if (mode == THUMBNAILER_GNOME2) {
        if (argc != 5)
            die("Wrong number of arguments for mode %s.\n", argv[1]);
        maxsize = strtol(argv[2], NULL, 0);
        inputfile = argv[3];
        outputfile = argv[4];
    }
    if (mode == THUMBNAILER_TMS) {
        if (argc != 4)
            die("Wrong number of arguments for mode %s.\n", argv[1]);
        if (gwy_strequal(argv[2], "normal") || strtol(argv[2], NULL, 0) == 128)
            maxsize = 128;
        else
            die("Maximum size must be normal or 128.");
        inputfile = argv[3];
        outputfile = NULL;
    }
    else
        g_assert_not_reached();

    /* FIXME: Handle failure to produce the URI better. */
    uri = g_filename_to_uri(gwy_canonicalize_path(inputfile), NULL, NULL);
    if (!uri)
        uri = g_strdup("");

    if (mode == THUMBNAILER_TMS)
        outputfile = gwy_recent_file_thumbnail_name(uri);

    /* Perform a sanity check before we load modules. */
    if (maxsize < 2)
        die("Invalid maximum size %d.\n", maxsize);

    /* Initialize Gwyddion */
    gwy_draw_type_init();
    gwy_app_settings_load(gwy_app_settings_get_settings_filename(), NULL);
    gwy_resource_class_load(g_type_class_peek(GWY_TYPE_GRADIENT));
    load_modules();

    /* Go... */
    write_thumbnail(inputfile, outputfile, uri, maxsize);

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
