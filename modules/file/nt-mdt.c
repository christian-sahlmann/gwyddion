/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
#define DEBUG 1
#include <libgwyddion/gwymacros.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>

#define MAGIC "\x01\xb0\x93\xff"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".mdt"

#define Angstrom (1e-10)

typedef enum {
    MDT_FRAME_SCANNED      = 0,
    MDT_FRAME_SPECTROSCOPY = 1,
    MDT_FRAME_TEXT         = 3,
    MDT_FRAME_OLD_MDA      = 105,
    MDT_FRAME_MDA          = 106,
    MDT_FRAME_PALETTE      = 107
} MDTFrameType;

typedef enum {
    MDT_UNIT_RAMAN_SHIFT     = -10,
    MDT_UNIT_RESERVED0       = -9,
    MDT_UNIT_RESERVED1       = -8,
    MDT_UNIT_RESERVED2       = -7,
    MDT_UNIT_RESERVED3       = -6,
    MDT_UNIT_METER           = -5,
    MDT_UNIT_CENTIMETER      = -4,
    MDT_UNIT_MILLIMETER      = -3,
    MDT_UNIT_MIKROMETER      = -2,
    MDT_UNIT_NANOMETER       = -1,
    MDT_UNIT_ANGSTROM        = 0,
    MDT_UNIT_NANOAMPERE      = 1,
    MDT_UNIT_VOLT            = 2,
    MDT_UNIT_NONE            = 3,
    MDT_UNIT_KILOHERZ        = 4,
    MDT_UNIT_DEGREES         = 5,
    MDT_UNIT_PERCENT         = 6,
    MDT_UNIT_CELSIUM_DEGREE  = 7,
    MDT_UNIT_VOLT_HIGH       = 8,
    MDT_UNIT_SECOND          = 9,
    MDT_UNIT_MILLISECOND     = 10,
    MDT_UNIT_MIKROSECOND     = 11,
    MDT_UNIT_NANOSECOND      = 12,
    MDT_UNIT_COUNTS          = 13,
    MDT_UNIT_PIXELS          = 14,
    MDT_UNIT_RESERVED_SFOM0  = 15,
    MDT_UNIT_RESERVED_SFOM1  = 16,
    MDT_UNIT_RESERVED_SFOM2  = 17,
    MDT_UNIT_RESERVED_SFOM3  = 18,
    MDT_UNIT_RESERVED_SFOM4  = 19,
    MDT_UNIT_AMPERE2         = 20,
    MDT_UNIT_MILLIAMPERE     = 21,
    MDT_UNIT_MIKROAMPERE     = 22,
    MDT_UNIT_NANOAMPERE2     = 23,
    MDT_UNIT_PICOAMPERE      = 24,
    MDT_UNIT_VOLT2           = 25,
    MDT_UNIT_MILLIVOLT       = 26,
    MDT_UNIT_MIKROVOLT       = 27,
    MDT_UNIT_NANOVOLT        = 28,
    MDT_UNIT_PICOVOLT        = 29,
    MDT_UNIT_NEWTON          = 30,
    MDT_UNIT_MILLINEWTON     = 31,
    MDT_UNIT_MIKRONEWTON     = 32,
    MDT_UNIT_NANONEWTON      = 33,
    MDT_UNIT_PICONEWTON      = 34,
    MDT_UNIT_RESERVED_DOS0   = 35,
    MDT_UNIT_RESERVED_DOS1   = 36,
    MDT_UNIT_RESERVED_DOS2   = 37,
    MDT_UNIT_RESERVED_DOS3   = 38,
    MDT_UNIT_RESERVED_DOS4   = 39
} MDTUnit;

typedef enum {
    MDT_MODE_STM = 0,
    MDT_MODE_AFM = 1
} MDTMode;

typedef enum {
    MDT_INPUT_EXTENSION_SLOT = 0,
    MDT_INPUT_BIAS_V         = 1,
    MDT_INPUT_GROUND         = 2
} MDTInputSignal;

typedef enum {
    MDT_TUNE_STEP  = 0,
    MDT_TUNE_FINE  = 1,
    MDT_TUNE_SLOPE = 2
} MDTLiftMode;

