/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/arithmetic.h>
#include <libprocess/correlation.h>
#include <libdraw/gwypixfield.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define IMMERSE_RUN_MODES GWY_RUN_INTERACTIVE

/* Some empirical factors */

/* Search window of improve for kernel dimension @k and image dimension @i */
#define improve_search_window(k, i) GWY_ROUND(1.0/(2.0/(k) + 6.0/(i)))

/* Universal downsample factor to get approximately optimal run time in
 * two-stage search */
#define downsample_factor 0.18

/* But don't downsample kernels below certain size (in pixels) */
#define downsample_limit 20

enum {
    PREVIEW_SIZE = 400
};

typedef enum {
    GWY_IMMERSE_SAMPLING_UP,
    GWY_IMMERSE_SAMPLING_DOWN,
    GWY_IMMERSE_SAMPLING_LAST
} GwyImmerseSamplingType;

typedef enum {
    GWY_IMMERSE_LEVEL_NONE,
    GWY_IMMERSE_LEVEL_MEAN,
    GWY_IMMERSE_LEVEL_LAST
} GwyImmerseLevelType;

typedef enum {
    IMMERSE_RESPONSE_IMPROVE = 1,
    IMMERSE_RESPONSE_LOCATE
} ImmerseResponseType;

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    GwyImmerseSamplingType sampling;
    GwyImmerseLevelType leveling;
    gboolean draw_frame;
    GwyDataObjectId image;
    GwyDataObjectId detail;
    /* Interface only */
    gdouble xpos;
    gdouble ypos;
} ImmerseArgs;

typedef struct {
    ImmerseArgs *args;
    GtkWidget *dialog;
    GwyContainer *mydata;
    GtkWidget *view;
    GtkWidget *pos;
    GSList *sampling;
    GSList *leveling;
    GtkWidget *draw_frame;
    GdkPixbuf *detail;
    GwySIValueFormat *vf;
    gdouble xmax;
    gdouble ymax;
    gint xc;
    gint yc;
    gint button;
    GdkCursor *near_cursor;
    GdkCursor *move_cursor;
} ImmerseControls;

static gboolean module_register             (void);
static void     immerse                     (GwyContainer *data,
                                             GwyRunType run);
static gboolean immerse_dialog              (ImmerseArgs *args);
static void     immerse_controls_destroy    (ImmerseControls *controls);
static void     immerse_detail_cb           (GwyDataChooser *chooser,
                                             ImmerseControls *controls);
static void     immerse_update_detail_pixbuf(ImmerseControls *controls);
static gboolean immerse_data_filter         (GwyContainer *data,
                                             gint id,
                                             gpointer user_data);
static void     immerse_search              (ImmerseControls *controls,
                                             ImmerseResponseType search_type);
static void     immerse_correlate           (GwyDataField *image,
                                             GwyDataField *kernel,
                                             gint *col,
                                             gint *row);
static void     immerse_find_maximum        (GwyDataField *score,
                                             gint *col,
                                             gint *row);
static void     immerse_do                  (ImmerseArgs *args);
static void     immerse_sampling_changed    (GtkWidget *button,
                                             ImmerseControls *controls);
static void     immerse_leveling_changed    (GtkWidget *button,
                                             ImmerseControls *controls);
static void     immerse_frame_toggled       (GtkToggleButton *check,
                                             ImmerseControls *controls);
static gboolean immerse_view_expose         (GtkWidget *view,
                                             GdkEventExpose *event,
                                             ImmerseControls *controls);
static gboolean immerse_view_button_press   (GtkWidget *view,
                                             GdkEventButton *event,
                                             ImmerseControls *controls);
static gboolean immerse_view_button_release (GtkWidget *view,
                                             GdkEventButton *event,
                                             ImmerseControls *controls);
static gboolean immerse_view_motion_notify  (GtkWidget *view,
                                             GdkEventMotion *event,
                                             ImmerseControls *controls);
static gboolean immerse_view_inside_detail  (ImmerseControls *controls,
                                             gint x,
                                             gint y);
static gboolean immerse_clamp_detail_offset (ImmerseControls *controls,
                                             gdouble xpos,
                                             gdouble ypos);
static void     immerse_load_args           (GwyContainer *settings,
                                             ImmerseArgs *args);
