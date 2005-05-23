/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klstptek.
 *  E-mail: yeti@gwyddion.net, klstptek@gwyddion.net.
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
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAGIC "UK SOFT\r\n"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define DATA_MAGIC "Data_section  \r\n"
#define DATA_MAGIC_SIZE (sizeof(DATA_MAGIC) - 1)

#define EXTENSION ".stp"

#define KEY_LEN 14

#define Angstrom (1e-10)

typedef struct {
    gint id;
    gint xres;
    gint yres;
    const guint16 *data;
    GHashTable *meta;
} STPData;

typedef struct {
    guint n;
    STPData *buffers;
    GHashTable *meta;
} STPFile;

typedef struct {
    STPFile *file;
    GwyContainer *data;
    GtkWidget *data_view;
} STPControls;

static gboolean      module_register       (const gchar *name);
static gint          stpfile_detect        (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* stpfile_load          (const gchar *filename);
static guint         find_data_start       (const guchar *buffer,
                                            gsize size);
static void          stpfile_free          (STPFile *stpfile);
static void          read_data_field       (GwyDataField *dfield,
                                            STPData *stpdata);
static guint         file_read_header      (STPFile *stpfile,
                                            gchar *buffer);
static void          process_metadata      (STPFile *stpfile,
                                            guint id,
                                            GwyContainer *container);
static guint         select_which_data     (STPFile *stpfile);
static void          selection_changed     (GtkWidget *button,
                                            STPControls *controls);

static const GwyEnum channels[] = {
    { N_("Topography"), 1  },
    { N_("Amplitude"),  2  },
    { N_("Phase"),      13 },
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Molecular Imaging STP data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo stpfile_func_info = {
        "stpfile",
        N_("STP files (.stp)"),
        (GwyFileDetectFunc)&stpfile_detect,
        (GwyFileLoadFunc)&stpfile_load,
        NULL
    };

    gwy_file_func_register(name, &stpfile_func_info);

    return TRUE;
}

static gint
stpfile_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && !memcmp(fileinfo->buffer, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static GwyContainer*
stpfile_load(const gchar *filename)
{
    STPFile *stpfile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    guint header_size;
    gchar *p;
    gboolean ok;
    guint i = 0, pos;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (strncmp(buffer, MAGIC, MAGIC_SIZE)
        || size <= DATA_MAGIC_SIZE
        || !(header_size = find_data_start(buffer, size))) {
        g_warning("File %s is not a STP file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    stpfile = g_new0(STPFile, 1);
    p = g_strndup(buffer, header_size);
    ok = file_read_header(stpfile, p);
    g_free(p);

    /* TODO: check size */
    if (ok) {
        pos = header_size;
        for (i = 0; i < stpfile->n; i++) {
            stpfile->buffers[i].data = (const guint16*)(buffer + pos);
            gwy_debug("buffer %i pos: %u", i, pos);
            pos += 2*stpfile->buffers[i].xres * stpfile->buffers[i].yres;
        }

        i = select_which_data(stpfile);
        if (i == (guint)-1)
            ok = FALSE;
    }

    if (ok) {
        dfield = gwy_data_field_new(stpfile->buffers[i].xres,
                                    stpfile->buffers[i].yres,
                                    1.0, 1.0, FALSE);
        read_data_field(dfield, stpfile->buffers + i);
    }
    gwy_file_abandon_contents(buffer, size, NULL);

    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);
        process_metadata(stpfile, i, container);
    }
    stpfile_free(stpfile);

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
stpfile_free(STPFile *stpfile)
{
    guint i;

    for (i = 0; i < stpfile->n; i++)
        g_hash_table_destroy(stpfile->buffers[i].meta);

    g_free(stpfile->buffers);
    g_hash_table_destroy(stpfile->meta);
    g_free(stpfile);
}

static guint
file_read_header(STPFile *stpfile,
                 gchar *buffer)
{
    STPData *data = NULL;
    GHashTable *meta;
    gchar *line, *key, *value = NULL;

    stpfile->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          g_free, g_free);
    meta = stpfile->meta;
    while ((line = gwy_str_next_line(&buffer))) {
        if (!strncmp(line, "buffer_id     ", KEY_LEN)) {
            gwy_debug("buffer id = %s", line + KEY_LEN);
            stpfile->n++;
            stpfile->buffers = g_renew(STPData, stpfile->buffers, stpfile->n);
            data = stpfile->buffers + (stpfile->n - 1);
            data->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                               g_free, g_free);
            data->data = NULL;
            data->xres = data->yres = 0;
            data->id = atol(line + KEY_LEN);
            meta = data->meta;
        }
        if (line[0] == ' ')
            continue;

        key = g_strstrip(g_strndup(line, KEY_LEN));
        value = g_strstrip(g_strdup(line + KEY_LEN));
        g_hash_table_replace(meta, key, value);

        if (data) {
            if (!strcmp(key, "samples_x"))
                data->xres = atol(value);
            if (!strcmp(key, "samples_y"))
                data->yres = atol(value);
        }
    }

    return stpfile->n;
}

static gboolean
stpfile_get_double(GHashTable *meta,
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
process_metadata(STPFile *stpfile,
                 guint id,
                 GwyContainer *container)
{
    STPData *data;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble q, r;
    gchar *p, *s;
    const gchar *title;
    guint mode;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(container,
                                                             "/0/data"));

    data = stpfile->buffers + id;
    if ((p = g_hash_table_lookup(data->meta, "source_mode"))) {
        mode = atol(p);
        title = gwy_enum_to_string(mode, channels, G_N_ELEMENTS(channels));
        if (title)
            gwy_container_set_string_by_name(container, "/filename/title",
                                             g_strdup(title));
    }
    else
        mode = 1;

    /* Fix value scale */
    switch (mode) {
        case 1:
        case 9:
        case 10:
        if (stpfile_get_double(stpfile->meta, "z_v_to_angs", &q)
            && stpfile_get_double(stpfile->meta, "max_z_volt", &r)) {
            gwy_data_field_multiply(dfield, q*r/32768*Angstrom);
            gwy_debug("z_v_to_angs = %g, max_z_volt = %g", q, r);
        }
        else
            gwy_data_field_multiply(dfield, Angstrom/32768);
        break;

        case 2:
        case 8:
        case 12:
        case 13:
        case 14:
        r = 1.0;
        stpfile_get_double(stpfile->meta, "convers_coeff", &r);
        gwy_data_field_add(dfield, -32768.0);
        gwy_data_field_multiply(dfield, 10.0/32768.0);
        siunit = gwy_si_unit_new("V");
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);
        break;

        default:
        g_warning("Unknown mode %u", mode);
        gwy_data_field_multiply(dfield, Angstrom/32768);
        break;
    }

    /* Fix lateral scale */
    if (!stpfile_get_double(data->meta, "length_x", &r)) {
        g_warning("Missing or invalid x length");
        r = 1e-6;
    }
    gwy_data_field_set_xreal(dfield, r*Angstrom);

    if (!stpfile_get_double(data->meta, "length_y", &r)) {
        g_warning("Missing or invalid y length");
        r = 1e-6;
    }
    gwy_data_field_set_yreal(dfield, r*Angstrom);

    /* Metadata */
    if ((p = g_hash_table_lookup(stpfile->meta, "software")))
        gwy_container_set_string_by_name(container, "/meta/Software",
                                         g_strdup(p));
    if ((p = g_hash_table_lookup(stpfile->meta, "op_mode")))
        gwy_container_set_string_by_name(container, "/meta/Operation mode",
                                         g_strdup(p));

    /* Local metadata */
    if ((p = g_hash_table_lookup(data->meta, "Date"))
        && (s = g_hash_table_lookup(data->meta, "time")))
        gwy_container_set_string_by_name(container, "/meta/Date",
                                         g_strconcat(p, " ", s, NULL));
    if ((p = g_hash_table_lookup(data->meta, "tip_bias")))
        gwy_container_set_string_by_name(container, "/meta/Bias",
                                         g_strdup_printf("%s V", p));
    if ((p = g_hash_table_lookup(data->meta, "tip_bias")))
        gwy_container_set_string_by_name(container, "/meta/Bias",
                                         g_strdup_printf("%s V", p));
    if ((p = g_hash_table_lookup(data->meta, "line_freq")))
        gwy_container_set_string_by_name(container, "/meta/Line frequency",
                                         g_strdup_printf("%s Hz", p));
    if ((p = g_hash_table_lookup(data->meta, "scan_dir"))) {
        if (!strcmp(p, "0"))
            gwy_container_set_string_by_name(container,
                                             "/meta/Scanning direction",
                                             g_strdup("Top to bottom"));
        else if (strcmp(p, "1"))
            gwy_container_set_string_by_name(container,
                                             "/meta/Scanning direction",
                                             g_strdup("Bottom to top"));
    }
}

