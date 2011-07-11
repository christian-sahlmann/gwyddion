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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/filters.h>
#include <libprocess/linestats.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_FILTER            (gwy_tool_filter_get_type())
#define GWY_TOOL_FILTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_FILTER, GwyToolFilter))
#define GWY_IS_TOOL_FILTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_FILTER))
#define GWY_TOOL_FILTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_FILTER, GwyToolFilterClass))

/* Keep numbering for settings */
typedef enum {
    GWY_FILTER_MEAN          = 0,
    GWY_FILTER_MEDIAN        = 1,
    GWY_FILTER_CONSERVATIVE  = 2,
    GWY_FILTER_MINIMUM       = 3,
    GWY_FILTER_MAXIMUM       = 4,
    GWY_FILTER_KUWAHARA      = 5,
    GWY_FILTER_DECHECKER     = 6,
    GWY_FILTER_GAUSSIAN      = 7
} GwyFilterType;

typedef struct _GwyToolFilter      GwyToolFilter;
typedef struct _GwyToolFilterClass GwyToolFilterClass;

typedef struct {
    GwyFilterType filter_type;
    gint size;
} ToolArgs;

struct _GwyToolFilter {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GwyRectSelectionLabels *rlabels;
    GtkWidget *filter_type;
    GtkObject *size;
    GtkWidget *apply;

    /* potential class data */
    GType layer_type_rect;
};

struct _GwyToolFilterClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType    gwy_tool_filter_get_type         (void) G_GNUC_CONST;
static void     gwy_tool_filter_finalize         (GObject *object);
static void     gwy_tool_filter_init_dialog      (GwyToolFilter *tool);
static void     gwy_tool_filter_data_switched    (GwyTool *gwytool,
                                                  GwyDataView *data_view);
static void     gwy_tool_filter_data_changed     (GwyPlainTool *plain_tool);
static void     gwy_tool_filter_response         (GwyTool *tool,
                                                  gint response_id);
static void     gwy_tool_filter_selection_changed(GwyPlainTool *plain_tool,
                                                  gint hint);
static void     gwy_tool_filter_size_changed     (GwyToolFilter *tool,
                                                  GtkAdjustment *adj);
static void     gwy_tool_filter_type_changed     (GtkComboBox *combo,
                                                  GwyToolFilter *tool);
static gboolean gwy_tool_filter_is_sized         (GwyFilterType type);
static void     gwy_tool_filter_apply            (GwyToolFilter *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Filter tool, processes selected part of data with a filter "
       "(conservative denoise, mean, median. Kuwahara, minimum, maximum)."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "3.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static const gchar filter_type_key[] = "/module/filter/filter_type";
static const gchar size_key[]        = "/module/filter/size";

static const ToolArgs default_args = {
    GWY_FILTER_MEAN,
    5,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolFilter, gwy_tool_filter, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_FILTER);

    return TRUE;
}

static void
gwy_tool_filter_class_init(GwyToolFilterClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_filter_finalize;

    tool_class->stock_id = GWY_STOCK_FILTER;
    tool_class->title = _("Filter");
    tool_class->tooltip = _("Basic filters: mean, median, denoise, …");
    tool_class->prefix = "/module/filter";
    tool_class->data_switched = gwy_tool_filter_data_switched;
    tool_class->response = gwy_tool_filter_response;

    ptool_class->data_changed = gwy_tool_filter_data_changed;
    ptool_class->selection_changed = gwy_tool_filter_selection_changed;
}

static void
gwy_tool_filter_finalize(GObject *object)
{
    GwyToolFilter *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_FILTER(object);

    settings = gwy_app_settings_get();
    gwy_container_set_enum_by_name(settings, filter_type_key,
                                   tool->args.filter_type);
    gwy_container_set_int32_by_name(settings, size_key,
                                    tool->args.size);

    G_OBJECT_CLASS(gwy_tool_filter_parent_class)->finalize(object);
}

