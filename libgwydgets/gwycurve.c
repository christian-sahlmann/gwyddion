/*
 *  @(#) $Id$
 *  Copyright (C) 2005 Chris Anderson.
 *  E-mail: sidewinder.asu@gmail.com.
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

/* GTK - The GIMP Toolkit
 * Copyright (C) 1997 David Mosberger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * Modified by Chris Anderson 2005.
 * GwyCurve is based on GtkCurve (instead of subclassing) since GtkCurve
 * can be subject to removal from Gtk+ at some unspecified point in the future.
 */

/*TODO: Clean up code style to match gwyddion standards */
/*TODO: Fix cursor changes */
/*TODO: Fix warnings (unused variables, etc) */
/*TODO: Deal with freehand mode */
/*TODO: Change end-point behavior: hold constant value, don't go to zero */
/*TODO: Do something about problem of selecting end points (always gets red) */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gwycurve.h"
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtktable.h>
#include "gwydgettypes.h"

#define RADIUS          3   /* radius of the control points */
#define MIN_DISTANCE    8   /* min distance between control points */

#define GRAPH_MASK  (GDK_EXPOSURE_MASK |        \
                     GDK_POINTER_MOTION_MASK |  \
                     GDK_POINTER_MOTION_HINT_MASK | \
                     GDK_ENTER_NOTIFY_MASK |    \
                     GDK_BUTTON_PRESS_MASK |    \
                     GDK_BUTTON_RELEASE_MASK |  \
                     GDK_BUTTON1_MOTION_MASK)

#define find_slope(x1, y1, x2, y2) ((y2-y1)/(x2-x1))

#define SLOPE_TOLERANCE 0.05
#define compare_slopes(slope1, slope2) (fabs(slope1 - slope2) < SLOPE_TOLERANCE)


enum {
  PROP_0,
  PROP_CURVE_TYPE,
  PROP_MIN_X,
  PROP_MAX_X,
  PROP_MIN_Y,
  PROP_MAX_Y
};

static GtkDrawingAreaClass *parent_class = NULL;
static guint curve_type_changed_signal = 0;
static guint curve_edited_signal = 0;


/* forward declarations: */
static void gwy_curve_class_init    (GwyCurveClass *class);
static void gwy_curve_init          (GwyCurve *curve);
static void gwy_curve_get_property  (GObject *object,
                                     guint param_id,
                                     GValue *value,
                                     GParamSpec *pspec);
static void gwy_curve_set_property  (GObject *object,
                                     guint param_id,
                                     const GValue *value,
                                     GParamSpec *pspec);
static void gwy_curve_finalize      (GObject *object);
static void gwy_curve_size_graph    (GwyCurve *curve);

static gboolean gwy_curve_configure      (GwyCurve *c);
static gboolean gwy_curve_expose         (GwyCurve *c);
static gboolean gwy_curve_button_press   (GtkWidget *widget,
                                          GdkEventButton *event,
                                          GwyCurve *c);
static gboolean gwy_curve_button_release (GtkWidget *widget,
                                          GdkEventButton *event,
                                          GwyCurve *c);
static gboolean gwy_curve_motion_notify  (GwyCurve *c);

GType
gwy_curve_get_type (void)
{
  static GType curve_type = 0;

  if (!curve_type)
    {
      static const GTypeInfo curve_info =
      {
    sizeof (GwyCurveClass),
    NULL,       /* base_init */
    NULL,       /* base_finalize */
    (GClassInitFunc) gwy_curve_class_init,
    NULL,       /* class_finalize */
    NULL,       /* class_data */
    sizeof (GwyCurve),
    0,      /* n_preallocs */
    (GInstanceInitFunc) gwy_curve_init,
    NULL        /* value_table */
      };

      curve_type = g_type_register_static (GTK_TYPE_DRAWING_AREA, "GwyCurve",
                       &curve_info, 0);
    }
  return curve_type;
}

