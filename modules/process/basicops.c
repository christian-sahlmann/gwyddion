/* @(#) $Id$ */

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#define BASICOPS_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

static gboolean    module_register            (const gchar *name);
static gboolean    flip_horizontally          (GwyContainer *data,
                                               GwyRunType run);
static gboolean    flip_vertically            (GwyContainer *data,
                                               GwyRunType run);
static gboolean    invert_value               (GwyContainer *data,
                                               GwyRunType run);
static gboolean    rotate_clockwise_90        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    rotate_counterclockwise_90 (GwyContainer *data,
                                               GwyRunType run);
static gboolean    rotate_180                 (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "basicops",
    "Basic operations like inversion or flipping.",
    "Yeti",
    "1.0",
    "Yeti & PK",
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
        &flip_horizontally,
        GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS,
    };
    static GwyProcessFuncInfo flip_vertically_func_info = {
        "flip_vertically",
        "/_Basic Operations/Flip _Vertically",
        &flip_vertically,
        GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS,
    };
    static GwyProcessFuncInfo invert_value_func_info = {
        "invert_value",
        "/_Basic Operations/_Invert Value",
        &invert_value,
        GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS,
    };
    static GwyProcessFuncInfo rotate_clockwise_90_func_info = {
        "rotate_clockwise_90",
        "/_Basic Operations/_Rotate Clockwise",
        &rotate_clockwise_90,
        GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS,
    };
    static GwyProcessFuncInfo rotate_counterclockwise_90_func_info = {
        "rotate_counterclockwise_90",
        "/_Basic Operations/Rotate _Counterclockwise",
        &rotate_counterclockwise_90,
        GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS,
    };
    static GwyProcessFuncInfo rotate_180_func_info = {
        "rotate_180",
        "/_Basic Operations/Rotate 1_80 degrees",
        &rotate_180,
        GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS,
    };

    gwy_process_func_register(name, &flip_horizontally_func_info);
    gwy_process_func_register(name, &flip_vertically_func_info);
    gwy_process_func_register(name, &invert_value_func_info);
    gwy_process_func_register(name, &rotate_clockwise_90_func_info);
    gwy_process_func_register(name, &rotate_counterclockwise_90_func_info);
    gwy_process_func_register(name, &rotate_180_func_info);

    return TRUE;
}

static gboolean
flip_horizontally(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_assert(run & BASICOPS_RUN_MODES);
    dfield = (GwyDataField*)gwy_container_get_object_by_name(data, "/0/data");
    gwy_data_field_invert(dfield, FALSE, TRUE, FALSE);

    return TRUE;
}

static gboolean
flip_vertically(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_assert(run & BASICOPS_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);

    return TRUE;
}

static gboolean
invert_value(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_assert(run & BASICOPS_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_data_field_invert(dfield, FALSE, FALSE, TRUE);

    return TRUE;
}

static gboolean
rotate_clockwise_90(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_assert(run & BASICOPS_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_data_field_rotate(dfield, 270, GWY_INTERPOLATION_ROUND);

    return TRUE;
}

static gboolean
rotate_counterclockwise_90(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_assert(run & BASICOPS_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_data_field_rotate(dfield, 90, GWY_INTERPOLATION_ROUND);

    return TRUE;
}

static gboolean
rotate_180(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_assert(run & BASICOPS_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_data_field_rotate(dfield, 180, GWY_INTERPOLATION_ROUND);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
