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

/* Based on gtkmenubar.c (just with everything rotated pi/2) bearing following
 * copyright notice: */

/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkbindings.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtksettings.h>
#include <gtk/gtkwindow.h>

#include "gwyvmenubar.h"

#define BORDER_SPACING  0

#define GWY_VMENU_BAR_TYPE_NAME "GwyVMenuBar"

static void gwy_vmenu_bar_class_init        (GwyVMenuBarClass *klass);
static void gwy_vmenu_bar_size_request      (GtkWidget       *widget,
                                             GtkRequisition  *requisition);
static void gwy_vmenu_bar_size_allocate     (GtkWidget       *widget,
                                             GtkAllocation   *allocation);
static void gwy_vmenu_bar_paint             (GtkWidget       *widget,
                                             GdkRectangle    *area);
static gint gwy_vmenu_bar_expose            (GtkWidget       *widget,
                                             GdkEventExpose  *event);

static GtkShadowType get_shadow_type        (GtkMenuBar      *menubar);

static GtkMenuShellClass *parent_class = NULL;

GType
gwy_vmenu_bar_get_type(void)
{
    static GType gwy_menu_bar_type = 0;

    if (!gwy_menu_bar_type) {
        static const GTypeInfo gwy_menu_bar_info = {
            sizeof(GwyVMenuBarClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_vmenu_bar_class_init,
            NULL,
            NULL,
            sizeof(GwyVMenuBar),
            0,
            NULL,
            NULL,
        };

        gwy_menu_bar_type = g_type_register_static(GTK_TYPE_MENU_BAR,
                                                   GWY_VMENU_BAR_TYPE_NAME,
                                                   &gwy_menu_bar_info, 0);
    }

    return gwy_menu_bar_type;
}

static void
gwy_vmenu_bar_class_init(GwyVMenuBarClass *klass)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;
    GtkMenuShellClass *menu_shell_class;

    GtkBindingSet *binding_set;

    parent_class = g_type_class_peek_parent(klass);

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;
    menu_shell_class = (GtkMenuShellClass*)klass;

    widget_class->size_request = gwy_vmenu_bar_size_request;
    widget_class->size_allocate = gwy_vmenu_bar_size_allocate;
    widget_class->expose_event = gwy_vmenu_bar_expose;

    menu_shell_class->submenu_placement = GTK_LEFT_RIGHT;

    binding_set = gtk_binding_set_by_class(klass);
    gtk_binding_entry_add_signal(binding_set,
                                 GDK_Left, 0,
                                 "move_current", 1,
                                 GTK_TYPE_MENU_DIRECTION_TYPE,
                                 GTK_MENU_DIR_PARENT);
    gtk_binding_entry_add_signal(binding_set,
                                 GDK_KP_Left, 0,
                                 "move_current", 1,
                                 GTK_TYPE_MENU_DIRECTION_TYPE,
                                 GTK_MENU_DIR_PARENT);
    gtk_binding_entry_add_signal(binding_set,
                                 GDK_Right, 0,
                                 "move_current", 1,
                                 GTK_TYPE_MENU_DIRECTION_TYPE,
                                 GTK_MENU_DIR_CHILD);
    gtk_binding_entry_add_signal(binding_set,
                                 GDK_KP_Right, 0,
                                 "move_current", 1,
                                 GTK_TYPE_MENU_DIRECTION_TYPE,
                                 GTK_MENU_DIR_CHILD);
    gtk_binding_entry_add_signal(binding_set,
                                 GDK_Up, 0,
                                 "move_current", 1,
                                 GTK_TYPE_MENU_DIRECTION_TYPE,
                                 GTK_MENU_DIR_PREV);
    gtk_binding_entry_add_signal(binding_set,
                                 GDK_KP_Up, 0,
                                 "move_current", 1,
                                 GTK_TYPE_MENU_DIRECTION_TYPE,
                                 GTK_MENU_DIR_PREV);
    gtk_binding_entry_add_signal(binding_set,
                                 GDK_Down, 0,
                                 "move_current", 1,
                                 GTK_TYPE_MENU_DIRECTION_TYPE,
                                 GTK_MENU_DIR_NEXT);
    gtk_binding_entry_add_signal(binding_set,
                                 GDK_KP_Down, 0,
                                 "move_current", 1,
                                 GTK_TYPE_MENU_DIRECTION_TYPE,
                                 GTK_MENU_DIR_NEXT);
}

/**
 * gwy_vmenu_bar_new:
 *
 * Creates a new vertical menu bar.
 *
 * Returns: The newly created vertical menu bar, as a #GtkWidget.
 *
 * Since: 1.5
 **/
GtkWidget*
gwy_vmenu_bar_new(void)
{
    return g_object_new(GWY_TYPE_VMENU_BAR, NULL);
}

