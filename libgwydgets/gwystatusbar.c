/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GwyStatusbar Copyright (C) 1998 Shawn T. Amundson
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

/*
 * Inhertited from GtkStatusbar by Yeti 2004.
 *
 * GtkStatusbar doesn't support markup in the messages.
 */

#include <gtk/gtkstatusbar.h>
#include <gtk/gtklabel.h>

#include <libgwyddion/gwymacros.h>
#include "gwystatusbar.h"

#define GWY_STATUSBAR_TYPE_NAME "GwyStatusbar"

static void     gwy_statusbar_class_init     (GwyStatusbarClass *klass);
static void     gwy_statusbar_init           (GwyStatusbar      *statusbar);
static void     gwy_statusbar_update_markup  (GtkStatusbar *statusbar,
                                              guint context_id,
                                              const gchar *text);

static GtkStatusbarClass *parent_class;

GType
gwy_statusbar_get_type (void)
{
    static GType gwy_statusbar_type = 0;

    if (!gwy_statusbar_type) {
        static const GTypeInfo gwy_statusbar_info = {
            sizeof(GwyStatusbarClass),
            NULL,           /* base_init */
            NULL,           /* base_finalize */
            (GClassInitFunc)gwy_statusbar_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof(GwyStatusbar),
            0,              /* n_preallocs */
            (GInstanceInitFunc)gwy_statusbar_init,
            NULL,
        };
        gwy_debug("");
        gwy_statusbar_type = g_type_register_static(GTK_TYPE_STATUSBAR,
                                                    GWY_STATUSBAR_TYPE_NAME,
                                                    &gwy_statusbar_info, 0);
    }

    return gwy_statusbar_type;
}

static void
gwy_statusbar_class_init(GwyStatusbarClass *klass)
{
    GtkStatusbarClass *statusbar_class;

    statusbar_class = GTK_STATUSBAR_CLASS(klass);
    parent_class = g_type_class_peek_parent(klass);

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

    gtk_label_set_markup(GTK_LABEL(statusbar->label), text);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
