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
#include <app/settings.h>
#include <app/unitool.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef struct {
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *w;
    GtkWidget *h;
    GtkWidget *xp;
    GtkWidget *yp;
    GtkWidget *wp;
    GtkWidget *hp;
    GtkWidget *cdo_preview;
    gboolean do_preview;

    gboolean in_update;
} ToolControls;

static gboolean   module_register    (const gchar *name);
static gboolean   use                (GwyDataWindow *data_window,
                                      GwyToolSwitchEvent reason);
static void       layer_setup        (GwyUnitoolState *state);
static GtkWidget* dialog_create      (GwyUnitoolState *state);
static void       dialog_update      (GwyUnitoolState *state,
                                      GwyUnitoolUpdateType reason);
static void       dialog_abandon     (GwyUnitoolState *state);
static void       apply              (GwyUnitoolState *state);
static void       do_preview_updated (GtkWidget *toggle,
                                      GwyUnitoolState *state);
static void       load_args          (GwyContainer *container,
                                      ToolControls *controls);
static void       save_args          (GwyContainer *container,
                                      ToolControls *controls);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "icolorange",
    "Interactive color range tool.",
    "Yeti <yeti@gwyddion.net>",
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
    static GwyToolFuncInfo icolorange_func_info = {
        "icolorange",
        "gwy_graph_palette",  /* FIXME */
        "Stretch color range to part of data.",
        130,
        &use,
    };

    gwy_tool_func_register(name, &icolorange_func_info);

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
        state->apply_doesnt_close = TRUE;
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
    GwyContainer *settings;
    GtkWidget *dialog, *table, *label, *frame;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    dialog = gtk_dialog_new_with_buttons(_("Color Range"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(7, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Origin</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("X"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Y"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3, GTK_FILL, 0, 2, 2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Size</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Width"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Height"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 5, 6, GTK_FILL, 0, 2, 2);

    controls->x = gtk_label_new("");
    controls->y = gtk_label_new("");
    controls->w = gtk_label_new("");
    controls->h = gtk_label_new("");
    controls->xp = gtk_label_new("");
    controls->yp = gtk_label_new("");
    controls->wp = gtk_label_new("");
    controls->hp = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->x), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->y), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->w), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->h), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->xp), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->yp), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->wp), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->hp), 1.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->x, 2, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->y, 2, 3, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->w, 2, 3, 4, 5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->h, 2, 3, 5, 6);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->xp, 3, 4, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->yp, 3, 4, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->wp, 3, 4, 4, 5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->hp, 3, 4, 5, 6);

    controls->cdo_preview
        = gtk_check_button_new_with_mnemonic(_("_Instant apply"));
    g_signal_connect(controls->cdo_preview, "toggled",
                     G_CALLBACK(do_preview_updated), state);
    gtk_table_attach(GTK_TABLE(table), controls->cdo_preview, 0, 3, 6, 7,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    return dialog;
}

static void
dialog_update(GwyUnitoolState *state,
              G_GNUC_UNUSED GwyUnitoolUpdateType reason)
{
    gboolean is_visible, is_selected;
    ToolControls *controls;
    GwySIValueFormat *units;
    gint ximin, yimin, ximax, yimax;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble sel[4];
    gchar buf[16];

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    if (controls->in_update)
        return;

    units = state->coord_format;
    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    is_visible = state->is_visible;
    is_selected = gwy_vector_layer_get_selection(state->layer, sel);
    if (!is_visible && !is_selected) {
        if (reason == GWY_UNITOOL_UPDATED_DATA)
            apply(state);
        return;
    }

    if (is_selected) {
        gwy_unitool_update_label(units, controls->x, sel[0]);
        gwy_unitool_update_label(units, controls->y, sel[1]);
        gwy_unitool_update_label(units, controls->w, sel[2] - sel[0]);
        gwy_unitool_update_label(units, controls->h, sel[3] - sel[1]);
        ximin = gwy_data_field_rtoj(dfield, sel[0]);
        g_snprintf(buf, sizeof(buf), "%d px", ximin);
        gtk_label_set_text(GTK_LABEL(controls->xp), buf);
        yimin = gwy_data_field_rtoi(dfield, sel[1]);
        g_snprintf(buf, sizeof(buf), "%d px", yimin);
        gtk_label_set_text(GTK_LABEL(controls->yp), buf);
        ximax = gwy_data_field_rtoj(dfield, sel[2]) + 1;
        g_snprintf(buf, sizeof(buf), "%d px", ximax - ximin);
        gtk_label_set_text(GTK_LABEL(controls->wp), buf);
        yimax = gwy_data_field_rtoi(dfield, sel[3]) + 1;
        g_snprintf(buf, sizeof(buf), "%d px", yimax - yimin);
        gtk_label_set_text(GTK_LABEL(controls->hp), buf);
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls->x), "");
        gtk_label_set_text(GTK_LABEL(controls->y), "");
        gtk_label_set_text(GTK_LABEL(controls->w), "");
        gtk_label_set_text(GTK_LABEL(controls->h), "");
        gtk_label_set_text(GTK_LABEL(controls->xp), "");
        gtk_label_set_text(GTK_LABEL(controls->yp), "");
        gtk_label_set_text(GTK_LABEL(controls->wp), "");
        gtk_label_set_text(GTK_LABEL(controls->hp), "");
    }
    gwy_unitool_apply_set_sensitive(state, is_selected);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->cdo_preview)))
        apply(state);
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    GwyContainer *settings;
    ToolControls *controls;

    settings = gwy_app_settings_get();
    controls = (ToolControls*)state->user_data;
    save_args(settings, controls);

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
apply(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    gint ximin, yimin, ximax, yimax;
    gdouble sel[4];
    gdouble vmin, vmax;

    controls = (ToolControls*)state->user_data;
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    if (gwy_vector_layer_get_selection(state->layer, sel)) {
        ximin = gwy_data_field_rtoj(dfield, sel[0]);
        yimin = gwy_data_field_rtoi(dfield, sel[1]);
        ximax = gwy_data_field_rtoj(dfield, sel[2]) + 1;
        yimax = gwy_data_field_rtoi(dfield, sel[3]) + 1;

        vmin = gwy_data_field_area_get_min(dfield, ximin, yimin,
                                           ximax - ximin, yimax - yimin);
        vmax = gwy_data_field_area_get_max(dfield, ximin, yimin,
                                           ximax - ximin, yimax - yimin);
        gwy_container_set_double_by_name(data, "/0/base/min", vmin);
        gwy_container_set_double_by_name(data, "/0/base/max", vmax);
    }
    else {
        gwy_container_remove_by_name(data, "/0/base/min");
        gwy_container_remove_by_name(data, "/0/base/max");
    }
    gwy_data_view_update
        (GWY_DATA_VIEW(gwy_data_window_get_data_view
                           (GWY_DATA_WINDOW(state->data_window))));
    controls->in_update = FALSE;
}

static void
do_preview_updated(GtkWidget *toggle, GwyUnitoolState *state)
{
    ToolControls *controls;

    controls = (ToolControls*)state->user_data;
    controls->do_preview
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle));
    if (controls->do_preview)
        apply(state);
}

static const gchar *do_preview_key = "/tool/icolorange/do_preview";

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_container_set_boolean_by_name(container, do_preview_key,
                                      controls->do_preview);
}

static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->do_preview = FALSE;

    gwy_container_gis_boolean_by_name(container, do_preview_key,
                                      &controls->do_preview);

    /* sanitize */
    controls->do_preview = !!controls->do_preview;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

