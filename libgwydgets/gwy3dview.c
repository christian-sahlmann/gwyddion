/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *  Copyright (C) 2004 Martin Siler.
 *  E-mail: silerm@physics.muni.cz.
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
#include <gdk/gdkevents.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#ifdef HAVE_GTKGLEXT
#include <gtk/gtkgl.h>
#endif

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#endif

#ifdef HAVE_GTKGLEXT
#include <GL/gl.h>
#endif

#include <pango/pangoft2.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgets.h>

#define DEG_2_RAD (G_PI/180.0)
#define RAD_2_DEG (180.0/G_PI)

#define BITS_PER_SAMPLE 8

#define  GWY_3D_ORTHO_CORRECTION  2.0
#define  GWY_3D_Z_DEFORMATION     0.01
#define  GWY_3D_Z_TRANSFORMATION  0.5
#define  GWY_3D_Z_DISPLACEMENT   -0.2

#define GWY_3D_TIMEOUT_DELAY      1000

#define connect_swapped_after(obj, signal, cb, data) \
    g_signal_connect_object(obj, signal, G_CALLBACK(cb), data, \
                            G_CONNECT_SWAPPED | G_CONNECT_AFTER)

#ifdef HAVE_GTKGLEXT
enum {
    GWY_3D_VIEW_DEFAULT_SIZE_X = 260,
    GWY_3D_VIEW_DEFAULT_SIZE_Y = 260
};

enum {
    GWY_3D_SHAPE_FULL    = 0,
    GWY_3D_SHAPE_REDUCED = 1,
    GWY_3D_N_LISTS
};

/* Changed components */
enum {
    GWY_3D_GRADIENT   = 1 << 0,
    GWY_3D_MATERIAL   = 1 << 1,
    GWY_3D_DATA_FIELD = 1 << 2,
    GWY_3D_MASK       = 0x07
};

enum {
    PROP_0,
    PROP_MOVEMENT,
    PROP_REDUCED_SIZE,
    PROP_DATA_KEY,
    PROP_SETUP_PREFIX,
    PROP_GRADIENT_KEY,
    PROP_MATERIAL_KEY,
    PROP_LAST
};

typedef struct {
    GLfloat x, y, z;
} Gwy3DVector;

/**
 * Gwy3DListPool:
 * @base: The first list id in the pool.
 * @size: List id range length.
 * @pool: Bits correspond to GL lists, 0 means free, 1 assigned.
 *
 * GL list pool.
 **/
typedef struct {
    guint base;
    guint size;
    guint64 pool;
} Gwy3DListPool;

