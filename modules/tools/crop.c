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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwystock.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_CROP            (gwy_tool_crop_get_type())
#define GWY_TOOL_CROP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_CROP, GwyToolCrop))
#define GWY_IS_TOOL_CROP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_CROP))
#define GWY_TOOL_CROP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_CROP, GwyToolCropClass))

typedef struct _GwyToolCrop      GwyToolCrop;
typedef struct _GwyToolCropClass GwyToolCropClass;

typedef struct {
    gboolean keep_offsets;
    gboolean new_channel;
} ToolArgs;

struct _GwyToolCrop {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GwyPlainToolRectLabels *rlabels;
    GtkWidget *keep_offsets;
    GtkWidget *new_channel;
    GtkWidget *clear;
    GtkWidget *apply;

    gulong selection_id;

    /* potential class data */
    GType layer_type_rect;
};

struct _GwyToolCropClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register                  (void);

static GType  gwy_tool_crop_get_type            (void) G_GNUC_CONST;
static void   gwy_tool_crop_finalize            (GObject *object);
static void   gwy_tool_crop_init_dialog         (GwyToolCrop *tool);
static void   gwy_tool_crop_data_switched       (GwyTool *gwytool,
                                                 GwyDataView *data_view);
static void   gwy_tool_crop_data_changed        (GwyPlainTool *plain_tool);
static void   gwy_tool_crop_response            (GwyTool *tool,
                                                 gint response_id);
static void   gwy_tool_crop_selection_changed   (GwySelection *selection,
                                                 gint hint,
                                                 GwyToolCrop *tool);
static void   gwy_tool_crop_keep_offsets_toggled(GwyToolCrop *tool,
                                                 GtkToggleButton *toggle);
static void   gwy_tool_crop_new_data_toggled    (GwyToolCrop *tool,
                                                 GtkToggleButton *toggle);
static void   gwy_tool_crop_apply               (GwyToolCrop *tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Crop tool, crops data to smaller size."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

static const gchar keep_offsets_key[] = "/module/crop/keep_offsets";
static const gchar new_channel_key[]  = "/module/crop/new_channel";

static const ToolArgs default_args = {
    FALSE,
    TRUE,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolCrop, gwy_tool_crop, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_CROP);

    return TRUE;
}

static void
gwy_tool_crop_class_init(GwyToolCropClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_crop_finalize;

    tool_class->stock_id = GWY_STOCK_CROP;
    tool_class->title = _("Crop");
    tool_class->tooltip = _("Crop data");
    tool_class->prefix = "/module/crop";
    tool_class->data_switched = gwy_tool_crop_data_switched;
    tool_class->response = gwy_tool_crop_response;

    ptool_class->data_changed = gwy_tool_crop_data_changed;
}

static void
gwy_tool_crop_finalize(GObject *object)
{
    GwyToolCrop *tool;
    GwyPlainTool *plain_tool;
    GwySelection *selection;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(object);
    tool = GWY_TOOL_CROP(object);

    if (plain_tool->layer) {
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        gwy_signal_handler_disconnect(selection, tool->selection_id);
    }

    settings = gwy_app_settings_get();
    gwy_container_set_boolean_by_name(settings, keep_offsets_key,
                                      tool->args.keep_offsets);
    gwy_container_set_boolean_by_name(settings, new_channel_key,
                                      tool->args.new_channel);

    G_OBJECT_CLASS(gwy_tool_crop_parent_class)->finalize(object);
}

static void
gwy_tool_crop_init(GwyToolCrop *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_rect = gwy_plain_tool_check_layer_type(plain_tool,
                                                           "GwyLayerRectangle");
    if (!tool->layer_type_rect)
        return;

    plain_tool->unit_style = GWY_SI_UNIT_FORMAT_VFMARKUP;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_set_boolean_by_name(settings, keep_offsets_key,
                                      tool->args.keep_offsets);
    gwy_container_set_boolean_by_name(settings, new_channel_key,
                                      tool->args.new_channel);

    gwy_tool_crop_init_dialog(tool);
}

