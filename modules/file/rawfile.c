/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* define GENRTABLE to create RTABLE generator */
#ifndef GENRTABLE
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/app.h>
#include <app/settings.h>

#ifndef G_OS_WIN32
#include <unistd.h>
#endif

typedef enum {
    RAW_NONE = 0,
    RAW_SIGNED_BYTE,
    RAW_UNSIGNED_BYTE,
    RAW_SIGNED_WORD16,
    RAW_UNSIGNED_WORD16,
    RAW_SIGNED_WORD32,
    RAW_UNSIGNED_WORD32,
    RAW_IEEE_FLOAT,
    RAW_IEEE_DOUBLE,
    RAW_LAST
} RawFileBuiltin;

enum {
    RAW_BINARY,
    RAW_TEXT
};

enum {
    RESPONSE_RESET
};

/* note: size, skip, and rowskip are in bits */
typedef struct {
    gint format;  /* binary, text */
    RawFileBuiltin builtin;
    gsize offset;  /* offset from file start, in bytes */
    gsize size;  /* data sample size (auto if builtin) */
    gsize skip;  /* skip after each sample (multiple of 8 if builtin) */
    gsize rowskip;  /* extra skip after each sample row (multiple of 8 if
                       builtin) */
    gboolean sign;  /* take the number as signed? (unused if not integer) */
    gboolean revsample;  /* reverse bit order in samples? */
    gboolean revbyte;  /* reverse bit order in bytes as we read them? */
    gsize byteswap;  /* swap bytes (relative to HOST order), bit set means
                        swap blocks of this size (only for builtin) */
    gsize lineoffset;  /* start reading from this line (ASCII) */
    gchar *delimiter;  /* field delimiter (ASCII) */
    gsize skipfields;  /* skip this number of fields at line start (ASCII) */

    const gchar *filename;
    gsize filesize;
    gsize xres;
    gsize yres;
    gdouble xreal;
    gdouble yreal;
    gdouble zscale;
} RawFileArgs;

typedef struct {
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
    GtkWidget *xres;
    GtkWidget *yres;
    GtkWidget *xreal;
    GtkWidget *yreal;
    GtkWidget *zscale;
    RawFileArgs *args;
} RawFileControls;

static gboolean      module_register               (const gchar *name);
static gint          rawfile_detect                (const gchar *filename,
                                                    gboolean only_name);
static GwyContainer* rawfile_load                  (const gchar *filename);
static gboolean      rawfile_dialog                (RawFileArgs *args);
static void          rawfile_warn_too_short_file   (GtkWidget *parent,
                                                    RawFileArgs *args,
                                                    gsize reqsize);
static void          builtin_changed_cb            (GtkWidget *item,
                                                    RawFileControls *controls);
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
static void          rawfile_santinize_args        (RawFileArgs *args);
static void          rawfile_load_args             (GwyContainer *settings,
                                                    RawFileArgs *args);
static void          rawfile_save_args             (GwyContainer *settings,
                                                    RawFileArgs *args);
static gsize         rawfile_compute_required_size (RawFileArgs *args);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "rawfile",
    "Read raw data according to user-specified format.",
    "Yeti <yeti@physics.muni.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* sizes of RawFile built-in types */
