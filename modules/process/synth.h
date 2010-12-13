/*
 *  @(#) $Id$
 *  Copyright (C) 2009,2010 David Necas (Yeti).
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_SYNTH_H__
#define __GWY_SYNTH_H__ 1

#ifndef GWY_SYNTH_CONTROLS
#error GWY_SYNTH_CONTROLS must be defined to the GUI controls structure name
#endif
/* GWY_SYNTH_CONTROLS must have the following fields defined:
 * - table
 * - dims
 * - pxsize
 */

#ifndef GWY_SYNTH_INVALIDATE
#error GWY_SYNTH_INVALIDATE must be defined to the common invalidation code
#endif

#include <libgwydgets/gwydgetutils.h>

typedef void (*GwySynthUpdateValueFunc)(GWY_SYNTH_CONTROLS *controls);

static void          gwy_synth_shrink_table          (GtkTable *table,
                                                      guint last_row);
static guint         gwy_synth_extend_table          (GtkTable *table,
                                                      guint by_rows);
static void          gwy_synth_update_value_label    (GtkLabel *label,
                                                      const GwySIValueFormat *vf,
                                                      gdouble value);
static void          gwy_synth_boolean_changed       (GWY_SYNTH_CONTROLS *controls,
                                                      GtkToggleButton *toggle);
static void          gwy_synth_boolean_changed_silent(GtkToggleButton *button,
                                                      gboolean *target);
static void          gwy_synth_toggle_sensitive      (GtkToggleButton *toggle,
                                                      GtkWidget *widget);
static void          gwy_synth_double_changed        (GWY_SYNTH_CONTROLS *controls,
                                                      GtkAdjustment *adj);
static void          gwy_synth_angle_changed         (GWY_SYNTH_CONTROLS *controls,
                                                      GtkAdjustment *adj);
static void          gwy_synth_update_lateral        (GWY_SYNTH_CONTROLS *controls,
                                                      GtkAdjustment *adj);
static gint          gwy_synth_attach_lateral        (GWY_SYNTH_CONTROLS *controls,
                                                      gint row,
                                                      GtkObject *adj,
                                                      gdouble *target,
                                                      const gchar *name,
                                                      GwyHScaleStyle hscale_style,
                                                      GtkWidget **pspin,
                                                      GtkWidget **pvalue,
                                                      GtkWidget **punits);
static gint          gwy_synth_attach_height         (GWY_SYNTH_CONTROLS *controls,
                                                      gint row,
                                                      GtkObject **adj,
                                                      gdouble *target,
                                                      const gchar *name,
                                                      GtkWidget **pspin,
                                                      GtkWidget **punits);
static gint          gwy_synth_attach_orientation    (GWY_SYNTH_CONTROLS *controls,
                                                      gint row,
                                                      GtkObject **adj,
                                                      gdouble *target);
static gint          gwy_synth_attach_variance       (GWY_SYNTH_CONTROLS *controls,
                                                      gint row,
                                                      GtkObject **adj,
                                                      gdouble *target);
static gint          gwy_synth_attach_deformation    (GWY_SYNTH_CONTROLS *controls,
                                                      gint row,
                                                      GtkObject **adj_sigma,
                                                      gdouble *target_sigma,
                                                      GtkObject **adj_tau,
                                                      gdouble *target_tau,
                                                      GtkWidget **pvalue_tau,
                                                      GtkWidget **punits_tau);
static GtkWidget*    gwy_synth_instant_updates_new   (GWY_SYNTH_CONTROLS *controls,
                                                      GtkWidget **pupdate,
                                                      GtkWidget **pinstant,
                                                      gboolean *target);
static void          gwy_synth_randomize_seed        (GtkAdjustment *adj);
static GtkWidget*    gwy_synth_random_seed_new       (GWY_SYNTH_CONTROLS *controls,
                                                      GtkObject **adj,
                                                      gint *target);
static GtkWidget*    gwy_synth_randomize_new         (gboolean *target);
static GwyDataField* gwy_synth_surface_for_preview   (GwyDataField *dfield,
                                                      guint size);
static void          gwy_synth_load_arg_double       (GwyContainer *settings,
                                                      GString *key,
                                                      const gchar *name,
                                                      gdouble min,
                                                      gdouble max,
                                                      gdouble *value);
static void          gwy_synth_save_arg_double       (GwyContainer *settings,
                                                      GString *key,
                                                      const gchar *name,
                                                      gdouble value);
static void          gwy_synth_load_arg_boolean      (GwyContainer *settings,
                                                      GString *key,
                                                      const gchar *name,
                                                      gboolean *value);
static void          gwy_synth_save_arg_boolean      (GwyContainer *settings,
                                                      GString *key,
                                                      const gchar *name,
                                                      gboolean value);

