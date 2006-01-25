/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <libgwyddion/gwymacros.h>

#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libdraw/gwypixfield.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <app/settings.h>

#include "err.h"
#include "rawfilepreset.h"

#define EPS 1e-6

/* Special-cased text data delimiters */
enum {
    RAW_DELIM_ANY_WHITESPACE = -1,
    RAW_DELIM_OTHER          = -2,
    RAW_DELIM_TAB            =  9,
};

enum {
    RESPONSE_RESET
};

/* Preset table columns */
enum {
    RAW_PRESET_NAME = 0,
    RAW_PRESET_TYPE,
    RAW_PRESET_SIZE,
    RAW_PRESET_INFO,
    RAW_PRESET_LAST
};

enum {
    RAW_ASCII_PARSE_ERROR = 1,
};

typedef gdouble (*RawStrtodFunc)(const gchar *nptr, gchar **endptr);

typedef struct {
    gboolean takeover;
    GString *preset;
    gboolean xyreseq;
    gboolean xymeasureeq;
    GwyRawFilePresetData p;
} RawFileArgs;

typedef struct {
    const gchar *filename;
    guint filesize;
    guchar *buffer;
} RawFileFile;

typedef struct {
    gboolean in_update;
    GtkWidget *dialog;
    GtkWidget *takeover;
    GSList *format;
    GtkWidget *builtin;
    GtkWidget *offset;
    GtkWidget *size;
    GtkWidget *skip;
    GtkWidget *rowskip;
    GtkWidget *sign;
    GtkWidget *revbyte;
    GtkWidget *revsample;
    GtkWidget *byteswap;
    GtkWidget *byteswap_label;
    GtkWidget *lineoffset;
    GtkWidget *delimmenu;
    GtkWidget *delim_label;
    GtkWidget *delimiter;
    GtkWidget *otherdelim_label;
    GtkWidget *skipfields;
    GtkWidget *decomma;
    GtkWidget *xres;
    GtkWidget *yres;
    GtkWidget *xyreseq;
    GtkWidget *xreal;
    GtkWidget *yreal;
    GtkWidget *xymeasureeq;
    GtkWidget *xyexponent;
    GtkWidget *zscale;
    GtkWidget *zexponent;
    GtkWidget *presetlist;
    GtkWidget *presetname;
    GtkWidget *preview;
    GtkWidget *do_preview;
    GtkWidget *save;
    GtkWidget *load;
    GtkWidget *delete;
    GtkWidget *rename;
    GwyGradient *gradient;
    RawFileArgs *args;
    RawFileFile *file;
} RawFileControls;

static gboolean      module_register               (const gchar *name);
static gint          rawfile_detect                (void);
static GwyContainer* rawfile_load                  (const gchar *filename,
                                                    GwyRunType mode,
                                                    GError **error);
static GwyDataField* rawfile_dialog                (RawFileArgs *args,
                                                    RawFileFile *file);
static GtkWidget*    rawfile_dialog_preview_box    (RawFileControls *controls);
static GtkWidget*    rawfile_dialog_info_page      (RawFileArgs *args,
                                                    RawFileFile *file,
                                                    RawFileControls *controls);
static GtkWidget*    rawfile_dialog_format_page    (RawFileArgs *args,
                                                    RawFileControls *controls);
static GtkWidget*    rawfile_dialog_preset_page    (RawFileArgs *args,
                                                    RawFileControls *controls);
static GwyDataField* rawfile_read_data_field       (GtkWidget *parent,
                                                    RawFileArgs *args,
                                                    RawFileFile *file);
static void          rawfile_warn_too_short_file   (GtkWidget *parent,
                                                    RawFileFile *file,
                                                    guint reqsize);
static void          rawfile_warn_parse_error      (GtkWidget *parent,
                                                    RawFileFile *file,
                                                    GError *err);
static void          builtin_changed_cb            (GtkWidget *combo,
                                                    RawFileControls *controls);
static void          delimiter_changed_cb          (GtkWidget *combo,
                                                    RawFileControls *controls);
static void          xyres_changed_cb              (GtkAdjustment *adj,
                                                    RawFileControls *controls);
static void          xyreseq_changed_cb            (RawFileControls *controls);
static void          xyreal_changed_cb             (GtkAdjustment *adj,
                                                    RawFileControls *controls);
static void          xymeasureeq_changed_cb        (RawFileControls *controls);
static void          bintext_changed_cb            (GtkWidget *button,
                                                    RawFileControls *controls);
static void          preview_cb                    (RawFileControls *controls);
static void          preset_selected_cb            (RawFileControls *controls);
static void          preset_load_cb                (RawFileControls *controls);
static void          preset_store_cb               (RawFileControls *controls);
static void          preset_rename_cb              (RawFileControls *controls);
static void          preset_delete_cb              (RawFileControls *controls);
static gboolean      preset_validate_name          (RawFileControls *controls,
                                                    const gchar *name,
                                                    gboolean show_warning);
static void          update_dialog_controls        (RawFileControls *controls);
static void          update_dialog_values          (RawFileControls *controls);
static GtkWidget*    table_attach_heading          (GtkWidget *table,
                                                    const gchar *text,
                                                    gint row);
static void          rawfile_read_builtin          (RawFileArgs *args,
                                                    guchar *buffer,
                                                    gdouble *data);
static void          rawfile_read_bits             (RawFileArgs *args,
                                                    guchar *buffer,
                                                    gdouble *data);
static gboolean      rawfile_read_ascii            (RawFileArgs *args,
                                                    guchar *buffer,
                                                    gdouble *data,
                                                    GError **error);
static gdouble       gwy_comma_strtod              (const gchar *nptr,
                                                    gchar **endptr);
static void          rawfile_sanitize_args         (RawFileArgs *args);
static void          rawfile_load_args             (GwyContainer *settings,
                                                    RawFileArgs *args);
static void          rawfile_save_args             (GwyContainer *settings,
                                                    const RawFileArgs *args);
static guint         rawfile_compute_required_size (RawFileArgs *args);
static void          rawfile_import_1x_presets     (GwyContainer *settings);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports raw data files, both ASCII and binary, according to "
       "user-specified format."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* precomputed bitmask up to 32 bits */
static const guint32 BITMASK[] = {
    0x00000001UL, 0x00000003UL, 0x00000007UL, 0x0000000fUL,
    0x0000001fUL, 0x0000003fUL, 0x0000007fUL, 0x000000ffUL,
    0x000001ffUL, 0x000003ffUL, 0x000007ffUL, 0x00000fffUL,
    0x00001fffUL, 0x00003fffUL, 0x00007fffUL, 0x0000ffffUL,
    0x0001ffffUL, 0x0003ffffUL, 0x0007ffffUL, 0x000fffffUL,
    0x001fffffUL, 0x003fffffUL, 0x007fffffUL, 0x00ffffffUL,
    0x01ffffffUL, 0x03ffffffUL, 0x07ffffffUL, 0x0fffffffUL,
    0x1fffffffUL, 0x3fffffffUL, 0x7fffffffUL, 0xffffffffUL,
};

static const GwyEnum builtin_menu[] = {
    { N_("User-specified"),       RAW_NONE            },
    { N_("Signed byte"),          RAW_SIGNED_BYTE     },
    { N_("Unsigned byte"),        RAW_UNSIGNED_BYTE   },
    { N_("Signed 16bit word"),    RAW_SIGNED_WORD16   },
    { N_("Unsigned 16bit word"),  RAW_UNSIGNED_WORD16 },
    { N_("Signed 32bit word"),    RAW_SIGNED_WORD32   },
    { N_("Unsigned 32bit word"),  RAW_UNSIGNED_WORD32 },
    { N_("IEEE single"),          RAW_IEEE_FLOAT      },
    { N_("IEEE double"),          RAW_IEEE_DOUBLE     },
};

static const gboolean takeover_default    = FALSE;
static const gboolean xyreseq_default     = TRUE;
static const gboolean xymeasureeq_default = TRUE;

static const gchar builtin_key[]     = "/module/rawfile/builtin";
static const gchar byteswap_key[]    = "/module/rawfile/byteswap";
static const gchar decomma_key[]     = "/module/rawfile/decomma";
static const gchar delimiter_key[]   = "/module/rawfile/delimiter";
static const gchar format_key[]      = "/module/rawfile/format";
static const gchar lineoffset_key[]  = "/module/rawfile/lineoffset";
static const gchar offset_key[]      = "/module/rawfile/offset";
static const gchar preset_key[]      = "/module/rawfile/preset";
static const gchar revbyte_key[]     = "/module/rawfile/revbyte";
static const gchar revsample_key[]   = "/module/rawfile/revsample";
static const gchar rowskip_key[]     = "/module/rawfile/rowskip";
static const gchar sign_key[]        = "/module/rawfile/sign";
static const gchar size_key[]        = "/module/rawfile/size";
static const gchar skip_key[]        = "/module/rawfile/skip";
static const gchar skipfields_key[]  = "/module/rawfile/skipfields";
static const gchar takeover_key[]    = "/module/rawfile/takeover";
static const gchar xreal_key[]       = "/module/rawfile/xreal";
static const gchar xres_key[]        = "/module/rawfile/xres";
static const gchar xyexponent_key[]  = "/module/rawfile/xyexponent";
static const gchar xymeasureeq_key[] = "/module/rawfile/xymeasureeq";
static const gchar xyreseq_key[]     = "/module/rawfile/xyreseq";
static const gchar xyunit_key[]      = "/module/rawfile/xyunit";
static const gchar yreal_key[]       = "/module/rawfile/yreal";
static const gchar yres_key[]        = "/module/rawfile/yres";
static const gchar zexponent_key[]   = "/module/rawfile/zexponent";
static const gchar zscale_key[]      = "/module/rawfile/zscale";
static const gchar zunit_key[]       = "/module/rawfile/zunit";

