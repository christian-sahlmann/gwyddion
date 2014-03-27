/*
 *  @(#) $Id$
 *  Copyright (C) 2013-2014 David Necas (Yeti).
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
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/correct.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define AFFINE_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400,
};

enum {
    RESPONSE_RESET = 1,
    USER_DEFINED_LATTICE = -1,
};

enum {
    SENS_USER_LATTICE = 1,
    SENS_DIFFERENT_LENGTHS = 2,
    SENS_VALID_LATTICE = 4,
};

enum {
    INVALID_A1 = 1,
    INVALID_A2 = 2,
    INVALID_PHI = 4,
    INVALID_SEL = 8,
};

typedef enum {
    IMAGE_DATA,
    IMAGE_ACF,
    IMAGE_CORRECTED,
} ImageMode;

typedef enum {
    SCALING_AS_GIVEN,
    SCALING_PRESERVE_AREA,
    SCALING_PRESERVE_X,
} ScalingType;

typedef struct {
    gdouble a1;
    gdouble a2;
    gdouble phi;
} LatticePreset;

typedef struct {
    gdouble a1;
    gdouble a2;
    gdouble phi;
    gboolean different_lengths;
    gboolean avoid_rotation;
    GwyInterpolationType interp;
    ScalingType scaling;
    gint preset;

    ImageMode image_mode;
} AffcorArgs;

typedef struct {
    AffcorArgs *args;
    GwySensitivityGroup *sens;
    GtkWidget *dialog;
    GtkWidget *view;
    GwyVectorLayer *vlayer;
    GwySelection *selection;
    GwyContainer *mydata;
    GSList *image_mode;
    GtkWidget *acffield;
    GtkWidget *interp;
    GtkWidget *scaling;
    GwySIValueFormat *vf;
    GwySIValueFormat *vfphi;
    /* Actual */
    GtkWidget *a1_x;
    GtkWidget *a1_y;
    GtkWidget *a1_len;
    GtkWidget *a1_phi;
    GtkWidget *a2_x;
    GtkWidget *a2_y;
    GtkWidget *a2_len;
    GtkWidget *a2_phi;
    GtkWidget *phi;
    GtkWidget *refine;
    GtkWidget *preset;
    gdouble xy[4];
    /* Correct (wanted) */
    GtkWidget *a1_corr;
    GtkWidget *different_lengths;
    GtkWidget *a2_corr;
    GtkWidget *phi_corr;
    guint invalid_corr;
    gboolean calculated;
    gulong recalculate_id;
} AffcorControls;

static gboolean   module_register          (void);
static void       correct_affine           (GwyContainer *data,
                                            GwyRunType run);
static gint       affcor_dialog            (AffcorArgs *args,
                                            GwyContainer *data,
                                            GwyDataField *dfield,
                                            gint id);
static GtkWidget* make_lattice_table       (AffcorControls *controls);
static GtkWidget* add_lattice_entry        (GtkTable *table,
                                            const gchar *name,
                                            gdouble value,
                                            GwySensitivityGroup *sens,
                                            guint flags,
                                            gint *row,
                                            GwySIValueFormat *vf);
static gboolean   filter_acffield          (GwyContainer *data,
                                            gint id,
                                            gpointer user_data);
static void       a1_changed_manually      (GtkEntry *entry,
                                            AffcorControls *controls);
static void       a2_changed_manually      (GtkEntry *entry,
                                            AffcorControls *controls);
static void       init_selection           (GwySelection *selection,
                                            GwyDataField *dfield);
static void       image_mode_changed       (GtkToggleButton *button,
                                            AffcorControls *controls);
static void       preset_changed           (GtkComboBox *combo,
                                            AffcorControls *controls);
static void       a1_changed               (AffcorControls *controls,
                                            GtkEntry *entry);
static void       a2_changed               (AffcorControls *controls,
                                            GtkEntry *entry);
static void       phi_changed              (AffcorControls *controls,
                                            GtkEntry *entry);
static void       acffield_changed         (AffcorControls *controls,
                                            GwyDataChooser *chooser);
static void       calculate_acffield       (AffcorControls *controls,
                                            GwyDataField *dfield);
static void       different_lengths_toggled(AffcorControls *controls,
                                            GtkToggleButton *toggle);
static void       refine                   (AffcorControls *controls);
static void       selection_changed        (AffcorControls *controls);
static void       interp_changed           (GtkComboBox *combo,
                                            AffcorControls *controls);
static void       scaling_changed          (GtkComboBox *combo,
                                            AffcorControls *controls);
static void       invalidate               (AffcorControls *controls);
static gboolean   recalculate              (gpointer user_data);
static void       do_correction            (AffcorControls *controls);
static void       corner_max               (gdouble x,
                                            gdouble y,
                                            const gdouble *m,
                                            gdouble *vmax);
static void       solve_transform_real     (const gdouble *a1a2,
                                            const gdouble *a1a2_corr,
                                            gdouble *m);
static void       find_maximum             (GwyDataField *dfield,
                                            gdouble *x,
                                            gdouble *y,
                                            gint xwinsize,
                                            gint ywinsize);
static void       matrix_vector            (gdouble *dest,
                                            const gdouble *m,
                                            const gdouble *src);
static void       matrix_matrix            (gdouble *dest,
                                            const gdouble *m,
                                            const gdouble *src);
static void       invert_matrix            (gdouble *dest,
                                            const gdouble *src);
static gdouble    matrix_det               (const gdouble *m);
static void       affcor_load_args         (GwyContainer *container,
                                            AffcorArgs *args);
static void       affcor_save_args         (GwyContainer *container,
                                            AffcorArgs *args);
static void       affcor_sanitize_args     (AffcorArgs *args);

static const AffcorArgs affcor_defaults = {
    1.0, 1.0, 90.0, FALSE,
    TRUE,
    GWY_INTERPOLATION_LINEAR, SCALING_AS_GIVEN,
    -1,
    IMAGE_DATA,
};

static const LatticePreset lattice_presets[] = {
    { 2.46e-10, 2.46e-10, G_PI/3.0 },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Corrects affine distortion of images by matching image Bravais "
       "lattice to the true one."),
    "Yeti <yeti@gwyddion.net>",
    "1.3",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("correct_affine",
                              (GwyProcessFunc)&correct_affine,
                              N_("/_Correct Data/_Affine Distortion..."),
                              NULL,
                              AFFINE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Correct affine distortion"));

    return TRUE;
}

