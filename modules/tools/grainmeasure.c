/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
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
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/grains.h>
#include <libprocess/fractals.h>
#include <libprocess/correct.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <app/gwyapp.h>

enum { MAX_STATS = 32 };

#define GWY_TYPE_TOOL_GRAIN_MEASURE            (gwy_tool_grain_measure_get_type())
#define GWY_TOOL_GRAIN_MEASURE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_GRAIN_MEASURE, GwyToolGrainMeasure))
#define GWY_IS_TOOL_GRAIN_MEASURE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_GRAIN_MEASURE))
#define GWY_TOOL_GRAIN_MEASURE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_GRAIN_MEASURE, GwyToolGrainMeasureClass))

typedef struct _GwyToolGrainMeasure      GwyToolGrainMeasure;
typedef struct _GwyToolGrainMeasureClass GwyToolGrainMeasureClass;

struct _GwyToolGrainMeasure {
    GwyPlainTool parent_instance;

    gint ngrains;
    gint *grains;
    gint gno;
    GtkWidget *value_labels[MAX_STATS];
    GArray *values[MAX_STATS];

    gboolean same_units;
    GwySIUnit *area_unit;
    GwySIUnit *volume_unit;

    /* potential class data */
    GType layer_type_point;
};

struct _GwyToolGrainMeasureClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_grain_measure_get_type        (void) G_GNUC_CONST;
static void gwy_tool_grain_measure_finalize         (GObject *object);
static void gwy_tool_grain_measure_init_dialog      (GwyToolGrainMeasure *tool);
static void gwy_tool_grain_measure_data_switched    (GwyTool *gwytool,
                                                     GwyDataView *data_view);
static void gwy_tool_grain_measure_update_units     (GwyToolGrainMeasure *tool);
static void gwy_tool_grain_measure_selection_changed(GwyPlainTool *plain_tool,
                                                     gint hint);
static void gwy_tool_grain_measure_recalculate      (GwyToolGrainMeasure *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Grain measurement tool, calculates characteristics of selected "
       "countinous parts of mask."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2007",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolGrainMeasure, gwy_tool_grain_measure, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_GRAIN_MEASURE);

    return TRUE;
}

static void
gwy_tool_grain_measure_class_init(GwyToolGrainMeasureClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_grain_measure_finalize;

    tool_class->stock_id = GWY_STOCK_GRAINS_MEASURE;
    tool_class->title = _("Grain Remove");
    tool_class->tooltip = _("Measure individual grains "
                            "(continuous parts of mask)");
    tool_class->prefix = "/module/grainmeasure";
    tool_class->data_switched = gwy_tool_grain_measure_data_switched;

    ptool_class->selection_changed = gwy_tool_grain_measure_selection_changed;
}

static void
gwy_tool_grain_measure_finalize(GObject *object)
{
    GwyToolGrainMeasure *tool;
    guint i;

    tool = GWY_TOOL_GRAIN_MEASURE(object);

    g_free(tool->grains);
    g_object_unref(tool->area_unit);
    g_object_unref(tool->volume_unit);
    for (i = 0; i < MAX_STATS; i++) {
        if (tool->values[i]) {
            g_array_free(tool->values[i], TRUE);
            tool->values[i] = NULL;
        }
    }

    G_OBJECT_CLASS(gwy_tool_grain_measure_parent_class)->finalize(object);
}

static void
gwy_tool_grain_measure_init(GwyToolGrainMeasure *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_point = gwy_plain_tool_check_layer_type(plain_tool,
                                                             "GwyLayerPoint");
    if (!tool->layer_type_point)
        return;

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_point,
                                     "pointer");

    tool->area_unit = gwy_si_unit_new(NULL);
    tool->volume_unit = gwy_si_unit_new(NULL);
    gwy_tool_grain_measure_init_dialog(tool);
}

static void
gwy_tool_grain_measure_init_dialog(GwyToolGrainMeasure *tool)
{
    GtkWidget *label;
    GtkDialog *dialog;
    GtkTable *table;
    gint row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    table = GTK_TABLE(gtk_table_new(2, 2, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table), TRUE, TRUE, 0);
    row = 0;

    label = gtk_label_new(_("Remove:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_grain_measure_data_switched(GwyTool *gwytool,
                                     GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolGrainMeasure *tool;

    tool = GWY_TOOL_GRAIN_MEASURE(gwytool);
    g_free(tool->grains);
    tool->grains = NULL;
    tool->ngrains = 0;

    GWY_TOOL_CLASS(gwy_tool_grain_measure_parent_class)->data_switched(gwytool,
                                                                    data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    if (data_view) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_point,
                                "draw-marker", FALSE,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
        gwy_tool_grain_measure_update_units(tool);
    }
}

static void
gwy_tool_grain_measure_update_units(GwyToolGrainMeasure *tool)
{
    GwyPlainTool *plain_tool;
    GwySIUnit *siunitxy, *siunitz;

    plain_tool = GWY_PLAIN_TOOL(tool);
    siunitxy = gwy_data_field_get_si_unit_xy(plain_tool->data_field);
    siunitz = gwy_data_field_get_si_unit_z(plain_tool->data_field);
    tool->same_units = gwy_si_unit_equal(siunitxy, siunitz);
    tool->area_unit = gwy_si_unit_power(siunitxy, 2, tool->area_unit);
    gwy_si_unit_multiply(tool->area_unit, siunitz, tool->volume_unit);
}

static void
gwy_tool_grain_measure_selection_changed(GwyPlainTool *plain_tool,
                                         gint hint)
{
    GwyToolGrainMeasure *tool;
    gdouble point[2];
    gint col, row, xres;

    g_return_if_fail(hint <= 0);

    tool = GWY_TOOL_GRAIN_MEASURE(plain_tool);
    tool->gno = 0;

    if (!plain_tool->mask_field
        || !gwy_selection_get_object(plain_tool->selection, 0, point))
        return;

    row = gwy_data_field_rtoi(plain_tool->mask_field, point[1]);
    col = gwy_data_field_rtoj(plain_tool->mask_field, point[0]);
    if (!gwy_data_field_get_val(plain_tool->mask_field, col, row))
        return;

    if (!tool->grains)
        gwy_tool_grain_measure_recalculate(tool);

    xres = gwy_data_field_get_xres(plain_tool->mask_field);
    tool->gno = tool->grains[row*xres + col];
}

static void
gwy_tool_grain_measure_recalculate(GwyToolGrainMeasure *tool)
{
    GwyPlainTool *plain_tool;
    GwyDataField *dfield, *mask;
    guint i;

    plain_tool = GWY_PLAIN_TOOL(tool);
    dfield = plain_tool->data_field;
    mask = plain_tool->mask_field;

    if (!tool->grains) {
        tool->grains = g_new0(gint,
                              gwy_data_field_get_xres(dfield)
                              *gwy_data_field_get_yres(dfield));
        tool->ngrains = gwy_data_field_number_grains(mask, tool->grains);
    }

    for (i = 0; i < MAX_STATS; i++) {
        if (!tool->value_labels[i])
            continue;

        if (!tool->values[i])
            tool->values[i] = g_array_new(FALSE, FALSE, sizeof(gdouble));
        g_array_set_size(tool->values[i], tool->ngrains + 1);
        gwy_data_field_grains_get_values(dfield,
                                         (gdouble*)tool->values[i]->data,
                                         tool->ngrains, tool->grains, i);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
