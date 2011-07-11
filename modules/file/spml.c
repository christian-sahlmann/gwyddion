/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
/* TODO: check/write allocation/deallocation
 *       change GArray to GPtrArray where possible */

/* CHANGELOG:
 * 2006/08/09
 * * atof() replaced by g_ascii_strtod() and set_locale() removed
 * 2006/06/07 - version 0.1.4
 * * corrected problem with non-square datachannel rotation
 * 2006/05/18 - version 0.1.3
 * * Corrected width and length of datafiels (rounding problem)
 * * TODO: what happend when axis or dimensions are nonequal?
 * 2006/05/07 - version 0.1.2
 * * Fixed offset of datafield (datafield is starting on location of axes start)
 * * Rotation of datafield (consult the meaning of axis)
 * * FIXME: Correct width and length of datafiels (rounding problem)
 *
 */
/*#define DEBUG*/

/* XXX: The typical length of a XML declaration is about 40 bytes.  So while
 * there can be more stuff before <SPML than 60 bytes, we have to find a
 * compromise between generality and efficiency.  It's the SPML guys' fight,
 * they should have created a more easily detectable format... */
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-spml-spm">
 *   <comment>SPML data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="&lt;?xml">
 *       <match type="string" offset="20:60" value="&lt;SPML"/>
 *     </match>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * SPML (Scanning Probe Microscopy Markup Language)
 * .xml
 * Read
 **/

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>

#include <libxml/xmlreader.h>
#include "spml-utils.h"

#define EXTENSION ".xml"

typedef enum {
    SKIP_STATE,
    IN_DATACHANNELS,
    IN_DATACHANNELGROUP,
    READ_COMPLETE
} DatachannelListParserStates;

static gboolean      module_register(void);
static gint          spml_detect    (const GwyFileDetectInfo * fileinfo,
                                     gboolean only_name);
static GwyContainer* spml_load      (const gchar *filename);
static int           get_axis       (char *filename,
                                     char *datachannel_name,
                                     GArray **axes,
                                     GArray **units,
                                     GArray **names);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Loads SPML (Scanning Probe Microscopy Markup Language) "
       "data files."),
    "Jan Hořák <xhorak@gmail.com>",
    "0.1.4",
    "Jan Hořák",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("spml",
                           N_("SPML files (.xml)"),
                           (GwyFileDetectFunc) &spml_detect,
                           (GwyFileLoadFunc) &spml_load,
                           NULL,
                           NULL);
    return TRUE;
}

static GList *
get_list_of_datachannels(const gchar *filename)
{
    const xmlChar *name;        /*, *value; */
    DatachannelListParserStates state = SKIP_STATE;
    int ret;
    GList *l = NULL;
    dataChannelGroup *data_channel_group = NULL;
    xmlTextReaderPtr reader;

    gwy_debug("filename = %s", filename);

    reader = xmlReaderForFile(filename, NULL, 0);

    if (reader != NULL) {
        ret = xmlTextReaderRead(reader);
        while (ret == 1) {
            /* process node */
            name = xmlTextReaderConstName(reader);
            switch (state) {
                case SKIP_STATE:
                    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT
                        && strcmp(name, "DataChannels") == 0) {
                        /* start of datachannels */
                        gwy_debug("Switch to datachannels");
                        state = IN_DATACHANNELS;
                    }
                    break;
                case IN_DATACHANNELS:
                    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT
                        && strcmp(name, "DataChannelGroup") == 0) {
                        /* datachannelgroup, get and set name of datachannelgroup
                           data_channel_group is dynammicaly allocated,
                           and it must be freed only when returned list is going to
                           be disposed because GList does not create
                           copy of data_channel_group. */
                        data_channel_group = g_malloc(sizeof(dataChannelGroup));
                        data_channel_group->name =
                            xmlTextReaderGetAttribute(reader, "name");
                        data_channel_group->datachannels = NULL;
                        gwy_debug("Switch to datachannelGroup '%s'",
                                  data_channel_group->name);
                        state = IN_DATACHANNELGROUP;
                    }
                    else if (xmlTextReaderNodeType(reader) ==
                             XML_READER_TYPE_END_ELEMENT
                             && strcmp(name, "DataChannels") == 0) {
                        /* after datachannels, return possible? */
                        gwy_debug("Datachannels end, read complete.");
                        state = READ_COMPLETE;
                    }
                    break;
                case IN_DATACHANNELGROUP:
                    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT
                        && strcmp(name, "DataChannel") == 0
                        && data_channel_group != NULL) {
                        /* datachannelgroup, get and set name of datachannelgroup */
                        data_channel_group->datachannels =
                            g_list_append(data_channel_group->datachannels,
                                          xmlTextReaderGetAttribute(reader,
                                                                    "name"));
                        gwy_debug("Read info about datachannel.");
                        /* stay in current state */
                    }
                    else if (xmlTextReaderNodeType(reader) ==
                             XML_READER_TYPE_END_ELEMENT
                             && strcmp(name, "DataChannelGroup") == 0
                             && data_channel_group != NULL) {
                        l = g_list_append(l, data_channel_group);
                        /* end of channel group, save current data_channel_group to list */
                        gwy_debug("End of datachannelgroup");
                        state = IN_DATACHANNELS;
                    }
                    break;
                case READ_COMPLETE:    /* blank state */
                    break;
            }
            if (state == READ_COMPLETE) {       /* read is complete, we can leave reading cycle */
                break;
            }
            /* end process node */
            ret = xmlTextReaderRead(reader);
        }
        xmlFreeTextReader(reader);
    }
    else {
        g_warning("SPML: Unable to open %s!", filename);
        return NULL;
    }
    return l;
}

