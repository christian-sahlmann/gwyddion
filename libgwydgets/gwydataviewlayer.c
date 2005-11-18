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
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include "gwydataviewlayer.h"
#include "gwyvectorlayer.h"
#include "gwypixmaplayer.h"
#include "gwydataview.h"

#define BITS_PER_SAMPLE 8

enum {
    PLUGGED,
    UNPLUGGED,
    UPDATED,
    LAST_SIGNAL
};

/* Forward declarations */

static void gwy_data_view_layer_destroy       (GtkObject *object);
static void gwy_data_view_layer_real_plugged  (GwyDataViewLayer *layer);
static void gwy_data_view_layer_real_unplugged(GwyDataViewLayer *layer);

/* Local data */

static guint data_view_layer_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE(GwyDataViewLayer, gwy_data_view_layer, GTK_TYPE_OBJECT)

static void
gwy_data_view_layer_class_init(GwyDataViewLayerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

    object_class->destroy = gwy_data_view_layer_destroy;

    klass->plugged = gwy_data_view_layer_real_plugged;
    klass->unplugged = gwy_data_view_layer_real_unplugged;

   /**
    * GwyDataViewLayer::plugged:
    * @gwydataviewlayer: The #GwyDataViewLayer which received the signal.
    *
    * The ::plugged signal is emitted when a #GwyDataViewLayer is plugged into
    * a #GwyDataView.
    **/
    data_view_layer_signals[PLUGGED] =
        g_signal_new("plugged",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyDataViewLayerClass, plugged),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

   /**
    * GwyDataViewLayer::unplugged:
    * @gwydataviewlayer: The #GwyDataViewLayer which received the signal.
    *
    * The ::unplugged signal is emitted when a #GwyDataViewLayer is removed
    * from its #GwyDataView.
    **/
    data_view_layer_signals[UNPLUGGED] =
        g_signal_new("unplugged",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyDataViewLayerClass, unplugged),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /**
     * GwyDataViewLayer::updated:
     * @gwydataviewlayer: The #GwyDataViewLayer which received the signal.
     *
     * The ::updated signal is emitted when a #GwyDataViewLayer is updated;
     * the exact means how a layer can be updated depends its type.
     **/
    data_view_layer_signals[UPDATED] =
        g_signal_new("updated",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyDataViewLayerClass, updated),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

static void
gwy_data_view_layer_init(G_GNUC_UNUSED GwyDataViewLayer *layer)
{
}

static void
gwy_data_view_layer_destroy(GtkObject *object)
{
    GwyDataViewLayer *layer;

    layer = GWY_DATA_VIEW_LAYER(object);
    gwy_object_unref(layer->data);
    GTK_OBJECT_CLASS(gwy_data_view_layer_parent_class)->destroy(object);
}

/**
 * gwy_data_view_layer_plugged:
 * @layer: A data view layer.
 *
 * Emits a "plugged" singal on a layer.
 *
 * Primarily intended for #GwyDataView implementation.
 **/
void
gwy_data_view_layer_plugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    g_signal_emit(layer, data_view_layer_signals[PLUGGED], 0);
}

/**
 * gwy_data_view_layer_unplugged:
 * @layer: A data view layer.
 *
 * Emits a "unplugged" singal on a layer.
 *
 * Primarily intended for #GwyDataView implementation.
 **/
void
gwy_data_view_layer_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    g_signal_emit(layer, data_view_layer_signals[UNPLUGGED], 0);
}

/**
 * gwy_data_view_layer_updated:
 * @layer: A data view layer.
 *
 * Emits a "updated" singal on a layer.
 **/
void
gwy_data_view_layer_updated(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    g_signal_emit(layer, data_view_layer_signals[UPDATED], 0);
}

/**
 * gwy_data_view_layer_realize:
 * @layer: A data view layer.
 *
 * Tells a data view layer its parent was realized and it can create
 * display-specific resources.
 **/
void
gwy_data_view_layer_realize(GwyDataViewLayer *layer)
{
    void (*method)(GwyDataViewLayer*);

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    method = GWY_DATA_VIEW_LAYER_GET_CLASS(layer)->realize;
    if (method)
        method(layer);
}

/**
 * gwy_data_view_layer_unrealize:
 * @layer: A data view layer.
 *
 * Tells a data view layer its parent was unrealized and it should destroy
 * display-specific resources.
 **/
void
gwy_data_view_layer_unrealize(GwyDataViewLayer *layer)
{
    void (*method)(GwyDataViewLayer*);

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    method = GWY_DATA_VIEW_LAYER_GET_CLASS(layer)->unrealize;
    if (method)
        method(layer);
}

static void
gwy_data_view_layer_real_plugged(GwyDataViewLayer *layer)
{
    GwyContainer *data;

    gwy_debug("");

    gwy_object_unref(layer->data);

    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    g_return_if_fail(GWY_IS_CONTAINER(data));
    g_object_ref(data);
    layer->data = data;
}

static void
gwy_data_view_layer_real_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("");

    gwy_object_unref(layer->data);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwydataviewlayer
 * @title: GwyDataViewLayer
 * @short_description: Layer #GwyDataView is composed of
 * @see_also: #GwyDataView -- data display widget,
 *            <link linkend="libgwydraw-gwypixfield">gwypixfield</link> --
 *            low level functions for painting data fields,
 *
 * #GwyDataViewLayer's are parts of #GwyDataView.  They are not widgets and
 * they are not normally usable outside of a data view.  The perform a specific
 * visualization task: drawing the data, drawing mask, or drawing selection.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