G_GNUC_UNUSED
static void
gwy_synth_shrink_table(GtkTable *table,
                       guint keep_rows)
{
    /* Brutally get rid of the unwanted rows.  This seems to be the only
     * method that actually works. */
    GtkContainer *container = GTK_CONTAINER(table);
    GList *l, *children = gtk_container_get_children(container);
    guint cols;

    for (l = children; l; l = g_list_next(l)) {
        GtkWidget *widget = GTK_WIDGET(l->data);
        gint row;

        gtk_container_child_get(container, widget, "bottom-attach", &row, NULL);
        if (row > keep_rows)
            gtk_widget_destroy(widget);
    }
    g_list_free(children);

    g_object_get(table, "n-columns", &cols, NULL);
    g_object_set(table, "n-rows", keep_rows, NULL);
    gtk_table_resize(table, keep_rows, cols);
}

G_GNUC_UNUSED
static guint
gwy_synth_extend_table(GtkTable *table,
                       guint by_rows)
{
    guint rows, cols;

    g_object_get(table, "n-rows", &rows, "n-columns", &cols, NULL);
    g_object_set(table, "n-rows", rows + by_rows, NULL);
    gtk_table_resize(table, rows + by_rows, cols);

    return rows;
}

G_GNUC_UNUSED
static void
gwy_synth_update_value_label(GtkLabel *label,
                             const GwySIValueFormat *vf,
                             gdouble value)
{
    gchar buf[32];

    g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, value/vf->magnitude);
    gtk_label_set_markup(label, buf);
}

G_GNUC_UNUSED
static void
gwy_synth_boolean_changed(GWY_SYNTH_CONTROLS *controls,
                          GtkToggleButton *toggle)
{
    GObject *object = G_OBJECT(toggle);
    gboolean *target = g_object_get_data(object, "target");

    g_return_if_fail(target);
    *target = gtk_toggle_button_get_active(toggle);
    GWY_SYNTH_INVALIDATE(controls);
}

G_GNUC_UNUSED
static void
gwy_synth_boolean_changed_silent(GtkToggleButton *button,
                                 gboolean *target)
{
    *target = gtk_toggle_button_get_active(button);
}

G_GNUC_UNUSED
static void
gwy_synth_toggle_sensitive(GtkToggleButton *toggle,
                           GtkWidget *widget)
{
    gtk_widget_set_sensitive(widget, !gtk_toggle_button_get_active(toggle));
}

G_GNUC_UNUSED
static void
gwy_synth_int_changed(GWY_SYNTH_CONTROLS *controls,
                      GtkAdjustment *adj)
{
    GObject *object = G_OBJECT(adj);
    gint *target = g_object_get_data(object, "target");

    g_return_if_fail(target);
    *target = gwy_adjustment_get_int(adj);
    GWY_SYNTH_INVALIDATE(controls);
}

G_GNUC_UNUSED
static void
gwy_synth_double_changed(GWY_SYNTH_CONTROLS *controls,
                         GtkAdjustment *adj)
{
    GObject *object = G_OBJECT(adj);
    gdouble *target = g_object_get_data(object, "target");
    GwySynthUpdateValueFunc update_value = g_object_get_data(object,
                                                             "update-value");

    g_return_if_fail(target);
    *target = gtk_adjustment_get_value(adj);
    if (update_value)
        update_value(controls);
    GWY_SYNTH_INVALIDATE(controls);
}

G_GNUC_UNUSED
static void
gwy_synth_angle_changed(GWY_SYNTH_CONTROLS *controls,
                        GtkAdjustment *adj)
{
    GObject *object = G_OBJECT(adj);
    gdouble *target = g_object_get_data(object, "target");

    g_return_if_fail(target);
    *target = G_PI/180.0 * gtk_adjustment_get_value(adj);
    GWY_SYNTH_INVALIDATE(controls);
}

G_GNUC_UNUSED
static void
gwy_synth_update_lateral(GWY_SYNTH_CONTROLS *controls,
                         GtkAdjustment *adj)
{
    GtkWidget *label = g_object_get_data(G_OBJECT(adj), "value-label");
    gdouble val = gtk_adjustment_get_value(adj);

    gwy_synth_update_value_label(GTK_LABEL(label),
                                 controls->dims->xyvf, val * controls->pxsize);
}

G_GNUC_UNUSED
static gint
gwy_synth_attach_lateral(GWY_SYNTH_CONTROLS *controls,
                         gint row,
                         GtkObject *adj,
                         gdouble *target,
                         const gchar *name,
                         GwyHScaleStyle hscale_style,
                         GtkWidget **pspin,
                         GtkWidget **pvalue,
                         GtkWidget **punits)
{
    GtkWidget *spin;

    g_object_set_data(G_OBJECT(adj), "target", target);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, name, "px", adj, hscale_style);
    if (pspin)
        *pspin = spin;
    row++;

    *pvalue = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(*pvalue), 1.0, 0.5);
    gtk_table_attach(controls->table, *pvalue,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_object_set_data(G_OBJECT(adj), "value-label", *pvalue);

    *punits = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(*punits), 0.0, 0.5);
    gtk_table_attach(controls->table, *punits,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    g_signal_connect_swapped(adj, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), controls);
    g_signal_connect_swapped(adj, "value-changed",
                             G_CALLBACK(gwy_synth_update_lateral), controls);

    return row;
}

