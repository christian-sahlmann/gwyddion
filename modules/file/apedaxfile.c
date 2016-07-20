/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  APDT and DAX file format Importer Module
 *  Copyright (C) 2015 A.P.E. Research srl
 *  E-mail: infos@aperesearch.com
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
 *  <Scan>
 *      <Header>
 *          <FileVersion>  </FileVersion>
 *          <Date>  </Date>
 *          <ScanSize>
 *          <XRes> </XRes>
 *          <YRes> </YRes>
 *          <X> </X>
 *          <Y> </Y>
 *          ...
 *          </ScanSize>
 *          ...
 *          </Header>
 *          <Channels>
 *          <Channel>
 *              <Label>  </Label>
 *              <DataUnit> </DataUnit>
 *              <BINFile>  </BINFile>
 *              <ConversionFactor></ConversionFactor>
 *              <AcquiredUnit> </AcquiredUnit> // ONLY VISUAL
 *          </Channel>
 *      </Channels>
 *  </Scan>
 *
 **/

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-ape-dax-spm">
 *   <comment>A.P.E. Research DAX SPM data</comment>
 *   <glob pattern="*.dax"/>
 *   <glob pattern="*.DAX"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * A.P.E. Research DAX
 * .dax
 * Read
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * A.P.E. Research APDT
 * .apdt
 * Read
 **/

/*Includes*/

#include "config.h"
#include <libgwyddion/gwyddion.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "err.h"
#include "gwyzip.h"

/*Macros*/
#define FILE_TYPE "DAX"
#define EXTENSION ".dax"
#define APDT_FILE_TYPE "APDT"
#define APDT_EXTENSION ".apdt"
#define MAGIC "PK\x03\x04"
#define MAGIC_SIZE (sizeof(MAGIC)-1)


/**
 * The minimum information the file must have
 */
#define XML_FILE_NAME "scan.xml"

#define SPM_MODE_NAME "SpmMode"

#define XY_UNIT_STRING "m"

#define XML_PATH_SCAN_SIZE_XRES "/Scan/Header/ScanSize/XRes"
#define XML_PATH_SCAN_SIZE_YRES "/Scan/Header/ScanSize/YRes"
#define XML_PATH_SCAN_SIZE_X "/Scan/Header/ScanSize/X"
#define XML_PATH_SCAN_SIZE_Y "/Scan/Header/ScanSize/Y"

#define XML_CHANNEL_BINARY_FILE_NAME "BINFile"
#define XML_CHANNEL_DATA_UNIT_NAME "DataUnit"
#define XML_CHANNEL_CONVERSION_FACTOR_NAME "ConversionFactor"
#define XML_CHANNEL_LABEL_NAME "Label"


const gchar* scan_string = "/Scan";
const gchar* header_string = "/Scan/Header";
const gchar* scanSize_string = "/Scan/Header/ScanSize";
const gchar* channel_string = "/Scan/Channels/Channel";

static const gchar* scan[] = { "Header", "Channels" };
static const gchar* header[] = { "FileVersion", "Date", "ScanSize" };
static const gchar* scanSizeArray[] = { "XRes", "YRes", "X", "Y" };
static const gchar* channel[] = { "Label", "DataUnit", "BINFile" };

/*Enums*/

/*SPM modes*/
typedef enum {
    SPM_MODE_SNOM = 0,
    SPM_MODE_AFM_NONCONTACT = 1,
    SPM_MODE_AFM_CONTACT = 2,
    SPM_MODE_STM = 3,
    SPM_MODE_PHASE_DETECT_AFM = 4,
    SPM_MODE_LAST
} SPMModeType;

/*SPM Modes Labels*/
static const GwyEnum spm_modes_display_names[] = {
    { "SNOM",                  SPM_MODE_SNOM },
    { "AFM Non-contact",       SPM_MODE_AFM_NONCONTACT },
    { "AFM Contact",           SPM_MODE_AFM_CONTACT },
    { "STM",                   SPM_MODE_STM },
    { "Phase detection AFM",   SPM_MODE_PHASE_DETECT_AFM },
};

static const GwyEnum spm_modes_names[] = {
    { "SNOM",      SPM_MODE_SNOM },
    { "AFM_NC",    SPM_MODE_AFM_NONCONTACT },
    { "AFM_C",     SPM_MODE_AFM_CONTACT },
    { "STM",       SPM_MODE_STM },
    { "PHASE_AFM", SPM_MODE_PHASE_DETECT_AFM },
};

