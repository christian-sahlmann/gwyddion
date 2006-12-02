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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/elliptic.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

enum { RADIUS_MAX = 16 };

#define GWY_TYPE_TOOL_READ_VALUE            (gwy_tool_read_value_get_type())
#define GWY_TOOL_READ_VALUE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_READ_VALUE, GwyToolReadValue))
#define GWY_IS_TOOL_READ_VALUE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_READ_VALUE))
#define GWY_TOOL_READ_VALUE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_READ_VALUE, GwyToolReadValueClass))

typedef struct _GwyToolReadValue      GwyToolReadValue;
typedef struct _GwyToolReadValueClass GwyToolReadValueClass;

typedef struct {
    gint radius;
} ToolArgs;

struct _GwyToolReadValue {
    GwyPlainTool parent_instance;

    ToolArgs args;

    gdouble avg;
    gdouble bx;
    gdouble by;

    gdouble *values;
    gint *xpos;
    gint *ypos;

    GtkLabel *x;
    GtkLabel *y;
    GtkLabel *xunits;
    GtkLabel *yunits;
    GtkLabel *val;
    GtkLabel *zunits;
    GtkObject *radius;

    /* potential class data */
    GType layer_type_point;
};

struct _GwyToolReadValueClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType  gwy_tool_read_value_get_type         (void) G_GNUC_CONST;
static void   gwy_tool_read_value_finalize         (GObject *object);
static void   gwy_tool_read_value_init_dialog      (GwyToolReadValue *tool);
static void   gwy_tool_read_value_data_switched    (GwyTool *gwytool,
                                                    GwyDataView *data_view);
static void   gwy_tool_read_value_data_changed     (GwyPlainTool *plain_tool);
static void   gwy_tool_read_value_selection_changed(GwyPlainTool *plain_tool,
                                                    gint hint);
static void   gwy_tool_read_value_radius_changed   (GwyToolReadValue *tool);
static void   gwy_tool_read_value_update_headers   (GwyToolReadValue *tool);
static void   gwy_tool_read_value_update_values    (GwyToolReadValue *tool);
static void   gwy_tool_read_value_calculate        (GwyToolReadValue *tool,
                                                    const gdouble *point);

static const gchar radius_key[] = "/module/readvalue/radius";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Pointer tool, reads value under pointer."),
    "Yeti <yeti@gwyddion.net>",
    "2.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static const ToolArgs default_args = {
    1,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolReadValue, gwy_tool_read_value, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_READ_VALUE);

    return TRUE;
}

static void
gwy_tool_read_value_class_init(GwyToolReadValueClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_read_value_finalize;

    tool_class->stock_id = GWY_STOCK_POINTER_MEASURE;
    tool_class->title = _("Read Value");
    tool_class->tooltip = _("Read value under mouse cursor");
    tool_class->prefix = "/module/readvalue";
    tool_class->data_switched = gwy_tool_read_value_data_switched;

    ptool_class->data_changed = gwy_tool_read_value_data_changed;
    ptool_class->selection_changed = gwy_tool_read_value_selection_changed;
}

static void
gwy_tool_read_value_finalize(GObject *object)
{
    GwyToolReadValue *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_READ_VALUE(object);

    g_free(tool->values);
    g_free(tool->xpos);
    g_free(tool->ypos);

    settings = gwy_app_settings_get();
    gwy_container_set_int32_by_name(settings, radius_key, tool->args.radius);

    G_OBJECT_CLASS(gwy_tool_read_value_parent_class)->finalize(object);
}

static void
gwy_tool_read_value_init(GwyToolReadValue *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_point = gwy_plain_tool_check_layer_type(plain_tool,
                                                             "GwyLayerPoint");
    if (!tool->layer_type_point)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_VFMARKUP;
    plain_tool->lazy_updates = TRUE;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_int32_by_name(settings, radius_key, &tool->args.radius);

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_point,
                                     "pointer");

    gwy_tool_read_value_init_dialog(tool);
}

