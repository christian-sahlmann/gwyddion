/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include "convolutionfilterpreset.h"

#define CONVOLUTION_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 320
};

typedef enum {
    CONVOLUTION_FILTER_SYMMETRY_NONE,
    CONVOLUTION_FILTER_SYMMETRY_ODD,
    CONVOLUTION_FILTER_SYMMETRY_EVEN
} ConvolutionFilterSymmetryType;

typedef struct {
    GwyConvolutionFilterPreset *preset;
    ConvolutionFilterSymmetryType hsym;
    ConvolutionFilterSymmetryType vsym;
    gboolean computed;
} ConvolutionArgs;

typedef struct {
    ConvolutionArgs *args;
    GwyContainer *mydata;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *matrix;
    GtkWidget **coeff;
    gboolean in_update;
} ConvolutionControls;

static gboolean module_register                  (void);
static void     convolution_filter               (GwyContainer *data,
                                                  GwyRunType run);
static void     convolution_filter_dialog        (ConvolutionArgs *args,
                                                  GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  gint id,
                                                  GQuark dquark);
static void     convolution_filter_find_symmetry (ConvolutionArgs *args);
static void     convolution_filter_set_value     (ConvolutionControls *controls,
                                                  guint j,
                                                  guint i,
                                                  gdouble val);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generic convolution filter with user-defined matrix."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    static gint types_initialized = 0;
    GwyResourceClass *klass;

    if (!types_initialized) {
        types_initialized += gwy_convolution_filter_preset_get_type();
        klass = g_type_class_ref(GWY_TYPE_CONVOLUTION_FILTER_PRESET);
        gwy_resource_class_load(klass);
        g_type_class_unref(klass);
    }

    gwy_process_func_register("convolution_filter",
                              (GwyProcessFunc)&convolution_filter,
                              N_("/_Basic Operations/Convolution _Filter..."),
                              NULL,
                              CONVOLUTION_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Generic convolution filter"));

    return TRUE;
}

static void
convolution_filter(GwyContainer *data,
                   GwyRunType run)
{
    ConvolutionArgs args;
    GwyResourceClass *rklass;
    GwyDataField *dfield;
    GQuark dquark;
    gint id;

    g_return_if_fail(run & CONVOLUTION_RUN_MODES);

    rklass = g_type_class_ref(GWY_TYPE_CONVOLUTION_FILTER_PRESET);
    gwy_resource_class_mkdir(rklass);
    g_type_class_unref(rklass);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && dquark);

    /*
    gwy_app_undo_qcheckpointv(data, 1, &dquark);
    gwy_data_field_data_changed(dfield);
    */

    args.preset
        = gwy_convolution_filter_preset_new("Foo",
                                            &convolutionpresetdata_default,
                                            FALSE);
    convolution_filter_find_symmetry(&args);
    convolution_filter_dialog(&args, data, dfield, id, dquark);
}

