/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define MASK_GROW_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function. */
typedef struct {
    gint pixels;
} MaskGrowArgs;

typedef struct {
    GtkObject *pixels;
} MaskGrowControls;

static gboolean    module_register            (const gchar *name);
static gboolean    mask_grow                  (GwyContainer *data,
                                               GwyRunType run,
                                               const gchar *name);
static gboolean    mask_grow_dialog           (MaskGrowArgs *args,
                                               const gchar *title,
                                               const gchar *desc);
static void        mask_grow_do               (GwyDataField *dfield,
                                               gint by);
static void        mask_shrink_do             (GwyDataField *dfield,
                                               gint by);
static void        mask_grow_load_args        (GwyContainer *container,
                                               MaskGrowArgs *args,
                                               const gchar *name);
static void        mask_grow_save_args        (GwyContainer *container,
                                               MaskGrowArgs *args,
                                               const gchar *name);

MaskGrowArgs mask_grow_defaults = {
    1,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "mask_grow_shrink",
    "Grows and shrinks masks.",
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo mask_grow_func_info = {
        "mask_grow",
        "/_Mask Operations/_Grow Mask...",
        (GwyProcessFunc)&mask_grow,
        MASK_GROW_RUN_MODES,
        GWY_MENU_FLAG_DATA_MASK,
    };
    static GwyProcessFuncInfo mask_shrink_func_info = {
        "mask_shrink",
        "/_Mask Operations/_Shrink Mask...",
        (GwyProcessFunc)&mask_grow,
        MASK_GROW_RUN_MODES,
        GWY_MENU_FLAG_DATA_MASK,
    };

    gwy_process_func_register(name, &mask_shrink_func_info);
    gwy_process_func_register(name, &mask_grow_func_info);

    return TRUE;
}

static gboolean
mask_grow(GwyContainer *data, GwyRunType run, const gchar *name)
{
    static struct {
        const gchar *name;
        const gchar *title;
        const gchar *desc;
        void (*func)(GwyDataField*, gint);
    }
    const grow_shrink_meta[] = {
        { "mask_grow",   "Grow Mask",   "Grow mask by",   &mask_grow_do },
        { "mask_shrink", "Shrink Mask", "Shrink mask by", &mask_shrink_do },
    };
    GwyContainer *settings;
    GwyDataField *dfield;
    MaskGrowArgs args;
    gboolean ok;
    gsize i;

    g_return_val_if_fail(run & MASK_GROW_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/mask"));
    g_return_val_if_fail(dfield, FALSE);

    for (i = 0; i < G_N_ELEMENTS(grow_shrink_meta); i++) {
        if (!strcmp(grow_shrink_meta[i].name, name))
            break;
    }
    g_return_val_if_fail(i < G_N_ELEMENTS(grow_shrink_meta), FALSE);

    settings = gwy_app_settings_get();
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = mask_grow_defaults;
    else
        mask_grow_load_args(settings, &args, name);
    ok = (run != GWY_RUN_MODAL) || mask_grow_dialog(&args,
                                                    grow_shrink_meta[i].title,
                                                    grow_shrink_meta[i].desc);
    if (run == GWY_RUN_MODAL)
        mask_grow_save_args(settings, &args, name);
    if (!ok)
        return FALSE;

    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    grow_shrink_meta[i].func(dfield, args.pixels);

    return TRUE;
}

static gboolean
mask_grow_dialog(MaskGrowArgs *args,
                 const gchar *title,
                 const gchar *desc)
{
    GtkWidget *dialog, *table, *spin;
    MaskGrowControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(title,
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    table = gtk_table_new(1, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.pixels = gtk_adjustment_new(args->pixels, 1, 1024, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 0, desc, _("samples"),
                                       controls.pixels);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->pixels
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.pixels));
            args->pixels = CLAMP(args->pixels, 1, 1024);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = mask_grow_defaults;
            gtk_adjustment_set_value(GTK_ADJUSTMENT(controls.pixels),
                                     args->pixels);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->pixels = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.pixels));
    args->pixels = CLAMP(args->pixels, 1, 1024);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
