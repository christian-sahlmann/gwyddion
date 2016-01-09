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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nano-measuring-machine-spm">
 *   <comment>Nano Measuring Machine data header</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="------------------------------------------"/>
 *     <match type="string" offset="44" value="Scan procedure description file"/>
 *   </magic>
 *   <glob pattern="*.dsc"/>
 *   <glob pattern="*.DSC"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # Nano Measuring Machine profiles header
 * # Usually accompanied with unidentifiable data files.
 * 0 string ------------------------------------------
 * >44 string Scan\ procedure\ description\ file Nano Measuring Machine data header
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Nano Measuring Machine profile data
 * .dsc .dat
 * Read[1]
 * [1] XYZ data are interpolated to a regular grid upon import.
 **/

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define PointXYZ GwyTriangulationPointXYZ

#define EXTENSION ".dsc"
#define DASHED_LINE "------------------------------------------"

typedef struct {
    const gchar *filename;
    guint nfiles;
    guint blocksize;
    gulong ndata;
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;
} NMMXYZInfo;

typedef struct {
    gboolean *include_channel;
    guint nincluded;  // Derived quantity
    gint xres;
    gint yres;
    gboolean xymeasureeq;
    gboolean use_xres;
    gboolean plot_density;
} NMMXYZArgs;

typedef struct {
    NMMXYZArgs *args;
    NMMXYZInfo *info;
    GtkWidget *dialog;
    GtkObject *xres;
    GtkObject *yres;
    GtkWidget *xres_spin;
    GtkWidget *yres_spin;
    GtkWidget *xymeasureeq;
} NMMXYZControls;

typedef struct {
    guint id;
    guint npts;
    gchar *date;
    gchar *short_name;
    gchar *long_name;
} NMMXYZProfileDescription;

static gboolean      module_register           (void);
static gint          nmmxyz_detect             (const GwyFileDetectInfo *fileinfo,
                                                gboolean only_name);
static GwyContainer* nmmxyz_load               (const gchar *filename,
                                                GwyRunType mode,
                                                GError **error);
static void          update_nincluded          (NMMXYZArgs *args,
                                                guint blocksize);
static gboolean      nmmxyz_dialogue           (NMMXYZArgs *args,
                                                NMMXYZInfo *info,
                                                GArray *dscs);
static gint          add_info_count            (GtkTable *table,
                                                gint row,
                                                const gchar *description,
                                                gulong n);
static void          update_ok_sensitivity     (NMMXYZControls *controls);
static void          include_channel_changed   (GtkToggleButton *toggle,
                                                NMMXYZControls *controls);
static void          plot_density_changed      (NMMXYZControls *controls,
                                                GtkToggleButton *toggle);
static gint          construct_resolutions     (NMMXYZControls *controls,
                                                GtkTable *table,
                                                gint row);
static void          xres_changed              (NMMXYZControls *controls,
                                                GtkAdjustment *adj);
static void          yres_changed              (NMMXYZControls *controls,
                                                GtkAdjustment *adj);
static void          xymeasureeq_changed       (NMMXYZControls *controls,
                                                GtkToggleButton *toggle);
static gboolean      gather_data_files         (const gchar *filename,
                                                NMMXYZInfo *info,
                                                const NMMXYZArgs *args,
                                                GArray *dscs,
                                                GArray *data,
                                                GError **error);
static PointXYZ*     create_points_with_xy     (GArray *data,
                                                guint nincluded);
static void          create_data_field         (GwyContainer *container,
                                                const NMMXYZInfo *info,
                                                const NMMXYZArgs *args,
                                                GArray *dscs,
                                                GArray *data,
                                                PointXYZ *points,
                                                guint i,
                                                gboolean plot_density);
static void          find_data_range           (const PointXYZ *points,
                                                NMMXYZInfo *info);
static void          read_data_file            (GArray *data,
                                                const gchar *filename,
                                                guint nrec,
                                                const gboolean *include_channel,
                                                guint nincluded);
static gboolean      profile_descriptions_match(GArray *descs1,
                                                GArray *descs2);
static void          read_profile_description  (const gchar *filename,
                                                GArray *dscs);
