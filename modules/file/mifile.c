/*
 *  $Id$
 *  Copyright (C) 2005 Chris Anderson, Molecular Imaging Corp.
 *  E-mail: sidewinder.asu@gmail.com
 *
 *  This source code is based off of stpfile.c,
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klstptek.
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

#define MAGIC "fileType      Image"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)
#define DATA_MAGIC "data          \n"
#define DATA_MAGIC_SIZE (sizeof(DATA_MAGIC) - 1)
#define EXTENSION ".mi"
#define KEY_LEN 14

#define Angstrom (1e-10)
#ifndef HAVE_POW10
#define pow10(x) (exp(G_LN10*(x)))
#endif

typedef struct {
    gchar *id;
    const gint16 *data;
    GHashTable *meta;
} MIData;

typedef struct {
    gint xres;
    gint yres;
    guint n;
    MIData *buffers;
    GHashTable *meta;
} MIFile;

typedef struct {
    MIFile *file;
    GwyContainer *data;
    GtkWidget *data_view;
} MIControls;

typedef struct {
    const gchar *key;
    const gchar *meta;
    const gchar *format;
} MetaDataFormat;

/* Gwyddion File Module Functions */
static gboolean         module_register      (const gchar *name);
static gint             mifile_detect        (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*    mifile_load          (const gchar *filename);

/* Helper Functions */
static guint            find_data_start      (const guchar *buffer,
                                              gsize size);
static guint            file_read_header     (MIFile *mifile,
                                              gchar *buffer);
static guint            select_which_data    (MIFile *mifile);
static void             selection_changed    (GtkWidget *button,
                                              MIControls *controls);
static void             read_data_field      (GwyDataField *dfield,
                                              MIData *midata,
                                              gint xres, gint yres);
static void             mifile_free          (MIFile *mifile);
static gboolean         mifile_get_double    (GHashTable *meta,
                                              const gchar *key,
                                              gdouble *value);
static void             process_metadata     (MIFile *mifile,
                                              guint id,
                                              GwyContainer *container);
/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Molecular Imaging MI data files."),
    "Chris Anderson <sidewinder.asu@gmail.com>",
    "0.2.0",
    "Chris Anderson, Molecular Imaging Corp.",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo mifile_func_info = {
        "mifile",
        N_("PicoView Data Files (.mi)"),
        (GwyFileDetectFunc)&mifile_detect,
        (GwyFileLoadFunc)&mifile_load,
        NULL
    };

    gwy_file_func_register(name, &mifile_func_info);

    return TRUE;
}

static gint
mifile_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && !memcmp(fileinfo->buffer, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static GwyContainer*
mifile_load(const gchar *filename)
{
    MIFile *mifile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    guint header_size;
    gchar *p;
    gboolean ok;
    guint i = 0, pos;

    /* Open the file and load in its contents into "buffer" */
    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }

    /* Find out the length of the file header */
    if (strncmp(buffer, MAGIC, MAGIC_SIZE)
        || size <= DATA_MAGIC_SIZE
        || !(header_size = find_data_start(buffer, size))) {

        g_warning("File %s is not a MI file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    /* Load the header information into the MIFile structure */
    mifile = g_new0(MIFile, 1);
    p = g_strndup(buffer, header_size);
    ok = file_read_header(mifile, p);
    g_free(p);

    // TODO: check size
    /* Load the actual image data into the MIFile structure */
    if (ok) {
        pos = header_size;
        for (i = 0; i < mifile->n; i++) {
            mifile->buffers[i].data = (const guint16*)(buffer + pos);
            pos += 2*mifile->xres * mifile->yres;
        }
        /* Show the buffer previews to the user, and let
           him/her select which one to actually load */
        i = select_which_data(mifile);
        if (i == (guint)-1)
            ok = FALSE;
    }

    if (ok) {
        dfield = gwy_data_field_new(mifile->xres, mifile->yres,
                                    1.0, 1.0, FALSE);
        read_data_field(dfield, mifile->buffers + i,
                        mifile->xres, mifile->yres);
    }
    gwy_file_abandon_contents(buffer, size, NULL);

    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);
        process_metadata(mifile, i, container);
    }
    mifile_free(mifile);

    return container;
}