static void
gwy_vmenu_bar_size_request(GtkWidget      *widget,
                           GtkRequisition *requisition)
{
    GtkMenuBar *menu_bar;
    GtkMenuShell *menu_shell;
    GtkWidget *child;
    GList *children;
    gint nchildren;
    GtkRequisition child_requisition;
    gint ipadding;

    g_return_if_fail(GTK_IS_MENU_BAR(widget));
    g_return_if_fail(requisition != NULL);

    requisition->width = 0;
    requisition->height = 0;

    if (GTK_WIDGET_VISIBLE(widget)) {
        menu_bar = GTK_MENU_BAR(widget);
        menu_shell = GTK_MENU_SHELL(widget);

        nchildren = 0;
        children = menu_shell->children;

        while (children) {
            child = children->data;
            children = children->next;

            if (GTK_WIDGET_VISIBLE(child)) {
                gint toggle_size;

                GTK_MENU_ITEM(child)->show_submenu_indicator = FALSE;
                gtk_widget_size_request(child, &child_requisition);
                gtk_menu_item_toggle_size_request(GTK_MENU_ITEM(child),
                                                  &toggle_size);

                requisition->width = MAX(requisition->width,
                                         child_requisition.width + toggle_size);
                requisition->height += child_requisition.height;
                nchildren += 1;
            }
        }

        gtk_widget_style_get(widget, "internal_padding", &ipadding, NULL);

        requisition->width += (GTK_CONTAINER(menu_bar)->border_width +
                               ipadding +
                               BORDER_SPACING) * 2;
        requisition->height += (GTK_CONTAINER(menu_bar)->border_width +
                                ipadding +
                                BORDER_SPACING) * 2;

        if (get_shadow_type(menu_bar) != GTK_SHADOW_NONE) {
            requisition->width += widget->style->xthickness * 2;
            requisition->height += widget->style->ythickness * 2;
        }
    }
}

static void
gwy_vmenu_bar_size_allocate(GtkWidget     *widget,
                            GtkAllocation *allocation)
{
    GtkMenuBar *menu_bar;
    GtkMenuShell *menu_shell;
    GtkWidget *child;
    GList *children;
    GtkAllocation child_allocation;
    GtkRequisition child_requisition;
    guint offset;
    GtkTextDirection direction;
    gint ltr_x, top_y;
    gint ipadding;

    g_return_if_fail(GTK_IS_MENU_BAR(widget));
    g_return_if_fail(allocation != NULL);

    menu_bar = GTK_MENU_BAR(widget);
    menu_shell = GTK_MENU_SHELL(widget);

    direction = gtk_widget_get_direction(widget);

    widget->allocation = *allocation;
    if (GTK_WIDGET_REALIZED(widget))
        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);

    gtk_widget_style_get(widget, "internal_padding", &ipadding, NULL);

    if (menu_shell->children) {
        child_allocation.x = (GTK_CONTAINER(menu_bar)->border_width +
                              ipadding +
                              BORDER_SPACING);
        child_allocation.y = (GTK_CONTAINER(menu_bar)->border_width +
                              BORDER_SPACING);

        if (get_shadow_type(menu_bar) != GTK_SHADOW_NONE) {
            child_allocation.x += widget->style->xthickness;
            child_allocation.y += widget->style->ythickness;
        }

        child_allocation.width = MAX(1, (gint)allocation->width
                                         - child_allocation.x * 2);

        offset = child_allocation.x;    /* Window edge to menubar start */
        ltr_x = child_allocation.x;
        top_y = child_allocation.y;

        children = menu_shell->children;
        while (children) {
            gint toggle_size;

            child = children->data;
            children = children->next;

            gtk_menu_item_toggle_size_request(GTK_MENU_ITEM(child),
                                              &toggle_size);
            gtk_widget_get_child_requisition(child, &child_requisition);

            child_requisition.width += toggle_size;

            /* Support for the right justified help menu
             * FIXME: verticalization might broke this, I cannot properly
             * test RTL */
            if ((children == NULL) && (GTK_IS_MENU_ITEM(child))
                && (GTK_MENU_ITEM(child)->right_justify)) {
                ltr_x = allocation->width - child_requisition.width - offset;
            }
            if (GTK_WIDGET_VISIBLE(child)) {
                if (direction == GTK_TEXT_DIR_LTR)
                    child_allocation.x = ltr_x;
                else
                    child_allocation.x = allocation->width
                                         - child_requisition.width - ltr_x;

                child_allocation.width = child_requisition.width;
                child_allocation.y = top_y;
                child_allocation.height = child_requisition.height;

                gtk_menu_item_toggle_size_allocate(GTK_MENU_ITEM(child),
                                                   toggle_size);
                gtk_widget_size_allocate(child, &child_allocation);

                top_y += child_allocation.height;
            }
        }
    }
}

static void
gwy_vmenu_bar_paint(GtkWidget *widget, GdkRectangle *area)
{
    g_return_if_fail(GTK_IS_MENU_BAR(widget));

    if (GTK_WIDGET_DRAWABLE(widget)) {
        gint border;

        border = GTK_CONTAINER(widget)->border_width;

        gtk_paint_box(widget->style,
                      widget->window,
                      GTK_WIDGET_STATE(widget),
                      get_shadow_type(GTK_MENU_BAR(widget)),
                      area, widget, "menubar",
                      border, border,
                      widget->allocation.width - border * 2,
                      widget->allocation.height - border * 2);
    }
}

static gint
gwy_vmenu_bar_expose(GtkWidget      *widget,
                     GdkEventExpose *event)
{
    g_return_val_if_fail(GTK_IS_MENU_BAR(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    if (GTK_WIDGET_DRAWABLE(widget)) {
        gwy_vmenu_bar_paint(widget, &event->area);
        (*GTK_WIDGET_CLASS(parent_class)->expose_event)(widget, event);
    }

    return FALSE;
}


static GtkShadowType
get_shadow_type(GtkMenuBar *menubar)
{
    GtkShadowType shadow_type = GTK_SHADOW_OUT;

    gtk_widget_style_get(GTK_WIDGET(menubar),
                         "shadow_type", &shadow_type,
                         NULL);

    return shadow_type;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
