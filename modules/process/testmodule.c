/* @(#) $Id$ */

#include <libgwymodule/gwymodule.h>

GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    "testmodule",
    "This is just a dummy test module.\nIt does nothing.",
    "Yeti",
    "1.0",
    "Yeti",
    "2003",
};

GWY_MODULE_REGISTER_FUNC(module)
{
    g_message("@@@@@@@@@@@ Foo! @@@@@@@@@@");
    return &module_info;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
