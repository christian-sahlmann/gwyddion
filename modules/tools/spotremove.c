/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>
#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define CHECK_LAYER_TYPE(l) \
    (G_TYPE_CHECK_INSTANCE_TYPE((l), func_slots.layer_type))

#define MAX_SIZE 64
#define SCALE 4

typedef struct {
    GwyUnitoolState *state;
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *w;
    GtkWidget *h;
    GtkWidget *view;
    gchar *pal;
    gint algorithm;
} ToolControls;

static gboolean   module_register      (const gchar *name);
static gboolean   use                  (GwyDataWindow *data_window,
                                        GwyToolSwitchEvent reason);
static void       layer_setup          (GwyUnitoolState *state);
static GtkWidget* dialog_create        (GwyUnitoolState *state);
static void       dialog_update        (GwyUnitoolState *state,
                                        GwyUnitoolUpdateType reason);
static void       dialog_abandon       (GwyUnitoolState *state);
static void       apply                (GwyUnitoolState *state);
static void       load_args            (GwyContainer *container,
                                        ToolControls *controls);
static void       save_args            (GwyContainer *container,
                                        ToolControls *controls);
static void       draw_zoom            (ToolControls *controls,
                                        GwyDataField *dfield,
                                        gint ximin,
                                        gint yimin,
                                        gint ximax,
                                        gint yimax);
static gboolean   find_subrange        (gint min,
                                        gint max,
                                        gint res,
                                        gint size,
                                        gint *from,
                                        gint *to,
                                        gint *dest);
static void       crisscross_average   (GwyDataField *dfield,
                                        gint ximin,
                                        gint yimin,
                                        gint ximax,
                                        gint yimax);
static void       selection_to_rowcol  (GwyDataField *dfield,
                                        gdouble *sel,
                                        gint *ximin,
                                        gint *yimin,
                                        gint *ximax,
                                        gint *yimax);
static void       algorithm_changed_cb (GObject *item,
                                        ToolControls *controls);

static const gchar *algorithm_key = "/tool/spotremove/algorithm";

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "spotremove",
    "Removes spots.",
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static GwyUnitoolSlots func_slots = {
    0,                             /* layer type, must be set runtime */
    layer_setup,                   /* layer setup func */
    dialog_create,                 /* dialog constructor */
    dialog_update,                 /* update view and controls */
    dialog_abandon,                /* dialog abandon hook */
    apply,                         /* apply action */
    NULL,                          /* nonstandard response handler */
};

enum {
    SPOT_REMOVE_HYPER_FLATTEN
};

static const GwyEnum algorithms[] = {
    { "Hyperbolic flatten", SPOT_REMOVE_HYPER_FLATTEN },
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyToolFuncInfo func_info = {
        "spotremove",
        GWY_STOCK_SPOT_REMOVE,
        "Manually remove spots",
        120,
        use,
    };

    gwy_tool_func_register(name, &func_info);

    return TRUE;
}

static gboolean
use(GwyDataWindow *data_window,
    GwyToolSwitchEvent reason)
{
    static const gchar *layer_name = "GwyLayerSelect";
    static GwyUnitoolState *state = NULL;
    ToolControls *controls;

    if (!state) {
        func_slots.layer_type = g_type_from_name(layer_name);
        if (!func_slots.layer_type) {
            g_warning("Layer type `%s' not available", layer_name);
            return FALSE;
        }
        state = g_new0(GwyUnitoolState, 1);
        state->func_slots = &func_slots;
        state->user_data = g_new0(ToolControls, 1);
        state->apply_doesnt_close = TRUE;
    }
    controls = (ToolControls*)state->user_data;
    controls->state = state;
    if (controls->view && data_window) {
        GwyContainer *data, *mydata;
        GwyDataField *dfield;
        gdouble min, max;

        data = gwy_data_window_get_data(data_window);
        mydata = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        min = gwy_data_field_get_min(dfield);
        max = gwy_data_field_get_max(dfield);
        gwy_container_set_double_by_name(mydata, "/0/base/min", min);
        gwy_container_set_double_by_name(mydata, "/0/base/max", max);
    }
    return gwy_unitool_use(state, data_window, reason);
}

