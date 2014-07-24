/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2007,2014 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
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

enum { RADIUS_MAX = 32 };

#define GWY_TYPE_TOOL_READ_VALUE            (gwy_tool_read_value_get_type())
#define GWY_TOOL_READ_VALUE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_READ_VALUE, GwyToolReadValue))
#define GWY_IS_TOOL_READ_VALUE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_READ_VALUE))
#define GWY_TOOL_READ_VALUE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_READ_VALUE, GwyToolReadValueClass))

typedef struct _GwyToolReadValue      GwyToolReadValue;
typedef struct _GwyToolReadValueClass GwyToolReadValueClass;

typedef struct {
    gdouble k1;
    gdouble k2;
    gdouble phi1;
    gdouble phi2;
    gdouble xc;
    gdouble yc;
    gdouble zc;
} GwyCurvatureParams;

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
    gdouble k1;
    gdouble k2;

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
    GtkWidget *curv1;
    GtkWidget *curv2;
    GtkObject *radius;
    GtkWidget *show_selection;
    GtkWidget *set_zero;

    gboolean same_units;

    GwyDataField *xunc;
    GwyDataField *yunc;
    GwyDataField *zunc;
    gboolean has_calibration;

    /* potential class data */
    GwySIValueFormat *pixel_format;
    GwySIValueFormat *angle_format;
    GType layer_type_point;
};

struct _GwyToolReadValueClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_read_value_get_type              (void)                      G_GNUC_CONST;
static void  gwy_tool_read_value_finalize              (GObject *object);
static void  gwy_tool_read_value_init_dialog           (GwyToolReadValue *tool);
static void  gwy_tool_read_value_data_switched         (GwyTool *gwytool,
                                                        GwyDataView *data_view);
static void  gwy_tool_read_value_update_units          (GwyToolReadValue *tool);
static void  gwy_tool_read_value_data_changed          (GwyPlainTool *plain_tool);
static void  gwy_tool_read_value_selection_changed     (GwyPlainTool *plain_tool,
                                                        gint hint);
static void  gwy_tool_read_value_radius_changed        (GwyToolReadValue *tool);
static void  gwy_tool_read_value_show_selection_changed(GtkToggleButton *check,
                                                        GwyToolReadValue *tool);
static void  gwy_tool_read_value_update_values         (GwyToolReadValue *tool);
static void  gwy_tool_read_value_calculate             (GwyToolReadValue *tool,
                                                        gint col,
                                                        gint row);
static void  gwy_tool_read_value_set_zero              (GwyToolReadValue *tool);
static void  calc_curvatures                           (const gdouble *values,
                                                        const gint *xpos,
                                                        const gint *ypos,
                                                        guint npts,
                                                        gdouble dx,
                                                        gdouble dy,
                                                        gdouble *pc1,
                                                        gdouble *pc2);

