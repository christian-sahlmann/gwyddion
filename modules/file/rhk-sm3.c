/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "get.h"

/* Ugly Microsfot UTF-16... */
static const guchar MAGIC[] = {
  0x9e, 0x00, 0x53, 0x00, 0x54, 0x00, 0x69, 0x00, 0x4d, 0x00, 0x61, 0x00,
  0x67, 0x00, 0x65, 0x00, 0x20, 0x00, 0x30, 0x00, 0x30, 0x00, 0x34, 0x00,
  0x2e, 0x00, 0x30, 0x00, 0x30, 0x00, 0x31, 0x00, 0x20, 0x00, 0x31, 0x00
};
#define MAGIC_SIZE (G_N_ELEMENTS(MAGIC))

#define EXTENSION ".sm3"

enum { HEADER_SIZE = MAGIC_SIZE + 2*4 + 15*4 + 11*4 + 16 };

typedef enum {
    RHK_TYPE_IMAGE          = 0,
    RHK_TYPE_LINE           = 1,
    RHK_TYPE_ANNOTATED_LINE = 3
} RHKType;

typedef enum {
    RHK_PAGE_UNDEFINED                = 0,
    RHK_PAGE_TOPOGAPHIC               = 1,
    RHK_PAGE_CURRENT                  = 2,
    RHK_PAGE_AUX                      = 3,
    RHK_PAGE_FORCE                    = 4,
    RHK_PAGE_SIGNAL                   = 5,
    RHK_PAGE_FFT                      = 6,
    RHK_PAGE_NOISE_POWER_SPECTRUM     = 7,
    RHK_PAGE_LINE_TEST                = 8,
    RHK_PAGE_OSCILLOSCOPE             = 9,
    RHK_PAGE_IV_SPECTRA               = 10,
    RHK_PAGE_IV_4x4                   = 11,
    RHK_PAGE_IV_8x8                   = 12,
    RHK_PAGE_IV_16x16                 = 13,
    RHK_PAGE_IV_32x32                 = 14,
    RHK_PAGE_IV_CENTER                = 15,
    RHK_PAGE_INTERACTIVE_SPECTRA      = 16,
    RHK_PAGE_AUTOCORRELATION          = 17,
    RHK_PAGE_IZ_SPECTRA               = 18,
    RHK_PAGE_4_GAIN_TOPOGRAPHY        = 19,
    RHK_PAGE_8_GAIN_TOPOGRAPHY        = 20,
    RHK_PAGE_4_GAIN_CURRENT           = 21,
    RHK_PAGE_8_GAIN_CURRENT           = 22,
    RHK_PAGE_IV_64x64                 = 23,
    RHK_PAGE_AUTOCORRELATION_SPECTRUM = 24,
    RHK_PAGE_COUNTER                  = 25,
    RHK_PAGE_MULTICHANNEL_ANALYSER    = 26,
    RHK_PAGE_AFM_100                  = 27
} RHKPageType;

typedef enum {
    RHK_LINE_NOT_A_LINE                     = 0,
    RHK_LINE_HISTOGRAM                      = 1,
    RHK_LINE_CROSS_SECTION                  = 2,
    RHK_LINE_LINE_TEST                      = 3,
    RHK_LINE_OSCILLOSCOPE                   = 4,
    RHK_LINE_NOISE_POWER_SPECTRUM           = 6,
    RHK_LINE_IV_SPECTRUM                    = 7,
    RHK_LINE_IZ_SPECTRUM                    = 8,
    RHK_LINE_IMAGE_X_AVERAGE                = 9,
    RHK_LINE_IMAGE_Y_AVERAGE                = 10,
    RHK_LINE_NOISE_AUTOCORRELATION_SPECTRUM = 11,
    RHK_LINE_MULTICHANNEL_ANALYSER_DATA     = 12,
    RHK_LINE_RENORMALIZED_IV                = 13,
    RHK_LINE_IMAGE_HISTOGRAM_SPECTRA        = 14,
    RHK_LINE_IMAGE_CROSS_SECTION            = 15,
    RHK_LINE_IMAGE_AVERAGE                  = 16
} RHKLineType;

