/*
 *  @(#) $Id$
 *  Copyright (C) 2013 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */
#define DEBUG 1
#include "config.h"
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define AFFINE_RUN_MODES (GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400,
};

enum {
    RESPONSE_RESET = 1,
};

typedef enum {
    IMAGE_DATA,
    IMAGE_ACF,
} ImageMode;

typedef struct {
    gboolean whatever;

    ImageMode image_mode;
} AffcorArgs;

typedef struct {
    AffcorArgs *args;
    GtkWidget *dialog;
    GtkWidget *view;
    GwySelection *selection;
    GwyContainer *mydata;
    GSList *image_mode;
    GwySIValueFormat *vf;
    GwySIValueFormat *vfphi;
    GtkWidget *a1;
    GtkWidget *a2;
    GtkWidget *a1_len;
    GtkWidget *a2_len;
    GtkWidget *phi;
} AffcorControls;

static gboolean   module_register     (void);
static void       correct_affine      (GwyContainer *data,
                                       GwyRunType run);
static void       affcor_dialog       (AffcorArgs *args,
                                       GwyContainer *data,
                                       GwyDataField *dfield,
                                       gint id);
static GtkWidget* add_lattice_label   (GtkTable *table,
                                       const gchar *name,
                                       gint *row,
                                       GwySIValueFormat *vf);
static void       init_selection      (GwySelection *selection,
                                       GwyDataField *dfield);
static void       refine              (AffcorControls *controls);
static void       find_maximum        (GwyDataField *dfield,
                                       gdouble *x,
                                       gdouble *y,
                                       gint xwinsize,
                                       gint ywinsize);
static void       selection_changed   (AffcorControls *controls);
static void       image_mode_changed  (GtkToggleButton *button,
                                       AffcorControls *controls);
static void       affcor_load_args    (GwyContainer *container,
                                       AffcorArgs *args);
static void       affcor_save_args    (GwyContainer *container,
                                       AffcorArgs *args);
static void       affcor_sanitize_args(AffcorArgs *args);