static void     gwy_3d_view_destroy              (GtkObject *object);
static void     gwy_3d_view_finalize             (GObject *object);
static void     gwy_3d_view_set_property         (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_3d_view_get_property         (GObject*object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_3d_view_realize              (GtkWidget *widget);
static void     gwy_3d_view_unrealize            (GtkWidget *widget);
static void     gwy_3d_view_container_connect    (Gwy3DView *gwy3dview,
                                                  const gchar *data_key_string,
                                                  gulong *id,
                                                  GCallback callback);
static void     gwy_3d_view_data_item_changed    (Gwy3DView *gwy3dview);
static void     gwy_3d_view_data_field_connect   (Gwy3DView *gwy3dview);
static void     gwy_3d_view_data_field_disconnect(Gwy3DView *gwy3dview);
static void     gwy_3d_view_data_field_changed   (Gwy3DView *gwy3dview);
static void     gwy_3d_view_setup_item_changed   (Gwy3DView *gwy3dview);
static void     gwy_3d_view_ensure_setup         (Gwy3DView *gwy3dview);
static void     gwy_3d_view_setup_connect        (Gwy3DView *gwy3dview);
static void     gwy_3d_view_setup_disconnect     (Gwy3DView *gwy3dview);
static void     gwy_3d_view_setup_changed        (Gwy3DView *gwy3dview,
                                                  GParamSpec *pspec);
static void     gwy_3d_view_gradient_item_changed(Gwy3DView *gwy3dview);
static void     gwy_3d_view_gradient_connect     (Gwy3DView *gwy3dview);
static void     gwy_3d_view_gradient_disconnect  (Gwy3DView *gwy3dview);
static void     gwy_3d_view_gradient_changed     (Gwy3DView *gwy3dview);
static void     gwy_3d_view_material_item_changed(Gwy3DView *gwy3dview);
static void     gwy_3d_view_material_connect     (Gwy3DView *gwy3dview);
static void     gwy_3d_view_material_disconnect  (Gwy3DView *gwy3dview);
static void     gwy_3d_view_material_changed     (Gwy3DView *gwy3dview);
static void     gwy_3d_view_update_lists         (Gwy3DView *gwy3dview);
static void     gwy_3d_view_downsample_data      (Gwy3DView *gwy3dview);
static void     gwy_3d_view_realize_gl           (Gwy3DView *gwy3dview);
static void     gwy_3d_view_size_request         (GtkWidget *widget,
                                                  GtkRequisition *requisition);
static void     gwy_3d_view_size_allocate        (GtkWidget *widget,
                                                  GtkAllocation *allocation);
static gboolean gwy_3d_view_expose               (GtkWidget *widget,
                                                  GdkEventExpose *event);
static gboolean gwy_3d_view_configure            (GtkWidget *widget,
                                                  GdkEventConfigure *event);
static void     gwy_3d_view_send_configure       (Gwy3DView *gwy3dview);
static gboolean gwy_3d_view_button_press         (GtkWidget *widget,
                                                  GdkEventButton *event);
static Gwy3DVector* gwy_3d_make_normals          (GwyDataField *dfield,
                                                  Gwy3DVector *normals);
static gboolean gwy_3d_view_motion_notify        (GtkWidget *widget,
                                                  GdkEventMotion *event);
static void     gwy_3d_make_list                 (Gwy3DView *gwy3D,
                                                  GwyDataField *dfield,
                                                  gint shape);
static void     gwy_3d_draw_axes                 (Gwy3DView *gwy3dview);
static void     gwy_3d_draw_light_position       (Gwy3DView *gwy3dview);
static void     gwy_3d_set_projection            (Gwy3DView *gwy3dview);
static void     gwy_3d_view_update_labels        (Gwy3DView *gwy3dview);
static void     gwy_3d_view_label_changed        (Gwy3DView *gwy3dview);
static void     gwy_3d_view_timeout_start        (Gwy3DView *gwy3dview,
                                                  gboolean invalidate_now);
static gboolean gwy_3d_view_timeout_func         (gpointer user_data);
static void     gwy_3d_pango_ft2_render_layout   (PangoLayout *layout);
static void     gwy_3d_print_text                (Gwy3DView *gwy3dview,
                                                  Gwy3DViewLabel id,
                                                  GLfloat raster_x,
                                                  GLfloat raster_y,
                                                  GLfloat raster_z,
                                                  guint size,
                                                  gint vjustify,
                                                  gint hjustify);
static void     gwy_3d_view_class_make_list_pool (Gwy3DListPool *pool);
static void     gwy_3d_view_assign_lists         (Gwy3DView *gwy3dview);
static void     gwy_3d_view_release_lists        (Gwy3DView *gwy3dview);

/* Must match Gwy3DViewLabel */
static const struct {
    const gchar *key;
    const gchar *default_text;
}
labels[GWY_3D_VIEW_NLABELS] = {
    { "x",   "x: $x", },
    { "y",   "y: $y", },
    { "min", "$min",  },
    { "max", "$max",  },
};
#endif /* HAVE_GTKGLEXT */

G_DEFINE_TYPE(Gwy3DView, gwy_3d_view, GTK_TYPE_WIDGET)

#ifdef HAVE_GTKGLEXT
static void
gwy_3d_view_class_init(Gwy3DViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gwy_debug(" ");

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    gobject_class->finalize = gwy_3d_view_finalize;
    gobject_class->set_property = gwy_3d_view_set_property;
    gobject_class->get_property = gwy_3d_view_get_property;

    object_class->destroy = gwy_3d_view_destroy;

    widget_class->realize = gwy_3d_view_realize;
    widget_class->unrealize = gwy_3d_view_unrealize;
    widget_class->expose_event = gwy_3d_view_expose;
    widget_class->configure_event = gwy_3d_view_configure;
    widget_class->size_request = gwy_3d_view_size_request;
    widget_class->size_allocate = gwy_3d_view_size_allocate;
    widget_class->button_press_event = gwy_3d_view_button_press;
    widget_class->motion_notify_event = gwy_3d_view_motion_notify;

    klass->list_pool = g_new0(Gwy3DListPool, 1);

    /**
     * Gwy3DView:movement-type:
     *
     * The :movement-type property represents type of action on user pointer
     * drag.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_MOVEMENT,
         g_param_spec_enum("movement-type",
                           "Movement type",
                           "What quantity is changed when uses moves pointer",
                           GWY_TYPE_3D_MOVEMENT, GWY_3D_MOVEMENT_NONE,
                           G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_REDUCED_SIZE,
         g_param_spec_uint("reduced-size",
                           "Reduced size",
                           "The size of downsampled data in quick view",
                           2, G_MAXINT, 96,
                           G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_DATA_KEY,
         g_param_spec_string("data-key",
                             "Data key",
                             "Key identifying data field in container",
                             NULL, G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_SETUP_PREFIX,
         g_param_spec_string("setup-prefix",
                             "Setup prefix",
                             "Key prefix identifying view settings and labels "
                             "in container",
                             NULL, G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_GRADIENT_KEY,
         g_param_spec_string("gradient-key",
                             "Gradient key",
                             "Key identifying color gradient in container",
                             NULL, G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_MATERIAL_KEY,
         g_param_spec_string("material-key",
                             "Material key",
                             "Key identifying GL material in container",
                             NULL, G_PARAM_READWRITE));
}

static void
gwy_3d_view_init(Gwy3DView *gwy3dview)
{
    gwy3dview->view_scale_max = 3.0;
    gwy3dview->view_scale_min = 0.5;
    gwy3dview->movement       = GWY_3D_MOVEMENT_NONE;
    gwy3dview->reduced_size   = 96;

    gwy3dview->variables = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 NULL, g_free);

    gwy3dview->labels = g_new0(Gwy3DLabel*, GWY_3D_VIEW_NLABELS);
    gwy3dview->label_ids = g_new0(gulong, GWY_3D_VIEW_NLABELS);
}

static void
gwy_3d_view_destroy(GtkObject *object)
{
    Gwy3DView *gwy3dview;

    gwy3dview = GWY_3D_VIEW(object);

    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->data_item_id);
    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->gradient_item_id);
    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->material_item_id);

    gwy_3d_view_setup_disconnect(gwy3dview);
    gwy_3d_view_data_field_disconnect(gwy3dview);
    gwy_3d_view_gradient_disconnect(gwy3dview);
    gwy_3d_view_material_disconnect(gwy3dview);

    if (gwy3dview->shape_list_base > 0)
        gwy_3d_view_release_lists(gwy3dview);

    gwy_object_unref(gwy3dview->data_field);
    gwy_object_unref(gwy3dview->downsampled);
    gwy_object_unref(gwy3dview->data);

    GTK_OBJECT_CLASS(gwy_3d_view_parent_class)->destroy(object);
}

static void
gwy_3d_view_finalize(GObject *object)
{
    Gwy3DView *gwy3dview;

    gwy3dview = GWY_3D_VIEW(object);

    g_free(gwy3dview->labels);
    g_free(gwy3dview->label_ids);

    g_hash_table_destroy(gwy3dview->variables);

    G_OBJECT_CLASS(gwy_3d_view_parent_class)->finalize(object);
}

static void
gwy_3d_view_set_property(GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
    Gwy3DView *view = GWY_3D_VIEW(object);

    switch (prop_id) {
        case PROP_MOVEMENT:
        gwy_3d_view_set_movement_type(view, g_value_get_enum(value));
        break;

        case PROP_REDUCED_SIZE:
        gwy_3d_view_set_reduced_size(view, g_value_get_uint(value));
        break;

        case PROP_DATA_KEY:
        gwy_3d_view_set_data_key(view, g_value_get_string(value));
        break;

        case PROP_SETUP_PREFIX:
        gwy_3d_view_set_setup_prefix(view, g_value_get_string(value));
        break;

        case PROP_GRADIENT_KEY:
        gwy_3d_view_set_gradient_key(view, g_value_get_string(value));
        break;

        case PROP_MATERIAL_KEY:
        gwy_3d_view_set_material_key(view, g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_3d_view_get_property(GObject*object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
    Gwy3DView *view = GWY_3D_VIEW(object);

    switch (prop_id) {
        case PROP_MOVEMENT:
        g_value_set_enum(value, view->movement);
        break;

        case PROP_REDUCED_SIZE:
        g_value_set_uint(value, view->reduced_size);
        break;

        case PROP_DATA_KEY:
        g_value_set_static_string(value, g_quark_to_string(view->data_key));
        break;

        case PROP_SETUP_PREFIX:
        g_value_set_static_string(value, g_quark_to_string(view->setup_key));
        break;

        case PROP_GRADIENT_KEY:
        g_value_set_static_string(value, g_quark_to_string(view->gradient_key));
        break;

        case PROP_MATERIAL_KEY:
        g_value_set_static_string(value, g_quark_to_string(view->material_key));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_3d_view_unrealize(GtkWidget *widget)
{
    Gwy3DView *gwy3dview;

    gwy3dview = GWY_3D_VIEW(widget);

    gwy_3d_view_release_lists(gwy3dview);
    g_object_unref(gwy3dview->ft2_context);
    g_object_unref(gwy3dview->ft2_font_map);
    gwy_object_unref(gwy3dview->downsampled);

    if (GTK_WIDGET_CLASS(gwy_3d_view_parent_class)->unrealize)
        GTK_WIDGET_CLASS(gwy_3d_view_parent_class)->unrealize(widget);
}

/**
 * gwy_3d_view_new:
 * @data: A #GwyContainer containing the data to display.
 *
 * Creates a new threedimensional OpenGL display of @data.
 *
 * The widget is initialized from container @data.
 *
 * Returns: The new OpenGL 3D widget as a #GtkWidget.
 **/
GtkWidget*
gwy_3d_view_new(GwyContainer *data)
{
    GdkGLConfig *glconfig;
    GtkWidget *widget;
    Gwy3DView *gwy3dview;

    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);
    glconfig = gwy_widgets_get_gl_config();
    g_return_val_if_fail(glconfig, NULL);

    g_object_ref(data);

    gwy3dview = (Gwy3DView*)g_object_new(GWY_TYPE_3D_VIEW, NULL);
    widget = GTK_WIDGET(gwy3dview);

    gwy3dview->data = data;

    if (!gtk_widget_set_gl_capability(GTK_WIDGET(gwy3dview),
                                      glconfig,
                                      NULL,
                                      TRUE,
                                      GDK_GL_RGBA_TYPE))
        g_critical("Cannot set GL capability on widget");

    return widget;
}

static GwySIValueFormat*
gwy_3d_view_update_label(Gwy3DView *gwy3dview,
                         const gchar *key,
                         gdouble value,
                         gdouble maximum,
                         gdouble step,
                         gboolean is_z,
                         GwySIValueFormat *vf)
{
    GwySIUnit *unit;
    gchar buffer[80], *s;

    if (is_z)
        unit = gwy_data_field_get_si_unit_z(gwy3dview->data_field);
    else
        unit = gwy_data_field_get_si_unit_xy(gwy3dview->data_field);

    vf = gwy_si_unit_get_format_with_resolution(unit,
                                                GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                maximum, step, vf);
    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
               vf->precision, value/vf->magnitude, vf->units);
    s = g_hash_table_lookup(gwy3dview->variables, key);
    if (!s || !gwy_strequal(s, buffer))
        g_hash_table_insert(gwy3dview->variables, (gpointer)key,
                            g_strdup(buffer));

    return vf;
}

static void
gwy_3d_view_update_labels(Gwy3DView *gwy3dview)
{
    GwySIValueFormat *format;
    gdouble xreal, yreal, data_min, data_max, range, maximum;

    if (!gwy3dview->data_field)
        return;

    xreal = gwy_data_field_get_xreal(gwy3dview->data_field);
    yreal = gwy_data_field_get_yreal(gwy3dview->data_field);
    gwy_data_field_get_min_max(gwy3dview->data_field, &data_min, &data_max);
    range = fabs(data_max - data_min);
    maximum = MAX(fabs(data_min), fabs(data_max));

    format = NULL;
    format = gwy_3d_view_update_label(gwy3dview, "x",
                                      xreal, xreal, xreal/12.0,
                                      FALSE, format);
    format = gwy_3d_view_update_label(gwy3dview, "y",
                                      yreal, yreal, yreal/12.0,
                                      FALSE, format);
    format = gwy_3d_view_update_label(gwy3dview, "max",
                                      data_max, maximum, range/12.0,
                                      FALSE, format);
    format = gwy_3d_view_update_label(gwy3dview, "min",
                                      data_min, maximum, range/12.0,
                                      FALSE, format);
    gwy_si_unit_value_format_free(format);
}

static void
gwy_3d_view_container_connect(Gwy3DView *gwy3dview,
                              const gchar *data_key_string,
                              gulong *id,
                              GCallback callback)
{
    gchar *detailed_signal;

    if (!data_key_string || !gwy3dview->data) {
        gwy_debug("%p zeroing id for <%s>", gwy3dview, data_key_string);
        *id = 0;
        return;
    }
    gwy_debug("%p connecting to <%s>", gwy3dview, data_key_string);
    detailed_signal = g_newa(gchar, sizeof("item-changed::")
                                    + strlen(data_key_string));
    g_stpcpy(g_stpcpy(detailed_signal, "item-changed::"), data_key_string);
    *id = connect_swapped_after(gwy3dview->data, detailed_signal,
                                callback, gwy3dview);
}

void
gwy_3d_view_set_data_key(Gwy3DView *gwy3dview,
                         const gchar *key)
{
    GQuark quark;

    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    gwy_debug("%p <%s>", gwy3dview, key);

    quark = key ? g_quark_from_string(key) : 0;
    if (gwy3dview->data_key == quark)
        return;

    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->data_item_id);
    gwy_3d_view_data_field_disconnect(gwy3dview);
    gwy3dview->data_key = quark;
    gwy_3d_view_data_field_connect(gwy3dview);
    gwy_3d_view_container_connect(gwy3dview, key,
                                  &gwy3dview->data_item_id,
                                  G_CALLBACK(gwy_3d_view_data_item_changed));
    g_object_notify(G_OBJECT(gwy3dview), "data-key");
    gwy_3d_view_data_field_changed(gwy3dview);
}

const gchar*
gwy_3d_view_get_data_key(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(gwy3dview), NULL);
    return g_quark_to_string(gwy3dview->data_key);
}

static void
gwy_3d_view_data_item_changed(Gwy3DView *gwy3dview)
{
    gwy_3d_view_data_field_disconnect(gwy3dview);
    gwy_3d_view_data_field_connect(gwy3dview);
    gwy_3d_view_data_field_changed(gwy3dview);
}

static void
gwy_3d_view_data_field_connect(Gwy3DView *gwy3dview)
{
    g_return_if_fail(!gwy3dview->data_field);
    if (!gwy3dview->data_key)
        return;

    if (!gwy_container_gis_object(gwy3dview->data, gwy3dview->data_key,
                                  &gwy3dview->data_field))
        return;

    g_object_ref(gwy3dview->data_field);
    gwy3dview->data_id
        = g_signal_connect_swapped(gwy3dview->data_field,
                                   "data-changed",
                                   G_CALLBACK(gwy_3d_view_data_field_changed),
                                   gwy3dview);
}

static void
gwy_3d_view_data_field_disconnect(Gwy3DView *gwy3dview)
{
    gwy_signal_handler_disconnect(gwy3dview->data_field, gwy3dview->data_id);
    gwy_object_unref(gwy3dview->data_field);
}

static void
gwy_3d_view_data_field_changed(Gwy3DView *gwy3dview)
{
    gwy3dview->changed |= GWY_3D_DATA_FIELD;
    gwy_3d_view_update_labels(gwy3dview);
    gwy_3d_view_update_lists(gwy3dview);
}

const gchar*
gwy_3d_view_get_setup_prefix(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return g_quark_to_string(gwy3dview->gradient_key);
}

void
gwy_3d_view_set_setup_prefix(Gwy3DView *gwy3dview,
                             const gchar *key)
{
    GQuark quark;

    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    gwy_debug("%p <%s>", gwy3dview, key);

    quark = key ? g_quark_from_string(key) : 0;
    if (gwy3dview->setup_key == quark)
        return;

    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->setup_item_id);
    gwy_3d_view_setup_disconnect(gwy3dview);
    gwy3dview->setup_key = quark;
    gwy_3d_view_ensure_setup(gwy3dview);
    gwy_3d_view_setup_connect(gwy3dview);
    gwy_3d_view_container_connect(gwy3dview, key,
                                  &gwy3dview->setup_item_id,
                                  G_CALLBACK(gwy_3d_view_setup_item_changed));

    g_object_notify(G_OBJECT(gwy3dview), "setup-prefix");
    /* TODO: must not call with NULL or handle it as `all' */
    gwy_3d_view_setup_changed(gwy3dview, NULL);
}

