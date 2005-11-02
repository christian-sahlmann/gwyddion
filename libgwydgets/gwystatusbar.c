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

/************************** Documentation ****************************/

/**
 * SECTION:gwystatusbar
 * @title: GwyStatusbar
 * @short_description: Statusbar with Pango markup support
 *
 * This widget is completely identical to #GtkStatusbar except that it
 * interprets Pango markup in its messages.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
