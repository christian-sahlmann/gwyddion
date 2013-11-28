/*
 *  @(#) $Id$
 *  Copyright (C) 2013 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-layer.h>

#include "layer.h"

#define GWY_TYPE_LAYER_LATTICE            (gwy_layer_lattice_get_type())
#define GWY_LAYER_LATTICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_LATTICE, GwyLayerLattice))
#define GWY_IS_LAYER_LATTICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_LATTICE))
#define GWY_LAYER_LATTICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_LATTICE, GwyLayerLatticeClass))

#define GWY_TYPE_SELECTION_LATTICE            (gwy_selection_lattice_get_type())
#define GWY_SELECTION_LATTICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_LATTICE, GwySelectionLattice))
#define GWY_IS_SELECTION_LATTICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_LATTICE))
#define GWY_SELECTION_LATTICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_LATTICE, GwySelectionLatticeClass))

enum {
    OBJECT_SIZE = 4
};

enum {
    PROP_0,
    PROP_N_LINES,
};

typedef struct _GwyLayerLattice          GwyLayerLattice;
typedef struct _GwyLayerLatticeClass     GwyLayerLatticeClass;
typedef struct _GwySelectionLattice      GwySelectionLattice;
typedef struct _GwySelectionLatticeClass GwySelectionLatticeClass;

struct _GwyLayerLattice {
    GwyVectorLayer parent_instance;

    GdkCursor *move_cursor;

    /* Properties */
    guint n_lines;

    /* Dynamic state */
    gdouble xorig, yorig;    /* Point where the user clicked. */
    gdouble xy[OBJECT_SIZE];
};

struct _GwyLayerLatticeClass {
    GwyVectorLayerClass parent_class;
};

struct _GwySelectionLattice {
    GwySelection parent_instance;
};

struct _GwySelectionLatticeClass {
    GwySelectionClass parent_class;
};

