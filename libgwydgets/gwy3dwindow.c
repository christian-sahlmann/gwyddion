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

#include "config.h"
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libprocess/datafield.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwydgets.h"

#define DEFAULT_SIZE 360

enum {
    FOO,
    LAST_SIGNAL
};

enum {
    N_BUTTONS = GWY_3D_MOVEMENT_LIGHT + 1
};

/* Forward declarations */

static void     gwy_3d_window_destroy             (GtkObject *object);
static void     gwy_3d_window_finalize            (GObject *object);
static void     gwy_3d_window_pack_buttons        (Gwy3DWindow *gwy3dwindow,
                                                   guint offset,
                                                   GtkBox *box);
static void     gwy_3d_window_set_mode            (gpointer userdata,
                                                   GtkWidget *button);
static void     gwy_3d_window_set_gradient        (GtkTreeSelection *selection,
                                                   Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_set_material        (GtkWidget *item,
                                                   Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_select_controls     (gpointer data,
                                                   GtkWidget *button);
static void     gwy_3d_window_set_labels          (GtkWidget *combo,
                                                   Gwy3DWindow *gwy3dwindow);
static void     gwy_3d_window_projection_changed  (GtkToggleButton *check,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_show_axes_changed   (GtkToggleButton *check,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_show_labels_changed (GtkToggleButton *check,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_display_mode_changed(GtkRadioButton *radio,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_auto_scale_changed  (GtkToggleButton *check,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_labels_entry_activate(GtkEntry *entry,
                                                   Gwy3DWindow *window);
static void     gwy_3d_window_labels_reset_clicked(Gwy3DWindow *window);
static void     gwy_3d_window_set_tooltip         (GtkWidget *widget,
                                                   const gchar *tip_text);

/* Local data */

/* These are actually class data.  To put them to Class struct someone would
 * have to do class_ref() and live with this reference to the end of time. */
static GtkTooltips *tooltips = NULL;
static gboolean tooltips_set = FALSE;

/*static guint gwy3dwindow_signals[LAST_SIGNAL] = { 0 };*/

G_DEFINE_TYPE(Gwy3DWindow, gwy_3d_window, GTK_TYPE_WINDOW)

static void
gwy_3d_window_class_init(Gwy3DWindowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;

    object_class = (GtkObjectClass*)klass;

    gobject_class->finalize = gwy_3d_window_finalize;
    object_class->destroy = gwy_3d_window_destroy;

    /*
    gwy3dwindow_signals[TITLE_CHANGED] =
        g_signal_new("title-changed",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(Gwy3DWindowClass, title_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
                     */
}

static void
gwy_3d_window_init(G_GNUC_UNUSED Gwy3DWindow *gwy3dwindow)
{
    if (!tooltips_set && !tooltips) {
        tooltips = gtk_tooltips_new();
        g_object_ref(tooltips);
        gtk_object_sink(GTK_OBJECT(tooltips));
    }
}

static void
gwy_3d_window_finalize(GObject *object)
{
    Gwy3DWindow *gwy3dwindow;

    gwy3dwindow = GWY_3D_WINDOW(object);

    G_OBJECT_CLASS(gwy_3d_window_parent_class)->finalize(object);
}

static void
gwy_3d_window_destroy(GtkObject *object)
{
    Gwy3DWindow *gwy3dwindow;

    gwy3dwindow = GWY_3D_WINDOW(object);
    g_free(gwy3dwindow->buttons);
    gwy3dwindow->buttons = NULL;

    GTK_OBJECT_CLASS(gwy_3d_window_parent_class)->destroy(object);
}

static void
gwy_3d_window_pack_buttons(Gwy3DWindow *gwy3dwindow,
                           guint offset,
                           GtkBox *box)
{
    static struct {
        Gwy3DMovement mode;
        const gchar *stock_id;
        const gchar *tooltip;
    }
    const buttons[] = {
        {
            GWY_3D_MOVEMENT_ROTATION,
            GWY_STOCK_ROTATE,
            N_("Rotate view")
        },
        {
            GWY_3D_MOVEMENT_SCALE,
            GWY_STOCK_SCALE,
            N_("Scale view as a whole")
        },
        {
            GWY_3D_MOVEMENT_DEFORMATION,
            GWY_STOCK_Z_SCALE,
            N_("Scale value range")
        },
        {
            GWY_3D_MOVEMENT_LIGHT,
            GWY_STOCK_LIGHT_ROTATE,
            N_("Move light source")
        },
    };
    GtkWidget *button;
    GtkRadioButton *group = NULL;
    guint i;

    for (i = 0; i < G_N_ELEMENTS(buttons); i++) {
        button = gtk_radio_button_new_from_widget(group);
        gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
        g_object_set(G_OBJECT(button), "draw-indicator", FALSE, NULL);
        gtk_container_add(GTK_CONTAINER(button),
                          gtk_image_new_from_stock(buttons[i].stock_id,
                                                   GTK_ICON_SIZE_BUTTON));
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(gwy_3d_window_set_mode),
                                 GINT_TO_POINTER(buttons[i].mode));
        g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
        gwy_3d_window_set_tooltip(button, _(buttons[i].tooltip));
        gwy3dwindow->buttons[offset + buttons[i].mode] = button;
        if (!group)
            group = GTK_RADIO_BUTTON(button);
    }
}

/**
 * gwy_3d_window_new:
 * @gwy3dview: A #Gwy3DView containing the data-displaying widget to show.
 *
 * Creates a new OpenGL 3D data displaying window.
 *
 * Returns: A newly created widget, as #GtkWidget.
 **/
GtkWidget*
gwy_3d_window_new(Gwy3DView *gwy3dview)
{
    static const GwyEnum label_entries[] = {
        { N_("X-axis"),          GWY_3D_VIEW_LABEL_X, },
        { N_("Y-axis"),          GWY_3D_VIEW_LABEL_Y, },
        { N_("Minimum z value"), GWY_3D_VIEW_LABEL_MIN, },
        { N_("Maximum z value"), GWY_3D_VIEW_LABEL_MAX, }
    };
    static const GwyEnum display_modes[] = {
        { N_("_Lighting"),    GWY_3D_VISUALIZATION_LIGHTING },
        { N_("P_alette"),     GWY_3D_VISUALIZATION_GRADIENT },
    };
    Gwy3DWindow *gwy3dwindow;
    GwyGLMaterial *material;
    GtkRequisition size_req;
    const gchar *name;
    GtkWidget *vbox, *hbox, *hbox2, *table, *spin, *button, *omenu, *combo,
              *label, *check, *entry;
    GSList *display_mode_group;
    Gwy3DLabel *gwy3dlabel;
    Gwy3DVisualization visual;
    guint row;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_3D_VIEW(gwy3dview), NULL);

    gwy3dwindow = (Gwy3DWindow*)g_object_new(GWY_TYPE_3D_WINDOW, NULL);
    gtk_window_set_wmclass(GTK_WINDOW(gwy3dwindow), "data",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(gwy3dwindow), TRUE);

    gwy3dwindow->buttons = g_new0(GtkWidget*, 2*N_BUTTONS);
    gwy3dwindow->in_update = FALSE;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(gwy3dwindow), hbox);

    gwy3dwindow->gwy3dview = (GtkWidget*)gwy3dview;
    gtk_box_pack_start(GTK_BOX(hbox), gwy3dwindow->gwy3dview, TRUE, TRUE, 0);

    /* Small toolbar */
    gwy3dwindow->vbox_small = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), gwy3dwindow->vbox_small, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(gwy3dwindow->vbox_small), 4);

    button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox_small), button,
                       FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(GWY_STOCK_LESS,
                                               GTK_ICON_SIZE_BUTTON));
    gwy_3d_window_set_tooltip(button, _("Show full controls"));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_3d_window_select_controls),
                             GINT_TO_POINTER(FALSE));

    gwy_3d_window_pack_buttons(gwy3dwindow, 0,
                               GTK_BOX(gwy3dwindow->vbox_small));

    /* Large toolbar */
    gwy3dwindow->vbox_large = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), gwy3dwindow->vbox_large, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(gwy3dwindow->vbox_large), 4);
    gtk_widget_set_no_show_all(gwy3dwindow->vbox_large, TRUE);

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox_large), hbox2,
                       FALSE, FALSE, 0);

    button = gtk_button_new();
    gtk_box_pack_end(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(GWY_STOCK_MORE,
                                               GTK_ICON_SIZE_BUTTON));
    gwy_3d_window_set_tooltip(button, _("Hide full controls"));
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_3d_window_select_controls),
                             GINT_TO_POINTER(TRUE));

    gwy_3d_window_pack_buttons(gwy3dwindow, N_BUTTONS, GTK_BOX(hbox2));

    gwy3dwindow->notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox_large), gwy3dwindow->notebook,
                       TRUE, TRUE, 0);

    /* Basic table */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(gwy3dwindow->notebook),
                             vbox, gtk_label_new(_("Basic")));

    table = gtk_table_new(7, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    row = 0;

    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Phi:"), _("deg"),
                (GtkObject*)gwy_3d_view_get_rot_x_adjustment(gwy3dview));
    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Theta:"), _("deg"),
                (GtkObject*)gwy_3d_view_get_rot_y_adjustment(gwy3dview));
    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Scale:"), NULL,
                (GtkObject*)gwy_3d_view_get_view_scale_adjustment(gwy3dview));
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Value scale:"), NULL,
                (GtkObject*)gwy_3d_view_get_z_deformation_adjustment(gwy3dview));
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);

    check = gtk_check_button_new_with_mnemonic(_("Show _axes"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 gwy_3d_view_get_show_axes(gwy3dview));
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_show_axes_changed), gwy3dwindow);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Show _labels"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 gwy_3d_view_get_show_labels(gwy3dview));
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_show_labels_changed),
                     gwy3dwindow);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("_Ortographic projection"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 !gwy_3d_view_get_projection(gwy3dview));
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_projection_changed),
                     gwy3dwindow);
    row++;

    /* Light & Material table */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(gwy3dwindow->notebook),
                             vbox, gtk_label_new(_("Light & Material")));

    table = gtk_table_new(4, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    row = 0;

    visual = gwy_3d_view_get_visualization(gwy3dview);
    display_mode_group
        = gwy_radio_buttons_create(display_modes, G_N_ELEMENTS(display_modes),
                                   "display-mode",
                                   G_CALLBACK(gwy_3d_window_display_mode_changed),
                                   gwy3dwindow,
                                   visual);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(display_mode_group->data),
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Material:"));
    gwy3dwindow->material_label = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_widget_set_sensitive(label, visual);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    material = gwy_3d_view_get_material(gwy3dview);
    name = gwy_gl_material_get_name(material);
    omenu = gwy_option_menu_gl_material(G_CALLBACK(gwy_3d_window_set_material),
                                        gwy3dwindow, name);
    gwy3dwindow->material_menu = omenu;
    gtk_widget_set_sensitive(omenu, visual);
    gtk_table_attach(GTK_TABLE(table), omenu,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    spin = gwy_table_attach_spinbutton
               (table, row++, _("Light _phi:"), _("deg"),
                (GtkObject*)gwy_3d_view_get_light_z_adjustment(gwy3dview));
    gwy3dwindow->lights_spin1 = spin;
    gtk_widget_set_sensitive(spin, visual);

    spin = gwy_table_attach_spinbutton
               (table, row++, _("Light _theta:"), _("deg"),
                (GtkObject*)gwy_3d_view_get_light_y_adjustment(gwy3dview));
    gwy3dwindow->lights_spin2 = spin;
    gtk_widget_set_sensitive(spin, visual);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 12);
    gtk_widget_set_sensitive(gwy3dwindow->buttons[GWY_3D_MOVEMENT_LIGHT],
                             visual);
    gtk_widget_set_sensitive(gwy3dwindow->buttons[N_BUTTONS
                                                  + GWY_3D_MOVEMENT_LIGHT],
                             visual);
    row++;

    gtk_table_attach(GTK_TABLE(table),
                     GTK_WIDGET(display_mode_group->next->data),
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    name = gwy_3d_view_get_gradient(gwy3dview);
    omenu = gwy_gradient_selection_new(G_CALLBACK(gwy_3d_window_set_gradient),
                                       gwy3dwindow, name);
    gtk_widget_set_sensitive(omenu, !visual);
    gwy3dwindow->gradient_menu = omenu;
    gtk_table_attach(GTK_TABLE(table), omenu,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /* Labels table */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(gwy3dwindow->notebook),
                             vbox, gtk_label_new(_("Labels")));

    table = gtk_table_new(8, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    row = 0;

    combo = gwy_enum_combo_box_new(label_entries, G_N_ELEMENTS(label_entries),
                                   G_CALLBACK(gwy_3d_window_set_labels),
                                   gwy3dwindow, -1, TRUE);
    gwy_table_attach_row(table, row, _("_Label:"), NULL, combo);
    gwy3dwindow->labels_menu = combo;
    row++;


    label = gtk_label_new_with_mnemonic(_("_Text:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);

    gwy3dlabel = gwy_3d_view_get_label(gwy3dview, GWY_3D_VIEW_LABEL_X);
    entry = gtk_entry_new_with_max_length(100);
    g_signal_connect (entry, "activate",
                      G_CALLBACK(gwy_3d_window_labels_entry_activate),
                      (gpointer)gwy3dwindow);
    gtk_entry_set_text(GTK_ENTRY(entry), gwy_3d_label_get_text(gwy3dlabel));
    gtk_editable_select_region(GTK_EDITABLE(entry),
                               0, GTK_ENTRY(entry)->text_length);

    gtk_table_attach(GTK_TABLE(table), entry,
                     1, 3, row, row+1, GTK_FILL, 0, 2, 2);
    gwy3dwindow->labels_text = entry;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new(_("Move label"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);
    row++;

    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Horizontally:"), "px",
                GTK_OBJECT(gwy_3d_label_get_delta_x_adjustment(gwy3dlabel)));
    gwy3dwindow->labels_delta_x = spin;
    row++;

    spin = gwy_table_attach_spinbutton
               (table, row++, _("_Vertically:"), "px",
                GTK_OBJECT(gwy_3d_label_get_delta_y_adjustment(gwy3dlabel)));
    gwy3dwindow->labels_delta_y = spin;
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    check = gtk_check_button_new_with_mnemonic(_("Scale size _automatically"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
                                 !gwy_3d_label_get_fixed_size(gwy3dlabel));
    gtk_table_attach(GTK_TABLE(table), check,
                     0, 3, row, row+1, GTK_FILL, 0, 2, 2);
    g_signal_connect(check, "toggled",
                     G_CALLBACK(gwy_3d_window_auto_scale_changed), gwy3dwindow);
    gwy3dwindow->labels_autosize_check = check;
    row++;

    spin = gwy_table_attach_spinbutton
               (table, row++, _("Si_ze:"), _("pixels"),
                GTK_OBJECT(gwy_3d_label_get_size_adjustment(gwy3dlabel)));
    gtk_widget_set_sensitive(spin, gwy_3d_label_get_fixed_size(gwy3dlabel));
    gwy3dwindow->labels_size = spin;
    row++;

    button = gtk_button_new_with_mnemonic(_("_Reset"));
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_3d_window_labels_reset_clicked),
                             gwy3dwindow);
    gtk_table_attach(GTK_TABLE(table), button,
                     0, 1, row, row+1, GTK_FILL, 0, 2, 2);
    gwy3dwindow->actions = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox_large), gwy3dwindow->actions,
                       FALSE, FALSE, 0);

    gtk_widget_show_all(hbox);

    /* make the 3D view at least DEFAULT_SIZE x DEFAULT_SIZE */
    gtk_widget_size_request(gwy3dwindow->vbox_large, &size_req);
    size_req.height = MAX(size_req.height, DEFAULT_SIZE);
    gtk_window_set_default_size(GTK_WINDOW(gwy3dwindow),
                                size_req.width/2 + size_req.height,
                                size_req.height);

    return GTK_WIDGET(gwy3dwindow);
}

/**
 * gwy_3d_window_get_3d_view:
 * @gwy3dwindow: A 3D data view window.
 *
 * Returns the #Gwy3DView widget this 3D window currently shows.
 *
 * Returns: The currently shown #GwyDataView.
 **/
GtkWidget*
gwy_3d_window_get_3d_view(Gwy3DWindow *gwy3dwindow)
{
    g_return_val_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow), NULL);

    return gwy3dwindow->gwy3dview;
}

