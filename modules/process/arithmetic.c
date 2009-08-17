/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2009 David Necas (Yeti).
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyexpr.h>
#include <libprocess/datafield.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define ARITH_RUN_MODES GWY_RUN_INTERACTIVE

enum {
    NARGS = 5
};

enum {
    ARITHMETIC_VALUE,
    ARITHMETIC_DER_X,
    ARITHMETIC_DER_Y,
    ARITHMETIC_NVARS,
};

enum {
    ARITHMETIC_OK   = 0,
    ARITHMETIC_DATA = 1,
    ARITHMETIC_EXPR = 2
};

typedef GwyDataField* (*MakeFieldFunc)(GwyDataField *dfield);

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    GwyExpr *expr;
    gchar *expression;
    guint err;
    GwyDataObjectId objects[NARGS];
    gchar *name[NARGS*ARITHMETIC_NVARS];
    guint pos[NARGS*ARITHMETIC_NVARS];
} ArithmeticArgs;

typedef struct {
    ArithmeticArgs *args;
    GtkWidget *dialog;
    GtkWidget *expression;
    GtkWidget *result;
    GtkWidget *data[NARGS];
} ArithmeticControls;

static gboolean      module_register         (void);
static void          arithmetic              (GwyContainer *data,
                                              GwyRunType run);
static void          arithmetic_load_args    (GwyContainer *settings,
                                              ArithmeticArgs *args);
static void          arithmetic_save_args    (GwyContainer *settings,
                                              ArithmeticArgs *args);
static gboolean      arithmetic_dialog       (ArithmeticArgs *args);
static void          arithmetic_data_cb      (GwyDataChooser *chooser,
                                              ArithmeticControls *controls);
static void          arithmetic_expr_cb      (GtkWidget *entry,
                                              ArithmeticControls *controls);
static void          arithmetic_maybe_preview(ArithmeticControls *controls);
static const gchar*  arithmetic_check        (ArithmeticArgs *args);
static void          arithmetic_do           (ArithmeticArgs *args);
static void          arithmetic_need_data    (const ArithmeticArgs *args,
                                              gboolean *need_data);
static GwyDataField* make_x_der              (GwyDataField *dfield);
static GwyDataField* make_y_der              (GwyDataField *dfield);

static const gchar default_expression[] = "d1 - d2";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Simple arithmetic operations with two data fields "
       "(or a data field and a scalar)."),
    "Yeti <yeti@gwyddion.net>",
    "2.4",
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
    gint id;

    g_return_if_fail(run & ARITH_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id, 0);

    settings = gwy_app_settings_get();
    for (i = 0; i < NARGS; i++) {
        args.objects[i].data = data;
        args.objects[i].id = id;
    }
    arithmetic_load_args(settings, &args);
    args.expr = gwy_expr_new();

    gwy_expr_define_constant(args.expr, "pi", G_PI, NULL);
    gwy_expr_define_constant(args.expr, "π", G_PI, NULL);

    if (arithmetic_dialog(&args)) {
        arithmetic_do(&args);
    }
    arithmetic_save_args(settings, &args);
    gwy_expr_free(args.expr);
    for (i = 0; i < NARGS*ARITHMETIC_NVARS; i++)
        g_free(args.name[i]);
}

static gboolean
arithmetic_dialog(ArithmeticArgs *args)
{
    ArithmeticControls controls;
    GtkWidget *dialog, *table, *chooser, *entry, *label;
    guint i, row, response;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Arithmetic"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4 + NARGS, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(_("Operands:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    for (i = 0; i < NARGS; i++) {
        /* VALUE is 0 */
        args->name[i] = g_strdup_printf("d_%d", i+1);
        label = gtk_label_new_with_mnemonic(args->name[i]);
        gwy_strkill(args->name[i], "_");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);

        args->name[NARGS*ARITHMETIC_DER_X + i] = g_strdup_printf("bx%d", i+1);
        args->name[NARGS*ARITHMETIC_DER_Y + i] = g_strdup_printf("by%d", i+1);

        chooser = gwy_data_chooser_new_channels();
        gwy_data_chooser_set_active(GWY_DATA_CHOOSER(chooser),
                                    args->objects[i].data, args->objects[i].id);
        g_signal_connect(chooser, "changed",
                         G_CALLBACK(arithmetic_data_cb), &controls);
        g_object_set_data(G_OBJECT(chooser), "index", GUINT_TO_POINTER(i));
        gtk_table_attach(GTK_TABLE(table), chooser, 1, 2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), chooser);
        controls.data[i] = chooser;

        row++;
    }
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gtk_label_new_with_mnemonic(_("_Expression:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.expression = entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), args->expression);
    gtk_table_attach(GTK_TABLE(table), entry, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(entry, "changed",
                     G_CALLBACK(arithmetic_expr_cb), &controls);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    row++;

    controls.result = label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    gtk_widget_grab_focus(controls.expression);
    gtk_widget_show_all(dialog);
    arithmetic_expr_cb(entry, &controls);
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

static void
arithmetic_data_cb(GwyDataChooser *chooser,
                   ArithmeticControls *controls)
{
    ArithmeticArgs *args;
    guint i;

    args = controls->args;
    i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(chooser), "index"));
    args->objects[i].data = gwy_data_chooser_get_active(chooser,
                                                        &args->objects[i].id);
    if (!(args->err & ARITHMETIC_EXPR))
        arithmetic_maybe_preview(controls);
}

static void
arithmetic_expr_cb(GtkWidget *entry,
                   ArithmeticControls *controls)
{
    ArithmeticArgs *args;
    guint nvars;
    gdouble v;
    gchar *s;

    args = controls->args;
    g_free(args->expression);
    args->expression = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
    args->err = ARITHMETIC_OK;

    if (gwy_expr_compile(args->expr, args->expression, NULL)) {
        nvars = gwy_expr_get_variables(args->expr, NULL);
        g_return_if_fail(nvars);
        if (nvars == 1) {
            v = gwy_expr_execute(args->expr, NULL);
            s = g_strdup_printf("%g", v);
            gtk_label_set_text(GTK_LABEL(controls->result), s);
            g_free(s);
            gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                              GTK_RESPONSE_OK, FALSE);
        }
        else {
            if (!gwy_expr_resolve_variables(args->expr, NARGS*ARITHMETIC_NVARS,
                                            (const gchar*const*)args->name,
                                            args->pos)) {
                arithmetic_maybe_preview(controls);
            }
            else {
                args->err = ARITHMETIC_EXPR;
                gtk_label_set_text(GTK_LABEL(controls->result), "");
                gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                                  GTK_RESPONSE_OK, FALSE);
            }
        }
    }
    else {
        args->err = ARITHMETIC_EXPR;
        gtk_label_set_text(GTK_LABEL(controls->result), "");
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                          GTK_RESPONSE_OK, FALSE);
    }
}