static guint
find_data_start(const guchar *buffer,
                gsize size)
{
    /* NOTE: TRY USING g_strrstr () INSTEAD */
    const guchar *p;

    size -= DATA_MAGIC_SIZE;

    for (p = buffer;
         p && strncmp(p, DATA_MAGIC, DATA_MAGIC_SIZE);
         p = memchr(p+1, (DATA_MAGIC)[0], size - (p - buffer) - 1))
        ;

    return p ? (p - buffer) + DATA_MAGIC_SIZE : 0;
}

static guint
file_read_header(MIFile *mifile,
                 gchar *buffer)
{
    MIData *data = NULL;
    GHashTable *meta;
    gchar *line, *key, *value = NULL;

    mifile->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                         g_free, g_free);
    mifile->xres = mifile->yres = 0;
    meta = mifile->meta;
    while ((line = gwy_str_next_line(&buffer))) {
        if (!strncmp(line, "bufferLabel   ", KEY_LEN)) {
            mifile->n++;
            mifile->buffers = g_renew(MIData, mifile->buffers, mifile->n);
            data = mifile->buffers + (mifile->n - 1);
            data->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                               g_free, g_free);
            data->data = NULL;
            data->id = g_strstrip(g_strdup(line + KEY_LEN));
            meta = data->meta;
        }
        if (line[0] == ' ')
            continue;

        key = g_strstrip(g_strndup(line, KEY_LEN));
        value = g_strstrip(g_strdup(line + KEY_LEN));
        g_hash_table_replace(meta, key, value);

        if (!strcmp(key, "xPixels"))
            mifile->xres = atol(value);
        if (!strcmp(key, "yPixels"))
            mifile->yres = atol(value);
    }

    return mifile->n;
}

static guint
select_which_data(MIFile *mifile)
{
    MIControls controls;
    MIData *data;
    GtkWidget *dialog, *label, *vbox, *hbox, *align;
    GwyDataField *dfield;
    GwyEnum *choices;
    GwyPixmapLayer *layer;
    GSList *radio, *rl;
    guint i, b = (guint)-1;
    const gchar *title;

    if (!mifile->n)
        return b;

    if (mifile->n == 1)
        return 0;

    controls.file = mifile;
    choices = g_new(GwyEnum, mifile->n);
    for (i = 0; i < mifile->n; i++) {
        data = mifile->buffers + i;
        choices[i].value = i;
        title = NULL;
        title = g_strdup(data->id);
        if (title)
            choices[i].name = g_strdup_printf(_("Buffer %u (%s)"), i, title);
        else
            choices[i].name = g_strdup_printf(_("Buffer %u"), i);
    }

    dialog = gtk_dialog_new_with_buttons(_("Select Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(TRUE, 0);
    gtk_container_add(GTK_CONTAINER(align), vbox);

    label = gtk_label_new(_("Data to load:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

    radio = gwy_radio_buttons_create(choices, mifile->n, "data",
                                     G_CALLBACK(selection_changed), &controls,
                                     0);
    for (i = 0, rl = radio; rl; i++, rl = g_slist_next(rl))
        gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(rl->data), TRUE, TRUE, 0);

    /* preview */
    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    controls.data = gwy_container_new();
    dfield = gwy_data_field_new(mifile->xres, mifile->yres, 1.0, 1.0, FALSE);
    read_data_field(dfield, mifile->buffers, mifile->xres, mifile->yres);
    gwy_container_set_object_by_name(controls.data, "/0/data", dfield);
    gwy_container_set_enum_by_name(controls.data, "/0/base/range-type",
                                   GWY_LAYER_BASIC_RANGE_AUTO);
    g_object_unref(dfield);

    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.data_view),
                           120.0/MAX(mifile->xres, mifile->yres));
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_range_type_key(GWY_LAYER_BASIC(layer),
                                       "/0/base/range-type");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view), layer);
    gtk_container_add(GTK_CONTAINER(align), controls.data_view);

    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
    switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
        gtk_widget_destroy(dialog);
        case GTK_RESPONSE_NONE:
        break;

        case GTK_RESPONSE_OK:
        b = GPOINTER_TO_UINT(gwy_radio_buttons_get_current(radio, "data"));
        gtk_widget_destroy(dialog);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    for (i = 0; i < mifile->n; i++)
        g_free((gpointer)choices[i].name);
    g_free(choices);

    return b;
}

