/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/app.h>

typedef struct {
    gboolean is_visible;  /* XXX: GTK_WIDGET_VISIBLE() returns BS? */
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *w;
    GtkWidget *h;
    GtkWidget *ra;
    GtkWidget *rms;
    GtkWidget *skew;
    GtkWidget *kurtosis;
    GtkWidget *avg;
    gdouble mag;
    gint precision;
    gchar *units;
} StatsControls;

static gboolean   module_register               (const gchar *name);
static void       stats_use                      (GwyDataWindow *data_window,
                                                 GwyToolSwitchEvent reason);
static GtkWidget* stats_dialog_create            (GwyDataView *data_view);
static void       stats_do                       (void);
static void       stats_selection_finished_cb    (void);
static void       stats_dialog_response_cb       (gpointer unused,
                                                 gint response);
static void       stats_dialog_abandon           (void);
static void       stats_dialog_set_visible       (gboolean visible);

static GtkWidget *stats_dialog = NULL;
static StatsControls controls;
static gulong finished_id = 0;
static gulong response_id = 0;
static GwyDataViewLayer *select_layer = NULL;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "stats",
    "Statistical quantities.",
    "pk <petr@klapetek.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo stats_func_info = {
        "stats",
        "gwy_graph_halfgauss",
        "Statistical quantities",
        67,
        stats_use,
    };

    gwy_tool_func_register(name, &stats_func_info);

    return TRUE;
}

static void
stats_use(GwyDataWindow *data_window,
          G_GNUC_UNUSED GwyToolSwitchEvent reason)
{
    GwyDataViewLayer *layer;
    GwyDataView *data_view;

    gwy_debug("%p", data_window);

    if (!data_window) {
        stats_dialog_abandon();
        return;
    }
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = (GwyDataView*)gwy_data_window_get_data_view(data_window);
    layer = gwy_data_view_get_top_layer(data_view);
/*    if (layer && layer == select_layer)
        return;*/
    if (select_layer && finished_id)
        g_signal_handler_disconnect(select_layer, finished_id);

    if (layer && GWY_IS_LAYER_SELECT(layer))
        select_layer = layer;
    else {
        select_layer = (GwyDataViewLayer*)gwy_layer_select_new();
        gwy_data_view_set_top_layer(data_view, select_layer);
    }
    /*gwy_layer_select_set_is_crop(select_layer, TRUE);*/
    if (!stats_dialog)
        stats_dialog = stats_dialog_create(data_view);

    finished_id = g_signal_connect(select_layer, "updated",
                                   G_CALLBACK(stats_selection_finished_cb),
                                   NULL);
    stats_selection_finished_cb();
}

static void
stats_do(void)
{
    GtkWidget *data_window;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble xmin, ymin, xmax, ymax;
    gdouble avg, ra, rms, skew, kurtosis;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(select_layer->parent));
    gwy_app_clean_up_data(data);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    

    if (gwy_layer_select_get_selection(select_layer,
                                        &xmin, &ymin, &xmax, &ymax))
    {
        gwy_data_field_get_area_stats(dfield, gwy_data_field_rtoj(dfield, xmin), 
                                      gwy_data_field_rtoj(dfield, ymin), 
                                      gwy_data_field_rtoj(dfield, xmax), 
                                      gwy_data_field_rtoj(dfield, ymax), 
                                      &avg, &ra, &rms, &skew, &kurtosis);
    }
    else
    {
        gwy_data_field_get_stats(dfield, &avg, &ra, &rms, &skew, &kurtosis);
        xmin = ymin = 0; 
        xmax = gwy_data_field_get_xreal(dfield);
        ymax = gwy_data_field_get_yreal(dfield);
    }
    
    
}

static void
stats_dialog_abandon(void)
{
    gwy_debug("");
    if (select_layer && finished_id)
        g_signal_handler_disconnect(select_layer, finished_id);
    finished_id = 0;
    select_layer = NULL;
    if (stats_dialog) {
        g_signal_handler_disconnect(stats_dialog, response_id);
        gtk_widget_destroy(stats_dialog);
        stats_dialog = NULL;
        response_id = 0;
        g_free(controls.units);
        controls.is_visible = FALSE;
    }
}