static void          free_profile_descriptions (GArray *dscs,
                                                gboolean free_array);
static void          copy_profile_descriptions (GArray *source,
                                                GArray *dest);
static void          nmmxyz_load_args          (GwyContainer *container,
                                                NMMXYZArgs *args,
                                                GArray *dscs);
static void          nmmxyz_save_args          (GwyContainer *container,
                                                NMMXYZArgs *args,
                                                GArray *dscs);

static const NMMXYZArgs nmmxyz_defaults = {
    NULL, 0,
    1200, 1200,
    FALSE, TRUE,
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nano Measuring Machine profile files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nmmxyz",
                           N_("Nano Measuring Machine files (*.dsc)"),
                           (GwyFileDetectFunc)&nmmxyz_detect,
                           (GwyFileLoadFunc)&nmmxyz_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
nmmxyz_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (g_str_has_prefix(fileinfo->head, DASHED_LINE)
        && strstr(fileinfo->head, "Scan procedure description file"))
        score = 80;

    return score;
}

static GwyContainer*
nmmxyz_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    const NMMXYZProfileDescription *dsc;
    NMMXYZInfo info;
    NMMXYZArgs args;
    GwyContainer *settings;
    GwyContainer *container = NULL;
    GArray *data = NULL;
    GArray *dscs = NULL;
    PointXYZ *points = NULL;
    gboolean waiting = FALSE;
    guint i, ntodo;

    /* In principle we can load data non-interactively, but it is going to
     * take several minutes which is not good for previews... */
    if (mode != GWY_RUN_INTERACTIVE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_INTERACTIVE,
                    _("Nano Measuring Machine data import "
                      "must be run as interactive."));
        return NULL;
    }

    info.filename = filename;
    args = nmmxyz_defaults;
    dscs = g_array_new(FALSE, FALSE, sizeof(NMMXYZProfileDescription));
    if (!gather_data_files(filename, &info, &args, dscs, NULL, error))
        goto fail;

    gwy_debug("ndata from DSC files %lu", info.ndata);
    if (!info.ndata) {
        err_NO_DATA(error);
        goto fail;
    }

    if (info.blocksize < 3) {
        err_NO_DATA(error);
        goto fail;
    }

    dsc = &g_array_index(dscs, NMMXYZProfileDescription, 0);
    if (!gwy_strequal(dsc->short_name, "Lx"))
        g_warning("First channel is not Lx.");

    dsc = &g_array_index(dscs, NMMXYZProfileDescription, 1);
    if (!gwy_strequal(dsc->short_name, "Ly"))
        g_warning("Second channel is not Ly.");

    settings = gwy_app_settings_get();
    nmmxyz_load_args(settings, &args, dscs);
    if (!nmmxyz_dialogue(&args, &info, dscs)) {
        err_CANCELLED(error);
        goto fail;
    }
    nmmxyz_save_args(settings, &args, dscs);

    waiting = TRUE;
    gwy_app_wait_start(NULL, _("Reading files..."));

    free_profile_descriptions(dscs, FALSE);
    data = g_array_sized_new(FALSE, FALSE, sizeof(gdouble),
                             info.ndata*args.nincluded);
    if (!gather_data_files(filename, &info, &args, dscs, data, error))
        goto fail;

    if (!gwy_app_wait_set_message(_("Rendering surface..."))) {
        err_CANCELLED(error);
        goto fail;
    }
    gwy_app_wait_set_fraction(0.0);

    points = create_points_with_xy(data, args.nincluded);
    if (!gwy_app_wait_set_fraction(1.0/args.nincluded)) {
        err_CANCELLED(error);
        goto fail;
    }

    find_data_range(points, &info);
    if (!gwy_app_wait_set_fraction(2.0/args.nincluded)) {
        err_CANCELLED(error);
        goto fail;
    }

    container = gwy_container_new();
    ntodo = args.nincluded-2;
    for (i = 2; i < info.blocksize; i++) {
        if (args.include_channel[i]) {
            gdouble f;

            create_data_field(container, &info, &args, dscs, data, points, i,
                              args.plot_density && ntodo == 1);
            ntodo--;
            f = 1.0 - (gdouble)ntodo/args.nincluded;
            f = CLAMP(f, 0.0, 1.0);
            if (!gwy_app_wait_set_fraction(f)) {
                gwy_object_unref(container);
                err_CANCELLED(error);
                goto fail;
            }
        }
    }