static const gsize BUILTIN_SIZE[] = {
    0, 8, 8, 16, 16, 32, 32, 32, 64,
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

/* precomputed reverted bitorders up to 8 bits */
static const guint32 RTABLE_0[] = {
    0x0, 
};

static const guint32 RTABLE_1[] = {
    0x0, 0x1, 
};

static const guint32 RTABLE_2[] = {
    0x0, 0x2, 0x1, 0x3, 
};

static const guint32 RTABLE_3[] = {
    0x0, 0x4, 0x2, 0x6, 0x1, 0x5, 0x3, 0x7, 
};

static const guint32 RTABLE_4[] = {
    0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe, 
    0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf, 
};

static const guint32 RTABLE_5[] = {
    0x00, 0x10, 0x08, 0x18, 0x04, 0x14, 0x0c, 0x1c, 
    0x02, 0x12, 0x0a, 0x1a, 0x06, 0x16, 0x0e, 0x1e, 
    0x01, 0x11, 0x09, 0x19, 0x05, 0x15, 0x0d, 0x1d, 
    0x03, 0x13, 0x0b, 0x1b, 0x07, 0x17, 0x0f, 0x1f, 
};

static const guint32 RTABLE_6[] = {
    0x00, 0x20, 0x10, 0x30, 0x08, 0x28, 0x18, 0x38, 
    0x04, 0x24, 0x14, 0x34, 0x0c, 0x2c, 0x1c, 0x3c, 
    0x02, 0x22, 0x12, 0x32, 0x0a, 0x2a, 0x1a, 0x3a, 
    0x06, 0x26, 0x16, 0x36, 0x0e, 0x2e, 0x1e, 0x3e, 
    0x01, 0x21, 0x11, 0x31, 0x09, 0x29, 0x19, 0x39, 
    0x05, 0x25, 0x15, 0x35, 0x0d, 0x2d, 0x1d, 0x3d, 
    0x03, 0x23, 0x13, 0x33, 0x0b, 0x2b, 0x1b, 0x3b, 
    0x07, 0x27, 0x17, 0x37, 0x0f, 0x2f, 0x1f, 0x3f, 
};

static const guint32 RTABLE_7[] = {
    0x00, 0x40, 0x20, 0x60, 0x10, 0x50, 0x30, 0x70, 
    0x08, 0x48, 0x28, 0x68, 0x18, 0x58, 0x38, 0x78, 
    0x04, 0x44, 0x24, 0x64, 0x14, 0x54, 0x34, 0x74, 
    0x0c, 0x4c, 0x2c, 0x6c, 0x1c, 0x5c, 0x3c, 0x7c, 
    0x02, 0x42, 0x22, 0x62, 0x12, 0x52, 0x32, 0x72, 
    0x0a, 0x4a, 0x2a, 0x6a, 0x1a, 0x5a, 0x3a, 0x7a, 
    0x06, 0x46, 0x26, 0x66, 0x16, 0x56, 0x36, 0x76, 
    0x0e, 0x4e, 0x2e, 0x6e, 0x1e, 0x5e, 0x3e, 0x7e, 
    0x01, 0x41, 0x21, 0x61, 0x11, 0x51, 0x31, 0x71, 
    0x09, 0x49, 0x29, 0x69, 0x19, 0x59, 0x39, 0x79, 
    0x05, 0x45, 0x25, 0x65, 0x15, 0x55, 0x35, 0x75, 
    0x0d, 0x4d, 0x2d, 0x6d, 0x1d, 0x5d, 0x3d, 0x7d, 
    0x03, 0x43, 0x23, 0x63, 0x13, 0x53, 0x33, 0x73, 
    0x0b, 0x4b, 0x2b, 0x6b, 0x1b, 0x5b, 0x3b, 0x7b, 
    0x07, 0x47, 0x27, 0x67, 0x17, 0x57, 0x37, 0x77, 
    0x0f, 0x4f, 0x2f, 0x6f, 0x1f, 0x5f, 0x3f, 0x7f, 
};

static const guint32 RTABLE_8[] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0, 
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4, 
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc, 
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa, 
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6, 
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1, 
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9, 
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd, 
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3, 
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7, 
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff, 
};

static const guint32 *const RTABLE[] = {
    RTABLE_0,
    RTABLE_1,
    RTABLE_2,
    RTABLE_3,
    RTABLE_4,
    RTABLE_5,
    RTABLE_6,
    RTABLE_7,
    RTABLE_8,
};

