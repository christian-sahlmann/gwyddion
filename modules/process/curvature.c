/*
 *  @(#) $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.net.
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
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/level.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwynullstore.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define CURVATURE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 320,
};

typedef enum {
    PARAM_X0,
    PARAM_Y0,
    PARAM_A,
    PARAM_R1,
    PARAM_R2,
    PARAM_PHI1,
    PARAM_PHI2,
    PARAM_NPARAMS
} CurvatureParamType;

typedef struct {
    gdouble d, t, x, y;
} Intersection;

typedef struct {
    gboolean set_selection;
    gboolean plot_graph;
    GwyMaskingType masking;
} CurvatureArgs;

typedef struct {
    CurvatureArgs *args;
    double params[PARAM_NPARAMS];
    GwySIUnit *unit;
    GtkWidget *dialog;
    GSList *masking_group;
    GtkWidget *set_selection;
    GtkWidget *plot_graph;
    GtkWidget *view;
    GtkWidget *graph;
    GtkWidget *warning;
    GwyNullStore *paramstore;
    GwyGraphModel *gmodel;
    GwySelection *selection;
    GwyContainer *data;
} CurvatureControls;

static gboolean   module_register                (void);
static void       curvature                      (GwyContainer *data,
                                                  GwyRunType run);
static void       curvature_do                   (GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  GwyDataField *mfield,
                                                  gint oldid,
                                                  const CurvatureArgs *args);
static gboolean   curvature_dialog               (CurvatureArgs *args,
                                                  GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  GwyDataField *mfield,
                                                  gint id);
static GtkWidget* curvature_add_aux_button       (GtkWidget *hbox,
                                                  const gchar *stock_id,
                                                  const gchar *tooltip);
static void       curvature_set_selection_changed(GtkToggleButton *button,
                                                  CurvatureControls *controls);
static void       curvature_plot_graph_changed   (GtkToggleButton *button,
                                                  CurvatureControls *controls);
static void       curvature_dialog_update        (CurvatureControls *controls,
                                                  CurvatureArgs *args);
static void       curvature_masking_changed      (GtkToggleButton *button,
                                                  CurvatureControls *controls);
static void       curvature_update_preview       (CurvatureControls *controls,
                                                  CurvatureArgs *args);
static void       curvature_save                 (CurvatureControls *controls);
static void       curvature_copy                 (CurvatureControls *controls);
static void       load_args                      (GwyContainer *container,
                                                  CurvatureArgs *args);
static void       save_args                      (GwyContainer *container,
                                                  CurvatureArgs *args);
static void       sanitize_args                  (CurvatureArgs *args);

static const CurvatureArgs curvature_defaults = {
    TRUE,
    FALSE,
    GWY_MASK_IGNORE,
};

static const gchar *param_names[] = {
    N_("Center x position"),
    N_("Center y position"),
    N_("Center value"),
    N_("Curvature radius 1"),
    N_("Curvature radius 2"),
    N_("Direction 1"),
    N_("Direction 2"),
};

static const gchar *param_symbols[] = {
    "x<sub>0</sub>",
    "y<sub>0</sub>",
    "z<sub>0</sub>",
    "r<sub>1</sub>",
    "r<sub>2</sub>",
    "φ<sub>1</sub>",
    "φ<sub>2</sub>",
};

static const gchar *param_symbols_plain[] = {
    "x0",
    "y0",
    "z0",
    "r1",
    "r2",
    "φ1",
    "φ2",
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates overall curvature."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("curvature",
                              (GwyProcessFunc)&curvature,
                              N_("/_Level/_Curvature..."),
                              NULL,
                              CURVATURE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Calculate overall curvature"));

    return TRUE;
}

static void
curvature(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield;
    CurvatureArgs args;
    gboolean ok;
    gint id;

    g_return_if_fail(run & CURVATURE_RUN_MODES);
    g_return_if_fail(g_type_from_name("GwyLayerLine"));
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    if (!gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(dfield),
                           gwy_data_field_get_si_unit_z(dfield))) {
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new
                        (gwy_app_find_window_for_channel(data, id),
                         GTK_DIALOG_DESTROY_WITH_PARENT,
                         GTK_MESSAGE_ERROR,
                         GTK_BUTTONS_OK,
                         _("Curvature: Lateral dimensions and value must "
                           "be the same physical quantity."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = curvature_dialog(&args, data, dfield, mfield, id);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }
    curvature_do(data, dfield, mfield, id, &args);
}

static int
compare_double(const void *a, const void *b)
{
    const gdouble *da = (const gdouble*)a;
    const gdouble *db = (const gdouble*)b;

    if (*da < *db)
        return -1;
    if (*da > *db)
        return 1;
    return 0;
}

static gboolean
intersect_with_boundary(gdouble x_0, gdouble y_0,
                        gdouble phi,
                        gdouble w, gdouble h,
                        Intersection *i1, Intersection *i2)
{
    enum { NISEC = 4 };
    Intersection isec[NISEC];
    gdouble diag;
    guint i;

    /* With x = 0 */
    isec[0].t = -x_0/cos(phi);
    isec[0].x = 0.0;
    isec[0].y = y_0 - x_0*tan(phi);

    /* With x = w */
    isec[1].t = (w - x_0)/cos(phi);
    isec[1].x = w;
    isec[1].y = y_0 + (w - x_0)*tan(phi);

    /* With y = 0 */
    isec[2].t = -y_0/sin(phi);
    isec[2].x = x_0 - y_0/tan(phi);
    isec[2].y = 0.0;

    /* With y = h */
    isec[3].t = (h - y_0)/sin(phi);
    isec[3].x = x_0 + (h - y_0)/tan(phi);
    isec[3].y = h;

    /* Distance from centre must be at most half the diagonal. */
    diag = 0.5*hypot(w, h);
    for (i = 0; i < NISEC; i++) {
        isec[i].d = hypot(isec[i].x - 0.5*w, isec[i].y - 0.5*h)/diag;
        gwy_debug("isec[%u]: %g", i, isec[i].d);
    }

    qsort(isec, NISEC, sizeof(Intersection), compare_double);

    for (i = 0; i < NISEC; i++) {
        if (isec[i].d > 1.0)
            break;
    }

    gwy_debug("intersections: %u", i);
    switch (i) {
        case 0:
        case 2:
        break;

        case 1:
        i = 0;
        break;

        case 3:
        i = 2;
        break;

        case 4:
        i = 2;
        /* Pick the right two intersections if it goes through two opposite
         * corners. */
        if (fabs(isec[0].t - isec[1].t) < fabs(isec[0].t - isec[2].t))
            isec[1] = isec[2];
        break;

        default:
        g_assert_not_reached();
        break;
    }

    if (i) {
        if (isec[0].t <= isec[1].t) {
            *i1 = isec[0];
            *i2 = isec[1];
        }
        else {
            *i1 = isec[1];
            *i2 = isec[0];
        }
        return TRUE;
    }
    return FALSE;
}

