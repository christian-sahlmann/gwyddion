/*
 *  @(#) $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 * [FILE-MAGIC-USERGUIDE]
 * OpenEXR images
 * .exr
 * Read Export
 **/

#include "config.h"
#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#ifdef HAVE_PNG
#include <png.h>
#endif

#ifdef HAVE_EXR
#include <exception>
#include <ImfChannelList.h>
#include <ImfDoubleAttribute.h>
#include <ImfFrameBuffer.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>
#include <ImfStringAttribute.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "gwytiff.h"

#define EXR_EXTENSION ".exr"
#define EXR_MAGIC "\x76\x2f\x31\x01"
#define EXR_MAGIC_SIZE sizeof(EXR_MAGIC)-1

typedef enum {
    /* Used with common image formats supporting 16bit greyscale */
    GWY_BIT_DEPTH_INT16 = 16,
    /* Used with HDR greyscale images */
    GWY_BIT_DEPTH_HALF = 17,
    GWY_BIT_DEPTH_INT32 = 32,
    GWY_BIT_DEPTH_FLOAT = 33,
} GwyBitDepth;

typedef struct {
    GwyBitDepth bit_depth;
    gdouble zscale;
} EXRSaveArgs;

typedef struct {
    EXRSaveArgs *args;
    GSList *bit_depth;
    GtkWidget *zscale;
    GtkWidget *min_label;
    GtkWidget *min;
    GtkWidget *max_label;
    GtkWidget *max;
    GtkWidget *centre_label;
    GtkWidget *centre;
    GtkWidget *use_centre;
    GtkWidget *message;
} EXRSaveControls;

static gboolean module_register  (void);
static gint     exr_detect       (const GwyFileDetectInfo *fileinfo,
                                  gboolean only_name,
                                  const gchar *name);
static gboolean exr_export       (GwyContainer *data,
                                  const gchar *filename,
                                  GwyRunType mode,
                                  GError **error);
static gboolean exr_save_dialog(EXRSaveArgs *args,
                GwyDataField *field,
                gdouble pmin, gdouble pmax, gdouble pcentre);
static void exr_save_bit_depth_changed(GtkWidget *button,
                           EXRSaveControls *controls);
static void     exr_write_image  (GwyDataField *field,
                                  gchar *imagedata,
                                  const gchar *filename,
                                  const gchar *title,
                                  GwyBitDepth bit_depth,
                                  gdouble zscale);
static void     find_range       (GwyDataField *field,
                                  gdouble *pmin,
                                  gdouble *pmax,
                                  gdouble *pcentre);
static gchar*   create_image_data(GwyDataField *field,
                                  GwyBitDepth bit_depth,
                                  gdouble zscale);

static const EXRSaveArgs exr_save_defaults = {
    GWY_BIT_DEPTH_HALF, 0.0
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports 16bit grayscale PPM, PNG and TIFF images, imports and exports "
       "OpenEXR images (if available)."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("openexr",
                           N_("OpenEXR images (.exr)"),
                           (GwyFileDetectFunc)&exr_detect,
                           //(GwyFileLoadFunc)&exr_load,
                           NULL,
                           NULL,
                           (GwyFileSaveFunc)&exr_export);

    return TRUE;
}

