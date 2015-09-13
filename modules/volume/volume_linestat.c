/*
 *  @(#) $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/brick.h>
#include <libprocess/arithmetic.h>
#include <libprocess/linestats.h>
#include <libprocess/gwyprocesstypes.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-volume.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define LINE_STAT_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 360,
    /* 16 is good for current processors; increasing it to 32 might not
     * hurt in the future. */
    BLOCK_SIZE = 16,
};

enum {
    RESPONSE_RESET = 1,
};

typedef enum {
    OUTPUT_IMAGE = 0,
    OUTPUT_PREVIEW = 1,
    NOUTPUTS
} LineStatOutput;

typedef gdouble (*LineStatFunc)(GwyDataLine *dataline);

typedef struct {
    GwyLineStatQuantity quantity;
    LineStatOutput output_type;
    gint x;
    gint y;
    gint zfrom;
    gint zto;
    /* TODO: We need an instant update option! */
    /* Dynamic state. */
    GwyBrick *brick;
    GwyDataLine *calibration;
} LineStatArgs;

typedef struct {
    LineStatArgs *args;
    GwyContainer *mydata;
    GwyDataField *image;
    GtkWidget *dialog;
    GtkWidget *view;
    GwyPixmapLayer *player;
    GwyVectorLayer *vlayer;
    GtkWidget *graph;
    GtkWidget *quantity;
    GSList *output_type;
    GtkWidget *zfrom;
    GtkWidget *zto;
    GwySIValueFormat *zvf;
} LineStatControls;

typedef struct {
    GwyBrick *brick;
    const gdouble *db;
    GwyDataLine *dline;
    gdouble *buf;
    guint npts;
    guint npixels;
    guint k;
} LineStatIter;

static gboolean module_register        (void);
static void     line_stat              (GwyContainer *data,
                                        GwyRunType run);
static gboolean line_stat_dialog       (LineStatArgs *args,
                                        GwyContainer *data,
                                        gint id);
static void     line_stat_do           (LineStatArgs *args,
                                        GwyContainer *data,
                                        gint id);
static void     line_stat_reset        (LineStatControls *controls);
static void     point_selection_changed(LineStatControls *controls,
                                        gint id,
                                        GwySelection *selection);
static void     graph_selection_changed(LineStatControls *controls,
                                        gint id,
                                        GwySelection *selection);
static void     quantity_changed       (GtkComboBox *combo,
                                        LineStatControls *controls);
static void     output_type_changed    (GtkToggleButton *button,
                                        LineStatControls *controls);
static void     extract_summary_image  (const LineStatArgs *args,
                                        GwyDataField *dfield);
static void     extract_graph_curve    (const LineStatArgs *args,
                                        GwyGraphCurveModel *gcmodel);
static void     extract_gmodel         (const LineStatArgs *args,
                                        GwyGraphModel *gmodel);
static void     line_stat_sanitize_args(LineStatArgs *args);
static void     line_stat_load_args    (GwyContainer *container,
                                        LineStatArgs *args);
static void     line_stat_save_args    (GwyContainer *container,
                                        LineStatArgs *args);

/*
static void     zfrom_changed          (LineStatControls *controls,
                                        GtkWidget *entry);
static void     zto_changed            (LineStatControls *controls,
                                        GtkWidget *entry);
                                        */

static const LineStatArgs line_stat_defaults = {
    GWY_LINE_STAT_MEAN, OUTPUT_IMAGE,
    -1, -1, -1, -1,
    /* Dynamic state. */
    NULL, NULL,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Summarizes profiles of volume data to a channel."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_volume_func_register("volume_linestat",
                             (GwyVolumeFunc)&line_stat,
                             N_("/Summarize _Profiles..."),
                             NULL,
                             LINE_STAT_RUN_MODES,
                             GWY_MENU_FLAG_VOLUME,
                             N_("Summarize profiles"));

    return TRUE;
}

