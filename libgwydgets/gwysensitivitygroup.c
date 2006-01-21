/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwydgets/gwysensitivitygroup.h>

typedef struct {
    guint mask;
    gboolean dirty;
    GwySensitivityGroup *parent;
    GList *widgets;
} SensList;

static void      gwy_sensitivity_group_finalize   (GObject *object);
static void      gwy_sensitivity_group_check_dirty(GwySensitivityGroup *sensgroup);
static gboolean  gwy_sensitivity_group_commit(gpointer data);
static SensList* gwy_sensitivity_group_find_list  (GwySensitivityGroup *sensgroup,
                                                   guint mask);
static void      gwy_sensitivity_group_widget_gone(GObject *object,
                                                   GList *item);
static SensList* gwy_sensitivity_group_get_senslist(GwySensitivityGroup *sensgroup,
                                                    GObject *object);

static GQuark sensitivity_group_quark = 0;

G_DEFINE_TYPE(GwySensitivityGroup, gwy_sensitivity_group, G_TYPE_OBJECT)

static void
gwy_sensitivity_group_class_init(GwySensitivityGroupClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_sensitivity_group_finalize;
    sensitivity_group_quark
        = g_quark_from_static_string("gwy-sensitivity-group-list");
}

static void
gwy_sensitivity_group_init(GwySensitivityGroup *sensgroup)
{
    gwy_debug_objects_creation(G_OBJECT(sensgroup));
}

static void
gwy_sensitivity_group_finalize(GObject *object)
{
    GwySensitivityGroup *sensgroup;

    sensgroup = GWY_SENSITIVITY_GROUP(object);

    if (sensgroup->source_id) {
        g_source_remove(sensgroup->source_id);
        sensgroup->source_id = 0;
    }
    if (sensgroup->lists) {
        g_critical("Sensitivity group is finialized when it still contains "
                   "widget lists.");
    }

    G_OBJECT_CLASS(gwy_sensitivity_group_parent_class)->finalize(object);
}

/**
 * gwy_sensitivity_group_new:
 *
 * Creates a new flag-based widget sensitivity group.
 *
 * Returns: The newly created sensitivity group.  It starts with zero state.
 **/
GwySensitivityGroup*
gwy_sensitivity_group_new(void)
{
    return (GwySensitivityGroup*)g_object_new(GWY_TYPE_SENSITIVITY_GROUP, NULL);
}

/**
 * gwy_sensitivity_group_add_widget:
 * @sensgroup: A widget flag sensitivity group.
 * @widget: Widget to add to @sensgroup.
 * @mask: Which flags the widget is sensitive to.  See
 *        gwy_sensitivity_group_set_state() for details.
 *
 * Adds a widget to flag sensitivity group.
 *
 * Widget sensitivity should not be set manually after the addition as the
 * result is likely to be a fight over sensitivity setting.
 *
 * The added widget takes a reference on @sensgroup.  So the group is not
 * destroyed when they are any widgets in, generally, you can release your
 * initial reference after adding widgets to the group.
 **/
void
gwy_sensitivity_group_add_widget(GwySensitivityGroup *sensgroup,
                                 GtkWidget *widget,
                                 guint mask)
{
    SensList *senslist;
    GObject *object;
    gboolean sens;

    g_return_if_fail(GWY_IS_SENSITIVITY_GROUP(sensgroup));
    g_return_if_fail(GTK_IS_WIDGET(widget));
    object = G_OBJECT(widget);
    if (g_object_get_qdata(object, sensitivity_group_quark)) {
        g_warning("Widget cannot be member of more than one sensitivity "
                  "group at once.");
        return;
    }

    senslist = gwy_sensitivity_group_find_list(sensgroup, mask);
    if (!senslist) {
        senslist = g_new(SensList, 1);
        senslist->widgets = NULL;
        senslist->dirty = FALSE;
        senslist->mask = mask;
        senslist->parent = sensgroup;
        sensgroup->lists = g_list_prepend(sensgroup->lists, senslist);
    }
    senslist->widgets = g_list_prepend(senslist->widgets, widget);
    g_object_set_qdata(object, sensitivity_group_quark, senslist);
    /* Pass the list item as cbdata */
    g_signal_connect(object, "destroy",
                     G_CALLBACK(gwy_sensitivity_group_widget_gone),
                     senslist->widgets);
    /* Self-reference (pretend the widget has referenced us) */
    g_object_ref(sensgroup);

    sens = ((senslist->mask & sensgroup->state) == senslist->mask);
    gtk_widget_set_sensitive(widget, sens);
}