G_GNUC_UNUSED
static gint
gwy_synth_attach_height(GWY_SYNTH_CONTROLS *controls,
                        gint row,
                        GtkObject **adj,
                        gdouble *target,
                        const gchar *name,
                        GtkWidget **pspin,
                        GtkWidget **punits)
{
    GtkWidget *spin;

    *adj = gtk_adjustment_new(*target, 0.0001, 10000.0, 0.1, 10.0, 0);
    g_object_set_data(G_OBJECT(*adj), "target", target);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, name, "", *adj, GWY_HSCALE_LOG);
    if (pspin)
        *pspin = spin;

    *punits = gwy_table_hscale_get_units(*adj);
    g_signal_connect_swapped(*adj, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), controls);
    row++;

    return row;
}

G_GNUC_UNUSED
static gint
gwy_synth_attach_orientation(GWY_SYNTH_CONTROLS *controls,
                             gint row,
                             GtkObject **adj,
                             gdouble *target)
{
    GtkWidget *spin;

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Orientation")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    *adj = gtk_adjustment_new(*target * 180.0/G_PI,
                              -180.0, 180.0, 1.0, 10.0, 0);
    g_object_set_data(G_OBJECT(*adj), "target", target);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, _("Orien_tation:"), "deg", *adj,
                                   GWY_HSCALE_DEFAULT);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    g_signal_connect_swapped(*adj, "value-changed",
                             G_CALLBACK(gwy_synth_angle_changed), controls);

    row++;

    return row;
}

G_GNUC_UNUSED
static gint
gwy_synth_attach_variance(GWY_SYNTH_CONTROLS *controls,
                          gint row,
                          GtkObject **adj,
                          gdouble *target)
{
    GtkWidget *spin;

    *adj = gtk_adjustment_new(*target, 0.0, 1.0, 0.01, 0.1, 0);
    g_object_set_data(G_OBJECT(*adj), "target", target);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, _("Variance:"), NULL, *adj,
                                   GWY_HSCALE_SQRT);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), FALSE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    row++;

    g_signal_connect_swapped(*adj, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), controls);

    return row;
}

G_GNUC_UNUSED
static gint
gwy_synth_attach_deformation(GWY_SYNTH_CONTROLS  *controls,
                             gint row,
                             GtkObject **adj_sigma,
                             gdouble *target_sigma,
                             GtkObject **adj_tau,
                             gdouble *target_tau,
                             GtkWidget **pvalue_tau,
                             GtkWidget **punits_tau)
{
    GtkWidget *spin;

    gtk_table_set_row_spacing(controls->table, row-1, 8);
    gtk_table_attach(controls->table, gwy_label_new_header(_("Deformation")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    *adj_sigma = gtk_adjustment_new(*target_sigma, 0.0, 100.0, 0.01, 1.0, 0);
    g_object_set_data(G_OBJECT(*adj_sigma), "target", target_sigma);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, _("_Amplitude:"), NULL, *adj_sigma,
                                   GWY_HSCALE_SQRT);
    row++;

    *adj_tau = gtk_adjustment_new(*target_tau, 0.1, 1000.0, 0.01, 1.0, 0);
    g_object_set_data(G_OBJECT(*adj_tau), "target", target_tau);

    spin = gwy_table_attach_hscale(GTK_WIDGET(controls->table),
                                   row, _("_Lateral scale:"), "px", *adj_tau,
                                   GWY_HSCALE_LOG);
    row++;

    *pvalue_tau = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(*pvalue_tau), 1.0, 0.5);
    gtk_table_attach(controls->table, *pvalue_tau,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_object_set_data(G_OBJECT(*adj_tau), "value-label", *pvalue_tau);

    *punits_tau = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(*punits_tau), 0.0, 0.5);
    gtk_table_attach(controls->table, *punits_tau,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    g_signal_connect_swapped(*adj_sigma, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), controls);
    g_signal_connect_swapped(*adj_tau, "value-changed",
                             G_CALLBACK(gwy_synth_double_changed), controls);
    g_signal_connect_swapped(*adj_tau, "value-changed",
                             G_CALLBACK(gwy_synth_update_lateral), controls);

    return row;
}

