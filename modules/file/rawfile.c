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
#include <string.h>
#ifndef GENRTABLE
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/app.h>

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

/* note: size, skip, and rowskip are in bits */
typedef struct {
    RawFileBuiltin builtin;
    gsize offset;  /* offset from file start, in bytes */
    gsize size;  /* data sample size (unused if builtin) */
    gsize skip;  /* skip after each sample (unused if builtin) */
    gsize rowskip;  /* extra skip after each sample row */
    gboolean sign;  /* take the number as signed? (unused if not integer) */
    gboolean revsample;  /* reverse bit order in samples? */
    gboolean revbyte;  /* reverse bit order in bytes as we read them? */
    gsize byteswap;  /* swap bytes (relative to HOST order), bit set means
                        swap blocks of this size (only for builtin) */
    gsize lineoffset;  /* start reading from this line (ASCII) */
    gchar *delimiter;  /* field delimiter (ASCII) */
    gsize skipfields;  /* skip this number of fields at line start (ASCII) */
} RawFileSpec;

typedef struct {
    const gchar *filename;
    gsize xres;
    gsize yres;
    gdouble xreal;
    gdouble yreal;
    gdouble zscale;
} RawFileParams;

static gboolean      module_register     (const gchar *name);
static gint          rawfile_detect      (const gchar *filename,
                                          gboolean only_name);
static GwyContainer* rawfile_load        (const gchar *filename);
static gboolean      rawfile_dialog      (RawFileSpec *spec,
                                          RawFileParams *param,
                                          gsize filesize);
static GtkWidget*    table_attach_heading(GtkWidget *table,
                                          const gchar *text,
                                          gint row);
static void          rawfile_read_builtin(RawFileSpec *spec,
                                          RawFileParams *param,
                                          guchar *buffer,
                                          gdouble *data);
static void          rawfile_read_bits   (RawFileSpec *spec,
                                          RawFileParams *param,
                                          guchar *buffer,
                                          gdouble *data);
static gsize         rawfile_compute_size(RawFileSpec *spec,
                                          RawFileParams *param);


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
    0x00,
};

static const guint32 RTABLE_1[] = {
    0x00, 0x01,
};

static const guint32 RTABLE_2[] = {
    0x00, 0x02, 0x01, 0x03,
};

static const guint32 RTABLE_3[] = {
    0x00, 0x04, 0x02, 0x06, 0x01, 0x05, 0x03, 0x07,
};