static void
gwy_curve_class_init (GwyCurveClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gwy_curve_finalize;

  gobject_class->set_property = gwy_curve_set_property;
  gobject_class->get_property = gwy_curve_get_property;

  g_object_class_install_property (gobject_class,
                   PROP_CURVE_TYPE,
                   g_param_spec_enum ("curve-type",
                              "Curve type",
                  "Is this curve linear, spline interpolated, or free-form",
                              GWY_TYPE_CURVE_TYPE,
                              GWY_CURVE_TYPE_LINEAR,
                              G_PARAM_READABLE |
                              G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                   PROP_MIN_X,
                   g_param_spec_float ("min-x",
                               "Minimum X",
                               "Minimum possible value for X",
                               -G_MAXFLOAT,
                               G_MAXFLOAT,
                               0.0,
                               G_PARAM_READABLE |
                               G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                   PROP_MAX_X,
                   g_param_spec_float ("max-x",
                               "Maximum X",
                               "Maximum possible X value",
                               -G_MAXFLOAT,
                               G_MAXFLOAT,
                                                       1.0,
                               G_PARAM_READABLE |
                               G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                   PROP_MIN_Y,
                   g_param_spec_float ("min-y",
                               "Minimum Y",
                               "Minimum possible value for Y",
                                                       -G_MAXFLOAT,
                               G_MAXFLOAT,
                               0.0,
                               G_PARAM_READABLE |
                               G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                   PROP_MAX_Y,
                   g_param_spec_float ("max-y",
                               "Maximum Y",
                               "Maximum possible value for Y",
                               -G_MAXFLOAT,
                               G_MAXFLOAT,
                               1.0,
                               G_PARAM_READABLE |
                               G_PARAM_WRITABLE));

    curve_type_changed_signal =
        g_signal_new ("curve-type-changed",
                      G_OBJECT_CLASS_TYPE (gobject_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (GwyCurveClass, curve_type_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    curve_edited_signal =
        g_signal_new ("curve-edited",
                      G_OBJECT_CLASS_TYPE (gobject_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (GwyCurveClass, curve_edited),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

static void
gwy_curve_init (GwyCurve *curve)
{
    gint old_mask;
    gint i;

    curve->cursor_type = GDK_TOP_LEFT_ARROW;
    curve->pixmap = NULL;
    curve->curve_type = GWY_CURVE_TYPE_LINEAR;
    curve->height = 0;
    curve->grab_point = -1;
    curve->grab_channel = -1;

    for (i=0; i<3; i++) {
        curve->channel_data[i].num_points = 0;
        curve->channel_data[i].points = NULL;
        curve->channel_data[i].num_ctlpoints = 0;
        curve->channel_data[i].ctlpoints = NULL;
    }

    curve->min_x = 0.0;
    curve->max_x = 1.0;
    curve->min_y = 0.0;
    curve->max_y = 1.0;

    old_mask = gtk_widget_get_events(GTK_WIDGET (curve));
    gtk_widget_set_events(GTK_WIDGET(curve), old_mask | GRAPH_MASK);

    g_signal_connect_swapped(curve, "configure-event",
                             G_CALLBACK(gwy_curve_configure), curve);
    g_signal_connect_swapped(curve, "expose-event",
                             G_CALLBACK(gwy_curve_expose), curve);
    g_signal_connect(curve, "button-press-event",
                     G_CALLBACK(gwy_curve_button_press), curve);
    g_signal_connect(curve, "button-release-event",
                     G_CALLBACK(gwy_curve_button_release), curve);
    g_signal_connect_swapped(curve, "motion-notify-event",
                             G_CALLBACK(gwy_curve_motion_notify), curve);

    gwy_curve_size_graph (curve);
}

static void
gwy_curve_set_property (GObject              *object,
            guint                 prop_id,
            const GValue         *value,
            GParamSpec           *pspec)
{
  GwyCurve *curve = GWY_CURVE (object);

  switch (prop_id)
    {
    case PROP_CURVE_TYPE:
      gwy_curve_set_curve_type (curve, g_value_get_enum (value));
      break;
    case PROP_MIN_X:
      gwy_curve_set_range (curve, g_value_get_float (value), curve->max_x,
               curve->min_y, curve->max_y);
      break;
    case PROP_MAX_X:
      gwy_curve_set_range (curve, curve->min_x, g_value_get_float (value),
               curve->min_y, curve->max_y);
      break;
    case PROP_MIN_Y:
      gwy_curve_set_range (curve, curve->min_x, curve->max_x,
               g_value_get_float (value), curve->max_y);
      break;
    case PROP_MAX_Y:
      gwy_curve_set_range (curve, curve->min_x, curve->max_x,
               curve->min_y, g_value_get_float (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gwy_curve_get_property (GObject              *object,
            guint                 prop_id,
            GValue               *value,
            GParamSpec           *pspec)
{
  GwyCurve *curve = GWY_CURVE (object);

  switch (prop_id)
    {
    case PROP_CURVE_TYPE:
      g_value_set_enum (value, curve->curve_type);
      break;
    case PROP_MIN_X:
      g_value_set_float (value, curve->min_x);
      break;
    case PROP_MAX_X:
      g_value_set_float (value, curve->max_x);
      break;
    case PROP_MIN_Y:
      g_value_set_float (value, curve->min_y);
      break;
    case PROP_MAX_Y:
      g_value_set_float (value, curve->max_y);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static int
project (gfloat value, gfloat min, gfloat max, int norm)
{
  return (norm - 1) * ((value - min) / (max - min)) + 0.5;
}

static gfloat
unproject (gint value, gfloat min, gfloat max, int norm)
{
  return value / (gfloat) (norm - 1) * (max - min) + min;
}

/* Solve the tridiagonal equation system that determines the second
   derivatives for the interpolation points.  (Based on Numerical
   Recipies 2nd Edition.) */
static void
spline_solve (int n, gfloat x[], gfloat y[], gfloat y2[])
{
  gfloat p, sig, *u;
  gint i, k;

  u = g_malloc ((n - 1) * sizeof (u[0]));

  y2[0] = u[0] = 0.0;   /* set lower boundary condition to "natural" */

  for (i = 1; i < n - 1; ++i)
    {
      sig = (x[i] - x[i - 1]) / (x[i + 1] - x[i - 1]);
      p = sig * y2[i - 1] + 2.0;
      y2[i] = (sig - 1.0) / p;
      u[i] = ((y[i + 1] - y[i])
          / (x[i + 1] - x[i]) - (y[i] - y[i - 1]) / (x[i] - x[i - 1]));
      u[i] = (6.0 * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
    }

  y2[n - 1] = 0.0;
  for (k = n - 2; k >= 0; --k)
    y2[k] = y2[k] * y2[k + 1] + u[k];

  g_free (u);
}

static gfloat
spline_eval (int n, gfloat x[], gfloat y[], gfloat y2[], gfloat val)
{
  gint k_lo, k_hi, k;
  gfloat h, b, a;

  /* do a binary search for the right interval: */
  k_lo = 0; k_hi = n - 1;
  while (k_hi - k_lo > 1)
    {
      k = (k_hi + k_lo) / 2;
      if (x[k] > val)
    k_hi = k;
      else
    k_lo = k;
    }

  h = x[k_hi] - x[k_lo];
  g_assert (h > 0.0);

  a = (x[k_hi] - val) / h;
  b = (val - x[k_lo]) / h;
  return a*y[k_lo] + b*y[k_hi] +
    ((a*a*a - a)*y2[k_lo] + (b*b*b - b)*y2[k_hi]) * (h*h)/6.0;
}

static void
gwy_curve_interpolate(GwyCurve *c, gint width, gint height)
{
    gfloat *vector;
    GwyChannelData *channel;
    int i, c_index;

    vector = g_malloc(width * sizeof (vector[0]));

    for (c_index=0; c_index<3; c_index++) {
        channel = &c->channel_data[c_index];

        gwy_curve_get_vector(c, c_index, width, vector);

        c->height = height;
        if (channel->num_points != width) {
            channel->num_points = width;
            if (channel->points)
                g_free (channel->points);
            channel->points = g_malloc(channel->num_points * sizeof(GwyPoint));
        }

        for (i=0; i<width; ++i) {
            channel->points[i].x = RADIUS + i;
            channel->points[i].y =
                RADIUS+height - project(vector[i], c->min_y, c->max_y, height);
        }
    }
    g_free (vector);
}

static GdkColor
get_color_from_rgb(gint red, gint green, gint blue)
{
    GdkColor col;

    col.red = 256 * red;
    col.green = 256 * green;
    col.blue = 256 * blue;
    col.pixel = 0;

    return col;
}

static void
gwy_curve_draw(GwyCurve *c, gint width, gint height)
{
    GtkStateType state;
    GtkStyle *style;
    gint i, c_index;
    GwyChannelData *channel;
    gboolean flag;
    gint x, y;
    GdkGC *gc;
    GdkColor colors[3];
    gint lastx, lasty;

    if (!c->pixmap)
        return;

    /* If the dimensions of the graph window have changed, then re-interpolate
       the curve points to match */
    flag = FALSE;
    for (i=0; i<3; i++)
        if (c->channel_data[i].num_points != width)
            flag = TRUE;
    if (c->height != height || flag)
        gwy_curve_interpolate(c, width, height);

    state = GTK_STATE_NORMAL;
    if (!GTK_WIDGET_IS_SENSITIVE (GTK_WIDGET (c)))
        state = GTK_STATE_INSENSITIVE;

    style = GTK_WIDGET(c)->style;

    /* clear the pixmap: */
    gtk_paint_flat_box(style, c->pixmap, GTK_STATE_NORMAL, GTK_SHADOW_NONE,
                       NULL, GTK_WIDGET(c), "curve_bg",
                       0, 0, width + RADIUS * 2, height + RADIUS * 2);

    /* draw the grid lines: (XXX make more meaningful) */
    for (i = 0; i < 5; i++) {
        gdk_draw_line(c->pixmap, style->dark_gc[state],
                      RADIUS, i * (height / 4.0) + RADIUS,
                      width + RADIUS, i * (height / 4.0) + RADIUS);
        gdk_draw_line(c->pixmap, style->dark_gc[state],
                      i * (width / 4.0) + RADIUS, RADIUS,
                      i * (width / 4.0) + RADIUS, height + RADIUS);
    }

    /* Setup colors */
    gc = gdk_gc_new(c->pixmap);
    colors[0]= get_color_from_rgb(255, 0, 0);
    colors[1] = get_color_from_rgb(0, 255, 0);
    colors[2] = get_color_from_rgb(0, 0, 255);

    /* Draw the curve points (for each channel) */
    for (c_index=0; c_index<3; c_index++) {
        gdk_gc_set_rgb_fg_color(gc, &colors[c_index]);

        channel = &c->channel_data[c_index];
        lastx = lasty = -1;
        for (i=0; i<channel->num_points; i++) {
            if (lastx > -1 && lasty > -1) {
                gdk_draw_line(c->pixmap, gc,
                              lastx, lasty,
                              (gint)channel->points[i].x,
                              (gint)channel->points[i].y);
            }
            lastx = (gint)channel->points[i].x;
            lasty = (gint)channel->points[i].y;
        }
    }

    /* Draw the control points (for each channel) */
    if (c->curve_type != GWY_CURVE_TYPE_FREE) {
        for (c_index=0; c_index<3; c_index++) {
            channel = &c->channel_data[c_index];
            for (i=0; i<channel->num_ctlpoints; ++i) {
                if (channel->ctlpoints[i].x < c->min_x)
                    continue;
                x = project(channel->ctlpoints[i].x,
                            c->min_x, c->max_x, width);
                y = height - project(channel->ctlpoints[i].y,
                                     c->min_y, c->max_y, height);

                /* draw a bullet: */
                gdk_draw_arc(c->pixmap, style->fg_gc[state], TRUE, x, y,
                             RADIUS * 2, RADIUS*2, 0, 360*64);
            }
        }
    }

    gdk_draw_drawable(GTK_WIDGET(c)->window, style->fg_gc[state], c->pixmap,
                      0, 0, 0, 0, width + RADIUS * 2, height + RADIUS * 2);

    g_object_unref(gc);
}

static gboolean
gwy_curve_configure(GwyCurve *c)
{
    GtkWidget *w;
    gint width, height;

    w = GTK_WIDGET(c);
    width = w->allocation.width - RADIUS * 2;
    height = w->allocation.height - RADIUS * 2;

    if (c->pixmap)
        g_object_unref(c->pixmap);
    c->pixmap = NULL;

    gwy_curve_expose(c);

    return TRUE;
}

static gboolean
gwy_curve_expose(GwyCurve *c)
{
    GtkWidget *w;
    gint width, height;

    w = GTK_WIDGET(c);
    width = w->allocation.width - RADIUS * 2;
    height = w->allocation.height - RADIUS * 2;
    if (width <= 0 || height <= 0)
        return TRUE;

    if (!c->pixmap)
        c->pixmap = gdk_pixmap_new(w->window,
                                   w->allocation.width,
                                   w->allocation.height, -1);
    gwy_curve_draw (c, width, height);
    return TRUE;
}

static gboolean
gwy_curve_button_press(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventButton *event,
                       GwyCurve *c)
{
    GdkCursorType new_type = c->cursor_type;
    gint tx, ty, x, y, cx, width, height;
    gint i;
    gint j;
    guint distance, distance2;
    gint closest_point = 0;
    gint closest_channel = 0;
    GtkWidget *w;
    GwyChannelData *channel;

    /* get the widget size */
    w = GTK_WIDGET(c);
    width = w->allocation.width - RADIUS * 2;
    height = w->allocation.height - RADIUS * 2;

    /*  get the pointer position  */
    gdk_window_get_pointer(w->window, &tx, &ty, NULL);
    x = CLAMP((tx - RADIUS), 0, width-1);
    y = CLAMP((ty - RADIUS), 0, height-1);

    /* determine closest channel to pointer */
    for (i=0, distance=~0U; i<3; i++) {
        channel = &c->channel_data[i];
        for (j=0, distance2=~0U; j<channel->num_points; j++) {
           if ((guint)abs(x - (guint)channel->points[j].x) < distance2) {
                distance2 = abs(x - (guint)channel->points[j].x);
                closest_point = j;
            }
        }
        if ((guint)abs(y-(guint)channel->points[closest_point].y) < distance) {
            distance = abs(y - (guint)channel->points[closest_point].y);
            closest_channel = i;
        }
    }
    closest_point = 0;
    channel = &c->channel_data[closest_channel];

    /* determine closest control point to pointer */
    for (i=0, distance = ~0U; i<channel->num_ctlpoints; ++i) {
        cx = project(channel->ctlpoints[i].x, c->min_x, c->max_x, width);
        if ((guint) abs(x - cx) < distance) {
            distance = abs(x - cx);
            closest_point = i;
        }
    }

    /* either add new point, or grab closest one */
    gtk_grab_add(widget);
    new_type = GDK_TCROSS;
    switch (c->curve_type) {
        case GWY_CURVE_TYPE_LINEAR:
        case GWY_CURVE_TYPE_SPLINE:
        if (distance > MIN_DISTANCE) {
            /* insert a new control point */
            if (channel->num_ctlpoints > 0) {
                cx = project(channel->ctlpoints[closest_point].x,
                             c->min_x, c->max_x, width);
                if (x > cx)
                    ++closest_point;
            }
            ++channel->num_ctlpoints;
            channel->ctlpoints =
                g_realloc(channel->ctlpoints,
                          channel->num_ctlpoints * sizeof(GwyPoint));
            for (i=channel->num_ctlpoints - 1; i>closest_point; --i)
                memcpy(channel->ctlpoints + i, channel->ctlpoints + i - 1,
                       sizeof(GwyPoint));
        }
        c->grab_point = closest_point;
        c->grab_channel = closest_channel;
        channel->ctlpoints[c->grab_point].x =
            unproject(x, c->min_x, c->max_x, width);
        channel->ctlpoints[c->grab_point].y =
            unproject(height - y, c->min_y, c->max_y, height);
        gwy_curve_interpolate(c, width, height);
        break;

        case GWY_CURVE_TYPE_FREE:
        /*TODO: Add freehand mode?
        c->point[x].x = RADIUS + x;
        c->point[x].y = RADIUS + y;
        c->grab_point = x;
        c->last = y;
        */
        break;
    }
    gwy_curve_draw(c, width, height);

    g_signal_emit(c, curve_edited_signal, 0);
    return TRUE;
}

static gboolean
gwy_curve_button_release(GtkWidget *widget,
                         G_GNUC_UNUSED GdkEventButton *event,
                         GwyCurve *c)
{
    GdkCursorType new_type = c->cursor_type;
    gint src, dst;
    gint width, height;
    GtkWidget *w;
    GwyChannelData *channel;

    w = GTK_WIDGET(c);
    width = w->allocation.width - RADIUS * 2;
    height = w->allocation.height - RADIUS * 2;

    gtk_grab_remove (widget);

    channel = &c->channel_data[c->grab_channel];

    /* delete inactive points: */
    if (c->curve_type != GWY_CURVE_TYPE_FREE) {
        for (src = dst = 0; src < channel->num_ctlpoints; ++src) {
            if (channel->ctlpoints[src].x >= c->min_x) {
                memcpy(channel->ctlpoints + dst, channel->ctlpoints + src,
                       sizeof(GwyPoint));
                ++dst;
            }
        }
        if (dst < src) {
            channel->num_ctlpoints -= (src - dst);
            if (channel->num_ctlpoints <= 0) {
                channel->num_ctlpoints = 1;
                channel->ctlpoints[0].x = c->min_x;
                channel->ctlpoints[0].y = c->min_y;
                gwy_curve_interpolate(c, width, height);
                gwy_curve_draw(c, width, height);
            }
            channel->ctlpoints =
                g_realloc(channel->ctlpoints,
                          channel->num_ctlpoints * sizeof(GwyPoint));
        }
    }
    new_type = GDK_FLEUR;
    c->grab_point = -1;
    c->grab_channel = -1;

    g_signal_emit(c, curve_edited_signal, 0);
    return TRUE;
}

static gboolean
gwy_curve_motion_notify(GwyCurve *c)
{
    GdkCursorType new_type = c->cursor_type;
    gint tx, ty, x, y, cx, width, height;
    gint i, j, leftbound, rightbound;
    gfloat rx, ry;
    GtkWidget *w;
    GwyChannelData *channel;
    guint distance, distance2;
    gint closest_point = 0;
    gint closest_channel = 0;

    /* get the widget size */
    w = GTK_WIDGET(c);
    width = w->allocation.width - RADIUS * 2;
    height = w->allocation.height - RADIUS * 2;

    /*  get the pointer position  */
    gdk_window_get_pointer(w->window, &tx, &ty, NULL);
    x = CLAMP((tx - RADIUS), 0, width-1);
    y = CLAMP((ty - RADIUS), 0, height-1);

    /* determine closest channel to pointer */
    for (i=0, distance=~0U; i<3; i++) {
        channel = &c->channel_data[i];
        for (j=0, distance2=~0U; j<channel->num_points; j++) {
           if ((guint)abs(x - (guint)channel->points[j].x) < distance2) {
                distance2 = abs(x - (guint)channel->points[j].x);
                closest_point = j;
            }
        }
        if ((guint)abs(y-(guint)channel->points[closest_point].y) < distance) {
            distance = abs(y - (guint)channel->points[closest_point].y);
            closest_channel = i;
        }
    }
    closest_point = 0;
    channel = &c->channel_data[closest_channel];

    /* determine closest control point to pointer */
    for (i=0, distance = ~0U; i<channel->num_ctlpoints; ++i) {
        cx = project(channel->ctlpoints[i].x, c->min_x, c->max_x, width);
        if ((guint) abs(x - cx) < distance) {
            distance = abs(x - cx);
            closest_point = i;
        }
    }
    /* determine closest control point to pointer */
    /*
    for (i=0; i<c->num_ctlpoints; ++i) {
        cx = project(c->ctlpoint[i][0], c->min_x, c->max_x, width);
        if ((guint) abs (x - cx) < distance)
        {
            distance = abs (x - cx);
            closest_point = i;
        }
    }
    */

    switch (c->curve_type) {
        case GWY_CURVE_TYPE_LINEAR:
        case GWY_CURVE_TYPE_SPLINE:
        if (c->grab_point == -1) {
            if (distance <= MIN_DISTANCE)
                new_type = GDK_FLEUR;
            else
                new_type = GDK_TCROSS;
        } else {
            /* drag the grabbed point  */
            channel = &c->channel_data[c->grab_channel];

            new_type = GDK_TCROSS;

            leftbound = -MIN_DISTANCE;
            if (c->grab_point > 0)
                leftbound = project(channel->ctlpoints[c->grab_point - 1].x,
                                    c->min_x, c->max_x, width);

            rightbound = width + RADIUS * 2 + MIN_DISTANCE;
            if (c->grab_point + 1 < channel->num_ctlpoints)
                rightbound = project(channel->ctlpoints[c->grab_point + 1].x,
                                     c->min_x, c->max_x, width);

            if (tx <= leftbound || tx >= rightbound
                || ty > height + RADIUS * 2 + MIN_DISTANCE
                || ty < -MIN_DISTANCE)
                channel->ctlpoints[c->grab_point].x = c->min_x - 1.0;
            else {
                rx = unproject(x, c->min_x, c->max_x, width);
                ry = unproject(height - y, c->min_y, c->max_y, height);
                channel->ctlpoints[c->grab_point].x = rx;
                channel->ctlpoints[c->grab_point].y = ry;
            }
            gwy_curve_interpolate(c, width, height);
            gwy_curve_draw(c, width, height);

            g_signal_emit(c, curve_edited_signal, 0);
        }
        break;


        case GWY_CURVE_TYPE_FREE:
        /*TODO: Add a free hand mode?
        if(c->grab_point != -1) {
            if (c->grab_point > x) {
                x_1 = x;
                x_2 = c->grab_point;
                y_1 = y;
                y_2 = c->last;
            } else {
                x_1 = c->grab_point;
                x_2 = x;
                y_1 = c->last;
                y_2 = y;
            }

            if (x_2 != x_1)
                for (i = x_1; i <= x_2; i++) {
                    c->point[i].x = RADIUS + i;
                    c->point[i].y = RADIUS +
                                    (y_1 + ((y_2 - y_1) *
                                    (i - x_1)) / (x_2 - x_1));
                }
            else {
                c->point[x].x = RADIUS + x;
                c->point[x].y = RADIUS + y;
            }
            c->grab_point = x;
            c->last = y;
            gwy_curve_draw(c, width, height);
        }
        if (event->state & GDK_BUTTON1_MASK)
            new_type = GDK_TCROSS;
        else
            new_type = GDK_PENCIL;
        */
        break;
    }
    if (new_type != (GdkCursorType)c->cursor_type) {
        GdkCursor *cursor;

        c->cursor_type = new_type;

        cursor = gdk_cursor_new_for_display(gtk_widget_get_display(w),
                                            c->cursor_type);
        gdk_window_set_cursor(w->window, cursor);
        gdk_cursor_unref(cursor);
    }

    return TRUE;
}

void
gwy_curve_set_curve_type(GwyCurve *c, GwyCurveType new_type)
{
    gfloat rx, dx;
    gint x, i;
    gint width, height;
    gint c_index;
    GwyChannelData *channel;

    if (new_type != c->curve_type) {
        width = GTK_WIDGET(c)->allocation.width - RADIUS * 2;
        height = GTK_WIDGET(c)->allocation.height - RADIUS * 2;

        if (new_type == GWY_CURVE_TYPE_FREE) {
            /* Going from any type to freehand */
            gwy_curve_interpolate(c, width, height);
            c->curve_type = new_type;
        }
        else if (c->curve_type == GWY_CURVE_TYPE_FREE) {
            /* Going from freehand, to some other type
            (need to generate control points based on the data): */
            for (c_index=0; c_index<3; c_index++) {
                channel = &c->channel_data[c_index];

                if (channel->ctlpoints)
                    g_free(channel->ctlpoints);
                channel->num_ctlpoints = 9;
                channel->ctlpoints =
                    g_malloc(channel->num_ctlpoints * sizeof(GwyPoint));

                rx = 0.0;
                dx = (width - 1) / (gfloat)(channel->num_ctlpoints - 1);

                for (i=0; i<channel->num_ctlpoints; ++i, rx += dx) {
                    x = (int)(rx + 0.5);
                    channel->ctlpoints[i].x =
                        unproject(x, c->min_x, c->max_x, width);
                    channel->ctlpoints[i].y =
                        unproject(RADIUS + height - channel->points[x].y,
                                  c->min_y, c->max_y, height);
                }
            }
            c->curve_type = new_type;
            gwy_curve_interpolate(c, width, height);
        }
        else {
            /* Going from spline to linear, or linear to spline */
            c->curve_type = new_type;
            gwy_curve_interpolate(c, width, height);
        }

        g_signal_emit(c, curve_type_changed_signal, 0);
        g_object_notify(G_OBJECT(c), "curve-type");
        gwy_curve_draw(c, width, height);
    }
}

static void
gwy_curve_size_graph (GwyCurve *curve)
{
  gint width, height;
  gfloat aspect;
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (curve));

  width  = (curve->max_x - curve->min_x) + 1;
  height = (curve->max_y - curve->min_y) + 1;
  aspect = width / (gfloat) height;
  if (width > gdk_screen_get_width (screen) / 4)
    width  = gdk_screen_get_width (screen) / 4;
  if (height > gdk_screen_get_height (screen) / 4)
    height = gdk_screen_get_height (screen) / 4;

  if (aspect < 1.0)
    width  = height * aspect;
  else
    height = width / aspect;

  gtk_widget_set_size_request (GTK_WIDGET (curve),
                   width + RADIUS * 2,
                   height + RADIUS * 2);
}

static void
gwy_curve_reset_vector(GwyCurve *curve)
{
    gint width, height;
    gint c_index;
    GwyChannelData *channel;

    for (c_index=0; c_index<3; c_index++) {
        channel = &curve->channel_data[c_index];

        if (channel->ctlpoints)
            g_free(channel->ctlpoints);

        channel->num_ctlpoints = 2;
        channel->ctlpoints = g_malloc(2 * sizeof(GwyPoint));
        channel->ctlpoints[0].x = curve->min_x;
        channel->ctlpoints[0].y = curve->min_y;
        channel->ctlpoints[1].x = curve->max_x;
        channel->ctlpoints[1].y = curve->max_y;
    }

    if (curve->pixmap) {
        width = GTK_WIDGET(curve)->allocation.width - RADIUS * 2;
        height = GTK_WIDGET(curve)->allocation.height - RADIUS * 2;

        if (curve->curve_type == GWY_CURVE_TYPE_FREE) {
            curve->curve_type = GWY_CURVE_TYPE_LINEAR;
            gwy_curve_interpolate(curve, width, height);
            curve->curve_type = GWY_CURVE_TYPE_FREE;
        }
        else
            gwy_curve_interpolate(curve, width, height);

        gwy_curve_draw(curve, width, height);
    }
}

void
gwy_curve_reset (GwyCurve *c)
{
  GwyCurveType old_type;

  old_type = c->curve_type;
  c->curve_type = GWY_CURVE_TYPE_LINEAR;
  gwy_curve_reset_vector (c);

  if (old_type != GWY_CURVE_TYPE_LINEAR)
    {
       g_signal_emit (c, curve_type_changed_signal, 0);
       g_object_notify (G_OBJECT (c), "curve-type");
    }
}

/*XXX - fixme
void
gwy_curve_set_gamma (GwyCurve *c, gfloat gamma_val)
{
  gfloat x, one_over_gamma, height, one_over_width;
  GwyCurveType old_type;
  gint i;

  if (c->num_points < 2)
    return;

  old_type = c->curve_type;
  c->curve_type = GWY_CURVE_TYPE_FREE;

  if (gamma_val <= 0)
    one_over_gamma = 1.0;
  else
    one_over_gamma = 1.0 / gamma_val;
  one_over_width = 1.0 / (c->num_points - 1);
  height = c->height;
  for (i = 0; i < c->num_points; ++i)
    {
      x = (gfloat) i / (c->num_points - 1);
      c->point[i].x = RADIUS + i;
      c->point[i].y =
    RADIUS + (height * (1.0 - pow (x, one_over_gamma)) + 0.5);
    }

  if (old_type != GWY_CURVE_TYPE_FREE)
    g_signal_emit (c, curve_type_changed_signal, 0);

  gwy_curve_draw (c, c->num_points, c->height);
}
*/

void
gwy_curve_set_range (GwyCurve *curve,
                     gfloat    min_x,
                     gfloat    max_x,
                     gfloat    min_y,
                     gfloat    max_y)
{
  g_object_freeze_notify (G_OBJECT (curve));
  if (curve->min_x != min_x) {
     curve->min_x = min_x;
     g_object_notify (G_OBJECT (curve), "min-x");
  }
  if (curve->max_x != max_x) {
     curve->max_x = max_x;
     g_object_notify (G_OBJECT (curve), "max-x");
  }
  if (curve->min_y != min_y) {
     curve->min_y = min_y;
     g_object_notify (G_OBJECT (curve), "min-y");
  }
  if (curve->max_y != max_y) {
     curve->max_y = max_y;
     g_object_notify (G_OBJECT (curve), "max-y");
  }
  g_object_thaw_notify (G_OBJECT (curve));

  gwy_curve_size_graph (curve);
  gwy_curve_reset_vector (curve);
}

void
gwy_curve_set_control_points(GwyCurve *curve, GwyChannelData *channel_data)
{
    GArray *remove_points;
    gint width, height;
    gint c_index, i, val;
    gint src, dst;
    gfloat slope1, slope2;
    GwyChannelData *channel;

    /* Copy the control point data out of channel_data into our curve */
    for (c_index=0; c_index<3; c_index++) {
        channel = &curve->channel_data[c_index];

        if (channel->ctlpoints)
            g_free(channel->ctlpoints);

        channel->num_ctlpoints = channel_data[c_index].num_ctlpoints;
        channel->ctlpoints = g_malloc(channel->num_ctlpoints*sizeof(GwyPoint));
        for (i=0; i<channel->num_ctlpoints; i++) {
            channel->ctlpoints[i].x = channel_data[c_index].ctlpoints[i].x;
            channel->ctlpoints[i].y = channel_data[c_index].ctlpoints[i].y;
        }
    }

    /* Show the slopes of each line segment for red */
    g_debug("\n\n\n\n\n\nSLOPES:");
    channel = &curve->channel_data[0];
    for (i=0; i<channel->num_ctlpoints-1; i++) {
        g_debug("Slope: %f",
        find_slope(channel->ctlpoints[i].x, channel->ctlpoints[i].y,
                   channel->ctlpoints[i+1].x, channel->ctlpoints[i+1].y));
    }

    g_debug("Compare:");

    /* Redundant Control Point Pruning: */
    for (c_index=0; c_index<3; c_index++) {
        channel = &curve->channel_data[c_index];

        /* Mark redundant control points for removal (by comparing slopes) */
        remove_points = g_array_new(FALSE, TRUE, sizeof(gint));
        for (i=0; i<channel->num_ctlpoints-2; i++) {
            slope1 =
            find_slope(channel->ctlpoints[i].x, channel->ctlpoints[i].y,
                       channel->ctlpoints[i+1].x, channel->ctlpoints[i+1].y);
            slope2 =
            find_slope(channel->ctlpoints[i+1].x, channel->ctlpoints[i+1].y,
                       channel->ctlpoints[i+2].x, channel->ctlpoints[i+2].y);

            if (c_index==0) {
                g_debug("Slope1: %f  Slope2: %f  diff: %f", slope1, slope2,
                        fabs(slope1 - slope2));
            }

            if (compare_slopes(slope1, slope2)) {
                val = i+1;
                g_array_append_val(remove_points, val);
            }
        }

        /* Delete marked control points: */
        for (i=0; i < remove_points->len; i++) {
            val = g_array_index(remove_points, gint, i);
            channel->ctlpoints[val].x = -1;
        }
        g_array_free(remove_points, TRUE);
        for (src = dst = 0; src < channel->num_ctlpoints; ++src) {
            if (channel->ctlpoints[src].x >= curve->min_x) {
                memcpy(channel->ctlpoints + dst, channel->ctlpoints + src,
                    sizeof(GwyPoint));
                ++dst;
            }
        }
        if (dst < src) {
            channel->num_ctlpoints -= (src - dst);
            if (channel->num_ctlpoints <= 0) {
                channel->num_ctlpoints = 1;
                channel->ctlpoints[0].x = curve->min_x;
                channel->ctlpoints[0].y = curve->min_y;
            }
            channel->ctlpoints = g_realloc(channel->ctlpoints,
                                channel->num_ctlpoints * sizeof(GwyPoint));
        }
    }

    /* Update */
    if (curve->pixmap) {
        width = GTK_WIDGET(curve)->allocation.width - RADIUS * 2;
        height = GTK_WIDGET(curve)->allocation.height - RADIUS * 2;

        if (curve->curve_type == GWY_CURVE_TYPE_FREE) {
            curve->curve_type = GWY_CURVE_TYPE_LINEAR;
            gwy_curve_interpolate(curve, width, height);
            curve->curve_type = GWY_CURVE_TYPE_FREE;
        }
        else
            gwy_curve_interpolate(curve, width, height);

        gwy_curve_draw(curve, width, height);
    }
}

gint
compare_func(gconstpointer a, gconstpointer b)
{
    gfloat val1, val2;

    val1 = *((gfloat*)a);
    val2 = *((gfloat*)b);

    if (val1 < val2)
        return -1;
    if (val1 == val2)
        return 0;
    if (val1 > val2)
        return 1;
    return 0;
}

void
remove_repeats(GArray *array)
{
    GArray *new_array;
    gint i;
    gfloat last = -1;
    gfloat val;

    /* Store array values into new_array, leaving out repeats */
    new_array = g_array_new(FALSE, TRUE, sizeof(gfloat));
    for (i=0; i < array->len; i++) {
        val = g_array_index(array, gfloat, i);
        if (val != last)
            g_array_append_val(new_array, val);
        last = val;
    }

    /* Empty out array */
    g_array_set_size(array, 0);

    /* Copy cleaned values into array */
    for (i=0; i < new_array->len; i++) {
        val = g_array_index(new_array, gfloat, i);
        g_array_append_val(array, val);
    }

    /* Free up new_array */
    g_array_free(new_array, TRUE);
}

void
gwy_curve_get_control_points(GwyCurve *curve, GwyChannelData *channel_data)
{
    /* WILL BE TRYING NEW IDEA FOR THIS */

    /* Copy curve->channel_data into channel_data */

    /* THIS IS THE OLD VERSION */
    GArray *x_array;
    GwyChannelData *channel;
    GwyChannelData *curve_channel;
    gint c_index, i, j, num_pts;
    gfloat val;
    GtkWidget *w;
    gint width, height, x, y;
    gfloat rx, ry, distance, min_distance, y_val;

    /* get the widget size */
    w = GTK_WIDGET(curve);
    width = w->allocation.width - RADIUS * 2;
    height = w->allocation.height - RADIUS * 2;

    /* First, store the x values of each control
    point from each channel into an array:*/
    x_array = g_array_new(FALSE, TRUE, sizeof(gfloat));
    for (c_index=0; c_index<3; c_index++) {
        curve_channel = &curve->channel_data[c_index];
        for (i=0; i < curve_channel->num_ctlpoints; i++)
            g_array_append_val(x_array, curve_channel->ctlpoints[i].x);
    }

    /* Sort the array, smallest to largest */
    g_array_sort(x_array, compare_func);

    /* Remove any repeated x-values */
    remove_repeats(x_array);
    num_pts = x_array->len;

    /* Create control point triplets within channel_data for each x-value: */
    for (c_index=0; c_index<3; c_index++) {
        channel = &channel_data[c_index];
        curve_channel = &curve->channel_data[c_index];
        channel->num_ctlpoints = num_pts;
        channel->ctlpoints = g_new(GwyPoint, num_pts);

        /* For each control point to be created, set its y-value based on the
        actual point that is nearest it */
        for (i=0; i < num_pts; i++) {
            val = g_array_index(x_array, gfloat, i);
            channel->ctlpoints[i].x = val;
            channel->ctlpoints[i].y = 1;

            min_distance = -1;
            y_val = 0;
            for (j=0; j < curve_channel->num_points; j++) {
                x = CLAMP((curve_channel->points[j].x - RADIUS), 0, width);
                y = CLAMP((curve_channel->points[j].y - RADIUS), 0, height);
                rx = unproject(x, curve->min_x, curve->max_x, width);
                ry = unproject(height - y, curve->min_y, curve->max_y, height);
                distance = fabs(rx - val);
                if (min_distance == -1 || distance < min_distance) {
                    min_distance = distance;
                    y_val = ry;
                }
            }
            channel->ctlpoints[i].y = y_val;
        }
    }

    g_array_free(x_array, TRUE);

    /* Dump array
    g_debug("\n\n\n\n");
    for (i=0; i<x_array->len; i++) {
        val = g_array_index(x_array, gfloat, i);
        g_debug("i: %i, val: %f", i, val);
    }
    */


    /* Copy the ctlpoints out of the gwycurve into channel_data */
    /*XXX: Old, remove later:
    for (c_index=0; c_index<3; c_index++) {
        channel = &channel_data[c_index];
        curve_channel = &curve->channel_data[c_index];
        channel->num_ctlpoints = curve_channel->num_ctlpoints;
        channel->ctlpoints = g_new(GwyPoint, channel->num_ctlpoints);
        for (i=0; i<channel->num_ctlpoints; i++) {
            channel->ctlpoints[i].x = curve_channel->ctlpoints[i].x;
            channel->ctlpoints[i].y = curve_channel->ctlpoints[i].y;
        }
    }
    */
}

/*XXX - fixme
void
gwy_curve_set_vector (GwyCurve *c, int veclen, gfloat vector[])
{
  GwyCurveType old_type;
  gfloat rx, dx, ry;
  gint i, height;
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (c));

  old_type = c->curve_type;
  c->curve_type = GWY_CURVE_TYPE_FREE;

  if (c->point)
    height = GTK_WIDGET (c)->allocation.height - RADIUS * 2;
  else
    {
      height = (c->max_y - c->min_y);
      if (height > gdk_screen_get_height (screen) / 4)
    height = gdk_screen_get_height (screen) / 4;

      c->height = height;
      c->num_points = veclen;
      c->point = g_malloc (c->num_points * sizeof (c->point[0]));
    }
  rx = 0;
  dx = (veclen - 1.0) / (c->num_points - 1.0);

  for (i = 0; i < c->num_points; ++i, rx += dx)
    {
      ry = vector[(int) (rx + 0.5)];
      if (ry > c->max_y) ry = c->max_y;
      if (ry < c->min_y) ry = c->min_y;
      c->point[i].x = RADIUS + i;
      c->point[i].y =
    RADIUS + height - project (ry, c->min_y, c->max_y, height);
    }
  if (old_type != GWY_CURVE_TYPE_FREE)
    {
       g_signal_emit (c, curve_type_changed_signal, 0);
       g_object_notify (G_OBJECT (c), "curve-type");
    }

  gwy_curve_draw (c, c->num_points, height);
}
*/

void
gwy_curve_get_vector(GwyCurve *c, gint c_index,
                     gint veclen, gfloat vector[])
{
    gfloat rx, ry, dx, dy, delta_x, *mem, *xv, *yv, *y2v, prev;
    gint dst, i, x, next, num_active_ctlpoints = 0, first_active = -1;
    GwyChannelData *channel;

    channel = &c->channel_data[c_index];

    if (c->curve_type != GWY_CURVE_TYPE_FREE) {

        /* count active points: */
        prev = c->min_x - 1.0;
        for (i=num_active_ctlpoints=0; i<channel->num_ctlpoints; ++i) {
            if (channel->ctlpoints[i].x > prev) {
                if (first_active < 0)
                    first_active = i;
                prev = channel->ctlpoints[i].x;
                ++num_active_ctlpoints;
            }
        }

        /* handle degenerate case: */
        if (num_active_ctlpoints < 2) {
            if (num_active_ctlpoints > 0)
                ry = channel->ctlpoints[first_active].y;
            else
                ry = c->min_y;
            if (ry < c->min_y) ry = c->min_y;
            if (ry > c->max_y) ry = c->max_y;
            for (x = 0; x < veclen; ++x)
                vector[x] = ry;
            return;
        }
    }

    switch (c->curve_type) {
        case GWY_CURVE_TYPE_SPLINE:
        mem = g_malloc(3 * num_active_ctlpoints * sizeof (gfloat));
        xv  = mem;
        yv  = mem + num_active_ctlpoints;
        y2v = mem + 2*num_active_ctlpoints;

        prev = c->min_x - 1.0;
        for (i = dst = 0; i < channel->num_ctlpoints; ++i) {
            if (channel->ctlpoints[i].x > prev) {
                prev = channel->ctlpoints[i].x;
                xv[dst] = channel->ctlpoints[i].x;
                yv[dst] = channel->ctlpoints[i].y;
                ++dst;
            }
        }

        spline_solve(num_active_ctlpoints, xv, yv, y2v);

        rx = c->min_x;
        dx = (c->max_x - c->min_x) / (veclen - 1);
        for (x = 0; x < veclen; ++x, rx += dx) {
            ry = spline_eval(num_active_ctlpoints, xv, yv, y2v, rx);
            if (ry < c->min_y) ry = c->min_y;
            if (ry > c->max_y) ry = c->max_y;
            vector[x] = ry;
        }

        g_free (mem);
        break;

        case GWY_CURVE_TYPE_LINEAR:
        dx = (c->max_x - c->min_x) / (veclen - 1);
        rx = c->min_x;
        ry = c->min_y;
        dy = 0.0;
        i  = first_active;
        for (x = 0; x < veclen; ++x, rx += dx) {
            if (rx >= channel->ctlpoints[i].x) {
                if (rx > channel->ctlpoints[i].x)
                    ry = c->min_y;
                dy = 0.0;
                next = i + 1;
                while (next < channel->num_ctlpoints &&
                       channel->ctlpoints[next].x <= channel->ctlpoints[i].x)
                    ++next;

                if (next < channel->num_ctlpoints) {
                    delta_x = channel->ctlpoints[next].x -
                              channel->ctlpoints[i].x;
                    dy = ((channel->ctlpoints[next].y -
                         channel->ctlpoints[i].y) / delta_x);
                    dy *= dx;
                    ry = channel->ctlpoints[i].y;
                    i = next;
                }
            }
            vector[x] = ry;
            ry += dy;
        }
        break;

        case GWY_CURVE_TYPE_FREE:
        if (channel->points) {
            rx = 0.0;
            dx = channel->num_points / (double) veclen;
            for (x = 0; x < veclen; ++x, rx += dx) {
                vector[x] = unproject(RADIUS + c->height -
                                      channel->points[(int)rx].y,
                                      c->min_y, c->max_y,
                                      c->height);
            }
        }
        else
            memset(vector, 0, veclen * sizeof(vector[0]));
        break;
    }
}

GtkWidget*
gwy_curve_new (void)
{
  return g_object_new (GWY_TYPE_CURVE, NULL);
}

static void
gwy_curve_finalize (GObject *object)
{
  GwyCurve *curve;
  gint i;

  g_return_if_fail (GWY_IS_CURVE (object));

  curve = GWY_CURVE (object);
  if (curve->pixmap)
    g_object_unref (curve->pixmap);

  for (i=0; i<3; i++) {
      if (curve->channel_data[i].points)
          g_free(curve->channel_data[i].points);
      if (curve->channel_data[i].ctlpoints)
          g_free(curve->channel_data[i].ctlpoints);
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

#define __GWY_CURVE_C__