/* Does not include x and y offsets of the data field */
static gboolean
curvature_calculate(GwyDataField *dfield,
                    GwyDataField *mask,
                    const CurvatureArgs *args,
                    double *params,
                    Intersection *i1,
                    Intersection *i2)
{
    enum { DEGREE = 2 };
    enum { A, BX, CXX, BY, CXY, CYY, NTERMS };
    gint term_powers[2*NTERMS];
    gdouble coeffs[NTERMS];
    gdouble xreal, yreal, qx, qy;
    gdouble a, a1, bx, by, cxx, cxy, cyy, cx, cy;
    gdouble x_0, y_0, phi;
    gint xres, yres, i, j, k;
    gboolean ok;

    k = 0;
    g_assert(NTERMS == (DEGREE + 1)*(DEGREE + 2)/2);
    for (i = 0; i <= DEGREE; i++) {
        for (j = 0; j <= DEGREE - i; j++) {
            term_powers[k++] = j;
            term_powers[k++] = i;
        }
    }

    gwy_data_field_fit_poly(dfield, mask, NTERMS, term_powers,
                            args->masking != GWY_MASK_INCLUDE, coeffs);
    gwy_debug("NORM a=%g, bx=%g, by=%g, cxx=%g, cxy=%g, cyy=%g",
              coeffs[A], coeffs[BX], coeffs[BY],
              coeffs[CXX], coeffs[CXY], coeffs[CYY]);

    /* Transform coeffs from normalized coordinates to real coordinates */
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    qx = 2.0/xreal*xres/(xres - 1.0);
    qy = 2.0/yreal*yres/(yres - 1.0);

    a1 = coeffs[A];
    bx = qx*coeffs[BX];
    by = qy*coeffs[BY];
    cxx = qx*qx*coeffs[CXX];
    cxy = qx*qy*coeffs[CXY];
    cyy = qy*qy*coeffs[CYY];

    /* Eliminate the mixed term */
    if (fabs(cxx) + fabs(cxy) + fabs(cyy)
        <= 1e-14*(fabs(bx)/xreal + fabs(by)/yreal)) {
        /* Linear gradient */
        phi = 0.0;
        cx = cy = 0.0;
        x_0 = y_0 = 0.0;
        a = a1;
    }
    else {
        /* At least one quadratic term */
        gdouble cm = cxx - cyy;
        gdouble cp = cxx + cyy;
        gdouble bx1, by1, xc, yc;

        phi = 0.5*atan2(cxy, cm);
        cx = cp + hypot(cm, cxy);
        cy = cp - hypot(cm, cxy);
        bx1 = bx*cos(phi) + by*sin(phi);
        by1 = -bx*sin(phi) + by*cos(phi);

        /* Eliminate linear terms */
        if (fabs(cx) < 1e-14*fabs(cy)) {
            /* Only y quadratic term */
            xc = 0.0;
            yc = -by1/cy;
        }
        else if (fabs(cy) < 1e-14*fabs(cx)) {
            /* Only x quadratic term */
            xc = -bx1/cx;
            yc = 0.0;
        }
        else {
            /* Two quadratic terms */
            xc = -bx1/cx;
            yc = -by1/cy;
        }
        a = a1 + xc*bx1 + yc*by1 + xc*xc*cx + yc*yc*cy;
        x_0 = xc*cos(phi) - yc*sin(phi);
        y_0 = xc*sin(phi) + yc*cos(phi);
    }

    /* Shift to coordinate system with [0,0] in the corner */
    x_0 += 0.5*xreal;
    y_0 += 0.5*yreal;
    gwy_debug("x0=%g, y0=%g", x_0, y_0);

    params[PARAM_X0] = x_0;
    params[PARAM_Y0] = y_0;
    params[PARAM_A] = a;
    params[PARAM_R1] = 1.0/cx;
    params[PARAM_R2] = 1.0/cy;
    params[PARAM_PHI1] = fmod(phi, G_PI);
    if (params[PARAM_PHI1] > G_PI/2.0)
        params[PARAM_PHI1] -= G_PI;
    params[PARAM_PHI2] = fmod(phi + G_PI/2.0, G_PI);
    if (params[PARAM_PHI2] > G_PI/2.0)
        params[PARAM_PHI2] -= G_PI;

    ok = TRUE;
    for (i = 0; i < 2; i++) {
        ok &= intersect_with_boundary(params[PARAM_X0], params[PARAM_Y0],
                                      params[PARAM_PHI1 + i],
                                      xreal, yreal, i1 + i, i2 + i);
    }

    return ok;
}

