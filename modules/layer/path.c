/*
 *  @(#) $Id: path.c 17612 2015-10-19 12:54:58Z yeti-dn $
 *  Copyright (C) 2016 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/spline.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-layer.h>

#include "layer.h"

#define GWY_SELECTION_PATH_TYPE_NAME "GwySelectionPath"

#define GWY_TYPE_LAYER_PATH            (gwy_layer_path_get_type())
#define GWY_LAYER_PATH(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_PATH, GwyLayerPath))
#define GWY_IS_LAYER_PATH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_PATH))
#define GWY_LAYER_PATH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_PATH, GwyLayerPathClass))

#define GWY_TYPE_SELECTION_PATH            (gwy_selection_path_get_type())
#define GWY_SELECTION_PATH(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_PATH, GwySelectionPath))
#define GWY_IS_SELECTION_PATH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_PATH))
#define GWY_SELECTION_PATH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_PATH, GwySelectionPathClass))

enum {
    OBJECT_SIZE = 2
};

enum {
    PROP_0,
    PROP_SLACKNESS,
    PROP_CLOSED,
};

enum {
    PROP_00,
    PROP_THICKNESS,
};

typedef struct _GwyLayerPath          GwyLayerPath;
typedef struct _GwyLayerPathClass     GwyLayerPathClass;
typedef struct _GwySelectionPath      GwySelectionPath;
typedef struct _GwySelectionPathClass GwySelectionPathClass;

struct _GwyLayerPath {
    GwyVectorLayer parent_instance;

    GdkCursor *near_cursor;
    GdkCursor *move_cursor;

    /* Properties */
    gint thickness;
    gboolean point_numbers;

    /* Dynamic state */
    GwySpline *spline;
};

struct _GwyLayerPathClass {
    GwyVectorLayerClass parent_class;
};

struct _GwySelectionPath {
    GwySelection parent_instance;

    gdouble slackness;
    gboolean closed;
};

struct _GwySelectionPathClass {
    GwySelectionClass parent_class;
};

static gboolean    module_register                     (void);
static GType       gwy_layer_path_get_type             (void)                         G_GNUC_CONST;
static GType       gwy_selection_path_get_type         (void)                         G_GNUC_CONST;
static void        gwy_selection_path_set_property     (GObject *object,
                                                        guint prop_id,
                                                        const GValue *value,
                                                        GParamSpec *pspec);
static void        gwy_selection_path_get_property     (GObject*object,
                                                        guint prop_id,
                                                        GValue *value,
                                                        GParamSpec *pspec);
static void        gwy_selection_path_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_selection_path_serialize        (GObject *serializable,
                                                        GByteArray *buffer);
static GObject*    gwy_selection_path_deserialize      (const guchar *buffer,
                                                        gsize size,
                                                        gsize *position);
static GObject*    gwy_selection_path_duplicate        (GObject *object);
static void        gwy_selection_path_clone            (GObject *source,
                                                        GObject *copy);
static gboolean    gwy_selection_path_crop_object      (GwySelection *selection,
                                                        gint i,
                                                        gpointer user_data);
static void        gwy_selection_path_crop             (GwySelection *selection,
                                                        gdouble xmin,
                                                        gdouble ymin,
                                                        gdouble xmax,
                                                        gdouble ymax);
static void        gwy_selection_path_move             (GwySelection *selection,
                                                        gdouble vx,
                                                        gdouble vy);
static void        gwy_selection_path_set_slackness    (GwySelectionPath *selection,
                                                        gdouble slackness);
static void        gwy_selection_path_set_closed       (GwySelectionPath *selection,
                                                        gboolean closed);
static void        gwy_layer_path_finalize             (GObject *object);
static void        gwy_layer_path_set_property         (GObject *object,
                                                        guint prop_id,
                                                        const GValue *value,
                                                        GParamSpec *pspec);
static void        gwy_layer_path_get_property         (GObject*object,
                                                        guint prop_id,
                                                        GValue *value,
                                                        GParamSpec *pspec);