static void
layer_setup(GwyUnitoolState *state)
{
    g_assert(CHECK_LAYER_TYPE(state->layer));
    g_object_set(state->layer, "is_crop", FALSE, NULL);
}

static GtkWidget*
dialog_create(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyContainer *settings;
    GwyDataField *dfield;
    GwyPixmapLayer *layer;
    GtkWidget *dialog, *table, *label, *frame, *vbox, *omenu;
    gdouble min, max;

    gwy_debug("");
    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    load_args(settings, controls);

    dialog = gtk_dialog_new_with_buttons(_("Remove Spots"), NULL, 0, NULL);
    gwy_unitool_dialog_add_button_clear(dialog);
    gwy_unitool_dialog_add_button_hide(dialog);
    gwy_unitool_dialog_add_button_apply(dialog);

    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    controls->pal
        = g_strdup(gwy_container_get_string_by_name(data, "/0/base/palette"));
    min = gwy_data_field_get_min(dfield);
    max = gwy_data_field_get_max(dfield);

    table = gtk_table_new(1, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 0);

    dfield = GWY_DATA_FIELD(gwy_data_field_new(MAX_SIZE, MAX_SIZE,
                                               1.0, 1.0, FALSE));
    data = GWY_CONTAINER(gwy_container_new());
    gwy_container_set_object_by_name(data, "/0/data", G_OBJECT(dfield));
    gwy_container_set_double_by_name(data, "/0/base/min", min);
    gwy_container_set_double_by_name(data, "/0/base/max", max);
    gwy_container_set_string_by_name(data, "/0/base/palette",
                                     g_strdup(controls->pal));
    g_object_unref(dfield);
    controls->view = gwy_data_view_new(data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls->view), (gdouble)SCALE);

    layer = GWY_PIXMAP_LAYER(gwy_layer_basic_new());
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls->view), layer);
    gtk_table_attach(GTK_TABLE(table), controls->view, 0, 1, 0, 1,
                     GTK_FILL, 0, 2, 2);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_table_attach(GTK_TABLE(table), vbox, 1, 2, 0, 1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);

    frame = gwy_unitool_windowname_frame_create(state);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    table = gtk_table_new(8, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

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

    controls->x = gtk_label_new("");
    controls->y = gtk_label_new("");
    controls->w = gtk_label_new("");
    controls->h = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls->x), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->y), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->w), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls->h), 1.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->x, 2, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->y, 2, 3, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->w, 2, 3, 4, 5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls->h, 2, 3, 5, 6);

    gtk_table_set_row_spacing(GTK_TABLE(table), 5, 8);

    label = gtk_label_new_with_mnemonic(_("Removal _method"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 3, 6, 7, GTK_FILL, 0, 2, 2);

    omenu = gwy_option_menu_create(algorithms, G_N_ELEMENTS(algorithms),
                                   "algorithm",
                                   G_CALLBACK(algorithm_changed_cb), &controls,
                                   controls->algorithm);
    gtk_table_attach(GTK_TABLE(table), omenu, 0, 3, 7, 8, GTK_FILL, 0, 2, 2);

    return dialog;
}

