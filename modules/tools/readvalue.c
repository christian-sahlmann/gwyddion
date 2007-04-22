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
    gboolean show_selection;
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

    GtkWidget *x;
    GtkWidget *xpx;
    GtkWidget *y;
    GtkWidget *ypx;
    GtkWidget *z;
    GtkWidget *theta;
    GtkWidget *phi;
    GtkObject *radius;
    GtkWidget *show_selection;

    gboolean same_units;

    /* potential class data */
    GwySIValueFormat *pixel_format;
    GwySIValueFormat *angle_format;
    GType layer_type_point;
};

struct _GwyToolReadValueClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_read_value_get_type             (void) G_GNUC_CONST;
static void gwy_tool_read_value_finalize              (GObject *object);
static void gwy_tool_read_value_init_dialog           (GwyToolReadValue *tool);
static void gwy_tool_read_value_data_switched         (GwyTool *gwytool,
                                                       GwyDataView *data_view);
static void gwy_tool_read_value_update_units          (GwyToolReadValue *tool);
static void gwy_tool_read_value_data_changed         (GwyPlainTool *plain_tool);
static void gwy_tool_read_value_selection_changed     (GwyPlainTool *plain_tool,
                                                       gint hint);
static void gwy_tool_read_value_radius_changed        (GwyToolReadValue *tool);
static void gwy_tool_read_value_show_selection_changed(GtkToggleButton *check,
                                                       GwyToolReadValue *tool);
static void gwy_tool_read_value_update_values         (GwyToolReadValue *tool);
static void gwy_tool_read_value_calculate             (GwyToolReadValue *tool,
                                                       gint col,
                                                       gint row);

static const gchar radius_key[]         = "/module/readvalue/radius";
static const gchar show_selection_key[] = "/module/readvalue/show-selection";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Pointer tool, reads value under pointer."),
    "Yeti <yeti@gwyddion.net>",
    "2.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static const ToolArgs default_args = {
    1, FALSE,
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
    gwy_container_set_boolean_by_name(settings, show_selection_key,
                                      tool->args.show_selection);

    if (tool->pixel_format)
        gwy_si_unit_value_format_free(tool->pixel_format);
    if (tool->angle_format)
        gwy_si_unit_value_format_free(tool->angle_format);

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

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_MARKUP;
    plain_tool->lazy_updates = TRUE;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_int32_by_name(settings, radius_key, &tool->args.radius);
    gwy_container_gis_boolean_by_name(settings, show_selection_key,
                                      &tool->args.show_selection);

    tool->pixel_format = g_new0(GwySIValueFormat, 1);
    tool->pixel_format->magnitude = 1.0;
    tool->pixel_format->precision = 0;
    gwy_si_unit_value_format_set_units(tool->pixel_format, "px");

    tool->angle_format = g_new0(GwySIValueFormat, 1);
    tool->angle_format->magnitude = 1.0;
    tool->angle_format->precision = 1;
    gwy_si_unit_value_format_set_units(tool->angle_format, "deg");

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_point,
                                     "pointer");

    gwy_tool_read_value_init_dialog(tool);
}

