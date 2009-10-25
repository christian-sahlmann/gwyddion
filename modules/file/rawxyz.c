/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
 * Raw XYZ data
 * .xyz .dat
 * Read[1]
 * [1] XYZ data are interpolated to a regular grid upon import.
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libprocess/delaunay.h>
#include <libdraw/gwypixfield.h>
#include <libdraw/gwygradient.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>
#include <app/settings.h>

#include "err.h"

#define EPSREL 1e-7

/* Use smaller cell sides than the triangulation algorithm as we only need them
 * for identical point detection and border extension. */
#define CELL_SIDE 1.6

enum {
    PREVIEW_SIZE = 240,
    UNDEF = G_MAXUINT
};

enum {
    GWY_INTERPOLATION_FIELD = -1,
};

typedef struct {
    /* XXX: Not all values of interpolation and exterior are possible. */
    GwyInterpolationType interpolation;
    GwyExteriorType exterior;
    gchar *xy_units;
    gchar *z_units;
    gint xres;
    gint yres;
    gboolean xydimeq;
    gboolean xymeasureeq;
    /* Interface only */
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;
} RawXYZArgs;

typedef struct {
    GArray *points;
    guint norigpoints;
    guint nbasepoints;
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;
    gdouble step;
    gdouble zmin;
    gdouble zmax;
} RawXYZFile;

typedef struct {
    RawXYZArgs *args;
    RawXYZFile *rfile;
    GtkWidget *dialog;
    GwyGradient *gradient;
    GtkObject *xmin;
    GtkObject *xmax;
    GtkObject *ymin;
    GtkObject *ymax;
    GtkWidget *xydimeq;
    GtkWidget *xymeasureeq;
    GtkObject *xres;
    GtkObject *yres;
    GtkWidget *xy_units;
    GtkWidget *xy_units_parsed;
    GtkWidget *z_units;
    GtkWidget *z_units_parsed;
    GtkWidget *interpolation;
    GtkWidget *exterior;
    GtkWidget *preview;
    GtkWidget *do_preview;
    GtkWidget *error;
    gboolean in_update;
} RawXYZControls;

typedef struct {
    guint *id;
    guint pos;
    guint len;
    guint size;
} WorkQueue;

static gboolean      module_register      (void);
static GwyContainer* rawxyz_load          (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static gboolean      rawxyz_dialog        (RawXYZArgs *arg,
                                           RawXYZFile *rfile);
static void          xyunits_changed      (RawXYZControls *controls,
                                           GtkEntry *entry);
static void          zunits_changed       (RawXYZControls *controls,
                                           GtkEntry *entry);
static void          xres_changed         (RawXYZControls *controls,
                                           GtkAdjustment *adj);
static void          yres_changed         (RawXYZControls *controls,
                                           GtkAdjustment *adj);
static void          xmin_changed         (RawXYZControls *controls,
                                           GtkAdjustment *adj);
static void          xmax_changed         (RawXYZControls *controls,
                                           GtkAdjustment *adj);
static void          ymin_changed         (RawXYZControls *controls,
                                           GtkAdjustment *adj);
static void          ymax_changed         (RawXYZControls *controls,
                                           GtkAdjustment *adj);
static void          xydimeq_changed      (RawXYZControls *controls,
                                           GtkToggleButton *button);
static void          xymeasureeq_changed  (RawXYZControls *controls,
                                           GtkToggleButton *button);
static void          interpolation_changed(RawXYZControls *controls,
                                           GtkComboBox *combo);
static void          exterior_changed     (RawXYZControls *controls,
                                           GtkComboBox *combo);
static void          preview              (RawXYZControls *controls);
static GwyDataField* rawxyz_do            (RawXYZFile *rfile,
                                           const RawXYZArgs *args);
static void          interpolate_field    (guint npoints,
                                           const GwyDelaunayPointXYZ *points,
                                           GwyDataField *dfield);
static void          extend_borders       (RawXYZFile *rfile,
                                           const RawXYZArgs *args,
                                           gdouble epsrel);
static void          rawxyz_free          (RawXYZFile *rfile);
static GArray*       read_points          (gchar *p);
static void          initialize_ranges    (const RawXYZFile *rfile,
                                           RawXYZArgs *args);
static void          analyse_points       (RawXYZFile *rfile,
                                           double epsrel);
static void          rawxyz_load_args     (GwyContainer *container,
                                           RawXYZArgs *args);
static void          rawxyz_save_args     (GwyContainer *container,
                                           RawXYZArgs *args);

static const RawXYZArgs rawxyz_defaults = {
    GWY_INTERPOLATION_LINEAR, GWY_EXTERIOR_MIRROR_EXTEND,
    NULL, NULL,
    500, 500,
    TRUE, TRUE,
    /* Interface only */
    0.0, 0.0, 0.0, 0.0
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports raw XYZ files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("rawxyz",
                           N_("Raw XYZ data"),
                           NULL,
                           (GwyFileLoadFunc)&rawxyz_load,
                           NULL,
                           NULL);

    return TRUE;
}

static GwyContainer*
rawxyz_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *settings, *container = NULL;
    GwyDataField *dfield;
    RawXYZArgs args;
    RawXYZFile rfile;
    gchar *buffer = NULL;
    gsize size;
    GError *err = NULL;
    gboolean ok;

    /* Someday we can load XYZ data with default settings */
    if (mode != GWY_RUN_INTERACTIVE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_INTERACTIVE,
                    _("Raw XYZ data import must be run as interactive."));
        return NULL;
    }

    gwy_clear(&rfile, 1);

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    rfile.points = read_points(buffer);
    g_free(buffer);
    if (!rfile.points->len) {
        err_NO_DATA(error);
        goto fail;
    }

    settings = gwy_app_settings_get();
    rawxyz_load_args(settings, &args);
    analyse_points(&rfile, EPSREL);
    initialize_ranges(&rfile, &args);
    ok = rawxyz_dialog(&args, &rfile);
    rawxyz_save_args(settings, &args);
    if (!ok) {
        err_CANCELLED(error);
        goto fail;
    }

    dfield = rawxyz_do(&rfile, &args);
    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup("Regularized XYZ"));