/**
 * gwy_3d_window_add_action_widget:
 * @gwy3dwindow: A 3D data view window.
 * @widget: A widget to pack into the action area.
 *
 * Adds a widget (usually a button) to 3D window action area.
 *
 * The action area is located under the parameter notebook.
 **/
void
gwy_3d_window_add_action_widget(Gwy3DWindow *gwy3dwindow,
                                GtkWidget *widget)
{
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    g_return_if_fail(GTK_IS_WIDGET(widget));

    gtk_box_pack_start(GTK_BOX(gwy3dwindow->actions), widget,
                       FALSE, FALSE, 0);
}

/**
 * gwy_3d_window_add_small_toolbar_button:
 * @gwy3dwindow: A 3D data view window.
 * @stock_id: Button pixmap stock id, like #GTK_STOCK_SAVE.
 * @tooltip: Button tooltip.
 * @callback: Callback action for "clicked" signal.  It is connected swapped,
 *            that is it gets @cbdata as its first argument, the clicked button
 *            as the last.
 * @cbdata: Data to pass to @callback.
 *
 * Adds a button to small @gwy3dwindow toolbar.
 *
 * The small toolbar is those visible when full controls are hidden.  Due to
 * space constraints the button must be contain only a pixmap.
 **/
void
gwy_3d_window_add_small_toolbar_button(Gwy3DWindow *gwy3dwindow,
                                       const gchar *stock_id,
                                       const gchar *tooltip,
                                       GCallback callback,
                                       gpointer cbdata)
{
    GtkWidget *button;

    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    g_return_if_fail(stock_id);

    button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(gwy3dwindow->vbox_small), button,
                       FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON));
    gwy_3d_window_set_tooltip(button, tooltip);
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    g_signal_connect_swapped(button, "clicked", G_CALLBACK(callback), cbdata);
}

