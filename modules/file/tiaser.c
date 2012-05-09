/*
 *  @(#) $Id$
 *  Copyright (C) 2012 David Necas (Yeti), Daniil Bratashov (dn2010).
 *  E-mail: yeti@gwyddion.net, dn2010@gmail.com.
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

/*
 *  Format description from here:
 *  http://www.microscopy.cen.dtu.dk/~cbb/info/TIAformat/index.html
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-tiaser-tem">
 *   <comment>FEI TIA (Emispec) data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" word-size="2" value="\x49\x49\x01\x97"/>
 *   </magic>
 *   <glob pattern="*.ser"/>
 *   <glob pattern="*.SER"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * FEI Tecnai imaging and analysis (former Emispec) data
 * .ser
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC1 "\x49\x49\x97\x01"
#define MAGIC2 "\x49\x49\x01\x97"
#define MAGIC_SIZE (sizeof(MAGIC1)-1)

#define EXTENSION ".ser"

typedef enum {
    TIA_DATA_UINT8          = 1,
    TIA_DATA_UINT16         = 2,
    TIA_DATA_UINT32         = 3,
    TIA_DATA_INT8           = 4,
    TIA_DATA_INT16          = 5,
    TIA_DATA_INT32          = 6,
    TIA_DATA_FLOAT          = 7,
    TIA_DATA_DOUBLE         = 8,
    TIA_DATA_FLOAT_COMPLEX  = 9,
    TIA_DATA_DOUBLE_COMPLEX = 10
} TIADataType;

typedef enum {
    TIA_ES_LE       = 0x4949,
    TIA_ES_MAGIC    = 0x0197,
    TIA_ES_VERSION  = 0x0210,
    TIA_1D_DATA     = 0x4120,
    TIA_2D_DATA     = 0x4122,
    TIA_TAG_TIME    = 0x4152,
    TIA_TAG_TIMEPOS = 0x4142,
    TIA_HEADER_SIZE = 3 * 2 + 6 * 4,
    TIA_DIM_SIZE    = 4 * 4 + 2 * 8,
    TIA_2D_SIZE     = 50,
} TIAConsts;

typedef struct {
    gint16 byteorder;
    gint16 seriesid;
    gint16 seriesversion;
    gint32 datatypeid;
    gint32 tagtypeid;
    gint32 totalnumberelements;
    gint32 validnumberelements;
    gint32 offsetarrayoffset;
    gint32 numberdimensions;
} TIAHeader;

typedef struct {
    gint32  numelements;
    gdouble calibrationoffset;
    gdouble calibrationdelta;
    gint32  calibrationelement;
    gint32  descriptionlength;
    gchar  *description;
    gint32  unitslength;
    gchar  *units;
} TIADimensionArray;

typedef struct {
    gint16  tagtypeid;
    gint32  time;
} TIATimeTag;

typedef struct {
    gint16  tagtypeid;
    gint32  time;
    gdouble positionx;
    gdouble positiony;
} TIATimePosTag;

typedef struct {
    gdouble     calibrationoffset;
    gdouble     calibrationdelta;
    gint32      calibrationelement;
    TIADataType datatype;
    gint32      arraylength;
    gchar      *data;
} TIA1DData;

typedef struct {
    gdouble     calibrationoffsetx;
    gdouble     calibrationdeltax;
    gint32      calibrationelementx;
    gdouble     calibrationoffsety;
    gdouble     calibrationdeltay;
    gint32      calibrationelementy;
    TIADataType datatype;
    gint32      arraylengthx;
    gint32      arraylengthy;
    gchar      *data;
} TIA2DData;

static gboolean      module_register   (void);
static gint          tia_detect        (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* tia_load          (const gchar *filename,
                                        GwyRunType mode,
                                        GError **error);
static void          tia_load_header   (const guchar *p,
                                        TIAHeader *header);
static gboolean      tia_check_header  (TIAHeader *header, gsize size);
static gboolean      tia_load_dimarray (const guchar *p,
                                        TIADimensionArray *dimarray,
                                        gsize size);
static GwyDataField* tia_read_2d       (const guchar *p,
                                        gsize size);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports FEI Tecnai imaging and analysis (former Emispec) files."),
    "dn2010 <dn2010@gmail.com>",
    "0.1",
    "David Nečas (Yeti), Daniil Bratashov (dn2010)",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("tiaser",
                           N_("FEI TIA (Emispec) data"),
                           (GwyFileDetectFunc)&tia_detect,
                           (GwyFileLoadFunc)&tia_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
tia_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC1, MAGIC_SIZE) == 0)
        score = 100;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC2, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static void tia_load_header(const guchar *p, TIAHeader *header)
{
    header->byteorder           = gwy_get_guint16_le(&p);
    header->seriesid            = gwy_get_guint16_le(&p);
    header->seriesversion       = gwy_get_guint16_le(&p);
    header->datatypeid          = gwy_get_guint32_le(&p);
    header->tagtypeid           = gwy_get_guint32_le(&p);
    header->totalnumberelements = gwy_get_guint32_le(&p);
    header->validnumberelements = gwy_get_guint32_le(&p);
    header->offsetarrayoffset   = gwy_get_guint32_le(&p);
    header->numberdimensions    = gwy_get_guint32_le(&p);

    gwy_debug("bo=%X si=%X sv=%X dtid=%X ttid=%X",
              header->byteorder,
              header->seriesid,
              header->seriesversion,
              header->datatypeid,
              header->tagtypeid);
    gwy_debug("elemtot=%i elemvalid=%i offset=%i ndim=%i",
              header->totalnumberelements,
              header->validnumberelements,
              header->offsetarrayoffset,
              header->numberdimensions);
}

static gboolean tia_check_header(TIAHeader *header, gsize size)
{
    if ((header->byteorder != TIA_ES_LE)
     || (header->seriesid != TIA_ES_MAGIC)
     || (header->seriesversion != TIA_ES_VERSION)
    || ((header->datatypeid != TIA_1D_DATA)
     && (header->datatypeid != TIA_2D_DATA))
    || ((header->tagtypeid != TIA_TAG_TIME)
     && (header->tagtypeid != TIA_TAG_TIMEPOS))
     || (header->totalnumberelements < header->validnumberelements)
     || (header->offsetarrayoffset >= size)
     || (size-header->offsetarrayoffset < 8 * header->totalnumberelements))
        return FALSE;

    return TRUE;
}

static gboolean
tia_load_dimarray(const guchar *p, TIADimensionArray *dimarray, gsize size)
{
    dimarray->numelements        = gwy_get_guint32_le(&p);
    dimarray->calibrationoffset  = gwy_get_gdouble_le(&p);
    dimarray->calibrationdelta   = gwy_get_gdouble_le(&p);
    dimarray->calibrationelement = gwy_get_guint32_le(&p);
    gwy_debug("numelem=%i caloffset=%G caldelta=%G calelem=%i",
              dimarray->numelements,
              dimarray->calibrationoffset,
              dimarray->calibrationdelta,
              dimarray->calibrationelement);

    dimarray->descriptionlength  = gwy_get_guint32_le(&p);
    if (dimarray->descriptionlength >= size - TIA_HEADER_SIZE
                                            - TIA_DIM_SIZE) {
        return FALSE;
    }
    dimarray->description = g_strndup(p, dimarray->descriptionlength);
    p += dimarray->descriptionlength;
    dimarray->unitslength  = gwy_get_guint32_le(&p);
    if (dimarray->unitslength + dimarray->descriptionlength >= size
                                     - TIA_HEADER_SIZE - TIA_DIM_SIZE) {
        return FALSE;
    }
    dimarray->units = g_strndup(p, dimarray->unitslength);
    p += dimarray->unitslength;
    gwy_debug("descr = \"%s\" units=\"%s\"",
              dimarray->description,
              dimarray->units);

    return TRUE;
}

static GwyContainer*
tia_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode, GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer;
    gsize size;
    GError *err = NULL;
    const guchar *p;
    TIAHeader *header;
    TIADimensionArray *dimarray;
    GArray *dataoffsets, *tagoffsets;
    GwyDataField *dfield;
    gint i, offset;
    gchar *strkey;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < TIA_HEADER_SIZE) {
        err_TOO_SHORT(error);
        goto fail;
    }

    header = g_new0(TIAHeader, 1);
    p = buffer;
    tia_load_header(p, header);
    if(!tia_check_header(header, size)) {
        err_FILE_TYPE(error, "FEI TIA");
        goto fail2;
    }
    p += TIA_HEADER_SIZE;

    dimarray = g_new0(TIADimensionArray, 1);
    if(!tia_load_dimarray(p, dimarray, size - TIA_HEADER_SIZE)) {
        err_FILE_TYPE(error, "FEI TIA");
        goto fail2;
    }
    p += TIA_DIM_SIZE
       + dimarray->descriptionlength + dimarray->unitslength;

    p = buffer+header->offsetarrayoffset;
    dataoffsets = g_array_new(FALSE, TRUE, sizeof(gint32));
    for (i = 0; i < header->totalnumberelements; i++) {
        offset = gwy_get_guint32_le(&p);
        g_array_append_val(dataoffsets, offset);
    }
    tagoffsets = g_array_new(FALSE, TRUE, sizeof(gint32));
    for (i = 0; i < header->totalnumberelements; i++) {
        offset = gwy_get_guint32_le(&p);
        g_array_append_val(tagoffsets, offset);
    }

    container = gwy_container_new();
    if (header->datatypeid == TIA_2D_DATA)
        for (i = 0; i < header->validnumberelements; i++) {
            offset = g_array_index(dataoffsets, gint32, i);
            if ((offset > size)||(size-offset < 50)) {
                gwy_debug("Attempt to read after EOF");
            }
            else {
                dfield = tia_read_2d(buffer + offset, size - offset);
                if (dfield) {
                    GQuark key = gwy_app_get_data_key_for_id(i);

                    gwy_container_set_object(container, key, dfield);
                    g_object_unref(dfield);

                    strkey = g_strdup_printf("/%u/data/title", i);
                    gwy_container_set_string_by_name(container,
                                                     strkey,
                                                     g_strdup("TEM"));
                    g_free(strkey);
                }
            }
        }

    g_array_free(dataoffsets, TRUE);
    g_array_free(tagoffsets, TRUE);
    g_free(dimarray->description);
    g_free(dimarray->units);
    g_free(dimarray);
fail2:
    g_free(header);
fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    if (container && !gwy_container_get_n_items(container)) {
        g_object_unref(container);
        container = NULL;
    }

    return container;
}

static GwyDataField* tia_read_2d(const guchar *p, gsize size)
{
    GwyDataField *dfield = NULL;
    TIA2DData *fielddata;
    gint i, n;
    gdouble *data;
    gdouble xoffset, yoffset;
    gint tia_datasizes[11] = {0, 1, 1, 2, 2, 4, 4, 4, 8, 8, 16};

    fielddata = g_new0(TIA2DData, 1);
    fielddata->calibrationoffsetx  = gwy_get_gdouble_le(&p);
    fielddata->calibrationdeltax   = gwy_get_gdouble_le(&p);
    fielddata->calibrationelementx = gwy_get_guint32_le(&p);
    fielddata->calibrationoffsety  = gwy_get_gdouble_le(&p);
    fielddata->calibrationdeltay   = gwy_get_gdouble_le(&p);
    fielddata->calibrationelementy = gwy_get_guint32_le(&p);
    fielddata->datatype            = (TIADataType)gwy_get_guint16_le(&p);
    fielddata->arraylengthx        = gwy_get_guint32_le(&p);
    fielddata->arraylengthy        = gwy_get_guint32_le(&p);
    fielddata->data = (gchar *)p;

    if ((fielddata->datatype < TIA_DATA_UINT8)
     || (fielddata->datatype > TIA_DATA_DOUBLE)
     || (size < 50 + fielddata->arraylengthx * fielddata->arraylengthy
              * tia_datasizes[fielddata->datatype])) {
        goto fail_2d;
    }

    gwy_debug("X: caloffset=%G caldelta=%G calelem=%i",
              fielddata->calibrationoffsetx,
              fielddata->calibrationdeltax,
              fielddata->calibrationelementx);
    gwy_debug("Y: caloffset=%G caldelta=%G calelem=%i",
              fielddata->calibrationoffsety,
              fielddata->calibrationdeltay,
              fielddata->calibrationelementy);
    gwy_debug("nx=%i ny=%i type=%i", fielddata->arraylengthx,
                                     fielddata->arraylengthy,
                                     fielddata->datatype);

    dfield = gwy_data_field_new(fielddata->arraylengthx,
                 fielddata->arraylengthy,
                 fielddata->arraylengthx*fielddata->calibrationdeltax,
                 fielddata->arraylengthy*fielddata->calibrationdeltay,
                 TRUE);
    xoffset = fielddata->calibrationoffsetx
        - fielddata->calibrationdeltax * fielddata->calibrationelementx;
    yoffset = fielddata->calibrationoffsety
        - fielddata->calibrationdeltay * fielddata->calibrationelementy;
    if (dfield) {
        gwy_data_field_set_xoffset (dfield, xoffset);
        gwy_data_field_set_yoffset (dfield, yoffset);
        gwy_si_unit_set_from_string(
                            gwy_data_field_get_si_unit_xy(dfield), "m");
        data = gwy_data_field_get_data(dfield);
        n = fielddata->arraylengthx * fielddata->arraylengthy;
        switch (fielddata->datatype) {
            case TIA_DATA_UINT8:
            {
                for(i = 0; i < n; i++)
                    *(data++) = (*(p++)) / (gdouble)G_MAXUINT8;
            }
            break;
            case TIA_DATA_UINT16:
            {
                const guint16 *tp = (const guint16 *)p;

                for(i = 0; i < n; i++)
                    *(data++) = GUINT16_FROM_LE(*(tp++))
                                                 / (gdouble)G_MAXUINT16;
            }
            break;
            case TIA_DATA_UINT32:
            {
                const guint32 *tp = (const guint32 *)p;

                for(i = 0; i < n; i++)
                    *(data++) = GUINT32_FROM_LE(*(tp++))
                                                 / (gdouble)G_MAXUINT32;
            }
            break;
            case TIA_DATA_INT8:
            {
                const gchar *tp = (const gchar *)p;

                for(i = 0; i < n; i++)
                    *(data++) = (*(tp++)) / (gdouble)G_MAXINT8;
            }
            break;
            case TIA_DATA_INT16:
            {
                const gint16 *tp = (const gint16 *)p;

                for(i = 0; i < n; i++)
                    *(data++) = GINT16_FROM_LE(*(tp++))
                                                  / (gdouble)G_MAXINT16;
            }
            break;
            case TIA_DATA_INT32:
            {
                const gint32 *tp = (const gint32 *)p;

                for(i = 0; i < n; i++)
                    *(data++) = GINT32_FROM_LE(*(tp++))
                                                  / (gdouble)G_MAXINT32;
            }
            break;
            case TIA_DATA_FLOAT:
            {
                for(i = 0; i < n; i++)
                    *(data++) = gwy_get_gfloat_le(&p);
            }
            break;
            case TIA_DATA_DOUBLE:
            {
                for(i = 0; i < n; i++)
                    *(data++) = gwy_get_gdouble_le(&p);
            }
            break;
            default:
            g_assert_not_reached();
            break;
        }
    }

    fail_2d:
    g_free(fielddata);
    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