static const AffcorArgs affcor_defaults = {
    FALSE,
    IMAGE_DATA,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Corrects affine distortion of images by matching image Bravais "
       "lattice to the true one."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("correct_affine",
                              (GwyProcessFunc)&correct_affine,
                              N_("/_Correct Data/_Affine Distortion..."),
                              NULL,
                              AFFINE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Correct affine distortion"));

    return TRUE;
}

static void
correct_affine(GwyContainer *data, GwyRunType run)
{
    AffcorArgs args;
    GwyDataField *dfield;
    gint id;

    g_return_if_fail(run & AFFINE_RUN_MODES);
    g_return_if_fail(g_type_from_name("GwyLayerLattice"));
    affcor_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    affcor_dialog(&args, data, dfield, id);
    affcor_save_args(gwy_app_settings_get(), &args);
}

static void
affcor_dialog(AffcorArgs *args,
              GwyContainer *data,
              GwyDataField *dfield,
              gint id)
{
    GtkWidget *hbox, *label, *button;
    GtkDialog *dialog;
    GtkTable *table;
    GwyDataField *acf, *tmp;
    AffcorControls controls;
    gint response;
    GwyPixmapLayer *layer;
    GwyVectorLayer *vlayer;
    GwySIUnit *unitphi;
    gint row;

    controls.args = args;

    controls.dialog = gtk_dialog_new_with_buttons(_("Affine Correction"),
                                                  NULL, 0,
                                                  _("_Reset"),
                                                  RESPONSE_RESET,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_CANCEL,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_OK,
                                                  NULL);
    dialog = GTK_DIALOG(controls.dialog);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);
    gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_OK, FALSE);

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    tmp = gwy_data_field_duplicate(dfield);
    gwy_data_field_add(tmp, -gwy_data_field_get_avg(tmp));
    acf = gwy_data_field_new_alike(dfield, FALSE);
    gwy_data_field_area_2dacf(tmp, acf, 0, 0, dfield->xres, dfield->yres,
                              dfield->xres/4, dfield->yres/4);
    g_object_unref(tmp);

    gwy_container_set_object_by_name(controls.mydata, "/1/data", acf);
    g_object_unref(acf);
    gwy_app_sync_data_items(data, controls.mydata, id, 1, FALSE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);

    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);

    vlayer = g_object_new(g_type_from_name("GwyLayerLattice"),
                          "selection-key", "/0/select/vector",
                          NULL);
    gwy_data_view_set_top_layer(GWY_DATA_VIEW(controls.view), vlayer);
    controls.selection = gwy_vector_layer_ensure_selection(vlayer);
    gwy_selection_set_max_objects(controls.selection, 1);
    g_signal_connect_swapped(controls.selection, "changed",
                             G_CALLBACK(selection_changed), &controls);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = GTK_TABLE(gtk_table_new(10, 4, FALSE));
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    row = 0;

    /* XXX: This is not a preview, we should also have a real preview. */
    label = gtk_label_new(_("Preview type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.image_mode
        = gwy_radio_buttons_createl(G_CALLBACK(image_mode_changed), &controls,
                                    args->image_mode,
                                    _("Data"), IMAGE_DATA,
                                    _("2D ACF"), IMAGE_ACF,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.image_mode,
                                            GTK_TABLE(table), 4, row);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gwy_label_new_header(_("Lattice Vectors"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.vf
        = gwy_data_field_get_value_format_xy(dfield,
                                             GWY_SI_UNIT_FORMAT_VFMARKUP, NULL);
    unitphi = gwy_si_unit_new("deg");
    controls.vfphi
        = gwy_si_unit_get_format_with_resolution(unitphi,
                                                 GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                 180.0, 0.1, NULL);
    g_object_unref(unitphi);

    controls.a1 = add_lattice_label(table, "a<sub>1</sub>",
                                    &row, controls.vf);
    controls.a1_len = add_lattice_label(table, "|a<sub>1</sub>|",
                                        &row, controls.vf);
    controls.a2 = add_lattice_label(table, "a<sub>2</sub>",
                                    &row, controls.vf);
    controls.a2_len = add_lattice_label(table, "|a<sub>2</sub>|",
                                        &row, controls.vf);
    controls.phi = add_lattice_label(table, "ϕ",
                                     &row, controls.vfphi);

    button = gtk_button_new_with_mnemonic(_("Re_fine"));
    gtk_table_attach(GTK_TABLE(table), button,
                     2, 4, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(refine), &controls);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    init_selection(controls.selection, dfield);

    gtk_widget_show_all(controls.dialog);
    do {
        response = gtk_dialog_run(dialog);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(controls.dialog);
            case GTK_RESPONSE_NONE:
            goto finalize;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            /* TODO gwy_selection_clear(controls.selection); */
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(controls.dialog);
finalize:
    gwy_si_unit_value_format_free(controls.vf);
    gwy_si_unit_value_format_free(controls.vfphi);
    g_object_unref(controls.mydata);
}

static GtkWidget*
add_lattice_label(GtkTable *table,
                  const gchar *name,
                  gint *row,
                  GwySIValueFormat *vf)
{
    GtkWidget *label;
    GtkRequisition req;

    label = gtk_label_new("(-1234.5, 1234.5)");
    gtk_widget_size_request(label, &req);

    gtk_label_set_markup(GTK_LABEL(label), name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, *row, *row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new(" = ");
    gtk_table_attach(table, label, 1, 2, *row, *row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), vf->units);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 3, 4, *row, *row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new(NULL);
    gtk_widget_set_size_request(label, req.width, -1);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label,
                     2, 3, *row, *row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    (*row)++;

    return label;
}

static void
init_selection(GwySelection *selection,
               GwyDataField *dfield)
{
    gdouble xy[4] = { 0.0, 0.0, 0.0, 0.0 };

    xy[0] = dfield->xreal/20;
    xy[3] = -dfield->yreal/20;
    gwy_selection_set_data(selection, 1, xy);
}

static void
refine(AffcorControls *controls)
{
    GwyDataField *acf;
    gint xwinsize, ywinsize;
    gdouble xy[4];

    if (!gwy_selection_get_object(controls->selection, 0, xy))
        return;

    acf = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                          "/1/data"));
    xwinsize = (gint)(0.28*MAX(fabs(xy[0]), fabs(xy[2]))
                      /gwy_data_field_get_xmeasure(acf) + 0.5);
    ywinsize = (gint)(0.28*MAX(fabs(xy[1]), fabs(xy[3]))
                      /gwy_data_field_get_ymeasure(acf) + 0.5);
    gwy_debug("window size: %dx%d", xwinsize, ywinsize);

    xy[0] = (xy[0] - acf->xoff)/gwy_data_field_get_xmeasure(acf);
    xy[1] = (xy[1] - acf->yoff)/gwy_data_field_get_ymeasure(acf);
    xy[2] = (xy[2] - acf->xoff)/gwy_data_field_get_xmeasure(acf);
    xy[3] = (xy[3] - acf->yoff)/gwy_data_field_get_ymeasure(acf);
    find_maximum(acf, xy + 0, xy + 1, xwinsize, ywinsize);
    find_maximum(acf, xy + 2, xy + 3, xwinsize, ywinsize);
    xy[0] = (xy[0] + 0.5)*gwy_data_field_get_xmeasure(acf) + acf->xoff;
    xy[1] = (xy[1] + 0.5)*gwy_data_field_get_ymeasure(acf) + acf->yoff;
    xy[2] = (xy[2] + 0.5)*gwy_data_field_get_xmeasure(acf) + acf->xoff;
    xy[3] = (xy[3] + 0.5)*gwy_data_field_get_ymeasure(acf) + acf->yoff;
    gwy_selection_set_object(controls->selection, 0, xy);
}

static void
find_maximum(GwyDataField *dfield,
             gdouble *x, gdouble *y,
             gint xwinsize, gint ywinsize)
{
    gint xj = (gint)*x, yi = (gint)*y;
    gdouble max = -G_MAXDOUBLE;
    gint mi = yi, mj = xj, i, j;
    gint xres = dfield->xres, yres = dfield->yres;
    const gdouble *d = dfield->data;
    gdouble sz, szx, szy, szxx, szxy, szyy;
    gdouble v, bx, by, cxx, cxy, cyy, D, sx, sy;
    gdouble m[6], rhs[3];

    for (i = -ywinsize; i <= ywinsize; i++) {
        if (i + yi < 0 || i + yi > yres-1)
            continue;
        for (j = -xwinsize; j <= xwinsize; j++) {
            if (j + xj < 0 || j + xj > xres-1)
                continue;

            v = d[(i + yi)*xres + (j + xj)];
            if (v > max) {
                max = v;
                mi = i + yi;
                mj = j + xj;
            }
        }
    }

    *x = mj;
    *y = mi;

    /* Don't try any sub-pixel refinement if it's on the edge. */
    if (mi < 1 || mi+1 > yres-1 || mj < 1 || mj+1 > xres-1)
        return;

    sz = (d[(mi - 1)*xres + (mj - 1)]
          + d[(mi - 1)*xres + mj]
          + d[(mi - 1)*xres + (mj + 1)]
          + d[mi*xres + (mj - 1)]
          + d[mi*xres + mj]
          + d[mi*xres + (mj + 1)]
          + d[(mi + 1)*xres + (mj - 1)]
          + d[(mi + 1)*xres + mj]
          + d[(mi + 1)*xres + (mj + 1)]);
    szx = (-d[(mi - 1)*xres + (mj - 1)]
           + d[(mi - 1)*xres + (mj + 1)]
           - d[mi*xres + (mj - 1)]
           + d[mi*xres + (mj + 1)]
           - d[(mi + 1)*xres + (mj - 1)]
           + d[(mi + 1)*xres + (mj + 1)]);
    szy = (-d[(mi - 1)*xres + (mj - 1)]
           - d[(mi - 1)*xres + mj]
           - d[(mi - 1)*xres + (mj + 1)]
           + d[(mi + 1)*xres + (mj - 1)]
           + d[(mi + 1)*xres + mj]
           + d[(mi + 1)*xres + (mj + 1)]);
    szxx = (d[(mi - 1)*xres + (mj - 1)]
            + d[(mi - 1)*xres + (mj + 1)]
            + d[mi*xres + (mj - 1)]
            + d[mi*xres + (mj + 1)]
            + d[(mi + 1)*xres + (mj - 1)]
            + d[(mi + 1)*xres + (mj + 1)]);
    szxy = (d[(mi - 1)*xres + (mj - 1)]
            - d[(mi - 1)*xres + (mj + 1)]
            - d[(mi + 1)*xres + (mj - 1)]
            + d[(mi + 1)*xres + (mj + 1)]);
    szyy = (d[(mi - 1)*xres + (mj - 1)]
            + d[(mi - 1)*xres + mj]
            + d[(mi - 1)*xres + (mj + 1)]
            + d[(mi + 1)*xres + (mj - 1)]
            + d[(mi + 1)*xres + mj]
            + d[(mi + 1)*xres + (mj + 1)]);

    m[0] = 9.0;
    m[1] = m[2] = m[3] = m[5] = 6.0;
    m[4] = 4.0;
    gwy_math_choleski_decompose(3, m);

    rhs[0] = sz;
    rhs[1] = szxx;
    rhs[2] = szyy;
    gwy_math_choleski_solve(3, m, rhs);

    bx = szx/6.0;
    by = szy/6.0;
    cxx = rhs[1];
    cxy = szxy/4.0;
    cyy = rhs[2];

    D = 4.0*cxx*cyy - cxy*cxy;
    /* Don't try the sub-pixel refinement if bad cancellation occurs. */
    if (fabs(D) < 1e-8*MAX(fabs(4.0*cxx*cyy), fabs(cxy*cxy)))
        return;

    sx = (by*cxy - 2.0*bx*cyy)/D;
    sy = (bx*cxy - 2.0*by*cxx)/D;

    /* Don't trust the sub-pixel refinement if it moves the maximum outside
     * the 3×3 neighbourhood. */
    if (fabs(sx) > 1.5 || fabs(sy) > 1.5)
        return;

    *x += sx;
    *y += sy;
}

static void
selection_changed(AffcorControls *controls)
{
    GwySIValueFormat *vf;
    gdouble xy[4];
    gdouble phi;
    gchar *buf;

    if (!gwy_selection_get_data(controls->selection, NULL)) {
        gtk_label_set_text(GTK_LABEL(controls->a1), "");
        gtk_label_set_text(GTK_LABEL(controls->a2), "");
        return;
    }

    gwy_selection_get_object(controls->selection, 0, xy);
    vf = controls->vf;

    buf = g_strdup_printf("(%.*f, %.*f)",
                          vf->precision, xy[0]/vf->magnitude,
                          vf->precision, -xy[1]/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a1), buf);
    g_free(buf);

    buf = g_strdup_printf("%.*f",
                          vf->precision, hypot(xy[0], xy[1])/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a1_len), buf);
    g_free(buf);

    buf = g_strdup_printf("(%.*f, %.*f)",
                          vf->precision, xy[2]/vf->magnitude,
                          vf->precision, -xy[3]/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a2), buf);
    g_free(buf);

    buf = g_strdup_printf("%.*f",
                          vf->precision, hypot(xy[2], xy[3])/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->a2_len), buf);
    g_free(buf);

    vf = controls->vfphi;
    phi = atan2(-xy[3], xy[2]) - atan2(-xy[1], xy[0]);
    if (phi < 0.0)
        phi += 2.0*G_PI;
    buf = g_strdup_printf("%.*f",
                          vf->precision, 180.0/G_PI*phi/vf->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->phi), buf);
    g_free(buf);
}

static void
image_mode_changed(G_GNUC_UNUSED GtkToggleButton *button,
                   AffcorControls *controls)
{
    AffcorArgs *args = controls->args;
    GwyPixmapLayer *layer;
    ImageMode mode;

    mode = gwy_radio_buttons_get_current(controls->image_mode);
    if (mode == args->image_mode)
        return;
    args->image_mode = mode;
    layer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));

    if (args->image_mode == IMAGE_DATA)
        gwy_pixmap_layer_set_data_key(layer, "/0/data");
    else if (args->image_mode == IMAGE_ACF)
        gwy_pixmap_layer_set_data_key(layer, "/1/data");

    gwy_set_data_preview_size(GWY_DATA_VIEW(controls->view), PREVIEW_SIZE);
}

//static const gchar separate_key[]      = "/module/fft_profile/separate";

static void
affcor_sanitize_args(AffcorArgs *args)
{
}

static void
affcor_load_args(GwyContainer *container,
                 AffcorArgs *args)
{
    *args = affcor_defaults;

    affcor_sanitize_args(args);
}

static void
affcor_save_args(GwyContainer *container,
                 AffcorArgs *args)
{
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