/* for read_ascii_data() error reporting */
static GQuark error_domain = 0;

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static gint types_initialized = 0;
    GwyResourceClass *klass;

    if (!types_initialized) {
        types_initialized += gwy_raw_file_preset_get_type();
        klass = g_type_class_ref(GWY_TYPE_RAW_FILE_PRESET);
        gwy_resource_class_load(klass);
        g_type_class_unref(klass);
    }
    gwy_file_func_register("rawfile",
                           N_("Raw data files"),
                           (GwyFileDetectFunc)&rawfile_detect,
                           (GwyFileLoadFunc)&rawfile_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
rawfile_detect(void)
{
    GwyContainer *settings;
    gboolean takeover = takeover_default;

    settings = gwy_app_settings_get();
    gwy_container_gis_boolean_by_name(settings, takeover_key, &takeover);

    /* Claim ownership of anything, with lowest possible priority */
    if (takeover)
        return 1;
    return 0;
}

static GwyContainer*
rawfile_load(const gchar *filename,
             GwyRunType mode,
             GError **error)
{
    RawFileArgs args;
    RawFileFile file;
    GwyContainer *settings, *data;
    GwyDataField *dfield;
    GError *err = NULL;
    gsize size = 0;

    if (mode != GWY_RUN_INTERACTIVE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_INTERACTIVE,
                    _("Raw data import must be run as interactive."));
        return FALSE;
    }

    settings = gwy_app_settings_get();
    rawfile_import_1x_presets(settings);
    rawfile_load_args(settings, &args);
    file.buffer = NULL;
    if (!g_file_get_contents(filename, (gchar**)&file.buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    data = NULL;
    file.filename = filename;
    file.filesize = size;
    if ((dfield = rawfile_dialog(&args, &file))) {
        data = gwy_container_new();
        gwy_container_set_object_by_name(data, "/0/data", dfield);
        g_object_unref(dfield);
    }
    else
        err_CANCELLED(error);
    rawfile_save_args(settings, &args);
    g_free(file.buffer);
    g_string_free(args.preset, TRUE);
    g_free(args.p.delimiter);
    g_free(args.p.xyunit);
    g_free(args.p.zunit);

    return data;
}

static GwyDataField*
rawfile_dialog(RawFileArgs *args,
               RawFileFile *file)
{
    RawFileControls controls;
    GwyDataField *dfield = NULL;
    GtkWidget *dialog, *vbox, *label, *notebook, *hbox;
    GtkAdjustment *adj2;
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Read Raw File"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;
    controls.args = args;
    controls.file = file;
    controls.gradient = gwy_gradients_get_gradient(NULL);
    gwy_resource_use(GWY_RESOURCE(controls.gradient));
    controls.in_update = FALSE;

    vbox = GTK_DIALOG(dialog)->vbox;

    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(hbox), notebook, TRUE, TRUE, 0);

    /* Sample info */
    vbox = rawfile_dialog_info_page(args, file, &controls);
    label = gtk_label_new(_("Information"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

    /* General data format */
    vbox = rawfile_dialog_format_page(args, &controls);
    label = gtk_label_new(_("Data Format"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

    /* Presets */
    vbox = rawfile_dialog_preset_page(args, &controls);
    label = gtk_label_new(_("Presets"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

    gtk_box_pack_start(GTK_BOX(hbox), gtk_vseparator_new(), TRUE, TRUE, 0);

    /* Preview */
    vbox = rawfile_dialog_preview_box(&controls);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    /* Callbacks */
    update_dialog_controls(&controls);

    /* xres/yres sync */
    g_signal_connect_swapped(controls.xyreseq, "toggled",
                             G_CALLBACK(xyreseq_changed_cb), &controls);
    adj2 = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls.xres));
    g_signal_connect(adj2, "value-changed",
                     G_CALLBACK(xyres_changed_cb), &controls);
    adj2 = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls.yres));
    g_signal_connect(adj2, "value-changed",
                     G_CALLBACK(xyres_changed_cb), &controls);

    /* xreal/yreal sync */
    g_signal_connect_swapped(controls.xymeasureeq, "toggled",
                             G_CALLBACK(xymeasureeq_changed_cb), &controls);
    adj2 = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls.xreal));
    g_signal_connect(adj2, "value-changed",
                     G_CALLBACK(xyreal_changed_cb), &controls);
    adj2 = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls.yreal));
    g_signal_connect(adj2, "value-changed",
                     G_CALLBACK(xyreal_changed_cb), &controls);

    /* preview */
    g_signal_connect_swapped(controls.do_preview, "clicked",
                             G_CALLBACK(preview_cb), &controls);

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        update_dialog_values(&controls);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            gwy_resource_release(GWY_RESOURCE(controls.gradient));
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            dfield = rawfile_read_data_field(dialog, args, file);
            if (!dfield)
                response = GTK_RESPONSE_NONE;
            break;

            case RESPONSE_RESET:
            gwy_raw_file_preset_data_copy(&rawfilepresetdata_default, &args->p);
            /* TODO: Set xyeq, preset name */
            break;

            default:
            g_assert_not_reached();
            break;
        }
        update_dialog_controls(&controls);
    } while (response != GTK_RESPONSE_OK);
    gwy_resource_release(GWY_RESOURCE(controls.gradient));
    gtk_widget_destroy(dialog);

    return dfield;
}

static GtkWidget*
rawfile_dialog_preview_box(RawFileControls *controls)
{
    GtkWidget *align, *label, *vbox;
    GdkPixbuf *pixbuf;

    align = gtk_alignment_new(0.5, 0.0, 0.0, 0.0);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(align), vbox);

    label = gtk_label_new(_("Preview"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    controls->preview = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(vbox), controls->preview, FALSE, FALSE, 0);

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 120, 120);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    gdk_pixbuf_fill(pixbuf, 0);
    gtk_image_set_from_pixbuf(GTK_IMAGE(controls->preview), pixbuf);
    g_object_unref(pixbuf);

    controls->do_preview = gtk_button_new_with_mnemonic(_("_Update"));
    gtk_box_pack_start(GTK_BOX(vbox), controls->do_preview, FALSE, FALSE, 4);

    return align;
}

