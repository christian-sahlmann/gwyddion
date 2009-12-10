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
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/elliptic.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/menu.h>
#include <app/gwyplaintool.h>

#define ITEM_CHANGED "item-changed::"

enum {
    RLABEL_X, RLABEL_Y, RLABEL_W, RLABEL_H, NRLABELS
};

typedef void (*GwyRectSelectionLabelsFunc)(gpointer user_data);

struct _GwyRectSelectionLabels {
    GtkWidget *table;
    GtkLabel *real[NRLABELS];
    GtkWidget *px[NRLABELS];
    GtkSpinButton *pix[NRLABELS];
    gulong pix_id[NRLABELS];
    gboolean in_update;

    gboolean none_is_full;
    GCallback callback;
    gpointer cbdata;
};

static void gwy_plain_tool_finalize              (GObject *object);
static void gwy_plain_tool_show                  (GwyTool *tool);
static void gwy_plain_tool_hide                  (GwyTool *tool);
static void gwy_plain_tool_data_switched         (GwyTool *tool,
                                                  GwyDataView *data_view);
static void gwy_plain_tool_reconnect_container   (GwyPlainTool *plain_tool,
                                                  GwyDataView *data_view);
static void gwy_plain_tool_data_item_changed     (GwyContainer *container,
                                                  GQuark quark,
                                                  GwyPlainTool *plain_tool);
static void gwy_plain_tool_mask_item_changed     (GwyContainer *container,
                                                  GQuark quark,
                                                  GwyPlainTool *plain_tool);
static void gwy_plain_tool_show_item_changed     (GwyContainer *container,
                                                  GQuark quark,
                                                  GwyPlainTool *plain_tool);
static void gwy_plain_tool_selection_item_changed(GwyContainer *container,
                                                  GQuark quark,
                                                  GwyPlainTool *plain_tool);
static void gwy_plain_tool_data_changed          (GwyPlainTool *plain_tool);
static void gwy_plain_tool_mask_changed          (GwyPlainTool *plain_tool);
static void gwy_plain_tool_show_changed          (GwyPlainTool *plain_tool);
static void gwy_plain_tool_selection_disconnect  (GwyPlainTool *plain_tool);
static void gwy_plain_tool_selection_reconnect   (GwyPlainTool *plain_tool);
static void gwy_plain_tool_selection_changed     (GwySelection *selection,
                                                  gint hint,
                                                  GwyPlainTool *plain_tool);
static void gwy_plain_tool_selection_finished    (GwySelection *selection,
                                                  GwyPlainTool *plain_tool);
static void gwy_plain_tool_update_units          (GwyPlainTool *plain_tool);
static void gwy_plain_tool_response              (GwyTool *tool,
                                                  gint response_id);

static void gwy_rect_selection_labels_spinned (GtkSpinButton *spin,
                                               GwyRectSelectionLabels *rlabels);
static void gwy_rect_selection_labels_set_sensitive(GwyRectSelectionLabels *rlabels,
                                                    gboolean sensitive);

G_DEFINE_ABSTRACT_TYPE(GwyPlainTool, gwy_plain_tool, GWY_TYPE_TOOL)

static GType selection_type_rect = 0;

static void
gwy_plain_tool_class_init(GwyPlainToolClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);

    gobject_class->finalize = gwy_plain_tool_finalize;

    tool_class->hide = gwy_plain_tool_hide;
    tool_class->show = gwy_plain_tool_show;
    tool_class->data_switched = gwy_plain_tool_data_switched;
    tool_class->response = gwy_plain_tool_response;
}

static void
gwy_plain_tool_finalize(GObject *object)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(object);
    gwy_plain_tool_selection_disconnect(plain_tool);
    gwy_object_unref(plain_tool->layer);
    gwy_plain_tool_reconnect_container(plain_tool, NULL);

    g_free(plain_tool->selection_bname);
    if (plain_tool->coord_format)
        gwy_si_unit_value_format_free(plain_tool->coord_format);
    if (plain_tool->value_format)
        gwy_si_unit_value_format_free(plain_tool->value_format);

    G_OBJECT_CLASS(gwy_plain_tool_parent_class)->finalize(object);
}

static void
gwy_plain_tool_init(GwyPlainTool *plain_tool)
{
    plain_tool->id = -1;
}