static void        gwy_layer_path_draw                 (GwyVectorLayer *layer,
                                                        GdkDrawable *drawable,
                                                        GwyRenderingTarget target);
static gboolean    gwy_layer_path_motion_notify        (GwyVectorLayer *layer,
                                                        GdkEventMotion *event);
static gboolean    gwy_layer_path_button_pressed       (GwyVectorLayer *layer,
                                                        GdkEventButton *event);
static gboolean    gwy_layer_path_button_released      (GwyVectorLayer *layer,
                                                        GdkEventButton *event);
static void        gwy_layer_path_set_thickness        (GwyLayerPath *layer,
                                                        gint thickness);
static void        gwy_layer_path_realize              (GwyDataViewLayer *dlayer);
static void        gwy_layer_path_unrealize            (GwyDataViewLayer *dlayer);
static gint        gwy_layer_path_near_point           (GwyVectorLayer *layer,
                                                        gdouble xreal,
                                                        gdouble yreal);

/* Allow to express intent. */
#define gwy_layer_path_undraw        gwy_layer_path_draw

static GwySerializableIface *gwy_selection_path_serializable_parent_iface;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of a single long curve."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE_EXTENDED
    (GwySelectionPath, gwy_selection_path, GWY_TYPE_SELECTION, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_selection_path_serializable_init))
G_DEFINE_TYPE(GwyLayerPath, gwy_layer_path, GWY_TYPE_VECTOR_LAYER)

static gboolean
module_register(void)
{
    gwy_layer_func_register(GWY_TYPE_LAYER_PATH);
    return TRUE;
}

static void
gwy_selection_path_class_init(GwySelectionPathClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    gobject_class->set_property = gwy_selection_path_set_property;
    gobject_class->get_property = gwy_selection_path_get_property;

    sel_class->object_size = OBJECT_SIZE;
    sel_class->crop = gwy_selection_path_crop;
    sel_class->move = gwy_selection_path_move;

    g_object_class_install_property
        (gobject_class,
         PROP_SLACKNESS,
         g_param_spec_double("slackness",
                             "Slackness",
                             "Slackness parameter of the spline curve",
                             0.0, 1.0, 1.0/G_SQRT2,
                             G_PARAM_READABLE | G_PARAM_WRITABLE));

    g_object_class_install_property
        (gobject_class,
         PROP_CLOSED,
         g_param_spec_boolean("closed",
                              "Closed",
                              "Whether the curve is closed, as opposed to "
                              "open-ended.",
                              FALSE,
                              G_PARAM_READWRITE));
}

static void
gwy_selection_path_serializable_init(GwySerializableIface *iface)
{
    gwy_selection_path_serializable_parent_iface
        = g_type_interface_peek_parent(iface);

    iface->serialize = gwy_selection_path_serialize;
    iface->deserialize = gwy_selection_path_deserialize;
    iface->duplicate = gwy_selection_path_duplicate;
    iface->clone = gwy_selection_path_clone;
}

static void
gwy_layer_path_class_init(GwyLayerPathClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    gobject_class->finalize = gwy_layer_path_finalize;
    gobject_class->set_property = gwy_layer_path_set_property;
    gobject_class->get_property = gwy_layer_path_get_property;

    layer_class->realize = gwy_layer_path_realize;
    layer_class->unrealize = gwy_layer_path_unrealize;

    vector_class->selection_type = GWY_TYPE_SELECTION_PATH;
    vector_class->draw = gwy_layer_path_draw;
    vector_class->motion_notify = gwy_layer_path_motion_notify;
    vector_class->button_press = gwy_layer_path_button_pressed;
    vector_class->button_release = gwy_layer_path_button_released;

    g_object_class_install_property
        (gobject_class,
         PROP_THICKNESS,
         g_param_spec_int("thickness",
                          "Thickness",
                          "Radius of marker to draw",
                          -1, 1024, 1,
                          G_PARAM_READWRITE));
}