enum {
    FILE_HEADER_SIZE = 32,
    FRAME_HEADER_SIZE = 22,
    FRAME_MODE_SIZE = 8,
    AXIS_SCALES_SIZE = 30,
    SCAN_VARS_MIN_SIZE = 77,
    SPECTRO_VARS_MIN_SIZE = 38
};

typedef struct {
    gdouble offset;    /* r0 (physical units) */
    gdouble step;    /* r (physical units) */
    MDTUnit unit;    /* U */
} MDTAxisScale;

typedef struct {
    MDTAxisScale x_scale;
    MDTAxisScale y_scale;
    MDTAxisScale z_scale;
    gint channel_index;    /* s_mode */
    MDTMode mode;    /* s_dev */
    gint xres;    /* s_nx */
    gint yres;    /* s_ny */
    gint ndacq;    /* s_rv6; obsolete */
    gdouble step_length;    /* s_rs */
    guint adt;    /* s_adt */
    guint adc_gain_amp_log10;    /* s_adc_a */
    guint adc_index;    /* s_a12 */
    /* XXX: Some fields have different meaning in different versions */
    union {
        guint input_signal;    /* MDTInputSignal smp_in; s_smp_in */
        guint version;    /* s_8xx */
    } s16;
    union {
        guint substr_plane_order;    /* s_spl */
        guint pass_num;    /* z_03 */
    } s17;
    guint scan_dir;    /* s_xy TODO: interpretation */
    gboolean power_of_2;    /* s_2n */
    gdouble velocity;    /* s_vel (Angstrom/second) */
    gdouble setpoint;    /* s_i0 */
    gdouble bias_voltage;    /* s_ut */
    gboolean draw;    /* s_draw */
    gint xoff;    /* s_x00 (in DAC quants) */
    gint yoff;    /* s_y00 (in DAC quants) */
    gboolean nl_corr;    /* s_cor */
#if 0
    guint orig_format;    /* s_oem */
    MDTLiftMode tune;    /* z_tune */
    gdouble feedback_gain;    /* s_fbg */
    gint dac_scale;    /* s_s */
    gint overscan;    /* s_xov (in %) */
#endif
    /* XXX: much more stuff here */

    /* Frame mode stuff */
    guint fm_mode;    /* m_mode */
    guint fm_xres;    /* m_nx */
    guint fm_yres;    /* m_ny */
    guint fm_ndots;    /* m_nd */

    /* Data */
    const guchar *dots;
    const guchar *image;
} MDTScannedDataFrame;

typedef struct {
    gsize size;     /* h_sz */
    MDTFrameType type;     /* h_what */
    gint version;  /* h_ver0, h_ver1 */

    gint year;    /* h_yea */
    gint month;    /* h_mon */
    gint day;    /* h_day */
    gint hour;    /* h_h */
    gint min;    /* h_m */
    gint sec;    /* h_s */

    gint var_size;    /* h_am, v6 and older only */

    gpointer frame_data;
} MDTFrame;

typedef struct {
    gsize size;  /* f_sz */
    gsize last_frame; /* f_nt */
    MDTFrame *frames;
} MDTFile;

static gboolean       module_register     (const gchar *name);
static gint           mdt_detect          (const gchar *filename,
                                           gboolean only_name);
static GwyContainer*  mdt_load            (const gchar *filename);
static gsize          select_which_data   (MDTFile *mdtfile,
                                           GwyEnum *choices,
                                           gsize n);
static GwyDataField*  extract_data        (MDTFile *mdtfile,
                                           gsize ch,
                                           gsize im);
static void           add_metadata        (MDTFile *mdtfile,
                                           gsize ch,
                                           gsize im,
                                           GwyContainer *data);
static gboolean       mdt_real_load       (const guchar *buffer,
                                           gsize size,
                                           MDTFile *mdtfile);
GwyDataField*         extract_scanned_data(MDTScannedDataFrame *dataframe);

static const GwyEnum frame_types[] = {
    { "Scanned",      MDT_FRAME_SCANNED },
    { "Spectroscopy", MDT_FRAME_SPECTROSCOPY },
    { "Text",         MDT_FRAME_TEXT },
    { "Old MDA",      MDT_FRAME_OLD_MDA },
    { "MDA",          MDT_FRAME_MDA },
    { "Palette",      MDT_FRAME_PALETTE },
};