static void
gwy_plain_tool_show(GwyTool *tool)
{
    GwyPlainTool *plain_tool;
    GwyPlainToolClass *klass;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (plain_tool->lazy_updates && plain_tool->pending_updates) {
        klass = GWY_PLAIN_TOOL_GET_CLASS(tool);

        /* Since we remember only what has changed, the order can be different.
         * The following order should cause the least odd effects.  If a tool
         * has problems with it, it can simply look at @pending_updates in its
         * show() method, update self and clear @pending_updates before
         * calling parent's show(). */
        if (plain_tool->pending_updates & GWY_PLAIN_TOOL_CHANGED_DATA) {
            gwy_plain_tool_update_units(plain_tool);
            if (klass->data_changed)
                klass->data_changed(plain_tool);
            plain_tool->pending_updates &= ~GWY_PLAIN_TOOL_CHANGED_DATA;
        }

        if (plain_tool->pending_updates & GWY_PLAIN_TOOL_CHANGED_MASK) {
            if (klass->mask_changed)
                klass->mask_changed(plain_tool);
            plain_tool->pending_updates &= ~GWY_PLAIN_TOOL_CHANGED_MASK;
        }

        if (plain_tool->pending_updates & GWY_PLAIN_TOOL_CHANGED_SHOW) {
            if (klass->show_changed)
                klass->show_changed(plain_tool);
            plain_tool->pending_updates &= ~GWY_PLAIN_TOOL_CHANGED_SHOW;
        }

        if (plain_tool->pending_updates & GWY_PLAIN_TOOL_CHANGED_SELECTION) {
            /* This is gross. */
            if (klass->selection_changed)
                klass->selection_changed(plain_tool, -1);
            plain_tool->pending_updates &= ~GWY_PLAIN_TOOL_CHANGED_SELECTION;
        }

        if (plain_tool->pending_updates & GWY_PLAIN_TOOL_FINISHED_SELECTION) {
            if (klass->selection_finished)
                klass->selection_finished(plain_tool);
            plain_tool->pending_updates &= ~GWY_PLAIN_TOOL_FINISHED_SELECTION;
        }

        if (plain_tool->pending_updates) {
            g_warning("Stray bits in pending_updates: %u",
                      plain_tool->pending_updates);
            plain_tool->pending_updates = 0;
        }
    }
    GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->show(tool);
}

static void
gwy_plain_tool_hide(GwyTool *tool)
{
    if (GWY_PLAIN_TOOL(tool)->pending_updates)
        g_warning("We have pending updates when tool is being hidden?");

    GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->hide(tool);
}

