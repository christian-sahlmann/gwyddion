/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libprocess/datafield.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwydgets.h"

#define GWY_3D_WINDOW_TYPE_NAME "Gwy3DWindow"

#define CBRT2 1.259921049894873164767210607277
#define DEFAULT_SIZE 360

enum {
    FOO,
    LAST_SIGNAL
};

/* Forward declarations */

static void     gwy_3d_window_class_init        (Gwy3DWindowClass *klass);
static void     gwy_3d_window_init              (Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_finalize          (GObject *object);
static void     gwy_3d_window_set_mode          (gpointer userdata,
                                                 GtkWidget *button);
static void     gwy_3d_window_set_palette       (GtkWidget *item,
                                                 Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_set_material      (GtkWidget *item,
                                                 Gwy3DWindow *gwy3dwindow);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

static guint gwy3dwindow_signals[LAST_SIGNAL] = { 0 };

static const gdouble zoom_factors[] = {
    G_SQRT2,
    CBRT2,
    1.0,
    0.5,
};

GType
gwy_3d_window_get_type(void)
{
    static GType gwy_3d_window_type = 0;

    if (!gwy_3d_window_type) {
        static const GTypeInfo gwy_3d_window_info = {
            sizeof(Gwy3DWindowClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_3d_window_class_init,
            NULL,
            NULL,
            sizeof(Gwy3DWindow),
            0,
            (GInstanceInitFunc)gwy_3d_window_init,
            NULL,
        };
        gwy_debug("");
        gwy_3d_window_type = g_type_register_static(GTK_TYPE_WINDOW,
                                                    GWY_3D_WINDOW_TYPE_NAME,
                                                    &gwy_3d_window_info,
                                                    0);
    }

    return gwy_3d_window_type;
}

static void
gwy_3d_window_class_init(Gwy3DWindowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_3d_window_finalize;

    /*
    gwy3dwindow_signals[TITLE_CHANGED] =
        g_signal_new("title_changed",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(Gwy3DWindowClass, title_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
                     */
}

static void
gwy_3d_window_init(Gwy3DWindow *gwy3dwindow)
{
    gwy_debug("");

    gwy3dwindow->gwy3dview = NULL;
    gwy3dwindow->statusbar = NULL;
    gwy3dwindow->zoom_mode = GWY_ZOOM_MODE_HALFPIX;
    gwy3dwindow->statusbar_context_id = 0;
    gwy3dwindow->statusbar_message_id = 0;
}

static void
gwy_3d_window_finalize(GObject *object)
{
    Gwy3DWindow *gwy3dwindow;

    gwy_debug("finalizing a Gwy3DWindow %p (refcount = %u)",
              object, object->ref_count);

    g_return_if_fail(GWY_IS_3D_WINDOW(object));

    gwy3dwindow = GWY_3D_WINDOW(object);
    /*
    g_free(gwy3dwindow->coord_format);
    g_free(gwy3dwindow->value_format);
    */

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_3d_window_new:
 * @data_view: A #GwyDataView containing the data-displaying widget to show.
 *
 * Creates a new data displaying window.
 *
 * Returns: A newly created widget, as #GtkWidget.
 *
 * Since: 1.5
 **/
GtkWidget*
gwy_3d_window_new(Gwy3DView *gwy3dview)
{
    Gwy3DWindow *gwy3dwindow;
    GwyPalette *palette;
    GwyGLMaterial *material;
    GtkRequisition size_req;
    const gchar *name;
    GtkWidget *vbox, *hbox, *table, *toolbar, *spin, *button, *omenu, *group,
               *label;
    gboolean is_none;
    gint row;

    gwy_debug("");

    gwy3dwindow = (Gwy3DWindow*)g_object_new(GWY_TYPE_3D_WINDOW, NULL);
    gtk_window_set_wmclass(GTK_WINDOW(gwy3dwindow), "data",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(gwy3dwindow), TRUE);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(gwy3dwindow), hbox);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

    gwy3dwindow->gwy3dview = (GtkWidget*)gwy3dview;
    gtk_box_pack_start(GTK_BOX(hbox), gwy3dwindow->gwy3dview, TRUE, TRUE, 0);

    /* Toolbar */
    toolbar = gwy_toolbox_new(4);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                NULL, _("Rotate the data"),
                                NULL, GWY_STOCK_ROTATE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_ROTATION));
    group = button;
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Scale the data"),
                                NULL, GWY_STOCK_SCALE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_SCALE));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Scale value range"),
                                NULL, GWY_STOCK_Z_SCALE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_DEFORMATION));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_RADIO_BUTTON,
                                group, _("Scale value range"),
                                NULL, GWY_STOCK_LIGHT_ROTATE,
                                G_CALLBACK(gwy_3d_window_set_mode),
                                GINT_TO_POINTER(GWY_3D_LIGHT_MOVEMENT));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);

    /* Parameter table */
    table = gtk_table_new(8, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    row = 0;

    label = gtk_label_new(_("Material:"));
    gwy3dwindow->material_label = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    material = gwy_3d_view_get_material(gwy3dview);
    name = gwy_gl_material_get_name(material);
    is_none = !strcmp(name, "None");
    omenu = gwy_option_menu_gl_material(G_CALLBACK(gwy_3d_window_set_material),
                                        gwy3dwindow, name);
    gwy3dwindow->material_menu = omenu;
    gtk_table_attach(GTK_TABLE(table), omenu,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    label = gtk_label_new(_("Palette:"));
    gwy3dwindow->palette_label = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_widget_set_sensitive(label, is_none);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    palette = gwy_3d_view_get_palette(gwy3dview);
    name = gwy_palette_def_get_name(gwy_palette_get_palette_def(palette));
    omenu = gwy_option_menu_palette(G_CALLBACK(gwy_3d_window_set_palette),
                                    gwy3dwindow, name);
    gtk_widget_set_sensitive(omenu, is_none);
    gwy3dwindow->palette_menu = omenu;
    gtk_table_attach(GTK_TABLE(table), omenu,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    /* TODO: meaningful description, don't access 3DView fields directly! */
    spin = gwy_table_attach_spinbutton(table, row++, _("Rot_x"), _("???"),
                                       (GtkObject*)gwy3dview->rot_x);
    spin = gwy_table_attach_spinbutton(table, row++, _("Rot_y"), _("???"),
                                       (GtkObject*)gwy3dview->rot_y);
    spin = gwy_table_attach_spinbutton(table, row++, _("Scale"), "",
                                       (GtkObject*)gwy3dview->view_scale);
    spin = gwy_table_attach_spinbutton(table, row++, _("Value scale"), "",
                                       (GtkObject*)gwy3dview->deformation_z);
    spin = gwy_table_attach_spinbutton(table, row++, _("Light_z"), _("???"),
                                       (GtkObject*)gwy3dview->light_z);
    spin = gwy_table_attach_spinbutton(table, row++, _("Light_y"), _("???"),
                                       (GtkObject*)gwy3dview->light_y);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    button = gwy_stock_like_button_new(_("_Export"), GTK_STOCK_SAVE);
    gtk_table_attach(GTK_TABLE(table), button,
                     0, 1, row, row+1, 0, 0, 2, 2);

    gtk_widget_show_all(hbox);

#if 0
    g_signal_connect(gwy3dwindow, "size-allocate",
                     G_CALLBACK(gwy_3d_window_measure_changed), NULL);
    g_signal_connect(gwy3dwindow, "key-press-event",
                     G_CALLBACK(gwy_3d_window_key_pressed), NULL);
#endif

    /* make the 3D view at least DEFAULT_SIZE x DEFAULT_SIZE */
    gtk_widget_size_request(vbox, &size_req);
    size_req.height = MAX(size_req.height, DEFAULT_SIZE);
    gtk_window_set_default_size(GTK_WINDOW(gwy3dwindow),
                                size_req.width + size_req.height,
                                size_req.height);

    return GTK_WIDGET(gwy3dwindow);
}

/**
 * gwy_3d_window_get_3d_view:
 * @gwy3dwindow: A data view window.
 *
 * Returns the #Gwy3DView widget this 3D window currently shows.
 *
 * Returns: The currently shown #GwyDataView.
 *
 * Since: 1.5
 **/
GtkWidget*
gwy_3d_window_get_3d_view(Gwy3DWindow *gwy3dwindow)
{
    g_return_val_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow), NULL);

    return gwy3dwindow->gwy3dview;
}

