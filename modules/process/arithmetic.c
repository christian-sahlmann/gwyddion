/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyexpr.h>
#include <libprocess/datafield.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#define ARITH_RUN_MODES \
    (GWY_RUN_MODAL)

enum {
    WIN_ARGS = 3
};

typedef struct {
    GwyExpr *expr;
    gchar *expression;
    GwyDataWindow *win[WIN_ARGS];
    gchar *name[WIN_ARGS];
} ArithmeticArgs;

typedef struct {
    GtkWidget *win[WIN_ARGS];
    GtkWidget *expression;
    GtkWidget *result;
    ArithmeticArgs *args;
} ArithmeticControls;

static gboolean   module_register             (const gchar *name);
static gboolean   arithmetic                  (GwyContainer *data,
                                               GwyRunType run);
static void       arithmetic_load_args        (GwyContainer *settings,
                                               ArithmeticArgs *args);
static void       arithmetic_save_args        (GwyContainer *settings,
                                               ArithmeticArgs *args);
static gboolean   arithmetic_dialog           (ArithmeticArgs *args);
static void       arithmetic_data_cb          (GtkWidget *item,
                                               ArithmeticControls *controls);
static void       arithmetic_expr_cb          (GtkWidget *entry,
                                               ArithmeticControls *controls);
static gboolean   arithmetic_check            (ArithmeticArgs *args);
static void       arithmetic_do               (ArithmeticArgs *args);


static const gchar default_expression[] = "data1 - data2";

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "arithmetic",
    N_("Simple arithmetic operations with two data fields "
       "(or a data field and a scalar)."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo arithmetic_func_info = {
        "arithmetic",
        N_("/M_ultidata/_Arithmetic..."),
        (GwyProcessFunc)&arithmetic,
        ARITH_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &arithmetic_func_info);

    return TRUE;
}

/* FIXME: we ignore the Container argument and use current data window */
gboolean
arithmetic(GwyContainer *data, GwyRunType run)
{
    ArithmeticArgs args;
    guint i;
    GwyContainer *settings;

    g_return_val_if_fail(run & ARITH_RUN_MODES, FALSE);
    settings = gwy_app_settings_get();
    for (i = 0; i < WIN_ARGS; i++)
        args.win[i] = gwy_app_data_window_get_current();
    arithmetic_load_args(settings, &args);
    args.expr = gwy_expr_new();

    g_assert(gwy_data_window_get_data(args.win[0]) == data);
    if (arithmetic_dialog(&args)) {
        arithmetic_do(&args);
    }
    arithmetic_save_args(settings, &args);
    gwy_expr_free(args.expr);

    return FALSE;
}

