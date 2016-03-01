/*
 *  $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libprocess/grains.h>
#include <libprocess/triangulation.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-xyz.h>
#include <app/gwyapp.h>

#define XYZRAS_RUN_MODES (GWY_RUN_INTERACTIVE | GWY_RUN_IMMEDIATE)

#define EPSREL 1e-8

/* Use smaller cell sides than the triangulation algorithm as we only need them
 * for identical point detection and border extension. */
#define CELL_SIDE 1.6

enum {
    PREVIEW_SIZE = 400,
    UNDEF = G_MAXUINT
};

enum {
    GWY_INTERPOLATION_FIELD = -1,
    GWY_INTERPOLATION_AVERAGE = -2,
};

typedef struct {
    /* XXX: Not all values of interpolation and exterior are possible. */
    GwyInterpolationType interpolation;
    GwyExteriorType exterior;
    gint xres;
    gint yres;
    /* Interface only. */
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;
} XYZRasArgs;

typedef struct {
    GwySurface *surface;
    GwyTriangulation *triangulation;
    GArray *points;
    guint norigpoints;
    guint nbasepoints;
    gdouble step;
    gdouble xymag;
} XYZRasData;

typedef struct {
    XYZRasArgs *args;
    XYZRasData *rdata;
    GwyContainer *mydata;
    GtkWidget *dialog;
    GtkWidget *xmin;
    GtkWidget *xmax;
    GtkWidget *ymin;
    GtkWidget *ymax;
    GtkObject *xres;
    GtkObject *yres;
    GtkWidget *interpolation;
    GtkWidget *exterior;
    GtkWidget *view;
    GtkWidget *do_preview;
    GtkWidget *error;
    gboolean in_update;
} XYZRasControls;

typedef struct {
    guint *id;
    guint pos;
    guint len;
    guint size;
} WorkQueue;

static gboolean      module_register        (void);
static void          xyzras                 (GwyContainer *data,
                                             GwyRunType run);
static gboolean      xyzras_dialog          (XYZRasArgs *arg,
                                             XYZRasData *rdata,
                                             GwyContainer *data,
                                             gint id);
static gint          construct_resolutions  (XYZRasControls *controls,
                                             GtkTable *table,
                                             gint row);
static gint          construct_physical_dims(XYZRasControls *controls,
                                             GtkTable *table,
                                             gint row);
static gint          construct_options      (XYZRasControls *controls,
                                             GtkTable *table,
                                             gint row);
static void          xres_changed           (XYZRasControls *controls,
                                             GtkAdjustment *adj);
static void          yres_changed           (XYZRasControls *controls,
                                             GtkAdjustment *adj);
static void          xmin_changed           (XYZRasControls *controls,
                                             GtkEntry *entry);
static void          xmax_changed           (XYZRasControls *controls,
                                             GtkEntry *entry);
static void          ymin_changed           (XYZRasControls *controls,
                                             GtkEntry *entry);
static void          ymax_changed           (XYZRasControls *controls,
                                             GtkEntry *entry);
static void          interpolation_changed  (XYZRasControls *controls,
                                             GtkComboBox *combo);
static void          exterior_changed       (XYZRasControls *controls,
                                             GtkComboBox *combo);
static void          reset_ranges           (XYZRasControls *controls);
static void          preview                (XYZRasControls *controls);
static void          triangulation_info     (XYZRasControls *controls);
static GwyDataField* xyzras_do              (XYZRasData *rdata,
                                             const XYZRasArgs *args,
                                             GtkWindow *dialog,
                                             gchar **error);
static gboolean      interpolate_field      (guint npoints,
                                             const GwyXYZ *points,
                                             GwyDataField *dfield,
                                             GwySetFractionFunc set_fraction,
                                             GwySetMessageFunc set_message);
static gboolean      extend_borders         (XYZRasData *rdata,
                                             const XYZRasArgs *args,
                                             gboolean check_for_changes,
                                             gdouble epsrel);
static void          xyzras_free            (XYZRasData *rdata);
static void          initialize_ranges      (const XYZRasData *rdata,
                                             XYZRasArgs *args);
static void          analyse_points         (XYZRasData *rdata,
                                             double epsrel);
static GwyDataField* check_regular_grid     (GwySurface *surface);
static void          xyzras_load_args       (GwyContainer *container,
                                             XYZRasArgs *args);
static void          xyzras_save_args       (GwyContainer *container,
                                             XYZRasArgs *args);

static const XYZRasArgs xyzras_defaults = {
    GWY_INTERPOLATION_LINEAR, GWY_EXTERIOR_MIRROR_EXTEND,
    512, 512,
    /* Interface only. */
    0.0, 0.0, 0.0, 0.0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Rastrerizes XYZ data to images."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_xyz_func_register("xyz_raster",
                          (GwyXYZFunc)&xyzras,
                          N_("/_Rasterize..."),
                          NULL,
                          XYZRAS_RUN_MODES,
                          GWY_MENU_FLAG_XYZ,
                          N_("Rasterize to image"));

    return TRUE;
}

