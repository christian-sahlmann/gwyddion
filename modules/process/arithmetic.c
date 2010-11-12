/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2010 David Necas (Yeti).
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
#include <string.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyexpr.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define ARITH_RUN_MODES GWY_RUN_INTERACTIVE

enum {
    NARGS = 8,
    HISTSIZE = 25,
    RESPONSE_PREVIEW = 1,
    USER_UNITS_ID = G_MAXINT,
    PREVIEW_SIZE = 400,
};

enum {
    SENS_EXPR_OK = 1 << 0,
    SENS_USERUINTS = 1 << 1
};

enum {
    ARITHMETIC_VALUE,
    ARITHMETIC_MASK,
    ARITHMETIC_DER_X,
    ARITHMETIC_DER_Y,
    ARITHMETIC_NVARS,
};

enum {
    ARITHMETIC_NARGS = NARGS * ARITHMETIC_NVARS
};

enum {
    ARITHMETIC_OK      = 0,
    ARITHMETIC_DATA    = 1,
    ARITHMETIC_EXPR    = 2,
    ARITHMETIC_NUMERIC = 4
};

typedef GwyDataField* (*MakeFieldFunc)(GwyDataField *dfield);

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    GwyExpr *expr;
    gchar *expression;
    gint dataunits;
    gchar *userunits;
    GtkTreeModel *history;
    guint err;
    GwyDataObjectId objects[NARGS];
    gchar *name[ARITHMETIC_NARGS];
    guint pos[ARITHMETIC_NARGS];
    GPtrArray *ok_masks;
} ArithmeticArgs;

typedef struct {
    ArithmeticArgs *args;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *expression;
    GtkWidget *userunits;
    GtkWidget *userunits_label;
    GtkWidget *result;
    GtkWidget *data[NARGS];
    GSList *dataunits;
    GwyContainer *mydata;
} ArithmeticControls;

static gboolean      module_register              (void);
static void          arithmetic                   (GwyContainer *data,
                                                   GwyRunType run);
static void          arithmetic_load_args         (GwyContainer *settings,
                                                   ArithmeticArgs *args);
static void          arithmetic_save_args         (GwyContainer *settings,
                                                   ArithmeticArgs *args);
static gboolean      arithmetic_dialog            (GwyContainer *data,
                                                   gint id,
                                                   ArithmeticArgs *args);
static void          arithmetic_data_chosen       (GwyDataChooser *chooser,
                                                   ArithmeticControls *controls);
static void          arithmetic_expr_changed      (GtkWidget *entry,
                                                   ArithmeticControls *controls);
static void          arithmetic_userunits_changed (GtkEntry *entry,
                                                   ArithmeticControls *controls);
static void          arithmetic_dataunits_selected(ArithmeticControls *controls);
static void          arithmetic_show_state        (ArithmeticControls *controls,
                                                   const gchar *message);
static const gchar*  arithmetic_check_fields      (ArithmeticArgs *args);
static void          arithmetic_preview           (ArithmeticControls *controls);
static GwyDataField* arithmetic_do                (ArithmeticArgs *args);
static void          arithmetic_need_data         (const ArithmeticArgs *args,
                                                   gboolean *need_data);
static void          arithmetic_update_history    (ArithmeticArgs *args);
static void          arithmetic_fix_mask_field    (ArithmeticArgs *args,
                                                   GwyDataField *mfield);
static GwyDataField* make_x_der                   (GwyDataField *dfield);
static GwyDataField* make_y_der                   (GwyDataField *dfield);

static const gchar default_units[] = "";
static const gchar default_expression[] = "d1 - d2";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Simple arithmetic operations with data fields."),
    "Yeti <yeti@gwyddion.net>",
    "3.0",
    "David Nečas (Yeti)",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("arithmetic",
                              (GwyProcessFunc)&arithmetic,
                              N_("/M_ultidata/_Arithmetic..."),
                              GWY_STOCK_ARITHMETIC,
                              ARITH_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Arithmetic operations on data"));

    return TRUE;
}

