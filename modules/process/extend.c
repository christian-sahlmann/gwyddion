/*
 *  @(#) $Id$
 *  Copyright (C) 2010-2012 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
/* NB: This module contains considerable portions of libgwy/field-filter.c
 * from Gwyddion 3.  As the sole author of that code I relicense it for use in
 * Gwyddion 2 under GNU GPL version 2 (Yeti). */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <gtk/gtk.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define EXTEND_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    EXTEND_MAX = 2048
};

typedef struct {
    gint left;
    gint right;
    gint up;
    gint down;
    GwyExteriorType exterior;
    gboolean keep_offsets;
    gboolean new_channel;
} ExtendArgs;

typedef struct {
    ExtendArgs *args;
    GtkObject *left;
    GtkObject *right;
    GtkObject *up;
    GtkObject *down;
    GtkWidget *symmetrical;
    GtkWidget *exterior;
    GtkWidget *keep_offsets;
    GtkWidget *new_channel;
    gboolean in_update;
    guint last_active;
    gboolean is_symmetrical;
} ExtendControls;

static gboolean module_register     (void);
static void     extend              (GwyContainer *data,
                                     GwyRunType run);
static gboolean extend_dialog       (ExtendArgs *args);
static void     dialog_reset        (ExtendControls *controls);
static void     up_changed          (GtkAdjustment *adj,
                                     ExtendControls *controls);
static void     down_changed        (GtkAdjustment *adj,
                                     ExtendControls *controls);
static void     left_changed        (GtkAdjustment *adj,
                                     ExtendControls *controls);
static void     right_changed       (GtkAdjustment *adj,
                                     ExtendControls *controls);
static void     symmetrical_toggled (GtkToggleButton *button,
                                     ExtendControls *controls);
static void     exterior_changed    (GtkComboBox *combo,
                                     ExtendControls *controls);
static void     new_channel_toggled (GtkToggleButton *button,
                                     ExtendControls *controls);
static void     keep_offsets_toggled(GtkToggleButton *button,
                                     ExtendControls *controls);
static void     extend_sanitize_args(ExtendArgs *args);
static void     extend_load_args    (GwyContainer *container,
                                     ExtendArgs *args);
static void     extend_save_args    (GwyContainer *container,
                                     ExtendArgs *args);
static void     extend_field        (GwyDataField *field,
                                     GwyDataField *target,
                                     guint left,
                                     guint right,
                                     guint up,
                                     guint down,
                                     GwyExteriorType exterior,
                                     gdouble fill_value,
                                     gboolean keep_offsets);

static const ExtendArgs extend_defaults = {
    0, 0, 0, 0,
    GWY_EXTERIOR_MIRROR_EXTEND,
    FALSE, FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Extends image by adding borders."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("extend",
                              (GwyProcessFunc)&extend,
                              N_("/_Basic Operations/E_xtend..."),
                              NULL,
                              EXTEND_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Extend by adding borders"));

    return TRUE;
}

static void
extend(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *result;
    GQuark quark;
    ExtendArgs args;
    gboolean ok;
    gint oldid;

    g_return_if_fail(run & EXTEND_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfield);

    extend_load_args(gwy_app_settings_get(), &args);

    if (run == GWY_RUN_INTERACTIVE) {
        ok = extend_dialog(&args);
        extend_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    result = gwy_data_field_new(1, 1, 1.0, 1.0, FALSE);
    if (!args.new_channel)
        gwy_app_undo_qcheckpointv(data, 1, &quark);

    extend_field(dfield, result,
                 args.left, args.right, args.up, args.down,
                 args.exterior, gwy_data_field_get_avg(dfield),
                 args.keep_offsets);

    if (!args.new_channel) {
        gwy_serializable_clone(G_OBJECT(result), G_OBJECT(dfield));
        gwy_data_field_data_changed(dfield);
    }
    else {
        gint newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
        gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                GWY_DATA_ITEM_RANGE,
                                GWY_DATA_ITEM_REAL_SQUARE,
                                GWY_DATA_ITEM_SELECTIONS,
                                0);
        gwy_app_set_data_field_title(data, newid, _("Extended"));
    }
    g_object_unref(result);
}

