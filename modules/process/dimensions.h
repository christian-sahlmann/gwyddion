/*
 *  @(#) $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GWY_DIMENSIONS_H__
#define __GWY_DIMENSIONS_H__ 1

#include <libgwydgets/gwycombobox.h>

enum {
    GWY_DIMENSIONS_SENS = 1
};

typedef struct {
    gint xres;
    gint yres;
    gdouble measure;
    gchar *xyunits;
    gchar *zunits;
    gint xypow10;
    gint zpow10;
    gboolean replace;    /* do not create a new channel */
    gboolean add;        /* starts from the current, but may create new */
} GwyDimensionArgs;

#define GWY_DIMENSION_ARGS_INIT \
    { 256, 256, 1.0, NULL, NULL, 0, 0, FALSE, FALSE }

typedef struct {
    GwyDimensionArgs *args;
    /* These are provided for the user callbacks to that he can easily update
     * unit labels */
    GwySIValueFormat *xyvf;
    GwySIValueFormat *zvf;
    GwySIUnit *xysiunit;
    GwySIUnit *zsiunit;
    GwySensitivityGroup *sensgroup;
    GwyDataField *template_;
    GtkWidget *table;
    GtkAdjustment *xres;
    GtkAdjustment *yres;
    GtkWidget *xyreseq;
    GtkAdjustment *xreal;
    GtkAdjustment *yreal;
    GtkWidget *xunitslab;
    GtkWidget *yunitslab;
    GtkWidget *xypow10;
    GtkWidget *xyunits;
    GtkWidget *zpow10;
    GtkWidget *zunits;
    GtkWidget *replace;
    GtkWidget *add;
    gboolean in_update;
} GwyDimensions;

