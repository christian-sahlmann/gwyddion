/* @(#) $Id$ */

#ifndef __GWY_WATCHABLE_H__
#define __GWY_WATCHABLE_H__

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_WATCHABLE                  (gwy_watchable_get_type())
#define GWY_WATCHABLE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_WATCHABLE, GwyWatchable))
#define GWY_WATCHABLE_CLASS(klass)          (G_TYPE_CHECK_INSTANCE_CAST((klass), GWY_TYPE_WATCHABLE, GwyWatchableClass))
#define GWY_IS_WATCHABLE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_WATCHABLE))
#define GWY_IS_WATCHABLE_CLASS(klass)       (G_TYPE_CHECK_INSTANCE_TYPE((klass), GWY_TYPE_WATCHABLE))
#define GWY_WATCHABLE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_INTERFACE((obj), GWY_TYPE_WATCHABLE, GwyWatchableClass))


typedef struct _GwyWatchable GwyWatchable;
typedef struct _GwyWatchableClass GwyWatchableClass;

struct _GwyWatchable {
    GObject parent_instance;
};

struct _GwyWatchableClass {
    GObjectClass parent_class;

    void (*value_changed)(GObject *watchable);
};


GType         gwy_watchable_get_type         (void) G_GNUC_CONST;
void          gwy_watchable_value_changed    (GObject *watchable);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_WATCHABLE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