static void
gwy_3d_view_ensure_setup(Gwy3DView *gwy3dview)
{
    guint i, len;
    GString *key;
    Gwy3DSetup *setup;
    Gwy3DLabel *label;

    if (!gwy3dview->setup_key)
        return;

    key = g_string_new(g_quark_to_string(gwy3dview->setup_key));
    g_string_append_c(key, GWY_CONTAINER_PATHSEP);
    len = key->len;

    g_string_append(key, "setup");
    if (!gwy_container_gis_object_by_name(gwy3dview->data, key->str,
                                          &setup)) {
        setup = gwy_3d_setup_new();
        gwy_container_set_object_by_name(gwy3dview->data, key->str, setup);
        g_object_unref(setup);
    }

    for (i = 0; i < GWY_3D_VIEW_NLABELS; i++) {
        g_string_truncate(key, len);
        g_string_append(key, labels[i].key);
        if (gwy_container_gis_object_by_name(gwy3dview->data, key->str, &label))
            continue;

        label = gwy_3d_label_new(labels[i].default_text);
        gwy_container_set_object_by_name(gwy3dview->data, key->str, label);
        g_object_unref(label);
    }
    g_string_free(key, TRUE);
}

static void
gwy_3d_view_setup_item_changed(Gwy3DView *gwy3dview)
{
    gwy_3d_view_setup_disconnect(gwy3dview);
    gwy_3d_view_setup_connect(gwy3dview);
    gwy_3d_view_setup_changed(gwy3dview, NULL);
}

/* Here we assert setup and labels exist as they were instantiated by
 * gwy_3d_view_ensure_setup() (if not present). */
static void
gwy_3d_view_setup_connect(Gwy3DView *gwy3dview)
{
    guint i, len;
    GString *key;

    g_return_if_fail(!gwy3dview->setup);
    if (!gwy3dview->setup_key)
        return;

    key = g_string_new(g_quark_to_string(gwy3dview->setup_key));
    g_string_append_c(key, GWY_CONTAINER_PATHSEP);
    len = key->len;

    g_string_append(key, "setup");
    gwy_container_gis_object_by_name(gwy3dview->data, key->str,
                                     &gwy3dview->setup);
    g_assert(gwy3dview->setup);
    g_object_ref(gwy3dview->setup);
    gwy3dview->setup_id
        = g_signal_connect_swapped(gwy3dview->setup, "notify",
                                   G_CALLBACK(gwy_3d_view_setup_changed),
                                   gwy3dview);

    for (i = 0; i < GWY_3D_VIEW_NLABELS; i++) {
        if (gwy3dview->labels[i]) {
            g_critical("Label %u already set!", i);
            continue;
        }
        g_string_truncate(key, len);
        g_string_append(key, labels[i].key);
        gwy_container_gis_object_by_name(gwy3dview->data, key->str,
                                         &gwy3dview->labels[i]);
        g_assert(gwy3dview->labels[i]);
        g_object_ref(gwy3dview->labels[i]);
        gwy3dview->label_ids[i]
            = g_signal_connect_swapped(gwy3dview->labels[i], "notify",
                                       G_CALLBACK(gwy_3d_view_label_changed),
                                       gwy3dview);
    }
    g_string_free(key, TRUE);
}

static void
gwy_3d_view_setup_disconnect(Gwy3DView *gwy3dview)
{
    guint i;

    gwy_signal_handler_disconnect(gwy3dview->setup, gwy3dview->setup_id);
    gwy_object_unref(gwy3dview->setup);

    for (i = 0; i < GWY_3D_VIEW_NLABELS; i++) {
        gwy_signal_handler_disconnect(gwy3dview->labels[i],
                                      gwy3dview->label_ids[i]);
        gwy_object_unref(gwy3dview->labels[i]);
    }
}

static void
gwy_3d_view_setup_changed(Gwy3DView *gwy3dview,
                          GParamSpec *pspec)
{
    gwy_debug("%p <%s>", gwy3dview, pspec ? pspec->name : "NULL");
    /* TODO: must decide what needs redraw, if anything */
    if (pspec) {
        if (gwy3dview->setup->visualization == GWY_3D_VISUALIZATION_GRADIENT
            && (gwy_strequal(pspec->name, "light-theta")
                || gwy_strequal(pspec->name, "light-phi")))
            return;
        if (!gwy3dview->setup->axes_visible
            && gwy_strequal(pspec->name, "labels-visible"))
            return;
    }

    gwy_3d_view_timeout_start(gwy3dview, TRUE);
}

/**
 * gwy_3d_view_get_gradient_key:
 * @gwy3dview: A 3D data view widget.
 *
 * Gets key identifying color gradient.
 *
 * Returns: The string key, or %NULL if it isn't set.
 **/
const gchar*
gwy_3d_view_get_gradient_key(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return g_quark_to_string(gwy3dview->gradient_key);
}

/**
 * gwy_3d_view_set_gradient_key:
 * @gwy3dview: A 3D data view widget.
 * @key: Container string key identifying the color gradient to use for
 *       gradient visualization mode.
 *
 * Sets the color gradient to use to visualize data in a 3D view.
 **/
void
gwy_3d_view_set_gradient_key(Gwy3DView *gwy3dview,
                             const gchar *key)
{
    GQuark quark;

    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    gwy_debug("%p <%s>", gwy3dview, key);

    quark = key ? g_quark_from_string(key) : 0;
    if (gwy3dview->gradient_key == quark)
        return;

    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->gradient_item_id);
    gwy_3d_view_gradient_disconnect(gwy3dview);
    gwy3dview->gradient_key = quark;
    gwy_3d_view_gradient_connect(gwy3dview);
    gwy_3d_view_container_connect
                               (gwy3dview, key,
                                &gwy3dview->gradient_item_id,
                                G_CALLBACK(gwy_3d_view_gradient_item_changed));
    g_object_notify(G_OBJECT(gwy3dview), "gradient-key");
    gwy_3d_view_gradient_changed(gwy3dview);
}

static void
gwy_3d_view_gradient_item_changed(Gwy3DView *gwy3dview)
{
    gwy_3d_view_gradient_disconnect(gwy3dview);
    gwy_3d_view_gradient_connect(gwy3dview);
    gwy_3d_view_gradient_changed(gwy3dview);
}

static void
gwy_3d_view_gradient_connect(Gwy3DView *gwy3dview)
{
    const guchar *s = NULL;

    g_return_if_fail(!gwy3dview->gradient);
    if (gwy3dview->gradient_key)
        gwy_container_gis_string(gwy3dview->data, gwy3dview->gradient_key, &s);
    gwy3dview->gradient = gwy_gradients_get_gradient(s);
    gwy_resource_use(GWY_RESOURCE(gwy3dview->gradient));
    gwy3dview->gradient_id
        = g_signal_connect_swapped(gwy3dview->gradient, "data-changed",
                                   G_CALLBACK(gwy_3d_view_gradient_changed),
                                   gwy3dview);
}

static void
gwy_3d_view_gradient_disconnect(Gwy3DView *gwy3dview)
{
    if (!gwy3dview->gradient)
        return;

    gwy_signal_handler_disconnect(gwy3dview->gradient, gwy3dview->gradient_id);
    gwy_resource_release(GWY_RESOURCE(gwy3dview->gradient));
    gwy3dview->gradient = NULL;
}

static void
gwy_3d_view_gradient_changed(Gwy3DView *gwy3dview)
{
    gwy3dview->changed |= GWY_3D_GRADIENT;
    if (gwy3dview->setup->visualization == GWY_3D_VISUALIZATION_GRADIENT)
        gwy_3d_view_update_lists(gwy3dview);
}

/**
 * gwy_3d_view_get_material_key:
 * @gwy3dview: A 3D data view widget.
 *
 * Gets key identifying GL material.
 *
 * Returns: The string key, or %NULL if it isn't set.
 **/
const gchar*
gwy_3d_view_get_material_key(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return g_quark_to_string(gwy3dview->material_key);
}

/**
 * gwy_3d_view_set_material_key:
 * @gwy3dview: A 3D data view widget.
 * @key: Container string key identifying the color material to use for
 *       material visualization mode.
 *
 * Sets the GL material to use to visualize data in a 3D view.
 **/
void
gwy_3d_view_set_material_key(Gwy3DView *gwy3dview,
                            const gchar *key)
{
    GQuark quark;

    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    gwy_debug("%p <%s>", gwy3dview, key);

    quark = key ? g_quark_from_string(key) : 0;
    if (gwy3dview->material_key == quark)
        return;

    gwy_signal_handler_disconnect(gwy3dview->data, gwy3dview->material_item_id);
    gwy_3d_view_material_disconnect(gwy3dview);
    gwy3dview->material_key = quark;
    gwy_3d_view_material_connect(gwy3dview);
    gwy_3d_view_container_connect
                            (gwy3dview, key,
                             &gwy3dview->material_item_id,
                             G_CALLBACK(gwy_3d_view_material_item_changed));
    g_object_notify(G_OBJECT(gwy3dview), "material-key");
    gwy_3d_view_material_changed(gwy3dview);
}

static void
gwy_3d_view_material_item_changed(Gwy3DView *gwy3dview)
{
    gwy_3d_view_material_disconnect(gwy3dview);
    gwy_3d_view_material_connect(gwy3dview);
    gwy_3d_view_material_changed(gwy3dview);
}

static void
gwy_3d_view_material_connect(Gwy3DView *gwy3dview)
{
    const guchar *s = NULL;

    g_return_if_fail(!gwy3dview->material);
    if (gwy3dview->material_key)
        gwy_container_gis_string(gwy3dview->data, gwy3dview->material_key, &s);
    gwy3dview->material = gwy_gl_materials_get_gl_material(s);
    gwy_resource_use(GWY_RESOURCE(gwy3dview->material));
    gwy3dview->material_id
        = g_signal_connect_swapped(gwy3dview->material, "data-changed",
                                   G_CALLBACK(gwy_3d_view_material_changed),
                                   gwy3dview);
}

static void
gwy_3d_view_material_disconnect(Gwy3DView *gwy3dview)
{
    if (!gwy3dview->material)
        return;

    gwy_signal_handler_disconnect(gwy3dview->material, gwy3dview->material_id);
    gwy_resource_release(GWY_RESOURCE(gwy3dview->material));
    gwy3dview->material = NULL;
}