static GtkWidget*
stats_dialog_create(GwyDataView *data_view)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GtkWidget *dialog, *table, *label;
    gdouble xreal, yreal, max, unit;

    gwy_debug("");
    data = gwy_data_view_get_data(data_view);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(dfield),
               yreal/gwy_data_field_get_yres(dfield));
    controls.mag = gwy_math_humanize_numbers(unit, max, &controls.precision);
    controls.units = g_strconcat(gwy_math_SI_prefix(controls.mag), "m", NULL);

    dialog = gtk_dialog_new_with_buttons(_("Statistical quantities"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    g_signal_connect(dialog, "delete_event",
                     G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);
    response_id = g_signal_connect(dialog, "response",
                                   G_CALLBACK(stats_dialog_response_cb), NULL);
    table = gtk_table_new(12, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Parameters</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Ra"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Rms"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Skew"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 3, 4, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Kurtosis"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("Average height"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 5, 6, GTK_FILL, 0, 2, 2);
    
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Origin</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 6, 7, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("X"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 7, 8, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Y"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 8, 9, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Size</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 9, 10, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Width"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 10, 11, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Height"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 11, 12, GTK_FILL, 0, 2, 2);

    controls.x = gtk_label_new("");
    controls.y = gtk_label_new("");
    controls.w = gtk_label_new("");
    controls.h = gtk_label_new("");
    controls.ra = gtk_label_new("");
    controls.rms = gtk_label_new("");
    controls.avg = gtk_label_new("");
    controls.skew = gtk_label_new("");
    controls.kurtosis = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.x), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.y), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.w), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.h), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.ra), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.rms), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.avg), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.skew), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.kurtosis), 1.0, 0.5);     
    gtk_table_attach_defaults(GTK_TABLE(table), controls.x, 2, 3, 7, 8);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.y, 2, 3, 8, 9);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.w, 2, 3, 10, 11);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.h, 2, 3, 11, 12);
    
    gtk_table_attach_defaults(GTK_TABLE(table), controls.ra, 2, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.rms, 2, 3, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.skew, 2, 3, 3, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.kurtosis, 2, 3, 4, 5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.avg, 2, 3, 5, 6);

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
stats_selection_finished_cb(void)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble xmin, ymin, xmax, ymax;
    gboolean is_visible, is_selected;
    gdouble avg, ra, rms, skew, kurtosis;    
    gchar buffer[16];

    gwy_debug("");
    /*XXX: seems broken
     * is_visible = GTK_WIDGET_VISIBLE(stats_dialog);*/
    is_visible = controls.is_visible;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(select_layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    if (is_selected=gwy_layer_select_get_selection(select_layer, &xmin, &ymin, &xmax, &ymax))
    {
        gwy_data_field_get_area_stats(dfield, gwy_data_field_rtoj(dfield, xmin), 
                                      gwy_data_field_rtoj(dfield, ymin), 
                                      gwy_data_field_rtoj(dfield, xmax), 
                                      gwy_data_field_rtoj(dfield, ymax), 
                                      &avg, &ra, &rms, &skew, &kurtosis);
    }
    else
        gwy_data_field_get_stats(dfield, &avg, &ra, &rms, &skew, &kurtosis);
    
    g_snprintf(buffer, sizeof(buffer), "%2.3e", ra);
    gtk_label_set_text(GTK_LABEL(controls.ra), buffer); 
    g_snprintf(buffer, sizeof(buffer), "%2.3e", rms);
    gtk_label_set_text(GTK_LABEL(controls.rms), buffer); 
    g_snprintf(buffer, sizeof(buffer), "%2.3e", skew);
    gtk_label_set_text(GTK_LABEL(controls.skew), buffer); 
    g_snprintf(buffer, sizeof(buffer), "%2.3e", kurtosis);
    gtk_label_set_text(GTK_LABEL(controls.kurtosis), buffer); 
    g_snprintf(buffer, sizeof(buffer), "%2.3e", avg);
    gtk_label_set_text(GTK_LABEL(controls.avg), buffer); 
     
    
 /*   if (!is_visible && !is_selected)
        return;*/
    if (is_selected) {
        update_label(controls.x, MIN(xmin, xmax));
        update_label(controls.y, MIN(ymin, ymax));
        update_label(controls.w, fabs(xmax - xmin));
        update_label(controls.h, fabs(ymax - ymin));
    }
    else {
        update_label(controls.x, 0);
        update_label(controls.y, 0);
        update_label(controls.w, gwy_data_field_get_xreal(dfield));
        update_label(controls.h, gwy_data_field_get_yreal(dfield));
    }
    if (!is_visible)
        stats_dialog_set_visible(TRUE);
}

static void
stats_dialog_response_cb(G_GNUC_UNUSED gpointer unused,
                        gint response)
{
    gwy_debug("response %d", response);
    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        stats_dialog_set_visible(FALSE);
        break;

        case GTK_RESPONSE_NONE:
        g_warning("Tool dialog destroyed.");
        stats_use(NULL, 0);
        break;

        case GTK_RESPONSE_APPLY:
        stats_do();
        stats_dialog_set_visible(FALSE);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
stats_dialog_set_visible(gboolean visible)
{
    gwy_debug("now %d, setting to %d",
              controls.is_visible, visible);
    if (controls.is_visible == visible)
        return;

    controls.is_visible = visible;
    if (visible)
        gtk_window_present(GTK_WINDOW(stats_dialog));
    else
        gtk_widget_hide(stats_dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