static gboolean
curvature_set_selection(GwyDataField *dfield,
                        const Intersection *i1,
                        const Intersection *i2,
                        GwySelection *selection)
{
    gdouble xreal, yreal;
    gdouble xy[4];
    gint xres, yres;
    guint i;

    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    for (i = 0; i < 2; i++) {
        xy[0] = CLAMP(i1[i].x, 0, xreal*(xres - 1)/xres);
        xy[1] = CLAMP(i1[i].y, 0, yreal*(yres - 1)/yres);
        xy[2] = CLAMP(i2[i].x, 0, xreal*(xres - 1)/xres);
        xy[3] = CLAMP(i2[i].y, 0, yreal*(yres - 1)/yres);
        gwy_selection_set_object(selection, i, xy);
    }

    return TRUE;
}

static gboolean
curvature_plot_graph(GwyDataField *dfield,
                     const Intersection *i1,
                     const Intersection *i2,
                     GwyGraphModel *gmodel)
{
    GwyGraphCurveModel *gcmodel;
    GwyDataLine *dline;
    gint xres, yres;
    guint i;

    if (!gwy_graph_model_get_n_curves(gmodel)) {
        GwySIUnit *siunitxy, *siunitz;
        gchar *s;

        siunitxy = gwy_si_unit_duplicate(gwy_data_field_get_si_unit_xy(dfield));
        siunitz = gwy_si_unit_duplicate(gwy_data_field_get_si_unit_z(dfield));
        g_object_set(gmodel,
                     "title", _("Curvature Sections"),
                     "si-unit-x", siunitxy,
                     "si-unit-y", siunitz,
                     NULL);
        g_object_unref(siunitxy);
        g_object_unref(siunitz);

        for (i = 0; i < 2; i++) {
            gcmodel = gwy_graph_curve_model_new();
            s = g_strdup_printf(_("Profile %d"), (gint)i+1);
            g_object_set(gcmodel,
                         "description", s,
                         "mode", GWY_GRAPH_CURVE_LINE,
                         "color", gwy_graph_get_preset_color(i),
                         NULL);
            g_free(s);
            gwy_graph_model_add_curve(gmodel, gcmodel);
            g_object_unref(gcmodel);
        }
    }
    else {
        g_assert(gwy_graph_model_get_n_curves(gmodel) == 2);
    }

    dline = gwy_data_line_new(1, 1.0, FALSE);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    for (i = 0; i < 2; i++) {
        gint col1 = gwy_data_field_rtoj(dfield, i1[i].x);
        gint row1 = gwy_data_field_rtoi(dfield, i1[i].y);
        gint col2 = gwy_data_field_rtoj(dfield, i2[i].x);
        gint row2 = gwy_data_field_rtoi(dfield, i2[i].y);

        gwy_data_field_get_profile(dfield, dline,
                                   CLAMP(col1, 0, xres-1),
                                   CLAMP(row1, 0, yres-1),
                                   CLAMP(col2, 0, xres-1),
                                   CLAMP(row2, 0, yres-1),
                                   -1, 1, GWY_INTERPOLATION_BILINEAR);
        gwy_data_line_set_offset(dline,
                                 i1[i].t/(i2[i].t - i1[i].t)
                                 * gwy_data_line_get_real(dline));
        gcmodel = gwy_graph_model_get_curve(gmodel, i);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, dline, 0, 0);
    }
    g_object_unref(dline);

    return TRUE;
}