static const GwyEnum mdt_units[] = {
    { "1/cm", MDT_UNIT_RAMAN_SHIFT },
    { "",     MDT_UNIT_RESERVED0 },
    { "",     MDT_UNIT_RESERVED1 },
    { "",     MDT_UNIT_RESERVED2 },
    { "",     MDT_UNIT_RESERVED3 },
    { "m",    MDT_UNIT_METER },
    { "cm",   MDT_UNIT_CENTIMETER },
    { "mm",   MDT_UNIT_MILLIMETER },
    { "µm",   MDT_UNIT_MIKROMETER },
    { "nm",   MDT_UNIT_NANOMETER },
    { "Å",    MDT_UNIT_ANGSTROM },
    { "nA",   MDT_UNIT_NANOAMPERE },
    { "V",    MDT_UNIT_VOLT },
    { "",     MDT_UNIT_NONE },
    { "kHz",  MDT_UNIT_KILOHERZ },
    { "deg",  MDT_UNIT_DEGREES },
    { "%",    MDT_UNIT_PERCENT },
    { "°C",   MDT_UNIT_CELSIUM_DEGREE },
    { "V",    MDT_UNIT_VOLT_HIGH },
    { "s",    MDT_UNIT_SECOND },
    { "ms",   MDT_UNIT_MILLISECOND },
    { "µs",   MDT_UNIT_MIKROSECOND },
    { "ns",   MDT_UNIT_NANOSECOND },
    { "",     MDT_UNIT_COUNTS },
    { "px",   MDT_UNIT_PIXELS },
    { "",     MDT_UNIT_RESERVED_SFOM0 },
    { "",     MDT_UNIT_RESERVED_SFOM1 },
    { "",     MDT_UNIT_RESERVED_SFOM2 },
    { "",     MDT_UNIT_RESERVED_SFOM3 },
    { "",     MDT_UNIT_RESERVED_SFOM4 },
    { "A",    MDT_UNIT_AMPERE2 },
    { "mA",   MDT_UNIT_MILLIAMPERE },
    { "µA",   MDT_UNIT_MIKROAMPERE },
    { "nA",   MDT_UNIT_NANOAMPERE2 },
    { "pA",   MDT_UNIT_PICOAMPERE },
    { "V",    MDT_UNIT_VOLT2 },
    { "mV",   MDT_UNIT_MILLIVOLT },
    { "µV",   MDT_UNIT_MIKROVOLT },
    { "nV",   MDT_UNIT_NANOVOLT },
    { "pV",   MDT_UNIT_PICOVOLT },
    { "N",    MDT_UNIT_NEWTON },
    { "mN",   MDT_UNIT_MILLINEWTON },
    { "µN",   MDT_UNIT_MIKRONEWTON },
    { "nN",   MDT_UNIT_NANONEWTON },
    { "pN",   MDT_UNIT_PICONEWTON },
    { "",     MDT_UNIT_RESERVED_DOS0 },
    { "",     MDT_UNIT_RESERVED_DOS1 },
    { "",     MDT_UNIT_RESERVED_DOS2 },
    { "",     MDT_UNIT_RESERVED_DOS3 },
    { "",     MDT_UNIT_RESERVED_DOS4 },
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "mdtfile",
    N_("Load NT-MDT data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo mdt_func_info = {
        "mdtfile",
        N_("NT-MDT files (.mdt)"),
        (GwyFileDetectFunc)&mdt_detect,
        (GwyFileLoadFunc)&mdt_load,
        NULL,
    };

    gwy_file_func_register(name, &mdt_func_info);

    return TRUE;
}

static gint
mdt_detect(const gchar *filename,
           gboolean only_name)
{
    FILE *fh;
    gchar magic[MAGIC_SIZE];
    gint score;

    gwy_debug("");
    if (only_name) {
        gchar *filename_lc;

        filename_lc = g_ascii_strdown(filename, -1);
        score = g_str_has_suffix(filename_lc, EXTENSION) ? 20 : 0;
        g_free(filename_lc);

        return score;
    }

    if (!(fh = fopen(filename, "rb")))
        return 0;
    score = 0;
    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && memcmp(magic, MAGIC, MAGIC_SIZE) == 0)
        score = 100;
    fclose(fh);

    return score;
}