static GtkWidget*
rawfile_dialog_info_page(RawFileArgs *args,
                         RawFileFile *file,
                         RawFileControls *controls)
{
    GtkWidget *vbox, *label, *table, *button, *align;
    GwySIValueFormat *format;
    GwySIUnit *unit;
    GtkObject *adj;
    gint row;
    gchar *s;

    vbox = gtk_vbox_new(FALSE, 0);   /* to prevent notebook expanding tables */

    table = gtk_table_new(16, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>File</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 3, row, row+1);
    row++;

    s = g_path_get_basename(file->filename);
    label = gtk_label_new(s);
    g_free(s);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 2, row, row+1);

    unit = gwy_si_unit_new("B");
    format = gwy_si_unit_get_format(unit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                    file->filesize, NULL);
    s = g_strdup_printf("(%.*f %s)", format->precision,
                        file->filesize/format->magnitude, format->units);
    gwy_si_unit_value_format_free(format);
    label = gtk_label_new(s);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 2, 3, row, row+1);
    g_free(s);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    table_attach_heading(table, _("<b>Resolution</b>"), row);
    row++;

    adj = gtk_adjustment_new(args->p.xres, 1, 16384, 1, 10, 100);
    controls->xres = gwy_table_attach_spinbutton(table, row,
                                                 _("_Horizontal size:"),
                                                 _("data samples"), adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls->xres), 0);
    row++;

    adj = gtk_adjustment_new(args->p.yres, 1, 16384, 1, 10, 100);
    controls->yres = gwy_table_attach_spinbutton(table, row,
                                                 _("_Vertical size:"),
                                                 _("data samples"), adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls->yres), 0);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("S_quare sample"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls->xyreseq = button;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    table_attach_heading(table, _("<b>Physical dimensions</b>"), row);
    row++;

    adj = gtk_adjustment_new(args->p.xreal, 0.01, 10000, 1, 100, 100);
    controls->xreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls->xreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls->xreal,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new_with_mnemonic(_("_Width:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls->xreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    align = gtk_alignment_new(0.0, 0.5, 0.2, 0.0);
    gwy_si_unit_set_unit_string(unit, "m");
    controls->xyexponent = gwy_combo_box_metric_unit_new(NULL, NULL,
                                                         -12, 3, unit,
                                                         args->p.xyexponent);
    gtk_container_add(GTK_CONTAINER(align), controls->xyexponent);
    gtk_table_attach(GTK_TABLE(table), align, 2, 3, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 2, 2);
    row++;

    adj = gtk_adjustment_new(args->p.yreal, 0.01, 10000, 1, 100, 100);
    controls->yreal = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls->yreal), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls->yreal,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new_with_mnemonic(_("H_eight:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls->yreal);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Identical _measures"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls->xymeasureeq = button;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    adj = gtk_adjustment_new(args->p.zscale, 0.01, 10000, 1, 100, 100);
    controls->zscale = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(controls->zscale), TRUE);
    gtk_table_attach(GTK_TABLE(table), controls->zscale,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new_with_mnemonic(_("_Z-scale (per sample unit):"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls->zscale);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    align = gtk_alignment_new(0.0, 0.5, 0.2, 0.0);
    controls->zexponent = gwy_combo_box_metric_unit_new(NULL, NULL,
                                                        -12, 3, unit,
                                                        args->p.zexponent);
    gtk_container_add(GTK_CONTAINER(align), controls->zexponent);
    gtk_table_attach(GTK_TABLE(table), align, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 2, 2);
    g_object_unref(unit);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    table_attach_heading(table, _("<b>Options</b>"), row);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("_Automatically offer raw "
                                                  "data import of unknown "
                                                  "files"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls->takeover = button;
    row++;

    return vbox;
}

static GtkWidget*
rawfile_dialog_format_page(RawFileArgs *args,
                           RawFileControls *controls)
{
    static const GwyEnum formats[] = {
        { N_("_Text data"),   RAW_TEXT,   },
        { N_("_Binary data"), RAW_BINARY, },
    };
    static const GwyEnum delimiter_menu[] = {
        { N_("Any whitespace"),       RAW_DELIM_ANY_WHITESPACE  },
        { N_("TAB character"),        RAW_DELIM_TAB             },
        { N_("Ohter character"),      RAW_DELIM_OTHER           },
    };
    GtkWidget *vbox, *label, *table, *button, *entry, *omenu, *combo;
    GtkObject *adj;
    gint row;

    row = 0;
    vbox = gtk_vbox_new(FALSE, 0);   /* to prevent notebook expanding tables */

    table = gtk_table_new(15, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_table_set_col_spacings(GTK_TABLE(table), 2);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

    controls->format = gwy_radio_buttons_create(formats, G_N_ELEMENTS(formats),
                                                "format",
                                                G_CALLBACK(bintext_changed_cb),
                                                controls,
                                                args->p.format);
    gtk_table_attach_defaults(GTK_TABLE(table),
                              GTK_WIDGET(controls->format->data),
                              0, 3, row, row+1);
    row++;

    adj = gtk_adjustment_new(args->p.lineoffset, 0, 1 << 30, 1, 10, 10);
    controls->lineoffset = gwy_table_attach_spinbutton(table, row,
                                                       _("Start from _line:"),
                                                       "", adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls->lineoffset), 0);
    row++;

    adj = gtk_adjustment_new(args->p.skipfields, 0, 1 << 30, 1, 10, 10);
    controls->skipfields = gwy_table_attach_spinbutton(table, row,
                                                       _("E_ach row skip:"),
                                                       "fields", adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls->skipfields), 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Field delimiter:"));
    controls->delim_label = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    combo = gwy_enum_combo_box_new(delimiter_menu, G_N_ELEMENTS(delimiter_menu),
                                   G_CALLBACK(delimiter_changed_cb), controls,
                                   -1, TRUE);
    controls->delimmenu = combo;
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
    gtk_table_attach(GTK_TABLE(table), combo, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Other delimiter:"));
    controls->otherdelim_label = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    controls->delimiter = entry = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    gtk_entry_set_max_length(GTK_ENTRY(entry), 17);
    gtk_table_attach(GTK_TABLE(table), entry, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("_Decimal separator "
                                                  "is comma"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls->decomma = button;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 12);
    row++;

    gtk_table_attach_defaults(GTK_TABLE(table),
                              GTK_WIDGET(controls->format->next->data),
                              0, 1, row, row+1);

    omenu = gwy_enum_combo_box_new(builtin_menu, G_N_ELEMENTS(builtin_menu),
                                   G_CALLBACK(builtin_changed_cb), controls,
                                   args->p.builtin, TRUE);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    controls->builtin = omenu;
    row++;

    label = gtk_label_new_with_mnemonic(_("Byte s_wap pattern:"));
    controls->byteswap_label = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    controls->byteswap = entry = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    gtk_entry_set_max_length(GTK_ENTRY(entry), 17);
    gtk_table_attach(GTK_TABLE(table), entry, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    adj = gtk_adjustment_new(args->p.offset, 0, 1 << 30, 16, 1024, 1024);
    controls->offset = gwy_table_attach_spinbutton(table, row,
                                                   _("Start at _offset:"),
                                                   "bytes", adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls->offset), 0);
    row++;

    adj = gtk_adjustment_new(args->p.size, 1, 24, 1, 8, 8);
    controls->size = gwy_table_attach_spinbutton(table, row,
                                                 _("_Sample size:"),
                                                 "bits", adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls->size), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(controls->size), TRUE);
    row++;

    adj = gtk_adjustment_new(args->p.skip, 0, 1 << 30, 1, 8, 8);
    controls->skip = gwy_table_attach_spinbutton(table, row,
                                                 _("After each sample s_kip:"),
                                                 "bits", adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls->skip), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(controls->skip), TRUE);
    row++;

    adj = gtk_adjustment_new(args->p.rowskip, 0, 1 << 30, 1, 8, 8);
    controls->rowskip = gwy_table_attach_spinbutton(table, row,
                                                    _("After each _row skip:"),
                                                    "bits", adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls->rowskip), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(controls->rowskip), TRUE);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("_Reverse bits in bytes"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls->revbyte = button;
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Reverse bi_ts in samples"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls->revsample = button;
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Samples are si_gned"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls->sign = button;
    row++;

    return vbox;
}

static void
rawfile_preset_cell_renderer(G_GNUC_UNUSED GtkTreeViewColumn *column,
                             GtkCellRenderer *cell,
                             GtkTreeModel *model,
                             GtkTreeIter *piter,
                             gpointer data)
{
    GwyRawFilePreset *preset;
    gulong id;
    const gchar *delim;
    gchar *s;

    id = GPOINTER_TO_UINT(data);
    g_assert(id < RAW_PRESET_LAST);
    gtk_tree_model_get(model, piter, 0, &preset, -1);
    switch (id) {
        case RAW_PRESET_NAME:
        g_object_set(cell, "text", gwy_resource_get_name(GWY_RESOURCE(preset)),
                     NULL);
        break;

        case RAW_PRESET_TYPE:
        s = preset->data.format == RAW_BINARY ? _("Binary") : _("Text"),
        g_object_set(cell, "text", s, NULL);
        break;

        case RAW_PRESET_SIZE:
        s = g_strdup_printf("%u×%u", preset->data.xres, preset->data.yres);
        g_object_set(cell, "text", s, NULL);
        g_free(s);
        break;

        case RAW_PRESET_INFO:
        switch (preset->data.format) {
            case RAW_BINARY:
            g_object_set(cell, "text",
                         gwy_enum_to_string(preset->data.builtin, builtin_menu,
                                            G_N_ELEMENTS(builtin_menu)),
                         NULL);
            break;

            case RAW_TEXT:
            delim = preset->data.delimiter;
            if (!delim || !*delim)
                g_object_set(cell, "text", _("Delimiter: whitespace"), NULL);
            else {
                if (delim[1] == '\0' && !g_ascii_isgraph(delim[0]))
                    s = g_strdup_printf(_("Delimiter: 0x%02x"), delim[0]);
                else
                    s = g_strdup_printf(_("Delimiter: %s"), delim);
                g_object_set(cell, "text", s, NULL);
                g_free(s);
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static GtkWidget*
rawfile_dialog_preset_page(RawFileArgs *args,
                           RawFileControls *controls)
{
    static const GwyEnum columns[] = {
        { N_("Name"), RAW_PRESET_NAME },
        { N_("Type"), RAW_PRESET_TYPE },
        { N_("Size"), RAW_PRESET_SIZE },
        { N_("Info"), RAW_PRESET_INFO },
    };
    GwyInventoryStore *store;
    GtkTreeSelection *tselect;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeIter iter;
    GtkWidget *vbox, *label, *table, *button, *scroll, *bbox;
    guint i, row;

    row = 0;
    vbox = gtk_vbox_new(FALSE, 0);   /* to prevent notebook expanding tables */
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

    label = gtk_label_new_with_mnemonic(_("Preset l_ist"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    store = gwy_inventory_store_new(gwy_raw_file_presets());
    controls->presetlist = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(controls->presetlist), TRUE);
    g_object_unref(store);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls->presetlist);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_(columns[i].name),
                                                          renderer,
                                                          NULL);
        gtk_tree_view_column_set_cell_data_func
                                         (column, renderer,
                                          rawfile_preset_cell_renderer,
                                          GUINT_TO_POINTER(columns[i].value),
                                          NULL);  /* destroy notify */
        gtk_tree_view_append_column(GTK_TREE_VIEW(controls->presetlist),
                                    column);
    }

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(scroll), controls->presetlist);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_START);
    gtk_container_set_border_width(GTK_CONTAINER(bbox), 2);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    button = gtk_button_new_with_mnemonic(_("_Load"));
    controls->load = button;
    gtk_container_add(GTK_CONTAINER(bbox), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(preset_load_cb), controls);

    button = gtk_button_new_with_mnemonic(_("_Store"));
    controls->save = button;
    gtk_container_add(GTK_CONTAINER(bbox), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(preset_store_cb), controls);

    button = gtk_button_new_with_mnemonic(_("_Rename"));
    controls->rename = button;
    gtk_container_add(GTK_CONTAINER(bbox), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(preset_rename_cb), controls);

    button = gtk_button_new_with_mnemonic(_("_Delete"));
    controls->delete = button;
    gtk_container_add(GTK_CONTAINER(bbox), button);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(preset_delete_cb), controls);

    table = gtk_table_new(1, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 2);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    controls->presetname = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(controls->presetname), args->preset->str);
    gwy_table_attach_row(table, row, _("Preset _name:"), "",
                         controls->presetname);
    gtk_entry_set_max_length(GTK_ENTRY(controls->presetname), 40);
    row++;

    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->presetlist));
    gtk_tree_selection_set_mode(tselect, GTK_SELECTION_SINGLE);
    g_signal_connect_swapped(tselect, "changed",
                             G_CALLBACK(preset_selected_cb), controls);
    if (gwy_inventory_store_get_iter(store, args->preset->str, &iter))
        gtk_tree_selection_select_iter(tselect, &iter);
    else {
        gtk_widget_set_sensitive(controls->load, FALSE);
        gtk_widget_set_sensitive(controls->delete, FALSE);
        gtk_widget_set_sensitive(controls->rename, FALSE);
    }

    return vbox;
}

static GwyDataField*
rawfile_read_data_field(GtkWidget *parent,
                        RawFileArgs *args,
                        RawFileFile *file)
{
    GwyDataField *dfield = NULL;
    GError *err = NULL;
    guint reqsize;
    gdouble m;

    reqsize = rawfile_compute_required_size(args);
    if (reqsize > file->filesize) {
        rawfile_warn_too_short_file(parent, file, reqsize);
        return NULL;
    }

    m = pow10(args->p.xyexponent);
    switch (args->p.format) {
        case RAW_BINARY:
        dfield = gwy_data_field_new(args->p.xres, args->p.yres,
                                    m*args->p.xreal, m*args->p.yreal,
                                    FALSE);
        if (args->p.builtin)
            rawfile_read_builtin(args, file->buffer,
                                 gwy_data_field_get_data(dfield));
        else
            rawfile_read_bits(args, file->buffer,
                              gwy_data_field_get_data(dfield));
        break;

        case RAW_TEXT:
        dfield = gwy_data_field_new(args->p.xres, args->p.yres,
                                    m*args->p.xreal, m*args->p.yreal,
                                    FALSE);
        if (!rawfile_read_ascii(args, file->buffer,
                                gwy_data_field_get_data(dfield), &err)) {
            rawfile_warn_parse_error(parent, file, err);
            g_object_unref(dfield);
            g_clear_error(&err);
            return NULL;
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }

    gwy_data_field_multiply(dfield, pow10(args->p.zexponent)*args->p.zscale);
    return dfield;
}

static void
rawfile_warn_too_short_file(GtkWidget *parent,
                            RawFileFile *file,
                            guint32 reqsize)
{
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                    GTK_DIALOG_DESTROY_WITH_PARENT
                                    | GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_OK,
                                    _("Too short file."));
    gtk_message_dialog_format_secondary_text
        (GTK_MESSAGE_DIALOG(dialog),
         _("The format would require %u bytes long file (at least), "
           "but the length of `%s' is only %u bytes."),
         reqsize, file->filename, file->filesize);
    gtk_window_set_modal(GTK_WINDOW(parent), FALSE);  /* Bug #66 workaround. */
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    gtk_window_set_modal(GTK_WINDOW(parent), TRUE);  /* Bug #66 workaround. */
}

static void
rawfile_warn_parse_error(GtkWidget *parent,
                         RawFileFile *file,
                         GError *err)
{
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                    GTK_DIALOG_DESTROY_WITH_PARENT
                                    | GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_OK,
                                    _("Parsing of %s failed."),
                                    file->filename);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                             "%s.", err->message);
    gtk_window_set_modal(GTK_WINDOW(parent), FALSE);  /* Bug #66 workaround. */
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    gtk_window_set_modal(GTK_WINDOW(parent), TRUE);  /* Bug #66 workaround. */
}

