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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */
/* TODO: check/write allocation/deallocation !! */
//xmlTextReaderGetAttribute  !!!!! check leaks
#define DEBUG
// #define VERSION_1_15

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>


#include <libxml/xmlreader.h>
#include "spml-utils.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define EXTENSION ".xml"
//#define MAGIC "GWYO"
//#define MAGIC2 "GWYP"
//#define MAGIC_SIZE (sizeof(MAGIC)-1)
#ifdef VERSION_1_15
static gboolean      module_register            (const gchar *name);
static gint          spml_detect                (const gchar *filename,
                                                 gboolean only_name);
#else
static gboolean      module_register            (void);
static gint          spml_detect                (const GwyFileDetectInfo *fileinfo,
                                                 gboolean only_name);
#endif


static GwyContainer* spml_load               (const gchar *filename);
/*static gboolean      spml_save               (GwyContainer *data,
                                                 const gchar *filename);*/
/*static GObject*      gwy_container_deserialize2 (const guchar *buffer,
                                                 gsize size,
                                                 gsize *position);
*/


/* The module info for 1.15 version */
#ifdef VERSION_1_15
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "spml",
    N_("Loads and saves SPML data files."),
    "Jan Horak <xhorak@gmail.com>",
    "0.1.0",
    "Jan Horak",
    "2006",
};
#else
/* The module info for recent version */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Loads and saves SPML data files."),
    "Jan Horak <xhorak@gmail.com>",
    "0.1.0",
    "Jan Horak",
    "2006",
};
#endif

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

#ifdef VERSION_1_15
/* module_register for Gwyddion 1.15 */
static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo gwyfile_func_info = {
        "spml",
        N_("SPML: Scanning Probe Microscopy Markup Language (.xml)"),
        (GwyFileDetectFunc)&spml_detect,
        (GwyFileLoadFunc)&spml_load,
        NULL,
//        (GwyFileSaveFunc)&spml_save,
    };

    gwy_file_func_register(name, &gwyfile_func_info);

    return TRUE;
}
#else
/* module_register for recent Gwyddion version */
static gboolean
module_register(void)
{
  gwy_file_func_register ("spml",
                          N_("SPML: Scanning Probe Microscopy Markup Language (.xml)"),
                          (GwyFileDetectFunc)&spml_detect,
                          (GwyFileLoadFunc)&spml_load,
                          NULL,
                          NULL);
    return TRUE;
}
#endif


typedef enum {
    SKIP_STATE, IN_DATACHANNELS, IN_DATACHANNELGROUP, READ_COMPLETE
} datachannel_list_parser_states;

GList*
get_list_of_datachannels(const gchar* filename)
{
    const xmlChar *name; //, *value;
    datachannel_list_parser_states state = SKIP_STATE;
    int ret;
    GList* l = NULL;
    dataChannelGroup* data_channel_group = NULL;
    xmlTextReaderPtr reader;

    gwy_debug("filename = %s", filename);

    reader = xmlReaderForFile(filename, NULL, 0);

    if (reader != NULL) {
        ret = xmlTextReaderRead(reader);
        while (ret == 1) {
            // process node
            name = xmlTextReaderConstName(reader);
            switch (state) {
                case SKIP_STATE:
                    if ( xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT 
                         && strcmp(name, "DataChannels") == 0 ) {
                        // start of datachannels
                        gwy_debug("Switch to datachannels");
                        state = IN_DATACHANNELS;
                    }
                break;
                case IN_DATACHANNELS:
                    if ( xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT 
                         && strcmp(name, "DataChannelGroup") == 0 ) {
                        // datachannelgroup, get and set name of datachannelgroup
                        // data_channel_group is dynammicaly allocated,
                        // and it must be freed only when returned list is going to
                        // be disposed because GList does not create 
                        // copy of data_channel_group.
                        data_channel_group = g_malloc(sizeof(dataChannelGroup));
                        data_channel_group->name =
                            xmlTextReaderGetAttribute(reader, "name");
                        data_channel_group->datachannels = NULL;
                        gwy_debug("Switch to datachannelGroup '%s'", data_channel_group->name);
                        state = IN_DATACHANNELGROUP;
                    } else if ( xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT 
                        && strcmp(name, "DataChannels") == 0 ) {
                        // after datachannels, return possible?
                        gwy_debug("Datachannels end, read complete.");
                        state = READ_COMPLETE;
                    }
                break;
                case IN_DATACHANNELGROUP:
                    if ( xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT 
                         && strcmp(name, "DataChannel") == 0 
                         && data_channel_group != NULL) {
                        // datachannelgroup, get and set name of datachannelgroup
                        data_channel_group->datachannels = 
                            g_list_append(data_channel_group->datachannels, 
                                    xmlTextReaderGetAttribute(reader, "name"));
                        gwy_debug("Read info about datachannel.");
                        // stay in current state
                    } else if ( xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT 
                        && strcmp(name, "DataChannelGroup") == 0 
                        && data_channel_group != NULL) {
                        l = g_list_append(l, data_channel_group);
                        // end of channel group, save current data_channel_group to list
                        gwy_debug("End of datachannelgroup");
                        state = IN_DATACHANNELS;
                    }
                break;
                case READ_COMPLETE: // blank state
                break;
            }
            if (state == READ_COMPLETE) { // read is complete, we can leave reading cycle
                break;
            }
            // end process node
            ret = xmlTextReaderRead(reader);
        }
        xmlFreeTextReader(reader);
    } else {
        g_warning("SPML: Unable to open %s!", filename);
        return NULL;
    }
    return l;
}