/**
 * gwy_3d_window_class_set_tooltips:
 * @tips: Tooltips object #Gwy3DWindow's should use for setting tooltips.
 *        A %NULL value disables tooltips altogether.
 *
 * Sets the tooltips object to use for adding tooltips to 3D window parts.
 *
 * This is a class method.  It affects only newly cerated 3D windows, existing
 * 3D windows will continue to use the tooltips they were constructed with.
 *
 * If no class tooltips object is set before first #Gwy3DWindow is created,
 * the class instantiates one on its own.  You can normally obtain it with
 * gwy_3d_window_class_get_tooltips() then.  The class takes a reference on
 * the tooltips in either case.
 **/
void
gwy_3d_window_class_set_tooltips(GtkTooltips *tips)
{
    g_return_if_fail(!tips || GTK_IS_TOOLTIPS(tips));

    if (tips) {
        g_object_ref(tips);
        gtk_object_sink(GTK_OBJECT(tips));
    }
    gwy_object_unref(tooltips);
    tooltips = tips;
    tooltips_set = TRUE;
}

/**
 * gwy_3d_window_class_get_tooltips:
 *
 * Gets the tooltips object used for adding tooltips to 3D window parts.
 *
 * Returns: The #GtkTooltips object.
 **/
GtkTooltips*
gwy_3d_window_class_get_tooltips(void)
{
    return tooltips;
}

