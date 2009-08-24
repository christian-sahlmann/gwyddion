#ifndef __GWY_DIMENSIONS_H__
#define __GWY_DIMENSIONS_H__ 1

#include <libgwydgets/gwycombobox.h>

typedef struct {
    gint xres;
    gint yres;
    gdouble measure;
    gchar *xyunits;
    gchar *zunits;
    gint xyexponent;
    gint zexponent;
} GwyDimensionArgs;

#define GWY_DIMENSION_ARGS_INIT { 256, 256, 1.0, NULL, NULL, 0, 0 }

typedef struct {
    GwyDimensionArgs *args;
    GwySIUnit *siunit;
    GtkWidget *table;
    GtkAdjustment *xres;
    GtkAdjustment *yres;
    GtkWidget *xyreseq;
    GtkAdjustment *xreal;
    GtkAdjustment *yreal;
    GtkWidget *xunitslab;
    GtkWidget *yunitslab;
    GtkWidget *xyexponent;
    GtkWidget *xyunits;
    GtkWidget *zexponent;
    GtkWidget *zunits;
    gboolean in_update;
} GwyDimensions;

static GtkAdjustment*
gwy_dimensions_make_res(GtkTable *table,
                        gint row,
                        const gchar *name,
                        gint value)
{
    GtkWidget *label, *spin;
    GtkAdjustment *adj;
    GtkObject *obj;

    label = gtk_label_new_with_mnemonic(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    obj = gtk_adjustment_new(value, 2, 16384, 1, 100, 0);
    adj = GTK_ADJUSTMENT(obj);
    spin = gtk_spin_button_new(adj, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);
    gtk_table_attach(table, spin, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new("px");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    return adj;
}

static GtkAdjustment*
gwy_dimensions_make_real(GtkTable *table,
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
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    obj = gtk_adjustment_new(value, 0.001, 10000.0, 1, 100, 0);
    adj = GTK_ADJUSTMENT(obj);
    spin = gtk_spin_button_new(adj, 0, 3);
    gtk_table_attach(table, spin, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    *unitlab = gtk_label_new(units);
    gtk_misc_set_alignment(GTK_MISC(*unitlab), 0.0, 0.5);
    gtk_table_attach(table, *unitlab, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    return adj;
}

static GtkWidget*
gwy_dimensions_make_units(GtkTable *table,
                          gint row,
                          const gchar *name,
                          gint pwr,
                          GwySIUnit *siunit,
                          GtkWidget **combo)
{
    GtkWidget *label, *changer;

    label = gtk_label_new_with_mnemonic(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    *combo = gwy_combo_box_metric_unit_new(NULL, NULL,
                                           pwr - 6, pwr + 6, siunit, pwr);
    gtk_table_attach(table, *combo, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    changer = gtk_button_new_with_label(_("Change"));
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
    g_object_unref(unit);
}

static void
gwy_dimensions_units_changed(GtkWidget *button,
                             GwyDimensions *dims)
{
    GtkWidget *dialog, *hbox, *label, *toplevel;
    GtkEntry *entry;
    GtkWindow *parent = NULL;
    const gchar *id, *unit;
    gint response;

    id = g_object_get_data(G_OBJECT(button), "id");
    g_return_if_fail(gwy_strequal(id, "xy") || gwy_strequal(id, "z"));
    toplevel = gtk_widget_get_toplevel(dims->table);
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
    if (gwy_strequal(id, "xy"))
        gtk_entry_set_text(entry, dims->args->xyunits);
    else if (gwy_strequal(id, "z"))
        gtk_entry_set_text(entry, dims->args->zunits);
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
    if (gwy_strequal(id, "xy")) {
        g_free(dims->args->xyunits);
        dims->args->xyunits = g_strdup(unit);
        gwy_dimensions_set_combo_from_unit(GTK_COMBO_BOX(dims->xyexponent),
                                           unit);
    }
    else if (gwy_strequal(id, "z")) {
        g_free(dims->args->zunits);
        dims->args->zunits = g_strdup(unit);
        gwy_dimensions_set_combo_from_unit(GTK_COMBO_BOX(dims->zexponent),
                                           unit);
    }

    gtk_widget_destroy(dialog);
}

static void
gwy_dimensions_xres_changed(GtkAdjustment *adj,
                            GwyDimensions *dims)
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
gwy_dimensions_yres_changed(GtkAdjustment *adj,
                            GwyDimensions *dims)
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
gwy_dimensions_xreal_changed(GtkAdjustment *adj,
                             GwyDimensions *dims)
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
gwy_dimensions_yreal_changed(GtkAdjustment *adj,
                             GwyDimensions *dims)
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
gwy_dimensions_xyreseq_toggled(GtkToggleButton *toggle,
                               GwyDimensions *dims)
{
    if (gtk_toggle_button_get_active(toggle))
        gtk_adjustment_set_value(dims->yres, dims->args->xres);
}

static GwyDimensions*
gwy_dimensions_new(GwyDimensionArgs *args)
{
    GwyDimensions *dims = g_new0(GwyDimensions, 1);
    GtkWidget *label;
    GtkTable *table;
    gint row;

    dims->args = args;
    dims->siunit = gwy_si_unit_new(NULL);

    dims->table = gtk_table_new(10, 3, FALSE);
    table = GTK_TABLE(dims->table);
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    row = 0;

    /* Resolution */
    label = gwy_label_new_header(_("Resolution"));
    gtk_table_attach(table, label, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    dims->xres = gwy_dimensions_make_res(table, row++, _("_Horizontal size:"),
                                         args->xres);
    dims->yres = gwy_dimensions_make_res(table, row++, _("_Vertical size:"),
                                         args->yres);

    dims->xyreseq = gtk_check_button_new_with_mnemonic(_("S_quare image"));
    gtk_table_attach(table, dims->xyreseq, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dims->xyreseq),
                                 args->xres == args->yres);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    /* Physical dimensions */
    label = gwy_label_new_header(_("Physical Dimensions"));
    gtk_table_attach(table, label, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    /* XXX: Must recalculate the unit strings! */
    dims->xres = gwy_dimensions_make_real(table, row++, _("_Width:"),
                                          args->xres * args->measure,
                                          args->xyunits, &dims->xunitslab);
    dims->yres = gwy_dimensions_make_real(table, row++, _("H_eight:"),
                                          args->yres * args->measure,
                                          args->zunits, &dims->yunitslab);
    gtk_table_set_row_spacing(table, row-1, 8);

    /* Units */
    label = gwy_label_new_header(_("Units"));
    gtk_table_attach(table, label, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    gwy_si_unit_set_from_string(dims->siunit, args->xyunits);
    dims->xyunits = gwy_dimensions_make_units(table, row++,
                                              _("Dimension units:"),
                                              args->xyexponent, dims->siunit,
                                              &dims->xyexponent);

    gwy_si_unit_set_from_string(dims->siunit, args->zunits);
    dims->zunits = gwy_dimensions_make_units(table, row++,
                                             _("Value units:"),
                                             args->zexponent, dims->siunit,
                                             &dims->zexponent);

    /* Final setup */
    /*
    g_signal_connect(dims->xres, "value-changed",
                     G_CALLBACK(gwy_dimensions_xres_changed), dims);
    g_signal_connect(dims->yres, "value-changed",
                     G_CALLBACK(gwy_dimensions_yres_changed), dims);
    g_signal_connect(dims->xyreseq, "toggled",
                     G_CALLBACK(gwy_dimensions_xyreseq_toggled), dims);
    g_signal_connect(dims->xreal, "value-changed",
                     G_CALLBACK(gwy_dimensions_xreal_changed), dims);
    g_signal_connect(dims->yreal, "value-changed",
                     G_CALLBACK(gwy_dimensions_yreal_changed), dims);
                     */

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
    g_object_unref(dims->siunit);
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

    g_string_append(g_string_truncate(key, len), "xyexponent");
    gwy_container_gis_int32_by_name(settings, key->str, &args->xyexponent);

    g_string_append(g_string_truncate(key, len), "zexponent");
    gwy_container_gis_int32_by_name(settings, key->str, &args->zexponent);

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

    g_string_append(g_string_truncate(key, len), "xyexponent");
    gwy_container_set_int32_by_name(settings, key->str, args->xyexponent);

    g_string_append(g_string_truncate(key, len), "zexponent");
    gwy_container_set_int32_by_name(settings, key->str, args->zexponent);

    g_string_append(g_string_truncate(key, len), "xyunits");
    s = args->xyunits;
    gwy_container_set_string_by_name(settings, key->str, g_strdup(s ? s : ""));

    g_string_append(g_string_truncate(key, len), "zunits");
    s = args->zunits;
    gwy_container_set_string_by_name(settings, key->str, g_strdup(s ? s : ""));

    g_string_free(key, TRUE);
}

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
