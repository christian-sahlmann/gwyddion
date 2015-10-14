/*
 *  $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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
 * [FILE-MAGIC-USERGUIDE]
 * Princeton Instruments camera SPE
 * .spe
 * Read
 **/
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define TAIL_MAGIC "</SpeFormat>"
#define TAIL_MAGIC_SIZE (sizeof(TAIL_MAGIC)-1)
#define BLOODY_UTF8_BOM "\xef\xbb\xbf"
#define EXTENSION ".spe"

#define STRIDE_KEY "/SpeFormat/DataFormat/DataBlock::stride"

/* The only fields where anything at all seems to be present in newer files. */
enum {
    XRES_CCD_OFFSET  = 0x06,
    YRES_CCD_OFFSET  = 0x12,
    NOSCAN_OFFSET    = 0x22,
    XRES_OFFSET      = 0x2a,
    DATA_TYPE_OFFSET = 0x6c,
    YRES_OFFSET      = 0x290,
    SCRAMBLE_OFFSET  = 0x292,
    LNOSCAN_OFFSET   = 0x298,
    FOOTER_OFFSET    = 0x2a6,
    NUMFRAMES_OFFSET = 0x5a6,
    VERSION_OFFSET   = 0x7c8,
    HEADER_SIZE      = 0x1004,
};

typedef enum {
    PSPE_DATA_FLOAT = 0,
    PSPE_DATA_LONG = 1,
    PSPE_DATA_SHORT = 2,
    PSPE_DATA_USHORT = 3,
    PSPE_DATA_NTYPES
} PSPEDataType;

typedef struct {
    gsize size;
    guchar *buffer;
    guint xres_ccd;
    guint yres_ccd;
    guint xres;
    guint yres;
    PSPEDataType data_type;
    guint scramble;
    guint num_frames;
    guint noscan;
    guint lnoscan;
    guint footer_offset;
    gdouble version;
    /* Derived data. */
    guint stride;
    GwyRawDataType rawtype;
    GString *str;
    GString *path;
    GHashTable *hash;
} PSPEFile;