fail:
    rawxyz_free(&rfile);

    return container;
}

static gboolean
rawxyz_dialog(RawXYZArgs *args,
              RawXYZFile *rfile)
{
    GtkWidget *dialog, *vbox, *align, *label, *spin, *hbox, *button;
    GtkTable *table;
    RawXYZControls controls;
    GdkPixbuf *pixbuf;
    gint row, response;

    controls.args = args;
    controls.rfile = rfile;
    controls.gradient = gwy_gradients_get_gradient(NULL);
    gwy_resource_use(GWY_RESOURCE(controls.gradient));

    /* Enforce xydimeq */
    if (args->xydimeq) {
        gdouble c, dx, dy;

        dx = args->xmax - args->xmin;
        dy = args->ymax - args->ymin;
        if (dx > dy) {
            c = args->ymin + args->ymax;
            args->ymin = 0.5*(c - dx);
            args->ymax = 0.5*(c + dx);
        }
        else {
            c = args->xmin + args->xmax;
            args->xmin = 0.5*(c - dy);
            args->xmax = 0.5*(c + dy);
        }
    }

    dialog = gtk_dialog_new_with_buttons(_("Import XYZ Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    /* Left column */
    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 0);

    table = GTK_TABLE(gtk_table_new(12, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(table));
    row = 0;

    /* Resolution */
    gtk_table_attach(table, gwy_label_new_header(_("Resolution")),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Horizontal size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.xres = gtk_adjustment_new(args->xres, 2, 16384, 1, 100, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xres), 0, 0);
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
    controls.yres = gtk_adjustment_new(args->yres, 2, 16384, 1, 100, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.yres), 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gtk_table_attach(table, spin, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new("px");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Identical _measures"));
    controls.xymeasureeq = button;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), args->xymeasureeq);
    gtk_table_attach(table, button, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    /* Resolution */
    gtk_table_attach(table, gwy_label_new_header(_("Physical Dimensions")),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_X-range:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.xmin = gtk_adjustment_new(args->xmin, -1000.0, 1000.0, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xmin), 0, 3);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gtk_table_attach(table, spin, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_attach(table, gtk_label_new("–"), 2, 3, row, row+1, 0, 0, 0, 0);
    controls.xmax = gtk_adjustment_new(args->xmax, -1000.0, 1000.0, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xmax), 0, 3);
    gtk_table_attach(table, spin, 3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Y-range:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.ymin = gtk_adjustment_new(args->ymin, -1000.0, 1000.0, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.ymin), 0, 3);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gtk_table_attach(table, spin, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_attach(table, gtk_label_new("–"), 2, 3, row, row+1, 0, 0, 0, 0);
    controls.ymax = gtk_adjustment_new(args->ymax, -1000.0, 1000.0, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.ymax), 0, 3);
    gtk_table_attach(table, spin, 3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("S_quare sample"));
    controls.xydimeq = button;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), args->xydimeq);
    gtk_table_attach(table, button, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Lateral units:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.xy_units = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.xy_units);
    gtk_entry_set_text(GTK_ENTRY(controls.xy_units), args->xy_units);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.xy_units), 6);
    gtk_table_attach(table, controls.xy_units, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.xy_units_parsed = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(controls.xy_units_parsed), args->xy_units);
    gtk_misc_set_alignment(GTK_MISC(controls.xy_units_parsed), 0.0, 0.5);
    gtk_table_attach(table, controls.xy_units_parsed, 3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Value units:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.z_units = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.z_units);
    gtk_entry_set_text(GTK_ENTRY(controls.z_units), args->z_units);
    gtk_entry_set_width_chars(GTK_ENTRY(controls.z_units), 6);
    gtk_table_attach(table, controls.z_units, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.z_units_parsed = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(controls.z_units_parsed), args->z_units);
    gtk_misc_set_alignment(GTK_MISC(controls.z_units_parsed), 0.0, 0.5);
    gtk_table_attach(table, controls.z_units_parsed, 3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    /* Options */
    gtk_table_attach(table, gwy_label_new_header(_("Options")),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.interpolation
        = gwy_enum_combo_box_newl(NULL, NULL,
                                  args->interpolation,
                                  _("Round"), GWY_INTERPOLATION_ROUND,
                                  _("Linear"), GWY_INTERPOLATION_LINEAR,
                                  _("Field"), GWY_INTERPOLATION_FIELD,
                                  NULL);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.interpolation);
    gtk_table_attach(table, controls.interpolation, 1, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Exterior type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.exterior
        = gwy_enum_combo_box_newl(NULL, NULL,
                                  args->exterior,
                                  _("Border"), GWY_EXTERIOR_BORDER_EXTEND,
                                  _("Mirror"), GWY_EXTERIOR_MIRROR_EXTEND,
                                  _("Periodic"), GWY_EXTERIOR_PERIODIC,
                                  NULL);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.exterior);
    gtk_table_attach(table, controls.exterior, 1, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    /* Right column */
    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Preview"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    controls.preview = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(vbox), controls.preview, FALSE, FALSE, 0);

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                            PREVIEW_SIZE, PREVIEW_SIZE);
    gdk_pixbuf_fill(pixbuf, 0);
    gtk_image_set_from_pixbuf(GTK_IMAGE(controls.preview), pixbuf);
    g_object_unref(pixbuf);

    controls.do_preview = gtk_button_new_with_mnemonic(_("_Update"));
    gtk_box_pack_start(GTK_BOX(vbox), controls.do_preview, FALSE, FALSE, 4);

    controls.error = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.error), 0.0, 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(controls.error), TRUE);
    gtk_widget_set_size_request(controls.error, PREVIEW_SIZE, -1);
    gtk_box_pack_start(GTK_BOX(vbox), controls.error, FALSE, FALSE, 0);

    g_signal_connect_swapped(controls.xy_units, "changed",
                             G_CALLBACK(xyunits_changed), &controls);
    g_signal_connect_swapped(controls.z_units, "changed",
                             G_CALLBACK(zunits_changed), &controls);
    g_signal_connect_swapped(controls.do_preview, "clicked",
                             G_CALLBACK(preview), &controls);
    g_signal_connect_swapped(controls.xres, "value-changed",
                             G_CALLBACK(xres_changed), &controls);
    g_signal_connect_swapped(controls.yres, "value-changed",
                             G_CALLBACK(yres_changed), &controls);
    g_signal_connect_swapped(controls.xmin, "value-changed",
                             G_CALLBACK(xmin_changed), &controls);
    g_signal_connect_swapped(controls.xmax, "value-changed",
                             G_CALLBACK(xmax_changed), &controls);
    g_signal_connect_swapped(controls.ymin, "value-changed",
                             G_CALLBACK(ymin_changed), &controls);
    g_signal_connect_swapped(controls.ymax, "value-changed",
                             G_CALLBACK(ymax_changed), &controls);
    g_signal_connect_swapped(controls.xydimeq, "toggled",
                             G_CALLBACK(xydimeq_changed), &controls);
    g_signal_connect_swapped(controls.xymeasureeq, "toggled",
                             G_CALLBACK(xymeasureeq_changed), &controls);
    g_signal_connect_swapped(controls.interpolation, "changed",
                             G_CALLBACK(interpolation_changed), &controls);
    g_signal_connect_swapped(controls.exterior, "changed",
                             G_CALLBACK(exterior_changed), &controls);
    controls.in_update = FALSE;

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            gwy_resource_release(GWY_RESOURCE(controls.gradient));
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
    gwy_resource_release(GWY_RESOURCE(controls.gradient));

    return TRUE;
}