void
gwy_plain_tool_data_switched(GwyTool *tool,
                             GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;

    gwy_debug("%s %p", GWY_TOOL_GET_CLASS(tool)->title, data_view);
    if (GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->data_switched)
        GWY_TOOL_CLASS(gwy_plain_tool_parent_class)->data_switched(tool,
                                                                   data_view);

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (data_view == plain_tool->data_view)
        return;

    gwy_plain_tool_selection_disconnect(plain_tool);
    gwy_plain_tool_reconnect_container(plain_tool, data_view);
    gwy_plain_tool_update_units(plain_tool);
    if (data_view && plain_tool->layer_type) {
        gwy_plain_tool_ensure_layer(plain_tool, plain_tool->layer_type);
        gwy_plain_tool_selection_reconnect(plain_tool);
    }
    else {
        gwy_object_unref(plain_tool->layer);
        gwy_plain_tool_selection_changed(NULL, -1, plain_tool);
    }
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

    gwy_debug("%s %p", GWY_TOOL_GET_CLASS(plain_tool)->title, data_view);
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

    plain_tool->id = -1;

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
    g_return_if_fail(len > 5 && data_key[0] == '/'
                     && gwy_strequal(data_key + len-5, "/data"));
    plain_tool->id = atoi(data_key + 1);
    g_return_if_fail(plain_tool->id >= 0);
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
gwy_plain_tool_selection_item_changed(GwyContainer *container,
                                      GQuark quark,
                                      GwyPlainTool *plain_tool)
{
    gwy_signal_handler_disconnect(plain_tool->selection,
                                  plain_tool->selection_cid);
    gwy_signal_handler_disconnect(plain_tool->selection,
                                  plain_tool->selection_fid);
    gwy_object_unref(plain_tool->selection);

    if (gwy_container_gis_object(container, quark, &plain_tool->selection)) {
        g_object_ref(plain_tool->selection);
        plain_tool->selection_cid
            = g_signal_connect(plain_tool->selection, "changed",
                               G_CALLBACK(gwy_plain_tool_selection_changed),
                               plain_tool);
        plain_tool->selection_fid
            = g_signal_connect(plain_tool->selection, "finished",
                               G_CALLBACK(gwy_plain_tool_selection_finished),
                               plain_tool);
    }

    gwy_plain_tool_selection_changed(plain_tool->selection, -1, plain_tool);
}

static void
gwy_plain_tool_data_changed(GwyPlainTool *plain_tool)
{
    GwyPlainToolClass *klass;

    klass = GWY_PLAIN_TOOL_GET_CLASS(plain_tool);
    if (plain_tool->lazy_updates
        && !gwy_tool_is_visible(GWY_TOOL(plain_tool)))
        plain_tool->pending_updates |= GWY_PLAIN_TOOL_CHANGED_DATA;
    else {
        gwy_plain_tool_update_units(plain_tool);
        if (klass->data_changed)
            klass->data_changed(plain_tool);
    }
}

static void
gwy_plain_tool_mask_changed(GwyPlainTool *plain_tool)
{
    GwyPlainToolClass *klass;

    klass = GWY_PLAIN_TOOL_GET_CLASS(plain_tool);
    if (plain_tool->lazy_updates
        && !gwy_tool_is_visible(GWY_TOOL(plain_tool)))
        plain_tool->pending_updates |= GWY_PLAIN_TOOL_CHANGED_MASK;
    else if (klass->mask_changed)
        klass->mask_changed(plain_tool);
}

static void
gwy_plain_tool_show_changed(GwyPlainTool *plain_tool)
{
    GwyPlainToolClass *klass;

    klass = GWY_PLAIN_TOOL_GET_CLASS(plain_tool);
    if (plain_tool->lazy_updates
        && !gwy_tool_is_visible(GWY_TOOL(plain_tool)))
        plain_tool->pending_updates |= GWY_PLAIN_TOOL_CHANGED_SHOW;
    else if (klass->show_changed)
        klass->show_changed(plain_tool);
}

static void
gwy_plain_tool_selection_changed(GwySelection *selection,
                                 gint hint,
                                 GwyPlainTool *plain_tool)
{
    GwyPlainToolClass *klass;

    if (plain_tool->clear)
        gtk_widget_set_sensitive(plain_tool->clear,
                                 selection
                                 && gwy_selection_get_data(selection, NULL));

    klass = GWY_PLAIN_TOOL_GET_CLASS(plain_tool);
    if (plain_tool->lazy_updates
        && !gwy_tool_is_visible(GWY_TOOL(plain_tool)))
        plain_tool->pending_updates |= GWY_PLAIN_TOOL_CHANGED_SELECTION;
    else if (klass->selection_changed)
        klass->selection_changed(plain_tool, hint);
}

static void
gwy_plain_tool_selection_finished(G_GNUC_UNUSED GwySelection *selection,
                                  GwyPlainTool *plain_tool)
{
    GwyPlainToolClass *klass;

    klass = GWY_PLAIN_TOOL_GET_CLASS(plain_tool);
    if (plain_tool->lazy_updates
        && !gwy_tool_is_visible(GWY_TOOL(plain_tool)))
        plain_tool->pending_updates |= GWY_PLAIN_TOOL_FINISHED_SELECTION;
    else if (klass->selection_finished)
        klass->selection_finished(plain_tool);
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
    if (plain_tool->data_field && plain_tool->unit_style) {
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
        if (plain_tool->value_format) {
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
 * gwy_plain_tool_connect_selection:
 * @plain_tool: A plain tool.
 * @layer_type: Layer type.  Use gwy_plain_tool_check_layer_type() in tool
 *              instance initialization to check for layer types.
 * @bname: Selection key base name, for example <literal>"line"</literal>.
 *
 * Sets up a plain tool to automatically manage layer selection.
 *
 * When @layer_type is 0 and @bname %NULL, plain tool stops automatically
 * managing selection (hopefully).
 *
 * This method performs gwy_plain_tool_ensure_layer() and
 * gwy_plain_tool_set_selection_key(), connecting to the selection and making
 * sure the @selection field always points to the correct selection object
 * (or is %NULL).
 *
 * The @selection_changed method of #GwyPlainToolClass is only invoked for a
 * tool instance once this method was called to set up the selection tracking.
 **/
void
gwy_plain_tool_connect_selection(GwyPlainTool *plain_tool,
                                 GType layer_type,
                                 const gchar *bname)
{
    gwy_debug("%s %s", GWY_TOOL_GET_CLASS(plain_tool)->title, bname);
    g_return_if_fail(GWY_IS_PLAIN_TOOL(plain_tool));
    if (layer_type || bname) {
        g_return_if_fail(g_type_is_a(layer_type, GWY_TYPE_VECTOR_LAYER));
        g_return_if_fail(bname);
    }

    gwy_plain_tool_selection_disconnect(plain_tool);
    g_free(plain_tool->selection_bname);
    plain_tool->selection_bname = NULL;

    plain_tool->layer_type = layer_type;
    plain_tool->selection_bname = g_strdup(bname);

    if (!bname || !layer_type || !plain_tool->data_view)
        return;

    gwy_plain_tool_ensure_layer(plain_tool, layer_type);
    gwy_plain_tool_selection_reconnect(plain_tool);
}

static void
gwy_plain_tool_selection_disconnect(GwyPlainTool *plain_tool)
{
    gwy_debug("");

    gwy_signal_handler_disconnect(plain_tool->container,
                                  plain_tool->selection_item_id);
    gwy_signal_handler_disconnect(plain_tool->selection,
                                  plain_tool->selection_cid);
    gwy_signal_handler_disconnect(plain_tool->selection,
                                  plain_tool->selection_fid);
    gwy_object_unref(plain_tool->selection);
}

static void
gwy_plain_tool_selection_reconnect(GwyPlainTool *plain_tool)
{
    const gchar *key;
    gchar *sigdetail;

    gwy_debug("%s", plain_tool->selection_bname);

    if (!plain_tool->selection_bname)
        return;

    key = gwy_plain_tool_set_selection_key(plain_tool,
                                           plain_tool->selection_bname);
    sigdetail = g_strconcat(ITEM_CHANGED, key, NULL);

    plain_tool->selection_item_id
        = g_signal_connect(plain_tool->container, sigdetail,
                           G_CALLBACK(gwy_plain_tool_selection_item_changed),
                           plain_tool);
    if (gwy_container_gis_object_by_name(plain_tool->container, key,
                                         &plain_tool->selection)) {
        g_object_ref(plain_tool->selection);
        plain_tool->selection_cid
            = g_signal_connect(plain_tool->selection, "changed",
                               G_CALLBACK(gwy_plain_tool_selection_changed),
                               plain_tool);
        plain_tool->selection_fid
            = g_signal_connect(plain_tool->selection, "finished",
                               G_CALLBACK(gwy_plain_tool_selection_finished),
                               plain_tool);
    }

    g_free(sigdetail);

    gwy_plain_tool_selection_changed(plain_tool->selection, -1, plain_tool);
}

/**
 * gwy_plain_tool_set_selection_key:
 * @plain_tool: A plain tool.
 * @bname: Selection key base name, for example <literal>"line"</literal>.
 *
 * Constructs selection key from data key and sets it on the vector layer.
 *
 * This is a low-level function, normally you would use
 * gwy_plain_tool_connect_selection().
 *
 * Returns: The full key (as a layer-owned string).
 **/
const gchar*
gwy_plain_tool_set_selection_key(GwyPlainTool *plain_tool,
                                 const gchar *bname)
{
    GwyPixmapLayer *layer;
    GwyContainer *container;
    const gchar *data_key;
    gchar *key;
    guint len;

    gwy_debug("%s", bname);
    g_return_val_if_fail(GWY_IS_PLAIN_TOOL(plain_tool), NULL);
    g_return_val_if_fail(GWY_IS_DATA_VIEW(plain_tool->data_view), NULL);
    g_return_val_if_fail(GWY_IS_VECTOR_LAYER(plain_tool->layer), NULL);
    g_return_val_if_fail(bname, NULL);

    container = gwy_data_view_get_data(plain_tool->data_view);
    layer = gwy_data_view_get_base_layer(plain_tool->data_view);
    data_key = gwy_pixmap_layer_get_data_key(layer);
    gwy_debug("data_key: <%s>", data_key);
    len = strlen(data_key);
    g_return_val_if_fail(len > 5 && gwy_strequal(data_key + len-5, "/data"),
                         NULL);

    key = g_strdup_printf("%.*s/select/%s", len-5, data_key, bname);
    gwy_vector_layer_set_selection_key(plain_tool->layer, key);
    gwy_debug("selection key: <%s>", key);
    g_free(key);

    gwy_vector_layer_ensure_selection(plain_tool->layer);

    /* The returned value is a quark and should be safe to use anytime. */
    return gwy_vector_layer_get_selection_key(plain_tool->layer);
}

/**
 * gwy_plain_tool_ensure_layer:
 * @plain_tool: A plain tool.
 * @layer_type: Layer type.  Use gwy_plain_tool_check_layer_type() in
 *              tool instance initialization to check for layer types.
 *
 * Makes sure a plain tool's layer is of the correct type.
 *
 * This is a low-level function, normally you would use
 * gwy_plain_tool_connect_selection().
 **/
void
gwy_plain_tool_ensure_layer(GwyPlainTool *plain_tool,
                            GType layer_type)
{
    gwy_debug("%s %s",
              GWY_TOOL_GET_CLASS(plain_tool)->title, g_type_name(layer_type));
    g_return_if_fail(GWY_IS_PLAIN_TOOL(plain_tool));
    g_return_if_fail(g_type_is_a(layer_type, GWY_TYPE_VECTOR_LAYER));

    gwy_object_unref(plain_tool->layer);
    plain_tool->layer = gwy_data_view_get_top_layer(plain_tool->data_view);
    if (!plain_tool->layer
        || G_TYPE_FROM_INSTANCE(plain_tool->layer) != layer_type) {
        plain_tool->layer = g_object_new(layer_type, NULL);
        gwy_data_view_set_top_layer(plain_tool->data_view, plain_tool->layer);
    }
    g_object_ref(plain_tool->layer);
}

/**
 * gwy_plain_tool_add_clear_button:
 * @plain_tool: A plain tool.
 *
 * Adds a `Clear' button to a plain tool.
 *
 * This button works with automatically managed selection (see
 * gwy_plain_tool_connect_selection()).  If you want to manage selection
 * yourself add the button with gtk_dialog_add_button().
 *
 * Returns: The button widget.
 **/
GtkWidget*
gwy_plain_tool_add_clear_button(GwyPlainTool *plain_tool)
{
    GtkDialog *dialog;
    GtkTooltips *tips;

    g_return_val_if_fail(GWY_IS_PLAIN_TOOL(plain_tool), NULL);
    g_return_val_if_fail(!plain_tool->clear, NULL);

    dialog = GTK_DIALOG(GWY_TOOL(plain_tool)->dialog);
    tips = gwy_app_get_tooltips();
    plain_tool->clear = gtk_dialog_add_button(dialog, GTK_STOCK_CLEAR,
                                              GWY_TOOL_RESPONSE_CLEAR);
    gtk_tooltips_set_tip(tips, plain_tool->clear,
                         _("Clear selected objects"), NULL);
    if (!plain_tool->selection
        || !gwy_selection_get_data(plain_tool->selection, NULL))
        gtk_widget_set_sensitive(plain_tool->clear, FALSE);

    return plain_tool->clear;
}

static void
gwy_plain_tool_response(GwyTool *tool,
                        gint response_id)
{
    GwyPlainTool *plain_tool;

    plain_tool = GWY_PLAIN_TOOL(tool);
    if (response_id == GWY_TOOL_RESPONSE_CLEAR && plain_tool->clear) {
        gwy_selection_clear(plain_tool->selection);
    }
}

/**
 * gwy_plain_tool_get_z_average:
 * @data_field: A data field.
 * @point: Real X and Y-coordinate of area center in physical units.
 * @radius: Area radius in pixels, 1 means a signle pixel.  The actual radius
 *          passed to gwy_data_field_circular_area_extract() is @radius-0.5.
 *
 * Computes average value over a part of data field @dfield.
 *
 * It is not an error if part of it lies outside the data field borders
 * (it is simply not counted in), however the intersection have to be nonempty.
 *
 * Returns: The average value.
 **/
gdouble
gwy_plain_tool_get_z_average(GwyDataField *data_field,
                             const gdouble *point,
                             gint radius)
{
    gint col, row, n, i;
    gdouble *values;
    gdouble avg;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0.0);
    g_return_val_if_fail(point, 0.0);
    g_return_val_if_fail(radius > 0, 0.0);

    col = gwy_data_field_rtoj(data_field, point[0]);
    row = gwy_data_field_rtoi(data_field, point[1]);

    if (radius == 1)
        return gwy_data_field_get_val(data_field, col, row);

    values = g_new(gdouble, (2*radius + 1)*(2*radius + 1));
    n = gwy_data_field_circular_area_extract(data_field, col, row, radius - 0.5,
                                             values);
    avg = 0.0;
    if (n) {
        for (i = 0; i < n; i++)
            avg += values[i];
        avg /= n;
    }
    else
        g_warning("Z average calculated from an empty area");

    g_free(values);

    return avg;
}

static gboolean
gwy_plain_tool_delete_selection_object(GtkTreeView *treeview,
                                       GdkEventKey *event,
                                       GwyPlainTool *plain_tool)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    const gint *indices;

    if (event->keyval != GDK_Delete)
        return FALSE;

    selection = gtk_tree_view_get_selection(treeview);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return FALSE;

    path = gtk_tree_model_get_path(model, &iter);
    indices = gtk_tree_path_get_indices(path);
    gwy_selection_delete_object(plain_tool->selection, indices[0]);
    gtk_tree_path_free(path);

    return TRUE;
}

/**
 * gwy_plain_tool_enable_object_deletion:
 * @plain_tool: A plain tool.
 * @treeview: A tree view that displays selection objects in order, each row
 *            corresponding to one selection object.
 *
 * Enables deletion of selection objects by presssing Delete in a tree view
 * diplaying them.
 *
 * Since: 2.7
 **/
void
gwy_plain_tool_enable_object_deletion(GwyPlainTool *plain_tool,
                                      GtkTreeView *treeview)
{
    g_return_if_fail(GTK_IS_TREE_VIEW(treeview));
    g_return_if_fail(GWY_IS_PLAIN_TOOL(plain_tool));

    g_signal_connect(treeview, "key-press-event",
                     G_CALLBACK(gwy_plain_tool_delete_selection_object),
                     plain_tool);
}

static GtkLabel*
gwy_rect_selection_labels_make_rlabel(GtkTable *table,
                                      gint col, gint row)
{
    GtkLabel *label;

    label = GTK_LABEL(gtk_label_new(NULL));
    gtk_label_set_single_line_mode(label, TRUE);
    gtk_label_set_width_chars(label, 10);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, GTK_WIDGET(label), col, col+1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    return label;
}

static GtkSpinButton*
gwy_rect_selection_labels_make_pspin(GtkTable *table,
                                     gint col, gint row,
                                     gboolean from_1,
                                     GtkWidget **px)
{
    GtkWidget *spin, *label, *hbox;
    GtkObject *adj;

    if (from_1)
        adj = gtk_adjustment_new(1.0, 1.0, 100.0, 1.0, 10.0, 0.0);
    else
        adj = gtk_adjustment_new(0.0, 0.0, 100.0, 1.0, 10.0, 0.0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0.0, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(spin), 4);

    hbox = gtk_hbox_new(FALSE, 4);

    label = gtk_label_new("px");
    gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

    gtk_table_attach(table, hbox, col, col+1, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    if (px)
        *px = label;

    return GTK_SPIN_BUTTON(spin);
}

/**
 * gwy_rect_selection_labels_new:
 * @none_is_full: %TRUE to tread unselected state as full data selected.
 * @callback: Callback to call when the selection is edited.
 * @cbdata: Data to pass to @callback (as the first argument).
 *
 * Creates a table displaying rectangular selection information.
 *
 * The returned object will destroy itself when the table is destroyed.
 *
 * Returns: The newly created rectangular selection information, as an opaque
 *          pointer.  The table widget can be obtained with
 *          gwy_rect_selection_labels_get_table().
 **/
GwyRectSelectionLabels*
gwy_rect_selection_labels_new(gboolean none_is_full,
                              GCallback callback,
                              gpointer cbdata)
{
    GwyRectSelectionLabels *rlabels;
    GtkTable *table;
    GtkWidget *label, *px;
    guint i;

    if (!selection_type_rect) {
        selection_type_rect = g_type_from_name("GwySelectionRectangle");
        g_return_val_if_fail(selection_type_rect, FALSE);
    }

    if (!callback)
        g_warning("Without a callback, reverse updates will hardly work");

    rlabels = g_new(GwyRectSelectionLabels, 1);
    rlabels->none_is_full = none_is_full;
    rlabels->callback = callback;
    rlabels->cbdata = cbdata;
    rlabels->in_update = TRUE;

    rlabels->table = gtk_table_new(6, 3, FALSE);
    table = GTK_TABLE(rlabels->table);
    gtk_table_set_col_spacings(table, 8);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    g_signal_connect_swapped(table, "destroy", G_CALLBACK(g_free), rlabels);

    label = gwy_label_new_header(_("Origin"));
    gtk_table_attach(table, label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new("X");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new("Y");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gwy_label_new_header(_("Size"));
    gtk_table_attach(table, label, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Width"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Height"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, 5, 6, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    for (i = 0; i < NRLABELS; i++) {
        rlabels->real[i] = gwy_rect_selection_labels_make_rlabel(table,
                                                                 1, i+1 + i/2);
        rlabels->pix[i] = gwy_rect_selection_labels_make_pspin(table,
                                                               2, i+1 + i/2,
                                                               i/2, &px);
        rlabels->px[i] = px;
        g_signal_connect(rlabels->pix[i], "value-changed",
                         G_CALLBACK(gwy_rect_selection_labels_spinned),
                         rlabels);
    }
    gwy_rect_selection_labels_set_sensitive(rlabels, FALSE);

    rlabels->in_update = FALSE;

    return rlabels;
}

/**
 * gwy_rect_selection_labels_get_table:
 * @rlabels: Rectangular selection information table.
 *
 * Gets the table widget of a rectangular selection information.
 *
 * Returns: The table as a #GtkWidget.
 **/
GtkWidget*
gwy_rect_selection_labels_get_table(GwyRectSelectionLabels *rlabels)
{
    return rlabels->table;
}

static void
gwy_rect_selection_labels_spinned(GtkSpinButton *spin,
                                  GwyRectSelectionLabels *rlabels)
{
    GtkAdjustment *adj;
    gint val;

    if (rlabels->in_update)
        return;

    rlabels->in_update = TRUE;

    adj = gtk_spin_button_get_adjustment(spin);
    val = gwy_adjustment_get_int(adj);

    if (spin == rlabels->pix[RLABEL_X]) {
        gtk_spin_button_set_range(rlabels->pix[RLABEL_W],
                                  2.0, adj->upper + 1.0 - val);
    }
    else if (spin == rlabels->pix[RLABEL_Y]) {
        gtk_spin_button_set_range(rlabels->pix[RLABEL_H],
                                  2.0, adj->upper + 1.0 - val);
    }
    else if (spin == rlabels->pix[RLABEL_W]) {
    }
    else if (spin == rlabels->pix[RLABEL_H]) {
    }
    else
        g_critical("Stray spin button signal");

    if (rlabels->callback) {
        GwyRectSelectionLabelsFunc func;

        func = (GwyRectSelectionLabelsFunc)rlabels->callback;
        func(rlabels->cbdata);
    }

    rlabels->in_update = FALSE;
}

void
gwy_rect_selection_labels_select(GwyRectSelectionLabels *rlabels,
                                 GwySelection *selection,
                                 GwyDataField *dfield)
{
    GtkAdjustment *adj;
    gint isel[4];
    gdouble sel[4];
    guint i;

    g_return_if_fail(!dfield || GWY_IS_DATA_FIELD(dfield));
    g_return_if_fail(!selection
                     || g_type_is_a(G_TYPE_FROM_INSTANCE(selection),
                                    selection_type_rect));

    if (!GTK_WIDGET_SENSITIVE(rlabels->pix[0])) {
        g_warning("Nothing is selected");
        return;
    }

    rlabels->in_update = TRUE;

    for (i = 0; i < NRLABELS; i++) {
        adj = gtk_spin_button_get_adjustment(rlabels->pix[i]);
        isel[i] = gwy_adjustment_get_int(adj);
    }

    if (isel[2] && isel[3]) {
        isel[2] += isel[0] - 1;
        isel[3] += isel[1] - 1;

        sel[0] = gwy_data_field_jtor(dfield, isel[0] + 0.5);
        sel[1] = gwy_data_field_itor(dfield, isel[1] + 0.5);
        sel[2] = gwy_data_field_jtor(dfield, isel[2] + 0.5);
        sel[3] = gwy_data_field_itor(dfield, isel[3] + 0.5);

        gwy_selection_set_data(selection, 1, sel);
    }
    else
        gwy_selection_clear(selection);

    rlabels->in_update = FALSE;
}

/**
 * gwy_rect_selection_labels_fill:
 * @rlabels: Rectangular selection info table.
 * @selection: A rectangular selection to fill information from.  It can
 *             be %NULL to clear the labels.
 * @dfield: A data field to use for real/pixel coordinate transforms.
 *          It can be %NULL to clear the labels.
 * @selreal: If not %NULL, must be an array of size at least 4 and will be
 *           filled with selection data xmin, ymin, xmax, ymax in physical
 *           units.
 * @selpix: If not %NULL, must be an array of size at least 4 and will be
 *          filled with selection data xmin, ymin, xmax, ymax in pixels.
 *
 * Updates rectangular selection info display.
 *
 * It is possible to pass %NULL @dfield but non-%NULL @selection.  This can
 * lead to %TRUE return value (if the selection is non-empty), but the labels
 * will be still cleared as there is no way to convert between real and
 * pixel coordinates.
 *
 * Returns: %TRUE if a selection is present, %FALSE otherwise.
 **/
gboolean
gwy_rect_selection_labels_fill(GwyRectSelectionLabels *rlabels,
                               GwySelection *selection,
                               GwyDataField *dfield,
                               gdouble *selreal,
                               gint *selpix)
{
    GwySIValueFormat *vf;
    gdouble sel[4];
    gint isel[4];
    static gchar buffer[64];
    gdouble xoff, yoff;
    gboolean is_selected;
    GtkAdjustment *adj;
    gint xres, yres;
    guint i;

    g_return_val_if_fail(!dfield || GWY_IS_DATA_FIELD(dfield), FALSE);
    g_return_val_if_fail(!selection
                         || g_type_is_a(G_TYPE_FROM_INSTANCE(selection),
                                        selection_type_rect),
                         FALSE);

    rlabels->in_update = TRUE;

    is_selected = selection && gwy_selection_get_object(selection, 0, sel);
    if (!selection || !dfield || (!is_selected && !rlabels->none_is_full)) {
        gwy_rect_selection_labels_set_sensitive(rlabels, FALSE);
        for (i = 0; i < NRLABELS; i++) {
            gtk_label_set_text(rlabels->real[i], "");
            gtk_spin_button_set_value(rlabels->pix[i], 0.0);
        }

        rlabels->in_update = FALSE;
        return is_selected;
    }

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xoff = gwy_data_field_get_xoffset(dfield);
    yoff = gwy_data_field_get_yoffset(dfield);

    if (is_selected) {
        if (sel[0] > sel[2])
            GWY_SWAP(gdouble, sel[0], sel[2]);
        if (sel[1] > sel[3])
            GWY_SWAP(gdouble, sel[1], sel[3]);

        isel[0] = gwy_data_field_rtoj(dfield, sel[0]);
        isel[1] = gwy_data_field_rtoi(dfield, sel[1]);
        isel[2] = gwy_data_field_rtoj(dfield, sel[2]);
        isel[3] = gwy_data_field_rtoi(dfield, sel[3]);
    }
    else {
        sel[0] = sel[1] = 0.0;
        sel[2] = gwy_data_field_get_xreal(dfield);
        sel[3] = gwy_data_field_get_yreal(dfield);
        isel[0] = isel[1] = 0;
        isel[2] = xres;
        isel[3] = yres;
    }

    if (selreal)
        memcpy(selreal, sel, 4*sizeof(gdouble));
    if (selpix)
        memcpy(selpix, isel, 4*sizeof(gint));

    sel[2] -= sel[0];
    sel[3] -= sel[1];
    sel[0] += xoff;
    sel[1] += yoff;

    vf = gwy_data_field_get_value_format_xy(dfield, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                            NULL);
    for (i = 0; i < NRLABELS; i++) {
        g_snprintf(buffer, sizeof(buffer), "%.*f%s%s",
                   vf->precision, sel[i]/vf->magnitude,
                   *vf->units ? " " : "", vf->units);
        gtk_label_set_markup(rlabels->real[i], buffer);
    }
    gwy_si_unit_value_format_free(vf);

    isel[2] -= isel[0] - 1;
    isel[3] -= isel[1] - 1;

    gtk_spin_button_set_range(rlabels->pix[RLABEL_X], 0.0, xres - 1.0);
    gtk_spin_button_set_range(rlabels->pix[RLABEL_W], 2.0, xres - isel[0]);
    gtk_spin_button_set_range(rlabels->pix[RLABEL_Y], 0.0, yres - 1.0);
    gtk_spin_button_set_range(rlabels->pix[RLABEL_H], 2.0, yres - isel[1]);

    for (i = 0; i < NRLABELS; i++) {
        gwy_rect_selection_labels_set_sensitive(rlabels, TRUE);
        adj = gtk_spin_button_get_adjustment(rlabels->pix[i]);
        gtk_adjustment_set_value(adj, isel[i]);
    }

    rlabels->in_update = FALSE;

    return is_selected;
}

static void
gwy_rect_selection_labels_set_sensitive(GwyRectSelectionLabels *rlabels,
                                        gboolean sensitive)
{
    guint i;

    for (i = 0; i < NRLABELS; i++) {
        gtk_widget_set_sensitive(GTK_WIDGET(rlabels->pix[i]), sensitive);
        gtk_widget_set_sensitive(rlabels->px[i], sensitive);
    }
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyplaintool
 * @title: GwyPlainTool
 * @short_description: Base class for simple tools
 **/

/**
 * GwyPlainToolChanged:
 * @GWY_PLAIN_TOOL_CHANGED_DATA: Channel data field was changed.
 * @GWY_PLAIN_TOOL_CHANGED_MASK: Mask data field was changed.
 * @GWY_PLAIN_TOOL_CHANGED_SHOW: Presentation data field was changed.
 * @GWY_PLAIN_TOOL_CHANGED_SELECTION: Selection was changed.
 * @GWY_PLAIN_TOOL_FINISHED_SELECTION: Selection was finished (that is emitted
 *                                     #GwySelection::finished).
 *
 * The type of pending changes that accumulated during tool inactivity.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
