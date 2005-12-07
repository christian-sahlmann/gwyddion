/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwyddion.h>
#include <libprocess/filters.h>
#include <libprocess/arithmetic.h>
#include <libdraw/gwygradient.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/resource-editor.h>
#include <app/gradient-editor.h>

enum {
    GWY_MARKER_OPERATION_NONE = -1,
    GWY_MARKER_OPERATION_CURVE = -2 /*XXX: works for now */
};

enum {
    PREVIEW_HEIGHT = 30,
    MARKER_HEIGHT = 12,
    BITS_PER_SAMPLE = 8
};

typedef enum {
    EDITING_MODE_POINTS,
    EDITING_MODE_CURVE
} EditingMode;

#define GWY_TYPE_GRADIENT_EDITOR             (gwy_gradient_editor_get_type())
#define GWY_GRADIENT_EDITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRADIENT_EDITOR, GwyGradientEditor))
#define GWY_GRADIENT_EDITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRADIENT_EDITOR, GwyGradientEditorClass))
#define GWY_IS_GRADIENT_EDITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRADIENT_EDITOR))
#define GWY_IS_GRADIENT_EDITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRADIENT_EDITOR))
#define GWY_GRADIENT_EDITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRADIENT_EDITOR, GwyGradientEditorClass))

typedef struct _GwyGradientEditor      GwyGradientEditor;
typedef struct _GwyGradientEditorClass GwyGradientEditorClass;

typedef struct {
    GwyMarkerOperationType optype;
    gint i;
} PendingOperation;

struct _GwyGradientEditor {
    GwyResourceEditor parent_instance;

    GwyGradient *gradient;
    gulong gradient_id;

    GSList *mode_group;
    PendingOperation pendop;
    GtkWidget *curve;
    GtkWidget *colorsel;
    GtkWidget *markers;
    GtkWidget *preview;
    GdkPixbuf *preview_pixbuf;
    GwyRGBA old;
};

struct _GwyGradientEditorClass {
    GwyResourceEditorClass parent_class;
};

static GType gwy_gradient_editor_get_type       (void) G_GNUC_CONST;
static void gwy_gradient_editor_destroy         (GtkObject *object);
static void gwy_gradient_editor_construct       (GwyResourceEditor *res_editor);
static void gwy_gradient_editor_preview_expose  (GwyGradientEditor *editor);
static void gwy_gradient_editor_apply           (GwyResourceEditor *res_editor);
static void gwy_gradient_editor_switch          (GwyResourceEditor *res_editor);
static void gwy_gradient_editor_update_curve    (GwyGradientEditor *editor);
static void gwy_gradient_editor_update          (GwyGradientEditor *editor);
static void gwy_gradient_editor_marker_selected (GwyGradientEditor *editor,
                                                 gint i);
static void gwy_gradient_editor_color_changed   (GwyGradientEditor *editor);
static void gwy_gradient_editor_marker_moved    (GwyGradientEditor *editor,
                                                 gint i);
static void gwy_gradient_editor_marker_added    (GwyGradientEditor *editor,
                                                 gint i);
static void gwy_gradient_editor_marker_removed  (GwyGradientEditor *editor,
                                                 gint i);
static void gwy_resource_editor_gradient_changed(GwyGradientEditor *editor);
static void gwy_gradient_editor_mode_changed    (GtkWidget *toggle,
                                                 GwyGradientEditor *editor);
static void gwy_gradient_editor_save_view       (GwyGradientEditor *editor);
static void gwy_gradient_editor_load_view       (GwyGradientEditor *editor);
static void gwy_gradient_editor_curve_edited    (GwyGradientEditor *editor);

G_DEFINE_TYPE(GwyGradientEditor, gwy_gradient_editor,
              GWY_TYPE_RESOURCE_EDITOR)