static void
builtin_changed_cb(GtkWidget *combo,
                   RawFileControls *controls)
{
    gint builtin;
    GtkAdjustment *adj;

    builtin = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (!controls->in_update)
        update_dialog_values(controls);
    if (builtin) {
        rawfile_sanitize_args(controls->args);

        adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->size));
        adj->upper = 64.0;
        gtk_adjustment_changed(adj);

        adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->skip));
        adj->step_increment = 8.0;
        gtk_adjustment_changed(adj);

        adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->rowskip));
        adj->step_increment = 8.0;
        gtk_adjustment_changed(adj);
    }
    else {
        adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->size));
        adj->upper = 24.0;
        gtk_adjustment_changed(adj);

        adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->skip));
        adj->step_increment = 1.0;
        gtk_adjustment_changed(adj);

        adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->rowskip));
        adj->step_increment = 1.0;
        gtk_adjustment_changed(adj);
    }
    if (!controls->in_update)
        update_dialog_controls(controls);
}

static void
delimiter_changed_cb(GtkWidget *combo,
                     RawFileControls *controls)
{
    gint delim;

    delim = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    gtk_widget_set_sensitive(controls->delimiter, delim == RAW_DELIM_OTHER);
    if (delim != RAW_DELIM_OTHER)
        g_free(controls->args->p.delimiter);
    if (delim == RAW_DELIM_TAB)
        controls->args->p.delimiter = g_strdup("\t");
    else if (delim == RAW_DELIM_ANY_WHITESPACE)
        controls->args->p.delimiter = g_strdup("");
    gtk_entry_set_text(GTK_ENTRY(controls->delimiter),
                       controls->args->p.delimiter);
}

static void
xyres_changed_cb(GtkAdjustment *adj,
                 RawFileControls *controls)
{
    static gboolean in_update = FALSE;
    GtkAdjustment *radj;
    gdouble value;

    value = gtk_adjustment_get_value(adj);
    radj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xres));
    if (radj == adj) {
        /* x */
        controls->args->p.xres = (gint)(value + 0.499);
        radj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yres));
    }
    else {
        /* y */
        controls->args->p.yres = (gint)(value + 0.499);
    }

    if (!in_update && controls->args->xyreseq) {
        in_update = TRUE;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(radj), value);
        in_update = FALSE;
    }

    /* FIXME: this way of synchrnonization may be contrainituitive.
     * but which one *is* intuitive? */
    if (controls->args->xymeasureeq)
        xyreal_changed_cb(gtk_spin_button_get_adjustment(
                              GTK_SPIN_BUTTON(controls->xreal)),
                          controls);
}

static void
xyreseq_changed_cb(RawFileControls *controls)
{
    controls->args->xyreseq
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->xyreseq));
    if (!controls->in_update && controls->args->xyreseq) {
        update_dialog_values(controls);
        update_dialog_controls(controls);
    }
}

static void
xyreal_changed_cb(GtkAdjustment *adj,
                 RawFileControls *controls)
{
    static gboolean in_update = FALSE;
    GtkAdjustment *radj;
    gdouble value;

    value = gtk_adjustment_get_value(adj);
    radj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    if (radj == adj) {
        /* x */
        controls->args->p.xreal = value;
        radj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
        value *= controls->args->p.yres/(gdouble)controls->args->p.xres;
    }
    else {
        /* y */
        controls->args->p.yreal = value;
        value *= controls->args->p.xres/(gdouble)controls->args->p.yres;
    }

    if (!in_update && controls->args->xymeasureeq) {
        in_update = TRUE;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(radj), value);
        in_update = FALSE;
    }
}

static void
xymeasureeq_changed_cb(RawFileControls *controls)
{
    controls->args->xymeasureeq = gtk_toggle_button_get_active
                                    (GTK_TOGGLE_BUTTON(controls->xymeasureeq));
    if (!controls->in_update && controls->args->xymeasureeq) {
        update_dialog_values(controls);
        update_dialog_controls(controls);
    }
}

static void
bintext_changed_cb(G_GNUC_UNUSED GtkWidget *button,
                   RawFileControls *controls)
{
    if (!controls->in_update) {
        update_dialog_values(controls);
        update_dialog_controls(controls);
    }
}

static void
preview_cb(RawFileControls *controls)
{
    GwyDataField *dfield;
    GdkPixbuf *pixbuf, *pixbuf2;
    gint xres, yres;
    gdouble zoom, avg, rms;

    update_dialog_values(controls);
    if (!(dfield = rawfile_read_data_field(controls->dialog,
                                           controls->args,
                                           controls->file)))
        return;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    zoom = 120.0/MAX(xres, yres);
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, xres, yres);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    avg = gwy_data_field_get_avg(dfield);
    rms = gwy_data_field_get_rms(dfield);
    gwy_pixbuf_draw_data_field_with_range(pixbuf, dfield, controls->gradient,
                                          avg - 1.8*rms, avg + 1.8*rms);
    pixbuf2 = gdk_pixbuf_scale_simple(pixbuf,
                                      ceil(xres*zoom), ceil(yres*zoom),
                                      GDK_INTERP_TILES);
    gwy_debug_objects_creation(G_OBJECT(pixbuf2));
    gtk_image_set_from_pixbuf(GTK_IMAGE(controls->preview), pixbuf2);
    g_object_unref(pixbuf2);
    g_object_unref(pixbuf);
    g_object_unref(dfield);
}