/**
 * gwy_sensitivity_group_set_state:
 * @sensgroup: A widget flag sensitivity group.
 * @affected_mask: Which bits in @state to copy to @sensgroup state.
 *                 1's in @affected_mask causes corresponding bits in
 *                 @sensgroup state to be set to the value of corresponding
 *                 bit in @state.
 *                 0's in @affected_mask cause corresponding state bits to be
 *                 kept on their current value.
 * @state: The new state of @sensgroup (masked with @affected_mask).
 *
 * Sets the state of a widget flag sensitivity group.
 *
 * Widget sensitivity states are then updated accordingly.
 *
 * More precisely, widget will be made sensitive when all bits in its @mask are
 * set in current @sensgroup state, insensitive otherwise.  This means when
 * @mask is zero, widget will be always sensitive.
 **/
void
gwy_sensitivity_group_set_state(GwySensitivityGroup *sensgroup,
                                guint affected_mask,
                                guint state)
{
    guint newstate;

    g_return_if_fail(GWY_IS_SENSITIVITY_GROUP(sensgroup));

    newstate = (~affected_mask & sensgroup->state) | (affected_mask & state);
    if (newstate == sensgroup->state)
        return;

    sensgroup->state = newstate;
    gwy_sensitivity_group_check_dirty(sensgroup);
}

/**
 * gwy_sensitivity_group_get_state:
 * @sensgroup: A widget flag sensitivity group.
 *
 * Gets the current state of a widget flag sensitivity group.
 *
 * Returns: The current state as set with gwy_sensitivity_group_set_state().
 **/
guint
gwy_sensitivity_group_get_state(GwySensitivityGroup *sensgroup)
{
    g_return_val_if_fail(GWY_IS_SENSITIVITY_GROUP(sensgroup), 0);
    return sensgroup->state;
}

/**
 * gwy_sensitivity_group_release_widget:
 * @sensgroup: A widget flag sensitivity group.
 * @widget: Widget to remove from @sensgroup.
 *
 * Removes a widget from flag sensitivity group.
 **/
void
gwy_sensitivity_group_release_widget(GwySensitivityGroup *sensgroup,
                                     GtkWidget *widget)
{
    SensList *senslist;
    gboolean sens;
    GList *item;

    g_return_if_fail(GWY_IS_SENSITIVITY_GROUP(sensgroup));
    g_return_if_fail(GTK_IS_WIDGET(widget));
    senslist = gwy_sensitivity_group_get_senslist(sensgroup, G_OBJECT(widget));
    if (!senslist)
        return;

    /* Commit sensitivity changes before removal */
    if (senslist->dirty) {
        sens = ((senslist->mask & sensgroup->state) == senslist->mask);
        gtk_widget_set_sensitive(widget, sens);
    }

    item = g_list_find(senslist->widgets, widget);
    g_assert(item);
    g_signal_handlers_disconnect_by_func(widget,
                                         gwy_sensitivity_group_widget_gone,
                                         item);
    senslist->widgets = g_list_delete_link(senslist->widgets, item);
    /* Destroy whole list when there are no widgets in it */
    if (!senslist->widgets)
        sensgroup->lists = g_list_remove(sensgroup->lists, senslist);

    /* Self-dereference (pretend the widget has dereferenced us) */
    g_object_unref(sensgroup);
}

/**
 * gwy_sensitivity_group_get_widget_mask:
 * @sensgroup: A widget flag sensitivity group.
 * @widget: Widget to get flags of.
 *
 * Gets sensitivity flags of a widget in a flag sensitivity group.
 *
 * Returns: The mask as passed to gwy_sensitivity_group_add_widget()
 *          or gwy_sensitivity_group_set_widget_mask().
 **/