int
decode_data(double **data, const xmlChar* input, dataFormat data_format, 
            codingTypes coding, byteOrder byte_order, 
            int max_input_len) 
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
            /// XXX: strlen() may not be nice there
            if (decode_b64((char*) input, &debase64_buf, strlen(input)) != 0) {
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
            /// XXX: strlen() may not be nice there
            if (decode_b64((char*) input, &data_stream, strlen(input)) != 0) {
                g_warning("Cannot decode data in BASE64 code.");
                if (data_stream != NULL) {
                    g_array_free(data_stream, TRUE);
                }
                *data = NULL;
                return 0;
            }
        break;
        case ASCII:
            p = (char*) input;
            data_stream = g_array_new(FALSE, FALSE, sizeof(double));
            while (p != NULL) {
                double num;
                if (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
                    p++;
                    continue;
                }
                num = strtod(p, &end_ptr);                
                if (num == 0 && end_ptr == p) {
                    g_warning("SPML: decode_data(): No conversion performed from ASCII string.");
                    break;
                }                
                g_array_append_val(data_stream, num);
                p = end_ptr;
                data_count++;
            }

        break;
        /// TODO:
        case HEX:
        case BINARY:
            g_warning("SPML: decode_data(): Data coding 'HEX' and 'BINARY' supported.");
        break;
        case UNKNOWN_CODING:
        break;
    }
    if (coding == ASCII) {
        // we have already decoded data
        if (max_input_len != -1 &&  data_count != max_input_len) {
            // not enough input data to fill array defined by length max_input_len
            g_warning("SPML: decode_data():\n"
                    "Input has not the same length as declared in dimensions\n"
                    "(max:%d vs read:%d). Has the channel attribute\n"
                    "'channelReadMethodName'? The channel may be  one\n"
                    "dimensional data used for axis values but not as\n" 
                    "a source of data for Gwyddion.", 
                    max_input_len, data_count);
            g_array_free(data_stream, TRUE);
            *data = NULL;
            return 0;
        } else {

            *data = (double*) data_stream->data;
            g_array_free(data_stream, FALSE);  // we can free dynamic array, but not
                                           // containing data.
            gwy_debug("Datacount: %d", data_count);
            return data_count;
        }
    }
    decoded_data = g_array_new(FALSE, FALSE, sizeof(double));
    p = data_stream->data;
    i = 0;
    switch (data_format) {
        case FLOAT32:
            while (i < data_stream->len ) {
                val = get_float32(&p, byte_order);
                g_array_append_val(decoded_data, val);
                data_count++;
                i += sizeof(float);
            }
            break;
        case FLOAT64:
            while (i < data_stream->len ) {
                val = get_float64(&p, byte_order);
                g_array_append_val(decoded_data, val);
                i += sizeof(double);
            }
            break;
        case INT8:
            while (i < data_stream->len ) {
                val = get_int8(&p);
                g_array_append_val(decoded_data, val);
                i += sizeof(gint8);
            }
            break;
        case INT16:
            while (i < data_stream->len ) {
                val = get_int16(&p, byte_order);
                g_array_append_val(decoded_data, val);
                i += sizeof(gint16);
            }
            break;
        case INT32:
            while (i < data_stream->len ) {
                val = get_int32(&p, byte_order);
                g_array_append_val(decoded_data, val);
                i += sizeof(gint32);
            }
            break;
        case UINT32:
            while (i < data_stream->len ) {
                val = get_uint32(&p, byte_order);
                g_array_append_val(decoded_data, val);
                i += sizeof(guint32);
            }
            break;
        case UINT8:
            while (i < data_stream->len ) {
                val = get_uint8(&p);
                g_array_append_val(decoded_data, val);
                i += sizeof(guint8);
            }
            break;
        case UINT16:
            while (i < data_stream->len ) {
                val = get_uint16(&p, byte_order);
                g_array_append_val(decoded_data, val);
                i += sizeof(guint16);
            }
            break;
        case STRING:
            g_warning("SPML: decode_data(): Data format 'String' not supported.");
            break;
        case UNKNOWN_DATAFORMAT:
            g_warning("SPML: decode_data(): Unknown dataformat.");
            break;
    }
    g_array_free(data_stream, TRUE);
    data_count = decoded_data->len;
    if (max_input_len != -1 &&  data_count != max_input_len) {
        g_warning("SPML: decode_data():\n"
                  "Input has not the same length as declared in dimensions\n"
                  "(max:%d vs read:%d). Has the channel attribute\n"
                  "'channelReadMethodName'? The channel may be  one\n"
                  "dimensional data used for axis values but not as\n" 
                  "a source of data for Gwyddion.", 
                  max_input_len, data_count);
        g_array_free(decoded_data, TRUE);
        *data = NULL;
        return 0;
    }
    *data = (double*) decoded_data->data;
    g_array_free(decoded_data, FALSE); // we can free dynamic array, but not
                                       // containing data.
    gwy_debug("Datacount: %d", data_count);
    return data_count;

}