static void     immerse_save_args           (GwyContainer *settings,
                                             ImmerseArgs *args);
static void     immerse_sanitize_args       (ImmerseArgs *args);

static const ImmerseArgs immerse_defaults = {
    GWY_IMMERSE_SAMPLING_UP,
    GWY_IMMERSE_LEVEL_MEAN,
    TRUE,
    { NULL, -1 },
    { NULL, -1 },
    /* Ensure initial pos update */
    -1e38,
    -1e38,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Immerse high resolution detail into overall image."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("immerse",
                              (GwyProcessFunc)&immerse,
                              N_("/M_ultidata/_Immerse Detail..."),
                              GWY_STOCK_IMMERSE,
                              IMMERSE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Immerse a detail into image"));

    return TRUE;
}

static void
immerse(GwyContainer *data, GwyRunType run)
{
    ImmerseArgs args;
    GwyContainer *settings;

    g_return_if_fail(run & IMMERSE_RUN_MODES);

    settings = gwy_app_settings_get();
    immerse_load_args(settings, &args);

    args.image.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &args.image.id, 0);
    args.detail.data = NULL;

    if (immerse_dialog(&args))
        immerse_do(&args);

    immerse_save_args(settings, &args);
}

static gboolean
immerse_dialog(ImmerseArgs *args)
{
    ImmerseControls controls;
    GtkDialog *dialog;
    GtkWidget *table, *chooser, *hbox, *alignment, *label, *button, *vbox;
    GtkTooltips *tooltips;
    GdkDisplay *display;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    gint response, row, id;
    gdouble zoomval;
    gboolean ok;

    memset(&controls, 0, sizeof(ImmerseControls));
    controls.args = args;

    tooltips = gwy_app_get_tooltips();

    controls.dialog = gtk_dialog_new_with_buttons(_("Immerse Detail"),
                                                  NULL, 0, NULL);
    dialog = GTK_DIALOG(controls.dialog);
    gtk_dialog_set_has_separator(dialog, FALSE);
    button = gtk_dialog_add_button(dialog,
                                   _("_Locate"), IMMERSE_RESPONSE_LOCATE);
    gtk_tooltips_set_tip(tooltips, button,
                         _("Locate detail by full image correlation search"),
                         NULL);
    button = gtk_dialog_add_button(dialog,
                                   _("_Improve"), IMMERSE_RESPONSE_IMPROVE);
    gtk_tooltips_set_tip(tooltips, button,
                         _("Improve detail position by "
                           "correlation search in neighborhood"), NULL);
    gtk_dialog_add_button(dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(dialog, GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), hbox, FALSE, FALSE, 4);

    /* Preview */
    id = args->image.id;
    dfield = gwy_container_get_object(args->image.data,
                                      gwy_app_get_data_key_for_id(id));
    controls.vf
        = gwy_data_field_get_value_format_xy(dfield,
                                             GWY_SI_UNIT_FORMAT_VFMARKUP, NULL);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(args->image.data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            0);
    gwy_container_set_boolean_by_name(controls.mydata, "/0/data/realsquare",
                                      TRUE);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    /* XXX: This is wrong with realsquare=TRUE */
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    alignment = GTK_WIDGET(gtk_alignment_new(0.5, 0, 0, 0));
    gtk_container_add(GTK_CONTAINER(alignment), controls.view);
    gtk_box_pack_start(GTK_BOX(hbox), alignment, FALSE, FALSE, 4);

    g_signal_connect_after(controls.view, "expose-event",
                           G_CALLBACK(immerse_view_expose), &controls);
    g_signal_connect(controls.view, "button-press-event",
                     G_CALLBACK(immerse_view_button_press), &controls);
    g_signal_connect(controls.view, "button-release-event",
                     G_CALLBACK(immerse_view_button_release), &controls);
    g_signal_connect(controls.view, "motion-notify-event",
                     G_CALLBACK(immerse_view_motion_notify), &controls);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    /* Parameters table */
    table = gtk_table_new(8, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 4);
    row = 0;

    /* Detail to immerse */
    chooser = gwy_data_chooser_new_channels();
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(chooser),
                                immerse_data_filter, &args->image, NULL);
    g_signal_connect(chooser, "changed",
                     G_CALLBACK(immerse_detail_cb), &controls);
    gwy_table_attach_hscale(table, row, _("_Detail image:"), NULL,
                            GTK_OBJECT(chooser), GWY_HSCALE_WIDGET_NO_EXPAND);
    row++;

    /* Detail position */
    label = gtk_label_new(_("Position:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    controls.pos = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.pos), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.pos,
                     1, 3, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /* Sampling */
    label = gtk_label_new(_("Result Sampling"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.sampling
        = gwy_radio_buttons_createl(G_CALLBACK(immerse_sampling_changed),
                                    &controls,
                                    args->sampling,
                                    _("_Upsample large image"),
                                    GWY_IMMERSE_SAMPLING_UP,
                                    _("_Downsample detail"),
                                    GWY_IMMERSE_SAMPLING_DOWN,
                                    NULL);

    row = gwy_radio_buttons_attach_to_table(controls.sampling, GTK_TABLE(table),
                                            4, row);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    /* Leveling */
    label = gtk_label_new(_("Detail Leveling"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.leveling
        = gwy_radio_buttons_createl(G_CALLBACK(immerse_leveling_changed),
                                    &controls,
                                    args->leveling,
                                    _("_None"), GWY_IMMERSE_LEVEL_NONE,
                                    _("_Mean value"), GWY_IMMERSE_LEVEL_MEAN,
                                    NULL);

    row = gwy_radio_buttons_attach_to_table(controls.leveling, GTK_TABLE(table),
                                            4, row);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    /* Draw frame */
    controls.draw_frame = gtk_check_button_new_with_mnemonic(_("Show _frame"));
    gtk_table_attach(GTK_TABLE(table), controls.draw_frame,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.draw_frame),
                                 args->draw_frame);
    g_signal_connect(controls.draw_frame, "toggled",
                     G_CALLBACK(immerse_frame_toggled), &controls);

    gtk_widget_show_all(controls.dialog);
    display = gtk_widget_get_display(controls.dialog);
    controls.near_cursor = gdk_cursor_new_for_display(display, GDK_FLEUR);
    controls.move_cursor = gdk_cursor_new_for_display(display, GDK_CROSS);
    immerse_detail_cb(GWY_DATA_CHOOSER(chooser), &controls);

    ok = FALSE;
    do {
        response = gtk_dialog_run(dialog);
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            immerse_controls_destroy(&controls);
            return FALSE;
            break;

            case IMMERSE_RESPONSE_IMPROVE:
            case IMMERSE_RESPONSE_LOCATE:
            immerse_search(&controls, response);
            break;

            case GTK_RESPONSE_OK:
            ok = TRUE;
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    immerse_controls_destroy(&controls);

    return TRUE;
}

static void
immerse_controls_destroy(ImmerseControls *controls)
{
    gtk_widget_destroy(controls->dialog);
    gwy_si_unit_value_format_free(controls->vf);
    g_object_unref(controls->mydata);
    gdk_cursor_unref(controls->near_cursor);
    gdk_cursor_unref(controls->move_cursor);
    gwy_object_unref(controls->detail);
}

static void
immerse_detail_cb(GwyDataChooser *chooser,
                  ImmerseControls *controls)
{
    GwyDataField *dfield, *ifield;
    GQuark quark;
    GwyDataObjectId *object;

    object = &controls->args->detail;
    object->data = gwy_data_chooser_get_active(chooser, &object->id);
    gwy_debug("data: %p %d", object->data, object->id);

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      GTK_RESPONSE_OK,
                                      object->data != NULL);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      IMMERSE_RESPONSE_LOCATE,
                                      object->data != NULL);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      IMMERSE_RESPONSE_IMPROVE,
                                      object->data != NULL);

    if (object->data) {
        quark = gwy_app_get_data_key_for_id(object->id);
        dfield = gwy_container_get_object(object->data, quark);
        quark = gwy_app_get_data_key_for_id(controls->args->image.id);
        ifield = gwy_container_get_object(controls->args->image.data, quark);
        controls->xmax = (gwy_data_field_get_xreal(ifield)
                          - gwy_data_field_get_xreal(dfield)
                          + gwy_data_field_get_xmeasure(ifield)/2.0);
        controls->ymax = (gwy_data_field_get_yreal(ifield)
                          - gwy_data_field_get_yreal(dfield)
                          + gwy_data_field_get_ymeasure(ifield)/2.0);
    }

    immerse_update_detail_pixbuf(controls);
    immerse_clamp_detail_offset(controls,
                                controls->args->xpos, controls->args->ypos);
    if (GTK_WIDGET_DRAWABLE(controls->view))
        gtk_widget_queue_draw(controls->view);
}

static void
immerse_update_detail_pixbuf(ImmerseControls *controls)
{
    GwyDataField *dfield;
    GwyGradient *gradient;
    GdkPixbuf *pixbuf;
    const guchar *name;
    gchar *key;
    GQuark quark;
    gint w, h, xres, yres;

    gwy_object_unref(controls->detail);
    if (!controls->args->detail.data)
        return;

    quark = gwy_app_get_data_key_for_id(controls->args->detail.id);
    dfield = gwy_container_get_object(controls->args->detail.data, quark);
    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(controls->view),
                                    gwy_data_field_get_xreal(dfield),
                                    gwy_data_field_get_yreal(dfield),
                                    &w, &h);
    gwy_debug("%dx%d", w, h);
    w = MAX(w, 2);
    h = MAX(h, 2);

    key = g_strdup_printf("/%d/base/palette", controls->args->image.id);
    name = NULL;
    gwy_container_gis_string_by_name(controls->args->image.data, key, &name);
    g_free(key);
    gradient = gwy_gradients_get_gradient(name);

    /* Handle real-square properly by using an intermediate
     * pixel-square pixbuf with sufficient resolution */
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, xres, yres);
    gwy_pixbuf_draw_data_field(pixbuf, dfield, gradient);
    controls->detail = gdk_pixbuf_scale_simple(pixbuf, w, h, GDK_INTERP_TILES);
    g_object_unref(pixbuf);
}