static void
gwy_tool_read_value_init_dialog(GwyToolReadValue *tool)
{
    GtkDialog *dialog;
    GtkTable *table;
    GtkWidget *label;
    GtkRequisition req;
    gint row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    table = GTK_TABLE(gtk_table_new(9, 3, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 0);
    row = 0;

    gtk_table_attach(table, gwy_label_new_header(_("Position")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new("X");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    tool->xpx = gtk_label_new("123456 px");
    gtk_widget_size_request(tool->xpx, &req);
    gtk_widget_set_size_request(tool->xpx, req.width, -1);
    gtk_label_set_text(GTK_LABEL(tool->xpx), "");
    gtk_misc_set_alignment(GTK_MISC(tool->xpx), 1.0, 0.5);
    gtk_table_attach(table, tool->xpx, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    tool->x = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(tool->x), 1.0, 0.5);
    gtk_table_attach(table, tool->x, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new("Y");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    tool->ypx = gtk_label_new(NULL);
    gtk_widget_set_size_request(tool->xpx, req.width, -1);
    gtk_misc_set_alignment(GTK_MISC(tool->ypx), 1.0, 0.5);
    gtk_table_attach(table, tool->ypx, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);

    tool->y = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(tool->y), 1.0, 0.5);
    gtk_table_attach(table, tool->y, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    gtk_table_attach(table, gwy_label_new_header(_("Value")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new("Z");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    tool->z = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(tool->z), 1.0, 0.5);
    gtk_table_attach(table, tool->z,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    gtk_table_attach(table, gwy_label_new_header(_("Facet")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new(_("Inclination θ"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    tool->theta = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(tool->theta), 1.0, 0.5);
    gtk_table_attach(table, tool->theta,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new(_("Inclination φ"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    tool->phi = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(tool->phi), 1.0, 0.5);
    gtk_table_attach(table, tool->phi,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    table = GTK_TABLE(gtk_table_new(2, 4, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 0);
    row = 0;

    tool->radius = gtk_adjustment_new(tool->args.radius,
                                      1, RADIUS_MAX, 1, 5, 0);
    gwy_table_attach_spinbutton(GTK_WIDGET(table), row,
                                _("_Averaging radius:"), "px", tool->radius);
    g_signal_connect_swapped(tool->radius, "value-changed",
                             G_CALLBACK(gwy_tool_read_value_radius_changed),
                             tool);
    row++;

    tool->show_selection
        = gtk_check_button_new_with_mnemonic(_("Show _selection"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->show_selection),
                                 tool->args.show_selection);
    gtk_table_attach(GTK_TABLE(table), tool->show_selection,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect(tool->show_selection, "toggled",
                     G_CALLBACK(gwy_tool_read_value_show_selection_changed),
                             tool);
    row++;

    gwy_plain_tool_add_clear_button(GWY_PLAIN_TOOL(tool));
    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);

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
                                "draw-marker", tool->args.show_selection,
                                "marker-radius", tool->args.radius,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
        gwy_tool_read_value_update_units(tool);
    }
}

static void
gwy_tool_read_value_update_units(GwyToolReadValue *tool)
{
    GwyPlainTool *plain_tool;
    GwySIUnit *siunitxy, *siunitz;

    plain_tool = GWY_PLAIN_TOOL(tool);

    siunitxy = gwy_data_field_get_si_unit_xy(plain_tool->data_field);
    siunitz = gwy_data_field_get_si_unit_z(plain_tool->data_field);
    tool->same_units = gwy_si_unit_equal(siunitxy, siunitz);
}

static void
gwy_tool_read_value_data_changed(GwyPlainTool *plain_tool)
{
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
    GwyPlainTool *plain_tool;

    tool->args.radius = gwy_adjustment_get_int(tool->radius);
    plain_tool = GWY_PLAIN_TOOL(tool);
    if (plain_tool->layer)
        g_object_set(plain_tool->layer,
                     "marker-radius", tool->args.radius,
                     NULL);
    if (plain_tool->selection)
        gwy_tool_read_value_update_values(tool);
}

static void
gwy_tool_read_value_show_selection_changed(GtkToggleButton *check,
                                           GwyToolReadValue *tool)
{
    GwyPlainTool *plain_tool;

    tool->args.show_selection = gtk_toggle_button_get_active(check);
    plain_tool = GWY_PLAIN_TOOL(tool);
    if (plain_tool->layer)
        g_object_set(plain_tool->layer,
                     "draw-marker", tool->args.show_selection,
                     NULL);
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
gwy_tool_read_value_update_values(GwyToolReadValue *tool)
{
    GwyPlainTool *plain_tool;
    gboolean is_selected = FALSE;
    gdouble point[2];
    gdouble xoff, yoff;
    gint col, row;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (plain_tool->data_field && plain_tool->selection)
        is_selected = gwy_selection_get_object(plain_tool->selection, 0, point);

    if (!is_selected) {
        gtk_label_set_text(GTK_LABEL(tool->x), NULL);
        gtk_label_set_text(GTK_LABEL(tool->xpx), NULL);
        gtk_label_set_text(GTK_LABEL(tool->y), NULL);
        gtk_label_set_text(GTK_LABEL(tool->ypx), NULL);
        gtk_label_set_text(GTK_LABEL(tool->z), NULL);
        gtk_label_set_text(GTK_LABEL(tool->theta), NULL);
        gtk_label_set_text(GTK_LABEL(tool->phi), NULL);
        return;
    }

    xoff = gwy_data_field_get_xoffset(plain_tool->data_field);
    yoff = gwy_data_field_get_yoffset(plain_tool->data_field);

    col = gwy_data_field_rtoj(plain_tool->data_field, point[0]);
    row = gwy_data_field_rtoi(plain_tool->data_field, point[1]);

    update_label(plain_tool->coord_format, tool->x, point[0] + xoff);
    update_label(tool->pixel_format, tool->xpx, col);
    update_label(plain_tool->coord_format, tool->y, point[1] + yoff);
    update_label(tool->pixel_format, tool->ypx, row);
    gwy_tool_read_value_calculate(tool, col, row);
    update_label(plain_tool->value_format, tool->z, tool->avg);

    if (tool->same_units) {
        update_label(tool->angle_format, tool->theta,
                     180.0/G_PI*atan(hypot(tool->bx, tool->by)));
        update_label(tool->angle_format, tool->phi,
                     180.0/G_PI*atan2(tool->by, tool->bx));
    }
    else {
        gtk_label_set_text(GTK_LABEL(tool->theta), _("N.A."));
        gtk_label_set_text(GTK_LABEL(tool->phi), _("N.A."));
    }
}

static void
gwy_tool_read_value_calculate(GwyToolReadValue *tool,
                              gint col,
                              gint row)
{
    GwyPlainTool *plain_tool;
    GwyDataField *dfield;
    gint n, i;
    gdouble m[6], z[3];

    plain_tool = GWY_PLAIN_TOOL(tool);
    dfield = plain_tool->data_field;

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
        m[2] += tool->xpos[i] * tool->xpos[i];
        m[3] += tool->ypos[i];
        m[4] += tool->xpos[i] * tool->ypos[i];
        m[5] += tool->ypos[i] * tool->ypos[i];
        z[0] += tool->values[i];
        z[1] += tool->values[i] * tool->xpos[i];
        z[2] += tool->values[i] * tool->ypos[i];
    }
    tool->avg = z[0]/n;
    gwy_math_choleski_decompose(3, m);
    gwy_math_choleski_solve(3, m, z);
    /* The signs may seem odd.  We have to invert y due to coordinate system
     * and then invert both for downward slopes.  As a result x is inverted. */
    tool->bx = -z[1]/gwy_data_field_get_xmeasure(dfield);
    tool->by = z[2]/gwy_data_field_get_ymeasure(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