static void
update_unit_label(GtkLabel *label,
                  const gchar *unitstring)
{
    GwySIValueFormat *vf;
    GwySIUnit *unit;
    gint power10;

    unit = gwy_si_unit_new_parse(unitstring, &power10);
    vf = gwy_si_unit_get_format_for_power10(unit, GWY_SI_UNIT_FORMAT_MARKUP,
                                            power10, NULL);
    gtk_label_set_markup(label, vf->units);
    gwy_si_unit_value_format_free(vf);
}

static void
xyunits_changed(RawXYZControls *controls,
                GtkEntry *entry)
{
    RawXYZArgs *args = controls->args;

    g_free(args->xy_units);
    args->xy_units = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, G_MAXINT);
    update_unit_label(GTK_LABEL(controls->xy_units_parsed), args->xy_units);
}

static void
zunits_changed(RawXYZControls *controls,
               GtkEntry *entry)
{
    RawXYZArgs *args = controls->args;

    g_free(args->z_units);
    args->z_units = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, G_MAXINT);
    update_unit_label(GTK_LABEL(controls->z_units_parsed), args->z_units);
}

static void
set_adjustment_in_update(RawXYZControls *controls,
                         GtkAdjustment *adj,
                         gdouble value)
{
    controls->in_update = TRUE;
    gtk_adjustment_set_value(adj, value);
    controls->in_update = FALSE;
}