G_GNUC_UNUSED
static GtkWidget*
gwy_synth_instant_updates_new(GWY_SYNTH_CONTROLS *controls,
                              GtkWidget **pupdate,
                              GtkWidget **pinstant,
                              gboolean *target)
{
    GtkWidget *hbox;

    hbox = gtk_hbox_new(FALSE, 6);

    *pupdate = gtk_button_new_with_mnemonic(_("_Update"));
    gtk_widget_set_sensitive(*pupdate, !*target);
    gtk_box_pack_start(GTK_BOX(hbox), *pupdate, FALSE, FALSE, 0);

    *pinstant = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(*pinstant), *target);
    gtk_box_pack_start(GTK_BOX(hbox), *pinstant, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(*pinstant), "target", target);
    g_signal_connect_swapped(*pinstant, "toggled",
                             G_CALLBACK(gwy_synth_boolean_changed), controls);
    g_signal_connect(*pinstant, "toggled",
                     G_CALLBACK(gwy_synth_toggle_sensitive), *pupdate);

    return hbox;
}

G_GNUC_UNUSED
static void
gwy_synth_randomize_seed(GtkAdjustment *adj)
{
    /* Use the GLib's global PRNG for seeding */
    gtk_adjustment_set_value(adj, g_random_int() & 0x7fffffff);
}

G_GNUC_UNUSED
static GtkWidget*
gwy_synth_random_seed_new(GWY_SYNTH_CONTROLS *controls,
                          GtkObject **adj,
                          gint *target)
{
    GtkWidget *hbox, *button, *label, *spin;

    hbox = gtk_hbox_new(FALSE, 6);

    *adj = gtk_adjustment_new(*target, 1, 0x7fffffff, 1, 10, 0);
    g_object_set_data(G_OBJECT(*adj), "target", target);
    g_signal_connect_swapped(*adj, "value-changed",
                             G_CALLBACK(gwy_synth_int_changed), controls);

    label = gtk_label_new_with_mnemonic(_("R_andom seed:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(*adj), 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

    button = gtk_button_new_with_mnemonic(gwy_sgettext("seed|_New"));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_synth_randomize_seed), *adj);

    return hbox;
}

G_GNUC_UNUSED
static GtkWidget*
gwy_synth_randomize_new(gboolean *target)
{
    GtkWidget *button = gtk_check_button_new_with_mnemonic(_("Randomi_ze"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), *target);
    g_signal_connect(button, "toggled",
                     G_CALLBACK(gwy_synth_boolean_changed_silent), target);
    return button;
}

/* Create a square base surface for preview generation of an exact size */
G_GNUC_UNUSED
static GwyDataField*
gwy_synth_surface_for_preview(GwyDataField *dfield,
                              guint size)
{
    GwyDataField *retval;
    gint xres, yres, xoff, yoff;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    /* If the field is large enough, just cut an area from the centre. */
    if (xres >= size && yres >= size) {
        xoff = (xres - size)/2;
        yoff = (yres - size)/2;
        return gwy_data_field_area_extract(dfield, xoff, yoff, size, size);
    }

    if (xres <= yres) {
        yoff = (yres - xres)/2;
        dfield = gwy_data_field_area_extract(dfield, 0, yoff, xres, xres);
    }
    else {
        xoff = (xres - yres)/2;
        dfield = gwy_data_field_area_extract(dfield, xoff, 0, yres, yres);
    }

    retval = gwy_data_field_new_resampled(dfield, size, size,
                                          GWY_INTERPOLATION_KEY);
    g_object_unref(dfield);

    return retval;
}

G_GNUC_UNUSED
static void
gwy_synth_load_arg_double(GwyContainer *settings,
                          GString *key, const gchar *name,
                          gdouble min, gdouble max, gdouble *value)
{
    guint len = key->len;

    g_string_append(key, name);
    gwy_container_gis_double_by_name(settings, key->str, value);
    *value = CLAMP(*value, min, max);
    g_string_truncate(key, len);
}

G_GNUC_UNUSED
static void
gwy_synth_save_arg_double(GwyContainer *settings,
                          GString *key, const gchar *name,
                          gdouble value)
{
    guint len = key->len;

    g_string_append(key, name);
    gwy_container_set_double_by_name(settings, key->str, value);
    g_string_truncate(key, len);
}

G_GNUC_UNUSED
static void
gwy_synth_load_arg_boolean(GwyContainer *settings,
                           GString *key, const gchar *name,
                           gboolean *value)
{
    guint len = key->len;

    g_string_append(key, name);
    gwy_container_gis_boolean_by_name(settings, key->str, value);
    *value = !!*value;
    g_string_truncate(key, len);
}

G_GNUC_UNUSED
static void
gwy_synth_save_arg_boolean(GwyContainer *settings,
                           GString *key, const gchar *name,
                           gboolean value)
{
    guint len = key->len;

    g_string_append(key, name);
    gwy_container_set_boolean_by_name(settings, key->str, value);
    g_string_truncate(key, len);
}

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