static gboolean
arithmetic_dialog(ArithmeticArgs *args)
{
    ArithmeticControls controls;
    gboolean ok = FALSE;
    GtkWidget *dialog, *table, *omenu, *entry, *label, *menu;
    guint i, row;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Arithmetic"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4 + WIN_ARGS, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(_("Operands:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    for (i = 0; i < WIN_ARGS; i++) {
        args->name[i] = g_strdup_printf("data_%d", i+1);
        label = gtk_label_new_with_mnemonic(args->name[i]);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);

        omenu = gwy_option_menu_data_window(G_CALLBACK(arithmetic_data_cb),
                                            &controls, NULL,
                                            GTK_WIDGET(args->win[i]));
        menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
        g_object_set_data(G_OBJECT(menu), "index", GUINT_TO_POINTER(i));
        gtk_table_attach(GTK_TABLE(table), omenu, 1, 2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
        controls.win[i] = omenu;

        row++;
    }
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gtk_label_new_with_mnemonic(_("_Expression:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.expression = entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), args->expression);
    gtk_table_attach(GTK_TABLE(table), entry, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(entry, "changed",
                     G_CALLBACK(arithmetic_expr_cb), &controls);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    row++;

    controls.result = label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    gtk_widget_show_all(dialog);
    arithmetic_expr_cb(entry, &controls);
    do {
        switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            ok = arithmetic_check(args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
arithmetic_data_cb(GtkWidget *item,
                   ArithmeticControls *controls)
{
    GtkWidget *menu;
    guint i;

    menu = gtk_widget_get_parent(item);
    i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(menu), "index"));
    controls->args->win[i] = g_object_get_data(G_OBJECT(item), "data-window");
}

static void
arithmetic_expr_cb(GtkWidget *entry,
                   ArithmeticControls *controls)
{
    ArithmeticArgs *args;
    gdouble v;
    gchar *s;

    args = controls->args;
    g_free(args->expression);
    args->expression = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

    if (gwy_expr_compile(args->expr, args->expression, NULL)) {
        if (!gwy_expr_get_variables(args->expr, NULL)) {
            v = gwy_expr_execute(args->expr, NULL);
            s = g_strdup_printf("%g", v);
            gtk_label_set_text(GTK_LABEL(controls->result), s);
            g_free(s);
        }
        else {
            gtk_label_set_text(GTK_LABEL(controls->result),
                               _("Window arguments"));
        }
    }
    else
        gtk_label_set_text(GTK_LABEL(controls->result), "");
}

static gboolean
arithmetic_check(ArithmeticArgs *args)
{
#if 0
    GtkWidget *dialog;
    GwyDataWindow *operand1, *operand2;
    GwyContainer *data;
    GwyDataField *dfield1, *dfield2;
    gdouble scalar1, scalar2;
    gdouble xreal1, xreal2, yreal1, yreal2;

    operand1 = args->win1;
    operand2 = args->win2;
    scalar1 = args->scalar1;
    scalar2 = args->scalar2;

    /***** scalar x scalar (silly, handled completely here) *****/
    if (!operand1 && !operand2) {
        gdouble value = 0.0;

        switch (args->operation) {
            case GWY_ARITH_ADD:
            value = scalar1 + scalar2;
            break;

            case GWY_ARITH_SUBSTRACT:
            value = scalar1 - scalar2;
            break;

            case GWY_ARITH_MULTIPLY:
            value = scalar1 * scalar2;
            break;

            case GWY_ARITH_DIVIDE:
            value = scalar1/scalar2;
            break;

            case GWY_ARITH_MAXIMUM:
            value = MAX(scalar1, scalar2);
            break;

            case GWY_ARITH_MINIMUM:
            value = MIN(scalar1, scalar2);
            break;

            default:
            g_assert_not_reached();
            break;
        }
        dialog = gtk_message_dialog_new(GTK_WINDOW(arithmetic_window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_INFO,
                                        GTK_BUTTONS_CLOSE,
                                        _("The result is %g, but no data "
                                          "to operate on were selected.\n"),
                                        value);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return FALSE;
    }

    /***** datafield x datafield, must check *****/
    if (operand1 && operand2) {
        data = gwy_data_window_get_data(operand2);
        dfield2 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                  "/0/data"));
        data = gwy_data_window_get_data(operand1);
        dfield1 = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                  "/0/data"));

        if ((gwy_data_field_get_xres(dfield1)
             != gwy_data_field_get_xres(dfield2))
            || (gwy_data_field_get_yres(dfield1)
                != gwy_data_field_get_yres(dfield2))) {
            dialog = gtk_message_dialog_new(GTK_WINDOW(arithmetic_window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_INFO,
                                            GTK_BUTTONS_CLOSE,
                                            _("Data dimensions differ.\n"));
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            return FALSE;
        }
        xreal1 = gwy_data_field_get_xreal(dfield1);
        yreal1 = gwy_data_field_get_yreal(dfield1);
        xreal2 = gwy_data_field_get_xreal(dfield2);
        yreal2 = gwy_data_field_get_yreal(dfield2);
        if (fabs(log(xreal1/xreal2)) > 0.0001
            || fabs(log(yreal1/yreal2)) > 0.0001) {
            dialog = gtk_message_dialog_new(GTK_WINDOW(arithmetic_window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_INFO,
                                            GTK_BUTTONS_CLOSE,
                                            _("Physical dimensions differ.\n"));
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            return FALSE;
        }
    }

    /* everything else is possible */
    return TRUE;
#endif
    return FALSE;
}

static void
arithmetic_do(ArithmeticArgs *args)
{
}

static const gchar *expression_key = "/module/arithmetic/expression";

static void
arithmetic_load_args(GwyContainer *settings,
                     ArithmeticArgs *args)
{
    const guchar *exprstr;

    /* TODO: remove this someday */
    gwy_container_remove_by_prefix(settings, "/app/arith");
    gwy_container_remove_by_name(settings, "/module/arithmetic/scalar1");
    gwy_container_remove_by_name(settings, "/module/arithmetic/scalar2");
    gwy_container_remove_by_name(settings, "/module/arithmetic/scalar_is1");
    gwy_container_remove_by_name(settings, "/module/arithmetic/scalar_is2");

    exprstr = default_expression;
    gwy_container_gis_string_by_name(settings, expression_key, &exprstr);
    args->expression = g_strdup(exprstr);
}

static void
arithmetic_save_args(GwyContainer *settings,
                     ArithmeticArgs *args)
{
    gwy_container_set_string_by_name(settings, expression_key,
                                     args->expression);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