static void
recalculate_xres(RawXYZControls *controls)
{
    RawXYZArgs *args = controls->args;
    gint xres;

    if (controls->in_update || !args->xymeasureeq)
        return;

    xres = GWY_ROUND((args->xmax - args->xmin)/(args->ymax - args->ymin)
                     *args->yres);
    xres = CLAMP(xres, 2, 16384);
    set_adjustment_in_update(controls, GTK_ADJUSTMENT(controls->xres), xres);
}

static void
recalculate_yres(RawXYZControls *controls)
{
    RawXYZArgs *args = controls->args;
    gint yres;

    if (controls->in_update || !args->xymeasureeq)
        return;

    yres = GWY_ROUND((args->ymax - args->ymin)/(args->xmax - args->xmin)
                     *args->xres);
    yres = CLAMP(yres, 2, 16384);
    set_adjustment_in_update(controls, GTK_ADJUSTMENT(controls->yres), yres);
}

static void
xres_changed(RawXYZControls *controls,
             GtkAdjustment *adj)
{
    RawXYZArgs *args = controls->args;

    args->xres = gwy_adjustment_get_int(adj);
    recalculate_yres(controls);
}

static void
yres_changed(RawXYZControls *controls,
             GtkAdjustment *adj)
{
    RawXYZArgs *args = controls->args;

    args->yres = gwy_adjustment_get_int(adj);
    recalculate_xres(controls);
}

static void
xmin_changed(RawXYZControls *controls,
             GtkAdjustment *adj)
{
    RawXYZArgs *args = controls->args;
    gdouble val = gtk_adjustment_get_value(adj);

    if (val >= args->xmax && !args->xydimeq)
        set_adjustment_in_update(controls, adj, args->xmin);
    else {
        args->xmin = val;
        if (args->xydimeq && !controls->in_update)
            set_adjustment_in_update(controls, GTK_ADJUSTMENT(controls->xmax),
                                     args->xmin + (args->ymax - args->ymin));
        recalculate_xres(controls);
    }
}

static void
xmax_changed(RawXYZControls *controls,
             GtkAdjustment *adj)
{
    RawXYZArgs *args = controls->args;
    gdouble val = gtk_adjustment_get_value(adj);

    if (val <= args->xmin)
        set_adjustment_in_update(controls, adj, args->xmax);
    else {
        args->xmax = val;
        if (args->xydimeq && !controls->in_update)
            set_adjustment_in_update(controls, GTK_ADJUSTMENT(controls->ymax),
                                     args->ymin + (args->xmax - args->xmin));
        recalculate_xres(controls);
    }
}

static void
ymin_changed(RawXYZControls *controls,
             GtkAdjustment *adj)
{
    RawXYZArgs *args = controls->args;
    gdouble val = gtk_adjustment_get_value(adj);

    if (val >= args->ymax && !args->xydimeq)
        set_adjustment_in_update(controls, adj, args->ymin);
    else {
        args->ymin = val;
        if (args->xydimeq && !controls->in_update)
            set_adjustment_in_update(controls, GTK_ADJUSTMENT(controls->ymax),
                                     args->ymin + (args->xmax - args->xmin));
        recalculate_yres(controls);
    }
}