static GtkAdjustment*
gwy_dimensions_make_res(GtkTable *table,
                        GwySensitivityGroup *sensgroup,
                        gint row,
                        const gchar *name,
                        gint value)
{
    GtkWidget *label, *spin;
    GtkAdjustment *adj;
    GtkObject *obj;

    label = gtk_label_new_with_mnemonic(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gwy_sensitivity_group_add_widget(sensgroup, label, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    obj = gtk_adjustment_new(value, 2, 16384, 1, 100, 0);
    adj = GTK_ADJUSTMENT(obj);
    spin = gtk_spin_button_new(adj, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gwy_sensitivity_group_add_widget(sensgroup, spin, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, spin, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new("px");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gwy_sensitivity_group_add_widget(sensgroup, label, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, label, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    return adj;
}

static GtkAdjustment*
gwy_dimensions_make_real(GtkTable *table,
                         GwySensitivityGroup *sensgroup,
                         gint row,
                         const gchar *name,
                         gdouble value,
                         const gchar *units,
                         GtkWidget **unitlab)
{
    GtkWidget *label, *spin;
    GtkAdjustment *adj;
    GtkObject *obj;

    label = gtk_label_new_with_mnemonic(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gwy_sensitivity_group_add_widget(sensgroup, label, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    obj = gtk_adjustment_new(value, 0.001, 10000.0, 1, 100, 0);
    adj = GTK_ADJUSTMENT(obj);
    spin = gtk_spin_button_new(adj, 0, 3);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gwy_sensitivity_group_add_widget(sensgroup, spin, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, spin, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    *unitlab = gtk_label_new(units);
    gtk_label_set_use_markup(GTK_LABEL(*unitlab), TRUE);
    gtk_misc_set_alignment(GTK_MISC(*unitlab), 0.0, 0.5);
    gwy_sensitivity_group_add_widget(sensgroup, *unitlab, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, *unitlab, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    return adj;
}

static GtkWidget*
gwy_dimensions_make_units(GtkTable *table,
                          GwySensitivityGroup *sensgroup,
                          gint row,
                          const gchar *name,
                          gint pwr,
                          GwySIUnit *siunit,
                          GtkWidget **combo)
{
    GtkWidget *label, *changer;

    label = gtk_label_new_with_mnemonic(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gwy_sensitivity_group_add_widget(sensgroup, label, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    *combo = gwy_combo_box_metric_unit_new(NULL, NULL,
                                           pwr - 6, pwr + 6, siunit, pwr);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), *combo);
    gwy_sensitivity_group_add_widget(sensgroup, *combo, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, *combo, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    changer = gtk_button_new_with_label(gwy_sgettext("verb|Change"));
    gwy_sensitivity_group_add_widget(sensgroup, changer, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, changer, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    return changer;
}

static void
gwy_dimensions_set_combo_from_unit(GtkComboBox *combo,
                                   const gchar *str)
{
    GwySIUnit *unit;
    gint power10;

    unit = gwy_si_unit_new_parse(str, &power10);
    gwy_combo_box_metric_unit_set_unit(combo, power10 - 6, power10 + 6, unit);
    gwy_enum_combo_box_set_active(combo, power10);
    g_object_unref(unit);
}

static void
gwy_dimensions_units_changed(GtkWidget *widget,
                             GtkComboBox *combo,
                             gchar **unitstr)
{
    GtkWidget *dialog, *hbox, *label, *toplevel;
    GtkEntry *entry;
    GtkWindow *parent = NULL;
    const gchar *unit;
    gint response;

    toplevel = gtk_widget_get_toplevel(widget);
    if (GTK_WIDGET_TOPLEVEL(toplevel) && GTK_IS_WINDOW(toplevel))
        parent = GTK_WINDOW(toplevel);
    dialog = gtk_dialog_new_with_buttons(_("Change Units"), parent,
                                         GTK_DIALOG_MODAL
                                         | GTK_DIALOG_NO_SEPARATOR,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("New _units:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_text(entry, *unitstr ? *unitstr : "");
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(entry));
    gtk_entry_set_activates_default(entry, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(entry), TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    unit = gtk_entry_get_text(entry);
    g_free(*unitstr);
    *unitstr = g_strdup(unit);
    gwy_dimensions_set_combo_from_unit(combo, unit);

    gtk_widget_destroy(dialog);
}

static void
gwy_dimensions_xres_changed(GwyDimensions *dims,
                            GtkAdjustment *adj)
{
    GwyDimensionArgs *args = dims->args;

    args->xres = gwy_adjustment_get_int(adj);
    if (!dims->in_update) {
        dims->in_update = TRUE;
        gtk_adjustment_set_value(dims->xreal, args->xres * args->measure);
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dims->xyreseq))) {
            gtk_adjustment_set_value(dims->yres, args->xres);
            gtk_adjustment_set_value(dims->yreal, args->yres * args->measure);
        }
        dims->in_update = FALSE;
    }
}

static void
gwy_dimensions_yres_changed(GwyDimensions *dims,
                            GtkAdjustment *adj)
{
    GwyDimensionArgs *args = dims->args;

    args->yres = gwy_adjustment_get_int(adj);
    if (!dims->in_update) {
        dims->in_update = TRUE;
        gtk_adjustment_set_value(dims->yreal, args->yres * args->measure);
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dims->xyreseq))) {
            gtk_adjustment_set_value(dims->xres, args->yres);
            gtk_adjustment_set_value(dims->xreal, args->xres * args->measure);
        }
        dims->in_update = FALSE;
    }
}

static void
gwy_dimensions_xreal_changed(GwyDimensions *dims,
                             GtkAdjustment *adj)
{
    GwyDimensionArgs *args = dims->args;

    if (!dims->in_update) {
        dims->in_update = TRUE;
        args->measure = gtk_adjustment_get_value(adj)/args->xres;
        gtk_adjustment_set_value(dims->yreal, args->yres * args->measure);
        dims->in_update = FALSE;
    }
}

static void
gwy_dimensions_yreal_changed(GwyDimensions *dims,
                             GtkAdjustment *adj)
{
    GwyDimensionArgs *args = dims->args;

    if (!dims->in_update) {
        dims->in_update = TRUE;
        args->measure = gtk_adjustment_get_value(adj)/args->yres;
        gtk_adjustment_set_value(dims->xreal, args->xres * args->measure);
        dims->in_update = FALSE;
    }
}

static void
gwy_dimensions_xyreseq_toggled(GwyDimensions *dims,
                               GtkToggleButton *toggle)
{
    if (!dims->in_update && gtk_toggle_button_get_active(toggle))
        gtk_adjustment_set_value(dims->yres, dims->args->xres);
}

static void
gwy_dimensions_xyunits_changed(GwyDimensions *dims)
{
    gwy_dimensions_units_changed(dims->table, GTK_COMBO_BOX(dims->xypow10),
                                 &dims->args->xyunits);
}

static void
gwy_dimensions_zunits_changed(GwyDimensions *dims)
{
    gwy_dimensions_units_changed(dims->table, GTK_COMBO_BOX(dims->zpow10),
                                 &dims->args->zunits);
}

static void
gwy_dimensions_xypow10_changed(GwyDimensions *dims,
                               GtkComboBox *combo)
{
    dims->args->xypow10 = gwy_enum_combo_box_get_active(combo);
    gwy_si_unit_set_from_string(dims->xysiunit, dims->args->xyunits);
    gwy_si_unit_get_format_for_power10(dims->xysiunit,
                                       GWY_SI_UNIT_FORMAT_VFMARKUP,
                                       dims->args->xypow10, dims->xyvf);
    /* Update our parameter unit labels. */
    gtk_label_set_markup(GTK_LABEL(dims->xunitslab), dims->xyvf->units);
    gtk_label_set_markup(GTK_LABEL(dims->yunitslab), dims->xyvf->units);
}

static void
gwy_dimensions_zpow10_changed(GwyDimensions *dims,
                              GtkComboBox *combo)
{
    dims->args->zpow10 = gwy_enum_combo_box_get_active(combo);
    gwy_si_unit_set_from_string(dims->zsiunit, dims->args->zunits);
    gwy_si_unit_get_format_for_power10(dims->zsiunit,
                                       GWY_SI_UNIT_FORMAT_VFMARKUP,
                                       dims->args->zpow10, dims->zvf);
}

static void
gwy_dimensions_use_template(GwyDimensions *dims)
{
    gint xres = gwy_data_field_get_xres(dims->template_);
    gint yres = gwy_data_field_get_yres(dims->template_);
    gdouble xreal = gwy_data_field_get_xreal(dims->template_);
    gdouble yreal = gwy_data_field_get_yreal(dims->template_);
    GwySIValueFormat *xyvf
        = gwy_data_field_get_value_format_xy(dims->template_,
                                             GWY_SI_UNIT_FORMAT_PLAIN, NULL);
    GwySIValueFormat *zvf
        = gwy_data_field_get_value_format_z(dims->template_,
                                            GWY_SI_UNIT_FORMAT_PLAIN, NULL);

    dims->args->measure = xreal/xyvf->magnitude/xres;
    dims->in_update = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dims->xyreseq),
                                 xres == yres);
    gtk_adjustment_set_value(dims->xres, xres);
    gtk_adjustment_set_value(dims->yres, yres);
    gtk_adjustment_set_value(dims->xreal, xreal/xyvf->magnitude);
    gtk_adjustment_set_value(dims->yreal, yreal/xyvf->magnitude);
    g_free(dims->args->xyunits);
    dims->args->xyunits = g_strdup(xyvf->units);
    gwy_dimensions_set_combo_from_unit(GTK_COMBO_BOX(dims->xypow10),
                                       xyvf->units);
    g_free(dims->args->zunits);
    dims->args->zunits = g_strdup(zvf->units);
    gwy_dimensions_set_combo_from_unit(GTK_COMBO_BOX(dims->zpow10),
                                       zvf->units);
    dims->in_update = FALSE;

    gwy_si_unit_value_format_free(xyvf);
    gwy_si_unit_value_format_free(zvf);
}

static void
gwy_dimensions_replace(GwyDimensions *dims,
                       GtkWidget *toggle)
{
    gboolean replace, sens;

    replace = toggle && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));

    dims->args->replace = replace;
    if (replace)
        gwy_dimensions_use_template(dims);
    sens = !dims->args->replace && !dims->args->add;
    gwy_sensitivity_group_set_state(dims->sensgroup,
                                    GWY_DIMENSIONS_SENS,
                                    sens ? GWY_DIMENSIONS_SENS : 0);
}

static void
gwy_dimensions_add(GwyDimensions *dims,
                   GtkWidget *toggle)
{
    gboolean add, sens;

    add = toggle && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));

    dims->args->add = add;
    if (add)
        gwy_dimensions_use_template(dims);
    sens = !dims->args->replace && !dims->args->add;
    gwy_sensitivity_group_set_state(dims->sensgroup,
                                    GWY_DIMENSIONS_SENS,
                                    sens ? GWY_DIMENSIONS_SENS : 0);
}

static GwyDimensions*
gwy_dimensions_new(GwyDimensionArgs *args,
                   GwyDataField *template_)
{
    GwyDimensions *dims = g_new0(GwyDimensions, 1);
    GwySensitivityGroup *sensgroup;
    GtkWidget *label, *button;
    GtkTable *table;
    gint row;

    dims->args = args;
    dims->template_ = template_ ? g_object_ref(template_) : NULL;
    dims->sensgroup = sensgroup = gwy_sensitivity_group_new();
    dims->xysiunit = gwy_si_unit_new(dims->args->xyunits);
    dims->xyvf = gwy_si_unit_get_format_for_power10(dims->xysiunit,
                                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                    args->xypow10, NULL);
    dims->xyvf->precision = 3;
    dims->zsiunit = gwy_si_unit_new(dims->args->zunits);
    dims->zvf = gwy_si_unit_get_format_for_power10(dims->zsiunit,
                                                   GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                   args->zpow10, NULL);
    dims->zvf->precision = 3;

    dims->table = gtk_table_new(10 + (dims->template_ ? 4 : 0), 3, FALSE);
    table = GTK_TABLE(dims->table);
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    row = 0;

    /* Resolution */
    label = gwy_label_new_header(_("Resolution"));
    gwy_sensitivity_group_add_widget(sensgroup, label, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, label, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    dims->xres = gwy_dimensions_make_res(table, sensgroup, row++,
                                         _("_Horizontal size:"),
                                         args->xres);
    dims->yres = gwy_dimensions_make_res(table, sensgroup, row++,
                                         _("_Vertical size:"),
                                         args->yres);

    dims->xyreseq = gtk_check_button_new_with_mnemonic(_("S_quare image"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dims->xyreseq),
                                 args->xres == args->yres);
    gwy_sensitivity_group_add_widget(sensgroup, dims->xyreseq,
                                     GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, dims->xyreseq, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(table, row, 12);
    row++;

    /* Physical dimensions */
    label = gwy_label_new_header(_("Physical Dimensions"));
    gwy_sensitivity_group_add_widget(sensgroup, label, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, label, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    dims->xreal = gwy_dimensions_make_real(table, sensgroup, row++,
                                           _("_Width:"),
                                           args->xres * args->measure,
                                           dims->xyvf->units, &dims->xunitslab);
    dims->yreal = gwy_dimensions_make_real(table, sensgroup, row++,
                                           _("H_eight:"),
                                           args->yres * args->measure,
                                           dims->xyvf->units, &dims->yunitslab);
    gtk_table_set_row_spacing(table, row-1, 12);

    /* Units */
    label = gwy_label_new_header(_("Units"));
    gwy_sensitivity_group_add_widget(sensgroup, label, GWY_DIMENSIONS_SENS);
    gtk_table_attach(table, label, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    dims->xyunits = gwy_dimensions_make_units(table, sensgroup, row++,
                                              _("_Dimension units:"),
                                              args->xypow10, dims->xysiunit,
                                              &dims->xypow10);

    dims->zunits = gwy_dimensions_make_units(table, sensgroup, row++,
                                             _("_Value units:"),
                                             args->zpow10, dims->zsiunit,
                                             &dims->zpow10);

    /* Template */
    if (dims->template_) {
        gtk_table_set_row_spacing(table, row-1, 12);

        label = gwy_label_new_header(_("Current Channel"));
        gtk_table_attach(table, label, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
        row++;

        button = gtk_button_new_with_mnemonic(_("_Take Dimensions from Current Channel"));
        gwy_sensitivity_group_add_widget(sensgroup, button,
                                         GWY_DIMENSIONS_SENS);
        gtk_table_attach(table, button, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(gwy_dimensions_use_template), dims);
        row++;

        button = gtk_check_button_new_with_mnemonic(_("_Replace the current "
                                                      "channel"));
        dims->replace = button;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), args->replace);
        gtk_table_attach(table, button, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
        g_signal_connect_swapped(button, "toggled",
                                 G_CALLBACK(gwy_dimensions_replace), dims);
        row++;

        button = gtk_check_button_new_with_mnemonic(_("_Start from the current "
                                                      "channel"));
        dims->add = button;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), args->add);
        gtk_table_attach(table, button, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
        g_signal_connect_swapped(button, "toggled",
                                 G_CALLBACK(gwy_dimensions_add), dims);
        row++;
    }

    /* Final setup */
    g_signal_connect_swapped(dims->xres, "value-changed",
                             G_CALLBACK(gwy_dimensions_xres_changed), dims);
    g_signal_connect_swapped(dims->yres, "value-changed",
                             G_CALLBACK(gwy_dimensions_yres_changed), dims);
    g_signal_connect_swapped(dims->xyreseq, "toggled",
                             G_CALLBACK(gwy_dimensions_xyreseq_toggled), dims);
    g_signal_connect_swapped(dims->xreal, "value-changed",
                             G_CALLBACK(gwy_dimensions_xreal_changed), dims);
    g_signal_connect_swapped(dims->yreal, "value-changed",
                             G_CALLBACK(gwy_dimensions_yreal_changed), dims);
    g_signal_connect_swapped(dims->xyunits, "clicked",
                             G_CALLBACK(gwy_dimensions_xyunits_changed), dims);
    g_signal_connect_swapped(dims->zunits, "clicked",
                             G_CALLBACK(gwy_dimensions_zunits_changed), dims);
    g_signal_connect_swapped(dims->xypow10, "changed",
                             G_CALLBACK(gwy_dimensions_xypow10_changed), dims);
    g_signal_connect_swapped(dims->zpow10, "changed",
                             G_CALLBACK(gwy_dimensions_zpow10_changed), dims);

    gwy_dimensions_replace(dims, dims->replace);
    /* FIXME: Necessary? */
    gwy_dimensions_add(dims, dims->add);

    return dims;
}

static GtkWidget*
gwy_dimensions_get_widget(GwyDimensions *dims)
{
    return dims->table;
}

static void
gwy_dimensions_free(GwyDimensions *dims)
{
    g_object_unref(dims->sensgroup);
    g_object_unref(dims->xysiunit);
    g_object_unref(dims->zsiunit);
    gwy_si_unit_value_format_free(dims->xyvf);
    gwy_si_unit_value_format_free(dims->zvf);
    gwy_object_unref(dims->template_);
    g_free(dims);
}

static void
gwy_dimensions_copy_args(const GwyDimensionArgs *source,
                         GwyDimensionArgs *dest)
{
    gchar *oldxyunits = dest->xyunits;
    gchar *oldzunits = dest->zunits;

    if (dest == source)
        return;

    *dest = *source;
    dest->xyunits = g_strdup(dest->xyunits);
    dest->zunits = g_strdup(dest->zunits);
    g_free(oldxyunits);
    g_free(oldzunits);
}

static void
gwy_dimensions_free_args(GwyDimensionArgs *args)
{
    g_free(args->xyunits);
    g_free(args->zunits);
    args->xyunits = NULL;
    args->zunits = NULL;
}

static void
gwy_dimensions_load_args(GwyDimensionArgs *args,
                         GwyContainer *settings,
                         const gchar *prefix)
{
    GString *key;
    guint len;
    const guchar *s;

    key = g_string_new(prefix);
    if (!g_str_has_suffix(prefix, "/"))
        g_string_append_c(key, '/');
    len = key->len;

    g_string_append(g_string_truncate(key, len), "xres");
    gwy_container_gis_int32_by_name(settings, key->str, &args->xres);

    g_string_append(g_string_truncate(key, len), "yres");
    gwy_container_gis_int32_by_name(settings, key->str, &args->yres);

    g_string_append(g_string_truncate(key, len), "measure");
    gwy_container_gis_double_by_name(settings, key->str, &args->measure);

    g_string_append(g_string_truncate(key, len), "xypow10");
    gwy_container_gis_int32_by_name(settings, key->str, &args->xypow10);

    g_string_append(g_string_truncate(key, len), "zpow10");
    gwy_container_gis_int32_by_name(settings, key->str, &args->zpow10);

    g_string_append(g_string_truncate(key, len), "xyunits");
    if (gwy_container_gis_string_by_name(settings, key->str, &s)) {
        g_free(args->xyunits);
        args->xyunits = g_strdup(s);
    }

    g_string_append(g_string_truncate(key, len), "zunits");
    if (gwy_container_gis_string_by_name(settings, key->str, &s)) {
        g_free(args->zunits);
        args->zunits = g_strdup(s);
    }

    g_string_append(g_string_truncate(key, len), "replace");
    gwy_container_gis_boolean_by_name(settings, key->str, &args->replace);

    g_string_append(g_string_truncate(key, len), "add");
    gwy_container_gis_boolean_by_name(settings, key->str, &args->add);

    g_string_free(key, TRUE);
}

static void
gwy_dimensions_save_args(const GwyDimensionArgs *args,
                         GwyContainer *settings,
                         const gchar *prefix)
{
    GString *key;
    guint len;
    const gchar *s;

    key = g_string_new(prefix);
    if (!g_str_has_suffix(prefix, "/"))
        g_string_append_c(key, '/');
    len = key->len;

    g_string_append(g_string_truncate(key, len), "xres");
    gwy_container_set_int32_by_name(settings, key->str, args->xres);

    g_string_append(g_string_truncate(key, len), "yres");
    gwy_container_set_int32_by_name(settings, key->str, args->yres);

    g_string_append(g_string_truncate(key, len), "measure");
    gwy_container_set_double_by_name(settings, key->str, args->measure);

    g_string_append(g_string_truncate(key, len), "xypow10");
    gwy_container_set_int32_by_name(settings, key->str, args->xypow10);

    g_string_append(g_string_truncate(key, len), "zpow10");
    gwy_container_set_int32_by_name(settings, key->str, args->zpow10);

    g_string_append(g_string_truncate(key, len), "xyunits");
    s = args->xyunits;
    gwy_container_set_string_by_name(settings, key->str, g_strdup(s ? s : ""));

    g_string_append(g_string_truncate(key, len), "zunits");
    s = args->zunits;
    gwy_container_set_string_by_name(settings, key->str, g_strdup(s ? s : ""));

    g_string_append(g_string_truncate(key, len), "replace");
    gwy_container_set_boolean_by_name(settings, key->str, args->replace);

    g_string_append(g_string_truncate(key, len), "add");
    gwy_container_set_boolean_by_name(settings, key->str, args->add);

    g_string_free(key, TRUE);
}

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
