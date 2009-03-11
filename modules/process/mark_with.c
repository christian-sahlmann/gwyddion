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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/stats.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define MARK_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
    MARK_WITH_MASK = 0,
    MARK_WITH_DATA = 1,
    MARK_WITH_SHOW = 2,
    MARK_WITH_N
} MarkWithWhat;

/* Copied from the mask editor tool, might make sense to use the same enum. */
typedef enum {
    MASK_EDIT_SET       = 0,
    MASK_EDIT_ADD       = 1,
    MASK_EDIT_REMOVE    = 2,
    MASK_EDIT_INTERSECT = 3
} MaskEditMode;

enum {
    PREVIEW_SIZE = 240,
};

enum {
    RESPONSE_PREVIEW = 1,
};

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    MarkWithWhat mark_with;
    MaskEditMode operation;
    gdouble min;
    gdouble max;
    gboolean update;
} MarkArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *view_source;
    GtkWidget *view_result;
    GSList *operation;
    GSList *mark_with;
    GtkWidget *channels[MARK_WITH_N];
    gboolean has_any[MARK_WITH_N];
    GtkAdjustment *min;
    GtkAdjustment *max;
    GtkWidget *update;
    GwyContainer *mydata;
    gdouble data_min;
    gdouble data_max;
    GwyDataField *original_mask;
    GwyDataObjectId target;
    GwyDataObjectId source;
    MarkArgs *args;
    gboolean calculated;
} MarkControls;

static gboolean      module_register            (void);
static void          mark                       (GwyContainer *data,
                                                 GwyRunType run);
static void          mark_dialog                (MarkArgs *args,
                                                 const GwyDataObjectId *target,
                                                 GQuark mquark);
static void          mark_load_args             (GwyContainer *container,
                                                 MarkArgs *args);
static void          mark_save_args             (GwyContainer *container,
                                                 MarkArgs *args);
static void          mark_sanitize_args         (MarkArgs *args);
static void          operation_changed          (MarkControls *controls,
                                                 GtkWidget *button);
static void          mark_with_changed          (GtkWidget *button,
                                                 MarkControls *controls);
static void          channel_changed            (GwyDataChooser *chooser,
                                                 MarkControls *controls);
static void          min_changed                (MarkControls *controls,
                                                 GtkAdjustment *adj);
static void          max_changed                (MarkControls *controls,
                                                 GtkAdjustment *adj);
static void          update_changed             (GtkToggleButton *button,
                                                 MarkControls *controls);
static void          perform_operation          (MarkControls *controls);
static void          setup_source_view_data     (MarkControls *controls);
static void          ensure_mask_color          (GwyContainer *container,
                                                 const gchar *prefix);
static void          update_source_mask         (MarkControls *controls);
static GwyDataField* create_mask_field          (GwyDataField *dfield);
static void          gwy_data_field_threshold_to(GwyDataField *source,
                                                 GwyDataField *dest,
                                                 gdouble min,
                                                 gdouble max);
static gboolean      mask_attach_filter         (GwyContainer *source,
                                                 gint id,
                                                 gpointer user_data);
static gboolean      data_attach_filter         (GwyContainer *source,
                                                 gint id,
                                                 gpointer user_data);
static gboolean      show_attach_filter         (GwyContainer *source,
                                                 gint id,
                                                 gpointer user_data);

static const MarkArgs mark_with_defaults = {
    MARK_WITH_MASK,
    MASK_EDIT_SET,
    0.0, 1.0,
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates or modifies a mask using other channels."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("mark_with",
                              (GwyProcessFunc)&mark,
                              N_("/_Mask/Mark _With..."),
                              NULL,
                              MARK_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Mask combining and modification"));
    return TRUE;
}

static void
mark(GwyContainer *data, GwyRunType run)
{
    MarkArgs args;
    GwyDataObjectId target;
    GQuark mquark;

    g_return_if_fail(run & MARK_RUN_MODES);

    mark_load_args(gwy_app_settings_get(), &args);

    target.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &target.id,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);

    mark_dialog(&args, &target, mquark);
    mark_save_args(gwy_app_settings_get(), &args);
}

