/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klrhktek.
 *  E-mail: yeti@gwyddion.net, klrhktek@gwyddion.net.
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

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>

#include <string.h>
#include <stdlib.h>

#define HEADER_SIZE 512

#define MAGIC "STiMage 3.1"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)
#define EXTENSION ".sm2"

typedef enum {
    RHK_TYPE_IMAGE =            0,
    RHK_TYPE_LINE =             1,
    RHK_TYPE_ANNOTATED_LINE =   3
} RHKType;

typedef enum {
    RHK_DATA_SIGNLE    = 0,
    RHK_DATA_INT16     = 1,
    RHK_DATA_INT32     = 2,
    RHK_DATA_INT8      = 3
} RHKDataType;

typedef enum {
    RHK_IMAGE_UNDEFINED                = 0,
    RHK_IMAGE_TOPOGAPHIC               = 1,
    RHK_IMAGE_CURRENT                  = 2,
    RHK_IMAGE_AUX                      = 3,
    RHK_IMAGE_FORCE                    = 4,
    RHK_IMAGE_SIGNAL                   = 5,
    RHK_IMAGE_FFT                      = 6,
    RHK_IMAGE_NOISE_POWER_SPECTRUM     = 7,
    RHK_IMAGE_LINE_TEST                = 8,
    RHK_IMAGE_OSCILLOSCOPE             = 9,
    RHK_IMAGE_IV_SPECTRA               = 10,
    RHK_IMAGE_IV_4x4                   = 11,
    RHK_IMAGE_IV_8x8                   = 12,
    RHK_IMAGE_IV_16x16                 = 13,
    RHK_IMAGE_IV_32x32                 = 14,
    RHK_IMAGE_IV_CENTER                = 15,
    RHK_IMAGE_INTERACTIVE_SPECTRA      = 16,
    RHK_IMAGE_AUTOCORRELATION          = 17,
    RHK_IMAGE_IZ_SPECTRA               = 18,
    RHK_IMAGE_4_GAIN_TOPOGRAPHY        = 19,
    RHK_IMAGE_8_GAIN_TOPOGRAPHY        = 20,
    RHK_IMAGE_4_GAIN_CURRENT           = 21,
    RHK_IMAGE_8_GAIN_CURRENT           = 22,
    RHK_IMAGE_IV_64x64                 = 23,
    RHK_IMAGE_AUTOCORRELATION_SPECTRUM = 24,
    RHK_IMAGE_COUNTER                  = 25,
    RHK_IMAGE_MULTICHANNEL_ANALYSER    = 26,
    RHK_IMAGE_AFM_100                  = 27,
    RHK_IMAGE_LAST
} RHKPageType;

typedef struct {
    gdouble scale;
    gdouble offset;
    gchar *units;
} RHKRange;

typedef struct {
    gchar *date;
    guint xres;
    guint yres;
    RHKType type;
    RHKDataType data_type;
    guint item_size;
    guint line_type;
    guint size;
    RHKPageType page_type;
    RHKRange x;
    RHKRange y;
    RHKRange z;
    gdouble xyskew;
    gdouble alpha;
    RHKRange iv;
    guint scan;
    gdouble period;
    guint id;
    guint data_offset;
    gchar *label;
    gchar *comment;

    const guchar *buffer;
} RHKPage;

typedef struct {
    GArray *file;
    GwyContainer *data;
    GtkWidget *data_view;
} RHKControls;

static gboolean      module_register         (const gchar *name);
static gint          rhkspm32_detect         (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* rhkspm32_load           (const gchar *filename);
static gboolean      rhkspm32_read_header    (RHKPage *rhkpage);
static gboolean      rhkspm32_read_range     (const gchar *buffer,
                                              const gchar *name,
                                              RHKRange *range);
static void          rhkspm32_free           (RHKPage *rhkpage);
static void          rhkspm32_store_metadata (RHKPage *rhkpage,
                                              GwyContainer *container);
static GwyDataField* rhkspm32_read_data      (RHKPage *rhkpage,
                                              GwyDataField *dfield);
static guint         select_which_data       (GArray *rhkfile);
static void          selection_changed       (GtkWidget *button,
                                              RHKControls *controls);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports RHK Technology SPM32 data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo rhkspm32_func_info = {
        "rhk-spm32",
        N_("RHK SPM32 files (.sm2)"),
        (GwyFileDetectFunc)&rhkspm32_detect,
        (GwyFileLoadFunc)&rhkspm32_load,
        NULL
    };

    gwy_file_func_register(name, &rhkspm32_func_info);

    return TRUE;
}

