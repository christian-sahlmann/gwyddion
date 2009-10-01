/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nanoscan-spm">
 *   <comment>NanoScan SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="&lt;?xml">
 *       <match type="string" offset="40:120" value="xmlns=&quot;http://www.swissprobe.com/SPM&quot;"/>
 *     </match>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Nanoscan XML
 * .xml
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC1 "<?xml"
#define MAGIC1_SIZE (sizeof(MAGIC1)-1)
#define MAGIC2 "<scan"
#define MAGIC3 "xmlns=\"http://www.swissprobe.com/SPM\""

#define EXTENSION ".xml"

#ifdef HAVE_MEMRCHR
#define strlenrchr(s,c,len) (gchar*)memrchr((s),(c),(len))
#else
#define strlenrchr(s,c,len) strchr((s),(c))
#endif

#if GLIB_CHECK_VERSION(2, 12, 0)
#define TREAT_CDATA_AS_TEXT G_MARKUP_TREAT_CDATA_AS_TEXT
#else
#define TREAT_CDATA_AS_TEXT 0
#endif

#define SCAN_PREFIX "/scan/vector/contents"
#define SCAN_PREFIX_SIZE (sizeof(SCAN_PREFIX)-1)

#define RES_PREFIX "/scan/vector/contents/size/contents"
#define RES_PREFIX_SIZE (sizeof(RES_PREFIX)-1)

#define DIMS_PREFIX "/scan/vector/contents/area/contents"
#define DIMS_PREFIX_SIZE (sizeof(DIMS_PREFIX)-1)

#define AXIS_PREFIX "/scan/vector/contents/axis/vector/contents"
#define AXIS_PREFIX_SIZE (sizeof(AXIS_PREFIX)-1)

#define DATA_PREFIX "/scan/vector/contents/direction/vector/contents"
#define DATA_PREFIX_SIZE (sizeof(DATA_PREFIX)-1)

#define CHANNEL_PREFIX "/scan/vector/contents/direction/vector/contents/channel/vector/contents"
#define CHANNEL_PREFIX_SIZE (sizeof(CHANNEL_PREFIX)-1)

#define META_PREFIX "/scan/vector/contents/instrumental_parameters/contents"
#define META_PREFIX_SIZE (sizeof(META_PREFIX)-1)

typedef enum {
    SCAN_UNKNOWN = 0,
    SCAN_FORWARD = 1,
    SCAN_BACKWARD = -1,
} NanoScanDirection;

typedef struct {
    gchar *name;
    gchar *value;
    gchar *units;
} NanoScanMeta;

typedef struct {
    gchar *name;
    gchar *zunits;
    gfloat *data;
    NanoScanDirection direction;
    gboolean already_added;
} NanoScanChannel;

typedef struct {
    gchar *name;
    gchar *units;
    gchar *display_units;
    gdouble display_scale;
    /* FIXME: These can be vectors in the energy scan mode! */
    gdouble start;
    gdouble stop;
} NanoScanAxis;

typedef struct {
    GString *path;
    gchar *xyunits;
    gint xres;
    gint yres;
    gdouble xreal;
    gdouble yreal;
    NanoScanDirection current_direction;
    GArray *axes;
    GArray *channels;
    GArray *meta;
} NanoScanFile;