static void
mark_dialog(MarkArgs *args,
            const GwyDataObjectId *target,
            GQuark mquark)
{
    GtkWidget *dialog, *hbox, *vbox, *label;
    GtkTable *table;
    GtkTooltips *tips;
    MarkControls controls;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    GSList *l;
    gint response, row;
    guint i;

    dialog = gtk_dialog_new_with_buttons(_("Mark With"), NULL, 0, NULL);
    controls.dialog = dialog;
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), RESPONSE_PREVIEW,
                                      !args->update);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    tips = gwy_app_get_tooltips();

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       TRUE, TRUE, 4);

    controls.args = args;
    controls.target = *target;
    controls.source = *target;
    controls.original_mask = NULL;
    controls.calculated = FALSE;
    /* 0 == source, 1 == result */
    controls.mydata = gwy_container_new();

    gwy_container_gis_object(controls.target.data,
                             gwy_app_get_mask_key_for_id(controls.target.id),
                             &controls.original_mask);
    gwy_container_gis_object(controls.target.data,
                             gwy_app_get_data_key_for_id(controls.target.id),
                             &dfield);
    setup_source_view_data(&controls);

    /*setup result container*/
    gwy_container_set_object_by_name(controls.mydata, "/1/data", dfield);
    gwy_app_sync_data_items(target->data, controls.mydata, target->id, 1, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    if (controls.original_mask)
        dfield = gwy_data_field_duplicate(controls.original_mask);
    else
        dfield = create_mask_field(dfield);
    gwy_container_set_object_by_name(controls.mydata, "/1/mask", dfield);
    g_object_unref(dfield);
    ensure_mask_color(controls.mydata, "/1/mask");

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    /* Source mask preview */
    controls.view_source = gwy_data_view_new(controls.mydata);
    gtk_box_pack_start(GTK_BOX(vbox), controls.view_source, FALSE, FALSE, 4);

    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view_source), layer);
    layer = gwy_layer_mask_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/mask");
    gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
    gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls.view_source), layer);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view_source),
                                  "/0/data");
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view_source),
                              PREVIEW_SIZE);

    /* Result preview */
    controls.view_result = gwy_data_view_new(controls.mydata);
    gtk_box_pack_start(GTK_BOX(vbox), controls.view_result, FALSE, FALSE, 4);

    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/1/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/1/base/palette");
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view_result),
                                  "/1/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view_result), layer);
    layer = gwy_layer_mask_new();
    gwy_pixmap_layer_set_data_key(layer, "/1/mask");
    gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/1/mask");
    gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls.view_result), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view_result),
                              PREVIEW_SIZE);

    /* Controls */
    table = GTK_TABLE(gtk_table_new(12, 4, FALSE));
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(table), FALSE, TRUE, 0);
    row = 0;

    /* Operations */
    label = gtk_label_new(_("Operation:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.operation
        = gwy_radio_buttons_createl(G_CALLBACK(operation_changed), &controls,
                                    args->operation,
                                    _("Se_t mask"), MASK_EDIT_SET,
                                    _("_Add mask"), MASK_EDIT_ADD,
                                    _("_Subtract mask"), MASK_EDIT_REMOVE,
                                    _("_Intersect masks"), MASK_EDIT_INTERSECT,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.operation, table, 3, row);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    /* Mark with */
    label = gtk_label_new(_("Mark with:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.mark_with
        = gwy_radio_buttons_createl(G_CALLBACK(mark_with_changed), &controls,
                                    args->mark_with,
                                    _("_Mask"), MARK_WITH_MASK,
                                    _("_Data"), MARK_WITH_DATA,
                                    _("_Presentation"), MARK_WITH_SHOW,
                                    NULL);
    for (l = controls.mark_with; l; l = g_slist_next(l)) {
        GwyDataChooser *chooser;

        gtk_table_attach(table, GTK_WIDGET(l->data),
                         0, 1, row, row+1, GTK_FILL, 0, 0, 0);
        i = gwy_radio_button_get_value(GTK_WIDGET(l->data));
        controls.channels[i] = gwy_data_chooser_new_channels();
        chooser = GWY_DATA_CHOOSER(controls.channels[i]);
        if (i == MARK_WITH_MASK)
            gwy_data_chooser_set_filter(chooser, &mask_attach_filter,
                                        &controls.target, NULL);
        else if (i == MARK_WITH_DATA)
            gwy_data_chooser_set_filter(chooser, &data_attach_filter,
                                        &controls.target, NULL);
        else
            gwy_data_chooser_set_filter(chooser, &show_attach_filter,
                                        &controls.target, NULL);
        gtk_table_attach(table, controls.channels[i],
                         1, 3, row, row+1, GTK_FILL, 0, 0, 0);
        controls.has_any[i] = !!gwy_data_chooser_get_active(chooser, NULL);
        gtk_widget_set_sensitive(GTK_WIDGET(l->data), controls.has_any[i]);
        g_signal_connect(chooser, "changed",
                         G_CALLBACK(channel_changed), &controls);
        row++;
    }
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    /* Minimum */
    controls.min = GTK_ADJUSTMENT(gtk_adjustment_new(100.0*args->min,
                                                     0.0, 100.0, 0.01, 1.0, 0));
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("M_inimum:"), "%",
                            GTK_OBJECT(controls.min), 0);
    g_signal_connect_swapped(controls.min, "value-changed",
                             G_CALLBACK(min_changed), &controls);
    row++;

    /* Maximum */
    controls.max = GTK_ADJUSTMENT(gtk_adjustment_new(100.0*args->max,
                                                     0.0, 100.0, 0.01, 1.0, 0));
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("M_inimum:"), "%",
                            GTK_OBJECT(controls.max), 0);
    g_signal_connect_swapped(controls.max, "value-changed",
                             G_CALLBACK(max_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /* Instant update */
    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    g_signal_connect(controls.update, "toggled",
                     G_CALLBACK(update_changed), &controls);
    row++;

    /* Ensure something sensible is selected.  We know that at least the data
     * of the target channel itself is available, so there is not need to
     * handle the nothing-to-choose-from case. */
    if (controls.has_any[controls.args->mark_with])
        gwy_radio_buttons_set_current(controls.mark_with,
                                      controls.args->mark_with);
    else {
        for (i = 0; i < MARK_WITH_N; i++) {
            if (controls.has_any[i]) {
                gwy_radio_buttons_set_current(controls.mark_with, i);
                break;
            }
        }
    }

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            if (!controls.calculated) {
                setup_source_view_data(&controls);
                update_source_mask(&controls);
            }
            break;

            case RESPONSE_PREVIEW:
            setup_source_view_data(&controls);
            update_source_mask(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    dfield = gwy_container_get_object_by_name(controls.mydata, "/1/mask");
    gwy_app_undo_qcheckpointv(target->data, 1, &mquark);
    gwy_container_set_object(target->data, mquark, dfield);

    g_object_unref(controls.mydata);
}

/*fit data*/
static void
perform_operation(MarkControls *controls)
{
    MarkArgs *args = controls->args;
    GwyDataField *source, *mfield;

    source = gwy_container_get_object_by_name(controls->mydata, "/0/mask");
    mfield = gwy_container_get_object_by_name(controls->mydata, "/1/mask");

    /* Simple cases. */
    if (!controls->original_mask || args->operation == MASK_EDIT_SET) {
        if (args->operation == MASK_EDIT_SET
            || args->operation == MASK_EDIT_ADD)
            gwy_data_field_copy(source, mfield, FALSE);
        else
            gwy_data_field_clear(mfield);

        gwy_data_field_data_changed(mfield);
        return;
    }

    /* Not so simple cases. */
    if (args->operation == MASK_EDIT_ADD)
        gwy_data_field_max_of_fields(mfield, controls->original_mask, source);
    else if (args->operation == MASK_EDIT_INTERSECT)
        gwy_data_field_min_of_fields(mfield, controls->original_mask, source);
    else if (args->operation == MASK_EDIT_REMOVE) {
        const gdouble *sdata, *odata;
        gdouble *mdata;
        gint i, n;

        n = gwy_data_field_get_xres(source) * gwy_data_field_get_yres(source);
        mdata = gwy_data_field_get_data(mfield);
        odata = gwy_data_field_get_data_const(controls->original_mask);
        sdata = gwy_data_field_get_data_const(source);
        for (i = 0; i < n; i++)
            mdata[i] = MIN(odata[i], 1.0 - sdata[i]);
    }

    gwy_data_field_data_changed(mfield);
    controls->calculated = TRUE;
}

static void
operation_changed(MarkControls *controls,
                  GtkWidget *button)
{
    controls->args->operation = gwy_radio_button_get_value(button);
    controls->calculated = FALSE;
    if (controls->args->update)
        perform_operation(controls);
}

static void
mark_with_changed(GtkWidget *button,
                  MarkControls *controls)
{
    controls->args->mark_with = gwy_radio_button_get_value(button);

    /* TODO: Must do also on dialog setup. */
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->min),
                                   controls->args->mark_with != MARK_WITH_MASK);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->max),
                                   controls->args->mark_with != MARK_WITH_MASK);

    channel_changed(GWY_DATA_CHOOSER(controls->channels[controls->args->mark_with]),
                    controls);
}

