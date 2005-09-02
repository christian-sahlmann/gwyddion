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

#ifndef __GWY_VECTORLAYER_H__
#define __GWY_VECTORLAYER_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <libdraw/gwyselection.h>
#include <libgwydgets/gwydataviewlayer.h>

G_BEGIN_DECLS

#define GWY_TYPE_VECTOR_LAYER            (gwy_vector_layer_get_type())
#define GWY_VECTOR_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_VECTOR_LAYER, GwyVectorLayer))
#define GWY_VECTOR_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_VECTOR_LAYER, GwyVectorLayerClass))
#define GWY_IS_VECTOR_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_VECTOR_LAYER))
#define GWY_IS_VECTOR_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_VECTOR_LAYER))
#define GWY_VECTOR_LAYER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_VECTOR_LAYER, GwyVectorLayerClass))

typedef struct _GwyVectorLayer      GwyVectorLayer;
typedef struct _GwyVectorLayerClass GwyVectorLayerClass;

struct _GwyVectorLayer {
    GwyDataViewLayer parent_instance;

    GdkGC *gc;
    PangoLayout *layout;
    gboolean in_selection;

    /* For new-style layers */
    GwySelection *selection;
    GQuark selection_key;
    gulong item_changed_id;
    gulong selection_changed_id;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyVectorLayerClass {
    GwyDataViewLayerClass parent_class;

    /* renderers */
    void (*draw)(GwyVectorLayer *layer, GdkDrawable *drawable);
    /* events */
    gboolean (*button_press)(GwyVectorLayer *layer, GdkEventButton *event);
    gboolean (*button_release)(GwyVectorLayer *layer, GdkEventButton *event);
    gboolean (*motion_notify)(GwyVectorLayer *layer, GdkEventMotion *event);
    gboolean (*key_press)(GwyVectorLayer *layer, GdkEventKey *event);
    gboolean (*key_release)(GwyVectorLayer *layer, GdkEventKey *event);
    /* signal functions */
    void (*selection_finished)(GwyVectorLayer *layer);
    /* selection */
    gint (*get_selection)(GwyVectorLayer *layer, gdouble *selection);
    void (*set_selection)(GwyVectorLayer *layer,
                          gint nselected, gdouble *selection);
    void (*unselect)(GwyVectorLayer *layer);

    gpointer reserved1;
    gpointer reserved2;
};

GType            gwy_vector_layer_get_type             (void) G_GNUC_CONST;
void             gwy_vector_layer_draw                 (GwyVectorLayer *layer,
                                                        GdkDrawable *drawable);
gboolean         gwy_vector_layer_button_press         (GwyVectorLayer *layer,
                                                        GdkEventButton *event);
gboolean         gwy_vector_layer_button_release       (GwyVectorLayer *layer,
                                                        GdkEventButton *event);
gboolean         gwy_vector_layer_motion_notify        (GwyVectorLayer *layer,
                                                        GdkEventMotion *event);
gboolean         gwy_vector_layer_key_press            (GwyVectorLayer *layer,
                                                        GdkEventKey *event);
gboolean         gwy_vector_layer_key_release          (GwyVectorLayer *layer,
                                                        GdkEventKey *event);
void             gwy_vector_layer_selection_finished   (GwyVectorLayer *layer);
gint             gwy_vector_layer_get_selection        (GwyVectorLayer *layer,
                                                        gdouble *selection);
void             gwy_vector_layer_set_selection        (GwyVectorLayer *layer,
                                                        gint nselected,
                                                        gdouble *selection);
void             gwy_vector_layer_unselect             (GwyVectorLayer *layer);
void             gwy_vector_layer_updated              (GwyVectorLayer *layer);

/* For new-style layers */
void             gwy_vector_layer_set_selection_key    (GwyVectorLayer *layer,
                                                        const gchar *key);
const gchar*     gwy_vector_layer_get_selection_key    (GwyVectorLayer *layer);

/* helpers */
void             gwy_vector_layer_setup_gc             (GwyVectorLayer *layer);

G_END_DECLS

#endif /* __GWY_VECTORLAYER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