fail:
    if (waiting)
        gwy_app_wait_finish();

    g_free(args.include_channel);
    g_free(points);
    free_profile_descriptions(dscs, TRUE);
    if (data)
        g_array_free(data, TRUE);

    return container;
}

static void
update_nincluded(NMMXYZArgs *args, guint blocksize)
{
    guint i;

    args->nincluded = 0;
    for (i = 0; i < blocksize; i++) {
        if (args->include_channel[i])
            args->nincluded++;
    }
}

static gboolean
nmmxyz_dialogue(NMMXYZArgs *args, NMMXYZInfo *info, GArray *dscs)
{
    GtkWidget *dialog, *label, *check;
    GtkTable *table;
    NMMXYZControls controls;
    guint i, nchannels;
    gint row, response;

    gwy_clear(&controls, 1);
    controls.args = args;
    controls.info = info;

    nchannels = info->blocksize - 2;

    dialog = gtk_dialog_new_with_buttons(_("Import NMM Profile Set"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_file_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    table = GTK_TABLE(gtk_table_new(10 + nchannels, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(table),
                       TRUE, TRUE, 0);
    row = 0;

    label = gwy_label_new_header(_("Information"));
    gtk_table_attach(table, label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    row = add_info_count(table, row,
                         _("Number of data files:"), info->nfiles);
    row = add_info_count(table, row,
                         _("Total number of points:"), info->ndata);
    row = add_info_count(table, row,
                         _("Points per profile:"), info->ndata/info->nfiles);

    gtk_table_set_row_spacing(table, row-1, 8);
    row = construct_resolutions(&controls, table, row);

    label = gwy_label_new_header(_("Imported Channels"));
    gtk_table_attach(table, label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    for (i = 0; i < nchannels; i++) {
        const NMMXYZProfileDescription *dsc
            = &g_array_index(dscs, NMMXYZProfileDescription, i + 2);

        check = gtk_check_button_new_with_label(dsc->long_name);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                     args->include_channel[i + 2]);
        g_object_set_data(G_OBJECT(check), "id", GUINT_TO_POINTER(i + 2));
        gtk_table_attach(table, check, 0, 4, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;

        g_signal_connect(check, "toggled",
                         G_CALLBACK(include_channel_changed), &controls);
    }

    gtk_table_set_row_spacing(table, row-1, 8);
    check = gtk_check_button_new_with_mnemonic(_("Plot point density map"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), args->plot_density);
    gtk_table_attach(table, check, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(check, "toggled",
                             G_CALLBACK(plot_density_changed), &controls);
    row++;

    update_nincluded(controls.args, info->blocksize);
    update_ok_sensitivity(&controls);
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

static gint
add_info_count(GtkTable *table, gint row, const gchar *description, gulong n)
{
    GtkWidget *label;
    gchar buf[16];

    label = gtk_label_new(description);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    g_snprintf(buf, sizeof(buf), "%lu", n);
    label = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    return row+1;
}

static void
update_ok_sensitivity(NMMXYZControls *controls)
{
    /* XXX: We might want to just plot the density but this is not implemented
     * by the regularisation function.  So require an actual channel to be
     * selected. */
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      GTK_RESPONSE_OK,
                                      controls->args->nincluded > 2);
}

static void
include_channel_changed(GtkToggleButton *toggle,
                        NMMXYZControls *controls)
{
    NMMXYZArgs *args = controls->args;
    guint i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(toggle), "id"));

    args->include_channel[i] = gtk_toggle_button_get_active(toggle);
    update_nincluded(args, controls->info->blocksize);
    update_ok_sensitivity(controls);
}

static void
plot_density_changed(NMMXYZControls *controls,
                     GtkToggleButton *toggle)
{
    NMMXYZArgs *args = controls->args;

    args->plot_density = gtk_toggle_button_get_active(toggle);
}

static gint
construct_resolutions(NMMXYZControls *controls,
                      GtkTable *table,
                      gint row)
{
    NMMXYZArgs *args = controls->args;
    GtkWidget *spin, *label, *button;

    gtk_table_attach(table, gwy_label_new_header(_("Resolution")),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Horizontal size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls->xres = gtk_adjustment_new(args->xres, 2, 16384, 1, 100, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls->xres), 0, 0);
    controls->xres_spin = spin;
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
    controls->yres_spin = spin;
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gtk_table_attach(table, spin, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new("px");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    button = gtk_check_button_new_with_mnemonic(_("Identical _measures"));
    controls->xymeasureeq = button;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), args->xymeasureeq);
    gtk_table_attach(table, button, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    if (args->xymeasureeq) {
        if (args->use_xres)
            gtk_entry_set_text(GTK_ENTRY(controls->yres_spin), "");
        else
            gtk_entry_set_text(GTK_ENTRY(controls->xres_spin), "");
    }

    g_signal_connect_swapped(controls->xres, "value-changed",
                             G_CALLBACK(xres_changed), controls);
    g_signal_connect_swapped(controls->yres, "value-changed",
                             G_CALLBACK(yres_changed), controls);
    g_signal_connect_swapped(controls->xymeasureeq, "toggled",
                             G_CALLBACK(xymeasureeq_changed), controls);

    return row;
}

static void
xres_changed(NMMXYZControls *controls, GtkAdjustment *adj)
{
    NMMXYZArgs *args = controls->args;

    args->xres = gwy_adjustment_get_int(adj);
    args->use_xres = TRUE;
    if (args->xymeasureeq)
        gtk_entry_set_text(GTK_ENTRY(controls->yres_spin), "");
}

static void
yres_changed(NMMXYZControls *controls, GtkAdjustment *adj)
{
    NMMXYZArgs *args = controls->args;

    args->yres = gwy_adjustment_get_int(adj);
    args->use_xres = FALSE;
    if (args->xymeasureeq)
        gtk_entry_set_text(GTK_ENTRY(controls->xres_spin), "");
}

static void
xymeasureeq_changed(NMMXYZControls *controls, GtkToggleButton *toggle)
{
    NMMXYZArgs *args = controls->args;

    args->xymeasureeq = gtk_toggle_button_get_active(toggle);
    if (args->xymeasureeq) {
        if (args->use_xres)
            gtk_entry_set_text(GTK_ENTRY(controls->yres_spin), "");
        else
            gtk_entry_set_text(GTK_ENTRY(controls->xres_spin), "");
    }
    else {
        if (args->use_xres) {
            g_signal_emit_by_name(controls->yres, "value-changed", NULL);
            args->use_xres = TRUE;
        }
        else {
            g_signal_emit_by_name(controls->xres, "value-changed", NULL);
            args->use_xres = FALSE;
        }
    }
}

static PointXYZ*
create_points_with_xy(GArray *data, guint nincluded)
{
    PointXYZ *points;
    const gdouble *d;
    gulong i, npts;

    gwy_debug("data->len %u, included block size %u", data->len, nincluded);
    d = (const gdouble*)data->data;
    npts = data->len/nincluded;
    points = g_new(PointXYZ, npts);
    gwy_debug("creating %lu XYZ points", npts);
    for (i = 0; i < npts; i++) {
        points[i].x = d[0];
        points[i].y = d[1];
        d += nincluded;
    }

    return points;
}

static void
find_data_range(const PointXYZ *points, NMMXYZInfo *info)
{
    gdouble xmin = G_MAXDOUBLE;
    gdouble ymin = G_MAXDOUBLE;
    gdouble xmax = -G_MAXDOUBLE;
    gdouble ymax = -G_MAXDOUBLE;
    guint i;

    gwy_debug("finding the XY range from %lu records", info->ndata);
    for (i = 0; i < info->ndata; i++) {
        const gdouble x = points[i].x, y = points[i].y;
        if (x < xmin)
            xmin = x;
        if (x > xmax)
            xmax = x;
        if (y < ymin)
            ymin = y;
        if (y > ymax)
            ymax = y;
    }

    info->xmin = xmin;
    info->xmax = xmax;
    info->ymin = ymin;
    info->ymax = ymax;
    gwy_debug("full data range [%g,%g]x[%g,%g]", xmin, xmax, ymin, ymax);
}

static void
create_data_field(GwyContainer *container,
                  const NMMXYZInfo *info,
                  const NMMXYZArgs *args,
                  GArray *dscs,
                  GArray *data,
                  PointXYZ *points,
                  guint i,
                  gboolean plot_density)
{
    const NMMXYZProfileDescription *dsc
        = &g_array_index(dscs, NMMXYZProfileDescription, i);
    GwyDataField *dfield, *density_map = NULL;
    gulong k, ndata;
    guint nincluded, xres, yres;
    gint id;
    const gdouble *d;
    const gchar *zunit = NULL;
    GQuark quark;
    gdouble q = 1.0;

    gwy_debug("regularising field #%u %s (%s)",
              i, dsc->short_name, dsc->long_name);
    xres = args->xres;
    yres = args->yres;
    if (args->xymeasureeq) {
        gdouble h;

        if (args->use_xres) {
            h = (info->xmax - info->xmin)/xres;
            yres = (guint)ceil((info->ymax - info->ymin)/h);
            yres = CLAMP(yres, 2, 32768);
        }
        else {
            h = (info->ymax - info->ymin)/yres;
            xres = (guint)ceil((info->xmax - info->xmin)/h);
            xres = CLAMP(xres, 2, 32768);
        }
    }
    gwy_debug("xres %u, yres %u", xres, yres);

    dfield = gwy_data_field_new(xres, yres,
                                info->xmax - info->xmin,
                                info->ymax - info->ymin,
                                FALSE);

    gwy_data_field_set_xoffset(dfield, info->xmin);
    gwy_data_field_set_yoffset(dfield, info->ymin);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");

    if (plot_density)
        density_map = gwy_data_field_new_alike(dfield, FALSE);

    if (gwy_stramong(dsc->short_name,
                     "Lz", "Az", "Az0", "Az1", "-Lz+Az", "XY vector", NULL))
        zunit = "m";
    else if (gwy_stramong(dsc->short_name, "Ax", NULL)) {
        zunit = "V";
        q = 10.0/65536.0;
    }
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), zunit);

    d = (const gdouble*)data->data + i;
    nincluded = args->nincluded;
    ndata = info->ndata;
    for (k = 0; k < ndata; k++) {
        points[k].z = *d;
        d += nincluded;
    }

    gwy_data_field_average_xyz(dfield, density_map, points, ndata);
    if (q != 1.0)
        gwy_data_field_multiply(dfield, q);

    id = i-2;
    quark = gwy_app_get_data_key_for_id(id);
    gwy_container_set_object(container, quark, dfield);
    g_object_unref(dfield);

    quark = gwy_app_get_data_title_key_for_id(id);
    gwy_container_set_const_string(container, quark, dsc->long_name);
    gwy_file_channel_import_log_add(container, id, NULL, info->filename);

    if (density_map) {
        id++;

        quark = gwy_app_get_data_key_for_id(id);
        gwy_container_set_object(container, quark, density_map);
        g_object_unref(density_map);

        quark = gwy_app_get_data_title_key_for_id(id);
        gwy_container_set_const_string(container, quark,
                                       _("Point density map"));
        gwy_file_channel_import_log_add(container, id, NULL, info->filename);
    }
}

/* Fills nfiles, blocksize and ndata fields of @info.  When @data is %NULL
 * the number of files and data is taken from headers.
 *
 * If non-NULL @data array is passed the raw data are immediately loaded.
 */
static gboolean
gather_data_files(const gchar *filename,
                  NMMXYZInfo *info, const NMMXYZArgs *args,
                  GArray *dscs, GArray *data,
                  GError **error)
{
    const NMMXYZProfileDescription *dsc;
    GArray *this_dscs = NULL;
    GDir *dir = NULL;
    const gchar *fname;
    gchar *dirname = NULL, *basename = NULL, *s;
    GString *str;
    gboolean ok = FALSE;
    guint nincluded = 0, oldblocksize, oldnfiles;

    oldblocksize = info->blocksize;
    oldnfiles = info->nfiles;

    info->nfiles = 0;
    info->ndata = 0;
    info->blocksize = 0;

    dirname = g_path_get_dirname(filename);
    basename = g_path_get_basename(filename);
    str = g_string_new(basename);
    g_free(basename);
    if ((s = strrchr(str->str, '.'))) {
        g_string_truncate(str, s - str->str);
        g_string_append(str, "_");
    }
    basename = g_strdup(str->str);

    gwy_debug("scanning dir <%s> for files <%s*%s>",
              dirname, basename, EXTENSION);
    dir = g_dir_open(dirname, 0, NULL);
    if (!dir)
        goto fail;

    this_dscs = g_array_new(FALSE, FALSE, sizeof(NMMXYZProfileDescription));
    while ((fname = g_dir_read_name(dir))) {
        gwy_debug("candidate file %s", fname);
        if (!g_str_has_prefix(fname, basename)
            || !g_str_has_suffix(fname, EXTENSION))
            continue;

        s = g_build_filename(dirname, fname, NULL);
        free_profile_descriptions(this_dscs, FALSE);
        read_profile_description(s, this_dscs);
        g_free(s);

        gwy_debug("found DSC file %s (%u records)", fname, this_dscs->len);
        if (!this_dscs->len)
            continue;

        /* Use the first reasonable .dsc file we find as the template and
         * require all other to be compatible. */
        if (!info->nfiles) {
            if (args->include_channel && this_dscs->len != oldblocksize) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_SPECIFIC,
                            _("Something is changing the data files on disk."));
                goto fail;
            }
            copy_profile_descriptions(this_dscs, dscs);
            info->blocksize = this_dscs->len;
            nincluded = (args->include_channel
                         ? args->nincluded
                         : info->blocksize);
            gwy_debug("setting nincluded to %u", nincluded);
        }
        else {
            if (!profile_descriptions_match(dscs, this_dscs)) {
                gwy_debug("non-matching profile descriptions for %s", fname);
                continue;
            }
        }

        info->nfiles++;

        /* Read the data if requested. */
        if (data) {
            gdouble f;

            s = g_build_filename(dirname, fname, NULL);
            g_string_assign(str, s);
            g_free(s);

            if ((s = strrchr(str->str, '.'))) {
                g_string_truncate(str, s - str->str);
                g_string_append(str, ".dat");
            }
            read_data_file(data, str->str, info->blocksize,
                           args->include_channel, args->nincluded);
            info->ndata = data->len/nincluded;

            f = (gdouble)info->nfiles/oldnfiles;
            f = CLAMP(f, 0.0, 1.0);
            if (!gwy_app_wait_set_fraction(f)) {
                err_CANCELLED(error);
                goto fail;
            }
        }
        else {
            dsc = &g_array_index(this_dscs, NMMXYZProfileDescription, 0);
            info->ndata += dsc->npts;
        }
    }

    /* This may not be correct.  There other checks to do... */
    ok = TRUE;

fail:
    free_profile_descriptions(this_dscs, TRUE);
    if (dir)
        g_dir_close(dir);
    g_string_free(str, TRUE);
    g_free(basename);
    g_free(dirname);

    return ok;
}