int
get_axis();

int 
get_data(gboolean read_data_only, char* filename, char *datachannel_name, 
         double *data[], int *dimensions[], char **unit, gboolean *scattered) 
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

    gwy_debug("read only: %d,  channel name==%s", read_data_only, datachannel_name);
    *dimensions = NULL;

    reader = xmlReaderForFile(filename, NULL, 0);

    if (reader != NULL) {
        ret = xmlTextReaderRead(reader);
        while (ret == 1) {
            // process node
            name = xmlTextReaderConstName(reader);
            if (name == NULL) {
                // no datachannels
                return -1;
            }
            if ( state == 0 
                 && xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT 
                 && strcmp(name, "DataChannel") == 0) {
                id = xmlTextReaderGetAttribute(reader, "name");
                if (id && strcmp(id, datachannel_name) == 0) {
                    format = get_data_format(xmlTextReaderGetAttribute(reader, "dataFormat"));
                    coding = get_data_coding(xmlTextReaderGetAttribute(reader, "coding"));
                    byte_order = get_byteorder(xmlTextReaderGetAttribute(reader, "byteOrder"));
                    if (read_data_only == FALSE) {
                        // when read_data_only is true, we skip dimensions array to prevent 
                        // infinite cycle when calling from get_axis to get non equidistant axis from
                        // datachannel
                        read_method_name = xmlTextReaderGetAttribute(reader, "channelReadMethodName");
                        if (read_method_name == NULL) {
                            // sequence of one dimensional values
                            *dimensions = NULL;
                        } else {
                            g_free((char*)read_method_name);
                            // get axes
                            data_dimension = get_axis( filename, datachannel_name, &axes, &names, &units);
                            if (data_dimension != 2) {
                                g_warning("SPML: get_data(): Input data are in %d dimension(s).", data_dimension);
                            }
                            if (data_dimension > 0) {
                                *dimensions = g_malloc(sizeof(int)*data_dimension);
                                for (i = 0; i < data_dimension; i++) {
                                    axis = g_array_index(axes, GArray*, i);
                                    (*dimensions)[i] = axis[0].len;
                                    max_input_len *= axis[0].len;
                                }
                            } else {
                                *dimensions = NULL;
                            }
                        }
                    } else {
                        // in the first member of dimension array store number of elements
                        max_input_len = -1; // we don't know the max_input_len
                    }

                    *unit = xmlTextReaderGetAttribute(reader, "unit");

                    gwy_debug("Datachannel summary: format %d, coding %d, order %d.",
                              format, coding, byte_order);
                    state = 1;
                }
                if (id) {
                    g_free(id);
                }
            } else if (state == 1) { 
                value = xmlTextReaderConstValue(reader);
                if (value == NULL) {
                    g_warning("SPML: get_data(): No data available for datachannel '%s'", 
                              datachannel_name);
                    *data = NULL;
                    break;
                }
                out_len = decode_data(data,  value, format, coding, byte_order, max_input_len);
                if (out_len == 0 || *data == NULL) {
                    g_warning("SPML: get_data(): No input data available for"
                              " datachannel '%s'.", datachannel_name);

                } else if (read_data_only == TRUE) {
                    *dimensions = g_malloc(sizeof(int));
                    (*dimensions)[0] = out_len;
                    data_dimension = 1;
                }
                break;
            }
            // end process node
            ret = xmlTextReaderRead(reader);
        } // end while (ret == 1)
        xmlFreeTextReader(reader);
    } else {
        g_warning("Unable to open %s.", filename);
        return -1;
    }
    return data_dimension;
}