static void
gwy_gradient_editor_class_init(GwyGradientEditorClass *klass)
{
    GwyResourceEditorClass *editor_class = GWY_RESOURCE_EDITOR_CLASS(klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

    object_class->destroy = gwy_gradient_editor_destroy;

    editor_class->resource_type = GWY_TYPE_GRADIENT;
    editor_class->base_resource = GWY_GRADIENT_DEFAULT;
    editor_class->window_title = _("Color Gradient Editor");
    editor_class->editor_title = _("Color Gradient `%s'");
    editor_class->construct_treeview = gwy_gradient_tree_view_new;
    editor_class->construct_editor = gwy_gradient_editor_construct;
    editor_class->apply_changes = gwy_gradient_editor_apply;
    editor_class->switch_resource = gwy_gradient_editor_switch;
    gwy_resource_editor_class_setup(editor_class);
}

static void
gwy_gradient_editor_init(G_GNUC_UNUSED GwyGradientEditor *editor)
{
}

static void
gwy_gradient_editor_destroy(GtkObject *object)
{
    GwyGradientEditor *editor;

    editor = GWY_GRADIENT_EDITOR(object);
    if (editor->gradient_id) {
        g_signal_handler_disconnect(editor->gradient, editor->gradient_id);
        editor->gradient_id = 0;
        editor->gradient = NULL;
    }
    gwy_object_unref(editor->preview_pixbuf);

    GTK_OBJECT_CLASS(gwy_gradient_editor_parent_class)->destroy(object);
}

/**
 * gwy_app_gradient_editor:
 *
 * Creates or presents color gradient editor.
 *
 * Gradient editor is singleton, therefore if it doesn't exist, this function
 * creates and displays it.  If it already exists, it simply calls
 * gtk_window_present() on the existing instance.  It exists until it's closed
 * by user.
 **/
void
gwy_app_gradient_editor(void)
{
    GwyGradientEditorClass *klass;
    GwyResourceEditor *editor;

    klass = g_type_class_ref(GWY_TYPE_GRADIENT_EDITOR);
    if ((editor = GWY_RESOURCE_EDITOR_CLASS(klass)->instance)) {
        gtk_window_present(GTK_WINDOW(editor));
        g_type_class_unref(klass);
        return;
    }

    editor = g_object_new(GWY_TYPE_GRADIENT_EDITOR, NULL);
    gwy_resource_editor_setup(editor);
    g_type_class_unref(klass);
    gtk_widget_show_all(GTK_WIDGET(editor));
}

static gboolean
gwy_gradient_editor_validate_marker(GwyMarkerBox *hmbox,
                                    GwyMarkerOperationType optype,
                                    gint *i,
                                    gdouble *pos)
{
    const gdouble *markers;
    gint j, n;

    n = gwy_marker_box_get_nmarkers(hmbox);

    /* Insertions are sorted an cannot happen outside border markers */
    if (optype == GWY_MARKER_OPERATION_ADD) {
        markers = gwy_marker_box_get_markers(hmbox);
        for (j = 0; j < n; j++) {
            if (*pos < markers[j])
                break;
        }
        if (j == 0 || j == n)
            return FALSE;
        *i = j;
        return TRUE;
    }

    /* Nothing at all can be done with border markers */
    if (*i == 0 || *i == n-1)
        return FALSE;

    /* Inner markers can be moved only from previous to next */
    if (optype == GWY_MARKER_OPERATION_MOVE) {
        markers = gwy_marker_box_get_markers(hmbox);
        *pos = CLAMP(*pos, markers[*i - 1], markers[*i + 1]);
    }
    return TRUE;
}

static void
gwy_gradient_editor_construct(GwyResourceEditor *res_editor)
{
    static const GwyEnum editing_modes[] = {
        { N_("_Points"), EDITING_MODE_POINTS, },
        { N_("_Curve"),  EDITING_MODE_CURVE,  },
    };
    static const gdouble default_markers[] = { 0.0, 1.0 };
    GtkWidget *vbox, *vvbox, *hbox, *colorsel;
    GSList *group, *l;
    GwyGradientEditor *editor;
    GwyRGBA colors[3];

    g_return_if_fail(GTK_IS_WINDOW(res_editor->edit_window));
    editor = GWY_GRADIENT_EDITOR(res_editor);

    gtk_container_set_border_width(GTK_CONTAINER(res_editor->edit_window),
                                   4);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_container_add(GTK_CONTAINER(res_editor->edit_window), vbox);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    g_signal_connect_swapped(hbox, "destroy",
                             G_CALLBACK(gwy_gradient_editor_save_view), editor);

    group = gwy_radio_buttons_create
                        (editing_modes, G_N_ELEMENTS(editing_modes),
                         "editing-mode",
                         G_CALLBACK(gwy_gradient_editor_mode_changed),
                         editor, EDITING_MODE_POINTS);
    for (l = group; l; l = g_slist_next(l)) {
        gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(l->data), FALSE);
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(l->data),
                           FALSE, FALSE, 0);
    }
    editor->mode_group = group;

    editor->curve = gwy_curve_new();
    gwy_curve_set_range(GWY_CURVE(editor->curve), 0, 1, 0, 1);
    colors[0].r = 1; colors[0].g = 0; colors[0].b = 0; colors[0].a = 1;
    colors[1].r = 0; colors[1].g = 1; colors[1].b = 0; colors[1].a = 1;
    colors[2].r = 0; colors[2].g = 0; colors[2].b = 1; colors[2].a = 1;
    gwy_curve_set_channels(GWY_CURVE(editor->curve), 3, colors);
    gtk_box_pack_start(GTK_BOX(vbox), editor->curve, TRUE, TRUE, 0);
    gtk_widget_set_size_request(editor->curve, 400, 200);
    g_signal_connect_swapped(editor->curve, "curve-edited",
                             G_CALLBACK(gwy_gradient_editor_curve_edited),
                             editor);

    colorsel = editor->colorsel = gtk_color_selection_new();
    gtk_box_pack_start(GTK_BOX(vbox), colorsel, TRUE, FALSE, 0);
    /* XXX */
    gtk_color_selection_set_has_opacity_control(GTK_COLOR_SELECTION(colorsel),
                                                FALSE);
    gtk_color_selection_set_has_palette(GTK_COLOR_SELECTION(colorsel), FALSE);
    g_signal_connect_swapped(colorsel, "color-changed",
                             G_CALLBACK(gwy_gradient_editor_color_changed),
                             res_editor);
    gtk_widget_set_sensitive(colorsel, FALSE);

    vvbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), vvbox, FALSE, FALSE, 0);

    editor->markers = gwy_hmarker_box_new();
    gtk_widget_set_size_request(editor->markers, -1, MARKER_HEIGHT);
    gwy_marker_box_set_markers(GWY_MARKER_BOX(editor->markers),
                               G_N_ELEMENTS(default_markers), default_markers);
    gwy_marker_box_set_flipped(GWY_MARKER_BOX(editor->markers), TRUE);
    gwy_marker_box_set_validator(GWY_MARKER_BOX(editor->markers),
                                 &gwy_gradient_editor_validate_marker);
    gtk_box_pack_start(GTK_BOX(vvbox), editor->markers, FALSE, FALSE, 0);
    g_signal_connect_swapped(editor->markers, "marker-selected",
                             G_CALLBACK(gwy_gradient_editor_marker_selected),
                             editor);
    g_signal_connect_swapped(editor->markers, "marker-moved",
                             G_CALLBACK(gwy_gradient_editor_marker_moved),
                             editor);
    g_signal_connect_swapped(editor->markers, "marker-added",
                             G_CALLBACK(gwy_gradient_editor_marker_added),
                             editor);
    g_signal_connect_swapped(editor->markers, "marker-removed",
                             G_CALLBACK(gwy_gradient_editor_marker_removed),
                             editor);

    editor->preview = gtk_drawing_area_new();
    gtk_widget_set_size_request(editor->preview, -1, PREVIEW_HEIGHT);
    g_signal_connect_swapped(editor->preview, "expose-event",
                             G_CALLBACK(gwy_gradient_editor_preview_expose),
                             editor);
    gtk_box_pack_start(GTK_BOX(vvbox), editor->preview, FALSE, FALSE, 0);

    gwy_gradient_editor_load_view(editor);
    gtk_widget_show_all(vbox);
    gwy_gradient_editor_switch(res_editor);
}