void
arithmetic(GwyContainer *data, GwyRunType run)
{
    ArithmeticArgs args;
    guint i;
    GwyContainer *settings;
    gint id, newid;

    g_return_if_fail(run & ARITH_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id, 0);

    settings = gwy_app_settings_get();
    for (i = 0; i < NARGS; i++) {
        args.objects[i].data = data;
        args.objects[i].id = id;
    }
    arithmetic_load_args(settings, &args);
    args.ok_masks = g_ptr_array_new();
    args.expr = gwy_expr_new();

    gwy_expr_define_constant(args.expr, "pi", G_PI, NULL);
    gwy_expr_define_constant(args.expr, "π", G_PI, NULL);

    if (arithmetic_dialog(data, id, &args)) {
        GwyDataField *result = arithmetic_do(&args);

        newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
        g_object_unref(result);
        gwy_app_set_data_field_title(data, newid, _("Calculated"));
        gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
    }
    arithmetic_save_args(settings, &args);
    g_ptr_array_free(args.ok_masks, TRUE);
    gwy_expr_free(args.expr);
    for (i = 0; i < NARGS*ARITHMETIC_NVARS; i++)
        g_free(args.name[i]);
}

static gboolean
arithmetic_dialog(GwyContainer *data,
                  gint id,
                  ArithmeticArgs *args)
{
    GtkWidget *dialog, *hbox, *hbox2, *vbox, *table, *chooser, *entry,
              *label, *button;
    GtkTooltips *tooltips;
    ArithmeticControls controls;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    guint i, row, response;
    gchar *s;

    controls.args = args;
    tooltips = gwy_app_get_tooltips();

    dialog = gtk_dialog_new_with_buttons(_("Arithmetic"), NULL, 0, NULL);
    controls.dialog = dialog;
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 4);
    /* Ensure no wild changes of the dialog size due to non-square data. */
    gtk_widget_set_size_request(vbox, PREVIEW_SIZE, PREVIEW_SIZE);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    dfield = gwy_data_field_new(PREVIEW_SIZE, PREVIEW_SIZE, 1.0, 1.0, TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_GRADIENT, 0);
    controls.view = gwy_data_view_new(controls.mydata);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 NULL);
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);

    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 0);

    table = gtk_table_new(5 + NARGS, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new_with_mnemonic(_("_Expression:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    entry = gtk_combo_box_entry_new_with_model(args->history, 0);
    controls.expression = entry;
    gtk_combo_box_set_active(GTK_COMBO_BOX(controls.expression), 0);
    gtk_table_attach(GTK_TABLE(table), entry, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(entry, "changed",
                     G_CALLBACK(arithmetic_expr_changed), &controls);
    g_signal_connect_swapped(gtk_bin_get_child(GTK_BIN(entry)), "activate",
                             G_CALLBACK(arithmetic_preview), &controls);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    row++;

    controls.result = label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gtk_label_new(_("Operands"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Units"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.dataunits = NULL;
    for (i = 0; i < NARGS; i++) {
        /* VALUE is 0 */
        args->name[i] = g_strdup_printf("d_%d", i+1);
        label = gtk_label_new_with_mnemonic(args->name[i]);
        gwy_strkill(args->name[i], "_");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);

        args->name[NARGS*ARITHMETIC_MASK + i] = g_strdup_printf("m%d", i+1);
        args->name[NARGS*ARITHMETIC_DER_X + i] = g_strdup_printf("bx%d", i+1);
        args->name[NARGS*ARITHMETIC_DER_Y + i] = g_strdup_printf("by%d", i+1);

        chooser = gwy_data_chooser_new_channels();
        gwy_data_chooser_set_active(GWY_DATA_CHOOSER(chooser),
                                    args->objects[i].data, args->objects[i].id);
        g_signal_connect(chooser, "changed",
                         G_CALLBACK(arithmetic_data_chosen), &controls);
        g_object_set_data(G_OBJECT(chooser), "index", GUINT_TO_POINTER(i));
        gtk_table_attach(GTK_TABLE(table), chooser, 1, 2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), chooser);
        controls.data[i] = chooser;

        button = gtk_radio_button_new(controls.dataunits);
        controls.dataunits
            = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
        gwy_radio_button_set_value(button, i);
        s = g_strdup_printf(_("Take result units from data d%d"), i+1);
        gtk_tooltips_set_tip(tooltips, button, s, NULL);
        g_free(s);
        gtk_table_attach(GTK_TABLE(table), button, 2, 3, row, row+1,
                         0, 0, 0, 0);
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(arithmetic_dataunits_selected),
                                 &controls);

        row++;
    }

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_table_attach(GTK_TABLE(table), hbox2, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new_with_mnemonic(_("Specify un_its:"));
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
    controls.userunits_label = label;
    gtk_widget_set_sensitive(controls.userunits_label,
                             args->dataunits == USER_UNITS_ID);

    controls.userunits = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(controls.userunits), args->userunits);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.userunits, TRUE, TRUE, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.userunits);
    gtk_widget_set_sensitive(controls.userunits,
                             args->dataunits == USER_UNITS_ID);
    g_signal_connect(controls.userunits, "changed",
                     G_CALLBACK(arithmetic_userunits_changed), &controls);

    button = gtk_radio_button_new(controls.dataunits);
    controls.dataunits = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
    gwy_radio_button_set_value(button, USER_UNITS_ID);
    gtk_tooltips_set_tip(tooltips, button,
                         _("Specify result units explicitly"), NULL);
    gtk_table_attach(GTK_TABLE(table), button, 2, 3, row, row+1,
                     0, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(arithmetic_dataunits_selected),
                             &controls);
    row++;

    gtk_widget_grab_focus(controls.expression);
    gtk_widget_show_all(dialog);
    gwy_radio_buttons_set_current(controls.dataunits, args->dataunits);
    arithmetic_expr_changed(entry, &controls);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case RESPONSE_PREVIEW:
            arithmetic_preview(&controls);
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

static void
arithmetic_data_chosen(GwyDataChooser *chooser,
                       ArithmeticControls *controls)
{
    ArithmeticArgs *args;
    guint i;

    args = controls->args;
    i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(chooser), "index"));
    args->objects[i].data = gwy_data_chooser_get_active(chooser,
                                                        &args->objects[i].id);
    if (!(args->err & ARITHMETIC_EXPR))
        arithmetic_show_state(controls, NULL);
}