static gboolean      module_register     (void);
static gint          nanoscan_detect     (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* nanoscan_load       (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static void          nanoscan_free       (NanoScanFile *nfile);
static void          add_channel         (GwyContainer *container,
                                          NanoScanFile *nfile,
                                          NanoScanChannel *channel,
                                          gint id);
static void          add_graph           (GwyContainer *container,
                                          NanoScanFile *nfile,
                                          NanoScanChannel *channel,
                                          gint id);
static void          add_curve_model     (NanoScanFile *nfile,
                                          NanoScanChannel *channel,
                                          guint i,
                                          GwyGraphModel *gmodel);
static void          add_multigraph      (GwyContainer *container,
                                          NanoScanFile *nfile,
                                          NanoScanChannel *channel,
                                          gint id);
static void          add_multicurve_model(NanoScanFile *nfile,
                                          NanoScanChannel *channel,
                                          guint i,
                                          GwyGraphModel *gmodel);
static void          fix_metadata        (NanoScanFile *nfile);
static void          add_metadata        (GwyContainer *container,
                                          NanoScanFile *nfile,
                                          gint id);
static void          start_element       (GMarkupParseContext *context,
                                          const gchar *element_name,
                                          const gchar **attribute_names,
                                          const gchar **attribute_values,
                                          gpointer user_data,
                                          GError **error);
static void          end_element         (GMarkupParseContext *context,
                                          const gchar *element_name,
                                          gpointer user_data,
                                          GError **error);
static void          text                (GMarkupParseContext *context,
                                          const gchar *text,
                                          gsize text_len,
                                          gpointer user_data,
                                          GError **error);
static void          add_meta            (NanoScanFile *nfile,
                                          const gchar *name,
                                          const gchar *value);
static gfloat*       read_channel_data   (const gchar *value,
                                          gsize value_len,
                                          guint npixels,
                                          GError **error);
static gsize         decode_base64       (const guchar *buf,
                                          gsize len,
                                          guchar *out);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports NanoScan XML files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanoscan",
                           N_("NanoScan XML files (.xml)"),
                           (GwyFileDetectFunc)&nanoscan_detect,
                           (GwyFileLoadFunc)&nanoscan_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gboolean
check_magic(const gchar *header)
{
    return (memcmp(header, MAGIC1, MAGIC1_SIZE) == 0
            && strstr(header, MAGIC2) != NULL
            && strstr(header, MAGIC3) != NULL);
}

static gint
nanoscan_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    return check_magic(fileinfo->head) ? 100 : 0;
}

static GwyContainer*
nanoscan_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    NanoScanFile nfile;
    gchar *head, *buffer = NULL;
    GMarkupParser parser = { start_element, end_element, text, NULL, NULL };
    GMarkupParseContext *context;
    gsize size;
    GError *err = NULL;
    guint i, id;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    gwy_clear(&nfile, 1);
    head = g_strndup(buffer, 1024);
    if (!check_magic(head)) {
        err_FILE_TYPE(error, "NanoScan XML");
        g_free(head);
        goto fail;
    }
    g_free(head);

    /* Parse the XML */
    nfile.path = g_string_new(NULL);
    nfile.channels = g_array_new(FALSE, FALSE, sizeof(NanoScanChannel));
    nfile.axes = g_array_new(FALSE, FALSE, sizeof(NanoScanAxis));
    nfile.meta = g_array_new(FALSE, FALSE, sizeof(NanoScanMeta));
    context = g_markup_parse_context_new(&parser,
                                         G_MARKUP_PREFIX_ERROR_POSITION
                                         | TREAT_CDATA_AS_TEXT,
                                         &nfile, NULL);
    if (!g_markup_parse_context_parse(context, buffer, size, &err)
        || !g_markup_parse_context_end_parse(context, &err)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("XML parsing failed: %s"), err->message);
        g_clear_error(&err);
        g_markup_parse_context_free(context);
        goto fail;
    }
    g_markup_parse_context_free(context);

    if (err_DIMENSION(error, nfile.xres) || err_DIMENSION(error, nfile.yres))
        goto fail;

    /* Depending on whether we have image or other data, some might be unset. */
    if (!((nfile.xreal = fabs(nfile.xreal)) > 0))
        nfile.xreal = nfile.xres;
    if (!((nfile.yreal = fabs(nfile.yreal)) > 0))
        nfile.yreal = nfile.yres;
    for (i = 0; i < nfile.axes->len; i++) {
        NanoScanAxis *axis = &g_array_index(nfile.axes, NanoScanAxis, i);
        if (axis->stop == axis->start) {
            axis->stop = 1.0;
            axis->start = 0.0;
        }
        if (!axis->display_scale)
            axis->display_scale = 1.0;
    }

    /* Construct a GwyContainer */
    container = gwy_container_new();
    fix_metadata(&nfile);
    for (i = id = 0; i < nfile.channels->len; i++) {
        NanoScanChannel *channel = &g_array_index(nfile.channels,
                                                  NanoScanChannel, i);
        if (!channel->data || channel->already_added)
            continue;

        if (nfile.yres == 1 && nfile.axes->len >= 1)
            add_graph(container, &nfile, channel, id+1);
        else if (nfile.axes->len >= 2)
            add_multigraph(container, &nfile, channel, id);
        else {
            add_channel(container, &nfile, channel, id);
            add_metadata(container, &nfile, id);
        }

        id++;
    }

    if (!id) {
        err_NO_DATA(error);
        gwy_object_unref(container);
    }