static gboolean
immerse_data_filter(GwyContainer *data,
                    gint id,
                    gpointer user_data)
{
    GwyDataObjectId *object = (GwyDataObjectId*)user_data;
    GwyDataField *image, *detail;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    detail = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    quark = gwy_app_get_data_key_for_id(object->id);
    image = GWY_DATA_FIELD(gwy_container_get_object(object->data, quark));

    /* It does not make sense to immerse itself */
    if (detail == image)
        return FALSE;

    if (gwy_data_field_check_compatibility(image, detail,
                                           GWY_DATA_COMPATIBILITY_LATERAL
                                           | GWY_DATA_COMPATIBILITY_VALUE))
        return FALSE;

    if (gwy_data_field_get_xreal(image) < gwy_data_field_get_xreal(detail)
        || gwy_data_field_get_xreal(image) < gwy_data_field_get_xreal(detail))
        return FALSE;

    return TRUE;
}

static void
immerse_search(ImmerseControls *controls,
               ImmerseResponseType search_type)
{
    GwyDataField *dfield, *dfieldsub, *ifield, *iarea;
    gdouble wr, hr, xpos, ypos, deltax, deltay;
    gint w, h, xfrom, xto, yfrom, yto, ixres, iyres, col, row;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(controls->args->detail.id);
    dfield = gwy_container_get_object(controls->args->detail.data, quark);
    quark = gwy_app_get_data_key_for_id(controls->args->image.id);
    ifield = gwy_container_get_object(controls->args->image.data, quark);
    ixres = gwy_data_field_get_xres(ifield);
    iyres = gwy_data_field_get_yres(ifield);

    wr = gwy_data_field_get_xreal(dfield)/gwy_data_field_get_xmeasure(ifield);
    hr = gwy_data_field_get_yreal(dfield)/gwy_data_field_get_ymeasure(ifield);
    if (wr*hr < 6.0) {
        g_warning("Detail image is too small for correlation");
        return;
    }

    w = GWY_ROUND(MAX(wr, 1.0));
    h = GWY_ROUND(MAX(hr, 1.0));
    gwy_debug("w: %d, h: %d", w, h);
    g_assert(w <= ixres && h <= iyres);
    if (search_type == IMMERSE_RESPONSE_IMPROVE) {
        xfrom = gwy_data_field_rtoj(ifield, controls->args->xpos);
        yfrom = gwy_data_field_rtoi(ifield, controls->args->ypos);
        /* Calculate the area we will search the detail in */
        deltax = improve_search_window(w, ixres);
        deltay = improve_search_window(h, iyres);
        gwy_debug("deltax: %g, deltay: %g", deltax, deltay);
        xto = MIN(xfrom + w + deltax, ixres);
        yto = MIN(yfrom + h + deltay, iyres);
        xfrom = MAX(xfrom - deltax, 0);
        yfrom = MAX(yfrom - deltay, 0);
    }
    else {
        xfrom = yfrom = 0;
        xto = ixres;
        yto = iyres;
    }
    gwy_debug("x: %d..%d, y: %d..%d", xfrom, xto, yfrom, yto);

    /* Cut out only the interesting part from the image data field */
    if (xfrom == 0 && yfrom == 0 && xto == ixres && yto == iyres)
        iarea = g_object_ref(ifield);
    else
        iarea = gwy_data_field_area_extract(ifield,
                                            xfrom, yfrom,
                                            xto - xfrom, yto - yfrom);

    dfieldsub = gwy_data_field_new_resampled(dfield, w, h,
                                             GWY_INTERPOLATION_LINEAR);

    immerse_correlate(iarea, dfieldsub, &col, &row);
    gwy_debug("[c] col: %d, row: %d", col, row);
    col += xfrom;
    row += yfrom;
    xpos = gwy_data_field_jtor(dfieldsub, col + 0.5);
    ypos = gwy_data_field_itor(dfieldsub, row + 0.5);
    g_object_unref(iarea);
    g_object_unref(dfieldsub);
    gwy_debug("[C] col: %d, row: %d", col, row);

    /* Upsample and refine */
    xfrom = MAX(col - 1, 0);
    yfrom = MAX(row - 1, 0);
    xto = MIN(col + w + 1, ixres);
    yto = MIN(row + h + 1, iyres);
    gwy_debug("x: %d..%d, y: %d..%d", xfrom, xto, yfrom, yto);
    iarea = gwy_data_field_area_extract(ifield,
                                        xfrom, yfrom,
                                        xto - xfrom, yto - yfrom);
    wr = gwy_data_field_get_xreal(iarea)/gwy_data_field_get_xmeasure(dfield);
    hr = gwy_data_field_get_yreal(iarea)/gwy_data_field_get_ymeasure(dfield);
    gwy_data_field_resample(iarea, GWY_ROUND(wr), GWY_ROUND(hr),
                            GWY_INTERPOLATION_LINEAR);
    immerse_correlate(iarea, dfield, &col, &row);
    gwy_debug("[U] col: %d, row: %d", col, row);

    xpos = gwy_data_field_jtor(dfield, col + 0.5)
           + gwy_data_field_jtor(ifield, xfrom);
    ypos = gwy_data_field_itor(dfield, row + 0.5)
           + gwy_data_field_itor(ifield, yfrom);

    g_object_unref(iarea);
    immerse_clamp_detail_offset(controls, xpos, ypos);
}