static gboolean
extend_dialog(ExtendArgs *args)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *label;
    ExtendControls controls;
    gint row, response;

    dialog = gtk_dialog_new_with_buttons(_("Extend"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    controls.is_symmetrical = (args->up == args->down
                               && args->down == args->left
                               && args->left == args->right);
    controls.last_active = 0;
    controls.args = args;
    controls.in_update = TRUE;

    table = gtk_table_new(10, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    row = 0;

    /* Borders */
    label = gwy_label_new_header(_("Borders"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.up = gtk_adjustment_new(args->up, 0, EXTEND_MAX, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Up:"), "px",
                            controls.up, GWY_HSCALE_SQRT);
    g_signal_connect(controls.up, "value-changed",
                     G_CALLBACK(up_changed), &controls);
    row++;

    controls.down = gtk_adjustment_new(args->down, 0, EXTEND_MAX, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Down:"), "px",
                            controls.down, GWY_HSCALE_SQRT);
    g_signal_connect(controls.down, "value-changed",
                     G_CALLBACK(down_changed), &controls);
    row++;

    controls.left = gtk_adjustment_new(args->left, 0, EXTEND_MAX, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Left:"), "px",
                            controls.left, GWY_HSCALE_SQRT);
    g_signal_connect(controls.left, "value-changed",
                     G_CALLBACK(left_changed), &controls);
    row++;

    controls.right = gtk_adjustment_new(args->right, 0, EXTEND_MAX, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Right:"), "px",
                            controls.right, GWY_HSCALE_SQRT);
    g_signal_connect(controls.right, "value-changed",
                     G_CALLBACK(right_changed), &controls);
    row++;

    controls.symmetrical
        = gtk_check_button_new_with_mnemonic(_("Extend _symetrically"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.symmetrical),
                                 controls.is_symmetrical);
    gtk_table_attach(GTK_TABLE(table), controls.symmetrical,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.symmetrical, "toggled",
                     G_CALLBACK(symmetrical_toggled), &controls);
    row++;

    /* Borders */
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gwy_label_new_header(_("Options"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.exterior
        = gwy_enum_combo_box_newl(G_CALLBACK(exterior_changed), &controls,
                                  args->exterior,
                                  _("Mean"), GWY_EXTERIOR_FIXED_VALUE,
                                  gwy_sgettext("exterior|Border"),
                                  GWY_EXTERIOR_BORDER_EXTEND,
                                  gwy_sgettext("exterior|Mirror"),
                                  GWY_EXTERIOR_MIRROR_EXTEND,
                                  gwy_sgettext("exterior|Periodic"),
                                  GWY_EXTERIOR_PERIODIC,
                                  NULL);
    gwy_table_attach_hscale(table, row, _("_Exterior type:"), NULL,
                            GTK_OBJECT(controls.exterior),
                            GWY_HSCALE_WIDGET_NO_EXPAND);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;


    controls.keep_offsets
        = gtk_check_button_new_with_mnemonic(_("Keep lateral offsets"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.keep_offsets),
                                 args->keep_offsets);
    gtk_table_attach(GTK_TABLE(table), controls.keep_offsets,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.keep_offsets, "toggled",
                     G_CALLBACK(keep_offsets_toggled), &controls);
    row++;

    controls.new_channel
        = gtk_check_button_new_with_mnemonic(_("Create new channel"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.new_channel),
                                 args->new_channel);
    gtk_table_attach(GTK_TABLE(table), controls.new_channel,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.new_channel, "toggled",
                     G_CALLBACK(new_channel_toggled), &controls);
    row++;

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
            dialog_reset(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
dialog_reset(ExtendControls *controls)
{
    ExtendArgs *args = controls->args;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->symmetrical),
                                 FALSE);
    controls->in_update = TRUE;
    *args = extend_defaults;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->keep_offsets),
                                 args->keep_offsets);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->new_channel),
                                 args->new_channel);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->up), args->up);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->down), args->down);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->left), args->left);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->right), args->right);
    controls->in_update = FALSE;
}