static void
convolution_filter_dialog(ConvolutionArgs *args,
                          GwyContainer *data,
                          GwyDataField *dfield,
                          gint id,
                          GQuark dquark)
{
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PREVIEW = 2
    };
    ConvolutionControls controls;
    GwyPixmapLayer *layer;
    GtkWidget *dialog, *hbox, *vbox, *table, *notebook, *label;
    gdouble zoomval;
    gint response;

    controls.args = args;
    args->computed = FALSE;

    dialog = gtk_dialog_new_with_buttons(_("Convolution Filter"), NULL, 0,
                                         _("_Update"), RESPONSE_PREVIEW,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(hbox), notebook, FALSE, FALSE, 4);

    /* Filter */
    vbox = gtk_vbox_new(FALSE, 8);
    label = gtk_label_new(_("Filter"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

    /* Presets */
    vbox = gtk_vbox_new(FALSE, 8);
    label = gtk_label_new(_("Presets"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            /* mark_dialog_update_values(&controls, args); */
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            /* TODO
            mark_dialog_update_controls(&controls, args);
            preview(&controls, args);
            */
            break;

            case RESPONSE_PREVIEW:
            /*
            mark_dialog_update_values(&controls, args);
            preview(&controls, args);
            */
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    if (args->computed) {
        dfield = gwy_container_get_object_by_name(controls.mydata, "/0/data");
        gwy_app_undo_qcheckpointv(data, 1, &dquark);
        gwy_container_set_object(data, dquark, dfield);
        g_object_unref(controls.mydata);
    }
    else {
        g_object_unref(controls.mydata);
        /* TODO
        run_noninteractive(args, data, dfield, mquark);
        */
    }
}

static void
convolution_filter_find_symmetry(ConvolutionArgs *args)
{
    const gdouble *matrix;
    guint size, i, j;
    guint hpossible, vpossible;

    matrix = args->preset->data.matrix;
    size = args->preset->data.size;
    hpossible = vpossible = (CONVOLUTION_FILTER_SYMMETRY_EVEN
                             | CONVOLUTION_FILTER_SYMMETRY_ODD
                             | CONVOLUTION_FILTER_SYMMETRY_NONE);

    for (i = 0; i < size/2; i++) {
        for (j = 0; j < size/2; j++) {
            if (matrix[i*size + j] != matrix[i*size + size-1-j])
                hpossible &= ~(1U << CONVOLUTION_FILTER_SYMMETRY_EVEN);
            if (matrix[i*size + j] != -matrix[i*size + size-1-j])
                hpossible &= ~(1U << CONVOLUTION_FILTER_SYMMETRY_ODD);
            if (matrix[i*size + j] != matrix[(size-j-i)*size + j])
                vpossible &= ~(1U << CONVOLUTION_FILTER_SYMMETRY_EVEN);
            if (matrix[i*size + j] != -matrix[(size-j-i)*size + j])
                vpossible &= ~(1U << CONVOLUTION_FILTER_SYMMETRY_ODD);
        }
    }

    if (hpossible & CONVOLUTION_FILTER_SYMMETRY_EVEN)
        args->hsym = CONVOLUTION_FILTER_SYMMETRY_EVEN;
    else if (hpossible & CONVOLUTION_FILTER_SYMMETRY_ODD)
        args->hsym = CONVOLUTION_FILTER_SYMMETRY_ODD;
    else
        args->hsym = CONVOLUTION_FILTER_SYMMETRY_NONE;

    if (vpossible & CONVOLUTION_FILTER_SYMMETRY_EVEN)
        args->vsym = CONVOLUTION_FILTER_SYMMETRY_EVEN;
    else if (vpossible & CONVOLUTION_FILTER_SYMMETRY_ODD)
        args->vsym = CONVOLUTION_FILTER_SYMMETRY_ODD;
    else
        args->vsym = CONVOLUTION_FILTER_SYMMETRY_NONE;
}

static void
convolution_filter_do_set_value(ConvolutionControls *controls,
                                guint j,
                                guint i,
                                gdouble val)
{
    gchar buf[16];
    GwyConvolutionFilterPresetData *pdata;

    pdata = &controls->args->preset->data;
    g_return_if_fail(i < pdata->size);
    g_return_if_fail(j < pdata->size);

    pdata->matrix[i*pdata->size + j] = val;
    g_snprintf(buf, sizeof(buf), "%.8g", val);
    gtk_entry_set_text(GTK_ENTRY(controls->coeff[i*pdata->size + j]), buf);
}

static void
convolution_filter_set_value(ConvolutionControls *controls,
                             guint j,
                             guint i,
                             gdouble val)
{
    guint size;

    size = controls->args->preset->data.size;

    convolution_filter_do_set_value(controls, j, i, val);
    if (controls->args->hsym == CONVOLUTION_FILTER_SYMMETRY_EVEN) {
        convolution_filter_do_set_value(controls, size-1-j, i, val);
        if (controls->args->vsym == CONVOLUTION_FILTER_SYMMETRY_EVEN) {
            convolution_filter_do_set_value(controls, j, size-1-i, val);
            convolution_filter_do_set_value(controls, size-1-j, size-1-i, val);
        }
        else if (controls->args->vsym == CONVOLUTION_FILTER_SYMMETRY_ODD) {
            convolution_filter_do_set_value(controls, j, size-1-i, -val);
            convolution_filter_do_set_value(controls, size-1-j, size-1-i, -val);
        }
    }
    else if (controls->args->hsym == CONVOLUTION_FILTER_SYMMETRY_ODD) {
        convolution_filter_do_set_value(controls, size-1-j, i, -val);
        if (controls->args->vsym == CONVOLUTION_FILTER_SYMMETRY_EVEN) {
            convolution_filter_do_set_value(controls, j, size-1-i, val);
            convolution_filter_do_set_value(controls, size-1-j, size-1-i, -val);
        }
        else if (controls->args->vsym == CONVOLUTION_FILTER_SYMMETRY_ODD) {
            convolution_filter_do_set_value(controls, j, size-1-i, -val);
            convolution_filter_do_set_value(controls, size-1-j, size-1-i, val);
        }
    }
    else {
        if (controls->args->vsym == CONVOLUTION_FILTER_SYMMETRY_EVEN)
            convolution_filter_do_set_value(controls, j, size-1-i, val);
        else if (controls->args->vsym == CONVOLUTION_FILTER_SYMMETRY_ODD)
            convolution_filter_do_set_value(controls, j, size-1-i, -val);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