static GwyContainer*
mdt_load(const gchar *filename)
{
    guchar *buffer;
    gsize size;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GwyContainer *data = NULL;
    MDTFile mdtfile;
    GwyEnum *choices = NULL;
    gsize n, i, j;

    gwy_debug("");
    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_clear_error(&err);
        return NULL;
    }
    memset(&mdtfile, 0, sizeof(mdtfile));
    if (mdt_real_load(buffer, size, &mdtfile)) {
        for (i = 0; i <= mdtfile.last_frame; i++) {
            if (mdtfile.frames[i].type == MDT_FRAME_SCANNED) {
                MDTScannedDataFrame *sdframe;

                sdframe = (MDTScannedDataFrame*)mdtfile.frames[i].frame_data;
                data = GWY_CONTAINER(gwy_container_new());
                dfield = extract_scanned_data(sdframe);
                gwy_container_set_object_by_name(data, "/0/data",
                                                 G_OBJECT(dfield));
                return data;
            }
        }
        /*
        n = 0;
        for (i = 0; i < mdtfile.nchannels; i++) {
            for (j = 0; j < mdtfile.channels[i].nimages; j++) {
                image = mdtfile.channels[i].images + j;
                if (image->image_data) {
                    n++;
                    gwy_debug("Available data: Channel #%u image #%u",
                              i+1, j+1);
                    choices = g_renew(GwyEnum, choices, n);
                    choices[n-1].value = 1024*i + j;
                    choices[n-1].name = g_strdup_printf("Channel %u, image %u "
                                                        "(%u×%u)",
                                                        i+1, j+1,
                                                        image->width,
                                                        image->height);
                }
            }
        }
        i = select_which_data(&mdtfile, choices, n);
        gwy_debug("Selected %u:%u", i/1024, i % 1024);
        if (i != (gsize)-1) {
            dfield = extract_data(&mdtfile, i/1024, i % 1024);
            if (dfield) {
                data = GWY_CONTAINER(gwy_container_new());
                gwy_container_set_object_by_name(data, "/0/data",
                                                 G_OBJECT(dfield));
                g_object_unref(dfield);
                add_metadata(&mdtfile, i/1024, i %1024, data);
            }
        }
        */
    }

    gwy_file_abandon_contents(buffer, size, NULL);

    return data;
}

#if 0
static void
selection_changed(GtkWidget *button,
                  MDTDialogControls *controls)
{
    gsize i;
    GwyDataField *dfield;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    i = gwy_radio_buttons_get_current_from_widget(button, "data");
    g_assert(i != (gsize)-1);
    dfield = extract_data(controls->mdtfile, i/1024, i % 1024);
    gwy_container_set_object_by_name(controls->data, "/0/data",
                                     G_OBJECT(dfield));
    g_object_unref(dfield);
    gwy_data_view_update(GWY_DATA_VIEW(controls->data_view));
}

static gsize
select_which_data(MDTFile *mdtfile,
                  GwyEnum *choices,
                  gsize n)
{
    GtkWidget *dialog, *label, *vbox, *hbox, *align;
    MDTDialogControls controls;
    GwyDataField *dfield;
    gint xres, yres;
    gdouble zoomval;
    GtkObject *layer;
    GSList *radio, *rl;
    gint response;
    gsize i;

    if (!n)
        return (gsize)-1;

    if (n == 1)
        return choices[0].value;

    controls.mdtfile = mdtfile;

    dialog = gtk_dialog_new_with_buttons(_("Select Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
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

    radio = gwy_radio_buttons_create(choices, n, "data",
                                     G_CALLBACK(selection_changed), &controls,
                                     0);
    for (i = 0, rl = radio; rl; i++, rl = g_slist_next(rl))
        gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(rl->data), TRUE, TRUE, 0);

    /* preview */
    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    i = choices[0].value;
    dfield = extract_data(mdtfile, i/1024, i % 1024);
    controls.data = GWY_CONTAINER(gwy_container_new());
    gwy_container_set_object_by_name(controls.data, "/0/data",
                                     G_OBJECT(dfield));
    g_object_unref(dfield);
    add_metadata(mdtfile, i/1024, i %1024, controls.data);
    xres = gwy_data_field_get_xres(GWY_DATA_FIELD(dfield));
    yres = gwy_data_field_get_yres(GWY_DATA_FIELD(dfield));
    zoomval = 120.0/MAX(xres, yres);

    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.data_view), zoomval);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view),
                                 GWY_PIXMAP_LAYER(layer));
    gtk_container_add(GTK_CONTAINER(align), controls.data_view);

    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return (gsize)-1;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    i = GPOINTER_TO_UINT(gwy_radio_buttons_get_current(radio, "data"));
    gtk_widget_destroy(dialog);

    return i;
}