static void
up_changed(GtkAdjustment *adj,
           ExtendControls *controls)
{
    ExtendArgs *args = controls->args;
    args->up = gwy_adjustment_get_int(adj);
    if (controls->in_update)
        return;

    controls->last_active = 0;
    if (controls->is_symmetrical) {
        controls->in_update = TRUE;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->down), args->up);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->left), args->up);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->right), args->up);
        controls->in_update = FALSE;
    }
}

static void
down_changed(GtkAdjustment *adj,
             ExtendControls *controls)
{
    ExtendArgs *args = controls->args;
    args->down = gwy_adjustment_get_int(adj);
    if (controls->in_update)
        return;

    controls->last_active = 1;
    if (controls->is_symmetrical) {
        controls->in_update = TRUE;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->up), args->down);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->left), args->down);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->right), args->down);
        controls->in_update = FALSE;
    }
}

static void
left_changed(GtkAdjustment *adj,
             ExtendControls *controls)
{
    ExtendArgs *args = controls->args;
    args->left = gwy_adjustment_get_int(adj);
    if (controls->in_update)
        return;

    controls->last_active = 2;
    if (controls->is_symmetrical) {
        controls->in_update = TRUE;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->up), args->left);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->down), args->left);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->right), args->left);
        controls->in_update = FALSE;
    }
}

static void
right_changed(GtkAdjustment *adj,
              ExtendControls *controls)
{
    ExtendArgs *args = controls->args;
    args->right = gwy_adjustment_get_int(adj);
    if (controls->in_update)
        return;

    controls->last_active = 3;
    if (controls->is_symmetrical) {
        controls->in_update = TRUE;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->up), args->right);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->down), args->right);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->left), args->right);
        controls->in_update = FALSE;
    }
}

static void
symmetrical_toggled(GtkToggleButton *button,
                    ExtendControls *controls)
{
    gdouble value = 0;

    controls->is_symmetrical = gtk_toggle_button_get_active(button);
    if (!controls->is_symmetrical)
        return;

    controls->in_update = TRUE;
    if (controls->last_active == 0)
        value = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->up));
    else if (controls->last_active == 1)
        value = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->down));
    else if (controls->last_active == 2)
        value = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->left));
    else if (controls->last_active == 3)
        value = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->right));

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->up), value);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->down), value);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->left), value);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->right), value);
    controls->in_update = FALSE;
}

static void
exterior_changed(GtkComboBox *combo,
                 ExtendControls *controls)
{
    controls->args->exterior = gwy_enum_combo_box_get_active(combo);
}

static void
new_channel_toggled(GtkToggleButton *button,
                    ExtendControls *controls)
{
    controls->args->new_channel = gtk_toggle_button_get_active(button);
}

static void
keep_offsets_toggled(GtkToggleButton *button,
                     ExtendControls *controls)
{
    controls->args->keep_offsets = gtk_toggle_button_get_active(button);
}

static const gchar up_key[]           = "/module/extend/up";
static const gchar down_key[]         = "/module/extend/down";
static const gchar left_key[]         = "/module/extend/left";
static const gchar right_key[]        = "/module/extend/right";
static const gchar exterior_key[]     = "/module/extend/exterior";
static const gchar keep_offsets_key[] = "/module/extend/keep_offsets";
static const gchar new_channel_key[]  = "/module/extend/new_channel";

static void
extend_sanitize_args(ExtendArgs *args)
{
    args->up = CLAMP(args->up, 0, EXTEND_MAX);
    args->down = CLAMP(args->down, 0, EXTEND_MAX);
    args->left = CLAMP(args->left, 0, EXTEND_MAX);
    args->right = CLAMP(args->right, 0, EXTEND_MAX);
    args->exterior = CLAMP(args->exterior,
                           GWY_EXTERIOR_BORDER_EXTEND,
                           GWY_EXTERIOR_FIXED_VALUE);
    args->keep_offsets = !!args->keep_offsets;
    args->new_channel = !!args->new_channel;
}

