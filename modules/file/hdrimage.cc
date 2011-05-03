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
#define DEBUG 1
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
#  include <exception>
#  include <half.h>
#  include <ImfChannelList.h>
#  include <ImfDoubleAttribute.h>
#  include <ImfFrameBuffer.h>
#  include <ImfInputFile.h>
#  include <ImfOutputFile.h>
#  include <ImfStringAttribute.h>
#else
#  define HALF_MIN 5.96046448e-08
#  define HALF_NRM_MIN 6.10351562e-05
#  define HALF_MAX 65504.0
#  define half guint16
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
#include "image-keys.h"

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
    /* Interface */
    gdouble pmin;
    gdouble pmax;
    gdouble pcentre;
    gdouble min;
    gdouble max;
} EXRSaveArgs;

typedef struct {
    EXRSaveArgs *args;
    GwyDataField *field;
    GSList *bit_depth;
    GtkWidget *zscale;
    GtkWidget *zscale_label;
    GtkWidget *zscale_units;
    GtkWidget *header_data;
    GtkWidget *header_representable;
    GtkWidget *min;
    GtkWidget *min_label;
    GtkWidget *min_image;
    GtkWidget *max;
    GtkWidget *max_label;
    GtkWidget *max_image;
    GtkWidget *centre;
    GtkWidget *centre_label;
    GtkWidget *use_centre;
} EXRSaveControls;

static gboolean module_register            (void);
#ifdef HAVE_EXR
static gint          exr_detect                 (const GwyFileDetectInfo *fileinfo,
                                                 gboolean only_name,
                                                 const gchar *name);
static GwyContainer* exr_load                   (const gchar *filename,
                                                 GwyRunType mode,
                                                 GError **error,
                                                 const gchar *name);
static gboolean      exr_export                 (GwyContainer *data,
                                                 const gchar *filename,
                                                 GwyRunType mode,
                                                 GError **error);
static gboolean      exr_save_dialog            (EXRSaveArgs *args,
                                                 GwyDataField *field);
static void          exr_save_update_zscale     (EXRSaveControls *controls);
static void          exr_save_update_ranges     (EXRSaveControls *controls);
static void          exr_save_bit_depth_changed (GtkWidget *button,
                                                 EXRSaveControls *controls);
static void          exr_save_zscale_changed    (GtkWidget *entry,
                                                 EXRSaveControls *controls);
static void          exr_save_use_centre_clicked(GtkWidget *button,
                                                 EXRSaveControls *controls);
static void          exr_write_image            (GwyDataField *field,
                                                 gchar *imagedata,
                                                 const gchar *filename,
                                                 const gchar *title,
                                                 GwyBitDepth bit_depth,
                                                 gdouble zscale);
static void          exr_save_load_args         (GwyContainer *container,
                                                 EXRSaveArgs *args);
static void          exr_save_save_args         (GwyContainer *container,
                                                 EXRSaveArgs *args);
#endif
#ifdef HAVE_PNG
static gint          png_detect                 (const GwyFileDetectInfo *fileinfo,
                                                 gboolean only_name,
                                                 const gchar *name);
static GwyContainer* png_load                   (const gchar *filename,
                                                 GwyRunType mode,
                                                 GError **error,
                                                 const gchar *name);
#endif
static gdouble  suggest_zscale             (GwyBitDepth bit_depth,
                                            gdouble pmin,
                                            gdouble pmax,
                                            gdouble pcentre);
static void     representable_range        (GwyBitDepth bit_depth,
                                            gdouble zscale,
                                            gdouble *min,
                                            gdouble *max);
static void     find_range                 (GwyDataField *field,
                                            gdouble *fmin,
                                            gdouble *fmax,
                                            gdouble *pmin,
                                            gdouble *pmax,
                                            gdouble *pcentre);
static gchar*   create_image_data          (GwyDataField *field,
                                            GwyBitDepth bit_depth,
                                            gdouble zscale,
                                            gdouble zmin,
                                            gdouble zmax);