static void
arithmetic_expr_changed(GtkWidget *entry,
                        ArithmeticControls *controls)
{
    ArithmeticArgs *args;
    GtkComboBox *combo;
    GError *error = NULL;
    const gchar *message = NULL;
    gchar *s = NULL;

    combo = GTK_COMBO_BOX(entry);
    args = controls->args;
    g_free(args->expression);
    args->expression = g_strdup(gtk_combo_box_get_active_text(combo));
    args->err = ARITHMETIC_OK;

    if (gwy_expr_compile(args->expr, args->expression, &error)) {
        guint nvars = gwy_expr_get_variables(args->expr, NULL);
        g_return_if_fail(nvars);
        if (nvars == 1) {
            gdouble v = gwy_expr_execute(args->expr, NULL);
            message = s = g_strdup_printf("%g", v);
            args->err = ARITHMETIC_NUMERIC;
        }
        else {
            if (gwy_expr_resolve_variables(args->expr, ARITHMETIC_NARGS,
                                           (const gchar*const*)args->name,
                                           args->pos)) {
                args->err = ARITHMETIC_EXPR;
                message = _("Expression contains unknown identifiers");
            }
        }
    }
    else {
        args->err = ARITHMETIC_EXPR;
        message = error->message;
    }

    arithmetic_show_state(controls, message);
    g_clear_error(&error);
    g_free(s);
}