static void
gwy_3d_window_set_mode(gpointer userdata, GtkWidget *button)
{
    Gwy3DWindow *gwy3dwindow;
    Gwy3DMovement movement;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    gwy3dwindow = (Gwy3DWindow*)g_object_get_data(G_OBJECT(button),
                                                  "gwy3dwindow");
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));
    if (gwy3dwindow->in_update)
        return;

    gwy3dwindow->in_update = TRUE;
    movement = GPOINTER_TO_INT(userdata);
    gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[movement]), TRUE);
    gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON(gwy3dwindow->buttons[movement + N_BUTTONS]), TRUE);

    gwy_3d_view_set_movement_type(GWY_3D_VIEW(gwy3dwindow->gwy3dview),
                                  movement);
    gwy3dwindow->in_update = FALSE;
}

static void
gwy_3d_window_set_gradient(GtkTreeSelection *selection,
                           Gwy3DWindow *gwy3dwindow)
{
    GwyResource *resource;
    GtkTreeModel *model;
    GtkTreeIter iter;

    gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_tree_model_get(model, &iter, 0, &resource, -1);
    gwy_3d_view_set_gradient(GWY_3D_VIEW(gwy3dwindow->gwy3dview),
                             gwy_resource_get_name(resource));
}

static void
gwy_3d_window_set_material(GtkWidget *item,
                           Gwy3DWindow *gwy3dwindow)
{
    gchar *material_name;
    GwyGLMaterial *material;

    material_name = g_object_get_data(G_OBJECT(item), "material-name");
    material = gwy_gl_material_get_by_name(material_name);
    gwy_3d_view_set_material(GWY_3D_VIEW(gwy3dwindow->gwy3dview), material);
}

