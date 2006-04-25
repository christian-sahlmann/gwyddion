/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/app.h>
#include <app/gwyplaintool.h>

#define ITEM_CHANGED "item-changed::"

static void gwy_plain_tool_finalize           (GObject *object);
static void gwy_plain_tool_show               (GwyTool *tool);
static void gwy_plain_tool_hide               (GwyTool *tool);
static void gwy_plain_tool_data_switched      (GwyTool *tool,
                                               GwyDataView *data_view);
static void gwy_plain_tool_reconnect_container(GwyPlainTool *plain_tool,
                                               GwyDataView *data_view);
static void gwy_plain_tool_data_item_changed  (GwyContainer *container,
                                               GQuark quark,
                                               GwyPlainTool *plain_tool);
static void gwy_plain_tool_mask_item_changed  (GwyContainer *container,
                                               GQuark quark,
                                               GwyPlainTool *plain_tool);
static void gwy_plain_tool_show_item_changed  (GwyContainer *container,
                                               GQuark quark,
                                               GwyPlainTool *plain_tool);
static void gwy_plain_tool_data_changed       (GwyPlainTool *plain_tool);
static void gwy_plain_tool_mask_changed       (GwyPlainTool *plain_tool);
static void gwy_plain_tool_show_changed       (GwyPlainTool *plain_tool);
static void gwy_plain_tool_update_units       (GwyPlainTool *plain_tool);

G_DEFINE_ABSTRACT_TYPE(GwyPlainTool, gwy_plain_tool, GWY_TYPE_TOOL)

static void
gwy_plain_tool_class_init(GwyPlainToolClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);

    gobject_class->finalize = gwy_plain_tool_finalize;

    tool_class->hide = gwy_plain_tool_hide;
    tool_class->show = gwy_plain_tool_show;
    tool_class->data_switched = gwy_plain_tool_data_switched;
}

static void
gwy_plain_tool_finalize(GObject *object)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(object);
    gwy_plain_tool_reconnect_container(plain_tool, NULL);

    if (plain_tool->coord_format)
        gwy_si_unit_value_format_free(plain_tool->coord_format);
    if (plain_tool->value_format)
        gwy_si_unit_value_format_free(plain_tool->value_format);

    G_OBJECT_CLASS(gwy_plain_tool_parent_class)->finalize(object);
}

static void
gwy_plain_tool_init(G_GNUC_UNUSED GwyPlainTool *tool)
{
}

static void
gwy_plain_tool_show(GwyTool *tool)
{
    GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->show(tool);
}

static void
gwy_plain_tool_hide(GwyTool *tool)
{
    GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->hide(tool);
}

void
gwy_plain_tool_data_switched(GwyTool *tool,
                             GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;

    gwy_debug("%p", data_view);
    if (GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->data_switched)
        GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->data_switched(tool,
                                                                   data_view);

    plain_tool = GWY_PLAIN_TOOL(tool);
    gwy_plain_tool_reconnect_container(plain_tool, data_view);
    gwy_plain_tool_update_units(plain_tool);
}

/**
 * gwy_plain_tool_reconnect_container:
 * @plain_tool: A plain tool.
 * @data_view: A new data view to switch the tool to.
 *
 * Performs signal diconnection and reconnection when data is swtiched.
 *
 * The @data_view and @container fields have to still point to the old
 * objects (or be %NULL).
 **/