static const gchar radius_key[]         = "/module/readvalue/radius";
static const gchar show_selection_key[] = "/module/readvalue/show-selection";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Pointer tool, reads value under pointer."),
    "Yeti <yeti@gwyddion.net>",
    "2.11",
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
    GtkWidget *label, *align;
    GtkRequisition req;
    GtkTooltips *tips;
    gint row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);
    tips = gwy_app_get_tooltips();

    table = GTK_TABLE(gtk_table_new(13, 3, FALSE));
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
    row++;

    align = gtk_alignment_new(1.0, 0.5, 0.0, 0.0);
    gtk_table_attach(table, align,
                     1, 3, row, row+1, GTK_FILL, 0, 0, 0);

    tool->set_zero = gtk_button_new_with_mnemonic(_("Set _Zero"));
    gtk_container_add(GTK_CONTAINER(align), tool->set_zero);
    gtk_tooltips_set_tip(tips, tool->set_zero,
                         _("Shift plane z=0 to pass through the "
                           "selected point"), NULL);
    gtk_widget_set_sensitive(tool->set_zero, FALSE);
    g_signal_connect_swapped(tool->set_zero, "clicked",
                             G_CALLBACK(gwy_tool_read_value_set_zero), tool);
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
    gtk_table_set_row_spacing(table, row, 8);
    row++;

    gtk_table_attach(table, gwy_label_new_header(_("Curvatures")),
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new(_("Curvature 1"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    tool->curv1 = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(tool->curv1), 1.0, 0.5);
    gtk_table_attach(table, tool->curv1,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new(_("Curvature 2"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    tool->curv2 = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(tool->curv2), 1.0, 0.5);
    gtk_table_attach(table, tool->curv2,
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
    gwy_help_add_to_tool_dialog(dialog, GWY_TOOL(tool), GWY_HELP_NO_BUTTON);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_read_value_data_switched(GwyTool *gwytool,
                                  GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolReadValue *tool;
    gboolean ignore;
    gchar xukey[24];
    gchar yukey[24];
    gchar zukey[24];


    plain_tool = GWY_PLAIN_TOOL(gwytool);
    ignore = (data_view == plain_tool->data_view);

    GWY_TOOL_CLASS(gwy_tool_read_value_parent_class)->data_switched(gwytool,
                                                                    data_view);

    if (ignore || plain_tool->init_failed)
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


        g_snprintf(xukey, sizeof(xukey), "/%d/data/cal_xunc", plain_tool->id);
        g_snprintf(yukey, sizeof(yukey), "/%d/data/cal_yunc", plain_tool->id);
        g_snprintf(zukey, sizeof(zukey), "/%d/data/cal_zunc", plain_tool->id);

        if (gwy_container_gis_object_by_name(plain_tool->container, xukey, &(tool->xunc))
            && gwy_container_gis_object_by_name(plain_tool->container, yukey, &(tool->yunc))
            && gwy_container_gis_object_by_name(plain_tool->container, zukey, &(tool->zunc)))
        {
            tool->has_calibration = TRUE;
        } else {
            tool->has_calibration = FALSE;
        }
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
    gchar xukey[24];
    gchar yukey[24];
    gchar zukey[24];

    g_snprintf(xukey, sizeof(xukey), "/%d/data/cal_xunc", plain_tool->id);
    g_snprintf(yukey, sizeof(yukey), "/%d/data/cal_yunc", plain_tool->id);
    g_snprintf(zukey, sizeof(zukey), "/%d/data/cal_zunc", plain_tool->id);

    if (gwy_container_gis_object_by_name(plain_tool->container, xukey, &(GWY_TOOL_READ_VALUE(plain_tool)->xunc))
        && gwy_container_gis_object_by_name(plain_tool->container, yukey, &(GWY_TOOL_READ_VALUE(plain_tool)->yunc))
        && gwy_container_gis_object_by_name(plain_tool->container, zukey, &(GWY_TOOL_READ_VALUE(plain_tool)->zunc)))
    {
        GWY_TOOL_READ_VALUE(plain_tool)->has_calibration = TRUE;
    } else {
        GWY_TOOL_READ_VALUE(plain_tool)->has_calibration = FALSE;
    }

    gwy_tool_read_value_update_values(GWY_TOOL_READ_VALUE(plain_tool));
}

static void
gwy_tool_read_value_selection_changed(GwyPlainTool *plain_tool,
                                      gint hint)
{
    GwyToolReadValue *tool;

    tool = GWY_TOOL_READ_VALUE(plain_tool);
    g_return_if_fail(hint <= 0);
    gwy_tool_read_value_update_values(tool);
    gtk_widget_set_sensitive(tool->set_zero,
                             plain_tool->selection != NULL
                             && gwy_selection_get_object(plain_tool->selection,
                                                         0, NULL));
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
update_curvature_label(GtkWidget *label, gdouble value,
                       GwyDataField *dfield)
{
    GwySIUnit *unit = gwy_data_field_get_si_unit_xy(dfield);
    GwySIUnit *curvunit = gwy_si_unit_power(unit, -1, NULL);
    GwySIValueFormat *vf;

    vf = gwy_si_unit_get_format_with_digits(curvunit,
                                            GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            value, 3, NULL);
    update_label(vf, label, value);
    gwy_si_unit_value_format_free(vf);
    g_object_unref(curvunit);
}

static void
update_label_unc(GwySIValueFormat *units,
             GtkWidget *label,
             gdouble value,
             gdouble unc)
{
    static gchar buffer[64];

    g_return_if_fail(units);
    g_return_if_fail(GTK_IS_LABEL(label));

    g_snprintf(buffer, sizeof(buffer), "(%.*f±%.*f)%s%s",
               units->precision, value/units->magnitude, units->precision, unc/units->magnitude,
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

    col = floor(gwy_data_field_rtoj(plain_tool->data_field, point[0]));
    row = floor(gwy_data_field_rtoi(plain_tool->data_field, point[1]));

    update_label(plain_tool->coord_format, tool->x, point[0] + xoff);
    update_label(tool->pixel_format, tool->xpx, col);
    update_label(plain_tool->coord_format, tool->y, point[1] + yoff);
    update_label(tool->pixel_format, tool->ypx, row);
    gwy_tool_read_value_calculate(tool, col, row);

    /*FIXME, use local plane fitting and uncertainty propagation*/
    if (tool->has_calibration)
          update_label_unc(plain_tool->value_format,
                           tool->z,
                           tool->avg,
                           gwy_data_field_get_dval_real(tool->zunc,
                                                        point[0],
                                                        point[1],
                                                        GWY_INTERPOLATION_BILINEAR));
    else
        update_label(plain_tool->value_format, tool->z, tool->avg);

    if (tool->same_units) {
        update_label(tool->angle_format, tool->theta,
                     180.0/G_PI*atan(hypot(tool->bx, tool->by)));
        update_label(tool->angle_format, tool->phi,
                     180.0/G_PI*atan2(tool->by, tool->bx));
        update_curvature_label(tool->curv1, tool->k1, plain_tool->data_field);
        update_curvature_label(tool->curv2, tool->k2, plain_tool->data_field);
    }
    else {
        gtk_label_set_text(GTK_LABEL(tool->theta), _("N.A."));
        gtk_label_set_text(GTK_LABEL(tool->phi), _("N.A."));
        gtk_label_set_text(GTK_LABEL(tool->curv1), _("N.A."));
        gtk_label_set_text(GTK_LABEL(tool->curv2), _("N.A."));
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
        tool->k1 = tool->k2 = 0.0;
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
        tool->bx = tool->by = 0.0;
        tool->k1 = tool->k2 = 0.0;
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

    calc_curvatures(tool->values, tool->xpos, tool->ypos, n,
                    gwy_data_field_get_xmeasure(dfield),
                    gwy_data_field_get_ymeasure(dfield),
                    &tool->k1, &tool->k2);
}

static void
gwy_tool_read_value_set_zero(GwyToolReadValue *tool)
{
    GwyPlainTool *plain_tool;
    GQuark quark;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (!plain_tool->data_field
        || !gwy_selection_get_data(plain_tool->selection, NULL)
        || !tool->avg)
        return;

    quark = gwy_app_get_data_key_for_id(plain_tool->id);
    gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);
    gwy_data_field_add(plain_tool->data_field, -tool->avg);
    gwy_data_field_data_changed(plain_tool->data_field);
}

static gdouble
standardize_direction(gdouble phi)
{
    phi = fmod(phi, G_PI);
    if (phi <= -G_PI/2.0)
        phi += G_PI;
    if (phi > G_PI/2.0)
        phi -= G_PI;
    return phi;
}

static guint
calc_quadratic_curvatue(GwyCurvatureParams *curvature,
                        gdouble a, gdouble bx, gdouble by,
                        gdouble cxx, gdouble cxy, gdouble cyy)
{
    /* At least one quadratic term */
    gdouble cm = cxx - cyy;
    gdouble cp = cxx + cyy;
    gdouble phi = 0.5*atan2(cxy, cm);
    gdouble cx = cp + hypot(cm, cxy);
    gdouble cy = cp - hypot(cm, cxy);
    gdouble bx1 = bx*cos(phi) + by*sin(phi);
    gdouble by1 = -bx*sin(phi) + by*cos(phi);
    guint degree = 2;
    gdouble xc, yc;

    /* Eliminate linear terms */
    if (fabs(cx) < 1e-10*fabs(cy)) {
        /* Only y quadratic term */
        xc = 0.0;
        yc = -by1/cy;
        degree = 1;
    }
    else if (fabs(cy) < 1e-10*fabs(cx)) {
        /* Only x quadratic term */
        xc = -bx1/cx;
        yc = 0.0;
        degree = 1;
    }
    else {
        /* Two quadratic terms */
        xc = -bx1/cx;
        yc = -by1/cy;
    }

    curvature->xc = xc*cos(phi) - yc*sin(phi);
    curvature->yc = xc*sin(phi) + yc*cos(phi);
    curvature->zc = a + xc*bx1 + yc*by1 + 0.5*(xc*xc*cx + yc*yc*cy);

    if (cx > cy) {
        GWY_SWAP(gdouble, cx, cy);
        phi += G_PI/2.0;
    }

    curvature->k1 = cx;
    curvature->k2 = cy;
    curvature->phi1 = phi;
    curvature->phi2 = phi + G_PI/2.0;

    return degree;
}

static guint
math_curvature_at_origin(const gdouble *coeffs,
                         GwyCurvatureParams *curvature)
{
    gdouble a = coeffs[0], bx = coeffs[1], by = coeffs[2],
            cxx = coeffs[3], cxy = coeffs[4], cyy = coeffs[5];
    gdouble b, beta;
    guint degree;

    /* Eliminate the mixed term */
    if (fabs(cxx) + fabs(cxy) + fabs(cyy) <= 1e-10*(fabs(bx) + fabs(by))) {
        /* Linear gradient */
        gwy_clear(curvature, 1);
        curvature->phi2 = G_PI/2.0;
        curvature->zc = a;
        return 0;
    }

    b = hypot(bx, by);
    beta = atan2(by, bx);
    if (b > 1e-10) {
        gdouble cosbeta = bx/b,
                sinbeta = by/b,
                cbeta2 = cosbeta*cosbeta,
                sbeta2 = sinbeta*sinbeta,
                csbeta = cosbeta*sinbeta,
                qb = hypot(1.0, b);
        gdouble cxx1 = (cxx*cbeta2 + cxy*csbeta + cyy*sbeta2)/(qb*qb*qb),
                cxy1 = (2.0*(cyy - cxx)*csbeta + cxy*(cbeta2 - sbeta2))/(qb*qb),
                cyy1 = (cyy*cbeta2 - cxy*csbeta + cxx*sbeta2)/qb;
        cxx = cxx1;
        cxy = cxy1;
        cyy = cyy1;
    }
    else
        beta = 0.0;

    degree = calc_quadratic_curvatue(curvature, a, 0, 0, cxx, cxy, cyy);

    curvature->phi1 = standardize_direction(curvature->phi1 + beta);
    curvature->phi2 = standardize_direction(curvature->phi2 + beta);
    // This should already hold approximately.  Enforce it exactly.
    curvature->xc = curvature->yc = 0.0;
    curvature->zc = a;

    return degree;
}

static void
calc_curvatures(const gdouble *values,
                const gint *xpos, const gint *ypos,
                guint npts,
                gdouble dx, gdouble dy,
                gdouble *pc1,
                gdouble *pc2)
{
    gdouble sx2 = 0.0, sy2 = 0.0, sx4 = 0.0, sx2y2 = 0.0, sy4 = 0.0;
    gdouble sz = 0.0, szx = 0.0, szy = 0.0, szx2 = 0.0, szxy = 0.0, szy2 = 0.0;
    gdouble scale = sqrt(dx*dy)*4.0;
    gdouble a[21], b[6];
    gint i, n = 0;
    GwyCurvatureParams params;

    for (i = 0; i < npts; i++) {
        gdouble x = xpos[i]*dx/scale;
        gdouble y = ypos[i]*dy/scale;
        gdouble z = values[i]/scale;
        gdouble xx = x*x, yy = y*y;

        sx2 += xx;
        sx2y2 += xx*yy;
        sy2 += yy;
        sx4 += xx*xx;
        sy4 += yy*yy;

        sz += z;
        szx += x*z;
        szy += y*z;
        szx2 += xx*z;
        szxy += x*y*z;
        szy2 += yy*z;
        n++;
    }

    gwy_clear(a, 21);
    a[0] = n;
    a[2] = a[6] = sx2;
    a[5] = a[15] = sy2;
    a[18] = a[14] = sx2y2;
    a[9] = sx4;
    a[20] = sy4;
    if (gwy_math_choleski_decompose(6, a)) {
        b[0] = sz;
        b[1] = szx;
        b[2] = szy;
        b[3] = szx2;
        b[4] = szxy;
        b[5] = szy2;
        gwy_math_choleski_solve(6, a, b);
    }
    else {
        *pc1 = *pc2 = 0.0;
        return;
    }

    math_curvature_at_origin(b, &params);
    *pc1 = params.k1/scale;
    *pc2 = params.k2/scale;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