static void
ymax_changed(RawXYZControls *controls,
             GtkAdjustment *adj)
{
    RawXYZArgs *args = controls->args;
    gdouble val = gtk_adjustment_get_value(adj);

    if (val <= args->ymin)
        set_adjustment_in_update(controls, adj, args->ymax);
    else {
        args->ymax = val;
        if (args->xydimeq && !controls->in_update)
            set_adjustment_in_update(controls, GTK_ADJUSTMENT(controls->xmax),
                                     args->xmin + (args->ymax - args->ymin));
        recalculate_xres(controls);
    }
}

static void
xydimeq_changed(RawXYZControls *controls,
                GtkToggleButton *button)
{
    RawXYZArgs *args = controls->args;

    args->xydimeq = gtk_toggle_button_get_active(button);
    if (args->xydimeq) {
        /* Force ymax update. */
        gtk_adjustment_value_changed(GTK_ADJUSTMENT(controls->xmax));
    }
}

static void
xymeasureeq_changed(RawXYZControls *controls,
                    GtkToggleButton *button)
{
    RawXYZArgs *args = controls->args;

    args->xymeasureeq = gtk_toggle_button_get_active(button);
    if (args->xymeasureeq) {
        /* Force yres. update */
        gtk_adjustment_value_changed(GTK_ADJUSTMENT(controls->xres));
    }
}

static void
interpolation_changed(RawXYZControls *controls,
                      GtkComboBox *combo)
{
    RawXYZArgs *args = controls->args;

    args->interpolation = gwy_enum_combo_box_get_active(combo);
}

static void
exterior_changed(RawXYZControls *controls,
                      GtkComboBox *combo)
{
    RawXYZArgs *args = controls->args;

    args->exterior = gwy_enum_combo_box_get_active(combo);
}

static void
preview(RawXYZControls *controls)
{
    RawXYZArgs *args = controls->args;
    GwyDataField *dfield;
    GdkPixbuf *pixbuf;
    GtkWidget *entry;
    gdouble avg, rms;
    gint xres, yres;

    entry = gtk_window_get_focus(GTK_WINDOW(controls->dialog));
    if (entry && GTK_IS_ENTRY(entry))
        gtk_widget_activate(entry);

    xres = args->xres;
    yres = args->yres;
    args->xres = PREVIEW_SIZE*xres/MAX(xres, yres);
    args->yres = PREVIEW_SIZE*yres/MAX(xres, yres);
    dfield = rawxyz_do(controls->rfile, args);
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                            args->xres, args->yres);
    args->xres = xres;
    args->yres = yres;
    avg = gwy_data_field_get_avg(dfield);
    rms = gwy_data_field_get_rms(dfield);
    gwy_pixbuf_draw_data_field(pixbuf, dfield, controls->gradient);
    gtk_image_set_from_pixbuf(GTK_IMAGE(controls->preview), pixbuf);
    g_object_unref(pixbuf);
    g_object_unref(dfield);
}

static GwyDataField*
rawxyz_do(RawXYZFile *rfile,
          const RawXYZArgs *args)
{
    GArray *points = rfile->points;
    GwyDelaunayTriangulation *triangulation;
    GwySIUnit *unitxy, *unitz;
    GwyDataField *dfield;
    gint xypow10, zpow10;
    gdouble mag;

    unitxy = gwy_si_unit_new_parse(args->xy_units, &xypow10);
    mag = pow10(xypow10);
    unitz = gwy_si_unit_new_parse(args->z_units, &zpow10);
    dfield = gwy_data_field_new(args->xres, args->yres,
                                args->xmax - args->xmin,
                                args->ymax - args->ymin,
                                FALSE);
    gwy_data_field_set_si_unit_xy(dfield, unitxy);
    gwy_data_field_set_si_unit_z(dfield, unitz);
    gwy_data_field_set_xoffset(dfield, args->xmin);
    gwy_data_field_set_yoffset(dfield, args->ymin);
    g_object_unref(unitxy);
    g_object_unref(unitz);

    extend_borders(rfile, args, EPSREL);
    if (args->interpolation == GWY_INTERPOLATION_FIELD) {
        interpolate_field(points->len, (GwyDelaunayPointXYZ*)points->data,
                          dfield);
    }
    else {
        triangulation = gwy_delaunay_triangulate(points->len, points->data,
                                                 sizeof(GwyDelaunayPointXYZ));
        gwy_delaunay_interpolate(triangulation,
                                 points->data, sizeof(GwyDelaunayPointXYZ),
                                 args->interpolation, dfield);
        gwy_delaunay_triangulation_free(triangulation);
    }

    /* Fix the scales according to real units. */
    gwy_data_field_set_xreal(dfield, mag*(args->xmax - args->xmin));
    gwy_data_field_set_yreal(dfield, mag*(args->ymax - args->ymin));
    gwy_data_field_set_xoffset(dfield, mag*args->xmin);
    gwy_data_field_set_yoffset(dfield, mag*args->ymin);
    gwy_data_field_multiply(dfield, pow10(zpow10));

    return dfield;
}