static void
selection_changed(GtkWidget *button,
                  MIControls *controls)
{
    MIFile *mifile;
    GwyDataField *dfield;
    guint i;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    i = gwy_radio_buttons_get_current_from_widget(button, "data");
    g_assert(i != (guint)-1);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->data,
                                                             "/0/data"));
    mifile = controls->file;
    read_data_field(dfield, mifile->buffers + i, mifile->xres, mifile->yres);
}

static void
read_data_field(GwyDataField *dfield,
                MIData *midata,
                gint xres, gint yres)
{
    gdouble *data;
    const gint16 *row;
    gint i, j;

    gwy_data_field_resample(dfield, xres, yres, GWY_INTERPOLATION_NONE);
    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < yres; i++) {
        row = midata->data + (yres-1 - i)*xres;
        for (j = 0; j < xres; j++)
            data[i*xres + j] = GINT16_FROM_LE(row[j]);
    }
    gwy_data_field_data_changed(dfield);
}

static void
mifile_free(MIFile *mifile)
{
    guint i;

    for (i = 0; i < mifile->n; i++) {
        g_hash_table_destroy(mifile->buffers[i].meta);
        g_free(mifile->buffers[i].id);
    }
    g_free(mifile->buffers);
    g_hash_table_destroy(mifile->meta);
    g_free(mifile);
}

static gboolean
mifile_get_double(GHashTable *meta,
                  const gchar *key,
                  gdouble *value)
{
    gchar *p, *end;
    gdouble r;

    p = g_hash_table_lookup(meta, key);
    if (!p)
        return FALSE;

    r = g_ascii_strtod(p, &end);
    if (end == p)
        return FALSE;

    *value = r;
    return TRUE;
}