static void
gwy_3d_view_material_changed(Gwy3DView *gwy3dview)
{
    gwy3dview->changed |= GWY_3D_MATERIAL;
    if (gwy3dview->setup->visualization == GWY_3D_VISUALIZATION_LIGHTING)
        gwy_3d_view_update_lists(gwy3dview);
}

static void
gwy_3d_view_update_lists(Gwy3DView *gwy3dview)
{
    if (!GTK_WIDGET_REALIZED(gwy3dview))
        return;

    gwy_3d_view_downsample_data(gwy3dview);

    gwy_3d_make_list(gwy3dview, gwy3dview->downsampled, GWY_3D_SHAPE_REDUCED);
    gwy_3d_make_list(gwy3dview, gwy3dview->data_field, GWY_3D_SHAPE_FULL);
    gwy_3d_view_timeout_start(gwy3dview, TRUE);
}

/**
 * gwy_3d_view_get_movement_type:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns a movement type describing actual type of response on
 * the mouse motion event.
 *
 * Returns: actual type of response on the mouse motion event
 **/
Gwy3DMovement
gwy_3d_view_get_movement_type(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), GWY_3D_MOVEMENT_NONE);
    return gwy3dview->movement;
}

/**
 * gwy_3d_view_set_movement_type:
 * @gwy3dview: A 3D data view widget.
 * @movement: A new type of response on the mouse motion event.
 *
 * Sets the type of widget response on the mouse motion event.
 **/
void
gwy_3d_view_set_movement_type(Gwy3DView *gwy3dview,
                              Gwy3DMovement movement)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    g_return_if_fail(movement <= GWY_3D_MOVEMENT_LIGHT);

    gwy3dview->movement = movement;
}

/**
 * gwy_3d_view_set_reduced_size:
 * @gwy3dview: A 3D data view widget.
 * @reduced_size: New reduced data size.
 *
 * Sets the reduced data size of a 3D view.
 *
 * Data larger than reduced size are show downsampled during transforms and
 * other changes to speed up the rendering.  Final, full-size rendering is
 * then performed after a timeout.
 *
 * In case of the original data are not square, the @reduced_size is the
 * greater size of the downsampled data, the other dimension is proportional
 * to the original size.
 *
 * Changes in reduced size do not cause immediate redraw when an operation
 * is pending and the view is shown in reduced size.  It only affects future
 * downsampling.
 **/
void
gwy_3d_view_set_reduced_size(Gwy3DView *gwy3dview,
                             guint reduced_size)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    g_return_if_fail(reduced_size >= 2);

    if (gwy3dview->reduced_size == reduced_size)
        return;

    gwy3dview->reduced_size = reduced_size;
    g_object_notify(G_OBJECT(gwy3dview), "reduced-size");
    /* FIXME: should we do anything else? */
}

/**
 * gwy_3d_view_get_reduced_size:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns the reduced data size of a 3D view.
 *
 * See gwy_3d_view_set_reduced_size() for details.
 *
 * Returns: The reduced data size.
 **/
guint
gwy_3d_view_get_reduced_size(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), 0);
    return gwy3dview->reduced_size;
}

/**
 * gwy_3d_view_downsample_data:
 * @gwy3dview: A 3D data view widget.
 *
 * Update downsampled data.
 *
 * If the full data field to display meets the reduced size condition, the
 * downsampled data field is destroyed and set to NULL.
 **/
static void
gwy_3d_view_downsample_data(Gwy3DView *gwy3dview)
{
    gint rx, ry;
    gdouble rs;

    rx = gwy_data_field_get_xres(gwy3dview->data_field);
    ry = gwy_data_field_get_yres(gwy3dview->data_field);
    rs = gwy3dview->reduced_size;

    gwy_object_unref(gwy3dview->downsampled);
    if (rx < rs && ry < rs) {
        gwy_debug("%p destroying downsampled", gwy3dview);
        return;
    }

    if (rx > ry) {
        ry = (guint)(rs*ry/rx);
        rx = rs;
    }
    else {
        rx = (guint)(rs*rx/ry);
        ry = rs;
    }

    gwy_debug("%p downsampling to %dx%d", gwy3dview, rx, ry);
    gwy3dview->downsampled
        = gwy_data_field_new_resampled(gwy3dview->data_field,
                                       rx, ry, GWY_INTERPOLATION_BILINEAR);
}

Gwy3DLabel*
gwy_3d_view_get_label(Gwy3DView *gwy3dview,
                      Gwy3DViewLabel label)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    g_return_val_if_fail(label < GWY_3D_VIEW_NLABELS, NULL);

    return gwy3dview->labels[label];
}

Gwy3DSetup*
gwy_3d_view_get_setup(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);

    return gwy3dview->setup;
}

/**
 * gwy_3d_view_get_pixbuf:
 * @gwy3dview: A 3D data view widget.
 *
 * Copies the contents of the framebuffer to the GdkPixbuf.
 *
 * The size of the pixbuf is currently indentical with the size of the widget.
 * @xres and @yres will be implemented to set the resolution of the pixbuf.
 *
 * Returns: A newly allocated GdkPixbuf with copy of the framebuffer.
 **/
GdkPixbuf*
gwy_3d_view_get_pixbuf(Gwy3DView *gwy3dview)
{
    int width, height, rowstride, n_channels, i, j;
    guchar *pixels, *a, *b, z;
    GdkPixbuf * pixbuf;

    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    g_return_val_if_fail(GTK_WIDGET_REALIZED(gwy3dview), NULL);

    width  = GTK_WIDGET(gwy3dview)->allocation.width;
    height = GTK_WIDGET(gwy3dview)->allocation.height;

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);

    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    a = pixels;
    b = pixels + (height -1)  * rowstride;
    for (i = 0; i < height/2; i ++, b = pixels + (height - 1 - i) * rowstride) {
        for (j = 0; j < rowstride; j++, a++, b++) {
             z = *a;
            *a = *b;
            *b =  z;
        }
    }

    return pixbuf;
}

/**
 * gwy_3d_view_get_data:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns the data container this 3D view displays.
 *
 * Returns: The container as a #GwyContainer.
 **/
GwyContainer*
gwy_3d_view_get_data(Gwy3DView *gwy3dview)
{
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);
    return gwy3dview->data;
}

/**
 * gwy_3d_view_get_scale_range:
 * @gwy3dview: A 3D data view widget.
 * @min_scale: Location to put minimum scale, or %NULL.
 * @max_scale: Location to put maximum scale, or %NULL.
 *
 * Obtains the minimum and maximum zoom of a 3D data view
 **/
void
gwy_3d_view_get_scale_range(Gwy3DView *gwy3dview,
                            gdouble *min_scale,
                            gdouble *max_scale)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));

    if (min_scale)
        *min_scale = gwy3dview->view_scale_min;
    if (max_scale)
        *max_scale = gwy3dview->view_scale_max;
}

/**
 * gwy_3d_view_set_scale_range:
 * @gwy3dview: A 3D data view widget.
 * @min_scale: Minimum zoom of the 3D data view, pass 0.0 to keep the current
 *             value.
 * @max_scale: Maximum zoom of the 3D data view, pass 0.0 to keep the current
 *             value.
 *
 * Sets the minimum and maximum zoom of a 3D data view.
 *
 * Recommended zoom values are 0.5 - 5.0.
 **/
void
gwy_3d_view_set_scale_range(Gwy3DView *gwy3dview,
                            gdouble min_scale,
                            gdouble max_scale)
{
    g_return_if_fail(GWY_IS_3D_VIEW(gwy3dview));
    g_return_if_fail(min_scale >= 0.0 && max_scale >= 0.0);

    if (!min_scale)
        min_scale = gwy3dview->view_scale_min;
    if (!max_scale)
        max_scale = gwy3dview->view_scale_max;

    g_return_if_fail(min_scale <= max_scale);

    gwy3dview->view_scale_min = min_scale;
    gwy3dview->view_scale_max = max_scale;
    if (gwy3dview->setup) {
        gdouble val;

        val = CLAMP(gwy3dview->setup->scale, min_scale, max_scale);
        if (val != gwy3dview->setup->scale)
            g_object_set(gwy3dview->setup, "scale", val, NULL);
    }
}

/******************************************************************************/
static void
gwy_3d_view_timeout_start(Gwy3DView *gwy3dview,
                          gboolean invalidate_now)
{
    gboolean add_timeout;

    gwy_debug("%p %d", gwy3dview, invalidate_now);

    if (gwy3dview->timeout_id) {
         g_source_remove(gwy3dview->timeout_id);
         gwy3dview->timeout_id = 0;
    }

    if (!GTK_WIDGET_DRAWABLE(gwy3dview))
        return;

    if (gwy3dview->downsampled) {
        gwy_debug("%p shape REDUCED", gwy3dview);
        gwy3dview->shape_current = GWY_3D_SHAPE_REDUCED;
        add_timeout = TRUE;
    }
    else {
        gwy_debug("%p shape FULL", gwy3dview);
        gwy3dview->shape_current = GWY_3D_SHAPE_FULL;
        add_timeout = FALSE;
    }

    if (invalidate_now)
        gtk_widget_queue_draw(GTK_WIDGET(gwy3dview));

    if (add_timeout)
        gwy3dview->timeout_id = g_timeout_add(GWY_3D_TIMEOUT_DELAY,
                                              gwy_3d_view_timeout_func,
                                              gwy3dview);
}

static gboolean
gwy_3d_view_timeout_func(gpointer user_data)
{
    Gwy3DView *gwy3dview = (Gwy3DView*)user_data;

    gwy_debug("%p shape FULL", gwy3dview);

    gwy3dview->shape_current = GWY_3D_SHAPE_FULL;
    gwy3dview->timeout_id = 0;
    if (GTK_WIDGET_DRAWABLE(gwy3dview))
        gdk_window_invalidate_rect(GTK_WIDGET(gwy3dview)->window,
                                   &GTK_WIDGET(gwy3dview)->allocation, FALSE);

    return FALSE;
}

static void
gwy_3d_view_label_changed(Gwy3DView *gwy3dview)
{
    gwy_3d_view_timeout_start(gwy3dview, TRUE);
}