static void
gwy_tool_crop_init_dialog(GwyToolCrop *tool)
{
    GtkDialog *dialog;
    GtkWidget *table;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);

    tool->rlabels = gwy_plain_tool_rect_labels_new(FALSE);
    gtk_box_pack_start(GTK_BOX(dialog->vbox),
                       gwy_plain_tool_rect_labels_get_table(tool->rlabels),
                       TRUE, TRUE, 0);

    table = gtk_table_new(2, 1, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), table, FALSE, FALSE, 0);

    tool->keep_offsets
        = gtk_check_button_new_with_mnemonic(_("Keep lateral offsets"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->keep_offsets),
                                 tool->args.keep_offsets);
    gtk_table_attach(GTK_TABLE(table), tool->keep_offsets, 0, 1, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(tool->keep_offsets, "toggled",
                             G_CALLBACK(gwy_tool_crop_keep_offsets_toggled),
                             tool);

    tool->new_channel
        = gtk_check_button_new_with_mnemonic(_("Create new channel"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->new_channel),
                                 tool->args.new_channel);
    gtk_table_attach(GTK_TABLE(table), tool->new_channel, 0, 1, 1, 2,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(tool->new_channel, "toggled",
                             G_CALLBACK(gwy_tool_crop_new_data_toggled),
                             tool);

    tool->clear = gtk_dialog_add_button(dialog, GTK_STOCK_CLEAR,
                                        GWY_TOOL_RESPONSE_CLEAR);
    gwy_tool_add_hide_button(GWY_TOOL(tool), FALSE);
    tool->apply = gtk_dialog_add_button(dialog, GTK_STOCK_APPLY,
                                        GTK_RESPONSE_APPLY);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);

    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_crop_data_switched(GwyTool *gwytool,
                            GwyDataView *data_view)
{
    GwyToolCrop *tool;
    GwySelection *selection;
    GwyPlainTool *plain_tool;

    GWY_TOOL_CLASS(gwy_tool_crop_parent_class)->data_switched(gwytool,
                                                              data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_CROP(gwytool);
    if (plain_tool->layer) {
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        gwy_signal_handler_disconnect(selection, tool->selection_id);
    }
    if (!data_view) {
        gtk_widget_set_sensitive(tool->clear, FALSE);
        gtk_widget_set_sensitive(tool->apply, FALSE);
        gwy_plain_tool_rect_labels_fill(tool->rlabels, NULL, NULL, NULL, NULL);
        return;
    }

    gwy_plain_tool_assure_layer(plain_tool, tool->layer_type_rect);
    gwy_plain_tool_set_selection_key(plain_tool, "rectangle");
    g_object_set(plain_tool->layer,
                 "draw-reflection", FALSE,
                 "is-crop", TRUE,
                 NULL);
    selection = gwy_vector_layer_get_selection(plain_tool->layer);
    gwy_selection_set_max_objects(selection, 1);
    tool->selection_id
        = g_signal_connect(selection, "changed",
                           G_CALLBACK(gwy_tool_crop_selection_changed), tool);

    gwy_tool_crop_data_changed(plain_tool);
}

static void
gwy_tool_crop_data_changed(GwyPlainTool *plain_tool)
{
    GwySelection *selection;
    GwyToolCrop *tool;

    tool = GWY_TOOL_CROP(plain_tool);
    selection = gwy_vector_layer_get_selection(plain_tool->layer);

    gwy_plain_tool_rect_labels_fill(tool->rlabels, selection,
                                    plain_tool->data_field, NULL, NULL);
    gwy_tool_crop_selection_changed(selection, 0, tool);
}

static void
gwy_tool_crop_response(GwyTool *tool,
                       gint response_id)
{
    GwyPlainTool *plain_tool;
    GwySelection *selection;

    switch (response_id) {
        case GWY_TOOL_RESPONSE_CLEAR:
        plain_tool = GWY_PLAIN_TOOL(tool);
        selection = gwy_vector_layer_get_selection(plain_tool->layer);
        gwy_selection_clear(selection);
        break;

        case GTK_RESPONSE_APPLY:
        gwy_tool_crop_apply(GWY_TOOL_CROP(tool));
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
gwy_tool_crop_selection_changed(GwySelection *selection,
                                gint hint,
                                GwyToolCrop *tool)
{
    gint n;

    g_return_if_fail(hint <= 0);
    n = gwy_selection_get_data(selection, NULL);
    g_return_if_fail(n == 0 || n == 1);

    gwy_plain_tool_rect_labels_fill(tool->rlabels, selection,
                                    GWY_PLAIN_TOOL(tool)->data_field,
                                    NULL, NULL);
    gtk_widget_set_sensitive(tool->apply, n);
    gtk_widget_set_sensitive(tool->clear, n);
}

static void
gwy_tool_crop_keep_offsets_toggled(GwyToolCrop *tool,
                                   GtkToggleButton *toggle)
{
    tool->args.keep_offsets = gtk_toggle_button_get_active(toggle);
}

static void
gwy_tool_crop_new_data_toggled(GwyToolCrop *tool,
                               GtkToggleButton *toggle)
{
    tool->args.new_channel = gtk_toggle_button_get_active(toggle);
}

static void
gwy_tool_crop_one_field(GwyDataField *dfield,
                        const gint *isel,
                        const gdouble *sel,
                        gboolean keep_offsets)
{
    gwy_data_field_resize(dfield, isel[0], isel[1], isel[2], isel[3]);

    if (keep_offsets) {
        gdouble xoff, yoff;

        xoff = gwy_data_field_get_xoffset(dfield);
        yoff = gwy_data_field_get_yoffset(dfield);
        gwy_data_field_set_xoffset(dfield, sel[0] + xoff);
        gwy_data_field_set_yoffset(dfield, sel[1] + yoff);
    }
    else {
        gwy_data_field_set_xoffset(dfield, 0.0);
        gwy_data_field_set_yoffset(dfield, 0.0);
    }
}

static void
gwy_tool_crop_apply(GwyToolCrop *tool)
{
    GwyPlainTool *plain_tool;
    GwySelection *selection;
    GwyContainer *container;
    GwyDataField *dfield;
    GQuark quarks[3];
    gdouble sel[4];
    gchar key[24];
    gint isel[4];
    gint id;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->id >= 0 && plain_tool->data_field != NULL);

    selection = gwy_vector_layer_get_selection(plain_tool->layer);
    if (!gwy_selection_get_object(selection, 0, sel)) {
        g_warning("Apply invoked when no selection is present");
        return;
    }

    isel[0] = gwy_data_field_rtoj(plain_tool->data_field, sel[0]);
    isel[1] = gwy_data_field_rtoi(plain_tool->data_field, sel[1]);
    isel[2] = gwy_data_field_rtoj(plain_tool->data_field, sel[2]) + 1;
    isel[3] = gwy_data_field_rtoi(plain_tool->data_field, sel[3]) + 1;

    container = plain_tool->container;
    if (tool->args.new_channel) {
        dfield = gwy_data_field_duplicate(plain_tool->data_field);
        gwy_tool_crop_one_field(dfield, isel, sel, tool->args.keep_offsets);
        id = gwy_app_data_browser_add_data_field(dfield, container, TRUE);
        g_object_unref(dfield);
        gwy_app_copy_data_items(plain_tool->container, container,
                                plain_tool->id, id,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_RANGE_TYPE,
                                GWY_DATA_ITEM_MASK_COLOR,
                                0);
        gwy_app_set_data_field_title(container, id, _("Detail"));

        if (plain_tool->mask_field) {
            dfield = gwy_data_field_duplicate(plain_tool->mask_field);
            gwy_tool_crop_one_field(dfield, isel, sel, tool->args.keep_offsets);
            quarks[1] = gwy_app_get_mask_key_for_id(id);
            gwy_container_set_object(container, quarks[1], dfield);
            g_object_unref(dfield);
        }

        if (plain_tool->show_field) {
            dfield = gwy_data_field_duplicate(plain_tool->show_field);
            gwy_tool_crop_one_field(dfield, isel, sel, tool->args.keep_offsets);
            quarks[2] = gwy_app_get_presentation_key_for_id(id);
            gwy_container_set_object(container, quarks[2], dfield);
            g_object_unref(dfield);
        }
    }
    else {
        quarks[0] = gwy_app_get_data_key_for_id(plain_tool->id);
        quarks[1] = quarks[2] = 0;
        if (plain_tool->mask_field)
            quarks[1] = gwy_app_get_mask_key_for_id(plain_tool->id);
        if (plain_tool->show_field)
            quarks[2] = gwy_app_get_presentation_key_for_id(plain_tool->id);
        gwy_app_undo_qcheckpointv(container, G_N_ELEMENTS(quarks), quarks);

        gwy_tool_crop_one_field(plain_tool->data_field, isel, sel,
                                tool->args.keep_offsets);
        gwy_data_field_data_changed(plain_tool->data_field);

        if (plain_tool->mask_field) {
            gwy_tool_crop_one_field(plain_tool->mask_field, isel, sel,
                                    tool->args.keep_offsets);
            gwy_data_field_data_changed(plain_tool->data_field);
        }

        if (plain_tool->show_field) {
            gwy_tool_crop_one_field(plain_tool->show_field, isel, sel,
                                    tool->args.keep_offsets);
            gwy_data_field_data_changed(plain_tool->data_field);
        }

        g_snprintf(key, sizeof(key), "/%d/select", plain_tool->id);
        gwy_container_remove_by_prefix(container, key);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