static GwyDataField*
extract_data(MDTFile *mdtfile,
             gsize ch,
             gsize im)
{
    GwyDataField *dfield;
    MDTChannel *channel;
    MDTImage *image;
    gdouble xreal, yreal, zreal;
    gdouble *d;
    gsize i;

    channel = mdtfile->channels + ch;
    image = channel->images + im;
    if (image->bpp != 1 && image->bpp != 2 && image->bpp != 4) {
        g_warning("Cannot extract image of bpp = %u", image->bpp);
        return NULL;
    }

    xreal = yreal = 100e-9;    /* XXX: whatever */
    zreal = 1e-9;
    if ((d = g_hash_table_lookup(mdtfile->params, GUINT_TO_POINTER(2))))
        xreal = *d * 1e-9;
    if ((d = g_hash_table_lookup(mdtfile->params, GUINT_TO_POINTER(3))))
        yreal = *d * 1e-9;
    if ((d = g_hash_table_lookup(mdtfile->params, GUINT_TO_POINTER(4))))
        zreal = *d * 1e-9;
    dfield = GWY_DATA_FIELD(gwy_data_field_new(image->width, image->height,
                                               xreal, yreal, FALSE));

    d = gwy_data_field_get_data(dfield);
    switch (image->bpp) {
        case 1:
        for (i = 0; i < image->width*image->height; i++)
            d[i] = image->image_data[i]/255.0;
        break;

        case 2:
        for (i = 0; i < image->width*image->height; i++)
            d[i] = (image->image_data[2*i]
                    + 256.0*image->image_data[2*i + 1])/65535.0;
        break;

        case 4:
        for (i = 0; i < image->width*image->height; i++)
            d[i] = (image->image_data[4*i]
                    + 256.0*(image->image_data[4*i + 1]
                             + 256.0*(image->image_data[4*i + 2]
                                      + 256.0*image->image_data[4*i + 3])))
                    /4294967296.0;
        break;

        default:
        g_assert_not_reached();
        break;
    }
    for (i = 0; i < image->width*image->height; i++)
        d[i] *= zreal;

    return dfield;
}

