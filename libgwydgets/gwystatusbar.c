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
#include <gtk/gtkstatusbar.h>
#include <gtk/gtklabel.h>

#include <libgwyddion/gwymacros.h>
#include "gwystatusbar.h"

static void     gwy_statusbar_update_markup  (GtkStatusbar *statusbar,
                                              guint context_id,
                                              const gchar *text);

G_DEFINE_TYPE(GwyStatusbar, gwy_statusbar, GTK_TYPE_STATUSBAR)

static void
gwy_statusbar_class_init(GwyStatusbarClass *klass)
{
    GtkStatusbarClass *statusbar_class;

    statusbar_class = GTK_STATUSBAR_CLASS(klass);

    statusbar_class->text_pushed = gwy_statusbar_update_markup;
    statusbar_class->text_popped = gwy_statusbar_update_markup;
}

static void
gwy_statusbar_init(G_GNUC_UNUSED GwyStatusbar *statusbar)
{
}

/**
 * gwy_statusbar_new:
 *
 * Creates a new Gwyddion statusbar.
 *
 * Gwyddion statusbar differs from #GtkStatusbar only in one thing: the
 * messages can contain Pango markup.
 *
 * Returns: The newly created statusbar, as a #GtkWidget.
 **/
GtkWidget*
gwy_statusbar_new(void)
{
    return g_object_new(GWY_TYPE_STATUSBAR, NULL);
}

static void
gwy_statusbar_update_markup(GtkStatusbar *statusbar,
                            G_GNUC_UNUSED guint context_id,
                            const gchar *text)
{
    g_return_if_fail(GTK_IS_STATUSBAR(statusbar));

    if (!text)
        text = "";

    /* FIXME: this causes size allocation request */
    gtk_label_set_markup(GTK_LABEL(statusbar->label), text);
}

/**
 * gwy_statusbar_set_markup:
 * @statusbar: A statusbar.
 * @markup: Text message to display in the statusbar.  It can contain Pango
 *          markup.
 *
 * Sets the text to display in a status bar.
 *
 * This method is intended for simple status bars that do not have stacks and
 * do not need contexts.  It does not mix with gtk_status_bar_push().  You can
 * use either this simple interface or the full stacks-and-contexts API with
 * #GwyStatusbar, but not both in the same status bar.
 **/
void
gwy_statusbar_set_markup(GwyStatusbar *statusbar,
                         const gchar *markup)
{
    GtkStatusbar *sbar;
    guint id;

    sbar = GTK_STATUSBAR(statusbar);
    if (!statusbar->context_id) {
        if (sbar->keys)
            g_warning("gwy_statusbar_set_markup() does not mix with "
                      "full stacks-and-context GwyStatusbar API");
        statusbar->context_id
            = gtk_statusbar_get_context_id(sbar, "GwyStatusbar-global-context");
    }

    id = gtk_statusbar_push(sbar, statusbar->context_id, markup);
    if (statusbar->message_id)
        gtk_statusbar_remove(sbar, statusbar->context_id,
                             statusbar->message_id);
    statusbar->message_id = id;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwystatusbar
 * @title: GwyStatusbar
 * @short_description: Statusbar with Pango markup support
 *
 * This widget is almost identical to #GtkStatusbar except that it
 * interprets Pango markup in its messages.
 *
 * It also provides a simple context-free message method
 * gwy_statusbar_set_markup() for status bars that do not need all the
 * complexity of #GtkStatusbar stacks.
 * 
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