static void
dialog_update(GwyUnitoolState *state,
              GwyUnitoolUpdateType reason)
{
    gboolean is_visible, is_selected, is_ok;
    ToolControls *controls;
    GwySIValueFormat *units;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyContainer *mydata;
    GwyDataField *mydfield;
    gdouble sel[4];
    const gchar *pal;

    gwy_debug("");

    controls = (ToolControls*)state->user_data;
    units = state->coord_format;

    data = gwy_data_window_get_data(state->data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    mydata = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    mydfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(mydata,
                                                               "/0/data"));
    pal = gwy_container_get_string_by_name(data, "/0/base/palette");
    if (strcmp(pal, controls->pal)) {
        GwyPixmapLayer *layer;
        GwyPalette *palette;

        g_free(controls->pal);
        controls->pal = g_strdup(pal);
        palette = GWY_PALETTE(gwy_palette_new(NULL));
        gwy_palette_set_by_name(palette, controls->pal);
        layer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));
        gwy_layer_basic_set_palette(GWY_LAYER_BASIC(layer), palette);
        g_object_unref(palette);
    }
    if (reason == GWY_UNITOOL_UPDATED_DATA) {
        gdouble min, max;

        gwy_debug("Recomputing min, max");
        min = gwy_data_field_get_min(dfield);
        max = gwy_data_field_get_max(dfield);
        gwy_container_set_double_by_name(mydata, "/0/base/min", min);
        gwy_container_set_double_by_name(mydata, "/0/base/max", max);
    }

    is_visible = state->is_visible;
    is_selected = gwy_vector_layer_get_selection(state->layer, sel);
    if (!is_visible && !is_selected)
        return;

    if (is_selected) {
        gint ximin, yimin, ximax, yimax;

        gwy_unitool_update_label(units, controls->x, sel[0]);
        gwy_unitool_update_label(units, controls->y, sel[1]);
        gwy_unitool_update_label(units, controls->w, sel[2] - sel[0]);
        gwy_unitool_update_label(units, controls->h, sel[3] - sel[1]);

        selection_to_rowcol(dfield, sel, &ximin, &yimin, &ximax, &yimax);
        is_ok = ximin > 0
                && yimin > 0
                && ximax + 1 < gwy_data_field_get_xres(dfield)
                && yimax + 1 < gwy_data_field_get_yres(dfield)
                && ximax - ximin <= MAX_SIZE
                && yimax - yimin <= MAX_SIZE;
        draw_zoom(controls, dfield, ximin, yimin, ximax, yimax);
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls->x), "");
        gtk_label_set_text(GTK_LABEL(controls->y), "");
        gtk_label_set_text(GTK_LABEL(controls->w), "");
        gtk_label_set_text(GTK_LABEL(controls->h), "");
        is_ok = FALSE;
        draw_zoom(controls, NULL, -1, -1, -1, -1);
    }
    gwy_unitool_apply_set_sensitive(state, is_ok);
}

static void
dialog_abandon(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *settings;

    controls = (ToolControls*)state->user_data;
    settings = gwy_app_settings_get();
    save_args(settings, controls);
    g_free(controls->pal);

    memset(state->user_data, 0, sizeof(ToolControls));
}


static void
apply(GwyUnitoolState *state)
{
    ToolControls *controls;
    GwyContainer *data;
    GwyDataField *dfield;
    GwyDataViewLayer *layer;
    gint ximin, yimin, ximax, yimax;
    gdouble sel[4];

    controls = (ToolControls*)state->user_data;
    layer = GWY_DATA_VIEW_LAYER(state->layer);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_vector_layer_get_selection(state->layer, sel);
    selection_to_rowcol(dfield, sel, &ximin, &yimin, &ximax, &yimax);
    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    crisscross_average(dfield, ximin, yimin, ximax, yimax);
    /*gwy_vector_layer_unselect(state->layer);*/
    gwy_data_view_update(GWY_DATA_VIEW(layer->parent));
}