static void
gwy_3d_view_realize(GtkWidget *widget)
{
    Gwy3DView *gwy3dview;
    GdkWindowAttr attributes;
    gint attributes_mask;

    gwy_debug("realizing a Gwy3DView (%ux%u)",
              widget->allocation.width, widget->allocation.height);

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    gwy3dview = GWY_3D_VIEW(widget);

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);
    attributes.event_mask = gtk_widget_get_events(widget)
                           | GDK_EXPOSURE_MASK
                           | GDK_BUTTON1_MOTION_MASK
                           | GDK_BUTTON2_MOTION_MASK
                           | GDK_BUTTON_PRESS_MASK
                           | GDK_BUTTON_RELEASE_MASK
                           | GDK_VISIBILITY_NOTIFY_MASK;

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
                                    &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, widget);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

    gwy_3d_view_downsample_data(gwy3dview);
    gwy_3d_view_send_configure(gwy3dview);
    gwy_3d_view_realize_gl(gwy3dview);

    /* Get PangoFT2 context. */
    gwy3dview->ft2_font_map = gwy_get_pango_ft2_font_map(FALSE);
    g_object_ref(gwy3dview->ft2_font_map);
    gwy3dview->ft2_context = pango_ft2_font_map_create_context
                                 (PANGO_FT2_FONT_MAP(gwy3dview->ft2_font_map));
}

static gboolean
gwy_3d_view_configure(GtkWidget *widget,
                      GdkEventConfigure *event)
{
    Gwy3DView *gwy3dview;

    GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

    GLfloat w = widget->allocation.width;
    GLfloat h = widget->allocation.height;

    if (GTK_WIDGET_CLASS(gwy_3d_view_parent_class)->configure_event)
        GTK_WIDGET_CLASS(gwy_3d_view_parent_class)->configure_event(widget,
                                                                    event);

    gwy_debug("width: %f, height: %f", w, h);
    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    gwy3dview = GWY_3D_VIEW(widget);

    /*** OpenGL BEGIN ***/
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return FALSE;

    glViewport(0, 0, w, h);
    gwy_3d_set_projection(gwy3dview);

    glMatrixMode(GL_MODELVIEW);

    gdk_gl_drawable_gl_end(gldrawable);
    /*** OpenGL END ***/

    return FALSE;
}

static void
gwy_3d_view_send_configure(Gwy3DView *gwy3dview)
{
    GtkWidget *widget;
    GdkEvent *event;

    event = gdk_event_new(GDK_CONFIGURE);

    widget = GTK_WIDGET(gwy3dview);
    event->configure.window = g_object_ref(widget->window);
    event->configure.send_event = TRUE;
    event->configure.x = widget->allocation.x;
    event->configure.y = widget->allocation.y;
    event->configure.width = widget->allocation.width;
    event->configure.height = widget->allocation.height;

    gtk_widget_event(widget, event);
    gdk_event_free(event);
}

static void
gwy_3d_view_size_request(G_GNUC_UNUSED GtkWidget *widget,
                         GtkRequisition *requisition)
{
    requisition->width = GWY_3D_VIEW_DEFAULT_SIZE_X;
    requisition->height = GWY_3D_VIEW_DEFAULT_SIZE_Y;
}

static void
gwy_3d_view_size_allocate(GtkWidget *widget,
                          GtkAllocation *allocation)
{
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED(widget)) {
        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);

        gwy_3d_view_send_configure(GWY_3D_VIEW(widget));
    }
}

/* Convert GwyRGBA to GLfloat[4] array */
static void
gwy_3d_view_rgba_dv(GLenum face,
                    GLenum pname,
                    const GwyRGBA *rgba)
{
    gfloat fparams[4];

    fparams[0] = rgba->r;
    fparams[1] = rgba->g;
    fparams[2] = rgba->b;
    fparams[3] = rgba->a;
    glMaterialfv(face, pname, fparams);
}

static gboolean
gwy_3d_view_expose(GtkWidget *widget,
                   G_GNUC_UNUSED GdkEventExpose *event)
{
    GwyGLMaterial *material;
    GdkGLContext  *glcontext;
    GdkGLDrawable *gldrawable;
    Gwy3DView *gwy3dview;

    GLfloat light_position[] = { 0.0, 0.0, 4.0, 1.0 };

    gwy_debug(" ");

    g_return_val_if_fail(GWY_IS_3D_VIEW(widget), FALSE);
    gwy3dview = GWY_3D_VIEW(widget);

    glcontext  = gtk_widget_get_gl_context(widget);
    gldrawable = gtk_widget_get_gl_drawable(widget);
    gwy_debug("GLContext: %p, GLDrawable: %p", glcontext, gldrawable);

    /*** OpenGL BEGIN ***/
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return FALSE;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();

    /* View transformation. */
    gwy_3d_set_projection(gwy3dview);
    glTranslatef(0.0, 0.0, -10.0);
    glScalef(gwy3dview->setup->scale,
             gwy3dview->setup->scale,
             gwy3dview->setup->scale);

    glRotatef(gwy3dview->setup->rotation_y*RAD_2_DEG, 1.0, 0.0, 0.0);
    glRotatef(gwy3dview->setup->rotation_x*RAD_2_DEG, 0.0,  0.0, 1.0);
    glScalef(1.0f, 1.0f, gwy3dview->setup->z_scale);

    /* Render shape */
    if (gwy3dview->setup->visualization == GWY_3D_VISUALIZATION_LIGHTING) {
        material = gwy3dview->material;

        glEnable(GL_LIGHTING);
        gwy_3d_view_rgba_dv(GL_FRONT, GL_AMBIENT,
                            gwy_gl_material_get_ambient(material));
        gwy_3d_view_rgba_dv(GL_FRONT, GL_DIFFUSE,
                            gwy_gl_material_get_diffuse(material));
        gwy_3d_view_rgba_dv(GL_FRONT, GL_SPECULAR,
                            gwy_gl_material_get_specular(material));
        glMaterialf(GL_FRONT, GL_SHININESS,
                    (GLfloat)gwy_gl_material_get_shininess(material)*128.0f);
        glPushMatrix();
        glRotatef(gwy3dview->setup->light_theta * RAD_2_DEG, 0.0f, 0.0f, 1.0f);
        glRotatef(gwy3dview->setup->light_phi * RAD_2_DEG, 0.0f, 1.0f, 0.0f);
        glLightfv(GL_LIGHT0, GL_POSITION, light_position);
        glPopMatrix();
    }
    else {
        glDisable(GL_LIGHTING);
    }


    glCallList(gwy3dview->shape_list_base + gwy3dview->shape_current);
    if (gwy3dview->setup->axes_visible)
        gwy_3d_draw_axes(gwy3dview);

    if (gwy3dview->movement == GWY_3D_MOVEMENT_LIGHT
        && gwy3dview->shape_current == GWY_3D_SHAPE_REDUCED)
        gwy_3d_draw_light_position(gwy3dview);

    /* Swap buffers */
    if (gdk_gl_drawable_is_double_buffered(gldrawable))
        gdk_gl_drawable_swap_buffers(gldrawable);
    else
        glFlush();

    gdk_gl_drawable_gl_end(gldrawable);
    /*** OpenGL END ***/

    return FALSE;
}

static gboolean
gwy_3d_view_button_press(GtkWidget *widget,
                         GdkEventButton *event)
{
    Gwy3DView *gwy3dview;

    gwy_debug(" ");

    gwy3dview = GWY_3D_VIEW(widget);

    gwy3dview->mouse_begin_x = event->x;
    gwy3dview->mouse_begin_y = event->y;

    return FALSE;
}

static gboolean
gwy_3d_view_motion_notify(GtkWidget *widget,
                          GdkEventMotion *event)
{
    Gwy3DView *gwy3dview;
    gdouble h, dx, dy, val;

    h = widget->allocation.height;
    gwy3dview = GWY_3D_VIEW(widget);
    dx = event->x - gwy3dview->mouse_begin_x;
    dy = event->y - gwy3dview->mouse_begin_y;
    gwy3dview->mouse_begin_x = event->x;
    gwy3dview->mouse_begin_y = event->y;

    gwy_debug("motion event: (%lf, %lf), shape=%d",
              event->x, event->y, gwy3dview->shape_current);

    if (event->state & GDK_BUTTON1_MASK) {
        switch (gwy3dview->movement) {
            case GWY_3D_MOVEMENT_NONE:
            break;

            case GWY_3D_MOVEMENT_ROTATION:
            g_object_set(gwy3dview->setup,
                         "rotation-x",
                         gwy3dview->setup->rotation_x + dx*DEG_2_RAD,
                         "rotation-y",
                         gwy3dview->setup->rotation_y + dy*DEG_2_RAD,
                         NULL);
            break;

            case GWY_3D_MOVEMENT_SCALE:
            val = gwy3dview->setup->scale*(1.0 + dy/h);
            val = CLAMP(val,
                        gwy3dview->view_scale_min, gwy3dview->view_scale_max);
            g_object_set(gwy3dview->setup, "scale", val, NULL);
            break;

            case GWY_3D_MOVEMENT_DEFORMATION:
            val = gwy3dview->setup->z_scale;
            val *= exp(-GWY_3D_Z_DEFORMATION*dy);
            g_object_set(gwy3dview->setup, "z-scale", val, NULL);
            break;

            case GWY_3D_MOVEMENT_LIGHT:
            g_object_set(gwy3dview->setup,
                         "light-theta",
                         gwy3dview->setup->light_theta + dx*DEG_2_RAD,
                         "light-phi",
                         gwy3dview->setup->light_phi + dx*DEG_2_RAD,
                         NULL);
            break;
        }
    }

    return FALSE;
}