static const guint32 RTABLE_4[] = {
    0x00, 0x08, 0x04, 0x0c, 0x02, 0x0a, 0x06, 0x0e,
    0x01, 0x09, 0x05, 0x0d, 0x03, 0x0b, 0x07, 0x0f,
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

static GwyEnum builtin_menu[] = {
    { "Signed byte",          RAW_SIGNED_BYTE     },
    { "Unsigned byte",        RAW_UNSIGNED_BYTE   },
    { "Signed 16bit word",    RAW_SIGNED_WORD16   },
    { "Unsigned 16bit word",  RAW_UNSIGNED_WORD16 },
    { "Signed 32bit word",    RAW_SIGNED_WORD32   },
    { "Unsigned 32bit word",  RAW_UNSIGNED_WORD32 },
    { "IEEE single",          RAW_IEEE_FLOAT      },
    { "IEEE double",          RAW_IEEE_DOUBLE     },
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
    RawFileSpec spec;
    RawFileParams param;
    GObject *object;
    GError *err = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    gsize pos = 0;

    /*
    if (!g_file_get_contents(filename, (gchar**)&buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < 4
        || memcmp(buffer, MAGIC, MAGIC_SIZE)) {
        g_warning("File %s doesn't seem to be a .gwy file", filename);
        g_free(buffer);
        return NULL;
    }

    object = gwy_serializable_deserialize(buffer + 4, size - 4, &pos);
    g_free(buffer);
    if (!object) {
        g_warning("File %s deserialization failed", filename);
        return NULL;
    }
    if (!GWY_IS_CONTAINER(object)) {
        g_warning("File %s contains some strange object", filename);
        g_object_unref(object);
        return NULL;
    }
    */
    param.filename = filename;
    rawfile_dialog(&spec, &param, 100000);
    return NULL;
    return (GwyContainer*)object;
}

static gboolean
rawfile_dialog(RawFileSpec *spec, RawFileParams *param,
               gsize filesize)
{
    GtkWidget *dialog, *vbox, *table, *label, *spin, *notebook, *button;
    GtkWidget *omenu, *entry;
    GtkObject *adj;
    gint response, row;

    dialog = gtk_dialog_new_with_buttons(_("Read Raw File"),
                                         GTK_WINDOW(gwy_app_main_window_get()),
                                         GTK_DIALOG_MODAL
                                         | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
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

    table = gtk_table_new(5, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>File</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 3, row, row+1);
    row++;

    label = gtk_label_new(param->filename);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 3, row, row+1);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = table_attach_heading(table, _("<b>Resolution</b>"), row);
    row++;

    adj = gtk_adjustment_new(param->xres, 0, 16384, 1, 10, 100);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("Horizontal size"),
                                       _("data samples"), adj);
    row++;

    adj = gtk_adjustment_new(param->yres, 0, 16384, 1, 10, 100);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("Vertical size"),
                                       _("data samples"), adj);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = table_attach_heading(table, _("<b>Physical dimensions</b>"), row);
    row++;

    adj = gtk_adjustment_new(param->xreal, 0, 1e9, 10, 1000, 1000);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("Width"), "nm", adj);
    row++;

    adj = gtk_adjustment_new(param->yreal, 0, 1e9, 10, 1000, 1000);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("Height"), "nm", adj);
    row++;

    adj = gtk_adjustment_new(param->yreal, 0, 1e100, 0.1, 10, 10);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("Z-scale"), _("nm/sample unit"), adj);
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

    button = gtk_radio_button_new_with_label(NULL, _("Binary data"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    row++;

    button
        = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button),
                                                      _("Text data"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    row++;

    button
        = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button),
                                                      _("Standard"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 1, row, row+1);

    omenu = gwy_option_menu_create(builtin_menu, G_N_ELEMENTS(builtin_menu),
                                   "builtin",
                                   NULL, NULL, spec->builtin);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    row++;

    label = gtk_label_new_with_mnemonic(_("S_wap bytes"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, row, row+1);

    entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), 17);
    gtk_table_attach_defaults(GTK_TABLE(table), entry, 1, 2, row, row+1);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = table_attach_heading(table, _("<b>Parameters</b>"), row);
    row++;

    adj = gtk_adjustment_new(spec->offset, 0, 1 << 31, 16, 1024, 1024);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("Start at _offset"), "bytes", adj);
    row++;

    adj = gtk_adjustment_new(spec->size, 1, 24, 1, 8, 8);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("_Sample size"), "bits", adj);
    row++;

    adj = gtk_adjustment_new(spec->skip, 0, 1 << 31, 1, 8, 8);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("After each sample s_kip"), "bits",
                                       adj);
    row++;

    adj = gtk_adjustment_new(spec->rowskip, 0, 1 << 31, 1, 8, 8);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("After each _row skip"), "bits", adj);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("_Reverse bits in bytes"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Reverse b_its in samples"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Samples are si_gned"));
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 3, row, row+1);
    row++;

    gtk_widget_show_all(dialog);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return FALSE;
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
rawfile_read_bits(RawFileSpec *spec,
                  RawFileParams *param,
                  guchar *buffer,
                  gdouble *data)
{
    gsize i, j, nb;
    guint32 b, bucket, x, rem;

    g_assert(spec->size <= 24);
    g_assert(spec->size > 1 || !spec->sign);

    buffer += spec->offset;
    nb = 0;
    bucket = 0;
    for (i = param->yres; i; i--) {
        for (j = param->xres; j; j--) {
            /* gather enough bits, new bits are put to the least significant
             * position */
            while (nb < spec->size) {
                b = *(buffer++);
                if (spec->revbyte)
                    b = RTABLE_8[b];
                bucket <<= 8;
                bucket |= b;
                nb += 8;
            }
            /* we have this many too much bits now (in the least significat
             * part of bucket) */
            rem = nb - spec->size;
            /* x is the data sample (in the most significat part of  bucket) */
            x = bucket >> rem;
            if (spec->revsample)
                x = reverse_bits(x, spec->size);
            /* rem bits remains in bucket */
            bucket &= BITMASK[rem];
            nb = rem;

            /* sign-extend to 32bit signed number if signed */
            if (spec->sign) {
                if (x & BITMASK[spec->size-1])
                    x |= ~BITMASK[spec->size];
                *(data++) = (gdouble)(gint32)x;
            }
            else
                *(data++) = (gdouble)x;

            /* skip spec->skip bits, only the last byte is important */
            if (nb < spec->skip) {
                /* skip what we have in the bucket */
                rem = spec->skip - nb;
                /* whole bytes */
                buffer += rem/8;
                rem %= 8;  /* remains to skip */
                nb = 8 - rem;  /* so this number of bits will be in bucket */
                b = *(buffer++);
                if (spec->revbyte)
                    b = RTABLE_8[b];
                bucket = b & BITMASK[nb];
            }
            else {
                /* we have enough bits in bucket, so just get rid of the
                 * extra ones */
                nb -= spec->skip;
                bucket &= BITMASK[nb];
            }
        }
        /* skip spec->rowskip bits, only the last byte is important */
        if (nb < spec->rowskip) {
            /* skip what we have in the bucket */
            rem = spec->rowskip - nb;
            /* whole bytes */
            buffer += rem/8;
            rem %= 8;  /* remains to skip */
            nb = 8 - rem;  /* so this number of bits will be in bucket */
            b = *(buffer++);
            if (spec->revbyte)
                b = RTABLE_8[b];
            bucket = b & BITMASK[nb];
        }
        else {
            /* we have enough bits in bucket, so just get rid of the
             * extra ones */
            nb -= spec->rowskip;
            bucket &= BITMASK[nb];
        }
    }
}