static void
immerse_correlate(GwyDataField *image,
                  GwyDataField *kernel,
                  gint *col,
                  gint *row)
{
    GwyDataField *subimage, *subkernel, *score, *imagearea;
    gdouble factor;
    gint ixres, iyres, kxres, kyres;
    gint sixres, siyres, skxres, skyres;
    gint xfrom, yfrom, xto, yto;
    gint sx, sy, delta;

    ixres = gwy_data_field_get_xres(image);
    iyres = gwy_data_field_get_yres(image);
    kxres = gwy_data_field_get_xres(kernel);
    kyres = gwy_data_field_get_yres(kernel);
    gwy_debug("kernel: %dx%d, image: %dx%d", kxres, kyres, ixres, iyres);

    factor = MAX(downsample_factor, downsample_limit/sqrt(kxres*kyres));
    factor = MIN(factor, 1.0);

    skxres = GWY_ROUND(factor*kxres);
    skyres = GWY_ROUND(factor*kyres);
    sixres = GWY_ROUND(factor*ixres);
    siyres = GWY_ROUND(factor*iyres);
    gwy_debug("skernel: %dx%d, simage: %dx%d", skxres, skyres, sixres, siyres);

    subimage = gwy_data_field_new_resampled(image, sixres, siyres,
                                            GWY_INTERPOLATION_LINEAR);
    score = gwy_data_field_new_alike(subimage, FALSE);
    subkernel = gwy_data_field_new_resampled(kernel, skxres, skyres,
                                             GWY_INTERPOLATION_LINEAR);

    gwy_data_field_correlate(subimage, subkernel, score,
                             GWY_CORRELATION_NORMAL);
    immerse_find_maximum(score, &sx, &sy);
    gwy_debug("sx: %d, sy: %d", sx, sy);
    g_object_unref(score);
    g_object_unref(subkernel);
    g_object_unref(subimage);

    /* Top left corner coordinate */
    sx -= (skxres - 1)/2;
    sy -= (skyres - 1)/2;
    /* Upscaled to original size */
    sx = GWY_ROUND((gdouble)ixres/sixres*sx);
    sy = GWY_ROUND((gdouble)iyres/siyres*sy);
    /* Uncertainty margin */
    delta = GWY_ROUND(1.5/factor + 1);
    /* Subarea to search */
    xfrom = MAX(sx - delta, 0);
    yfrom = MAX(sy - delta, 0);
    xto = MIN(sx + kxres + delta, ixres);
    yto = MIN(sy + kyres + delta, iyres);

    imagearea = gwy_data_field_area_extract(image,
                                            xfrom, yfrom,
                                            xto - xfrom, yto - yfrom);
    score = gwy_data_field_new_alike(imagearea, FALSE);
    gwy_data_field_correlate(imagearea, kernel, score,
                             GWY_CORRELATION_NORMAL);
    immerse_find_maximum(score, &sx, &sy);
    g_object_unref(score);
    g_object_unref(imagearea);

    *col = sx + xfrom - (kxres - 1)/2;
    *row = sy + yfrom - (kyres - 1)/2;
}

