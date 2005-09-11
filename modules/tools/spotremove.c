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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libprocess/correct.h>
#include <libprocess/fractals.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

#define MAX_SIZE 64
#define SCALE 4

enum {
    SPOT_REMOVE_HYPER_FLATTEN,
    SPOT_REMOVE_PSEUDO_LAPLACE,
    SPOT_REMOVE_LAPLACE,
    SPOT_REMOVE_FRACTAL,
};

typedef struct {
    GwyUnitoolRectLabels labels;
    GtkWidget *view;
    gchar *pal;
    gint algorithm;
} ToolControls;

typedef void (*AverageFunc)(GwyDataField *dfield,
                            gint ximin,
                            gint yimin,
                            gint ximax,
                            gint yimax);

static gboolean   module_register       (const gchar *name);
static gboolean   use                   (GwyDataWindow *data_window,
                                         GwyToolSwitchEvent reason);
static void       layer_setup           (GwyUnitoolState *state);
static GtkWidget* dialog_create         (GwyUnitoolState *state);
static void       dialog_update         (GwyUnitoolState *state,
                                         GwyUnitoolUpdateType reason);
static void       dialog_abandon        (GwyUnitoolState *state);
static void       apply                 (GwyUnitoolState *state);
static void       load_args             (GwyContainer *container,
                                         ToolControls *controls);
static void       save_args             (GwyContainer *container,
                                         ToolControls *controls);
static void       draw_zoom             (ToolControls *controls,
                                         GwyDataField *dfield,
                                         gint ximin,
                                         gint yimin,
                                         gint ximax,
                                         gint yimax);
static gboolean   find_subrange         (gint min,
                                         gint max,
                                         gint res,
                                         gint size,
                                         gint *from,
                                         gint *to,
                                         gint *dest);
static void       crisscross_average    (GwyDataField *dfield,
                                         gint ximin,
                                         gint yimin,
                                         gint ximax,
                                         gint yimax);
static void       laplace_average       (GwyDataField *dfield,
                                         gint ximin,
                                         gint yimin,
                                         gint ximax,
                                         gint yimax);
static void       fractal_average       (GwyDataField *dfield,
                                         gint ximin,
                                         gint yimin,
                                         gint ximax,
                                         gint yimax);
static void       pseudo_laplace_average(GwyDataField *dfield,
                                         gint ximin,
                                         gint yimin,
                                         gint ximax,
                                         gint yimax);
static void       algorithm_changed_cb  (GtkWidget *combo,
                                         GwyUnitoolState *state);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Spot removal tool, interpolates small parts of data (displayed on "
       "a zoomed view) using selected algorithm."),
    "Yeti <yeti@gwyddion.net>",
    "1.3.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    layer_setup,                   /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                 /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    apply,                         /* apply action */
    NULL,                          /* nonstandard response handler */
};