guint
gwy_sensitivity_group_get_widget_mask(GwySensitivityGroup *sensgroup,
                                      GtkWidget *widget)
{
    SensList *senslist;

    g_return_val_if_fail(GWY_IS_SENSITIVITY_GROUP(sensgroup), 0);
    g_return_val_if_fail(GTK_IS_WIDGET(widget), 0);
    senslist = gwy_sensitivity_group_get_senslist(sensgroup, G_OBJECT(widget));
    if (!senslist)
        return 0;

    return senslist->mask;
}

/**
 * gwy_sensitivity_group_set_widget_mask:
 * @sensgroup: A widget flag sensitivity group.
 * @widget: Widget to set flags of.
 * @mask: Which flags the widget is sensitive to.  See
 *        gwy_sensitivity_group_set_state() for details.
 *
 * Sets the flag mask of a widget in a flag sensitivity group.
 **/
void
gwy_sensitivity_group_set_widget_mask(GwySensitivityGroup *sensgroup,
                                      GtkWidget *widget,
                                      guint mask)
{
    SensList *senslist;

    g_return_if_fail(GWY_IS_SENSITIVITY_GROUP(sensgroup));
    g_return_if_fail(GTK_IS_WIDGET(widget));
    senslist = gwy_sensitivity_group_get_senslist(sensgroup, G_OBJECT(widget));
    if (!senslist)
        return;
    if (mask == senslist->mask)
        return;

    g_object_set_qdata(G_OBJECT(widget), sensitivity_group_quark, NULL);
    gwy_sensitivity_group_add_widget(sensgroup, widget, mask);
    g_object_unref(sensgroup);
}

/**
 * gwy_sensitivity_group_check_dirty:
 * @sensgroup: A widget flag sensitivity group.
 *
 * Recheck and maybe reset update handler.
 *
 * Dirty state (that is difference between old sensitivity and new sensitivity)
 * of all lists is recomputed.  If any of them is found dirty update handler is
 * either kept or set-up anew.  If none is found dirty, the handler is reset
 * or kept unset.
 **/
static void
gwy_sensitivity_group_check_dirty(GwySensitivityGroup *sensgroup)
{
    gboolean oldsens, newsens, dirty = FALSE;
    SensList *senslist;
    GList *l;

    for (l = sensgroup->lists; l; l = g_list_next(l)) {
        senslist = (SensList*)l->data;
        oldsens = ((senslist->mask & sensgroup->old_state) == senslist->mask);
        newsens = ((senslist->mask & sensgroup->state) == senslist->mask);
        senslist->dirty = (oldsens != newsens);
        dirty |= senslist->dirty;
    }

    if (dirty && !sensgroup->source_id)
        /* Go even before X server events, we want sensitivity to be
         * current when events are received from user. */
        sensgroup->source_id = g_idle_add_full(G_PRIORITY_HIGH,
                                               gwy_sensitivity_group_commit,
                                               sensgroup,
                                               NULL);
    else if (!dirty && sensgroup->source_id)
        g_source_remove(sensgroup->source_id);
}

/**
 * gwy_sensitivity_group_commit:
 * @data: A sensitivity group.
 *
 * Actually changes state of widgets in dirty widget lists.
 *
 * Returns: Always false to be removed as source id.
 **/
static gboolean
gwy_sensitivity_group_commit(gpointer data)
{
    GwySensitivityGroup *sensgroup = (GwySensitivityGroup*)data;
    SensList *senslist;
    GList *l, *wl;
    gboolean sens;

    for (l = sensgroup->lists; l; l = g_list_next(l)) {
        senslist = (SensList*)l->data;
        if (!senslist->dirty)
            continue;

        sens = ((senslist->mask & sensgroup->state) == senslist->mask);
        for (wl = senslist->widgets; wl; wl = g_list_next(wl))
            gtk_widget_set_sensitive((GtkWidget*)wl->data, sens);
    }
    sensgroup->old_state = sensgroup->state;
    sensgroup->source_id = 0;

    return FALSE;
}