static void
gwy_3d_window_select_controls(gpointer data, GtkWidget *button)
{
    Gwy3DWindow *gwy3dwindow;
    GtkWidget *show, *hide;

    gwy3dwindow = (Gwy3DWindow*)g_object_get_data(G_OBJECT(button),
                                                  "gwy3dwindow");
    g_return_if_fail(GWY_IS_3D_WINDOW(gwy3dwindow));

    show = data ? gwy3dwindow->vbox_small : gwy3dwindow->vbox_large;
    hide = data ? gwy3dwindow->vbox_large : gwy3dwindow->vbox_small;
    gtk_widget_hide(hide);
    gtk_widget_set_no_show_all(hide, TRUE);
    gtk_widget_set_no_show_all(show, FALSE);
    gtk_widget_show_all(show);
}

static void
gwy_3d_window_set_labels(GtkWidget *combo,
                         Gwy3DWindow *gwy3dwindow)
{
    gint id;
    Gwy3DLabel *label;

    id = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    label = gwy_3d_view_get_label(GWY_3D_VIEW(gwy3dwindow->gwy3dview), id);
    g_return_if_fail(label);

    gtk_entry_set_text(GTK_ENTRY(gwy3dwindow->labels_text),
                       gwy_3d_label_get_text(label));
    gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(gwy3dwindow->labels_delta_x),
                                   gwy_3d_label_get_delta_x_adjustment(label));
    gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(gwy3dwindow->labels_delta_y),
                                   gwy_3d_label_get_delta_y_adjustment(label));
    gtk_spin_button_set_adjustment(GTK_SPIN_BUTTON(gwy3dwindow->labels_size),
                                   gwy_3d_label_get_size_adjustment(label));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gwy3dwindow->labels_delta_x),
                              gwy_3d_label_get_delta_x_adjustment(label)
                              ->value);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gwy3dwindow->labels_delta_y),
                              gwy_3d_label_get_delta_y_adjustment(label)
                              ->value);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gwy3dwindow->labels_size),
                              gwy_3d_label_get_size_adjustment(label)->value);
    gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON(gwy3dwindow->labels_autosize_check),
         !gwy_3d_label_get_fixed_size(label));
}