#if 0
/* Not used any more? */
/*APDT Sensor types*/
typedef enum {
    SENSOR_UNKNOWN = -1,
    SENSOR_CANTILEVER = 0,
    SENSOR_CAPACITIVE = 1
} SensorType;

static const GwyEnum sensor_types[] = {
    { "Unknown",    SENSOR_UNKNOWN,    },
    { "Cantilever", SENSOR_CANTILEVER, },
    { "Capacitive", SENSOR_CAPACITIVE, },
};
#endif

/*Structures*/
typedef struct {
    gint XRes;
    gint YRes;
    gdouble XReal;
    gdouble YReal;
} APEScanSize;

typedef struct {
    gchar *name;
    gchar *display_name;
} APEXmlField;

/*XML fields arrays*/
static const APEXmlField dax_afm_c[] = {
    { "VPmt1", "BIAS DC Voltage" }
};

static const APEXmlField dax_afm_nc[] = {
    { "TipOscFreq", "Tip Oscillation Frequency" },
    { "VPmt1", "BIAS DC Voltage" }
};

static const APEXmlField dax_snom[] = {
    { "TipOscFreq", "Tip Oscillation Frequency" },
    { "VPmt1", "PMT 1 Voltage" },
    { "VPmt2", "PMT 2 Voltage" }
};

static const APEXmlField dax_stm[] = {
};

/*Prototypes*/

static gboolean      module_register               (void);
static gint          apedax_detect                 (const GwyFileDetectInfo *fileinfo,
                                                    gboolean only_name);
static GwyContainer* apedax_load                   (const gchar *filename,
                                                    GwyRunType mode,
                                                    GError **error);
static GwyContainer* apedax_get_meta               (guchar *scan_xml_content,
                                                    gsize content_size,
                                                    gboolean apdt_file);
static gchar*        apedax_get_xml_node_as_string (xmlDocPtr doc,
                                                    const gchar *node_xpath);
static GwyDataField* apedax_get_data_field         (GwyZipFile uFile,
                                                    const gchar *chFileName,
                                                    const APEScanSize *scan_size,
                                                    gchar *zUnit,
                                                    gdouble scale,
                                                    GError **error);
static gboolean      apedax_get_channels_data      (GwyZipFile uFile,
                                                    guchar *scan_xml_content,
                                                    gsize content_size,
                                                    const gchar *filename,
                                                    GwyContainer *container,
                                                    GwyContainer *meta,
                                                    const APEScanSize *scan_size,
                                                    GError **error);

static gboolean     apedax_is_xml_valid            (guchar *scan_xml_content,
                                                    gsize content_size);

static gboolean     apedax_is_xml_node_present     (xmlDocPtr doc,
                                                    const gchar *node_xbase_path,
                                                    const gchar *node_xpath,
                                                    gboolean unique);

static gboolean     apedax_get_scan_size           (xmlDocPtr doc,
                                                    APEScanSize *scan_size,
                                                    GError **error);

/*Informations about the module*/

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports A.P.E. Research DAX data files."),
    "Andrea Cervesato <infos@aperesearch.com>, "
        "Gianfranco Gallizia <infos@aperesearch.com>, "
        "Samo Ziberna <infos@aperesearch.com>",
    "0.7",
    "A.P.E. Research srl",
    "2015"
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("apedaxfile",
                           N_("A.P.E. Research DAX Files (.dax) and APDT File (.apdt)"),
                           (GwyFileDetectFunc)&apedax_detect,
                           (GwyFileLoadFunc)&apedax_load,
                           NULL,
                           NULL);

    return TRUE;
}

/*Detect function*/