#ifdef HAVE_EXR
static const EXRSaveArgs exr_save_defaults = {
    GWY_BIT_DEPTH_HALF, 0.0
};
#endif

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
#ifdef HAVE_EXR
    gwy_file_func_register("openexr",
                           N_("OpenEXR images (.exr)"),
                           (GwyFileDetectFunc)&exr_detect,
                           (GwyFileLoadFunc)&exr_load,
                           NULL,
                           (GwyFileSaveFunc)&exr_export);
#endif
#ifdef HAVE_PNG
    gwy_file_func_register("pnggray16",
                           N_("Grayscale 16bit PNG images (.png)"),
                           (GwyFileDetectFunc)&png_detect,
                           (GwyFileLoadFunc)&png_load,
                           NULL,
                           NULL);
#endif

    return TRUE;
}

/***************************************************************************
 *
 * OpenEXR
 *
 ***************************************************************************/

#ifdef HAVE_EXR
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

static GwyContainer*
exr_load(const gchar *filename,
         GwyRunType mode,
         GError **error,
         const gchar *name)
{
    // FIXME: We can import files with metadata directly.
    if (mode != GWY_RUN_INTERACTIVE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_INTERACTIVE,
                    _("Pixmap image import must be run as interactive."));
        return NULL;
    }

    g_warning("EXR: Implement me!");
    err_NO_DATA(error);
    return NULL;
}

static gboolean
exr_export(GwyContainer *data,
           const gchar *filename,
           GwyRunType mode,
           GError **error)
{
    EXRSaveArgs args;
    GwyDataField *field;
    guint id;
    gboolean ok = TRUE;
    const gchar *title = "Data";
    gchar *imagedata, *key;

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data,
                                     GWY_APP_DATA_FIELD, &field,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    if (!field) {
        err_NO_CHANNEL_EXPORT(error);
        return FALSE;
    }

    exr_save_load_args(gwy_app_settings_get(), &args);
    find_range(field,
               &args.min, &args.max, &args.pmin, &args.pmax, &args.pcentre);

    if (mode == GWY_RUN_INTERACTIVE
        && !exr_save_dialog(&args, field)) {
        exr_save_save_args(gwy_app_settings_get(), &args);
        err_CANCELLED(error);
        return FALSE;
    }
    exr_save_save_args(gwy_app_settings_get(), &args);

    imagedata = create_image_data(field, args.bit_depth,
                                  args.zscale, args.min, args.max);
    key = g_strdup_printf("/%d/data/title", id);
    gwy_container_gis_string_by_name(data, key, (const guchar**)&title);
    g_free(key);

    try {
        exr_write_image(field, imagedata,
                        filename, title, args.bit_depth, args.zscale);
    }
    catch (const std::exception &exc) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("EXR image writing failed with libImf error: %s"),
                    exc.what());
        ok = FALSE;
    }

    g_free(imagedata);

    return ok;
}

