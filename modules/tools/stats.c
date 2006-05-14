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
#include <stdio.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/datafield.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_STATS            (gwy_tool_stats_get_type())
#define GWY_TOOL_STATS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_STATS, GwyToolStats))
#define GWY_IS_TOOL_STATS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_STATS))
#define GWY_TOOL_STATS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_STATS, GwyToolStatsClass))

enum {
    GWY_TOOL_STATS_RESPONSE_SAVE = 1024
};

typedef struct _GwyToolStats      GwyToolStats;
typedef struct _GwyToolStatsClass GwyToolStatsClass;

typedef struct {
    gboolean use_mask;
    gboolean instant_update;
} ToolArgs;

typedef struct {
    gdouble sel[4];
    gint isel[4];

    gdouble avg;
    gdouble min;
    gdouble max;
    gdouble median;
    gdouble ra;
    gdouble rms;
    gdouble skew;
    gdouble kurtosis;
    gdouble area;
    gdouble projarea;
    gdouble theta;
    gdouble phi;
} ToolResults;

struct _GwyToolStats {
    GwyPlainTool parent_instance;

    ToolArgs args;
    ToolResults results;
    gboolean results_up_to_date;

    GwyRectSelectionLabels *rlabels;
    GtkWidget *update;
    GtkWidget *save;

    GtkWidget *avg;
    GtkWidget *min;
    GtkWidget *max;
    GtkWidget *median;
    GtkWidget *ra;
    GtkWidget *rms;
    GtkWidget *skew;
    GtkWidget *kurtosis;
    GtkWidget *area;
    GtkWidget *projarea;
    GtkWidget *theta;
    GtkWidget *phi;

    GtkWidget *use_mask;
    GtkWidget *instant_update;

    GwySIValueFormat *area_format;

    /* potential class data */
    GwySIValueFormat *angle_format;
    GType layer_type_rect;

    gboolean same_units;
};

struct _GwyToolStatsClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_stats_get_type              (void) G_GNUC_CONST;
static void  gwy_tool_stats_finalize              (GObject *object);
static void  gwy_tool_stats_init_dialog           (GwyToolStats *tool);
static void  gwy_tool_stats_data_switched         (GwyTool *gwytool,
                                                   GwyDataView *data_view);
static void  gwy_tool_stats_data_changed          (GwyPlainTool *plain_tool);
static void  gwy_tool_stats_mask_changed          (GwyPlainTool *plain_tool);
static void  gwy_tool_stats_response              (GwyTool *tool,
                                                   gint response_id);
static void  gwy_tool_stats_selection_changed     (GwyPlainTool *plain_tool,
                                                   gint hint);
static void  gwy_tool_stats_update_labels         (GwyToolStats *tool);
static gboolean gwy_tool_stats_calculate          (GwyToolStats *tool);
static void  gwy_tool_stats_update_units          (GwyToolStats *tool);
static void  update_label                         (GwySIValueFormat *units,
                                                   GtkWidget *label,
                                                   gdouble value);
static void  gwy_tool_stats_use_mask_changed      (GtkToggleButton *toggle,
                                                   GwyToolStats *tool);
static void  gwy_tool_stats_instant_update_changed(GtkToggleButton *check,
                                                   GwyToolStats *tool);
static void  gwy_tool_stats_save                  (GwyToolStats *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Statistics tool."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static const gchar instant_update_key[] = "/module/stats/instant_update";
static const gchar use_mask_key[]       = "/module/stats/use_mask";

static const ToolArgs default_args = {
    FALSE,
    TRUE,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolStats, gwy_tool_stats, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_STATS);

    return TRUE;
}

static void
gwy_tool_stats_class_init(GwyToolStatsClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_stats_finalize;

    tool_class->stock_id = GWY_STOCK_STAT_QUANTITIES;
    tool_class->title = _("Statistical Quantities");
    tool_class->tooltip = _("Statistical quantities");
    tool_class->prefix = "/module/stats";
    tool_class->data_switched = gwy_tool_stats_data_switched;
    tool_class->response = gwy_tool_stats_response;

    ptool_class->data_changed = gwy_tool_stats_data_changed;
    ptool_class->mask_changed = gwy_tool_stats_mask_changed;
    ptool_class->selection_changed = gwy_tool_stats_selection_changed;
}