static void
rawfile_read_builtin(RawFileSpec *spec,
                     RawFileParams *param,
                     guchar *buffer,
                     gdouble *data)
{
    gsize i, j, k, size, skip, rowskip;
    guint32 x32;
    guint16 x16;
    double good_alignment;
    guchar *b;

    g_assert(spec->builtin > RAW_NONE && spec->builtin < RAW_LAST);
    g_assert(spec->size <= 64 && spec->size % 8 == 0);
    g_assert(spec->skip % 8 == 0);
    g_assert(spec->rowskip % 8 == 0);

    buffer += spec->offset;
    size = spec->size/8;
    skip = spec->skip/8;
    rowskip = spec->rowskip/8;
    b = (guchar*)&good_alignment;
    memset(b, 0, 8);

    for (i = param->yres; i; i--) {
        for (j = param->xres; j; j--) {
            /* the XOR magic puts each byte where it belongs according to
             * byteswap */
            if (spec->revbyte) {
                for (k = 0; k < size; k++)
                    b[k ^ spec->byteswap] = RTABLE_8[*(buffer++)];
            }
            else {
                for (k = 0; k < size; k++)
                    b[k ^ spec->byteswap] = *(buffer++);
            }
            /* now interpret b as a number in HOST order */
            switch (spec->builtin) {
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
rawfile_builtin_to_spec(RawFileSpec *spec)
{
    g_assert(spec->builtin > RAW_NONE && spec->builtin < RAW_LAST);
    spec->size = 8*BUILTIN_SIZE[spec->builtin];
    spec->sign = (spec->builtin == RAW_SIGNED_BYTE)
                 || (spec->builtin == RAW_SIGNED_WORD16)
                 || (spec->builtin == RAW_SIGNED_WORD32);
    spec->skip = (spec->skip + 7)/8;
    spec->rowskip = (spec->rowskip + 7)/8;
}

static gsize
rawfile_compute_size(RawFileSpec *spec,
                     RawFileParams *param)
{
    gsize rowstride;

    rowstride = (spec->size + spec->skip)*param->xres + spec->rowskip;
    if (spec->builtin && rowstride%8) {
        g_warning("rowstride is not a whole number of bytes");
        rowstride = (rowstride + 7)%8;
    }
    return spec->offset + param->yres*rowstride;
}

#else /* not GENRTABLE */
/************************ RTABLE generator **************************/
/* compile as a standalone file with -DGENRTABLE */

int
main(void)
{
    unsigned long int i, j, k;
    int s, t;

    for (s = 0; s <= 8; s++) {
        printf("static const guchar RTABLE_%d[] = {\n", s);
        for (i = 0; i < (1 << s); i++) {
            if (i % 8 == 0)
                printf("    ");
            t = s;
            k = i;
            j = 0;
            while (t--) {
                j <<= 1;
                j |= k&1;
                k >>= 1;
            }
            printf("0x%02x, ", j);
            if ((i + 1) % 8 == 0)
                printf("\n");
        }
        if (i % 8 != 0)
            printf("\n");
        printf("};\n\n");
    }

    return 0;
}

#endif /* not GENRTABLE */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
