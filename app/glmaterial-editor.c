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
#include <app/glmaterial-editor.h>

enum {
    GL_MATERIAL_AMBIENT,
    GL_MATERIAL_DIFFUSE,
    GL_MATERIAL_SPECULAR,
    GL_MATERIAL_EMISSION
};

/* For late objectzation... */
typedef struct {
    GtkWidget *window;
    GtkWidget *treeview;
    GtkWidget *button_edit;
    GtkWidget *button_new;
    GtkWidget *button_delete;
    GtkWidget *button_default;
    GString *active;

    GtkWidget *edit_window;
    GSList *components;
    GtkWidget *colorsel;
    GtkObject *shininess;
    GtkWidget *preview;
} GwyGLMaterialEditor;

static void gwy_gl_material_editor_changed     (GtkTreeSelection *selection,
                                                GwyGLMaterialEditor *editor);
static void gwy_gl_material_editor_destroy     (GwyGLMaterialEditor *editor);
static void gwy_gl_material_editor_set_default (GwyGLMaterialEditor *editor);
static void gwy_gl_material_editor_edit        (GwyGLMaterialEditor *editor);
static void gwy_gl_material_editor_construct   (GwyGLMaterialEditor *editor);
static void gwy_gl_material_editor_preview_new (GwyGLMaterialEditor *editor);
static void gwy_gl_material_editor_make_data   (GwyDataField *dfield1);
static void gwy_gl_material_editor_closed      (GwyGLMaterialEditor *editor);
static void gwy_gl_material_editor_update      (GwyGLMaterialEditor *editor);
static void gwy_gl_material_editor_component_cb(GtkWidget *widget,
                                                GwyGLMaterialEditor *editor);
static void gwy_gl_material_editor_color_cb    (GtkWidget *widget,
                                                GwyGLMaterialEditor *editor);
static void gwy_gl_material_editor_shininess_cb(GtkWidget *widget,
                                                GwyGLMaterialEditor *editor);

void
gwy_app_gl_material_editor(void)
{
    static GwyGLMaterialEditor *editor = NULL;
    GtkWidget *treeview, *scwin, *vbox, *toolbox, *button;

    if (!editor) {
        editor = g_new0(GwyGLMaterialEditor, 1);
        editor->active = g_string_new("");
    }
    else if (editor->window) {
        gtk_window_present(GTK_WINDOW(editor->window));
        return;
    }

    /* Pop up */
    editor->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(editor->window), _("GL Material Editor"));
    gtk_window_set_default_size(GTK_WINDOW(editor->window), -1, 420);
    g_signal_connect_swapped(editor->window, "destroy",
                             G_CALLBACK(gwy_gl_material_editor_destroy), editor);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(editor->window), vbox);

    /* GL Materials */
    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), scwin, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    treeview
        = gwy_gl_material_tree_view_new(G_CALLBACK(gwy_gl_material_editor_changed),
                                        editor, editor->active->str);
    editor->treeview = treeview;
    gtk_container_add(GTK_CONTAINER(scwin), treeview);

    /* Controls */
    toolbox = gtk_hbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(toolbox), 2);
    gtk_box_pack_start(GTK_BOX(vbox), toolbox, FALSE, FALSE, 0);

    button = gtk_button_new_from_stock(GTK_STOCK_EDIT);
    editor->button_edit = button;
    gtk_box_pack_start(GTK_BOX(toolbox), button, TRUE, TRUE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_gl_material_editor_edit),
                             editor);

    button = gtk_button_new_from_stock(GTK_STOCK_NEW);
    editor->button_new = button;
    gtk_box_pack_start(GTK_BOX(toolbox), button, TRUE, TRUE, 0);

    button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
    editor->button_delete = button;
    gtk_box_pack_start(GTK_BOX(toolbox), button, TRUE, TRUE, 0);

    button = gtk_button_new_with_mnemonic(_("Set De_fault"));
    editor->button_default = button;
    gtk_box_pack_start(GTK_BOX(toolbox), button, TRUE, TRUE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_gl_material_editor_set_default),
                             editor);

    gtk_widget_show_all(vbox);
    gtk_window_present(GTK_WINDOW(editor->window));
}

static void
gwy_gl_material_editor_changed(GtkTreeSelection *selection,
                               GwyGLMaterialEditor *editor)
{
    GwyResource *resource;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean is_modifiable;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_widget_set_sensitive(editor->button_edit, FALSE);
        gtk_widget_set_sensitive(editor->button_delete, FALSE);
        gtk_widget_set_sensitive(editor->button_default, FALSE);
        return;
    }

    gtk_tree_model_get(model, &iter, 0, &resource, -1);
    g_string_assign(editor->active, gwy_resource_get_name(resource));

    gtk_widget_set_sensitive(editor->button_default, TRUE);
    is_modifiable = gwy_resource_get_is_modifiable(resource);
    gtk_widget_set_sensitive(editor->button_edit, is_modifiable);
    gtk_widget_set_sensitive(editor->button_delete, is_modifiable);
}

static void
gwy_gl_material_editor_destroy(GwyGLMaterialEditor *editor)
{
    GString *s = editor->active;

    memset(editor, 0, sizeof(GwyGLMaterialEditor));
    editor->active = s;
}

static void
gwy_gl_material_editor_set_default(GwyGLMaterialEditor *editor)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    GwyResource *resource;
    GwyInventory *inventory;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(editor->treeview));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        g_warning("Something should be selected for `Set Default'");
        return;
    }

    gtk_tree_model_get(model, &iter, 0, &resource, -1);
    inventory = gwy_inventory_store_get_inventory(GWY_INVENTORY_STORE(model));
    gwy_inventory_set_default_item_name(inventory,
                                        gwy_resource_get_name(resource));
}

