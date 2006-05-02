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
#include <libprocess/datafield.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_STATS            (gwy_tool_stats_get_type())
#define GWY_TOOL_STATS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_STATS, GwyToolStats))
#define GWY_IS_TOOL_STATS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_STATS))
#define GWY_TOOL_STATS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_STATS, GwyToolStatsClass))

typedef struct _GwyToolStats      GwyToolStats;
typedef struct _GwyToolStatsClass GwyToolStatsClass;

struct _GwyToolStats {
    GwyPlainTool parent_instance;

    GwyRectSelectionLabels *rlabels;
    GtkWidget *apply;

    GtkWidget *ra;
    GtkWidget *rms;
    GtkWidget *skew;
    GtkWidget *kurtosis;
    GtkWidget *avg;
    GtkWidget *min;
    GtkWidget *max;
    GtkWidget *median;
    GtkWidget *projarea;
    GtkWidget *area;
    GtkWidget *theta;
    GtkWidget *phi;
    GwySIValueFormat *vform2;
    GwySIValueFormat *vformdeg;

    /* potential class data */
    GType layer_type_rect;

    gboolean same_units;
};

struct _GwyToolStatsClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType  gwy_tool_stats_get_type            (void) G_GNUC_CONST;
static void   gwy_tool_stats_finalize            (GObject *object);
static void   gwy_tool_stats_init_dialog         (GwyToolStats *tool);
static void   gwy_tool_stats_data_switched       (GwyTool *gwytool,
                                                 GwyDataView *data_view);
static void   gwy_tool_stats_data_changed        (GwyPlainTool *plain_tool);
static void   gwy_tool_stats_response            (GwyTool *tool,
                                                 gint response_id);
static void   gwy_tool_stats_selection_changed   (GwyPlainTool *plain_tool,
                                                 gint hint);
static void   gwy_tool_stats_apply               (GwyToolStats *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Statistics tool."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
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
    tool_class->title = _("Statistical quantities");
    tool_class->tooltip = _("Statistical quantities");
    tool_class->prefix = "/module/stats";
    tool_class->data_switched = gwy_tool_stats_data_switched;
    tool_class->response = gwy_tool_stats_response;

    ptool_class->data_changed = gwy_tool_stats_data_changed;
    ptool_class->selection_changed = gwy_tool_stats_selection_changed;
}