static void
gwy_gradient_editor_preview_expose(GwyGradientEditor *editor)
{
    GwyResourceEditor *res_editor;
    GwyGradient *gradient;
    gint width, height;

    width = editor->preview->allocation.width;
    height = editor->preview->allocation.height;
    if (!editor->preview_pixbuf
        || width != gdk_pixbuf_get_width(editor->preview_pixbuf)
        || height != gdk_pixbuf_get_height(editor->preview_pixbuf)) {
        gwy_object_unref(editor->preview_pixbuf);
        editor->preview_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE,
                                                BITS_PER_SAMPLE, width, height);
    }

    res_editor = GWY_RESOURCE_EDITOR(editor);
    gradient = GWY_GRADIENT(gwy_resource_editor_get_edited(res_editor));
    gwy_gradient_sample_to_pixbuf(gradient, editor->preview_pixbuf);
    gdk_draw_pixbuf(editor->preview->window,
                    editor->preview->style->fg_gc[GTK_STATE_NORMAL],
                    editor->preview_pixbuf, 0, 0, 0, 0, -1, -1,
                    GDK_RGB_DITHER_NONE, 0, 0);
}

static void
gwy_gradient_editor_update(GwyGradientEditor *editor)
{
    GwyResourceEditor *res_editor;
    const GwyGradientPoint *points;
    GwyGradient *gradient;
    gdouble *positions;
    gint i, npoints;

    res_editor = GWY_RESOURCE_EDITOR(editor);
    gradient = GWY_GRADIENT(gwy_resource_editor_get_edited(res_editor));
    g_return_if_fail(gradient
                     && gwy_resource_get_is_modifiable(GWY_RESOURCE(gradient)));

    points = gwy_gradient_get_points(gradient, &npoints);
    positions = g_newa(gdouble, npoints);
    for (i = 0; i < npoints; i++)
        positions[i] = points[i].x;
    gwy_marker_box_set_markers(GWY_MARKER_BOX(editor->markers),
                               npoints, positions);

    gwy_gradient_editor_update_curve(editor);
}

