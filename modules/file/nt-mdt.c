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
/* TODO: some metadata, MDA, ... */

#include <libgwyddion/gwymacros.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>

#include "get.h"

#define MAGIC "\x01\xb0\x93\xff"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".mdt"

#define Angstrom (1e-10)
#define Nano (1e-9)

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
    guint size;     /* h_sz */
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
    guint size;  /* f_sz */
    guint last_frame; /* f_nt */
    MDTFrame *frames;
} MDTFile;

typedef struct {
    GtkWidget *data_view;
    GwyContainer *data;
    MDTFile *mdtfile;
} MDTDialogControls;

static gboolean       module_register     (const gchar *name);
static gint           mdt_detect          (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer*  mdt_load            (const gchar *filename);
static guint          select_which_data   (MDTFile *mdtfile,
                                           GwyEnum *choices,
                                           guint n);
static void           add_metadata        (MDTFile *mdtfile,
                                           guint i,
                                           GwyContainer *data);
static gboolean       mdt_real_load       (const guchar *buffer,
                                           guint size,
                                           MDTFile *mdtfile);
static GwyDataField*  extract_scanned_data(MDTScannedDataFrame *dataframe);

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
    N_("Imports NT-MDT data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.3.2",
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
mdt_detect(const GwyFileDetectInfo *fileinfo,
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
mdt_load(const gchar *filename)
{
    guchar *buffer;
    gsize size;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GwyContainer *data = NULL;
    MDTFile mdtfile;
    GwyEnum *choices = NULL;
    guint n, i;

    gwy_debug("");
    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_clear_error(&err);
        return NULL;
    }
    n = 0;
    memset(&mdtfile, 0, sizeof(mdtfile));
    if (mdt_real_load(buffer, size, &mdtfile)) {
        for (i = 0; i <= mdtfile.last_frame; i++) {
            if (mdtfile.frames[i].type == MDT_FRAME_SCANNED) {
                MDTScannedDataFrame *sdframe;

                sdframe = (MDTScannedDataFrame*)mdtfile.frames[i].frame_data;
                n++;
                choices = g_renew(GwyEnum, choices, n);
                choices[n-1].value = i;
                choices[n-1].name = g_strdup_printf("%s %u "
                                                    "(%u×%u)",
                                                    _("Frame"),
                                                    i+1,
                                                    sdframe->fm_xres,
                                                    sdframe->fm_yres);
            }
        }
        i = select_which_data(&mdtfile, choices, n);
        gwy_debug("Selected %u", i);
        if (i != (guint)-1) {
            MDTScannedDataFrame *sdframe;

            sdframe = (MDTScannedDataFrame*)mdtfile.frames[i].frame_data;
            dfield = extract_scanned_data(sdframe);
            if (dfield) {
                data = gwy_container_new();
                gwy_container_set_object_by_name(data, "/0/data",
                                                 G_OBJECT(dfield));
                g_object_unref(dfield);
                add_metadata(&mdtfile, i, data);
            }
        }
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    for (i = 0; i < n; i++)
        g_free((gpointer)choices[i].name);
    g_free(choices);

    return data;
}

static void
selection_changed(GtkWidget *button,
                  MDTDialogControls *controls)
{
    guint i;
    MDTScannedDataFrame *sdframe;
    GwyDataField *dfield;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    i = gwy_radio_buttons_get_current_from_widget(button, "data");
    g_assert(i != (guint)-1);
    sdframe = (MDTScannedDataFrame*)controls->mdtfile->frames[i].frame_data;
    dfield = extract_scanned_data(sdframe);
    gwy_container_set_object_by_name(controls->data, "data", dfield);
    g_object_unref(dfield);
}

static guint
select_which_data(MDTFile *mdtfile,
                  GwyEnum *choices,
                  guint n)
{
    GtkWidget *dialog, *label, *vbox, *hbox, *align, *scroll, *svbox;
    MDTScannedDataFrame *sdframe;
    MDTDialogControls controls;
    GwyDataField *dfield;
    gint xres, yres;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    GSList *radio, *rl;
    gint response;
    guint i;

    if (!n)
        return (guint)-1;

    if (n == 1)
        return choices[0].value;

    controls.mdtfile = mdtfile;

    dialog = gtk_dialog_new_with_buttons(_("Select Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 1.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(align), vbox);

    label = gtk_label_new(_("Data to load:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    if (n > 8) {
        scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

        svbox = gtk_vbox_new(TRUE, 0);
        gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll),
                                              svbox);
    }
    else
        svbox = vbox;

    radio = gwy_radio_buttons_create(choices, n, "data",
                                     G_CALLBACK(selection_changed), &controls,
                                     0);
    for (i = 0, rl = radio; rl; i++, rl = g_slist_next(rl))
        gtk_box_pack_start(GTK_BOX(svbox), GTK_WIDGET(rl->data),
                           FALSE, FALSE, 0);

    /* preview */
    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    i = choices[0].value;
    sdframe = (MDTScannedDataFrame*)mdtfile->frames[i].frame_data;
    dfield = extract_scanned_data(sdframe);
    controls.data = gwy_container_new();
    gwy_container_set_object_by_name(controls.data, "data", dfield);
    gwy_container_set_enum_by_name(controls.data, "range-type",
                                   GWY_LAYER_BASIC_RANGE_AUTO);
    g_object_unref(dfield);
    add_metadata(mdtfile, i, controls.data);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    zoomval = 120.0/MAX(xres, yres);

    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.data_view), zoomval);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "data");
    gwy_layer_basic_set_range_type_key(GWY_LAYER_BASIC(layer), "range-type");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view), layer);
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
            return (guint)-1;
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