static void
gwy_tool_stats_finalize(GObject *object)
{
    GwyToolStats *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_STATS(object);

    settings = gwy_app_settings_get();
    gwy_container_set_boolean_by_name(settings, use_mask_key,
                                      tool->args.use_mask);
    gwy_container_set_boolean_by_name(settings, instant_update_key,
                                      tool->args.instant_update);

    if (tool->angle_format)
        gwy_si_unit_value_format_free(tool->angle_format);
    if (tool->area_format)
        gwy_si_unit_value_format_free(tool->area_format);

    G_OBJECT_CLASS(gwy_tool_stats_parent_class)->finalize(object);
}

static void
gwy_tool_stats_init(GwyToolStats *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_rect = gwy_plain_tool_check_layer_type(plain_tool,
                                                           "GwyLayerRectangle");
    if (!tool->layer_type_rect)
        return;

    plain_tool->lazy_updates = TRUE;
    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_MARKUP;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_boolean_by_name(settings, use_mask_key,
                                      &tool->args.use_mask);
    gwy_container_gis_boolean_by_name(settings, instant_update_key,
                                      &tool->args.instant_update);

    tool->angle_format = g_new0(GwySIValueFormat, 1);
    tool->angle_format->magnitude = 1.0;
    tool->angle_format->precision = 1;
    gwy_si_unit_value_format_set_units(tool->angle_format, "deg");

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_rect,
                                     "rectangle");

    gwy_tool_stats_init_dialog(tool);
}

static void
gwy_tool_stats_rect_updated(GwyToolStats *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_rect_selection_labels_select(tool->rlabels,
                                     plain_tool->selection,
                                     plain_tool->data_field);
}