static gboolean module_register                  (void);
static GType    gwy_layer_lattice_get_type       (void)                           G_GNUC_CONST;
static GType    gwy_selection_lattice_get_type   (void)                           G_GNUC_CONST;
static void     gwy_layer_lattice_set_property   (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_layer_lattice_get_property   (GObject*object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_layer_lattice_draw           (GwyVectorLayer *layer,
                                                  GdkDrawable *drawable,
                                                  GwyRenderingTarget target);
static void     gwy_layer_lattice_draw_object    (GwyVectorLayer *layer,
                                                  GdkDrawable *drawable,
                                                  GwyRenderingTarget target,
                                                  gint id);
static gboolean gwy_layer_lattice_motion_notify  (GwyVectorLayer *layer,
                                                  GdkEventMotion *event);
static gboolean gwy_layer_lattice_button_pressed (GwyVectorLayer *layer,
                                                  GdkEventButton *event);
static gboolean gwy_layer_lattice_button_released(GwyVectorLayer *layer,
                                                  GdkEventButton *event);
static void     gwy_layer_lattice_set_n_lines    (GwyLayerLattice *layer,
                                                  guint nlines);
static void     gwy_layer_lattice_realize        (GwyDataViewLayer *dlayer);
static void     gwy_layer_lattice_unrealize      (GwyDataViewLayer *dlayer);
static void     transform_lattice                (GwyLayerLattice *layer_lattice,
                                                  gdouble xreal,
                                                  gdouble yreal,
                                                  gdouble *xy);
static void     matrix_mult                      (gdouble *dest,
                                                  const gdouble *src,
                                                  gdouble axx,
                                                  gdouble axy,
                                                  gdouble ayx,
                                                  gdouble ayy);

/* Allow to express intent. */
#define gwy_layer_lattice_undraw        gwy_layer_lattice_draw
#define gwy_layer_lattice_undraw_object gwy_layer_lattice_draw_object

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of a two-dimensional lattice."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2013",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwySelectionLattice, gwy_selection_lattice, GWY_TYPE_SELECTION)
G_DEFINE_TYPE(GwyLayerLattice, gwy_layer_lattice, GWY_TYPE_VECTOR_LAYER)

static gboolean
module_register(void)
{
    gwy_layer_func_register(GWY_TYPE_LAYER_LATTICE);
    return TRUE;
}

static void
gwy_selection_lattice_class_init(GwySelectionLatticeClass *klass)
{
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    sel_class->object_size = OBJECT_SIZE;
}

static void
gwy_layer_lattice_class_init(GwyLayerLatticeClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    gobject_class->set_property = gwy_layer_lattice_set_property;
    gobject_class->get_property = gwy_layer_lattice_get_property;

    layer_class->realize = gwy_layer_lattice_realize;
    layer_class->unrealize = gwy_layer_lattice_unrealize;

    vector_class->selection_type = GWY_TYPE_SELECTION_LATTICE;
    vector_class->draw = gwy_layer_lattice_draw;
    vector_class->motion_notify = gwy_layer_lattice_motion_notify;
    vector_class->button_press = gwy_layer_lattice_button_pressed;
    vector_class->button_release = gwy_layer_lattice_button_released;

    g_object_class_install_property
        (gobject_class,
         PROP_N_LINES,
         g_param_spec_uint("n-lines",
                           "N lines",
                           "Number of lattice lines to draw beside the "
                           "central ones",
                           0, 1024, 12,
                           G_PARAM_READWRITE));
}

static void
gwy_selection_lattice_init(GwySelectionLattice *selection)
{
    /* Set max. number of objects to one */
    g_array_set_size(GWY_SELECTION(selection)->objects, OBJECT_SIZE);
}

static void
gwy_layer_lattice_init(GwyLayerLattice *layer)
{
    layer->n_lines = 12;
}

static void
gwy_layer_lattice_set_property(GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    GwyLayerLattice *layer = GWY_LAYER_LATTICE(object);

    switch (prop_id) {
        case PROP_N_LINES:
        gwy_layer_lattice_set_n_lines(layer, g_value_get_uint(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_lattice_get_property(GObject*object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    GwyLayerLattice *layer = GWY_LAYER_LATTICE(object);

    switch (prop_id) {
        case PROP_N_LINES:
        g_value_set_uint(value, layer->n_lines);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_lattice_draw(GwyVectorLayer *layer,
                       GdkDrawable *drawable,
                       GwyRenderingTarget target)
{
    gint n;

    if (!layer->selection)
        return;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    if (!GWY_LAYER_LATTICE(layer)->n_lines)
        return;

    n = gwy_selection_get_data(layer->selection, NULL);
    if (n)
        gwy_layer_lattice_draw_object(layer, drawable, target, 0);
}

static void
gwy_layer_lattice_draw_object(GwyVectorLayer *layer,
                              GdkDrawable *drawable,
                              GwyRenderingTarget target,
                              gint id)
{
    GwyDataView *data_view;
    gdouble xy[OBJECT_SIZE];
    gboolean has_object;
    gint xi0, yi0, xi1, yi1, width, height, nlines, i, j;
    gdouble xsize, ysize, xoff, yoff, xc, yc;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_if_fail(data_view);

    gwy_debug("%d", id);
    has_object = gwy_selection_get_object(layer->selection, id, xy);
    g_return_if_fail(has_object);

    /* Just copied draw_vector()! */
    gwy_data_view_get_real_data_sizes(data_view, &xsize, &ysize);
    gwy_data_view_get_real_data_offsets(data_view, &xoff, &yoff);
    xc = xoff + 0.5*xsize;
    yc = yoff + 0.5*ysize;
    gdk_drawable_get_size(drawable, &width, &height);

    nlines = GWY_LAYER_LATTICE(layer)->n_lines;

    for (j = -nlines; j <= nlines; j++) {
        gdouble xfrom = j*xy[0] - nlines*xy[2] + xc,
                yfrom = j*xy[1] - nlines*xy[3] + yc,
                xto = j*xy[0] + nlines*xy[2] + xc,
                yto = j*xy[1] + nlines*xy[3] + yc;

        gdk_gc_set_line_attributes(layer->gc, 1,
                                   j ? GDK_LINE_ON_OFF_DASH : GDK_LINE_SOLID,
                                   GDK_CAP_BUTT,
                                   GDK_JOIN_BEVEL);
        switch (target) {
            case GWY_RENDERING_TARGET_SCREEN:
            gwy_data_view_coords_real_to_xy(data_view,
                                            xfrom, yfrom, &xi0, &yi0);
            gwy_data_view_coords_real_to_xy(data_view,
                                            xto, yto, &xi1, &yi1);
            gwy_data_view_coords_xy_cut_line(data_view,
                                             &xi0, &yi0, &xi1, &yi1);
            break;

            case GWY_RENDERING_TARGET_PIXMAP_IMAGE:
            xi0 = floor((xfrom)*width/xsize);
            yi0 = floor((yfrom)*height/ysize);
            xi1 = floor((xto)*width/xsize);
            yi1 = floor((yto)*height/ysize);
            break;

            default:
            g_return_if_reached();
            break;
        }
        gdk_draw_line(drawable, layer->gc, xi0, yi0, xi1, yi1);
    }

    for (i = -nlines; i <= nlines; i++) {
        gdouble xfrom = -nlines*xy[0] + i*xy[2] + xc,
                yfrom = -nlines*xy[1] + i*xy[3] + yc,
                xto = nlines*xy[0] + i*xy[2] + xc,
                yto = nlines*xy[1] + i*xy[3] + yc;

        gdk_gc_set_line_attributes(layer->gc, 1,
                                   i ? GDK_LINE_ON_OFF_DASH : GDK_LINE_SOLID,
                                   GDK_CAP_BUTT,
                                   GDK_JOIN_BEVEL);
        switch (target) {
            case GWY_RENDERING_TARGET_SCREEN:
            gwy_data_view_coords_real_to_xy(data_view,
                                            xfrom, yfrom, &xi0, &yi0);
            gwy_data_view_coords_real_to_xy(data_view,
                                            xto, yto, &xi1, &yi1);
            gwy_data_view_coords_xy_cut_line(data_view,
                                             &xi0, &yi0, &xi1, &yi1);
            break;

            case GWY_RENDERING_TARGET_PIXMAP_IMAGE:
            xi0 = floor((xfrom)*width/xsize);
            yi0 = floor((yfrom)*height/ysize);
            xi1 = floor((xto)*width/xsize);
            yi1 = floor((yto)*height/ysize);
            break;

            default:
            g_return_if_reached();
            break;
        }
        gdk_draw_line(drawable, layer->gc, xi0, yi0, xi1, yi1);
    }
}

static gboolean
gwy_layer_lattice_motion_notify(GwyVectorLayer *layer,
                                GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerLattice *layer_lattice;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];

    if (!layer->selection)
        return FALSE;

    if (!layer->editable)
        return FALSE;

    if (!layer->button || layer->selecting == -1)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
    window = GTK_WIDGET(data_view)->window;
    layer_lattice = GWY_LAYER_LATTICE(layer);

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
    g_assert(layer->selecting != -1);
    gwy_layer_lattice_undraw_object(layer, window,
                                    GWY_RENDERING_TARGET_SCREEN, i);
    transform_lattice(layer_lattice, xreal, yreal, xy);
    gwy_selection_set_object(layer->selection, i, xy);
    gwy_layer_lattice_draw_object(layer, window,
                                  GWY_RENDERING_TARGET_SCREEN, i);

    return FALSE;
}

static gboolean
gwy_layer_lattice_button_pressed(GwyVectorLayer *layer,
                                 GdkEventButton *event)
{
    GwyLayerLattice *layer_lattice;
    GwyDataView *data_view;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal;

    if (!layer->editable)
        return FALSE;

    if (!layer->selection)
        return FALSE;

    if (!gwy_selection_get_data(layer->selection, NULL))
        return FALSE;

    if (event->button != 1)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
    window = GTK_WIDGET(data_view)->window;
    layer_lattice = GWY_LAYER_LATTICE(layer);

    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    layer_lattice->xorig = xreal;
    layer_lattice->yorig = yreal;

    i = 0;
    layer->selecting = i;
    layer->button = event->button;
    gwy_selection_get_object(layer->selection, i, layer_lattice->xy);
    gdk_window_set_cursor(window, GWY_LAYER_LATTICE(layer)->move_cursor);
    gwy_vector_layer_object_chosen(layer, layer->selecting);

    return FALSE;
}

static gboolean
gwy_layer_lattice_button_released(GwyVectorLayer *layer,
                                  GdkEventButton *event)
{
    GwyLayerLattice *layer_lattice;
    GwyDataView *data_view;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];

    if (!layer->selection)
        return FALSE;

    if (!layer->button)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
    window = GTK_WIDGET(data_view)->window;
    layer_lattice = GWY_LAYER_LATTICE(layer);

    layer->button = 0;
    x = event->x;
    y = event->y;
    i = layer->selecting;
    gwy_debug("i = %d", i);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    gwy_layer_lattice_undraw_object(layer, window,
                                    GWY_RENDERING_TARGET_SCREEN, i);
    transform_lattice(layer_lattice, xreal, yreal, xy);
    gwy_selection_set_object(layer->selection, i, xy);
    gwy_layer_lattice_draw_object(layer, window, GWY_RENDERING_TARGET_SCREEN, i);

    layer->selecting = -1;

    gwy_selection_finished(layer->selection);

    return FALSE;
}

static void
gwy_layer_lattice_set_n_lines(GwyLayerLattice *layer, guint nlines)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_LATTICE(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (nlines == layer->n_lines)
        return;

    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_lattice_undraw(vector_layer, parent->window,
                                 GWY_RENDERING_TARGET_SCREEN);
    layer->n_lines = nlines;
    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_lattice_draw(vector_layer, parent->window,
                               GWY_RENDERING_TARGET_SCREEN);
    g_object_notify(G_OBJECT(layer), "n-lines");
}

static void
gwy_layer_lattice_realize(GwyDataViewLayer *dlayer)
{
    GwyLayerLattice *layer;
    GdkDisplay *display;

    gwy_debug("");

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_lattice_parent_class)->realize(dlayer);
    layer = GWY_LAYER_LATTICE(dlayer);
    display = gtk_widget_get_display(dlayer->parent);
    layer->move_cursor = gdk_cursor_new_for_display(display, GDK_CROSS);
}

static void
gwy_layer_lattice_unrealize(GwyDataViewLayer *dlayer)
{
    GwyLayerLattice *layer;

    gwy_debug("");

    layer = GWY_LAYER_LATTICE(dlayer);
    gdk_cursor_unref(layer->move_cursor);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_lattice_parent_class)->unrealize(dlayer);
}

static void
transform_lattice(GwyLayerLattice *layer_lattice,
                  gdouble xreal, gdouble yreal,
                  gdouble *xy)
{
    GwyVectorLayer *layer;
    GwyDataView *data_view;
    gdouble xsize, ysize, xoff, yoff, xc, yc, xorig, yorig;
    gdouble alpha, r, rho, ca, sa;

    layer = GWY_VECTOR_LAYER(layer_lattice);
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);

    gwy_data_view_get_real_data_sizes(data_view, &xsize, &ysize);
    gwy_data_view_get_real_data_offsets(data_view, &xoff, &yoff);
    xc = xoff + 0.5*xsize;
    yc = yoff + 0.5*ysize;
    xorig = layer_lattice->xorig - xc;
    yorig = layer_lattice->yorig - yc;
    xreal -= xc;
    yreal -= yc;

    alpha = atan2(yreal, xreal) - atan2(yorig, xorig);
    r = xreal*xreal + yreal*yreal;
    rho = sqrt(r)/sqrt(xorig*xorig + yorig*yorig);
    ca = cos(alpha)/r;
    sa = sin(alpha)/r;

    matrix_mult(xy + 0, layer_lattice->xy + 0, ca, -sa, sa, ca);
    matrix_mult(xy + 2, layer_lattice->xy + 2, ca, -sa, sa, ca);
    matrix_mult(xy + 0, xy + 0,
                rho*xreal*xreal + yreal*yreal, xreal*yreal*(rho - 1.0),
                xreal*yreal*(rho - 1.0), rho*yreal*yreal + xreal*xreal);
    matrix_mult(xy + 2, xy + 2,
                rho*xreal*xreal + yreal*yreal, xreal*yreal*(rho - 1.0),
                xreal*yreal*(rho - 1.0), rho*yreal*yreal + xreal*xreal);
}

/* Permit dest = src */
static void
matrix_mult(gdouble *dest, const gdouble *src,
            gdouble axx, gdouble axy, gdouble ayx, gdouble ayy)
{
    gdouble xy[2];

    xy[0] = axx*src[0] + axy*src[1];
    xy[1] = ayx*src[0] + ayy*src[1];
    dest[0] = xy[0];
    dest[1] = xy[1];
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