static void
immerse_find_maximum(GwyDataField *score,
                     gint *col,
                     gint *row)
{
    const gdouble *d;
    gint i, n, m;

    d = gwy_data_field_get_data_const(score);
    n = gwy_data_field_get_xres(score)*gwy_data_field_get_yres(score);
    m = 0;
    for (i = 1; i < n; i++) {
        if (d[i] > d[m])
            m = i;
    }

    *row = m/gwy_data_field_get_xres(score);
    *col = m % gwy_data_field_get_xres(score);
}

static void
immerse_do(ImmerseArgs *args)
{
    GwyContainer *data;
    GwyDataField *resampled, *image, *detail, *result;
    gint newid;
    gint ixres, kxres, iyres, kyres;
    gint x, y, w, h;
    gdouble iavg, davg;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(args->image.id);
    image = GWY_DATA_FIELD(gwy_container_get_object(args->image.data, quark));

    quark = gwy_app_get_data_key_for_id(args->detail.id);
    detail = GWY_DATA_FIELD(gwy_container_get_object(args->detail.data, quark));
    davg = gwy_data_field_get_avg(detail);

    ixres = gwy_data_field_get_xres(image);
    kxres = gwy_data_field_get_xres(detail);
    iyres = gwy_data_field_get_yres(image);
    kyres = gwy_data_field_get_yres(detail);

    switch (args->sampling) {
        case GWY_IMMERSE_SAMPLING_DOWN:
        result = gwy_data_field_duplicate(image);
        x = gwy_data_field_rtoj(image, args->xpos);
        y = gwy_data_field_rtoi(image, args->ypos);
        w = GWY_ROUND(gwy_data_field_get_xreal(detail)
                      /gwy_data_field_get_xmeasure(image));
        h = GWY_ROUND(gwy_data_field_get_yreal(detail)
                      /gwy_data_field_get_ymeasure(image));
        w = MAX(w, 1);
        h = MAX(h, 1);
        gwy_debug("w: %d, h: %d", w, h);
        resampled = gwy_data_field_new_resampled(detail, w, h,
                                                 GWY_INTERPOLATION_LINEAR);
        if (args->leveling == GWY_IMMERSE_LEVEL_MEAN) {
            iavg = gwy_data_field_area_get_avg(result, NULL, x, y, w, h);
            gwy_data_field_add(resampled, iavg - davg);
        }
        gwy_data_field_area_copy(resampled, result, 0, 0, w, h, x, y);
        g_object_unref(resampled);
        break;

        case GWY_IMMERSE_SAMPLING_UP:
        w = GWY_ROUND(gwy_data_field_get_xreal(image)
                      /gwy_data_field_get_xmeasure(detail));
        h = GWY_ROUND(gwy_data_field_get_yreal(image)
                      /gwy_data_field_get_ymeasure(detail));
        gwy_debug("w: %d, h: %d", w, h);
        result = gwy_data_field_new_resampled(image, w, h,
                                              GWY_INTERPOLATION_LINEAR);
        x = gwy_data_field_rtoj(result, args->xpos);
        y = gwy_data_field_rtoi(result, args->ypos);
        if (args->leveling == GWY_IMMERSE_LEVEL_MEAN) {
            iavg = gwy_data_field_area_get_avg(result, NULL,
                                               x, y, kxres, kyres);
            gwy_data_field_area_copy(detail, result, 0, 0, kxres, kyres, x, y);
            gwy_data_field_area_add(result, x, y, kxres, kyres, iavg - davg);
        }
        else
            gwy_data_field_area_copy(detail, result, 0, 0, kxres, kyres, x, y);
        break;

        default:
        g_return_if_reached();
        break;
    }

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
    newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
    gwy_app_set_data_field_title(data, newid, _("Immersed detail"));
    g_object_unref(result);
}