static int
decode_data(double **data, const xmlChar * input, dataFormat data_format,
            codingTypes coding, byteOrder byte_order, int max_input_len)
{
    GArray *data_stream, *debase64_buf, *decoded_data;
    char *p, *end_ptr;
    unsigned int i;
    double val;
    int data_count = 0;

    gwy_debug("start.");

    if (input == NULL) {
        g_warning("SPML: decode_data(): NULL input");
        *data = NULL;
        return 0;
    }
    switch (coding) {
        case ZLIB_COMPR_BASE64:
            /*/ XXX: strlen() may not be nice there */
            if (decode_b64((char *)input, &debase64_buf, strlen(input)) != 0) {
                if (debase64_buf != NULL) {
                    g_array_free(debase64_buf, TRUE);
                }
                g_warning("Cannot decode data in BASE64 code.");
                *data = NULL;
                return 0;
            }
            if (inflate_dynamic_array(debase64_buf, &data_stream) != 0) {
                g_warning("Cannot inflate compressed data.");
                g_array_free(debase64_buf, TRUE);
                if (data_stream != NULL) {
                    g_array_free(data_stream, TRUE);
                }
                *data = NULL;
                return 0;
            }
            g_array_free(debase64_buf, TRUE);
            break;
        case BASE64:
            /*/ XXX: strlen() may not be nice there */
            if (decode_b64((char *)input, &data_stream, strlen(input)) != 0) {
                g_warning("Cannot decode data in BASE64 code.");
                if (data_stream != NULL) {
                    g_array_free(data_stream, TRUE);
                }
                *data = NULL;
                return 0;
            }
            break;
        case ASCII:
            p = (char *)input;
            data_stream = g_array_new(FALSE, FALSE, sizeof(double));
            while (p != NULL) {
                double num;

                if (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
                    p++;
                    continue;
                }
                num = g_ascii_strtod(p, &end_ptr);
                if (num == 0 && end_ptr == p) {
                    g_warning("SPML: decode_data(): No conversion performed "
                              "from ASCII string.");
                    break;
                }
                g_array_append_val(data_stream, num);
                p = end_ptr;
                data_count++;
            }

            break;
            /*/ TODO: */
        case HEX:
        case BINARY:
            g_warning("SPML: decode_data(): Data coding 'HEX' and 'BINARY' "
                      "not supported.");
            break;
        case UNKNOWN_CODING:
            break;
    }
    if (coding == ASCII) {
        /* we have already decoded data */
        if (max_input_len != -1 && data_count != max_input_len) {
            /* not enough input data to fill array defined by length
             * max_input_len */
            g_warning("SPML: decode_data():\n"
                      "Input has not the same length as declared in "
                        "dimensions\n"
                      "(max:%d vs read:%d). Has the channel attribute\n"
                      "'channelReadMethodName'? The channel may be  one\n"
                      "dimensional data used for axis values but not as\n"
                      "a source of data for Gwyddion.",
                      max_input_len, data_count);
            g_array_free(data_stream, TRUE);
            *data = NULL;
            return 0;
        }
        else {
            *data = (double *)data_stream->data;
            /* we can free dynamic array, but not */
            g_array_free(data_stream, FALSE);
            /* containing data. */
            gwy_debug("Datacount: %d", data_count);
            return data_count;
        }
    }
    decoded_data = g_array_new(FALSE, FALSE, sizeof(double));
    p = data_stream->data;
    i = 0;
    switch (data_format) {
        case FLOAT32:
            while (i < data_stream->len) {
                val = get_float32(&p, byte_order);
                g_array_append_val(decoded_data, val);
                data_count++;
                i += sizeof(float);
            }
            break;
        case FLOAT64:
            while (i < data_stream->len) {
                val = get_float64(&p, byte_order);
                g_array_append_val(decoded_data, val);
                i += sizeof(double);
            }
            break;
        case INT8:
            while (i < data_stream->len) {
                val = get_int8(&p);
                g_array_append_val(decoded_data, val);
                i += sizeof(gint8);
            }
            break;
        case INT16:
            while (i < data_stream->len) {
                val = get_int16(&p, byte_order);
                g_array_append_val(decoded_data, val);
                i += sizeof(gint16);
            }
            break;
        case INT32:
            while (i < data_stream->len) {
                val = get_int32(&p, byte_order);
                g_array_append_val(decoded_data, val);
                i += sizeof(gint32);
            }
            break;
        case UINT32:
            while (i < data_stream->len) {
                val = get_uint32(&p, byte_order);
                g_array_append_val(decoded_data, val);
                i += sizeof(guint32);
            }
            break;
        case UINT8:
            while (i < data_stream->len) {
                val = get_uint8(&p);
                g_array_append_val(decoded_data, val);
                i += sizeof(guint8);
            }
            break;
        case UINT16:
            while (i < data_stream->len) {
                val = get_uint16(&p, byte_order);
                g_array_append_val(decoded_data, val);
                i += sizeof(guint16);
            }
            break;
        case STRING:
            g_warning
                ("SPML: decode_data(): Data format 'String' not supported.");
            break;
        case UNKNOWN_DATAFORMAT:
            g_warning("SPML: decode_data(): Unknown dataformat.");
            break;
    }
    g_array_free(data_stream, TRUE);
    data_count = decoded_data->len;
    if (max_input_len != -1 && data_count != max_input_len) {
        g_warning("SPML: decode_data():\n"
                  "Input has not the same length as declared in dimensions\n"
                  "(max:%d vs read:%d). Has the channel attribute\n"
                  "'channelReadMethodName'? The channel may be  one\n"
                  "dimensional data used for axis values but not as\n"
                  "a source of data for Gwyddion.", max_input_len, data_count);
        g_array_free(decoded_data, TRUE);
        *data = NULL;
        return 0;
    }
    *data = (double *)decoded_data->data;
    g_array_free(decoded_data, FALSE);  /* we can free dynamic array, but not */
    /* containing data. */
    gwy_debug("Datacount: %d", data_count);
    return data_count;

}