static void
correct_affine(GwyContainer *data, GwyRunType run)
{
    AffcorArgs args;
    GwyDataField *dfield;
    gint id, newid;

    g_return_if_fail(run & AFFINE_RUN_MODES);
    g_return_if_fail(g_type_from_name("GwyLayerLattice"));
    affcor_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    newid = affcor_dialog(&args, data, dfield, id);
    affcor_save_args(gwy_app_settings_get(), &args);
    if (newid != -1)
        gwy_app_channel_log_add(data, id, newid, "proc::correct_affine", NULL);
}

static gint
affcor_dialog(AffcorArgs *args,
              GwyContainer *data,
              GwyDataField *dfield,
              gint id)
{
    GtkWidget *hbox, *label, *button, *lattable, *alignment;
    GtkDialog *dialog;
    GtkTable *table;
    GwyDataField *corrected;
    AffcorControls controls;
    gint response, row, newid = -1;
    GwyPixmapLayer *layer;
    GwyVectorLayer *vlayer;
    GwySIUnit *unitphi;
    guint flags;

    gwy_clear(&controls, 1);
    controls.args = args;
    controls.sens = gwy_sensitivity_group_new();

    controls.dialog = gtk_dialog_new_with_buttons(_("Affine Correction"),
                                                  NULL, 0, NULL);
    dialog = GTK_DIALOG(controls.dialog);
    gtk_dialog_add_button(dialog, _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    button = gtk_dialog_add_button(dialog, GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);
    gwy_sensitivity_group_add_widget(controls.sens, button,
                                     SENS_VALID_LATTICE);

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    calculate_acffield(&controls, dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 1, FALSE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    alignment = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), alignment, FALSE, FALSE, 4);

    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);

    vlayer = g_object_new(g_type_from_name("GwyLayerLattice"),
                          "selection-key", "/0/select/vector",
                          NULL);
    controls.vlayer = g_object_ref(vlayer);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view), vlayer);
    controls.selection = gwy_vector_layer_ensure_selection(vlayer);
    gwy_selection_set_max_objects(controls.selection, 1);
    g_signal_connect_swapped(controls.selection, "changed",
                             G_CALLBACK(selection_changed), &controls);

    gtk_container_add(GTK_CONTAINER(alignment), controls.view);

    table = GTK_TABLE(gtk_table_new(15, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(_("Preview:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.image_mode
        = gwy_radio_buttons_createl(G_CALLBACK(image_mode_changed), &controls,
                                    args->image_mode,
                                    _("_Data"), IMAGE_DATA,
                                    _("2D _ACF"), IMAGE_ACF,
                                    _("Correc_ted data"), IMAGE_CORRECTED,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.image_mode,
                                            table, 4, row);
    button = gwy_radio_buttons_find(controls.image_mode, IMAGE_CORRECTED);
    gwy_sensitivity_group_add_widget(controls.sens, button,
                                     SENS_VALID_LATTICE);
    gtk_table_set_row_spacing(table, row-1, 8);

    label = gwy_label_new_header(_("Lattice Vectors"));
    gtk_table_attach(table, label, 0, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.vf
        = gwy_data_field_get_value_format_xy(dfield,
                                             GWY_SI_UNIT_FORMAT_MARKUP, NULL);
    controls.vf->precision += 2;

    unitphi = gwy_si_unit_new("deg");
    controls.vfphi
        = gwy_si_unit_get_format_with_resolution(unitphi,
                                                 GWY_SI_UNIT_FORMAT_MARKUP,
                                                 180.0, 0.01, NULL);
    g_object_unref(unitphi);

    lattable = make_lattice_table(&controls);
    gtk_table_attach(table, lattable,
                     0, 5, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.refine, "clicked",
                             G_CALLBACK(refine), &controls);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    /* TRANSLATORS: Correct is an adjective here. */
    label = gwy_label_new_header(_("Correct Lattice"));
    gtk_table_attach(table, label, 0, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Lattice type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 2, row, row+1, GTK_FILL, 0, 0, 0);

    controls.preset
        = gwy_enum_combo_box_newl(G_CALLBACK(preset_changed), &controls,
                                  args->preset,
                                  _("User defined"), USER_DEFINED_LATTICE,
                                  "HOPG", 0,
                                  NULL);
    gtk_table_attach(table, controls.preset,
                     2, 5, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.preset);
    row++;

    controls.a1_corr = add_lattice_entry(table, "a<sub>1</sub>:", args->a1,
                                         controls.sens, SENS_USER_LATTICE,
                                         &row, controls.vf);
    g_signal_connect_swapped(controls.a1_corr, "changed",
                             G_CALLBACK(a1_changed), &controls);

    controls.different_lengths
        = gtk_check_button_new_with_mnemonic(_("_Different lengths"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.different_lengths),
                                 args->different_lengths);
    gwy_sensitivity_group_add_widget(controls.sens, controls.different_lengths,
                                     SENS_USER_LATTICE);
    gtk_table_attach(table, controls.different_lengths,
                     3, 5, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.different_lengths, "toggled",
                             G_CALLBACK(different_lengths_toggled), &controls);

    controls.a2_corr = add_lattice_entry(table, "a<sub>2</sub>:", args->a2,
                                         controls.sens,
                                         SENS_USER_LATTICE
                                         | SENS_DIFFERENT_LENGTHS,
                                         &row, controls.vf);
    g_signal_connect_swapped(controls.a2_corr, "changed",
                             G_CALLBACK(a2_changed), &controls);

    controls.phi_corr = add_lattice_entry(table, "ϕ:", args->phi*180.0/G_PI,
                                          controls.sens, SENS_USER_LATTICE,
                                          &row, controls.vfphi);
    g_signal_connect_swapped(controls.phi_corr, "changed",
                             G_CALLBACK(phi_changed), &controls);
    gtk_table_set_row_spacing(table, row-1, 8);

    label = gwy_label_new_header(_("Options"));
    gtk_table_attach(table, label, 0, 5, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("Image for _ACF:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 2, row, row+1, GTK_FILL, 0, 0, 0);

    controls.acffield = gwy_data_chooser_new_channels();
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(controls.acffield),
                                filter_acffield, &controls, NULL);
    gwy_data_chooser_set_active(GWY_DATA_CHOOSER(controls.acffield), data, id);
    gtk_table_attach(table, controls.acffield,
                     2, 5, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.acffield, "changed",
                             G_CALLBACK(acffield_changed), &controls);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Interpolation type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 2, row, row+1, GTK_FILL, 0, 0, 0);

    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(interp_changed), &controls,
                                 args->interp, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.interp);
    gtk_table_attach(table, controls.interp,
                     2, 5, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Scaling:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 2, row, row+1, GTK_FILL, 0, 0, 0);

    controls.scaling
        = gwy_enum_combo_box_newl(G_CALLBACK(scaling_changed), &controls,
                                  args->scaling,
                                  _("Exactly as specified"), SCALING_AS_GIVEN,
                                  _("Preserve area"), SCALING_PRESERVE_AREA,
                                  _("Preserve X scale"), SCALING_PRESERVE_X,
                                  NULL);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.scaling);
    gtk_table_attach(table, controls.scaling,
                     2, 5, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    init_selection(controls.selection, dfield);
    flags = args->different_lengths ? SENS_DIFFERENT_LENGTHS : 0;
    gwy_sensitivity_group_set_state(controls.sens,
                                    SENS_DIFFERENT_LENGTHS, flags);
    preset_changed(GTK_COMBO_BOX(controls.preset), &controls);

    gtk_widget_show_all(controls.dialog);
    do {
        response = gtk_dialog_run(dialog);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(controls.dialog);
            case GTK_RESPONSE_NONE:
            goto finalize;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            init_selection(controls.selection, dfield);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    if (!controls.calculated)
        do_correction(&controls);

    corrected = gwy_container_get_object_by_name(controls.mydata, "/2/data");
    newid = gwy_app_data_browser_add_data_field(corrected, data, TRUE);
    gwy_app_set_data_field_title(data, newid, "Corrected");
    gwy_app_sync_data_items(data, data, id, newid, FALSE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_GRADIENT,
                            0);

    gtk_widget_destroy(controls.dialog);
finalize:
    if (controls.recalculate_id)
        g_source_remove(controls.recalculate_id);
    g_object_unref(controls.vlayer);
    g_object_unref(controls.sens);
    gwy_si_unit_value_format_free(controls.vf);
    gwy_si_unit_value_format_free(controls.vfphi);
    g_object_unref(controls.mydata);

    return newid;
}

static GtkWidget*
make_lattice_table(AffcorControls *controls)
{
    GtkWidget *table, *label, *button, *entry;
    GString *str = g_string_new(NULL);

    table = gtk_table_new(4, 5, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);

    /* header row */
    g_string_assign(str, "x");
    if (strlen(controls->vf->units))
        g_string_append_printf(str, " [%s]", controls->vf->units);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);

    g_string_assign(str, "y");
    if (strlen(controls->vf->units))
        g_string_append_printf(str, " [%s]", controls->vf->units);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);

    g_string_assign(str, _("length"));
    if (strlen(controls->vf->units))
        g_string_append_printf(str, " [%s]", controls->vf->units);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 0, 1, GTK_FILL, 0, 0, 0);

    g_string_assign(str, _("angle"));
    if (strlen(controls->vfphi->units))
        g_string_append_printf(str, " [%s]", controls->vfphi->units);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), str->str);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 0, 1, GTK_FILL, 0, 0, 0);

    /* a1 */
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "a<sub>1</sub>:");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

    controls->a1_x = entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 8);
    g_object_set_data(G_OBJECT(entry), "id", (gpointer)"x");
    gwy_widget_set_activate_on_unfocus(entry, TRUE);
    gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
    g_signal_connect(entry, "activate",
                     G_CALLBACK(a1_changed_manually), controls);

    controls->a1_y = entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 8);
    g_object_set_data(G_OBJECT(entry), "id", (gpointer)"y");
    gwy_widget_set_activate_on_unfocus(entry, TRUE);
    gtk_table_attach(GTK_TABLE(table), entry, 2, 3, 1, 2, GTK_FILL, 0, 0, 0);
    g_signal_connect(entry, "activate",
                     G_CALLBACK(a1_changed_manually), controls);

    controls->a1_len = entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 8);
    g_object_set_data(G_OBJECT(entry), "id", (gpointer)"len");
    gwy_widget_set_activate_on_unfocus(entry, TRUE);
    gtk_table_attach(GTK_TABLE(table), entry, 3, 4, 1, 2, GTK_FILL, 0, 0, 0);
    g_signal_connect(entry, "activate",
                     G_CALLBACK(a1_changed_manually), controls);

    controls->a1_phi = entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 8);
    g_object_set_data(G_OBJECT(entry), "id", (gpointer)"phi");
    gwy_widget_set_activate_on_unfocus(entry, TRUE);
    gtk_table_attach(GTK_TABLE(table), entry, 4, 5, 1, 2, GTK_FILL, 0, 0, 0);
    g_signal_connect(entry, "activate",
                     G_CALLBACK(a1_changed_manually), controls);

    /* a2 */
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "a<sub>2</sub>:");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);

    controls->a2_x = entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 8);
    g_object_set_data(G_OBJECT(entry), "id", (gpointer)"x");
    gwy_widget_set_activate_on_unfocus(entry, TRUE);
    gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);
    g_signal_connect(entry, "activate",
                     G_CALLBACK(a2_changed_manually), controls);

    controls->a2_y = entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 8);
    g_object_set_data(G_OBJECT(entry), "id", (gpointer)"y");
    gwy_widget_set_activate_on_unfocus(entry, TRUE);
    gtk_table_attach(GTK_TABLE(table), entry, 2, 3, 2, 3, GTK_FILL, 0, 0, 0);
    g_signal_connect(entry, "activate",
                     G_CALLBACK(a2_changed_manually), controls);

    controls->a2_len = entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 8);
    g_object_set_data(G_OBJECT(entry), "id", (gpointer)"len");
    gwy_widget_set_activate_on_unfocus(entry, TRUE);
    gtk_table_attach(GTK_TABLE(table), entry, 3, 4, 2, 3, GTK_FILL, 0, 0, 0);
    g_signal_connect(entry, "activate",
                     G_CALLBACK(a2_changed_manually), controls);

    controls->a2_phi = entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 8);
    g_object_set_data(G_OBJECT(entry), "id", (gpointer)"phi");
    gwy_widget_set_activate_on_unfocus(entry, TRUE);
    gtk_table_attach(GTK_TABLE(table), entry, 4, 5, 2, 3, GTK_FILL, 0, 0, 0);
    g_signal_connect(entry, "activate",
                     G_CALLBACK(a2_changed_manually), controls);

    /* phi */
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "ϕ:");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 3, 4, GTK_FILL, 0, 0, 0);

    controls->phi = label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), .0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 3, 4, GTK_FILL, 0, 0, 0);

    controls->refine = button = gtk_button_new_with_mnemonic(_("Re_fine"));
    gtk_table_attach(GTK_TABLE(table), button,
                     0, 3, 3, 4, GTK_FILL, 0, 0, 0);

    g_string_free(str, TRUE);

    return table;
}