static const GwyEnum builtin_menu[] = {
    { "User-specified",       RAW_NONE            },
    { "Signed byte",          RAW_SIGNED_BYTE     },
    { "Unsigned byte",        RAW_UNSIGNED_BYTE   },
    { "Signed 16bit word",    RAW_SIGNED_WORD16   },
    { "Unsigned 16bit word",  RAW_UNSIGNED_WORD16 },
    { "Signed 32bit word",    RAW_SIGNED_WORD32   },
    { "Unsigned 32bit word",  RAW_UNSIGNED_WORD32 },
    { "IEEE single",          RAW_IEEE_FLOAT      },
    { "IEEE double",          RAW_IEEE_DOUBLE     },
};

static const RawFileArgs rawfile_defaults = {
    RAW_BINARY,                     /* format */
    RAW_UNSIGNED_BYTE, 0, 8, 0, 0,  /* binary parameters */
    FALSE, FALSE, FALSE, 0,         /* binary options */
    0, " ", 0,                      /* text parameters */
    NULL, 0,                        /* file name and size */
    500, 500,                       /* xres, yres */
    1000.0, 1000.0, 10.0            /* physical dimensions */
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo rawfile_func_info = {
        "rawfile",
        "Raw data files",
        (GwyFileDetectFunc)&rawfile_detect,
        (GwyFileLoadFunc)&rawfile_load,
        NULL,
    };

    gwy_file_func_register(name, &rawfile_func_info);

    return TRUE;
}

static gint
rawfile_detect(const gchar *filename,
               gboolean only_name)
{
    FILE *fh;

    if (only_name)
        return 1;

    if (!(fh = fopen(filename, "rb")))
        return 0;
    fclose(fh);

    return 1;
}