static void
extend_load_args(GwyContainer *container,
                 ExtendArgs *args)
{
    *args = extend_defaults;

    gwy_container_gis_int32_by_name(container, up_key, &args->up);
    gwy_container_gis_int32_by_name(container, down_key, &args->down);
    gwy_container_gis_int32_by_name(container, left_key, &args->left);
    gwy_container_gis_int32_by_name(container, right_key, &args->right);
    gwy_container_gis_enum_by_name(container, exterior_key, &args->exterior);
    gwy_container_gis_boolean_by_name(container, keep_offsets_key,
                                      &args->keep_offsets);
    gwy_container_gis_boolean_by_name(container, new_channel_key,
                                      &args->new_channel);
    extend_sanitize_args(args);
}

static void
extend_save_args(GwyContainer *container,
                 ExtendArgs *args)
{
    gwy_container_set_int32_by_name(container, up_key, args->up);
    gwy_container_set_int32_by_name(container, down_key, args->down);
    gwy_container_set_int32_by_name(container, left_key, args->left);
    gwy_container_set_int32_by_name(container, right_key, args->right);
    gwy_container_set_enum_by_name(container, exterior_key, args->exterior);
    gwy_container_set_boolean_by_name(container, keep_offsets_key,
                                      args->keep_offsets);
    gwy_container_set_boolean_by_name(container, new_channel_key,
                                      args->new_channel);
}

/* code ported from libgwy3 */

#define gwy_assign(dest, source, n) \
    memcpy((dest), (source), (n)*sizeof((dest)[0]))

typedef void (*RowExtendFunc)(const gdouble *in,
                              gdouble *out,
                              guint pos,
                              guint width,
                              guint res,
                              guint extend_left,
                              guint extend_right,
                              gdouble value);

typedef void (*RectExtendFunc)(const gdouble *in,
                               guint inrowstride,
                               gdouble *out,
                               guint outrowstride,
                               guint xpos,
                               guint ypos,
                               guint width,
                               guint height,
                               guint xres,
                               guint yres,
                               guint extend_left,
                               guint extend_right,
                               guint extend_up,
                               guint extend_down,
                               gdouble value);

static inline void
fill_block(gdouble *data, guint len, gdouble value)
{
    while (len--)
        *(data++) = value;
}

static inline void
row_extend_base(const gdouble *in, gdouble *out,
                guint *pos, guint *width, guint res,
                guint *extend_left, guint *extend_right)
{
    guint e2r, e2l;

    // Expand the ROI to the right as far as possible
    e2r = MIN(*extend_right, res - (*pos + *width));
    *width += e2r;
    *extend_right -= e2r;

    // Expand the ROI to the left as far as possible
    e2l = MIN(*extend_left, *pos);
    *width += e2l;
    *extend_left -= e2l;
    *pos -= e2l;

    // Direct copy of the ROI
    gwy_assign(out + *extend_left, in + *pos, *width);
}

static void
row_extend_mirror(const gdouble *in, gdouble *out,
                  guint pos, guint width, guint res,
                  guint extend_left, guint extend_right,
                  G_GNUC_UNUSED gdouble value)
{
    guint res2 = 2*res, k0, j;
    gdouble *out2;
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    out2 = out + extend_left + width;
    for (j = 0; j < extend_right; j++, out2++) {
        guint k = (pos + width + j) % res2;
        *out2 = (k < res) ? in[k] : in[res2-1 - k];
    }
    // Backward-extend
    k0 = (extend_left/res2 + 1)*res2;
    out2 = out + extend_left-1;
    for (j = 1; j <= extend_left; j++, out2--) {
        guint k = (k0 + pos - j) % res2;
        *out2 = (k < res) ? in[k] : in[res2-1 - k];
    }
}

static void
row_extend_periodic(const gdouble *in, gdouble *out,
                    guint pos, guint width, guint res,
                    guint extend_left, guint extend_right,
                    G_GNUC_UNUSED gdouble value)
{
    guint k0, j;
    gdouble *out2;
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    out2 = out + extend_left + width;
    for (j = 0; j < extend_right; j++, out2++) {
        guint k = (pos + width + j) % res;
        *out2 = in[k];
    }
    // Backward-extend
    k0 = (extend_left/res + 1)*res;
    out2 = out + extend_left-1;
    for (j = 1; j <= extend_left; j++, out2--) {
        guint k = (k0 + pos - j) % res;
        *out2 = in[k];
    }
}