static void
immerse_sampling_changed(G_GNUC_UNUSED GtkWidget *button,
                         ImmerseControls *controls)
{
    controls->args->sampling
        = gwy_radio_buttons_get_current(controls->sampling);
}

static void
immerse_leveling_changed(G_GNUC_UNUSED GtkWidget *button,
                         ImmerseControls *controls)
{
    controls->args->leveling
        = gwy_radio_buttons_get_current(controls->leveling);
}

static void
immerse_frame_toggled(GtkToggleButton *check,
                      ImmerseControls *controls)
{
    controls->args->draw_frame = gtk_toggle_button_get_active(check);
    gtk_widget_queue_draw(controls->view);
}

static gboolean
immerse_view_expose(GtkWidget *view,
                    GdkEventExpose *event,
                    ImmerseControls *controls)
{
    if (event->count > 0)
        return FALSE;

    if (controls->detail) {
        GdkColor white = { 0, 0xffff, 0xffff, 0xffff };
        GdkGC *gc;
        gint w, h, xoff, yoff;

        gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(view),
                                        controls->args->xpos,
                                        controls->args->ypos,
                                        &xoff, &yoff);
        w = gdk_pixbuf_get_width(controls->detail);
        h = gdk_pixbuf_get_height(controls->detail);
        /* gwy_debug("(%d,%d) %dx%d", xoff, yoff, w, h); */
        gc = gdk_gc_new(view->window);
        gdk_draw_pixbuf(view->window, gc, controls->detail,
                        0, 0, xoff, yoff, w, h,
                        GDK_RGB_DITHER_NORMAL, 0, 0);
        if (controls->args->draw_frame) {
            gdk_gc_set_function(gc, GDK_XOR);
            gdk_gc_set_rgb_fg_color(gc, &white);
            gdk_draw_rectangle(view->window, gc, FALSE, xoff, yoff, w-1, h-1);
        }
        g_object_unref(gc);
    }

    return FALSE;
}

