/* @(#) $Id$ */

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#define GWY_RUN_ANY \
    (GWY_RUN_INTERACTIVE | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

static gboolean    module_register            (const gchar *name);
static gboolean    rotate                     (GwyContainer *data,
                                               GwyRunType run);
static gboolean    rotate_dialog              (GwyContainer *data);

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
        "/_Basic Operations/Rotate by _angle...",
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

    gwy_debug("%s", __FUNCTION__);
    g_assert(run & (GWY_RUN_INTERACTIVE | GWY_RUN_NONINTERACTIVE));
    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
    dfield = (GwyDataField*)gwy_container_get_object_by_name(data, "/0/data");
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);

    if (run == GWY_RUN_INTERACTIVE)
        return rotate_dialog(data);

    return TRUE;
}

static gboolean
rotate_dialog(GwyContainer *data)
{
    GwyDataField *dfield;
    gpointer data_weak;
    GtkWidget *dialog, *hbox, *widget;
    GtkAdjustment *adj;
    gdouble angle;

    g_object_add_weak_pointer(G_OBJECT(data), &data_weak);
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
    /* TODO: remember */
    adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, -360, 360, 5, 30, 0));
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

    angle = gtk_adjustment_get_value(GTK_ADJUSTMENT(adj));
    gtk_widget_destroy(dialog);
    if (!data_weak)
        return FALSE;
    dfield = (GwyDataField*)gwy_container_get_object_by_name(data, "/0/data");
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);
    gwy_data_field_rotate(dfield, angle, GWY_INTERPOLATION_BILINEAR);
    g_object_remove_weak_pointer(G_OBJECT(data), &data_weak);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
