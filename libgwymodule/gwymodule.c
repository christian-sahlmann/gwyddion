/* @(#) $Id$ */

/* XXX: This file is to be replaced. */
#include <string.h>
#include <glib-object.h>
#include <gmodule.h>

#include "gwymodule.h"

#define GWY_MODULE_TYPE_NAME "GwyModule"


static void gwy_module_class_init(GwyModuleClass *klass);
static void gwy_module_init(GwyModule *module);


GType
gwy_module_get_type(void)
{
    static GType gwy_module_type = 0;

    if (!gwy_module_type) {
        static const GTypeInfo gwy_module_info = {
            sizeof(GwyModuleClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_module_class_init,
            NULL,
            NULL,
            sizeof(GwyModule),
            0,
            (GInstanceInitFunc)gwy_module_init,
            NULL,
        };

        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
        gwy_module_type = g_type_register_static(G_TYPE_OBJECT,
                                                 GWY_MODULE_TYPE_NAME,
                                                 &gwy_module_info,
                                                 0);
    }

    return gwy_module_type;
}

static void
gwy_module_class_init(GwyModuleClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
}

static void
gwy_module_init(GwyModule *module)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    module->abi_version = GWY_MODULE_ABI_VERSION;
    module->module_info = NULL;
    /* TODO */
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