static gint
rhkspm32_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->buffer, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
rhkspm32_load(const gchar *filename)
{
    GArray *rhkfile;
    RHKPage *rhkpage;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gsize totalpos, pagesize;
    const gchar *message = NULL;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < HEADER_SIZE) {
        g_warning("File %s is not a RHK SPM32 file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    rhkfile = g_array_new(FALSE, TRUE, sizeof(RHKPage));
    totalpos = 0;

    while (totalpos < size) {
        g_array_set_size(rhkfile, rhkfile->len + 1);
        rhkpage = &g_array_index(rhkfile, RHKPage, rhkfile->len - 1);
        rhkpage->buffer = buffer + totalpos;
        if (!rhkspm32_read_header(rhkpage)) {
            g_array_set_size(rhkfile, rhkfile->len - 1);
            message = "Cannot parse file header";
            break;
        }

        pagesize = rhkpage->data_offset
                   + rhkpage->item_size*rhkpage->xres*rhkpage->yres;
        if (size < totalpos + pagesize) {
            rhkspm32_free(rhkpage);
            g_array_set_size(rhkfile, rhkfile->len - 1);
            message = "Truncated file";
            break;
        }

        totalpos += pagesize;
    }

    i = select_which_data(rhkfile);
    if (i != (guint)-1) {
        rhkpage = &g_array_index(rhkfile, RHKPage, i);
        dfield = rhkspm32_read_data(rhkpage, NULL);
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);
        rhkspm32_store_metadata(rhkpage, container);
    }
    else if (message)
        g_warning("%s", message);

    gwy_file_abandon_contents(buffer, size, NULL);
    for (i = 0; i < rhkfile->len; i++)
        rhkspm32_free(&g_array_index(rhkfile, RHKPage, i));
    g_array_free(rhkfile, TRUE);

    return container;
}

static gboolean
rhkspm32_read_header(RHKPage *rhkpage)
{
    const gchar *buffer;
    gchar *end;
    guint pos;

    buffer = rhkpage->buffer;

    rhkpage->date = g_strstrip(g_strndup(buffer + MAGIC_SIZE,
                                         0x20 - MAGIC_SIZE));
    if (sscanf(buffer + 0x20, "%d %d %d %d %d %d %d",
               (gint*)&rhkpage->type, (gint*)&rhkpage->data_type,
               &rhkpage->line_type,
               &rhkpage->xres, &rhkpage->yres, &rhkpage->size,
               (gint*)&rhkpage->page_type) != 7
        || rhkpage->xres <= 0 || rhkpage->yres <= 0)
        return FALSE;
    gwy_debug("type = %u, data = %u, line = %u, image = %u",
              rhkpage->type, rhkpage->data_type, rhkpage->line_type,
              rhkpage->page_type);
    gwy_debug("xres = %d, yres = %d", rhkpage->xres, rhkpage->yres);

    if (rhkpage->type != RHK_TYPE_IMAGE) {
        g_warning("Cannot read non-image files");
        return FALSE;
    }
    /* FIXME */
    if (rhkpage->data_type != RHK_DATA_INT16) {
        g_warning("Cannot read images with data type != int16");
        return FALSE;
    }
    rhkpage->item_size = 2;

    if (!rhkspm32_read_range(buffer + 0x40, "X", &rhkpage->x)
        || !rhkspm32_read_range(buffer + 0x60, "Y", &rhkpage->y)
        || !rhkspm32_read_range(buffer + 0x80, "Z", &rhkpage->z))
        return FALSE;

    if (!g_str_has_prefix(buffer + 0xa0, "XY "))
        return FALSE;
    pos = 0xa0 + sizeof("XY");
    rhkpage->xyskew = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos)
        return FALSE;
    pos = (end - buffer) + 2;
    rhkpage->alpha = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos)
        return FALSE;

    if (!rhkspm32_read_range(buffer + 0xc0, "IV", &rhkpage->iv))
        return FALSE;

    if (!g_str_has_prefix(buffer + 0xe0, "scan "))
    pos = 0xe0 + sizeof("scan");
    rhkpage->scan = strtol(buffer + pos, &end, 10);
    if (end == buffer + pos)
        return FALSE;
    pos = (end - buffer);
    rhkpage->period = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos)
        return FALSE;

    if (sscanf(buffer + 0x100, "id %u %u",
               &rhkpage->id, &rhkpage->data_offset) != 2)
        return FALSE;
    gwy_debug("data_offset = %u", rhkpage->data_offset);
    if (rhkpage->data_offset < HEADER_SIZE)
        return FALSE;

    rhkpage->label = g_strstrip(g_strndup(buffer + 0x140, 0x20));
    rhkpage->comment = g_strstrip(g_strndup(buffer + 0x160,
                                           HEADER_SIZE - 0x160));

    return TRUE;
}