static void
gwy_tool_read_value_init_dialog(GwyToolReadValue *tool)
{
    GtkDialog *dialog;
    GtkTable *table;
    GtkWidget *table2;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    table = GTK_TABLE(gtk_table_new(2, 3, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table), TRUE, TRUE, 0);

    tool->xunits = GTK_LABEL(gtk_label_new(NULL));
    gtk_table_attach(table, GTK_WIDGET(tool->xunits),
                     0, 1, 0, 1, GTK_FILL, 0, 0, 0);

    tool->yunits = GTK_LABEL(gtk_label_new(NULL));
    gtk_table_attach(table, GTK_WIDGET(tool->yunits),
                     1, 2, 0, 1, GTK_FILL, 0, 0, 0);

    tool->zunits = GTK_LABEL(gtk_label_new(NULL));
    gtk_table_attach(table, GTK_WIDGET(tool->zunits),
                     2, 3, 0, 1, GTK_FILL, 0, 0, 0);

    tool->x = GTK_LABEL(gtk_label_new(NULL));
    gtk_misc_set_alignment(GTK_MISC(tool->x), 1.0, 0.5);
    gtk_table_attach(table, GTK_WIDGET(tool->x),
                     0, 1, 1, 2, 0, 0, 0, 0);

    tool->y = GTK_LABEL(gtk_label_new(NULL));
    gtk_misc_set_alignment(GTK_MISC(tool->y), 1.0, 0.5);
    gtk_table_attach(table, GTK_WIDGET(tool->y),
                     1, 2, 1, 2, 0, 0, 0, 0);

    tool->val = GTK_LABEL(gtk_label_new(NULL));
    gtk_misc_set_alignment(GTK_MISC(tool->val), 1.0, 0.5);
    gtk_table_attach(table, GTK_WIDGET(tool->val),
                     2, 3, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    table2 = gtk_table_new(1, 4, FALSE);
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table2), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), table2, TRUE, TRUE, 0);

    tool->radius = gtk_adjustment_new(tool->args.radius,
                                      1, RADIUS_MAX, 1, 5, 0);
    gwy_table_attach_spinbutton(table2, 9, _("_Averaging radius:"), "px",
                                tool->radius);
    g_signal_connect_swapped(tool->radius, "value-changed",
                             G_CALLBACK(gwy_tool_read_value_radius_changed),
                             tool);

    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);
    gwy_tool_read_value_update_headers(tool);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_read_value_data_switched(GwyTool *gwytool,
                                  GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolReadValue *tool;

    GWY_TOOL_CLASS(gwy_tool_read_value_parent_class)->data_switched(gwytool,
                                                                    data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_READ_VALUE(gwytool);
    if (data_view) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_point,
                                "draw-marker", FALSE,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
    }
    gwy_tool_read_value_update_headers(tool);
}

static void
gwy_tool_read_value_data_changed(GwyPlainTool *plain_tool)
{
    gwy_tool_read_value_update_headers(GWY_TOOL_READ_VALUE(plain_tool));
    gwy_tool_read_value_update_values(GWY_TOOL_READ_VALUE(plain_tool));
}

static void
gwy_tool_read_value_selection_changed(GwyPlainTool *plain_tool,
                                      gint hint)
{
    g_return_if_fail(hint <= 0);
    gwy_tool_read_value_update_values(GWY_TOOL_READ_VALUE(plain_tool));
}

static void
gwy_tool_read_value_radius_changed(GwyToolReadValue *tool)
{
    tool->args.radius = gwy_adjustment_get_int(tool->radius);
    if (GWY_PLAIN_TOOL(tool)->selection)
        gwy_tool_read_value_update_values(tool);
}

static void
gwy_tool_read_value_update_headers(GwyToolReadValue *tool)
{
    GwyPlainTool *plain_tool;
    GString *str;

    plain_tool = GWY_PLAIN_TOOL(tool);
    str = g_string_new("");

    g_string_assign(str, "<b>x</b>");
    if (plain_tool->coord_format)
        g_string_append_printf(str, " [%s]", plain_tool->coord_format->units);
    gtk_label_set_markup(tool->xunits, str->str);

    g_string_assign(str, "<b>y</b>");
    if (plain_tool->coord_format)
        g_string_append_printf(str, " [%s]", plain_tool->coord_format->units);
    gtk_label_set_markup(tool->yunits, str->str);

    g_string_assign(str, _("<b>Value</b>"));
    if (plain_tool->value_format)
        g_string_append_printf(str, " [%s]", plain_tool->value_format->units);
    gtk_label_set_markup(tool->zunits, str->str);

    g_string_free(str, TRUE);
}

