/* @(#) $Id$ */

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/file.h>

#define GWY_RUN_ANY \
    (GWY_RUN_INTERACTIVE | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

#define ROUND(x) ((gint)floor((x) + 0.5))

/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
typedef struct {
    gdouble ratio;
    GwyInterpolationType interp;
    /* interface only */
    gint xres;
    gint yres;
} ScaleArgs;

typedef struct {
    GtkObject *ratio;
    GtkWidget *interp;
    /* interface only */
    GtkObject *xres;
    GtkObject *yres;
    gboolean in_update;
} ScaleControls;

static gboolean    module_register           (const gchar *name);
static gboolean    scale                     (GwyContainer *data,
                                              GwyRunType run);
static gboolean    scale_dialog              (ScaleArgs *args);
static void        interp_changed_cb         (GObject *item,
                                              ScaleArgs *args);
static void        scale_changed_cb          (GtkAdjustment *adj,
                                              ScaleArgs *args);
static void        width_changed_cb          (GtkAdjustment *adj,
                                              ScaleArgs *args);
static void        height_changed_cb         (GtkAdjustment *adj,
                                              ScaleArgs *args);
static void        scale_load_args           (GwyContainer *container,
                                              ScaleArgs *args);
static void        scale_save_args           (GwyContainer *container,
                                              ScaleArgs *args);
static void        scale_dialog_update       (ScaleControls *controls,
                                              ScaleArgs *args);

ScaleArgs scale_defaults = {
    1.0,
    GWY_INTERPOLATION_BILINEAR,
    0,
    0,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "scale",
    "Scale data by an arbitrary factor.",
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
    static GwyProcessFuncInfo scale_func_info = {
        "scale",
        "/_Basic Operations/Scale...",
        &scale,
        GWY_RUN_ANY,
    };

    gwy_register_process_func(name, &scale_func_info);

    return TRUE;
}

static gboolean
scale(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield;
    ScaleArgs args;
    gboolean ok;

    g_assert(run & GWY_RUN_ANY);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = scale_defaults;
    else
        scale_load_args(gwy_app_settings_get(), &args);
    args.xres = gwy_data_field_get_xres(dfield);
    args.yres = gwy_data_field_get_yres(dfield);
    ok = (run != GWY_RUN_INTERACTIVE) || scale_dialog(&args);
    if (ok) {
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
        g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
        /* not needed for scale? gwy_app_clean_up_data(data); */
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        gwy_data_field_resample(dfield,
                                ROUND(args.ratio*args.xres),
                                ROUND(args.ratio*args.yres),
                                args.interp);
        data_window = gwy_app_create_data_window(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window));
        if (run != GWY_RUN_WITH_DEFAULTS)
            scale_save_args(gwy_app_settings_get(), &args);
    }

    return FALSE;
}

static gboolean
scale_dialog(ScaleArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    ScaleControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Scale"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    table = gtk_table_new(4, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.ratio = gtk_adjustment_new(args->ratio, 0.01, 100, 0.01, 0.1, 0);
    spin = gwy_table_attach_spinbutton(table, 0, _("Scale by ratio:"), "",
                                       controls.ratio);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    g_object_set_data(G_OBJECT(controls.ratio), "controls", &controls);
    g_signal_connect(controls.ratio, "value_changed",
                     G_CALLBACK(scale_changed_cb), args);

    controls.xres = gtk_adjustment_new(args->ratio*args->xres,
                                       1, 10000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 1, _("New width:"), _("px"),
                                       controls.xres);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
    g_object_set_data(G_OBJECT(controls.xres), "controls", &controls);
    g_signal_connect(controls.xres, "value_changed",
                     G_CALLBACK(width_changed_cb), args);

    controls.yres = gtk_adjustment_new(args->ratio*args->yres,
                                       1, 10000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 2, _("New height:"), _("px"),
                                       controls.yres);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
    g_object_set_data(G_OBJECT(controls.yres), "controls", &controls);
    g_signal_connect(controls.yres, "value_changed",
                     G_CALLBACK(height_changed_cb), args);

    controls.interp
        = gwy_interpolation_option_menu(G_CALLBACK(interp_changed_cb),
                                        args, args->interp);
    gwy_table_attach_row(table, 3, _("Interpolation type:"), "",
                         controls.interp);

    controls.in_update = FALSE;

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
            args->ratio = scale_defaults.ratio;
            args->interp = scale_defaults.interp;
            scale_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->ratio = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.ratio));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
interp_changed_cb(GObject *item,
                  ScaleArgs *args)
{
    args->interp = GPOINTER_TO_INT(g_object_get_data(item,
                                                     "interpolation-type"));
}

static void
scale_changed_cb(GtkAdjustment *adj,
                 ScaleArgs *args)
{
    ScaleControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->ratio = gtk_adjustment_get_value(adj);
    scale_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
width_changed_cb(GtkAdjustment *adj,
                 ScaleArgs *args)
{
    ScaleControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->ratio = gtk_adjustment_get_value(adj)/args->xres;
    scale_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
height_changed_cb(GtkAdjustment *adj,
                  ScaleArgs *args)
{
    ScaleControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->ratio = gtk_adjustment_get_value(adj)/args->yres;
    scale_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static const gchar *ratio_key = "/module/scale/ratio";
static const gchar *interp_key = "/module/scale/interp";

static void
scale_load_args(GwyContainer *container,
                ScaleArgs *args)
{
    *args = scale_defaults;

    if (gwy_container_contains_by_name(container, ratio_key))
        args->ratio = gwy_container_get_double_by_name(container, ratio_key);
    if (gwy_container_contains_by_name(container, interp_key))
        args->interp = gwy_container_get_int32_by_name(container, interp_key);
}

static void
scale_save_args(GwyContainer *container,
                ScaleArgs *args)
{
    gwy_container_set_double_by_name(container, ratio_key, args->ratio);
    gwy_container_set_int32_by_name(container, interp_key, args->interp);
}

static void
scale_dialog_update(ScaleControls *controls,
                    ScaleArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ratio),
                             args->ratio);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xres),
                             ROUND(args->ratio*args->xres));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yres),
                             ROUND(args->ratio*args->yres));
    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