#define HASH_SET_META(fmt, val, key) \
    g_string_printf(s, fmt, val); \
    gwy_container_set_string_by_name(data, "/meta/" key, g_strdup(s->str))

static void
add_metadata(MDTFile *mdtfile,
             guint i,
             GwyContainer *data)
{
    MDTFrame *frame;
    MDTScannedDataFrame *sdframe;
    GString *s;

    g_return_if_fail(i <= mdtfile->last_frame);
    frame = mdtfile->frames + i;
    g_return_if_fail(frame->type == MDT_FRAME_SCANNED);
    sdframe = (MDTScannedDataFrame*)frame->frame_data;

    s = g_string_new("");
    g_string_printf(s, "%d-%02d-%02d %02d:%02d:%02d",
                    frame->year, frame->month, frame->day,
                    frame->hour, frame->min, frame->sec);
    gwy_container_set_string_by_name(data, "/meta/Date & time",
                                     g_strdup(s->str));

    g_string_printf(s, "%d.%d",
                    frame->version/0x100, frame->version % 0x100);
    gwy_container_set_string_by_name(data, "/meta/Version",
                                     g_strdup(s->str));

    g_string_printf(s, "%c%c%c%s",
                    (sdframe->scan_dir & 0x02) ? '-' : '+',
                    (sdframe->scan_dir & 0x01) ? 'X' : 'Y',
                    (sdframe->scan_dir & 0x04) ? '-' : '+',
                    (sdframe->scan_dir & 0x80) ? " (double pass)" : "");
    gwy_container_set_string_by_name(data, "/meta/Scan direction",
                                     g_strdup(s->str));

    HASH_SET_META("%d", sdframe->channel_index, "Channel index");
    HASH_SET_META("%d", sdframe->mode, "Mode");
    HASH_SET_META("%d", sdframe->ndacq, "Step (DAC)");
    HASH_SET_META("%.2f nm", sdframe->step_length/Nano, "Step length");
    HASH_SET_META("%.0f nm/s", sdframe->velocity/Nano, "Scan velocity");
    HASH_SET_META("%.2f nA", sdframe->setpoint/Nano, "Setpoint value");
    HASH_SET_META("%.2f V", sdframe->bias_voltage, "Bias voltage");

    g_string_free(s, TRUE);
}

static void
mdt_read_axis_scales(const guchar *p,
                     MDTAxisScale *x_scale,
                     MDTAxisScale *y_scale,
                     MDTAxisScale *z_scale)
{
    x_scale->offset = get_FLOAT(&p);
    x_scale->step = get_FLOAT(&p);
    x_scale->unit = (gint16)get_WORD(&p);
    gwy_debug("x: *%g +%g [%d:%s]",
              x_scale->step, x_scale->offset, x_scale->unit,
              gwy_enum_to_string(x_scale->unit,
                                 mdt_units, G_N_ELEMENTS(mdt_units)));
    x_scale->step = fabs(x_scale->step);
    if (!x_scale->step) {
        g_warning("x_scale.step == 0, changing to 1");
        x_scale->step = 1.0;
    }

    y_scale->offset = get_FLOAT(&p);
    y_scale->step = get_FLOAT(&p);
    y_scale->unit = (gint16)get_WORD(&p);
    gwy_debug("y: *%g +%g [%d:%s]",
              y_scale->step, y_scale->offset, y_scale->unit,
              gwy_enum_to_string(y_scale->unit,
                                 mdt_units, G_N_ELEMENTS(mdt_units)));
    y_scale->step = fabs(y_scale->step);
    if (!y_scale->step) {
        g_warning("y_scale.step == 0, changing to 1");
        y_scale->step = 1.0;
    }

    z_scale->offset = get_FLOAT(&p);
    z_scale->step = get_FLOAT(&p);
    z_scale->unit = (gint16)get_WORD(&p);
    gwy_debug("z: *%g +%g [%d:%s]",
              z_scale->step, z_scale->offset, z_scale->unit,
              gwy_enum_to_string(z_scale->unit,
                                 mdt_units, G_N_ELEMENTS(mdt_units)));
    if (!z_scale->step) {
        g_warning("z_scale.step == 0, changing to 1");
        z_scale->step = 1.0;
    }
}