static void
arithmetic_userunits_changed(GtkEntry *entry,
                             ArithmeticControls *controls)
{
    g_free(controls->args->userunits);
    controls->args->userunits = g_strdup(gtk_entry_get_text(entry));
}

static void
arithmetic_dataunits_selected(ArithmeticControls *controls)
{
    ArithmeticArgs *args = controls->args;
    args->dataunits = gwy_radio_buttons_get_current(controls->dataunits);
    gtk_widget_set_sensitive(controls->userunits,
                             args->dataunits == USER_UNITS_ID);
    gtk_widget_set_sensitive(controls->userunits_label,
                             args->dataunits == USER_UNITS_ID);
}

static void
arithmetic_show_state(ArithmeticControls *controls,
                      const gchar *message)
{
    ArithmeticArgs *args = controls->args;
    GtkDialog *dialog = GTK_DIALOG(controls->dialog);
    gboolean ok;

    if (!message && args->err != ARITHMETIC_NUMERIC) {
        message = arithmetic_check_fields(args);
        if (args->err)
            gtk_label_set_text(GTK_LABEL(controls->result), message);
        else
            gtk_label_set_text(GTK_LABEL(controls->result), "");
    }
    else {
        if (message)
            gtk_label_set_text(GTK_LABEL(controls->result), message);
    }

    ok = (args->err == ARITHMETIC_OK || args->err == ARITHMETIC_NUMERIC);
    gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_OK, ok);
    gtk_dialog_set_response_sensitive(dialog, RESPONSE_PREVIEW, ok);

    if (!ok) {
        GdkColor gdkcolor = { 0, 51118, 0, 0 };
        gtk_widget_modify_fg(controls->result, GTK_STATE_NORMAL, &gdkcolor);
    }
    else
        gtk_widget_modify_fg(controls->result, GTK_STATE_NORMAL, NULL);
}

static const gchar*
arithmetic_check_fields(ArithmeticArgs *args)
{
    guint first = 0, i;
    GwyContainer *data;
    GQuark quark;
    GwyDataField *dfirst, *dfield;
    GwyDataCompatibilityFlags diff;
    gboolean need_data[NARGS];

    if (args->err & (ARITHMETIC_EXPR | ARITHMETIC_NUMERIC))
        return NULL;

    arithmetic_need_data(args, need_data);

    for (i = 0; i < NARGS; i++) {
        if (need_data[i]) {
            first = i;
            break;
        }
    }
    if (i == NARGS) {
        /* no variables */
        args->err &= ~ARITHMETIC_DATA;
        return NULL;
    }

    /* each window must match with first, this is transitive */
    data = args->objects[first].data;
    quark = gwy_app_get_data_key_for_id(args->objects[first].id);
    dfirst = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    for (i = first+1; i < NARGS; i++) {
        if (!need_data[i])
            continue;

        data = args->objects[i].data;
        quark = gwy_app_get_data_key_for_id(args->objects[i].id);
        dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

        diff = gwy_data_field_check_compatibility
                                            (dfirst, dfield,
                                             GWY_DATA_COMPATIBILITY_RES
                                             | GWY_DATA_COMPATIBILITY_REAL
                                             | GWY_DATA_COMPATIBILITY_LATERAL);
        if (diff) {
            args->err |= ARITHMETIC_DATA;
            if (diff & GWY_DATA_COMPATIBILITY_RES)
                return _("Pixel dimensions differ");
            if (diff & GWY_DATA_COMPATIBILITY_LATERAL)
                return _("Lateral dimensions are different physical "
                         "quantities");
            if (diff & GWY_DATA_COMPATIBILITY_REAL)
                return _("Physical dimensions differ");
        }
    }

    args->err &= ~ARITHMETIC_DATA;
    return NULL;
}