static void
gwy_tool_filter_init(GwyToolFilter *tool)
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
    gwy_container_gis_enum_by_name(settings, filter_type_key,
                                   &tool->args.filter_type);
    gwy_container_gis_int32_by_name(settings, size_key,
                                    &tool->args.size);

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_rect,
                                     "rectangle");

    gwy_tool_filter_init_dialog(tool);
}

static void
gwy_tool_filter_rect_updated(GwyToolFilter *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_rect_selection_labels_select(tool->rlabels,
                                     plain_tool->selection,
                                     plain_tool->data_field);
}

static void
gwy_tool_filter_init_dialog(GwyToolFilter *tool)
{
    static const GwyEnum filters[] = {
        { N_("Mean value"),           GWY_FILTER_MEAN,         },
        { N_("Median value"),         GWY_FILTER_MEDIAN,       },
        { N_("Conservative denoise"), GWY_FILTER_CONSERVATIVE, },
        { N_("Minimum"),              GWY_FILTER_MINIMUM,      },
        { N_("Maximum"),              GWY_FILTER_MAXIMUM,      },
        { N_("Kuwahara"),             GWY_FILTER_KUWAHARA,     },
        { N_("Dechecker"),            GWY_FILTER_DECHECKER,    },
        { N_("filter|Gaussian"),      GWY_FILTER_GAUSSIAN,     },
    };
    GtkDialog *dialog;
    GtkTable *table;
    GtkWidget *label;
    gint row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    /* Selection info */
    tool->rlabels = gwy_rect_selection_labels_new
                         (TRUE, G_CALLBACK(gwy_tool_filter_rect_updated), tool);
    gtk_box_pack_start(GTK_BOX(dialog->vbox),
                       gwy_rect_selection_labels_get_table(tool->rlabels),
                       FALSE, FALSE, 0);

    /* Options */
    table = GTK_TABLE(gtk_table_new(4, 4, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 0);
    row = 0;

    label = gwy_label_new_header(_("Filter"));
    gtk_table_attach(table, label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    tool->filter_type = gwy_enum_combo_box_new
                                     (filters, G_N_ELEMENTS(filters),
                                      G_CALLBACK(gwy_tool_filter_type_changed),
                                      tool,
                                      tool->args.filter_type, TRUE);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Type:"), NULL,
                            GTK_OBJECT(tool->filter_type), GWY_HSCALE_WIDGET);
    row++;

    tool->size = gtk_adjustment_new(tool->args.size, 2, 20, 1, 5, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("Si_ze:"), "px",
                            tool->size, 0);
    gwy_table_hscale_set_sensitive
                 (tool->size, gwy_tool_filter_is_sized(tool->args.filter_type));
    g_signal_connect_swapped(tool->size, "value-changed",
                             G_CALLBACK(gwy_tool_filter_size_changed), tool);
    row++;

    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_filter_data_switched(GwyTool *gwytool,
                              GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolFilter *tool;
    gboolean ignore;

    plain_tool = GWY_PLAIN_TOOL(gwytool);
    ignore = (data_view == plain_tool->data_view);

    GWY_TOOL_CLASS(gwy_tool_filter_parent_class)->data_switched(gwytool,
                                                                data_view);

    if (ignore || plain_tool->init_failed)
        return;

    tool = GWY_TOOL_FILTER(gwytool);
    if (data_view) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_rect,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
    }

    gtk_widget_set_sensitive(tool->apply, data_view != NULL);
}

static void
gwy_tool_filter_data_changed(GwyPlainTool *plain_tool)
{
    gwy_rect_selection_labels_fill(GWY_TOOL_FILTER(plain_tool)->rlabels,
                                   plain_tool->selection,
                                   plain_tool->data_field,
                                   NULL, NULL);
    gwy_tool_filter_selection_changed(plain_tool, 0);
}

static void
gwy_tool_filter_response(GwyTool *tool,
                       gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_filter_parent_class)->response(tool, response_id);

    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_filter_apply(GWY_TOOL_FILTER(tool));
}

static void
gwy_tool_filter_selection_changed(GwyPlainTool *plain_tool,
                                gint hint)
{
    GwyToolFilter *tool;
    gint n = 0;

    tool = GWY_TOOL_FILTER(plain_tool);
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
}