static void
channel_changed(GwyDataChooser *chooser,
                MarkControls *controls)
{
    /* The channel type is recorded in mark_with, it cannot change here. */
    controls->source.data = gwy_data_chooser_get_active(chooser,
                                                        &controls->source.id);
    controls->calculated = FALSE;
    if (controls->args->update) {
        setup_source_view_data(controls);
        update_source_mask(controls);
    }
}

static void
min_changed(MarkControls *controls,
            GtkAdjustment *adj)
{
    controls->args->min = gtk_adjustment_get_value(adj)/100.0;
    controls->calculated = FALSE;
    if (controls->args->update)
        update_source_mask(controls);
}

static void
max_changed(MarkControls *controls,
            GtkAdjustment *adj)
{
    controls->args->max = gtk_adjustment_get_value(adj)/100.0;
    controls->calculated = FALSE;
    if (controls->args->update)
        update_source_mask(controls);
}

static void
setup_source_view_data(MarkControls *controls)
{
    MarkArgs *args = controls->args;
    GwyDataField *dfield, *mfield;
    GQuark quark;

    /* Base field to display under the source mask.  This is read-only, so
     * we can just reference it. */
    if (args->mark_with == MARK_WITH_MASK || args->mark_with == MARK_WITH_DATA)
        quark = gwy_app_get_data_key_for_id(controls->source.id);
    else
        quark = gwy_app_get_show_key_for_id(controls->source.id);

    dfield = gwy_container_get_object(controls->source.data, quark);
    gwy_container_set_object_by_name(controls->mydata, "/0/data", dfield);

    if (args->mark_with == MARK_WITH_DATA || args->mark_with == MARK_WITH_SHOW)
        gwy_data_field_get_min_max(dfield,
                                   &controls->data_min, &controls->data_max);
    else {
        controls->data_min = 0.0;
        controls->data_max = 1.0;
    }

    /* Source mask, this is modified often, just ensure it exists */
    if (!gwy_container_gis_object_by_name(controls->mydata, "/0/mask",
                                          &mfield)) {
        if (controls->original_mask)
            mfield = gwy_data_field_duplicate(controls->original_mask);
        else {
            quark = gwy_app_get_data_key_for_id(controls->target.id);
            mfield = gwy_container_get_object(controls->target.data, quark);
            mfield = create_mask_field(mfield);
        }
        gwy_container_set_object_by_name(controls->mydata, "/0/mask", mfield);
        g_object_unref(mfield);
    }

    /* Sync meta-stuff to make the source look like in the rest of the app */
    gwy_app_sync_data_items(controls->source.data, controls->mydata,
                            controls->source.id, 0,
                            TRUE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            /* FIXME: Does not work with show GWY_DATA_ITEM_RANGE, */
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    ensure_mask_color(controls->mydata, "/0/mask");
}

static void
ensure_mask_color(GwyContainer *container,
                  const gchar *prefix)
{
    GwyRGBA rgba;

    if (!gwy_rgba_get_from_container(&rgba, container, prefix)) {
        gwy_rgba_get_from_container(&rgba, gwy_app_settings_get(), "/mask");
        gwy_rgba_store_to_container(&rgba, container, prefix);
    }
}

static void
update_source_mask(MarkControls *controls)
{
    MarkArgs *args = controls->args;
    GQuark quark;
    GwyDataField *dfield, *mfield;
    double d;

    /* Assume setup_source_view_data() has been called. */
    mfield = gwy_container_get_object_by_name(controls->mydata, "/0/mask");

    if (args->mark_with == MARK_WITH_MASK) {
        quark = gwy_app_get_mask_key_for_id(controls->source.id);
        dfield = gwy_container_get_object(controls->source.data, quark);
        gwy_data_field_copy(dfield, mfield, FALSE);
        gwy_data_field_data_changed(mfield);
        return;
    }

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    d = controls->data_max - controls->data_min;
    gwy_data_field_threshold_to(dfield, mfield,
                                controls->data_min + d*args->min,
                                controls->data_min + d*args->max);
    gwy_data_field_data_changed(mfield);
    /* FIXME: Does not really belong here... */
    perform_operation(controls);
}

static GwyDataField*
create_mask_field(GwyDataField *dfield)
{
    GwyDataField *mfield;
    GwySIUnit *siunit;

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    siunit = gwy_si_unit_new(NULL);
    gwy_data_field_set_si_unit_z(mfield, siunit);
    g_object_unref(siunit);

    return mfield;
}

static void
gwy_data_field_threshold_to(GwyDataField *source, GwyDataField *dest,
                            gdouble min, gdouble max)
{
    const gdouble *dsrc;
    gdouble *ddst;
    gint i, n;

    n = gwy_data_field_get_xres(source) * gwy_data_field_get_yres(source);
    dsrc = gwy_data_field_get_data_const(source);
    ddst = gwy_data_field_get_data(dest);

    if (min <= max) {
        for (i = 0; i < n; i++)
            ddst[i] = (dsrc[i] >= min && dsrc[i] <= max);
    }
    else {
        for (i = 0; i < n; i++)
            ddst[i] = (dsrc[i] >= min || dsrc[i] <= max);
    }
}

static void
update_changed(GtkToggleButton *button, MarkControls *controls)
{
    controls->args->update = gtk_toggle_button_get_active(button);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->update);
    if (controls->args->update && !controls->calculated) {
        setup_source_view_data(controls);
        update_source_mask(controls);
    }
}