static gint
exr_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name,
           G_GNUC_UNUSED const gchar *name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXR_EXTENSION)
               ? 20 : 0;

    if (fileinfo->buffer_len > EXR_MAGIC_SIZE
        && memcmp(fileinfo->head, EXR_MAGIC, EXR_MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static gboolean
exr_export(G_GNUC_UNUSED GwyContainer *data,
           const gchar *filename,
           GwyRunType mode,
           GError **error)
{
    EXRSaveArgs args = exr_save_defaults;
    GwyDataField *field;
    guint id;
    gboolean ok = TRUE;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &field,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    if (!field) {
        err_NO_CHANNEL_EXPORT(error);
        return FALSE;
    }

    gdouble pmax, pmin, pcentre;
    /* Statistics of positive values */
    find_range(field, &pmin, &pmax, &pcentre);

    if (mode == GWY_RUN_INTERACTIVE
        && !exr_save_dialog(&args, field, pmin, pmax, pcentre)) {
        //exr_save_save_args(settings, &args);
        err_CANCELLED(error);
        return FALSE;
    }

    gchar *imagedata = create_image_data(field, args.bit_depth, args.zscale);

    try {
        exr_write_image(field, imagedata,
                        filename, "Height", args.bit_depth, args.zscale);
    }
    catch (const std::exception &exc) {
        g_warning("Exception from libImf: %s", exc.what());
        ok = FALSE;
    }

    g_free(imagedata);

    return ok;
}

static gboolean
exr_save_dialog(EXRSaveArgs *args,
                GwyDataField *field,
                gdouble pmin, gdouble pmax, gdouble pcentre)
{
    GtkWidget *dialog, *label, *align;
    GtkTable *table;
    EXRSaveControls controls;
    gint row, response, power10;
    GwySIValueFormat *vf;

    // XXX:
    args->zscale = pcentre;
    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Export EXR Image"), NULL,
                                         (GtkDialogFlags)0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = GTK_TABLE(gtk_table_new(10, 3, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                      GTK_WIDGET(table));
    row = 0;

    label = gtk_label_new(_("Data format:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);
    row++;

    controls.bit_depth
        = gwy_radio_buttons_createl(G_CALLBACK(exr_save_bit_depth_changed),
                                    &controls, (gint)args->bit_depth,
                                    _("Half (16bit float)"), GWY_BIT_DEPTH_HALF,
                                    _("Float (32bit)"), GWY_BIT_DEPTH_FLOAT,
                                    _("Integer (32bit)"), GWY_BIT_DEPTH_INT32,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.bit_depth, table, 3, row);

    gtk_table_set_row_spacing(table, row-1, 8);
    label = gtk_label_new_with_mnemonic(_("_Z scale:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.zscale = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(controls.zscale), 8);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.zscale);
    // TODO: Set default value
    gtk_table_attach(table, controls.zscale, 1, 2, row, row+1,
                     (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                     (GtkAttachOptions)0,
                     0, 0);
    gwy_widget_set_activate_on_unfocus(controls.zscale, TRUE);

    // TODO: Handle null range.
    power10 = 3*GWY_ROUND(log(pcentre)/M_LN10/3.0);
    vf = gwy_si_unit_get_format_for_power10(gwy_data_field_get_si_unit_z(field),
                                            GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            power10, NULL);
    label = gtk_label_new(vf->units);
    gwy_si_unit_value_format_free(vf);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 2, 3, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);
    row++;

    controls.min_label = label = gtk_label_new("Min");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.min = label = gtk_label_new("0.1");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 1, 2, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);
    row++;

    controls.max_label = label = gtk_label_new("Max");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.max = label = gtk_label_new("1.0");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 1, 2, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);
    row++;

    controls.centre_label = label = gtk_label_new("Z scale");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.centre = label = gtk_label_new("0.3");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 1, 2, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);
    row++;

    align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
    gtk_table_attach(table, align, 1, 2, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.use_centre = gtk_button_new_with_mnemonic(_("_Use"));
    gtk_container_add(GTK_CONTAINER(align), controls.use_centre);
    row++;

    gtk_table_set_row_spacing(table, row-1, 8);
    controls.message = label = gtk_label_new("Warning");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 3, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);
    row++;

    // TODO: Update min/max and message according to the data type.

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
exr_save_bit_depth_changed(G_GNUC_UNUSED GtkWidget *button,
                           EXRSaveControls *controls)
{
    controls->args->bit_depth
        = (GwyBitDepth)gwy_radio_buttons_get_current(controls->bit_depth);
    // TODO
}

// NB: This function raises a C++ exception instead of reporting the error via
// GError.  The caller must catch it.
static void
exr_write_image(GwyDataField *field,
                gchar *imagedata,
                const gchar *filename,
                const gchar *title,
                GwyBitDepth bit_depth,
                gdouble zscale)
{
    guint xres = gwy_data_field_get_xres(field);
    guint yres = gwy_data_field_get_yres(field);

    Imf::Header header(xres, yres);
    header.lineOrder() = Imf::INCREASING_Y;

    Imf::PixelType pixel_type;
    if (bit_depth == GWY_BIT_DEPTH_HALF)
        pixel_type = Imf::HALF;
    else if (bit_depth == GWY_BIT_DEPTH_FLOAT)
        pixel_type = Imf::FLOAT;
    else if (bit_depth == GWY_BIT_DEPTH_INT32)
        pixel_type = Imf::UINT;
    else {
        g_assert_not_reached();
    }

    gdouble v;
    v = gwy_data_field_get_xreal(field);
    header.insert("Gwy::XReal", Imf::DoubleAttribute(v));
    v = gwy_data_field_get_xreal(field);
    header.insert("Gwy::YReal", Imf::DoubleAttribute(v));
    header.insert("Gwy::ZScale", Imf::DoubleAttribute(zscale));
    if ((v = gwy_data_field_get_xoffset(field)))
        header.insert("Gwy::XOffset", Imf::DoubleAttribute(v));
    if ((v = gwy_data_field_get_yoffset(field)))
        header.insert("Gwy::YOffset", Imf::DoubleAttribute(v));

    header.insert("Gwy::Title", Imf::StringAttribute(title));

    gchar *s;
    s = gwy_si_unit_get_string(gwy_data_field_get_si_unit_xy(field),
                               GWY_SI_UNIT_FORMAT_PLAIN);
    header.insert("Gwy::XYUnits", Imf::StringAttribute(s));
    g_free(s);

    s = gwy_si_unit_get_string(gwy_data_field_get_si_unit_z(field),
                               GWY_SI_UNIT_FORMAT_PLAIN);
    header.insert("Gwy::ZUnits", Imf::StringAttribute(s));
    g_free(s);

    header.channels().insert("Y", Imf::Channel(pixel_type));

    Imf::OutputFile outfile(filename, header);
    Imf::FrameBuffer framebuffer;

    if (pixel_type == Imf::HALF)
        framebuffer.insert("Y", Imf::Slice(pixel_type, imagedata,
                                           sizeof(half), xres*sizeof(half)));
    else if (pixel_type == Imf::FLOAT)
        framebuffer.insert("Y", Imf::Slice(pixel_type, imagedata,
                                           sizeof(float), xres*sizeof(float)));
    else if (pixel_type == Imf::UINT)
        framebuffer.insert("Y", Imf::Slice(pixel_type, imagedata,
                                           sizeof(guint32),
                                           xres*sizeof(guint32)));
    else {
        g_assert_not_reached();
    }

    outfile.setFrameBuffer(framebuffer);
    outfile.writePixels(yres);
}

static gchar*
create_image_data(GwyDataField *field,
                  GwyBitDepth bit_depth,
                  gdouble zscale)
{
    guint xres = gwy_data_field_get_xres(field);
    guint yres = gwy_data_field_get_yres(field);
    const gdouble *d = gwy_data_field_get_data_const(field);
    gchar *retval = NULL;
    guint i;

    if (zscale == GWY_BIT_DEPTH_INT16) {
        guint16 *imagedata = g_new(guint16, xres*yres);
        retval = (gchar*)imagedata;

        for (i = xres*yres; i; i--, d++, imagedata++)
            *imagedata = (guint16)CLAMP(*d/zscale, 0.0, 65535.0);
    }
    else if (bit_depth == GWY_BIT_DEPTH_INT32) {
        guint32 *imagedata = g_new(guint32, xres*yres);
        retval = (gchar*)imagedata;

        for (i = xres*yres; i; i--, d++, imagedata++)
            *imagedata = (guint32)CLAMP(*d/zscale, 0.0, 4294967295.0);
    }
    else if (bit_depth == GWY_BIT_DEPTH_FLOAT) {
        gfloat *imagedata = g_new(gfloat, xres*yres);
        retval = (gchar*)imagedata;

        for (i = xres*yres; i; i--, d++, imagedata++)
            *imagedata = (gfloat)(*d/zscale);
    }
    else if (bit_depth == GWY_BIT_DEPTH_HALF) {
        half *imagedata = g_new(half, xres*yres);
        retval = (gchar*)imagedata;

        for (i = xres*yres; i; i--, d++, imagedata++)
            *imagedata = (half)(*d/zscale);
    }
    else {
        g_assert_not_reached();
    }

    return retval;
}

static void
find_range(GwyDataField *field,
           gdouble *pmin, gdouble *pmax, gdouble *pcentre)
{
    gdouble min = G_MAXDOUBLE, max = G_MINDOUBLE, logcentre = 0.0;
    guint i, nc = 0;
    guint xres = gwy_data_field_get_xres(field),
          yres = gwy_data_field_get_yres(field);
    const gdouble *d = gwy_data_field_get_data_const(field);
    gdouble v;

    for (i = xres*yres; i; i--, d++) {
        if (!(v = *d))
            continue;

        v = fabs(v);
        if (v < min)
            min = v;
        if (v > max)
            max = v;
        logcentre += log(v);
        nc++;
    }

    *pmax = max;
    *pmin = min;
    *pcentre = exp(logcentre/nc);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