static void
gwy_3d_window_projection_changed(GtkToggleButton *check,
                                 Gwy3DWindow *window)
{
    gwy_3d_view_set_projection(GWY_3D_VIEW(window->gwy3dview),
                               gtk_toggle_button_get_active(check)
                               ? GWY_3D_PROJECTION_ORTHOGRAPHIC
                               : GWY_3D_PROJECTION_PERSPECTIVE);
}

static void
gwy_3d_window_show_axes_changed(GtkToggleButton *check,
                                Gwy3DWindow *window)
{
    gwy_3d_view_set_show_axes(GWY_3D_VIEW(window->gwy3dview),
                              gtk_toggle_button_get_active(check));
}

static void
gwy_3d_window_show_labels_changed(GtkToggleButton *check,
                                  Gwy3DWindow *window)
{
    gwy_3d_view_set_show_labels(GWY_3D_VIEW(window->gwy3dview),
                                gtk_toggle_button_get_active(check));
}

static void
gwy_3d_window_display_mode_changed(GtkRadioButton *radio,
                                   Gwy3DWindow *window)
{
    Gwy3DVisualization visual;
    GwyGLMaterial *material;
    GtkWidget *menu, *item;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio)))
        return;

    visual
        = gwy_radio_buttons_get_current(gtk_radio_button_get_group(radio),
                                        "display-mode");
    gwy_3d_view_set_visualization(GWY_3D_VIEW(window->gwy3dview), visual);
    gtk_widget_set_sensitive(window->material_menu, visual);
    gtk_widget_set_sensitive(window->material_label, visual);
    gtk_widget_set_sensitive(window->gradient_menu, !visual);
    gtk_widget_set_sensitive(window->lights_spin1, visual);
    gtk_widget_set_sensitive(window->lights_spin2, visual);
    gtk_widget_set_sensitive(window->buttons[GWY_3D_MOVEMENT_LIGHT], visual);
    gtk_widget_set_sensitive(window->buttons[N_BUTTONS + GWY_3D_MOVEMENT_LIGHT],
                             visual);
    if (visual) {
        material = gwy_3d_view_get_material(GWY_3D_VIEW(window->gwy3dview));
        /* FIXME: A hack. Maybe should GLMaterial set default different from
         * None? */
        if (strcmp(gwy_gl_material_get_name(material), GWY_GL_MATERIAL_NONE)
            == 0) {
            menu = gtk_option_menu_get_menu
                                 (GTK_OPTION_MENU(window->material_menu));
            item = gtk_menu_get_active(GTK_MENU(menu));
            gwy_3d_window_set_material(item, window);
        }
    }
}