static void
preset_selected_cb(RawFileControls *controls)
{
    GwyRawFilePreset *preset;
    GtkTreeModel *store;
    GtkTreeSelection *tselect;
    GtkTreeIter iter;
    const gchar *name;

    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->presetlist));
    g_return_if_fail(tselect);
    if (!gtk_tree_selection_get_selected(tselect, &store, &iter)) {
        g_string_assign(controls->args->preset, "");
        gtk_widget_set_sensitive(controls->load, FALSE);
        gtk_widget_set_sensitive(controls->delete, FALSE);
        gtk_widget_set_sensitive(controls->rename, FALSE);
        gwy_debug("Nothing is selected");
        return;
    }

    gtk_tree_model_get(store, &iter, 0, &preset, -1);
    name = gwy_resource_get_name(GWY_RESOURCE(preset));
    gtk_entry_set_text(GTK_ENTRY(controls->presetname), name);
    g_string_assign(controls->args->preset, name);

    gtk_widget_set_sensitive(controls->load, TRUE);
    gtk_widget_set_sensitive(controls->delete, TRUE);
    gtk_widget_set_sensitive(controls->rename, TRUE);
}

static void
preset_load_cb(RawFileControls *controls)
{
    GwyRawFilePreset *preset;
    RawFileArgs *args;
    GtkTreeModel *store;
    GtkTreeSelection *tselect;
    GtkTreeIter iter;
    gdouble expected_yreal;

    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->presetlist));
    if (!gtk_tree_selection_get_selected(tselect, &store, &iter))
        return;

    gtk_tree_model_get(store, &iter, 0, &preset, -1);
    args = controls->args;
    gwy_raw_file_preset_data_copy(&preset->data, &args->p);
    args->xyreseq = (args->p.xres == args->p.yres);
    expected_yreal = args->p.xreal/args->p.xres*args->p.yres;
    args->xymeasureeq = (fabs(log(expected_yreal/args->p.yreal)) < EPS);
    update_dialog_controls(controls);
}

static void
preset_store_cb(RawFileControls *controls)
{
    GwyRawFilePreset *preset;
    GtkTreeModel *model;
    GtkTreeSelection *tselect;
    GtkTreeIter iter;
    const gchar *name;
    gchar *filename;
    GString *str;
    FILE *fh;

    update_dialog_values(controls);
    name = gtk_entry_get_text(GTK_ENTRY(controls->presetname));
    if (!preset_validate_name(controls, name, TRUE))
        return;
    gwy_debug("Now I'm saving `%s'", name);
    preset = gwy_inventory_get_item(gwy_raw_file_presets(), name);
    if (!preset) {
        gwy_debug("Appending `%s'", name);
        preset = gwy_raw_file_preset_new(name, &controls->args->p, FALSE);
        gwy_inventory_insert_item(gwy_raw_file_presets(), preset);
        g_object_unref(preset);
    }
    else {
        gwy_debug("Setting `%s'", name);
        gwy_raw_file_preset_data_copy(&controls->args->p, &preset->data);
        gwy_resource_data_changed(GWY_RESOURCE(preset));
    }

    filename = gwy_resource_build_filename(GWY_RESOURCE(preset));
    fh = g_fopen(filename, "w");
    if (!fh) {
        g_warning("Cannot save preset: %s", filename);
        g_free(filename);
        return;
    }
    g_free(filename);

    str = gwy_resource_dump(GWY_RESOURCE(preset));
    fwrite(str->str, 1, str->len, fh);
    fclose(fh);
    g_string_free(str, TRUE);

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->presetlist));
    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->presetlist));
    gwy_inventory_store_get_iter(GWY_INVENTORY_STORE(model), name, &iter);
    gtk_tree_selection_select_iter(tselect, &iter);
}

static void
preset_rename_cb(RawFileControls *controls)
{
    GwyRawFilePreset *preset;
    GwyInventory *inventory;
    GtkTreeModel *model;
    GtkTreeSelection *tselect;
    GtkTreeIter iter;
    const gchar *newname, *oldname;
    gchar *oldfilename, *newfilename;

    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->presetlist));
    if (!gtk_tree_selection_get_selected(tselect, &model, &iter))
        return;

    inventory = gwy_raw_file_presets();
    gtk_tree_model_get(model, &iter, 0, &preset, -1);
    oldname = gwy_resource_get_name(GWY_RESOURCE(preset));
    newname = gtk_entry_get_text(GTK_ENTRY(controls->presetname));
    if (gwy_strequal(newname, oldname)
        || !preset_validate_name(controls, newname, TRUE)
        || gwy_inventory_get_item(inventory, newname))
        return;

    gwy_debug("Now I will rename `%s' to `%s'", oldname, newname);

    oldfilename = gwy_resource_build_filename(GWY_RESOURCE(preset));
    gwy_inventory_rename_item(inventory, oldname, newname);
    newfilename = gwy_resource_build_filename(GWY_RESOURCE(preset));
    if (g_rename(oldfilename, newfilename) != 0) {
        g_warning("Cannot rename preset %s to %s", oldfilename, newfilename);
        gwy_inventory_rename_item(inventory, newname, oldname);
    }
    g_free(oldfilename);
    g_free(newfilename);

    gwy_inventory_store_get_iter(GWY_INVENTORY_STORE(model), newname, &iter);
    gtk_tree_selection_select_iter(tselect, &iter);
}

static void
preset_delete_cb(RawFileControls *controls)
{
    GwyRawFilePreset *preset;
    GtkTreeModel *model;
    GtkTreeSelection *tselect;
    GtkTreeIter iter;
    gchar *filename;
    const gchar *name;

    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->presetlist));
    if (!gtk_tree_selection_get_selected(tselect, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter, 0, &preset, -1);
    name = gwy_resource_get_name(GWY_RESOURCE(preset));
    filename = gwy_resource_build_filename(GWY_RESOURCE(preset));
    if (g_remove(filename))
        g_warning("Cannot remove preset %s", filename);
    g_free(filename);
    gwy_inventory_delete_item(gwy_raw_file_presets(), name);
}

static gboolean
preset_validate_name(RawFileControls *controls,
                     const gchar *name,
                     gboolean show_warning)
{
    GtkWidget *dialog, *parent;

    if (*name && !strchr(name, '/'))
        return TRUE;
    if (!show_warning)
        return FALSE;

    parent = controls->dialog;
    dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                    GTK_DIALOG_MODAL
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_CLOSE,
                                    _("The name `%s' is invalid."),
                                    name);
    gtk_window_set_modal(GTK_WINDOW(parent), FALSE);  /* Bug #66 workaround. */
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    gtk_window_set_modal(GTK_WINDOW(parent), TRUE);  /* Bug #66 workaround. */

    return FALSE;
}

static void
update_dialog_controls(RawFileControls *controls)
{
    RawFileArgs *args;
    GtkTreeSelection *tselect;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkAdjustment *adj;
    gchar buf[16];
    RawFileBuiltin builtin;
    int delim;
    gboolean bin;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args = controls->args;

    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xres));
    gtk_adjustment_set_value(adj, args->p.xres);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yres));
    gtk_adjustment_set_value(adj, args->p.yres);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->xyreseq),
                                 args->xyreseq);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    gtk_adjustment_set_value(adj, args->p.xreal);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
    gtk_adjustment_set_value(adj, args->p.yreal);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->xymeasureeq),
                                 args->xymeasureeq);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->xyexponent),
                                  args->p.xyexponent);

    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->zscale));
    gtk_adjustment_set_value(adj, args->p.zscale);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->zexponent),
                                  args->p.zexponent);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->takeover),
                                 args->takeover);

    gwy_radio_buttons_set_current(controls->format, "format", args->p.format);

    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->offset));
    gtk_adjustment_set_value(adj, args->p.offset);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->size));
    gtk_adjustment_set_value(adj, args->p.size);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->skip));
    gtk_adjustment_set_value(adj, args->p.skip);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->rowskip));
    gtk_adjustment_set_value(adj, args->p.rowskip);

    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->lineoffset));
    gtk_adjustment_set_value(adj, args->p.lineoffset);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->skipfields));
    gtk_adjustment_set_value(adj, args->p.skipfields);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->decomma),
                                 args->p.decomma);

    gtk_entry_set_text(GTK_ENTRY(controls->delimiter), args->p.delimiter);
    if (!args->p.delimiter || !*args->p.delimiter)
        delim = RAW_DELIM_ANY_WHITESPACE;
    else if (args->p.delimiter[0] == '\t' && args->p.delimiter[1] == '\0')
        delim = RAW_DELIM_TAB;
    else
        delim = RAW_DELIM_OTHER;
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->delimmenu), delim);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->sign),
                                 args->p.sign);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->revbyte),
                                 args->p.revbyte);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->revsample),
                                 args->p.revsample);

    g_snprintf(buf, sizeof(buf), "%u", args->p.byteswap);
    gtk_entry_set_text(GTK_ENTRY(controls->byteswap), buf);

    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->builtin),
                                  args->p.builtin);

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(controls->presetlist));
    tselect = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->presetlist));
    if (gwy_inventory_store_get_iter(GWY_INVENTORY_STORE(model),
                                     args->preset->str, &iter))
        gtk_tree_selection_select_iter(tselect, &iter);

    bin = (args->p.format == RAW_BINARY);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->offset), bin);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->skip), bin);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->rowskip), bin);
    gtk_widget_set_sensitive(controls->builtin, bin);
    gtk_widget_set_sensitive(controls->revbyte, bin);
    gtk_widget_set_sensitive(controls->byteswap, bin);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->lineoffset), !bin);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->skipfields), !bin);
    gtk_widget_set_sensitive(controls->delimmenu, !bin);
    gtk_widget_set_sensitive(controls->delim_label, !bin);
    gtk_widget_set_sensitive(controls->decomma, !bin);

    builtin = args->p.builtin;
    switch (args->p.format) {
        case RAW_BINARY:
        gtk_widget_set_sensitive(controls->delimiter, FALSE);
        gtk_widget_set_sensitive(controls->otherdelim_label, FALSE);
        gtk_widget_set_sensitive(controls->byteswap,
                                 builtin
                                 && builtin != RAW_UNSIGNED_BYTE
                                 && builtin != RAW_SIGNED_BYTE);
        gtk_widget_set_sensitive(controls->byteswap_label,
                                 builtin
                                 && builtin != RAW_UNSIGNED_BYTE
                                 && builtin != RAW_SIGNED_BYTE);
        gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->size), !builtin);
        gtk_widget_set_sensitive(controls->sign, !builtin);
        gtk_widget_set_sensitive(controls->revsample, !builtin);
        break;

        case RAW_TEXT:
        gtk_widget_set_sensitive(controls->delimiter, delim == RAW_DELIM_OTHER);
        gtk_widget_set_sensitive(controls->otherdelim_label,
                                 delim == RAW_DELIM_OTHER);
        gtk_widget_set_sensitive(controls->byteswap, FALSE);
        gtk_widget_set_sensitive(controls->byteswap_label, FALSE);
        gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->size), FALSE);
        gtk_widget_set_sensitive(controls->sign, FALSE);
        gtk_widget_set_sensitive(controls->revsample, FALSE);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    controls->in_update = FALSE;
}