static Gwy3DVector*
gwy_3d_make_normals(GwyDataField *dfield,
                    Gwy3DVector *normals)
{
   typedef struct { Gwy3DVector A, B; } RectangleNorm;
   const gdouble *data;
   gint i, j, xres, yres;
   RectangleNorm * norms;

   g_return_val_if_fail(normals, NULL);

   xres = gwy_data_field_get_xres(dfield);
   yres = gwy_data_field_get_yres(dfield);
   data = gwy_data_field_get_data_const(dfield);

   /* memory for normals of triangles
    * total count of rectangels is (xres-1)*(yres-1), each one has 2n triangles
    * 3 componernts per vector
    */
   norms = g_new(RectangleNorm, (xres - 1) * (yres - 1));
   if (norms == NULL)
       return NULL;

   /* Calculation of nornals of triangles */
   for (j = 0; j < yres-1; j++) {
      for (i = 0; i < xres-1; i++) {
         GLfloat a, b, c, n;

         a = data[(yres-1 - j)*xres + i];
         b = data[(yres-2 - j)*xres + i];
         c = data[(yres-1 - j)*xres + i+1];
         n = 1.0 / sqrt((a-c)*(a-c) + (b-a)*(b-a) + 1.0);
         norms[j*(xres-1) + i].A.x = (a-c)*n;
         norms[j*(xres-1) + i].A.y = (b-a)*n;
         norms[j*(xres-1)+ i].A.z = n;

         a = b;
         b = c;
         c = data[(yres-2 - j)*xres + i+1];
         n = 1.0 / sqrt((a-c)*(a-c) + (c-b)*(c-b) + 1.0);
         norms[j*(xres-1) + i].B.x = (a-c)*n;
         norms[j*(xres-1) + i].B.y = (c-b)*n;
         norms[j*(xres-1) + i].B.z = n;
      }
   }

   /* two of corner vertecies have only one triangle adjacent
    * (first and last vertex) */
   normals[0] = norms[0].A;
   normals[xres * yres - 1] = norms[(xres-1) * (yres-1) - 1].B;
   /*the other two corner vertecies have two triangles adjacent*/
   normals[xres - 1].x = (norms[xres - 2].A.x + norms[xres - 2].B.x)/2.0f;
   normals[xres - 1].y = (norms[xres - 2].A.y + norms[xres - 2].B.y)/2.0f;
   normals[xres - 1].z = (norms[xres - 2].A.z + norms[xres - 2].B.z)/2.0f;
   normals[xres * (yres -1)].x = (norms[(xres - 1)*(yres -2)].A.x
                                  + norms[(xres - 1)*(yres -2)].B.x)/2.0f;
   normals[xres * (yres -1)].y = (norms[xres - 2].A.y
                                  + norms[(xres - 1)*(yres -2)].B.y)/2.0f;
   normals[xres * (yres -1)].z = (norms[xres - 2].A.z
                                  + norms[(xres - 1)*(yres -2)].B.z)/2.0f;
   /*the vertexies on the edge og the matrix have three adjacent triangles*/
   for (i = 1; i < xres - 1; i ++) {
       const Gwy3DVector *a = &(norms[i-1].A);
       const Gwy3DVector *b = &(norms[i-1].B);
       const Gwy3DVector *c = &(norms[i].A);

       normals[i].x = (a->x + b->x + c->x)/3.0;
       normals[i].y = (a->y + b->y + c->y)/3.0;
       normals[i].z = (a->z + b->z + c->z)/3.0;
   }

   for (i = 1; i < xres - 1; i ++) {
       const Gwy3DVector *a = &(norms[(xres - 1)*(yres - 2) + i-1].B);
       const Gwy3DVector *b = &(norms[(xres - 1)*(yres - 2) + i-1].A);
       const Gwy3DVector *c = &(norms[(xres - 1)*(yres - 2) + i].B);

       normals[xres * (yres - 1) + i].x = (a->x + b->x + c->x)/3.0;
       normals[xres * (yres - 1) + i].y = (a->y + b->y + c->y)/3.0;
       normals[xres * (yres - 1) + i].z = (a->z + b->z + c->z)/3.0;
   }

   for (i = 1; i < yres - 1; i ++) {
       const Gwy3DVector *a = &(norms[(i-1) * (xres-1)].A);
       const Gwy3DVector *b = &(norms[(i-1) * (xres-1)].B);
       const Gwy3DVector *c = &(norms[i *     (xres-1)].A);

       normals[i * xres].x = (a->x + b->x + c->x)/3.0;
       normals[i * xres].y = (a->y + b->y + c->y)/3.0;
       normals[i * xres].z = (a->z + b->z + c->z)/3.0;
   }

   for (i = 1; i < yres - 1; i ++) {
       const Gwy3DVector *a = &(norms[i     * (xres-1) - 1].B);
       const Gwy3DVector *b = &(norms[(i+1) * (xres-1) - 1].A);
       const Gwy3DVector *c = &(norms[(i+1) * (xres-1) - 1].B);

       normals[i * xres - 1].x = (a->x + b->x + c->x)/3.0;
       normals[i * xres - 1].y = (a->y + b->y + c->y)/3.0;
       normals[i * xres - 1].z = (a->z + b->z + c->z)/3.0;
   }

   /* The vertecies inside of matrix have six adjacent triangles*/
   for (j = 1; j < yres - 1; j++) {
       for (i = 1; i < xres - 1; i++) {
           Gwy3DVector *adj_tri[6];
           int k;

           adj_tri[0] = &(norms[(j-1)*(xres-1) + i - 1].B);
           adj_tri[1] = &(norms[(j-1)*(xres-1) + i    ].A);
           adj_tri[2] = &(norms[(j-1)*(xres-1) + i    ].B);
           adj_tri[3] = &(norms[j    *(xres-1) + i    ].A);
           adj_tri[4] = &(norms[j    *(xres-1) + i - 1].A);
           adj_tri[5] = &(norms[j    *(xres-1) + i - 1].B);
           normals[j*xres + i].x = 0.0f;
           normals[j*xres + i].y = 0.0f;
           normals[j*xres + i].z = 0.0f;
           for (k = 0; k < 6; k++) {
               normals[j*xres + i].x += adj_tri[k]->x;
               normals[j*xres + i].y += adj_tri[k]->y;
               normals[j*xres + i].z += adj_tri[k]->z;
           }
           normals[j*xres + i].x /= 6.0f;
           normals[j*xres + i].y /= 6.0f;
           normals[j*xres + i].z /= 6.0f;
       }
   }

   g_free(norms);

   return normals;
}

static void
gwy_3d_make_list(Gwy3DView *gwy3dview,
                 GwyDataField *dfield,
                 gint shape)
{
    gint i, j, xres, yres, res;
    gdouble data_min, data_max;
    GLdouble zdifr;
    Gwy3DVector *normals;
    const gdouble *data;
    GwyGradient *grad;
    GwyRGBA color;

    if (!dfield && shape == GWY_3D_SHAPE_REDUCED)
        return;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gwy_data_field_get_min_max(gwy3dview->data_field, &data_min, &data_max);
    data = gwy_data_field_get_data_const(dfield);
    res  = xres > yres ? xres : yres;
    grad = gwy3dview->gradient;
    if (!grad)
        grad = gwy_gradients_get_gradient(NULL);

    glNewList(gwy3dview->shape_list_base + shape, GL_COMPILE);
    glPushMatrix();
    glTranslatef(-(xres/(double)res), -(yres/(double)res),
                 GWY_3D_Z_DISPLACEMENT);
    glScalef(2.0/res, 2.0/res, GWY_3D_Z_TRANSFORMATION/(data_max - data_min));
    glTranslatef(0.0, 0.0, -data_min);
    zdifr = 1.0/(data_max - data_min);
    normals = g_new(Gwy3DVector, xres * yres);
    if (!gwy_3d_make_normals(dfield, normals)) {
        /*TODO solve not enough memory problem*/
    }

    /* FIXME: This should be avoided in lighting visualization mode, create
     * it only upon a switch to gradient mode. */
    for (j = 0; j < yres-1; j++) {
        glBegin(GL_TRIANGLE_STRIP);
        for (i = 0; i < xres-1; i++) {
            gdouble a, b;

            a = data[(yres-1 - j)*xres + i];
            b = data[(yres-2 - j)*xres + i];
            glNormal3d(normals[j*xres+i].x,
                       normals[j*xres+i].y,
                       normals[j*xres+i].z);
            gwy_gradient_get_color(grad, (a - data_min)*zdifr, &color);
            glColor3d(color.r , color.g, color.b);
            glVertex3d((double)i, (double)j, a);
            glNormal3d(normals[(j+1)*xres+i].x,
                       normals[(j+1)*xres+i].y,
                       normals[(j+1)*xres+i].z);
            gwy_gradient_get_color(grad, (b - data_min)*zdifr, &color);
            glColor3d(color.r , color.g, color.b);
            glVertex3d((double)i, (double)(j+1), b);
        }
        glEnd();
    }
    g_free(normals);
    glPopMatrix();
    glEndList();

}