static void
gwy_gradient_editor_apply(GwyResourceEditor *res_editor)
{
    GwyGradientEditor *editor;
    GtkColorSelection *colorsel;
    GwyMarkerBox *hmbox;
    GwyGradientPoint point, prev, next;
    GwyGradient *gradient;
    GwyChannelData *channel_data;
    GwyGradientPoint *points;
    GdkColor gdkcolor;
    gdouble q;
    gint i, num_pts;;

    gradient = GWY_GRADIENT(gwy_resource_editor_get_edited(res_editor));
    g_return_if_fail(gradient
                     && gwy_resource_get_is_modifiable(GWY_RESOURCE(gradient)));

    editor = GWY_GRADIENT_EDITOR(res_editor);
    colorsel = GTK_COLOR_SELECTION(editor->colorsel);
    hmbox = GWY_MARKER_BOX(editor->markers);
    i = editor->pendop.i;
    switch (editor->pendop.optype)  {
        /* This is both actual move and color change. */
        case GWY_MARKER_OPERATION_MOVE:
        point.x = gwy_marker_box_get_marker_position(hmbox, i);
        gtk_color_selection_get_current_color(colorsel, &gdkcolor);
        gwy_rgba_from_gdk_color(&point.color, &gdkcolor);
        point.color.a = 1.0;    /* FIXME */
        gwy_gradient_set_point(gradient, i, &point);
        break;

        case GWY_MARKER_OPERATION_ADD:
        point.x = gwy_marker_box_get_marker_position(hmbox, i);
        prev = gwy_gradient_get_point(gradient, i-1);
        /* This is would-be-(i+1)-th point, but it's still at i-th position */
        next = gwy_gradient_get_point(gradient, i);
        if (prev.x == next.x)
            q = 0.5;
        else
            q = (point.x - prev.x)/(next.x - prev.x);
        gwy_rgba_interpolate(&prev.color, &next.color, q, &point.color);
        gwy_gradient_insert_point(gradient, i, &point);
        break;

        case GWY_MARKER_OPERATION_REMOVE:
        gwy_gradient_delete_point(gradient, i);
        break;

        case GWY_MARKER_OPERATION_CURVE:
        /* Get gwycurve channel information */
        channel_data = g_new(GwyChannelData, 3);
        gwy_curve_get_control_points(GWY_CURVE(editor->curve), channel_data);

        /* Copy the channel info into the gradient */
        /*
        color.r = channel_data[0].ctlpoints[1].y;
        color.g = channel_data[1].ctlpoints[1].y;
        color.b = channel_data[2].ctlpoints[1].y;
        color.a = 1;
        g_debug("r: %f  g: %f  b: %f", color.r, color.g, color.b);
        gwy_gradient_set_point_color(gradient, 1, &color);
        */

        num_pts = channel_data[0].num_ctlpoints;
        points = g_new(GwyGradientPoint, num_pts);
        for (i=0; i<num_pts; i++) {
            points[i].x = channel_data[0].ctlpoints[i].x;
            points[i].color.r =
                    channel_data[GWY_CURVE_CHANNEL_RED].ctlpoints[i].y;
            points[i].color.g =
                    channel_data[GWY_CURVE_CHANNEL_GREEN].ctlpoints[i].y;
            points[i].color.b =
                    channel_data[GWY_CURVE_CHANNEL_BLUE].ctlpoints[i].y;
            points[i].color.a = 1;
        }
        gwy_gradient_set_points(gradient, num_pts, points);

        /* Free up structures */
        g_free(points);
        for (i = 0; i < 3; i++) {
            g_free(channel_data[i].ctlpoints);
        }
        g_free(channel_data);
        break;

        default:
        editor->pendop.optype = GWY_MARKER_OPERATION_NONE;
        g_return_if_reached();
        break;
    }
    editor->pendop.optype = GWY_MARKER_OPERATION_NONE;
}

