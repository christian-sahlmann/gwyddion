/* @(#) $Id$ */

#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include "tools.h"

typedef struct {
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *w;
    GtkWidget *h;
    gdouble mag;
    gint precision;
    gchar *units;
} CropControls;

static GtkWidget* crop_dialog_create            (GwyDataView *data_view);
static void       crop_do                       (void);
static void       crop_selection_finished_cb    (void);
static void       crop_dialog_response_cb       (gpointer unused,
                                                 gint response);
static void       crop_dialog_abandon           (void);

static GtkWidget *dialog;
static CropControls controls;
static gulong finished_id = 0;
static gulong response_id = 0;
static GwyDataViewLayer *select_layer = NULL;

void
gwy_tools_crop_use(GwyDataWindow *data_window)
{
    GwyDataViewLayer *layer;
    GwyDataView *data_view;

    gwy_debug("%s", __FUNCTION__);

    if (!data_window) {
        crop_dialog_abandon();
        return;
    }
    if (select_layer && finished_id)
        g_signal_handler_disconnect(select_layer, finished_id);

    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = (GwyDataView*)gwy_data_window_get_data_view(data_window);
    layer = gwy_data_view_get_top_layer(data_view);
    if (layer && GWY_IS_LAYER_SELECT(layer)) {
        select_layer = layer;
    }
    else {
        select_layer = (GwyDataViewLayer*)gwy_layer_select_new();
        gwy_data_view_set_top_layer(data_view, select_layer);
    }
    if (!dialog)
        dialog = crop_dialog_create(data_view);
    finished_id = g_signal_connect(select_layer, "finished",
                                   G_CALLBACK(crop_selection_finished_cb),
                                   NULL);
}

static void
crop_do(void)
{
    gdouble x0, y0, x1, y1;

    if (!gwy_layer_select_get_selection(select_layer, &x0, &y0, &x1, &y1))
        return;
    g_warning("Implement me!");
}

static void
crop_dialog_abandon(void)
{
    if (select_layer && finished_id)
        g_signal_handler_disconnect(select_layer, finished_id);
    finished_id = 0;
    select_layer = NULL;
    if (dialog) {
        g_signal_handler_disconnect(dialog, response_id);
        gtk_widget_destroy(dialog);
        dialog = NULL;
        response_id = 0;
        g_free(controls.units);
    }
}

static GtkWidget*
crop_dialog_create(GwyDataView *data_view)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GtkWidget *dialog, *table, *label;
    gdouble xreal, yreal, max, unit;

    gwy_debug("%s", __FUNCTION__);
    data = gwy_data_view_get_data(data_view);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(dfield),
               yreal/gwy_data_field_get_yres(dfield));
    controls.mag = gwy_math_humanize_numbers(unit, max, &controls.precision);
    controls.units = g_strconcat(gwy_math_SI_prefix(controls.mag), "m", NULL);

    dialog = gtk_dialog_new_with_buttons("Crop",
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    g_signal_connect(dialog, "delete_event",
                     G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);
    response_id = g_signal_connect(dialog, "response",
                                   G_CALLBACK(crop_dialog_response_cb), NULL);
    table = gtk_table_new(6, 3, FALSE);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    controls.x = gtk_label_new("");
    controls.y = gtk_label_new("");
    controls.w = gtk_label_new("");
    controls.h = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.x), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.y), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.w), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.h), 1.0, 0.5);
    label = gtk_label_new("");
    gwy_table_attach_row(table, 0, "Origin", "", label);
    gwy_table_attach_row(table, 1, "X", controls.units, controls.x);
    gwy_table_attach_row(table, 2, "Y", controls.units, controls.y);
    label = gtk_label_new("");
    gwy_table_attach_row(table, 3, "Size", "", label);
    gwy_table_attach_row(table, 4, "Width", controls.units, controls.w);
    gwy_table_attach_row(table, 5, "Height", controls.units, controls.h);
    gtk_widget_show_all(table);

    return dialog;
}

static void
crop_selection_finished_cb(void)
{
    gdouble x0, y0, x1, y1;
    gchar buffer[64];

    gwy_debug("%s", __FUNCTION__);
    if (!gwy_layer_select_get_selection(select_layer, &x0, &y0, &x1, &y1))
        return;
    if (!GTK_WIDGET_VISIBLE(dialog))
        gtk_window_present(GTK_WINDOW(dialog));
    g_snprintf(buffer, sizeof(buffer), "%.*f",
               controls.precision, MIN(x0, x1)/controls.mag);
    gtk_label_set_text(GTK_LABEL(controls.x), buffer);
    g_snprintf(buffer, sizeof(buffer), "%.*f",
               controls.precision, MIN(y0, y1)/controls.mag);
    gtk_label_set_text(GTK_LABEL(controls.y), buffer);
    g_snprintf(buffer, sizeof(buffer), "%.*f",
               controls.precision, fabs(x1 - x0)/controls.mag);
    gtk_label_set_text(GTK_LABEL(controls.w), buffer);
    g_snprintf(buffer, sizeof(buffer), "%.*f",
               controls.precision, fabs(y1 - y0)/controls.mag);
    gtk_label_set_text(GTK_LABEL(controls.h), buffer);
}

static void
crop_dialog_response_cb(gpointer unused, gint response)
{
    gwy_debug("%s: response %d", __FUNCTION__, response);
    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        gtk_widget_hide(dialog);
        break;

        case GTK_RESPONSE_NONE:
        gwy_tools_crop_use(NULL);
        break;

        case GTK_RESPONSE_APPLY:
        crop_do();
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