typedef enum {
    RHK_SOURCE_RAW_PAGE        = 0,
    RHK_SOURCE_PROCESSED_PAGE  = 1,
    RHK_SOURCE_CALCULATED_PAGE = 2,
    RHK_SOURCE_IMPORTED_PAGE   = 3
} RHKSourceType;

typedef enum {
    RHK_IMAGE_NORMAL         = 0,
    RHK_IMAGE_AUTOCORRELATED = 1
} RHKImageType;

typedef enum {
    RHK_SCAN_RIGHT = 0,
    RHK_SCAN_LEFT  = 1,
    RHK_SCAN_UP    = 2,
    RHK_SCAN_DOWN  = 3
} RHKScanType;

typedef enum {
    RHK_STRING_LABEL,
    RHK_STRING_SYSTEM_TEXT,
    RHK_STRING_SESSION_TEXT,
    RHK_STRING_USER_TEXT,
    RHK_STRING_PATH,
    RHK_STRING_DATE,
    RHK_STRING_TIME,
    RHK_STRING_X_UNITS,
    RHK_STRING_Y_UNITS,
    RHK_STRING_Z_UNITS,
    RHK_STRING_X_LABEL,
    RHK_STRING_Y_LABEL,
    RHK_STRING_NSTRINGS
} RHKStringType;

typedef struct {
    guint size;
    /*
    gfloat *hue_start;
    gfloat *saturation_start;
    gfloat *brightness_start;
    gfloat *hue_end;
    gfloat *saturation_end;
    gfloat *brightness_end;
    gboolean *color_direction;
    guint *color_entries;
    */
} RHKColorInformation;

typedef struct {
    guint pageno;  /* Our counter */
    guint param_size;
    gchar version[36];
    guint string_count;
    RHKType type;
    RHKPageType page_type;
    guint data_sub_source;
    RHKLineType line_type;
    gint x_coord;
    gint y_coord;
    guint x_size;
    guint y_size;
    RHKSourceType source_type;
    RHKImageType image_type;
    RHKScanType scan_dir;
    guint group_id;
    guint data_size;
    guint min_z_value;
    guint max_z_value;
    gdouble x_scale;
    gdouble y_scale;
    gdouble z_scale;
    gdouble xy_scale;
    gdouble x_offset;
    gdouble y_offset;
    gdouble z_offset;
    gdouble period;
    gdouble bias;
    gdouble current;
    gdouble angle;
    guchar page_id[16];
    gchar *strings[RHK_STRING_NSTRINGS];
    /* Always 32bit signed integers */
    const guchar *page_data;
    /* Varies, FIXME: we don't import this yet */
    const guchar *spectral_data;
    RHKColorInformation color_info;
} RHKPage;

typedef struct {
    GPtrArray *file;
    GwyContainer *data;
    GtkWidget *data_view;
} RHKControls;