static gboolean
mdt_scanned_data_vars(const guchar *p,
                      const guchar *fstart,
                      MDTScannedDataFrame *frame,
                      guint frame_size,
                      guint vars_size)
{
    mdt_read_axis_scales(p, &frame->x_scale, &frame->y_scale, &frame->z_scale);
    p += AXIS_SCALES_SIZE;

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
    frame->setpoint = Nano*get_FLOAT(&p);
    frame->bias_voltage = get_FLOAT(&p);
    frame->draw = (gboolean)(*p++);
    p++;
    frame->xoff = get_DWORD(&p);  /* FIXME: sign? */
    frame->yoff = get_DWORD(&p);  /* FIXME: sign? */
    frame->nl_corr = (gboolean)(*p++);

    p = fstart + FRAME_HEADER_SIZE + vars_size;
    if ((guint)(p - fstart) + FRAME_MODE_SIZE > frame_size) {
        gwy_debug("FAILED: Frame too short for Frame Mode");
        return FALSE;
    }
    frame->fm_mode = get_WORD(&p);
    frame->fm_xres = get_WORD(&p);
    frame->fm_yres = get_WORD(&p);
    frame->fm_ndots = get_WORD(&p);
    gwy_debug("mode = %u, xres = %u, yres = %u, ndots = %u",
              frame->fm_mode, frame->fm_xres, frame->fm_yres, frame->fm_ndots);

    if ((guint)(p - fstart)
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
              guint size,
              MDTFile *mdtfile)
{
    guint i;
    const guchar *p, *fstart;
    MDTScannedDataFrame *scannedframe;

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
        if ((guint)(p - buffer) + FRAME_HEADER_SIZE > size) {
            gwy_debug("FAILED: File truncated in frame header #%u", i);
            return FALSE;
        }
        frame->size = get_DWORD(&p);
        gwy_debug("Frame #%u size: %u", i, frame->size);
        if ((guint)(p - buffer) + frame->size - 4 > size) {
            gwy_debug("FAILED: File truncated in frame #%u (len %u, have %u)",
                      i, frame->size, size - (p - buffer) - 4);
            return FALSE;
        }
        frame->type = get_WORD(&p);
        gwy_debug("Frame #%u type: %s", i,
                  gwy_enum_to_string(frame->type,
                                     frame_types, G_N_ELEMENTS(frame_types)));
        frame->version = ((guint)p[0] << 8) + (gsize)p[1];
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

static GwyDataField*
extract_scanned_data(MDTScannedDataFrame *dataframe)
{
    GwyDataField *dfield;
    GwySIUnit *siunitxy, *siunitz;
    guint i;
    gdouble *data;
    gdouble xreal, yreal, zscale;
    gint power10xy, power10z;
    const gint16 *p;
    const gchar *unit;

    if (dataframe->x_scale.unit != dataframe->y_scale.unit)
        g_warning("Different x and y units, using x for both (incorrect).");
    unit = gwy_enum_to_string(dataframe->x_scale.unit,
                              mdt_units, G_N_ELEMENTS(mdt_units));
    siunitxy = gwy_si_unit_new_parse(unit, &power10xy);
    xreal = dataframe->fm_xres*pow10(power10xy)*dataframe->x_scale.step;
    yreal = dataframe->fm_yres*pow10(power10xy)*dataframe->y_scale.step;

    unit = gwy_enum_to_string(dataframe->z_scale.unit,
                              mdt_units, G_N_ELEMENTS(mdt_units));
    siunitz = gwy_si_unit_new_parse(unit, &power10z);
    zscale = pow10(power10z)*dataframe->z_scale.step;

    dfield = gwy_data_field_new(dataframe->fm_xres, dataframe->fm_yres,
                                xreal, yreal,
                                FALSE);
    gwy_data_field_set_si_unit_xy(dfield, siunitxy);
    g_object_unref(siunitxy);
    gwy_data_field_set_si_unit_z(dfield, siunitz);
    g_object_unref(siunitz);

    data = gwy_data_field_get_data(dfield);
    p = (gint16*)dataframe->image;
    for (i = 0; i < dataframe->fm_xres*dataframe->fm_yres; i++)
        data[i] = zscale*GINT16_FROM_LE(p[i]);

    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

