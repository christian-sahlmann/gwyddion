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
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/correlation.h>
#include <libdraw/gwypixfield.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define IMMERSE_RUN_MODES GWY_RUN_INTERACTIVE

enum {
    PREVIEW_SIZE = 320
};

typedef enum {
    GWY_IMMERSE_LEVELING_LARGE,
    GWY_IMMERSE_LEVELING_DETAIL,
    GWY_IMMERSE_LEVELING_NONE,
    GWY_IMMERSE_LEVELING_LAST
} GwyImmerseLevelingType;

typedef enum {
    GWY_IMMERSE_MODE_CORRELATE,
    GWY_IMMERSE_MODE_NONE,
    GWY_IMMERSE_MODE_LAST
} GwyImmerseModeType;

typedef enum {
    GWY_IMMERSE_SAMPLING_UP,
    GWY_IMMERSE_SAMPLING_DOWN,
    GWY_IMMERSE_SAMPLING_LAST
} GwyImmerseSamplingType;

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    GwyImmerseLevelingType leveling;
    GwyImmerseModeType mode;
    GwyImmerseSamplingType sampling;
    GwyDataObjectId image;
    GwyDataObjectId detail;
} ImmerseArgs;

typedef struct {
    ImmerseArgs *args;
    GtkWidget *dialog;
    GwyContainer *mydata;
    GtkWidget *view;
    GtkWidget *pos;
    GdkPixbuf *detail;
    GwySIValueFormat *vf;
    gdouble xmax;
    gdouble ymax;
    gdouble xpos;
    gdouble ypos;
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
static gboolean immerse_do                  (ImmerseArgs *args);
static void     immerse_mode_cb             (GtkWidget *combo,
                                             ImmerseArgs *args);
static void     immerse_sampling_cb         (GtkWidget *combo,
                                             ImmerseArgs *args);
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
static gboolean get_score_iteratively       (GwyDataField *data_field,
                                             GwyDataField *kernel_field,
                                             GwyDataField *score,
                                             ImmerseArgs *args);
static void     find_score_maximum          (GwyDataField *correlation_score,
                                             gint *max_col,
                                             gint *max_row);

static const GwyEnum levelings[] = {
    { N_("Large image"),   GWY_IMMERSE_LEVELING_LARGE },
    { N_("Detail image"),  GWY_IMMERSE_LEVELING_DETAIL },
    { N_("None"),          GWY_IMMERSE_LEVELING_NONE },
};

static const GwyEnum modes[] = {
    { N_("Correlation"),   GWY_IMMERSE_MODE_CORRELATE },
    { N_("None"),          GWY_IMMERSE_MODE_NONE },
};

static const GwyEnum samplings[] = {
    { N_("Upsample large image"), GWY_IMMERSE_SAMPLING_UP },
    { N_("Downsample detail"),    GWY_IMMERSE_SAMPLING_DOWN },
};

static const ImmerseArgs immerse_defaults = {
    GWY_IMMERSE_LEVELING_DETAIL,
    GWY_IMMERSE_MODE_CORRELATE,
    GWY_IMMERSE_SAMPLING_UP,
    { NULL, -1 },
    { NULL, -1 },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Immerse high resolution detail into overall image."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.0",
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
                              NULL,
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
    GtkWidget *table, *chooser, *combo, *hbox, *alignment, *label;
    GtkWidget *vbox;
    GdkDisplay *display;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    gint response, row, id;
    gdouble zoomval;
    gboolean ok;

    memset(&controls, 0, sizeof(ImmerseControls));
    controls.args = args;

    controls.dialog
        = gtk_dialog_new_with_buttons(_("Immerse Detail"), NULL, 0,
                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                      GTK_STOCK_OK, GTK_RESPONSE_OK,
                                      NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(controls.dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(controls.dialog),
                                    GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(controls.dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

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
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
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

    /* Parameters */
    table = gtk_table_new(4, 4, FALSE);
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
                            GTK_OBJECT(chooser), GWY_HSCALE_WIDGET);
    row++;

    /*Parameters*/
    /*TODO, uncomment after it is clear what this should do*/
    /*combo = gwy_enum_combo_box_new(levelings, G_N_ELEMENTS(levelings),
                                   G_CALLBACK(immerse_leveling_cb), args,
                                   args->leveling, TRUE);
    gwy_table_attach_hscale(table, row, _("_Leveling:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;
    */
    combo = gwy_enum_combo_box_new(modes, G_N_ELEMENTS(modes),
                                   G_CALLBACK(immerse_mode_cb), args,
                                   args->mode, TRUE);
    gwy_table_attach_hscale(table, row, _("_Align detail:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    combo = gwy_enum_combo_box_new(samplings, G_N_ELEMENTS(samplings),
                                   G_CALLBACK(immerse_sampling_cb), args,
                                   args->sampling, TRUE);
    gwy_table_attach_hscale(table, row, _("_Result sampling:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    /* Position */
    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_end(GTK_BOX(vbox), table, FALSE, FALSE, 4);
    row = 0;

    label = gtk_label_new(_("Detail position:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, 0, 0, 0, 0);

    controls.pos = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.pos), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.pos,
                     1, 3, row, row+1, 0, 0, 0, 0);
    row++;

    gtk_widget_show_all(controls.dialog);
    display = gtk_widget_get_display(controls.dialog);
    controls.near_cursor = gdk_cursor_new_for_display(display, GDK_FLEUR);
    controls.move_cursor = gdk_cursor_new_for_display(display, GDK_CROSS);
    immerse_detail_cb(GWY_DATA_CHOOSER(chooser), &controls);

    do {
        response = gtk_dialog_run(GTK_DIALOG(controls.dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            immerse_controls_destroy(&controls);
            return FALSE;
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

    if (object->data) {
        quark = gwy_app_get_data_key_for_id(object->id);
        dfield = gwy_container_get_object(object->data, quark);
        quark = gwy_app_get_data_key_for_id(controls->args->image.id);
        ifield = gwy_container_get_object(controls->args->image.data, quark);
        controls->xmax = (gwy_data_field_get_xreal(ifield)
                          - gwy_data_field_get_xreal(dfield));
        controls->ymax = (gwy_data_field_get_yreal(ifield)
                          - gwy_data_field_get_yreal(dfield));
    }

    immerse_update_detail_pixbuf(controls);
    immerse_clamp_detail_offset(controls, controls->xpos, controls->ypos);
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

static gboolean
immerse_do(ImmerseArgs *args)
{
    GwyContainer *data;
    GwyDataField *resampled, *score, *image, *detail, *result;
    gint max_col = 0, max_row = 0, newid;
    gint xres1, xres2, yres1, yres2;
    gint newxres, newyres;
    gdouble rx, ry;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(args->image.id);
    image = GWY_DATA_FIELD(gwy_container_get_object(args->image.data, quark));

    quark = gwy_app_get_data_key_for_id(args->detail.id);
    detail = GWY_DATA_FIELD(gwy_container_get_object(args->detail.data, quark));

    result = gwy_data_field_new_alike(image, FALSE);

    xres1 = gwy_data_field_get_xres(image);
    xres2 = gwy_data_field_get_xres(detail);
    yres1 = gwy_data_field_get_yres(image);
    yres2 = gwy_data_field_get_yres(detail);
    rx = gwy_data_field_get_xreal(image)/gwy_data_field_get_xreal(detail);
    ry = gwy_data_field_get_yreal(image)/gwy_data_field_get_yreal(detail);

    if (args->sampling == GWY_IMMERSE_SAMPLING_DOWN) {
        resampled = gwy_data_field_new_alike(detail, FALSE);
        gwy_data_field_resample(result, xres1, yres1, GWY_INTERPOLATION_NONE);

        gwy_data_field_copy(image, result, FALSE);
        gwy_data_field_copy(detail, resampled, FALSE);
        newxres = MAX(xres1/rx, 1);
        newyres = MAX(yres1/ry, 1);
        gwy_data_field_resample(resampled, newxres, newyres,
                                GWY_INTERPOLATION_BILINEAR);

        if (args->mode == GWY_IMMERSE_MODE_CORRELATE) {
            score = gwy_data_field_new_alike(image, FALSE);
            if (!get_score_iteratively(image, resampled, score, args)) {
                g_object_unref(score);
                g_object_unref(result);
                g_object_unref(resampled);
                return FALSE;
            }
            find_score_maximum(score, &max_col, &max_row);
            g_object_unref(score);
        }

        gwy_data_field_area_copy(resampled, result,
                                 0, 0, newxres, newyres,
                                 max_col - newxres/2, max_row - newyres/2);
        g_object_unref(resampled);
    }
    else {
        gwy_data_field_resample(result, xres1, yres1, GWY_INTERPOLATION_NONE);
        gwy_data_field_copy(image, result, FALSE);

        newxres = MAX(xres2*rx, 1);
        newyres = MAX(yres2*ry, 1);
        gwy_data_field_resample(result, newxres, newyres,
                                GWY_INTERPOLATION_BILINEAR);

        if (args->mode == GWY_IMMERSE_MODE_CORRELATE) {
            score = gwy_data_field_new_alike(result, FALSE);
            if (!get_score_iteratively(result, detail, score, args)) {
                g_object_unref(score);
                g_object_unref(result);
                return FALSE;
            }
            find_score_maximum(score, &max_col, &max_row);
            g_object_unref(score);
        }
        gwy_data_field_area_copy(detail, result,
                                 0, 0, xres2, yres2,
                                 max_col - xres2/2, max_row - yres2/2);

    }

    /* set right output */
    if (result) {
        gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
        newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
        gwy_app_set_data_field_title(data, newid, _("Immersed detail data"));
        g_object_unref(result);
    }

    return TRUE;
}

/* compute crosscorelation */
static gboolean
get_score_iteratively(GwyDataField *data_field, GwyDataField *kernel_field,
                      GwyDataField *score, ImmerseArgs *args)
{
    enum { WORK_PER_UPDATE = 50000000 };
    GwyComputationState *state;
    gboolean ok = FALSE;
    int work, wpi;

    work = 0;
    wpi = gwy_data_field_get_xres(kernel_field)
          *gwy_data_field_get_yres(kernel_field);
    wpi = MIN(wpi, WORK_PER_UPDATE);
    state = gwy_data_field_correlate_init(data_field, kernel_field, score);

    /* FIXME */
    gwy_app_wait_start(gwy_app_find_window_for_channel(args->image.data,
                                                       args->image.id),
                       _("Initializing"));
    gwy_data_field_correlate_iteration(state);
    if (!gwy_app_wait_set_message(_("Correlating")))
        goto get_score_fail;
    do {
        gwy_data_field_correlate_iteration(state);
        work += wpi;
        if (work > WORK_PER_UPDATE) {
            work -= WORK_PER_UPDATE;
            if (!gwy_app_wait_set_fraction(state->fraction))
                goto get_score_fail;
        }
    } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
    ok = TRUE;

get_score_fail:
    gwy_data_field_correlate_finalize(state);
    gwy_app_wait_finish();

    return ok;
}

static void
find_score_maximum(GwyDataField *correlation_score,
                   gint *max_col,
                   gint *max_row)
{
    gint i, n, maxi = 0;
    gdouble max = -G_MAXDOUBLE;
    const gdouble *data;

    n = gwy_data_field_get_xres(correlation_score)
        *gwy_data_field_get_yres(correlation_score);
    data = gwy_data_field_get_data_const(correlation_score);

    for (i = 0; i < n; i++) {
        if (max < data[i]) {
            max = data[i];
            maxi = i;
        }
    }

    *max_row = (gint)floor(maxi/gwy_data_field_get_xres(correlation_score));
    *max_col = maxi - (*max_row)*gwy_data_field_get_xres(correlation_score);
}

/*
static void
immerse_leveling_cb(GtkWidget *combo, ImmerseArgs *args)
{
    args->leveling = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}
*/

static void
immerse_mode_cb(GtkWidget *combo, ImmerseArgs *args)
{
    args->mode = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void
immerse_sampling_cb(GtkWidget *combo, ImmerseArgs *args)
{
    args->sampling = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static gboolean
immerse_view_expose(GtkWidget *view,
                    GdkEventExpose *event,
                    ImmerseControls *controls)
{
    if (event->count > 0)
        return FALSE;

    gwy_debug("%p", controls->detail);
    if (controls->detail) {
        GdkColor white = { 0, 0xffff, 0xffff, 0xffff };
        GdkGC *gc;
        gint w, h, xoff, yoff;

        gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(view),
                                        controls->xpos, controls->ypos,
                                        &xoff, &yoff);
        w = gdk_pixbuf_get_width(controls->detail);
        h = gdk_pixbuf_get_height(controls->detail);
        gwy_debug("(%d,%d) %dx%d", xoff, yoff, w, h);
        gc = gdk_gc_new(view->window);
        gdk_draw_pixbuf(view->window, gc, controls->detail,
                        0, 0, xoff, yoff, w, h,
                        GDK_RGB_DITHER_NORMAL, 0, 0);
        gdk_gc_set_function(gc, GDK_XOR);
        gdk_gc_set_rgb_fg_color(gc, &white);
        gdk_draw_rectangle(view->window, gc, FALSE, xoff, yoff, w-1, h-1);
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
                                    controls->xpos, controls->ypos,
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
                                    controls->xpos, controls->ypos,
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
    if (xpos != controls->xpos || ypos != controls->ypos) {
        gchar *s;

        controls->xpos = xpos;
        controls->ypos = ypos;
        s = g_strdup_printf("(%.*f, %.*f) %s",
                            controls->vf->precision,
                            xpos/controls->vf->magnitude,
                            controls->vf->precision,
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


static const gchar leveling_key[] = "/module/immerse/leveling";
static const gchar mode_key[]     = "/module/immerse/mode";
static const gchar sampling_key[] = "/module/immerse/sampling";

static void
immerse_sanitize_args(ImmerseArgs *args)
{
    args->leveling = MIN(args->leveling, GWY_IMMERSE_LEVELING_LAST - 1);
    args->mode = MIN(args->mode, GWY_IMMERSE_MODE_LAST - 1);
    args->sampling = MIN(args->sampling, GWY_IMMERSE_SAMPLING_LAST - 1);
}

static void
immerse_load_args(GwyContainer *settings,
                   ImmerseArgs *args)
{
    *args = immerse_defaults;
    gwy_container_gis_enum_by_name(settings, leveling_key, &args->leveling);
    gwy_container_gis_enum_by_name(settings, mode_key, &args->mode);
    gwy_container_gis_enum_by_name(settings, sampling_key, &args->sampling);
    immerse_sanitize_args(args);
}

static void
immerse_save_args(GwyContainer *settings,
                   ImmerseArgs *args)
{
    gwy_container_set_enum_by_name(settings, leveling_key, args->leveling);
    gwy_container_set_enum_by_name(settings, mode_key, args->mode);
    gwy_container_set_enum_by_name(settings, sampling_key, args->sampling);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