static void
update_dialog_values(RawFileControls *controls)
{
    RawFileArgs *args;

    gwy_debug("controls %p", controls);
    args = controls->args;
    g_free(args->p.delimiter);

    args->p.xres
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->xres));
    args->p.yres
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->yres));
    args->xyreseq
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->xyreseq));
    args->p.xreal
        = gtk_spin_button_get_value(GTK_SPIN_BUTTON(controls->xreal));
    args->p.yreal
        = gtk_spin_button_get_value(GTK_SPIN_BUTTON(controls->yreal));
    args->xymeasureeq
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->xymeasureeq));
    args->p.xyexponent
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->xyexponent));

    args->p.zscale
        = gtk_spin_button_get_value(GTK_SPIN_BUTTON(controls->zscale));
    args->p.zexponent
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->zexponent));

    args->p.offset
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->offset));
    args->p.size
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->size));
    args->p.skip
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->skip));
    args->p.rowskip
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->rowskip));

    args->p.delimiter
        = g_strdup(gtk_entry_get_text(GTK_ENTRY(controls->delimiter)));
    args->p.lineoffset
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->lineoffset));
    args->p.skipfields
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->skipfields));
    args->p.decomma
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->decomma));

    args->p.sign
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->sign));
    args->p.revbyte
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->revbyte));
    args->p.revsample
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->revsample));

    args->p.byteswap = atoi(gtk_entry_get_text(GTK_ENTRY(controls->byteswap)));
    gwy_debug("byteswap = %u", args->p.byteswap);

    args->p.builtin
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->builtin));
    args->p.format = gwy_radio_buttons_get_current(controls->format, "format");

    args->takeover
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->takeover));

    rawfile_sanitize_args(args);
}

static GtkWidget*
table_attach_heading(GtkWidget *table,
                     const gchar *text,
                     gint row)
{
    GtkWidget *label;
    gchar *s;

    s = g_strconcat("<b>", text, "</b>", NULL);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), s);
    g_free(s);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 3, row, row+1);

    return label;
}

static inline guint32
reverse_bits(guint32 x, guint n)
{
    gulong y = 0;

    while (n--) {
        y <<= 1;
        y |= x&1;
        x >>= 1;
    }
    return y;
}

/* XXX: the max size this can handle is 24 bits */
static void
rawfile_read_bits(RawFileArgs *args,
                  guchar *buffer,
                  gdouble *data)
{
    guchar *rtable = NULL;
    guchar *rtable8 = NULL;
    guint i, j, nb;
    guint32 b, bucket, x, rem;

    g_assert(args->p.size <= 24);
    g_assert(args->p.size > 1 || !args->p.sign);

    if (args->p.revsample && args->p.size <= 8) {
        rtable = g_new(guchar, 1 << args->p.size);
        for (i = 0; i < 1 << args->p.size; i++)
            rtable[i] = reverse_bits(i, args->p.size);
    }
    if (args->p.revbyte) {
        rtable8 = g_new(guchar, 1 << 8);
        for (i = 0; i < 1 << 8; i++)
            rtable8[i] = reverse_bits(i, 8);
    }

    buffer += args->p.offset;
    nb = 0;
    bucket = 0;

    for (i = args->p.yres; i; i--) {
        for (j = args->p.xres; j; j--) {
            /* gather enough bits, new bits are put to the least significant
             * position */
            while (nb < args->p.size) {
                b = *(buffer++);
                if (args->p.revbyte)
                    b = rtable8[b];
                bucket <<= 8;
                bucket |= b;
                nb += 8;
            }
            /* we have this many too much bits now (in the least significat
             * part of bucket) */
            rem = nb - args->p.size;
            /* x is the data sample (in the most significat part of  bucket) */
            x = bucket >> rem;
            if (args->p.revsample) {
                if (rtable)
                    x = rtable[x];
                else
                    x = reverse_bits(x, args->p.size);
            }
            /* rem bits remains in bucket */
            bucket &= BITMASK[rem];
            nb = rem;

            /* sign-extend to 32bit signed number if signed */
            if (args->p.sign) {
                if (x & BITMASK[args->p.size-1])
                    x |= ~BITMASK[args->p.size];
                *(data++) = (gdouble)(gint32)x;
            }
            else
                *(data++) = (gdouble)x;

            /* skip args->p.skip bits, only the last byte is important */
            if (nb < args->p.skip) {
                /* skip what we have in the bucket */
                rem = args->p.skip - nb;
                /* whole bytes */
                buffer += rem/8;
                rem %= 8;  /* remains to skip */
                nb = 8 - rem;  /* so this number of bits will be in bucket */
                b = *(buffer++);
                if (args->p.revbyte)
                    b = rtable8[b];
                bucket = b & BITMASK[nb];
            }
            else {
                /* we have enough bits in bucket, so just get rid of the
                 * extra ones */
                nb -= args->p.skip;
                bucket &= BITMASK[nb];
            }
        }
        /* skip args->p.rowskip bits, only the last byte is important */
        if (nb < args->p.rowskip) {
            /* skip what we have in the bucket */
            rem = args->p.rowskip - nb;
            /* whole bytes */
            buffer += rem/8;
            rem %= 8;  /* remains to skip */
            nb = 8 - rem;  /* so this number of bits will be in bucket */
            b = *(buffer++);
            if (args->p.revbyte)
                b = rtable8[b];
            bucket = b & BITMASK[nb];
        }
        else {
            /* we have enough bits in bucket, so just get rid of the
             * extra ones */
            nb -= args->p.rowskip;
            bucket &= BITMASK[nb];
        }
    }
    g_free(rtable8);
    g_free(rtable);
}

static void
rawfile_read_builtin(RawFileArgs *args,
                     guchar *buffer,
                     gdouble *data)
{
    guchar *rtable8 = NULL;
    guint i, j, k, size, skip, rowskip;
    double good_alignment;
    guchar *b;

    g_assert(args->p.builtin > RAW_NONE && args->p.builtin < RAW_LAST);
    g_assert(args->p.size <= 64 && args->p.size % 8 == 0);
    g_assert(args->p.skip % 8 == 0);
    g_assert(args->p.rowskip % 8 == 0);

    if (args->p.revbyte) {
        rtable8 = g_new(guchar, 1 << 8);
        for (i = 0; i < 1 << 8; i++)
            rtable8[i] = reverse_bits(i, 8);
    }

    buffer += args->p.offset;
    size = args->p.size/8;
    skip = args->p.skip/8;
    rowskip = args->p.rowskip/8;
    b = (guchar*)&good_alignment;
    memset(b, 0, 8);

    for (i = args->p.yres; i; i--) {
        for (j = args->p.xres; j; j--) {
            /* the XOR magic puts each byte where it belongs according to
             * byteswap */
            if (args->p.revbyte) {
                for (k = 0; k < size; k++)
                    b[k ^ args->p.byteswap] = rtable8[*(buffer++)];
            }
            else {
                for (k = 0; k < size; k++)
                    b[k ^ args->p.byteswap] = *(buffer++);
            }
            /* now interpret b as a number in HOST order */
            switch (args->p.builtin) {
                case RAW_SIGNED_BYTE:
                *(data++) = (gdouble)(gchar)b[0];
                break;

                case RAW_UNSIGNED_BYTE:
                *(data++) = (gdouble)b[0];
                break;

                case RAW_SIGNED_WORD16:
                *(data++) = (gdouble)*(gint16*)b;
                break;

                case RAW_UNSIGNED_WORD16:
                *(data++) = (gdouble)*(guint16*)b;
                break;

                case RAW_SIGNED_WORD32:
                *(data++) = (gdouble)*(gint32*)b;
                break;

                case RAW_UNSIGNED_WORD32:
                *(data++) = (gdouble)*(guint32*)b;
                break;

                case RAW_IEEE_FLOAT:
                *(data++) = *(float*)b;
                break;

                case RAW_IEEE_DOUBLE:
                *(data++) = *(double*)b;
                break;

                default:
                g_assert_not_reached();
                break;
            }
            buffer += skip;
        }
        buffer += rowskip;
    }
    g_free(rtable8);
}

