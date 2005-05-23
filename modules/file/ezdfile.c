/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klezdtek.
 *  E-mail: yeti@gwyddion.net, klezdtek@gwyddion.net.
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

#include "get.h"

#define MAGIC "[DataSet]\r\n"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define DATA_MAGIC "#!"
#define DATA_MAGIC_SIZE (sizeof(DATA_MAGIC) - 1)

#define EXTENSION ".ezd"

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
static GwyContainer* ezdfile_load          (const gchar *filename);
static guint         find_data_start       (const guchar *buffer,
                                            gsize size);
static void          ezdfile_free          (GPtrArray *ezdfile);
static void          read_data_field       (GwyDataField *dfield,
                                            EZDSection *section);
static gboolean      file_read_header      (GPtrArray *ezdfile,
                                            gchar *buffer);
static guint         find_data_offsets     (const gchar *buffer,
                                            gsize size,
                                            GPtrArray *ezdfile);
static void          process_metadata      (GPtrArray *ezdfile,
                                            GwyContainer *container);
static void          fix_scales            (EZDSection *section,
                                            GwyContainer *container);
static guint         select_which_data     (GPtrArray *ezdfile,
                                            guint ndata);
static void          selection_changed     (GtkWidget *button,
                                            EZDControls *controls);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanosurf EZD data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
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
        N_("Nanosurf files (.ezd)"),
        (GwyFileDetectFunc)&ezdfile_detect,
        (GwyFileLoadFunc)&ezdfile_load,
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
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && !memcmp(fileinfo->buffer, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static GwyContainer*
ezdfile_load(const gchar *filename)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    EZDSection *section = NULL;
    GwyDataField *dfield = NULL;
    GPtrArray *ezdfile;
    guint header_size, i;
    gchar *p;
    gboolean ok;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (strncmp(buffer, MAGIC, MAGIC_SIZE)
        || !(header_size = find_data_start(buffer, size))) {
        g_warning("File %s is not a EZD file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    ezdfile = g_ptr_array_new();
    p = g_strndup(buffer, header_size - DATA_MAGIC_SIZE);
    ok = file_read_header(ezdfile, p);
    g_free(p);

    if (ok) {
        if ((i = find_data_offsets(buffer + header_size, size - header_size,
                                   ezdfile))) {
            if ((i = select_which_data(ezdfile, i)) != (guint)-1) {
                section = (EZDSection*)g_ptr_array_index(ezdfile, i);
                dfield = gwy_data_field_new(section->xres, section->yres,
                                            1.0, 1.0, FALSE);
                read_data_field(dfield, section);
            }
        }
    }
    gwy_file_abandon_contents(buffer, size, NULL);

    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);
        fix_scales(section, container);
        process_metadata(ezdfile, container);
    }
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
                 gchar *buffer)
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
            g_warning("Garbage before header");
            continue;
        }
        /* Skip comments */
        if (g_str_has_prefix(line, "--"))
            continue;

        p = strchr(line, '=');
        if (!p) {
            g_warning("Cannot parse line <%s>", line);
            continue;
        }
        *p = '\0';
        p++;

        if (!strcmp(line, "SaveMode")) {
            if (strcmp(p, "Binary"))
                g_warning("SaveMode is not Binary, this is not supported");
        }
        else if (!strcmp(line, "SaveBits"))
            section->bitdepth = atol(p);
        else if (!strcmp(line, "SaveSign")) {
            section->sign = !strcmp(p, "Signed");
            if (!section->sign)
                g_warning("SaveSign is not Signed, this is not supported");
        }
        else if (!strcmp(line, "SaveOrder")) {
            if (!strcmp(p, "Intel"))
                section->byteorder = G_LITTLE_ENDIAN;
            else
                g_warning("SaveOrder is not Intel, this is not supported");
        }
        else if (!strcmp(line, "Points"))
            section->xres = atol(p);
        else if (!strcmp(line, "Lines"))
            section->yres = atol(p);
        /* FIXME: this is ugly and eventually incorrect, if dimensions can
         * be exchanged */
        else if (!strcmp(line, "Dim0Name"))
            section->xrange.name = g_strdup(p);
        else if (!strcmp(line, "Dim1Name"))
            section->yrange.name = g_strdup(p);
        else if (!strcmp(line, "Dim2Name"))
            section->zrange.name = g_strdup(p);
        else if (!strcmp(line, "Dim0Unit"))
            section->xrange.unit = g_strdup(p);
        else if (!strcmp(line, "Dim1Unit"))
            section->yrange.unit = g_strdup(p);
        else if (!strcmp(line, "Dim2Unit"))
            section->zrange.unit = g_strdup(p);
        else if (!strcmp(line, "Dim0Min"))
            section->xrange.min = g_ascii_strtod(p, NULL);
        else if (!strcmp(line, "Dim1Min"))
            section->yrange.min = g_ascii_strtod(p, NULL);
        else if (!strcmp(line, "Dim2Min"))
            section->zrange.min = g_ascii_strtod(p, NULL);
        else if (!strcmp(line, "Dim0Range"))
            section->xrange.range = g_ascii_strtod(p, NULL);
        else if (!strcmp(line, "Dim1Range"))
            section->yrange.range = g_ascii_strtod(p, NULL);
        else if (!strcmp(line, "Dim2Range"))
            section->zrange.range = g_ascii_strtod(p, NULL);
        else
            g_hash_table_replace(section->meta, g_strdup(line), g_strdup(p));
    }

    return TRUE;
}