static void
xyzras(GwyContainer *data, GwyRunType run)
{
    XYZRasArgs args;
    XYZRasData rdata;

    GwyContainer *settings;
    GwySurface *surface = NULL;
    GwyDataField *dfield;
    gboolean ok = TRUE;
    gint id, newid;

    g_return_if_fail(run & XYZRAS_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_SURFACE, &surface,
                                     GWY_APP_SURFACE_ID, &id,
                                     0);
    g_return_if_fail(GWY_IS_SURFACE(surface));

    if ((dfield = check_regular_grid(surface))) {
        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        gwy_app_channel_log_add(data, -1, newid, "xyz::xyz_raster", NULL);
        return;
    }

    settings = gwy_app_settings_get();
    xyzras_load_args(settings, &args);
    gwy_clear(&rdata, 1);
    rdata.surface = surface;
    rdata.points = g_array_new(FALSE, FALSE, sizeof(GwyXYZ));
    analyse_points(&rdata, EPSREL);
    initialize_ranges(&rdata, &args);

    if (run == GWY_RUN_INTERACTIVE)
        ok = xyzras_dialog(&args, &rdata, data, id);

    xyzras_save_args(settings, &args);

    if (ok) {
        gchar *error = NULL;
        dfield = xyzras_do(&rdata, &args, NULL, &error);
        if (dfield) {
            newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
            gwy_app_channel_log_add(data, -1, newid, "xyz::xyz_raster", NULL);
        }
        else {
            /* TODO */
            g_free(error);
        }
    }

    xyzras_free(&rdata);
}

static gboolean
xyzras_dialog(XYZRasArgs *args,
              XYZRasData *rdata,
              GwyContainer *data,
              gint id)
{
    GtkWidget *dialog, *vbox, *align, *label, *hbox, *button;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    GtkTable *table;
    XYZRasControls controls;
    gint row, response;
    const guchar *gradient;
    GQuark quark;

    controls.args = args;
    controls.rdata = rdata;
    controls.mydata = gwy_container_new();

    dialog = gtk_dialog_new_with_buttons(_("Rasterize XYZ Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_xyz_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    /* Left column */
    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 0);

    table = GTK_TABLE(gtk_table_new(10, 5, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(table));
    row = 0;

    row = construct_resolutions(&controls, table, row);
    row = construct_physical_dims(&controls, table, row);

    button = gtk_button_new_with_mnemonic(_("Reset Ran_ges"));
    gtk_table_attach(table, button, 1, 4, row, row+1,
                     GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(reset_ranges), &controls);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    row = construct_options(&controls, table, row);

    /* Right column */
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Preview"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    quark = gwy_app_get_surface_palette_key_for_id(id);
    if (gwy_container_gis_string(data, quark, &gradient)) {
        gwy_container_set_const_string_by_name(controls.mydata,
                                               "/0/base/palette", gradient);
    }
    dfield = gwy_data_field_new(PREVIEW_SIZE, PREVIEW_SIZE, 1.0, 1.0, TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    g_object_unref(dfield);

    controls.view = gwy_data_view_new(controls.mydata);
    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 0);

    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);

    controls.do_preview = gtk_button_new_with_mnemonic(_("_Update"));
    gtk_box_pack_start(GTK_BOX(vbox), controls.do_preview, FALSE, FALSE, 4);

    controls.error = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.error), 0.0, 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(controls.error), TRUE);
    gtk_widget_set_size_request(controls.error, PREVIEW_SIZE, -1);
    gtk_box_pack_start(GTK_BOX(vbox), controls.error, FALSE, FALSE, 0);
    triangulation_info(&controls);

    g_signal_connect_swapped(controls.do_preview, "clicked",
                             G_CALLBACK(preview), &controls);
    g_signal_connect_swapped(controls.xres, "value-changed",
                             G_CALLBACK(xres_changed), &controls);
    g_signal_connect_swapped(controls.yres, "value-changed",
                             G_CALLBACK(yres_changed), &controls);
    g_signal_connect_swapped(controls.xmin, "activate",
                             G_CALLBACK(xmin_changed), &controls);
    g_signal_connect_swapped(controls.xmax, "activate",
                             G_CALLBACK(xmax_changed), &controls);
    g_signal_connect_swapped(controls.ymin, "activate",
                             G_CALLBACK(ymin_changed), &controls);
    g_signal_connect_swapped(controls.ymax, "activate",
                             G_CALLBACK(ymax_changed), &controls);
    g_signal_connect_swapped(controls.interpolation, "changed",
                             G_CALLBACK(interpolation_changed), &controls);
    g_signal_connect_swapped(controls.exterior, "changed",
                             G_CALLBACK(exterior_changed), &controls);

    controls.in_update = FALSE;

    reset_ranges(&controls);

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
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
    g_object_unref(controls.mydata);

    return TRUE;
}