static void
gwy_3d_draw_axes(Gwy3DView *widget)
{
    GLfloat rx, Ax, Ay, Bx, By, Cx, Cy;
    gdouble data_min, data_max;
    gint xres, yres, res;
    gboolean yfirst;
    GwyGLMaterial *mat_none;

    gwy_debug(" ");

    xres = gwy_data_field_get_xres(widget->data_field);
    yres = gwy_data_field_get_yres(widget->data_field);
    gwy_data_field_get_min_max(widget->data_field, &data_min, &data_max);
    res  = xres > yres ? xres : yres;

    Ax = Ay = Bx = By = Cx = Cy = 0.0f;
    yfirst = TRUE;
    rx = fmod(widget->setup->rotation_x*RAD_2_DEG, 360.0);
    if (rx < 0.0)
        rx += 360.0;

    mat_none = gwy_gl_materials_get_gl_material(GWY_GL_MATERIAL_NONE);

    glPushMatrix();
    glTranslatef(-(xres/(double)res), -(yres/(double)res),
                 GWY_3D_Z_DISPLACEMENT);
    glScalef(2.0/res, 2.0/res, GWY_3D_Z_TRANSFORMATION/(data_max - data_min));
    gwy_3d_view_rgba_dv(GL_FRONT, GL_AMBIENT,
                        gwy_gl_material_get_ambient(mat_none));
    gwy_3d_view_rgba_dv(GL_FRONT, GL_DIFFUSE,
                        gwy_gl_material_get_diffuse(mat_none));
    gwy_3d_view_rgba_dv(GL_FRONT, GL_SPECULAR,
                        gwy_gl_material_get_specular(mat_none));
    glMaterialf(GL_FRONT, GL_SHININESS,
                (GLfloat)gwy_gl_material_get_shininess(mat_none)*128.0f);

    if (rx >= 0.0 && rx <= 90.0) {
        Ay = yres;
        Cx = xres;
        yfirst = TRUE;
    } else if (rx > 90.0 && rx <= 180.0) {
        Ax = xres;
        Ay = yres;
        By = yres;
        yfirst = FALSE;
    } else if (rx > 180.0 && rx <= 270.0) {
        Ax = xres;
        Bx = xres;
        By = yres;
        Cy = yres;
       yfirst = TRUE;
    } else if (rx >= 270.0 && rx <= 360.0) {
        Bx = xres;
        Cx = xres;
        Cy = yres;
        yfirst = FALSE;
    }
    glBegin(GL_LINE_STRIP);
        glColor3f(0.0, 0.0, 0.0);
        glVertex3f(Ax, Ay, 0.0f);
        glVertex3f(Bx, By, 0.0f);
        glVertex3f(Cx, Cy, 0.0f);
        glVertex3f(Cx, Cy, data_max - data_min);
    glEnd();
    glBegin(GL_LINES);
        glVertex3f(Ax, Ay, 0.0f);
        glVertex3f(Ax - (Cx-Bx)*0.02, Ay - (Cy-By)*0.02, 0.0f );
        glVertex3f((Ax+Bx)/2, (Ay+By)/2, 0.0f);
        glVertex3f((Ax+Bx)/2 - (Cx-Bx)*0.02,
                   (Ay+By)/2 - (Cy-By)*0.02, 0.0f );
        glVertex3f(Bx , By, 0.0f);
        glVertex3f(Bx - (Cx-Bx)*0.02, By - (Cy-By)*0.02, 0.0f );
        glVertex3f(Bx, By, 0.0f);
        glVertex3f(Bx - (Ax-Bx)*0.02, By - (Ay-By)*0.02, 0.0f );
        glVertex3f((Cx+Bx)/2, (Cy+By)/2, 0.0f);
        glVertex3f((Cx+Bx)/2 - (Ax-Bx)*0.02,
                   (Cy+By)/2 - (Ay-By)*0.02, 0.0f );
        glVertex3f(Cx , Cy, 0.0f);
        glVertex3f(Cx - (Ax-Bx)*0.02, Cy - (Ay-By)*0.02, 0.0f );
    glEnd();

    glBegin(GL_LINES);
        glVertex3f(Cx, Cy, data_max - data_min);
        glVertex3f(Cx - (Ax-Bx)*0.02, Cy - (Ay-By)*0.02,
                 data_max - data_min);
        glVertex3f(Cx, Cy, (data_max - data_min)/2);
        glVertex3f(Cx - (Ax-Bx)*0.02, Cy - (Ay-By)*0.02,
                   (data_max - data_min)/2);
    glEnd();

    /*
    TODO: create bitmaps with labels in the beginning (possibly in init_gl)
          into display lists and draw here
    */
    if (widget->setup->labels_visible) {
        guint view_size;
        gint size;

        view_size = MIN(GTK_WIDGET(widget)->allocation.width,
                        GTK_WIDGET(widget)->allocation.height);
        size = (gint)(sqrt(view_size)*0.8);

        glColor3f(0.0, 0.0, 0.0);
        gwy_3d_print_text(widget, yfirst ? GWY_3D_VIEW_LABEL_Y
                                         : GWY_3D_VIEW_LABEL_X,
                          (Ax+2*Bx)/3 - (Cx-Bx)*0.1,
                          (Ay+2*By)/3 - (Cy-By)*0.1, -0.0f,
                          size, 1, 1);

        gwy_3d_print_text(widget, yfirst ? GWY_3D_VIEW_LABEL_X
                                         : GWY_3D_VIEW_LABEL_Y,
                          (2*Bx+Cx)/3 - (Ax-Bx)*0.1,
                          (2*By+Cy)/3 - (Ay-By)*0.1, -0.0f,
                          size, 1, -1);

        gwy_3d_print_text(widget, GWY_3D_VIEW_LABEL_MAX,
                          Cx - (Ax-Bx)*0.1, Cy - (Ay-By)*0.1,
                          (data_max - data_min),
                          size, 0, -1);

        gwy_3d_print_text(widget, GWY_3D_VIEW_LABEL_MIN,
                          Cx - (Ax-Bx)*0.1, Cy - (Ay-By)*0.1, 0.0f,
                          size, 0, -1);
    }

   glPopMatrix();
}

static void
gwy_3d_draw_light_position(Gwy3DView *widget)
{
    GwyGLMaterial *mat_none;
    GLfloat plane_z;
    gdouble data_min, data_max, data_mean;
    int i;

    gwy_debug(" ");

    gwy_data_field_get_min_max(widget->data_field, &data_min, &data_max);
    data_mean = gwy_data_field_get_avg(widget->data_field);

    mat_none = gwy_gl_materials_get_gl_material(GWY_GL_MATERIAL_NONE);
    gwy_3d_view_rgba_dv(GL_FRONT, GL_AMBIENT,
                        gwy_gl_material_get_ambient(mat_none));
    gwy_3d_view_rgba_dv(GL_FRONT, GL_DIFFUSE,
                        gwy_gl_material_get_diffuse(mat_none));
    gwy_3d_view_rgba_dv(GL_FRONT, GL_SPECULAR,
                        gwy_gl_material_get_specular(mat_none));
    glMaterialf(GL_FRONT, GL_SHININESS,
                (GLfloat)gwy_gl_material_get_shininess(mat_none)*128.0f);
    glPushMatrix();
    plane_z = GWY_3D_Z_TRANSFORMATION
              *(data_mean - data_min)/(data_max  - data_min)
              + GWY_3D_Z_DISPLACEMENT;

    glTranslatef(0.0f, 0.0f, plane_z);
    glRotatef(widget->setup->light_theta * RAD_2_DEG, 0.0f, 0.0f, 1.0f);
    glRotatef(-widget->setup->light_phi * RAD_2_DEG, 0.0f, 1.0f, 0.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_QUAD_STRIP);
        for (i = -180; i <= 180; i += 5) {
            GLfloat x = cos(i * DEG_2_RAD) * G_SQRT2;
            GLfloat z = sin(i * DEG_2_RAD) * G_SQRT2;
            glVertex3f(x,  0.05f, z);
            glVertex3f(x, -0.05f, z);
        }
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_LINE_STRIP);
        glVertex3f(0.0f,  0.0f,  0.0f);
        glVertex3f(0.0f,  0.05f, G_SQRT2);
        glVertex3f(0.0f, -0.05f, G_SQRT2);
        glVertex3f(0.0f,  0.0f,  0.0f);
    glEnd();
    glPopMatrix();
}

static void
gwy_3d_view_realize_gl(Gwy3DView *gwy3dview)
{
    GdkGLContext *glcontext;
    GdkGLDrawable *gldrawable;

    GLfloat ambient[] = { 0.1, 0.1, 0.1, 1.0 };
    GLfloat diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat position[] = { 0.0, 3.0, 3.0, 1.0 };

    GLfloat lmodel_ambient[] = { 0.2, 0.2, 0.2, 1.0 };
    GLfloat local_view[] = { 0.0 } ;

    gwy_debug("GL capable %d", gtk_widget_is_gl_capable(GTK_WIDGET(gwy3dview)));

    glcontext = gtk_widget_get_gl_context(GTK_WIDGET(gwy3dview));
    gldrawable = gtk_widget_get_gl_drawable(GTK_WIDGET(gwy3dview));
    /*** OpenGL BEGIN ***/
    gwy_debug("drawable %p, context %p", gldrawable, glcontext);

    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return;

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClearDepth(1.0);

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
    glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, local_view);

    glFrontFace(GL_CW);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_AUTO_NORMAL);
    glEnable(GL_NORMALIZE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    /* Shape display lists */
    gwy_3d_view_assign_lists(gwy3dview);
    gwy_3d_make_list(gwy3dview, gwy3dview->data_field, GWY_3D_SHAPE_FULL);
    gwy_3d_make_list(gwy3dview, gwy3dview->downsampled, GWY_3D_SHAPE_REDUCED);

    gdk_gl_drawable_gl_end(gldrawable);
    /*** OpenGL END ***/

    return;
}

static void
gwy_3d_set_projection(Gwy3DView *gwy3dview)
{
    GLfloat w, h;
    GLfloat aspect;

    gwy_debug(" ");

    w = GTK_WIDGET(gwy3dview)->allocation.width;
    h = GTK_WIDGET(gwy3dview)->allocation.height;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (w > h) {
        aspect = w / h;
        switch (gwy3dview->setup->projection) {
            case GWY_3D_PROJECTION_ORTHOGRAPHIC:
            glOrtho(-aspect * GWY_3D_ORTHO_CORRECTION,
                     aspect * GWY_3D_ORTHO_CORRECTION,
                     -1.0   * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                      1.0   * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                      5.0,
                      60.0);
            break;

            case GWY_3D_PROJECTION_PERSPECTIVE:
            glFrustum(-aspect, aspect, -1.0 , 1.0, 5.0, 60.0);
            break;
        }
    }
    else {
        aspect = h / w;
        switch (gwy3dview->setup->projection) {
            case GWY_3D_PROJECTION_ORTHOGRAPHIC:
            glOrtho( -1.0   * GWY_3D_ORTHO_CORRECTION,
                      1.0   * GWY_3D_ORTHO_CORRECTION,
                    -aspect * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                     aspect * GWY_3D_ORTHO_CORRECTION /* * deformation_z*/,
                      5.0,
                      60.0);
            break;

            case GWY_3D_PROJECTION_PERSPECTIVE:
            glFrustum(-1.0, 1.0, -aspect, aspect, 5.0, 60.0);
            break;
        }
    }
    glMatrixMode(GL_MODELVIEW);
}