static void
gwy_3d_window_auto_scale_changed(GtkToggleButton *check,
                                 Gwy3DWindow *window)
{
    Gwy3DLabel *label;
    gboolean active;
    gint id;

    active = gtk_toggle_button_get_active(check);
    gtk_widget_set_sensitive(window->labels_size, !active);

    id = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(window->labels_menu));
    label = gwy_3d_view_get_label(GWY_3D_VIEW(window->gwy3dview), id);
    gwy_3d_label_set_fixed_size(label, !active);
}

static void
gwy_3d_window_labels_entry_activate(GtkEntry *entry,
                                    Gwy3DWindow *window)
{
    Gwy3DLabel *label;
    gint id;

    id = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(window->labels_menu));
    label = gwy_3d_view_get_label(GWY_3D_VIEW(window->gwy3dview), id);
    gwy_3d_label_set_text(label, gtk_entry_get_text(entry));
}

static void
gwy_3d_window_labels_reset_clicked(Gwy3DWindow *window)
{
    Gwy3DLabel *label;
    gint id;

    id = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(window->labels_menu));
    label = gwy_3d_view_get_label(GWY_3D_VIEW(window->gwy3dview), id);
    gwy_3d_label_reset(label);

     gtk_toggle_button_set_active
         (GTK_TOGGLE_BUTTON(window->labels_autosize_check), TRUE);
}

static void
gwy_3d_window_set_tooltip(GtkWidget *widget,
                          const gchar *tip_text)
{
    if (tooltips)
        gtk_tooltips_set_tip(tooltips, widget, tip_text, NULL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