static gboolean
exr_save_dialog(EXRSaveArgs *args,
                GwyDataField *field)
{
    GtkWidget *dialog, *label, *align;
    GtkTable *table;
    EXRSaveControls controls;
    gint row, response;
    GwySIValueFormat *vf;
    gchar *s;

    controls.args = args;
    controls.field = field;

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
    controls.zscale_label = label = gtk_label_new_with_mnemonic(_("_Z scale:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.zscale = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(controls.zscale), 10);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.zscale);
    gtk_table_attach(table, controls.zscale, 1, 2, row, row+1,
                     (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                     (GtkAttachOptions)0,
                     0, 0);
    g_signal_connect(controls.zscale, "activate",
                     G_CALLBACK(exr_save_zscale_changed), &controls);
    gwy_widget_set_activate_on_unfocus(controls.zscale, TRUE);

    s = gwy_si_unit_get_string(gwy_data_field_get_si_unit_z(field),
                               GWY_SI_UNIT_FORMAT_VFMARKUP);
    controls.zscale_units = label = gtk_label_new(s);
    g_free(s);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 2, 3, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);
    row++;

    gtk_table_set_row_spacing(table, row-1, 8);
    controls.header_data = label = gtk_label_new(_("Data"));
    gtk_table_attach(table, label, 1, 2, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);
    controls.header_representable = label = gtk_label_new(_("Representable"));
    gtk_table_attach(table, label, 2, 3, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);
    row++;

    controls.min_label = label = gtk_label_new(_("Minimum:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.min = label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 1, 2, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.min_image = label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 2, 3, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);
    row++;

    controls.max_label = label = gtk_label_new(_("Maximum:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.max = label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 1, 2, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.max_image = label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 2, 3, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);
    row++;

    gtk_table_set_row_spacing(table, row-1, 8);
    controls.centre_label = label = gtk_label_new("Suggested z scale:");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.centre = label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 1, 2, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    align = gtk_alignment_new(1.0, 0.5, 0.0, 0.0);
    gtk_table_attach(table, align, 2, 3, row, row+1,
                     GTK_FILL, (GtkAttachOptions)0, 0, 0);

    controls.use_centre = gtk_button_new_with_mnemonic(_("_Use"));
    gtk_container_add(GTK_CONTAINER(align), controls.use_centre);
    g_signal_connect(controls.use_centre, "clicked",
                     G_CALLBACK(exr_save_use_centre_clicked), &controls);
    row++;

    exr_save_update_zscale(&controls);
    exr_save_update_ranges(&controls);

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
exr_save_update_zscale(EXRSaveControls *controls)
{
    gdouble zscale = controls->args->zscale;
    gchar *s = g_strdup_printf("%g", zscale);
    gtk_entry_set_text(GTK_ENTRY(controls->zscale), s);
    g_free(s);
}

static void
exr_save_update_ranges(EXRSaveControls *controls)
{
    EXRSaveArgs *args = controls->args;
    gdouble zscale, rmin, rmax;
    GwySIUnit *unit;
    GwySIValueFormat *vf;
    gchar *s;
    gboolean sensitive;

    sensitive = (args->bit_depth == GWY_BIT_DEPTH_HALF);

    gtk_widget_set_sensitive(controls->zscale, sensitive);
    gtk_widget_set_sensitive(controls->zscale_label, sensitive);
    gtk_widget_set_sensitive(controls->zscale_units, sensitive);
    gtk_widget_set_sensitive(controls->header_data, sensitive);
    gtk_widget_set_sensitive(controls->header_representable, sensitive);
    gtk_widget_set_sensitive(controls->min, sensitive);
    gtk_widget_set_sensitive(controls->min_label, sensitive);
    gtk_widget_set_sensitive(controls->min_image, sensitive);
    gtk_widget_set_sensitive(controls->max, sensitive);
    gtk_widget_set_sensitive(controls->max_label, sensitive);
    gtk_widget_set_sensitive(controls->max_image, sensitive);
    gtk_widget_set_sensitive(controls->centre, sensitive);
    gtk_widget_set_sensitive(controls->centre_label, sensitive);
    gtk_widget_set_sensitive(controls->use_centre, sensitive);

    if (!sensitive) {
        gtk_label_set_text(GTK_LABEL(controls->min_image), "");
        gtk_label_set_text(GTK_LABEL(controls->max_image), "");
        gtk_label_set_text(GTK_LABEL(controls->centre), "");
        return;
    }

    unit = gwy_data_field_get_si_unit_z(controls->field);

    vf = gwy_si_unit_get_format_with_digits(unit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            args->min, 3, NULL);
    s = g_strdup_printf("%.*f%s%s",
                        vf->precision, args->min/vf->magnitude,
                        *vf->units ? " " : "",
                        vf->units);
    gtk_label_set_markup(GTK_LABEL(controls->min), s);
    g_free(s);

    vf = gwy_si_unit_get_format_with_digits(unit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            args->max, 3, vf);
    s = g_strdup_printf("%.*f%s%s",
                        vf->precision, args->max/vf->magnitude,
                        *vf->units ? " " : "",
                        vf->units);
    gtk_label_set_markup(GTK_LABEL(controls->max), s);
    g_free(s);

    zscale = suggest_zscale(args->bit_depth,
                            args->pmin, args->pmax, args->pcentre);
    vf = gwy_si_unit_get_format_with_digits(unit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            zscale, 3, vf);
    s = g_strdup_printf("%.*f%s%s",
                        vf->precision, zscale/vf->magnitude,
                        *vf->units ? " " : "",
                        vf->units);
    gtk_label_set_markup(GTK_LABEL(controls->centre), s);
    g_free(s);

    representable_range(args->bit_depth, args->zscale, &rmin, &rmax);
    vf = gwy_si_unit_get_format_with_digits(unit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            rmin, 3, vf);
    s = g_strdup_printf("%.*f%s%s",
                        vf->precision, rmin/vf->magnitude,
                        *vf->units ? " " : "",
                        vf->units);
    gtk_label_set_markup(GTK_LABEL(controls->min_image), s);
    g_free(s);

    representable_range(args->bit_depth, args->zscale, &rmin, &rmax);
    vf = gwy_si_unit_get_format_with_digits(unit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            rmax, 3, vf);
    s = g_strdup_printf("%.*f%s%s",
                        vf->precision, rmax/vf->magnitude,
                        *vf->units ? " " : "",
                        vf->units);
    gtk_label_set_markup(GTK_LABEL(controls->max_image), s);
    g_free(s);

    gwy_si_unit_value_format_free(vf);
}

static void
exr_save_bit_depth_changed(G_GNUC_UNUSED GtkWidget *button,
                           EXRSaveControls *controls)
{
    gint value = gwy_radio_buttons_get_current(controls->bit_depth);
    controls->args->bit_depth = (GwyBitDepth)value;
    exr_save_update_ranges(controls);
}

static void
exr_save_zscale_changed(GtkWidget *entry,
                        EXRSaveControls *controls)
{
    const gchar *value = gtk_entry_get_text(GTK_ENTRY(entry));
    gdouble zscale;

    if ((zscale = g_strtod(value, NULL)) > 0.0)
        controls->args->zscale = zscale;
    else
        exr_save_update_zscale(controls);

    exr_save_update_ranges(controls);
}

static void
exr_save_use_centre_clicked(G_GNUC_UNUSED GtkWidget *button,
                            EXRSaveControls *controls)
{
    EXRSaveArgs *args = controls->args;
    args->zscale = suggest_zscale(args->bit_depth,
                                  args->pmin, args->pmax, args->pcentre);
    exr_save_update_zscale(controls);
    gtk_widget_activate(controls->zscale);
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
    header.insert(GWY_IMGKEY_XREAL, Imf::DoubleAttribute(v));
    v = gwy_data_field_get_xreal(field);
    header.insert(GWY_IMGKEY_YREAL, Imf::DoubleAttribute(v));
    if (bit_depth == GWY_BIT_DEPTH_INT32) {
        gdouble zmin, zmax;
        gwy_data_field_get_min_max(field, &zmin, &zmax);
        header.insert(GWY_IMGKEY_ZMIN, Imf::DoubleAttribute(zmin));
        header.insert(GWY_IMGKEY_ZMAX, Imf::DoubleAttribute(zmax));
    }
    else if (zscale != 1.0)
        header.insert(GWY_IMGKEY_ZSCALE, Imf::DoubleAttribute(zscale));
    if ((v = gwy_data_field_get_xoffset(field)))
        header.insert(GWY_IMGKEY_XOFFSET, Imf::DoubleAttribute(v));
    if ((v = gwy_data_field_get_yoffset(field)))
        header.insert(GWY_IMGKEY_YOFFSET, Imf::DoubleAttribute(v));

    header.insert(GWY_IMGKEY_TITLE, Imf::StringAttribute(title));
    header.insert("Software", Imf::StringAttribute("Gwyddion"));

    gchar *s;
    s = gwy_si_unit_get_string(gwy_data_field_get_si_unit_xy(field),
                               GWY_SI_UNIT_FORMAT_PLAIN);
    header.insert(GWY_IMGKEY_XYUNIT, Imf::StringAttribute(s));
    g_free(s);

    s = gwy_si_unit_get_string(gwy_data_field_get_si_unit_z(field),
                               GWY_SI_UNIT_FORMAT_PLAIN);
    header.insert(GWY_IMGKEY_ZUNIT, Imf::StringAttribute(s));
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

static const gchar bit_depth_key[] = "/module/openexr/bit_depth";
static const gchar zscale_key[]    = "/module/openexr/zscale";

static void
exr_save_sanitize_args(EXRSaveArgs *args)
{
    if (args->bit_depth != GWY_BIT_DEPTH_HALF
        && args->bit_depth != GWY_BIT_DEPTH_INT32
        && args->bit_depth != GWY_BIT_DEPTH_FLOAT)
        args->bit_depth = GWY_BIT_DEPTH_HALF;

    if (!(args->zscale > 0.0))
        args->zscale = 1.0;
}

static void
exr_save_load_args(GwyContainer *container,
                   EXRSaveArgs *args)
{
    *args = exr_save_defaults;

    gwy_container_gis_double_by_name(container, zscale_key, &args->zscale);
    gwy_container_gis_enum_by_name(container, bit_depth_key,
                                   (guint*)&args->bit_depth);
    exr_save_sanitize_args(args);
}

static void
exr_save_save_args(GwyContainer *container,
                   EXRSaveArgs *args)
{
    gwy_container_set_double_by_name(container, zscale_key, args->zscale);
    gwy_container_set_enum_by_name(container, bit_depth_key, args->bit_depth);
}
#endif

/***************************************************************************
 *
 * PNG
 *
 ***************************************************************************/

#ifdef HAVE_PNG
static gint
png_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name,
           const gchar *name)
{
    typedef struct {
        guint width;
        guint height;
        guint bit_depth;
        guint colour_type;
        guint compression_method;
        guint filter_method;
        guint interlace_method;
    } IHDR;

    IHDR header;
    const guchar *p;

    // Export is done in pixmap.c, we cannot have multiple exporters of the
    // same type (unlike loaders).
    if (only_name)
        return 0;

    if (fileinfo->buffer_len < 64)
        return 0;
    if (memcmp(fileinfo->head, "\x89PNG\r\n\x1a\n\x00\x00\x00\x0dIHDR", 16)
        != 0)
        return 0;

    p = fileinfo->head + 16;
    header.width = gwy_get_guint32_be(&p);
    header.height = gwy_get_guint32_be(&p);
    header.bit_depth = *(p++);
    header.colour_type = *(p++);
    header.compression_method = *(p++);
    header.filter_method = *(p++);
    header.interlace_method = *(p++);
    if (!header.width || !header.height
        || header.colour_type != 0 || header.bit_depth != 16)
        return 0;

    return 95;
}

static gboolean
get_png_text_double(const png_textp text_chunks, guint ncomments,
                    const gchar *key, gdouble *value)
{
    guint i;

    for (i = 0; i < ncomments; i++) {
        if (gwy_strequal(text_chunks[i].key, key)) {
            *value = g_ascii_strtod(text_chunks[i].text, NULL);
            return TRUE;
        }
    }
    return FALSE;
}

static const gchar*
get_png_text_string(const png_textp text_chunks, guint ncomments,
                    const gchar *key)
{
    guint i;

    for (i = 0; i < ncomments; i++) {
        if (gwy_strequal(text_chunks[i].key, key))
            return text_chunks[i].text;
    }
    return NULL;
}

static GwyContainer*
png_load(const gchar *filename,
         GwyRunType mode,
         GError **error,
         const gchar *name)
{
    png_structp reader = NULL;
    png_infop reader_info = NULL;
    png_bytepp rows = NULL;
    png_textp text_chunks = NULL;
    png_int_32 pcal_X0, pcal_X1;
    png_charp pcal_purpose, pcal_units;
    png_charpp pcal_params;
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    guint transform_flags = PNG_TRANSFORM_SWAP_ENDIAN;
#endif
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    guint transform_flags = PNG_TRANSFORM_IDENTITY;
#endif
    GwyContainer *container = NULL;
    GwyDataField **fields = NULL;
    gdouble **data = NULL;
    FILE *fr = NULL;
    GwySIUnit *unitxy = NULL, *unitz = NULL;
    gboolean have_sCAL, have_pCAL, manual_import = TRUE;
    guint xres, yres, bit_depth, colour_type, nchannels, rowbytes, ncomments;
    guint id, i, j;
    int scal_unit, pcal_type, pcal_nparams, power10;
    gdouble xreal, yreal, xoff, yoff, zmin, zmax, q, scal_xreal, scal_yreal;
    const gchar *title = NULL;
    png_byte magic[8];

    if (!(fr = g_fopen(filename, "rb"))) {
        err_OPEN_READ(error);
        goto fail;
    }
    if (fread(magic, 1, sizeof(magic), fr) != sizeof(magic)) {
        err_READ(error);
        goto fail;
    }
    if (png_sig_cmp(magic, 0, sizeof(magic)) != 0) {
        err_FILE_TYPE(error, "PNG");
        goto fail;
    }

    reader = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!reader) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("libpng initialization error (in %s)"),
                    "png_create_read_struct");
        goto fail;
    }

    reader_info = png_create_info_struct(reader);
    if (!reader_info) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("libpng initialization error (in %s)"),
                    "png_create_info_struct");
        goto fail;
    }

    if (setjmp(png_jmpbuf(reader))) {
        /* FIXME: Not very helpful.  Thread-unsafe. */
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("libpng error occured"));
        goto fail;
    }

    png_init_io(reader, fr);
    png_set_sig_bytes(reader, sizeof(magic));
    /* The same as in err_DIMENSIONS(). */
    png_set_user_limits(reader, 1 << 15, 1 << 15);
    png_read_png(reader, reader_info, transform_flags, NULL);
    /* png_get_IHDR() causes trouble with the too type-picky C++ compiler. */
    xres = png_get_image_width(reader, reader_info);
    yres = png_get_image_height(reader, reader_info);
    bit_depth = png_get_bit_depth(reader, reader_info);
    if (bit_depth != 16) {
        err_BPP(error, bit_depth);
        goto fail;
    }
    colour_type = png_get_color_type(reader, reader_info);
    nchannels = png_get_channels(reader, reader_info);
    gwy_debug("xres: %u, yres: %u, bit_depth: %u, type: %u, nchannels: %u",
              xres, yres, bit_depth, colour_type, nchannels);
    rowbytes = png_get_rowbytes(reader, reader_info);
    ncomments = png_get_text(reader, reader_info, &text_chunks, NULL);
    have_sCAL = png_get_sCAL(reader, reader_info,
                             &scal_unit, &scal_xreal, &scal_yreal);
    have_pCAL = png_get_pCAL(reader, reader_info,
                             &pcal_purpose, &pcal_X0, &pcal_X1, &pcal_type,
                             &pcal_nparams, &pcal_units, &pcal_params);
    gwy_debug("ncomments: %u, sCAL: %d, pCAL: %d",
              ncomments, have_sCAL, have_pCAL);
    rows = png_get_rows(reader, reader_info);

    /* Gwyddion tEXT chunks. */
    if (get_png_text_double(text_chunks, ncomments,
                            GWY_IMGKEY_XREAL, &xreal)
        && get_png_text_double(text_chunks, ncomments,
                               GWY_IMGKEY_YREAL, &yreal)
        && get_png_text_double(text_chunks, ncomments,
                               GWY_IMGKEY_ZMIN, &zmin)
        && get_png_text_double(text_chunks, ncomments,
                               GWY_IMGKEY_ZMAX, &zmax)) {
        gwy_debug("Found Gwyddion image keys, using for direct import.");
        xoff = yoff = 0.0;
        get_png_text_double(text_chunks, ncomments, GWY_IMGKEY_XOFFSET, &xoff);
        get_png_text_double(text_chunks, ncomments, GWY_IMGKEY_YOFFSET, &yoff);
        unitxy = gwy_si_unit_new_parse(get_png_text_string(text_chunks,
                                                           ncomments,
                                                           GWY_IMGKEY_XYUNIT),
                                       &power10);
        q = pow10(power10);
        xreal *= q;
        yreal *= q;
        xoff *= q;
        yoff *= q;
        unitz = gwy_si_unit_new_parse(get_png_text_string(text_chunks,
                                                          ncomments,
                                                          GWY_IMGKEY_ZUNIT),
                                      &power10);
        q = pow10(power10);
        zmin *= q;
        zmax *= q;
        title = get_png_text_string(text_chunks, ncomments, GWY_IMGKEY_TITLE);

        if (!((xreal = fabs(xreal)) > 0.0)) {
            g_warning("Real y size is 0.0, fixing to 1.0");
            xreal = 1.0;
        }
        if (!((xreal = fabs(xreal)) > 0.0)) {
            g_warning("Real y size is 0.0, fixing to 1.0");
            xreal = 1.0;
        }
        manual_import = FALSE;
    }
    else if (have_sCAL
             && have_pCAL
             && pcal_nparams == 2
             && gwy_strequal(pcal_purpose, "Z")) {
        gwy_debug("Found sCAL and pCAL chnunks, using for direct import.");
        if (pcal_X0 != 0 || pcal_X1 != G_MAXUINT16)
            g_warning("PNG pCAL X0 and X1 transform is not implemented");

        xreal = scal_xreal;
        yreal = scal_yreal;
        xoff = yoff = 0.0;
        zmin = g_ascii_strtod(pcal_params[0], NULL);
        zmax = zmin + G_MAXUINT16*g_ascii_strtod(pcal_params[0], NULL);

        unitxy = gwy_si_unit_new("m");
        unitz = gwy_si_unit_new_parse(pcal_units, &power10);
        q = pow10(power10);
        zmin *= q;
        zmax *= q;

        if (!((xreal = fabs(xreal)) > 0.0)) {
            g_warning("Real y size is 0.0, fixing to 1.0");
            xreal = 1.0;
        }
        if (!((xreal = fabs(xreal)) > 0.0)) {
            g_warning("Real y size is 0.0, fixing to 1.0");
            xreal = 1.0;
        }
        manual_import = FALSE;
    }
    else {
        gwy_debug("Manual import is necessary.");
    }

    if (!title)
        title = get_png_text_string(text_chunks, ncomments, "Title");

    if (manual_import) {
        g_warning("PNG: Implement me!");
        err_NO_DATA(error);
        goto fail;
    }

    fields = g_new(GwyDataField*, nchannels);
    data = g_new(gdouble*, nchannels);
    for (id = 0; id < nchannels; id++) {
        GwyDataField *f;

        fields[id] = f = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
        gwy_serializable_clone(G_OBJECT(unitxy),
                               G_OBJECT(gwy_data_field_get_si_unit_xy(f)));
        gwy_serializable_clone(G_OBJECT(unitz),
                               G_OBJECT(gwy_data_field_get_si_unit_z(f)));
        gwy_data_field_set_xoffset(f, xoff);
        gwy_data_field_set_yoffset(f, yoff);
        data[id] = gwy_data_field_get_data(f);
    }

    q = (zmax - zmin)/G_MAXUINT16;
    for (i = 0; i < yres; i++) {
        const guint16 *row = (const guint16*)rows[i];
        for (j = 0; j < xres; j++) {
            for (id = 0; id < nchannels; id++) {
                data[id][i*xres + j] = q*row[j*nchannels + id] + zmin;
            }
        }
    }

    container = gwy_container_new();
    for (id = 0; id < nchannels; id++) {
        const gchar *basetitle;
        gchar buf[40];
        gchar *t = NULL;

        g_snprintf(buf, sizeof(buf), "/%u/data", id);
        gwy_container_set_object_by_name(container, buf, fields[id]);
        g_object_unref(fields[id]);

        g_snprintf(buf, sizeof(buf), "/%u/data/title", id);
        if (nchannels == 1)
            basetitle = "Gray";
        else if (nchannels == 2)
            basetitle = id ? "Alpha" : "Gray";
        else if (nchannels == 3)
            basetitle = id ? (id == 1 ? "G" : "B") : "R";
        else if (nchannels == 4)
            basetitle = id ? (id == 1 ? "G" : (id == 2 ? "B" : "Alpha")) : "R";
        else
            basetitle = NULL;

        if (title && (nchannels == 1 || !basetitle))
            t = g_strdup(title);
        else if (title)
            t = g_strdup_printf("%s %s", basetitle, title);
        else if (basetitle)
            t = g_strdup(basetitle);

        if (t)
            gwy_container_set_string_by_name(container, buf, (const guchar*)t);
    }

fail:
    g_free(data);
    g_free(fields);
    gwy_object_unref(unitxy);
    gwy_object_unref(unitz);
    if (reader)
        png_destroy_read_struct(&reader,
                                reader_info ? &reader_info : NULL,
                                NULL);
    if (fr)
        fclose(fr);
    return container;
}
#endif

/***************************************************************************
 *
 * Common HDR image functions
 *
 ***************************************************************************/

G_GNUC_UNUSED static gdouble
suggest_zscale(GwyBitDepth bit_depth,
               gdouble pmin, gdouble pmax, gdouble pcentre)
{
    if (bit_depth == GWY_BIT_DEPTH_FLOAT)
        return 1.0;

    g_return_val_if_fail(bit_depth == GWY_BIT_DEPTH_HALF, 1.0);

    // Range OK as-is
    if (pmin >= HALF_NRM_MIN && pmax <= HALF_MAX)
        return 1.0;

    // Range OK if scaled
    if (pmax/pmin < (double)HALF_MAX/HALF_NRM_MIN)
        return sqrt(pmax/HALF_MAX * pmin/HALF_NRM_MIN);

    // Range not OK, may need a bit more sopistication here...
    return pcentre;
}

G_GNUC_UNUSED static void
representable_range(GwyBitDepth bit_depth, gdouble zscale,
                    gdouble *min, gdouble *max)
{
    if (bit_depth == GWY_BIT_DEPTH_FLOAT) {
        *min = zscale*G_MINFLOAT;
        *max = zscale*G_MAXFLOAT;
    }
    else if (bit_depth == GWY_BIT_DEPTH_HALF) {
        *min = zscale*HALF_NRM_MIN;
        *max = zscale*HALF_MAX;
    }
    else {
        g_assert_not_reached();
    }
}

G_GNUC_UNUSED static gchar*
create_image_data(GwyDataField *field,
                  GwyBitDepth bit_depth,
                  gdouble zscale,
                  gdouble zmin,
                  gdouble zmax)
{
    guint xres = gwy_data_field_get_xres(field);
    guint yres = gwy_data_field_get_yres(field);
    const gdouble *d = gwy_data_field_get_data_const(field);
    gchar *retval = NULL;
    guint i;

    if (zscale == GWY_BIT_DEPTH_INT16) {
        guint16 *imagedata = g_new(guint16, xres*yres);
        gdouble q = (G_MAXUINT16 + 0.999)/(zmax - zmin);
        retval = (gchar*)imagedata;

        for (i = xres*yres; i; i--, d++, imagedata++)
            *imagedata = (guint16)CLAMP(q*(*d - zmin),
                                        0.0, G_MAXUINT16 + 0.999);
    }
    else if (bit_depth == GWY_BIT_DEPTH_INT32) {
        guint32 *imagedata = g_new(guint32, xres*yres);
        gdouble q = (G_MAXUINT32 + 0.999)/(zmax - zmin);
        retval = (gchar*)imagedata;

        for (i = xres*yres; i; i--, d++, imagedata++)
            *imagedata = (guint32)CLAMP(q*(*d - zmin),
                                        0.0, G_MAXUINT32 + 0.999);
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

G_GNUC_UNUSED static void
find_range(GwyDataField *field,
           gdouble *fmin, gdouble *fmax,
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

    gwy_data_field_get_min_max(field, fmin, fmax);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
