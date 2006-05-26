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
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwylayer-basic.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_COLOR_RANGE            (gwy_tool_color_range_get_type())
#define GWY_TOOL_COLOR_RANGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_COLOR_RANGE, GwyToolColorRange))
#define GWY_IS_TOOL_COLOR_RANGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_COLOR_RANGE))
#define GWY_TOOL_COLOR_RANGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_COLOR_RANGE, GwyToolColorRangeClass))

typedef enum {
    USE_SELECTION = 0,
    USE_HISTOGRAM
} ColorRangeSource;

typedef struct _GwyToolColorRange      GwyToolColorRange;
typedef struct _GwyToolColorRangeClass GwyToolColorRangeClass;

struct _GwyToolColorRange {
    GwyPlainTool parent_instance;

    GwyRectSelectionLabels *rlabels;

    GwyGraph *histogram;
    GwyGraphModel *histogram_model;
    GwyDataLine *heightdist;
    GwySelection *graph_selection;

    GtkLabel *min;
    GtkLabel *max;
    GtkLabel *datamin;
    GtkLabel *datamax;

    ColorRangeSource range_source;
    gboolean programmatic_update;
    GSList *modelist;

    GQuark key_min;
    GQuark key_max;

    /* potential class data */
    GType layer_type_rect;
};

struct _GwyToolColorRangeClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType  gwy_tool_color_range_get_type         (void) G_GNUC_CONST;
static void   gwy_tool_color_range_finalize         (GObject *object);
static void   gwy_tool_color_range_init_dialog      (GwyToolColorRange *tool);
static void   gwy_tool_color_range_data_switched    (GwyTool *gwytool,
                                                     GwyDataView *data_view);
static void   gwy_tool_color_range_data_changed     (GwyPlainTool *plain_tool);
static void   gwy_tool_color_range_selection_changed(GwyPlainTool *plain_tool,
                                                     gint hint);
static void   gwy_tool_color_range_xsel_changed     (GwySelection *selection,
                                                     gint hint,
                                                     GwyToolColorRange *tool);
static void   gwy_tool_color_range_type_changed     (GtkWidget *radio,
                                                     GwyToolColorRange *tool);
static GwyLayerBasicRangeType gwy_tool_color_range_get_range_type(GwyToolColorRange *tool);
static void   gwy_tool_color_range_set_range_type   (GwyToolColorRange *tool,
                                                     GwyLayerBasicRangeType range_type);
static void   gwy_tool_color_range_get_min_max      (GwyToolColorRange *tool,
                                                     gdouble *selection);
static void   gwy_tool_color_range_set_min_max      (GwyToolColorRange *tool);
static void   gwy_tool_color_range_update_fullrange (GwyToolColorRange *tool);
static void   gwy_tool_color_range_update_histogram (GwyToolColorRange *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Interactive color range tool, allows to select data range false "
       "color scale should map to, either on data or on height distribution "
       "histogram."),
    "Yeti <yeti@gwyddion.net>",
    "3.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolColorRange, gwy_tool_color_range, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_COLOR_RANGE);

    return TRUE;
}

static void
gwy_tool_color_range_class_init(GwyToolColorRangeClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_color_range_finalize;

    tool_class->stock_id = GWY_STOCK_COLOR_RANGE;
    tool_class->title = _("Color Range");
    tool_class->tooltip = _("Stretch color range to part of data");
    tool_class->prefix = "/module/colorrange";
    tool_class->data_switched = gwy_tool_color_range_data_switched;

    ptool_class->data_changed = gwy_tool_color_range_data_changed;
    ptool_class->selection_changed = gwy_tool_color_range_selection_changed;
}

static void
gwy_tool_color_range_finalize(GObject *object)
{
    GwyToolColorRange *tool;

    tool = GWY_TOOL_COLOR_RANGE(object);
    gwy_object_unref(tool->heightdist);

    G_OBJECT_CLASS(gwy_tool_color_range_parent_class)->finalize(object);
}

static void
gwy_tool_color_range_init(GwyToolColorRange *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_rect = gwy_plain_tool_check_layer_type(plain_tool,
                                                           "GwyLayerRectangle");
    if (!tool->layer_type_rect)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_VFMARKUP;

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_rect,
                                     "rectangle");

    gwy_tool_color_range_init_dialog(tool);
}