static gint
apedax_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    gint score = 0;
    GwyZipFile uFile;
    guchar *buffer;
    gsize size = 0;
    GError **error = NULL;

    score += (g_str_has_suffix(fileinfo->name_lowercase, EXTENSION)
              ? 10 : 0);
    score += (g_str_has_suffix(fileinfo->name_lowercase, APDT_EXTENSION)
              ? 10 : 0);

    if (only_name)
        return score;

    if (fileinfo->file_size > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score += 30;
    else
        return 0;

    gwy_debug("Opening the file with MiniZIP");
    uFile = gwyzip_open(fileinfo->name);

    if (uFile == NULL) {
        return 0;
    }

    if (gwyzip_locate_file(uFile, XML_FILE_NAME, 0, NULL)) {

        buffer = gwyzip_get_file_content(uFile, &size, error);

        if (buffer != NULL) {
            score += 30;
            g_free(buffer);
        }
        else {
            score = 0;
        }
    }
    else {
        score = 0;
    }

    gwyzip_close(uFile);

    return score;
}

/*Load function*/

static GwyContainer*
apedax_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    GwyContainer *container = NULL;
    GwyContainer *meta = NULL;
    GwyZipFile uFile;
    guchar *buffer = NULL;
    gsize content_size = 0;
    gboolean apdt_flag = FALSE;
    gchar *lowercaseFilename;
    xmlDocPtr doc = NULL;
    gboolean channels_data_result = FALSE;
    gboolean scan_size_result = FALSE;
    APEScanSize scan_size;

    scan_size.XRes = 0;
    scan_size.YRes = 0;
    scan_size.XReal = 0.0;
    scan_size.YReal = 0.0;

    lowercaseFilename = g_ascii_strdown(filename, -1);
    apdt_flag = g_str_has_suffix(lowercaseFilename, APDT_EXTENSION);
    g_free(lowercaseFilename);

    gwy_debug("Opening the file with MiniZIP");
    uFile = gwyzip_open(filename);

    if (uFile == NULL) {
        goto fail;
    }

    gwy_debug("Locating the XML file");

    if (!gwyzip_locate_file(uFile, XML_FILE_NAME, 0, NULL))
        goto fail;

    buffer = gwyzip_get_file_content(uFile, &content_size, error);

    if (buffer == NULL)
        goto fail;

    if (!apedax_is_xml_valid(buffer, content_size))
        goto fail;

    container = gwy_container_new();

    meta = apedax_get_meta(buffer, content_size, apdt_flag);

    if (meta == NULL) {
        gwy_debug("Metadata Container is NULL");
        goto fail;
    }

    doc = xmlReadMemory(buffer, content_size, XML_FILE_NAME, NULL, 0);
    if (doc == NULL)
        goto fail;

    scan_size_result = apedax_get_scan_size(doc, &scan_size, error);

    if (!scan_size_result)
        goto fail;

    channels_data_result = apedax_get_channels_data(uFile,
                                                    buffer,
                                                    content_size,
                                                    filename,
                                                    container,
                                                    meta,
                                                    &scan_size,
                                                    error);
    if (!channels_data_result)
        goto fail;

    goto cleanup;

fail:
    if (apdt_flag) {
        err_FILE_TYPE(error, APDT_FILE_TYPE);
    }
    else {
        err_FILE_TYPE(error, FILE_TYPE);
    }
    gwy_debug("Cleaning up after a fail");
    if (container != NULL) {
        gwy_object_unref(container);
    }

cleanup:
    /* Clean */
    xmlFreeDoc(doc);
    g_free(buffer);
    if (meta != NULL) {
        g_object_unref(meta);
    }
    if (uFile != NULL) {
        gwyzip_close(uFile);
    }

    return container;
}