static void
interpolate_field(guint npoints,
                  const GwyDelaunayPointXYZ *points,
                  GwyDataField *dfield)
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

    for (i = 0; i < yres; i++) {
        gdouble y = yoff + qy*(i + 0.5);

        for (j = 0; j < xres; j++) {
            gdouble x = xoff + qx*(j + 0.5);
            gdouble w = 0.0;
            gdouble s = 0.0;

            for (k = 0; k < npoints; k++) {
                const GwyDelaunayPointXYZ *pt = points + k;
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
    }
}

static void
extend_borders(RawXYZFile *rfile,
               const RawXYZArgs *args,
               gdouble epsrel)
{
    gdouble xmin, xmax, ymin, ymax, xreal, yreal, eps;
    guint i;

    g_array_set_size(rfile->points, rfile->nbasepoints);

    if (args->exterior == GWY_EXTERIOR_BORDER_EXTEND)
        return;

    xreal = rfile->xmax - rfile->xmin;
    yreal = rfile->ymax - rfile->ymin;
    xmin = args->xmin - 2*rfile->step;
    xmax = args->xmax + 2*rfile->step;
    ymin = args->ymin - 2*rfile->step;
    ymax = args->ymax + 2*rfile->step;
    eps = epsrel*rfile->step;

    /* Extend the field according to requester boder extension, however,
     * create at most 3 full copies (4 halves and 4 quarters) of the base set.
     * Anyone asking for more is either clueless or malicious. */
    for (i = 0; i < rfile->nbasepoints; i++) {
        const GwyDelaunayPointXYZ *pt = &g_array_index(rfile->points,
                                                       GwyDelaunayPointXYZ, i);
        GwyDelaunayPointXYZ pt2;
        gdouble txl, txr, tyt, tyb;
        gboolean txlok, txrok, tytok, tybok;

        pt2.z = pt->z;
        if (args->exterior == GWY_EXTERIOR_MIRROR_EXTEND) {
            txl = 2.0*rfile->xmin - pt->x;
            tyt = 2.0*rfile->ymin - pt->y;
            txr = 2.0*rfile->xmax - pt->x;
            tyb = 2.0*rfile->ymax - pt->y;
            txlok = pt->x - rfile->xmin < 0.5*xreal;
            tytok = pt->y - rfile->ymin < 0.5*yreal;
            txrok = rfile->xmax - pt->x < 0.5*xreal;
            tybok = rfile->ymax - pt->y < 0.5*yreal;
        }
        else if (args->exterior == GWY_EXTERIOR_PERIODIC) {
            txl = pt->x - xreal;
            tyt = pt->y - yreal;
            txr = pt->x + xreal;
            tyb = pt->y + yreal;
            txlok = rfile->xmax - pt->x < 0.5*xreal;
            tytok = rfile->ymax - pt->y < 0.5*yreal;
            txrok = pt->x - rfile->xmin < 0.5*xreal;
            tybok = pt->y - rfile->ymin < 0.5*yreal;
        }
        else {
            g_assert_not_reached();
        }

        txlok = txlok && (txl >= xmin && txl <= xmax
                          && fabs(txl - rfile->xmin) > eps);
        tytok = tytok && (tyt >= ymin && tyt <= ymax
                          && fabs(tyt - rfile->ymin) > eps);
        txrok = txrok && (txr >= ymin && txr <= xmax
                          && fabs(txr - rfile->xmax) > eps);
        tybok = tybok && (tyb >= ymin && tyb <= xmax
                          && fabs(tyb - rfile->ymax) > eps);

        if (txlok) {
            pt2.x = txl;
            pt2.y = pt->y - eps;
            g_array_append_val(rfile->points, pt2);
        }
        if (txlok && tytok) {
            pt2.x = txl + eps;
            pt2.y = tyt - eps;
            g_array_append_val(rfile->points, pt2);
        }
        if (tytok) {
            pt2.x = pt->x + eps;
            pt2.y = tyt;
            g_array_append_val(rfile->points, pt2);
        }
        if (txrok && tytok) {
            pt2.x = txr + eps;
            pt2.y = tyt + eps;
            g_array_append_val(rfile->points, pt2);
        }
        if (txrok) {
            pt2.x = txr;
            pt2.y = pt->y + eps;
            g_array_append_val(rfile->points, pt2);
        }
        if (txrok && tybok) {
            pt2.x = txr - eps;
            pt2.y = tyb + eps;
            g_array_append_val(rfile->points, pt2);
        }
        if (tybok) {
            pt2.x = pt->x - eps;
            pt2.y = tyb;
            g_array_append_val(rfile->points, pt2);
        }
        if (txlok && tybok) {
            pt2.x = txl - eps;
            pt2.y = tyb - eps;
            g_array_append_val(rfile->points, pt2);
        }
    }
}

static void
rawxyz_free(RawXYZFile *rfile)
{
    g_array_free(rfile->points, TRUE);
}

static GArray*
read_points(gchar *p)
{
    GArray *points;
    gchar *line, *end;

    points = g_array_new(FALSE, FALSE, sizeof(GwyDelaunayPointXYZ));
    for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
        GwyDelaunayPointXYZ pt;

        if (!line[0] || line[0] == '#')
            continue;

        if (!(pt.x = g_ascii_strtod(line, &end)) && end == line)
            continue;
        line = end;
        if (!(pt.y = g_ascii_strtod(line, &end)) && end == line)
            continue;
        line = end;
        if (!(pt.z = g_ascii_strtod(line, &end)) && end == line)
            continue;

        g_array_append_val(points, pt);
    }

    return points;
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
    gdouble base = pow10(floor(log10(range)));

    *minval = round_with_base(*minval, base);
    *maxval = round_with_base(*maxval, base);
}

