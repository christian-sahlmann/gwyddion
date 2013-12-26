/*
 *  @(#) $Id$
 *  Copyright (C) 2013 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-ome-tiff">
 *   <comment>Open Microscopy OME-TIFF</comment>
 *   <magic priority="10">
 *     <match type="string" offset="0" value="II\x2a\x00"/>
 *     <match type="string" offset="0" value="II\x2b\x00"/>
 *     <match type="string" offset="0" value="MM\x00\x2a"/>
 *     <match type="string" offset="0" value="MM\x00\x2b"/>
 *   </magic>
 *   <glob pattern="*.ome.tiff"/>
 *   <glob pattern="*.ome.tif"/>
 *   <glob pattern="*.OME.TIFF"/>
 *   <glob pattern="*.OME.TIF"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Open Microscopy OME TIFF
 * .ome.tiff, .ome.tif
 * Read
 **/
#define DEBUG 1
#include "config.h"
#include <stdlib.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>
#include "err.h"
#include "gwytiff.h"

#define Micrometre (1e-6)

#ifdef HAVE_MEMRCHR
#define strlenrchr(s,c,len) (gchar*)memrchr((s),(c),(len))
#else
#define strlenrchr(s,c,len) strrchr((s),(c))
#endif

#define MAGIC_COMMENT1 "<OME "
#define MAGIC_COMMENT2 "http://www.openmicroscopy.org/"

typedef enum {
    OME_DIM_ORDER_XYZCT,
    OME_DIM_ORDER_XYZTC,
    OME_DIM_ORDER_XYCTZ,
    OME_DIM_ORDER_XYCZT,
    OME_DIM_ORDER_XYTCZ,
    OME_DIM_ORDER_XYTZC,
} OMEDimensionOrder;

typedef struct {
    guint ifd;
    guint firstz;
    guint firstt;
    guint firstc;
    guint planecount;
} OMEData;

typedef struct {
    GString *path;
    /* From Pixels (or possibly Image) element */
    OMEDimensionOrder dim_order;
    guint xres;
    guint yres;
    guint zres;
    guint tres;
    guint cres;
    gdouble xreal;
    gdouble yreal;
    gdouble zreal;
    gdouble dt;
    /* Data planes */
    GArray *data;
    GHashTable *meta;
} OMEFile;

static gboolean      module_register   (void);
static gint          ome_detect       (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* ome_load         (const gchar *filename,
                                        GwyRunType mode,
                                        GError **error);
static GwyContainer* ome_load_tiff    (const GwyTIFF *tiff,
                                        GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports OME-TIFF data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("ometiff",
                           N_("Open Microscopy OME-TIFF (.ome.tiff)"),
                           (GwyFileDetectFunc)&ome_detect,
                           (GwyFileLoadFunc)&ome_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
ome_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    GwyTIFF *tiff;
    gint score = 0;
    gchar *comment = NULL;

    if (only_name)
        return score;

    /* Weed out non-TIFFs */
    if (!gwy_tiff_detect(fileinfo->head, fileinfo->buffer_len, NULL, NULL))
        return 0;

    /* Use GwyTIFF for detection to avoid problems with fragile libtiff. */
    if ((tiff = gwy_tiff_load(fileinfo->name, NULL))
        && gwy_tiff_get_string0(tiff, GWY_TIFFTAG_IMAGE_DESCRIPTION, &comment)
        && strstr(comment, MAGIC_COMMENT1)
        && strstr(comment, MAGIC_COMMENT2))
        score = 100;

    g_free(comment);
    if (tiff)
        gwy_tiff_free(tiff);

    return score;
}

static GwyContainer*
ome_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyTIFF *tiff;
    GwyContainer *container = NULL;

    tiff = gwy_tiff_load(filename, error);
    if (!tiff)
        return NULL;

    container = ome_load_tiff(tiff, error);
    gwy_tiff_free(tiff);

    return container;
}

static void
start_element(G_GNUC_UNUSED GMarkupParseContext *context,
              const gchar *element_name,
              const gchar **attribute_names,
              const gchar **attribute_values,
              gpointer user_data,
              GError **error)
{
    static const GwyEnum dim_orders[] = {
        { "XYZCT", OME_DIM_ORDER_XYZCT, },
        { "XYZTC", OME_DIM_ORDER_XYZTC, },
        { "XYCTZ", OME_DIM_ORDER_XYCTZ, },
        { "XYCZT", OME_DIM_ORDER_XYCZT, },
        { "XYTCZ", OME_DIM_ORDER_XYTCZ, },
        { "XYTZC", OME_DIM_ORDER_XYTZC, },
    };

    OMEFile *omefile = (OMEFile*)user_data;
    guint i;

    gwy_debug("<%s>", element_name);
    if (!omefile->path->len && !gwy_strequal(element_name, "OME")) {
        g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                    _("Top-level element is not ‘%s’."), "OME");
        return;
    }

    g_string_append_c(omefile->path, '/');
    g_string_append(omefile->path, element_name);

    for (i = 0; attribute_names[i]; i++) {
        gwy_debug("%s:%s <%s>",
                  omefile->path->str, attribute_names[i], attribute_values[i]);
    }

    /* Just read the values here, validate them later. */
    if (gwy_stramong(omefile->path->str,
                     "/OME/Image", "/OME/Image/Pixels",
                     NULL)) {
        for (i = 0; attribute_names[i]; i++) {
            const gchar *name = attribute_names[i], *val = attribute_values[i];
            if (gwy_strequal(name, "DimensionOrder")) {
                omefile->dim_order
                    = gwy_string_to_enum(val,
                                         dim_orders, G_N_ELEMENTS(dim_orders));
            }
            else if (gwy_strequal(name, "SizeX"))
                omefile->xres = atoi(val);
            else if (gwy_strequal(name, "SizeY"))
                omefile->yres = atoi(val);
            else if (gwy_strequal(name, "SizeZ"))
                omefile->zres = atoi(val);
            else if (gwy_strequal(name, "SizeC"))
                omefile->cres = atoi(val);
            else if (gwy_strequal(name, "SizeT"))
                omefile->tres = atoi(val);
            else if (gwy_strequal(name, "PhysicalSizeX"))
                omefile->xreal = Micrometre * g_ascii_strtod(val, NULL);
            else if (gwy_strequal(name, "PhysicalSizeY"))
                omefile->yreal = Micrometre * g_ascii_strtod(val, NULL);
            else if (gwy_strequal(name, "PhysicalSizeZ"))
                omefile->zreal = Micrometre * g_ascii_strtod(val, NULL);
            else if (gwy_strequal(name, "TimeStep"))
                omefile->dt = g_ascii_strtod(val, NULL);
        }
    }
}