static void
read_data_file(GArray *data, const gchar *filename,
               guint nrec, const gboolean *include_channel, guint nincluded)
{
    static const gboolean floating_point_chars[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 1,
    };

    gchar *buffer = NULL, *line, *p, *end;
    gdouble *rec = NULL;
    gsize size;
    guint i, j, linecount = 0;

    g_assert(include_channel);
    gwy_debug("reading data file %s", filename);
    if (!g_file_get_contents(filename, &buffer, &size, NULL)) {
        gwy_debug("cannot read file %s", filename);
        goto fail;
    }

    p = buffer;
    rec = g_new(gdouble, nincluded);

    while ((line = gwy_str_next_line(&p))) {
        /* Data channels. */
        for (i = j = 0; i < nrec; i++) {
            /* Only read channels that were selected.  This saves both memory
             * and time. */
            if (include_channel[i]) {
                rec[j] = g_ascii_strtod(line, &end);
                if (end == line) {
                    gwy_debug("line %u terminated prematurely", linecount);
                    goto fail;
                }
                line = end;
                j++;
            }
            else {
                /* Skip whitespace and the next number so we end up at the
                 * beggining of next whitespace or line end.  Or garbage. */
                end = line;
                while (g_ascii_isspace(*line))
                    line++;
                while ((guchar)(*line) < G_N_ELEMENTS(floating_point_chars)
                       && floating_point_chars[(guchar)(*line)])
                    line++;

                if (end == line) {
                    gwy_debug("line %u terminated prematurely", linecount);
                    goto fail;
                }
            }
        }
        /* Now we have read a complete record so append it to the array. */
        g_array_append_vals(data, rec, nincluded);
        linecount++;
    }

    gwy_debug("read %u records", linecount);

fail:
    g_free(rec);
    g_free(buffer);
}

