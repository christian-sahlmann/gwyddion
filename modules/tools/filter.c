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
#include <app/gwyapp.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

typedef struct {
    GwyUnitoolRectLabels labels;
    GtkWidget *filter;
    GtkWidget *direction;
    GtkObject *size;
    GtkWidget *size_spin;
    GtkWidget *update;
    GwyFilterType fil;
    GtkOrientation dir;
    gint siz;
    gboolean upd;
    gboolean data_were_updated;
    gpointer last_preview;
    gint old_isel[4];
    gboolean state_changed;
} ToolControls;

static gboolean   module_register      (const gchar *name);
static gboolean   use                  (GwyDataWindow *data_window,
                                        GwyToolSwitchEvent reason);
static void       layer_setup          (GwyUnitoolState *state);
static GtkWidget* dialog_create        (GwyUnitoolState *state);
static void       dialog_update        (GwyUnitoolState *state,
                                        GwyUnitoolUpdateType reason);
static void       dialog_abandon       (GwyUnitoolState *state);
static void       apply                (GwyUnitoolState *state);
static void       do_apply             (GwyDataField *dfield,
                                        GwyFilterType filter_type,
                                        gint size,
                                        gint direction,
                                        gint *isel);

static void       direction_changed_cb (GObject *item,
                                        GwyUnitoolState *state);
static void       filter_changed_cb    (GObject *item,
                                        GwyUnitoolState *state);
static void       update_changed_cb    (GtkToggleButton *button,
                                        GwyUnitoolState *state);
static void       size_changed_cb      (GwyUnitoolState *state);

static void       load_args            (GwyContainer *container,
                                        ToolControls *controls);
static void       save_args            (GwyContainer *container,
                                        ToolControls *controls);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "filter",
    N_("Basic filtering procedures."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.0",
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
        "filter",
        GWY_STOCK_FILTER,
        N_("Basic filters"),
        49,
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
    ToolControls *controls;

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
    controls = (ToolControls*)state->user_data;
    controls->state_changed = TRUE;

    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;
    GtkWidget *data_view;
    GObject *shadefield;

    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer, "is_crop", FALSE, NULL);

    controls = (ToolControls*)state->user_data;
    gwy_debug("last preview %p\n", controls->last_preview);
    if (controls->last_preview) {
        data_view = gwy_data_window_get_data_view(
                                     GWY_DATA_WINDOW(controls->last_preview));
        g_assert(data_view);
        data = GWY_CONTAINER(gwy_data_view_get_data(GWY_DATA_VIEW(data_view)));
        g_assert(data);
        shadefield = gwy_container_get_object_by_name(data, "/0/show");
        g_object_remove_weak_pointer(shadefield, &controls->last_preview);
        controls->last_preview = NULL;
        gwy_container_remove_by_name(data, "/0/show");
        gwy_app_data_view_update(data_view);
    }
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;
    GtkWidget *dialog, *table, *table2, *label, *frame;

    gwy_debug(" ");

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    dialog = gtk_dialog_new_with_buttons(_("Filters"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);

    table = gtk_table_new(6, 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    gwy_unitool_rect_info_table_setup(&controls->labels,
                                      GTK_TABLE(table), 0, 0);
    controls->labels.unselected_is_full = TRUE;

    table2 = gtk_table_new(4, 2, FALSE);

    gtk_container_set_border_width(GTK_CONTAINER(table2), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table2);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Filter</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table2), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);

    controls->filter
        = gwy_option_menu_filter(G_CALLBACK(filter_changed_cb),
                                 state, controls->fil);
    gwy_table_attach_row(table2, 1, _("_Type:"), NULL, controls->filter);

    controls->direction
        = gwy_option_menu_direction(G_CALLBACK(direction_changed_cb),
                                    state, controls->dir);
    gwy_table_attach_row(table2, 2, _("_Direction:"), NULL,
                         controls->direction);
    label = gwy_table_get_child_widget(table2, 2, 0);

    /* TODO uncomment this when some directional filter is avalilable
     * TODO also convert to radiobutton then, as in polynom.c
    if (controls->fil == GWY_FILTER_SOBEL || controls->fil == GWY_FILTER_PREWITT)
        gtk_widget_set_sensitive(controls->direction, TRUE);
    else
    */
    gtk_widget_set_sensitive(controls->direction, FALSE);
    gtk_widget_set_sensitive(label, FALSE);

    controls->size = gtk_adjustment_new(controls->siz, 1, 20, 1, 5, 0);
    controls->size_spin = gwy_table_attach_spinbutton(table2, 3,
                                                      _("Si_ze:"), "px",
                                                      controls->size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size), controls->siz);
    g_signal_connect_swapped(controls->size, "value-changed",
                             G_CALLBACK(size_changed_cb), state);

    controls->update
        = gtk_check_button_new_with_mnemonic(_("_Update preview dynamically"));
    gtk_table_attach(GTK_TABLE(table2), controls->update, 0, 3, 4, 5,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 controls->upd);
    g_signal_connect(controls->update, "toggled",
                     G_CALLBACK(update_changed_cb), state);

    return dialog;
}