static gboolean
rhkspm32_read_range(const gchar *buffer,
                    const gchar *name,
                    RHKRange *range)
{
    gchar *end;
    guint pos;

    if (!g_str_has_prefix(buffer, name))
        return FALSE;
    pos = strlen(name) + 1;

    range->scale = fabs(g_ascii_strtod(buffer + pos, &end));
    if (end == buffer + pos || pos > 0x20)
        return FALSE;
    pos = end - buffer;

    range->offset = g_ascii_strtod(buffer + pos, &end);
    if (end == buffer + pos || pos > 0x20)
        return FALSE;
    pos = end - buffer;

    range->units = g_strstrip(g_strndup(buffer + pos, 0x20 - pos));
    gwy_debug("<%s> %g %g <%s>",
              name, range->scale, range->offset, range->units);

    return TRUE;
}

static void
rhkspm32_free(RHKPage *rhkpage)
{
    g_free(rhkpage->date);
    g_free(rhkpage->x.units);
    g_free(rhkpage->y.units);
    g_free(rhkpage->z.units);
    g_free(rhkpage->iv.units);
    g_free(rhkpage->label);
    g_free(rhkpage->comment);
}

static void
rhkspm32_store_metadata(RHKPage *rhkpage,
                        GwyContainer *container)
{
    const GwyEnum page_types[] = {
        { "Topographic",              RHK_IMAGE_TOPOGAPHIC               },
        { "Current",                  RHK_IMAGE_CURRENT                  },
        { "Aux",                      RHK_IMAGE_AUX                      },
        { "Force",                    RHK_IMAGE_FORCE                    },
        { "Signal",                   RHK_IMAGE_SIGNAL                   },
        { "FFT transform",            RHK_IMAGE_FFT                      },
        { "Noise power spectrum",     RHK_IMAGE_NOISE_POWER_SPECTRUM     },
        { "Line test",                RHK_IMAGE_LINE_TEST                },
        { "Oscilloscope",             RHK_IMAGE_OSCILLOSCOPE             },
        { "IV spectra",               RHK_IMAGE_IV_SPECTRA               },
        { "Image IV 4x4",             RHK_IMAGE_IV_4x4                   },
        { "Image IV 8x8",             RHK_IMAGE_IV_8x8                   },
        { "Image IV 16x16",           RHK_IMAGE_IV_16x16                 },
        { "Image IV 32x32",           RHK_IMAGE_IV_32x32                 },
        { "Image IV Center",          RHK_IMAGE_IV_CENTER                },
        { "Interactive spectra",      RHK_IMAGE_INTERACTIVE_SPECTRA      },
        { "Autocorrelation",          RHK_IMAGE_AUTOCORRELATION          },
        { "IZ spectra",               RHK_IMAGE_IZ_SPECTRA               },
        { "4 gain topography",        RHK_IMAGE_4_GAIN_TOPOGRAPHY        },
        { "8 gain topography",        RHK_IMAGE_8_GAIN_TOPOGRAPHY        },
        { "4 gain current",           RHK_IMAGE_4_GAIN_CURRENT           },
        { "8 gain current",           RHK_IMAGE_8_GAIN_CURRENT           },
        { "Image IV 64x64",           RHK_IMAGE_IV_64x64                 },
        { "Autocorrelation spectrum", RHK_IMAGE_AUTOCORRELATION_SPECTRUM },
        { "Counter data",             RHK_IMAGE_COUNTER                  },
        { "Multichannel analyser",    RHK_IMAGE_MULTICHANNEL_ANALYSER    },
        { "AFM using AFM-100",        RHK_IMAGE_AFM_100                  },
    };
    const gchar *s;

    gwy_container_set_string_by_name(container, "/meta/Tunneling voltage",
                                     g_strdup_printf("%g mV",
                                                     1e3*rhkpage->iv.offset));
    gwy_container_set_string_by_name(container, "/meta/Current",
                                     g_strdup_printf("%g nA",
                                                     1e9*rhkpage->iv.scale));
    gwy_container_set_string_by_name(container, "/meta/Id",
                                     g_strdup_printf("%u", rhkpage->id));
    if (rhkpage->date && *rhkpage->date)
        gwy_container_set_string_by_name(container, "/meta/Date",
                                         g_strdup(rhkpage->date));
    if (rhkpage->comment && *rhkpage->comment)
        gwy_container_set_string_by_name(container, "/meta/Comment",
                                         g_strdup(rhkpage->comment));
    if (rhkpage->label && *rhkpage->label) {
        gwy_container_set_string_by_name(container, "/meta/Label",
                                         g_strdup(rhkpage->label));
        gwy_container_set_string_by_name(container, "/filename/title",
                                         g_strdup(rhkpage->label));
    }

    s = gwy_enum_to_string(rhkpage->page_type,
                           page_types, G_N_ELEMENTS(page_types));
    if (s && *s)
        gwy_container_set_string_by_name(container, "/meta/Image type",
                                         g_strdup(s));
}