static void
add_metadata(MDTFile *mdtfile,
             gsize ch,
             gsize im,
             GwyContainer *data)
{
    static const guint good_metadata[] = {
        0, 1, 9, 10, 12, 13, 14, 15, 16, 18, 20, 21, 22, 23, 24, 25, 26, 27,
    };
    MDTChannel *channel;
    MDTImage *image;
    gsize i, j;
    guchar *key, *value;
    gpointer *p;

    channel = mdtfile->channels + ch;
    image = channel->images + im;
    for (i = 0; i < G_N_ELEMENTS(good_metadata); i++) {
        for (j = 0; j < G_N_ELEMENTS(mdt_parameters); j++) {
            if (mdt_parameters[j].idx == good_metadata[i])
                break;
        }
        g_assert(j < G_N_ELEMENTS(mdt_parameters));
        p = g_hash_table_lookup(mdtfile->params, GUINT_TO_POINTER(j));
        if (!p)
            continue;

        key = g_strdup_printf("/meta/%s", mdt_parameters[j].meta);
        switch (mdt_parameters[j].type) {
            case G_TYPE_STRING:
            value = g_strdup((gchar*)p);
            break;

            case G_TYPE_INT:
            if (mdt_parameters[j].units)
                value = g_strdup_printf("%d %s",
                                        *(gsize*)p, mdt_parameters[j].units);
            else
                value = g_strdup_printf("%d", *(gsize*)p);
            break;

            case G_TYPE_DOUBLE:
            if (mdt_parameters[j].units)
                value = g_strdup_printf("%.5g %s",
                                        *(gdouble*)p, mdt_parameters[j].units);
            else
                value = g_strdup_printf("%.5g", *(gdouble*)p);
            break;

            default:
            g_assert_not_reached();
            value = NULL;
            break;
        }
        gwy_container_set_string_by_name(data, key, value);
        g_free(key);
    }

    /* Special metadata */
    if ((p = g_hash_table_lookup(mdtfile->params, GUINT_TO_POINTER(28)))) {
        value = g_strdup(gwy_enum_to_string(*(gsize*)p,
                                            mdt_palettes,
                                            G_N_ELEMENTS(mdt_palettes)));
        gwy_container_set_string_by_name(data, "/0/base/palette", value);
    }

    if ((p = g_hash_table_lookup(mdtfile->params, GUINT_TO_POINTER(6)))) {
        value = g_strdup(gwy_enum_to_string(*(gsize*)p,
                                            mdt_aquisitions,
                                            G_N_ELEMENTS(mdt_aquisitions)));
        gwy_container_set_string_by_name(data, "/meta/Aqusition type", value);
    }

    value = g_strdup(gwy_enum_to_string(channel->data_type,
                                        mdt_data_types,
                                        G_N_ELEMENTS(mdt_data_types)));
    gwy_container_set_string_by_name(data, "/meta/Data type", value);

    value = g_strdup(gwy_enum_to_string(channel->signal_source,
                                        mdt_signal_sources,
                                        G_N_ELEMENTS(mdt_signal_sources)));
    gwy_container_set_string_by_name(data, "/meta/Signal source", value);
}
#endif

static inline gsize
get_WORD(const guchar **p)
{
    gsize z = (gsize)(*p)[0] + ((gsize)(*p)[1] << 8);
    *p += 2;
    return z;
}

static inline gsize
get_DWORD(const guchar **p)
{
    gsize z = (gsize)(*p)[0] + ((gsize)(*p)[1] << 8)
              + ((gsize)(*p)[2] << 16) + ((gsize)(*p)[3] << 24);
    *p += 4;
    return z;
}

static inline gsize
get_FLOAT(const guchar **p)
{
    union { guchar pp[4]; float f; } z;

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    memcpy(z.pp, *p, sizeof(float));
#else
    z.pp[0] = *p[3];
    z.pp[1] = *p[2];
    z.pp[2] = *p[1];
    z.pp[3] = *p[0];
#endif
    *p += sizeof(float);
    return z.f;
}

static inline gsize
get_DOUBLE(const guchar **p)
{
    union { guchar pp[8]; double d; } z;

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    memcpy(z.pp, *p, sizeof(double));
#else
    z.pp[0] = *p[7];
    z.pp[1] = *p[6];
    z.pp[2] = *p[5];
    z.pp[3] = *p[4];
    z.pp[4] = *p[3];
    z.pp[5] = *p[2];
    z.pp[6] = *p[1];
    z.pp[7] = *p[0];
#endif
    *p += sizeof(double);
    return z.d;
}