static void
curvature_do(GwyContainer *data,
             GwyDataField *dfield,
             GwyDataField *mfield,
             gint oldid,
             const CurvatureArgs *args)
{
    gdouble params[PARAM_NPARAMS];
    Intersection i1[2], i2[2];
    gint newid;
    gchar *key;

    if (!curvature_calculate(dfield, mfield, args, params, i1, i2))
        return;

    if (args->set_selection) {
        GwySelection *selection;

        selection = g_object_new(g_type_from_name("GwySelectionLine"),
                                 "max-objects", 1024,
                                 NULL);
        curvature_set_selection(dfield, i1, i2, selection);
        key = g_strdup_printf("/%d/select/line", oldid);
        gwy_container_set_object_by_name(data, key, selection);
        g_object_unref(selection);
    }

    if (args->plot_graph) {
        GwyGraphModel *gmodel;

        gmodel = gwy_graph_model_new();
        curvature_plot_graph(dfield, i1, i2, gmodel);
        newid = gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        g_object_unref(gmodel);
    }
}

static void
render_name(G_GNUC_UNUSED GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            gpointer data)
{
    const gchar **names = (const gchar**)data;
    gint i;

    gtk_tree_model_get(model, iter, 0, &i, -1);
    g_object_set(renderer, "text", _(names[i]), NULL);
}