mask_grow_do(GwyDataField *dfield,
             gint by)
{
    gdouble *data, *buffer, *prow;
    gdouble min, q1, q2;
    gint xres, yres, rowstride;
    gint i, j, iter;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);
    rowstride = xres;

    buffer = g_new(gdouble, xres);
    prow = g_new(gdouble, xres);
    for (iter = 0; iter < by; iter++) {
        min = G_MAXDOUBLE;
        for (j = 0; j < xres; j++)
            prow[j] = -G_MAXDOUBLE;
        memcpy(buffer, data, xres*sizeof(gdouble));
        for (i = 0; i < yres; i++) {
            gdouble *row = data + i*xres;

            if (i == yres-1)
                rowstride = 0;

            j = 0;
            q2 = MAX(buffer[j], buffer[j+1]);
            q1 = MAX(prow[j], row[j+rowstride]);
            row[j] = MAX(q1, q2);
            min = MIN(min, row[j]);
            for (j = 1; j < xres-1; j++) {
                q1 = MAX(prow[j], buffer[j-1]);
                q2 = MAX(buffer[j], buffer[j+1]);
                q2 = MAX(q2, row[j+rowstride]);
                row[j] = MAX(q1, q2);
                min = MIN(min, row[j]);
            }
            j = xres-1;
            q2 = MAX(buffer[j-1], buffer[j]);
            q1 = MAX(prow[j], row[j+rowstride]);
            row[j] = MAX(q1, q2);
            min = MIN(min, row[j]);

            GWY_SWAP(gdouble*, prow, buffer);
            if (i < yres-1)
                memcpy(buffer, data + (i+1)*xres, xres*sizeof(gdouble));
        }
        if (min >= 1.0)
            break;
    }
    g_free(buffer);
    g_free(prow);
}

static void
mask_shrink_do(GwyDataField *dfield,
               gint by)
{
    gdouble *data, *buffer, *prow;
    gdouble q1, q2, max;
    gint xres, yres, rowstride;
    gint i, j, iter;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);
    rowstride = xres;

    buffer = g_new(gdouble, xres);
    prow = g_new(gdouble, xres);
    for (iter = 0; iter < by; iter++) {
        max = -G_MAXDOUBLE;
        for (j = 0; j < xres; j++)
            prow[j] = G_MAXDOUBLE;
        memcpy(buffer, data, xres*sizeof(gdouble));
        for (i = 0; i < yres; i++) {
            gdouble *row = data + i*xres;

            if (i == yres-1)
                rowstride = 0;

            j = 0;
            q2 = MIN(buffer[j], buffer[j+1]);
            q1 = MIN(prow[j], row[j+rowstride]);
            row[j] = MIN(q1, q2);
            max = MAX(max, row[j]);
            for (j = 1; j < xres-1; j++) {
                q1 = MIN(prow[j], buffer[j-1]);
                q2 = MIN(buffer[j], buffer[j+1]);
                q2 = MIN(q2, row[j+rowstride]);
                row[j] = MIN(q1, q2);
                max = MAX(max, row[j]);
            }
            j = xres-1;
            q2 = MIN(buffer[j-1], buffer[j]);
            q1 = MIN(prow[j], row[j+rowstride]);
            row[j] = MIN(q1, q2);
            max = MAX(max, row[j]);

            GWY_SWAP(gdouble*, prow, buffer);
            if (i < yres-1)
                memcpy(buffer, data + (i+1)*xres, xres*sizeof(gdouble));
        }
        if (max <= 0.0)
            break;
    }
    g_free(buffer);
    g_free(prow);
}

static const gchar *pixels_key = "/module/%s/pixels";

static void
mask_grow_load_args(GwyContainer *container,
                    MaskGrowArgs *args,
                    const gchar *name)
{
    gchar *s;

    *args = mask_grow_defaults;

    s = g_strdup_printf(pixels_key, name);
    gwy_container_gis_int32_by_name(container, pixels_key, &args->pixels);
    args->pixels = CLAMP(args->pixels, 1, 1024);
    g_free(s);
}

static void
mask_grow_save_args(GwyContainer *container,
                    MaskGrowArgs *args,
                    const gchar *name)
{
    gchar *s;

    s = g_strdup_printf(pixels_key, name);
    gwy_container_set_int32_by_name(container, pixels_key, args->pixels);
    g_free(s);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