static void
line_stat(GwyContainer *data, GwyRunType run)
{
    LineStatArgs args;
    GwyBrick *brick = NULL;
    gint id;

    g_return_if_fail(run & LINE_STAT_RUN_MODES);
    g_return_if_fail(g_type_from_name("GwyLayerPoint"));

    line_stat_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_BRICK, &brick,
                                     GWY_APP_BRICK_ID, &id,
                                     0);
    g_return_if_fail(GWY_IS_BRICK(brick));
    args.brick = brick;

    args.calibration = gwy_brick_get_zcalibration(brick);
    if (args.calibration
        && (gwy_brick_get_zres(brick)
            != gwy_data_line_get_res(args.calibration)))
        args.calibration = NULL;

    if (CLAMP(args.x, 0, brick->xres-1) != args.x)
        args.x = brick->xres/2;
    if (CLAMP(args.y, 0, brick->yres-1) != args.y)
        args.y = brick->yres/2;
    if (CLAMP(args.zfrom, 0, brick->zres-1) != args.zfrom)
        args.zfrom = 0;
    if (CLAMP(args.zto, 0, brick->zres-1) != args.zto)
        args.zto = brick->zres;

    if (line_stat_dialog(&args, data, id))
        line_stat_do(&args, data, id);

    line_stat_save_args(gwy_app_settings_get(), &args);
}