static gboolean      module_register       (const gchar *name);
static gint          rhk_sm3_detect        (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* rhk_sm3_load          (const gchar *filename);
static void          rhk_sm3_store_metadata(RHKPage *rhkpage,
                                            GwyContainer *container);
static guint         select_which_data     (GPtrArray *rhkfile);
static void          selection_changed     (GtkWidget *button,
                                            RHKControls *controls);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports RHK Technology SM3 data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

static const GwyEnum page_types[] = {
    { "Topographic",              RHK_PAGE_TOPOGAPHIC               },
    { "Current",                  RHK_PAGE_CURRENT                  },
    { "Aux",                      RHK_PAGE_AUX                      },
    { "Force",                    RHK_PAGE_FORCE                    },
    { "Signal",                   RHK_PAGE_SIGNAL                   },
    { "FFT transform",            RHK_PAGE_FFT                      },
    { "Noise power spectrum",     RHK_PAGE_NOISE_POWER_SPECTRUM     },
    { "Line test",                RHK_PAGE_LINE_TEST                },
    { "Oscilloscope",             RHK_PAGE_OSCILLOSCOPE             },
    { "IV spectra",               RHK_PAGE_IV_SPECTRA               },
    { "Image IV 4x4",             RHK_PAGE_IV_4x4                   },
    { "Image IV 8x8",             RHK_PAGE_IV_8x8                   },
    { "Image IV 16x16",           RHK_PAGE_IV_16x16                 },
    { "Image IV 32x32",           RHK_PAGE_IV_32x32                 },
    { "Image IV Center",          RHK_PAGE_IV_CENTER                },
    { "Interactive spectra",      RHK_PAGE_INTERACTIVE_SPECTRA      },
    { "Autocorrelation",          RHK_PAGE_AUTOCORRELATION          },
    { "IZ spectra",               RHK_PAGE_IZ_SPECTRA               },
    { "4 gain topography",        RHK_PAGE_4_GAIN_TOPOGRAPHY        },
    { "8 gain topography",        RHK_PAGE_8_GAIN_TOPOGRAPHY        },
    { "4 gain current",           RHK_PAGE_4_GAIN_CURRENT           },
    { "8 gain current",           RHK_PAGE_8_GAIN_CURRENT           },
    { "Image IV 64x64",           RHK_PAGE_IV_64x64                 },
    { "Autocorrelation spectrum", RHK_PAGE_AUTOCORRELATION_SPECTRUM },
    { "Counter data",             RHK_PAGE_COUNTER                  },
    { "Multichannel analyser",    RHK_PAGE_MULTICHANNEL_ANALYSER    },
    { "AFM using AFM-100",        RHK_PAGE_AFM_100                  },
};

static const GwyEnum page_sources[] = {
    { "Raw",        RHK_SOURCE_RAW_PAGE,        },
    { "Processed",  RHK_SOURCE_PROCESSED_PAGE,  },
    { "Calculated", RHK_SOURCE_CALCULATED_PAGE, },
    { "Imported",   RHK_SOURCE_IMPORTED_PAGE,   },
};

static const GwyEnum scan_directions[] = {
    { "Right", RHK_SCAN_RIGHT, },
    { "Left",  RHK_SCAN_LEFT,  },
    { "Up",    RHK_SCAN_UP,    },
    { "Down",  RHK_SCAN_DOWN,  },
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo rhk_sm3_func_info = {
        "rhk-sm3",
        N_("RHK SM3 files (.sm3)"),
        (GwyFileDetectFunc)&rhk_sm3_detect,
        (GwyFileLoadFunc)&rhk_sm3_load,
        NULL
    };

    gwy_file_func_register(name, &rhk_sm3_func_info);

    return TRUE;
}

static gint
rhk_sm3_detect(const GwyFileDetectInfo *fileinfo,
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

static guchar*
rhk_sm3_read_string(const guchar **buffer,
                    gsize len)
{
    gchar *s, *p;
    guint i, n;
    gunichar chr;

    if (len < 2)
        return NULL;

    n = get_WORD(buffer);
    len -= 2;
    if (len < 2*n)
        return NULL;

    if (!n)
        return g_strdup("");

    p = s = g_new(gchar, 6*n + 1);
    for (i = 0; i < n; i++) {
        chr = get_WORD(buffer);
        p += g_unichar_to_utf8(chr, p);
    }
    *p = '\0';
    g_strstrip(s);
    gwy_debug("String: <%s>", s);

    return s;
}

static void
rhk_sm3_page_free(RHKPage *page)
{
    guint i;

    for (i = 0; i < RHK_STRING_NSTRINGS; i++)
        g_free(page->strings[i]);
    g_free(page);
}

static RHKPage*
rhk_sm3_read_page(const guchar **buffer,
                  gsize *len)

{
    RHKPage *page;
    const guchar *p = *buffer;
    guint i, expected;

    if (!*len)
        return NULL;

    if (*len < HEADER_SIZE + 4) {
        g_warning("Page header truncated");
        return NULL;
    }
    if (memcmp(*buffer, MAGIC, MAGIC_SIZE) != 0) {
        g_warning("Magic doesn't match");
        return NULL;
    }

    page = g_new0(RHKPage, 1);
    page->param_size = get_WORD(&p);
    gwy_debug("param_size = %u", page->param_size);
    if (*len < page->param_size + 4) {
        g_warning("Real page header truncated");
        goto FAIL;
    }
    /* Convert to UTF-8 */
    memcpy(page->version, p, 36);
    p += 36;
    page->string_count = get_WORD(&p);
    gwy_debug("string_count = %u", page->string_count);
    page->type = get_DWORD(&p);
    gwy_debug("type = %u", page->type);
    page->page_type = get_DWORD(&p);
    gwy_debug("page_type = %u", page->page_type);
    page->data_sub_source = get_DWORD(&p);
    page->line_type = get_DWORD(&p);
    page->x_coord = get_DWORD(&p);
    page->y_coord = get_DWORD(&p);
    page->x_size = get_DWORD(&p);
    page->y_size = get_DWORD(&p);
    gwy_debug("x_size = %u, y_size = %u", page->x_size, page->y_size);
    page->source_type = get_DWORD(&p);
    page->image_type = get_DWORD(&p);
    gwy_debug("image_type = %u", page->image_type);
    page->scan_dir = get_DWORD(&p);
    gwy_debug("scan_dir = %u", page->scan_dir);
    page->group_id = get_DWORD(&p);
    gwy_debug("group_id = %u", page->group_id);
    page->data_size = get_DWORD(&p);
    gwy_debug("data_size = %u", page->data_size);
    page->min_z_value = (gint)get_DWORD(&p);
    page->max_z_value = (gint)get_DWORD(&p);
    gwy_debug("min,max_z_value = %d %d", page->min_z_value, page->max_z_value);
    page->x_scale = get_FLOAT(&p);
    page->y_scale = get_FLOAT(&p);
    page->z_scale = get_FLOAT(&p);
    gwy_debug("x,y,z_scale = %g %g %g",
              page->x_scale, page->y_scale, page->z_scale);
    page->xy_scale = get_FLOAT(&p);
    page->x_offset = get_FLOAT(&p);
    page->y_offset = get_FLOAT(&p);
    page->z_offset = get_FLOAT(&p);
    gwy_debug("x,y,z_offset = %g %g %g",
              page->x_offset, page->y_offset, page->z_offset);
    page->period = get_FLOAT(&p);
    page->bias = get_FLOAT(&p);
    page->current = get_FLOAT(&p);
    page->angle = get_FLOAT(&p);
    gwy_debug("period = %g, bias = %g, current = %g, angle = %g",
              page->period, page->bias, page->current, page->angle);
    get_CHARARRAY(page->page_id, &p);

    p = *buffer + 2 + page->param_size;
    for (i = 0; i < page->string_count; i++) {
        gchar *s;

        gwy_debug("position %04x", p - *buffer);
        s = rhk_sm3_read_string(&p, *len - (p - *buffer));
        if (!s) {
            g_warning("String truncated");
            goto FAIL;
        }
        if (i < RHK_STRING_NSTRINGS)
            page->strings[i] = s;
        else
            g_free(s);
    }

    expected = page->x_size * page->y_size * sizeof(gint32);
    gwy_debug("expecting %u bytes of page data now", expected);
    if (*len < (p - *buffer) + expected) {
        g_warning("Page data truncated");
        goto FAIL;
    }

    if (page->type == RHK_TYPE_IMAGE)
        page->page_data = p;
    else
        page->spectral_data = p;
    p += expected;

    if (page->type == RHK_TYPE_IMAGE) {
        if (*len < (p - *buffer) + 4) {
            g_warning("Color data header truncated");
            goto FAIL;
        }
        /* Info size includes itself */
        page->color_info.size = get_DWORD(&p) - 2;
        if (*len < (p - *buffer) + page->color_info.size) {
            g_warning("Color data truncated");
            goto FAIL;
        }

        p += page->color_info.size;
    }

    *len -= p - *buffer;
    *buffer = p;
    return page;

FAIL:
    rhk_sm3_page_free(page);
    return NULL;
}

static GwyDataField*
rhk_sm3_page_to_data_field(const RHKPage *page)
{
    GwyDataField *dfield;
    GwySIUnit *siunit;
    const gchar *unit;
    gint xres, yres, i;
    const gint32 *pdata;
    gdouble *data;

    xres = page->x_size;
    yres = page->y_size;
    dfield = gwy_data_field_new(xres, yres,
                                xres*fabs(page->x_scale),
                                yres*fabs(page->y_scale),
                                FALSE);
    data = gwy_data_field_get_data(dfield);
    pdata = (const gint32*)page->page_data;
    for (i = 0; i < xres*yres; i++)
        data[i] = GINT32_FROM_LE(pdata[i])*page->z_scale + page->z_offset;

    if (page->strings[RHK_STRING_X_UNITS]
        && page->strings[RHK_STRING_Y_UNITS]) {
        if (!gwy_strequal(page->strings[RHK_STRING_X_UNITS],
                          page->strings[RHK_STRING_Y_UNITS]))
            g_warning("X and Y units are different, using X");
        unit = page->strings[RHK_STRING_X_UNITS];
    }
    else if (page->strings[RHK_STRING_X_UNITS])
        unit = page->strings[RHK_STRING_X_UNITS];
    else if (page->strings[RHK_STRING_Y_UNITS])
        unit = page->strings[RHK_STRING_Y_UNITS];
    else
        unit = "";
    siunit = gwy_si_unit_new(unit);
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    if (page->strings[RHK_STRING_Z_UNITS])
        unit = page->strings[RHK_STRING_Z_UNITS];
    else
        unit = "";
    siunit = gwy_si_unit_new(unit);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    return dfield;
}

static GwyContainer*
rhk_sm3_load(const gchar *filename)
{
    GPtrArray *rhkfile;
    RHKPage *rhkpage;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    const guchar *p;
    guint i, count;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < HEADER_SIZE) {
        g_warning("File %s is not a RHK SM3 file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    rhkfile = g_ptr_array_new();

    p = buffer;
    count = 0;
    gwy_debug("position %04x", p - buffer);
    while ((rhkpage = rhk_sm3_read_page(&p, &size))) {
        gwy_debug("Page #%u read OK", count);
        count++;
        rhkpage->pageno = count;
        gwy_debug("position %04x", p - buffer);
        if (rhkpage->type != RHK_TYPE_IMAGE) {
            gwy_debug("Page is not IMAGE, skipping");
            rhk_sm3_page_free(rhkpage);
            continue;
        }
        g_ptr_array_add(rhkfile, rhkpage);
    }

    i = select_which_data(rhkfile);
    if (i != (guint)-1) {
        rhkpage = g_ptr_array_index(rhkfile, i);
        container = gwy_container_new();
        dfield = rhk_sm3_page_to_data_field(rhkpage);
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);
        rhk_sm3_store_metadata(rhkpage, container);
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    for (i = 0; i < rhkfile->len; i++)
        rhk_sm3_page_free(g_ptr_array_index(rhkfile, i));
    g_ptr_array_free(rhkfile, TRUE);

    return container;
}

static void
rhk_sm3_store_metadata(RHKPage *rhkpage,
                       GwyContainer *container)
{
    const gchar *s;
    gchar *str;
    guint i;

    s = gwy_enum_to_string(rhkpage->page_type,
                           page_types, G_N_ELEMENTS(page_types));
    if (s && *s)
        gwy_container_set_string_by_name(container, "/meta/Type",
                                         g_strdup(s));

    s = gwy_enum_to_string(rhkpage->scan_dir,
                           scan_directions, G_N_ELEMENTS(scan_directions));
    if (s && *s)
        gwy_container_set_string_by_name(container, "/meta/Scan Direction",
                                         g_strdup(s));

    s = gwy_enum_to_string(rhkpage->source_type,
                           page_sources, G_N_ELEMENTS(page_sources));
    if (s && *s)
        gwy_container_set_string_by_name(container, "/meta/Source",
                                         g_strdup(s));

    gwy_container_set_string_by_name(container, "/meta/Bias",
                                     g_strdup_printf("%g V", rhkpage->bias));
    gwy_container_set_string_by_name(container, "/meta/Rotation angle",
                                     g_strdup_printf("%f", rhkpage->angle));
    gwy_container_set_string_by_name(container, "/meta/Period",
                                     g_strdup_printf("%f s", rhkpage->period));

    s = rhkpage->strings[RHK_STRING_DATE];
    if (s && *s) {
        str = g_strconcat(s, " ", rhkpage->strings[RHK_STRING_TIME], NULL);
        gwy_container_set_string_by_name(container, "/meta/Date", str);
    }

    s = rhkpage->strings[RHK_STRING_LABEL];
    if (s && *s) {
        gwy_container_set_string_by_name(container, "/meta/Label", g_strdup(s));
        gwy_container_set_string_by_name(container, "/filename/title",
                                         g_strdup(s));
    }

    s = rhkpage->strings[RHK_STRING_PATH];
    if (s && *s)
        gwy_container_set_string_by_name(container, "/meta/Path", g_strdup(s));

    s = rhkpage->strings[RHK_STRING_SYSTEM_TEXT];
    if (s && *s)
        gwy_container_set_string_by_name(container, "/meta/System comment",
                                         g_strdup(s));

    s = rhkpage->strings[RHK_STRING_SESSION_TEXT];
    if (s && *s)
        gwy_container_set_string_by_name(container, "/meta/Session comment",
                                         g_strdup(s));

    s = rhkpage->strings[RHK_STRING_USER_TEXT];
    if (s && *s)
        gwy_container_set_string_by_name(container, "/meta/User comment",
                                         g_strdup(s));

    str = g_new(gchar, 33);
    for (i = 0; i < 16; i++) {
        static const gchar hex[] = "0123456789abcdef";

        str[2*i] = hex[rhkpage->page_id[i]/16];
        str[2*i + 1] = hex[rhkpage->page_id[i] % 16];
    }
    str[32] = '\0';
    gwy_container_set_string_by_name(container, "/meta/Page ID", str);
}

static guint
select_which_data(GPtrArray *rhkfile)
{
    RHKControls controls;
    RHKPage *rhkpage;
    GtkWidget *dialog, *label, *vbox, *hbox, *align;
    GwyDataField *dfield;
    GwyEnum *choices;
    GwyPixmapLayer *layer;
    GSList *radio, *rl;
    guint i, b = (guint)-1;
    const gchar *s;

    if (!rhkfile->len)
        return b;

    if (rhkfile->len == 1)
        return 0;

    controls.file = rhkfile;
    choices = g_new(GwyEnum, rhkfile->len);
    for (i = 0; i < rhkfile->len; i++) {
        rhkpage = g_ptr_array_index(rhkfile, i);
        choices[i].value = i;
        s = rhkpage->strings[RHK_STRING_LABEL];
        if (s && *s)
            choices[i].name = g_strdup_printf(_("Page %u (%s)"),
                                              rhkpage->pageno, s);
        else
            choices[i].name = g_strdup_printf(_("Page %u"), rhkpage->pageno);
    }
    rhkpage = g_ptr_array_index(rhkfile, 0);

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
    dfield = rhk_sm3_page_to_data_field(rhkpage);
    gwy_container_set_object_by_name(controls.data, "data", dfield);
    gwy_container_set_enum_by_name(controls.data, "range-type",
                                   GWY_LAYER_BASIC_RANGE_AUTO);
    g_object_unref(dfield);

    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.data_view),
                           120.0/MAX(rhkpage->x_size, rhkpage->y_size));
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
    rhkpage = g_ptr_array_index(controls->file, i);
    dfield = rhk_sm3_page_to_data_field(rhkpage);
    gwy_container_set_object_by_name(controls->data, "data", dfield);
    g_object_unref(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