static void
gwy_3d_window_set_mode(gpointer userdata, GtkWidget *button)
{
    Gwy3DWindow *gwy3dwindow;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    gwy3dwindow = g_object_get_data(G_OBJECT(button), "gwy3dwindow");
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    gwy_3d_view_set_status(GWY_3D_VIEW(gwy3dwindow->gwy3dview),
                           GPOINTER_TO_INT(userdata));
}

static void
gwy_3d_window_set_palette(GtkWidget *item,
                          Gwy3DWindow *gwy3dwindow)
{
    gchar *palette_name;
    GObject *palette;

    /* FIXME: wouldn't it be better to allow a name in 3DView? */
    palette_name = g_object_get_data(G_OBJECT(item), "palette-name");
    palette = gwy_palette_new(NULL);
    gwy_palette_set_by_name(GWY_PALETTE(palette), palette_name);
    gwy_3d_view_set_palette(GWY_3D_VIEW(gwy3dwindow->gwy3dview),
                            GWY_PALETTE(palette));
    g_object_unref(palette);
}

static void
gwy_3d_window_set_material(GtkWidget *item,
                           Gwy3DWindow *gwy3dwindow)
{
    gchar *material_name;
    GObject *material;
    gboolean is_none;

    material_name = g_object_get_data(G_OBJECT(item), "material-name");
    is_none = !strcmp(material_name, "None");
    material = gwy_gl_material_new(material_name);
    gwy_3d_view_set_material(GWY_3D_VIEW(gwy3dwindow->gwy3dview),
                             GWY_GL_MATERIAL(material));
    g_object_unref(material);

    gtk_widget_set_sensitive(gwy3dwindow->palette_menu, is_none);
    gtk_widget_set_sensitive(gwy3dwindow->palette_label, is_none);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