static void
render_symbol(G_GNUC_UNUSED GtkTreeViewColumn *column,
              GtkCellRenderer *renderer,
              GtkTreeModel *model,
              GtkTreeIter *iter,
              gpointer data)
{
    const gchar **names = (const gchar**)data;
    gint i;

    gtk_tree_model_get(model, iter, 0, &i, -1);
    g_object_set(renderer, "markup", names[i], NULL);
}

static void
render_value(G_GNUC_UNUSED GtkTreeViewColumn *column,
             GtkCellRenderer *renderer,
             GtkTreeModel *model,
             GtkTreeIter *iter,
             gpointer data)
{
    const CurvatureControls *controls = (const CurvatureControls*)data;
    GwySIValueFormat *vf;
    gdouble val;
    gchar *s;
    gint i;

    gtk_tree_model_get(model, iter, 0, &i, -1);
    val = controls->params[i];
    if (i == PARAM_PHI1 || i == PARAM_PHI2) {
        s = g_strdup_printf("%.2f deg", -val*180.0/G_PI);
    }
    else {
        vf = gwy_si_unit_get_format_with_digits(controls->unit,
                                                GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                val, 3, NULL);
        s = g_strdup_printf("%.*f%s%s",
                            vf->precision, val/vf->magnitude,
                            *vf->units ? " " : "", vf->units);
        gwy_si_unit_value_format_free(vf);
    }
    g_object_set(renderer, "markup", s, NULL);
    g_free(s);
}

static gboolean
curvature_dialog(CurvatureArgs *args,
                 GwyContainer *data,
                 GwyDataField *dfield,
                 GwyDataField *mfield,
                 gint id)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *label, *hbox, *vbox, *treeview, *button;
    GtkTreeSelection *selection;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GwyPixmapLayer *player;
    GwyVectorLayer *vlayer;
    CurvatureControls controls;
    gint response;
    gint row;

    controls.args = args;
    controls.unit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_clear(controls.params, PARAM_NPARAMS);

    dialog = gtk_dialog_new_with_buttons(_("Curvature"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    controls.data = gwy_container_new();
    gwy_container_set_object_by_name(controls.data, "/0/data", dfield);
    if (mfield)
        gwy_container_set_object_by_name(controls.data, "/0/mask", mfield);
    gwy_app_sync_data_items(data, controls.data, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    controls.view = gwy_data_view_new(controls.data);
    player = gwy_layer_basic_new();
    g_object_set(player,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), player);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);

    vlayer = g_object_new(g_type_from_name("GwyLayerLine"), NULL);
    gwy_vector_layer_set_selection_key(vlayer, "/0/select/line");
    gwy_vector_layer_set_editable(vlayer, FALSE);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view),
                                GWY_VECTOR_LAYER(vlayer));
    controls.selection = gwy_vector_layer_ensure_selection(vlayer);
    g_object_set(controls.selection, "max-objects", 2, NULL);

    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(4 + (mfield ? 4 : 0), 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = 0;

    label = gwy_label_new_header(_("Output type"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.set_selection
        = gtk_check_button_new_with_mnemonic(_("_Set selection"));
    gtk_table_attach(GTK_TABLE(table), controls.set_selection,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.set_selection),
                                 args->set_selection);
    g_signal_connect(controls.set_selection, "toggled",
                     G_CALLBACK(curvature_set_selection_changed), &controls);
    row++;

    controls.plot_graph
        = gtk_check_button_new_with_mnemonic(_("_Plot graph"));
    gtk_table_attach(GTK_TABLE(table), controls.plot_graph,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.plot_graph),
                                 args->plot_graph);
    g_signal_connect(controls.plot_graph, "toggled",
                     G_CALLBACK(curvature_plot_graph_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    if (mfield) {
        label = gwy_label_new_header(_("Masking Mode"));
        gtk_table_attach(GTK_TABLE(table), label,
                        0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;

        controls.masking_group
            = gwy_radio_buttons_createl(G_CALLBACK(curvature_masking_changed),
                                        &controls, args->masking,
                                        _("_Exclude region under mask"),
                                        GWY_MASK_EXCLUDE,
                                        _("Exclude region _outside mask"),
                                        GWY_MASK_INCLUDE,
                                        _("Use entire _image (ignore mask)"),
                                        GWY_MASK_IGNORE,
                                        NULL);
        row = gwy_radio_buttons_attach_to_table(controls.masking_group,
                                                GTK_TABLE(table), 3, row);
        gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    }
    else
        controls.masking_group = NULL;

    controls.warning = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.warning), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.warning,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    controls.gmodel = gwy_graph_model_new();
    controls.graph = gwy_graph_new(controls.gmodel);
    gtk_widget_set_size_request(controls.graph, 320, 260);
    g_object_unref(controls.gmodel);

    gtk_box_pack_start(GTK_BOX(vbox), controls.graph, TRUE, TRUE, 4);

    controls.paramstore = gwy_null_store_new(PARAM_NPARAMS);
    treeview
        = gtk_tree_view_new_with_model(GTK_TREE_MODEL(controls.paramstore));
    g_object_unref(controls.paramstore);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), treeview, FALSE, FALSE, 4);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Parameter"), renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_name, param_names, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Symbol"), renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_symbol, param_symbols, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 1.0, NULL);
    column = gtk_tree_view_column_new_with_attributes(_("Value"), renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_value, &controls, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = curvature_add_aux_button(hbox, GTK_STOCK_SAVE,
                                      _("Save table to a file"));
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(curvature_save), &controls);

    button = curvature_add_aux_button(hbox, GTK_STOCK_COPY,
                                      _("Copy table to clipboard"));
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(curvature_copy), &controls);

    curvature_update_preview(&controls, args);

    gtk_widget_show_all(dialog);
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

            case RESPONSE_RESET:
            *args = curvature_defaults;
            curvature_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static GtkWidget*