static gboolean
rawfile_read_ascii(RawFileArgs *args,
                   guchar *buffer,
                   gdouble *data,
                   GError **error)
{
    RawStrtodFunc strtod_func;
    guint i, j, n;
    gint cdelim = '\0';
    gint delimtype;
    gdouble x;
    guchar *end;

    if (!error_domain)
        error_domain = g_quark_from_static_string("RAWFILE_ERROR");

    if (args->p.decomma)
        strtod_func = &gwy_comma_strtod;
    else
        strtod_func = &g_ascii_strtod;

    /* skip lines */
    for (i = 0; i < args->p.lineoffset; i++) {
        buffer = strchr(buffer, '\n');
        if (!buffer) {
            g_set_error(error, error_domain, RAW_ASCII_PARSE_ERROR,
                        _("Not enough lines (%d) for offset (%d)"),
                        i, args->p.lineoffset);
            return FALSE;
        }
        buffer++;
    }

    if (!args->p.delimiter)
        delimtype = 0;
    else {
        delimtype = strlen(args->p.delimiter);
        cdelim = args->p.delimiter[0];
    }

    for (n = 0; n < args->p.yres; n++) {
        /* skip fields */
        switch (delimtype) {
            case 0:
            buffer += strspn(buffer, " \t\n\r");
            for (i = 0; i < args->p.skipfields; i++) {
                j = strcspn(buffer, " \t\n\r");
                buffer += j;
                j = strspn(buffer, " \t\n\r");
                if (!j) {
                    g_set_error(error, error_domain, RAW_ASCII_PARSE_ERROR,
                                _("Expected whitespace to skip more fields "
                                  "in row %u, got `%.16s'"),
                                n, buffer);
                    return FALSE;
                }
            }
            break;

            case 1:
            for (i = 0; i < args->p.skipfields; i++) {
                end = strchr(buffer, cdelim);
                if (!end) {
                    g_set_error(error, error_domain, RAW_ASCII_PARSE_ERROR,
                                _("Expected `%c' to skip more fields "
                                  "in row %u, got `%.16s'"),
                                cdelim, n, buffer);
                    return FALSE;
                }
                buffer = end + 1;
            }
            break;

            default:
            for (i = 0; i < args->p.skipfields; i++) {
                end = strstr(buffer, args->p.delimiter);
                if (!end) {
                    g_set_error(error, error_domain, RAW_ASCII_PARSE_ERROR,
                                _("Expected `%s' to skip more fields "
                                  "in row %u, got `%.16s'"),
                                args->p.delimiter, n, buffer);
                    return FALSE;
                }
                buffer = end + delimtype;
            }
            break;
        }

        /* read data */
        switch (delimtype) {
            case 0:
            for (i = 0; i < args->p.xres; i++) {
                x = strtod_func(buffer, (char**)&end);
                if (end == buffer) {
                    g_set_error(error, error_domain, RAW_ASCII_PARSE_ERROR,
                                _("Garbage `%.16s' in row %u, column %u"),
                                buffer, n, i);
                    return FALSE;
                }
                buffer = end;
                *(data++) = x;
            }
            break;

            case 1:
            for (i = 0; i < args->p.xres; i++) {
                x = strtod_func(buffer, (char**)&end);
                if (end == buffer) {
                    g_set_error(error, error_domain, RAW_ASCII_PARSE_ERROR,
                                _("Garbage `%.16s' in row %u, column %u"),
                                buffer, n, i);
                    return FALSE;
                }
                buffer = end + strspn(end, " \t");
                if (*buffer == cdelim)
                    buffer++;
                else if (i + 1 == args->p.xres
                         && (j = strspn(buffer, "\n\r")))
                    buffer += j;
                else {
                    g_set_error(error, error_domain, RAW_ASCII_PARSE_ERROR,
                                _("Expected delimiter `%c' after data "
                                  "in row %u, column %u, got `%c'"),
                                cdelim, n, i, *buffer);
                    return FALSE;
                }
                *(data++) = x;
            }
            break;

            default:
            for (i = 0; i < args->p.xres; i++) {
                x = strtod_func(buffer, (char**)&end);
                if (end == buffer) {
                    g_set_error(error, error_domain, RAW_ASCII_PARSE_ERROR,
                                _("Garbage `%.16s' in row %u, column %u"),
                                buffer, n, i);
                    return FALSE;
                }
                buffer = end + strspn(end, " \t");
                if (strncmp(buffer, args->p.delimiter, delimtype) == 0)
                    buffer += delimtype;
                else if (i + 1 == args->p.xres
                         && (j = strspn(buffer, "\n\r")))
                    buffer += j;
                else {
                    g_set_error(error, error_domain, RAW_ASCII_PARSE_ERROR,
                                _("Expected delimiter `%s' after data "
                                  "in row %u, column %u, got `%.16s'"),
                                args->p.delimiter, n, i, buffer);
                    return FALSE;
                }
                *(data++) = x;
            }
            break;
        }
    }

    return TRUE;
}

static gdouble
gwy_comma_strtod(const gchar *nptr, gchar **endptr)
{
    gchar *fail_pos;
    gdouble val;
    struct lconv *locale_data;
    const char *decimal_point;
    int decimal_point_len;
    const char *p, *decimal_point_pos;
    const char *end = NULL;     /* Silence gcc */

    g_return_val_if_fail(nptr != NULL, 0);

    fail_pos = NULL;

    locale_data = localeconv();
    decimal_point = locale_data->decimal_point;
    decimal_point_len = strlen(decimal_point);

    g_assert(decimal_point_len != 0);

    decimal_point_pos = NULL;
    if (decimal_point[0] != ',' || decimal_point[1] != 0) {
        p = nptr;
        /* Skip leading space */
        while (g_ascii_isspace(*p))
            p++;

        /* Skip leading optional sign */
        if (*p == '+' || *p == '-')
            p++;

        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
            /* HEX - find the (optional) decimal point */

            while (g_ascii_isxdigit(*p))
                p++;

            if (*p == ',') {
                decimal_point_pos = p++;

                while (g_ascii_isxdigit(*p))
                    p++;

                if (*p == 'p' || *p == 'P')
                    p++;
                if (*p == '+' || *p == '-')
                    p++;
                while (g_ascii_isdigit(*p))
                    p++;
            }
        }
        else {
            while (g_ascii_isdigit(*p))
                p++;

            if (*p == ',') {
                decimal_point_pos = p++;

                while (g_ascii_isdigit(*p))
                    p++;

                if (*p == 'e' || *p == 'E')
                    p++;
                if (*p == '+' || *p == '-')
                    p++;
                while (g_ascii_isdigit(*p))
                    p++;
            }
        }
        /* For the other cases, we need not convert the decimal point */
        end = p;
    }

    /* Set errno to zero, so that we can distinguish zero results
       and underflows */
    errno = 0;

    if (decimal_point_pos) {
        char *copy, *c;

        /* We need to convert the ',' to the locale specific decimal point */
        copy = g_malloc(end - nptr + 1 + decimal_point_len);

        c = copy;
        memcpy(c, nptr, decimal_point_pos - nptr);
        c += decimal_point_pos - nptr;
        memcpy(c, decimal_point, decimal_point_len);
        c += decimal_point_len;
        memcpy(c, decimal_point_pos + 1, end - (decimal_point_pos + 1));
        c += end - (decimal_point_pos + 1);
        *c = 0;

        val = strtod(copy, &fail_pos);

        if (fail_pos) {
            if (fail_pos - copy > decimal_point_pos - nptr)
                fail_pos =
                    (char *)nptr + (fail_pos - copy) - (decimal_point_len - 1);
            else
                fail_pos = (char *)nptr + (fail_pos - copy);
        }

        g_free(copy);

    }
    else if (decimal_point[0] != ',' || decimal_point[1] != 0) {
        char *copy;

        copy = g_malloc(end - (char *)nptr + 1);
        memcpy(copy, nptr, end - nptr);
        *(copy + (end - (char *)nptr)) = 0;

        val = strtod(copy, &fail_pos);

        if (fail_pos) {
            fail_pos = (char *)nptr + (fail_pos - copy);
        }

        g_free(copy);
    }
    else {
        val = strtod(nptr, &fail_pos);
    }

    if (endptr)
        *endptr = fail_pos;

    return val;
}