static void
gwy_gradient_editor_switch(GwyResourceEditor *res_editor)
{
    GwyGradient *gradient;
    GwyGradientEditor *editor;

    editor = GWY_GRADIENT_EDITOR(res_editor);
    if (editor->gradient_id) {
        g_signal_handler_disconnect(editor->gradient, editor->gradient_id);
        editor->gradient_id = 0;
        editor->gradient = NULL;
    }

    gradient = GWY_GRADIENT(gwy_resource_editor_get_edited(res_editor));
    g_return_if_fail(gradient
                     && gwy_resource_get_is_modifiable(GWY_RESOURCE(gradient)));
    editor->gradient_id = g_signal_connect_swapped
                              (gradient, "data-changed",
                               G_CALLBACK(gwy_resource_editor_gradient_changed),
                               editor);
    editor->gradient = gradient;
    gwy_gradient_editor_update(editor);
    if (GTK_WIDGET_VISIBLE(editor->preview))
        gtk_widget_queue_draw(editor->preview);
}

static void
gwy_gradient_editor_update_curve(GwyGradientEditor *editor)
{
    GwyResourceEditor *res_editor;
    const GwyGradientPoint *grad_points;
    GwyChannelData *channel_data;
    GwyGradient *gradient;
    GwyPoint *points;
    gint c_index, i, point_count;

    res_editor = GWY_RESOURCE_EDITOR(editor);
    gradient = GWY_GRADIENT(gwy_resource_editor_get_edited(res_editor));
    g_return_if_fail(gradient
            && gwy_resource_get_is_modifiable(GWY_RESOURCE(gradient)));
    grad_points = gwy_gradient_get_points(gradient, &point_count);

    channel_data = g_new(GwyChannelData, 3);
    for (c_index = 0; c_index < 3; c_index++) {
        points = g_new(GwyPoint, point_count);
        for (i = 0; i < point_count; i++) {
            points[i].x = grad_points[i].x;
            if (c_index == GWY_CURVE_CHANNEL_RED)
                points[i].y = grad_points[i].color.r;
            else if (c_index == GWY_CURVE_CHANNEL_GREEN)
                points[i].y = grad_points[i].color.g;
            else if (c_index == GWY_CURVE_CHANNEL_BLUE)
                points[i].y = grad_points[i].color.b;
        }
        channel_data[c_index].ctlpoints = points;
        channel_data[c_index].num_ctlpoints = point_count;
    }

    /* Send these control points to the GwyCuve. Note that GwyCurve will
       copy out the channel data, so we must still free the array */
    gwy_curve_set_control_points(GWY_CURVE(editor->curve), channel_data);

    for (i = 0; i < 3; i++) {
        g_free(channel_data[i].ctlpoints);
    }
    g_free(channel_data);
}