static int
get_data(gboolean read_data_only, char *filename, char *datachannel_name,
         double *data[], int *dimensions[], char **unit,
         G_GNUC_UNUSED gboolean *scattered)
{
    dataFormat format = UNKNOWN_DATAFORMAT;
    codingTypes coding = UNKNOWN_CODING;
    byteOrder byte_order = UNKNOWN_BYTEORDER;
    int ret, i;
    int state = 0;
    GArray *axes, *names, *units, *axis;
    const xmlChar *name, *value, *read_method_name;
    xmlChar *id;
    xmlTextReaderPtr reader;
    int data_dimension = -1;
    int out_len;
    int max_input_len = 1;

    gwy_debug("read only: %d,  channel name==%s", read_data_only,
              datachannel_name);
    *dimensions = NULL;

    reader = xmlReaderForFile(filename, NULL, 0);

    if (reader != NULL) {
        ret = xmlTextReaderRead(reader);
        while (ret == 1) {
            /* process node */
            name = xmlTextReaderConstName(reader);
            if (name == NULL) {
                /* no datachannels */
                return -1;
            }
            if (state == 0
                && xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT
                && strcmp(name, "DataChannel") == 0) {
                id = xmlTextReaderGetAttribute(reader, "name");
                if (id && strcmp(id, datachannel_name) == 0) {
                    format =
                        get_data_format(xmlTextReaderGetAttribute
                                        (reader, "dataFormat"));
                    coding =
                        get_data_coding(xmlTextReaderGetAttribute
                                        (reader, "coding"));
                    byte_order =
                        get_byteorder(xmlTextReaderGetAttribute
                                      (reader, "byteOrder"));
                    if (read_data_only == FALSE) {
                        /* when read_data_only is true, we skip dimensions array to prevent
                           infinite cycle when calling from get_axis to get non equidistant axis from
                           datachannel */
                        read_method_name =
                            xmlTextReaderGetAttribute(reader,
                                                      "channelReadMethodName");
                        if (read_method_name == NULL) {
                            /* sequence of one dimensional values */
                            *dimensions = NULL;
                        }
                        else {
                            g_free((char *)read_method_name);
                            /* get axes */
                            data_dimension =
                                get_axis(filename, datachannel_name, &axes,
                                         &names, &units);
                            if (data_dimension != 2) {
                                g_warning
                                    ("SPML: get_data(): Input data are in %d dimension(s).",
                                     data_dimension);
                            }
                            /* one dimension array */
                            if (data_dimension > 0) {
                                *dimensions =
                                    g_malloc(sizeof(int) * data_dimension);
                                for (i = 0; i < data_dimension; i++) {
                                    axis = g_array_index(axes, GArray *, i);
                                    (*dimensions)[i] = axis[0].len;
                                    max_input_len *= axis[0].len;
                                }
                            }
                            else {
                                *dimensions = NULL;
                            }
                            /* free what get_axis allocated */
                            if (axes) {
                                for (i = 0; i < axes->len; i++) {
                                    g_array_free(g_array_index
                                                 (axes, GArray *, i), TRUE);
                                }
                                g_array_free(axes, TRUE);
                            }
                            if (names)
                                g_array_free(names, TRUE);
                            if (units)
                                g_array_free(units, TRUE);
                        }
                    }
                    else {
                        /* in the first member of dimension array store number of elements */
                        max_input_len = -1;     /* we don't know the max_input_len */
                    }

                    *unit = xmlTextReaderGetAttribute(reader, "unit");

                    gwy_debug
                        ("Datachannel summary: format %d, coding %d, order %d.",
                         format, coding, byte_order);
                    state = 1;
                }
                if (id) {
                    g_free(id);
                }
            }
            else if (state == 1) {
                value = xmlTextReaderConstValue(reader);
                if (value == NULL) {
                    g_warning("SPML: get_data(): No data available for "
                              "datachannel '%s'", datachannel_name);
                    *data = NULL;
                    break;
                }
                out_len =
                    decode_data(data, value, format, coding, byte_order,
                                max_input_len);
                if (out_len == 0 || *data == NULL) {
                    g_warning("SPML: get_data(): No input data available for"
                              " datachannel '%s'.", datachannel_name);

                }
                else if (read_data_only == TRUE) {
                    *dimensions = g_malloc(sizeof(int));
                    (*dimensions)[0] = out_len;
                    data_dimension = 1;
                }
                break;
            }
            /* end process node */
            ret = xmlTextReaderRead(reader);
        }                       /* end while (ret == 1) */
    }
    else {
        g_warning("Unable to open %s.", filename);
        return -1;
    }
    xmlFreeTextReader(reader);

    return data_dimension;
}

