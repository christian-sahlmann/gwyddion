/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include "gwywatchable.h"

#define GWY_WATCHABLE_TYPE_NAME "GwyWatchable"

enum {
    VALUE_CHANGED,
    LAST_SIGNAL
};

static guint gwy_watchable_signals[LAST_SIGNAL] = { 0 };

static void gwy_watchable_base_init     (gpointer g_class);

GType
gwy_watchable_get_type(void)
{
    static GType gwy_watchable_type = 0;

    if (!gwy_watchable_type) {
        static const GTypeInfo gwy_watchable_info = {
            sizeof(GwyWatchableIface),
            (GBaseInitFunc)gwy_watchable_base_init,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL,
            NULL,
        };

        gwy_watchable_type = g_type_register_static(G_TYPE_INTERFACE,
                                                    GWY_WATCHABLE_TYPE_NAME,
                                                    &gwy_watchable_info,
                                                    0);
        g_type_interface_add_prerequisite(gwy_watchable_type, G_TYPE_OBJECT);
    }

    gwy_debug("%lu", gwy_watchable_type);
    return gwy_watchable_type;
}

static void
gwy_watchable_base_init(gpointer g_class)
{
    static gboolean initialized = FALSE;

    gwy_debug("initialized = %d", initialized);
    if (initialized)
        return;

    gwy_watchable_signals[VALUE_CHANGED] =
        g_signal_new("value_changed",
                     GWY_TYPE_WATCHABLE,
                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                     G_STRUCT_OFFSET(GwyWatchableIface, value_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
    initialized = TRUE;
}

/**
 * gwy_watchable_value_changed:
 * @watchable: A #GObject implementing #GwyWatchable interface.
 *
 * Emits a "value_changed" signal on a watchable object.
 **/
void
gwy_watchable_value_changed(GObject *watchable)
{
    g_return_if_fail(watchable);
    g_return_if_fail(GWY_IS_WATCHABLE(watchable));
    gwy_debug("emitting value_changed on %s",
              g_type_name(G_TYPE_FROM_INSTANCE(watchable)));

    g_signal_emit(watchable, gwy_watchable_signals[VALUE_CHANGED], 0);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