static gboolean
profile_descriptions_match(GArray *descs1, GArray *descs2)
{
    guint i;

    if (descs1->len != descs2->len) {
        gwy_debug("non-matching channel numbers %u vs %u",
                  descs1->len, descs2->len);
        return FALSE;
    }

    for (i = 0; i < descs1->len; i++) {
        const NMMXYZProfileDescription *desc1
            = &g_array_index(descs1, NMMXYZProfileDescription, i);
        const NMMXYZProfileDescription *desc2
            = &g_array_index(descs2, NMMXYZProfileDescription, i);

        if (!gwy_strequal(desc1->short_name, desc2->short_name)) {
            gwy_debug("non-matching channel names %s vs %s",
                      desc1->short_name, desc2->short_name);
            return FALSE;
        }
    }

    return TRUE;
}

static void
read_profile_description(const gchar *filename, GArray *dscs)
{
    NMMXYZProfileDescription dsc;
    gchar *buffer = NULL, *line, *p;
    gchar **pieces = NULL;
    gsize size;
    guint i;

    if (!g_file_get_contents(filename, &buffer, &size, NULL))
        goto fail;

    p = buffer;
    if (!(line = gwy_str_next_line(&p)) || !gwy_strequal(line, DASHED_LINE))
        goto fail;

    g_assert(!dscs->len);
    while ((line = gwy_str_next_line(&p))) {
        if (gwy_strequal(line, DASHED_LINE))
            break;

        pieces = g_strsplit(line, " : ", -1);
        if (g_strv_length(pieces) != 5)
            goto fail;

        dsc.id = atoi(pieces[0]);
        dsc.date = g_strdup(pieces[1]);
        dsc.short_name = g_strdup(pieces[2]);
        dsc.npts = atoi(pieces[3]);
        dsc.long_name = g_strdup(pieces[4]);
        g_array_append_val(dscs, dsc);
    }

    /* The ids should match the line numbers. */
    for (i = 0; i < dscs->len; i++) {
        if (g_array_index(dscs, NMMXYZProfileDescription, i).id != i) {
            gwy_debug("non-matching channel id #%u", i);
            goto fail;
        }
    }

    /* We cannot read files with different number of points for each channel. */
    for (i = 1; i < dscs->len; i++) {
        if (g_array_index(dscs, NMMXYZProfileDescription, i).npts
            != g_array_index(dscs, NMMXYZProfileDescription, i-1).npts) {
            gwy_debug("non-matching number of points per channel #%u", i);
            goto fail;
        }
    }

fail:
    g_free(buffer);
    if (pieces)
        g_strfreev(pieces);
}