static void
gwy_selection_path_init(GwySelectionPath *selection)
{
    /* Set max. number of objects to one */
    g_array_set_size(GWY_SELECTION(selection)->objects, OBJECT_SIZE);
    selection->slackness = 1.0/G_SQRT2;
    selection->closed = FALSE;
}

static GByteArray*
gwy_selection_path_serialize(GObject *serializable,
                             GByteArray *buffer)
{
    GwySelection *selection;

    g_return_val_if_fail(GWY_IS_SELECTION_PATH(serializable), NULL);

    selection = GWY_SELECTION(serializable);
    {
        guint32 len = selection->n * OBJECT_SIZE;
        guint32 max = selection->objects->len/OBJECT_SIZE;
        gboolean closed = GWY_SELECTION_PATH(selection)->closed;
        gdouble slackness = GWY_SELECTION_PATH(selection)->slackness;
        gpointer pdata = len ? &selection->objects->data : NULL;
        GwySerializeSpec spec[] = {
            { 'i', "max",       &max,       NULL, },
            { 'b', "closed",    &closed,    NULL, },
            { 'd', "slackness", &slackness, NULL, },
            { 'D', "data",      pdata,      &len, },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_SELECTION_PATH_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_selection_path_deserialize(const guchar *buffer,
                               gsize size,
                               gsize *position)
{
    gdouble *data = NULL;
    guint32 len = 0, max = 0;
    gdouble slackness = 1.0/G_SQRT2;
    gboolean closed = FALSE;
    GwySerializeSpec spec[] = {
        { 'i', "max",       &max,       NULL, },
        { 'b', "closed",    &closed,    NULL, },
        { 'd', "slackness", &slackness, NULL, },
        { 'D', "data",      &data,      &len, },
    };
    GwySelection *selection;

    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_SELECTION_PATH_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        return NULL;
    }

    selection = g_object_new(GWY_TYPE_SELECTION_PATH, NULL);
    GWY_SELECTION_PATH(selection)->closed = closed;
    GWY_SELECTION_PATH(selection)->slackness = slackness;
    g_array_set_size(selection->objects, 0);
    if (data && len) {
        if (len % OBJECT_SIZE)
            g_warning("Selection data size not multiple of object size. "
                      "Ignoring it.");
        else {
            g_array_append_vals(selection->objects, data, len);
            selection->n = len/OBJECT_SIZE;
        }
        g_free(data);
    }
    if (max > selection->n)
        g_array_set_size(selection->objects, max*OBJECT_SIZE);

    return (GObject*)selection;
}

static GObject*
gwy_selection_path_duplicate(GObject *object)
{
    GObject *copy;

    copy = gwy_selection_path_serializable_parent_iface->duplicate(object);
    GWY_SELECTION_PATH(copy)->closed = GWY_SELECTION_PATH(object)->closed;
    GWY_SELECTION_PATH(copy)->slackness
        = GWY_SELECTION_PATH(object)->slackness;

    return copy;
}

static void
gwy_selection_path_clone(GObject *source,
                         GObject *copy)
{
    GWY_SELECTION_PATH(copy)->closed
        = GWY_SELECTION_PATH(source)->closed;
    GWY_SELECTION_PATH(copy)->slackness
        = GWY_SELECTION_PATH(source)->slackness;
    /* Must do this at the end, it emits a signal. */
    gwy_selection_path_serializable_parent_iface->clone(source, copy);
}

static void
gwy_selection_path_set_property(GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
    GwySelectionPath *selection = GWY_SELECTION_PATH(object);

    switch (prop_id) {
        case PROP_SLACKNESS:
        gwy_selection_path_set_slackness(selection, g_value_get_double(value));
        break;

        case PROP_CLOSED:
        gwy_selection_path_set_closed(selection, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_selection_path_get_property(GObject*object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
    GwySelectionPath *selection = GWY_SELECTION_PATH(object);

    switch (prop_id) {
        case PROP_SLACKNESS:
        g_value_set_double(value, selection->slackness);
        break;

        case PROP_CLOSED:
        g_value_set_boolean(value, selection->closed);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/* FIXME: Is this reasonable?  The entire selection is one line so we may
 * just want to kill it as a whole if it sticks outside. */
static gboolean
gwy_selection_path_crop_object(GwySelection *selection,
                               gint i,
                               gpointer user_data)
{
    const gdouble *minmax = (const gdouble*)user_data;
    gdouble xy[OBJECT_SIZE];

    gwy_selection_get_object(selection, i, xy);
    return (xy[1] >= minmax[1] && xy[1] <= minmax[3]
            && xy[0] >= minmax[0] && xy[0] <= minmax[2]);
}

static void
gwy_selection_path_crop(GwySelection *selection,
                        gdouble xmin,
                        gdouble ymin,
                        gdouble xmax,
                        gdouble ymax)
{
    gdouble minmax[4] = { xmin, ymin, xmax, ymax };

    gwy_selection_filter(selection, gwy_selection_path_crop_object, minmax);
}

static void
gwy_selection_path_move(GwySelection *selection,
                        gdouble vx,
                        gdouble vy)
{
    gdouble *data = (gdouble*)selection->objects->data;
    guint i, n = selection->objects->len/OBJECT_SIZE;

    for (i = 0; i < n; i++) {
        data[OBJECT_SIZE*i + 0] += vx;
        data[OBJECT_SIZE*i + 1] += vy;
    }
}

static void
gwy_selection_path_set_slackness(GwySelectionPath *selection,
                                 gdouble slackness)
{
    g_return_if_fail(slackness >= 0.0 && slackness <= 1.0);
    if (slackness == selection->slackness)
        return;

    selection->slackness = slackness;
    g_object_notify(G_OBJECT(selection), "slackness");
}

static void
gwy_selection_path_set_closed(GwySelectionPath *selection,
                              gboolean closed)
{
    if (!closed == !selection->closed)
        return;

    selection->closed = !!closed;
    g_object_notify(G_OBJECT(selection), "closed");
}

static void
gwy_layer_path_init(GwyLayerPath *layer)
{
    layer->point_numbers = FALSE;
    layer->thickness = 0;
    layer->spline = gwy_spline_new();
}

static void
gwy_layer_path_finalize(GObject *object)
{
    GwyLayerPath *layer = GWY_LAYER_PATH(object);

    gwy_spline_free(layer->spline);
    if (G_OBJECT_CLASS(gwy_layer_path_parent_class)->finalize)
        G_OBJECT_CLASS(gwy_layer_path_parent_class)->finalize(object);
}

static void
gwy_layer_path_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    GwyLayerPath *layer = GWY_LAYER_PATH(object);

    switch (prop_id) {
        case PROP_THICKNESS:
        gwy_layer_path_set_thickness(layer, g_value_get_uint(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_path_get_property(GObject*object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    GwyLayerPath *layer = GWY_LAYER_PATH(object);

    switch (prop_id) {
        case PROP_THICKNESS:
        g_value_set_int(value, layer->thickness);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_path_draw(GwyVectorLayer *layer,
                    GdkDrawable *drawable,
                    GwyRenderingTarget target)
{
    GwyDataView *data_view;
    GwyLayerPath *layer_path;
    GwySelection *selection = layer->selection;
    GwySpline *spline;
    gdouble xy[OBJECT_SIZE];
    GwyTriangulationPointXY *screenxy;
    const GwyTriangulationPointXY *segmentxy;
    GdkPoint *ixy;
    gint width, height, xsize, ysize;
    gdouble xreal, yreal;
    guint i, n, nseg;

    if (!layer->selection)
        return;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_if_fail(data_view);

    layer_path = GWY_LAYER_PATH(layer);
    n = selection->n;

    /* Scale coordinates to screen/image pixels. */
    gdk_drawable_get_size(drawable, &width, &height);
    gwy_data_view_get_pixel_data_sizes(data_view, &xsize, &ysize);
    screenxy = g_new(GwyTriangulationPointXY, n);
    if (target == GWY_RENDERING_TARGET_PIXMAP_IMAGE) {
        gwy_data_view_get_real_data_sizes(data_view, &xreal, &yreal);
    }

    for (i = 0; i < n; i++) {
        GwyTriangulationPointXY *pt = screenxy + i;

        gwy_selection_get_object(selection, i, xy);
        switch (target) {
            case GWY_RENDERING_TARGET_SCREEN:
            gwy_data_view_coords_real_to_xy_float(data_view, xy[0], xy[1],
                                                  &pt->x, &pt->y);
            break;

            case GWY_RENDERING_TARGET_PIXMAP_IMAGE:
            pt->x = xy[0]*width/xreal;
            pt->y = xy[1]*height/yreal;
            break;

            default:
            g_return_if_reached();
            break;
        }
    }

    /* Construct a segmented approximation of the spline. */
    spline = layer_path->spline;
    gwy_spline_set_points(spline, screenxy, n);
    g_free(screenxy);

    gwy_spline_set_closed(spline, GWY_SELECTION_PATH(selection)->closed);
    gwy_spline_set_slackness(spline, GWY_SELECTION_PATH(selection)->slackness);

    segmentxy = gwy_spline_sample_naturally(spline, &nseg);

    /* Draw the segments. */
    ixy = g_new(GdkPoint, nseg);
    for (i = 0; i < nseg; i++) {
        ixy[i].x = (gint)floor(segmentxy[i].x + 0.001);
        ixy[i].y = (gint)floor(segmentxy[i].y + 0.001);
    }
    gdk_draw_lines(drawable, layer->gc, ixy, nseg);
    g_free(ixy);
}

static gboolean
gwy_layer_path_motion_notify(GwyVectorLayer *layer,
                             GdkEventMotion *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GdkCursor *cursor;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];

    if (!layer->selection)
        return FALSE;

    /* FIXME: No cursor change hint -- a bit too crude? */
    if (!layer->editable)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
    window = GTK_WIDGET(data_view)->window;

    i = layer->selecting;
    if (event->is_hint)
        gdk_window_get_pointer(window, &x, &y, NULL);
    else {
        x = event->x;
        y = event->y;
    }
    gwy_debug("x = %d, y = %d", x, y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (i > -1)
        gwy_selection_get_object(layer->selection, i, xy);
    if (i > -1 && xreal == xy[0] && yreal == xy[1])
        return FALSE;

    if (!layer->button) {
        i = gwy_layer_path_near_point(layer, xreal, yreal);
        cursor = GWY_LAYER_PATH(layer)->near_cursor;
        gdk_window_set_cursor(window, i == -1 ? NULL : cursor);
        return FALSE;
    }

    gwy_layer_path_undraw(layer, window, GWY_RENDERING_TARGET_SCREEN);
    g_assert(layer->selecting != -1);
    xy[0] = xreal;
    xy[1] = yreal;
    gwy_selection_set_object(layer->selection, i, xy);
    gwy_layer_path_draw(layer, window, GWY_RENDERING_TARGET_SCREEN);

    return FALSE;
}

static gboolean
gwy_layer_path_button_pressed(GwyVectorLayer *layer,
                               GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];

    if (!layer->selection)
        return FALSE;

    if (event->button != 1)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
    window = GTK_WIDGET(data_view)->window;

    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    xy[0] = xreal;
    xy[1] = yreal;

    i = gwy_layer_path_near_point(layer, xreal, yreal);
    /* just emit "object-chosen" when selection is not editable */
    if (!layer->editable) {
        if (i >= 0)
            gwy_vector_layer_object_chosen(layer, i);
        return FALSE;
    }
    /* handle existing selection */
    if (i >= 0) {
        layer->selecting = i;
    }
    else {
        /* add an object, or do nothing when maximum is reached */
        i = -1;
        if (gwy_selection_is_full(layer->selection)) {
            if (gwy_selection_get_max_objects(layer->selection) > 1)
                return FALSE;
            i = 0;
        }
        gwy_layer_path_undraw(layer, window, GWY_RENDERING_TARGET_SCREEN);
        layer->selecting = 0;    /* avoid "update" signal emission */
        layer->selecting = gwy_selection_set_object(layer->selection, i, xy);
        gwy_layer_path_draw(layer, window, GWY_RENDERING_TARGET_SCREEN);
    }
    layer->button = event->button;

    gdk_window_set_cursor(window, GWY_LAYER_PATH(layer)->move_cursor);
    gwy_vector_layer_object_chosen(layer, layer->selecting);

    return FALSE;
}

static gboolean
gwy_layer_path_button_released(GwyVectorLayer *layer,
                                GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GdkCursor *cursor;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean outside;

    if (!layer->selection)
        return FALSE;

    if (!layer->button)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
    window = GTK_WIDGET(data_view)->window;

    gwy_layer_path_undraw(layer, window, GWY_RENDERING_TARGET_SCREEN);
    layer->button = 0;
    x = event->x;
    y = event->y;
    i = layer->selecting;
    gwy_debug("i = %d", i);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    outside = (event->x != x) || (event->y != y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    xy[0] = xreal;
    xy[1] = yreal;
    gwy_selection_set_object(layer->selection, i, xy);
    gwy_layer_path_draw(layer, window, GWY_RENDERING_TARGET_SCREEN);

    layer->selecting = -1;
    i = gwy_layer_path_near_point(layer, xreal, yreal);
    outside = outside || (i == -1);
    cursor = GWY_LAYER_PATH(layer)->near_cursor;
    gdk_window_set_cursor(window, outside ? NULL : cursor);
    gwy_selection_finished(layer->selection);

    return FALSE;
}

static void
gwy_layer_path_set_thickness(GwyLayerPath *layer, gint thickness)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_PATH(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (thickness == layer->thickness)
        return;

    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_path_undraw(vector_layer, parent->window,
                               GWY_RENDERING_TARGET_SCREEN);
    layer->thickness = thickness;
    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_path_draw(vector_layer, parent->window,
                             GWY_RENDERING_TARGET_SCREEN);
    g_object_notify(G_OBJECT(layer), "thickness");
}

static void
gwy_layer_path_realize(GwyDataViewLayer *dlayer)
{
    GwyLayerPath *layer;
    GdkDisplay *display;

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_path_parent_class)->realize(dlayer);
    layer = GWY_LAYER_PATH(dlayer);
    display = gtk_widget_get_display(dlayer->parent);
    layer->near_cursor = gdk_cursor_new_for_display(display, GDK_FLEUR);
    layer->move_cursor = gdk_cursor_new_for_display(display, GDK_CROSS);
}

static void
gwy_layer_path_unrealize(GwyDataViewLayer *dlayer)
{
    GwyLayerPath *layer;

    layer = GWY_LAYER_PATH(dlayer);
    gdk_cursor_unref(layer->near_cursor);
    gdk_cursor_unref(layer->move_cursor);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_path_parent_class)->unrealize(dlayer);
}

static gint
gwy_layer_path_near_point(GwyVectorLayer *layer,
                           gdouble xreal, gdouble yreal)
{
    gdouble d2min, metric[4];
    gint i, n;

    if (!(n = gwy_selection_get_data(layer->selection, NULL))
        || layer->focus >= n)
        return -1;

    gwy_data_view_get_metric(GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent),
                             metric);
    if (layer->focus >= 0) {
        gdouble xy[OBJECT_SIZE];

        gwy_selection_get_object(layer->selection, layer->focus, xy);
        i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 1, xy, metric);
    }
    else {
        gdouble *xy = g_newa(gdouble, n*OBJECT_SIZE);

        gwy_selection_get_data(layer->selection, xy);
        i = gwy_math_find_nearest_point(xreal, yreal, &d2min, n, xy, metric);
    }
    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