static void
gwy_tool_read_value_update_value(GtkLabel *label,
                                 GwySIValueFormat *vf,
                                 gdouble val)
{
    gchar buf[32];

    if (vf)
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, val/vf->magnitude);
    else
        g_snprintf(buf, sizeof(buf), "%.3g", val);

    gtk_label_set_markup(label, buf);
}

static void
gwy_tool_read_value_update_values(GwyToolReadValue *tool)
{
    GwyPlainTool *plain_tool;
    gboolean is_selected = FALSE;
    gdouble point[2];
    gdouble xoff, yoff;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (plain_tool->data_field && plain_tool->selection)
        is_selected = gwy_selection_get_object(plain_tool->selection, 0, point);

    if (is_selected) {
        xoff = gwy_data_field_get_xoffset(plain_tool->data_field);
        yoff = gwy_data_field_get_yoffset(plain_tool->data_field);

        gwy_tool_read_value_update_value(tool->x, plain_tool->coord_format,
                                         point[0] + xoff);
        gwy_tool_read_value_update_value(tool->y, plain_tool->coord_format,
                                         point[1] + yoff);
        gwy_tool_read_value_calculate(tool, point);
        gwy_tool_read_value_update_value(tool->val, plain_tool->value_format,
                                         tool->avg);
    }
    else {
        gtk_label_set_text(tool->x, "");
        gtk_label_set_text(tool->y, "");
        gtk_label_set_text(tool->val, "");
    }
}

static void
gwy_tool_read_value_calculate(GwyToolReadValue *tool,
                              const gdouble *point)
{
    GwyPlainTool *plain_tool;
    GwyDataField *dfield;
    gint col, row, n, i;
    gdouble m[6], z[3];

    plain_tool = GWY_PLAIN_TOOL(tool);
    dfield = plain_tool->data_field;

    col = gwy_data_field_rtoj(dfield, point[0]);
    row = gwy_data_field_rtoi(dfield, point[1]);

    if (tool->args.radius == 1) {
        tool->avg = gwy_data_field_get_val(dfield, col, row);
        tool->bx = gwy_data_field_get_xder(dfield, col, row);
        tool->by = gwy_data_field_get_yder(dfield, col, row);
        return;
    }

    /* Create the arrays the first time radius > 1 is requested */
    if (!tool->values) {
        n = gwy_data_field_get_circular_area_size(RADIUS_MAX - 0.5);
        tool->values = g_new(gdouble, n);
        tool->xpos = g_new(gint, n);
        tool->ypos = g_new(gint, n);
    }

    n = gwy_data_field_circular_area_extract_with_pos(dfield, col, row,
                                                      tool->args.radius - 0.5,
                                                      tool->values,
                                                      tool->xpos, tool->ypos);
    tool->avg = 0.0;
    if (!n) {
        g_warning("Z average calculated from an empty area");
        return;
    }

    /* Fit plane through extracted data */
    memset(m, 0, 6*sizeof(gdouble));
    memset(z, 0, 3*sizeof(gdouble));
    for (i = 0; i < n; i++) {
        m[0] += 1.0;
        m[1] += tool->xpos[i];
        m[2] += tool->ypos[i];
        m[3] += tool->xpos[i] * tool->xpos[i];
        m[4] += tool->xpos[i] * tool->ypos[i];
        m[5] += tool->ypos[i] * tool->ypos[i];
        z[0] += tool->values[i];
        z[1] += tool->values[i] * tool->xpos[i];
        z[2] += tool->values[i] * tool->ypos[i];
    }
    tool->avg = z[0]/n;
    gwy_math_choleski_decompose(3, m);
    gwy_math_choleski_solve(3, m, z);
    tool->bx = z[1]/gwy_data_field_get_xmeasure(dfield);
    tool->by = z[2]/gwy_data_field_get_ymeasure(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