static void
row_extend_border(const gdouble *in, gdouble *out,
                  guint pos, guint width, guint res,
                  guint extend_left, guint extend_right,
                  G_GNUC_UNUSED gdouble value)
{
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    fill_block(out + extend_left + width, extend_right, in[res-1]);
    // Backward-extend
    fill_block(out, extend_left, in[0]);
}

static void
row_extend_fill(const gdouble *in, gdouble *out,
                guint pos, guint width, guint res,
                guint extend_left, guint extend_right,
                gdouble value)
{
    row_extend_base(in, out, &pos, &width, res, &extend_left, &extend_right);
    // Forward-extend
    fill_block(out + extend_left + width, extend_right, value);
    // Backward-extend
    fill_block(out, extend_left, value);
}
static inline void
rect_extend_base(const gdouble *in, guint inrowstride,
                 gdouble *out, guint outrowstride,
                 guint xpos, guint *ypos,
                 guint width, guint *height,
                 guint xres, guint yres,
                 guint extend_left, guint extend_right,
                 guint *extend_up, guint *extend_down,
                 RowExtendFunc extend_row, gdouble fill_value)
{
    guint e2r, e2l, i;
    // Expand the ROI down as far as possible
    e2r = MIN(*extend_down, yres - (*ypos + *height));
    *height += e2r;
    *extend_down -= e2r;

    // Expand the ROI up as far as possible
    e2l = MIN(*extend_up, *ypos);
    *height += e2l;
    *extend_up -= e2l;
    *ypos -= e2l;

    // Row-wise extension within the vertical range of the ROI
    for (i = 0; i < *height; i++)
        extend_row(in + (*ypos + i)*inrowstride,
                   out + (*extend_up + i)*outrowstride,
                   xpos, width, xres, extend_left, extend_right, fill_value);
}

static void
rect_extend_mirror(const gdouble *in, guint inrowstride,
                   gdouble *out, guint outrowstride,
                   guint xpos, guint ypos,
                   guint width, guint height,
                   guint xres, guint yres,
                   guint extend_left, guint extend_right,
                   guint extend_up, guint extend_down,
                   G_GNUC_UNUSED gdouble value)
{
    guint yres2, i, k0;
    gdouble *out2;
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_mirror, value);
    // Forward-extend
    yres2 = 2*yres;
    out2 = out + outrowstride*(extend_up + height);
    for (i = 0; i < extend_down; i++, out2 += outrowstride) {
        guint k = (ypos + height + i) % yres2;
        if (k >= yres)
            k = yres2-1 - k;
        row_extend_mirror(in + k*inrowstride, out2,
                          xpos, width, xres, extend_left, extend_right, value);
    }
    // Backward-extend
    k0 = (extend_up/yres2 + 1)*yres2;
    out2 = out + outrowstride*(extend_up - 1);
    for (i = 1; i <= extend_up; i++, out2 -= outrowstride) {
        guint k = (k0 + ypos - i) % yres2;
        if (k >= yres)
            k = yres2-1 - k;
        row_extend_mirror(in + k*inrowstride, out2,
                          xpos, width, xres, extend_left, extend_right, value);
    }
}

static void
rect_extend_periodic(const gdouble *in, guint inrowstride,
                     gdouble *out, guint outrowstride,
                     guint xpos, guint ypos,
                     guint width, guint height,
                     guint xres, guint yres,
                     guint extend_left, guint extend_right,
                     guint extend_up, guint extend_down,
                     G_GNUC_UNUSED gdouble value)
{
    guint i, k0;
    gdouble *out2;
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_periodic, value);
    // Forward-extend
    out2 = out + outrowstride*(extend_up + height);
    for (i = 0; i < extend_down; i++, out2 += outrowstride) {
        guint k = (ypos + height + i) % yres;
        row_extend_periodic(in + k*inrowstride, out2,
                            xpos, width, xres, extend_left, extend_right, value);
    }
    // Backward-extend
    k0 = (extend_up/yres + 1)*yres;
    out2 = out + outrowstride*(extend_up - 1);
    for (i = 1; i <= extend_up; i++, out2 -= outrowstride) {
        guint k = (k0 + ypos - i) % yres;
        row_extend_periodic(in + k*inrowstride, out2,
                            xpos, width, xres, extend_left, extend_right, value);
    }
}