static GtkWidget*
add_lattice_entry(GtkTable *table,
                  const gchar *name,
                  gdouble value,
                  GwySensitivityGroup *sens,
                  guint flags,
                  gint *row,
                  GwySIValueFormat *vf)
{
    GtkWidget *label, *entry;
    gchar *buf;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, *row, *row+1, GTK_FILL, 0, 0, 0);
    gwy_sensitivity_group_add_widget(sens, label, flags);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), vf->units);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 2, 3, *row, *row+1, GTK_FILL, 0, 0, 0);
    gwy_sensitivity_group_add_widget(sens, label, flags);

    entry = gtk_entry_new();
    buf = g_strdup_printf("%g", value);
    gtk_entry_set_text(GTK_ENTRY(entry), buf);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 6);
    g_free(buf);
    gtk_table_attach(table, entry,
                     1, 2, *row, *row+1, GTK_FILL, 0, 0, 0);
    gwy_sensitivity_group_add_widget(sens, entry, flags);

    (*row)++;

    return entry;
}

static gboolean
filter_acffield(GwyContainer *data, gint id, gpointer user_data)
{
    AffcorControls *controls = (AffcorControls*)user_data;
    GwyDataField *dfield, *acffield;
    gdouble r;

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    acffield = gwy_container_get_object(data, gwy_app_get_data_key_for_id(id));
    /* Do not check value, we may want to align channels of a different
     * physical quantity.  But check order-of-magnitude pixel size for
     * elementary sanity. */
    if (gwy_data_field_check_compatibility(dfield, acffield,
                                           GWY_DATA_COMPATIBILITY_LATERAL))
        return FALSE;

    r = (gwy_data_field_get_xmeasure(dfield)
         /gwy_data_field_get_xmeasure(acffield));
    if (r > 16.0 || r < 1.0/16.0)
        return FALSE;

    r = (gwy_data_field_get_ymeasure(dfield)
         /gwy_data_field_get_ymeasure(acffield));
    if (r > 16.0 || r < 1.0/16.0)
        return FALSE;

    return TRUE;
}

