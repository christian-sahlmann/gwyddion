/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/app.h>
#include <app/unitool.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef struct {
    GwyUnitoolRectLabels labels;
    GtkWidget *ra;
    GtkWidget *rms;
    GtkWidget *skew;
    GtkWidget *kurtosis;
    GtkWidget *avg;
    GtkWidget *projarea;
    GtkWidget *area;
} ToolControls;

static gboolean   module_register  (const gchar *name);
static gboolean   use              (GwyDataWindow *data_window,
                                    GwyToolSwitchEvent reason);
static void       layer_setup      (GwyUnitoolState *state);
static GtkWidget* dialog_create    (GwyUnitoolState *state);
static void       dialog_update    (GwyUnitoolState *state,
                                    GwyUnitoolUpdateType reason);
static void       dialog_abandon   (GwyUnitoolState *state);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "stats",
    N_("Statistical quantities."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    layer_setup,                   /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                 /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    NULL,                          /* apply action */
    NULL,                          /* nonstandard response handler */
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo func_info = {
        "stats",
        GWY_STOCK_STAT_QUANTITIES,
        N_("Statistical quantities"),
        67,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerSelect";
    static GwyUnitoolState *state = NULL;

    if (!state) {
        func_slots.layer_type = g_type_from_name(layer_name);
        if (!func_slots.layer_type) {
            g_warning("Layer type `%s' not available", layer_name);
            return FALSE;
        }
        state = g_new0(GwyUnitoolState, 1);
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
    }
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer, "is_crop", FALSE, NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwySIValueFormat *units;
    GtkWidget *dialog, *table, *label, *frame;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    units = state->coord_format;

    dialog = gtk_dialog_new_with_buttons(_("Statistical Quantities"),
                                         NULL, 0, NULL);
    gwy_unitool_dialog_add_button_clear(dialog);
    gwy_unitool_dialog_add_button_hide(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(14, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Parameters</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Ra"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Rms"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Skew"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 3, 4, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Kurtosis"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Average height"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 5, 6, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Projected area"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 6, 7, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Area"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 7, 8, GTK_FILL, 0, 2, 2);

    gwy_unitool_rect_info_table_setup(&controls->labels,
                                      GTK_TABLE(table), 0, 8);
    controls->labels.unselected_is_full = TRUE;

    controls->ra = gtk_label_new("");
    controls->rms = gtk_label_new("");
    controls->avg = gtk_label_new("");
    controls->skew = gtk_label_new("");
    controls->kurtosis = gtk_label_new("");
    controls->projarea = gtk_label_new("");
    controls->area = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->ra), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->rms), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->avg), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->skew), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->kurtosis), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->projarea), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->area), 1.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(controls->ra), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->rms), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->avg), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->skew), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->kurtosis), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->projarea), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->area), TRUE);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->ra, 2, 4, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->rms, 2, 4, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->skew, 2, 4, 3, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->kurtosis, 2, 4, 4, 5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->avg, 2, 4, 5, 6);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->projarea, 2, 4, 6, 7);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->area, 2, 4, 7, 8);

    return dialog;
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    GwySIValueFormat *units;
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gdouble xy[4];
    gint isel[4];
    gboolean is_visible, is_selected;
    gdouble avg, ra, rms, skew, kurtosis;
    gdouble projarea, area;
    gchar buffer[30];

    gwy_debug("");
    is_visible = state->is_visible;

    controls = (ToolControls*)state->user_data;
    units = state->coord_format;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    is_selected = gwy_vector_layer_get_selection(state->layer, NULL);
    if (!is_visible && !is_selected)
        return;

    gwy_unitool_rect_info_table_fill(state, &controls->labels, xy, isel);
    gwy_data_field_get_area_stats(dfield, isel[0], isel[1], isel[2], isel[3],
                                  &avg, &ra, &rms, &skew, &kurtosis);
    area = gwy_data_field_get_area_surface_area(dfield,
                                                isel[0], isel[1],
                                                isel[2], isel[3],
                                                GWY_INTERPOLATION_BILINEAR);
    projarea = fabs((gwy_data_field_rtoj(dfield, xy[0])
                     - gwy_data_field_rtoj(dfield, xy[2])))
               * fabs((gwy_data_field_rtoj(dfield, xy[1])
                       * - gwy_data_field_rtoj(dfield, xy[3])))
               * dfield->xreal*dfield->xreal/dfield->xres/dfield->xres;
    /*FIXME: this is to prevent rounding errors to produce nonreal 
     * results on very flat surfaces*/
    if (area < projarea) area = projarea;

    gwy_unitool_update_label(state->value_format, controls->ra, ra);
    gwy_unitool_update_label(state->value_format, controls->rms, rms);
    g_snprintf(buffer, sizeof(buffer), "%2.3g", skew);
    gtk_label_set_text(GTK_LABEL(controls->skew), buffer);
    g_snprintf(buffer, sizeof(buffer), "%2.3g", kurtosis);
    gtk_label_set_text(GTK_LABEL(controls->kurtosis), buffer);
    gwy_unitool_update_label(state->value_format, controls->avg, avg);

    g_snprintf(buffer, sizeof(buffer), "%2.3g %s<sup>2</sup>",
               projarea, gwy_si_unit_get_unit_string(dfield->si_unit_z));
    gtk_label_set_markup(GTK_LABEL(controls->projarea), buffer);
    g_snprintf(buffer, sizeof(buffer), "%2.3g %s<sup>2</sup>",
               area, gwy_si_unit_get_unit_string(dfield->si_unit_z));
    gtk_label_set_markup(GTK_LABEL(controls->area), buffer);
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    memset(state->user_data, 0, sizeof(ToolControls));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