static gboolean
line_stat_dialog(LineStatArgs *args, GwyContainer *data, gint id)
{
    /* XXX: This is identical to tools/linestat.c. */
    static const GwyEnum quantities[] =  {
        { N_("Mean"),               GWY_LINE_STAT_MEAN,      },
        { N_("Median"),             GWY_LINE_STAT_MEDIAN,    },
        { N_("Minimum"),            GWY_LINE_STAT_MINIMUM,   },
        { N_("Maximum"),            GWY_LINE_STAT_MAXIMUM,   },
        { N_("Range"),              GWY_LINE_STAT_RANGE,     },
        { N_("Length"),             GWY_LINE_STAT_LENGTH,    },
        { N_("Slope"),              GWY_LINE_STAT_SLOPE,     },
        { N_("tan β<sub>0</sub>"),  GWY_LINE_STAT_TAN_BETA0, },
        { N_("Variation"),          GWY_LINE_STAT_VARIATION, },
        { N_("Ra"),                 GWY_LINE_STAT_RA,        },
        { N_("Rq (RMS)"),           GWY_LINE_STAT_RMS,       },
        { N_("Rz"),                 GWY_LINE_STAT_RZ,        },
        { N_("Rt"),                 GWY_LINE_STAT_RT,        },
        { N_("Skew"),               GWY_LINE_STAT_SKEW,      },
        { N_("Kurtosis"),           GWY_LINE_STAT_KURTOSIS,  },
    };

    static const GwyEnum output_types[] = {
        { N_("_Extract image"), OUTPUT_IMAGE, },
        { N_("Set _preview"),   OUTPUT_PREVIEW, },
    };

    GtkWidget *dialog, *table, *hbox, *label, *area;
    LineStatControls controls;
    GwyDataField *dfield;
    GwyGraphCurveModel *gcmodel;
    GwyGraphModel *gmodel;
    gint response, row;
    GwyPixmapLayer *layer;
    GwyVectorLayer *vlayer = NULL;
    GwySelection *selection;
    const guchar *gradient;
    gchar key[40];
    gdouble xy[2];

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Summarize Volume Profiles"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_volume_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    controls.image = dfield = gwy_data_field_new(1, 1, 1.0, 1.0, TRUE);
    extract_summary_image(args, dfield);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    g_object_unref(dfield);

    g_snprintf(key, sizeof(key), "/brick/%d/preview/palette", id);
    if (gwy_container_gis_string_by_name(data, key, &gradient)) {
        gwy_container_set_const_string_by_name(controls.mydata,
                                               "/0/base/palette", gradient);
    }

    controls.view = gwy_data_view_new(controls.mydata);
    controls.player = layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);
    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 0);

    controls.vlayer = vlayer = g_object_new(g_type_from_name("GwyLayerPoint"),
                                            NULL);
    gwy_vector_layer_set_selection_key(vlayer, "/0/select/pointer");
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view), vlayer);
    selection = gwy_vector_layer_ensure_selection(vlayer);
    gwy_selection_set_max_objects(selection, 1);
    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(point_selection_changed), &controls);

    gmodel = gwy_graph_model_new();
    g_object_set(gmodel, "label-visible", FALSE, NULL);
    extract_gmodel(args, gmodel);
    gcmodel = gwy_graph_curve_model_new();
    gwy_graph_model_add_curve(gmodel, gcmodel);
    extract_graph_curve(args, gcmodel);
    g_object_unref(gcmodel);

    controls.graph = gwy_graph_new(gmodel);
    gwy_graph_set_axis_visible(GWY_GRAPH(controls.graph), GTK_POS_LEFT,
                               FALSE);
    gwy_graph_set_axis_visible(GWY_GRAPH(controls.graph), GTK_POS_BOTTOM,
                               FALSE);
    gtk_widget_set_size_request(controls.graph, PREVIEW_SIZE, PREVIEW_SIZE);
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, TRUE, TRUE, 0);

    area = gwy_graph_get_area(GWY_GRAPH(controls.graph));
    gwy_graph_area_set_status(GWY_GRAPH_AREA(area), GWY_GRAPH_STATUS_XSEL);
    selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                             GWY_GRAPH_STATUS_XSEL);
    gwy_selection_set_max_objects(selection, 1);
    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(graph_selection_changed), &controls);

    hbox = gtk_hbox_new(FALSE, 24);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 4);

    table = gtk_table_new(4, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new_with_mnemonic(_("_Quantity:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    controls.quantity
        = gwy_enum_combo_box_new(quantities, G_N_ELEMENTS(quantities),
                                 G_CALLBACK(quantity_changed), &controls,
                                 args->quantity, TRUE);
    gtk_table_attach(GTK_TABLE(table), controls.quantity,
                     1, 2, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.quantity);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gtk_label_new(_("Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.output_type
        = gwy_radio_buttons_create(output_types, G_N_ELEMENTS(output_types),
                                   G_CALLBACK(output_type_changed), &controls,
                                   args->output_type);
    row = gwy_radio_buttons_attach_to_table(controls.output_type,
                                            GTK_TABLE(table), 2, row);

    /* TODO: Numeric control of the graph selection. */
    selection = gwy_vector_layer_ensure_selection(vlayer);
    xy[0] = gwy_brick_itor(args->brick, args->x);
    xy[1] = gwy_brick_jtor(args->brick, args->y);
    gwy_selection_set_object(selection, 0, xy);

    selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                             GWY_GRAPH_STATUS_XSEL);
    if (args->zfrom > 0 || args->zto < args->brick->zres-1) {
        if (args->calibration) {
            xy[0] = args->calibration->data[args->zfrom];
            xy[1] = args->calibration->data[MIN(args->zto,
                                                args->brick->zres-1)];
        }
        else {
            xy[0] = gwy_brick_itor(args->brick, args->zfrom);
            xy[1] = gwy_brick_itor(args->brick, args->zto);
        }
        gwy_selection_set_object(selection, 0, xy);
    }
    else
        gwy_selection_clear(selection);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            //gwy_si_unit_value_format_free(controls.zvf);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            line_stat_reset(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    g_object_unref(controls.mydata);
    //gwy_si_unit_value_format_free(controls.zvf);

    return TRUE;
}

static void
point_selection_changed(LineStatControls *controls,
                        G_GNUC_UNUSED gint id,
                        GwySelection *selection)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    LineStatArgs *args = controls->args;
    GwyBrick *brick = args->brick;
    gdouble xy[2];

    if (!gwy_selection_get_object(selection, 0, xy))
        return;

    args->x = CLAMP(gwy_brick_rtoi(brick, xy[0]), 0, brick->xres-1);
    args->y = CLAMP(gwy_brick_rtoj(brick, xy[1]), 0, brick->yres-1);

    gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
    gcmodel = gwy_graph_model_get_curve(gmodel, 0);
    extract_graph_curve(args, gcmodel);
}

static void
graph_selection_changed(LineStatControls *controls,
                        G_GNUC_UNUSED gint id,
                        GwySelection *selection)
{
    LineStatArgs *args = controls->args;
    GwyBrick *brick = args->brick;
    gdouble z[2];

    if (!gwy_selection_get_object(selection, 0, z)) {
        args->zfrom = args->zto = -1;
    }
    else {
        args->zfrom = CLAMP(gwy_brick_rtoi(brick, z[0]), 0, brick->zres);
        args->zto = CLAMP(gwy_brick_rtoi(brick, z[1])+1, 0, brick->zres);
        if (args->zto < args->zfrom)
            GWY_SWAP(gint, args->zfrom, args->zto);
        if (args->zto - args->zfrom < 2)
            args->zfrom = args->zto = -1;
    }
    extract_summary_image(controls->args, controls->image);
    gwy_data_field_data_changed(controls->image);
}

static void
quantity_changed(GtkComboBox *combo, LineStatControls *controls)
{
    LineStatArgs *args = controls->args;

    args->quantity = gwy_enum_combo_box_get_active(combo);
    extract_summary_image(controls->args, controls->image);
    gwy_data_field_data_changed(controls->image);
}

static void
output_type_changed(GtkToggleButton *button, LineStatControls *controls)
{
    LineStatArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(button))
        return;

    args->output_type = gwy_radio_buttons_get_current(controls->output_type);
}

static gdouble
get_data_line_range(GwyDataLine *dataline)
{
    gdouble min, max;

    gwy_data_line_get_min_max(dataline, &min, &max);
    return max - min;
}

static void
line_stat_iter_init(LineStatIter *iter, GwyBrick *brick,
                    gint zfrom, gint zto)
{
    gwy_clear(iter, 1);
    iter->brick = brick;
    iter->npts = zto - zfrom;
    iter->npixels = brick->xres * brick->yres;
    iter->db = gwy_brick_get_data_const(brick) + zfrom*iter->npixels;
    iter->buf = g_new(gdouble, iter->npixels * iter->npts);
    iter->dline = gwy_data_line_new(1, 1.0, FALSE);
    iter->k = (guint)(-1);
    /* Sets up line properties. */
    gwy_brick_extract_line(brick, iter->dline, 0, 0, zfrom, 0, 0, zto, TRUE);
}

static void
line_stat_iter_next(LineStatIter *iter)
{
    guint blocksize, npts, npixels, kk, m;

    npts = iter->npts;
    npixels = iter->npixels;
    iter->k++;
    g_return_if_fail(iter->k < npixels);

    kk = iter->k % BLOCK_SIZE;
    if (!kk) {
        blocksize = MIN(BLOCK_SIZE, npixels - iter->k);
        for (m = 0; m < npts; m++) {
            const gdouble *db = iter->db + m*npixels + iter->k;
            for (kk = 0; kk < blocksize; kk++)
                iter->buf[kk*npts + m] = db[kk];
        }
        kk = 0;
    }
    memcpy(iter->dline->data, iter->buf + kk*npts, npts * sizeof(gdouble));
}

static void
line_stat_iter_free(LineStatIter *iter)
{
    g_free(iter->buf);
    gwy_object_unref(iter->dline);
}

static void
extract_summary_image(const LineStatArgs *args, GwyDataField *dfield)
{
    static const struct {
        GwyLineStatQuantity quantity;
        LineStatFunc func;
    }
    line_stat_funcs[] = {
        { GWY_LINE_STAT_MEAN,      gwy_data_line_get_avg,       },
        { GWY_LINE_STAT_MEDIAN,    gwy_data_line_get_median,    },
        { GWY_LINE_STAT_MINIMUM,   gwy_data_line_get_min,       },
        { GWY_LINE_STAT_MAXIMUM,   gwy_data_line_get_max,       },
        { GWY_LINE_STAT_RANGE,     get_data_line_range,         },
        { GWY_LINE_STAT_LENGTH,    gwy_data_line_get_length,    },
        { GWY_LINE_STAT_TAN_BETA0, gwy_data_line_get_tan_beta0, },
        { GWY_LINE_STAT_VARIATION, gwy_data_line_get_variation, },
        { GWY_LINE_STAT_RMS,       gwy_data_line_get_rms,       },
    };
    /* TODO:
        { GWY_LINE_STAT_SLOPE,    },
        { GWY_LINE_STAT_RA,        },
        { GWY_LINE_STAT_RZ,        },
        { GWY_LINE_STAT_RT,        },
        { GWY_LINE_STAT_SKEW,      },
        { GWY_LINE_STAT_KURTOSIS,  },
    */

    GwyLineStatQuantity quantity = args->quantity;
    GwyBrick *brick = args->brick;
    gint xres = brick->xres, yres = brick->yres;
    gint zfrom = args->zfrom, zto = args->zto;
    LineStatIter iter;
    LineStatFunc lsfunc = NULL;
    gint i;
    guint k;

    if (zfrom == -1 && zto == -1) {
        zfrom = 0;
        zto = brick->zres;
    }

    /* Quantities we handle (somewhat inefficiently) by using DataLine
     * statistics. */
    for (k = 0; k < G_N_ELEMENTS(line_stat_funcs); k++) {
        if (quantity == line_stat_funcs[k].quantity) {
            lsfunc = line_stat_funcs[k].func;
            break;
        }
    }

    gwy_brick_extract_plane(brick, dfield, 0, 0, 0, xres, yres, -1, FALSE);
    if (!lsfunc) {
        g_warning("Quantity %u is still unimplemented.", quantity);
        gwy_data_field_clear(dfield);
        return;
    }

    /* Use an iterator interface to formally process data profile by profle,
     * but physically extract them from the brick by larger blocks, gaining a
     * speedup about 3 from the much improved memory access pattern. */
    line_stat_iter_init(&iter, brick, zfrom, zto);
    for (i = 0; i < xres*yres; i++) {
        line_stat_iter_next(&iter);
        dfield->data[i] = lsfunc(iter.dline);
    }
    line_stat_iter_free(&iter);

    gwy_data_field_invalidate(dfield);
    /* TODO: Fix value units that are not set up correctly by the initial
     * gwy_brick_extract_plane(). */
}

static void
extract_graph_curve(const LineStatArgs *args,
                    GwyGraphCurveModel *gcmodel)
{
    GwyDataLine *line = gwy_data_line_new(1, 1.0, FALSE);
    GwyBrick *brick = args->brick;

    gwy_brick_extract_line(brick, line,
                           args->x, args->y, 0,
                           args->x, args->y, brick->zres,
                           FALSE);
    g_object_set(gcmodel, "mode", GWY_GRAPH_CURVE_LINE, NULL);

    if (args->calibration) {
        gwy_graph_curve_model_set_data(gcmodel,
                                       gwy_data_line_get_data(args->calibration),
                                       gwy_data_line_get_data(line),
                                       gwy_data_line_get_res(line));
    }
    else
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, line, 0, 0);

    g_object_unref(line);
}