static void
a1_changed_manually(GtkEntry *entry,
                    AffcorControls *controls)
{
    GwySIValueFormat *vf = controls->vf;
    gdouble x, y, len, phi;
    const gchar *id, *text;
    gdouble value;

    id = g_object_get_data(G_OBJECT(entry), "id");
    text = gtk_entry_get_text(GTK_ENTRY(entry));
    value = g_strtod(text, NULL);

    x = controls->xy[0];
    y = -controls->xy[1];
    len = hypot(x, y);
    phi = atan2(y, x);
    if (gwy_strequal(id, "x"))
        controls->xy[0] = vf->magnitude * value;
    else if (gwy_strequal(id, "y"))
        controls->xy[1] = vf->magnitude * -value;
    else if (gwy_strequal(id, "len")) {
        controls->xy[0] = vf->magnitude * value * cos(phi);
        controls->xy[1] = vf->magnitude * value * -sin(phi);
    }
    else if (gwy_strequal(id, "phi")) {
        phi = G_PI/180.0 * value;
        controls->xy[0] = len * cos(phi);
        controls->xy[1] = len * -sin(phi);
    }

    /* This actually recalculates everything.  But it does not activate
     * entries so we will not recurse. */
    gwy_selection_set_data(controls->selection, 1, controls->xy);
}

static void
a2_changed_manually(GtkEntry *entry,
                    AffcorControls *controls)
{
    GwySIValueFormat *vf = controls->vf;
    gdouble x, y, len, phi;
    const gchar *id, *text;
    gdouble value;

    id = g_object_get_data(G_OBJECT(entry), "id");
    text = gtk_entry_get_text(GTK_ENTRY(entry));
    value = g_strtod(text, NULL);

    x = controls->xy[2];
    y = -controls->xy[3];
    len = hypot(x, y);
    phi = atan2(y, x);
    if (gwy_strequal(id, "x"))
        controls->xy[2] = vf->magnitude * value;
    else if (gwy_strequal(id, "y"))
        controls->xy[3] = vf->magnitude * -value;
    else if (gwy_strequal(id, "len")) {
        controls->xy[2] = vf->magnitude * value * cos(phi);
        controls->xy[3] = vf->magnitude * value * -sin(phi);
    }
    else if (gwy_strequal(id, "phi")) {
        phi = G_PI/180.0 * value;
        controls->xy[2] = len * cos(phi);
        controls->xy[3] = len * -sin(phi);
    }

    /* This actually recalculates everything.  But it does not activate
     * entries so we will not recurse. */
    gwy_selection_set_data(controls->selection, 1, controls->xy);
}

static void
init_selection(GwySelection *selection,
               GwyDataField *dfield)
{
    gdouble xy[4] = { 0.0, 0.0, 0.0, 0.0 };

    xy[0] = dfield->xreal/20;
    xy[3] = -dfield->yreal/20;
    gwy_selection_set_data(selection, 1, xy);
}

static void
image_mode_changed(G_GNUC_UNUSED GtkToggleButton *button,
                   AffcorControls *controls)
{
    AffcorArgs *args = controls->args;
    GwyDataView *dataview;
    GwyPixmapLayer *layer;
    ImageMode mode;

    mode = gwy_radio_buttons_get_current(controls->image_mode);
    if (mode == args->image_mode)
        return;
    args->image_mode = mode;
    dataview = GWY_DATA_VIEW(controls->view);
    layer = gwy_data_view_get_base_layer(dataview);

    if (args->image_mode == IMAGE_DATA) {
        gwy_pixmap_layer_set_data_key(layer, "/0/data");
        if (!gwy_data_view_get_top_layer(dataview))
            gwy_data_view_set_top_layer(dataview, controls->vlayer);
    }
    else if (args->image_mode == IMAGE_ACF) {
        gwy_pixmap_layer_set_data_key(layer, "/1/data");
        if (!gwy_data_view_get_top_layer(dataview))
            gwy_data_view_set_top_layer(dataview, controls->vlayer);
    }
    else if (args->image_mode == IMAGE_CORRECTED) {
        if (!controls->calculated)
            do_correction(controls);
        gwy_pixmap_layer_set_data_key(layer, "/2/data");
        gwy_data_view_set_top_layer(dataview, NULL);
    }

    gwy_set_data_preview_size(dataview, PREVIEW_SIZE);
}

