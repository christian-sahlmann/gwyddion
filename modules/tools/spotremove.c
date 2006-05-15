/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/stats.h>
#include <libprocess/fractals.h>
#include <libprocess/correct.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwylayer-basic.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_SPOT_REMOVER            (gwy_tool_spot_remover_get_type())
#define GWY_TOOL_SPOT_REMOVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_SPOT_REMOVER, GwyToolSpotRemover))
#define GWY_IS_TOOL_SPOT_REMOVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_SPOT_REMOVER))
#define GWY_TOOL_SPOT_REMOVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_SPOT_REMOVER, GwyToolSpotRemoverClass))

enum {
    MAX_SIZE = 64,
    SCALE = 4
};

typedef enum {
    GWY_SPOT_REMOVE_HYPER_FLATTEN  = 0,
    GWY_SPOT_REMOVE_PSEUDO_LAPLACE = 1,
    GWY_SPOT_REMOVE_LAPLACE        = 2,
    GWY_SPOT_REMOVE_FRACTAL        = 3
} SpotRemoveMethod;

typedef struct _GwyToolSpotRemover      GwyToolSpotRemover;
typedef struct _GwyToolSpotRemoverClass GwyToolSpotRemoverClass;

typedef void (*InterpolateFunc)(GwyDataField *data_field,
                                gint ximin,
                                gint yimin,
                                gint ximax,
                                gint yimax);

typedef struct {
    SpotRemoveMethod method;
} ToolArgs;

struct _GwyToolSpotRemover {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GwyContainer *data;
    GwyDataField *detail;

    GwyRectSelectionLabels *rlabels;
    GtkWidget *zoomview;
    GtkWidget *method;
    GtkWidget *apply;

    gulong palette_id;

    /* potential class data */
    GType layer_type_rect;
};

struct _GwyToolSpotRemoverClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_spot_remover_get_type        (void) G_GNUC_CONST;
static void gwy_tool_spot_remover_finalize         (GObject *object);
static void gwy_tool_spot_remover_init_dialog      (GwyToolSpotRemover *tool);
static void gwy_tool_spot_remover_data_switched    (GwyTool *gwytool,
                                                    GwyDataView *data_view);
static void gwy_tool_spot_remover_data_changed     (GwyPlainTool *plain_tool);
static void gwy_tool_spot_remover_palette_changed  (GwyToolSpotRemover *tool);
static void gwy_tool_spot_remover_response         (GwyTool *tool,
                                                    gint response_id);
static void gwy_tool_spot_remover_selection_changed(GwyPlainTool *plain_tool,
                                                    gint hint);
static void gwy_tool_spot_remover_draw_zoom        (GwyToolSpotRemover *tool);
static void gwy_tool_spot_remover_method_changed   (GtkComboBox *combo,
                                                    GwyToolSpotRemover *tool);
static void gwy_tool_spot_remover_apply            (GwyToolSpotRemover *tool);
static void hyperbolic_average                     (GwyDataField *dfield,
                                                    gint ximin,
                                                    gint yimin,
                                                    gint ximax,
                                                    gint yimax);
static void laplace_average                        (GwyDataField *dfield,
                                                    gint ximin,
                                                    gint yimin,
                                                    gint ximax,
                                                    gint yimax);
static void fractal_average                        (GwyDataField *dfield,
                                                    gint ximin,
                                                    gint yimin,
                                                    gint ximax,
                                                    gint yimax);
static void pseudo_laplace_average                 (GwyDataField *dfield,
                                                    gint ximin,
                                                    gint yimin,
                                                    gint ximax,
                                                    gint yimax);
static gboolean find_subrange                      (gint min,
                                                    gint max,
                                                    gint res,
                                                    gint size,
                                                    gint *from,
                                                    gint *to,
                                                    gint *dest);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Spot removal tool, interpolates small parts of data (displayed on "
       "a zoomed view) using selected algorithm."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static const gchar method_key[] = "/module/spotremover/method";

static const ToolArgs default_args = {
    GWY_SPOT_REMOVE_PSEUDO_LAPLACE,
};

static const InterpolateFunc method_functions[] = {
    &hyperbolic_average,
    &pseudo_laplace_average,
    &laplace_average,
    &fractal_average,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolSpotRemover, gwy_tool_spot_remover, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_SPOT_REMOVER);

    return TRUE;
}

