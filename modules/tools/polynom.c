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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/level.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_POLYNOM            (gwy_tool_polynom_get_type())
#define GWY_TOOL_POLYNOM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_POLYNOM, GwyToolPolynom))
#define GWY_IS_TOOL_POLYNOM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_POLYNOM))
#define GWY_TOOL_POLYNOM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_POLYNOM, GwyToolPolynomClass))

typedef struct _GwyToolPolynom      GwyToolPolynom;
typedef struct _GwyToolPolynomClass GwyToolPolynomClass;

typedef struct {
    gint order;
    GwyOrientation direction;
    gboolean exclude;
} ToolArgs;

struct _GwyToolPolynom {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GwyRectSelectionLabels *rlabels;
    GtkWidget *order;
    GSList *direction;
    GtkWidget *exclude;
    GtkWidget *apply;

    /* potential class data */
    GType layer_type_rect;
};

struct _GwyToolPolynomClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType  gwy_tool_polynom_get_type         (void) G_GNUC_CONST;
static void   gwy_tool_polynom_finalize         (GObject *object);
static void   gwy_tool_polynom_init_dialog      (GwyToolPolynom *tool);
static void   gwy_tool_polynom_data_switched    (GwyTool *gwytool,
                                                 GwyDataView *data_view);
static void   gwy_tool_polynom_data_changed     (GwyPlainTool *plain_tool);
static void   gwy_tool_polynom_response         (GwyTool *tool,
                                                 gint response_id);
static void   gwy_tool_polynom_selection_changed(GwyPlainTool *plain_tool,
                                                 gint hint);
static void   gwy_tool_polynom_direction_changed(GObject *button,
                                                 GwyToolPolynom *tool);
static void   gwy_tool_polynom_exclude_changed  (GtkToggleButton *button,
                                                 GwyToolPolynom *tool);
static void   gwy_tool_polynom_apply            (GwyToolPolynom *tool);

static const gchar order_key[]     = "/module/polynom/order";
static const gchar direction_key[] = "/module/polynom/direction";
static const gchar exclude_key[]   = "/module/polynom/exclude";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Polynomial fit tool, fits polynomials to X or Y profiles and "
       "subtracts them."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static const ToolArgs default_args = {
    0,
    GWY_ORIENTATION_HORIZONTAL,
    FALSE,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolPolynom, gwy_tool_polynom, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_POLYNOM);

    return TRUE;
}

static void
gwy_tool_polynom_class_init(GwyToolPolynomClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_polynom_finalize;

    tool_class->stock_id = GWY_STOCK_POLYNOM;
    tool_class->title = _("Polynomial");
    tool_class->tooltip = _("Fit X or Y profiles with polynomials"),
    tool_class->prefix = "/module/polynom";
    tool_class->data_switched = gwy_tool_polynom_data_switched;
    tool_class->response = gwy_tool_polynom_response;

    ptool_class->data_changed = gwy_tool_polynom_data_changed;
    ptool_class->selection_changed = gwy_tool_polynom_selection_changed;
}

static void
gwy_tool_polynom_finalize(GObject *object)
{
    GwyToolPolynom *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_POLYNOM(object);

    settings = gwy_app_settings_get();
    gwy_container_set_int32_by_name(settings, order_key,
                                    tool->args.order);
    gwy_container_set_enum_by_name(settings, direction_key,
                                   tool->args.direction);
    gwy_container_set_boolean_by_name(settings, exclude_key,
                                      tool->args.exclude);

    G_OBJECT_CLASS(gwy_tool_polynom_parent_class)->finalize(object);
}

static void
gwy_tool_polynom_init(GwyToolPolynom *tool)
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
    gwy_container_gis_int32_by_name(settings, order_key,
                                    &tool->args.order);
    gwy_container_gis_enum_by_name(settings, direction_key,
                                   &tool->args.direction);
    gwy_container_gis_boolean_by_name(settings, exclude_key,
                                      &tool->args.exclude);

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_rect,
                                     "rectangle");

    gwy_tool_polynom_init_dialog(tool);
}

static void
gwy_tool_polynom_rect_updated(GwyToolPolynom *tool)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_rect_selection_labels_select(tool->rlabels,
                                     plain_tool->selection,
                                     plain_tool->data_field);
}

