/* @(#) $Id$ */

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <app/settings.h>

#define BASICOPS_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_MODAL | GWY_RUN_WITH_DEFAULTS)

static gboolean    module_register            (const gchar *name);
static gboolean    plugin_proxy               (GwyContainer *data,
                                               GwyRunType run,
                                               const gchar *name);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "plugin-proxy",
    "Plug-in proxy is a module capable of querying, registering, and running "
        "external programs (plug-ins) on data.",
    "Yeti",
    "1.0",
    "Yeti",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo flip_horizontally_func_info = {
        "flip_horizontally",
        "/_Basic Operations/Flip _Horizontally",
        &plugin_proxy,
        GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS,
    };
    GwyContainer *settings;
    const gchar *plugin_path;

    /*gwy_process_func_register(name, &flip_horizontally_func_info);*/
    settings = gwy_app_settings_get();
    plugin_path = gwy_container_get_string_by_name(settings, "/app/plugindir");
    g_return_val_if_fail(plugin_path, FALSE);
    gwy_debug("%s: plug-in path is: %s", __FUNCTION__, plugin_path);

    return TRUE;
}

static gboolean
plugin_proxy(GwyContainer *data, GwyRunType run, const gchar *name)
{
    GwyDataField *dfield;

    g_assert(run & BASICOPS_RUN_MODES);
    dfield = (GwyDataField*)gwy_container_get_object_by_name(data, "/0/data");
    gwy_debug("%s: called as %s with %d run mode", __FUNCTION__, name, run);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