static void
initialize_ranges(const RawXYZFile *rfile,
                  RawXYZArgs *args)
{
    args->xmin = rfile->xmin;
    args->xmax = rfile->xmax;
    args->ymin = rfile->ymin;
    args->ymax = rfile->ymax;
    round_to_nice(&args->xmin, &args->xmax);
    round_to_nice(&args->ymin, &args->ymax);
}

static inline guint
coords_to_grid_index(guint xres,
                     guint yres,
                     gdouble step,
                     gdouble x,
                     gdouble y)
{
    guint ix, iy;

    ix = (gint)floor(x/step);
    if (G_UNLIKELY(ix == xres))
        ix--;

    iy = (gint)floor(y/step);
    if (G_UNLIKELY(iy == yres))
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
        if (queue->id[i] == i)
            return;
    }
    work_queue_add(queue, id);
}

static inline gdouble
point_dist2(const GwyDelaunayPointXYZ *p,
            const GwyDelaunayPointXYZ *q)
{
    gdouble dx = p->x - q->x;
    gdouble dy = p->y - q->y;

    return dx*dx + dy*dy;
}

static gboolean
maybe_add_point(WorkQueue *pointqueue,
                const GwyDelaunayPointXYZ *newpoints,
                guint id,
                gdouble eps2)
{
    guint i;

    for (i = 0; i < pointqueue->pos; i++) {
        if (point_dist2(newpoints + id, newpoints + pointqueue->id[i]) < eps2) {
            GWY_SWAP(guint, pointqueue->id[i], pointqueue->id[pointqueue->pos]);
            pointqueue->pos++;
            return TRUE;
        }
    }
    return FALSE;
}

/* Calculate coordinate ranges and ensure points are more than epsrel*cellside
 * appart where cellside is the side of equivalent-area square for one point. */
