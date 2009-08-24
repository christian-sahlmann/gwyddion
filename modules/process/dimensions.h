#ifndef __GWY_DIMENSIONS_H__
#define __GWY_DIMENSIONS_H__ 1

#include <libgwydgets/gwycombobox.h>

typedef struct {
    gint xres;
    gint yres;
    gdouble measure;
    gdouble zscale;
    gchar *xyunits;
    gchar *zunits;
    gint xyexponent;
    gint zexponent;
} GwyDimensionArgs;

#define GWY_DIMENSION_ARGS_INIT { 256, 256, 1.0, 1.0, NULL, NULL, 0, 0 }

typedef struct {
    GwyDimensionArgs *args;
    GtkWidget *table;
    GtkAdjustment *xres;
    GtkAdjustment *yres;
    GtkWidget *xyreseq;
    GtkAdjustment *xreal;
    GtkAdjustment *yreal;
    GtkAdjustment *zscale;
    GtkWidget *xyexponent;
    GtkWidget *zexponent;
    gboolean in_update;
} GwyDimensions;

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

static GtkWidget*
gwy_dimensions_make_unit_changer(GwyDimensions *dims,
                                 const gchar *id,
                                 gint *pwr,
                                 const gchar *unit,
                                 GtkSizeGroup *sizegroup,
                                 GtkWidget **combo)
{
    GtkWidget *hbox, *align, *changer;
    GwySIUnit *siunit;

    align = gtk_alignment_new(0.0, 0.5, 1.0, 0.0);

    hbox = gtk_hbox_new(FALSE, 6);
    gtk_size_group_add_widget(sizegroup, hbox);
    gtk_container_add(GTK_CONTAINER(align), hbox);

    siunit = gwy_si_unit_new(unit);
    *combo = gwy_combo_box_metric_unit_new
                              (G_CALLBACK(gwy_enum_combo_box_update_int), pwr,
                               *pwr - 6, *pwr + 6, siunit, *pwr);
    g_object_unref(siunit);
    gtk_box_pack_start(GTK_BOX(hbox), *combo, FALSE, FALSE, 0);

    changer = gtk_button_new_with_label(_("Change"));
    g_object_set_data(G_OBJECT(changer), "id", (gpointer)id);
    g_signal_connect(changer, "clicked",
                     G_CALLBACK(gwy_dimensions_units_changed), dims);
    gtk_box_pack_end(GTK_BOX(hbox), changer, FALSE, FALSE, 0);

    return align;
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

static void
gwy_dimensions_zscale_changed(GtkAdjustment *adj,
                              GwyDimensions *dims)
{
    dims->args->zscale = gtk_adjustment_get_value(adj);
}

static GwyDimensions*
gwy_dimensions_new(GwyDimensionArgs *args,
                   const gchar *zname)
{
    GwyDimensions *dims = g_new0(GwyDimensions, 1);
    GtkWidget *label, *align, *spin;
    GtkSizeGroup *sizegroup;
    GtkObject *obj;
    GtkTable *table;
    gint row;

    dims->args = args;

    sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    dims->table = gtk_table_new(4, 5, FALSE);
    table = GTK_TABLE(dims->table);
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    //gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    row = 0;

    /* Horizontal */
    label = gtk_label_new_with_mnemonic(_("_Horizontal size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    obj = gtk_adjustment_new(args->xres, 2, 16384, 1, 100, 0);
    dims->xres = GTK_ADJUSTMENT(obj);
    spin = gtk_spin_button_new(dims->xres, 0, 0);
    gtk_table_attach(table, spin, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    obj = gtk_adjustment_new(args->xres * args->measure,
                             0.001, 10000.0, 1, 100, 0);
    dims->xreal = GTK_ADJUSTMENT(obj);
    spin = gtk_spin_button_new(dims->xreal, 0, 3);
    gtk_table_attach(table, spin, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    row++;

    /* Vertical */
    label = gtk_label_new_with_mnemonic(_("_Vertical size:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    obj = gtk_adjustment_new(args->yres, 2, 16384, 1, 100, 0);
    dims->yres = GTK_ADJUSTMENT(obj);
    spin = gtk_spin_button_new(dims->yres, 0, 0);
    gtk_table_attach(table, spin, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    obj = gtk_adjustment_new(args->yres * args->measure,
                             0.001, 10000.0, 1, 100, 0);
    dims->yreal = GTK_ADJUSTMENT(obj);
    spin = gtk_spin_button_new(dims->yreal, 0, 3);
    gtk_table_attach(table, spin, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    /* Lateral units */
    align = gwy_dimensions_make_unit_changer(dims, "xy", &args->xyexponent,
                                             args->xyunits, sizegroup,
                                             &dims->xyexponent);
    gtk_table_attach(table, align, 3, 4, row-1, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
    row++;

    /* Squareness */
    dims->xyreseq = gtk_check_button_new_with_mnemonic(_("S_quare sample"));
    gtk_table_attach_defaults(table, dims->xyreseq, 0, 4, row, row+1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dims->xyreseq),
                                 args->xres == args->yres);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    /* Z scale */
    label = gtk_label_new_with_mnemonic(zname);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 2, row, row+1, GTK_FILL, 0, 0, 0);

    obj = gtk_adjustment_new(args->zscale, 0.001, 10000.0, 1, 100, 0);
    dims->zscale = GTK_ADJUSTMENT(obj);
    spin = gtk_spin_button_new(dims->zscale, 0, 3);
    gtk_table_attach(table, spin, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    /* Z units */
    align = gwy_dimensions_make_unit_changer(dims, "z", &args->zexponent,
                                             args->zunits, sizegroup,
                                             &dims->zexponent);
    gtk_table_attach(table, align, 3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
    row++;

    /* Final setup */
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
    g_signal_connect(dims->zscale, "value-changed",
                     G_CALLBACK(gwy_dimensions_zscale_changed), dims);

    g_object_unref(sizegroup);

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

    g_string_append(g_string_truncate(key, len), "zscale");
    gwy_container_gis_double_by_name(settings, key->str, &args->zscale);

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

    g_string_append(g_string_truncate(key, len), "zscale");
    gwy_container_set_double_by_name(settings, key->str, args->zscale);

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