/* TODO */
static void
apply(GwyUnitoolState *state)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    ToolControls *controls;
    gint isel[4];

    gwy_debug(" ");
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    controls = (ToolControls*)state->user_data;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));

    gwy_app_clean_up_data(data);
    gwy_container_remove_by_name(data, "/0/show");
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    gwy_unitool_rect_info_table_fill(state, &controls->labels, NULL, isel);
    controls->siz = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->size));

    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    do_apply(dfield, controls->fil, controls->siz, controls->dir, isel);

    gwy_vector_layer_unselect(state->layer);
    gwy_data_view_update(GWY_DATA_VIEW(layer->parent));
}

static void
dialog_update(GwyUnitoolState *state,
              GwyUnitoolUpdateType reason)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *shadefield, *dfield;
    GwyDataViewLayer *layer;
    gint isel[4];
    gint i;

    gwy_debug(" ");

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    gwy_unitool_rect_info_table_fill(state, &controls->labels, NULL, isel);
    controls->siz = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->size));

    for (i = 0; i < 4; i++) {
        if (isel[i] != controls->old_isel[i]) {
            memcpy(controls->old_isel, isel, 4*sizeof(gint));
            controls->state_changed = TRUE;
            break;
        }
    }

    if (reason == GWY_UNITOOL_UPDATED_DATA
        && controls->data_were_updated == FALSE) {
        controls->state_changed = TRUE;
        controls->data_were_updated = TRUE;
    }
    else {
        controls->data_were_updated = FALSE;
    }


    if (controls->state_changed) {
        if (gwy_container_contains_by_name(data, "/0/show")) {
            shadefield
                = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                  "/0/show"));
            gwy_data_field_resample(shadefield,
                                    gwy_data_field_get_xres(dfield),
                                    gwy_data_field_get_yres(dfield),
                                    GWY_INTERPOLATION_NONE);
            gwy_data_field_copy(dfield, shadefield);
        }
        else {
            shadefield
                = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
            gwy_container_set_object_by_name(data, "/0/show",
                                             G_OBJECT(shadefield));
            g_object_unref(shadefield);

            g_assert(!controls->last_preview);
            controls->last_preview = state->data_window;
            gwy_debug("setting last preview %p", controls->last_preview);
            g_object_add_weak_pointer(G_OBJECT(shadefield),
                                      &controls->last_preview);
        }
        g_object_set_data(G_OBJECT(shadefield), "is_preview",
                          GINT_TO_POINTER(TRUE));

        if (controls->upd)
            do_apply(shadefield,
                     controls->fil, controls->siz, controls->dir, isel);

        controls->state_changed = FALSE;
        gwy_data_view_update(GWY_DATA_VIEW(layer->parent));
    }
}

static void
do_apply(GwyDataField *dfield,
         GwyFilterType filter_type,
         gint size,
         G_GNUC_UNUSED gint direction,
         gint *isel)
{
    switch (filter_type) {
        case GWY_FILTER_MEAN:
        gwy_data_field_area_filter_mean(dfield, size,
                                        isel[0], isel[1],
                                        isel[2]-isel[0],
                                        isel[3]-isel[1]);
        break;

        case GWY_FILTER_MEDIAN:
        gwy_data_field_area_filter_median(dfield, size,
                                          isel[0], isel[1],
                                          isel[2]-isel[0],
                                          isel[3]-isel[1]);
        break;

        case GWY_FILTER_MINIMUM:
        gwy_data_field_area_filter_minimum(dfield, size,
                                           isel[0], isel[1],
                                           isel[2]-isel[0],
                                           isel[3]-isel[1]);
        break;

        case GWY_FILTER_MAXIMUM:
        gwy_data_field_area_filter_maximum(dfield, size,
                                           isel[0], isel[1],
                                           isel[2]-isel[0],
                                           isel[3]-isel[1]);
        break;

        case GWY_FILTER_CONSERVATIVE:
        gwy_data_field_area_filter_conservative(dfield, size,
                                                isel[0], isel[1],
                                                isel[2]-isel[0],
                                                isel[3]-isel[1]);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
dialog_abandon(GwyUnitoolState *state)
{

    GwyContainer *settings;
    GwyContainer *data;
    ToolControls *controls;
    GwyDataViewLayer *layer;

    settings = gwy_app_settings_get();
    controls = (ToolControls*)state->user_data;
    save_args(settings, controls);
    if (!state->data_window)
        return;

    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));

    gwy_container_remove_by_name(data, "/0/show");
    gwy_data_view_update(GWY_DATA_VIEW(layer->parent));

    memset(state->user_data, 0, sizeof(ToolControls));
}