static GwyContainer*
rawfile_load(const gchar *filename)
{
    RawFileArgs args;
    GwyContainer *settings, *data;
    GwyDataField *dfield;
    GError *err = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    gboolean ok;

    settings = gwy_app_settings_get();
    rawfile_load_args(settings, &args);
    if (!g_file_get_contents(filename, (gchar**)&buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    data = NULL;
    args.filename = filename;
    args.filesize = size;
    ok = rawfile_dialog(&args);
    if (ok) {
        /* FIXME: units */
        dfield = GWY_DATA_FIELD(gwy_data_field_new(args.xres, args.yres,
                                                   1e-9*args.xreal,
                                                   1e-9*args.yreal,
                                                   FALSE));
        if (args.builtin)
            rawfile_read_builtin(&args, buffer,
                                 gwy_data_field_get_data(dfield));
        else
            rawfile_read_bits(&args, buffer, gwy_data_field_get_data(dfield));
        gwy_data_field_multiply(dfield, 1e-9*args.zscale);
        data = GWY_CONTAINER(gwy_container_new());
        gwy_container_set_object_by_name(data, "/0/data", G_OBJECT(dfield));
        rawfile_save_args(settings, &args);
    }
    g_free(buffer);

    return data;
}

static gboolean
rawfile_dialog(RawFileArgs *args)
{
    RawFileControls controls;
    GtkWidget *dialog, *vbox, *table, *label, *notebook, *button;
    GtkWidget *omenu, *entry;
    GtkObject *adj;
    gint response, row, precision;
    gsize reqsize;
    gdouble magnitude;
    gchar *s;

    dialog = gtk_dialog_new_with_buttons(_("Read Raw File"),
                                         GTK_WINDOW(gwy_app_main_window_get()),
                                         GTK_DIALOG_MODAL
                                         | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    vbox = GTK_DIALOG(dialog)->vbox;

    notebook = gtk_notebook_new();
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    /***** Sample info *****/
    vbox = gtk_vbox_new(FALSE, 0);   /* to prevent notebook expanding tables */
    label = gtk_label_new(_("Information"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

    table = gtk_table_new(12, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>File</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 3, row, row+1);
    row++;

    label = gtk_label_new(args->filename);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 2, row, row+1);

    magnitude = gwy_math_humanize_numbers(0.004*args->filesize,
                                          1.0*args->filesize,
                                          &precision);
    s = g_strdup_printf("(%.*f %sB)", precision, args->filesize/magnitude,
                        gwy_math_SI_prefix(magnitude));
    label = gtk_label_new(s);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 2, 3, row, row+1);
    g_free(s);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = table_attach_heading(table, _("<b>Resolution</b>"), row);
    row++;

    adj = gtk_adjustment_new(args->xres, 0, 16384, 1, 10, 100);
    controls.xres = gwy_table_attach_spinbutton(table, row,
                                                _("_Horizontal size"),
                                                _("data samples"), adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls.xres), 0);
    row++;

    adj = gtk_adjustment_new(args->yres, 0, 16384, 1, 10, 100);
    controls.yres = gwy_table_attach_spinbutton(table, row,
                                                _("_Vertical size"),
                                                _("data samples"), adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls.yres), 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = table_attach_heading(table, _("<b>Physical dimensions</b>"), row);
    row++;

    adj = gtk_adjustment_new(args->xreal, 0, 1e9, 10, 1000, 1000);
    controls.xreal = gwy_table_attach_spinbutton(table, row,
                                                 _("_Width"), "nm", adj);
    row++;

    adj = gtk_adjustment_new(args->yreal, 0, 1e9, 10, 1000, 1000);
    controls.yreal = gwy_table_attach_spinbutton(table, row,
                                                 _("H_eight"), "nm", adj);
    row++;

    adj = gtk_adjustment_new(args->yreal, 0, 1e100, 0.1, 10, 10);
    controls.zscale = gwy_table_attach_spinbutton(table, row, _("_Z-scale"),
                                                  _("nm/sample unit"), adj);
    row++;

    /***** General data format *****/
    vbox = gtk_vbox_new(FALSE, 0);   /* to prevent notebook expanding tables */
    label = gtk_label_new(_("Data Format"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

    table = gtk_table_new(5, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

    label = table_attach_heading(table, _("<b>Format</b>"), row);
    row++;

    button = gtk_radio_button_new_with_mnemonic(NULL, _("_Text data"));
    gtk_widget_set_sensitive(button, FALSE);
    g_object_set_data(G_OBJECT(button), "format", GINT_TO_POINTER(RAW_TEXT));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    row++;

    button
        = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(button),
                                                         _("_Binary data"));
    g_object_set_data(G_OBJECT(button), "format", GINT_TO_POINTER(RAW_BINARY));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 1, row, row+1);
    controls.format = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));

    omenu = gwy_option_menu_create(builtin_menu, G_N_ELEMENTS(builtin_menu),
                                   "builtin",
                                   G_CALLBACK(builtin_changed_cb), &controls,
                                   args->builtin);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    controls.builtin = omenu;
    row++;

    label = gtk_label_new_with_mnemonic(_("Byte s_wap pattern"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, row, row+1);

    controls.byteswap = entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), 17);
    gtk_table_attach_defaults(GTK_TABLE(table), entry, 1, 2, row, row+1);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = table_attach_heading(table, _("<b>Binary Parameters</b>"), row);
    row++;

    adj = gtk_adjustment_new(args->offset, 0, 1 << 30, 16, 1024, 1024);
    controls.offset = gwy_table_attach_spinbutton(table, row,
                                                  _("Start at _offset"),
                                                  "bytes", adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls.offset), 0);
    row++;

    adj = gtk_adjustment_new(args->size, 1, 24, 1, 8, 8);
    controls.size = gwy_table_attach_spinbutton(table, row,
                                                _("_Sample size"), "bits", adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls.size), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(controls.size), TRUE);
    row++;

    adj = gtk_adjustment_new(args->skip, 0, 1 << 30, 1, 8, 8);
    controls.skip = gwy_table_attach_spinbutton(table, row,
                                                _("After each sample s_kip"),
                                                "bits", adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls.skip), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(controls.skip), TRUE);
    row++;

    adj = gtk_adjustment_new(args->rowskip, 0, 1 << 30, 1, 8, 8);
    controls.rowskip = gwy_table_attach_spinbutton(table, row,
                                                   _("After each _row skip"),
                                                   "bits", adj);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(controls.rowskip), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(controls.rowskip), TRUE);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("_Reverse bits in bytes"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls.revbyte = button;
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Reverse b_its in samples"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls.revsample = button;
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Samples are si_gned"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    controls.sign = button;
    row++;

    controls.args = args;

    update_dialog_controls(&controls);
    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        update_dialog_values(&controls);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            reqsize = rawfile_compute_required_size(args);
            if (reqsize > args->filesize) {
                rawfile_warn_too_short_file(dialog, args, reqsize);
                response = GTK_RESPONSE_NONE;
            }
            break;

            case RESPONSE_RESET:
            {
                const gchar *filename = args->filename;
                gsize filesize = args->filesize;

                *args = rawfile_defaults;
                args->filename = filename;
                args->filesize = filesize;
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
        update_dialog_controls(&controls);
    } while (response != GTK_RESPONSE_OK);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
rawfile_warn_too_short_file(GtkWidget *parent,
                            RawFileArgs *args,
                            gsize reqsize)
{
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                    GTK_DIALOG_DESTROY_WITH_PARENT
                                    | GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_OK,
                                    _("The format would require %u bytes "
                                      "long file (at least), "
                                      "but the length of `%s' "
                                      "is only %u bytes."),
                                    reqsize, args->filename, args->filesize),
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void
builtin_changed_cb(GtkWidget *item,
                   RawFileControls *controls)
{
    gint builtin;
    GtkAdjustment *adj;

    builtin = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item), "builtin"));
    update_dialog_values(controls);
    if (builtin) {
        rawfile_santinize_args(controls->args);

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
    update_dialog_controls(controls);
}