fail:
    g_free(buffer);
    nanoscan_free(&nfile);

    return container;
}

static void
nanoscan_free(NanoScanFile *nfile)
{
    guint i;

    g_free(nfile->xyunits);
    if (nfile->path)
        g_string_free(nfile->path, TRUE);

    if (nfile->axes) {
        for (i = 0; i < nfile->axes->len; i++) {
            NanoScanAxis *axis = &g_array_index(nfile->axes, NanoScanAxis, i);
            g_free(axis->name);
            g_free(axis->units);
        }
        g_array_free(nfile->axes, TRUE);
    }
    if (nfile->channels) {
        for (i = 0; i < nfile->channels->len; i++) {
            NanoScanChannel *channel = &g_array_index(nfile->channels,
                                                      NanoScanChannel, i);
            g_free(channel->name);
            g_free(channel->zunits);
            g_free(channel->data);
        }
        g_array_free(nfile->channels, TRUE);
    }
    if (nfile->meta) {
        for (i = 0; i < nfile->channels->len; i++) {
            NanoScanMeta *meta = &g_array_index(nfile->meta, NanoScanMeta, i);
            g_free(meta->name);
            g_free(meta->value);
            g_free(meta->units);
        }
        g_array_free(nfile->meta, TRUE);
    }
}

static void
add_channel(GwyContainer *container,
            NanoScanFile *nfile,
            NanoScanChannel *channel,
            gint id)
{
    GwyDataField *dfield;
    GwySIUnit *unit;
    gint power10;
    gdouble *d;
    GQuark quark;
    gchar *key, *title;
    gdouble q = 1.0;
    gint i, j;

    dfield = gwy_data_field_new(nfile->xres, nfile->yres,
                                nfile->xreal, nfile->yreal,
                                FALSE);
    if (nfile->xyunits) {
        unit = gwy_data_field_get_si_unit_xy(dfield);
        gwy_si_unit_set_from_string_parse(unit, nfile->xyunits, &power10);
        gwy_data_field_set_xreal(dfield, pow10(power10)*nfile->xreal);
        gwy_data_field_set_yreal(dfield, pow10(power10)*nfile->yreal);
    }
    if (channel->zunits) {
        unit = gwy_data_field_get_si_unit_z(dfield);
        gwy_si_unit_set_from_string_parse(unit, channel->zunits, &power10);
        q = pow10(power10);
    }
    d = gwy_data_field_get_data(dfield);
    for (i = 0; i < nfile->yres; i++) {
        for (j = 0; j < nfile->xres; j++) {
            d[(nfile->yres-1 - i)*nfile->xres + j]
                = q*channel->data[i*nfile->xres + j];
        }
    }
    quark = gwy_app_get_data_key_for_id(id);
    gwy_container_set_object(container, quark, dfield);
    g_object_unref(dfield);

    if (channel->name) {
        key = g_strconcat(g_quark_to_string(quark), "/title", NULL);
        if (channel->direction == SCAN_FORWARD)
            title = g_strconcat(channel->name, " [Forward]", NULL);
        else if (channel->direction == SCAN_BACKWARD)
            title = g_strconcat(channel->name, " [Backward]", NULL);
        else {
            title = channel->name;
            channel->name = NULL;
        }
        gwy_container_set_string_by_name(container, key, title);
        g_free(key);
    }
    channel->already_added = TRUE;
}