/** 
 * Find node of given name in parent's children
 * @param parent node
 * @return NULL when children by given name not found otherwise
 * return pointer to first children of specified name
 */ 
xmlNodePtr
get_node_ptr(xmlNodePtr parent,  xmlChar* name) 
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
xmlNodePtr
get_next_node_ptr(xmlNodePtr prev,  xmlChar* name) 
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

xmlChar*
get_attribute_value_of_named_node(xmlNodePtr node, xmlChar* datachannel_name, xmlChar *attr_name) 
{
    xmlChar* tmp_ch;

    gwy_debug("datachannel name = %s, attr name = %s", datachannel_name, attr_name );
    tmp_ch = xmlGetProp(node, (const xmlChar *)"name");
    if (tmp_ch) {
        if (!xmlStrcmp(datachannel_name, tmp_ch) ) {
            // found Datachannel of given name
            g_free(tmp_ch);
            return xmlGetProp(node, attr_name);
        } 
        g_free(tmp_ch);
    }
    return NULL;
}

GArray* 
get_axis_datapoints(char* filename, xmlNodePtr axis_node)
{
    int j;
    GArray* axes_values;
    xmlChar* ch;
    int num_of_dimensions;
    float step, start, size;
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
        // points are saved in datachannel
        // TODO: load points from datachannel
        num_of_dimensions = get_data(TRUE, filename, ch, &data, &dimensions, &unit, &scattered);
        if (num_of_dimensions == 1) {
            axes_values = g_array_new(TRUE, FALSE, sizeof(double));
            axes_values = g_array_append_vals(axes_values, data, dimensions[0]);
            return axes_values;
        }
        g_warning("SPML: Loading scattered data.");
        return NULL;
    } else {
        // points will be computed from start step and size attribute
        ch = xmlGetProp(axis_node, "start");
        if (ch) {
            start = atof(ch);
            g_free(ch);
        } else return NULL;
        ch = xmlGetProp(axis_node, "step");
        if (ch) {
            step = atof(ch);
            g_free(ch);
        } else return NULL;
        ch = xmlGetProp(axis_node, "size");
        if (ch) {
            size = atof(ch);
            g_free(ch);
        } else return NULL;
        axes_values = g_array_new(TRUE, FALSE, sizeof(double));
        for (j = 0; j < size; j++) {
            value = j*step + start;
            axes_values = g_array_append_val(axes_values, value);
        }
        return axes_values;
    }
    return NULL;
}