static void
read_data_field(GwyDataField *dfield,
                STPData *stpdata)
{
    gdouble *data;
    const guint16 *row;
    gint i, j, xres, yres;

    xres = stpdata->xres;
    yres = stpdata->yres;
    gwy_data_field_resample(dfield, xres, yres, GWY_INTERPOLATION_NONE);
    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < yres; i++) {
        row = stpdata->data + (yres-1 - i)*xres;
        for (j = 0; j < xres; j++)
            data[i*xres + j] = GUINT16_FROM_LE(row[j]);
    }
}

static guint
select_which_data(STPFile *stpfile)
{
    STPControls controls;
    STPData *data;
    GtkWidget *dialog, *label, *vbox, *hbox, *align;
    GwyDataField *dfield;
    GwyEnum *choices;
    GtkObject *layer;
    GSList *radio, *rl;
    guint i, b, mode;
    gchar *p;
    const gchar *title;

    if (!stpfile->n)
        return (guint)-1;

    if (stpfile->n == 1)
        return 0;

    controls.file = stpfile;
    choices = g_new(GwyEnum, stpfile->n);
    for (i = 0; i < stpfile->n; i++) {
        data = stpfile->buffers + i;
        choices[i].value = i;
        title = NULL;
        if ((p = g_hash_table_lookup(data->meta, "source_mode"))) {
            mode = atol(p);
            title = gwy_enum_to_string(mode, channels, G_N_ELEMENTS(channels));
        }
        if (title)
            choices[i].name = g_strdup_printf(_("Buffer %u (%s)"),
                                              stpfile->buffers[i].id, title);
        else
            choices[i].name = g_strdup_printf(_("Buffer %u"),
                                              stpfile->buffers[i].id);
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

    radio = gwy_radio_buttons_create(choices, stpfile->n, "data",
                                     G_CALLBACK(selection_changed), &controls,
                                     0);
    for (i = 0, rl = radio; rl; i++, rl = g_slist_next(rl))
        gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(rl->data), TRUE, TRUE, 0);

    /* preview */
    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    controls.data = gwy_container_new();
    dfield = gwy_data_field_new(stpfile->buffers[0].xres,
                                stpfile->buffers[0].yres,
                                1.0, 1.0, FALSE);
    read_data_field(dfield, stpfile->buffers);
    gwy_container_set_object_by_name(controls.data, "/0/data",
                                     (GObject*)dfield);
    g_object_unref(dfield);

    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.data_view),
                           120.0/MAX(stpfile->buffers[0].xres,
                                     stpfile->buffers[0].yres));
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(GWY_PIXMAP_LAYER(layer), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view),
                                 GWY_PIXMAP_LAYER(layer));
    gtk_container_add(GTK_CONTAINER(align), controls.data_view);

    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
    switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
        gtk_widget_destroy(dialog);
        case GTK_RESPONSE_NONE:
        b = (guint)-1;
        break;

        case GTK_RESPONSE_OK:
        b = GPOINTER_TO_UINT(gwy_radio_buttons_get_current(radio, "data"));
        gtk_widget_destroy(dialog);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    for (i = 0; i < stpfile->n; i++)
        g_free((gpointer)choices[i].name);
    g_free(choices);

    return b;
}

static void
selection_changed(GtkWidget *button,
                  STPControls *controls)
{
    STPFile *stpfile;
    GwyDataField *dfield;
    guint i;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    i = gwy_radio_buttons_get_current_from_widget(button, "data");
    g_assert(i != (guint)-1);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->data,
                                                             "/0/data"));
    stpfile = controls->file;
    read_data_field(dfield, stpfile->buffers + i);
    gwy_data_field_data_changed(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

