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
#include <libprocess/grains.h>
#include <libprocess/fractals.h>
#include <libprocess/correct.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_GRAIN_REMOVER            (gwy_tool_grain_remover_get_type())
#define GWY_TOOL_GRAIN_REMOVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_GRAIN_REMOVER, GwyToolGrainRemover))
#define GWY_IS_TOOL_GRAIN_REMOVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_GRAIN_REMOVER))
#define GWY_TOOL_GRAIN_REMOVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_GRAIN_REMOVER, GwyToolGrainRemoverClass))

typedef enum {
    GRAIN_REMOVE_MASK = 1 << 0,
    GRAIN_REMOVE_DATA = 1 << 1,
    GRAIN_REMOVE_BOTH = GRAIN_REMOVE_DATA | GRAIN_REMOVE_MASK
} RemoveMode;

typedef enum {
    GRAIN_REMOVE_LAPLACE = 1,
    GRAIN_REMOVE_FRACTAL
} RemoveAlgorithm;

typedef struct _GwyToolGrainRemover      GwyToolGrainRemover;
typedef struct _GwyToolGrainRemoverClass GwyToolGrainRemoverClass;

typedef struct {
    RemoveMode mode;
    RemoveAlgorithm method;
} ToolArgs;

struct _GwyToolGrainRemover {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GtkWidget *method;
    GtkWidget *method_label;

    /* potential class data */
    GType layer_type_point;
};

struct _GwyToolGrainRemoverClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_grain_remover_get_type        (void) G_GNUC_CONST;
static void gwy_tool_grain_remover_finalize         (GObject *object);
static void gwy_tool_grain_remover_init_dialog      (GwyToolGrainRemover *tool);
static void gwy_tool_grain_remover_data_switched    (GwyTool *gwytool,
                                                     GwyDataView *data_view);
static void gwy_tool_grain_remover_mode_changed     (GtkWidget *radio,
                                                     GwyToolGrainRemover *tool);
static void gwy_tool_grain_remover_method_changed   (GtkComboBox *combo,
                                                     GwyToolGrainRemover *tool);
static void gwy_tool_grain_remover_selection_finised(GwyPlainTool *plain_tool);

static void laplace_interpolation                   (GwyDataField *dfield,
                                                     GwyDataField *grain);

static const gchar mode_key[]   = "/module/grainremover/mode";
static const gchar method_key[] = "/module/grainremover/method";

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Grain removal tool, removes continuous parts of mask and/or "
       "underlying data."),
    "Petr Klapetek <klapetek@gwyddion.net>, Yeti <yeti@gwyddion.net>",
    "3.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static const GwyEnum modes[] = {
    { N_("_Mask"), GRAIN_REMOVE_MASK },
    { N_("_Data"), GRAIN_REMOVE_DATA },
    { N_("_Both"), GRAIN_REMOVE_BOTH },
};

static const GwyEnum methods[] = {
    { N_("Laplace solver"),     GRAIN_REMOVE_LAPLACE },
    { N_("Fractal correction"), GRAIN_REMOVE_FRACTAL },
};

static const ToolArgs default_args = {
    GRAIN_REMOVE_BOTH,
    GRAIN_REMOVE_LAPLACE,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolGrainRemover, gwy_tool_grain_remover, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_GRAIN_REMOVER);

    return TRUE;
}

static void
gwy_tool_grain_remover_class_init(GwyToolGrainRemoverClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_grain_remover_finalize;

    tool_class->stock_id = GWY_STOCK_GRAINS_REMOVE;
    tool_class->title = _("Grain Remove");
    tool_class->tooltip = _("Remove individual grains "
                            "(continuous parts of mask)");
    tool_class->prefix = "/module/grainremover";
    tool_class->data_switched = gwy_tool_grain_remover_data_switched;

    ptool_class->selection_finished = gwy_tool_grain_remover_selection_finised;
}

static void
gwy_tool_grain_remover_finalize(GObject *object)
{
    GwyToolGrainRemover *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_GRAIN_REMOVER(object);

    settings = gwy_app_settings_get();
    gwy_container_set_int32_by_name(settings,
                                    mode_key, tool->args.mode);
    gwy_container_set_int32_by_name(settings,
                                    method_key, tool->args.method);

    G_OBJECT_CLASS(gwy_tool_grain_remover_parent_class)->finalize(object);
}

static void
gwy_tool_grain_remover_init(GwyToolGrainRemover *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_point = gwy_plain_tool_check_layer_type(plain_tool,
                                                             "GwyLayerPoint");
    if (!tool->layer_type_point)
        return;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_enum_by_name(settings, mode_key, &tool->args.mode);
    gwy_container_gis_enum_by_name(settings, method_key, &tool->args.method);

    gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_point,
                                     "pointer");

    gwy_tool_grain_remover_init_dialog(tool);
}