static void
extract_gmodel(const LineStatArgs *args, GwyGraphModel *gmodel)
{
    GwyBrick *brick = args->brick;
    GwySIUnit *xunit = NULL, *yunit;

    if (args->calibration)
        xunit = gwy_data_line_get_si_unit_x(args->calibration);
    else
        xunit = gwy_brick_get_si_unit_z(brick);
    xunit = gwy_si_unit_duplicate(xunit);
    yunit = gwy_si_unit_duplicate(gwy_brick_get_si_unit_w(brick));

    g_object_set(gmodel,
                 "si-unit-x", xunit,
                 "si-unit-y", yunit,
                 NULL);
    g_object_unref(xunit);
    g_object_unref(yunit);
}

static void
line_stat_reset(LineStatControls *controls)
{
    GwyBrick *brick = controls->args->brick;
    GtkWidget *area;
    GwySelection *selection;
    gdouble xy[2];

    xy[0] = 0.5*brick->xreal;
    xy[1] = 0.5*brick->yreal;
    selection = gwy_vector_layer_ensure_selection(controls->vlayer);
    gwy_selection_set_object(selection, 0, xy);

    area = gwy_graph_get_area(GWY_GRAPH(controls->graph));
    selection = gwy_graph_area_get_selection(GWY_GRAPH_AREA(area),
                                             GWY_GRAPH_STATUS_XSEL);
    gwy_selection_clear(selection);
}

