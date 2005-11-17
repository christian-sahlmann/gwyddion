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
#include <libdraw/gwyglmaterial.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/resource-editor.h>
#include <app/glmaterial-editor.h>

enum {
    GL_MATERIAL_AMBIENT,
    GL_MATERIAL_DIFFUSE,
    GL_MATERIAL_SPECULAR,
    GL_MATERIAL_EMISSION,
    GL_MATERIAL_N
};

#define GWY_TYPE_GL_MATERIAL_EDITOR             (gwy_gl_material_editor_get_type())
#define GWY_GL_MATERIAL_EDITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GL_MATERIAL_EDITOR, GwyGLMaterialEditor))
#define GWY_GL_MATERIAL_EDITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GL_MATERIAL_EDITOR, GwyGLMaterialEditorClass))
#define GWY_IS_GL_MATERIAL_EDITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GL_MATERIAL_EDITOR))
#define GWY_IS_GL_MATERIAL_EDITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GL_MATERIAL_EDITOR))
#define GWY_GL_MATERIAL_EDITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GL_MATERIAL_EDITOR, GwyGLMaterialEditorClass))

typedef struct _GwyGLMaterialEditor      GwyGLMaterialEditor;
typedef struct _GwyGLMaterialEditorClass GwyGLMaterialEditorClass;

struct _GwyGLMaterialEditor {
    GwyResourceEditor parent_instance;

    guint last_component;
    GSList *components;
    GtkWidget *colorsel;
    GtkObject *shininess;
    GtkWidget *preview;
    GwyRGBA old[GL_MATERIAL_N];
};

struct _GwyGLMaterialEditorClass {
    GwyResourceEditorClass parent_class;
};

static GType gwy_gl_material_editor_get_type    (void) G_GNUC_CONST;
static void  gwy_gl_material_editor_construct   (GwyResourceEditor *res_editor);
static void  gwy_gl_material_editor_apply       (GwyResourceEditor *res_editor);
static void  gwy_gl_material_editor_switch      (GwyResourceEditor *res_editor);
static void  gwy_gl_material_editor_preview_new (GwyGLMaterialEditor *editor);
static void  gwy_gl_material_editor_save_view   (GwyContainer *container);
static void  gwy_gl_material_editor_set_view    (GwyContainer *container,
                                                 gboolean save);
static void  gwy_gl_material_editor_make_data   (GwyDataField *dfield1);
static void  gwy_gl_material_editor_update      (GwyGLMaterialEditor *editor);
static void  gwy_gl_material_editor_component_cb(GtkWidget *widget,
                                                 GwyGLMaterialEditor *editor);

G_DEFINE_TYPE(GwyGLMaterialEditor, gwy_gl_material_editor,
              GWY_TYPE_RESOURCE_EDITOR)

static void
gwy_gl_material_editor_class_init(GwyGLMaterialEditorClass *klass)
{
    GwyResourceEditorClass *editor_class = GWY_RESOURCE_EDITOR_CLASS(klass);

    editor_class->resource_type = GWY_TYPE_GL_MATERIAL;
    editor_class->base_resource = GWY_GL_MATERIAL_DEFAULT;
    editor_class->window_title = _("GL Material Editor");
    editor_class->editor_title = _("GL Material  `%s'");
    editor_class->construct_treeview = gwy_gl_material_tree_view_new;
    editor_class->construct_editor = gwy_gl_material_editor_construct;
    editor_class->apply_changes = gwy_gl_material_editor_apply;
    editor_class->switch_resource = gwy_gl_material_editor_switch;
    gwy_resource_editor_class_setup(editor_class);
}

static void
gwy_gl_material_editor_init(GwyGLMaterialEditor *editor)
{
    editor->last_component = GL_MATERIAL_AMBIENT;
}

/**
 * gwy_app_gl_material_editor:
 *
 * Creates or presents OpenGL material editor.
 *
 * Material editor is singleton, therefore if it doesn't exist, this function
 * creates and displays it.  If it already exists, it simply calls
 * gtk_window_present() on the existing instance.  It exists until it's closed
 * by user.
 **/