static void
direction_changed_cb(GObject *item,
                     GwyUnitoolState *state)
{
    ToolControls *controls;

    gwy_debug(" ");

    controls = (ToolControls*)state->user_data;
    controls->dir = GPOINTER_TO_INT(g_object_get_data(item, "direction-type"));
    controls->state_changed = TRUE;
    dialog_update(state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
filter_changed_cb(GObject *item, GwyUnitoolState *state)
{
    ToolControls *controls;
    gboolean direction_sensitive = FALSE;
    gboolean size_sensitive = FALSE;

    gwy_debug(" ");
    controls = (ToolControls*)state->user_data;
    controls->fil = GPOINTER_TO_INT(g_object_get_data(item, "filter-type"));
    controls->state_changed = TRUE;

    switch (controls->fil) {
        case GWY_FILTER_LAPLACIAN:
        break;

        /*TODO put here directional filters, if there are any*/
/*        case GWY_FILTER_SOBEL:
        case GWY_FILTER_PREWITT:
        direction_sensitive = TRUE;
        break;
*/
        case GWY_FILTER_MEAN:
        case GWY_FILTER_MEDIAN:
        case GWY_FILTER_CONSERVATIVE:
        case GWY_FILTER_MINIMUM:
        case GWY_FILTER_MAXIMUM:
        size_sensitive = TRUE;
        break;

        default:
        g_assert_not_reached();
        break;
    }
    gtk_widget_set_sensitive(controls->direction, direction_sensitive);
    gtk_widget_set_sensitive(controls->size_spin, size_sensitive);

    dialog_update(state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
update_changed_cb(GtkToggleButton *button, GwyUnitoolState *state)
{
    ToolControls *controls;

    gwy_debug(" ");
    controls = (ToolControls*)state->user_data;
    controls->upd = gtk_toggle_button_get_active(button);
    controls->state_changed = TRUE;
    dialog_update(state, GWY_UNITOOL_UPDATED_CONTROLS);
}

static void
size_changed_cb(GwyUnitoolState *state)
{
    ToolControls *controls;

    gwy_debug(" ");
    controls = (ToolControls*)state->user_data;
    controls->siz = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->size));
    if (controls->upd) {
        controls->state_changed = TRUE;
        dialog_update(state, GWY_UNITOOL_UPDATED_CONTROLS);
    }
}


static const gchar *upd_key = "/tool/filter/update";
static const gchar *siz_key = "/tool/filter/size";
static const gchar *fil_key = "/tool/filter/filter";
static const gchar *dir_key = "/tool/filter/direction";

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    /* TODO: remove someday, old misnamed keys */
    gwy_container_remove_by_name(container, "/tool/profile/size");
    gwy_container_remove_by_name(container, "/tool/profile/filter");
    gwy_container_remove_by_name(container, "/tool/profile/direction");

    gwy_container_set_boolean_by_name(container, upd_key, controls->upd);
    gwy_container_set_int32_by_name(container, siz_key, controls->siz);
    gwy_container_set_enum_by_name(container, fil_key, controls->fil);
    gwy_container_set_enum_by_name(container, dir_key, controls->dir);
}

static void
load_args(GwyContainer *container, ToolControls *controls)
{
    controls->upd = FALSE;
    controls->siz = 6;
    controls->fil = GWY_FILTER_MEAN;
    controls->dir = GTK_ORIENTATION_HORIZONTAL;

    gwy_container_gis_boolean_by_name(container, upd_key, &controls->upd);
    gwy_container_gis_int32_by_name(container, siz_key, &controls->siz);
    gwy_container_gis_enum_by_name(container, fil_key, &controls->fil);
    gwy_container_gis_enum_by_name(container, dir_key, &controls->dir);

    /* sanitize */
    controls->upd = !!controls->upd;
    controls->siz = CLAMP(controls->siz, 1, 20);
    controls->fil = MIN(controls->fil, GWY_FILTER_MAXIMUM);
    controls->dir = MIN(controls->dir, GTK_ORIENTATION_VERTICAL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