static GwyDataField*
rhkspm32_read_data(RHKPage *rhkpage,
                   GwyDataField *dfield)
{
    const guint16 *p;
    GwySIUnit *siunit;
    gdouble *data;
    guint i;

    p = (const guint16*)(rhkpage->buffer + rhkpage->data_offset);
    if (!dfield)
        dfield = gwy_data_field_new(rhkpage->xres, rhkpage->yres,
                                    rhkpage->xres * rhkpage->x.scale,
                                    rhkpage->yres * rhkpage->y.scale,
                                    FALSE);
    else {
        gwy_data_field_resample(dfield, rhkpage->xres, rhkpage->yres,
                                GWY_INTERPOLATION_NONE);
        gwy_data_field_set_xreal(dfield, rhkpage->xres * rhkpage->x.scale);
        gwy_data_field_set_yreal(dfield, rhkpage->yres * rhkpage->y.scale);
    }

    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < rhkpage->xres*rhkpage->yres; i++)
        data[i] = GINT16_FROM_LE(p[i]);

    gwy_data_field_multiply(dfield, rhkpage->z.scale);

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_set_unit_string(siunit, rhkpage->x.units);

    siunit = gwy_data_field_get_si_unit_z(dfield);
    gwy_si_unit_set_unit_string(siunit, rhkpage->z.units);

    return dfield;
}

static guint
select_which_data(GArray *rhkfile)
{
    RHKControls controls;
    RHKPage *rhkpage;
    GtkWidget *dialog, *label, *vbox, *hbox, *align;
    GwyDataField *dfield;
    GwyEnum *choices;
    GwyPixmapLayer *layer;
    GSList *radio, *rl;
    guint i, b = (guint)-1;

    if (!rhkfile->len)
        return b;

    if (rhkfile->len == 1)
        return 0;

    controls.file = rhkfile;
    choices = g_new(GwyEnum, rhkfile->len);
    for (i = 0; i < rhkfile->len; i++) {
        rhkpage = &g_array_index(rhkfile, RHKPage, i);
        choices[i].value = i;
        choices[i].name = g_strdup_printf(_("Page %u (%s)"),
                                          i+1, rhkpage->label);
    }
    rhkpage = &g_array_index(rhkfile, RHKPage, 0);

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

    radio = gwy_radio_buttons_create(choices, rhkfile->len, "data",
                                     G_CALLBACK(selection_changed), &controls,
                                     0);
    for (i = 0, rl = radio; rl; i++, rl = g_slist_next(rl))
        gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(rl->data), TRUE, TRUE, 0);

    /* preview */
    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    controls.data = gwy_container_new();
    dfield = rhkspm32_read_data(rhkpage, NULL);
    gwy_container_set_object_by_name(controls.data, "data", dfield);
    gwy_container_set_enum_by_name(controls.data, "range-type",
                                   GWY_LAYER_BASIC_RANGE_AUTO);
    g_object_unref(dfield);

    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.data_view),
                           120.0/MAX(rhkpage->xres, rhkpage->yres));
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "data");
    gwy_layer_basic_set_range_type_key(GWY_LAYER_BASIC(layer), "range-type");
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

    for (i = 0; i < rhkfile->len; i++)
        g_free((gpointer)choices[i].name);
    g_free(choices);

    return b;
}

static void
selection_changed(GtkWidget *button,
                  RHKControls *controls)
{
    RHKPage *rhkpage;
    GwyDataField *dfield;
    guint i;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    i = gwy_radio_buttons_get_current_from_widget(button, "data");
    g_assert(i != (guint)-1);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->data,
                                                             "data"));
    rhkpage = &g_array_index(controls->file, RHKPage, i);
    rhkspm32_read_data(rhkpage, dfield);
    gwy_data_field_data_changed(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

