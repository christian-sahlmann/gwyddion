/* @(#) $Id$ */

#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/file.h>
#include <app/app.h>
#include "tools.h"

typedef struct {
    gboolean is_visible;  /* GTK_WIDGET_VISIBLE() returns BS? */
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
static void       crop_dialog_set_visible       (gboolean visible);

static GtkWidget *dialog;
static CropControls controls;
static gulong finished_id = 0;
static gulong response_id = 0;
static GwyDataViewLayer *select_layer = NULL;

void
gwy_tool_crop_use(GwyDataWindow *data_window)
{
    GwyDataViewLayer *layer;
    GwyDataView *data_view;

    gwy_debug("%s: %p", __FUNCTION__, data_window);

    if (!data_window) {
        crop_dialog_abandon();
        return;
    }
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = (GwyDataView*)gwy_data_window_get_data_view(data_window);
    layer = gwy_data_view_get_top_layer(data_view);
    if (layer && layer == select_layer)
        return;
    if (select_layer && finished_id)
        g_signal_handler_disconnect(select_layer, finished_id);

    if (layer && GWY_IS_LAYER_SELECT(layer))
        select_layer = layer;
    else {
        select_layer = (GwyDataViewLayer*)gwy_layer_select_new();
        gwy_data_view_set_top_layer(data_view, select_layer);
    }
    if (!dialog)
        dialog = crop_dialog_create(data_view);

    finished_id = g_signal_connect(select_layer, "finished",
                                   G_CALLBACK(crop_selection_finished_cb),
                                   NULL);
    crop_selection_finished_cb();
}

static void
crop_do(void)
{
    GtkWidget *data_window;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble x0, y0, x1, y1;

    if (!gwy_layer_select_get_selection(select_layer, &x0, &y0, &x1, &y1))
        return;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(select_layer->parent));
    data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    gwy_app_clean_up_data(data);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    x0 = gwy_data_field_rtoj(dfield, x0);
    y0 = gwy_data_field_rtoi(dfield, y0);
    x1 = gwy_data_field_rtoj(dfield, x1) + 1;
    y1 = gwy_data_field_rtoi(dfield, y1) + 1;
    gwy_data_field_resize(dfield, y0, x0, y1, x1);
    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window));
    gwy_data_view_update(GWY_DATA_VIEW(select_layer->parent));
    gwy_debug("%s: %d %d", __FUNCTION__,
              gwy_data_field_get_xres(dfield), gwy_data_field_get_yres(dfield));
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
        controls.is_visible = FALSE;
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

    dialog = gtk_dialog_new_with_buttons(_("Crop"),
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
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Origin</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("X"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Y"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Size</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Width"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Height"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 5, 6, GTK_FILL, 0, 2, 2);

    controls.x = gtk_label_new("");
    controls.y = gtk_label_new("");
    controls.w = gtk_label_new("");
    controls.h = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.x), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.y), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.w), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.h), 1.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.x, 2, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.y, 2, 3, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.w, 2, 3, 4, 5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.h, 2, 3, 5, 6);
    gtk_widget_show_all(table);
    controls.is_visible = FALSE;

    return dialog;
}

static void
update_label(GtkWidget *label, gdouble value)
{
    gchar buffer[16];

    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
               controls.precision, value/controls.mag, controls.units);
    gtk_label_set_text(GTK_LABEL(label), buffer);
}

static void
crop_selection_finished_cb(void)
{
    gdouble x0, y0, x1, y1;
    gboolean is_visible, is_selected;

    gwy_debug("%s", __FUNCTION__);
    /*XXX: seems broken
     * is_visible = GTK_WIDGET_VISIBLE(dialog);*/
    is_visible = controls.is_visible;
    is_selected = gwy_layer_select_get_selection(select_layer,
                                                 &x0, &y0, &x1, &y1);
    if (!is_visible && !is_selected)
        return;
    if (is_selected) {
        update_label(controls.x, MIN(x0, x1));
        update_label(controls.y, MIN(y0, y1));
        update_label(controls.w, fabs(x1 - x0));
        update_label(controls.h, fabs(y1 - y0));
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls.x), "");
        gtk_label_set_text(GTK_LABEL(controls.y), "");
        gtk_label_set_text(GTK_LABEL(controls.w), "");
        gtk_label_set_text(GTK_LABEL(controls.h), "");
    }
    if (!is_visible)
        crop_dialog_set_visible(TRUE);
}

static void
crop_dialog_response_cb(gpointer unused, gint response)
{
    gwy_debug("%s: response %d", __FUNCTION__, response);
    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        crop_dialog_set_visible(FALSE);
        break;

        case GTK_RESPONSE_NONE:
        gwy_tool_crop_use(NULL);
        break;

        case GTK_RESPONSE_APPLY:
        crop_do();
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
crop_dialog_set_visible(gboolean visible)
{
    gwy_debug("%s: now %d, setting to %d",
              __FUNCTION__, controls.is_visible, visible);
    if (controls.is_visible == visible)
        return;

    controls.is_visible = visible;
    if (visible)
        gtk_window_present(GTK_WINDOW(dialog));
    else
        gtk_widget_hide(dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