static void
add_graph(GwyContainer *container,
          NanoScanFile *nfile,
          NanoScanChannel *channel,
          gint id)
{
    NanoScanAxis *axis0 = &g_array_index(nfile->axes, NanoScanAxis, 0);
    GwyGraphModel *gmodel;
    GQuark quark;
    guint j, i;

    gmodel = gwy_graph_model_new();
    add_curve_model(nfile, channel, 0, gmodel);
    for (j = i = 0; j < nfile->channels->len; j++) {
        NanoScanChannel *other = &g_array_index(nfile->channels,
                                                NanoScanChannel, j);
        if (other == channel || other->already_added
            || !other->name || !channel->name
            || !gwy_strequal(other->name, channel->name))
            continue;
        add_curve_model(nfile, other, ++i, gmodel);
    }
    if (channel->name) {
        g_object_set(gmodel, "axis-label-left", channel->name, NULL);
        g_object_set(gmodel, "title", channel->name, NULL);
    }
    if (axis0->name)
        g_object_set(gmodel, "axis-label-bottom", axis0->name, NULL);

    quark = gwy_app_get_graph_key_for_id(id);
    gwy_container_set_object(container, quark, gmodel);
    g_object_unref(gmodel);
}

static void
add_curve_model(NanoScanFile *nfile,
                NanoScanChannel *channel,
                guint i,
                GwyGraphModel *gmodel)
{
    NanoScanAxis *axis0 = &g_array_index(nfile->axes, NanoScanAxis, 0);
    GwyGraphCurveModel *gcmodel;
    GwyDataLine *dline;
    GwySIUnit *unit;
    gdouble real, q = 1.0;
    gdouble *d;
    gint power10, j;

    real = axis0->stop - axis0->start;
    dline = gwy_data_line_new(nfile->xres, real, FALSE);
    gwy_data_line_set_offset(dline, axis0->start);
    if (axis0->units) {
        unit = gwy_data_line_get_si_unit_x(dline);
        gwy_si_unit_set_from_string_parse(unit, axis0->units, &power10);
        gwy_data_line_set_real(dline, pow10(power10)*real);
        gwy_data_line_set_offset(dline, pow10(power10)*axis0->start);
    }
    if (channel->zunits) {
        unit = gwy_data_line_get_si_unit_y(dline);
        gwy_si_unit_set_from_string_parse(unit, channel->zunits, &power10);
        q = pow10(power10);
    }
    d = gwy_data_line_get_data(dline);
    for (j = 0; j < nfile->xres; j++) {
        d[j] = q*channel->data[j];
    }
    gcmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_data_from_dataline(gcmodel, dline, 0, 0);
    g_object_set(gcmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "color", gwy_graph_get_preset_color(i),
                 NULL);

    if (channel->direction == SCAN_FORWARD)
        g_object_set(gcmodel, "description", "Forward", NULL);
    else if (channel->direction == SCAN_BACKWARD)
        g_object_set(gcmodel, "description", "Backward", NULL);
    else
        g_object_set(gcmodel, "description", "Unknown direction", NULL);

    gwy_graph_model_add_curve(gmodel, gcmodel);
    g_object_unref(gcmodel);
    gwy_graph_model_set_units_from_data_line(gmodel, dline);
    g_object_unref(dline);
    channel->already_added = TRUE;
}

