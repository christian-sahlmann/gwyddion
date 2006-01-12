/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "err.h"
#include "get.h"

#define MAGIC "[DataSet]\r\n"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define DATA_MAGIC "#!"
#define DATA_MAGIC_SIZE (sizeof(DATA_MAGIC) - 1)

#define EXTENSION1 ".ezd"
#define EXTENSION2 ".nid"

typedef enum {
    SCAN_UNKNOWN = 0,
    SCAN_FORWARD = 1,
    SCAN_BACKWARD = -1
} ScanDirection;

typedef struct {
    gchar *name;
    gchar *unit;
    gdouble min;
    gdouble range;
} EZDRange;

typedef struct {
    gchar *name;
    GHashTable *meta;
    /* following fields are meaningful only for data */
    ScanDirection direction;
    gint group;
    gint channel;
    gint xres;
    gint yres;
    EZDRange xrange;
    EZDRange yrange;
    EZDRange zrange;
    guint bitdepth;
    guint byteorder;
    gboolean sign;
    const gchar *data;
} EZDSection;

typedef struct {
    GPtrArray *file;
    GwyContainer *data;
    GtkWidget *data_view;
} EZDControls;

typedef struct {
    GString *str;
    GwyContainer *container;
} StoreMetaData;

static gboolean      module_register       (const gchar *name);
static gint          ezdfile_detect        (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* ezdfile_load          (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static guint         find_data_start       (const guchar *buffer,
                                            gsize size);
static void          ezdfile_free          (GPtrArray *ezdfile);
static void          read_data_field       (GwyDataField *dfield,
                                            EZDSection *section);
static gboolean      file_read_header      (GPtrArray *ezdfile,
                                            gchar *buffer,
                                            GError **error);
static guint         find_data_offsets     (const gchar *buffer,
                                            gsize size,
                                            GPtrArray *ezdfile,
                                            GError **error);
static void          process_metadata      (GPtrArray *ezdfile,
                                            GwyContainer *container);
static void          fix_scales            (EZDSection *section,
                                            GwyContainer *container);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanosurf EZD and NID data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo ezdfile_func_info = {
        "ezdfile",
        N_("Nanosurf files (.ezd, .nid)"),
        (GwyFileDetectFunc)&ezdfile_detect,
        (GwyFileLoadFunc)&ezdfile_load,
        NULL,
        NULL
    };

    gwy_file_func_register(name, &ezdfile_func_info);

    return TRUE;
}

static gint
ezdfile_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return (g_str_has_suffix(fileinfo->name_lowercase, EXTENSION1)
                || g_str_has_suffix(fileinfo->name_lowercase, EXTENSION2))
                ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && !memcmp(fileinfo->buffer, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static GwyContainer*
ezdfile_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    EZDSection *section = NULL;
    GwyDataField *dfield = NULL;
    GPtrArray *ezdfile;
    guint header_size, n;
    gint i;
    gchar *p;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (strncmp(buffer, MAGIC, MAGIC_SIZE)
        || !(header_size = find_data_start(buffer, size))) {
        err_FILE_TYPE(error, "EZD/NID");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    ezdfile = g_ptr_array_new();
    p = g_strndup(buffer, header_size - DATA_MAGIC_SIZE);
    if (!file_read_header(ezdfile, p, error)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        g_free(p);
        return NULL;
    }
    g_free(p);

    n = find_data_offsets(buffer + header_size, size - header_size, ezdfile,
                          error);
    if (!n) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    container = gwy_container_new();
    i = 0;
    for (n = 0; n < ezdfile->len; n++) {
        gchar key[24];

        section = (EZDSection*)g_ptr_array_index(ezdfile, n);
        if (!section->data)
            continue;

        dfield = gwy_data_field_new(section->xres, section->yres,
                                    1.0, 1.0, FALSE);
        read_data_field(dfield, section);
        g_snprintf(key, sizeof(key), "/%d/data", i);
        gwy_container_set_object_by_name(container, key, dfield);
        g_object_unref(dfield);
        fix_scales(section, container);
        /* FIXME: not yet:
         * process_metadata(ezdfile, container); */
        i++;
    }
    gwy_file_abandon_contents(buffer, size, NULL);
    ezdfile_free(ezdfile);

    return container;
}

static guint
find_data_start(const guchar *buffer,
                gsize size)
{
    const guchar *p;

    size -= DATA_MAGIC_SIZE;

    for (p = buffer;
         p && strncmp(p, DATA_MAGIC, DATA_MAGIC_SIZE);
         p = memchr(p+1, (DATA_MAGIC)[0], size - (p - buffer) - 1))
        ;

    return p ? (p - buffer) + DATA_MAGIC_SIZE : 0;
}

static void
ezdfile_free(GPtrArray *ezdfile)
{
    EZDSection *section;
    guint i;

    for (i = 0; i < ezdfile->len; i++) {
        section = (EZDSection*)g_ptr_array_index(ezdfile, i);
        g_hash_table_destroy(section->meta);
        g_free(section->name);
        g_free(section->xrange.unit);
        g_free(section->yrange.unit);
        g_free(section->zrange.unit);
        g_free(section->xrange.name);
        g_free(section->yrange.name);
        g_free(section->zrange.name);
        g_free(section);
    }
    g_ptr_array_free(ezdfile, TRUE);
}

static gboolean
file_read_header(GPtrArray *ezdfile,
                 gchar *buffer,
                 GError **error)
{
    EZDSection *section = NULL;
    gchar *p, *line;
    guint len;

    while ((line = gwy_str_next_line(&buffer))) {
        line = g_strstrip(line);
        if (!(len = strlen(line)))
            continue;
        if (line[0] == '[' && line[len-1] == ']') {
            section = g_new0(EZDSection, 1);
            g_ptr_array_add(ezdfile, section);
            line[len-1] = '\0';
            section->name = g_strdup(line + 1);
            section->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free, g_free);
            gwy_debug("Section <%s>", section->name);
            continue;
        }
        if (!section) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Garbage before first header section."));
            return FALSE;
        }
        /* Skip comments */
        if (g_str_has_prefix(line, "--"))
            continue;

        p = strchr(line, '=');
        if (!p) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Malformed header line (missing =)."));
            return FALSE;
        }
        *p = '\0';
        p++;

        if (gwy_strequal(line, "SaveMode")) {
            if (strcmp(p, "Binary"))
                g_warning("SaveMode is not Binary, this is not supported");
        }
        else if (gwy_strequal(line, "SaveBits"))
            section->bitdepth = atol(p);
        else if (gwy_strequal(line, "SaveSign")) {
            section->sign = gwy_strequal(p, "Signed");
            if (!section->sign)
                g_warning("SaveSign is not Signed, this is not supported");
        }
        else if (gwy_strequal(line, "SaveOrder")) {
            if (gwy_strequal(p, "Intel"))
                section->byteorder = G_LITTLE_ENDIAN;
            else
                g_warning("SaveOrder is not Intel, this is not supported");
        }
        else if (gwy_strequal(line, "Frame")) {
            if (gwy_strequal(p, "Scan forward"))
                section->direction = SCAN_FORWARD;
            else if (gwy_strequal(p, "Scan backward"))
                section->direction = SCAN_BACKWARD;
        }
        else if (gwy_strequal(line, "Points"))
            section->xres = atol(p);
        else if (gwy_strequal(line, "Lines"))
            section->yres = atol(p);
        /* FIXME: this is ugly, and incorrect for non-2D data */
        else if (gwy_strequal(line, "Dim0Name"))
            section->xrange.name = g_strdup(p);
        else if (gwy_strequal(line, "Dim1Name"))
            section->yrange.name = g_strdup(p);
        else if (gwy_strequal(line, "Dim2Name"))
            section->zrange.name = g_strdup(p);
        else if (gwy_strequal(line, "Dim0Unit"))
            section->xrange.unit = g_strdup(p);
        else if (gwy_strequal(line, "Dim1Unit"))
            section->yrange.unit = g_strdup(p);
        else if (gwy_strequal(line, "Dim2Unit"))
            section->zrange.unit = g_strdup(p);
        else if (gwy_strequal(line, "Dim0Min"))
            section->xrange.min = g_ascii_strtod(p, NULL);
        else if (gwy_strequal(line, "Dim1Min"))
            section->yrange.min = g_ascii_strtod(p, NULL);
        else if (gwy_strequal(line, "Dim2Min"))
            section->zrange.min = g_ascii_strtod(p, NULL);
        else if (gwy_strequal(line, "Dim0Range"))
            section->xrange.range = g_ascii_strtod(p, NULL);
        else if (gwy_strequal(line, "Dim1Range"))
            section->yrange.range = g_ascii_strtod(p, NULL);
        else if (gwy_strequal(line, "Dim2Range"))
            section->zrange.range = g_ascii_strtod(p, NULL);
        else
            g_hash_table_replace(section->meta, g_strdup(line), g_strdup(p));
    }

    return TRUE;
}