static void
crisscross_average(GwyDataField *dfield,
                   gint ximin, gint yimin,
                   gint ximax, gint yimax)
{
    gdouble *data;
    gint i, j, rowstride;

    gwy_debug("(%d,%d) x (%d,%d)", ximin, ximax, yimin, yimax);
    data = gwy_data_field_get_data(dfield);
    rowstride = gwy_data_field_get_xres(dfield);

    for (i = yimin; i < yimax; i++) {
        gdouble px = data[i*rowstride + ximin - 1];
        gdouble qx = data[i*rowstride + ximax];
        gdouble y = (i - yimin + 1.0)/(yimax - yimin + 1.0);
        gdouble wx = 1.0/y + 1.0/(1.0 - y);

        for (j = ximin; j < ximax; j++) {
            gdouble py = data[(yimin - 1)*rowstride + j];
            gdouble qy = data[yimax*rowstride + j];
            gdouble x = (j - ximin + 1.0)/(ximax - ximin + 1.0);
            gdouble vy = px/x + qx/(1.0 - x);
            gdouble vx = py/y + qy/(1.0 - y);
            gdouble wy = 1.0/x + 1.0/(1.0 - x);

            data[i*rowstride + j] = (vx + vy)/(wx + wy);
        }
    }
}

static void
selection_to_rowcol(GwyDataField *dfield,
                    gdouble *sel,
                    gint *ximin, gint *yimin,
                    gint *ximax, gint *yimax)
{
    *ximin = gwy_data_field_rtoj(dfield, sel[0]);
    *yimin = gwy_data_field_rtoi(dfield, sel[1]);
    *ximax = gwy_data_field_rtoj(dfield, sel[2]) + 1;
    *yimax = gwy_data_field_rtoi(dfield, sel[3]) + 1;

}

static void
draw_zoom(ToolControls *controls,
          GwyDataField *dfield,
          gint ximin, gint yimin,
          gint ximax, gint yimax)
{
    GwyContainer *mydata;
    GwyDataField *mydfield;
    gint xfrom, xto, xdest, yfrom, yto, ydest;
    gdouble min;
    gboolean complete;

    mydata = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    mydfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(mydata,
                                                               "/0/data"));
    min = gwy_container_get_double_by_name(mydata, "/0/base/min");
    if (!dfield) {
        gwy_data_field_fill(mydfield, min);
        gwy_data_view_update(GWY_DATA_VIEW(controls->view));
        return;
    }

    complete = TRUE;
    complete &= find_subrange(ximin, ximax,
                              gwy_data_field_get_xres(dfield), MAX_SIZE,
                              &xfrom, &xto, &xdest);
    complete &= find_subrange(yimin, yimax,
                              gwy_data_field_get_yres(dfield), MAX_SIZE,
                              &yfrom, &yto, &ydest);
    if (!complete)
        gwy_data_field_fill(mydfield, min);
    gwy_data_field_area_copy(dfield, mydfield, xfrom, yfrom, xto, yto,
                             xdest, ydest);
    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
}

static gboolean
find_subrange(gint min, gint max, gint res, gint size,
              gint *from, gint *to, gint *dest)
{
    gint length = max - min;

    /* complete interval always fit in size */
    if (res <= size) {
        *from = 0;
        *to = res;
        *dest = (size - res)/2;
        return FALSE;
    }

    /* interval larger than size */
    if (length >= size) {
        *from = min;
        *to = min + size;
        *dest = 0;
        return TRUE;
    }

    /* interval shorter, fit intelligently */
    *from = MAX(0, min - (size - length)/2);
    *to = MIN(*from + size, res);
    *from = *to - size;
    g_assert(*from >= 0);
    *dest = 0;
    return TRUE;
}

static void
algorithm_changed_cb(GObject *item, ToolControls *controls)
{
    controls->algorithm = GPOINTER_TO_INT(g_object_get_data(item, "algorithm"));
}

static void
load_args(GwyContainer *container, ToolControls *controls)
{
    gwy_debug("");
    gwy_container_gis_int32_by_name(container, algorithm_key,
                                    &controls->algorithm);
}

static void
save_args(GwyContainer *container, ToolControls *controls)
{
    gwy_debug("");
    gwy_container_set_int32_by_name(container, algorithm_key,
                                    controls->algorithm);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