static void
arithmetic_preview(ArithmeticControls *controls)
{
    GwyDataField *result;

    /* We can also get here by activation of the entry so check again. */
    if (controls->args->err != ARITHMETIC_OK)
        return;

    result = arithmetic_do(controls->args);
    g_return_if_fail(result);

    gwy_container_set_object_by_name(controls->mydata, "/0/data", result);
    g_object_unref(result);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls->view), PREVIEW_SIZE);
}

static GwyDataField*
arithmetic_do(ArithmeticArgs *args)
{
    static MakeFieldFunc derivers[ARITHMETIC_NVARS] = {
        NULL, NULL, make_x_der, make_y_der,
    };
    GwyContainer *data;
    GwySIUnit *unit, *unit2;
    GQuark quark;
    GwyDataField **data_fields, *dfield, *mfield,
                 *void_mask = NULL, *result = NULL;
    const gdouble **d;
    gboolean need_data[NARGS];
    gdouble *r = NULL;
    gboolean first = TRUE;
    guint n = 0, i;

    g_return_val_if_fail(args->err == ARITHMETIC_OK, NULL);

    arithmetic_need_data(args, need_data);
    /* We know the expression can't contain more variables */
    data_fields = g_new0(GwyDataField*, ARITHMETIC_NARGS);
    d = g_new0(const gdouble*, ARITHMETIC_NARGS + 1);
    d[0] = NULL;

    /* First get all the data fields we directly have */
    for (i = 0; i < NARGS; i++) {
        gwy_debug("dfield[%u]: %s", i, need_data[i] ? "NEEDED" : "not needed");
        if (!need_data[i])
            continue;

        data = args->objects[i].data;
        quark = gwy_app_get_data_key_for_id(args->objects[i].id);
        dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
        data_fields[i] = dfield;
        d[args->pos[i]] = gwy_data_field_get_data_const(dfield);
        gwy_debug("d[%u] set to PRIMARY %u", args->pos[i], i);
        if (first) {
            first = FALSE;
            n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
            result = gwy_data_field_new_alike(dfield, FALSE);
            r = gwy_data_field_get_data(result);
        }
    }

    /* Then the mask fields */
    for (i = NARGS*ARITHMETIC_MASK; i < NARGS*(ARITHMETIC_MASK + 1); i++) {
        gwy_debug("mfield[%u]: %s", i, need_data[i] ? "NEEDED" : "not needed");
        if (!need_data[i % NARGS])
            continue;

        data = args->objects[i % NARGS].data;
        quark = gwy_app_get_data_key_for_id(args->objects[i % NARGS].id);
        dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
        if (first) {
            first = FALSE;
            n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
            result = gwy_data_field_new_alike(dfield, FALSE);
            r = gwy_data_field_get_data(result);
        }
        quark = gwy_app_get_mask_key_for_id(args->objects[i % NARGS].id);
        mfield = NULL;
        if (!gwy_container_gis_object(data, quark, &mfield)) {
            if (!void_mask)
                void_mask = gwy_data_field_new_alike(dfield, TRUE);
            mfield = void_mask;
        }
        else
            arithmetic_fix_mask_field(args, mfield);
        d[args->pos[i]] = gwy_data_field_get_data_const(mfield);
    }

    /* Derive derived fields */
    for (i = NARGS*ARITHMETIC_DER_X; i < NARGS*(ARITHMETIC_DER_Y + 1); i++) {
        if (!args->pos[i])
            continue;

        g_return_val_if_fail(data_fields[i % NARGS], NULL);
        dfield = derivers[i/NARGS](data_fields[i % NARGS]);
        gwy_debug("d[%u] set to DERIVED %u (type %u)",
                  args->pos[i], i/NARGS, i%NARGS);
        data_fields[i] = dfield;
        d[args->pos[i]] = gwy_data_field_get_data_const(dfield);
    }

    /* Execute */
    gwy_expr_vector_execute(args->expr, n, d, r);

    /* Set units. */
    unit = gwy_data_field_get_si_unit_z(result);
    if (args->dataunits == USER_UNITS_ID)
        gwy_si_unit_set_from_string(unit, args->userunits);
    else {
        i = args->dataunits % NARGS;
        if (!(dfield = data_fields[i])) {
            data = args->objects[i].data;
            quark = gwy_app_get_data_key_for_id(args->objects[i].id);
            dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
        }
        unit2 = gwy_data_field_get_si_unit_z(dfield);
        gwy_serializable_clone(G_OBJECT(unit2), G_OBJECT(unit));
    }

    /* Free stuff */
    gwy_object_unref(void_mask);
    for (i = NARGS; i < ARITHMETIC_NVARS; i++) {
        if (data_fields[i])
            g_object_unref(data_fields[i]);
    }
    g_free(data_fields);
    g_free(d);

    return result;
}