static void
preset_changed(GtkComboBox *combo,
               AffcorControls *controls)
{
    AffcorArgs *args = controls->args;
    const LatticePreset *preset;
    gboolean different_lengths;
    GString *str;

    args->preset = gwy_enum_combo_box_get_active(combo);
    if (args->preset == USER_DEFINED_LATTICE) {
        gwy_sensitivity_group_set_state(controls->sens,
                                        SENS_USER_LATTICE, SENS_USER_LATTICE);
        return;
    }

    preset = lattice_presets + args->preset;
    different_lengths = (preset->a1 != preset->a2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->different_lengths),
                                 different_lengths);

    str = g_string_new(NULL);
    g_string_printf(str, "%g", preset->a1/controls->vf->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->a1_corr), str->str);
    g_string_printf(str, "%g", preset->a2/controls->vf->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->a2_corr), str->str);
    g_string_printf(str, "%g", preset->phi/G_PI*180.0/controls->vfphi->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->phi_corr), str->str);
    g_string_free(str, TRUE);

    gwy_sensitivity_group_set_state(controls->sens, SENS_USER_LATTICE, 0);
}

static void
a1_changed(AffcorControls *controls,
           GtkEntry *entry)
{
    AffcorArgs *args = controls->args;
    const gchar *buf;
    guint flags;

    buf = gtk_entry_get_text(entry);
    args->a1 = g_strtod(buf, NULL);
    if (args->a1 > 0.0)
        controls->invalid_corr &= ~INVALID_A1;
    else
        controls->invalid_corr |= INVALID_A1;

    if (!args->different_lengths)
        gtk_entry_set_text(GTK_ENTRY(controls->a2_corr), buf);

    flags = controls->invalid_corr ? 0 : SENS_VALID_LATTICE;
    gwy_sensitivity_group_set_state(controls->sens, SENS_VALID_LATTICE, flags);
    invalidate(controls);
}

static void
a2_changed(AffcorControls *controls,
           GtkEntry *entry)
{
    AffcorArgs *args = controls->args;
    const gchar *buf;
    guint flags;

    buf = gtk_entry_get_text(entry);
    args->a2 = g_strtod(buf, NULL);
    if (args->a2 > 0.0)
        controls->invalid_corr &= ~INVALID_A2;
    else
        controls->invalid_corr |= INVALID_A2;

    flags = controls->invalid_corr ? 0 : SENS_VALID_LATTICE;
    gwy_sensitivity_group_set_state(controls->sens, SENS_VALID_LATTICE, flags);
    invalidate(controls);
}

static void
phi_changed(AffcorControls *controls,
            GtkEntry *entry)
{
    AffcorArgs *args = controls->args;
    const gchar *buf;
    guint flags;

    buf = gtk_entry_get_text(entry);
    args->phi = g_strtod(buf, NULL)*G_PI/180.0;
    if (args->phi > 1e-3 && args->phi < G_PI - 1e-3)
        controls->invalid_corr &= ~INVALID_PHI;
    else
        controls->invalid_corr |= INVALID_PHI;

    flags = controls->invalid_corr ? 0 : SENS_VALID_LATTICE;
    gwy_sensitivity_group_set_state(controls->sens, SENS_VALID_LATTICE, flags);
    invalidate(controls);
}

static void
acffield_changed(AffcorControls *controls,
                 GwyDataChooser *chooser)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gint id;

    data = gwy_data_chooser_get_active(chooser, &id);
    g_return_if_fail(data);
    dfield = gwy_container_get_object(data, gwy_app_get_data_key_for_id(id));
    calculate_acffield(controls, dfield);
}

static void
calculate_acffield(AffcorControls *controls,
                   GwyDataField *dfield)
{
    GwyDataField *acf;
    guint acfwidth, acfheight;

    dfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_add(dfield, -gwy_data_field_get_avg(dfield));
    acf = gwy_data_field_new_alike(dfield, FALSE);
    acfwidth = MIN(dfield->xres/4, PREVIEW_SIZE/2 + (gint)sqrt(dfield->xres));
    acfheight = MIN(dfield->yres/4, PREVIEW_SIZE/2 + (gint)sqrt(dfield->yres));
    gwy_data_field_area_2dacf(dfield, acf, 0, 0, dfield->xres, dfield->yres,
                              MAX(acfwidth, 4), MAX(acfheight, 4));
    g_object_unref(dfield);
    gwy_container_set_object_by_name(controls->mydata, "/1/data", acf);
    g_object_unref(acf);
}

static void
different_lengths_toggled(AffcorControls *controls,
                          GtkToggleButton *toggle)
{
    AffcorArgs *args = controls->args;
    guint flags;

    args->different_lengths = gtk_toggle_button_get_active(toggle);
    if (!args->different_lengths) {
        const gchar *buf = gtk_entry_get_text(GTK_ENTRY(controls->a1_corr));
        gtk_entry_set_text(GTK_ENTRY(controls->a2_corr), buf);
    }
    flags = args->different_lengths ? SENS_DIFFERENT_LENGTHS : 0;
    gwy_sensitivity_group_set_state(controls->sens,
                                    SENS_DIFFERENT_LENGTHS, flags);
}

static void
refine(AffcorControls *controls)
{
    GwyDataField *acf;
    gint xwinsize, ywinsize;
    gdouble xy[4];

    if (!gwy_selection_get_object(controls->selection, 0, xy))
        return;

    acf = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                          "/1/data"));
    xwinsize = (gint)(0.28*MAX(fabs(xy[0]), fabs(xy[2]))
                      /gwy_data_field_get_xmeasure(acf) + 0.5);
    ywinsize = (gint)(0.28*MAX(fabs(xy[1]), fabs(xy[3]))
                      /gwy_data_field_get_ymeasure(acf) + 0.5);
    gwy_debug("window size: %dx%d", xwinsize, ywinsize);

    xy[0] = (xy[0] - acf->xoff)/gwy_data_field_get_xmeasure(acf);
    xy[1] = (xy[1] - acf->yoff)/gwy_data_field_get_ymeasure(acf);
    xy[2] = (xy[2] - acf->xoff)/gwy_data_field_get_xmeasure(acf);
    xy[3] = (xy[3] - acf->yoff)/gwy_data_field_get_ymeasure(acf);
    find_maximum(acf, xy + 0, xy + 1, xwinsize, ywinsize);
    find_maximum(acf, xy + 2, xy + 3, xwinsize, ywinsize);
    xy[0] = (xy[0] + 0.5)*gwy_data_field_get_xmeasure(acf) + acf->xoff;
    xy[1] = (xy[1] + 0.5)*gwy_data_field_get_ymeasure(acf) + acf->yoff;
    xy[2] = (xy[2] + 0.5)*gwy_data_field_get_xmeasure(acf) + acf->xoff;
    xy[3] = (xy[3] + 0.5)*gwy_data_field_get_ymeasure(acf) + acf->yoff;
    gwy_selection_set_object(controls->selection, 0, xy);
}