void
gwy_app_gl_material_editor(void)
{
    GwyGLMaterialEditorClass *klass;
    GwyResourceEditor *editor;

    klass = g_type_class_ref(GWY_TYPE_GL_MATERIAL_EDITOR);
    if ((editor = GWY_RESOURCE_EDITOR_CLASS(klass)->instance)) {
        gtk_window_present(GTK_WINDOW(editor));
        g_type_class_unref(klass);
        return;
    }

    editor = g_object_new(GWY_TYPE_GL_MATERIAL_EDITOR, NULL);
    gwy_resource_editor_setup(editor);
    g_type_class_unref(klass);
    gtk_widget_show_all(GTK_WIDGET(editor));
}

static void
gwy_gl_material_editor_construct(GwyResourceEditor *res_editor)
{
    static const GwyEnum color_components[] = {
        { N_("Ambient"),  GL_MATERIAL_AMBIENT,  },
        { N_("Diffuse"),  GL_MATERIAL_DIFFUSE,  },
        { N_("Specular"), GL_MATERIAL_SPECULAR, },
        /*{ N_("Emission"), GL_MATERIAL_EMISSION, },*/
    };
    GtkWidget *vbox, *hbox, *buttonbox, *colorsel, *table, *spin;
    GwyGLMaterialEditor *editor;
    GtkObject *adj;
    GSList *group, *l;

    g_return_if_fail(GTK_IS_WINDOW(res_editor->edit_window));
    editor = GWY_GL_MATERIAL_EDITOR(res_editor);

    gtk_container_set_border_width(GTK_CONTAINER(res_editor->edit_window),
                                   4);

    hbox = gtk_hbox_new(FALSE, 12);
    gtk_container_add(GTK_CONTAINER(res_editor->edit_window), hbox);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    buttonbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), buttonbox, FALSE, FALSE, 0);

    group = gwy_radio_buttons_create
                        (color_components, G_N_ELEMENTS(color_components),
                         "color-component",
                         G_CALLBACK(gwy_gl_material_editor_component_cb),
                         editor,
                         editor->last_component);
    for (l = group; l; l = g_slist_next(l)) {
        gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(l->data), FALSE);
        gtk_box_pack_start(GTK_BOX(buttonbox), GTK_WIDGET(l->data),
                           FALSE, FALSE, 0);
    }
    editor->components = group;

    colorsel = editor->colorsel = gtk_color_selection_new();
    gtk_box_pack_start(GTK_BOX(vbox), colorsel, FALSE, FALSE, 0);
    /* XXX */
    gtk_color_selection_set_has_opacity_control(GTK_COLOR_SELECTION(colorsel),
                                                FALSE);
    gtk_color_selection_set_has_palette(GTK_COLOR_SELECTION(colorsel), FALSE);
    g_signal_connect_swapped(colorsel, "color-changed",
                             G_CALLBACK(gwy_resource_editor_queue_commit),
                             res_editor);

    table = gtk_table_new(1, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    adj = gtk_adjustment_new(0.0, 0.0, 1.0, 0.001, 0.1, 0.0);
    editor->shininess = adj;
    spin = gwy_table_attach_hscale(table, 0, _("Shininess:"), NULL,
                                   adj, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(adj, "value-changed",
                             G_CALLBACK(gwy_resource_editor_queue_commit),
                             res_editor);

    gwy_gl_material_editor_preview_new(editor);
    if (editor->preview)
        gtk_box_pack_end(GTK_BOX(hbox), editor->preview, FALSE, FALSE, 0);
    gtk_widget_show_all(hbox);

    /* switch */
    gwy_gl_material_editor_switch(res_editor);
}

static void
gwy_gl_material_editor_preview_new(GwyGLMaterialEditor *editor)
{
    enum { N = 96 };
    GtkWidget *view;
    GwyContainer *container;
    GwyDataField *dfield;
    GtkAdjustment *adj;

    if (!gwy_app_gl_is_ok()) {
        editor->preview = NULL;
        return;
    }

    dfield = gwy_data_field_new(N, N, 1.0, 1.0, FALSE);
    gwy_gl_material_editor_make_data(dfield);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_gl_material_editor_set_view(container, FALSE);

    view = gwy_3d_view_new(container);
    g_object_unref(container);
    g_object_set(view,
                 "movement-type", GWY_3D_MOVEMENT_ROTATION,
                 "visualization", GWY_3D_VISUALIZATION_LIGHTING,
                 "reduced-size", N,
                 "show-axes", FALSE,
                 "show-labels", FALSE,
                 NULL);
    adj = gwy_3d_view_get_view_scale_adjustment(GWY_3D_VIEW(view));
    gtk_adjustment_set_value(adj, 1.4*gtk_adjustment_get_value(adj));
    g_signal_connect_swapped(view, "destroy",
                             G_CALLBACK(gwy_gl_material_editor_save_view),
                             container);

    editor->preview = view;
}

static void
gwy_gl_material_editor_save_view(GwyContainer *container)
{
    gwy_gl_material_editor_set_view(container, TRUE);
}

static void
gwy_gl_material_editor_set_view(GwyContainer *container,
                                gboolean save)
{
    static const gchar *keys[] = { "rot_x", "rot_y" };
    static const gchar *ctemplate = "/0/3d/%s";
    static const gchar *stemplate = "/app/%s/editor/3d/%s";
    GwyContainer *from, *to;
    GwyResourceClass *klass;
    const gchar *name;
    GString *src, *dest;
    gdouble val;
    guint i;

    klass = g_type_class_ref(GWY_TYPE_GL_MATERIAL);
    name = gwy_resource_class_get_name(klass);
    g_type_class_unref(klass);

    from = gwy_app_settings_get();
    to = container;
    if (save)
        GWY_SWAP(GwyContainer*, from, to);

    src = g_string_new("");
    dest = g_string_new("");
    for (i = 0; i < G_N_ELEMENTS(keys); i++) {
        g_string_printf(src, stemplate, name, keys[i]);
        g_string_printf(dest, ctemplate, keys[i]);
        if (save)
            GWY_SWAP(GString*, src, dest);
        if (gwy_container_gis_double_by_name(from, src->str, &val))
            gwy_container_set_double_by_name(to, dest->str, val);
    }

    g_string_free(src, TRUE);
    g_string_free(dest, TRUE);
}

static void
gwy_gl_material_editor_update(GwyGLMaterialEditor *editor)
{
    GwyResourceEditor *res_editor;
    GtkColorSelection *colorsel;
    GwyGLMaterial *material;
    GdkColor gdkcolor;
    const GwyRGBA *color;

    res_editor = GWY_RESOURCE_EDITOR(editor);
    editor->last_component = gwy_radio_buttons_get_current(editor->components,
                                                           "color-component");
    material = GWY_GL_MATERIAL(gwy_resource_editor_get_edited(res_editor));
    g_return_if_fail(material
                     && gwy_resource_get_is_modifiable(GWY_RESOURCE(material)));

    switch (editor->last_component) {
        case GL_MATERIAL_AMBIENT:
        color = gwy_gl_material_get_ambient(material);
        break;

        case GL_MATERIAL_DIFFUSE:
        color = gwy_gl_material_get_diffuse(material);
        break;

        case GL_MATERIAL_SPECULAR:
        color = gwy_gl_material_get_specular(material);
        break;

        case GL_MATERIAL_EMISSION:
        color = gwy_gl_material_get_emission(material);
        break;

        default:
        g_return_if_reached();
        break;
    }
    colorsel = GTK_COLOR_SELECTION(editor->colorsel);
    gwy_rgba_to_gdk_color(color, &gdkcolor);
    g_signal_handlers_block_by_func(colorsel,
                                    &gwy_resource_editor_queue_commit,
                                    editor);
    gtk_color_selection_set_current_color(colorsel, &gdkcolor);
    gwy_rgba_to_gdk_color(&editor->old[editor->last_component], &gdkcolor);
    gtk_color_selection_set_previous_color(colorsel, &gdkcolor);
    g_signal_handlers_unblock_by_func(colorsel,
                                      &gwy_resource_editor_queue_commit,
                                      editor);

    gtk_adjustment_set_value(GTK_ADJUSTMENT(editor->shininess),
                             gwy_gl_material_get_shininess(material));
}

static void
gwy_gl_material_editor_make_data(GwyDataField *dfield1)
{
    GwyDataField *dfield2;
    gint i, j, m, n;
    gdouble *data;
    gdouble x;
    GRand *rng;

    rng = g_rand_new();

    dfield2 = gwy_data_field_new_alike(dfield1, FALSE);
    n = gwy_data_field_get_xres(dfield1);
    m = 3*n/5;

    gwy_data_field_clear(dfield1);
    data = gwy_data_field_get_data(dfield1);
    for (i = 0; i < n*n; i++)
        data[i] = g_rand_double_range(rng, -0.2, 0.2);
    gwy_data_field_filter_median(dfield1, 5);
    gwy_data_field_filter_median(dfield1, 5);
    gwy_data_field_filter_median(dfield1, 5);
    gwy_data_field_filter_median(dfield1, 5);

    data = gwy_data_field_get_data(dfield2);
    for (i = 0; i < n*n; i++)
        data[i] = g_rand_double_range(rng, -0.07, 0.07);
    gwy_data_field_filter_median(dfield2, 7);
    gwy_data_field_sum_fields(dfield1, dfield1, dfield2);

    gwy_data_field_clear(dfield2);
    data = gwy_data_field_get_data(dfield2);
    for (i = 0; i < m; i++) {
        for (j = 0; j < m; j++) {
            x = hypot(i - (m-1)/2.0, j - (m-1)/2.0)/(m-1)*2.0;
            data[n*i + j] = 0.4*sqrt(CLAMP(1.0 - x*x, 0.0, 1.0));
        }
    }
    gwy_data_field_sum_fields(dfield1, dfield1, dfield2);

    gwy_data_field_clear(dfield2);
    gwy_data_field_area_fill(dfield2, 2*n/3, 0, 3*n/4+1, n, 0.15);
    gwy_data_field_area_fill(dfield2, 3*n/4, 0, 5*n/6+1, n, 0.1);
    gwy_data_field_area_fill(dfield2, 5*n/6, 0, 7*n/8+1, n, 0.07);
    gwy_data_field_filter_mean(dfield2, 3);
    gwy_data_field_sum_fields(dfield1, dfield1, dfield2);

    g_rand_free(rng);
}

static void
gwy_gl_material_editor_component_cb(G_GNUC_UNUSED GtkWidget *widget,
                                    GwyGLMaterialEditor *editor)
{
    gwy_resource_editor_commit(GWY_RESOURCE_EDITOR(editor));
    gwy_gl_material_editor_update(editor);
}

static void
gwy_gl_material_editor_apply(GwyResourceEditor *res_editor)
{
    GwyGLMaterialEditor *editor;
    GwyGLMaterial *material;
    GwyRGBA color;
    GdkColor gdkcolor;
    gdouble val;

    material = GWY_GL_MATERIAL(gwy_resource_editor_get_edited(res_editor));
    g_return_if_fail(material
                     && gwy_resource_get_is_modifiable(GWY_RESOURCE(material)));

    editor = GWY_GL_MATERIAL_EDITOR(res_editor);
    gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(editor->colorsel),
                                          &gdkcolor);
    gwy_rgba_from_gdk_color(&color, &gdkcolor);
    color.a = 1.0;    /* FIXME */
    switch (editor->last_component) {
        case GL_MATERIAL_AMBIENT:
        gwy_gl_material_set_ambient(material, &color);
        break;

        case GL_MATERIAL_DIFFUSE:
        gwy_gl_material_set_diffuse(material, &color);
        break;

        case GL_MATERIAL_SPECULAR:
        gwy_gl_material_set_specular(material, &color);
        break;

        case GL_MATERIAL_EMISSION:
        gwy_gl_material_set_emission(material, &color);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    val = gtk_adjustment_get_value(GTK_ADJUSTMENT(editor->shininess));
    gwy_gl_material_set_shininess(material, val);
}

static void
gwy_gl_material_editor_switch(GwyResourceEditor *res_editor)
{
    GwyGLMaterial *material;
    GwyGLMaterialEditor *editor;

    editor = GWY_GL_MATERIAL_EDITOR(res_editor);

    material = GWY_GL_MATERIAL(gwy_resource_editor_get_edited(res_editor));
    g_return_if_fail(material
                     && gwy_resource_get_is_modifiable(GWY_RESOURCE(material)));

    if (gwy_app_gl_is_ok())
        gwy_3d_view_set_material(GWY_3D_VIEW(editor->preview),
                                 gwy_resource_get_name(GWY_RESOURCE(material)));

    editor->old[GL_MATERIAL_AMBIENT] = *gwy_gl_material_get_ambient(material);
    editor->old[GL_MATERIAL_DIFFUSE] = *gwy_gl_material_get_diffuse(material);
    editor->old[GL_MATERIAL_SPECULAR] = *gwy_gl_material_get_specular(material);
    editor->old[GL_MATERIAL_EMISSION] = *gwy_gl_material_get_emission(material);
    gwy_gl_material_editor_update(editor);
}

/************************** Documentation ****************************/

/**
 * SECTION:glmaterial-editor
 * @title: GwyGLMaterialEditor
 * @short_description: OpenGL material editor
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