static gboolean
immerse_view_button_press(GtkWidget *view,
                          GdkEventButton *event,
                          ImmerseControls *controls)
{
    gint xoff, yoff;

    if (event->button != 1
        || !immerse_view_inside_detail(controls, event->x, event->y))
        return FALSE;

    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(view),
                                    controls->args->xpos,
                                    controls->args->ypos,
                                    &xoff, &yoff);
    controls->button = event->button;
    /* Cursor offset wrt top left corner */
    controls->xc = event->x - xoff;
    controls->yc = event->y - yoff;
    gdk_window_set_cursor(view->window, controls->move_cursor);

    return TRUE;
}

static gboolean
immerse_view_button_release(GtkWidget *view,
                            GdkEventButton *event,
                            ImmerseControls *controls)
{
    gdouble xpos, ypos;

    if (event->button != controls->button)
        return FALSE;

    controls->button = 0;
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(view),
                                    event->x - controls->xc,
                                    event->y - controls->yc,
                                    &xpos,
                                    &ypos);
    immerse_clamp_detail_offset(controls, xpos, ypos);
    gdk_window_set_cursor(view->window, controls->near_cursor);

    return TRUE;
}

static gboolean
immerse_view_motion_notify(GtkWidget *view,
                           GdkEventMotion *event,
                           ImmerseControls *controls)
{
    GdkWindow *window;
    gint x, y;

    if (!controls->detail)
        return FALSE;

    window = view->window;
    if (event->is_hint)
        gdk_window_get_pointer(window, &x, &y, NULL);
    else {
        x = event->x;
        y = event->y;
    }

    if (!controls->button) {
        if (immerse_view_inside_detail(controls, x, y))
            gdk_window_set_cursor(window, controls->near_cursor);
        else
            gdk_window_set_cursor(window, NULL);
    }
    else {
        gdouble xpos, ypos;

        gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(view),
                                        x - controls->xc,
                                        y - controls->yc,
                                        &xpos, &ypos);
        immerse_clamp_detail_offset(controls, xpos, ypos);
    }

    return TRUE;
}

