/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <math.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/app.h>
#include <app/unitool.h>

typedef struct {
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *w;
    GtkWidget *h;
    GtkWidget *ra;
    GtkWidget *rms;
    GtkWidget *skew;
    GtkWidget *kurtosis;
    GtkWidget *avg;
} ToolControls;

static gboolean   module_register  (const gchar *name);
static void       use              (GwyDataWindow *data_window,
                                    GwyToolSwitchEvent reason);
static void       layer_setup      (GwyUnitoolState *state);
static GtkWidget* dialog_create    (GwyUnitoolState *state);
static void       dialog_update    (GwyUnitoolState *state);
static void       dialog_abandon   (GwyUnitoolState *state);
static void       apply            (GwyUnitoolState *state);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "stats",
    "Statistical quantities.",
    "Petr Klapetek <petr@klapetek.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    layer_setup,                   /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                 /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    apply,                         /* apply action */
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
        "gwy_stat_quantities",
        "Statistical quantities",
        67,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static void
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static GwyUnitoolState *state = NULL;

    if (!state) {
        state = g_new0(GwyUnitoolState, 1);
        func_slots.layer_type = GWY_TYPE_LAYER_SELECT;
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
    }
    gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(GWY_IS_LAYER_SELECT(state->layer));
    g_object_set(state->layer, "is_crop", FALSE, NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyUnitoolUnits *units;
    GtkWidget *dialog, *table, *label, *frame;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    units = &state->coord_units;

    dialog = gtk_dialog_new_with_buttons(_("Statistical quantities"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
 
    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(12, 3, FALSE);
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
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Origin</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 6, 7, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("X"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 7, 8, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Y"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 8, 9, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Size</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 9, 10, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Width"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 10, 11, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Height"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 11, 12, GTK_FILL, 0, 2, 2);

    controls->x = gtk_label_new("");
    controls->y = gtk_label_new("");
    controls->w = gtk_label_new("");
    controls->h = gtk_label_new("");
    controls->ra = gtk_label_new("");
    controls->rms = gtk_label_new("");
    controls->avg = gtk_label_new("");
    controls->skew = gtk_label_new("");
    controls->kurtosis = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->x), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->y), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->w), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->h), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->ra), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->rms), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->avg), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->skew), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->kurtosis), 1.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(controls->x), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->y), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->w), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->h), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->ra), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->rms), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->avg), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->skew), TRUE);
    gtk_label_set_selectable(GTK_LABEL(controls->kurtosis), TRUE);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->x, 2, 3, 7, 8);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->y, 2, 3, 8, 9);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->w, 2, 3, 10, 11);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->h, 2, 3, 11, 12);

    gtk_table_attach_defaults(GTK_TABLE(table), controls->ra, 2, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->rms, 2, 3, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->skew, 2, 3, 3, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->kurtosis, 2, 3, 4, 5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->avg, 2, 3, 5, 6);

    return dialog;
}

/* TODO */
static void
apply(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gdouble xmin, ymin, xmax, ymax;
    gdouble avg, ra, rms, skew, kurtosis;

    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    gwy_app_clean_up_data(data);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));


    if (gwy_layer_select_get_selection(GWY_LAYER_SELECT(state->layer),
                                       &xmin, &ymin, &xmax, &ymax)) {
        gwy_data_field_get_area_stats(dfield, gwy_data_field_rtoj(dfield, xmin),
                                      gwy_data_field_rtoj(dfield, ymin),
                                      gwy_data_field_rtoj(dfield, xmax),
                                      gwy_data_field_rtoj(dfield, ymax),
                                      &avg, &ra, &rms, &skew, &kurtosis);
    }
    else {
        gwy_data_field_get_stats(dfield, &avg, &ra, &rms, &skew, &kurtosis);
        xmin = ymin = 0;
        xmax = gwy_data_field_get_xreal(dfield);
        ymax = gwy_data_field_get_yreal(dfield);
    }
}

static void
dialog_update(GwyUnitoolState *state)
{
    GwyUnitoolUnits *units;
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gdouble xmin, ymin, xmax, ymax;
    gboolean is_visible, is_selected;
    gdouble avg, ra, rms, skew, kurtosis;
    gchar buffer[16];

    gwy_debug("");
    is_visible = state->is_visible;

    controls = (ToolControls*)state->user_data;
    units = &state->coord_units;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    is_selected = gwy_layer_select_get_selection(GWY_LAYER_SELECT(state->layer),
                                                 &xmin, &ymin, &xmax, &ymax);
    if (is_selected)
        gwy_data_field_get_area_stats(dfield,
                                      gwy_data_field_rtoj(dfield, xmin),
                                      gwy_data_field_rtoj(dfield, ymin),
                                      gwy_data_field_rtoj(dfield, xmax),
                                      gwy_data_field_rtoj(dfield, ymax),
                                      &avg, &ra, &rms, &skew, &kurtosis);
    else
        gwy_data_field_get_stats(dfield, &avg, &ra, &rms, &skew, &kurtosis);

    g_snprintf(buffer, sizeof(buffer), "%2.3e", ra);
    gtk_label_set_text(GTK_LABEL(controls->ra), buffer);
    g_snprintf(buffer, sizeof(buffer), "%2.3e", rms);
    gtk_label_set_text(GTK_LABEL(controls->rms), buffer);
    g_snprintf(buffer, sizeof(buffer), "%2.3e", skew);
    gtk_label_set_text(GTK_LABEL(controls->skew), buffer);
    g_snprintf(buffer, sizeof(buffer), "%2.3e", kurtosis);
    gtk_label_set_text(GTK_LABEL(controls->kurtosis), buffer);
    g_snprintf(buffer, sizeof(buffer), "%2.3e", avg);
    gtk_label_set_text(GTK_LABEL(controls->avg), buffer);


    if (!is_visible && !is_selected)
        return;
    if (is_selected) {
        gwy_unitool_update_label(units, controls->x, MIN(xmin, xmax));
        gwy_unitool_update_label(units, controls->y, MIN(ymin, ymax));
        gwy_unitool_update_label(units, controls->w, fabs(xmax - xmin));
        gwy_unitool_update_label(units, controls->h, fabs(ymax - ymin));
    }
    else {
        gwy_unitool_update_label(units, controls->x, 0);
        gwy_unitool_update_label(units, controls->y, 0);
        gwy_unitool_update_label(units, controls->w,
                                 gwy_data_field_get_xreal(dfield));
        gwy_unitool_update_label(units, controls->h,
                                 gwy_data_field_get_yreal(dfield));
    }
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    memset(state->user_data, 0, sizeof(ToolControls));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