static void
gwy_tool_grain_remover_init_dialog(GwyToolGrainRemover *tool)
{
    GtkWidget *label, *combo;
    GtkDialog *dialog;
    GtkTable *table;
    GSList *group;
    gboolean sensitive;
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

    group = gwy_radio_buttons_create
                       (modes, G_N_ELEMENTS(modes),
                        G_CALLBACK(gwy_tool_grain_remover_mode_changed), tool,
                        tool->args.mode);
    row = gwy_radio_buttons_attach_to_table(group, table, 2, row);
    gtk_table_set_row_spacing(table, row-1, 8);

    label = gtk_label_new_with_mnemonic(_("_Interpolation method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    tool->method_label = label;
    row++;

    combo = gwy_enum_combo_box_new
                   (methods, G_N_ELEMENTS(methods),
                    G_CALLBACK(gwy_tool_grain_remover_method_changed), tool,
                    tool->args.method, TRUE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
    gtk_table_attach(table, combo,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    tool->method = combo;
    row++;

    sensitive = (tool->args.mode == GRAIN_REMOVE_DATA
                 || tool->args.mode == GRAIN_REMOVE_BOTH);
    gtk_widget_set_sensitive(tool->method, sensitive);
    gtk_widget_set_sensitive(tool->method_label, sensitive);

    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_grain_remover_data_switched(GwyTool *gwytool,
                                     GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolGrainRemover *tool;

    GWY_TOOL_CLASS(gwy_tool_grain_remover_parent_class)->data_switched(gwytool,
                                                                    data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_GRAIN_REMOVER(gwytool);
    if (data_view) {
        gwy_object_set_or_reset(plain_tool->layer,
                                tool->layer_type_point,
                                "draw-marker", FALSE,
                                "editable", TRUE,
                                "focus", -1,
                                NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
    }
}

static void
gwy_tool_grain_remover_mode_changed(GtkWidget *radio,
                                    GwyToolGrainRemover *tool)
{
    RemoveMode mode;
    gboolean sensitive;

    mode = gwy_radio_button_get_value(radio);
    tool->args.mode = mode;

    sensitive = (mode == GRAIN_REMOVE_DATA || mode == GRAIN_REMOVE_BOTH);
    gtk_widget_set_sensitive(tool->method, sensitive);
    gtk_widget_set_sensitive(tool->method_label, sensitive);
}

static void
gwy_tool_grain_remover_method_changed(GtkComboBox *combo,
                                      GwyToolGrainRemover *tool)
{
    tool->args.method = gwy_enum_combo_box_get_active(combo);
}

static void
gwy_tool_grain_remover_selection_finised(GwyPlainTool *plain_tool)
{
    gdouble point[2];
    GQuark quarks[2];
    gint col, row;
    RemoveMode mode;
    GwyDataField *tmp;

    if (!plain_tool->mask_field
        || !gwy_selection_get_object(plain_tool->selection, 0, point))
        return;

    row = gwy_data_field_rtoi(plain_tool->mask_field, point[1]);
    col = gwy_data_field_rtoj(plain_tool->mask_field, point[0]);
    if (!gwy_data_field_get_val(plain_tool->mask_field, col, row))
        return;

    mode = GWY_TOOL_GRAIN_REMOVER(plain_tool)->args.mode;
    quarks[0] = quarks[1] = 0;
    if (mode & GRAIN_REMOVE_DATA)
        quarks[0] = gwy_app_get_data_key_for_id(plain_tool->id);
    if (mode & GRAIN_REMOVE_MASK)
        quarks[1] = gwy_app_get_mask_key_for_id(plain_tool->id);

    gwy_app_undo_qcheckpointv(plain_tool->container, 2, quarks);
    if (mode & GRAIN_REMOVE_DATA) {
        tmp = gwy_data_field_duplicate(plain_tool->mask_field);
        gwy_data_field_grains_extract_grain(tmp, col, row);
        switch (GWY_TOOL_GRAIN_REMOVER(plain_tool)->args.method) {
            case GRAIN_REMOVE_LAPLACE:
            laplace_interpolation(plain_tool->data_field, tmp);
            break;

            case GRAIN_REMOVE_FRACTAL:
            gwy_data_field_fractal_correction(plain_tool->data_field, tmp,
                                              GWY_INTERPOLATION_BILINEAR);
            break;
        }
        g_object_unref(tmp);
        gwy_data_field_data_changed(plain_tool->data_field);
    }
    if (mode & GRAIN_REMOVE_MASK) {
        gwy_data_field_grains_remove_grain(plain_tool->mask_field, col, row);
        gwy_data_field_data_changed(plain_tool->mask_field);
    }

    gwy_selection_clear(plain_tool->selection);
}

static void
laplace_interpolation(GwyDataField *dfield,
                      GwyDataField *grain)
{
    GwyDataField *area, *buffer, *mask;
    gdouble error, maxer, cor;
    const gdouble *data;
    gint xres, yres, xmin, xmax, ymin, ymax;
    gint i, j;

    /* Find mask bounds */
    xmin = ymin = G_MAXINT;
    xmax = ymax = -1;
    xres = gwy_data_field_get_xres(grain);
    yres = gwy_data_field_get_yres(grain);
    data = gwy_data_field_get_data_const(grain);
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            if (data[i*xres + j]) {
                if (i < ymin)
                    ymin = i;
                if (i > ymax)
                    ymax = i;
                if (j < xmin)
                    xmin = j;
                if (j > xmax)
                    xmax = j;
            }
        }
    }
    g_return_if_fail(xmax > -1 && ymax > -1);
    xmin = MAX(0, xmin-1);
    xmax = MIN(xres, xmax+2);
    ymin = MAX(0, ymin-1);
    ymax = MIN(yres, ymax+2);

    /* Create smaller working datafields */
    area = gwy_data_field_area_extract(dfield,
                                       xmin, ymin, xmax - xmin, ymax - ymin);
    mask = gwy_data_field_area_extract(grain,
                                       xmin, ymin, xmax - xmin, ymax - ymin);

    /* Interpolate */
    maxer = gwy_data_field_get_rms(area)/1.0e3;
    gwy_data_field_correct_average(area, mask);
    buffer = gwy_data_field_new_alike(mask, FALSE);
    cor = 0.2;
    error = 0;
    i = 0;
    do {
        gwy_data_field_correct_laplace_iteration(area, mask, buffer,
                                                 cor, &error);
        i++;
    } while (error >= maxer && i < 1000);
    g_object_unref(buffer);
    g_object_unref(mask);

    /* Copy result back */
    gwy_data_field_area_copy(area, dfield, 0, 0, xmax - xmin, ymax - ymin,
                             xmin, ymin);
    g_object_unref(area);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