static guint
find_data_offsets(const gchar *buffer,
                  gsize size,
                  GPtrArray *ezdfile)
{
    EZDSection *dataset, *section;
    GString *grkey;
    guint required_size = 0;
    gint ngroups, nchannels, i, j, k;
    guint ndata = 0;
    gchar *p;

    /* Sanity check */
    if (!ezdfile->len) {
        g_warning("No section found");
        return 0;
    }
    dataset = (EZDSection*)g_ptr_array_index(ezdfile, 0);
    if (strcmp(dataset->name, "DataSet")) {
        g_warning("First section isn't DataSet");
        return 0;
    }

    if (!(p = g_hash_table_lookup(dataset->meta, "GroupCount"))
        || (ngroups = atol(p)) <= 0) {
        g_warning("No or invalid GroupCount in [DataSet]");
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
                if (!strcmp(section->name, p))
                    break;
            }
            if (!section) {
                g_warning("Cannot find section for %s", p);
                continue;
            }

            /* Compute data position */
            gwy_debug("Data %s at offset %u from data start",
                      grkey->str, required_size);
            gwy_debug("xres = %d, yres = %d, bpp = %d",
                      section->xres, section->yres, section->bitdepth);
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
    if (section->zrange.name)
        gwy_container_set_string_by_name(container, "/meta/Channel name",
                                         g_strdup(section->zrange.name));
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
        if (!strcmp(section->name, "DataSet-Info"))
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

static guint
select_which_data(GPtrArray *ezdfile,
                  guint ndata)
{
    EZDControls controls;
    EZDSection *section;
    GtkWidget *dialog, *label, *vbox, *hbox, *align;
    GwyDataField *dfield;
    GwyEnum *choices;
    GtkObject *layer;
    GSList *radio, *rl;
    guint i, b;

    if (!ndata)
        return (guint)-1;

    if (ndata == 1)
        return 0;

    controls.file = ezdfile;
    choices = g_new(GwyEnum, ndata);
    for (i = b = 0; i < ezdfile->len; i++) {
        section = (EZDSection*)g_ptr_array_index(ezdfile, i);
        if (!section->data)
            continue;
        choices[b].value = i;
        choices[b].name = g_strdup_printf(_("Group %d, Channel %d"),
                                          section->group, section->channel);
        b++;
    }
    b = choices[0].value;
    section = (EZDSection*)g_ptr_array_index(ezdfile, b);

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

    radio = gwy_radio_buttons_create(choices, ndata, "data",
                                     G_CALLBACK(selection_changed), &controls,
                                     b);
    for (i = 0, rl = radio; rl; i++, rl = g_slist_next(rl))
        gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(rl->data), TRUE, TRUE, 0);

    /* preview */
    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    controls.data = gwy_container_new();
    dfield = gwy_data_field_new(section->xres, section->yres, 1.0, 1.0, FALSE);
    read_data_field(dfield, section);
    gwy_container_set_object_by_name(controls.data, "/0/data",
                                     (GObject*)dfield);
    g_object_unref(dfield);

    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.data_view),
                           120.0/MAX(section->xres, section->yres));
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

    for (i = 0; i < ndata; i++)
        g_free((gpointer)choices[i].name);
    g_free(choices);

    return b;
}

static void
selection_changed(GtkWidget *button,
                  EZDControls *controls)
{
    GPtrArray *ezdfile;
    GwyDataField *dfield;
    guint i;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    i = gwy_radio_buttons_get_current_from_widget(button, "data");
    g_assert(i != (guint)-1);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->data,
                                                             "/0/data"));
    ezdfile = controls->file;
    read_data_field(dfield, (EZDSection*)g_ptr_array_index(ezdfile, i));
    gwy_data_field_data_changed(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