static void
update_dialog_controls(RawFileControls *controls)
{
    RawFileArgs *args;
    GSList *group;
    GtkAdjustment *adj;
    gchar buf[16];
    RawFileBuiltin builtin;

    args = controls->args;

    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xres));
    gtk_adjustment_set_value(adj, args->xres);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yres));
    gtk_adjustment_set_value(adj, args->yres);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->xreal));
    gtk_adjustment_set_value(adj, args->xreal);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->yreal));
    gtk_adjustment_set_value(adj, args->yreal);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->zscale));
    gtk_adjustment_set_value(adj, args->zscale);

    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->offset));
    gtk_adjustment_set_value(adj, args->offset);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->size));
    gtk_adjustment_set_value(adj, args->size);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->skip));
    gtk_adjustment_set_value(adj, args->skip);
    adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(controls->rowskip));
    gtk_adjustment_set_value(adj, args->rowskip);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->sign),
                                 args->sign);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->revbyte),
                                 args->revbyte);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->revsample),
                                 args->revsample);

    g_snprintf(buf, sizeof(buf), "%u", args->byteswap);
    gtk_entry_set_text(GTK_ENTRY(controls->byteswap), buf);

    gwy_option_menu_set_history(controls->builtin, "builtin",
                                args->builtin);

    for (group = controls->format; group; group = g_slist_next(group)) {
        if (GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(group->data), "format"))
            == args->format) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(group->data), TRUE);
            break;
        }
    }

    builtin = args->builtin;
    gtk_widget_set_sensitive(controls->byteswap,
                             builtin
                             && builtin != RAW_UNSIGNED_BYTE
                             && builtin != RAW_SIGNED_BYTE);
    gtk_widget_set_sensitive(controls->size, !builtin);
    gtk_widget_set_sensitive(controls->sign, !builtin);
    gtk_widget_set_sensitive(controls->revsample, !builtin);
}