int
get_axis(char* filename, char* datachannel_name, GArray** axes, GArray** units, GArray** names) 
{
/*    <DataChannel> => channelReadMethodName =>readMethod=>ReadAxis
        name=> Axis =>array_of_values unit, nr of elements.
*/
    int i;
    int axis_count = 0;
    xmlDocPtr doc;
    xmlNodePtr cur, sub_node, axes_node, datachannels_node;
    xmlChar* channelReadMethodName = NULL;
    xmlChar* tmp_ch, *tmp_ch2, *read_method_name;
    GArray* read_method_axes = g_array_new(TRUE , FALSE , sizeof(xmlChar*));
    GArray* axis_values = NULL;

    gwy_debug("datachannel name == %s", datachannel_name);
    doc = xmlParseFile(filename);
    if (doc == NULL ) {
        g_warning("SPML: get_axis(): Input file was not parsed successfully.");
        *axes = NULL; *units = NULL; *names = NULL;
        g_array_free(read_method_axes, TRUE);
        return 0;
    }
    // get pointer to Axes and DataChannels elements
    cur = xmlDocGetRootElement(doc);
    axes_node = get_node_ptr(cur, "Axes");
    datachannels_node = get_node_ptr(cur, "DataChannels");

    if (axes_node == NULL || datachannels_node == NULL) {
        // incomplete xml file
        g_warning("SPML: get_axis(): incomplete file, missing Axes or Datachannels tags.");
        xmlFreeDoc(doc);
        *axes = NULL; *units = NULL; *names = NULL;
        return 0;
    }

    // get channelReadMethodName    
    cur = get_node_ptr(datachannels_node, "DataChannelGroup");  
    while (cur != NULL) {
        sub_node = get_node_ptr(cur, "DataChannel");
        while (sub_node != NULL) {
            channelReadMethodName = 
                get_attribute_value_of_named_node(sub_node, datachannel_name, 
                                                  "channelReadMethodName");
            if (channelReadMethodName) {
                // channelReadMethodName found, leave searching
                break;

            }
            sub_node = get_next_node_ptr(sub_node, "DataChannel");
        }
        if (channelReadMethodName) {
            // channelReadMethodName found, leave searching 
            break;
        }
        cur = get_next_node_ptr(cur, "DataChannelGroup");
    }

    if (channelReadMethodName == NULL) {
        g_warning("SPML: get_axis(): Datachannel '%s' not found.", datachannel_name);
        xmlFreeDoc(doc);
        *axes = NULL; *units = NULL; *names = NULL;
        g_array_free(read_method_axes, TRUE);
        return 0;
    }

    // get readMethod from DataChannels
    cur = get_node_ptr(datachannels_node, "ReadMethod");
    while (cur != NULL) {
        tmp_ch = xmlGetProp(cur, "name");
        if (tmp_ch) {
            // found ReadMethod according to selected datachannel_name
            if (!xmlStrcmp(tmp_ch, channelReadMethodName)) { 
                sub_node = get_node_ptr(cur, "ReadAxis");
                while (sub_node != NULL) {
                    read_method_name = xmlGetProp(sub_node, "name");
                    if (read_method_name) {
                        read_method_axes = 
                            g_array_append_val(read_method_axes, read_method_name);
                    }
                    sub_node = get_next_node_ptr(sub_node, "ReadAxis");
                }
            }
            g_free(tmp_ch);
        }

        cur = get_next_node_ptr(cur, "ReadMethod");
    }
    if ( g_array_index(read_method_axes, xmlChar*, 0) == NULL) {
        // ReadMethod mentioned in selected DataChannel not found in ReadMethod section
        g_warning("SPML: get_axis(): ReadMethod '%s' for datachannel '%s' not found.", 
                  channelReadMethodName, datachannel_name);
        xmlFreeDoc(doc);
        *axes = NULL; *units = NULL; *names = NULL;
        g_array_free(read_method_axes, TRUE);
        g_free(channelReadMethodName);
        return 0;
    }

    // We have name of axes for given datachannel_name in GArray read_method_axes.
    // Time to load axes and fill output arrays.
    *names = g_array_new(TRUE, FALSE, sizeof(xmlChar *) );
    *units = g_array_new(TRUE, FALSE, sizeof(xmlChar *) );
    *axes = g_array_new(FALSE, FALSE, sizeof(GArray*) );
    cur = get_node_ptr(axes_node, "AxisGroup");
    while (cur != NULL) {
        sub_node = get_node_ptr(cur, "Axis");
        while (sub_node != NULL) {
            tmp_ch2 = xmlGetProp(sub_node, "name");
            if (tmp_ch2) {
                for (i = 0; (read_method_name = g_array_index(read_method_axes, xmlChar*, i)) != NULL; i++) {
                    if ( !xmlStrcmp(read_method_name, tmp_ch2) ) {
                        // we found axis we are searching for
                        // add name to *names array
                        tmp_ch = xmlGetProp(sub_node, "name");
                        g_array_append_val(*names, tmp_ch);
                        // add units to *units array
                        tmp_ch = xmlGetProp(sub_node, "unit");
                        if (tmp_ch != NULL) {
                            g_array_append_val(*units, tmp_ch);
                        } else {
                            g_warning("SPML: get_axis(): unknown unit for axis.");
                            tmp_ch = g_malloc(4);
                            sprintf(tmp_ch, "N/A");
                            g_array_append_val(*units, tmp_ch);
                        }
                        // get axis values
                        //
                        axis_values = get_axis_datapoints(filename, sub_node);
                        if (axis_values != NULL) {
                            gwy_debug("Axis len: %d.", axis_values->len);
                            g_array_append_val(*axes, axis_values);
                            axis_count++;
                        } else {
                            g_warning("SPML: get_axis(): Cannot compute or read axis data.");
                            g_array_free(*axes, TRUE);
                            g_array_free(*units, TRUE);
                            g_array_free(*names, TRUE);
                            *axes = NULL; *units = NULL; *names = NULL;
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
    xmlFreeDoc(doc);
    g_free(channelReadMethodName);
    return axis_count;
}

// TODO not working
/*gint
signal_handler_event(GtkWidget *widget, GdkEvent *event, gpointer func_data)
{
    if (GTK_IS_BUTTON(widget)
        && (event->type==GDK_2BUTTON_PRESS 
            || event->type==GDK_3BUTTON_PRESS) 
        ) {
        printf("I feel %s clicked with button %d\n",
        event->type==GDK_2BUTTON_PRESS ? "double" : "triple",
        event->button);
        gtk_dialog_response(GTK_DIALOG(func_data), GTK_RESPONSE_OK);
    }
    return FALSE;
}
*/
char*
query_for_datachannel_name(char* filename) 
{
  GList *l, *first_l, *sub_l, *old_sub_l, *first_sub_l;
  dataChannelGroup* dc_group;
  gchar* name;
  
  GtkWidget *dialog_channellist;
  GtkWidget *dialog_vbox1;
  GtkWidget *hbox1;
  GtkWidget *fr_list;
  GtkWidget *alignment1;
  GtkWidget *scrolledwindow1;
  GtkWidget *list_datachannels;
  GtkWidget *l_datachan_list;
  GtkWidget *fr_preview;
  GtkWidget *alignment2;
  GtkWidget *l_preview;
  GtkWidget *dialog_action_area1;

  GtkListStore *store;
  GtkTreeIter iter;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  gint response;

  gwy_debug("file = %s", filename);
  l = get_list_of_datachannels(filename);
  if (!l) {
     g_warning("Cannot find any datachannel in '%s'", filename);
     return NULL;
  }

  dialog_channellist = gtk_dialog_new_with_buttons("Select datachannel to import",
                                                   NULL, GTK_DIALOG_MODAL, 
                                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                   GTK_STOCK_OK, GTK_RESPONSE_OK,
                                                   NULL);
  gtk_widget_set_size_request(GTK_WIDGET(dialog_channellist), 550, 300);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_channellist), GTK_RESPONSE_OK);


  dialog_vbox1 = GTK_DIALOG (dialog_channellist)->vbox;
  gtk_widget_show (dialog_vbox1);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), hbox1, TRUE, TRUE, 0);

  fr_list = gtk_frame_new (NULL);
  gtk_widget_show (fr_list);
  gtk_box_pack_start (GTK_BOX (hbox1), fr_list, TRUE, TRUE, 0);
  gtk_frame_set_shadow_type (GTK_FRAME (fr_list), GTK_SHADOW_NONE);

  alignment1 = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (alignment1);
  gtk_container_add (GTK_CONTAINER (fr_list), alignment1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment1), 0, 0, 12, 0);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow1);
  gtk_container_add (GTK_CONTAINER (alignment1), scrolledwindow1);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_SHADOW_IN);

  // fill list_datachannels
  store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
  first_l = l;
  while (l != NULL) {
      dc_group = (dataChannelGroup*) l->data;
      sub_l = dc_group->datachannels;
      first_sub_l = sub_l;
      while (sub_l != NULL) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, sub_l->data, 1, 
                           dc_group->name, -1);
        
        old_sub_l = sub_l;
        sub_l = g_list_next(sub_l);
        // free useless name of datachannel
        g_free(old_sub_l->data);
      }
      // free useless list of datachannels
      g_list_free(first_sub_l);

      l = g_list_next(l);
      // free useless name of datachannelgroup
      g_free(dc_group->name);
  }
  // free useless list of datachannelgroups
  g_list_free(first_l);

  list_datachannels = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Channel name", renderer, 
                                                     "text", 0,
                                                     NULL);
  
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_datachannels), column);
  column = gtk_tree_view_column_new_with_attributes ("Channel group", renderer, 
                                                     "text", 1,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_datachannels), column);
  
  gtk_widget_show (list_datachannels);