static void
gwy_plain_tool_reconnect_container(GwyPlainTool *plain_tool,
                                   GwyDataView *data_view)
{
    GwyPixmapLayer *layer;
    const gchar *data_key;
    gchar *key, *sigdetail;
    guint len;

    gwy_signal_handler_disconnect(plain_tool->data_field, plain_tool->data_id);
    gwy_signal_handler_disconnect(plain_tool->mask_field, plain_tool->mask_id);
    gwy_signal_handler_disconnect(plain_tool->show_field, plain_tool->show_id);

    gwy_signal_handler_disconnect(plain_tool->container,
                                  plain_tool->data_item_id);
    gwy_signal_handler_disconnect(plain_tool->container,
                                  plain_tool->mask_item_id);
    gwy_signal_handler_disconnect(plain_tool->container,
                                  plain_tool->show_item_id);

    gwy_object_unref(plain_tool->data_field);
    gwy_object_unref(plain_tool->mask_field);
    gwy_object_unref(plain_tool->show_field);
    gwy_object_unref(plain_tool->container);

    if (!(plain_tool->data_view = data_view))
        return;

    plain_tool->container = gwy_data_view_get_data(data_view);
    g_object_ref(plain_tool->container);
    layer = gwy_data_view_get_base_layer(data_view);
    data_key = gwy_pixmap_layer_get_data_key(layer);

    /* @sigdetail has the form "item-changed::/0/data", @key is a pointer to
     * the key part.  The "data" tail is subsequently replaced with "mask"
     * and "show". */
    len = strlen(data_key);
    g_return_if_fail(len > 5 && gwy_strequal(data_key + len-5, "/data"));
    len += sizeof(ITEM_CHANGED)-1;
    sigdetail = g_new(gchar, len+1);
    key = sigdetail + sizeof(ITEM_CHANGED)-1;

    strcpy(sigdetail, ITEM_CHANGED);
    strcpy(sigdetail + sizeof(ITEM_CHANGED)-1, data_key);
    plain_tool->data_item_id
        = g_signal_connect(plain_tool->container, sigdetail,
                           G_CALLBACK(gwy_plain_tool_data_item_changed),
                           plain_tool);
    if (gwy_container_gis_object_by_name(plain_tool->container, key,
                                         &plain_tool->data_field)) {
        g_object_ref(plain_tool->data_field);
        plain_tool->data_id
            = g_signal_connect_swapped(plain_tool->data_field, "data-changed",
                                       G_CALLBACK(gwy_plain_tool_data_changed),
                                       plain_tool);
    }

    strcpy(sigdetail + len-4, "mask");
    plain_tool->mask_item_id
        = g_signal_connect(plain_tool->container, sigdetail,
                           G_CALLBACK(gwy_plain_tool_mask_item_changed),
                           plain_tool);
    if (gwy_container_gis_object_by_name(plain_tool->container, key,
                                         &plain_tool->mask_field)) {
        g_object_ref(plain_tool->mask_field);
        plain_tool->mask_id
            = g_signal_connect_swapped(plain_tool->mask_field, "data-changed",
                                       G_CALLBACK(gwy_plain_tool_mask_changed),
                                       plain_tool);
    }

    strcpy(sigdetail + len-4, "show");
    plain_tool->show_item_id
        = g_signal_connect(plain_tool->container, sigdetail,
                           G_CALLBACK(gwy_plain_tool_show_item_changed),
                           plain_tool);
    if (gwy_container_gis_object_by_name(plain_tool->container, key,
                                         &plain_tool->show_field)) {
        g_object_ref(plain_tool->show_field);
        plain_tool->show_id
            = g_signal_connect_swapped(plain_tool->show_field, "data-changed",
                                       G_CALLBACK(gwy_plain_tool_show_changed),
                                       plain_tool);
    }

    g_free(sigdetail);
}

static void
gwy_plain_tool_data_item_changed(GwyContainer *container,
                                 GQuark quark,
                                 GwyPlainTool *plain_tool)
{
    gwy_signal_handler_disconnect(plain_tool->data_field, plain_tool->data_id);
    gwy_object_unref(plain_tool->data_field);

    if (gwy_container_gis_object(container, quark, &plain_tool->data_field)) {
        g_object_ref(plain_tool->data_field);
        plain_tool->data_id
            = g_signal_connect_swapped(plain_tool->data_field, "data-changed",
                                       G_CALLBACK(gwy_plain_tool_data_changed),
                                       plain_tool);
    }

    gwy_plain_tool_data_changed(plain_tool);
}

static void
gwy_plain_tool_mask_item_changed(GwyContainer *container,
                                 GQuark quark,
                                 GwyPlainTool *plain_tool)
{
    gwy_signal_handler_disconnect(plain_tool->mask_field, plain_tool->mask_id);
    gwy_object_unref(plain_tool->mask_field);

    if (gwy_container_gis_object(container, quark, &plain_tool->mask_field)) {
        g_object_ref(plain_tool->mask_field);
        plain_tool->mask_id
            = g_signal_connect_swapped(plain_tool->mask_field, "data-changed",
                                       G_CALLBACK(gwy_plain_tool_mask_changed),
                                       plain_tool);
    }

    gwy_plain_tool_mask_changed(plain_tool);
}

static void
gwy_plain_tool_show_item_changed(GwyContainer *container,
                                 GQuark quark,
                                 GwyPlainTool *plain_tool)
{
    gwy_signal_handler_disconnect(plain_tool->show_field, plain_tool->show_id);
    gwy_object_unref(plain_tool->show_field);

    if (gwy_container_gis_object(container, quark, &plain_tool->show_field)) {
        g_object_ref(plain_tool->show_field);
        plain_tool->show_id
            = g_signal_connect_swapped(plain_tool->show_field, "data-changed",
                                       G_CALLBACK(gwy_plain_tool_show_changed),
                                       plain_tool);
    }

    gwy_plain_tool_show_changed(plain_tool);
}

static void
gwy_plain_tool_data_changed(GwyPlainTool *plain_tool)
{
    GwyPlainToolClass *klass;

    gwy_plain_tool_update_units(plain_tool);

    klass = GWY_PLAIN_TOOL_GET_CLASS(plain_tool);
    if (klass->data_changed)
        klass->data_changed(plain_tool);
}

static void
gwy_plain_tool_mask_changed(GwyPlainTool *plain_tool)
{
    GwyPlainToolClass *klass;

    klass = GWY_PLAIN_TOOL_GET_CLASS(plain_tool);
    if (klass->mask_changed)
        klass->mask_changed(plain_tool);
}