static gboolean
mask_attach_filter(GwyContainer *source,
                   gint id,
                   gpointer user_data)
{
    const GwyDataObjectId *target = (const GwyDataObjectId*)user_data;
    GwyDataField *source_dfield, *target_dfield;
    GQuark quark;

    quark = gwy_app_get_mask_key_for_id(id);
    if (!gwy_container_gis_object(source, quark, &source_dfield))
        return FALSE;

    quark = gwy_app_get_data_key_for_id(target->id);
    target_dfield = GWY_DATA_FIELD(gwy_container_get_object(target->data,
                                                            quark));

    return !gwy_data_field_check_compatibility(source_dfield, target_dfield,
                                               GWY_DATA_COMPATIBILITY_RES
                                               | GWY_DATA_COMPATIBILITY_REAL
                                               | GWY_DATA_COMPATIBILITY_LATERAL);
}

static gboolean
data_attach_filter(GwyContainer *source,
                   gint id,
                   gpointer user_data)
{
    const GwyDataObjectId *target = (const GwyDataObjectId*)user_data;
    GwyDataField *source_dfield, *target_dfield;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    if (!gwy_container_gis_object(source, quark, &source_dfield))
        return FALSE;

    quark = gwy_app_get_data_key_for_id(target->id);
    target_dfield = GWY_DATA_FIELD(gwy_container_get_object(target->data,
                                                            quark));

    return !gwy_data_field_check_compatibility(source_dfield, target_dfield,
                                               GWY_DATA_COMPATIBILITY_RES
                                               | GWY_DATA_COMPATIBILITY_REAL
                                               | GWY_DATA_COMPATIBILITY_LATERAL);
}

