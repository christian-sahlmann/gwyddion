/* @(#) $Id$ */

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>

#define GWY_RUN_ANY \
    (GWY_RUN_INTERACTIVE | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
typedef struct {
    gdouble angle;
    GwyInterpolationType interp;
} RotateArgs;

typedef struct {
    GtkObject *angle;
    GtkWidget *interp;
} RotateControls;

static gboolean    module_register            (const gchar *name);
static gboolean    rotate                     (GwyContainer *data,
                                               GwyRunType run);
static gboolean    rotate_dialog              (RotateArgs *args);
static void        interp_changed_cb          (GObject *item,
                                               RotateArgs *args);
static void        rotate_load_args           (GwyContainer *container,
                                               RotateArgs *args);
static void        rotate_save_args           (GwyContainer *container,
                                               RotateArgs *args);
static void        rotate_dialog_update       (RotateControls *controls,
                                               RotateArgs *args);

RotateArgs rotate_defaults = {
    0.0,
    GWY_INTERPOLATION_BILINEAR,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "rotate",
    "Rotation by an arbitrary angle.",
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
    static GwyProcessFuncInfo rotate_func_info = {
        "rotate",
        "/_Basic Operations/Rotate By _Angle...",
        &rotate,
        GWY_RUN_ANY,
    };

    gwy_register_process_func(name, &rotate_func_info);

    return TRUE;
}

static gboolean
rotate(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    RotateArgs args;
    gboolean ok;

    g_assert(run & GWY_RUN_ANY);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = rotate_defaults;
    else
        rotate_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_INTERACTIVE) || rotate_dialog(&args);
    if (ok) {
       gwy_data_field_rotate(dfield, args.angle, args.interp);
       if (run != GWY_RUN_WITH_DEFAULTS)
           rotate_save_args(gwy_app_settings_get(), &args);
    }

    return FALSE;
}

static gboolean
rotate_dialog(RotateArgs *args)
{
    GtkWidget *dialog, *table;
    RotateControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Rotate"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    table = gtk_table_new(2, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.angle = gtk_adjustment_new(args->angle, -360, 360, 5, 30, 0);
    gwy_table_attach_spinbutton(table, 0, _("Rotate by angle:"), _("deg (CCW)"),
                                controls.angle);

    controls.interp
        = gwy_interpolation_option_menu(G_CALLBACK(interp_changed_cb),
                                        args, args->interp);
    gwy_table_attach_row(table, 1, _("Interpolation type:"), "",
                         controls.interp);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = rotate_defaults;
            rotate_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->angle = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.angle));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
interp_changed_cb(GObject *item,
                  RotateArgs *args)
{
    args->interp = GPOINTER_TO_INT(g_object_get_data(item,
                                                     "interpolation-type"));
}

static const gchar *angle_key = "/module/rotate/angle";
static const gchar *interp_key = "/module/rotate/interp";

static void
rotate_load_args(GwyContainer *container,
                 RotateArgs *args)
{
    *args = rotate_defaults;

    if (gwy_container_contains_by_name(container, angle_key))
        args->angle = gwy_container_get_double_by_name(container, angle_key);
    if (gwy_container_contains_by_name(container, interp_key))
        args->interp = gwy_container_get_int32_by_name(container, interp_key);
}

static void
rotate_save_args(GwyContainer *container,
                 RotateArgs *args)
{
    gwy_container_set_double_by_name(container, angle_key, args->angle);
    gwy_container_set_int32_by_name(container, interp_key, args->interp);
}

static void
rotate_dialog_update(RotateControls *controls,
                     RotateArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->angle),
                             args->angle);
    /* FIXME: this assumes index == interp type */
    gtk_option_menu_set_history(GTK_OPTION_MENU(controls->interp),
                                args->interp);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