static void
gwy_tool_stats_finalize(GObject *object)
{

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
    GtkDialog *dialog;
    GtkWidget *table, *label, **plabel;
    gint i;
    static struct {
        const gchar *name;
        gsize offset;
    }
    const values[] = {
        { N_("Ra"),             G_STRUCT_OFFSET(GwyToolStats, ra)       },
        { N_("Rms"),            G_STRUCT_OFFSET(GwyToolStats, rms)      },
        { N_("Skew"),           G_STRUCT_OFFSET(GwyToolStats, skew)     },
        { N_("Kurtosis"),       G_STRUCT_OFFSET(GwyToolStats, kurtosis) },
        { N_("Average height"), G_STRUCT_OFFSET(GwyToolStats, avg)      },
        { N_("Minimum"),        G_STRUCT_OFFSET(GwyToolStats, min)      },
        { N_("Maximum"),        G_STRUCT_OFFSET(GwyToolStats, max)      },
        { N_("Median"),         G_STRUCT_OFFSET(GwyToolStats, median)   },
        { N_("Projected area"), G_STRUCT_OFFSET(GwyToolStats, projarea) },
        { N_("Area"),           G_STRUCT_OFFSET(GwyToolStats, area)     },
        { N_("Inclination theta"), G_STRUCT_OFFSET(GwyToolStats, theta) },
        { N_("Inclination phi"), G_STRUCT_OFFSET(GwyToolStats, phi) },
    };


    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    tool->rlabels = gwy_rect_selection_labels_new
                         (FALSE, G_CALLBACK(gwy_tool_stats_rect_updated), tool);
    gtk_box_pack_start(GTK_BOX(dialog->vbox),
                       gwy_rect_selection_labels_get_table(tool->rlabels),
                       TRUE, TRUE, 0);

    table = gtk_table_new(16, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Parameters</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    for (i = 0; i < G_N_ELEMENTS(values); i++) {
        label = gtk_label_new(_(values[i].name));
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);

        plabel = (GtkWidget**)G_STRUCT_MEMBER_P(tool, values[i].offset);
        *plabel = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(*plabel), 1.0, 0.5);
        gtk_label_set_selectable(GTK_LABEL(*plabel), TRUE);
        gtk_table_attach(GTK_TABLE(table), *plabel, 1, 3, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
    }


    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);

    gtk_widget_set_sensitive(tool->apply, TRUE);
    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_stats_data_switched(GwyTool *gwytool,
                            GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;

    GWY_TOOL_CLASS(gwy_tool_stats_parent_class)->data_switched(gwytool,
                                                              data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    if (data_view) {
        g_object_set(plain_tool->layer,
                     "draw-reflection", FALSE,
                     "is-stats", TRUE,
                     NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
    }
}

static void
gwy_tool_stats_data_changed(GwyPlainTool *plain_tool)
{
    gwy_rect_selection_labels_fill(GWY_TOOL_STATS(plain_tool)->rlabels,
                                   plain_tool->selection,
                                   plain_tool->data_field,
                                   NULL, NULL);
    gwy_tool_stats_selection_changed(plain_tool, 0);
}

static void
gwy_tool_stats_response(GwyTool *tool,
                       gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_stats_parent_class)->response(tool, response_id);

    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_stats_apply(GWY_TOOL_STATS(tool));
}

static void
gwy_tool_stats_selection_changed(GwyPlainTool *plain_tool,
                                gint hint)
{
    GwyToolStats *tool;
    gint n = 0, w, h;
    gdouble avg, ra, rms, skew, kurtosis, min, max, median, q;
    gdouble projarea, area = 0.0;
    gdouble theta, phi;    
    gdouble sel[4];
    gint isel[4];

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

   
    if (!gwy_selection_get_object(plain_tool->selection, 0, sel)) {
        isel[0] = 0;
        isel[1] = 0;
        isel[2] = gwy_data_field_get_xres(plain_tool->data_field);
        isel[3] = gwy_data_field_get_yres(plain_tool->data_field);
    }
    else {
        isel[0] = gwy_data_field_rtoj(plain_tool->data_field, sel[0]);
        isel[1] = gwy_data_field_rtoi(plain_tool->data_field, sel[1]);
        isel[2] = gwy_data_field_rtoj(plain_tool->data_field, sel[2]) + 1;
        isel[3] = gwy_data_field_rtoi(plain_tool->data_field, sel[3]) + 1;
    }
    w = isel[2] - isel[0];
    h = isel[3] - isel[1];
    gwy_data_field_area_get_stats(plain_tool->data_field, isel[0], isel[1], w, h,
                                  &avg, &ra, &rms, &skew, &kurtosis);
    gwy_data_field_area_get_min_max(plain_tool->data_field, isel[0], isel[1], w, h, &min, &max);
    median = gwy_data_field_area_get_median(plain_tool->data_field, isel[0], isel[1], w, h);
    q = gwy_data_field_get_xreal(plain_tool->data_field)/gwy_data_field_get_xres(plain_tool->data_field)
        *gwy_data_field_get_yreal(plain_tool->data_field)/gwy_data_field_get_yres(plain_tool->data_field);
    projarea = w*h*q;
    if (tool->same_units) {
        area = gwy_data_field_area_get_surface_area(plain_tool->data_field, isel[0], isel[1],
                                                    w, h);
        gwy_data_field_area_get_inclination(plain_tool->data_field, isel[0], isel[1], w, h,
                                            &theta, &phi);
    } 


    

}


static void
gwy_tool_stats_apply(GwyToolStats *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *container;
    GwyDataField *dfield;
    GQuark quarks[3];
    gdouble sel[4];
    gchar key[24];
    gint isel[4];
    gint id;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->id >= 0 && plain_tool->data_field != NULL);

    if (!gwy_selection_get_object(plain_tool->selection, 0, sel)) {
        g_warning("Apply invoked when no selection is present");
        return;
    }

    isel[0] = gwy_data_field_rtoj(plain_tool->data_field, sel[0]);
    isel[1] = gwy_data_field_rtoi(plain_tool->data_field, sel[1]);
    isel[2] = gwy_data_field_rtoj(plain_tool->data_field, sel[2]) + 1;
    isel[3] = gwy_data_field_rtoi(plain_tool->data_field, sel[3]) + 1;

}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