static gboolean
show_attach_filter(GwyContainer *source,
                   gint id,
                   gpointer user_data)
{
    const GwyDataObjectId *target = (const GwyDataObjectId*)user_data;
    GwyDataField *source_dfield, *target_dfield;
    GQuark quark;

    quark = gwy_app_get_show_key_for_id(id);
    if (!gwy_container_gis_object(source, quark, &source_dfield))
        return FALSE;

    quark = gwy_app_get_data_key_for_id(target->id);
    target_dfield = GWY_DATA_FIELD(gwy_container_get_object(target->data,
                                                            quark));

    return !gwy_data_field_check_compatibility(source_dfield, target_dfield,
                                               GWY_DATA_COMPATIBILITY_RES
                                               | GWY_DATA_COMPATIBILITY_REAL
                                               | GWY_DATA_COMPATIBILITY_LATERAL);
}

static const gchar mark_with_key[] = "/module/mark_with/mark_with";
static const gchar operation_key[] = "/module/mark_with/operation";
static const gchar min_key[]       = "/module/mark_with/min";
static const gchar max_key[]       = "/module/mark_with/max";
static const gchar update_key[]    = "/module/fft_filter_1d/update";

static void
mark_sanitize_args(MarkArgs *args)
{
    args->mark_with = MIN(args->mark_with, MARK_WITH_N-1);
    args->operation = MIN(args->operation, MASK_EDIT_INTERSECT);
    args->min = CLAMP(args->min, 0.0, 1.0);
    args->max = CLAMP(args->max, 0.0, 1.0);
    args->update = !!args->update;
}

static void
mark_load_args(GwyContainer *container,
               MarkArgs *args)
{
    *args = mark_with_defaults;

    gwy_container_gis_enum_by_name(container, mark_with_key, &args->mark_with);
    gwy_container_gis_enum_by_name(container, operation_key, &args->operation);
    gwy_container_gis_double_by_name(container, min_key, &args->min);
    gwy_container_gis_double_by_name(container, max_key, &args->max);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);

    mark_sanitize_args(args);
}

static void
mark_save_args(GwyContainer *container,
               MarkArgs *args)
{
    gwy_container_set_enum_by_name(container, mark_with_key, args->mark_with);
    gwy_container_set_enum_by_name(container, operation_key, args->operation);
    gwy_container_set_double_by_name(container, min_key, args->min);
    gwy_container_set_double_by_name(container, max_key, args->max);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