static void
rawfile_sanitize_args(RawFileArgs *args)
{
    args->takeover = !!args->takeover;
    gwy_raw_file_preset_data_sanitize(&args->p);
    if (args->xyreseq)
        args->p.yres = args->p.xres;
    if (args->xymeasureeq)
        args->p.yreal = args->p.xreal/args->p.xres*args->p.yres;
}

static guint
rawfile_compute_required_size(RawFileArgs *args)
{
    guint rowstride;

    switch (args->p.format) {
        case RAW_BINARY:
        rowstride = (args->p.size + args->p.skip)*args->p.xres
                    + args->p.rowskip;
        if (args->p.builtin && rowstride%8) {
            g_warning("rowstride is not a whole number of bytes");
            rowstride = ((rowstride + 7)/8)*8;
        }
        return args->p.offset + args->p.yres*rowstride/8;
        break;

        case RAW_TEXT:
        rowstride = (args->p.xres + args->p.skipfields)
                     *(1 + MAX(strlen(args->p.delimiter), 1));
        return args->p.lineoffset + args->p.yres*rowstride;
        break;
    }

    g_assert_not_reached();
    return 0;
}

static void
rawfile_load_args(GwyContainer *settings,
                  RawFileArgs *args)
{
    GwyRawFilePresetData *data = &args->p;
    const guchar *s = NULL;

    args->preset = g_string_new("");
    args->takeover = takeover_default;
    args->xyreseq = xyreseq_default;
    args->xymeasureeq = xymeasureeq_default;
    memset(&args->p, 0, sizeof(GwyRawFilePresetData));
    gwy_raw_file_preset_data_copy(&rawfilepresetdata_default, data);

    gwy_container_gis_boolean_by_name(settings, takeover_key, &args->takeover);
    gwy_container_gis_string_by_name(settings, preset_key, &s);
    if (s)
        g_string_assign(args->preset, s);
    gwy_container_gis_boolean_by_name(settings, xyreseq_key, &args->xyreseq);
    gwy_container_gis_boolean_by_name(settings, xymeasureeq_key,
                                      &args->xymeasureeq);

    gwy_container_gis_enum_by_name(settings, format_key, &data->format);

    /* Information */
    gwy_container_gis_int32_by_name(settings, xres_key, &data->xres);
    gwy_container_gis_int32_by_name(settings, yres_key, &data->yres);
    gwy_container_gis_int32_by_name(settings, xyexponent_key,
                                    &data->xyexponent);
    gwy_container_gis_int32_by_name(settings, zexponent_key, &data->zexponent);
    gwy_container_gis_double_by_name(settings, xreal_key, &data->xreal);
    gwy_container_gis_double_by_name(settings, yreal_key, &data->yreal);
    gwy_container_gis_double_by_name(settings, zscale_key, &data->zscale);
    gwy_container_gis_string_by_name(settings, xyunit_key,
                                     (const guchar**)&data->xyunit);
    gwy_container_gis_string_by_name(settings, zunit_key,
                                     (const guchar**)&data->zunit);

    data->xyunit = g_strdup(data->xyunit);
    data->zunit = g_strdup(data->zunit);

    /* Binary */
    gwy_container_gis_enum_by_name(settings, builtin_key, &data->builtin);
    gwy_container_gis_int32_by_name(settings, offset_key, &data->offset);
    gwy_container_gis_int32_by_name(settings, size_key, &data->size);
    gwy_container_gis_int32_by_name(settings, skip_key, &data->skip);
    gwy_container_gis_int32_by_name(settings, rowskip_key, &data->rowskip);
    gwy_container_gis_int32_by_name(settings, byteswap_key, &data->byteswap);
    gwy_container_gis_boolean_by_name(settings, sign_key, &data->sign);
    gwy_container_gis_boolean_by_name(settings, revsample_key,
                                      &data->revsample);
    gwy_container_gis_boolean_by_name(settings, revbyte_key, &data->revbyte);

    /* Text */
    gwy_container_gis_boolean_by_name(settings, decomma_key, &data->decomma);
    gwy_container_gis_int32_by_name(settings, lineoffset_key,
                                    &data->lineoffset);
    gwy_container_gis_int32_by_name(settings, skipfields_key,
                                    &data->skipfields);
    gwy_container_gis_string_by_name(settings, delimiter_key,
                                    (const guchar**) &data->delimiter);

    data->delimiter = g_strdup(data->delimiter);

    rawfile_sanitize_args(args);
}

static void
rawfile_save_args(GwyContainer *settings,
                  const RawFileArgs *args)
{
    const GwyRawFilePresetData *data = &args->p;

    gwy_container_set_boolean_by_name(settings, takeover_key, args->takeover);
    gwy_container_set_boolean_by_name(settings, xyreseq_key, args->xyreseq);
    gwy_container_set_boolean_by_name(settings, xymeasureeq_key,
                                      args->xymeasureeq);
    gwy_container_set_string_by_name(settings, preset_key,
                                     g_strdup(args->preset->str));

    gwy_container_set_enum_by_name(settings, format_key, data->format);

    /* Information */
    gwy_container_set_int32_by_name(settings, xres_key, data->xres);
    gwy_container_set_int32_by_name(settings, yres_key, data->yres);
    gwy_container_set_int32_by_name(settings, xyexponent_key, data->xyexponent);
    gwy_container_set_int32_by_name(settings, zexponent_key, data->zexponent);
    gwy_container_set_double_by_name(settings, xreal_key, data->xreal);
    gwy_container_set_double_by_name(settings, yreal_key, data->yreal);
    gwy_container_set_double_by_name(settings, zscale_key, data->zscale);
    gwy_container_set_string_by_name(settings, xyunit_key,
                                     g_strdup(data->xyunit));
    gwy_container_set_string_by_name(settings, zunit_key,
                                     g_strdup(data->zunit));

    /* Binary */
    gwy_container_set_enum_by_name(settings, builtin_key, data->builtin);
    gwy_container_set_int32_by_name(settings, offset_key, data->offset);
    gwy_container_set_int32_by_name(settings, size_key, data->size);
    gwy_container_set_int32_by_name(settings, skip_key, data->skip);
    gwy_container_set_int32_by_name(settings, rowskip_key, data->rowskip);
    gwy_container_set_int32_by_name(settings, byteswap_key, data->byteswap);
    gwy_container_set_boolean_by_name(settings, sign_key, data->sign);
    gwy_container_set_boolean_by_name(settings, revsample_key, data->revsample);
    gwy_container_set_boolean_by_name(settings, revbyte_key, data->revbyte);

    /* Text */
    gwy_container_set_boolean_by_name(settings, decomma_key, data->decomma);
    gwy_container_set_int32_by_name(settings, lineoffset_key, data->lineoffset);
    gwy_container_set_int32_by_name(settings, skipfields_key, data->skipfields);
    gwy_container_set_string_by_name(settings, delimiter_key,
                                     g_strdup(data->delimiter));
}

static void
rawfile_import_1x_presets(GwyContainer *settings)
{
    GwyResourceClass *rklass;
    GwyRawFilePreset *preset;
    GwyInventory *inventory;
    GwyContainer *container;
    RawFileArgs args;
    const guchar *presets = NULL;
    GString *from, *str;
    gchar **preset_list;
    gchar *filename;
    FILE *fh;
    guint i;

    rklass = g_type_class_ref(GWY_TYPE_RAW_FILE_PRESET);
    gwy_resource_class_mkdir(rklass);
    g_type_class_unref(rklass);

    gwy_container_gis_string_by_name(settings, "/module/rawfile/presets",
                                     &presets);
    if (!presets)
        return;

    inventory = gwy_raw_file_presets();
    preset_list = g_strsplit(presets, "\n", 0);
    from = g_string_new("");
    for (i = 0; preset_list[i]; i++) {
        if (gwy_inventory_get_item(inventory, preset_list[i])) {
            g_warning("Preset `%s' already exists, cannot import from 1.x.",
                      preset_list[i]);
            continue;
        }
        gwy_debug("Importing `%s' from 1.x", preset_list[i]);
        g_string_assign(from, "/module/rawfile/preset/");
        g_string_append(from, preset_list[i]);

        /* This is inefficient, but we do not care, it will be run only once. */
        container = gwy_container_new();
        gwy_container_transfer(settings, container,
                               from->str, "/module/rawfile", TRUE);
        memset(&args, 0, sizeof(RawFileArgs));
        rawfile_load_args(container, &args);
        preset = gwy_raw_file_preset_new(preset_list[i], &args.p, FALSE);
        gwy_inventory_insert_item(inventory, preset);

        g_object_unref(preset);
        g_object_unref(container);
        g_string_free(args.preset, TRUE);
        g_free(args.p.delimiter);
        g_free(args.p.xyunit);
        g_free(args.p.zunit);

        filename = gwy_resource_build_filename(GWY_RESOURCE(preset));
        fh = g_fopen(filename, "w");
        if (!fh) {
            g_warning("Cannot save preset: %s", filename);
            g_free(filename);
            continue;
        }

        str = gwy_resource_dump(GWY_RESOURCE(preset));
        fwrite(str->str, 1, str->len, fh);
        fclose(fh);
        g_string_free(str, TRUE);
    }
    g_string_free(from, TRUE);
    g_strfreev(preset_list);

    gwy_container_remove_by_prefix(settings, "/module/rawfile/preset");
    gwy_container_remove_by_name(settings, "/module/rawfile/presets");
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