static gboolean
mdt_scanned_data_vars(const guchar *p,
                      const guchar *fstart,
                      MDTScannedDataFrame *frame,
                      gsize frame_size,
                      gsize vars_size)
{
    frame->x_scale.offset = get_FLOAT(&p);
    frame->x_scale.step = get_FLOAT(&p);
    frame->x_scale.unit = get_WORD(&p);
    gwy_debug("x: *%g +%g [%d:%s]",
              frame->x_scale.step, frame->x_scale.offset, frame->x_scale.unit,
              gwy_enum_to_string(frame->x_scale.unit,
                                 mdt_units, G_N_ELEMENTS(mdt_units)));
    frame->y_scale.offset = get_FLOAT(&p);
    frame->y_scale.step = get_FLOAT(&p);
    frame->y_scale.unit = get_WORD(&p);
    gwy_debug("y: *%g +%g [%d:%s]",
              frame->y_scale.step, frame->y_scale.offset, frame->y_scale.unit,
              gwy_enum_to_string(frame->y_scale.unit,
                                 mdt_units, G_N_ELEMENTS(mdt_units)));
    frame->z_scale.offset = get_FLOAT(&p);
    frame->z_scale.step = get_FLOAT(&p);
    frame->z_scale.unit = get_WORD(&p);
    gwy_debug("z: *%g +%g [%d:%s]",
              frame->z_scale.step, frame->z_scale.offset, frame->z_scale.unit,
              gwy_enum_to_string(frame->z_scale.unit,
                                 mdt_units, G_N_ELEMENTS(mdt_units)));

    frame->channel_index = (gint)(*p++);
    frame->mode = (gint)(*p++);
    frame->xres = get_WORD(&p);
    frame->yres = get_WORD(&p);
    gwy_debug("channel_index = %d, mode = %d, xres = %d, yres = %d",
              frame->channel_index, frame->mode, frame->xres, frame->yres);
    frame->ndacq = get_WORD(&p);
    frame->step_length = Angstrom*get_FLOAT(&p);
    frame->adt = get_WORD(&p);
    frame->adc_gain_amp_log10 = (guint)(*p++);
    frame->adc_index = (guint)(*p++);
    frame->s16.version = (guint)(*p++);
    frame->s17.pass_num = (guint)(*p++);
    frame->scan_dir = (guint)(*p++);
    frame->power_of_2 = (gboolean)(*p++);
    frame->velocity = Angstrom*get_FLOAT(&p);
    frame->setpoint = get_FLOAT(&p);
    frame->bias_voltage = get_FLOAT(&p);
    frame->draw = (gboolean)(*p++);
    p++;
    frame->xoff = get_DWORD(&p);  /* FIXME: sign? */
    frame->yoff = get_DWORD(&p);  /* FIXME: sign? */
    frame->nl_corr = (gboolean)(*p++);

    p = fstart + FRAME_HEADER_SIZE + vars_size;
    if ((gsize)(p - fstart) + FRAME_MODE_SIZE > frame_size) {
        gwy_debug("FAILED: Frame too short for Frame Mode");
        return FALSE;
    }
    frame->fm_mode = get_WORD(&p);
    frame->fm_xres = get_WORD(&p);
    frame->fm_yres = get_WORD(&p);
    frame->fm_ndots = get_WORD(&p);
    gwy_debug("mode = %u, xres = %u, yres = %u, ndots = %u",
              frame->fm_mode, frame->fm_xres, frame->fm_yres, frame->fm_ndots);

    if ((gsize)(p - fstart)
        + sizeof(gint16)*(2*frame->fm_ndots + frame->fm_xres * frame->fm_yres)
        > frame_size) {
        gwy_debug("FAILED: Frame too short for dots or data");
        return FALSE;
    }

    if (frame->fm_ndots) {
        frame->dots = p;
        p += sizeof(gint16)*2*frame->fm_ndots;
    }
    if (frame->fm_xres * frame->fm_yres) {
        frame->image = p;
    }

    return TRUE;
}