static void
process_metadata(MIFile *mifile,
                 guint id,
                 GwyContainer *container)
{
    static const MetaDataFormat global_metadata[] = {
        { "version", "Version", "%s" },
        { "dateAcquired", "Date acquired", "%s" },
        { "mode", "mode", "%s" },
        { "xSensitivity", "xSensitivity", "%s" },
        { "xNonlinearity", "xNonlinearity", "%s" },
        { "xHysteresis", "xHysteresis", "%s" },
        { "ySensitivity", "ySensitivity", "%s" },
        { "yNonlinearity", "yNonlinearity", "%s" },
        { "yHysteresis", "yHysteresis", "%s" },
        { "zSensitivity", "zSensitivity", "%s" },
        { "reverseX", "reverseX", "%s" },
        { "reverseY", "reverseY", "%s" },
        { "reverseZ", "reverseZ", "%s" },
        { "xDacRange", "xDacRange", "%s" },
        { "yDacRange", "yDacRange", "%s" },
        { "zDacRange", "zDacRange", "%s" },
        { "xPixels", "xPixels", "%s" },
        { "yPixels", "yPixels", "%s" },
        { "xOffset", "xOffset", "%s" },
        { "yOffset", "yOffset", "%s" },
        { "xLength", "xLength", "%s" },
        { "yLength", "yLength", "%s" },
        /*{ "scanUp", "scanUp", "%s" },*/
        { "scanSpeed", "scanSpeed", "%s" },
        { "scanAngle", "scanAngle", "%s" },
        { "servoSetpoint", "servoSetpoint", "%s" },
        { "biasSample", "biasSample", "%s" },
        { "bias", "bias", "%s" },
        { "servoIGain", "servoIGain", "%s" },
        { "servoPGain", "servoPGain", "%s" },
        { "servoRange", "servoRange", "%s" },
        { "servoInputGain", "servoInputGain", "%s" },
    };
    static const MetaDataFormat local_metadata[] = {
        { "trace", "trace", "%s" },
    };

    MIData *data;
    GwyDataField *dfield;
    const gchar *mode;
    gchar *bufferUnit;
    GwySIUnit *siunit;
    gint power10;
    gdouble bufferRange, conversion;
    GString *key, *value;
    gchar *p;
    guint i;
    gdouble xLength, yLength;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(container,
                                                             "/0/data"));
    /* Make "data" point to the selected buffer */
    data = mifile->buffers + id;

    /* Get the buffer mode */
    mode = NULL;
    mode = g_hash_table_lookup(data->meta, "bufferLabel");

    /* Set the container's title to whatever the buffer mode is */
    if (mode)
        gwy_container_set_string_by_name(container, "/filename/title",
                                         g_strdup(mode));
    else
        gwy_container_set_string_by_name(container, "/filename/title",
                                         g_strdup("Unknown"));

    /* Fix z-value scale */
    bufferUnit = NULL;
    power10 = 0;
    bufferUnit = g_hash_table_lookup(data->meta, "bufferUnit");
    if (bufferUnit) {
        siunit = gwy_si_unit_new_parse(bufferUnit, &power10);
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);
    }
    if (mifile_get_double(data->meta, "bufferRange", &bufferRange)) {
        bufferRange *= pow10(power10);
        conversion = bufferRange / 32768;
        gwy_data_field_multiply(dfield, conversion);
    }

    /* Fix x-y value scale */
    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    if (!mifile_get_double(mifile->meta, "xLength", &xLength)) {
        g_warning("Missing or invalid x length");
        xLength = 1e-9;
    }
    gwy_data_field_set_xreal(dfield, xLength);

    if (!mifile_get_double(mifile->meta, "yLength", &yLength)) {
        g_warning("Missing or invalid y length");
        yLength = 1e-9;
    }
    gwy_data_field_set_yreal(dfield, yLength);

    /* Store Metadata */
    key = g_string_new("/meta/");
    value = g_string_new("");

    /* Global */
    for (i = 0; i < G_N_ELEMENTS(global_metadata); i++) {
        if (!(p = g_hash_table_lookup(mifile->meta, global_metadata[i].key)))
            continue;

        g_string_truncate(key, sizeof("/meta"));
        g_string_append(key, global_metadata[i].meta);
        g_string_printf(value, global_metadata[i].format, p);
        gwy_container_set_string_by_name(container, key->str,
                                         g_strdup(value->str));
    }

    /* Local */
    for (i = 0; i < G_N_ELEMENTS(local_metadata); i++) {
        if (!(p = g_hash_table_lookup(data->meta, local_metadata[i].key)))
            continue;

        g_string_truncate(key, sizeof("/meta"));
        g_string_append(key, local_metadata[i].meta);
        g_string_printf(value, local_metadata[i].format, p);
        gwy_container_set_string_by_name(container, key->str,
                                         g_strdup(value->str));
    }
    g_string_free(key, TRUE);
    g_string_free(value, TRUE);

    /* Store "Special" metadata */

    /*
    if ((p = g_hash_table_lookup(data->meta, "Date"))
        && (s = g_hash_table_lookup(data->meta, "time")))
        gwy_container_set_string_by_name(container, "/meta/Date",
                                         g_strconcat(p, " ", s, NULL));
    */

    if ((p = g_hash_table_lookup(mifile->meta, "scanUp"))) {
        if (g_str_equal(p, "FALSE"))
            gwy_container_set_string_by_name(container,
                                             "/meta/Scanning direction",
                                             g_strdup("Top to bottom"));
        else if (g_str_equal(p, "TRUE"))
            gwy_container_set_string_by_name(container,
                                             "/meta/Scanning direction",
                                             g_strdup("Bottom to top"));
    }

    /*
    if ((p = g_hash_table_lookup(data->meta, "collect_mode"))) {
        if (!strcmp(p, "1"))
            gwy_container_set_string_by_name(container,
                                             "/meta/Line direction",
                                             g_strdup("Left to right"));
        else if (!strcmp(p, "2"))
            gwy_container_set_string_by_name(container,
                                             "/meta/Line direction",
                                             g_strdup("Right to left"));
    }
    */
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