static void
selection_changed(AffcorControls *controls)
{
    GwySIValueFormat *vf;
    GwyDataField *dfield;
    gdouble xy[4];
    gdouble a1, a2, phi1, phi2, phi;
    guint flags, i;
    GString *str = g_string_new(NULL);

    if (!gwy_selection_get_data(controls->selection, NULL)) {
        controls->invalid_corr |= INVALID_SEL;
        gwy_sensitivity_group_set_state(controls->sens, SENS_VALID_LATTICE, 0);
        invalidate(controls);
        return;
    }

    gwy_selection_get_object(controls->selection, 0, xy);
    for (i = 0; i < 4; i++)
        controls->xy[i] = xy[i];

    vf = controls->vf;
    g_string_printf(str, "%.*f", vf->precision, xy[0]/vf->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->a1_x), str->str);

    g_string_printf(str, "%.*f", vf->precision, -xy[1]/vf->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->a1_y), str->str);

    a1 = hypot(xy[0], xy[1]);
    g_string_printf(str, "%.*f", vf->precision, a1/vf->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->a1_len), str->str);

    vf = controls->vfphi;
    phi1 = atan2(-xy[1], xy[0]);
    g_string_printf(str, "%.*f", vf->precision, 180.0/G_PI*phi1/vf->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->a1_phi), str->str);

    vf = controls->vf;
    g_string_printf(str, "%.*f", vf->precision, xy[2]/vf->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->a2_x), str->str);

    g_string_printf(str, "%.*f", vf->precision, -xy[3]/vf->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->a2_y), str->str);

    a2 = hypot(xy[2], xy[3]);
    g_string_printf(str, "%.*f", vf->precision, a2/vf->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->a2_len), str->str);

    vf = controls->vfphi;
    phi2 = atan2(-xy[3], xy[2]);
    g_string_printf(str, "%.*f", vf->precision, 180.0/G_PI*phi2/vf->magnitude);
    gtk_entry_set_text(GTK_ENTRY(controls->a2_phi), str->str);

    phi = phi2 - phi1;
    if (phi < 0.0)
        phi += 2.0*G_PI;
    g_string_printf(str, "%.*f", vf->precision, 180.0/G_PI*phi/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->phi), str->str);

    g_string_free(str, TRUE);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    if (hypot(xy[0]/gwy_data_field_get_xmeasure(dfield),
              xy[1]/gwy_data_field_get_ymeasure(dfield)) >= 0.9
        && hypot(xy[2]/gwy_data_field_get_xmeasure(dfield),
                 xy[3]/gwy_data_field_get_ymeasure(dfield)) >= 0.9
        && phi >= 1e-3
        && phi <= G_PI - 1e-3)
        controls->invalid_corr &= ~INVALID_SEL;
    else
        controls->invalid_corr |= INVALID_SEL;

    flags = controls->invalid_corr ? 0 : SENS_VALID_LATTICE;
    gwy_sensitivity_group_set_state(controls->sens, SENS_VALID_LATTICE, flags);
    invalidate(controls);
}

static void
interp_changed(GtkComboBox *combo,
               AffcorControls *controls)
{
    controls->args->interp = gwy_enum_combo_box_get_active(combo);
    invalidate(controls);
}

static void
scaling_changed(GtkComboBox *combo,
                AffcorControls *controls)
{
    controls->args->scaling = gwy_enum_combo_box_get_active(combo);
    invalidate(controls);
}

static void
invalidate(AffcorControls *controls)
{
    controls->calculated = FALSE;
    if (controls->invalid_corr
        || controls->args->image_mode != IMAGE_CORRECTED)
        return;

    if (controls->recalculate_id)
        return;

    controls->recalculate_id = g_idle_add(recalculate, controls);
}

static gboolean
recalculate(gpointer user_data)
{
    AffcorControls *controls = (AffcorControls*)user_data;
    do_correction(controls);
    controls->recalculate_id = 0;
    return FALSE;
}