static void
gwy_gl_material_editor_edit(GwyGLMaterialEditor *editor)
{
    if (!editor->edit_window) {
        gwy_gl_material_editor_construct(editor);
    }
    gwy_3d_view_set_material(GWY_3D_VIEW(editor->preview), editor->active->str);
    gwy_gl_material_editor_update(editor);
    gtk_window_present(GTK_WINDOW(editor->edit_window));
}

static void
gwy_gl_material_editor_construct(GwyGLMaterialEditor *editor)
{
    static const GwyEnum color_components[] = {
        { N_("Ambient"),  GL_MATERIAL_AMBIENT,  },
        { N_("Diffuse"),  GL_MATERIAL_DIFFUSE,  },
        { N_("Specular"), GL_MATERIAL_SPECULAR, },
        { N_("Emission"), GL_MATERIAL_EMISSION, },
    };
    GtkWidget *vbox, *hbox, *buttonbox, *colorsel, *table, *spin;
    GtkObject *adj;
    GSList *group, *l;

    g_return_if_fail(editor->edit_window == NULL);

    /* Popup color edit window */
    editor->edit_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(editor->edit_window), 4);
    gtk_window_set_title(GTK_WINDOW(editor->edit_window),
                            _("GL Material Editor"));
    g_signal_connect_swapped(editor->edit_window, "destroy",
                             G_CALLBACK(gwy_gl_material_editor_closed), editor);

    hbox = gtk_hbox_new(FALSE, 12);
    gtk_container_add(GTK_CONTAINER(editor->edit_window), hbox);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    buttonbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), buttonbox, FALSE, FALSE, 0);

    group = gwy_radio_buttons_create
                        (color_components, G_N_ELEMENTS(color_components),
                         "color-component",
                         G_CALLBACK(gwy_gl_material_editor_component_cb),
                         editor,
                         GL_MATERIAL_AMBIENT);
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
    g_signal_connect(colorsel, "color-changed",
                     G_CALLBACK(gwy_gl_material_editor_color_cb), editor);

    table = gtk_table_new(1, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    adj = gtk_adjustment_new(0.0, 0.0, 1.0, 0.001, 0.1, 0.0);
    g_signal_connect(adj, "changed",
                     G_CALLBACK(gwy_gl_material_editor_shininess_cb),
                     editor);
    spin = gwy_table_attach_hscale(table, 0, _("Shininess:"), NULL, adj,
                                   GWY_HSCALE_DEFAULT);
    editor->shininess = adj;

    gwy_gl_material_editor_preview_new(editor);
    gtk_box_pack_end(GTK_BOX(hbox), editor->preview, FALSE, FALSE, 0);
    gtk_widget_show_all(hbox);
}

static void
gwy_gl_material_editor_preview_new(GwyGLMaterialEditor *editor)
{
    enum { N = 96 };
    GtkWidget *view;
    GwyContainer *container;
    GwyDataField *dfield;
    GtkAdjustment *adj;

    dfield = gwy_data_field_new(N, N, 1.0, 1.0, FALSE);
    gwy_gl_material_editor_make_data(dfield);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

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

    editor->preview = view;
}

static void
gwy_gl_material_editor_update(GwyGLMaterialEditor *editor)
{
    GwyGLMaterial *material;
    GdkColor gdkcolor;
    const GwyRGBA *color;
    gint component;

    material = gwy_inventory_get_item(gwy_gl_materials(), editor->active->str);
    if (!material) {
        g_warning("Editing non-existent material.  "
                  "Either make it impossible, or implement some reasonable "
                  "behaviour.");
        return;
    }
    component = gwy_radio_buttons_get_current(editor->components,
                                              "color-component");
    switch (component) {
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
    gwy_rgba_to_gdk_color(color, &gdkcolor);
    gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(editor->colorsel),
                                          &gdkcolor);

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
gwy_gl_material_editor_component_cb(GtkWidget *widget,
                                    GwyGLMaterialEditor *editor)
{
    gwy_gl_material_editor_update(editor);
}

static void
gwy_gl_material_editor_color_cb(GtkWidget *widget,
                                GwyGLMaterialEditor *editor)
{
    GwyGLMaterial *material;
    GwyRGBA color;
    GdkColor gdkcolor;
    gint component;

    material = gwy_inventory_get_item(gwy_gl_materials(), editor->active->str);
    if (!material || !gwy_resource_get_is_modifiable(GWY_RESOURCE(material))) {
        g_warning("Current material is nonexistent/unmodifiable");
        return;
    }

    component = gwy_radio_buttons_get_current(editor->components,
                                              "color-component");
    gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(editor->colorsel),
                                          &gdkcolor);
    gwy_rgba_from_gdk_color(&color, &gdkcolor);
    color.a = 1.0;    /* FIXME */
    switch (component) {
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
        g_return_if_reached();
        break;
    }
}

static void
gwy_gl_material_editor_shininess_cb(GtkWidget *widget,
                                    GwyGLMaterialEditor *editor)
{
    GwyGLMaterial *material;
    gdouble val;

    material = gwy_inventory_get_item(gwy_gl_materials(), editor->active->str);
    if (!material || !gwy_resource_get_is_modifiable(GWY_RESOURCE(material))) {
        g_warning("Current material is nonexistent/unmodifiable");
        return;
    }
    val = gtk_adjustment_get_value(GTK_ADJUSTMENT(editor->shininess));
    gwy_gl_material_set_shininess(material, val);
}

static void
gwy_gl_material_editor_closed(GwyGLMaterialEditor *editor)
{
    editor->edit_window = NULL;
    editor->components = NULL;
    editor->colorsel = NULL;
    editor->preview = NULL;
    editor->shininess = NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