static void
gwy_tool_spot_remover_class_init(GwyToolSpotRemoverClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_spot_remover_finalize;

    tool_class->stock_id = GWY_STOCK_SPOT_REMOVE;
    tool_class->title = _("Remove Spots");
    tool_class->tooltip = _("Interpolate small defects, manually selected");
    tool_class->prefix = "/module/spotremover";
    tool_class->data_switched = gwy_tool_spot_remover_data_switched;
    tool_class->response = gwy_tool_spot_remover_response;

    ptool_class->data_changed = gwy_tool_spot_remover_data_changed;
    ptool_class->selection_changed = gwy_tool_spot_remover_selection_changed;
}

static void
gwy_tool_spot_remover_finalize(GObject *object)
{
    GwyToolSpotRemover *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_SPOT_REMOVER(object);

    settings = gwy_app_settings_get();
    gwy_container_set_enum_by_name(settings, method_key,
                                   tool->args.method);

    gwy_signal_handler_disconnect(GWY_PLAIN_TOOL(object)->container,
                                  tool->palette_id);
    gwy_object_unref(tool->data);
    gwy_object_unref(tool->detail);

    G_OBJECT_CLASS(gwy_tool_spot_remover_parent_class)->finalize(object);
}

static void
gwy_tool_spot_remover_init(GwyToolSpotRemover *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_rect = gwy_plain_tool_check_layer_type(plain_tool,
                                                           "GwyLayerRectangle");
    if (!tool->layer_type_rect)
        return;

    plain_tool->lazy_updates = TRUE;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_enum_by_name(settings, method_key,
                                   &tool->args.method);

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_rect,
                                     "rectangle");

    tool->data = gwy_container_new();
    tool->detail = gwy_data_field_new(MAX_SIZE, MAX_SIZE, MAX_SIZE, MAX_SIZE,
                                      TRUE);
    gwy_container_set_object_by_name(tool->data, "/0/data", tool->detail);
    gwy_container_set_double_by_name(tool->data, "/0/base/min", 0.0);
    gwy_container_set_double_by_name(tool->data, "/0/base/max", 0.0);
    gwy_container_set_enum_by_name(tool->data, "/0/base/range-type",
                                   GWY_LAYER_BASIC_RANGE_AUTO);

    gwy_tool_spot_remover_init_dialog(tool);
}

static void
gwy_tool_spot_remover_rect_updated(GwyToolSpotRemover *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_rect_selection_labels_select(tool->rlabels,
                                     plain_tool->selection,
                                     plain_tool->data_field);
}