static guint
find_data_offsets(const gchar *buffer,
                  gsize size,
                  GPtrArray *ezdfile,
                  GError **error)
{
    EZDSection *dataset, *section;
    GString *grkey;
    guint required_size = 0;
    gint ngroups, nchannels, i, j, k;
    guint ndata = 0;
    gchar *p;

    /* Sanity check */
    if (!ezdfile->len) {
        err_NO_DATA(error);
        return 0;
    }
    dataset = (EZDSection*)g_ptr_array_index(ezdfile, 0);
    if (strcmp(dataset->name, "DataSet")) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("First section isn't DataSet"));
        return 0;
    }

    if (!(p = g_hash_table_lookup(dataset->meta, "GroupCount"))
        || (ngroups = atol(p)) <= 0) {
        err_INVALID(error, _("GroupCount in [DataSet]"));
        return 0;
    }

    /* Scan groups */
    grkey = g_string_new("");
    for (i = 0; i < ngroups; i++) {
        g_string_printf(grkey, "Gr%d-Count", i);
        if (!(p = g_hash_table_lookup(dataset->meta, grkey->str))) {
            g_warning("No count for group %u", i);
            continue;
        }

        if ((nchannels = atol(p)) <= 0)
            continue;

        /* Scan channels inside a group, note it's OK there's less channels
         * than specified */
        for (j = 0; j < nchannels; j++) {
            g_string_printf(grkey, "Gr%d-Ch%d", i, j);
            if (!(p = g_hash_table_lookup(dataset->meta, grkey->str)))
                continue;

            section = NULL;
            for (k = 1; k < ezdfile->len; k++) {
                section = (EZDSection*)g_ptr_array_index(ezdfile, k);
                if (gwy_strequal(section->name, p))
                    break;
            }
            if (!section) {
                g_warning("Cannot find section for %s", p);
                continue;
            }

            /* Compute data position */
            gwy_debug("Data %s at offset %u from data start",
                      grkey->str, required_size);
            gwy_debug("xres = %d, yres = %d, bpp = %d, z-name = %s",
                      section->xres, section->yres, section->bitdepth,
                      section->zrange.name);
            if (section->yres < 2) {
                gwy_debug("Skipping 1D data Gr%d-Ch%d. FIXME.", i, j);
                continue;
            }
            ndata++;
            section->data = buffer + required_size;
            required_size += section->xres * section->yres
                             * (section->bitdepth/8);
            if (required_size > size) {
                g_warning("Truncated file, %s doesn't fit", grkey->str);
                g_string_free(grkey, TRUE);
                section->data = NULL;

                return 0;
            }
            section->group = i;
            section->channel = j;
        }
    }
    g_string_free(grkey, TRUE);

    if (!ndata)
        err_NO_DATA(error);

    return ndata;
}

