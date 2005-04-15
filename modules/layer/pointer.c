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

/*
 * XXX: this should be placed somewhere...
 * gwy_layer_pointer_new:
 *
 * Creates a new pointer layer.
 *
 * Container keys: "/0/select/pointer/x", "/0/select/pointer/y".
 *
 * The selection (as returned by gwy_vector_layer_get_selection()) consists
 * of a couple (array of size two) points: x and y.
 *
 * Returns: The newly created layer.
 */

#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwyvectorlayer.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule.h>

#define GWY_TYPE_LAYER_POINTER            (gwy_layer_pointer_get_type())
#define GWY_LAYER_POINTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_POINTER, GwyLayerPointer))
#define GWY_LAYER_POINTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_POINTER, GwyLayerPointerClass))
#define GWY_IS_LAYER_POINTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_POINTER))
#define GWY_IS_LAYER_POINTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_POINTER))
#define GWY_LAYER_POINTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_POINTER, GwyLayerPointerClass))

#define GWY_LAYER_POINTER_TYPE_NAME "GwyLayerPointer"

#define PROXIMITY_DISTANCE 8

#define BITS_PER_SAMPLE 8

typedef struct _GwyLayerPointer      GwyLayerPointer;
typedef struct _GwyLayerPointerClass GwyLayerPointerClass;

struct _GwyLayerPointer {
    GwyVectorLayer parent_instance;

    guint button;
    gboolean selected;
    gdouble x;
    gdouble y;
};

struct _GwyLayerPointerClass {
    GwyVectorLayerClass parent_class;

    GdkCursor *point_cursor;
};

/* Forward declarations */

static gboolean module_register                   (const gchar *name);
static GType    gwy_layer_pointer_get_type        (void) G_GNUC_CONST;
static void     gwy_layer_pointer_class_init      (GwyLayerPointerClass *klass);
static void     gwy_layer_pointer_init            (GwyLayerPointer *layer);
static void     gwy_layer_pointer_finalize        (GObject *object);
static void     gwy_layer_pointer_draw            (GwyVectorLayer *layer,
                                                   GdkDrawable *drawable);
static gboolean gwy_layer_pointer_motion_notify   (GwyVectorLayer *layer,
                                                   GdkEventMotion *event);
static gboolean gwy_layer_pointer_button_pressed  (GwyVectorLayer *layer,
                                                   GdkEventButton *event);
static gboolean gwy_layer_pointer_button_released (GwyVectorLayer *layer,
                                                   GdkEventButton *event);
static gint     gwy_layer_pointer_get_selection   (GwyVectorLayer *layer,
                                                   gdouble *selection);
static void     gwy_layer_pointer_unselect        (GwyVectorLayer *layer);
static void     gwy_layer_pointer_plugged         (GwyDataViewLayer *layer);
static void     gwy_layer_pointer_unplugged       (GwyDataViewLayer *layer);
static void     gwy_layer_pointer_save            (GwyLayerPointer *layer);
static void     gwy_layer_pointer_restore         (GwyLayerPointer *layer);

/* Local data */

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of a single point, more precisely "
       "just reading pointer coordinates."),
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static GtkObjectClass *parent_class = NULL;

static gboolean
module_register(const gchar *name)
{
    static GwyLayerFuncInfo func_info = {
        "pointer",
        0,
    };

    func_info.type = gwy_layer_pointer_get_type();
    gwy_layer_func_register(name, &func_info);

    return TRUE;
}