/*Gets the metadata from the XML file*/
static GwyContainer*
apedax_get_meta(guchar *scan_xml_content,
                gsize content_size,
                gboolean apdt_file)
{
    GwyContainer* meta = NULL;
    xmlDocPtr doc = NULL;
    xmlNodePtr cur = NULL;
    xmlNodePtr cur_child = NULL;
    const gchar* buffer = NULL;
    gint current_SPM_mode = -1;
    const APEXmlField *fields = NULL;
    guint fields_size = 0;
    guint i = 0;
    xmlXPathContextPtr context = NULL;
    xmlXPathObjectPtr path_object = NULL;
    xmlNodeSetPtr node_set;
    xmlNodePtr head_node;
    gchar *key = NULL;
    gchar *key_child = NULL;
    xmlChar *value = NULL;
    gboolean have_child = FALSE;
    gchar* concat_key = NULL;

    const gchar *spm_mode_disp = NULL;

    if (scan_xml_content == NULL || content_size == 0)
        return NULL;

    meta = gwy_container_new();

    gwy_debug("Parsing the scan XML file");
    doc = xmlReadMemory(scan_xml_content,
                        content_size,
                        XML_FILE_NAME,
                        NULL,
                        0);

    if (doc == NULL) {
        goto fail;
     }

    gwy_debug("Populating metadata container");

    context = xmlXPathNewContext(doc);
    if (context == NULL) {
        goto fail;
    }
    path_object = xmlXPathEvalExpression(header_string, context);

    if (path_object == NULL) {
        goto fail;
    }

    /*There must be at least one header node*/
    node_set = path_object->nodesetval;
    head_node = node_set->nodeTab[0];

    cur = head_node->xmlChildrenNode;

    while (cur) {
        if (cur->type == XML_ELEMENT_NODE) {
            have_child = FALSE;
            key = (gchar*)cur->name;
            cur_child = cur->xmlChildrenNode;
            while (cur_child) {
                if (cur_child->type == XML_ELEMENT_NODE) {
                    key_child = (gchar*)cur_child->name;
                    value = xmlNodeGetContent(cur_child);
                    if (value != NULL) {
                        concat_key = g_strconcat(key, " - ", key_child, NULL);
                        gwy_container_set_const_string_by_name(meta, concat_key,
                                                               (gchar*)value);
                        g_free(concat_key);
                        xmlFree(value);
                        have_child = TRUE;
                    }
                }
                cur_child = cur_child->next;
            }
            if (!have_child) {
                value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                if (value != NULL) {
                    gwy_container_set_const_string_by_name(meta, key,
                                                           (gchar*)value);
                    xmlFree(value);
                }
            }
        }
        cur = cur->next;
    }


    if (apdt_file) {
        gwy_debug("Selected APDT file fields");
    }
    else {
        gwy_debug("Selected DAX file fields");
        /*Fetch the SPM Mode*/
        if (gwy_container_contains_by_name(meta, SPM_MODE_NAME)) {
            buffer = gwy_container_get_string_by_name(meta, SPM_MODE_NAME);
            current_SPM_mode = gwy_string_to_enum(buffer,
                                                  spm_modes_names,
                                                  G_N_ELEMENTS(spm_modes_names));
            spm_mode_disp = gwy_enum_to_string(current_SPM_mode,
                                               spm_modes_display_names,
                                               G_N_ELEMENTS(spm_modes_display_names));
            gwy_container_set_const_string_by_name(meta, SPM_MODE_NAME,
                                                   spm_mode_disp);
            /* Change the meta names */
            switch (current_SPM_mode) {
                case SPM_MODE_AFM_CONTACT:
                fields = dax_afm_c;
                fields_size = G_N_ELEMENTS(dax_afm_c);
                break;

                case SPM_MODE_AFM_NONCONTACT:
                fields = dax_afm_nc;
                fields_size = G_N_ELEMENTS(dax_afm_nc);
                break;

                case SPM_MODE_SNOM:
                fields = dax_snom;
                fields_size = G_N_ELEMENTS(dax_snom);
                break;

                case SPM_MODE_STM:
                fields = dax_stm;
                fields_size = G_N_ELEMENTS(dax_stm);
                break;
            }

            for (i = 0; i < fields_size; i++) {
                if (gwy_container_contains_by_name(meta, fields[i].name)) {
                    gwy_container_rename_by_name(meta, fields[i].name,
                                                 fields[i].display_name, FALSE);
                }
                else {
                    gwy_debug("Some metadata not present!");
                }
            }
        }
        else {
            gwy_debug("Cannot get SpmMode field");
            goto fail;
        }
    }

    gwy_debug("Returning metadata container");
    goto cleanup;

fail:
    gwy_debug("Cleaning up after a fail");
    gwy_object_unref(meta);
cleanup:
    if (path_object != NULL) {
        xmlXPathFreeObject(path_object);
    }
    if (context != NULL) {
        xmlXPathFreeContext(context);
    }
    if (doc != NULL) {
        xmlFreeDoc(doc);
    }
    return meta;
}


/** Return the xml node as string
 *
 * Return the value of the xml node or NULL if not found
 */