static void
free_profile_descriptions(GArray *dscs, gboolean free_array)
{
    guint i;

    if (!dscs)
        return;

    for (i = 0; i < dscs->len; i++) {
        NMMXYZProfileDescription *dsc
            = &g_array_index(dscs, NMMXYZProfileDescription, i);

        g_free(dsc->date);
        g_free(dsc->short_name);
        g_free(dsc->long_name);
    }

    if (free_array)
        g_array_free(dscs, TRUE);
    else
        g_array_set_size(dscs, 0);
}

static void
copy_profile_descriptions(GArray *source, GArray *dest)
{
    guint i;

    g_assert(source);
    g_assert(dest);
    g_assert(!dest->len);

    for (i = 0; i < source->len; i++) {
        NMMXYZProfileDescription dsc
            = g_array_index(source, NMMXYZProfileDescription, i);

        dsc.date = g_strdup(dsc.date);
        dsc.short_name = g_strdup(dsc.short_name);
        dsc.long_name = g_strdup(dsc.long_name);
        g_array_append_val(dest, dsc);
    }
}

static const gchar include_channel_prefix[] = "/module/nmmxyz/include_channel/";

static const gchar plot_density_key[] = "/module/nmmxyz/plot-density";
static const gchar use_xres_key[]     = "/module/nmmxyz/use_xres";
static const gchar xres_key[]         = "/module/nmmxyz/xres";
static const gchar xymeasureeq_key[]  = "/module/nmmxyz/xymeasureeq";
static const gchar yres_key[]         = "/module/nmmxyz/yres";

