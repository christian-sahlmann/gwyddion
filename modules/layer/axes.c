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
 * gwy_layer_axes_new:
 *
 * Creates a new vertical/horizontal line selection layer.
 *
 * The default number of axes to select is three and the default orientation
 * os horizontal.
 *
 * Container keys: "/0/select/axes/0/x", "/0/select/axes/0/y",
 * "/0/select/axes/1/x", "/0/select/axes/1/y", etc.,
 * and "/0/select/axes/nselected".
 *
 * The selection (obtained from gwy_vector_layer_get_selection()) consists
 * of a list of x-coordinates alone (for vertical lines) or y-coordinates
 * alone (for horizontal lines).
 *
 * Returns: The newly created layer.
 */

#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwyvectorlayer.h>
#include <libgwydgets/gwydataview.h>
#include <libgwymodule/gwymodule.h>

#define GWY_TYPE_LAYER_AXES            (gwy_layer_axes_get_type())
#define GWY_LAYER_AXES(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_AXES, GwyLayerAxes))
#define GWY_LAYER_AXES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_AXES, GwyLayerAxesClass))
#define GWY_IS_LAYER_AXES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_AXES))
#define GWY_IS_LAYER_AXES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_AXES))
#define GWY_LAYER_AXES_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_AXES, GwyLayerAxesClass))

#define GWY_LAYER_AXES_TYPE_NAME "GwyLayerAxes"

#define PROXIMITY_DISTANCE 8
#define CROSS_SIZE 8

#define BITS_PER_SAMPLE 8

enum {
    PROP_0,
    PROP_MAX_AXES,
    PROP_ORIENTATION,
    PROP_LAST
};

typedef struct _GwyLayerAxes      GwyLayerAxes;
typedef struct _GwyLayerAxesClass GwyLayerAxesClass;

struct _GwyLayerAxes {
    GwyVectorLayer parent_instance;

    GtkOrientation orientation;
    gint naxes;
    gint nselected;
    gint inear;
    guint button;
    gdouble *axes;
};

struct _GwyLayerAxesClass {
    GwyVectorLayerClass parent_class;

    GdkCursor *near_cursor;
    GdkCursor *move_cursor;
};

/* Forward declarations */

static gboolean   module_register                (const gchar *name);
static GType      gwy_layer_axes_get_type        (void) G_GNUC_CONST;
static void       gwy_layer_axes_class_init      (GwyLayerAxesClass *klass);
static void       gwy_layer_axes_init            (GwyLayerAxes *layer);
static void       gwy_layer_axes_finalize        (GObject *object);
static void       gwy_layer_axes_set_property    (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void       gwy_layer_axes_get_property    (GObject*object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void       gwy_layer_axes_set_max_axes    (GwyLayerAxes *layer,
                                                  gint naxes);
static void       gwy_layer_axes_set_orientation (GwyLayerAxes *layer,
                                                  GtkOrientation orientation);
static void       gwy_layer_axes_draw            (GwyVectorLayer *layer,
                                                  GdkDrawable *drawable);
static void       gwy_layer_axes_draw_line       (GwyLayerAxes *layer,
                                                  GdkDrawable *drawable,
                                                  gint i);
static gboolean   gwy_layer_axes_motion_notify   (GwyVectorLayer *layer,
                                                  GdkEventMotion *event);
static gboolean   gwy_layer_axes_button_pressed  (GwyVectorLayer *layer,
                                                  GdkEventButton *event);
static gboolean   gwy_layer_axes_button_released (GwyVectorLayer *layer,
                                                  GdkEventButton *event);
static void       gwy_layer_axes_plugged         (GwyDataViewLayer *layer);
static void       gwy_layer_axes_unplugged       (GwyDataViewLayer *layer);
static gint       gwy_layer_axes_get_selection   (GwyVectorLayer *layer,
                                                  gdouble *selection);
static void       gwy_layer_axes_unselect        (GwyVectorLayer *layer);
static void       gwy_layer_axes_save            (GwyLayerAxes *layer,
                                                  gint i);
static void       gwy_layer_axes_restore         (GwyLayerAxes *layer);
static gint       gwy_layer_axes_near_point      (GwyLayerAxes *layer,
                                                  gdouble xreal,
                                                  gdouble yreal);

/* Local data */

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "layer-axes",
    "Layer allowing selection of horizontal or vertical lines.",
    "Yeti <yeti@gwyddion.net>",
    "1.0",
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
        "axes",
        0,
    };

    func_info.type = gwy_layer_axes_get_type();
    gwy_layer_func_register(name, &func_info);

    return TRUE;
}