static void
gwy_tool_filter_size_changed(GwyToolFilter *tool,
                             GtkAdjustment *adj)
{
    tool->args.size = gwy_adjustment_get_int(adj);
}

static void
gwy_tool_filter_type_changed(GtkComboBox *combo,
                             GwyToolFilter *tool)
{
    gboolean sensitive;

    tool->args.filter_type = gwy_enum_combo_box_get_active(combo);
    sensitive = gwy_tool_filter_is_sized(tool->args.filter_type);
    gwy_table_hscale_set_sensitive(tool->size, sensitive);
}

static gboolean
gwy_tool_filter_is_sized(GwyFilterType type)
{
    return (type != GWY_FILTER_KUWAHARA
            && type != GWY_FILTER_DECHECKER);
}

static void
gwy_tool_filter_apply(GwyToolFilter *tool)
{
    GwyPlainTool *plain_tool;
    gdouble sel[4];
    gint isel[4];

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->id >= 0 && plain_tool->data_field != NULL);

    if (gwy_selection_get_object(plain_tool->selection, 0, sel)) {
        isel[0] = floor(gwy_data_field_rtoj(plain_tool->data_field, sel[0]));
        isel[1] = floor(gwy_data_field_rtoi(plain_tool->data_field, sel[1]));
        isel[2] = floor(gwy_data_field_rtoj(plain_tool->data_field, sel[2]));
        isel[3] = floor(gwy_data_field_rtoi(plain_tool->data_field, sel[3]));

        if (sel[0] > sel[2])
            GWY_SWAP(gdouble, sel[0], sel[2]);
        if (sel[1] > sel[3])
            GWY_SWAP(gdouble, sel[1], sel[3]);
        isel[2] -= isel[0] + 1;
        isel[3] -= isel[1] + 1;
    }
    else {
        isel[0] = isel[1] = 0;
        isel[2] = gwy_data_field_get_xres(plain_tool->data_field);
        isel[3] = gwy_data_field_get_yres(plain_tool->data_field);
    }

    gwy_app_undo_qcheckpoint(plain_tool->container,
                             gwy_app_get_data_key_for_id(plain_tool->id), 0);

    switch (tool->args.filter_type) {
        case GWY_FILTER_MEAN:
        gwy_data_field_area_filter_mean(plain_tool->data_field,
                                        tool->args.size,
                                        isel[0], isel[1],
                                        isel[2], isel[3]);
        break;

        case GWY_FILTER_MEDIAN:
        gwy_data_field_area_filter_median(plain_tool->data_field,
                                          tool->args.size,
                                          isel[0], isel[1],
                                          isel[2], isel[3]);
        break;

        case GWY_FILTER_MINIMUM:
        gwy_data_field_area_filter_minimum(plain_tool->data_field,
                                           tool->args.size,
                                           isel[0], isel[1],
                                           isel[2], isel[3]);
        break;

        case GWY_FILTER_MAXIMUM:
        gwy_data_field_area_filter_maximum(plain_tool->data_field,
                                           tool->args.size,
                                           isel[0], isel[1],
                                           isel[2], isel[3]);
        break;

        case GWY_FILTER_CONSERVATIVE:
        gwy_data_field_area_filter_conservative(plain_tool->data_field,
                                                tool->args.size,
                                                isel[0], isel[1],
                                                isel[2], isel[3]);
        break;

        case GWY_FILTER_KUWAHARA:
        gwy_data_field_area_filter_kuwahara(plain_tool->data_field,
                                            isel[0], isel[1],
                                            isel[2], isel[3]);
        break;

        case GWY_FILTER_DECHECKER:
        gwy_data_field_area_filter_dechecker(plain_tool->data_field,
                                             isel[0], isel[1],
                                             isel[2], isel[3]);
        break;

        case GWY_FILTER_GAUSSIAN:
        gwy_data_field_area_filter_gaussian(plain_tool->data_field,
                                            tool->args.size/(2.0*sqrt(2*G_LN2)),
                                            isel[0], isel[1],
                                            isel[2], isel[3]);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    gwy_data_field_data_changed(plain_tool->data_field);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