static void
arithmetic_maybe_preview(ArithmeticControls *controls)
{
    ArithmeticArgs *args;
    const gchar *message;

    args = controls->args;
    message = arithmetic_check(args);
    if (args->err) {
        gtk_label_set_text(GTK_LABEL(controls->result), message);
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                          GTK_RESPONSE_OK, FALSE);
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls->result), "");
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                          GTK_RESPONSE_OK, TRUE);
        /* TODO: preview */
    }
}

static const gchar*
arithmetic_check(ArithmeticArgs *args)
{
    guint first = 0, i;
    GwyContainer *data;
    GQuark quark;
    GwyDataField *dfirst, *dfield;
    GwyDataCompatibilityFlags diff;
    gboolean need_data[NARGS];

    if (args->err & ARITHMETIC_EXPR)
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

        /* FIXME: what about value units? */
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
arithmetic_do(ArithmeticArgs *args)
{
    static MakeFieldFunc derivers[ARITHMETIC_NVARS] = {
        NULL, make_x_der, make_y_der,
    };
    GwyContainer *data, *firstdata = NULL;
    GQuark quark;
    GwyDataField **data_fields, *dfield, *result = NULL;
    const gdouble **d;
    gboolean need_data[NARGS];
    gdouble *r = NULL;
    gboolean first = TRUE;
    guint n = 0, i;
    gint firstid = -1, newid;

    g_return_if_fail(!args->err);

    arithmetic_need_data(args, need_data);
    /* We know the expression can't contain more variables */
    data_fields = g_new0(GwyDataField*, NARGS*ARITHMETIC_NVARS);
    d = g_new0(const gdouble*, NARGS*ARITHMETIC_NVARS + 1);
    d[0] = NULL;

    /* First get all the fields we directly have */
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
            firstdata = data;
            firstid = args->objects[i].id;
        }
    }
    g_return_if_fail(firstdata);

    /* Derive derived fields */
    for (i = NARGS; i < NARGS*ARITHMETIC_NVARS; i++) {
        if (!args->pos[i])
            continue;

        g_return_if_fail(data_fields[i % NARGS]);
        dfield = derivers[i/NARGS](data_fields[i % NARGS]);
        gwy_debug("d[%u] set to DERIVED %u (type %u)",
                  args->pos[i], i/NARGS, i%NARGS);
        data_fields[i] = dfield;
        d[args->pos[i]] = gwy_data_field_get_data_const(dfield);
    }

    /* Execute */
    gwy_expr_vector_execute(args->expr, n, d, r);

    /* Free stuff */
    for (i = NARGS; i < ARITHMETIC_NVARS; i++) {
        if (data_fields[i])
            g_object_unref(data_fields[i]);
    }
    g_free(data_fields);
    g_free(d);

    /* Create the new data field */
    newid = gwy_app_data_browser_add_data_field(result, firstdata, TRUE);
    g_object_unref(result);
    gwy_app_set_data_field_title(firstdata, newid, _("Calculated"));
    gwy_app_sync_data_items(firstdata, firstdata, firstid, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT, 0);
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

static const gchar expression_key[] = "/module/arithmetic/expression";

static void
arithmetic_load_args(GwyContainer *settings,
                     ArithmeticArgs *args)
{
    const guchar *exprstr;

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