static void
rect_extend_border(const gdouble *in, guint inrowstride,
                   gdouble *out, guint outrowstride,
                   guint xpos, guint ypos,
                   guint width, guint height,
                   guint xres, guint yres,
                   guint extend_left, guint extend_right,
                   guint extend_up, guint extend_down,
                   G_GNUC_UNUSED gdouble value)
{
    guint i;
    gdouble *out2;
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_border, value);
    // Forward-extend
    out2 = out + outrowstride*(extend_up + height);
    for (i = 0; i < extend_down; i++, out2 += outrowstride)
        row_extend_border(in + (yres-1)*inrowstride, out2,
                          xpos, width, xres, extend_left, extend_right, value);
    // Backward-extend
    out2 = out + outrowstride*(extend_up - 1);
    for (i = 1; i <= extend_up; i++, out2 -= outrowstride)
        row_extend_border(in, out2,
                          xpos, width, xres, extend_left, extend_right, value);
}

static void
rect_extend_fill(const gdouble *in, guint inrowstride,
                 gdouble *out, guint outrowstride,
                 guint xpos, guint ypos,
                 guint width, guint height,
                 guint xres, guint yres,
                 guint extend_left, guint extend_right,
                 guint extend_up, guint extend_down,
                 gdouble value)
{
    guint i;
    gdouble *out2;
    rect_extend_base(in, inrowstride, out, outrowstride,
                     xpos, &ypos, width, &height, xres, yres,
                     extend_left, extend_right, &extend_up, &extend_down,
                     &row_extend_fill, value);
    // Forward-extend
    out2 = out + outrowstride*(extend_up + height);
    for (i = 0; i < extend_down; i++, out2 += outrowstride)
        fill_block(out2, extend_left + width + extend_right, value);
    // Backward-extend
    out2 = out + outrowstride*(extend_up - 1);
    for (i = 1; i <= extend_up; i++, out2 -= outrowstride)
        fill_block(out2, extend_left + width + extend_right, value);
}

static RectExtendFunc
get_rect_extend_func(GwyExteriorType exterior)
{
    if (exterior == GWY_EXTERIOR_FIXED_VALUE)
        return &rect_extend_fill;
    if (exterior == GWY_EXTERIOR_BORDER_EXTEND)
        return &rect_extend_border;
    if (exterior == GWY_EXTERIOR_MIRROR_EXTEND)
        return &rect_extend_mirror;
    if (exterior == GWY_EXTERIOR_PERIODIC)
        return &rect_extend_periodic;
    g_return_val_if_reached(NULL);
}

static void
extend_field(GwyDataField *field,
             GwyDataField *target,
             guint left, guint right,
             guint up, guint down,
             GwyExteriorType exterior,
             gdouble fill_value,
             gboolean keep_offsets)
{
    guint col = 0, row = 0, width = field->xres, height = field->yres;
    gdouble dx, dy;

    RectExtendFunc extend_rect = get_rect_extend_func(exterior);
    if (!extend_rect)
        return;

    gwy_data_field_resample(target, width + left + right, height + up + down,
                            GWY_INTERPOLATION_NONE);
    extend_rect(field->data, field->xres, target->data, target->xres,
                col, row, width, height, field->xres, field->yres,
                left, right, up, down, fill_value);

    dx = field->xreal/field->xres, dy = field->yreal/field->yres;
    gwy_data_field_set_xreal(target, (width + left + right)*dx);
    gwy_data_field_set_yreal(target, (height + up + down)*dy);
    if (keep_offsets) {
        gwy_data_field_set_xoffset(target, field->xoff + col*dx - left*dx);
        gwy_data_field_set_yoffset(target, field->yoff + row*dy - up*dy);
    }
    else {
        gwy_data_field_set_xoffset(target, 0.0);
        gwy_data_field_set_yoffset(target, 0.0);
    }
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_xy(field)),
                           G_OBJECT(gwy_data_field_get_si_unit_xy(target)));
    gwy_serializable_clone(G_OBJECT(gwy_data_field_get_si_unit_z(field)),
                           G_OBJECT(gwy_data_field_get_si_unit_z(target)));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