static gchar*
apedax_get_xml_node_as_string(xmlDocPtr doc, const gchar *node_xpath)
{
    gchar *node_string = NULL;
    xmlChar *node_xstring = NULL;
    xmlXPathContextPtr context;
    xmlXPathObjectPtr path_object;
    xmlNodeSetPtr node_set;

    context = xmlXPathNewContext(doc);

    if (context == NULL) {
        return NULL;
    }

    path_object = xmlXPathEvalExpression((const xmlChar*)node_xpath, context);

    if (path_object != NULL) {

        if (!xmlXPathNodeSetIsEmpty(path_object->nodesetval)) {

            node_set = path_object->nodesetval;

            if (node_set->nodeNr == 1) {
                node_xstring = xmlNodeListGetString(doc,
                                                    node_set->nodeTab[0]->xmlChildrenNode,
                                                    1);
                node_string = g_strdup((gchar*)node_xstring);
                xmlFree(node_xstring);
            }
        }
        xmlXPathFreeObject(path_object);

    }
    xmlXPathFreeContext(context);

    return node_string;
}

static GwyDataField*
apedax_get_data_field(GwyZipFile uFile,
                      const gchar *channel_filename,
                      const APEScanSize *scan_size,
                      gchar *zUnit,
                      gdouble scale,
                      GError **error)
{
    GwyDataField *dfield = NULL;
    GwySIUnit *xyUnit;
    GwySIUnit *zSIUnit;
    gdouble *data;
    guchar *buffer;
    gsize content_size, expected_size;

    expected_size = scan_size->XRes * scan_size->YRes * sizeof(gdouble);

    if (!gwyzip_locate_file(uFile, channel_filename, 0, error)
        || !(buffer = gwyzip_get_file_content(uFile, &content_size, error)))
        return NULL;

    if (err_SIZE_MISMATCH(error, expected_size, content_size, FALSE)) {
        g_free(buffer);
        return NULL;
    }

    dfield = gwy_data_field_new(scan_size->XRes, scan_size->YRes,
                                scan_size->XReal, scan_size->YReal,
                                FALSE);

    data = gwy_data_field_get_data(dfield);

    xyUnit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_set_from_string(xyUnit, XY_UNIT_STRING);

    zSIUnit = gwy_data_field_get_si_unit_z(dfield);
    gwy_si_unit_set_from_string(zSIUnit, zUnit);

    gwy_debug("Reading RAW data");

    gwy_convert_raw_data(buffer,
                         scan_size->XRes * scan_size->YRes,
                         1,
                         GWY_RAW_DATA_DOUBLE,
                         GWY_BYTE_ORDER_LITTLE_ENDIAN,
                         data,
                         scale,
                         0.0);
    g_free(buffer);

    return dfield;
}