curvature_add_aux_button(GtkWidget *hbox,
                         const gchar *stock_id,
                         const gchar *tooltip)
{
    GtkTooltips *tips;
    GtkWidget *button;

    tips = gwy_app_get_tooltips();
    button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_tooltips_set_tip(tips, button, tooltip, NULL);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(stock_id,
                                               GTK_ICON_SIZE_SMALL_TOOLBAR));
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    return button;
}
static void
curvature_dialog_update(CurvatureControls *controls,
                         CurvatureArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->set_selection),
                                 args->set_selection);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->plot_graph),
                                 args->plot_graph);
    if (controls->masking_group)
        gwy_radio_buttons_set_current(controls->masking_group, args->masking);
}

static void
curvature_set_selection_changed(GtkToggleButton *button,
                                CurvatureControls *controls)
{
    CurvatureArgs *args = controls->args;

    args->set_selection = gtk_toggle_button_get_active(button);
}

static void
curvature_plot_graph_changed(GtkToggleButton *button,
                             CurvatureControls *controls)
{
    CurvatureArgs *args = controls->args;

    args->plot_graph = gtk_toggle_button_get_active(button);
}

static void
curvature_masking_changed(GtkToggleButton *button,
                          CurvatureControls *controls)
{
    CurvatureArgs *args;

    if (!gtk_toggle_button_get_active(button))
        return;

    args = controls->args;
    args->masking = gwy_radio_buttons_get_current(controls->masking_group);
    curvature_update_preview(controls, args);
}

static void
curvature_update_preview(CurvatureControls *controls,
                         CurvatureArgs *args)
{
    GwyDataField *source, *mask = NULL;
    GwySelection *selection;
    Intersection i1[2], i2[2];
    gboolean ok;
    guint i;

    source = gwy_container_get_object_by_name(controls->data, "/0/data");
    selection = gwy_container_get_object_by_name(controls->data,
                                                 "/0/select/line");
    gwy_container_gis_object_by_name(controls->data, "/0/mask", &mask);

    ok = curvature_calculate(source, mask, args, controls->params, i1, i2);
    for (i = 0; i < PARAM_NPARAMS; i++)
        gwy_null_store_row_changed(controls->paramstore, i);

    if (ok) {
        curvature_set_selection(source, i1, i2, selection);
        curvature_plot_graph(source, i1, i2, controls->gmodel);
        gtk_label_set_text(GTK_LABEL(controls->warning), "");
    }
    else {
        gwy_selection_clear(selection);
        gwy_graph_model_remove_all_curves(controls->gmodel);
        gtk_label_set_text(GTK_LABEL(controls->warning),
                           _("Axes are outside the image."));
    }
}