static void
gwy_3d_pango_ft2_render_layout(PangoLayout *layout)
{
    PangoRectangle logical_rect;
    FT_Bitmap bitmap;
    GLvoid *pixels;
    guint32 *p;
    GLfloat color[4];
    guint32 rgb;
    GLfloat a;
    guint8 *row, *row_end;
    int i;

    pango_layout_get_pixel_extents(layout, NULL, &logical_rect);
    if (logical_rect.width == 0 || logical_rect.height == 0)
        return;

    bitmap.rows = logical_rect.height;
    bitmap.width = logical_rect.width;
    bitmap.pitch = bitmap.width;
    bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
    bitmap.num_grays = 256;
    bitmap.buffer = g_malloc0(bitmap.rows * bitmap.pitch);

    pango_ft2_render_layout(&bitmap, layout, -logical_rect.x, 0);

    pixels = g_malloc(bitmap.rows * bitmap.width * 4);
    p = (guint32 *)pixels;

    glGetFloatv(GL_CURRENT_COLOR, color);
#if !defined(GL_VERSION_1_2) && G_BYTE_ORDER == G_LITTLE_ENDIAN
    rgb = ((guint32)(color[0] * 255.0))
          | (((guint32)(color[1] * 255.0)) << 8)
          | (((guint32)(color[2] * 255.0)) << 16);
#else
    rgb = (((guint32)(color[0] * 255.0)) << 24)
          | (((guint32)(color[1] * 255.0)) << 16)
          | (((guint32)(color[2] * 255.0)) << 8);
#endif
    a = color[3];

    row = bitmap.buffer + bitmap.rows * bitmap.width; /* past-the-end */
    row_end = bitmap.buffer;      /* beginning */

    if (a == 1.0) {
        do {
            row -= bitmap.width;
            for (i = 0; i < bitmap.width; i++)
#if !defined(GL_VERSION_1_2) && G_BYTE_ORDER == G_LITTLE_ENDIAN
                *p++ = rgb | (((guint32) row[i]) << 24);
#else
            *p++ = rgb | ((guint32) row[i]);
#endif
        }
        while (row != row_end);
    }
    else {
        do {
            row -= bitmap.width;
            for (i = 0; i < bitmap.width; i++)
#if !defined(GL_VERSION_1_2) && G_BYTE_ORDER == G_LITTLE_ENDIAN
                *p++ = rgb | (((guint32) (a * row[i])) << 24);
#else
            *p++ = rgb | ((guint32) (a * row[i]));
#endif
        }
        while (row != row_end);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#if !defined(GL_VERSION_1_2)
    glDrawPixels(bitmap.width, bitmap.rows,
                 GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels);
#else
    glDrawPixels(bitmap.width, bitmap.rows,
                 GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
                 pixels);
#endif

    glDisable(GL_BLEND);

    g_free(bitmap.buffer);
    g_free(pixels);
}

static void
gwy_3d_print_text(Gwy3DView     *gwy3dview,
                  Gwy3DViewLabel id,
                  GLfloat        raster_x,
                  GLfloat        raster_y,
                  GLfloat        raster_z,
                  guint          size,
                  gint           vjustify,
                  gint           hjustify)
{
    PangoContext *widget_context;
    PangoFontDescription *font_desc;
    PangoLayout *layout;
    PangoRectangle logical_rect;
    GLfloat text_w, text_h;
    guint hlp = 0;
    Gwy3DLabel *label;
    gint displacement_x, displacement_y;
    gchar *text;

    label = gwy3dview->labels[id];
    text = gwy_3d_label_expand_text(label, gwy3dview->variables);
    size = gwy_3d_label_user_size(label, size);
    displacement_x = ROUND(label->delta_x);
    displacement_y = ROUND(label->delta_y);

    /* Font */
    /* FIXME: is it possible for pango to write on trasnparent background? */
    widget_context = gtk_widget_get_pango_context(GTK_WIDGET(gwy3dview));
    font_desc = pango_context_get_font_description(widget_context);
    pango_font_description_set_size(font_desc, size * PANGO_SCALE);
    pango_context_set_font_description(gwy3dview->ft2_context, font_desc);

    /* Text layout */
    layout = pango_layout_new(gwy3dview->ft2_context);
    pango_layout_set_width(layout, -1);
    pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
    pango_layout_set_markup(layout, text, -1);
    /* TODO: use Pango to rotate text, after Pango is capable doing it */

    /* Text position */
    pango_layout_get_pixel_extents(layout, NULL, &logical_rect);
    text_w = logical_rect.width;
    text_h = logical_rect.height;

    glRasterPos3f(raster_x, raster_y, raster_z);
    glBitmap(0, 0, 0, 0, displacement_x, displacement_y, (GLubyte *)&hlp);
    if (vjustify < 0) {
       /* vertically justified to the bottom */
    }
    else if (vjustify == 0) {
       /* vertically justified to the middle */
       glBitmap(0, 0, 0, 0, 0, -text_h/2, (GLubyte *)&hlp);
    }
    else {
       /* vertically justified to the top */
       glBitmap(0, 0, 0, 0, 0, -text_h, (GLubyte *)&hlp);
    }

    if (hjustify < 0) {
       /* horizontally justified to the left */
    }
    else if (hjustify == 0) {
       /* horizontally justified to the middle */
       glBitmap(0, 0, 0, 0, -text_w/2, 0, (GLubyte *)&hlp);
    }
    else {
       /* horizontally justified to the right */
       glBitmap(0, 0, 0, 0, -text_w, 0, (GLubyte *)&hlp);
    }

    /* Render text */
    gwy_3d_pango_ft2_render_layout(layout);

    g_object_unref(G_OBJECT(layout));

    return ;
}

/**
 * gwy_3d_view_class_make_list_pool:
 * @klass: 3D view class.
 *
 * Allocates a pool of OpenGL lists to assign list numbers from.
 *
 * We are only able to get GLContext-unique list id's from glGenLists(),
 * but lists with the same id's seem to interfere across GLContexts.  See
 * bug #53 for more.
 *
 * We store the pool in bits of one 64bit integer.
 **/
static void
gwy_3d_view_class_make_list_pool(Gwy3DListPool *pool)
{
    guint try_size = 64;

    glGetError();
    while (try_size >= 1) {
        pool->base = glGenLists(GWY_3D_N_LISTS*try_size);
        if (!glGetError()) {
            gwy_debug("Allocated a pool with %u items (%u lists)",
                      try_size, GWY_3D_N_LISTS*try_size);
            pool->size = try_size;
            return;
        }
        try_size = (try_size*2)/3;
    }
    g_warning("Cannot get any OpenGL lists");
    pool->base = 0;
}

/**
 * gwy_3d_view_allocate_lists:
 * @gwy3dview: A 3D data view widget.
 *
 * Allocates a block of free OpenGL lists from pool for this 3D view to use.
 **/
static void
gwy_3d_view_assign_lists(Gwy3DView *gwy3dview)
{
    Gwy3DListPool *pool;
    guint64 b;
    guint i;

    pool = GWY_3D_VIEW_GET_CLASS(gwy3dview)->list_pool;
    if (!pool->size) {
        g_return_if_fail(GTK_WIDGET_REALIZED(gwy3dview));
        gwy_3d_view_class_make_list_pool(pool);
    }
    g_return_if_fail(pool->size > 0);

    b = 1;
    for (i = 0; i < pool->size && (pool->pool & b); i++)
        b <<= 1;
    if (i == pool->size) {
        g_critical("No more free OpenGL lists");
        return;
    }

    gwy_debug("Assigned list #%u", i);
    pool->pool |= b;
    gwy3dview->shape_list_base = pool->base + i*GWY_3D_N_LISTS;
}

/**
 * gwy_3d_view_release_lists:
 * @gwy3dview: A 3D data view widget.
 *
 * Returns OpenGL lists in use by this 3D view back to the pool.
 **/
static void
gwy_3d_view_release_lists(Gwy3DView *gwy3dview)
{
    Gwy3DListPool *pool;
    guint i;
    guint64 b = 1;

    pool = GWY_3D_VIEW_GET_CLASS(gwy3dview)->list_pool;
    g_return_if_fail(gwy3dview->shape_list_base >= pool->base);

    i = (gwy3dview->shape_list_base - pool->base)/GWY_3D_N_LISTS;
    gwy_debug("Released list #%u", i);
    b <<= i;
    pool->pool &= ~b;
    gwy3dview->shape_list_base = 0;
}

#else /* HAVE_GTKGLEXT */
/* Export the same set of symbols if we don't have OpenGL support.  But let
 * them all fail. */
static void
gwy_3d_view_init(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
}

static void
gwy_3d_view_class_init(G_GNUC_UNUSED Gwy3DViewClass *klass)
{
}

GtkWidget*
gwy_3d_view_new(G_GNUC_UNUSED GwyContainer *data)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

const gchar*
gwy_3d_view_get_setup_prefix(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

void
gwy_3d_view_set_setup_prefix(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                             G_GNUC_UNUSED const gchar *key)
{
    g_critical("OpenGL support was not compiled in.");
}

const gchar*
gwy_3d_view_get_data_key(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

void
gwy_3d_view_set_data_key(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                         G_GNUC_UNUSED const gchar *key)
{
    g_critical("OpenGL support was not compiled in.");
}

const gchar*
gwy_3d_view_get_gradient_key(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

void
gwy_3d_view_set_gradient_key(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                             G_GNUC_UNUSED const gchar *key)
{
    g_critical("OpenGL support was not compiled in.");
}

const gchar*
gwy_3d_view_get_material_key(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

void
gwy_3d_view_set_material_key(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                             G_GNUC_UNUSED const gchar *key)
{
    g_critical("OpenGL support was not compiled in.");
}

guint
gwy_3d_view_get_reduced_size(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return 0;
}

void
gwy_3d_view_set_reduced_size(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                             G_GNUC_UNUSED guint reduced_size)
{
    g_critical("OpenGL support was not compiled in.");
}

Gwy3DMovement
gwy_3d_view_get_movement_type(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return 0;
}

void
gwy_3d_view_set_movement_type(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                              G_GNUC_UNUSED Gwy3DMovement movement)
{
    g_critical("OpenGL support was not compiled in.");
}

GdkPixbuf*
gwy_3d_view_get_pixbuf(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

Gwy3DLabel*
gwy_3d_view_get_label(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                      G_GNUC_UNUSED Gwy3DViewLabel label)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

Gwy3DSetup*
gwy_3d_view_get_setup(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

GwyContainer*
gwy_3d_view_get_data(G_GNUC_UNUSED Gwy3DView *gwy3dview)
{
    g_critical("OpenGL support was not compiled in.");
    return NULL;
}

void
gwy_3d_view_get_scale_range(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                            G_GNUC_UNUSED gdouble *min_scale,
                            G_GNUC_UNUSED gdouble *max_scale)
{
    g_critical("OpenGL support was not compiled in.");
}

void
gwy_3d_view_set_scale_range(G_GNUC_UNUSED Gwy3DView *gwy3dview,
                            G_GNUC_UNUSED gdouble min_scale,
                            G_GNUC_UNUSED gdouble max_scale)
{
    g_critical("OpenGL support was not compiled in.");
}
#endif /* HAVE_GTKGLEXT */

/************************** Documentation ****************************/

/**
 * SECTION:gwy3dview
 * @title: Gwy3DView
 * @short_description: OpenGL 3D data display
 * @see_also: #Gwy3DWindow -- window combining 3D view with controls,
 *            #GwyGLMaterial -- OpenGL materials,
 *            #Gwy3DLabel -- Labels on 3D view,
 *            #Gwy3DSetup -- 3D scene setup
 *
 * #Gwy3DView displays a data field as a threedimensional heightfield using
 * OpenGL. You can create a new 3D view for a data container with
 * gwy_3d_view_new().  By default, it inherits properties like palette from
 * <link linkend="GwyDataView">data view</link> settings, but supports separate
 * settings -- see gwy_3d_view_set_palette() et al.
 *
 * #Gwy3DView allows the user to interactively rotate, scale, z-scale the data
 * or move lights, depending on its <link linkend="Gwy3DMovement">movement
 * state</link>. There are no controls provided for mode change, you have to
 * provide some yourself and set the movement mode with
 * gwy_3d_view_set_movement_type().
 *
 * You have initialize GtkGLExt with gtk_gl_init_check() and then Gwyddion's
 * OpenGL with gwy_widgets_gl_init() before you can use #Gwy3DView.  These
 * functions may not always succeed, see their description for more.  If
 * OpenGL initialization fails (possible because its support was not compiled
 * in) #Gwy3DView cannot be even instantiated.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