/**
 * gwy_sensitivity_group_find_list:
 * @sensgroup: A widget flag sensitivity group.
 * @mask: A widget list flag mask.
 *
 * Finds widget list with specified mask.
 *
 * Returns: The widget list, or %NULL if not found.
 **/
static SensList*
gwy_sensitivity_group_find_list(GwySensitivityGroup *sensgroup,
                                guint mask)
{
    SensList *senslist;
    GList *l;

    for (l = sensgroup->lists; l; l = g_list_next(l)) {
        senslist = (SensList*)l->data;
        if (senslist->mask == mask)
            return senslist;
    }
    return NULL;
}

static void
gwy_sensitivity_group_widget_gone(GObject *object,
                                  GList *item)
{
    GwySensitivityGroup *sensgroup;
    SensList *senslist;

    senslist = gwy_sensitivity_group_get_senslist(NULL, object);
    g_assert(senslist);
    senslist->widgets = g_list_delete_link(senslist->widgets, item);
    sensgroup = senslist->parent;

    /* Destroy whole list when there are no widgets in it */
    if (!senslist->widgets)
        sensgroup->lists = g_list_remove(sensgroup->lists, senslist);

    /* Self-dereference (pretend the widget has dereferenced us) */
    g_object_unref(sensgroup);
}

static SensList*
gwy_sensitivity_group_get_senslist(GwySensitivityGroup *sensgroup,
                                   GObject *object)
{
    SensList *senslist;

    senslist = (SensList*)g_object_get_qdata(object, sensitivity_group_quark);
    if (!senslist) {
        g_warning("Widget is not in any sensitivity group.");
        return NULL;
    }
    if (sensgroup && senslist->parent != sensgroup) {
        g_warning("Widget is in different sensitivity group.");
        return NULL;
    }
    return senslist;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwysensitivitygroup
 * @title: GwySensitivityGroup
 * @short_description: Control widget sensitivity by flags
 *
 * #GwySensitivityGroup is a tool to manage sensitivity of sets of related
 * widgets based on fulfilment of some conditions.
 *
 * A new sensitivity group is created with gwy_sensitivity_group_new().
 * Widgets are then added to it with gwy_sensitivity_group_add_widget(), each
 * with some set of flags it reacts to.  When state of the sensitivity group
 * is then changed with gwy_sensitivity_group_set_state(), widgets whose flags
 * are set are made sensitive, others insensitive (see
 * gwy_sensitivity_group_set_state() description for details).
 *
 * The interpretation of the flags is completely up to #GwySensitivityGroup
 * user, but they generally represent availability of some resource or
 * possibility of some action.
 *
 * In the following example we have two conditions, %SENS_IMAGE and
 * %SENS_GRAPH, representing availability of image data and graph data:
 * <informalexample><programlisting>
 * enum {
 *      SENS_IMAGE = 1 << 0,
 *      SENS_GRAPH = 1 << 1,
 *      SENS_MASK  = 0x03
 * };
 * </programlisting></informalexample>
 * We create buttons for three actions, one operates on image data, another
 * on graph data, and the last on both:
 * <informalexample><programlisting>
 * sensgroup = gwy_sensitivity_group_new();
 * button = gtk_button_new_with_label("Filter Data");
 * gwy_sensitivity_group_add_widget(sensgroup, button, SENS_IMAGE);
 * button = gtk_button_new_with_label("Fit Graph");
 * gwy_sensitivity_group_add_widget(sensgroup, button, SENS_GRAPH);
 * button = gtk_button_new_with_label("Add Profile");
 * gwy_sensitivity_group_add_widget(sensgroup, button, SENS_IMAGE | SENS_GRAPH);
 * g_object_unref(sensgroup);
 * </programlisting></informalexample>
 * When graph data becomes available, we simply call
 * <informalexample><programlisting>
 * gwy_sensitivity_group_set_state(sensgroup, SENS_GRAPH, SENS_GRAPH);
 * </programlisting></informalexample>
 * and when image data becomes unavailable
 * <informalexample><programlisting>
 * gwy_sensitivity_group_set_state(sensgroup, SENS_IMAGE, 0);
 * </programlisting></informalexample>
 * and the button sensitivities will be adjusted to match the situation.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