static void
add_multigraph(GwyContainer *container,
               NanoScanFile *nfile,
               NanoScanChannel *channel,
               gint id)
{
    NanoScanAxis *axis0 = &g_array_index(nfile->axes, NanoScanAxis, 0);
    GwyGraphModel *gmodel;
    GQuark quark;
    guint i;

    gmodel = gwy_graph_model_new();
    for (i = 0; i < nfile->yres; i++)
        add_multicurve_model(nfile, channel, i, gmodel);

    if (channel->name) {
        gchar *title;

        g_object_set(gmodel, "axis-label-left", channel->name, NULL);
        if (channel->direction == SCAN_FORWARD)
            title = g_strconcat(channel->name, " [Forward]", NULL);
        else if (channel->direction == SCAN_BACKWARD)
            title = g_strconcat(channel->name, " [Backward]", NULL);
        else {
            title = channel->name;
            channel->name = NULL;
        }
        g_object_set(gmodel, "title", title, NULL);
        g_free(title);
    }
    if (axis0->name)
        g_object_set(gmodel, "axis-label-bottom", axis0->name, NULL);

    quark = gwy_app_get_graph_key_for_id(id);
    gwy_container_set_object(container, quark, gmodel);
    g_object_unref(gmodel);
    channel->already_added = TRUE;
}

static void
add_multicurve_model(NanoScanFile *nfile,
                     NanoScanChannel *channel,
                     guint i,
                     GwyGraphModel *gmodel)
{
    NanoScanAxis *axis0 = &g_array_index(nfile->axes, NanoScanAxis, 0);
    NanoScanAxis *axis1 = &g_array_index(nfile->axes, NanoScanAxis, 1);
    GwyGraphCurveModel *gcmodel;
    GwyDataLine *dline;
    GwySIUnit *unit;
    gdouble real, yval, q = 1.0;
    gdouble *d;
    gchar *descr;
    gint power10, j;

    real = axis0->stop - axis0->start;
    dline = gwy_data_line_new(nfile->xres, real, FALSE);
    gwy_data_line_set_offset(dline, axis0->start);
    if (axis0->units) {
        unit = gwy_data_line_get_si_unit_x(dline);
        gwy_si_unit_set_from_string_parse(unit, axis0->units, &power10);
        gwy_data_line_set_real(dline, pow10(power10)*real);
        gwy_data_line_set_offset(dline, pow10(power10)*axis0->start);
    }
    if (channel->zunits) {
        unit = gwy_data_line_get_si_unit_y(dline);
        gwy_si_unit_set_from_string_parse(unit, channel->zunits, &power10);
        q = pow10(power10);
    }
    d = gwy_data_line_get_data(dline);
    for (j = 0; j < nfile->xres; j++) {
        d[j] = q*channel->data[i*nfile->xres + j];
    }
    gcmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_data_from_dataline(gcmodel, dline, 0, 0);
    yval = i/(nfile->yres - 1.0)*(axis1->stop - axis1->start) + axis1->start;
    descr = g_strdup_printf("%s %g%s%s",
                            axis1->name ? axis1->name : "Y",
                            yval * axis1->display_scale,
                            axis1->display_units ? " " : "",
                            axis1->display_units ? axis1->display_units : "");
    g_object_set(gcmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "color", gwy_graph_get_preset_color(i),
                 "description", descr,
                 NULL);
    g_free(descr);

    gwy_graph_model_add_curve(gmodel, gcmodel);
    g_object_unref(gcmodel);
    gwy_graph_model_set_units_from_data_line(gmodel, dline);
    g_object_unref(dline);
    channel->already_added = TRUE;
}

static void
fix_metadata(NanoScanFile *nfile)
{
    guint i;

    for (i = 0; i < nfile->meta->len; i++) {
        NanoScanMeta *meta = &g_array_index(nfile->meta, NanoScanMeta, i);
        guint j;

        meta->name[0] = g_ascii_toupper(meta->name[0]);
        for (j = 1; meta->name[j]; j++) {
            if (meta->name[j] == '_') {
                meta->name[j] = ' ';
                if (meta->name[j+1]) {
                    meta->name[j+1] = g_ascii_toupper(meta->name[j+1]);
                    j++;
                }
            }
        }
        if (meta->value && meta->units) {
            gchar *fullvalue = g_strconcat(meta->value, " ", meta->units, NULL);

            g_free(meta->value);
            meta->value = fullvalue;
            g_free(meta->units);
            meta->units = NULL;
        }
    }
}