/*  g_signal_connect(G_OBJECT(list_datachannels), "GtkWidget::event",
                   G_CALLBACK(signal_handler_event), dialog_channellist);*/
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), list_datachannels);

  l_datachan_list = gtk_label_new ("<b>Datachannels list</b>");
  gtk_widget_show (l_datachan_list);
  gtk_frame_set_label_widget (GTK_FRAME (fr_list), l_datachan_list);
  gtk_label_set_use_markup (GTK_LABEL (l_datachan_list), TRUE);

  fr_preview = gtk_frame_new (NULL);
  gtk_widget_show (fr_preview);
  gtk_box_pack_start (GTK_BOX (hbox1), fr_preview, TRUE, TRUE, 0);
  gtk_frame_set_shadow_type (GTK_FRAME (fr_preview), GTK_SHADOW_NONE);

  alignment2 = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (alignment2);
  gtk_container_add (GTK_CONTAINER (fr_preview), alignment2);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment2), 0, 0, 12, 0);

  l_preview = gtk_label_new ("<b>Preview</b>");
  gtk_widget_show (l_preview);
  gtk_frame_set_label_widget (GTK_FRAME (fr_preview), l_preview);
  gtk_label_set_use_markup (GTK_LABEL (l_preview), TRUE);

  dialog_action_area1 = GTK_DIALOG (dialog_channellist)->action_area;
  gtk_widget_show (dialog_action_area1);

  response = gtk_dialog_run(GTK_DIALOG(dialog_channellist));

  switch (response) {
    case GTK_RESPONSE_OK:
      if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(list_datachannels)), 
                                          &store, &iter)) {
            gtk_tree_model_get (GTK_TREE_MODEL(store), &iter, 0, &name, -1);
            gtk_widget_destroy(dialog_channellist);
            return name;
      } else {
          gtk_widget_destroy(dialog_channellist);
          return NULL;
      }
      break;
    default: 
      gtk_widget_destroy(dialog_channellist);
      return NULL;
  }

}