static GType
gwy_layer_axes_get_type(void)
{
    static GType gwy_layer_axes_type = 0;

    if (!gwy_layer_axes_type) {
        static const GTypeInfo gwy_layer_axes_info = {
            sizeof(GwyLayerAxesClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_layer_axes_class_init,
            NULL,
            NULL,
            sizeof(GwyLayerAxes),
            0,
            (GInstanceInitFunc)gwy_layer_axes_init,
            NULL,
        };
        gwy_debug("");
        gwy_layer_axes_type
            = g_type_register_static(GWY_TYPE_VECTOR_LAYER,
                                     GWY_LAYER_AXES_TYPE_NAME,
                                     &gwy_layer_axes_info,
                                     0);
    }

    return gwy_layer_axes_type;
}

static void
gwy_layer_axes_class_init(GwyLayerAxesClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_axes_finalize;
    gobject_class->set_property = gwy_layer_axes_set_property;
    gobject_class->get_property = gwy_layer_axes_get_property;

    layer_class->plugged = gwy_layer_axes_plugged;
    layer_class->unplugged = gwy_layer_axes_unplugged;

    vector_class->draw = gwy_layer_axes_draw;
    vector_class->motion_notify = gwy_layer_axes_motion_notify;
    vector_class->button_press = gwy_layer_axes_button_pressed;
    vector_class->button_release = gwy_layer_axes_button_released;
    vector_class->get_selection = gwy_layer_axes_get_selection;
    vector_class->unselect = gwy_layer_axes_unselect;

    klass->near_cursor = NULL;
    klass->move_cursor = NULL;

    g_object_class_install_property(
        gobject_class,
        PROP_MAX_AXES,
        g_param_spec_int("max_axes",
                         _("Maximum number of axes"),
                         _("The maximum number of axes that can be selected"),
                         1, 1024, 3,
                         G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(
        gobject_class,
        PROP_ORIENTATION,
        g_param_spec_enum("orientation",
                          _("Orientation"),
                          _("Orientation of selected lines"),
                          GTK_TYPE_ORIENTATION,
                          GTK_ORIENTATION_HORIZONTAL,
                          G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gwy_layer_axes_init(GwyLayerAxes *layer)
{
    GwyLayerAxesClass *klass;

    gwy_debug("");

    klass = GWY_LAYER_AXES_GET_CLASS(layer);
    gwy_vector_layer_cursor_new_or_ref(&klass->near_cursor, GDK_FLEUR);
    gwy_vector_layer_cursor_new_or_ref(&klass->move_cursor, GDK_CROSS);

    layer->orientation = GTK_ORIENTATION_HORIZONTAL;
    layer->naxes = 3;
    layer->nselected = 0;
    layer->inear = -1;
    layer->axes = g_new(gdouble, 2*layer->naxes);
}

static void
gwy_layer_axes_finalize(GObject *object)
{
    GwyLayerAxesClass *klass;
    GwyLayerAxes *layer;

    gwy_debug("");

    g_return_if_fail(GWY_IS_LAYER_AXES(object));

    layer = (GwyLayerAxes*)object;
    klass = GWY_LAYER_AXES_GET_CLASS(object);
    gwy_vector_layer_cursor_free_or_unref(&klass->near_cursor);
    gwy_vector_layer_cursor_free_or_unref(&klass->move_cursor);

    g_free(layer->axes);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_layer_axes_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyLayerAxes *layer = GWY_LAYER_AXES(object);

    switch (prop_id) {
        case PROP_MAX_AXES:
        gwy_layer_axes_set_max_axes(layer, g_value_get_int(value));
        break;

        case PROP_ORIENTATION:
        gwy_layer_axes_set_orientation(layer, g_value_get_enum(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_axes_get_property(GObject*object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    GwyLayerAxes *layer = GWY_LAYER_AXES(object);

    switch (prop_id) {
        case PROP_MAX_AXES:
        g_value_set_int(value, layer->naxes);
        break;

        case PROP_ORIENTATION:
        g_value_set_enum(value, layer->orientation);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_axes_set_max_axes(GwyLayerAxes *layer,
                            gint naxes)
{
    g_return_if_fail(GWY_IS_LAYER_AXES(layer));
    g_return_if_fail(naxes > 0 && naxes < 1024);

    layer->naxes = naxes;
    layer->nselected = MIN(layer->nselected, naxes);
    if (layer->inear >= naxes)
        layer->inear = -1;
    layer->axes = g_renew(gdouble, layer->axes, 2*layer->naxes);
}

static void
gwy_layer_axes_set_orientation(GwyLayerAxes *layer,
                               GtkOrientation orientation)
{
    g_return_if_fail(GWY_IS_LAYER_AXES(layer));
    g_return_if_fail(orientation == GTK_ORIENTATION_HORIZONTAL
                     || orientation == GTK_ORIENTATION_VERTICAL);
    if (orientation == layer->orientation)
        return;

    if (layer->nselected)
        gwy_layer_axes_unselect(GWY_VECTOR_LAYER(layer));
    layer->orientation = orientation;
}

static void
gwy_layer_axes_draw(GwyVectorLayer *layer,
                    GdkDrawable *drawable)
{
    GwyLayerAxes *axes_layer;
    gint i;

    g_return_if_fail(GWY_IS_LAYER_AXES(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    axes_layer = GWY_LAYER_AXES(layer);
    for (i = 0; i < axes_layer->nselected; i++)
        gwy_layer_axes_draw_line(axes_layer, drawable, i);
}

static void
gwy_layer_axes_draw_line(GwyLayerAxes *layer,
                         GdkDrawable *drawable,
                         gint i)
{
    GwyDataView *data_view;
    GwyVectorLayer *vector_layer;
    gint coord, width, height;

    g_return_if_fail(GWY_IS_LAYER_AXES(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    vector_layer = GWY_VECTOR_LAYER(layer);
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_if_fail(i >= 0 && i < layer->nselected);

    if (!vector_layer->gc)
        gwy_vector_layer_setup_gc(vector_layer);


    gdk_drawable_get_size(drawable, &width, &height);
    switch (layer->orientation) {
        case GTK_ORIENTATION_HORIZONTAL:
        gwy_data_view_coords_real_to_xy(data_view,
                                        0.0, layer->axes[i],
                                        NULL, &coord);
        gdk_draw_line(drawable, vector_layer->gc, 0, coord, width, coord);
        break;

        case GTK_ORIENTATION_VERTICAL:
        gwy_data_view_coords_real_to_xy(data_view,
                                        layer->axes[i], 0.0,
                                        &coord, NULL);
        gdk_draw_line(drawable, vector_layer->gc, coord, 0, coord, height);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static gboolean
gwy_layer_axes_motion_notify(GwyVectorLayer *layer,
                             GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerAxesClass *klass;
    GwyLayerAxes *axes_layer;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal, rcoord;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    axes_layer = GWY_LAYER_AXES(layer);
    i = axes_layer->inear;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    rcoord = (axes_layer->orientation == GTK_ORIENTATION_VERTICAL)
             ? xreal : yreal;
    if (i > -1 && rcoord == axes_layer->axes[i])
        return FALSE;

    if (!axes_layer->button) {
        i = gwy_layer_axes_near_point(axes_layer, xreal, yreal);
        klass = GWY_LAYER_AXES_GET_CLASS(axes_layer);
        gdk_window_set_cursor(window, i == -1 ? NULL : klass->near_cursor);
        return FALSE;
    }

    g_assert(axes_layer->inear != -1);
    gwy_layer_axes_draw_line(axes_layer, window, i);
    axes_layer->axes[i] = rcoord;

    gwy_layer_axes_draw_line(axes_layer, window, i);
    gwy_layer_axes_save(axes_layer, i);

    gwy_vector_layer_updated(layer);

    return FALSE;
}

static gboolean
gwy_layer_axes_button_pressed(GwyVectorLayer *layer,
                              GdkEventButton *event)
{
    GwyDataView *data_view;
    GwyLayerAxesClass *klass;
    GdkWindow *window;
    GwyLayerAxes *axes_layer;
    gint x, y, i;
    gdouble xreal, yreal;

    gwy_debug("");
    axes_layer = GWY_LAYER_AXES(layer);
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    /* handle existing axes */
    i = gwy_layer_axes_near_point(axes_layer, xreal, yreal);
    if (i >= 0) {
        gwy_layer_axes_draw_line(axes_layer, window, i);
        axes_layer->inear = i;
    }
    else {
        /* add a point, or do nothing when maximum is reached */
        if (axes_layer->nselected == axes_layer->naxes)
            return FALSE;
        i = axes_layer->inear = axes_layer->nselected;
        axes_layer->nselected++;
    }
    axes_layer->button = event->button;
    axes_layer->axes[i] = (axes_layer->orientation == GTK_ORIENTATION_VERTICAL)
                          ? xreal : yreal;
    gwy_layer_axes_draw_line(axes_layer, window, i);

    layer->in_selection = TRUE;
    klass = GWY_LAYER_AXES_GET_CLASS(axes_layer);
    gdk_window_set_cursor(window, klass->move_cursor);

    return FALSE;
}

static gboolean
gwy_layer_axes_button_released(GwyVectorLayer *layer,
                               GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerAxesClass *klass;
    GwyLayerAxes *axes_layer;
    gint x, y, i;
    gdouble xreal, yreal;
    gboolean outside;

    axes_layer = GWY_LAYER_AXES(layer);
    if (!axes_layer->button)
        return FALSE;
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    axes_layer->button = 0;
    x = event->x;
    y = event->y;
    i = axes_layer->inear;
    gwy_debug("i = %d", i);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    outside = (event->x != x) || (event->y != y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    gwy_layer_axes_draw_line(axes_layer, window, i);
    axes_layer->axes[i] = (axes_layer->orientation == GTK_ORIENTATION_VERTICAL)
                          ? xreal : yreal;
    gwy_layer_axes_save(axes_layer, i);
    gwy_layer_axes_draw_line(axes_layer, window, i);
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));

    layer->in_selection = FALSE;
    klass = GWY_LAYER_AXES_GET_CLASS(axes_layer);
    i = gwy_layer_axes_near_point(axes_layer, xreal, yreal);
    gdk_window_set_cursor(window,
                          (i == -1 || outside) ? NULL : klass->near_cursor);

    if (axes_layer->nselected == axes_layer->naxes)
        gwy_vector_layer_selection_finished(layer);

    return FALSE;
}

static gint
gwy_layer_axes_get_selection(GwyVectorLayer *layer,
                             gdouble *selection)
{
    GwyLayerAxes *axes_layer;

    g_return_val_if_fail(GWY_IS_LAYER_AXES(layer), 0);
    axes_layer = GWY_LAYER_AXES(layer);

    if (selection && axes_layer->nselected)
        memcpy(selection,
               axes_layer->axes, axes_layer->nselected*sizeof(gdouble));

    return axes_layer->nselected;
}

static void
gwy_layer_axes_unselect(GwyVectorLayer *layer)
{
    GwyLayerAxes *axes_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_AXES(layer));
    axes_layer = GWY_LAYER_AXES(layer);

    if (axes_layer->nselected == 0)
        return;

    parent = GWY_DATA_VIEW_LAYER(layer)->parent;
    /* this is in fact undraw */
    if (parent)
        gwy_layer_axes_draw(layer, parent->window);
    axes_layer->nselected = 0;
    gwy_layer_axes_save(axes_layer, -1);
}

static void
gwy_layer_axes_plugged(GwyDataViewLayer *layer)
{
    GwyLayerAxes *axes_layer;

    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_AXES(layer));
    axes_layer = GWY_LAYER_AXES(layer);

    axes_layer->nselected = 0;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    gwy_layer_axes_restore(axes_layer);
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_axes_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_AXES(layer));

    GWY_LAYER_AXES(layer)->nselected = 0;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_axes_save(GwyLayerAxes *layer,
                      gint i)
{
    GwyContainer *data;
    gchar key[64];
    gchar c;
    gint from, to, n;

    data = GWY_DATA_VIEW_LAYER(layer)->data;
    /* TODO Container */
    gwy_container_set_int32_by_name(data, "/0/select/axes/nselected",
                                    layer->nselected);
    gwy_container_set_int32_by_name(data, "/0/select/axes/orientation",
                                    layer->orientation);
    if (i < 0) {
        from = 0;
        to = layer->nselected - 1;
    }
    else
        from = to = i;

    c = (layer->orientation == GTK_ORIENTATION_VERTICAL) ? 'x' : 'y';
    for (i = from; i <= to; i++) {
        n = g_snprintf(key, sizeof(key), "/0/select/axes/%d/%c", i, c);
        gwy_container_set_double_by_name(data, key, layer->axes[i]);
    }
}

static void
gwy_layer_axes_restore(GwyLayerAxes *layer)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gchar key[24];
    gchar c;
    gdouble rcoord;
    gint i, n, nsel;

    data = GWY_DATA_VIEW_LAYER(layer)->data;
    if (gwy_container_contains_by_name(data, "/0/select/axes/orientation"))
        layer->orientation
            = gwy_container_get_int32_by_name(data,
                                              "/0/select/axes/orientation");

    /* TODO Container */
    if (!gwy_container_contains_by_name(data, "/0/select/axes/nselected"))
        return;

    c = (layer->orientation == GTK_ORIENTATION_VERTICAL) ? 'x' : 'y';
    nsel = gwy_container_get_int32_by_name(data, "/0/select/axes/nselected");
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    rcoord = (layer->orientation == GTK_ORIENTATION_VERTICAL)
             ? gwy_data_field_get_xreal(dfield)
             : gwy_data_field_get_yreal(dfield);
    for (i = layer->nselected = 0;
         i < nsel && layer->nselected < layer->naxes;
         i++) {
        n = g_snprintf(key, sizeof(key), "/0/select/axes/%d/%c", i, c);
        layer->axes[i] = gwy_container_get_double_by_name(data, key);
        if (layer->axes[i] >= 0.0 && layer->axes[i] <= rcoord)
            layer->nselected++;
    }
}

static gint
gwy_layer_axes_near_point(GwyLayerAxes *layer,
                          gdouble xreal, gdouble yreal)
{
    GwyDataViewLayer *dlayer;
    gdouble dmin, rcoord, d;
    gint i, m;

    if (!layer->nselected)
        return -1;

    rcoord = (layer->orientation == GTK_ORIENTATION_VERTICAL) ? xreal : yreal;
    m = 0;
    dmin = fabs(rcoord - layer->axes[0]);
    for (i = 1; i < layer->nselected; i++) {
        d = fabs(rcoord - layer->axes[i]);
        if (d < dmin) {
            dmin = d;
            m = i;
        }
    }

    dlayer = (GwyDataViewLayer*)layer;
    dmin /= (layer->orientation == GTK_ORIENTATION_VERTICAL)
            ? gwy_data_view_get_xmeasure(GWY_DATA_VIEW(dlayer->parent))
            : gwy_data_view_get_ymeasure(GWY_DATA_VIEW(dlayer->parent));

    if (dmin > PROXIMITY_DISTANCE)
        return -1;
    return m;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