/* Find which data we need, for derivatives or otherwise */
static void
arithmetic_need_data(const ArithmeticArgs *args,
                     gboolean *need_data)
{
    guint i;

    gwy_clear(need_data, NARGS);
    for (i = 0; i < NARGS*ARITHMETIC_NVARS; i++) {
        if (args->pos[i])
            need_data[i % NARGS] = TRUE;
    }
}

static void
arithmetic_fix_mask_field(ArithmeticArgs *args,
                          GwyDataField *mfield)
{
    gdouble min, max;
    guint i;

    /* Do not process masks we have already processed. */
    for (i = 0; i < args->ok_masks->len; i++) {
        if (g_ptr_array_index(args->ok_masks, i) == (gpointer)mfield)
            return;
    }

    /* Silently normalise the mask values if they are outside [0, 1].
     * We do not actually like any value different from 0 and 1, but let them
     * pass. */
    gwy_data_field_get_min_max(mfield, &min, &max);
    if (min < 0.0 || max > 1.0)
        gwy_data_field_clamp(mfield, 0.0, 1.0);

    g_ptr_array_add(args->ok_masks, mfield);
}

static GwyDataField*
make_x_der(GwyDataField *dfield)
{
    GwyDataField *result;
    const gdouble *d, *drow;
    gdouble *r, *rrow;
    guint xres, yres, i, j;
    gdouble h;

    result = gwy_data_field_new_alike(dfield, FALSE);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    h = 2.0*gwy_data_field_get_xreal(dfield)/xres;
    d = gwy_data_field_get_data_const(dfield);
    r = gwy_data_field_get_data(result);

    if (xres < 2) {
        gwy_data_field_clear(result);
        return result;
    }

    for (i = 0; i < yres; i++) {
        drow = d + i*xres;
        rrow = r + i*xres;
        rrow[0] = 2.0*(drow[1] - drow[0])/h;
        for (j = 1; j < xres-1; j++)
            rrow[j] = (drow[j+1] - drow[j-1])/h;
        rrow[xres-1] = 2.0*(drow[xres-1] - drow[xres-2])/h;
    }

    return result;
}

static GwyDataField*
make_y_der(GwyDataField *dfield)
{
    GwyDataField *result;
    const gdouble *d, *drow, *drowp, *drowm;
    gdouble *r, *rrow;
    guint xres, yres, i, j;
    gdouble h;

    result = gwy_data_field_new_alike(dfield, FALSE);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    h = 2.0*gwy_data_field_get_yreal(dfield)/yres;
    d = gwy_data_field_get_data_const(dfield);
    r = gwy_data_field_get_data(result);

    if (yres < 2) {
        gwy_data_field_clear(result);
        return result;
    }

    rrow = r;
    drow = d;
    drowp = d + xres;
    for (j = 0; j < xres; j++)
        rrow[j] = 2.0*(drowp[j] - drow[j])/h;

    for (i = 1; i < yres-1; i++) {
        drowm = drow;
        drow = drowp;
        drowp += xres;
        rrow += xres;
        for (j = 0; j < xres; j++)
            rrow[j] = (drowp[j] - drowm[j])/h;
    }

    drowm = drow;
    drow = drowp;
    rrow += xres;
    for (j = 0; j < xres; j++)
        rrow[j] = 2.0*(drow[j] - drowm[j])/h;

    return result;
}