static void
gwy_tool_crop_rect_updated(GwyToolColorRange *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_rect_selection_labels_select(tool->rlabels,
                                     plain_tool->selection,
                                     plain_tool->data_field);
}

static void
gwy_tool_color_range_init_dialog(GwyToolColorRange *tool)
{
    static const GwyEnum range_types[] = {
        { N_("Full"),     GWY_LAYER_BASIC_RANGE_FULL,  },
        { N_("Fixed"),    GWY_LAYER_BASIC_RANGE_FIXED, },
        { N_("Auto"),     GWY_LAYER_BASIC_RANGE_AUTO,  },
        { N_("Adaptive"), GWY_LAYER_BASIC_RANGE_ADAPT, },
    };

    GtkWidget *label, *hbox, *button;
    GtkTable *table;
    GtkDialog *dialog;
    GwyGraphCurveModel *cmodel;
    GwyGraphArea *garea;
    GSList *modelist, *l;
    gint row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    /* Mode switch */
    hbox = gtk_hbox_new(TRUE, 0);
    modelist = gwy_radio_buttons_create
                          (range_types, G_N_ELEMENTS(range_types),
                           G_CALLBACK(gwy_tool_color_range_type_changed), tool,
                           GWY_LAYER_BASIC_RANGE_FULL);
    for (l = modelist; l; l = g_slist_next(l)) {
        button = GTK_WIDGET(l->data);
        gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);
    }
    tool->modelist = modelist;
    gtk_box_pack_start(GTK_BOX(dialog->vbox), hbox, FALSE, FALSE, 0);

    /* Height distribution */
    tool->heightdist = gwy_data_line_new(1.0, 1.0, TRUE);
    cmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_description(cmodel,
                                          _("Height histogram"));
    gwy_graph_curve_model_set_curve_type(cmodel, GWY_GRAPH_CURVE_LINE);

    tool->histogram_model = gwy_graph_model_new();
    gwy_graph_model_add_curve(tool->histogram_model, cmodel);
    tool->histogram = GWY_GRAPH(gwy_graph_new(tool->histogram_model));
    gwy_graph_set_status(tool->histogram, GWY_GRAPH_STATUS_XSEL);
    garea = GWY_GRAPH_AREA(gwy_graph_get_area(tool->histogram));
    tool->graph_selection = gwy_graph_area_get_selection(garea,
                                                         GWY_GRAPH_STATUS_XSEL);
    g_return_if_fail(GWY_IS_SELECTION_GRAPH_1DAREA(tool->graph_selection));
    gwy_selection_set_max_objects(tool->graph_selection, 1);
    g_signal_connect(tool->graph_selection, "changed",
                     G_CALLBACK(gwy_tool_color_range_xsel_changed), tool);

    gwy_graph_model_set_label_visible(tool->histogram_model, FALSE);
    gwy_graph_set_axis_visible(tool->histogram, GTK_POS_TOP, FALSE);
    gwy_graph_set_axis_visible(tool->histogram, GTK_POS_BOTTOM, FALSE);
    gwy_graph_set_axis_visible(tool->histogram, GTK_POS_LEFT, FALSE);
    gwy_graph_set_axis_visible(tool->histogram, GTK_POS_RIGHT, FALSE);
    gwy_graph_enable_user_input(tool->histogram, FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(tool->histogram), FALSE);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(tool->histogram),
                       TRUE, TRUE, 2);

    /* Data ranges */
    table = GTK_TABLE(gtk_table_new(2, 3, TRUE));
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 0);
    row = 0;

    tool->min = GTK_LABEL(gtk_label_new(NULL));
    gtk_misc_set_alignment(GTK_MISC(tool->min), 0.0, 0.5);
    gtk_table_attach(table, GTK_WIDGET(tool->min),
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    tool->max = GTK_LABEL(gtk_label_new(NULL));
    gtk_misc_set_alignment(GTK_MISC(tool->max), 1.0, 0.5);
    gtk_table_attach(table, GTK_WIDGET(tool->max),
                     3, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("Range"));
    gtk_table_attach(table, label, 1, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    tool->datamin = GTK_LABEL(gtk_label_new(NULL));
    gtk_misc_set_alignment(GTK_MISC(tool->datamin), 0.0, 0.5);
    gtk_table_attach(table, GTK_WIDGET(tool->datamin),
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    tool->datamax = GTK_LABEL(gtk_label_new(NULL));
    gtk_misc_set_alignment(GTK_MISC(tool->datamax), 1.0, 0.5);
    gtk_table_attach(table, GTK_WIDGET(tool->datamax),
                     3, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

    label = gtk_label_new(_("Full"));
    gtk_table_attach(table, label, 1, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    /* Selection info */
    tool->rlabels = gwy_rect_selection_labels_new
                         (TRUE, G_CALLBACK(gwy_tool_crop_rect_updated), tool);
    gtk_box_pack_start(GTK_BOX(dialog->vbox),
                       gwy_rect_selection_labels_get_table(tool->rlabels),
                       FALSE, FALSE, 0);

    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_color_range_data_switched(GwyTool *gwytool,
                                   GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolColorRange *tool;
    GwyLayerBasicRangeType range_type;
    gchar key[32];

    GWY_TOOL_CLASS(gwy_tool_color_range_parent_class)->data_switched(gwytool,
                                                                     data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_COLOR_RANGE(gwytool);
    if (data_view) {
        g_object_set(plain_tool->layer,
                     "draw-reflection", FALSE,
                     "is-crop", FALSE,
                     NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);

        g_snprintf(key, sizeof(key), "/%d/base/min", plain_tool->id);
        tool->key_min = g_quark_from_string(key);
        g_snprintf(key, sizeof(key), "/%d/base/max", plain_tool->id);
        tool->key_max = g_quark_from_string(key);
    }
    else {
        gtk_widget_set_sensitive(GTK_WIDGET(tool->histogram), FALSE);
        tool->key_min = tool->key_max = 0;
    }

    /* TODO: make sure we do update when switching to a FIXED data (even from
     * another FIXED) */
    tool = GWY_TOOL_COLOR_RANGE(gwytool);
    range_type = gwy_tool_color_range_get_range_type(tool);
    gwy_radio_buttons_set_current(tool->modelist, range_type);
    gwy_tool_color_range_update_histogram(tool);
    gwy_tool_color_range_update_fullrange(tool);
    gwy_tool_color_range_set_min_max(tool);
}

static void
gwy_tool_color_range_data_changed(GwyPlainTool *plain_tool)
{
    gwy_rect_selection_labels_fill(GWY_TOOL_COLOR_RANGE(plain_tool)->rlabels,
                                   plain_tool->selection,
                                   plain_tool->data_field,
                                   NULL, NULL);
    gwy_tool_color_range_update_histogram(GWY_TOOL_COLOR_RANGE(plain_tool));
    gwy_tool_color_range_selection_changed(plain_tool, 0);
}

static void
gwy_tool_color_range_selection_changed(GwyPlainTool *plain_tool,
                                       gint hint)
{
    GwyToolColorRange *tool;
    GwyLayerBasicRangeType range_type;
    gboolean is_selected = FALSE;
    gdouble range[2];

    tool = GWY_TOOL_COLOR_RANGE(plain_tool);
    g_return_if_fail(hint <= 0);

    if (plain_tool->selection) {
        is_selected = gwy_selection_get_data(plain_tool->selection, range);
        gwy_rect_selection_labels_fill(tool->rlabels,
                                       plain_tool->selection,
                                       plain_tool->data_field,
                                       NULL, NULL);
    }
    else
        gwy_rect_selection_labels_fill(tool->rlabels, NULL, NULL, NULL, NULL);

    range_type = gwy_tool_color_range_get_range_type(tool);
    if (range_type != GWY_LAYER_BASIC_RANGE_FIXED)
        return;

    if (!tool->programmatic_update)
        tool->range_source = USE_SELECTION;

    gwy_tool_color_range_set_min_max(tool);
    if (!tool->programmatic_update) {
        tool->programmatic_update = TRUE;
        if (is_selected) {
            gwy_tool_color_range_get_min_max(tool, range);
            gwy_selection_set_object(tool->graph_selection, 0, range);
        }
        else
            gwy_selection_clear(tool->graph_selection);
        tool->programmatic_update = FALSE;
    }
}

static void
gwy_tool_color_range_xsel_changed(GwySelection *selection,
                                  gint hint,
                                  GwyToolColorRange *tool)
{
    g_return_if_fail(hint <= 0);

    if (tool->programmatic_update)
        return;

    if (gwy_selection_get_data(selection, NULL)) {
        tool->range_source = USE_HISTOGRAM;
        gwy_tool_color_range_set_min_max(tool);
    }
    else {
        tool->range_source = USE_SELECTION;
        tool->programmatic_update = TRUE;
        gwy_tool_color_range_selection_changed(GWY_PLAIN_TOOL(tool), -1);
        tool->programmatic_update = FALSE;
    }
}

/* TODO: this is not enough, we need to restore range from container.
 * add USE_CONTAINER source type? */
static void
gwy_tool_color_range_type_changed(GtkWidget *radio,
                                  GwyToolColorRange *tool)
{
    GwyLayerBasicRangeType range_type, old_mode;
    GwyPlainTool *plain_tool;
    gboolean fixed;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (!plain_tool->container)
        return;

    old_mode = gwy_tool_color_range_get_range_type(tool);
    range_type = gwy_radio_button_get_value(radio);
    if (old_mode == range_type)
        return;

    fixed = (range_type == GWY_LAYER_BASIC_RANGE_FIXED);
    gtk_widget_set_sensitive(GTK_WIDGET(tool->histogram), fixed);

    gwy_tool_color_range_set_range_type(tool, range_type);
    if (fixed)
        gwy_tool_color_range_set_min_max(tool);
}

static GwyLayerBasicRangeType
gwy_tool_color_range_get_range_type(GwyToolColorRange *tool)
{
    GwyLayerBasicRangeType range_type = GWY_LAYER_BASIC_RANGE_FULL;
    GwyPlainTool *plain_tool;
    GwyPixmapLayer *layer;
    const gchar *key;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (!plain_tool->data_view)
        return range_type;

    layer = gwy_data_view_get_base_layer(plain_tool->data_view);
    key = gwy_layer_basic_get_range_type_key(GWY_LAYER_BASIC(layer));
    if (key)
        gwy_container_gis_enum_by_name(plain_tool->container, key, &range_type);

    return range_type;
}

static void
gwy_tool_color_range_set_range_type(GwyToolColorRange *tool,
                                    GwyLayerBasicRangeType range_type)
{
    GwyPlainTool *plain_tool;
    GwyPixmapLayer *layer;
    const gchar *key;
    gchar buf[32];

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->data_view);

    layer = gwy_data_view_get_base_layer(plain_tool->data_view);
    key = gwy_layer_basic_get_range_type_key(GWY_LAYER_BASIC(layer));
    if (!key) {
        g_warning("Setting range type key.  This should be done by the app.");

        g_snprintf(buf, sizeof(buf), "/%d/base", plain_tool->id);
        gwy_layer_basic_set_min_max_key(GWY_LAYER_BASIC(layer), buf);
        strncat(buf, "/range-type", sizeof(buf)-1);
        gwy_layer_basic_set_range_type_key(GWY_LAYER_BASIC(layer), buf);
        key = buf;
    }
    gwy_container_set_enum_by_name(plain_tool->container, key, range_type);
}

static void
gwy_tool_color_range_get_min_max(GwyToolColorRange *tool,
                                 gdouble *selection)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->data_view && plain_tool->data_field);

    selection[0] = gwy_data_field_get_min(plain_tool->data_field);
    gwy_container_gis_double(plain_tool->container,
                             tool->key_min, &selection[0]);

    selection[1] = gwy_data_field_get_max(plain_tool->data_field);
    gwy_container_gis_double(plain_tool->container,
                             tool->key_max, &selection[1]);
}

static void
gwy_tool_color_range_set_min_max(GwyToolColorRange *tool)
{
    GwyPlainTool *plain_tool;
    const GwySIValueFormat *vf;
    gboolean clear = FALSE;
    gchar buf[40];
    gdouble sel[4];
    gint isel[4];

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (!plain_tool->container) {
        gtk_label_set_text(tool->min, NULL);
        gtk_label_set_text(tool->max, NULL);
        return;
    }

    switch (tool->range_source) {
        case USE_SELECTION:
        if (!plain_tool->selection
            || !gwy_selection_get_object(plain_tool->selection, 0, sel)
            || sel[0] == sel[2] || sel[1] == sel[3]) {
            clear = TRUE;
            break;
        }
        isel[0] = gwy_data_field_rtoj(plain_tool->data_field, sel[0]);
        isel[1] = gwy_data_field_rtoi(plain_tool->data_field, sel[1]);
        isel[2] = gwy_data_field_rtoj(plain_tool->data_field, sel[2]) + 1;
        isel[3] = gwy_data_field_rtoi(plain_tool->data_field, sel[3]) + 1;

        gwy_data_field_area_get_min_max(plain_tool->data_field, NULL,
                                        MIN(isel[0], isel[2]),
                                        MIN(isel[1], isel[3]),
                                        ABS(isel[2] - isel[0]),
                                        ABS(isel[3] - isel[1]),
                                        &sel[0], &sel[1]);
        break;

        case USE_HISTOGRAM:
        if (!gwy_selection_get_object(tool->graph_selection, 0, sel)
            || sel[0] == sel[1])
            clear = TRUE;
        break;

        default:
        g_return_if_reached();
        break;
    }

    if (clear) {
        gwy_container_remove(plain_tool->container, tool->key_min);
        gwy_container_remove(plain_tool->container, tool->key_max);
        gwy_data_field_get_min_max(plain_tool->data_field, &sel[0], &sel[1]);
    }
    else {
        gwy_container_set_double(plain_tool->container, tool->key_min, sel[0]);
        gwy_container_set_double(plain_tool->container, tool->key_max, sel[1]);
    }

    vf = plain_tool->value_format;
    g_snprintf(buf, sizeof(buf), "%.*f%s%s",
               vf->precision, sel[0]/vf->magnitude,
               *vf->units ? " " : "", vf->units);
    gtk_label_set_markup(tool->min, buf);
    g_snprintf(buf, sizeof(buf), "%.*f%s%s",
               vf->precision, sel[1]/vf->magnitude,
               *vf->units ? " " : "", vf->units);
    gtk_label_set_markup(tool->max, buf);
}

static void
gwy_tool_color_range_update_fullrange(GwyToolColorRange *tool)
{
    GwyPlainTool *plain_tool;
    const GwySIValueFormat *vf;
    gdouble min, max;
    gchar buf[40];

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (!plain_tool->container) {
        gtk_label_set_text(tool->datamin, NULL);
        gtk_label_set_text(tool->datamax, NULL);
        return;
    }

    gwy_data_field_get_min_max(plain_tool->data_field, &min, &max);

    vf = plain_tool->value_format;
    g_snprintf(buf, sizeof(buf), "%.*f%s%s",
               vf->precision, min/vf->magnitude,
               *vf->units ? " " : "", vf->units);
    gtk_label_set_markup(tool->datamin, buf);
    g_snprintf(buf, sizeof(buf), "%.*f%s%s",
               vf->precision, max/vf->magnitude,
               *vf->units ? " " : "", vf->units);
    gtk_label_set_markup(tool->datamax, buf);
}

static void
gwy_tool_color_range_update_histogram(GwyToolColorRange *tool)
{
    GwyPlainTool *plain_tool;
    GwyGraphCurveModel *cmodel;

    plain_tool = GWY_PLAIN_TOOL(tool);
    cmodel = gwy_graph_model_get_curve(tool->histogram_model, 0);
    if (!plain_tool->data_field) {
        gdouble data[2] = { 0.0, 0.0 };

        gwy_graph_curve_model_set_data(cmodel, data, data, G_N_ELEMENTS(data));
        return;
    }

    gwy_data_field_dh(plain_tool->data_field, tool->heightdist, 0);
    gwy_graph_curve_model_set_data_from_dataline(cmodel, tool->heightdist,
                                                 0, 0);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