static void
gwy_gradient_editor_curve_edited(GwyGradientEditor *editor)
{
    /* XXX Does this need to be here? */
    /*g_return_if_fail(editor->pendop.optype == GWY_MARKER_OPERATION_NONE
    || editor->pendop.optype == GWY_MARKER_OPERATION_MOVE);*/

    editor->pendop.optype = GWY_MARKER_OPERATION_CURVE;
    gwy_resource_editor_queue_commit(GWY_RESOURCE_EDITOR(editor));
}

static void
gwy_gradient_editor_marker_selected(GwyGradientEditor *editor,
                                    gint i)
{
    GwyResourceEditor *res_editor;
    GwyGradient *gradient;
    GwyGradientPoint point;
    GtkColorSelection *colorsel;
    GdkColor gdkcolor;
    gboolean selected;

    res_editor = GWY_RESOURCE_EDITOR(editor);
    gwy_resource_editor_commit(res_editor);

    selected = (i >= 0);
    gtk_widget_set_sensitive(editor->colorsel, selected);
    if (!selected)
        return;

    colorsel = GTK_COLOR_SELECTION(editor->colorsel);
    gradient = GWY_GRADIENT(gwy_resource_editor_get_edited(res_editor));
    point = gwy_gradient_get_point(gradient, i);
    gwy_rgba_to_gdk_color(&point.color, &gdkcolor);
    g_signal_handlers_block_by_func(colorsel,
                                    &gwy_gradient_editor_color_changed,
                                    editor);
    gtk_color_selection_set_current_color(colorsel, &gdkcolor);
    gtk_color_selection_set_previous_color(colorsel, &gdkcolor);
    g_signal_handlers_unblock_by_func(colorsel,
                                      &gwy_gradient_editor_color_changed,
                                      editor);
}

static void
gwy_gradient_editor_color_changed(GwyGradientEditor *editor)
{
    g_return_if_fail(editor->pendop.optype == GWY_MARKER_OPERATION_NONE
                     || editor->pendop.optype == GWY_MARKER_OPERATION_MOVE);
    editor->pendop.optype = GWY_MARKER_OPERATION_MOVE;
    editor->pendop.i = gwy_marker_box_get_selected_marker
                                             (GWY_MARKER_BOX(editor->markers));
    gwy_resource_editor_queue_commit(GWY_RESOURCE_EDITOR(editor));
}

static void
gwy_gradient_editor_marker_moved(GwyGradientEditor *editor,
                                 gint i)
{
    g_return_if_fail(editor->pendop.optype == GWY_MARKER_OPERATION_NONE
                     || editor->pendop.optype == GWY_MARKER_OPERATION_MOVE);
    editor->pendop.optype = GWY_MARKER_OPERATION_MOVE;
    editor->pendop.i = i;
    gwy_resource_editor_queue_commit(GWY_RESOURCE_EDITOR(editor));
}