static gint
construct_resolutions(XYZRasControls *controls,
                      GtkTable *table,
                      gint row)
{
    XYZRasArgs *args = controls->args;
    GtkWidget *spin, *label;

    gtk_table_attach(table, gwy_label_new_header(_("Resolution")),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Horizontal size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls->xres = gtk_adjustment_new(args->xres, 2, 16384, 1, 100, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls->xres), 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gtk_table_attach(table, spin, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new("px");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Vertical size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls->yres = gtk_adjustment_new(args->yres, 2, 16384, 1, 100, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls->yres), 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gtk_table_attach(table, spin, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new("px");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    return row;
}

static gint
construct_physical_dims(XYZRasControls *controls,
                        GtkTable *table,
                        gint row)
{
    GwySurface *surface = controls->rdata->surface;
    GwySIValueFormat *vf;
    GtkWidget *label;

    vf = gwy_surface_get_value_format_xy(surface, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                         NULL);

    gtk_table_attach(table, gwy_label_new_header(_("Physical Dimensions")),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_X-range:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls->xmin = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(controls->xmin), 7);
    gwy_widget_set_activate_on_unfocus(controls->xmin, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls->xmin);
    gtk_table_attach(table, controls->xmin, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_attach(table, gtk_label_new("–"), 2, 3, row, row+1, 0, 0, 0, 0);
    controls->xmax = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(controls->xmax), 7);
    gwy_widget_set_activate_on_unfocus(controls->xmax, TRUE);
    gtk_table_attach(table, controls->xmax, 3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label), vf->units);
    gtk_table_attach(table, label, 4, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Y-range:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls->ymin = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(controls->ymin), 7);
    gwy_widget_set_activate_on_unfocus(controls->ymin, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls->ymin);
    gtk_table_attach(table, controls->ymin, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_attach(table, gtk_label_new("–"), 2, 3, row, row+1, 0, 0, 0, 0);
    controls->ymax = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(controls->ymax), 7);
    gwy_widget_set_activate_on_unfocus(controls->ymax, TRUE);
    gtk_table_attach(table, controls->ymax, 3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label), vf->units);
    gtk_table_attach(table, label, 4, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls->rdata->xymag = vf->magnitude;
    gwy_si_unit_value_format_free(vf);

    return row;
}

static gint
construct_options(XYZRasControls *controls,
                  GtkTable *table,
                  gint row)
{
    XYZRasArgs *args = controls->args;
    GtkWidget *label;

    gtk_table_attach(table, gwy_label_new_header(_("Options")),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls->interpolation
        = gwy_enum_combo_box_newl(NULL, NULL,
                                  args->interpolation,
                                  _("Round"), GWY_INTERPOLATION_ROUND,
                                  _("Linear"), GWY_INTERPOLATION_LINEAR,
                                  _("Field"), GWY_INTERPOLATION_FIELD,
                                  _("Average"), GWY_INTERPOLATION_AVERAGE,
                                  NULL);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls->interpolation);
    gtk_table_attach(table, controls->interpolation, 1, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Exterior type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls->exterior
        = gwy_enum_combo_box_newl(NULL, NULL,
                                  args->exterior,
                                  gwy_sgettext("exterior|Border"),
                                  GWY_EXTERIOR_BORDER_EXTEND,
                                  gwy_sgettext("exterior|Mirror"),
                                  GWY_EXTERIOR_MIRROR_EXTEND,
                                  gwy_sgettext("exterior|Periodic"),
                                  GWY_EXTERIOR_PERIODIC,
                                  NULL);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls->exterior);
    gtk_table_attach(table, controls->exterior, 1, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    return row;
}

static void
set_adjustment_in_update(XYZRasControls *controls,
                         GtkAdjustment *adj,
                         gdouble value)
{
    controls->in_update = TRUE;
    gtk_adjustment_set_value(adj, value);
    controls->in_update = FALSE;
}

static void
set_physical_dimension(XYZRasControls *controls,
                       GtkEntry *entry,
                       gdouble value,
                       gboolean in_update)
{
    gchar buf[24];

    if (in_update) {
        g_assert(!controls->in_update);
        controls->in_update = TRUE;
    }

    g_snprintf(buf, sizeof(buf), "%g", value/controls->rdata->xymag);
    gtk_entry_set_text(entry, buf);

    if (in_update)
        controls->in_update = FALSE;
}

static void
recalculate_xres(XYZRasControls *controls)
{
    XYZRasArgs *args = controls->args;
    gint xres;

    if (controls->in_update)
        return;

    xres = GWY_ROUND((args->xmax - args->xmin)/(args->ymax - args->ymin)
                     *args->yres);
    xres = CLAMP(xres, 2, 16384);
    set_adjustment_in_update(controls, GTK_ADJUSTMENT(controls->xres), xres);
}

static void
recalculate_yres(XYZRasControls *controls)
{
    XYZRasArgs *args = controls->args;
    gint yres;

    if (controls->in_update)
        return;

    yres = GWY_ROUND((args->ymax - args->ymin)/(args->xmax - args->xmin)
                     *args->xres);
    yres = CLAMP(yres, 2, 16384);
    set_adjustment_in_update(controls, GTK_ADJUSTMENT(controls->yres), yres);
}

static void
xres_changed(XYZRasControls *controls,
             GtkAdjustment *adj)
{
    XYZRasArgs *args = controls->args;

    args->xres = gwy_adjustment_get_int(adj);
    recalculate_yres(controls);
}

static void
yres_changed(XYZRasControls *controls,
             GtkAdjustment *adj)
{
    XYZRasArgs *args = controls->args;

    args->yres = gwy_adjustment_get_int(adj);
    recalculate_xres(controls);
}

static void
xmin_changed(XYZRasControls *controls,
             GtkEntry *entry)
{
    XYZRasArgs *args = controls->args;
    gdouble val = g_strtod(gtk_entry_get_text(entry), NULL);

    args->xmin = val * controls->rdata->xymag;
    if (!controls->in_update) {
        args->xmax = args->xmin + (args->ymax - args->ymin);
        set_physical_dimension(controls, GTK_ENTRY(controls->xmax),
                               args->xmax, TRUE);
    }
    recalculate_xres(controls);
}

static void
xmax_changed(XYZRasControls *controls,
             GtkEntry *entry)
{
    XYZRasArgs *args = controls->args;
    gdouble val = g_strtod(gtk_entry_get_text(entry), NULL);

    args->xmax = val * controls->rdata->xymag;
    if (!controls->in_update) {
        args->ymax = args->ymin + (args->xmax - args->xmin);
        set_physical_dimension(controls, GTK_ENTRY(controls->ymax),
                               args->ymax, TRUE);
    }
    recalculate_xres(controls);
}

static void
ymin_changed(XYZRasControls *controls,
             GtkEntry *entry)
{
    XYZRasArgs *args = controls->args;
    gdouble val = g_strtod(gtk_entry_get_text(entry), NULL);

    args->ymin = val * controls->rdata->xymag;
    if (!controls->in_update) {
        args->ymax = args->ymin + (args->xmax - args->xmin);
        set_physical_dimension(controls, GTK_ENTRY(controls->ymax),
                               args->ymax, TRUE);
    }
    recalculate_yres(controls);
}

static void
ymax_changed(XYZRasControls *controls,
             GtkEntry *entry)
{
    XYZRasArgs *args = controls->args;
    gdouble val = g_strtod(gtk_entry_get_text(entry), NULL);

    args->ymax = val * controls->rdata->xymag;
    if (!controls->in_update) {
        args->xmax = args->xmin + (args->ymax - args->ymin);
        set_physical_dimension(controls, GTK_ENTRY(controls->xmax),
                               args->xmax, TRUE);
    }
    recalculate_xres(controls);
}

static void
interpolation_changed(XYZRasControls *controls,
                      GtkComboBox *combo)
{
    XYZRasArgs *args = controls->args;

    args->interpolation = gwy_enum_combo_box_get_active(combo);
}

static void
exterior_changed(XYZRasControls *controls,
                 GtkComboBox *combo)
{
    XYZRasArgs *args = controls->args;

    args->exterior = gwy_enum_combo_box_get_active(combo);
}

static void
reset_ranges(XYZRasControls *controls)
{
    XYZRasArgs myargs = *controls->args;

    initialize_ranges(controls->rdata, &myargs);
    set_physical_dimension(controls, GTK_ENTRY(controls->ymin), myargs.ymin,
                           TRUE);
    set_physical_dimension(controls, GTK_ENTRY(controls->ymax), myargs.ymax,
                           TRUE);
    set_physical_dimension(controls, GTK_ENTRY(controls->xmin), myargs.xmin,
                           TRUE);
    set_physical_dimension(controls, GTK_ENTRY(controls->xmax), myargs.xmax,
                           TRUE);
}

static void
preview(XYZRasControls *controls)
{
    XYZRasArgs *args = controls->args;
    GwyDataField *dfield;
    GtkWidget *entry;
    gint xres, yres;
    gchar *error = NULL;

    entry = gtk_window_get_focus(GTK_WINDOW(controls->dialog));
    if (entry && GTK_IS_ENTRY(entry))
        gtk_widget_activate(entry);

    xres = args->xres;
    yres = args->yres;
    args->xres = PREVIEW_SIZE*xres/MAX(xres, yres);
    args->yres = PREVIEW_SIZE*yres/MAX(xres, yres);
    dfield = xyzras_do(controls->rdata, args,
                       GTK_WINDOW(controls->dialog), &error);
    if (dfield) {
        triangulation_info(controls);
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls->error), error);
        g_free(error);
        dfield = gwy_data_field_new(args->xres, args->yres,
                                    args->xres, args->yres, TRUE);
    }
    args->xres = xres;
    args->yres = yres;

    gwy_container_set_object_by_name(controls->mydata, "/0/data", dfield);
    g_object_unref(dfield);
}

static void
triangulation_info(XYZRasControls *controls)
{
    XYZRasData *rdata;
    gchar *s;

    rdata = controls->rdata;
    s = g_strdup_printf(_("Number of points: %u\n"
                          "Merged as too close: %u\n"
                          "Added on the boundaries: %u"),
                        rdata->norigpoints,
                        rdata->norigpoints - rdata->nbasepoints,
                        rdata->points->len - rdata->nbasepoints);
    gtk_label_set_text(GTK_LABEL(controls->error), s);
    g_free(s);
}

static GwyDataField*
xyzras_do(XYZRasData *rdata,
          const XYZRasArgs *args,
          GtkWindow *window,
          gchar **error)
{
    GwyTriangulation *triangulation = rdata->triangulation;
    GArray *points = rdata->points;
    GwyDataField *dfield;
    GwySurface *surface = rdata->surface;
    GwySetMessageFunc set_message = (window ? gwy_app_wait_set_message : NULL);
    GwySetFractionFunc set_fraction = (window ? gwy_app_wait_set_fraction : NULL);
    gboolean ok = TRUE;

    gwy_debug("%g %g :: %g %g", args->xmin, args->xmax, args->ymin, args->ymax);
    if (!(args->xmax > args->xmin) || !(args->ymax > args->ymin)) {
        *error = g_strdup(_("Physical dimensions are invalid."));
        return NULL;
    }
    dfield = gwy_data_field_new(args->xres, args->yres,
                                args->xmax - args->xmin,
                                args->ymax - args->ymin,
                                FALSE);
    gwy_data_field_set_xoffset(dfield, args->xmin);
    gwy_data_field_set_yoffset(dfield, args->ymin);
    gwy_serializable_clone(G_OBJECT(gwy_surface_get_si_unit_xy(surface)),
                           G_OBJECT(gwy_data_field_get_si_unit_xy(dfield)));
    gwy_serializable_clone(G_OBJECT(gwy_surface_get_si_unit_z(surface)),
                           G_OBJECT(gwy_data_field_get_si_unit_z(dfield)));

    if ((gint)args->interpolation == GWY_INTERPOLATION_FIELD) {
        if (window)
            gwy_app_wait_start(window, _("Initializing..."));

        extend_borders(rdata, args, FALSE, EPSREL);
        ok = interpolate_field(points->len, (const GwyXYZ*)points->data, dfield,
                               set_fraction, set_message);
    }
    else if ((gint)args->interpolation == GWY_INTERPOLATION_AVERAGE) {
        extend_borders(rdata, args, FALSE, EPSREL);
        gwy_data_field_average_xyz(dfield, NULL,
                                   (const GwyXYZ*)points->data, points->len);
        ok = TRUE;
    }
    else {
        if (window)
            gwy_app_wait_start(window, _("Initializing..."));
        /* [Try to] perform triangulation if either there is none yet or
         * extend_borders() reports the points have changed. */
        gwy_debug("have triangulation: %d", !!triangulation);
        if (!triangulation || extend_borders(rdata, args, TRUE, EPSREL)) {
            gwy_debug("must triangulate");
            gwy_object_unref(rdata->triangulation);
            rdata->triangulation = triangulation = gwy_triangulation_new();
            ok = gwy_triangulation_triangulate_iterative(triangulation,
                                                         points->len,
                                                         points->data,
                                                         sizeof(GwyXYZ),
                                                         set_fraction,
                                                         set_message);
        }
        else {
            gwy_debug("points did not change, recycling triangulation");
        }

        if (triangulation && ok) {
            if (window)
                ok = set_message(_("Interpolating..."));
            if (ok)
                ok = gwy_triangulation_interpolate(triangulation,
                                                   args->interpolation, dfield);
        }
        if (window)
            gwy_app_wait_finish();
    }

    if (!ok) {
        gwy_object_unref(rdata->triangulation);
        g_object_unref(dfield);
        *error = g_strdup(_("XYZ data regularization failed due to "
                            "numerical instability or was interrupted."));
        return NULL;
    }

    return dfield;
}

static gboolean
interpolate_field(guint npoints,
                  const GwyXYZ *points,
                  GwyDataField *dfield,
                  GwySetFractionFunc set_fraction,
                  GwySetMessageFunc set_message)
{
    gdouble xoff, yoff, qx, qy;
    guint xres, yres, i, j, k;
    gdouble *d;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xoff = gwy_data_field_get_xoffset(dfield);
    yoff = gwy_data_field_get_yoffset(dfield);
    qx = gwy_data_field_get_xreal(dfield)/xres;
    qy = gwy_data_field_get_yreal(dfield)/yres;
    d = gwy_data_field_get_data(dfield);

    if (set_message)
        set_message(_("Interpolating..."));

    for (i = 0; i < yres; i++) {
        gdouble y = yoff + qy*(i + 0.5);

        for (j = 0; j < xres; j++) {
            gdouble x = xoff + qx*(j + 0.5);
            gdouble w = 0.0;
            gdouble s = 0.0;

            for (k = 0; k < npoints; k++) {
                const GwyXYZ *pt = points + k;
                gdouble dx = x - pt->x;
                gdouble dy = y - pt->y;
                gdouble r2 = dx*dx + dy*dy;

                r2 *= r2;
                if (G_UNLIKELY(r2 == 0.0)) {
                    s = pt->z;
                    w = 1.0;
                    break;
                }

                r2 = 1.0/r2;
                w += r2;
                s += r2*pt->z;
            }
            *(d++) = s/w;
        }

        if (set_fraction && !set_fraction(i/(gdouble)yres))
            return FALSE;
    }

    return TRUE;
}

/* Return TRUE if extpoints have changed. */
static gboolean
extend_borders(XYZRasData *rdata,
               const XYZRasArgs *args,
               gboolean check_for_changes,
               gdouble epsrel)
{
    GwySurface *surface = rdata->surface;
    gdouble xmin, xmax, ymin, ymax, xreal, yreal, eps;
    gdouble sxmin, sxmax, symin, symax;
    gdouble *oldextpoints = NULL;
    guint i, nbase, noldext;
    gboolean extchanged;

    /* Remember previous extpoints.  If they do not change we do not need to
     * repeat the triangulation. */
    nbase = rdata->nbasepoints;
    noldext = rdata->points->len - nbase;
    if (check_for_changes) {
        oldextpoints = g_memdup(&g_array_index(rdata->points, GwyXYZ, nbase),
                                noldext*sizeof(GwyXYZ));
    }
    g_array_set_size(rdata->points, nbase);

    if (args->exterior == GWY_EXTERIOR_BORDER_EXTEND) {
        g_free(oldextpoints);
        return FALSE;
    }

    gwy_surface_get_xrange(surface, &sxmin, &sxmax);
    gwy_surface_get_yrange(surface, &symin, &symax);
    xreal = sxmax - sxmin;
    yreal = symax - symin;

    xmin = args->xmin - 2*rdata->step;
    xmax = args->xmax + 2*rdata->step;
    ymin = args->ymin - 2*rdata->step;
    ymax = args->ymax + 2*rdata->step;
    eps = epsrel*rdata->step;

    /* Extend the field according to requester boder extension, however,
     * create at most 3 full copies (4 halves and 4 quarters) of the base set.
     * Anyone asking for more is either clueless or malicious. */
    for (i = 0; i < nbase; i++) {
        const GwyXYZ pt = g_array_index(rdata->points, GwyXYZ, i);
        GwyXYZ pt2;
        gdouble txl, txr, tyt, tyb;
        gboolean txlok, txrok, tytok, tybok;

        pt2.z = pt.z;
        if (args->exterior == GWY_EXTERIOR_MIRROR_EXTEND) {
            txl = 2.0*sxmin - pt.x;
            tyt = 2.0*symin - pt.y;
            txr = 2.0*sxmax - pt.x;
            tyb = 2.0*symax - pt.y;
            txlok = pt.x - sxmin < 0.5*xreal;
            tytok = pt.y - symin < 0.5*yreal;
            txrok = sxmax - pt.x < 0.5*xreal;
            tybok = symax - pt.y < 0.5*yreal;
        }
        else if (args->exterior == GWY_EXTERIOR_PERIODIC) {
            txl = pt.x - xreal;
            tyt = pt.y - yreal;
            txr = pt.x + xreal;
            tyb = pt.y + yreal;
            txlok = sxmax - pt.x < 0.5*xreal;
            tytok = symax - pt.y < 0.5*yreal;
            txrok = pt.x - sxmin < 0.5*xreal;
            tybok = pt.y - symin < 0.5*yreal;
        }
        else {
            g_assert_not_reached();
        }

        txlok = txlok && (txl >= xmin && txl <= xmax
                          && fabs(txl - sxmin) > eps);
        tytok = tytok && (tyt >= ymin && tyt <= ymax
                          && fabs(tyt - symin) > eps);
        txrok = txrok && (txr >= ymin && txr <= xmax
                          && fabs(txr - sxmax) > eps);
        tybok = tybok && (tyb >= ymin && tyb <= xmax
                          && fabs(tyb - symax) > eps);

        if (txlok) {
            pt2.x = txl;
            pt2.y = pt.y - eps;
            g_array_append_val(rdata->points, pt2);
        }
        if (txlok && tytok) {
            pt2.x = txl + eps;
            pt2.y = tyt - eps;
            g_array_append_val(rdata->points, pt2);
        }
        if (tytok) {
            pt2.x = pt.x + eps;
            pt2.y = tyt;
            g_array_append_val(rdata->points, pt2);
        }
        if (txrok && tytok) {
            pt2.x = txr + eps;
            pt2.y = tyt + eps;
            g_array_append_val(rdata->points, pt2);
        }
        if (txrok) {
            pt2.x = txr;
            pt2.y = pt.y + eps;
            g_array_append_val(rdata->points, pt2);
        }
        if (txrok && tybok) {
            pt2.x = txr - eps;
            pt2.y = tyb + eps;
            g_array_append_val(rdata->points, pt2);
        }
        if (tybok) {
            pt2.x = pt.x - eps;
            pt2.y = tyb;
            g_array_append_val(rdata->points, pt2);
        }
        if (txlok && tybok) {
            pt2.x = txl - eps;
            pt2.y = tyb - eps;
            g_array_append_val(rdata->points, pt2);
        }
    }

    if (!check_for_changes)
        return TRUE;

    extchanged = (noldext != rdata->points->len - nbase
                  || memcmp(&g_array_index(rdata->points, GwyXYZ, nbase),
                            oldextpoints,
                            noldext*sizeof(GwyXYZ)));
    g_free(oldextpoints);
    return extchanged;
}

static void
xyzras_free(XYZRasData *rdata)
{
    gwy_object_unref(rdata->triangulation);
    g_array_free(rdata->points, TRUE);
}

static gdouble
round_with_base(gdouble x, gdouble base)
{
    gint s;

    s = (x < 0) ? -1 : 1;
    x = fabs(x)/base;
    if (x <= 1.0)
        return GWY_ROUND(10.0*x)/10.0*s*base;
    else if (x <= 2.0)
        return GWY_ROUND(5.0*x)/5.0*s*base;
    else if (x <= 5.0)
        return GWY_ROUND(2.0*x)/2.0*s*base;
    else
        return GWY_ROUND(x)*s*base;
}

static void
round_to_nice(gdouble *minval, gdouble *maxval)
{
    gdouble range = *maxval - *minval;
    gdouble base = pow10(floor(log10(range) - 2.0));

    *minval = round_with_base(*minval, base);
    *maxval = round_with_base(*maxval, base);
}

static void
initialize_ranges(const XYZRasData *rdata,
                  XYZRasArgs *args)
{
    GwySurface *surface = rdata->surface;

    gwy_surface_get_xrange(surface, &args->xmin, &args->xmax);
    gwy_surface_get_yrange(surface, &args->ymin, &args->ymax);

    round_to_nice(&args->xmin, &args->xmax);
    round_to_nice(&args->ymin, &args->ymax);

    gwy_debug("%g %g :: %g %g", args->xmin, args->xmax, args->ymin, args->ymax);
}

static inline guint
coords_to_grid_index(guint xres,
                     guint yres,
                     gdouble step,
                     gdouble x,
                     gdouble y)
{
    guint ix, iy;

    ix = (guint)floor(x/step);
    if (G_UNLIKELY(ix >= xres))
        ix--;

    iy = (guint)floor(y/step);
    if (G_UNLIKELY(iy >= yres))
        iy--;

    return iy*xres + ix;
}

static inline void
index_accumulate(guint *index_array,
                 guint n)
{
    guint i;

    for (i = 1; i <= n; i++)
        index_array[i] += index_array[i-1];
}

static inline void
index_rewind(guint *index_array,
             guint n)
{
    guint i;

    for (i = n; i; i--)
        index_array[i] = index_array[i-1];
    index_array[0] = 0;
}

static void
work_queue_init(WorkQueue *queue)
{
    queue->size = 64;
    queue->len = 0;
    queue->id = g_new(guint, queue->size);
}

static void
work_queue_destroy(WorkQueue *queue)
{
    g_free(queue->id);
}

static void
work_queue_add(WorkQueue *queue,
               guint id)
{
    if (G_UNLIKELY(queue->len == queue->size)) {
        queue->size *= 2;
        queue->id = g_renew(guint, queue->id, queue->size);
    }
    queue->id[queue->len] = id;
    queue->len++;
}

static void
work_queue_ensure(WorkQueue *queue,
                  guint id)
{
    guint i;

    for (i = 0; i < queue->len; i++) {
        if (queue->id[i] == id)
            return;
    }
    work_queue_add(queue, id);
}

static inline gdouble
point_dist2(const GwyXYZ *p,
            const GwyXYZ *q)
{
    gdouble dx = p->x - q->x;
    gdouble dy = p->y - q->y;

    return dx*dx + dy*dy;
}

static gboolean
maybe_add_point(WorkQueue *pointqueue,
                const GwyXYZ *newpoints,
                guint ii,
                gdouble eps2)
{
    const GwyXYZ *pt;
    guint i;

    pt = newpoints + pointqueue->id[ii];
    for (i = 0; i < pointqueue->pos; i++) {
        if (point_dist2(pt, newpoints + pointqueue->id[i]) < eps2) {
            GWY_SWAP(guint,
                     pointqueue->id[ii], pointqueue->id[pointqueue->pos]);
            pointqueue->pos++;
            return TRUE;
        }
    }
    return FALSE;
}

/* Calculate coordinate ranges and ensure points are more than epsrel*cellside
 * appart where cellside is the side of equivalent-area square for one point. */
static void
analyse_points(XYZRasData *rdata,
               double epsrel)
{
    GwySurface *surface = rdata->surface;
    WorkQueue cellqueue, pointqueue;
    const GwyXYZ *points, *pt;
    GwyXYZ *newpoints;
    gdouble xreal, yreal, eps, eps2, xr, yr, step;
    guint npoints, i, ii, j, ig, xres, yres, ncells, oldpos;
    gdouble xmin, xmax, ymin, ymax;
    guint *cell_index;

    /* Calculate data ranges */
    npoints = rdata->norigpoints = surface->n;
    points = surface->data;
    gwy_surface_get_xrange(surface, &xmin, &xmax);
    gwy_surface_get_yrange(surface, &ymin, &ymax);

    xreal = xmax - xmin;
    yreal = ymax - ymin;

    if (xreal == 0.0 || yreal == 0.0) {
        g_warning("All points lie on a line, we are going to crash.");
    }

    /* Make a virtual grid */
    xr = xreal/sqrt(npoints)*CELL_SIDE;
    yr = yreal/sqrt(npoints)*CELL_SIDE;

    if (xr <= yr) {
        xres = (guint)ceil(xreal/xr);
        step = xreal/xres;
        yres = (guint)ceil(yreal/step);
    }
    else {
        yres = (guint)ceil(yreal/yr);
        step = yreal/yres;
        xres = (guint)ceil(xreal/step);
    }
    rdata->step = step;
    eps = epsrel*step;
    eps2 = eps*eps;

    ncells = xres*yres;
    cell_index = g_new0(guint, ncells + 1);

    for (i = 0; i < npoints; i++) {
        pt = points + i;
        ig = coords_to_grid_index(xres, yres, step, pt->x - xmin, pt->y - ymin);
        cell_index[ig]++;
    }

    index_accumulate(cell_index, xres*yres);
    g_assert(cell_index[xres*yres] == npoints);
    index_rewind(cell_index, xres*yres);
    newpoints = g_new(GwyXYZ, npoints);

    /* Sort points by cell */
    for (i = 0; i < npoints; i++) {
        pt = points + i;
        ig = coords_to_grid_index(xres, yres, step, pt->x - xmin, pt->y - ymin);
        newpoints[cell_index[ig]] = *pt;
        cell_index[ig]++;
    }
    g_assert(cell_index[xres*yres] == npoints);
    index_rewind(cell_index, xres*yres);

    /* Find groups of identical (i.e. closer than epsrel) points we need to
     * merge.  We collapse all merged points to that with the lowest id.
     * Closeness must be transitive so the group must be gathered iteratively
     * until it no longer grows. */
    work_queue_init(&pointqueue);
    work_queue_init(&cellqueue);
    g_array_set_size(rdata->points, 0);
    for (i = 0; i < npoints; i++) {
        /* Ignore merged points */
        if (newpoints[i].z == G_MAXDOUBLE)
            continue;

        pointqueue.len = 0;
        cellqueue.len = 0;
        cellqueue.pos = 0;
        work_queue_add(&pointqueue, i);
        pointqueue.pos = 1;
        oldpos = 0;

        do {
            /* Update the list of cells to process.  Most of the time this is
             * no-op. */
            while (oldpos < pointqueue.pos) {
                gdouble x, y;
                gint ix, iy;

                pt = newpoints + pointqueue.id[oldpos];
                x = (pt->x - xmin)/step;
                ix = (gint)floor(x);
                x -= ix;
                y = (pt->y - ymin)/step;
                iy = (gint)floor(y);
                y -= iy;

                if (ix < xres && iy < yres)
                    work_queue_ensure(&cellqueue, iy*xres + ix);
                if (ix > 0 && iy < yres && x <= eps)
                    work_queue_ensure(&cellqueue, iy*xres + ix-1);
                if (ix < xres && iy > 0 && y <= eps)
                    work_queue_ensure(&cellqueue, (iy - 1)*xres + ix);
                if (ix > 0 && iy > 0 && x < eps && y <= eps)
                    work_queue_ensure(&cellqueue, (iy - 1)*xres + ix-1);
                if (ix+1 < xres && iy < xres && 1-x <= eps)
                    work_queue_ensure(&cellqueue, iy*xres + ix+1);
                if (ix < xres && iy+1 < xres && 1-y <= eps)
                    work_queue_ensure(&cellqueue, (iy + 1)*xres + ix);
                if (ix+1 < xres && iy+1 < xres && 1-x <= eps && 1-y <= eps)
                    work_queue_ensure(&cellqueue, (iy + 1)*xres + ix+1);

                oldpos++;
            }

            /* Process all points from the cells and check if they belong to
             * the currently merged group. */
            while (cellqueue.pos < cellqueue.len) {
                j = cellqueue.id[cellqueue.pos];
                for (ii = cell_index[j]; ii < cell_index[j+1]; ii++) {
                    if (ii != i && newpoints[ii].z != G_MAXDOUBLE)
                        work_queue_add(&pointqueue, ii);
                }
                cellqueue.pos++;
            }

            /* Compare all not-in-group points with all group points, adding
             * them to the group on success. */
            for (ii = pointqueue.pos; ii < pointqueue.len; ii++)
                maybe_add_point(&pointqueue, newpoints, ii, eps2);
        } while (oldpos != pointqueue.pos);

        /* Calculate the representant of all contributing points. */
        {
            GwyXYZ avg = { 0.0, 0.0, 0.0 };

            for (ii = 0; ii < pointqueue.pos; ii++) {
                GwyXYZ *ptii = newpoints + pointqueue.id[ii];
                avg.x += ptii->x;
                avg.y += ptii->y;
                avg.z += ptii->z;
                ptii->z = G_MAXDOUBLE;
            }

            avg.x /= pointqueue.pos;
            avg.y /= pointqueue.pos;
            avg.z /= pointqueue.pos;
            g_array_append_val(rdata->points, avg);
        }
    }

    work_queue_destroy(&cellqueue);
    work_queue_destroy(&pointqueue);
    g_free(cell_index);
    g_free(newpoints);

    rdata->nbasepoints = rdata->points->len;
}

/* Create a data field directly if the XY positions form a complete regular
 * grid.  */
static GwyDataField*
check_regular_grid(GwySurface *surface)
{
    gdouble ymaxstep, xmin, xmax, ymin, ymax, dx, dy;
    guint n, xres, yres, k;
    GwyDataField *dfield;
    gdouble *yvalues, *data;
    gboolean *encountered;
    gboolean ok = TRUE;

    n = surface->n;
    if (n < 4)
        return NULL;

    /* Do not create Nx1 or 1xN fields. */
    gwy_surface_get_xrange(surface, &xmin, &xmax);
    gwy_surface_get_yrange(surface, &ymin, &ymax);
    if (xmin == xmax || ymin == ymax)
        return NULL;

    yvalues = g_new(gdouble, n);
    for (k = 0; k < n; k++)
        yvalues[k] = surface->data[k].y;
    gwy_math_sort(n, yvalues);

    ymaxstep = 0.0;
    for (k = 1; k < n; k++) {
        if (yvalues[k] - yvalues[k-1] > ymaxstep)
            ymaxstep = yvalues[k] - yvalues[k-1];
    }
    gwy_debug("ymaxstep %g", ymaxstep);
    yres = (gint)ceil((ymax - ymin)/ymaxstep) + 1;
    gwy_debug("estimated yres %d", yres);
    g_free(yvalues);

    if (n % yres != 0)
        return NULL;

    xres = n/yres;
    gwy_debug("yres %d", xres);
    dx = (xmax - xmin)/(xres - 1);
    dy = (ymax - ymin)/(yres - 1);
    xmin -= 0.5*dx;
    xmax += 0.5*dx;
    ymin -= 0.5*dy;
    ymax += 0.5*dy;

    dfield = gwy_data_field_new(xres, yres, xmax - xmin, ymax - ymin, FALSE);
    data = gwy_data_field_get_data(dfield);
    encountered = g_new0(gboolean, n);
    for (k = 0; k < n; k++) {
        gdouble y = (surface->data[k].y - ymin)/dy;
        gdouble x = (surface->data[k].x - xmin)/dx;
        gint i = (gint)floor(y);
        gint j = (gint)floor(x);

        if (fabs(x - j - 0.5) > 0.01) {
            gwy_debug("point (%d,%d) too far in x %g", j, i, x - j - 0.5);
            ok = FALSE;
            break;
        }
        if (fabs(y - i - 0.5) > 0.01) {
            gwy_debug("point (%d,%d) too far in y %g", j, i, y - i - 0.5);
            ok = FALSE;
            break;
        }
        if (encountered[i*xres + j]) {
            gwy_debug("point (%d,%d) encountered twice", j, i);
            ok = FALSE;
            break;
        }

        encountered[i*xres + j] = TRUE;
        data[i*xres + j] = surface->data[k].z;
    }

    g_free(encountered);
    if (!ok) {
        g_object_unref(dfield);
        return NULL;
    }

    gwy_data_field_set_xoffset(dfield, xmin);
    gwy_data_field_set_yoffset(dfield, ymin);
    gwy_serializable_clone(G_OBJECT(gwy_surface_get_si_unit_xy(surface)),
                           G_OBJECT(gwy_data_field_get_si_unit_xy(dfield)));
    gwy_serializable_clone(G_OBJECT(gwy_surface_get_si_unit_z(surface)),
                           G_OBJECT(gwy_data_field_get_si_unit_z(dfield)));
    return dfield;
}

static const gchar exterior_key[]      = "/module/xyz_raster/exterior";
static const gchar interpolation_key[] = "/module/xyz_raster/interpolation";
static const gchar xres_key[]          = "/module/xyz_raster/xres";
static const gchar yres_key[]          = "/module/xyz_raster/yres";

static void
xyzras_sanitize_args(XYZRasArgs *args)
{
    if (args->interpolation != GWY_INTERPOLATION_ROUND
        && (gint)args->interpolation != GWY_INTERPOLATION_FIELD
        && (gint)args->interpolation != GWY_INTERPOLATION_AVERAGE)
        args->interpolation = GWY_INTERPOLATION_LINEAR;
    if (args->exterior != GWY_EXTERIOR_MIRROR_EXTEND
        && args->exterior != GWY_EXTERIOR_PERIODIC)
        args->exterior = GWY_EXTERIOR_BORDER_EXTEND;
    args->xres = CLAMP(args->xres, 2, 16384);
    args->yres = CLAMP(args->yres, 2, 16384);
}

static void
xyzras_load_args(GwyContainer *container,
                 XYZRasArgs *args)
{
    *args = xyzras_defaults;

    gwy_container_gis_enum_by_name(container, interpolation_key,
                                   &args->interpolation);
    gwy_container_gis_enum_by_name(container, exterior_key, &args->exterior);
    gwy_container_gis_int32_by_name(container, xres_key, &args->xres);
    gwy_container_gis_int32_by_name(container, yres_key, &args->yres);

    xyzras_sanitize_args(args);
}

static void
xyzras_save_args(GwyContainer *container,
                 XYZRasArgs *args)
{
    gwy_container_set_enum_by_name(container, interpolation_key,
                                   args->interpolation);
    gwy_container_set_enum_by_name(container, exterior_key, args->exterior);
    gwy_container_set_int32_by_name(container, xres_key, args->xres);
    gwy_container_set_int32_by_name(container, yres_key, args->yres);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