static void
update_dialog_values(RawFileControls *controls)
{
    RawFileArgs *args;
    GSList *group;

    args = controls->args;

    args->xres
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->xres));
    args->yres
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->yres));
    args->xreal
        = gtk_spin_button_get_value(GTK_SPIN_BUTTON(controls->xreal));
    args->yreal
        = gtk_spin_button_get_value(GTK_SPIN_BUTTON(controls->yreal));
    args->zscale
        = gtk_spin_button_get_value(GTK_SPIN_BUTTON(controls->zscale));

    args->offset
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->offset));
    args->size
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->size));
    args->skip
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->skip));
    args->rowskip
        = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(controls->rowskip));

    args->sign
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->sign));
    args->revbyte
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->revbyte));
    args->revsample
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->revsample));

    args->byteswap = atoi(gtk_entry_get_text(GTK_ENTRY(controls->byteswap)));

    args->builtin = gwy_option_menu_get_history(controls->builtin, "builtin");

    for (group = controls->format; group; group = g_slist_next(group)) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(group->data))) {
            args->format
                = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(group->data),
                                                    "format"));
            break;
        }
    }

    rawfile_santinize_args(args);
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
reverse_bits(guint32 x, gsize n)
{
    gulong y = 0;

    if (n < G_N_ELEMENTS(RTABLE))
        return RTABLE[n][x];

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
    gsize i, j, nb;
    guint32 b, bucket, x, rem;

    g_assert(args->size <= 24);
    g_assert(args->size > 1 || !args->sign);

    buffer += args->offset;
    nb = 0;
    bucket = 0;
    for (i = args->yres; i; i--) {
        for (j = args->xres; j; j--) {
            /* gather enough bits, new bits are put to the least significant
             * position */
            while (nb < args->size) {
                b = *(buffer++);
                if (args->revbyte)
                    b = RTABLE_8[b];
                bucket <<= 8;
                bucket |= b;
                nb += 8;
            }
            /* we have this many too much bits now (in the least significat
             * part of bucket) */
            rem = nb - args->size;
            /* x is the data sample (in the most significat part of  bucket) */
            x = bucket >> rem;
            if (args->revsample)
                x = reverse_bits(x, args->size);
            /* rem bits remains in bucket */
            bucket &= BITMASK[rem];
            nb = rem;

            /* sign-extend to 32bit signed number if signed */
            if (args->sign) {
                if (x & BITMASK[args->size-1])
                    x |= ~BITMASK[args->size];
                *(data++) = (gdouble)(gint32)x;
            }
            else
                *(data++) = (gdouble)x;

            /* skip args->skip bits, only the last byte is important */
            if (nb < args->skip) {
                /* skip what we have in the bucket */
                rem = args->skip - nb;
                /* whole bytes */
                buffer += rem/8;
                rem %= 8;  /* remains to skip */
                nb = 8 - rem;  /* so this number of bits will be in bucket */
                b = *(buffer++);
                if (args->revbyte)
                    b = RTABLE_8[b];
                bucket = b & BITMASK[nb];
            }
            else {
                /* we have enough bits in bucket, so just get rid of the
                 * extra ones */
                nb -= args->skip;
                bucket &= BITMASK[nb];
            }
        }
        /* skip args->rowskip bits, only the last byte is important */
        if (nb < args->rowskip) {
            /* skip what we have in the bucket */
            rem = args->rowskip - nb;
            /* whole bytes */
            buffer += rem/8;
            rem %= 8;  /* remains to skip */
            nb = 8 - rem;  /* so this number of bits will be in bucket */
            b = *(buffer++);
            if (args->revbyte)
                b = RTABLE_8[b];
            bucket = b & BITMASK[nb];
        }
        else {
            /* we have enough bits in bucket, so just get rid of the
             * extra ones */
            nb -= args->rowskip;
            bucket &= BITMASK[nb];
        }
    }
}