static gchar*
curvature_make_report(const CurvatureControls *controls)
{
    GPtrArray *lines;
    GwySIValueFormat *vf;
    guint i, n, name_maxlen, sym_maxlen;
    gdouble val;
    GString *str;
    gchar *report;

    name_maxlen = sym_maxlen = 0;
    for (i = 0; i < PARAM_NPARAMS; i++) {
        n = g_utf8_strlen(_(param_names[i]), -1);
        name_maxlen = MAX(name_maxlen, n);
        n = g_utf8_strlen(param_symbols_plain[i], -1);
        sym_maxlen = MAX(sym_maxlen, n);
    }

    str = g_string_new(NULL);
    lines = g_ptr_array_new();
    for (i = 0; i < PARAM_NPARAMS; i++) {
        g_string_assign(str, _(param_names[i]));
        for (n = name_maxlen - g_utf8_strlen(_(param_names[i]), -1); n; n--)
            g_string_append_c(str, ' ');
        g_string_append(str, " ");
        g_string_append(str, param_symbols_plain[i]);
        for (n = sym_maxlen - g_utf8_strlen(param_symbols_plain[i], -1); n; n--)
            g_string_append_c(str, ' ');
        g_string_append(str, " = ");
        val = controls->params[i];
        if (i == PARAM_PHI1 || i == PARAM_PHI2) {
            g_string_append_printf(str, "%.2f deg", -val*180.0/G_PI);
        }
        else {
            vf = gwy_si_unit_get_format_with_digits(controls->unit,
                                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                    val, 3, NULL);
            g_string_append_printf(str, "%.*f%s%s",
                                   vf->precision, val/vf->magnitude,
                                   *vf->units ? " " : "", vf->units);
            gwy_si_unit_value_format_free(vf);
        }
        g_ptr_array_add(lines, g_strdup(str->str));
    }
    g_string_free(str, TRUE);

    g_ptr_array_add(lines, g_strdup(""));
    g_ptr_array_add(lines, NULL);
    report = g_strjoinv("\n", (gchar**)lines->pdata);
    for (i = 0; i < lines->len; i++)
        g_free(g_ptr_array_index(lines, i));
    g_ptr_array_free(lines, TRUE);

    return report;
}

static void
curvature_save(CurvatureControls *controls)
{
    gchar *text = curvature_make_report(controls);

    gwy_save_auxiliary_data(_("Save Curvature"), GTK_WINDOW(controls->dialog),
                            -1, text);
    g_free(text);
}

static void
curvature_copy(CurvatureControls *controls)
{
    GtkClipboard *clipboard;
    GdkDisplay *display;
    gchar *text = curvature_make_report(controls);

    display = gtk_widget_get_display(controls->dialog);
    clipboard = gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, text, -1);
    g_free(text);
}

static const gchar set_selection_key[] = "/module/curvature/set_selection";
static const gchar plot_graph_key[]    = "/module/curvature/plot_graph";
static const gchar masking_key[]       = "/module/curvature/masking";

static void
sanitize_args(CurvatureArgs *args)
{
    args->masking = MIN(args->masking, GWY_MASK_INCLUDE);
    args->set_selection = !!args->set_selection;
    args->plot_graph = !!args->plot_graph;
}

static void
load_args(GwyContainer *container,
          CurvatureArgs *args)
{
    *args = curvature_defaults;

    gwy_container_gis_enum_by_name(container, masking_key, &args->masking);
    gwy_container_gis_boolean_by_name(container, set_selection_key,
                                      &args->set_selection);
    gwy_container_gis_boolean_by_name(container, plot_graph_key,
                                      &args->plot_graph);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          CurvatureArgs *args)
{
    gwy_container_set_enum_by_name(container, masking_key, args->masking);
    gwy_container_set_boolean_by_name(container, set_selection_key,
                                      args->set_selection);
    gwy_container_set_boolean_by_name(container, plot_graph_key,
                                      args->plot_graph);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