static gboolean
apedax_get_channels_data(GwyZipFile uFile,
                         guchar *scan_xml_content,
                         gsize content_size,
                         const gchar *filename,
                         GwyContainer *container,
                         GwyContainer *meta,
                         const APEScanSize *scan_size,
                         GError **error)
{
    gboolean outcome = FALSE;
    xmlDocPtr doc = NULL;
    xmlNodePtr cur = NULL;
    xmlXPathContextPtr context = NULL;
    xmlXPathObjectPtr path_object = NULL;
    xmlNodeSetPtr node_set;
    xmlChar *buffer = NULL;
    gchar key[256];
    gint i;
    gint power10 = 0;
    gdouble scale_factor = 1.0;
    GwySIUnit *zUnit;
    gchar *label = NULL;
    gchar *zUnit_string = NULL;
    gchar *bin_filename = NULL;
    GwyDataField *dfield;
    GwyContainer *channel_meta;

    if (scan_xml_content == NULL || content_size == 0)
        return FALSE;

    gwy_clear(key, sizeof(key));

    doc = xmlReadMemory(scan_xml_content,
                        content_size,
                        XML_FILE_NAME,
                        NULL,
                        0);

    if (doc == NULL) {
        goto fail;
    }

    context = xmlXPathNewContext(doc);

    if (context == NULL) {
        goto fail;
    }

    path_object = xmlXPathEvalExpression(channel_string, context);

    if (path_object == NULL) {
        goto fail;
    }
    /*There must be at least one channel*/
    if (xmlXPathNodeSetIsEmpty(path_object->nodesetval)) {
        err_NO_DATA(error);
        goto fail;
    }

    node_set = path_object->nodesetval;

    if (node_set->nodeNr < 0) {
        goto fail;
    }

    for (i = 0; i < node_set->nodeNr; i++) {
        /* Initialize the values to default */
        scale_factor = 1.0;
        label = NULL;
        zUnit_string = NULL;
        bin_filename = NULL;

        cur = node_set->nodeTab[i]->xmlChildrenNode;
        /* Create a meta container for this channel */
        channel_meta = gwy_container_duplicate(meta);
        while (cur) {
            buffer = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            if (buffer != NULL) {
                const gchar *curname = cur->name;

                /*Label*/
                if (gwy_strequal(curname, XML_CHANNEL_LABEL_NAME)) {
                    label = g_strdup(buffer);
                }
                /*Factor*/
                else if (gwy_strequal(curname,
                                      XML_CHANNEL_CONVERSION_FACTOR_NAME)) {
                    scale_factor = g_ascii_strtod((gchar*)buffer, NULL);
                }
                /*Unit*/
                else if (gwy_strequal(curname, XML_CHANNEL_DATA_UNIT_NAME)) {
                    zUnit_string = g_strdup((gchar*)buffer);
                    zUnit = gwy_si_unit_new_parse(zUnit_string, &power10);
                    g_object_unref(zUnit);
                }
                /*Binary file name*/
                else if (gwy_strequal(curname, XML_CHANNEL_BINARY_FILE_NAME)) {
                    bin_filename = g_strdup((gchar*)buffer);
                }
                else {
                    /* Add the info to the metadata of the channel */
                    gwy_container_set_const_string_by_name(channel_meta,
                                                           curname,
                                                           (gchar*)buffer);
                }
                xmlFree(buffer);
            }
            cur = cur->next;
        }

        if (bin_filename != NULL && zUnit_string != NULL && label != NULL) {
            g_snprintf(key, sizeof(key), "/%d/data/title", i);
            gwy_container_set_string_by_name(container, key, label);
            scale_factor *= pow(10.0, power10);

            dfield = apedax_get_data_field(uFile,
                                           bin_filename,
                                           scan_size,
                                           zUnit_string,
                                           scale_factor,
                                           error);
            if (dfield) {
                g_snprintf(key, sizeof(key), "/%d/data", i);
                gwy_container_set_object_by_name(container, key, dfield);
                gwy_file_channel_import_log_add(container, i, NULL, filename);

                g_snprintf(key, sizeof(key), "/%d/meta", i);
                gwy_container_set_object_by_name(container, key, channel_meta);

                g_object_unref(dfield);
                outcome = TRUE; /* We have at least one channel */
            }

            g_free(bin_filename);
        }
        else {
            g_warning("Missing one or more chanel field(s). Ignoring chanel.");
        }
        g_object_unref(channel_meta);
    }

    if (!outcome) {
        /* We don't have channels */
        goto fail;
    }

    goto cleanup;

fail:
    /* Something went wrong!! */
    if (error == NULL) {
        err_FILE_TYPE(error, FILE_TYPE);
    }

cleanup:
    if (path_object != NULL) {
        xmlXPathFreeObject(path_object);
    }
    if (context != NULL) {
        xmlXPathFreeContext(context);
    }
    if (doc != NULL) {
        xmlFreeDoc(doc);
    }
    return outcome;
}

/**
 * Retrieve the scan size and check the dimensions.
 *
 * Returns TRUE if it find all the dimensions and if they are valid dimensions,
 * false otherwise.
 */
