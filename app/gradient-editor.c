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
    PREVIEW_HEIGHT = 30,
    MARKER_HEIGHT = 12,
    BITS_PER_SAMPLE = 8
};

#define GWY_TYPE_GRADIENT_EDITOR             (gwy_gradient_editor_get_type())
#define GWY_GRADIENT_EDITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRADIENT_EDITOR, GwyGradientEditor))
#define GWY_GRADIENT_EDITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRADIENT_EDITOR, GwyGradientEditorClass))
#define GWY_IS_GRADIENT_EDITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRADIENT_EDITOR))
#define GWY_IS_GRADIENT_EDITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRADIENT_EDITOR))
#define GWY_GRADIENT_EDITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRADIENT_EDITOR, GwyGradientEditorClass))

typedef struct _GwyGradientEditor      GwyGradientEditor;
typedef struct _GwyGradientEditorClass GwyGradientEditorClass;

struct _GwyGradientEditor {
    GwyResourceEditor parent_instance;

    gint last_marker;
    GtkWidget *colorsel;
    GtkWidget *markers;
    GtkWidget *preview;
    GdkPixbuf *preview_pixbuf;
    GwyRGBA old;
};

struct _GwyGradientEditorClass {
    GwyResourceEditorClass parent_class;
};

static GType gwy_gradient_editor_get_type     (void) G_GNUC_CONST;
static void gwy_gradient_editor_construct     (GwyResourceEditor *res_editor);
static void gwy_gradient_editor_preview_expose(GwyGradientEditor *editor);
static void gwy_gradient_editor_apply         (GwyResourceEditor *res_editor);
static void gwy_gradient_editor_switch        (GwyResourceEditor *res_editor);
static void gwy_gradient_editor_update        (GwyGradientEditor *editor);

G_DEFINE_TYPE(GwyGradientEditor, gwy_gradient_editor,
              GWY_TYPE_RESOURCE_EDITOR)

static void
gwy_gradient_editor_class_init(GwyGradientEditorClass *klass)
{
    GwyResourceEditorClass *editor_class = GWY_RESOURCE_EDITOR_CLASS(klass);

    editor_class->resource_type = GWY_TYPE_GRADIENT;
    editor_class->base_resource = GWY_GRADIENT_DEFAULT;
    editor_class->window_title = _("Color Gradient Editor");
    editor_class->editor_title = _("Color Gradient %s");
    editor_class->construct_treeview = gwy_gradient_tree_view_new;
    editor_class->construct_editor = gwy_gradient_editor_construct;
    editor_class->apply_changes = gwy_gradient_editor_apply;
    editor_class->switch_resource = gwy_gradient_editor_switch;
    gwy_resource_editor_class_setup(editor_class);
}

