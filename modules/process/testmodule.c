/* @(#) $Id$ */

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

static gboolean    module_register        (const gchar *name);
static gboolean    test_process_func      (GwyContainer *data,
                                           GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "testmodule",
    "This is just a dummy test module.\nIt does nothing.",
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
    static GwyProcessFuncInfo test_func_info = {
        "test_func",
        "/_Test/_Test",
        &test_process_func,
        GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS,
    };

    gwy_register_process_func(name, &test_func_info);

    return TRUE;
}

static gboolean
test_process_func(GwyContainer *data, GwyRunType run)
{
    GwyDataField *df;

    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
    df = (GwyDataField*)gwy_container_get_object_by_name(data, "/0/data");
    g_return_val_if_fail(GWY_IS_DATA_FIELD(df), FALSE);
    gwy_debug("data real size: %gx%g",
              gwy_data_field_get_xreal(df),
              gwy_data_field_get_yreal(df));

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