static void
analyse_points(RawXYZFile *rfile,
               double epsrel)
{
    WorkQueue cellqueue, pointqueue;
    GwyDelaunayPointXYZ *points, *newpoints, *pt;
    gdouble xreal, yreal, eps, eps2, xr, yr, step;
    guint npoints, i, j, ig, xres, yres, ncells, oldpos;
    guint *cell_index;

    /* Calculate data ranges */
    npoints = rfile->norigpoints = rfile->points->len;
    points = (GwyDelaunayPointXYZ*)rfile->points->data;
    rfile->xmin = rfile->xmax = points[0].x;
    rfile->ymin = rfile->ymax = points[0].y;
    rfile->zmin = rfile->zmax = points[0].z;
    for (i = 0; i < npoints; i++) {
        pt = points + i;

        if (pt->x < rfile->xmin)
            rfile->xmin = pt->x;
        else if (pt->x > rfile->xmax)
            rfile->xmax = pt->x;

        if (pt->y < rfile->ymin)
            rfile->ymin = pt->y;
        else if (pt->y > rfile->ymax)
            rfile->ymax = pt->y;

        if (pt->z < rfile->zmin)
            rfile->zmin = pt->z;
        else if (pt->z > rfile->zmax)
            rfile->zmax = pt->z;
    }

    xreal = rfile->xmax - rfile->xmin;
    yreal = rfile->ymax - rfile->ymin;

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
    rfile->step = step;
    eps = epsrel*step;
    eps2 = eps*eps;

    ncells = xres*yres;
    cell_index = g_new0(guint, ncells + 1);

    for (i = 0; i < npoints; i++) {
        pt = points + i;
        ig = coords_to_grid_index(xres, yres, step,
                                  pt->x - rfile->xmin, pt->y - rfile->ymin);
        cell_index[ig]++;
    }

    index_accumulate(cell_index, xres*yres);
    index_rewind(cell_index, xres*yres);
    newpoints = g_new(GwyDelaunayPointXYZ, npoints);

    /* Sort points by cell */
    for (i = 0; i < npoints; i++) {
        pt = points + i;
        ig = coords_to_grid_index(xres, yres, step,
                                  pt->x - rfile->xmin, pt->y - rfile->ymin);
        newpoints[cell_index[ig]] = *pt;
        cell_index[ig]++;
    }

    /* Find groups of identical (i.e. closer than epsrel) points we need to
     * merge.  We collapse all merged points to that with the lowest id.
     * Closeness must be transitive so the group must be gathered iteratively
     * until it no longer grows. */
    work_queue_init(&pointqueue);
    work_queue_init(&cellqueue);
    g_array_set_size(rfile->points, 0);
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
                guint ix, iy;

                pt = newpoints + pointqueue.id[oldpos];
                x = (pt->x - rfile->xmin)/step;
                ix = (guint)floor(x);
                x -= ix;
                y = (pt->y - rfile->ymin)/step;
                iy = (guint)floor(y);
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
                for (i = cell_index[j]; i < cell_index[j+1]; i++) {
                    if (newpoints[i].z != G_MAXDOUBLE)
                        work_queue_add(&pointqueue, i);
                }
                cellqueue.pos++;
            }

            /* Compare all not-in-group points with all group points, adding
             * them to the group on success. */
            for (i = pointqueue.pos; i < pointqueue.len; i++)
                maybe_add_point(&pointqueue, newpoints, i, eps2);
        } while (oldpos != pointqueue.pos);

        /* Calculate the representant of all contributing points. */
        {
            GwyDelaunayPointXYZ avg = { 0.0, 0.0, 0.0 };

            for (i = 0; i < pointqueue.pos; i++) {
                pt = newpoints + pointqueue.id[i];
                avg.x += pt->x;
                avg.y += pt->y;
                avg.z += pt->z;
                pt->z = G_MAXDOUBLE;
            }
            avg.x /= pointqueue.pos;
            avg.y /= pointqueue.pos;
            avg.z /= pointqueue.pos;
            g_array_append_val(rfile->points, avg);
        }
    }

    work_queue_destroy(&cellqueue);
    work_queue_destroy(&pointqueue);
    g_free(cell_index);
    g_free(newpoints);

    rfile->nbasepoints = rfile->points->len;
}

static const gchar exterior_key[]      = "/module/rawxyz/exterior";
static const gchar interpolation_key[] = "/module/rawxyz/interpolation";
static const gchar xy_units_key[]      = "/module/rawxyz/xy-units";
static const gchar z_units_key[]       = "/module/rawxyz/z-units";

static void
rawxyz_sanitize_args(RawXYZArgs *args)
{
    if (args->interpolation != GWY_INTERPOLATION_ROUND
        && args->interpolation != GWY_INTERPOLATION_FIELD)
        args->interpolation = GWY_INTERPOLATION_LINEAR;
    if (args->exterior != GWY_EXTERIOR_MIRROR_EXTEND
        && args->exterior != GWY_EXTERIOR_PERIODIC)
        args->exterior = GWY_EXTERIOR_BORDER_EXTEND;
}

static void
rawxyz_load_args(GwyContainer *container,
                 RawXYZArgs *args)
{
    *args = rawxyz_defaults;

    gwy_container_gis_enum_by_name(container, interpolation_key,
                                   &args->interpolation);
    gwy_container_gis_enum_by_name(container, exterior_key, &args->exterior);
    gwy_container_gis_string_by_name(container, xy_units_key,
                                     (const guchar**)&args->xy_units);
    gwy_container_gis_string_by_name(container, z_units_key,
                                     (const guchar**)&args->z_units);

    rawxyz_sanitize_args(args);
    args->xy_units = g_strdup(args->xy_units ? args->xy_units : "");
    args->z_units = g_strdup(args->z_units ? args->z_units : "");
}

static void
rawxyz_save_args(GwyContainer *container,
                 RawXYZArgs *args)
{
    gwy_container_set_enum_by_name(container, interpolation_key,
                                   args->interpolation);
    gwy_container_set_enum_by_name(container, exterior_key, args->exterior);
    gwy_container_set_string_by_name(container, xy_units_key,
                                     g_strdup(args->xy_units));
    gwy_container_set_string_by_name(container, z_units_key,
                                     g_strdup(args->z_units));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
