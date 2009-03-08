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
    GtkWidget *view_mask;
    GtkWidget *view_result;
    GSList *operation;
    GSList *mark_with;
    GtkWidget *channels[MARK_WITH_N];
    GtkAdjustment *min;
    GtkAdjustment *max;
    GtkWidget *update;
    GwyContainer *original_data;
    GwyContainer *result_data;
    GwyDataField *original_field;
    MarkArgs *args;
} MarkControls;

static gboolean module_register    (void);
static void     mark               (GwyContainer *data,
                                    GwyRunType run);
static void     mark_dialog        (MarkArgs *args,
                                    GwyContainer *data,
                                    GwyDataField *dfield,
                                    gint id);
static void     mark_load_args     (GwyContainer *container,
                                    MarkArgs *args);
static void     mark_save_args     (GwyContainer *container,
                                    MarkArgs *args);
static void     mark_sanitize_args (MarkArgs *args);
static void     mark_run           (MarkControls *controls,
                                    MarkArgs *args);
static void     mark_do            (MarkControls *controls);
static void     mark_dialog_abandon(MarkControls *controls,
                                    MarkArgs *args);
static void     operation_changed  (MarkControls *controls,
                                    GtkWidget *button);
static void     mark_with_changed  (GtkWidget *button,
                                    MarkControls *controls);
static void     min_changed        (MarkControls *controls,
                                    GtkAdjustment *adj);
static void     max_changed        (MarkControls *controls,
                                    GtkAdjustment *adj);
static void     update_changed_cb  (GtkToggleButton *button,
                                    MarkControls *controls);
static void     update_view        (MarkControls *controls,
                                    MarkArgs *args);
static gboolean mask_attach_filter (GwyContainer *source,
                                    gint id,
                                    gpointer user_data);
static gboolean data_attach_filter (GwyContainer *source,
                                    gint id,
                                    gpointer user_data);
static gboolean show_attach_filter (GwyContainer *source,
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
    GwyDataField *dfield, *mfield;
    GQuark mquark;
    gint id;

    g_return_if_fail(run & MARK_RUN_MODES);

    mark_load_args(gwy_app_settings_get(), &args);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);
    g_return_if_fail(dfield);

    mark_dialog(&args, data, dfield, id);
    mark_save_args(gwy_app_settings_get(), &args);
}

