/* @(#) $Id$ */

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#define GWY_RUN_ANY \
    (GWY_RUN_INTERACTIVE | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
typedef struct {
    gdouble angle;
} RotateArgs;

static gboolean    module_register            (const gchar *name);
static gboolean    rotate                     (GwyContainer *data,
                                               GwyRunType run);
static gboolean    rotate_dialog              (RotateArgs *args);
static void        rotate_load_args           (GwyContainer *container,
                                               RotateArgs *args);
static void        rotate_save_args           (GwyContainer *container,
                                               RotateArgs *args);

RotateArgs rotate_defaults = {
    0.0,
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
        rotate_load_args(data, &args);
    ok = (run != GWY_RUN_INTERACTIVE) || rotate_dialog(&args);
    if (ok) {
       gwy_data_field_rotate(dfield, args.angle, GWY_INTERPOLATION_BILINEAR);
       if (run != GWY_RUN_WITH_DEFAULTS)
           rotate_save_args(data, &args);
    }

    return FALSE;
}

static gboolean
rotate_dialog(RotateArgs *args)
{
    GtkWidget *dialog, *hbox, *widget;
    GtkAdjustment *adj;

    dialog = gtk_dialog_new_with_buttons(_("Rotate"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    widget = gtk_label_new(_("Rotate by angle"));
    gtk_box_pack_start(GTK_BOX(hbox),  widget, FALSE, FALSE, 0);
    adj = GTK_ADJUSTMENT(gtk_adjustment_new(args->angle, -360, 360, 5, 30, 0));
    widget = gtk_spin_button_new(adj, 1.0, 0);
    gtk_box_pack_start(GTK_BOX(hbox),  widget, FALSE, FALSE, 0);
    widget = gtk_label_new(_("deg (CCW)"));
    gtk_box_pack_start(GTK_BOX(hbox),  widget, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);
    switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
        gtk_widget_destroy(dialog);
        case GTK_RESPONSE_NONE:
        return FALSE;
        break;

        case GTK_RESPONSE_OK:
        break;

        default:
        g_assert_not_reached();
        break;
    }

    args->angle = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static const gchar *angle_key = "/module/rotate/angle";

static void
rotate_load_args(GwyContainer *container,
                 RotateArgs *args)
{
    if (gwy_container_contains_by_name(container, angle_key))
        args->angle = gwy_container_get_double_by_name(container, angle_key);
    else
        args->angle = rotate_defaults.angle;
}

static void
rotate_save_args(GwyContainer *container,
                 RotateArgs *args)
{
    gwy_container_set_double_by_name(container, angle_key, args->angle);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