static void
gwy_tool_polynom_init_dialog(GwyToolPolynom *tool)
{
    static const GwyEnum orders[] = {
        { N_("Fit height"),    0, },
        { N_("Fit linear"),    1, },
        { N_("Fit quadratic"), 2, },
        { N_("Fit cubic"),     3, },
    };
    static const GwyEnum directions[] = {
        { N_("_Horizontal direction"), GWY_ORIENTATION_HORIZONTAL, },
        { N_("_Vertical direction"),   GWY_ORIENTATION_VERTICAL,   },
    };

    GtkDialog *dialog;
    GtkTable *table;
    GtkWidget *label;
    GSList *radio;
    guint row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    tool->rlabels = gwy_rect_selection_labels_new
                      (TRUE, G_CALLBACK(gwy_tool_polynom_rect_updated), tool);
    gtk_box_pack_start(GTK_BOX(dialog->vbox),
                       gwy_rect_selection_labels_get_table(tool->rlabels),
                       FALSE, FALSE, 0);

    table = GTK_TABLE(gtk_table_new(4, 2, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 0);
    row = 0;

    label = gwy_label_new_header(_("Fiting Mode"));
    gtk_table_attach(table, label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    tool->order
        = gwy_enum_combo_box_new(orders, G_N_ELEMENTS(orders),
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &tool->args.order,
                                 tool->args.order, TRUE);
    gwy_table_attach_row(GTK_WIDGET(table), row, _("_Type:"), NULL,
                         tool->order);
    row++;

    radio = gwy_radio_buttons_create
                    (directions, G_N_ELEMENTS(directions),
                     G_CALLBACK(gwy_tool_polynom_direction_changed), tool,
                     tool->args.direction);
    tool->direction = radio;
    while (radio) {
        gtk_table_attach(table, GTK_WIDGET(radio->data),
                         0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;
        radio = g_slist_next(radio);
    }
    gtk_table_set_row_spacing(table, row-1, 8);

    tool->exclude
        = gtk_check_button_new_with_mnemonic(_("_Exclude area if selected"));
    gtk_table_attach(table, tool->exclude,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->exclude),
                                 tool->args.exclude);
    g_signal_connect(tool->exclude, "toggled",
                     G_CALLBACK(gwy_tool_polynom_exclude_changed), tool);
    row++;

    label = gtk_label_new(_("(otherwise it will be used for fitting)"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_polynom_data_switched(GwyTool *gwytool,
                               GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolPolynom *tool;

    GWY_TOOL_CLASS(gwy_tool_polynom_parent_class)->data_switched(gwytool,
                                                                data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_POLYNOM(gwytool);
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
gwy_tool_polynom_data_changed(GwyPlainTool *plain_tool)
{
    gwy_rect_selection_labels_fill(GWY_TOOL_POLYNOM(plain_tool)->rlabels,
                                   plain_tool->selection,
                                   plain_tool->data_field,
                                   NULL, NULL);
    gwy_tool_polynom_selection_changed(plain_tool, 0);
}

static void
gwy_tool_polynom_response(GwyTool *tool,
                          gint response_id)
{
    GWY_TOOL_CLASS(gwy_tool_polynom_parent_class)->response(tool, response_id);

    if (response_id == GTK_RESPONSE_APPLY)
        gwy_tool_polynom_apply(GWY_TOOL_POLYNOM(tool));
}

static void
gwy_tool_polynom_selection_changed(GwyPlainTool *plain_tool,
                                   gint hint)
{
    GwyToolPolynom *tool;
    gint n = 0;

    tool = GWY_TOOL_POLYNOM(plain_tool);
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
gwy_tool_polynom_direction_changed(G_GNUC_UNUSED GObject *button,
                                   GwyToolPolynom *tool)
{
    tool->args.direction = gwy_radio_buttons_get_current(tool->direction);
}

static void
gwy_tool_polynom_exclude_changed(GtkToggleButton *button,
                                 GwyToolPolynom *tool)
{
    tool->args.exclude = gtk_toggle_button_get_active(button);
}

static void
gwy_tool_polynom_apply(GwyToolPolynom *tool)
{
    GwyPlainTool *plain_tool;
    gint isel[4];
    GQuark quark;

    plain_tool = GWY_PLAIN_TOOL(tool);

    gwy_rect_selection_labels_fill(tool->rlabels,
                                   plain_tool->selection,
                                   plain_tool->data_field,
                                   NULL, isel);
    quark = gwy_app_get_data_key_for_id(plain_tool->id);
    gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);
    gwy_data_field_fit_lines(plain_tool->data_field,
                             isel[0], isel[1],
                             isel[2] - isel[0], isel[3] - isel[1],
                             tool->args.order,
                             tool->args.exclude,
                             tool->args.direction);
    gwy_data_field_data_changed(plain_tool->data_field);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