static void
do_correction(AffcorControls *controls)
{
    AffcorArgs *args = controls->args;
    GwyDataField *dfield, *corrected;
    gdouble dx, dy, q = 1.0;
    gdouble a1a2_corr[4], a1a2[4], m[6], vmax[2], tmp[4];
    guint xres, yres, i;

    gwy_selection_get_object(controls->selection, 0, a1a2);
    gwy_debug("a1a2 %g %g %g %g", a1a2[0], a1a2[1], a1a2[2], a1a2[3]);
    a1a2_corr[0] = args->a1 * controls->vf->magnitude;
    a1a2_corr[1] = 0.0;
    a1a2_corr[2] = args->a2 * controls->vf->magnitude * cos(args->phi);
    a1a2_corr[3] = -args->a2 * controls->vf->magnitude * sin(args->phi);
    gwy_debug("a1a2_corr %g %g %g %g", a1a2_corr[0], a1a2_corr[1], a1a2_corr[2], a1a2_corr[3]);
    /* This is an approximate rotation correction to get the base more or less
     * oriented in the plane as expected and not upside down. */
    if (args->avoid_rotation) {
        gdouble alpha = atan2(-a1a2[1], a1a2[0]);
        tmp[0] = tmp[3] = cos(alpha);
        tmp[1] = sin(alpha);
        tmp[2] = -sin(alpha);
        matrix_vector(a1a2_corr, tmp, a1a2_corr);
        matrix_vector(a1a2_corr + 2, tmp, a1a2_corr + 2);
    }
    solve_transform_real(a1a2, a1a2_corr, m);
    gwy_debug("m %g %g %g %g", m[0], m[1], m[2], m[3]);

    /* This is the exact rotation correction. */
    if (args->avoid_rotation) {
        gdouble alpha = atan2(m[2], m[0]);
        tmp[0] = tmp[3] = cos(alpha);
        tmp[1] = sin(alpha);
        tmp[2] = -sin(alpha);
        matrix_matrix(m, tmp, m);
    }

    if (args->scaling == SCALING_PRESERVE_AREA)
        q = 1.0/sqrt(matrix_det(m));
    else if (args->scaling == SCALING_PRESERVE_X)
        q = 1.0/hypot(m[0], m[2]);

    for (i = 0; i < 4; i++)
        m[i] *= q;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    vmax[0] = vmax[1] = 0.0;
    corner_max(dfield->xreal, dfield->yreal, m, vmax);
    corner_max(-dfield->xreal, dfield->yreal, m, vmax);
    corner_max(dfield->xreal, -dfield->yreal, m, vmax);
    corner_max(-dfield->xreal, -dfield->yreal, m, vmax);

    /* Prevent information loss by using a sufficient resolution to represent
     * original pixels. */
    tmp[0] = gwy_data_field_get_xmeasure(dfield);
    tmp[1] = tmp[2] = 0.0;
    tmp[3] = gwy_data_field_get_ymeasure(dfield);
    gwy_debug("dxdy %g %g", tmp[0], tmp[3]);
    matrix_matrix(tmp, m, tmp);
    gwy_debug("pix_corr %g %g %g %g", tmp[0], tmp[1], tmp[2], tmp[3]);
    dx = hypot(tmp[0]/G_SQRT2, tmp[1]/G_SQRT2);
    dy = hypot(tmp[2]/G_SQRT2, tmp[3]/G_SQRT2);
    dx = dy = MIN(dx, dy);
    xres = GWY_ROUND(vmax[0]/dx);
    yres = GWY_ROUND(vmax[1]/dy);
    gwy_debug("dxdy_corr %g %g", dx, dy);
    gwy_debug("res %u %u", xres, yres);

    corrected = gwy_data_field_new(xres, yres, dx*xres, dy*yres, FALSE);
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_xy(dfield)),
                           G_OBJECT(gwy_data_field_get_si_unit_xy(corrected)));
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_z(dfield)),
                           G_OBJECT(gwy_data_field_get_si_unit_z(corrected)));

    invert_matrix(m, m);
    gwy_debug("minv %g %g %g %g", m[0], m[1], m[2], m[3]);

    /* Multiply from right by pixel-to-real matrix in the corrected field. */
    tmp[0] = gwy_data_field_get_xmeasure(corrected);
    tmp[1] = tmp[2] = 0.0;
    tmp[3] = gwy_data_field_get_ymeasure(corrected);
    matrix_matrix(m, m, tmp);
    /* and from left by real-to-pixel matrix in the original field. */
    tmp[0] = 1.0/gwy_data_field_get_xmeasure(dfield);
    tmp[1] = tmp[2] = 0.0;
    tmp[3] = 1.0/gwy_data_field_get_ymeasure(dfield);
    matrix_matrix(m, tmp, m);
    gwy_debug("minvpix %g %g %g %g", m[0], m[1], m[2], m[3]);

    m[4] = 0.5*corrected->xres;
    m[5] = 0.5*corrected->yres;
    matrix_vector(m + 4, m, m + 4);
    m[4] = 0.5*dfield->xres - m[4];
    m[5] = 0.5*dfield->yres - m[5];
    gwy_debug("b %g %g", m[4], m[5]);
    gwy_data_field_affine(dfield, corrected, m, controls->args->interp,
                          GWY_EXTERIOR_FIXED_VALUE,
                          gwy_data_field_get_avg(dfield));

    gwy_container_set_object_by_name(controls->mydata, "/2/data", corrected);
    g_object_unref(corrected);
    controls->calculated = TRUE;
}

static void
corner_max(gdouble x, gdouble y, const gdouble *m, gdouble *vmax)
{
    gdouble v[2];

    v[0] = x;
    v[1] = y;
    matrix_vector(v, m, v);
    vmax[0] = MAX(vmax[0], fabs(v[0]));
    vmax[1] = MAX(vmax[1], fabs(v[1]));
}

static void
solve_transform_real(const gdouble *a1a2,
                     const gdouble *a1a2_corr,
                     gdouble *m)
{
    gdouble tmp[4];
    tmp[0] = a1a2[0];
    tmp[1] = a1a2[2];
    tmp[2] = a1a2[1];
    tmp[3] = a1a2[3];
    invert_matrix(m, tmp);
    tmp[0] = a1a2_corr[0];
    tmp[1] = a1a2_corr[2];
    tmp[2] = a1a2_corr[1];
    tmp[3] = a1a2_corr[3];
    matrix_matrix(m, tmp, m);
}

static void
find_maximum(GwyDataField *dfield,
             gdouble *x, gdouble *y,
             gint xwinsize, gint ywinsize)
{
    gint xj = (gint)*x, yi = (gint)*y;
    gdouble max = -G_MAXDOUBLE;
    gint mi = yi, mj = xj, i, j;
    gint xres = dfield->xres, yres = dfield->yres;
    const gdouble *d = dfield->data;
    gdouble sz, szx, szy, szxx, szxy, szyy;
    gdouble v, bx, by, cxx, cxy, cyy, D, sx, sy;
    gdouble m[6], rhs[3];

    for (i = -ywinsize; i <= ywinsize; i++) {
        if (i + yi < 0 || i + yi > yres-1)
            continue;
        for (j = -xwinsize; j <= xwinsize; j++) {
            if (j + xj < 0 || j + xj > xres-1)
                continue;

            v = d[(i + yi)*xres + (j + xj)];
            if (v > max) {
                max = v;
                mi = i + yi;
                mj = j + xj;
            }
        }
    }

    *x = mj;
    *y = mi;

    /* Don't try any sub-pixel refinement if it's on the edge. */
    if (mi < 1 || mi+1 > yres-1 || mj < 1 || mj+1 > xres-1)
        return;

    sz = (d[(mi - 1)*xres + (mj - 1)]
          + d[(mi - 1)*xres + mj]
          + d[(mi - 1)*xres + (mj + 1)]
          + d[mi*xres + (mj - 1)]
          + d[mi*xres + mj]
          + d[mi*xres + (mj + 1)]
          + d[(mi + 1)*xres + (mj - 1)]
          + d[(mi + 1)*xres + mj]
          + d[(mi + 1)*xres + (mj + 1)]);
    szx = (-d[(mi - 1)*xres + (mj - 1)]
           + d[(mi - 1)*xres + (mj + 1)]
           - d[mi*xres + (mj - 1)]
           + d[mi*xres + (mj + 1)]
           - d[(mi + 1)*xres + (mj - 1)]
           + d[(mi + 1)*xres + (mj + 1)]);
    szy = (-d[(mi - 1)*xres + (mj - 1)]
           - d[(mi - 1)*xres + mj]
           - d[(mi - 1)*xres + (mj + 1)]
           + d[(mi + 1)*xres + (mj - 1)]
           + d[(mi + 1)*xres + mj]
           + d[(mi + 1)*xres + (mj + 1)]);
    szxx = (d[(mi - 1)*xres + (mj - 1)]
            + d[(mi - 1)*xres + (mj + 1)]
            + d[mi*xres + (mj - 1)]
            + d[mi*xres + (mj + 1)]
            + d[(mi + 1)*xres + (mj - 1)]
            + d[(mi + 1)*xres + (mj + 1)]);
    szxy = (d[(mi - 1)*xres + (mj - 1)]
            - d[(mi - 1)*xres + (mj + 1)]
            - d[(mi + 1)*xres + (mj - 1)]
            + d[(mi + 1)*xres + (mj + 1)]);
    szyy = (d[(mi - 1)*xres + (mj - 1)]
            + d[(mi - 1)*xres + mj]
            + d[(mi - 1)*xres + (mj + 1)]
            + d[(mi + 1)*xres + (mj - 1)]
            + d[(mi + 1)*xres + mj]
            + d[(mi + 1)*xres + (mj + 1)]);

    m[0] = 9.0;
    m[1] = m[2] = m[3] = m[5] = 6.0;
    m[4] = 4.0;
    gwy_math_choleski_decompose(3, m);

    rhs[0] = sz;
    rhs[1] = szxx;
    rhs[2] = szyy;
    gwy_math_choleski_solve(3, m, rhs);

    bx = szx/6.0;
    by = szy/6.0;
    cxx = rhs[1];
    cxy = szxy/4.0;
    cyy = rhs[2];

    D = 4.0*cxx*cyy - cxy*cxy;
    /* Don't try the sub-pixel refinement if bad cancellation occurs. */
    if (fabs(D) < 1e-8*MAX(fabs(4.0*cxx*cyy), fabs(cxy*cxy)))
        return;

    sx = (by*cxy - 2.0*bx*cyy)/D;
    sy = (bx*cxy - 2.0*by*cxx)/D;

    /* Don't trust the sub-pixel refinement if it moves the maximum outside
     * the 3×3 neighbourhood. */
    if (fabs(sx) > 1.5 || fabs(sy) > 1.5)
        return;

    *x += sx;
    *y += sy;
}