static gboolean
apedax_get_scan_size(xmlDocPtr doc, APEScanSize *scan_size, GError **error)
{
    xmlChar *buffer = NULL;

    /*Number of columns (XRes)*/
    buffer = apedax_get_xml_node_as_string(doc, XML_PATH_SCAN_SIZE_XRES);

    if (buffer == NULL) {
        return FALSE;
    }
    scan_size->XRes = (gint)g_ascii_strtod(buffer, NULL);
    g_free(buffer);

    /*Number of rows (YRes)*/
    buffer = apedax_get_xml_node_as_string(doc, XML_PATH_SCAN_SIZE_YRES);

    if (buffer == NULL) {
        return FALSE;
    }
    scan_size->YRes = (gint)g_ascii_strtod(buffer, NULL);
    g_free(buffer);

    /*Width in nanometers*/
    buffer = apedax_get_xml_node_as_string(doc, XML_PATH_SCAN_SIZE_X);

    if (buffer == NULL) {
        return FALSE;
    }
    scan_size->XReal = g_ascii_strtod(buffer, NULL);
    scan_size->XReal *= 1e-9; /*nm to m conversion*/
    g_free(buffer);


    /*Height in nanometers*/
    buffer = apedax_get_xml_node_as_string(doc, XML_PATH_SCAN_SIZE_Y);

    if (buffer == NULL) {
        return FALSE;
    }
    scan_size->YReal = g_ascii_strtod(buffer, NULL);
    scan_size->YReal *= 1e-9; /*nm to m conversion*/
    g_free(buffer);

    /*Checking the dimensions*/
    if (err_DIMENSION(error, scan_size->XRes)) {
        return FALSE;
    }

    if (err_DIMENSION(error, scan_size->YRes)) {
        return FALSE;
    }

    /*If XReal it's not greater than 0 or XReal is NaN*/
    if (!(fabs(scan_size->XReal) > 0)) {
        err_UNSUPPORTED(error, "X scan size");
        return FALSE;
    }

    /*Same for YReal*/
    if (!(fabs(scan_size->YReal) > 0)) {
        err_UNSUPPORTED(error, "Y scan size");
        return FALSE;
    }
    return TRUE;
}

static gboolean
apedax_is_xml_valid(guchar *scan_xml_content, gsize content_size)
{
    xmlDocPtr doc;

    gboolean channel_ok = FALSE;
    gint num_elements = 0;
    gint i = 0;

    doc = xmlReadMemory(scan_xml_content,
                        content_size,
                        XML_FILE_NAME,
                        NULL,
                        0);

    if (doc == NULL) {
        return FALSE;
    }

    /* Check if is present all the minimum Scan data (names in scan[]) */
    num_elements = G_N_ELEMENTS(scan);
    for (i = 0; i < num_elements; i++) {
        if (!apedax_is_xml_node_present(doc, scan_string, scan[i], TRUE))
            return FALSE;
    }

    /* Check if is present all the minimum header data (names in header[]) */
    num_elements = G_N_ELEMENTS(header);
    for (i = 0; i < num_elements; i++) {
        if (!apedax_is_xml_node_present(doc, header_string, header[i], TRUE))
            return FALSE;
    }

    /* Check if is present all the minimum scanSize data (names in scanSize[])
     */
    num_elements = G_N_ELEMENTS(scanSizeArray);
    for (i = 0; i < num_elements; i++) {
        if (!apedax_is_xml_node_present(doc, scanSize_string,
                                        scanSizeArray[i], TRUE))
            return FALSE;
    }

    /* Check if is present at least one Channel with the minimum channel data
     * (names in channel[]) */
    channel_ok = TRUE;
    num_elements = G_N_ELEMENTS(channel);
    for (i = 0; i < num_elements; i++) {
        if (!apedax_is_xml_node_present(doc, channel_string,
                                        channel[i], FALSE)) {
            channel_ok = FALSE;
            break;
        }
    }
    return channel_ok;
}

static gboolean
apedax_is_xml_node_present(xmlDocPtr doc,
                           const gchar *node_xbase_path,
                           const gchar *node_xpath,
                           gboolean unique)
{
    xmlXPathContextPtr context;
    xmlXPathObjectPtr path_object;
    xmlNodeSetPtr node_set;
    gboolean result = FALSE;
    gchar* node_absolute_path;

    context = xmlXPathNewContext(doc);

    if (context == NULL) {
        return result;
    }

    node_absolute_path = g_strconcat(node_xbase_path, "/", node_xpath, NULL);

    if (node_absolute_path == NULL) {
        gwy_debug("Memory exception");
        return FALSE;
    }
    path_object = xmlXPathEvalExpression((const xmlChar*)node_absolute_path,
                                         context);

    if (path_object != NULL) {
        if (!xmlXPathNodeSetIsEmpty(path_object->nodesetval)) {
            node_set = path_object->nodesetval;
            if (node_set->nodeNr >= 1) {
                if (unique && node_set->nodeNr > 1) {
                    result = FALSE;
                }
                else {
                    result = TRUE;
                }
            }
        }
        xmlXPathFreeObject(path_object);
    }
    g_free(node_absolute_path);
    xmlXPathFreeContext(context);

    return result;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
