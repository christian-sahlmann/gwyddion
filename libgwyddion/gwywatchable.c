/* @(#) $Id$ */

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
    gwy_debug("%s (base init count = %d)",
              __FUNCTION__, gwy_watchable_base_init_count);
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
    gwy_debug("%s (base init count = %d)",
              __FUNCTION__, gwy_watchable_base_init_count);
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