/* Permit dest = src */
static void
matrix_vector(gdouble *dest,
              const gdouble *m,
              const gdouble *src)
{
    gdouble xy[2];

    xy[0] = m[0]*src[0] + m[1]*src[1];
    xy[1] = m[2]*src[0] + m[3]*src[1];
    dest[0] = xy[0];
    dest[1] = xy[1];
}

/* Permit dest = src */
static void
matrix_matrix(gdouble *dest,
              const gdouble *m,
              const gdouble *src)
{
    gdouble xy[4];

    xy[0] = m[0]*src[0] + m[1]*src[2];
    xy[1] = m[0]*src[1] + m[1]*src[3];
    xy[2] = m[2]*src[0] + m[3]*src[2];
    xy[3] = m[2]*src[1] + m[3]*src[3];
    dest[0] = xy[0];
    dest[1] = xy[1];
    dest[2] = xy[2];
    dest[3] = xy[3];
}

/* Permit dest = src */
static void
invert_matrix(gdouble *dest,
              const gdouble *src)
{
    gdouble D = matrix_det(src);
    gdouble xy[4];

    gwy_debug("D %g", D);
    xy[0] = src[3]/D;
    xy[1] = -src[1]/D;
    xy[2] = -src[2]/D;
    xy[3] = src[0]/D;
    dest[0] = xy[0];
    dest[1] = xy[1];
    dest[2] = xy[2];
    dest[3] = xy[3];
}

static gdouble
matrix_det(const gdouble *m)
{
    return m[0]*m[3] - m[1]*m[2];
}

static const gchar preset_key[]            = "/module/correct_affine/preset";
static const gchar a1_key[]                = "/module/correct_affine/a1";
static const gchar a2_key[]                = "/module/correct_affine/a2";
static const gchar phi_key[]               = "/module/correct_affine/phi";
static const gchar different_lengths_key[] = "/module/correct_affine/different-lengths";
static const gchar interp_key[]            = "/module/correct_affine/interpolation";
static const gchar scaling_key[]           = "/module/correct_affine/scaling";

static void
affcor_sanitize_args(AffcorArgs *args)
{
    args->interp = gwy_enum_sanitize_value(args->interp,
                                           GWY_TYPE_INTERPOLATION_TYPE);
    args->scaling = MIN(args->scaling, SCALING_PRESERVE_X);
    args->preset = CLAMP(args->preset,
                         USER_DEFINED_LATTICE,
                         (gint)G_N_ELEMENTS(lattice_presets)-1);

    if (args->preset == USER_DEFINED_LATTICE) {
        args->different_lengths = !!args->different_lengths;

        if (!(args->a1 > 0.0))
            args->a1 = 1.0;

        if (args->different_lengths) {
            if (!(args->a2 > 0.0))
                args->a2 = 1.0;
        }
        else
            args->a2 = args->a1;

        args->phi = fmod(args->phi, 2.0*G_PI);
        if (args->phi < 0.0)
            args->phi += 2.0*G_PI;
        if (args->phi > G_PI)
            args->phi -= G_PI;

        if (args->phi < 1e-3 || args->phi > G_PI - 1e-3)
            args->phi = 0.5*G_PI;
    }
}

static void
affcor_load_args(GwyContainer *container,
                 AffcorArgs *args)
{
    *args = affcor_defaults;

    gwy_container_gis_double_by_name(container, a1_key, &args->a1);
    gwy_container_gis_double_by_name(container, a2_key, &args->a2);
    gwy_container_gis_double_by_name(container, phi_key, &args->phi);
    gwy_container_gis_boolean_by_name(container, different_lengths_key,
                                      &args->different_lengths);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, scaling_key, &args->scaling);
    gwy_container_gis_int32_by_name(container, preset_key, &args->preset);

    affcor_sanitize_args(args);
}

static void
affcor_save_args(GwyContainer *container,
                 AffcorArgs *args)
{
    gwy_container_set_double_by_name(container, a1_key, args->a1);
    gwy_container_set_double_by_name(container, a2_key, args->a2);
    gwy_container_set_double_by_name(container, phi_key, args->phi);
    gwy_container_set_boolean_by_name(container, different_lengths_key,
                                      args->different_lengths);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, scaling_key, args->scaling);
    gwy_container_set_int32_by_name(container, preset_key, args->preset);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