static GType
gwy_layer_pointer_get_type(void)
{
    static GType gwy_layer_pointer_type = 0;

    if (!gwy_layer_pointer_type) {
        static const GTypeInfo gwy_layer_pointer_info = {
            sizeof(GwyLayerPointerClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_layer_pointer_class_init,
            NULL,
            NULL,
            sizeof(GwyLayerPointer),
            0,
            (GInstanceInitFunc)gwy_layer_pointer_init,
            NULL,
        };
        gwy_debug("");
        gwy_layer_pointer_type
            = g_type_register_static(GWY_TYPE_VECTOR_LAYER,
                                     GWY_LAYER_POINTER_TYPE_NAME,
                                     &gwy_layer_pointer_info,
                                     0);
    }

    return gwy_layer_pointer_type;
}

static void
gwy_layer_pointer_class_init(GwyLayerPointerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_pointer_finalize;

    layer_class->plugged = gwy_layer_pointer_plugged;
    layer_class->unplugged = gwy_layer_pointer_unplugged;

    vector_class->draw = gwy_layer_pointer_draw;
    vector_class->motion_notify = gwy_layer_pointer_motion_notify;
    vector_class->button_press = gwy_layer_pointer_button_pressed;
    vector_class->button_release = gwy_layer_pointer_button_released;
    vector_class->unselect = gwy_layer_pointer_unselect;
    vector_class->get_selection = gwy_layer_pointer_get_selection;

    klass->point_cursor = NULL;
}

static void
gwy_layer_pointer_init(GwyLayerPointer *layer)
{
    GwyLayerPointerClass *klass;

    gwy_debug("");

    klass = GWY_LAYER_POINTER_GET_CLASS(layer);
    gwy_gdk_cursor_new_or_ref(&klass->point_cursor, GDK_CROSS);
    layer->x = 0.0;
    layer->y = 0.0;
    layer->selected = FALSE;
}

static void
gwy_layer_pointer_finalize(GObject *object)
{
    GwyLayerPointerClass *klass;

    gwy_debug("");

    g_return_if_fail(GWY_IS_LAYER_POINTER(object));

    klass = GWY_LAYER_POINTER_GET_CLASS(object);
    gwy_gdk_cursor_free_or_unref(&klass->point_cursor);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_layer_pointer_draw(GwyVectorLayer *layer,
                       GdkDrawable *drawable)
{
    g_return_if_fail(GWY_IS_LAYER_POINTER(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    return;
}

static gboolean
gwy_layer_pointer_motion_notify(GwyVectorLayer *layer,
                                GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerPointer *pointer_layer;
    GdkWindow *window;
    gint x, y;
    gdouble oldx, oldy, xreal, yreal;

    pointer_layer = GWY_LAYER_POINTER(layer);
    if (!pointer_layer->button)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    oldx = pointer_layer->x;
    oldy = pointer_layer->y;
    if (event->is_hint)
        gdk_window_get_pointer(window, &x, &y, NULL);
    else {
        x = event->x;
        y = event->y;
    }
    gwy_debug("x = %d, y = %d", x, y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (xreal == oldx && yreal == oldy)
        return FALSE;

    pointer_layer->x = xreal;
    pointer_layer->y = yreal;
    gwy_layer_pointer_save(pointer_layer);
    gwy_vector_layer_updated(layer);

    return FALSE;
}

static gboolean
gwy_layer_pointer_button_pressed(GwyVectorLayer *layer,
                                 GdkEventButton *event)
{
    GwyDataView *data_view;
    GwyLayerPointerClass *klass;
    GwyLayerPointer *pointer_layer;
    gint x, y;
    gdouble xreal, yreal;

    gwy_debug("");
    if (event->button != 1)
        return FALSE;

    pointer_layer = GWY_LAYER_POINTER(layer);
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);

    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    pointer_layer->button = event->button;
    pointer_layer->x = xreal;
    pointer_layer->y = yreal;
    pointer_layer->selected = TRUE;
    layer->in_selection = TRUE;
    klass = GWY_LAYER_POINTER_GET_CLASS(layer);
    gdk_window_set_cursor(GTK_WIDGET(data_view)->window, klass->point_cursor);

    return FALSE;
}

static gboolean
gwy_layer_pointer_button_released(GwyVectorLayer *layer,
                                  GdkEventButton *event)
{
    GwyDataView *data_view;
    GwyLayerPointerClass *klass;
    GwyLayerPointer *pointer_layer;
    gint x, y;
    gdouble xreal, yreal;

    pointer_layer = GWY_LAYER_POINTER(layer);
    if (!pointer_layer->button)
        return FALSE;
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);

    pointer_layer->button = 0;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    pointer_layer->x = xreal;
    pointer_layer->y = yreal;
    pointer_layer->selected = TRUE;
    gwy_layer_pointer_save(pointer_layer);
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));

    layer->in_selection = FALSE;
    klass = GWY_LAYER_POINTER_GET_CLASS(pointer_layer);
    gdk_window_set_cursor(GTK_WIDGET(data_view)->window, NULL);

    gwy_vector_layer_selection_finished(layer);

    return FALSE;
}

static gboolean
gwy_layer_pointer_get_selection(GwyVectorLayer *layer,
                                gdouble *selection)
{
    GwyLayerPointer *pointer_layer;

    g_return_val_if_fail(GWY_IS_LAYER_POINTER(layer), FALSE);
    pointer_layer = GWY_LAYER_POINTER(layer);

    if (!pointer_layer->selected)
        return FALSE;

    if (selection) {
        selection[0] = pointer_layer->x;
        selection[1] = pointer_layer->y;
    }

    return TRUE;
}

static void
gwy_layer_pointer_unselect(GwyVectorLayer *layer)
{
    GwyLayerPointer *pointer_layer;

    g_return_if_fail(GWY_IS_LAYER_POINTER(layer));
    pointer_layer = GWY_LAYER_POINTER(layer);

    if (!pointer_layer->selected)
        return;

    pointer_layer->selected = FALSE;
    gwy_layer_pointer_save(pointer_layer);
}

static void
gwy_layer_pointer_plugged(GwyDataViewLayer *layer)
{
    GwyLayerPointer *pointer_layer;

    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_POINTER(layer));
    pointer_layer = GWY_LAYER_POINTER(layer);

    pointer_layer->selected = FALSE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    gwy_layer_pointer_restore(pointer_layer);
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_pointer_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_POINTER(layer));

    GWY_LAYER_POINTER(layer)->selected = FALSE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_pointer_save(GwyLayerPointer *layer)
{
    GwyContainer *data;

    data = GWY_DATA_VIEW_LAYER(layer)->data;
    /* TODO Container */
    gwy_container_set_double_by_name(data, "/0/select/pointer/x", layer->x);
    gwy_container_set_double_by_name(data, "/0/select/pointer/y", layer->y);
}

static void
gwy_layer_pointer_restore(GwyLayerPointer *layer)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble xreal, yreal;

    data = GWY_DATA_VIEW_LAYER(layer)->data;
    /* TODO Container */
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);

    if (gwy_container_contains_by_name(data, "/0/select/pointer/x")) {
        layer->x
            = gwy_container_get_double_by_name(data, "/0/select/pointer/x");
        layer->x = CLAMP(layer->x, 0.0, xreal);
    }
    if (gwy_container_contains_by_name(data, "/0/select/pointer/y")) {
        layer->y
            = gwy_container_get_double_by_name(data, "/0/select/pointer/y");
        layer->y = CLAMP(layer->y, 0.0, yreal);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