static void
end_element(G_GNUC_UNUSED GMarkupParseContext *context,
            const gchar *element_name,
            gpointer user_data,
            G_GNUC_UNUSED GError **error)
{
    OMEFile *omefile = (OMEFile*)user_data;
    gchar *pos;

    gwy_debug("</%s>", element_name);
    pos = strlenrchr(omefile->path->str, '/', omefile->path->len);
    /* GMarkupParser should raise a run-time error if this does not hold. */
    g_assert(pos && strcmp(pos + 1, element_name) == 0);
    g_string_truncate(omefile->path, pos - omefile->path->str);
}

static void
text(G_GNUC_UNUSED GMarkupParseContext *context,
     const gchar *value,
     gsize value_len,
     gpointer user_data,
     G_GNUC_UNUSED GError **error)
{
    OMEFile *omefile = (OMEFile*)user_data;
    const gchar *path = omefile->path->str;
    gchar *val;

    if (!gwy_stramong(path,
                      "/OME/Image/AcquisitionDate",
                      "/OME/Image/Description",
                      NULL))
        return;

    val = g_strndup(value, value_len);
    g_strstrip(val);
    if (*val) {
        gwy_debug("%s <%s>", path, val);
        g_hash_table_replace(omefile->meta, g_strdup(path), val);
    }
    else
        g_free(val);
}

static GwyContainer*
ome_load_tiff(const GwyTIFF *tiff, GError **error)
{
    const gchar *colour_channels[] = { "Red", "Green", "Blue" };
    const gchar *colour_channel_gradients[] = {
        "RGB-Red", "RGB-Green", "RGB-Blue"
    };

    GwyContainer *container = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    GwyTIFFImageReader *reader = NULL;
    GMarkupParser parser = { start_element, end_element, text, NULL, NULL };
    GHashTable *hash = NULL;
    GMarkupParseContext *context = NULL;
    OMEFile omefile;
    gchar *comment = NULL;
    gchar *title = NULL;
    gchar *channeltitle;
    const gchar *value, *image0title, *keytitle;
    GError *err = NULL;
    guint dir_num = 0, ch, spp;
    gint id = 0;
    GString *key;

    /* Comment with parameters is common for all data fields */
    if (!gwy_tiff_get_string0(tiff, GWY_TIFFTAG_IMAGE_DESCRIPTION, &comment)
        || !strstr(comment, MAGIC_COMMENT1)
        || !strstr(comment, MAGIC_COMMENT2)) {
        g_free(comment);
        err_FILE_TYPE(error, "OME-TIFF");
        return NULL;
    }

    /* Read the comment header. */
    gwy_clear(&omefile, 1);
    omefile.meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                         g_free, g_free);
    omefile.path = key = g_string_new(NULL);
    context = g_markup_parse_context_new(&parser, G_MARKUP_TREAT_CDATA_AS_TEXT,
                                         &omefile, NULL);
    if (!g_markup_parse_context_parse(context, comment, strlen(comment), &err)
        || !g_markup_parse_context_end_parse(context, &err)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("XML parsing failed: %s"), err->message);
        g_clear_error(&err);
        goto fail;
    }

    gwy_debug("res xy,ztc: %u %u, %u %u %u",
              omefile.xres, omefile.yres, omefile.zres, omefile.tres, omefile.cres);

    err_NO_DATA(error);

fail:
    g_free(title);
    g_free(comment);
    if (reader) {
        gwy_tiff_image_reader_free(reader);
        reader = NULL;
    }
    if (omefile.meta)
        g_hash_table_destroy(omefile.meta);
    if (key)
        g_string_free(key, TRUE);
    if (context)
        g_markup_parse_context_free(context);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
