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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
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
    GtkWidget *min;
    GtkWidget *max;
    GtkWidget *median;
    GtkWidget *projarea;
    GtkWidget *area;
    GtkWidget *theta;
    GtkWidget *phi;
    GwySIValueFormat *vform2;
    GwySIValueFormat *vformdeg;
    gboolean same_units;
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
    N_("Statistical quantities tool, calculates various statistical "
       "characteristics (mean, median, RMS, skew, kurtosis, inclination, "
       "area, projected area) of selected part of data."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.5",
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
    /* Compute area units and find out whether we should display non-projected
     * area. */
    if (data_window) {
        ToolControls *controls;
        GwyContainer *data;
        GwyDataField *dfield;
        GwySIUnit *siunitxy, *siunitz, *siunitdeg;
        gdouble xreal, yreal, q;

        controls = (ToolControls*)state->user_data;
        data = gwy_data_window_get_data(data_window);
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        siunitxy = gwy_data_field_get_si_unit_xy(dfield);
        siunitz = gwy_data_field_get_si_unit_z(dfield);
        controls->same_units = gwy_si_unit_equal(siunitxy, siunitz);

        xreal = gwy_data_field_get_xreal(dfield);
        yreal = gwy_data_field_get_xreal(dfield);
        q = xreal/gwy_data_field_get_xres(dfield)
            *yreal/gwy_data_field_get_yres(dfield);

        siunitxy = gwy_si_unit_duplicate(siunitxy);
        gwy_si_unit_power(siunitxy, 2, siunitxy);
        controls->vform2
            = gwy_si_unit_get_format_with_resolution(siunitxy,
                                                     GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                     xreal*yreal, q,
                                                     controls->vform2);
        g_object_unref(siunitxy);

        siunitdeg = (GwySIUnit*)gwy_si_unit_new("deg"); /* degree */
        controls->vformdeg
            = gwy_si_unit_get_format_with_resolution(siunitdeg,
                                                     GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                     360, 0.01,
                                                     controls->vformdeg);
        g_object_unref(siunitdeg);
    }

    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer, "is-crop", FALSE, NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    static struct {
        const gchar *name;
        gsize offset;
    }
    const values[] = {
        { N_("Ra"),             G_STRUCT_OFFSET(ToolControls, ra)       },
        { N_("Rms"),            G_STRUCT_OFFSET(ToolControls, rms)      },
        { N_("Skew"),           G_STRUCT_OFFSET(ToolControls, skew)     },
        { N_("Kurtosis"),       G_STRUCT_OFFSET(ToolControls, kurtosis) },
        { N_("Average height"), G_STRUCT_OFFSET(ToolControls, avg)      },
        { N_("Minimum"),        G_STRUCT_OFFSET(ToolControls, min)      },
        { N_("Maximum"),        G_STRUCT_OFFSET(ToolControls, max)      },
        { N_("Median"),         G_STRUCT_OFFSET(ToolControls, median)   },
        { N_("Projected area"), G_STRUCT_OFFSET(ToolControls, projarea) },
        { N_("Area"),           G_STRUCT_OFFSET(ToolControls, area)     },
        { N_("Inclination theta"), G_STRUCT_OFFSET(ToolControls, theta) },
        { N_("Inclination phi"), G_STRUCT_OFFSET(ToolControls, phi) },
    };
    ToolControls *controls;
    GwySIValueFormat *units;
    GtkWidget *dialog, *table, *label, *frame, **plabel;
    gint i;

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

        plabel = (GtkWidget**)G_STRUCT_MEMBER_P(controls, values[i].offset);
        *plabel = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(*plabel), 1.0, 0.5);
        gtk_label_set_selectable(GTK_LABEL(*plabel), TRUE);
        gtk_table_attach(GTK_TABLE(table), *plabel, 1, 3, i+1, i+2,
                         GTK_EXPAND | GTK_FILL, 0, 2, 2);
    }

    gwy_unitool_rect_info_table_setup(&controls->labels,
                                      GTK_TABLE(table),
                                      0, 1 + G_N_ELEMENTS(values));
    controls->labels.unselected_is_full = TRUE;

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
    gint w, h;
    gdouble avg, ra, rms, skew, kurtosis, min, max, median, q;
    gdouble projarea, area = 0.0;
    gdouble theta, phi;
    gchar buffer[48];

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    units = state->coord_format;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    gwy_unitool_rect_info_table_fill(state, &controls->labels, xy, isel);
    w = isel[2] - isel[0];
    h = isel[3] - isel[1];
    gwy_data_field_area_get_stats(dfield, isel[0], isel[1], w, h,
                                  &avg, &ra, &rms, &skew, &kurtosis);
    min = gwy_data_field_area_get_min(dfield, isel[0], isel[1], w, h);
    max = gwy_data_field_area_get_max(dfield, isel[0], isel[1], w, h);
    median = gwy_data_field_area_get_median(dfield, isel[0], isel[1], w, h);
    q = gwy_data_field_get_xreal(dfield)/gwy_data_field_get_xres(dfield)
        *gwy_data_field_get_yreal(dfield)/gwy_data_field_get_yres(dfield);
    projarea = w*h*q;
    if (controls->same_units) {
        area = gwy_data_field_area_get_surface_area(dfield, isel[0], isel[1],
                                                    w, h);
        gwy_data_field_area_get_inclination(dfield, isel[0], isel[1], w, h,
                                            &theta, &phi);
    }

    state->value_format->precision = 2;
    gwy_unitool_update_label(state->value_format, controls->ra, ra);
    gwy_unitool_update_label(state->value_format, controls->rms, rms);
    g_snprintf(buffer, sizeof(buffer), "%2.3g", skew);
    gtk_label_set_text(GTK_LABEL(controls->skew), buffer);
    g_snprintf(buffer, sizeof(buffer), "%2.3g", kurtosis);
    gtk_label_set_text(GTK_LABEL(controls->kurtosis), buffer);
    gwy_unitool_update_label(state->value_format, controls->avg, avg);

    gwy_unitool_update_label(state->value_format, controls->min, min);
    gwy_unitool_update_label(state->value_format, controls->max, max);
    gwy_unitool_update_label(state->value_format, controls->median, median);
    gwy_unitool_update_label(controls->vform2, controls->projarea, projarea);
    if (controls->same_units) {
        gwy_unitool_update_label(controls->vform2, controls->area, area);
        gwy_unitool_update_label(controls->vformdeg, controls->theta,
                                 180.0/G_PI * theta);
        gwy_unitool_update_label(controls->vformdeg, controls->phi,
                                 180.0/G_PI * phi);
    } else {
        gtk_label_set_text(GTK_LABEL(controls->area), _("N.A."));
        gtk_label_set_text(GTK_LABEL(controls->theta), _("N.A."));
        gtk_label_set_text(GTK_LABEL(controls->phi), _("N.A."));
    }
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    ToolControls *controls;

    controls = (ToolControls*)state->user_data;
    if (controls->vform2)
        gwy_si_unit_value_format_free(controls->vform2);
    memset(state->user_data, 0, sizeof(ToolControls));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