static void
gwy_gradient_editor_init(GwyGradientEditor *editor)
{
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
gwy_gradient_editor_validate_marker(GwyHMarkerBox *hmbox,
                                    GwyMarkerOperationType optype,
                                    gint i,
                                    gdouble *pos)
{
    gdouble prev, next;
    gint n;

    /* Nothing at all can be done with border markers */
    if (i == 0)
        return FALSE;
    n = gwy_hmarker_box_get_nmarkers(hmbox);
    if ((optype == GWY_MARKER_OPERATION_ADD && i == n)
        || (optype != GWY_MARKER_OPERATION_ADD && i == n-1))
        return FALSE;

    /* Other markers can be moved only from previous to next */
    if (optype == GWY_MARKER_OPERATION_MOVE) {
        prev = gwy_hmarker_box_get_marker_position(hmbox, i-1);
        next = gwy_hmarker_box_get_marker_position(hmbox, i+1);
        *pos = CLAMP(*pos, prev, next);
    }
    return TRUE;
}

static void
gwy_gradient_editor_construct(GwyResourceEditor *res_editor)
{
    static const gdouble default_markers[] = { 0.0, 1.0 };
    GtkWidget *vbox, *colorsel;
    GwyGradientEditor *editor;

    g_return_if_fail(GTK_IS_WINDOW(res_editor->edit_window));
    editor = GWY_GRADIENT_EDITOR(res_editor);

    gtk_container_set_border_width(GTK_CONTAINER(res_editor->edit_window),
                                   4);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(res_editor->edit_window), vbox);

    colorsel = editor->colorsel = gtk_color_selection_new();
    gtk_box_pack_start(GTK_BOX(vbox), colorsel, FALSE, FALSE, 0);
    /* XXX */
    gtk_color_selection_set_has_opacity_control(GTK_COLOR_SELECTION(colorsel),
                                                FALSE);
    gtk_color_selection_set_has_palette(GTK_COLOR_SELECTION(colorsel), FALSE);
    g_signal_connect_swapped(colorsel, "color-changed",
                             G_CALLBACK(gwy_resource_editor_queue_commit),
                             res_editor);
    gtk_widget_set_sensitive(colorsel, FALSE);

    editor->markers = gwy_hmarker_box_new();
    gwy_hmarker_box_set_markers(GWY_HMARKER_BOX(editor->markers),
                                G_N_ELEMENTS(default_markers), default_markers);
    gwy_hmarker_box_set_flipped(GWY_HMARKER_BOX(editor->markers), TRUE);
    gwy_hmarker_box_set_validator(GWY_HMARKER_BOX(editor->markers),
                                  &gwy_gradient_editor_validate_marker);
    gtk_box_pack_start(GTK_BOX(vbox), editor->markers, FALSE, FALSE, 0);

    editor->preview = gtk_drawing_area_new();
    gtk_widget_set_size_request(editor->preview, -1, PREVIEW_HEIGHT);
    g_signal_connect_swapped(editor->preview, "expose-event",
                             G_CALLBACK(gwy_gradient_editor_preview_expose),
                             editor);
    gtk_box_pack_start(GTK_BOX(vbox), editor->preview, FALSE, FALSE, 0);

    gtk_widget_show_all(vbox);

    /* switch */
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
    GtkColorSelection *colorsel;
    GwyGradient *gradient;
    GdkColor gdkcolor;
    const GwyRGBA *color;
    gdouble *positions;
    gint i, npoints;

    res_editor = GWY_RESOURCE_EDITOR(editor);
    /*
    editor->last_component = gwy_radio_buttons_get_current(editor->components,
                                                           "color-component");
                                                           */
    gradient = GWY_GRADIENT(gwy_resource_editor_get_edited(res_editor));
    g_return_if_fail(gradient
                     && gwy_resource_get_is_modifiable(GWY_RESOURCE(gradient)));

    points = gwy_gradient_get_points(gradient, &npoints);
    positions = g_newa(gdouble, npoints);
    for (i = 0; i < npoints; i++)
        positions[i] = points[i].x;
    gwy_hmarker_box_set_markers(GWY_HMARKER_BOX(editor->markers),
                                npoints, positions);

#if 0
    switch (editor->last_component) {
        case GRADIENT_AMBIENT:
        color = gwy_gradient_get_ambient(gradient);
        break;

        case GRADIENT_DIFFUSE:
        color = gwy_gradient_get_diffuse(gradient);
        break;

        case GRADIENT_SPECULAR:
        color = gwy_gradient_get_specular(gradient);
        break;

        case GRADIENT_EMISSION:
        color = gwy_gradient_get_emission(gradient);
        break;

        default:
        g_return_if_reached();
        break;
    }
    colorsel = GTK_COLOR_SELECTION(editor->colorsel);
    gwy_rgba_to_gdk_color(color, &gdkcolor);
    gtk_color_selection_set_current_color(colorsel, &gdkcolor);
    gwy_rgba_to_gdk_color(&editor->old[editor->last_component], &gdkcolor);
    gtk_color_selection_set_previous_color(colorsel, &gdkcolor);
#endif
}

static void
gwy_gradient_editor_apply(GwyResourceEditor *res_editor)
{
#if 0
    GwyGradientEditor *editor;
    GwyGradient *gradient;
    GwyRGBA color;
    GdkColor gdkcolor;
    gdouble val;

    gradient = GWY_GRADIENT(gwy_resource_editor_get_edited(res_editor));
    g_return_if_fail(gradient
                     && gwy_resource_get_is_modifiable(GWY_RESOURCE(gradient)));

    editor = GWY_GRADIENT_EDITOR(res_editor);
    gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(editor->colorsel),
                                          &gdkcolor);
    gwy_rgba_from_gdk_color(&color, &gdkcolor);
    color.a = 1.0;    /* FIXME */
    switch (editor->last_component) {
        case GRADIENT_AMBIENT:
        gwy_gradient_set_ambient(gradient, &color);
        break;

        case GRADIENT_DIFFUSE:
        gwy_gradient_set_diffuse(gradient, &color);
        break;

        case GRADIENT_SPECULAR:
        gwy_gradient_set_specular(gradient, &color);
        break;

        case GRADIENT_EMISSION:
        gwy_gradient_set_emission(gradient, &color);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    val = gtk_adjustment_get_value(GTK_ADJUSTMENT(editor->shininess));
    gwy_gradient_set_shininess(gradient, val);
#endif
}

static void
gwy_gradient_editor_switch(GwyResourceEditor *res_editor)
{
    GwyGradient *gradient;
    GwyGradientEditor *editor;

    editor = GWY_GRADIENT_EDITOR(res_editor);

    gradient = GWY_GRADIENT(gwy_resource_editor_get_edited(res_editor));
    g_return_if_fail(gradient
                     && gwy_resource_get_is_modifiable(GWY_RESOURCE(gradient)));

    /*
    if (gwy_app_gl_is_ok())
        gwy_3d_view_set_material(GWY_3D_VIEW(editor->preview),
                                 gwy_resource_get_name(GWY_RESOURCE(gradient)));

    editor->old[GRADIENT_AMBIENT] = *gwy_gradient_get_ambient(gradient);
    editor->old[GRADIENT_DIFFUSE] = *gwy_gradient_get_diffuse(gradient);
    editor->old[GRADIENT_SPECULAR] = *gwy_gradient_get_specular(gradient);
    editor->old[GRADIENT_EMISSION] = *gwy_gradient_get_emission(gradient);
    */
    gwy_gradient_editor_update(editor);
}

/************************** Documentation ****************************/

/**
 * SECTION:gradient-editor
 * @title: GwyGradientEditor
 * @short_description: OpenGL gradient editor
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