double
unit_to_meters(char* unit) 
{
    gwy_debug("unit == %s", unit);
    if (strcmp(unit, "nm") == 0 ) {
        return 1e-9;
    } else if (strcmp(unit, "m") == 0 ) {
        return 1;
    } else if (strcmp(unit, "mm") == 0 ) {
        return 1e-3;
    } else if (strcmp(unit, "um") == 0 ) {
        return 1e-6;
    } else if (strcmp(unit, "pm") == 0 ) {
        return 1e-12;
    } else {
        g_warning("SPML: Unknown unit. Use m, mm, um or pm for meters, "
                  "milimeters, micrometers and picometers.");
    } 
    return 1;
}

GArray*
get_dimensions(GArray* axes) 
{
    unsigned int i;
    double start, end, width;
    GArray *axis, *out_array;

    gwy_debug("start");
    if (axes == NULL)
        return NULL;

    out_array = g_array_new(FALSE, FALSE, sizeof(double));
    for (i = 0; i < axes->len; i++) {
        axis = g_array_index(axes, GArray*, i);
        start = g_array_index(axis, double, 0);
        end = g_array_index(axis, double, axis->len-1);
        width = end - start;
        out_array = g_array_append_val(out_array, width);
        gwy_debug("Start: %e End: %e Width: %e\n", start, end, width);
    }
    return out_array;

}

void get_min_max_z(double *data, int len,  double *min_z, double *max_z)
{
    int i;

    if (data == NULL) {
        *max_z = 0;
        *min_z = 0;
        return;
    }
    *max_z = data[0];
    *min_z = data[0];
    for (i = 0; i < len; i++) {
        if (data[i] > *max_z) {
            *max_z = data[i];
        } else if (data[i] < *min_z) {
            *min_z = data[i];
        }
    }
    return;

}