static gboolean
mdt_real_load(const guchar *buffer,
              gsize size,
              MDTFile *mdtfile)
{
    gsize start, id, i, j, len;
    const guchar *p, *fstart;
    MDTScannedDataFrame *scannedframe;
    gpointer idp;
    gdouble d;

    /* File Header */
    if (size < 32) {
        gwy_debug("FAILED: File shorter than file header");
        return FALSE;
    }
    p = buffer + 4;  /* magic header */
    mdtfile->size = get_DWORD(&p);
    gwy_debug("File size (w/o header): %u", mdtfile->size);
    p += 4;  /* reserved */
    mdtfile->last_frame = get_WORD(&p);
    gwy_debug("Last frame: %u", mdtfile->last_frame);
    p += 18;  /* reserved */
    /* XXX: documentation specifies 32 bytes long header, but zeroth frame
     * starts at 33th byte in reality */
    p++;

    if (mdtfile->size + 33 != size) {
        gwy_debug("FAILED: File size (%u) different from what it claims (%u)",
                  size, mdtfile->size + 32);
        return FALSE;
    }

    /* Frames */
    mdtfile->frames = g_new0(MDTFrame, mdtfile->last_frame + 1);
    for (i = 0; i <= mdtfile->last_frame; i++) {
        MDTFrame *frame = mdtfile->frames + i;

        fstart = p;
        if ((gsize)(p - buffer) + FRAME_HEADER_SIZE > size) {
            gwy_debug("FAILED: File truncated in frame header #%u", i);
            return FALSE;
        }
        frame->size = get_DWORD(&p);
        gwy_debug("Frame #%u size: %u", i, frame->size);
        if ((gsize)(p - buffer) + frame->size - 4 > size) {
            gwy_debug("FAILED: File truncated in frame #%u (len %u, have %u)",
                      i, frame->size, size - (p - buffer) - 4);
            return FALSE;
        }
        frame->type = get_WORD(&p);
        gwy_debug("Frame #%u type: %s", i,
                  gwy_enum_to_string(frame->type,
                                     frame_types, G_N_ELEMENTS(frame_types)));
        frame->version = ((gsize)p[0] << 8) + (gsize)p[1];
        p += 2;
        gwy_debug("Frame #%u version: %d.%d",
                  i, frame->version/0x100, frame->version % 0x100);
        frame->year = get_WORD(&p);
        frame->month = get_WORD(&p);
        frame->day = get_WORD(&p);
        frame->hour = get_WORD(&p);
        frame->min = get_WORD(&p);
        frame->sec = get_WORD(&p);
        gwy_debug("Frame #%u datetime: %d-%02d-%02d %02d:%02d:%02d",
                  i, frame->year, frame->month, frame->day,
                  frame->hour, frame->min, frame->sec);
        frame->var_size = get_WORD(&p);
        gwy_debug("Frame #%u var size: %u", i, frame->var_size);
        if (frame->var_size + FRAME_HEADER_SIZE > frame->size) {
            gwy_debug("FAILED: header + var size %u > %u frame size",
                      frame->var_size + FRAME_HEADER_SIZE, frame->size);
            return FALSE;
        }

        switch (frame->type) {
            case MDT_FRAME_SCANNED:
            if (frame->var_size < AXIS_SCALES_SIZE + SCAN_VARS_MIN_SIZE) {
                gwy_debug("FAILED: Frame #%u too short for scanned data header",
                          i);
                return FALSE;
            }
            scannedframe = g_new0(MDTScannedDataFrame, 1);
            /* XXX: check return value */
            mdt_scanned_data_vars(p, fstart, scannedframe,
                                  frame->size, frame->var_size);
            frame->frame_data = scannedframe;
            break;

            case MDT_FRAME_SPECTROSCOPY:
            gwy_debug("Spectroscropy frames make little sense to read now");
            break;

            case MDT_FRAME_TEXT:
            gwy_debug("Cannot read text frame");
            /*
            p = fstart + FRAME_HEADER_SIZE + frame->var_size;
            p += 16;
            for (j = 0; j < frame->size - (p - fstart); j++)
                g_print("%c", g_ascii_isprint(p[j]) ? p[j] : '.');
            g_printerr("%s\n", g_convert(p, frame->size - (p - fstart),
                                         "UCS-2", "UTF-8", NULL, &j, NULL));
                                         */
            break;

            case MDT_FRAME_OLD_MDA:
            gwy_debug("Cannot read old MDA frame");
            break;

            case MDT_FRAME_MDA:
            gwy_debug("Cannot read MDA frame");
            break;

            case MDT_FRAME_PALETTE:
            gwy_debug("Cannot read palette frame");
            break;

            default:
            g_warning("Unknown frame type %d", frame->type);
            break;
        }

        p = fstart + frame->size;
    }

    return TRUE;
}

GwyDataField*
extract_scanned_data(MDTScannedDataFrame *dataframe)
{
    GwyDataField *dfield;
    gsize i;
    gdouble *data;
    const guchar *p;

    dfield = GWY_DATA_FIELD(gwy_data_field_new(dataframe->fm_xres,
                                               dataframe->fm_yres,
                                               1.0, 1.0, FALSE));
    data = gwy_data_field_get_data(dfield);
    p = dataframe->image;
    for (i = 0; i < dataframe->fm_yres*dataframe->fm_yres; i++)
        data[i] = (p[2*i] + 256.0*p[2*i + 1])/65535.0;

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