static void
gwy_tool_spot_remover_init_dialog(GwyToolSpotRemover *tool)
{
    static const GwyEnum methods[] = {
        { N_("Hyperbolic flatten"),    GWY_SPOT_REMOVE_HYPER_FLATTEN,  },
        { N_("Pseudo-Laplace"),        GWY_SPOT_REMOVE_PSEUDO_LAPLACE, },
        { N_("Laplace solver"),        GWY_SPOT_REMOVE_LAPLACE,        },
        { N_("Fractal interpolation"), GWY_SPOT_REMOVE_FRACTAL,        },
    };
    GtkDialog *dialog;
    GtkTable *table;
    GtkWidget *hbox, *vbox, *label;
    GwyPixmapLayer *layer;
    gint row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    hbox = gtk_hbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), hbox, TRUE, TRUE, 0);

    /* Zoom view */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    tool->zoomview = gwy_data_view_new(tool->data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(tool->zoomview), (gdouble)SCALE);
    gtk_box_pack_start(GTK_BOX(vbox), tool->zoomview, FALSE, FALSE, 0);

    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_layer_basic_set_range_type_key(GWY_LAYER_BASIC(layer),
                                       "/0/base/range-type");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(tool->zoomview), layer);

    /* Right pane */
    vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    /* Selection info */
    tool->rlabels = gwy_rect_selection_labels_new
                         (FALSE, G_CALLBACK(gwy_tool_spot_remover_rect_updated),
                          tool);
    gtk_box_pack_start(GTK_BOX(vbox),
                       gwy_rect_selection_labels_get_table(tool->rlabels),
                       FALSE, FALSE, 0);

    /* Options */
    table = GTK_TABLE(gtk_table_new(3, 4, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Interpolation method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    tool->method = gwy_enum_combo_box_new
                             (methods, G_N_ELEMENTS(methods),
                              G_CALLBACK(gwy_tool_spot_remover_method_changed),
                              tool,
                              tool->args.method, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), tool->method);
    gtk_table_attach(GTK_TABLE(table), tool->method,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_spot_remover_data_switched(GwyTool *gwytool,
                                    GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolSpotRemover *tool;
    GwyPixmapLayer *layer;
    const gchar *key;
    gchar *sigdetail;

    tool = GWY_TOOL_SPOT_REMOVER(gwytool);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    gwy_signal_handler_disconnect(plain_tool->container, tool->palette_id);

    GWY_TOOL_CLASS(gwy_tool_spot_remover_parent_class)->data_switched(gwytool,
                                                                     data_view);
    if (plain_tool->init_failed)
        return;

    if (data_view) {
        g_object_set(plain_tool->layer,
                     "draw-reflection", FALSE,
                     "is-crop", FALSE,
                     NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);

        layer = gwy_data_view_get_base_layer(data_view);
        g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
        key = gwy_layer_basic_get_gradient_key(GWY_LAYER_BASIC(layer));
        if (key) {
            sigdetail = g_strconcat("item-changed::", key, NULL);
            tool->palette_id = g_signal_connect_swapped
                             (plain_tool->container, sigdetail,
                              G_CALLBACK(gwy_tool_spot_remover_palette_changed),
                              tool);
            g_free(sigdetail);
        }
        gwy_tool_spot_remover_palette_changed(tool);
    }
}

static void
gwy_tool_spot_remover_data_changed(GwyPlainTool *plain_tool)
{
    gwy_rect_selection_labels_fill(GWY_TOOL_SPOT_REMOVER(plain_tool)->rlabels,
                                   plain_tool->selection,
                                   plain_tool->data_field,
                                   NULL, NULL);
    gwy_tool_spot_remover_selection_changed(plain_tool, -1);
}

static void
gwy_tool_spot_remover_palette_changed(GwyToolSpotRemover *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_app_copy_data_items(plain_tool->container, tool->data,
                            plain_tool->id, 0,
                            GWY_DATA_ITEM_GRADIENT, 0);
}

static void
gwy_tool_spot_remover_response(GwyTool *tool,
                               gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_spot_remover_parent_class)->response(tool,
                                                                 response_id);

    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_spot_remover_apply(GWY_TOOL_SPOT_REMOVER(tool));
}

static void
gwy_tool_spot_remover_selection_changed(GwyPlainTool *plain_tool,
                                        gint hint)
{
    GwyToolSpotRemover *tool;
    gint n = 0;

    tool = GWY_TOOL_SPOT_REMOVER(plain_tool);
    g_return_if_fail(hint <= 0);

    if (plain_tool->selection) {
        n = gwy_selection_get_data(plain_tool->selection, NULL);
        g_return_if_fail(n == 0 || n == 1);
        gwy_rect_selection_labels_fill(tool->rlabels,
                                       plain_tool->selection,
                                       plain_tool->data_field,
                                       NULL, NULL);
    }
    else
        gwy_rect_selection_labels_fill(tool->rlabels, NULL, NULL, NULL, NULL);

    gwy_tool_spot_remover_draw_zoom(tool);
}

static void
gwy_tool_spot_remover_draw_zoom(GwyToolSpotRemover *tool)
{
    GwyPlainTool *plain_tool;
    gint xfrom, xto, xdest, yfrom, yto, ydest;
    gdouble min;
    gdouble sel[4];
    gint isel[4];
    gboolean complete, is_ok;

    plain_tool = GWY_PLAIN_TOOL(tool);

    if (!plain_tool->data_field
        || !plain_tool->selection
        || !gwy_selection_get_object(plain_tool->selection, 0, sel)) {
        gwy_data_field_clear(tool->detail);
        gwy_container_set_double_by_name(tool->data, "/0/base/min", 0.0);
        gwy_container_set_double_by_name(tool->data, "/0/base/max", 0.0);
        gwy_data_field_data_changed(tool->detail);
        gtk_widget_set_sensitive(tool->apply, FALSE);
        return;
    }

    isel[0] = gwy_data_field_rtoj(plain_tool->data_field, sel[0]);
    isel[1] = gwy_data_field_rtoi(plain_tool->data_field, sel[1]);
    isel[2] = gwy_data_field_rtoj(plain_tool->data_field, sel[2]) + 1;
    isel[3] = gwy_data_field_rtoi(plain_tool->data_field, sel[3]) + 1;
    if (isel[0] > isel[2])
        GWY_SWAP(gdouble, isel[0], isel[2]);
    if (isel[1] > isel[3])
        GWY_SWAP(gdouble, isel[1], isel[3]);

    is_ok = (isel[0] > 0
             && isel[1] > 0
             && isel[2] < gwy_data_field_get_xres(plain_tool->data_field)-1
             && isel[3] < gwy_data_field_get_yres(plain_tool->data_field)-1
             && isel[2] - isel[0] <= MAX_SIZE
             && isel[3] - isel[1] <= MAX_SIZE);

    complete = TRUE;
    complete &= find_subrange(isel[0], isel[2],
                              gwy_data_field_get_xres(plain_tool->data_field),
                              MAX_SIZE,
                              &xfrom, &xto, &xdest);
    complete &= find_subrange(isel[1], isel[3],
                              gwy_data_field_get_yres(plain_tool->data_field),
                              MAX_SIZE,
                              &yfrom, &yto, &ydest);
    if (!complete) {
        min = gwy_data_field_area_get_min(plain_tool->data_field, NULL,
                                          xfrom, yfrom,
                                          xto - xfrom, yto - yfrom);
        gwy_data_field_fill(tool->detail, min);
    }
    gwy_data_field_area_copy(plain_tool->data_field, tool->detail,
                             xfrom, yfrom, xto - xfrom, yto - yfrom,
                             xdest, ydest);
    gwy_data_field_data_changed(tool->detail);

    gtk_widget_set_sensitive(tool->apply, is_ok);
}

static gboolean
find_subrange(gint min,
              gint max,
              gint res,
              gint size,
              gint *from,
              gint *to,
              gint *dest)
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
gwy_tool_spot_remover_method_changed(GtkComboBox *combo,
                                     GwyToolSpotRemover *tool)
{
    tool->args.method = gwy_enum_combo_box_get_active(combo);
}

static void
gwy_tool_spot_remover_apply(GwyToolSpotRemover *tool)
{
    GwyPlainTool *plain_tool;
    gdouble sel[4];
    gint isel[4];

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->id >= 0 && plain_tool->data_field != NULL);
    g_return_if_fail(tool->args.method <= GWY_SPOT_REMOVE_FRACTAL);

    if (gwy_selection_get_object(plain_tool->selection, 0, sel)) {
        isel[0] = gwy_data_field_rtoj(plain_tool->data_field, sel[0]);
        isel[1] = gwy_data_field_rtoi(plain_tool->data_field, sel[1]);
        isel[2] = gwy_data_field_rtoj(plain_tool->data_field, sel[2]) + 1;
        isel[3] = gwy_data_field_rtoi(plain_tool->data_field, sel[3]) + 1;

        if (sel[0] > sel[2])
            GWY_SWAP(gdouble, sel[0], sel[2]);
        if (sel[1] > sel[3])
            GWY_SWAP(gdouble, sel[1], sel[3]);
    }
    else {
        isel[0] = isel[1] = 0;
        isel[2] = gwy_data_field_get_xres(plain_tool->data_field);
        isel[3] = gwy_data_field_get_yres(plain_tool->data_field);
    }

    gwy_app_undo_qcheckpoint(plain_tool->container,
                             gwy_app_get_data_key_for_id(plain_tool->id), 0);
    method_functions[tool->args.method](plain_tool->data_field,
                                        isel[0], isel[1], isel[2], isel[3]);
    gwy_data_field_data_changed(plain_tool->data_field);
}

static void
hyperbolic_average(GwyDataField *dfield,
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
    gwy_data_field_clear(mask);
    gwy_data_field_area_fill(mask, ximin, yimin, ximax - ximin, yimax - yimin,
                             1.0);

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
    gwy_data_field_clear(mask);
    gwy_data_field_area_fill(mask, ximin, yimin, ximax - ximin, yimax - yimin,
                             1.0);
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

                ww = disttable[ABS(k - i)*width + j-ximin+1];
                w += ww;
                s += ww*data[k*rowstride + ximin-1];

                ww = disttable[ABS(k - i)*width + ximax-j];
                w += ww;
                s += ww*data[k*rowstride + ximax];
            }

            for (k = ximin-1; k < ximax+1; k++) {
                gdouble ww;

                ww = disttable[ABS(i-yimin+1)*width + ABS(k - j)];
                w += ww;
                s += ww*data[(yimin-1)*rowstride + k];

                ww = disttable[ABS(yimax - i)*width + ABS(k - j)];
                w += ww;
                s += ww*data[yimax*rowstride + k];
            }

            data[i*rowstride + j] = s/w;
        }
    }

    g_free(disttable);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