static GwyContainer*
spml_load(const gchar *filename)
{
    int num_of_data_dimension;
    GwyContainer *object;
    double max_z, min_z;
    double *data;
    gdouble *gwy_data;
    GwySIUnit *siunit;
    int *dimensions;
    char *unit;
    int scattered;
    GwyDataField *dfield = NULL;
    char* channel_name;
    int num_of_axes;
    GArray *axes, *units, *names;
    double x_dimen, y_dimen;
    gint power10;
    GArray* real_width_height;

    gwy_debug("file = %s", filename);

    // get channel name to import
    channel_name = query_for_datachannel_name((char*) filename);

    if (channel_name == NULL) {
        g_warning("SPML: No datachannel selected.");
        return NULL;
    }
    // get data of chosen datachannel
    num_of_data_dimension = 
        get_data(FALSE, (char*)filename, channel_name, &data, &dimensions, &unit, &scattered);
    if (num_of_data_dimension < 2 || !data) {
        // datachannel has not 2D data, we can't import
        // clean and return null
        g_free(channel_name);
        if (data)
            g_free(data);
        if (dimensions)
            g_free(dimensions);
        if (unit)
            g_free(unit);
        g_warning("SPML: no data available!");
        return NULL;
    }
    // get axes of chosen datachannel
    num_of_axes = 
        get_axis((char*)filename, channel_name, &axes, &units, &names);
    if (num_of_axes < 2) {
        // not enough axes found for selected datachannel, we cannot
        // count real width and height of datachannel
        // clean and return Null
        g_free(channel_name); g_free(data); g_free(dimensions); g_free(unit);
        if (axes) 
            g_array_free(axes, TRUE); //TODO: free subGarrays too!!!
        if (units)
            g_array_free(units, TRUE);
        if (names)
            g_array_free(names, TRUE);
        return NULL;
    }
    // get real width and height for datachannel's axes
    real_width_height = get_dimensions(axes);
    if (!real_width_height || real_width_height->len < 2) {
        // we don't have real width and height for 2D datachannel
        // clean and return null
        g_free(channel_name); g_free(data); g_free(dimensions); g_free(unit);
        g_array_free(axes, TRUE); //TODO: free subGArrays too!!!
        g_array_free(units, TRUE);
        g_array_free(names, TRUE);
        if (real_width_height)
           g_array_free(real_width_height, TRUE);
        return NULL;
    }
    x_dimen = g_array_index(real_width_height, double, 0);
    y_dimen = g_array_index(real_width_height, double, 1);

    dfield = GWY_DATA_FIELD(gwy_data_field_new(dimensions[0], 
                                               dimensions[1], 
                                               x_dimen*unit_to_meters(g_array_index(units, xmlChar*, 0)), 
                                               y_dimen*unit_to_meters(g_array_index(units, xmlChar*, 1)), 
                                               FALSE));
    gwy_data = gwy_data_field_get_data(dfield);

    memcpy(gwy_data, data, dimensions[0]*dimensions[1]*sizeof(double));
    get_min_max_z(data, dimensions[0]*dimensions[1], &min_z, &max_z);
    g_free(data);
    g_free(dimensions);

    siunit = GWY_SI_UNIT(gwy_si_unit_new(g_array_index(units,char *,  0)));
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new_parse(unit, &power10);
//    siunit = GWY_SI_UNIT(gwy_si_unit_new("m"));
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);
    gwy_data_field_multiply(dfield, pow10(power10));

    object = GWY_CONTAINER(gwy_container_new());
    gwy_container_set_object_by_name(object, "/0/data",
                                     (GObject*)dfield);
    
    g_object_unref(dfield);

    g_free(channel_name);
    g_free(unit);
    return (GwyContainer*)object;
}

#define HEAD_SIZE 500
#ifdef VERSION_1_15
static gint
spml_detect(const gchar  *filename, gboolean only_name)
{
    FILE *fh;
    gchar head[HEAD_SIZE];
    gint score = 0;

    gwy_debug("%s", filename);
    if (g_str_has_suffix (filename, ".xml")) {
        score += 50;
        gwy_debug("Score += 50");
    }
    if (only_name) 
        return score;
    if (!(fh = fopen (filename, "rb")))
        return 0;
    fread(head, 1, HEAD_SIZE, fh);
    if (strstr("<SPML", head)) {
        gwy_debug("Score += 50");
        score +=50;
    }
    fclose(fh);
    return score;
}
#else
static gint
spml_detect(const GwyFileDetectInfo  *fileinfo, gboolean only_name)
{
    gint score = 0;

    gwy_debug("%s", fileinfo->name_lowercase);
    if (g_str_has_suffix (fileinfo->name_lowercase, ".xml")) {
        score += 50;
        gwy_debug("Score += 50");
    }
    if (only_name) 
        return score;
    if (strstr("<SPML", fileinfo->head)) {
        gwy_debug("Score += 50");
        score +=50;
    }
    return score;
}
#endif
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