static void
mark_dialog(MarkArgs *args,
            GwyContainer *data,
            GwyDataField *dfield,
            gint id)
{
    static struct {
        guint type;
        const gchar *stock_id;
        const gchar *text;
    }
    const operations[] = {
        {
            MASK_EDIT_SET,
            GWY_STOCK_MASK,
            N_("Set mask to selection"),
        },
        {
            MASK_EDIT_ADD,
            GWY_STOCK_MASK_ADD,
            N_("Add selection to mask"),
        },
        {
            MASK_EDIT_REMOVE,
            GWY_STOCK_MASK_SUBTRACT,
            N_("Subtract selection from mask"),
        },
        {
            MASK_EDIT_INTERSECT,
            GWY_STOCK_MASK_INTERSECT,
            N_("Intersect selection with mask"),
        },
    };

    GtkWidget *dialog, *hbox, *vbox, *label;
    GtkRadioButton *group;
    GtkTable *table;
    GtkBox *hbox2;
    GtkTooltips *tips;
    GwyDataObjectId target;
    MarkControls controls;
    GwyDataField *result_field;
    GwyPixmapLayer *layer;
    GSList *l;
    GQuark quark;
    gint response, row;
    guint i;
    gchar *key;

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

    /* store pointer to data container */
    controls.args = args;
    controls.original_data = data;
    controls.original_field = dfield;

    target.data = data;
    target.id = id;

    /*setup result container*/
    result_field = gwy_data_field_new_alike(dfield, TRUE);
    controls.result_data = gwy_container_new();
    gwy_container_set_object_by_name(controls.result_data, "/0/data",
                                     result_field);
    gwy_app_sync_data_items(data, controls.result_data, id, 0, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    g_object_unref(result_field);

    vbox = gtk_vbox_new(FALSE, 4);
    /*set up rescaled image of the surface*/
    controls.view_mask = gwy_data_view_new(controls.original_data);
    layer = gwy_layer_basic_new();
    quark = gwy_app_get_data_key_for_id(id);
    gwy_pixmap_layer_set_data_key(layer, g_quark_to_string(quark));
    key = g_strdup_printf("/%d/base/palette", id);
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), key);
    g_free(key);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view_mask),
                                  g_quark_to_string(quark));
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view_mask), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view_mask),
                              PREVIEW_SIZE);

    /*set up fit controls*/
    gtk_box_pack_start(GTK_BOX(vbox), controls.view_mask, FALSE, FALSE, 4);

    /*set up rescaled image of the result*/
    controls.view_result = gwy_data_view_new(controls.result_data);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view_result),
                                  "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view_result), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view_result),
                              PREVIEW_SIZE);

    gtk_box_pack_start(GTK_BOX(vbox), controls.view_result, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    /*settings*/
    table = GTK_TABLE(gtk_table_new(12, 4, FALSE));
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(table), FALSE, TRUE, 0);
    row = 0;

    /* Operations */
    label = gtk_label_new(_("Operation:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    hbox2 = GTK_BOX(gtk_hbox_new(FALSE, 0));
    gtk_table_attach(table, GTK_WIDGET(hbox2),
                     1, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    group = NULL;
    for (i = 0; i < G_N_ELEMENTS(operations); i++) {
        GtkWidget *button, *image;

        button = gtk_radio_button_new_from_widget(group);
        g_object_set(button, "draw-indicator", FALSE, NULL);
        image = gtk_image_new_from_stock(operations[i].stock_id,
                                         GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_container_add(GTK_CONTAINER(button), image);
        gwy_radio_button_set_value(button, operations[i].type);
        gtk_box_pack_start(hbox2, button, FALSE, FALSE, 0);
        gtk_tooltips_set_tip(tips, button, gettext(operations[i].text), NULL);
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(operation_changed), &controls);
        if (!group)
            group = GTK_RADIO_BUTTON(button);
    }
    controls.operation = gtk_radio_button_get_group(group);
    gwy_radio_buttons_set_current(controls.operation, controls.args->operation);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

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
        gtk_table_attach(table, GTK_WIDGET(l->data),
                         0, 1, row, row+1, GTK_FILL, 0, 0, 0);
        i = gwy_radio_button_get_value(GTK_WIDGET(l->data));
        controls.channels[i] = gwy_data_chooser_new_channels();
        if (i == MARK_WITH_MASK)
            gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(controls.channels[i]),
                                        &mask_attach_filter, &target, NULL);
        else if (i == MARK_WITH_DATA)
            gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(controls.channels[i]),
                                        &data_attach_filter, &target, NULL);
        else
            gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(controls.channels[i]),
                                        &show_attach_filter, &target, NULL);
        gtk_table_attach(table, controls.channels[i],
                         1, 3, row, row+1, GTK_FILL, 0, 0, 0);
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
                     G_CALLBACK(update_changed_cb), &controls);
    row++;

    update_view(&controls, args);

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
            mark_do(&controls);
            break;

            case RESPONSE_PREVIEW:
            mark_run(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    mark_dialog_abandon(&controls, args);
}

static void
mark_dialog_abandon(MarkControls *controls,
                    G_GNUC_UNUSED MarkArgs *args)
{
    g_object_unref(controls->result_data);
}

/*update preview depending on user's wishes*/
static void
update_view(MarkControls *controls,
            MarkArgs *args)
{
    GwyDataField *rfield;

    gwy_debug("args->update = %d", args->update);
    rfield = gwy_container_get_object_by_name(controls->result_data, "/0/data");

    gwy_data_field_data_changed(rfield);
}

/*fit data*/
static void
mark_run(MarkControls *controls,
            MarkArgs *args)
{
    update_view(controls, args);
}

/*dialog finished, export result data*/
static void
mark_do(MarkControls *controls)
{
    GwyDataField *rfield;
    gint newid;

    rfield = gwy_container_get_object_by_name(controls->result_data, "/0/data");
    newid = gwy_app_data_browser_add_data_field(rfield, controls->original_data,
                                                TRUE);
}

static void
operation_changed(MarkControls *controls,
                  GtkWidget *button)
{
}

static void
mark_with_changed(GtkWidget *button,
                  MarkControls *controls)
{
    controls->args->mark_with = gwy_radio_button_get_value(button);

    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->min),
                                   controls->args->mark_with != MARK_WITH_MASK);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->max),
                                   controls->args->mark_with != MARK_WITH_MASK);
}

static void
min_changed(MarkControls *controls,
            GtkAdjustment *adj)
{
}

static void
max_changed(MarkControls *controls,
            GtkAdjustment *adj)
{
}

static void
update_changed_cb(GtkToggleButton *button, MarkControls *controls)
{
    controls->args->update = gtk_toggle_button_get_active(button);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->update);
    if (controls->args->update)
        update_view(controls, controls->args);
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