static void
fix_scales(EZDSection *section,
           GwyContainer *container)
{
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gint power10;
    gdouble r;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(container,
                                                             "/0/data"));

    /* Fix value scale */
    siunit = gwy_si_unit_new_parse(section->zrange.unit, &power10);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);
    r = pow10(power10);
    gwy_data_field_multiply(dfield, r*section->zrange.range);
    gwy_data_field_add(dfield, r*section->zrange.min);

    /* Fix lateral scale */
    siunit = gwy_si_unit_new_parse(section->xrange.unit, &power10);
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);
    gwy_data_field_set_xreal(dfield, pow10(power10)*section->xrange.range);

    siunit = gwy_si_unit_new_parse(section->yrange.unit, &power10);
    gwy_data_field_set_yreal(dfield, pow10(power10)*section->yrange.range);
    g_object_unref(siunit);

    /* Some metadata */
    if (section->zrange.name) {
        const gchar *s;

        gwy_container_set_string_by_name(container, "/meta/Channel name",
                                         g_strdup(section->zrange.name));
        switch (section->direction) {
            case SCAN_FORWARD:
            s = " forward";
            break;

            case SCAN_BACKWARD:
            s = " backward";
            break;

            default:
            s = "";
            break;
        }
        gwy_container_set_string_by_name(container, "/filename/title",
                                         g_strdup_printf("%s%s",
                                                         section->zrange.name,
                                                         s));
    }
}

static void
store_meta(gpointer key,
           gpointer value,
           gpointer user_data)
{
    StoreMetaData *smd = (StoreMetaData*)user_data;
    gchar *cval;

    if (!(cval = g_convert(value, strlen(value), "UTF-8", "ISO-8859-1",
                           NULL, NULL, NULL)))
        return;
    g_string_truncate(smd->str, sizeof("/meta/") - 1);
    g_string_append(smd->str, key);
    gwy_container_set_string_by_name(smd->container, smd->str->str, cval);
}

static void
process_metadata(GPtrArray *ezdfile,
                 GwyContainer *container)
{
    StoreMetaData smd;
    EZDSection *section;
    guint i;

    smd.container = container;
    smd.str = g_string_new("/meta/");
    for (i = 0; i < ezdfile->len; i++) {
        section = (EZDSection*)g_ptr_array_index(ezdfile, i);
        if (gwy_strequal(section->name, "DataSet-Info"))
            g_hash_table_foreach(section->meta, store_meta, &smd);
    }
    g_string_free(smd.str, TRUE);
}

static void
read_data_field(GwyDataField *dfield,
                EZDSection *section)
{
    gdouble *data;
    gdouble q, z0;
    guint i, j;
    gint xres, yres;

    g_assert(section->data);
    xres = section->xres;
    yres = section->yres;
    gwy_data_field_resample(dfield, xres, yres, GWY_INTERPOLATION_NONE);
    data = gwy_data_field_get_data(dfield);

    q = 1 << section->bitdepth;
    z0 = q/2.0;
    if (section->bitdepth == 8) {
        const gchar *p = section->data;

        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++)
                data[i*xres + j] = (p[(yres-1 - i)*xres + j] + z0)/q;
        }
    }
    else if (section->bitdepth == 16) {
        const gint16 *p = (const gint16*)section->data;

        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                data[i*xres + j] = GINT16_FROM_LE(p[(yres-1 - i)*xres + j]);
                data[i*xres + j] = (data[i*xres + j] + z0)/q;
            }
        }
    }
    else
        g_warning("Damn! Bit depth %d is not implemented", section->bitdepth);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