static void
sanitize_channel_id(gchar *s)
{
    gchar *t = s;

    while (*s) {
        if (g_ascii_isalnum(*s) || *s == '-' || *s == '+' || *s == '_') {
            *t = *s;
            t++;
        }
        s++;
    }
    *t = '\0';
}

static void
nmmxyz_sanitize_args(NMMXYZArgs *args)
{
    args->xres = CLAMP(args->xres, 2, 16384);
    args->yres = CLAMP(args->yres, 2, 16384);
    args->plot_density = !!args->plot_density;
    args->xymeasureeq = !!args->xymeasureeq;
    args->use_xres = !!args->use_xres;
}

static void
nmmxyz_load_args(GwyContainer *container,
                 NMMXYZArgs *args,
                 GArray *dscs)
{
    guint i, blocksize;

    gwy_container_gis_int32_by_name(container, xres_key, &args->xres);
    gwy_container_gis_int32_by_name(container, yres_key, &args->yres);
    gwy_container_gis_boolean_by_name(container, plot_density_key,
                                      &args->plot_density);
    gwy_container_gis_boolean_by_name(container, xymeasureeq_key,
                                      &args->xymeasureeq);
    gwy_container_gis_boolean_by_name(container, use_xres_key, &args->use_xres);
    nmmxyz_sanitize_args(args);

    blocksize = dscs->len;
    args->include_channel = g_new0(gboolean, blocksize);
    args->include_channel[0] = args->include_channel[1] = TRUE;
    for (i = 2; i < blocksize; i++) {
        const NMMXYZProfileDescription *dsc
            = &g_array_index(dscs, NMMXYZProfileDescription, i);
        gchar *id = g_strdup(dsc->short_name);
        gchar *key;

        sanitize_channel_id(id);
        key = g_strconcat(include_channel_prefix, id, NULL);
        g_free(id);
        gwy_container_gis_boolean_by_name(container, key,
                                          args->include_channel + i);
        g_free(key);
    }
}

static void
nmmxyz_save_args(GwyContainer *container,
                 NMMXYZArgs *args,
                 GArray *dscs)
{
    guint i, blocksize;

    gwy_container_set_int32_by_name(container, xres_key, args->xres);
    gwy_container_set_int32_by_name(container, yres_key, args->yres);
    gwy_container_set_boolean_by_name(container, plot_density_key,
                                      args->plot_density);
    gwy_container_set_boolean_by_name(container, xymeasureeq_key,
                                      args->xymeasureeq);
    gwy_container_set_boolean_by_name(container, use_xres_key, args->use_xres);

    blocksize = dscs->len;
    for (i = 2; i < blocksize; i++) {
        const NMMXYZProfileDescription *dsc
            = &g_array_index(dscs, NMMXYZProfileDescription, i);
        gchar *id = g_strdup(dsc->short_name);
        gchar *key;

        sanitize_channel_id(id);
        key = g_strconcat(include_channel_prefix, id, NULL);
        g_free(id);
        gwy_container_set_boolean_by_name(container, key,
                                          args->include_channel[i]);
        g_free(key);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