static gboolean
immerse_view_inside_detail(ImmerseControls *controls,
                           gint x, gint y)
{
    gint xoff, yoff;

    if (!controls->detail)
        return FALSE;

    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(controls->view),
                                    controls->args->xpos,
                                    controls->args->ypos,
                                    &xoff, &yoff);
    return (x >= xoff
            && x < xoff + gdk_pixbuf_get_width(controls->detail)
            && y >= yoff
            && y < yoff + gdk_pixbuf_get_height(controls->detail));
}

static gboolean
immerse_clamp_detail_offset(ImmerseControls *controls,
                            gdouble xpos, gdouble ypos)
{
    xpos = CLAMP(xpos, 0.0, controls->xmax);
    ypos = CLAMP(ypos, 0.0, controls->ymax);
    if (xpos != controls->args->xpos || ypos != controls->args->ypos) {
        gchar *s;

        controls->args->xpos = xpos;
        controls->args->ypos = ypos;
        s = g_strdup_printf("(%.*f, %.*f) %s",
                            controls->vf->precision + 1,
                            xpos/controls->vf->magnitude,
                            controls->vf->precision + 1,
                            ypos/controls->vf->magnitude,
                            controls->vf->units);
        gtk_label_set_text(GTK_LABEL(controls->pos), s);
        g_free(s);

        if (GTK_WIDGET_DRAWABLE(controls->view))
            gtk_widget_queue_draw(controls->view);
        return TRUE;
    }

    return FALSE;
}

static const gchar sampling_key[]   = "/module/immerse/sampling";
static const gchar leveling_key[]   = "/module/immerse/leveling";
static const gchar draw_frame_key[] = "/module/immerse/draw-frame";

static void
immerse_sanitize_args(ImmerseArgs *args)
{
    args->sampling = MIN(args->sampling, GWY_IMMERSE_SAMPLING_LAST - 1);
    args->leveling = MIN(args->leveling, GWY_IMMERSE_LEVEL_LAST - 1);
    args->draw_frame = !!args->draw_frame;
}

static void
immerse_load_args(GwyContainer *settings,
                   ImmerseArgs *args)
{
    *args = immerse_defaults;
    gwy_container_gis_enum_by_name(settings, sampling_key, &args->sampling);
    gwy_container_gis_enum_by_name(settings, leveling_key, &args->leveling);
    gwy_container_gis_boolean_by_name(settings, draw_frame_key,
                                      &args->draw_frame);
    immerse_sanitize_args(args);
}

static void
immerse_save_args(GwyContainer *settings,
                   ImmerseArgs *args)
{
    gwy_container_set_enum_by_name(settings, sampling_key, args->sampling);
    gwy_container_set_enum_by_name(settings, leveling_key, args->leveling);
    gwy_container_set_boolean_by_name(settings, draw_frame_key,
                                      args->draw_frame);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