static void
add_metadata(GwyContainer *container,
             NanoScanFile *nfile,
             gint id)
{
    GwyContainer *metadata;
    gchar *key;
    guint i;

    if (!nfile->meta->len)
        return;

    metadata = gwy_container_new();
    for (i = 0; i < nfile->meta->len; i++) {
        NanoScanMeta *meta = &g_array_index(nfile->meta, NanoScanMeta, i);

        if (meta->value)
            gwy_container_set_string_by_name(metadata, meta->name,
                                             g_strdup(meta->value));
    }

    key = g_strdup_printf("/%d/meta", id);
    gwy_container_set_object_by_name(container, key, metadata);
    g_free(key);
    g_object_unref(metadata);
}

static void
start_element(G_GNUC_UNUSED GMarkupParseContext *context,
              const gchar *element_name,
              G_GNUC_UNUSED const gchar **attribute_names,
              G_GNUC_UNUSED const gchar **attribute_values,
              gpointer user_data,
              GError **error)
{
    NanoScanFile *nfile = (NanoScanFile*)user_data;

    if (!nfile->path->len && !gwy_strequal(element_name, "scan")) {
        g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                    _("Top-level element is not ‘scan’."));
        return;
    }

    g_string_append_c(nfile->path, '/');
    g_string_append(nfile->path, element_name);

    if (gwy_strequal(nfile->path->str, CHANNEL_PREFIX)) {
        NanoScanChannel channel;

        /* Individual channels are inside <contents> tags, but there can be
         * empty <contents> tags and stuff.  Just create what we see and hope
         * upstream cann sort it out. */
        gwy_clear(&channel, 1);
        channel.direction = nfile->current_direction;
        g_array_append_val(nfile->channels, channel);
        gwy_debug("channel #%u", (guint)nfile->channels->len);
    }
    else if (gwy_strequal(nfile->path->str, AXIS_PREFIX)) {
        NanoScanAxis axis;

        gwy_clear(&axis, 1);
        g_array_append_val(nfile->axes, axis);
        gwy_debug("axis #%u", (guint)nfile->axes->len);
    }
}

static void
end_element(G_GNUC_UNUSED GMarkupParseContext *context,
            const gchar *element_name,
            gpointer user_data,
            G_GNUC_UNUSED GError **error)
{
    NanoScanFile *nfile = (NanoScanFile*)user_data;
    gchar *pos;

    pos = strlenrchr(nfile->path->str, '/', nfile->path->len);
    /* GMarkupParser should raise a run-time error if this does not hold. */
    g_assert(pos && strcmp(pos + 1, element_name) == 0);
    g_string_truncate(nfile->path, pos - nfile->path->str);
}