static void
line_stat_do(LineStatArgs *args,
             GwyContainer *data,
             gint id)
{
}

static const gchar output_type_key[] = "/module/volume_line_stat/output_type";
static const gchar quantity_key[]    = "/module/volume_line_stat/quantity";
static const gchar xpos_key[]        = "/module/volume_line_stat/xpos";
static const gchar ypos_key[]        = "/module/volume_line_stat/ypos";
static const gchar zfrom_key[]       = "/module/volume_line_stat/zfrom";
static const gchar zto_key[]         = "/module/volume_line_stat/zto";

static void
line_stat_sanitize_args(LineStatArgs *args)
{
    /* Positions are validated against the brick. */
    args->quantity = gwy_enum_sanitize_value(args->quantity,
                                             GWY_TYPE_LINE_STAT_QUANTITY);
    args->output_type = MIN(args->output_type, NOUTPUTS-1);
}

static void
line_stat_load_args(GwyContainer *container,
                LineStatArgs *args)
{
    *args = line_stat_defaults;

    gwy_container_gis_enum_by_name(container, quantity_key, &args->quantity);
    gwy_container_gis_enum_by_name(container, output_type_key,
                                   &args->output_type);
    gwy_container_gis_int32_by_name(container, xpos_key, &args->x);
    gwy_container_gis_int32_by_name(container, ypos_key, &args->y);
    gwy_container_gis_int32_by_name(container, zfrom_key, &args->zfrom);
    gwy_container_gis_int32_by_name(container, zto_key, &args->zto);
    line_stat_sanitize_args(args);
}

static void
line_stat_save_args(GwyContainer *container,
                LineStatArgs *args)
{
    gwy_container_set_enum_by_name(container, quantity_key, args->quantity);
    gwy_container_set_enum_by_name(container, output_type_key,
                                   args->output_type);
    gwy_container_set_int32_by_name(container, xpos_key, args->x);
    gwy_container_set_int32_by_name(container, ypos_key, args->y);
    gwy_container_set_int32_by_name(container, zfrom_key, args->zfrom);
    gwy_container_set_int32_by_name(container, zto_key, args->zto);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