static void
arithmetic_update_history(ArithmeticArgs *args)
{
    GtkListStore *store;
    GtkTreeIter iter;
    gchar *s;

    if (!*args->expression)
        return;

    store = GTK_LIST_STORE(args->history);
    gtk_list_store_prepend(store, &iter);
    gtk_list_store_set(store, &iter, 0, args->expression, -1);

    while (gtk_tree_model_iter_next(args->history, &iter)) {
        gtk_tree_model_get(args->history, &iter, 0, &s, -1);
        if (gwy_strequal(s, args->expression)) {
            gtk_list_store_remove(store, &iter);
            break;
        }
    }
}

static const gchar expression_key[] = "/module/arithmetic/expression";
static const gchar dataunits_key[]  = "/module/arithmetic/dataunits";
static const gchar userunits_key[]  = "/module/arithmetic/userunits";

static void
arithmetic_load_args(GwyContainer *settings,
                     ArithmeticArgs *args)
{
    GtkListStore *store;
    const guchar *str;
    gchar *filename, *buffer, *line, *p;
    gsize size;

    str = default_expression;
    gwy_container_gis_string_by_name(settings, expression_key, &str);
    args->expression = g_strdup(str);

    str = default_units;
    gwy_container_gis_string_by_name(settings, userunits_key, &str);
    args->userunits = g_strdup(str);

    args->dataunits = 0;
    gwy_container_gis_int32_by_name(settings, dataunits_key, &args->dataunits);

    store = gtk_list_store_new(1, G_TYPE_STRING);
    args->history = GTK_TREE_MODEL(store);

    filename = g_build_filename(gwy_get_user_dir(), "arithmetic", "history",
                                NULL);
    if (g_file_get_contents(filename, &buffer, &size, NULL)) {
        p = buffer;
        for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
            GtkTreeIter iter;

            g_strstrip(line);
            if (*line) {
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 0, line, -1);
            }
        }
        g_free(buffer);
    }
    g_free(filename);

    /* Ensures args->expression comes first */
    arithmetic_update_history(args);
}

static void
arithmetic_save_args(GwyContainer *settings,
                     ArithmeticArgs *args)
{
    gchar *filename;
    FILE *fh;

    gwy_container_set_string_by_name(settings, expression_key,
                                     args->expression);
    gwy_container_set_string_by_name(settings, userunits_key,
                                     args->userunits);
    gwy_container_set_int32_by_name(settings, dataunits_key,
                                     args->dataunits);

    filename = g_build_filename(gwy_get_user_dir(), "arithmetic", NULL);
    if (!g_file_test(filename, G_FILE_TEST_IS_DIR))
        g_mkdir(filename, 0700);
    g_free(filename);

    filename = g_build_filename(gwy_get_user_dir(), "arithmetic", "history",
                                NULL);
    if ((fh = g_fopen(filename, "w"))) {
        GtkTreeIter iter;
        guint i = 0;
        gchar *s;

        if (gtk_tree_model_get_iter_first(args->history, &iter)) {
            do {
                gtk_tree_model_get(args->history, &iter, 0, &s, -1);
                fputs(s, fh);
                fputc('\n', fh);
                g_free(s);
            } while (++i < HISTSIZE
                     && gtk_tree_model_iter_next(args->history, &iter));
        }
        fclose(fh);
    }
    g_free(filename);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