static void
text(G_GNUC_UNUSED GMarkupParseContext *context,
     const gchar *value,
     gsize value_len,
     gpointer user_data,
     G_GNUC_UNUSED GError **error)
{
    NanoScanFile *nfile = (NanoScanFile*)user_data;
    const gchar *path = nfile->path->str;

    /* Content is not directly in elements such as <unit>.  Each element, in
     * spite of its content model being a single value, must contain a <v>
     * element that actually contains the value.  Fortunately, the genius that
     * has invented this did not realize that the <v> element cannot directly
     * contain the value either and it needs its own <v> element... */
    if (nfile->path->len < 3
        || nfile->path->str[nfile->path->len-1] != 'v'
        || nfile->path->str[nfile->path->len-2] != '/')
        return;
    nfile->path->str[nfile->path->len-2] = '\0';

    if (strncmp(path, RES_PREFIX, RES_PREFIX_SIZE) == 0) {
        path += RES_PREFIX_SIZE;
        if (gwy_strequal(path, "/fast_axis")) {
            nfile->xres = atoi(value);
            gwy_debug("xres: %d", nfile->xres);
        }
        else if (gwy_strequal(path, "/slow_axis")) {
            nfile->yres = atoi(value);
            gwy_debug("xres: %d", nfile->yres);
        }
    }
    else if (strncmp(path, DIMS_PREFIX, DIMS_PREFIX_SIZE) == 0) {
        path += RES_PREFIX_SIZE;
        if (gwy_strequal(path, "/unit")) {
            g_free(nfile->xyunits);
            nfile->xyunits = g_strdup(value);
            gwy_debug("xyunits: %s", value);
        }
        else if (gwy_strequal(path, "/size/contents/fast_axis")) {
            nfile->xreal = g_ascii_strtod(value, NULL);
            gwy_debug("xreal: %g", nfile->xreal);
        }
        else if (gwy_strequal(path, "/size/contents/slow_axis")) {
            nfile->yreal = g_ascii_strtod(value, NULL);
            gwy_debug("yreal: %g", nfile->yreal);
        }
    }
    else if (strncmp(path, AXIS_PREFIX, AXIS_PREFIX_SIZE) == 0
             && nfile->axes->len) {
        NanoScanAxis *axis = &g_array_index(nfile->axes, NanoScanAxis,
                                            nfile->axes->len - 1);

        path += AXIS_PREFIX_SIZE;
        if (gwy_strequal(path, "/name")) {
            g_free(axis->name);
            axis->name = g_strdup(value);
            gwy_debug("axis name: %s", value);
        }
        else if (gwy_strequal(path, "/unit")) {
            g_free(axis->units);
            axis->units = g_strdup(value);
            gwy_debug("axis units: %s", value);
        }
        else if (gwy_strequal(path, "/display_unit")) {
            g_free(axis->display_units);
            axis->display_units = g_strdup(value);
            gwy_debug("axis display_units: %s", value);
        }
        else if (gwy_strequal(path, "/display_scale")) {
            axis->display_scale = g_ascii_strtod(value, NULL);
            gwy_debug("axis display_scale: %g", axis->display_scale);
        }
        /* FIXME: These can be vectors in the energy scan mode! */
        else if (gwy_strequal(path, "/start/vector")) {
            axis->start = g_ascii_strtod(value, NULL);
            gwy_debug("axis start: %g", axis->start);
        }
        else if (gwy_strequal(path, "/stop/vector")) {
            axis->stop = g_ascii_strtod(value, NULL);
            gwy_debug("axis stop: %g", axis->stop);
        }
    }
    else if (strncmp(path, CHANNEL_PREFIX, CHANNEL_PREFIX_SIZE) == 0
             && nfile->channels->len) {
        NanoScanChannel *channel = &g_array_index(nfile->channels,
                                                  NanoScanChannel,
                                                  nfile->channels->len - 1);

        path += CHANNEL_PREFIX_SIZE;
        /* Individual channels are inside <contents> tags, but there can be
         * empty <contents> tags and stuff.  Just create what we see and hope
         * upstream cann sort it out. */
        if (gwy_strequal(path, "/name")) {
            g_free(channel->name);
            channel->name = g_strdup(value);
            gwy_debug("channel: %s", value);
        }
        else if (gwy_strequal(path, "/unit")) {
            g_free(channel->zunits);
            channel->zunits = g_strdup(value);
            gwy_debug("zunits: %s", value);
        }
        else if (gwy_strequal(path, "/data")) {
            g_free(channel->data);
            channel->data = read_channel_data(value, value_len,
                                              nfile->xres * nfile->yres, error);
            gwy_debug("DATA: %p", channel->data);
        }
    }
    else if (strncmp(path, DATA_PREFIX, DATA_PREFIX_SIZE) == 0) {
        /* The channels are groupped by direction.  So we need to remember
         * which direction are in because it is just an element along the
         * way. */
        path += DATA_PREFIX_SIZE;
        if (gwy_strequal(path, "/name")) {
            if (gwy_strequal(value, "forward")) {
                nfile->current_direction = SCAN_FORWARD;
                gwy_debug("direction: forward");
            }
            else if (gwy_strequal(value, "backward")) {
                nfile->current_direction = SCAN_BACKWARD;
                gwy_debug("direction: backward");
            }
            else
                g_warning("Unknown direction %s.", value);
        }
    }
    else if (strncmp(path, META_PREFIX, META_PREFIX_SIZE) == 0) {
        gchar *name;

        path += META_PREFIX_SIZE;
        name = strlenrchr(path, '/', nfile->path->len - META_PREFIX_SIZE) + 1;
        add_meta(nfile, name, value);
    }
    else if (strncmp(path, SCAN_PREFIX, SCAN_PREFIX_SIZE) == 0
             && !strchr(path + SCAN_PREFIX_SIZE + 1, '/')) {
        add_meta(nfile, path + SCAN_PREFIX_SIZE+1, value);
    }

    nfile->path->str[nfile->path->len-2] = '/';
}