static void
gwy_gradient_editor_marker_added(GwyGradientEditor *editor,
                                 gint i)
{
    GwyResourceEditor *res_editor;

    res_editor = GWY_RESOURCE_EDITOR(editor);
    gwy_resource_editor_commit(res_editor);

    editor->pendop.optype = GWY_MARKER_OPERATION_ADD;
    editor->pendop.i = i;
    /* Commit immediately */
    gwy_resource_editor_queue_commit(res_editor);
    gwy_resource_editor_commit(res_editor);
}

static void
gwy_gradient_editor_marker_removed(GwyGradientEditor *editor,
                                   gint i)
{
    GwyResourceEditor *res_editor;

    res_editor = GWY_RESOURCE_EDITOR(editor);
    gwy_resource_editor_commit(res_editor);

    editor->pendop.optype = GWY_MARKER_OPERATION_REMOVE;
    editor->pendop.i = i;
    /* Commit immediately */
    gwy_resource_editor_queue_commit(res_editor);
    gwy_resource_editor_commit(res_editor);
}

static void
gwy_resource_editor_gradient_changed(GwyGradientEditor *editor)
{
    if (GTK_WIDGET_VISIBLE(editor->preview))
        gtk_widget_queue_draw(editor->preview);
}

static void
gwy_gradient_editor_mode_changed(G_GNUC_UNUSED GtkWidget *toggle,
                                 GwyGradientEditor *editor)
{
    EditingMode mode;

    mode = gwy_radio_buttons_get_current(editor->mode_group, "editing-mode");
    switch (mode) {
        case EDITING_MODE_POINTS:
        gwy_gradient_editor_update(editor);
        gtk_widget_set_no_show_all(editor->curve, TRUE);
        gtk_widget_hide(editor->curve);
        gtk_widget_set_no_show_all(editor->markers, FALSE);
        gtk_widget_show(editor->markers);
        gtk_widget_set_no_show_all(editor->colorsel, FALSE);
        gtk_widget_show(editor->colorsel);
        break;

        case EDITING_MODE_CURVE:
        gwy_gradient_editor_update_curve(editor);
        gtk_widget_set_no_show_all(editor->colorsel, TRUE);
        gtk_widget_hide(editor->colorsel);
        gtk_widget_set_no_show_all(editor->markers, TRUE);
        gtk_widget_hide(editor->markers);
        gtk_widget_set_no_show_all(editor->curve, FALSE);
        gtk_widget_show(editor->curve);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
gwy_gradient_editor_save_view(GwyGradientEditor *editor)
{
    GwyResourceClass *klass;
    GwyContainer *settings;
    EditingMode mode;
    const gchar *name;
    GString *key;

    klass = g_type_class_ref(GWY_TYPE_GRADIENT);
    name = gwy_resource_class_get_name(klass);
    g_type_class_unref(klass);

    key = g_string_new("");
    g_string_printf(key, "/app/%s/editor/mode", name);

    settings = gwy_app_settings_get();
    mode = gwy_radio_buttons_get_current(editor->mode_group, "editing-mode");
    gwy_container_set_enum_by_name(settings, key->str, mode);
    g_string_free(key, TRUE);
}

static void
gwy_gradient_editor_load_view(GwyGradientEditor *editor)
{
    GwyResourceClass *klass;
    GwyContainer *settings;
    EditingMode mode;
    const gchar *name;
    GString *key;

    klass = g_type_class_ref(GWY_TYPE_GRADIENT);
    name = gwy_resource_class_get_name(klass);
    g_type_class_unref(klass);

    key = g_string_new("");
    g_string_printf(key, "/app/%s/editor/mode", name);

    settings = gwy_app_settings_get();
    mode = EDITING_MODE_POINTS;
    gwy_container_gis_enum_by_name(settings, key->str, &mode);
    g_string_free(key, TRUE);

    gwy_radio_buttons_set_current(editor->mode_group, "editing-mode", mode);
    /* The button doesn't emit anything when it's already in the right state. */
    gwy_gradient_editor_mode_changed(NULL, editor);
}

/************************** Documentation ****************************/

/**
 * SECTION:gradient-editor
 * @title: GwyGradientEditor
 * @short_description: Color gradient editor
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