static void
gwy_plain_tool_show_changed(GwyPlainTool *plain_tool)
{
    GwyPlainToolClass *klass;

    klass = GWY_PLAIN_TOOL_GET_CLASS(plain_tool);
    if (klass->show_changed)
        klass->show_changed(plain_tool);
}

/**
 * gwy_plain_tool_update_units:
 * @plain_tool: A plain tool.
 *
 * Updates plain tool's unit formats.
 *
 * More precisely, @coord_format and @value_format are updated according to
 * the current data field and @unit_style.  If @unit_style is
 * %GWY_SI_UNIT_FORMAT_NONE existing formats are destroyed and set to %NULL.
 **/
static void
gwy_plain_tool_update_units(GwyPlainTool *plain_tool)
{
    g_return_if_fail(GWY_IS_PLAIN_TOOL(plain_tool));
    g_return_if_fail(GWY_IS_DATA_FIELD(plain_tool->data_field));

    if (plain_tool->unit_style) {
        plain_tool->coord_format
            = gwy_data_field_get_value_format_xy(plain_tool->data_field,
                                                 plain_tool->unit_style,
                                                 plain_tool->coord_format);
        plain_tool->value_format
            = gwy_data_field_get_value_format_z(plain_tool->data_field,
                                                plain_tool->unit_style,
                                                plain_tool->value_format);
    }
    else {
        if (plain_tool->coord_format) {
            gwy_si_unit_value_format_free(plain_tool->coord_format);
            plain_tool->coord_format = NULL;
        }
        else if (plain_tool->value_format) {
            gwy_si_unit_value_format_free(plain_tool->value_format);
            plain_tool->value_format = NULL;
        }
    }
}

/**
 * gwy_plain_tool_check_layer_type:
 * @plain_tool: A plain tool.
 * @name: Layer type name (e.g. <literal>"GwyLayerPoint"</literal>).
 *
 * Checks for a required layer type.
 *
 * If the layer exists, its #GType is returned.  If it does not exist, zero
 * is returned and a warning message is added to the tool dialog.  In addition,
 * it sets @init_failed to %TRUE.
 *
 * Therefore, this function should be called early in tool instance
 * initialization and it should not be called again once it fails.
 *
 * Returns: The type of the layer, or 0 on failure.
 **/
GType
gwy_plain_tool_check_layer_type(GwyPlainTool *plain_tool,
                                const gchar *name)
{
    GtkWidget *label;
    GtkBox *vbox;
    GType type;
    gchar *s;

    g_return_val_if_fail(GWY_IS_PLAIN_TOOL(plain_tool), 0);
    g_return_val_if_fail(name, 0);

    if (plain_tool->init_failed) {
        g_warning("Tool layer check already failed.");
        return 0;
    }

    type = g_type_from_name(name);
    if (type)
        return type;

    plain_tool->init_failed = TRUE;

    vbox = GTK_BOX(GTK_DIALOG(GWY_TOOL(plain_tool)->dialog)->vbox);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label),
                         _("<big><b>Missing layer module.</b></big>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(label), 12, 0);
    gtk_box_pack_start(vbox, label, FALSE, FALSE, 6);

    label = gtk_label_new(NULL);
    s = g_strdup_printf(_("This tool requires layer of type %s to work, "
                          "which does not seem to be installed.  "
                          "Please check your installation."),
                        name);
    gtk_label_set_markup(GTK_LABEL(label), s);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(label), 12, 0);
    gtk_box_pack_start(vbox, label, FALSE, FALSE, 6);
    g_free(s);

    gwy_tool_add_hide_button(GWY_TOOL(plain_tool), TRUE);

    gtk_widget_show_all(GTK_WIDGET(vbox));

    return 0;
}

/**
 * gwy_plain_tool_set_selection_key:
 * @plain_tool: A plain tool.
 * @bname: Selection key base name, for example <literal>"line"</literal>.
 *
 * Constructs selection key from data key and sets it on the vector layer.
 **/
void
gwy_plain_tool_set_selection_key(GwyPlainTool *plain_tool,
                                 const gchar *bname)
{
    GwyPixmapLayer *layer;
    GwyContainer *container;
    const gchar *data_key;
    gchar *key;
    guint len;

    g_return_if_fail(GWY_IS_PLAIN_TOOL(plain_tool));
    g_return_if_fail(GWY_IS_DATA_VIEW(plain_tool->data_view));
    g_return_if_fail(GWY_IS_VECTOR_LAYER(plain_tool->layer));
    g_return_if_fail(bname);

    container = gwy_data_view_get_data(plain_tool->data_view);
    layer = gwy_data_view_get_base_layer(plain_tool->data_view);
    data_key = gwy_pixmap_layer_get_data_key(layer);
    gwy_debug("data_key: <%s>", data_key);
    len = strlen(data_key);
    g_return_if_fail(len > 5 && gwy_strequal(data_key + len-5, "/data"));

    key = g_strdup_printf("%.*s/select/%s", len-4, data_key, bname);
    gwy_vector_layer_set_selection_key(plain_tool->layer, key);
    g_free(key);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyplaintool
 * @title: GwyPlainTool
 * @short_description: Base class for simple tools
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