static const GwyEnum algorithms[] = {
    { N_("Hyperbolic flatten"), SPOT_REMOVE_HYPER_FLATTEN },
    { N_("Pseudo-Laplace"),     SPOT_REMOVE_PSEUDO_LAPLACE },
    { N_("Laplace solver"),     SPOT_REMOVE_LAPLACE },
    { N_("Fractal correction"), SPOT_REMOVE_FRACTAL },
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo func_info = {
        "spotremove",
        GWY_STOCK_SPOT_REMOVE,
        N_("Manually remove spots"),
        120,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerRectangle";
    static GwyUnitoolState *state = NULL;
    ToolControls *controls;

    if (!state) {
        func_slots.layer_type = g_type_from_name(layer_name);
        if (!func_slots.layer_type) {
            g_warning("Layer type `%s' not available", layer_name);
            return FALSE;
        }
        state = g_new0(GwyUnitoolState, 1);
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
        state->apply_doesnt_close = TRUE;
    }
    controls = (ToolControls*)state->user_data;
    if (controls->view && data_window) {
        GwyContainer *data, *mydata;
        GwyDataField *dfield;
        gdouble min, max;

        data = gwy_data_window_get_data(data_window);
        mydata = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        min = gwy_data_field_get_min(dfield);
        max = gwy_data_field_get_max(dfield);
        gwy_container_set_double_by_name(mydata, "/0/base/min", min);
        gwy_container_set_double_by_name(mydata, "/0/base/max", max);
    }
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer,
                 "selection-key", "/0/select/rectangle",
                 "is-crop", FALSE,
                 NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyContainer *settings;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    GtkWidget *dialog, *table, *label, *frame, *vbox, *omenu;
    gdouble min, max;
    const guchar *pal;
    gint row;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    dialog = gtk_dialog_new_with_buttons(_("Remove Spots"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_clear(dialog);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    pal = NULL;
    gwy_container_gis_string_by_name(data, "/0/base/palette", &pal);
    controls->pal = g_strdup(pal);
    min = gwy_data_field_get_min(dfield);
    max = gwy_data_field_get_max(dfield);

    table = gtk_table_new(1, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 0);

    dfield = gwy_data_field_new(MAX_SIZE, MAX_SIZE, 1.0, 1.0, FALSE);
    data = GWY_CONTAINER(gwy_container_new());
    gwy_container_set_object_by_name(data, "/0/data", dfield);
    gwy_container_set_double_by_name(data, "/0/base/min", min);
    gwy_container_set_double_by_name(data, "/0/base/max", max);
    if (controls->pal)
        gwy_container_set_string_by_name(data, "/0/base/palette",
                                         g_strdup(controls->pal));
    g_object_unref(dfield);
    controls->view = gwy_data_view_new(data);
    g_object_unref(data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls->view), (gdouble)SCALE);

    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls->view), layer);
    gtk_table_attach(GTK_TABLE(table), controls->view, 0, 1, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_table_attach(GTK_TABLE(table), vbox, 1, 2, 0, 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    table = gtk_table_new(8, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    row = gwy_unitool_rect_info_table_setup(&controls->labels,
                                            GTK_TABLE(table), 0, 0);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gtk_label_new_with_mnemonic(_("Removal _method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    omenu = gwy_enum_combo_box_new(algorithms, G_N_ELEMENTS(algorithms),
                                   G_CALLBACK(algorithm_changed_cb), state,
                                   controls->algorithm, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    gtk_table_attach(GTK_TABLE(table), omenu, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    return dialog;
}

static void
dialog_update(GwyUnitoolState *state,
              GwyUnitoolUpdateType reason)
{
    gboolean is_visible, is_selected, is_ok;
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyContainer *mydata;
    GwyDataField *mydfield;
    gint isel[4];
    const guchar *pal;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;

    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    mydata = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    mydfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(mydata,
                                                               "/0/data"));
    pal = NULL;
    gwy_container_gis_string_by_name(data, "/0/base/palette", &pal);
    if (pal && !gwy_strequal(pal, controls->pal)) {
        gwy_container_set_string_by_name(mydata, "/0/base/palette",
                                         g_strdup(pal));
        g_free(controls->pal);
        controls->pal = g_strdup(pal);
    }
    if (reason == GWY_UNITOOL_UPDATED_DATA) {
        gdouble min, max;

        gwy_debug("Recomputing min, max");
        min = gwy_data_field_get_min(dfield);
        max = gwy_data_field_get_max(dfield);
        gwy_container_set_double_by_name(mydata, "/0/base/min", min);
        gwy_container_set_double_by_name(mydata, "/0/base/max", max);
    }

    is_visible = state->is_visible;
    is_selected = gwy_vector_layer_get_selection(state->layer, NULL);
    if (!is_visible && !is_selected)
        return;

    if (gwy_unitool_rect_info_table_fill(state, &controls->labels,
                                         NULL, isel)) {
        is_ok = isel[0] > 0
                && isel[1] > 0
                && isel[2] + 1 < gwy_data_field_get_xres(dfield)
                && isel[3] + 1 < gwy_data_field_get_yres(dfield)
                && isel[2] - isel[0] <= MAX_SIZE
                && isel[3] - isel[1] <= MAX_SIZE;
        draw_zoom(controls, dfield, isel[0], isel[1], isel[2], isel[3]);
    }
    else {
        is_ok = FALSE;
        draw_zoom(controls, NULL, -1, -1, -1, -1);
    }
    gwy_unitool_apply_set_sensitive(state, is_ok);
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    save_args(settings, controls);
    g_free(controls->pal);

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
apply(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gint isel[4];

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_unitool_rect_info_table_fill(state, &controls->labels, NULL, isel);
    gwy_app_undo_checkpoint(data, "/0/data", NULL);

    switch (controls->algorithm) {
        case SPOT_REMOVE_HYPER_FLATTEN:
        crisscross_average(dfield, isel[0], isel[1], isel[2], isel[3]);
        break;

        case SPOT_REMOVE_LAPLACE:
        laplace_average(dfield, isel[0], isel[1], isel[2], isel[3]);
        break;

        case SPOT_REMOVE_FRACTAL:
        fractal_average(dfield, isel[0], isel[1], isel[2], isel[3]);
        break;

        case SPOT_REMOVE_PSEUDO_LAPLACE:
        pseudo_laplace_average(dfield, isel[0], isel[1], isel[2], isel[3]);
        break;

        default:
        g_assert_not_reached();
        break;
    }
    /*gwy_vector_layer_unselect(state->layer);*/
    gwy_data_field_data_changed(dfield);
}

static void
crisscross_average(GwyDataField *dfield,
                   gint ximin, gint yimin,
                   gint ximax, gint yimax)
{
    gdouble *data;
    gint i, j, rowstride;

    gwy_debug("hyperbolic: (%d,%d) x (%d,%d)", ximin, ximax, yimin, yimax);
    data = gwy_data_field_get_data(dfield);
    rowstride = gwy_data_field_get_xres(dfield);

    for (i = yimin; i < yimax; i++) {
        gdouble px = data[i*rowstride + ximin - 1];
        gdouble qx = data[i*rowstride + ximax];
        gdouble y = (i - yimin + 1.0)/(yimax - yimin + 1.0);
        gdouble wx = 1.0/y + 1.0/(1.0 - y);

        for (j = ximin; j < ximax; j++) {
            gdouble py = data[(yimin - 1)*rowstride + j];
            gdouble qy = data[yimax*rowstride + j];
            gdouble x = (j - ximin + 1.0)/(ximax - ximin + 1.0);
            gdouble vy = px/x + qx/(1.0 - x);
            gdouble vx = py/y + qy/(1.0 - y);
            gdouble wy = 1.0/x + 1.0/(1.0 - x);

            data[i*rowstride + j] = (vx + vy)/(wx + wy);
        }
    }
}

static void
laplace_average(GwyDataField *dfield,
                gint ximin, gint yimin,
                gint ximax, gint yimax)
{
    GwyDataField *mask, *buffer;
    gdouble cor = 0.2, error, maxer;
    gint i = 0;

    gwy_debug("laplace: (%d,%d) x (%d,%d)", ximin, ximax, yimin, yimax);
    /* do pseudo-laplace as the first step to make it converge faster */
    pseudo_laplace_average(dfield, ximin, yimin, ximax, yimax);
    buffer = gwy_data_field_new_alike(dfield, FALSE);
    mask = gwy_data_field_new_alike(dfield, FALSE);
    gwy_data_field_fill(mask, 0.0);
    gwy_data_field_area_fill(mask, ximin, yimin, ximax, yimax, 1.0);

    maxer = gwy_data_field_get_rms(dfield)/1.0e3;
    do {
        gwy_data_field_correct_laplace_iteration(dfield, mask, buffer,
                                                 &error, &cor);
        i++;
    } while (error >= maxer && i < 1000);

    g_object_unref(buffer);
    g_object_unref(mask);
}

static void
fractal_average(GwyDataField *dfield,
                gint ximin, gint yimin,
                gint ximax, gint yimax)
{
    GwyDataField *mask;

    gwy_debug("fractal: (%d,%d) x (%d,%d)", ximin, ximax, yimin, yimax);
    mask = gwy_data_field_duplicate(dfield);
    gwy_data_field_fill(mask, 0.0);
    gwy_data_field_area_fill(mask, ximin, yimin, ximax, yimax, 1.0);
    gwy_data_field_fractal_correction(dfield, mask, GWY_INTERPOLATION_BILINEAR);
    g_object_unref(mask);
}

static void
pseudo_laplace_average(GwyDataField *dfield,
                       gint ximin, gint yimin,
                       gint ximax, gint yimax)
{
    gdouble *data, *disttable;
    gint i, j, k, rowstride, width, height;

    gwy_debug("pseudo_laplace: (%d,%d) x (%d,%d)", ximin, ximax, yimin, yimax);
    data = gwy_data_field_get_data(dfield);
    rowstride = gwy_data_field_get_xres(dfield);

    /* compute table of weights between different grid points */
    width = ximax - ximin + 1;
    height = yimax - yimin + 1;
    disttable = g_new(gdouble, width*height);
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++)
            disttable[i*width + j] = 1.0/(i*i + j*j + 1e-16);
    }

    for (i = yimin; i < yimax; i++) {
        for (j = ximin; j < ximax; j++) {
            gdouble w = 0.0, s = 0.0;

            for (k = yimin-1; k < yimax+1; k++) {
                gdouble ww;

                ww = disttable[abs(k - i)*width + j-ximin+1];
                w += ww;
                s += ww*data[k*rowstride + ximin-1];

                ww = disttable[abs(k - i)*width + ximax-j];
                w += ww;
                s += ww*data[k*rowstride + ximax];
            }

            for (k = ximin-1; k < ximax+1; k++) {
                gdouble ww;

                ww = disttable[abs(i-yimin+1)*width + abs(k - j)];
                w += ww;
                s += ww*data[(yimin-1)*rowstride + k];

                ww = disttable[abs(yimax-i)*width + abs(k - j)];
                w += ww;
                s += ww*data[yimax*rowstride + k];
            }

            data[i*rowstride + j] = s/w;
        }
    }

    g_free(disttable);
}

static void
draw_zoom(ToolControls *controls,
          GwyDataField *dfield,
          gint ximin, gint yimin,
          gint ximax, gint yimax)
{
    GwyContainer *mydata;
    GwyDataField *mydfield;
    gint xfrom, xto, xdest, yfrom, yto, ydest;
    gdouble min;
    gboolean complete;

    mydata = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    mydfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(mydata,
                                                               "/0/data"));
    min = gwy_container_get_double_by_name(mydata, "/0/base/min");
    if (!dfield) {
        gwy_data_field_fill(mydfield, min);
        gwy_data_field_data_changed(mydfield);
        return;
    }

    complete = TRUE;
    complete &= find_subrange(ximin, ximax,
                              gwy_data_field_get_xres(dfield), MAX_SIZE,
                              &xfrom, &xto, &xdest);
    complete &= find_subrange(yimin, yimax,
                              gwy_data_field_get_yres(dfield), MAX_SIZE,
                              &yfrom, &yto, &ydest);
    if (!complete)
        gwy_data_field_fill(mydfield, min);
    gwy_data_field_area_copy(dfield, mydfield, xfrom, yfrom, xto, yto,
                             xdest, ydest);
    gwy_data_field_data_changed(mydfield);
}

static gboolean
find_subrange(gint min, gint max, gint res, gint size,
              gint *from, gint *to, gint *dest)
{
    gint length = max - min;

    /* complete interval always fit in size */
    if (res <= size) {
        *from = 0;
        *to = res;
        *dest = (size - res)/2;
        return FALSE;
    }

    /* interval larger than size */
    if (length >= size) {
        *from = min;
        *to = min + size;
        *dest = 0;
        return TRUE;
    }

    /* interval shorter, fit intelligently */
    *from = MAX(0, min - (size - length)/2);
    *to = MIN(*from + size, res);
    *from = *to - size;
    g_assert(*from >= 0);
    *dest = 0;
    return TRUE;
}

static void
algorithm_changed_cb(GtkWidget *combo, GwyUnitoolState *state)
{
    ToolControls *controls;

    controls = (ToolControls*)state->user_data;
    controls->algorithm = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static const gchar *algorithm_key = "/tool/spotremove/algorithm";

static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->algorithm = SPOT_REMOVE_HYPER_FLATTEN;

    gwy_container_gis_enum_by_name(container, algorithm_key,
                                   &controls->algorithm);

    /* sanitize */
    controls->algorithm = CLAMP(controls->algorithm,
                                SPOT_REMOVE_HYPER_FLATTEN,
                                SPOT_REMOVE_FRACTAL);
}

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_enum_by_name(container, algorithm_key,
                                   controls->algorithm);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