static void
add_meta(NanoScanFile *nfile,
         const gchar *name,
         const gchar *value)
{
    /* If the element ends with _units, it should be the units of the
     * previous element. */
    if (g_str_has_suffix(name, "_unit")) {
        if (nfile->meta->len) {
            NanoScanMeta *meta = &g_array_index(nfile->meta, NanoScanMeta,
                                                nfile->meta->len - 1);

            if (g_str_has_prefix(name, meta->name)
                && strlen(name) == strlen(meta->name) + sizeof("_unit")-1) {
                g_free(meta->units);
                meta->units = g_strdup(value);
                gwy_debug("units of %s: %s", meta->name, value);
            }
        }
    }
    else {
        NanoScanMeta meta = { NULL, NULL, NULL };

        meta.name = g_strdup(name);
        meta.value = g_strdup(value);
        gwy_debug("meta %s: %s", name, value);
        g_array_append_val(nfile->meta, meta);
    }
}

static gfloat*
read_channel_data(const gchar *value, gsize value_len,
                  guint npixels,
                  GError **error)
{
    gpointer *mem;
    gsize len, maxlen = 3*value_len/4;

    if (maxlen < npixels*sizeof(gfloat)) {
        g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("Wrong size of of Base64 encoded data."));
        return NULL;
    }

    mem = g_malloc(maxlen);
    len = decode_base64((const guchar*)value, value_len, (guchar*)mem);
    if (len != npixels*sizeof(gfloat)) {
        g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("Wrong size of of Base64 encoded data."));
        g_free(mem);
        return NULL;
    }

    return (gfloat*)mem;
}

static gsize
decode_base64(const guchar *buf, gsize len, guchar *out)
{
    static const guchar base64_codes[0x100] = {
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255,  62, 255, 255, 255,  63,
         52,  53,  54,  55,  56,  57,  58,  59,
         60,  61, 255, 255, 255,   0, 255, 255,
        255,   0,   1,   2,   3,   4,   5,   6,
          7,   8,   9,  10,  11,  12,  13,  14,
         15,  16,  17,  18,  19,  20,  21,  22,
         23,  24,  25, 255, 255, 255, 255, 255,
        255,  26,  27,  28,  29,  30,  31,  32,
         33,  34,  35,  36,  37,  38,  39,  40,
         41,  42,  43,  44,  45,  46,  47,  48,
         49,  50,  51, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
    };

    const guchar *end = buf + len;
    guchar *p = out;
    guchar c, code, last[2];
    guint i = 0, v = 0;

    last[0] = last[1] = 0;
    while (buf < end) {
        c = *buf++;
        code = base64_codes[c];
        if (code != 255) {
            last[1] = last[0];
            last[0] = c;
            v = (v << 6) | code;
            if (++i == 4) {
                *p++ = v >> 16;
                if (last[1] != '=')
                    *p++ = v >> 8;
                if (last[0] != '=')
                    *p++ = v;
                i = 0;
            }
        }
    }

    return p - out;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
