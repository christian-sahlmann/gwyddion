/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

static void gwy_watchable_base_init     (GwyWatchableClass *klass);
static void gwy_watchable_base_finalize (GwyWatchableClass *klass);

GType
gwy_watchable_get_type(void)
{
    static GType gwy_watchable_type = 0;

    if (!gwy_watchable_type) {
        static const GTypeInfo gwy_watchable_info = {
            sizeof(GwyWatchableClass),
            (GBaseInitFunc)gwy_watchable_base_init,
            (GBaseFinalizeFunc)gwy_watchable_base_finalize,
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

    return gwy_watchable_type;
}

static guint gwy_watchable_base_init_count = 0;

static void
gwy_watchable_base_init(GwyWatchableClass *klass)
{
    gwy_watchable_base_init_count++;
    gwy_debug("base init count = %d", gwy_watchable_base_init_count);
    if (gwy_watchable_base_init_count == 1) {
        gwy_watchable_signals[VALUE_CHANGED] =
            g_signal_new("value_changed",
                         G_OBJECT_CLASS_TYPE(klass),
                         G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                         G_STRUCT_OFFSET(GwyWatchableClass, value_changed),
                         NULL, NULL,
                         g_cclosure_marshal_VOID__VOID,
                         G_TYPE_NONE, 0);
    }
}

static void
gwy_watchable_base_finalize(GwyWatchableClass *klass)
{
    gwy_watchable_base_init_count--;
    gwy_debug("base init count = %d", gwy_watchable_base_init_count);
    if (gwy_watchable_base_init_count == 0) {
        /* destroy signals...
         * FIXME: but how?
         */
    }
    klass = klass;
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