static gboolean      module_register (void);
static gint          pspe_detect     (const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer* pspe_load       (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static gboolean      pspe_read_header(PSPEFile *pspefile,
                                      const guchar *buffer);
static gboolean      pspe_check_size (PSPEFile *pspefile,
                                      GError **error);
static void          parse_xml_footer(PSPEFile *pspefile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Princeton Instruments camera SPE files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("princetonspe",
                           N_("Princeton Instruments SPE files"),
                           (GwyFileDetectFunc)&pspe_detect,
                           (GwyFileLoadFunc)&pspe_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
pspe_detect(const GwyFileDetectInfo *fileinfo,
            gboolean only_name)
{
    PSPEFile pspefile;
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    gwy_clear(&pspefile, 1);
    pspefile.size = fileinfo->file_size;
    if (fileinfo->file_size > HEADER_SIZE
        && fileinfo->buffer_len >= NUMFRAMES_OFFSET + sizeof(guint32)
        && pspe_read_header(&pspefile, fileinfo->head)
        && pspe_check_size(&pspefile, NULL)) {
        score = 90;
        /* New files (3.0) have some XML at the end.  Check that for surer
         * identification. */
        if (gwy_memmem(fileinfo->tail, fileinfo->buffer_len,
                       TAIL_MAGIC, TAIL_MAGIC_SIZE))
            score = 100;
    }

    return score;
}

static GwyContainer*
pspe_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    PSPEFile pspefile;
    GwyContainer *container = NULL;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gchar *value;
    GQuark quark;
    guint i, typesize, imagelen, len, nframes, data_size;

    gwy_clear(&pspefile, 1);

    if (!gwy_file_get_contents(filename, &pspefile.buffer, &pspefile.size,
                               &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (pspefile.size < HEADER_SIZE) {
        err_TOO_SHORT(error);
        goto fail;
    }
    if (!pspe_read_header(&pspefile, pspefile.buffer)) {
        err_FILE_TYPE(error, "Princeton Instruments SPE");
        goto fail;
    }
    gwy_debug("ccd %u x %u", pspefile.xres_ccd, pspefile.yres_ccd);
    gwy_debug("res %u x %u", pspefile.xres, pspefile.yres);
    gwy_debug("num frames %u", pspefile.num_frames);
    gwy_debug("data type %u", pspefile.data_type);
    gwy_debug("version %g", pspefile.version);
    gwy_debug("footer offset %u", pspefile.footer_offset);

    if (!pspe_check_size(&pspefile, error))
        goto fail;

    typesize = gwy_raw_data_size(pspefile.rawtype);
    imagelen = typesize * pspefile.xres * pspefile.yres;
    nframes = pspefile.num_frames;

    parse_xml_footer(&pspefile);
    if (pspefile.hash
        && (value = g_hash_table_lookup(pspefile.hash, STRIDE_KEY))) {
        /* Not sure what is stored between images.  I have seen 8 bytes with
         * one double precision number there. */
        data_size = pspefile.footer_offset - HEADER_SIZE;
        len = atoi(value);
        if (len < imagelen) {
            err_INVALID(error, "DataBlock::stride");
            goto fail;
        }
        if (!len || data_size/len != nframes) {
            err_SIZE_MISMATCH(error, data_size, len*nframes, TRUE);
            goto fail;
        }
        imagelen = len;
    }

    container = gwy_container_new();
    for (i = 0; i < nframes; i++) {
        dfield = gwy_data_field_new(pspefile.xres, pspefile.yres,
                                    pspefile.xres, pspefile.yres,
                                    FALSE);
        gwy_convert_raw_data(pspefile.buffer + HEADER_SIZE + imagelen*i,
                             pspefile.xres * pspefile.yres, 1,
                             pspefile.rawtype, GWY_BYTE_ORDER_LITTLE_ENDIAN,
                             gwy_data_field_get_data(dfield), 1.0, 0.0);
        quark = gwy_app_get_data_key_for_id(i);
        gwy_container_set_object(container, quark, dfield);
        g_object_unref(dfield);

        gwy_app_set_data_field_title(container, i, "Intensity");
        gwy_file_channel_import_log_add(container, i, NULL, filename);
    }

fail:
    gwy_file_abandon_contents(pspefile.buffer, pspefile.size, NULL);
    if (pspefile.hash)
        g_hash_table_destroy(pspefile.hash);
    if (pspefile.str)
        g_string_free(pspefile.str, TRUE);

    return container;
}

static gboolean
pspe_read_header(PSPEFile *pspefile, const guchar *buffer)
{
    const guchar *p;

    p = buffer + XRES_CCD_OFFSET;
    pspefile->xres_ccd = gwy_get_guint16_le(&p);
    p = buffer + YRES_CCD_OFFSET;
    pspefile->yres_ccd = gwy_get_guint16_le(&p);

    p = buffer + XRES_OFFSET;
    pspefile->xres = gwy_get_guint16_le(&p);
    p = buffer + YRES_OFFSET;
    pspefile->yres = gwy_get_guint16_le(&p);

    p = buffer + DATA_TYPE_OFFSET;
    pspefile->data_type = gwy_get_guint16_le(&p);
    p = buffer + SCRAMBLE_OFFSET;
    pspefile->scramble = gwy_get_guint16_le(&p);
    p = buffer + NUMFRAMES_OFFSET;
    pspefile->num_frames = gwy_get_guint32_le(&p);

    p = buffer + NOSCAN_OFFSET;
    pspefile->noscan = gwy_get_guint16_le(&p);
    p = buffer + LNOSCAN_OFFSET;
    pspefile->lnoscan = gwy_get_guint32_le(&p);

    p = buffer + VERSION_OFFSET;
    pspefile->version = gwy_get_gfloat_le(&p);
    if (pspefile->version >= 3.0) {
        p = buffer + FOOTER_OFFSET;
        pspefile->footer_offset = gwy_get_guint32_le(&p);
    }

    /* The noscan and lnoscan fields must be filled with one-bits, apparently.
     * The rest is more difficult. */
    return (pspefile->noscan == 0xffff
            && pspefile->lnoscan == 0xffffffff
            && pspefile->scramble == 1
            && pspefile->data_type < PSPE_DATA_NTYPES);
}

static gboolean
pspe_check_size(PSPEFile *pspefile, GError **error)
{
    guint typesize, xres, yres, nframes, size = pspefile->size;

    if (err_DIMENSION(error, pspefile->xres))
        return FALSE;
    if (err_DIMENSION(error, pspefile->yres))
        return FALSE;
    if (!pspefile->num_frames) {
        err_NO_DATA(error);
        return FALSE;
    }

    if (pspefile->data_type == PSPE_DATA_FLOAT)
        pspefile->rawtype = GWY_RAW_DATA_FLOAT;
    else if (pspefile->data_type == PSPE_DATA_LONG)
        pspefile->rawtype = GWY_RAW_DATA_SINT32;
    else if (pspefile->data_type == PSPE_DATA_SHORT)
        pspefile->rawtype = GWY_RAW_DATA_SINT16;
    else if (pspefile->data_type == PSPE_DATA_USHORT)
        pspefile->rawtype = GWY_RAW_DATA_UINT16;
    else {
        err_DATA_TYPE(error, pspefile->data_type);
        return FALSE;
    }

    typesize = gwy_raw_data_size(pspefile->rawtype);
    xres = pspefile->xres;
    yres = pspefile->yres;
    nframes = pspefile->num_frames;
    /* Check the size safely with respect to integer overflows.  But do not
     * bother for the error message: the thing may need 128bit numbers to
     * format properly.  XXX: We may return FALSE with no error set with
     * unlucky numbers. */
    if ((size - HEADER_SIZE)/xres/yres/typesize < nframes) {
        err_SIZE_MISMATCH(error,
                          size - HEADER_SIZE, xres*yres*typesize*nframes,
                          TRUE);
        return FALSE;
    }

    if (pspefile->footer_offset < HEADER_SIZE
        || pspefile->footer_offset < HEADER_SIZE + xres*yres*typesize*nframes) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("XML footer overlaps with data."));
        return FALSE;
    }

    if (pspefile->footer_offset > pspefile->size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File is truncated."));
        return FALSE;
    }
    return TRUE;
}

static void
pspe_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
                   const gchar *element_name,
                   const gchar **attribute_names,
                   const gchar **attribute_values,
                   gpointer user_data,
                   G_GNUC_UNUSED GError **error)
{
    PSPEFile *pspefile = (PSPEFile*)user_data;
    GString *str = pspefile->str;
    gchar *path;
    guint i, len;

    g_string_append_c(pspefile->path, '/');
    g_string_append(pspefile->path, element_name);
    path = pspefile->path->str;

    g_string_assign(str, path);
    g_string_append(str, "::");
    len = str->len;
    for (i = 0; attribute_names[i]; i++) {
        if (!strlen(attribute_names[i]) || !strlen(attribute_values[i]))
            continue;
        g_string_append(str, attribute_names[i]);
        gwy_debug("%s <%s>", str->str, attribute_values[i]);
        g_hash_table_insert(pspefile->hash,
                            g_strdup(str->str), g_strdup(attribute_values[i]));
        g_string_truncate(str, len);
    }
}

static void
pspe_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
                 const gchar *element_name,
                 gpointer user_data,
                 G_GNUC_UNUSED GError **error)
{
    PSPEFile *pspefile = (PSPEFile*)user_data;
    guint n, len = pspefile->path->len;
    gchar *path = pspefile->path->str;

    n = strlen(element_name);
    g_return_if_fail(g_str_has_suffix(path, element_name));
    g_return_if_fail(len > n);
    g_return_if_fail(path[len-1 - n] == '/');
    g_string_set_size(pspefile->path, len-1 - n);
}

static void
pspe_text(G_GNUC_UNUSED GMarkupParseContext *context,
          const gchar *text,
          G_GNUC_UNUSED gsize text_len,
          gpointer user_data,
          G_GNUC_UNUSED GError **error)
{
    PSPEFile *pspefile = (PSPEFile*)user_data;
    gchar *path = pspefile->path->str;
    GString *str = pspefile->str;

    if (!strlen(text))
        return;

    g_string_assign(str, text);
    g_strstrip(str->str);
    if (!strlen(str->str))
        return;

    gwy_debug("%s <%s>", path, str->str);
    g_hash_table_insert(pspefile->hash, g_strdup(path), g_strdup(str->str));
}

static void
parse_xml_footer(PSPEFile *pspefile)
{
    GMarkupParser parser = {
        &pspe_start_element,
        &pspe_end_element,
        &pspe_text,
        NULL,
        NULL,
    };
    GMarkupParseContext *context = NULL;
    gchar *xmldata, *s;
    guint xmlsize = pspefile->size - pspefile->footer_offset;

    if (!xmlsize)
        return;

    xmldata = g_new(gchar, xmlsize + 1);
    memcpy(xmldata, pspefile->buffer + pspefile->footer_offset, xmlsize);
    xmldata[xmlsize] = '\0';

    gwy_strkill(xmldata, "\r");
    s = xmldata;
    /* Not seen in the wild but the XML people tend to use BOM in UTF-8... */
    if (g_str_has_prefix(s, BLOODY_UTF8_BOM))
        s += strlen(BLOODY_UTF8_BOM);

    pspefile->hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                           g_free, g_free);
    pspefile->path = g_string_new(NULL);
    pspefile->str = g_string_new(NULL);
    context = g_markup_parse_context_new(&parser, 0, pspefile, NULL);
    if (!g_markup_parse_context_parse(context, s, -1, NULL)
        || !g_markup_parse_context_end_parse(context, NULL)) {
        g_hash_table_destroy(pspefile->hash);
        pspefile->hash = NULL;
    }

    g_string_free(pspefile->path, TRUE);
    if (context)
        g_markup_parse_context_free(context);
    g_free(xmldata);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