/**
 * Find node of given name in parent's children
 * @param parent node
 * @return NULL when children by given name not found otherwise
 * return pointer to first children of specified name
 */
static xmlNodePtr
get_node_ptr(xmlNodePtr parent, xmlChar * name)
{
    xmlNodePtr node;

    gwy_debug("name = %s", name);
    if (parent == NULL) {
        return NULL;
    }

    node = parent->children;

    while (node != NULL) {
        if (!xmlStrcmp(node->name, name)
            && node->type == XML_ELEMENT_NODE) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

/**
 * Find next after previous node of given name
 * @param prev previous node
 * @return NULL when next node by given name not found otherwise
 * return pointer to first next node to prev of specified name
 */
static xmlNodePtr
get_next_node_ptr(xmlNodePtr prev, xmlChar * name)
{
    xmlNodePtr node;

    gwy_debug("name = %s", name);
    if (prev == NULL) {
        return NULL;
    }

    node = prev->next;

    while (node != NULL) {
        if (!xmlStrcmp(node->name, name)
            && node->type == XML_ELEMENT_NODE) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

static xmlChar *
get_attribute_value_of_named_node(xmlNodePtr node, xmlChar * datachannel_name,
                                  xmlChar * attr_name)
{
    xmlChar *tmp_ch;

    gwy_debug("datachannel name = %s, attr name = %s", datachannel_name,
              attr_name);
    tmp_ch = xmlGetProp(node, (const xmlChar *)"name");
    if (tmp_ch) {
        if (!xmlStrcmp(datachannel_name, tmp_ch)) {
            /* found Datachannel of given name */
            g_free(tmp_ch);
            return xmlGetProp(node, attr_name);
        }
        g_free(tmp_ch);
    }
    return NULL;
}

static GArray *
get_axis_datapoints(char *filename, xmlNodePtr axis_node)
{
    int j;
    GArray *axes_values;
    xmlChar *ch;
    int num_of_dimensions;
    double step, start, size;
    double *data, value;
    int *dimensions;
    char *unit;
    gboolean scattered;

    gwy_debug("axis node name = %s", axis_node->name);
    if (axis_node == NULL) {
        return NULL;
    }
    ch = xmlGetProp(axis_node, "dataChannelName");
    if (ch) {
        /* points are saved in datachannel */
        /* TODO: load points from datachannel */
        num_of_dimensions =
            get_data(TRUE, filename, ch, &data, &dimensions, &unit, &scattered);
        if (num_of_dimensions == 1) {
            axes_values = g_array_new(TRUE, FALSE, sizeof(double));
            axes_values = g_array_append_vals(axes_values, data, dimensions[0]);
            return axes_values;
        }
        g_warning("SPML: Loading scattered data.");
        return NULL;
    }
    else {
        /* points will be computed from start step and size attribute */
        ch = xmlGetProp(axis_node, "start");
        if (ch) {
            start = g_ascii_strtod(ch, NULL);
            g_free(ch);
        }
        else
            return NULL;
        ch = xmlGetProp(axis_node, "step");
        if (ch) {
            step = g_ascii_strtod(ch, NULL);
            g_free(ch);
        }
        else
            return NULL;
        ch = xmlGetProp(axis_node, "size");
        if (ch) {
            size = g_ascii_strtod(ch, NULL);
            g_free(ch);
        }
        else
            return NULL;
        gwy_debug("step: %e, start:%e", step, start);
        axes_values = g_array_new(TRUE, FALSE, sizeof(double));
        for (j = 0; j < size; j++) {
            value = j * step + start;
            axes_values = g_array_append_val(axes_values, value);
        }
        return axes_values;
    }
    return NULL;
}


static int
get_axis(char *filename, char *datachannel_name, GArray ** axes,
         GArray ** units, GArray ** names)
{
/*    <DataChannel> => channelReadMethodName =>readMethod=>ReadAxis
        name=> Axis =>array_of_values unit, nr of elements.
*/
    int i;
    int axis_count = 0;
    xmlDocPtr doc;
    xmlNodePtr cur, sub_node, axes_node, datachannels_node;
    xmlChar *channelReadMethodName = NULL;
    xmlChar *tmp_ch, *tmp_ch2, *read_method_name;
    GArray *read_method_axes = g_array_new(TRUE, FALSE, sizeof(xmlChar *));
    GArray *axis_values = NULL;

    gwy_debug("datachannel name == %s", datachannel_name);
    doc = xmlParseFile(filename);
    if (doc == NULL) {
        g_warning("SPML: get_axis(): Input file was not parsed successfully.");
        *axes = NULL;
        *units = NULL;
        *names = NULL;
        g_array_free(read_method_axes, TRUE);
        return 0;
    }
    /* get pointer to Axes and DataChannels elements */
    cur = xmlDocGetRootElement(doc);
    axes_node = get_node_ptr(cur, "Axes");
    datachannels_node = get_node_ptr(cur, "DataChannels");

    if (axes_node == NULL || datachannels_node == NULL) {
        /* incomplete xml file */
        g_warning("SPML: get_axis(): incomplete file, missing Axes or "
                  "Datachannels tags.");
        xmlFreeDoc(doc);
        *axes = NULL;
        *units = NULL;
        *names = NULL;
        return 0;
    }

    /* get channelReadMethodName     */
    cur = get_node_ptr(datachannels_node, "DataChannelGroup");
    while (cur != NULL) {
        sub_node = get_node_ptr(cur, "DataChannel");
        while (sub_node != NULL) {
            channelReadMethodName =
                get_attribute_value_of_named_node(sub_node, datachannel_name,
                                                  "channelReadMethodName");
            if (channelReadMethodName) {
                /* channelReadMethodName found, leave searching */
                break;

            }
            sub_node = get_next_node_ptr(sub_node, "DataChannel");
        }
        if (channelReadMethodName) {
            /* channelReadMethodName found, leave searching  */
            break;
        }
        cur = get_next_node_ptr(cur, "DataChannelGroup");
    }

    if (channelReadMethodName == NULL) {
        g_warning("SPML: get_axis(): Datachannel '%s' not found.",
                  datachannel_name);
        xmlFreeDoc(doc);
        *axes = NULL;
        *units = NULL;
        *names = NULL;
        g_array_free(read_method_axes, TRUE);
        return 0;
    }

    /* get readMethod from DataChannels */
    cur = get_node_ptr(datachannels_node, "ReadMethod");
    while (cur != NULL) {
        tmp_ch = xmlGetProp(cur, "name");
        if (tmp_ch) {
            /* found ReadMethod according to selected datachannel_name */
            if (!xmlStrcmp(tmp_ch, channelReadMethodName)) {
                sub_node = get_node_ptr(cur, "ReadAxis");
                while (sub_node != NULL) {
                    read_method_name = xmlGetProp(sub_node, "name");
                    if (read_method_name) {
                        read_method_axes =
                            g_array_append_val(read_method_axes,
                                               read_method_name);
                    }
                    sub_node = get_next_node_ptr(sub_node, "ReadAxis");
                }
            }
            g_free(tmp_ch);
        }

        cur = get_next_node_ptr(cur, "ReadMethod");
    }
    if (g_array_index(read_method_axes, xmlChar *, 0) == NULL) {
        /* ReadMethod mentioned in selected DataChannel not found in ReadMethod section */
        g_warning("SPML: get_axis(): ReadMethod '%s' for datachannel "
                  "'%s' not found.",
                  channelReadMethodName, datachannel_name);
        xmlFreeDoc(doc);
        *axes = NULL;
        *units = NULL;
        *names = NULL;
        g_array_free(read_method_axes, TRUE);
        g_free(channelReadMethodName);
        return 0;
    }

    /* We have name of axes for given datachannel_name in GArray read_method_axes. */
    /* Time to load axes and fill output arrays. */
    *names = g_array_new(TRUE, FALSE, sizeof(xmlChar *));
    *units = g_array_new(TRUE, FALSE, sizeof(xmlChar *));
    *axes = g_array_new(FALSE, FALSE, sizeof(GArray *));
    cur = get_node_ptr(axes_node, "AxisGroup");
    while (cur != NULL) {
        sub_node = get_node_ptr(cur, "Axis");
        while (sub_node != NULL) {
            tmp_ch2 = xmlGetProp(sub_node, "name");
            if (tmp_ch2) {
                for (i = 0;
                     (read_method_name =
                      g_array_index(read_method_axes, xmlChar *, i)) != NULL;
                     i++) {
                    if (!xmlStrcmp(read_method_name, tmp_ch2)) {
                        /* we found axis we are searching for
                           add name to *names array */
                        tmp_ch = xmlGetProp(sub_node, "name");
                        g_array_append_val(*names, tmp_ch);
                        /* add units to *units array */
                        tmp_ch = xmlGetProp(sub_node, "unit");
                        if (tmp_ch != NULL) {
                            g_array_append_val(*units, tmp_ch);
                        }
                        else {
                            g_warning
                                ("SPML: get_axis(): unknown unit for axis.");
                            tmp_ch = g_malloc(4);
                            sprintf(tmp_ch, "N/A");
                            g_array_append_val(*units, tmp_ch);
                        }
                        /* get axis values */
                        axis_values = get_axis_datapoints(filename, sub_node);
                        if (axis_values != NULL) {
                            gwy_debug("Axis len: %d.", axis_values->len);
                            g_array_append_val(*axes, axis_values);
                            axis_count++;
                        }
                        else {
                            g_warning
                                ("SPML: get_axis(): Cannot compute or read axis data.");
                            /* g_array_free(*axes, TRUE); */
                            if (*units)
                                g_array_free(*units, TRUE);
                            if (*names)
                                g_array_free(*names, TRUE);
                            *axes = NULL;
                            *units = NULL;
                            *names = NULL;

                            if (read_method_axes)
                                g_array_free(read_method_axes, TRUE);
                            xmlFreeDoc(doc);
                            g_free(channelReadMethodName);
                            return 0;
                        }
                    }
                }
                g_free(tmp_ch2);
            }
            sub_node = get_next_node_ptr(sub_node, "Axis");
        }
        cur = get_next_node_ptr(cur, "AxisGroup");
    }
    g_array_free(read_method_axes, TRUE);
    xmlFreeDoc(doc);
    g_free(channelReadMethodName);
    return axis_count;
}

static GArray *
get_dimensions(GArray * axes)
{
    unsigned int i;
    double width;
    GArray *axis, *out_array = NULL;

    gwy_debug("start");
    if (axes == NULL)
        return NULL;

    out_array = g_array_new(FALSE, FALSE, sizeof(double));
    for (i = 0; i < axes->len; i++) {
        axis = g_array_index(axes, GArray *, i);
        if (axis->len > 1) {
            width = (g_array_index(axis, double, 1) - g_array_index(axis, double, 0))
                * axis->len;
            out_array = g_array_append_val(out_array, width);
            gwy_debug("Start: %e, step: %e, count: %d, width: %e",
                      g_array_index(axis, double, 0),
                      g_array_index(axis, double, 1) - g_array_index(axis, double, 0),
                      axis->len,
                      width
                     );
        } else {
            g_array_free(out_array, TRUE);
            gwy_debug("Axis values count lesser than 2");
            return NULL;
        }
        /*
        start = g_array_index(axis, double, 0);
        end = g_array_index(axis, double, axis->len - 1);

        width = end - start;
        out_array = g_array_append_val(out_array, width);
        gwy_debug("Start: %e End: %e Width: %e\n", start, end, width);
        */
    }
    return out_array;

}

static GwyContainer *
spml_load(const gchar *filename)
{
    GwyContainer *object = NULL;
    gdouble *gwy_data;
    GwySIUnit *siunit;
    int i, j, *dimensions, scattered, channel_number = 0;
    GwyDataField *dfield = NULL;
    char *channel_name, *channel_key, *unit;
    GArray *axes, *axes_units, *names, *real_width_height;
    double x_dimen, y_dimen, *data = NULL;
    gint z_power10, x_power10, y_power10;
    GList *list_of_datagroups, *l, *sub_l, *list_of_channels;
    dataChannelGroup *dc_group;
    gchar *gwy_channel_location;
    int rotate = 90;

    gwy_debug("file = %s", filename);

    /* get list of datagroups and datachannels */
    list_of_datagroups = get_list_of_datachannels(filename);

    l = list_of_datagroups;
    while (l != NULL) {
        dc_group = (dataChannelGroup *) l->data;
        list_of_channels = sub_l = dc_group->datachannels;
        while (sub_l != NULL) {
            gwy_debug("freeing");

            channel_name = unit = NULL;
            data = NULL;
            dimensions = NULL;
            axes = axes_units = names = real_width_height = NULL;

            channel_name = sub_l->data;
            gwy_debug("Channelgroup: %s, channelname: %s", dc_group->name,
                      channel_name);
            if (channel_name
                /* get data and check it is 2 or more dimensional array */
                && get_data(FALSE, (char *)filename, channel_name, &data,
                            &dimensions, &unit, &scattered) >= 2 && data
                /* get axes and check we get 2 or more axes */
                && get_axis((char *)filename, channel_name, &axes, &axes_units,
                            &names) >= 2 && axes
                /* get width and height of 2D acording to axes */
                && (real_width_height = get_dimensions(axes)) != NULL
                && real_width_height->len >= 2) {

                x_dimen = g_array_index(real_width_height, double, 0);
                y_dimen = g_array_index(real_width_height, double, 1);

                /* parse unit and return power of 10 according to unit */
                siunit = gwy_si_unit_new_parse(g_array_index(axes_units,
                                                             xmlChar*, 0),
                                               &x_power10);
                g_object_unref(siunit);

                siunit = gwy_si_unit_new_parse(g_array_index(axes_units,
                                                             xmlChar*, 1),
                                               &y_power10);
                /* create and allocate datafield of given dimensions and given physical
                   dimensions */
                if (rotate == 90) {
                    dfield = gwy_data_field_new
                                   (dimensions[1], dimensions[0],
                                    y_dimen * pow10(y_power10),
                                    x_dimen * pow10(x_power10), FALSE);
                } else {
                dfield = gwy_data_field_new
                                   (dimensions[0], dimensions[1],
                                    x_dimen * pow10(x_power10),
                                    y_dimen * pow10(y_power10), FALSE);
                }
                gwy_debug("X real width: %f", x_dimen * pow10(x_power10));
                gwy_debug("X real_width_height: %f", x_dimen);
                gwy_data = gwy_data_field_get_data(dfield);
                /* copy raw array of doubles extracted from spml file to Gwyddion's
                   datafield
                   rotate -90 degrees: */
                if (rotate == 90) {
                    for (i = 0; i < dimensions[0]; i++) {
                        for (j = 0; j < dimensions[1]; j++) {
                            gwy_data[j+i*dimensions[1]] = data[i+j*dimensions[0]];
                        }
                    }
                } else {
                memcpy(gwy_data, data,
                       dimensions[0] * dimensions[1] * sizeof(double));
                }
                gwy_debug("Dimensions: %dx%d", dimensions[0], dimensions[1]);
                gwy_data_field_set_si_unit_xy(dfield, siunit);
                g_object_unref(siunit); /* unref siunit created before dfield */

                /* set unit for Z axis */
                siunit = gwy_si_unit_new_parse(unit, &z_power10);
                gwy_data_field_set_si_unit_z(dfield, siunit);
                g_object_unref(siunit);

                gwy_data_field_multiply(dfield, pow10(z_power10));

                /* set offset to match axes */
                siunit = gwy_si_unit_new_parse(g_array_index(axes_units, xmlChar*, 0), &z_power10);
                gwy_data_field_set_si_unit_z(dfield, siunit);
                g_object_unref(siunit);
                gwy_data_field_set_xoffset(dfield,
                    pow10(z_power10)*g_array_index(g_array_index(axes, GArray*, 0), double, 0) );

                siunit = gwy_si_unit_new_parse(g_array_index(axes_units, xmlChar*, 1), &z_power10);
                gwy_data_field_set_si_unit_z(dfield, siunit);
                g_object_unref(siunit);
                gwy_data_field_set_yoffset(dfield,
                    pow10(z_power10)*g_array_index(g_array_index(axes, GArray*, 1), double, 0) );

                if (object == NULL) {
                    /* create gwyddion container */
                    object = gwy_container_new();
                }
                /* put datachannel into container */
                gwy_channel_location =
                    g_strdup_printf("/%i/data", channel_number++);
                gwy_container_set_object_by_name(object, gwy_channel_location,
                                                 (GObject *)dfield);

                /* set name of datachannel to store in container */
                channel_key = g_strdup_printf("%s/title", gwy_channel_location);
                gwy_container_set_string_by_name(object, channel_key,
                                                 g_strdup(channel_name));
                g_free(channel_key);
                g_free(gwy_channel_location);
                g_object_unref(dfield);

            }
            /* Free possibly allocated memory */
            if (data)
                g_free(data);
            if (dimensions)
                g_free(dimensions);
            if (unit)
                g_free(unit);
            if (axes) {
                for (i = 0; i < axes->len; i++) {
                    g_array_free(g_array_index(axes, GArray *, i), TRUE);
                }
                g_array_free(axes, TRUE);
            }
            if (axes_units)
                g_array_free(axes_units, TRUE);
            if (names)
                g_array_free(names, TRUE);
            if (real_width_height)
                g_array_free(real_width_height, TRUE);
            g_free(sub_l->data);
            sub_l = g_list_next(sub_l);
        }
        /* free useless list of datachannels */
        g_list_free(list_of_channels);

        l = g_list_next(l);
        /* free useless name of datachannelgroup */
        g_free(dc_group->name);
    }
    /* free useless list of datachannelgroups */
    g_list_free(list_of_datagroups);

    return (GwyContainer *)object;
}

static gint
spml_detect(const GwyFileDetectInfo * fileinfo, gboolean only_name)
{
    gint score = 0;

    gwy_debug("%s: %s", fileinfo->name_lowercase, (only_name) ? "only_name" :
              "content");
    if (only_name) {
        if (g_str_has_suffix(fileinfo->name_lowercase, ".xml")) {
            score += 50;
            gwy_debug("Score += 50");
        }
    }
    else {
        if (fileinfo->head != NULL) {
            gwy_debug("head not null");
            if (strstr(fileinfo->head, "<SPML") != NULL) {
                gwy_debug("Score += 100");
                score += 100;
            }
        }
    }
    return score;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