static void
rawfile_read_builtin(RawFileArgs *args,
                     guchar *buffer,
                     gdouble *data)
{
    gsize i, j, k, size, skip, rowskip;
    double good_alignment;
    guchar *b;

    g_assert(args->builtin > RAW_NONE && args->builtin < RAW_LAST);
    g_assert(args->size <= 64 && args->size % 8 == 0);
    g_assert(args->skip % 8 == 0);
    g_assert(args->rowskip % 8 == 0);

    buffer += args->offset;
    size = args->size/8;
    skip = args->skip/8;
    rowskip = args->rowskip/8;
    b = (guchar*)&good_alignment;
    memset(b, 0, 8);

    for (i = args->yres; i; i--) {
        for (j = args->xres; j; j--) {
            /* the XOR magic puts each byte where it belongs according to
             * byteswap */
            if (args->revbyte) {
                for (k = 0; k < size; k++)
                    b[k ^ args->byteswap] = RTABLE_8[*(buffer++)];
            }
            else {
                for (k = 0; k < size; k++)
                    b[k ^ args->byteswap] = *(buffer++);
            }
            /* now interpret b as a number in HOST order */
            switch (args->builtin) {
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
}

static void
rawfile_santinize_args(RawFileArgs *args)
{
    args->builtin = MIN(args->builtin, RAW_LAST-1);
    if (args->builtin) {
        args->size = BUILTIN_SIZE[args->builtin];
        args->sign = (args->builtin == RAW_SIGNED_BYTE)
                    || (args->builtin == RAW_SIGNED_WORD16)
                    || (args->builtin == RAW_SIGNED_WORD32);
        args->skip = ((args->skip + 7)/8)*8;
        args->rowskip = ((args->rowskip + 7)/8)*8;
        args->byteswap = MIN(args->byteswap, args->size-1);
        args->revsample = FALSE;
    }
    else {
        args->builtin = MIN(args->builtin, 24);
        args->byteswap = 0;
    }
}

static gsize
rawfile_compute_required_size(RawFileArgs *args)
{
    gsize rowstride;

    rowstride = (args->size + args->skip)*args->xres + args->rowskip;
    if (args->builtin && rowstride%8) {
        g_warning("rowstride is not a whole number of bytes");
        rowstride = ((rowstride + 7)/8)*8;
    }
    return args->offset + args->yres*rowstride/8;
}

static const gchar *builtin_key =   "/module/rawfile/builtin";
static const gchar *offset_key =    "/module/rawfile/offset";
static const gchar *size_key =      "/module/rawfile/size";
static const gchar *skip_key =      "/module/rawfile/skip";
static const gchar *rowskip_key =   "/module/rawfile/rowskip";
static const gchar *sign_key =      "/module/rawfile/sign";
static const gchar *revsample_key = "/module/rawfile/revsample";
static const gchar *revbyte_key =   "/module/rawfile/revbyte";
static const gchar *byteswap_key =  "/module/rawfile/byteswap";
static const gchar *xres_key =      "/module/rawfile/xres";
static const gchar *yres_key =      "/module/rawfile/yres";
static const gchar *xreal_key =     "/module/rawfile/xreal";
static const gchar *yreal_key =     "/module/rawfile/yreal";
static const gchar *zscale_key =    "/module/rawfile/zscale";

static void
rawfile_load_args(GwyContainer *settings,
                  RawFileArgs *args)
{
    *args = rawfile_defaults;

    if (gwy_container_contains_by_name(settings, builtin_key))
        args->builtin = gwy_container_get_int32_by_name(settings, builtin_key);
    if (gwy_container_contains_by_name(settings, offset_key))
        args->offset = gwy_container_get_int32_by_name(settings, offset_key);
    if (gwy_container_contains_by_name(settings, size_key))
        args->size = gwy_container_get_int32_by_name(settings, size_key);
    if (gwy_container_contains_by_name(settings, skip_key))
        args->skip = gwy_container_get_int32_by_name(settings, skip_key);
    if (gwy_container_contains_by_name(settings, rowskip_key))
        args->rowskip = gwy_container_get_int32_by_name(settings, rowskip_key);
    if (gwy_container_contains_by_name(settings, byteswap_key))
        args->byteswap = gwy_container_get_int32_by_name(settings,
                                                         byteswap_key);
    if (gwy_container_contains_by_name(settings, sign_key))
        args->sign = gwy_container_get_boolean_by_name(settings, sign_key);
    if (gwy_container_contains_by_name(settings, revsample_key))
        args->revsample = gwy_container_get_boolean_by_name(settings,
                                                            revsample_key);
    if (gwy_container_contains_by_name(settings, revbyte_key))
        args->revbyte = gwy_container_get_boolean_by_name(settings,
                                                          revbyte_key);

    if (gwy_container_contains_by_name(settings, xres_key))
        args->xres = gwy_container_get_int32_by_name(settings, xres_key);
    if (gwy_container_contains_by_name(settings, yres_key))
        args->yres = gwy_container_get_int32_by_name(settings, yres_key);

    if (gwy_container_contains_by_name(settings, xreal_key))
        args->xreal = gwy_container_get_double_by_name(settings, xreal_key);
    if (gwy_container_contains_by_name(settings, yreal_key))
        args->yreal = gwy_container_get_double_by_name(settings, yreal_key);
    if (gwy_container_contains_by_name(settings, zscale_key))
        args->zscale = gwy_container_get_double_by_name(settings, zscale_key);

    rawfile_santinize_args(args);
}

static void
rawfile_save_args(GwyContainer *settings,
                  RawFileArgs *args)
{
    rawfile_santinize_args(args);

    gwy_container_set_int32_by_name(settings, builtin_key, args->builtin);
    gwy_container_set_int32_by_name(settings, offset_key, args->offset);
    gwy_container_set_int32_by_name(settings, size_key, args->size);
    gwy_container_set_int32_by_name(settings, skip_key, args->skip);
    gwy_container_set_int32_by_name(settings, rowskip_key, args->rowskip);
    gwy_container_set_int32_by_name(settings, byteswap_key, args->byteswap);
    gwy_container_set_boolean_by_name(settings, sign_key, args->sign);
    gwy_container_set_boolean_by_name(settings, revsample_key, args->revsample);
    gwy_container_set_boolean_by_name(settings, revbyte_key, args->revbyte);

    gwy_container_set_int32_by_name(settings, xres_key, args->xres);
    gwy_container_set_int32_by_name(settings, yres_key, args->yres);
    gwy_container_set_double_by_name(settings, xreal_key, args->xreal);
    gwy_container_set_double_by_name(settings, yreal_key, args->yreal);
    gwy_container_set_double_by_name(settings, zscale_key, args->zscale);
}

#else /* not GENRTABLE */
/************************ RTABLE generator **************************/
/* compile as a standalone file with -DGENRTABLE */

int
main(int argc, char *argv[])
{
    static const char *tab = "    ";
    unsigned long int i, j, k;
    int s, t, m = 8, w;

    if (argc > 1) {
        m = atoi(argv[1]);
        if (m < 0 || m > 16)
            m = 8;
    }

    for (s = 0; s <= m; s++) {
        w = (s + 3)/4;
        printf("static const guint32 RTABLE_%d[] = {\n", s);
        for (i = 0; i < (1 << s); i++) {
            if (i % 8 == 0)
                printf(tab);
            t = s;
            k = i;
            j = 0;
            while (t--) {
                j <<= 1;
                j |= k&1;
                k >>= 1;
            }
            printf("0x%0*x, ", w, j);
            if ((i + 1) % 8 == 0)
                printf("\n");
        }
        if (i % 8 != 0)
            printf("\n");
        printf("};\n\n");
    }

    printf("static const guint32 *const RTABLE[] = {\n");
    for (s = 0; s <= m; s++)
        printf("%sRTABLE_%d,\n", tab, s);
    printf("};\n\n");

    return 0;
}

#endif /* not GENRTABLE */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