static void
gwy_tool_stats_init_dialog(GwyToolStats *tool)
{
    static struct {
        const gchar *name;
        gsize offset;
    }
    const values[] = {
        { N_("Average value"),     G_STRUCT_OFFSET(GwyToolStats, avg),      },
        { N_("Minimum"),           G_STRUCT_OFFSET(GwyToolStats, min),      },
        { N_("Maximum"),           G_STRUCT_OFFSET(GwyToolStats, max),      },
        { N_("Median"),            G_STRUCT_OFFSET(GwyToolStats, median),   },
        { N_("Ra"),                G_STRUCT_OFFSET(GwyToolStats, ra),       },
        { N_("Rms"),               G_STRUCT_OFFSET(GwyToolStats, rms),      },
        { N_("Skew"),              G_STRUCT_OFFSET(GwyToolStats, skew),     },
        { N_("Kurtosis"),          G_STRUCT_OFFSET(GwyToolStats, kurtosis), },
        { N_("Surface area"),      G_STRUCT_OFFSET(GwyToolStats, area),     },
        { N_("Projected area"),    G_STRUCT_OFFSET(GwyToolStats, projarea), },
        { N_("Inclination theta"), G_STRUCT_OFFSET(GwyToolStats, theta),    },
        { N_("Inclination phi"),   G_STRUCT_OFFSET(GwyToolStats, phi),      },
    };
    GtkDialog *dialog;
    GtkWidget *hbox, *vbox, *image, *label, **plabel;
    GtkTable *table;
    gint i, row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    hbox = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), hbox, FALSE, FALSE, 0);

    /* Selection info */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    tool->rlabels = gwy_rect_selection_labels_new
                         (TRUE, G_CALLBACK(gwy_tool_stats_rect_updated), tool);
    gtk_box_pack_start(GTK_BOX(vbox),
                       gwy_rect_selection_labels_get_table(tool->rlabels),
                       FALSE, FALSE, 0);

    /* Options */
    table = GTK_TABLE(gtk_table_new(2, 4, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), FALSE, FALSE, 0);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    tool->use_mask = gtk_check_button_new_with_mnemonic(_("Use _mask"));
    gtk_table_attach(table, tool->use_mask, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->use_mask),
                                 tool->args.use_mask);
    g_signal_connect(tool->use_mask, "toggled",
                     G_CALLBACK(gwy_tool_stats_use_mask_changed), tool);
    row++;

    tool->instant_update
        = gtk_check_button_new_with_mnemonic(_("_Instant updates"));
    gtk_table_attach(table, tool->instant_update, 0, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->instant_update),
                                 tool->args.instant_update);
    g_signal_connect(tool->instant_update, "toggled",
                     G_CALLBACK(gwy_tool_stats_instant_update_changed), tool);
    row++;

    /* Parameters */
    table = GTK_TABLE(gtk_table_new(16, 2, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(table), TRUE, TRUE, 0);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Parameters</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    for (i = 0; i < G_N_ELEMENTS(values); i++) {
        label = gtk_label_new(_(values[i].name));
        gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(table, label, 0, 1, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);

        plabel = (GtkWidget**)G_STRUCT_MEMBER_P(tool, values[i].offset);
        *plabel = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(*plabel), 1.0, 0.5);
        gtk_label_set_single_line_mode(GTK_LABEL(*plabel), TRUE);
        gtk_label_set_selectable(GTK_LABEL(*plabel), TRUE);
        gtk_table_attach(table, *plabel, 1, 2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);

        row++;
    }

    tool->update = gtk_dialog_add_button(dialog, _("_Update"),
                                         GWY_TOOL_RESPONSE_UPDATE);
    image = gtk_image_new_from_stock(GTK_STOCK_EXECUTE, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(tool->update), image);
    tool->save = gtk_dialog_add_button(dialog, GTK_STOCK_SAVE,
                                       GWY_TOOL_STATS_RESPONSE_SAVE);
    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);

    gtk_widget_set_sensitive(tool->update, !tool->args.instant_update);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_stats_data_switched(GwyTool *gwytool,
                             GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolStats *tool;

    tool = GWY_TOOL_STATS(gwytool);
    if (tool->area_format)
        gwy_si_unit_value_format_free(tool->area_format);
    tool->area_format = NULL;

    GWY_TOOL_CLASS(gwy_tool_stats_parent_class)->data_switched(gwytool,
                                                               data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    if (data_view) {
        g_object_set(plain_tool->layer,
                     "draw-reflection", FALSE,
                     "is-crop", FALSE,
                     NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
    }

    gtk_widget_set_sensitive(tool->save, data_view != NULL);
}

static void
gwy_tool_stats_update_units(GwyToolStats *tool)
{
    GwyPlainTool *plain_tool;
    GwySIUnit *siunitxy, *siunitz, *siunitarea;
    gdouble xreal, yreal, q;

    plain_tool = GWY_PLAIN_TOOL(tool);

    siunitxy = gwy_data_field_get_si_unit_xy(plain_tool->data_field);
    siunitz = gwy_data_field_get_si_unit_z(plain_tool->data_field);
    tool->same_units = gwy_si_unit_equal(siunitxy, siunitz);

    xreal = gwy_data_field_get_xreal(plain_tool->data_field);
    yreal = gwy_data_field_get_xreal(plain_tool->data_field);
    q = xreal/gwy_data_field_get_xres(plain_tool->data_field)
        *yreal/gwy_data_field_get_yres(plain_tool->data_field);

    siunitarea = gwy_si_unit_power(siunitxy, 2, NULL);
    tool->area_format = gwy_si_unit_get_format_with_resolution
        (siunitarea, GWY_SI_UNIT_FORMAT_VFMARKUP,
         xreal*yreal, q, tool->area_format);
    g_object_unref(siunitarea);
}

static void
gwy_tool_stats_data_changed(GwyPlainTool *plain_tool)
{

    gwy_rect_selection_labels_fill(GWY_TOOL_STATS(plain_tool)->rlabels,
                                   plain_tool->selection,
                                   plain_tool->data_field,
                                   NULL, NULL);
    gwy_tool_stats_update_labels(GWY_TOOL_STATS(plain_tool));
}

static void
gwy_tool_stats_mask_changed(GwyPlainTool *plain_tool)
{
    if (GWY_TOOL_STATS(plain_tool)->args.use_mask)
        gwy_tool_stats_update_labels(GWY_TOOL_STATS(plain_tool));
}

static void
gwy_tool_stats_response(GwyTool *tool,
                        gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_stats_parent_class)->response(tool, response_id);

    if (response_id == GWY_TOOL_STATS_RESPONSE_SAVE)
        gwy_tool_stats_save(GWY_TOOL_STATS(tool));
    else if (response_id == GWY_TOOL_RESPONSE_UPDATE)
        gwy_tool_stats_update_labels(GWY_TOOL_STATS(tool));
}

static void
gwy_tool_stats_selection_changed(GwyPlainTool *plain_tool,
                                 gint hint)
{
    GwyToolStats *tool;
    gint n;

    tool = GWY_TOOL_STATS(plain_tool);
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

    tool->results_up_to_date = FALSE;
    if (tool->args.instant_update)
        gwy_tool_stats_update_labels(tool);
}

static void
gwy_tool_stats_update_labels(GwyToolStats *tool)
{
    GwyPlainTool *plain_tool;
    gboolean mask_in_use;
    gchar buffer[64];

    plain_tool = GWY_PLAIN_TOOL(tool);

    if (!plain_tool->data_field) {
        gtk_label_set_text(GTK_LABEL(tool->ra), "");
        gtk_label_set_text(GTK_LABEL(tool->rms), "");
        gtk_label_set_text(GTK_LABEL(tool->skew), "");
        gtk_label_set_text(GTK_LABEL(tool->kurtosis), "");
        gtk_label_set_text(GTK_LABEL(tool->avg), "");
        gtk_label_set_text(GTK_LABEL(tool->min), "");
        gtk_label_set_text(GTK_LABEL(tool->max), "");
        gtk_label_set_text(GTK_LABEL(tool->median), "");
        gtk_label_set_text(GTK_LABEL(tool->area), "");
        gtk_label_set_text(GTK_LABEL(tool->projarea), "");
        gtk_label_set_text(GTK_LABEL(tool->theta), "");
        gtk_label_set_text(GTK_LABEL(tool->phi), "");
        return;
    }

    if (!tool->area_format)
        gwy_tool_stats_update_units(tool);

    mask_in_use = gwy_tool_stats_calculate(tool);
    if (!tool->results_up_to_date)
        return;

    update_label(plain_tool->value_format, tool->ra, tool->results.ra);
    update_label(plain_tool->value_format, tool->rms, tool->results.rms);
    g_snprintf(buffer, sizeof(buffer), "%2.3g", tool->results.skew);
    gtk_label_set_text(GTK_LABEL(tool->skew), buffer);
    g_snprintf(buffer, sizeof(buffer), "%2.3g", tool->results.kurtosis);
    gtk_label_set_text(GTK_LABEL(tool->kurtosis), buffer);
    update_label(plain_tool->value_format, tool->avg, tool->results.avg);
    update_label(plain_tool->value_format, tool->min, tool->results.min);
    update_label(plain_tool->value_format, tool->max, tool->results.max);
    update_label(plain_tool->value_format, tool->median, tool->results.median);

    update_label(tool->area_format, tool->projarea, tool->results.projarea);
    if (tool->same_units && !mask_in_use) {
        update_label(tool->area_format, tool->area, tool->results.area);
        update_label(tool->angle_format, tool->theta,
                     180.0/G_PI * tool->results.theta);
        update_label(tool->angle_format, tool->phi,
                     180.0/G_PI * tool->results.phi);
    }
    else {
        gtk_label_set_text(GTK_LABEL(tool->area), _("N.A."));
        gtk_label_set_text(GTK_LABEL(tool->theta), _("N.A."));
        gtk_label_set_text(GTK_LABEL(tool->phi), _("N.A."));
    }
}

static gboolean
gwy_tool_stats_calculate(GwyToolStats *tool)
{
    GwyPlainTool *plain_tool;
    GwyDataField *mask;
    gdouble q;
    gint nn, w, h;
    gdouble sel[4];
    gint isel[4];

    plain_tool = GWY_PLAIN_TOOL(tool);

    tool->results_up_to_date = FALSE;
    if (!gwy_selection_get_object(plain_tool->selection, 0, sel)
        || sel[0] == sel[2] || sel[1] == sel[3]) {
        isel[0] = isel[1] = 0;
        isel[2] = gwy_data_field_get_xres(plain_tool->data_field);
        isel[3] = gwy_data_field_get_yres(plain_tool->data_field);
        sel[0] = sel[1] = 0.0;
        sel[2] = gwy_data_field_get_xreal(plain_tool->data_field);
        sel[3] = gwy_data_field_get_yreal(plain_tool->data_field);
    }
    else {
        isel[0] = gwy_data_field_rtoj(plain_tool->data_field, sel[0]);
        isel[1] = gwy_data_field_rtoi(plain_tool->data_field, sel[1]);
        isel[2] = gwy_data_field_rtoj(plain_tool->data_field, sel[2]) + 1;
        isel[3] = gwy_data_field_rtoi(plain_tool->data_field, sel[3]) + 1;
    }
    w = ABS(isel[2] - isel[0]);
    h = ABS(isel[3] - isel[1]);
    isel[0] = MIN(isel[0], isel[2]);
    isel[1] = MIN(isel[1], isel[3]);

    if (!w || !h)
        return FALSE;

    if (tool->args.use_mask && plain_tool->mask_field)
        mask = plain_tool->mask_field;
    else
        mask = NULL;

    q = gwy_data_field_get_xmeasure(plain_tool->data_field)
        * gwy_data_field_get_ymeasure(plain_tool->data_field);
    if (mask) {
        gwy_data_field_area_count_in_range(mask, NULL,
                                           isel[0], isel[1], w, h,
                                           0.0, 0.0, &nn, NULL);
        nn = w*h - nn;
    }
    else
        nn = w*h;
    tool->results.projarea = nn*q;
    /* TODO: do something more reasonable when nn == 0 */

    gwy_data_field_area_get_stats(plain_tool->data_field, mask,
                                  isel[0], isel[1], w, h,
                                  &tool->results.avg,
                                  &tool->results.ra,
                                  &tool->results.rms,
                                  &tool->results.skew,
                                  &tool->results.kurtosis);
    gwy_data_field_area_get_min_max(plain_tool->data_field, mask,
                                    isel[0], isel[1], w, h,
                                    &tool->results.min,
                                    &tool->results.max);
    tool->results.median
        = gwy_data_field_area_get_median(plain_tool->data_field, mask,
                                         isel[0], isel[1], w, h);

    if (tool->same_units && !mask) {
        tool->results.area
            = gwy_data_field_area_get_surface_area(plain_tool->data_field,
                                                   isel[0], isel[1], w, h);
        gwy_data_field_area_get_inclination(plain_tool->data_field,
                                            isel[0], isel[1], w, h,
                                            &tool->results.theta,
                                            &tool->results.phi);
    }

    memcpy(tool->results.isel, isel, sizeof(isel));
    memcpy(tool->results.sel, sel, sizeof(sel));
    sel[2] += gwy_data_field_get_xoffset(plain_tool->data_field);
    sel[3] += gwy_data_field_get_yoffset(plain_tool->data_field);
    tool->results_up_to_date = TRUE;

    return mask != NULL;
}

static void
update_label(GwySIValueFormat *units,
             GtkWidget *label,
             gdouble value)
{
    static gchar buffer[64];

    g_return_if_fail(units);
    g_return_if_fail(GTK_IS_LABEL(label));

    g_snprintf(buffer, sizeof(buffer), "%.*f%s%s",
               units->precision, value/units->magnitude,
               *units->units ? " " : "", units->units);
    gtk_label_set_markup(GTK_LABEL(label), buffer);
}

static void
gwy_tool_stats_use_mask_changed(GtkToggleButton *toggle,
                                GwyToolStats *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->args.use_mask = gtk_toggle_button_get_active(toggle);
    if (plain_tool->data_field)
        gwy_tool_stats_selection_changed(plain_tool, 0);
}

static void
gwy_tool_stats_instant_update_changed(GtkToggleButton *check,
                                      GwyToolStats *tool)
{
    tool->args.instant_update = gtk_toggle_button_get_active(check);
    gtk_widget_set_sensitive(tool->update, !tool->args.instant_update);
    if (tool->args.instant_update)
        gwy_tool_stats_selection_changed(GWY_PLAIN_TOOL(tool), -1);
}

#define fmt_val(v) \
    g_strdup_printf("%.*f%s%s", \
                    vf->precision, res->v/vf->magnitude, \
                    *vf->units ? " " : "", vf->units)

static void
gwy_tool_stats_save(GwyToolStats *tool)
{
    GtkWidget *dialog;
    GwySIUnit *siunitxy, *siunitarea;
    gdouble xreal, yreal, q;
    GwyPlainTool *plain_tool;
    GwySIValueFormat *vf = NULL;
    const guchar *title;
    gboolean mask_in_use;
    gint response;
    const ToolResults *res;
    gchar *key, *filename;
    gchar *ix, *iy, *iw, *ih, *rx, *ry, *rw, *rh, *muse, *uni;
    gchar *avg, *min, *max, *median, *rms, *ra, *skew, *kurtosis;
    gchar *area, *projarea, *theta, *phi;
    FILE *fh;

    dialog = gtk_file_chooser_dialog_new(_("Save Statistical Quantities"),
                                         GTK_WINDOW(GWY_TOOL(tool)->dialog),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        gwy_app_get_current_directory());
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    gtk_widget_destroy(dialog);

    if (!filename || response != GTK_RESPONSE_OK) {
        g_free(filename);
        return;
    }

    fh = g_fopen(filename, "w");
    if (!fh) {
        /* TODO: make noise */
        return;
    }

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->container);
    if (!tool->results_up_to_date)
        gwy_tool_stats_update_labels(tool);

    mask_in_use = tool->args.use_mask && plain_tool->mask_field;
    res = &tool->results;

    fputs(_("Statistical Quantities"), fh);
    fputs("\n\n", fh);

    /* Channel information */
    if (gwy_container_gis_string_by_name(plain_tool->container, "/filename",
                                         &title))
        fprintf(fh, _("File:              %s\n"), title);

    key = g_strdup_printf("/%d/data/title", plain_tool->id);
    if (gwy_container_gis_string_by_name(plain_tool->container, key, &title))
        fprintf(fh, _("Data channel:      %s\n"), title);
    g_free(key);

    fputs("\n", fh);

    iw = g_strdup_printf("%d", res->isel[2]);
    ih = g_strdup_printf("%d", res->isel[3]);
    ix = g_strdup_printf("%d", res->isel[0]);
    iy = g_strdup_printf("%d", res->isel[1]);

    vf = gwy_data_field_get_value_format_xy(plain_tool->data_field,
                                            GWY_SI_UNIT_FORMAT_PLAIN, vf);
    rw = g_strdup_printf("%.*f", vf->precision, res->sel[2]/vf->magnitude);
    rh = g_strdup_printf("%.*f", vf->precision, res->sel[3]/vf->magnitude);
    rx = g_strdup_printf("%.*f", vf->precision, res->sel[0]/vf->magnitude);
    ry = g_strdup_printf("%.*f", vf->precision, res->sel[1]/vf->magnitude);
    uni = g_strdup(vf->units);

    muse = g_strdup(mask_in_use ? _("Yes") : _("No"));

    vf = gwy_data_field_get_value_format_z(plain_tool->data_field,
                                           GWY_SI_UNIT_FORMAT_PLAIN, vf);
    avg = fmt_val(avg);
    min = fmt_val(min);
    max = fmt_val(max);
    median = fmt_val(median);
    ra = fmt_val(ra);
    rms = fmt_val(rms);

    skew = g_strdup_printf("%2.3g", res->skew);
    kurtosis = g_strdup_printf("%2.3g", res->kurtosis);

    siunitxy = gwy_data_field_get_si_unit_xy(plain_tool->data_field);
    siunitarea = gwy_si_unit_power(siunitxy, 2, NULL);
    xreal = gwy_data_field_get_xreal(plain_tool->data_field);
    yreal = gwy_data_field_get_xreal(plain_tool->data_field);
    q = xreal/gwy_data_field_get_xres(plain_tool->data_field)
        *yreal/gwy_data_field_get_yres(plain_tool->data_field);
    vf = gwy_si_unit_get_format_with_resolution(siunitarea,
                                                GWY_SI_UNIT_FORMAT_PLAIN,
                                                xreal*yreal, q, vf);
    g_object_unref(siunitarea);

    area = ((tool->same_units && !mask_in_use)
            ? fmt_val(area) : g_strdup(_("N.A")));
    projarea = fmt_val(projarea);

    gwy_si_unit_value_format_free(vf);
    vf = tool->angle_format;

    theta = ((tool->same_units && !mask_in_use)
             ? fmt_val(theta) : g_strdup(_("N.A")));
    phi = ((tool->same_units && !mask_in_use)
           ? fmt_val(phi) : g_strdup(_("N.A")));

    fprintf(fh,
            _("Selected area:     %s × %s at (%s, %s) px\n"
              "                   %s × %s at (%s, %s) %s\n"
              "Mask in use:       %s\n"
              "\n"
              "Average value:     %s\n"
              "Minimum:           %s\n"
              "Maximum:           %s\n"
              "Median:            %s\n"
              "Ra:                %s\n"
              "Rms:               %s\n"
              "Skew:              %s\n"
              "Kurtosis:          %s\n"
              "Surface area:      %s\n"
              "Projected area:    %s\n"
              "Inclination theta: %s\n"
              "Inclination phi:   %s\n"),
            iw, ih, ix, iy,
            rw, rh, rx, ry, uni,
            muse,
            avg, min, max, median, ra, rms, skew, kurtosis,
            area, projarea, theta, phi);

    fclose(fh);

    g_free(ix);
    g_free(iy);
    g_free(iw);
    g_free(ih);
    g_free(rx);
    g_free(ry);
    g_free(rw);
    g_free(rh);
    g_free(avg);
    g_free(min);
    g_free(max);
    g_free(median);
    g_free(ra);
    g_free(rms);
    g_free(skew);
    g_free(kurtosis);
    g_free(area);
    g_free(projarea);
    g_free(theta);
    g_free(phi);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
