/*
 *  @(#) $Id: simple.c 8929 2008-12-31 13:40:16Z yeti-dn $
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

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyexpr.h>
#include <libprocess/datafield.h>
#include <libprocess/gwyprocess.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define SIMPLE_RUN_MODES GWY_RUN_INTERACTIVE

enum {
    NARGS = 2
};

enum {
    SIMPLE_OK   = 0,
    SIMPLE_DATA = 1,
    SIMPLE_EXPR = 2
};

typedef enum {
    DUPLICATE_NONE = 0,
    DUPLICATE_OVERWRITE = 1,
    DUPLICATE_APPEND = 2
} ResponseDuplicate;


typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    GwyDataObjectId objects[NARGS];
    gchar *name[NARGS];
    guint pos[NARGS];
    gdouble xoffset;
    gdouble yoffset;
    gdouble zoffset;
    gdouble xperiod;
    gdouble yperiod;
    gdouble threshold;
    gint xyexponent;
    gint zexponent;
    gchar *calname;
    ResponseDuplicate duplicate;
    GwyCalData *caldata;
    gdouble *xs;
    gdouble *ys;
    gint noriginal;
    GwyDataField *mask;
    gboolean objects_found;
} SimpleArgs;

typedef struct {
    SimpleArgs *args;
    GtkWidget *dialog;
    GtkWidget *data[NARGS];
    GtkObject *xoffset;
    GtkObject *yoffset;
    GtkObject *zoffset;
    GtkObject *xperiod;
    GtkObject *yperiod;
    GtkObject *threshold;
    gboolean in_update;
    GtkWidget *xyunits;
    GtkWidget *zunits;
    GtkWidget *xyexponent;
    GtkWidget *zexponent;
    GtkWidget *name;
    GtkWidget *suggestion;
    GtkWidget *button_ok;
} SimpleControls;

static gboolean module_register      (void);
static void     simple               (GwyContainer *data,
                                      GwyRunType run);
static gboolean simple_dialog        (SimpleArgs *args,
                                      GwyDataField *dfield);
static void     simple_data_cb       (GwyDataChooser *chooser,
                                      SimpleControls *controls);
static void     simple_do            (SimpleArgs *args);
static void     xyexponent_changed_cb(GtkWidget *combo,
                                      SimpleControls *controls);
static void     zexponent_changed_cb (GtkWidget *combo,
                                      SimpleControls *controls);
static void     units_change_cb      (GtkWidget *button,
                                      SimpleControls *controls);
static void     set_combo_from_unit  (GtkWidget *combo,
                                      const gchar *str,
                                      gint basepower);
static void     xoffset_changed_cb   (GtkAdjustment *adj,
                                      SimpleControls *controls);
static void     yoffset_changed_cb   (GtkAdjustment *adj,
                                      SimpleControls *controls);
static void     zoffset_changed_cb   (GtkAdjustment *adj,
                                      SimpleControls *controls);
static void     xperiod_changed_cb   (GtkAdjustment *adj,
                                      SimpleControls *controls);
static void     yperiod_changed_cb   (GtkAdjustment *adj,
                                      SimpleControls *controls);
static void     threshold_changed_cb (GtkAdjustment *adj,
                                      SimpleControls *controls);
static void     get_object_list      (GtkWindow *window,
                                      GwyDataField *data,
                                      GwyDataField *kernel,
                                      gdouble threshold,
                                      gdouble *xs,
                                      gdouble *ys,
                                      gint *nobjects,
                                      GwyCorrelationType type,
                                      gboolean *objects_found);
static void     draw_cross           (GwyDataField *mask,
                                      gint size,
                                      gint xpos,
                                      gint ypos);
static void     draw_times           (GwyDataField *mask,
                                      gint size,
                                      gint xpos,
                                      gint ypos);
static void     find_next            (gdouble *xs,
                                      gdouble *ys,
                                      gdouble *pxs,
                                      gdouble *pys,
                                      gint *is_indexed,
                                      gint *index_col,
                                      gint *index_row,
                                      gint xxshift,
                                      gint xyshift,
                                      gint yxshift,
                                      gint yyshift,
                                      gint present_xs,
                                      gint present_pxs,
                                      gint ncol,
                                      gint nrow,
                                      gint n,
                                      gint *nind,
                                      gdouble *avs,
                                      gint *navs);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Simple error map based on grating measurements"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("simple",
                              (GwyProcessFunc)&simple,
                              N_("/Cali_bration/_Get Simple Errop Map..."),
                              NULL,
                              SIMPLE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Simple error mapping"));

    return TRUE;
}

void
simple(GwyContainer *data, GwyRunType run)
{
    SimpleArgs args;
    guint i;
    GwyContainer *settings;
    GwyDataField *dfield;
    GQuark mquark;
    gint n, id;
    GwyCalibration *calibration;
    GwyCalData *caldata=NULL;
    gchar *filename;
    gchar *contents;
    gsize len;
    GError *err = NULL;
    gsize pos = 0;
    GString *str;
    FILE *fh;

    g_return_if_fail(run & SIMPLE_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &(args.mask),
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);

    settings = gwy_app_settings_get();
    for (i = 0; i < NARGS; i++) {
        args.objects[i].data = data;
        args.objects[i].id = id;
    }

    if (simple_dialog(&args, dfield)) {
        if (args.mask == NULL)
        {
            gwy_app_undo_qcheckpointv(data, 1, &mquark);
            args.mask = gwy_data_field_new_alike(dfield, FALSE);
            gwy_container_set_object(data, mquark, args.mask);
        }

        simple_do(&args);
        gwy_data_field_data_changed(args.mask);

    } else return;

    if (!args.caldata) return;


    /*if append requested, copy newly created calibration into old one*/
    if (args.duplicate == DUPLICATE_APPEND && (calibration = gwy_inventory_get_item(gwy_calibrations(), args.calname)))
    {

        filename = g_build_filename(gwy_get_user_dir(), "caldata", calibration->filename, NULL);
        if (!g_file_get_contents(filename,
                                 &contents, &len, &err))
        {
             g_warning("Error loading file: %s\n", err->message);
             g_clear_error(&err);
             return;
        }
        else {
            if (len)
              caldata = GWY_CALDATA(gwy_serializable_deserialize(contents, len, &pos));
            g_free(contents);
        }
        n = gwy_caldata_get_ndata(caldata) + gwy_caldata_get_ndata(args.caldata);
        gwy_caldata_append(args.caldata, caldata);
    }

    /*now create and save the resource*/
    if ((calibration = GWY_CALIBRATION(gwy_inventory_get_item(gwy_calibrations(), args.calname)))==NULL)
    {
        calibration = gwy_calibration_new(args.calname, g_strconcat(args.name[0], ".dat", NULL));
        gwy_inventory_insert_item(gwy_calibrations(), calibration);
        g_object_unref(calibration);
    }
    calibration->caldata = args.caldata;

    filename = gwy_resource_build_filename(GWY_RESOURCE(calibration));
    if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
        g_mkdir(g_build_filename(gwy_get_user_dir(), "calibrations", NULL), 0700);
    }

    fh = g_fopen(filename, "w");
    if (!fh) {
        g_warning("Cannot save preset: %s", filename);
        g_free(filename);
        return;
    }
    g_free(filename);

    str = gwy_resource_dump(GWY_RESOURCE(calibration));
    fwrite(str->str, 1, str->len, fh);
    fclose(fh);
    g_string_free(str, TRUE);

    gwy_resource_data_saved(GWY_RESOURCE(calibration));

    /*now save the calibration data*/
    gwy_caldata_save_data(args.caldata, calibration->filename);

}

static gboolean
simple_dialog(SimpleArgs *args, GwyDataField *dfield)
{
    SimpleControls controls;
    GtkWidget *dialog, *dialog2, *table, *chooser,  *label, *spin;
    GwySIUnit *unit;
    guint i, row, response;

    enum {
        RESPONSE_DUPLICATE_OVERWRITE = 2,
        RESPONSE_DUPLICATE_APPEND = 3 };


    controls.args = args;
    args->objects_found = FALSE;

    /*FIXME: use defaults*/
    args->xoffset = 0;
    args->yoffset = 0;
    args->zoffset = 0;
    args->xperiod = 0;
    args->yperiod = 0;
    args->xyexponent = -6;
    args->zexponent = -6;
    args->xs = NULL;
    args->ys = NULL;
    args->noriginal = 0;

    dialog = gtk_dialog_new_with_buttons(_("Simple Error Map"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
    controls.dialog = dialog;
    controls.button_ok = gtk_dialog_add_button(GTK_DIALOG(dialog),
                                               GTK_STOCK_OK, GTK_RESPONSE_OK);

    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4 + NARGS, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(_("Operands:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    args->name[0] = g_strdup_printf(_("Grating image"));
    args->name[1] = g_strdup_printf(_("Detail"));
      for (i = 0; i < NARGS; i++) {
        label = gtk_label_new_with_mnemonic(args->name[i]);
        gwy_strkill(args->name[i], "_");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);

        chooser = gwy_data_chooser_new_channels();
        gwy_data_chooser_set_active(GWY_DATA_CHOOSER(chooser),
                                    args->objects[i].data, args->objects[i].id);
        g_signal_connect(chooser, "changed",
                         G_CALLBACK(simple_data_cb), &controls);
        g_object_set_data(G_OBJECT(chooser), "index", GUINT_TO_POINTER(i));
        gtk_table_attach(GTK_TABLE(table), chooser, 1, 2, row, row+1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), chooser);
        controls.data[i] = chooser;

        row++;
      }
    label = gtk_label_new_with_mnemonic(_("_X offset:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.xoffset = gtk_adjustment_new(args->xoffset/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xoffset), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    unit = gwy_data_field_get_si_unit_xy(dfield);
    controls.xyexponent
        = gwy_combo_box_metric_unit_new(G_CALLBACK(xyexponent_changed_cb),
                                        &controls, -15, 6, unit,
                                        args->xyexponent);
    gtk_table_attach(GTK_TABLE(table), controls.xyexponent, 2, 3, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    controls.xyunits = gtk_button_new_with_label(gwy_sgettext("verb|Change"));
    g_object_set_data(G_OBJECT(controls.xyunits), "id", (gpointer)"xy");
    gtk_table_attach(GTK_TABLE(table), controls.xyunits,
                     3, 4, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Y offset:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.yoffset = gtk_adjustment_new(args->yoffset/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.yoffset), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("X _period:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.xperiod = gtk_adjustment_new(args->xperiod/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xperiod), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("Y p_eriod:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.yperiod = gtk_adjustment_new(args->yperiod/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.yperiod), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;


    label = gtk_label_new_with_mnemonic(_("_Z offset:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.zoffset = gtk_adjustment_new(args->zoffset/pow10(args->zexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.zoffset), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    unit = gwy_data_field_get_si_unit_z(dfield);
    controls.zexponent
        = gwy_combo_box_metric_unit_new(G_CALLBACK(zexponent_changed_cb),
                                        &controls, -15, 6, unit,
                                        args->zexponent);
    gtk_table_attach(GTK_TABLE(table), controls.zexponent, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    controls.zunits = gtk_button_new_with_label(gwy_sgettext("verb|Change"));
    g_object_set_data(G_OBJECT(controls.zunits), "id", (gpointer)"z");
    gtk_table_attach(GTK_TABLE(table), controls.zunits,
                     3, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.threshold = gtk_adjustment_new(args->threshold, -1.0, 1.0, 0.1, 0.2, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Threshold"), "",
                                   controls.threshold, GWY_HSCALE_DEFAULT);

    g_signal_connect(controls.threshold, "value-changed",
                       G_CALLBACK(threshold_changed_cb), &controls);

    row++;


    label = gtk_label_new_with_mnemonic(_("Calibration name:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    args->calname = g_strdup("new"); 
    controls.name = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(controls.name), args->calname);
    gtk_table_attach(GTK_TABLE(table), controls.name,
                     1, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    row++;
    controls.suggestion = gtk_label_new_with_mnemonic(_("No suggestion\n"));
    gtk_misc_set_alignment(GTK_MISC(controls.suggestion), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.suggestion,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

      g_signal_connect(controls.xoffset, "value-changed",
                       G_CALLBACK(xoffset_changed_cb), &controls);
      g_signal_connect(controls.yoffset, "value-changed",
                       G_CALLBACK(yoffset_changed_cb), &controls);
      g_signal_connect(controls.zoffset, "value-changed",
                       G_CALLBACK(zoffset_changed_cb), &controls);
      g_signal_connect(controls.xperiod, "value-changed",
                       G_CALLBACK(xperiod_changed_cb), &controls);
      g_signal_connect(controls.yperiod, "value-changed",
                       G_CALLBACK(yperiod_changed_cb), &controls);


     g_signal_connect(controls.xyunits, "clicked",
                             G_CALLBACK(units_change_cb), &controls);

     g_signal_connect(controls.zunits, "clicked",
                             G_CALLBACK(units_change_cb), &controls);

     controls.in_update = FALSE;

     simple_data_cb(GWY_DATA_CHOOSER(controls.data[0]),
                     &controls);



      gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

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
            /*check whether this resource already exists*/
            args->calname = g_strdup(gtk_entry_get_text(GTK_ENTRY(controls.name)));
            if (gwy_inventory_get_item(gwy_calibrations(), args->calname))
            {
                dialog2 = gtk_message_dialog_new (GTK_WINDOW(dialog),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_WARNING,
                                                  GTK_BUTTONS_CANCEL,
                                                  _("Calibration '%s' already exists"),
                                                  args->calname);
                gtk_dialog_add_button(GTK_DIALOG(dialog2), _("Overwrite"), RESPONSE_DUPLICATE_OVERWRITE);
                gtk_dialog_add_button(GTK_DIALOG(dialog2), _("Append"), RESPONSE_DUPLICATE_APPEND);
                response = gtk_dialog_run(GTK_DIALOG(dialog2));
                if (response == RESPONSE_DUPLICATE_OVERWRITE) {
                    args->duplicate = DUPLICATE_OVERWRITE;
                    response = GTK_RESPONSE_OK;
                } else if (response == RESPONSE_DUPLICATE_APPEND) {
                    args->duplicate = DUPLICATE_APPEND;
                    response = GTK_RESPONSE_OK;
                }
                gtk_widget_destroy (dialog2);
            } else args->duplicate = DUPLICATE_NONE;

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
simple_data_cb(GwyDataChooser *chooser,
                   SimpleControls *controls)
{
    SimpleArgs *args;
    guint i;
    gchar message[50];
    GwyDataField *original, *detail;

    args = controls->args;
    i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(chooser), "index"));
    args->objects[i].data = gwy_data_chooser_get_active(chooser,
                                                        &args->objects[i].id);

    original = GWY_DATA_FIELD(gwy_container_get_object(controls->args->objects[0].data,
                                                       gwy_app_get_data_key_for_id(args->objects[0].id)));
    detail = GWY_DATA_FIELD(gwy_container_get_object(controls->args->objects[1].data,
                                                       gwy_app_get_data_key_for_id(args->objects[1].id)));

    if (original==detail) {
        gtk_label_set_text(GTK_LABEL(controls->suggestion), _("Data same as detail?"));
        gtk_widget_set_sensitive(controls->button_ok, FALSE);
    }
    else if (gwy_data_field_get_xres(original)<=gwy_data_field_get_xres(detail) ||
        gwy_data_field_get_yres(original)<=gwy_data_field_get_yres(detail))
    {
        gtk_label_set_text(GTK_LABEL(controls->suggestion), _("Data larger than detail?"));
        gtk_widget_set_sensitive(controls->button_ok, FALSE);
    }
    else {
        args->noriginal = 10000;
        if (args->xs==NULL || args->ys==NULL)
        {
           args->xs = (gdouble *)g_malloc(args->noriginal*sizeof(gdouble));
           args->ys = (gdouble *)g_malloc(args->noriginal*sizeof(gdouble));
        }

        get_object_list(GTK_WINDOW(controls->dialog), original, detail, args->threshold, args->xs, args->ys, &(args->noriginal), 
                        GWY_CORRELATION_NORMAL, &(args->objects_found));
        if (args->objects_found)
          g_snprintf(message, sizeof(message), _("%d objects found"), args->noriginal);
        else 
          g_snprintf(message, sizeof(message), _("Search cancelled"));
        gtk_label_set_text(GTK_LABEL(controls->suggestion), message);
        gtk_widget_set_sensitive(controls->button_ok, TRUE);


    }


}

static void
get_object_list(GtkWindow *window, GwyDataField *data, GwyDataField *kernel, gdouble threshold,
                gdouble *xs, gdouble *ys, gint *nobjects, GwyCorrelationType type,
                gboolean *objects_found)
{
    enum { WORK_PER_UPDATE = 50000000 };
    GwyDataField *score = gwy_data_field_new_alike(data, 0);
    GwyDataField *retfield;
    gdouble *sdata, *maxval, min, max;
    gint i, *grains, *maxpos, ngrains, work, wpi;
    GwyComputationState *state;
    
    gwy_app_wait_start(window, _("Initializing"));
    state = gwy_data_field_correlate_init(data, kernel, score);
    gwy_app_wait_set_message(_("Correlating"));
    work = 0;
    wpi = gwy_data_field_get_xres(kernel)*gwy_data_field_get_yres(kernel);
    wpi = MIN(wpi, WORK_PER_UPDATE);
    do {
        gwy_data_field_correlate_iteration(state);
        work += wpi;
        if (work > WORK_PER_UPDATE) {
            work -= WORK_PER_UPDATE;
            if (!gwy_app_wait_set_fraction(state->fraction)) {
                gwy_data_field_correlate_finalize(state);
                gwy_app_wait_finish();
                *objects_found = FALSE;
                return;
            }
        }
    } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
    gwy_data_field_correlate_finalize(state);
    gwy_app_wait_finish();


    //gwy_data_field_correlate(data, kernel, score, type);
    max = gwy_data_field_get_max(score);
    min = gwy_data_field_get_min(score);

    retfield = gwy_data_field_duplicate(score);
    gwy_data_field_threshold(retfield, threshold, 0.0, 1.0);


    grains = (gint *)g_malloc(gwy_data_field_get_xres(retfield)*gwy_data_field_get_yres(retfield)*sizeof(gint));
    ngrains = gwy_data_field_number_grains(retfield, grains);

    maxpos = (gint *) g_malloc(ngrains*sizeof(gint));
    maxval = (gdouble *) g_malloc(ngrains*sizeof(gdouble));
    sdata = gwy_data_field_get_data(score);

 
    for (i=0; i<ngrains; i++) maxval[i] = -G_MAXDOUBLE;

    //find correlation maximum of each grain
    for (i=0; i<(gwy_data_field_get_xres(score)*gwy_data_field_get_yres(score)); i++)
    {
        if (grains[i]!=0) {
            if (maxval[grains[i]-1]<sdata[i]) {
                maxval[grains[i]-1]=sdata[i];
                maxpos[grains[i]-1]=i;
            }
        }
    }

    //return correlation maxima (x, y), TODO do this in future with subpixel precision;
    *nobjects = MIN(*nobjects, ngrains);
    for (i=0; i<(*nobjects); i++) {
        ys[i] = (int)(maxpos[i]/gwy_data_field_get_xres(retfield));
        xs[i] = maxpos[i] - ys[i]*gwy_data_field_get_xres(retfield);
    }


    *objects_found = TRUE;
    g_object_unref(score);
    g_object_unref(retfield);
    g_free(grains);
    g_free(maxpos);
    g_free(maxval);

}

static void
simple_dialog_update(SimpleControls *controls,
                        SimpleArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xoffset),
                             args->xoffset/pow10(args->xyexponent));

}



static gdouble
get_prod_grid(GwyDataField *a, GwyDataField *b, gdouble period)
{
    gint i, j;
    gint xres = gwy_data_field_get_xres(a);
    gint yres = gwy_data_field_get_yres(a);
    gdouble suma, sumb;
    gint shift = -xres/2;

    suma = sumb = 0;
    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            suma += gwy_data_field_get_val(a, i, j)*((i+shift)*period) - gwy_data_field_get_val(b, i, j)*((j+shift)*period);
            sumb += period*period*((i+shift)*(i+shift) + (j+shift)*(j+shift));
        }
    }

    return suma/sumb;
}

static void
simple_do(SimpleArgs *args)
{
    GwyContainer *data;
    GwyDataField *original, *detail;
    gdouble *xs, *ys, *pxs, *pys;
    gint *is_indexed, *index_row, *index_col;
    gint i, noriginal, nind;
    gdouble xxshift, xyshift, yxshift, yyshift;
    gdouble tlmin, nextmin;
    gint tl, next;
    gint xres, yres, present_xs, present_pxs;
    gdouble angle, avs[4];
    gdouble *x, *y, *z, *xerr, *yerr, *zerr, *xunc, *yunc, *zunc;
    gdouble minx, maxx, miny, maxy, minz, maxz;
    gint navs[4];
    GQuark quark;

    xxshift = xyshift = yxshift = yyshift =  0;

    data = args->objects[0].data;
    quark = gwy_app_get_data_key_for_id(args->objects[0].id);
    original = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    data = args->objects[1].data;
    quark = gwy_app_get_data_key_for_id(args->objects[1].id);
    detail = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    xres = gwy_data_field_get_xres(original);
    yres = gwy_data_field_get_yres(original);

    if (!args->objects_found)
           get_object_list(gwy_app_find_window_for_channel(data, args->objects[0].id),
                           original, 
                           detail, args->threshold, 
                           args->xs, 
                           args->ys, &(args->noriginal), 
                           GWY_CORRELATION_NORMAL,
                           &(args->objects_found));

    //________________________________________________________original____________________________________________
    //find objects on original
    noriginal = args->noriginal;
    xs = args->xs;
    ys = args->ys;
    pxs = (gdouble *)g_malloc(noriginal*sizeof(gdouble));
    pys = (gdouble *)g_malloc(noriginal*sizeof(gdouble));
    is_indexed = (gint *)g_malloc(noriginal*sizeof(gint));
    index_col = (gint *)g_malloc(noriginal*sizeof(gint));
    index_row = (gint *)g_malloc(noriginal*sizeof(gint));

    for (i=0; i<noriginal; i++)
    {
        is_indexed[i] = 0;
    }
    nind = 0;

    //find center object
    tl = 0;
    tlmin = G_MAXDOUBLE;
    for (i=0; i<noriginal; i++) {
        if (((xs[i]-xres/2)*(xs[i]-xres/2) + (ys[i]-yres/2)*(ys[i]-yres/2))<tlmin) {
            tlmin = ((xs[i]-xres/2)*(xs[i]-xres/2) + (ys[i]-yres/2)*(ys[i]-yres/2));
            tl = i;
        }
    }

    //determine initial xdiff and ydiff for col and row
    nextmin = G_MAXDOUBLE;
    for (i=0; i<noriginal; i++) {
        if (i==tl || xs[i]<=xs[tl]) continue;
        if (((xs[i]-xs[tl]) + (ys[i]-ys[tl])*(ys[i]-ys[tl]))<nextmin) {
            nextmin = ((xs[i]-xs[tl]) + (ys[i]-ys[tl])*(ys[i]-ys[tl]));
            next = i;
            xxshift = xs[next] - xs[tl];
            xyshift = ys[next] - ys[tl];
        }
    }

    nextmin = G_MAXDOUBLE;
    for (i=0; i<noriginal; i++) {
        if (i==tl || ys[i]<=ys[tl]) continue;
        if (((ys[i]-ys[tl]) + (xs[i]-xs[tl])*(xs[i]-xs[tl]))<nextmin) {
            nextmin = ((ys[i]-ys[tl]) + (xs[i]-xs[tl])*(xs[i]-xs[tl]));
            next = i;
            yxshift = xs[next] - xs[tl];
            yyshift = ys[next] - ys[tl];
        }
    }


    nind = 0;
    present_xs = tl;
    present_pxs = 0;
    pxs[0] = xs[tl];
    pys[0] = ys[tl];
    is_indexed[tl] = 1;
    index_col[0] = 0;
    index_row[0] = 0;
    nind++;

    avs[0] = avs[1] = avs[2] = avs[3] = 0;
    navs[0] = navs[1] = navs[2] = navs[3] = 0;

    find_next(xs, ys, pxs, pys, is_indexed,
              index_col, index_row, xxshift, xyshift, yxshift, yyshift, tl/*present_xs*/, 0/*present_pxs*/,
              1/*ncol*/, 0/*nrow*/, noriginal, &nind, avs, navs);


    xxshift = avs[0]/navs[0];
    xyshift = avs[1]/navs[1];
    yxshift = avs[2]/navs[2];
    yyshift = avs[3]/navs[3];

    if (xxshift == 0 || yyshift == 0) return; 
    //printf("%g %g %g %g\n", xxshift, yyshift, xyshift, yxshift);

    angle = (atan(xyshift/xxshift) + atan(-yxshift/yyshift))/2.0;

    xxshift = args->xperiod*cos(angle);
    yyshift = args->yperiod*cos(angle);
    xyshift = args->xperiod*sin(angle);
    yxshift = -args->yperiod*sin(angle);
    //printf("%g  %g %g  %g %g      %g %g %g %g\n", angle,
    //       xxshift, yyshift, xyshift, yxshift,
    //      gwy_data_field_rtoi(original, xxshift),
    //       gwy_data_field_rtoj(original, yyshift),
    //       gwy_data_field_rtoi(original, xyshift),
    //       gwy_data_field_rtoj(original, yxshift));

    gwy_data_field_fill(args->mask, 0);
    for (i=0; i<(nind); i++)
    {
        /*draw times and crosses on mask*/
        draw_cross(args->mask, 5, pxs[i], pys[i]);
        draw_times(args->mask, 4,
                   xs[tl] + index_col[i]*gwy_data_field_rtoi(original, xxshift) 
                          + index_row[i]*gwy_data_field_rtoj(original, yxshift),
                   ys[tl] + index_col[i]*gwy_data_field_rtoi(original, xyshift)
                          + index_row[i]*gwy_data_field_rtoj(original, yyshift)
                   );

    }

    args->caldata = gwy_caldata_new(nind);
    x = gwy_caldata_get_x(args->caldata);
    y = gwy_caldata_get_y(args->caldata);
    z = gwy_caldata_get_z(args->caldata);
    xerr = gwy_caldata_get_xerr(args->caldata);
    yerr = gwy_caldata_get_yerr(args->caldata);
    zerr = gwy_caldata_get_zerr(args->caldata);
    xunc = gwy_caldata_get_xunc(args->caldata);
    yunc = gwy_caldata_get_yunc(args->caldata);
    zunc = gwy_caldata_get_zunc(args->caldata);

    gwy_caldata_set_si_unit_x(args->caldata, gwy_data_field_get_si_unit_xy(original));
    gwy_caldata_set_si_unit_y(args->caldata, gwy_data_field_get_si_unit_xy(original));
    gwy_caldata_set_si_unit_z(args->caldata, gwy_data_field_get_si_unit_z(original));

    minx = miny = minz = G_MAXDOUBLE;
    maxx = maxy = maxz = -G_MAXDOUBLE;

    for (i=0; i<(nind); i++)
    {
        x[i] = args->xoffset + gwy_data_field_itor(original, pxs[i]);
        if (minx > x[i]) minx = x[i];
        if (maxx < x[i]) maxx = x[i];

        y[i] = args->yoffset + gwy_data_field_jtor(original, pys[i]);
        if (miny > y[i]) miny = y[i];
        if (maxy < y[i]) maxy = y[i];

        z[i] = args->zoffset + gwy_data_field_get_val(original, pxs[i], pys[i]);
        if (minz > z[i]) minz = z[i];
        if (maxz < z[i]) maxz = z[i];

        xerr[i] = gwy_data_field_itor(original, xs[tl]) 
                    + gwy_data_field_itor(original, index_col[i])*xxshift 
                    + gwy_data_field_jtor(original, index_row[i])*yxshift
                    - x[i];
        yerr[i] = gwy_data_field_jtor(original, ys[tl]) 
                    + gwy_data_field_itor(original, index_col[i])*xyshift 
                    + gwy_data_field_jtor(original, index_row[i])*yyshift
                    - y[i];

        zerr[i] = gwy_data_field_get_val(original, pxs[i], pys[i]) 
                    - gwy_data_field_get_val(original, xs[tl], ys[tl]);

        xunc[i] = yunc[i] = zunc[i] = 0;
    }

    gwy_caldata_set_range(args->caldata, minx, maxx, miny, maxy, minz, maxz);

}

static void
find_next(gdouble *xs, gdouble *ys, gdouble *pxs, gdouble *pys, gint *is_indexed,
          gint *index_col, gint *index_row, gint xxshift, gint xyshift, gint yxshift, gint yyshift,
          gint present_xs, gint present_pxs, gint ncol, gint nrow, gint n, gint *nind,
          gdouble *avs, gint *navs)  //present: present in xs,ys,  pcol, prow, present detected pos
{
    gint i, pos=0;
    gdouble val, min = G_MAXDOUBLE;
    gint found = 0;
    /*check me; first find next "present_xs"*/

    for (i=0; i<n; i++)
    {
        if (i==present_xs) continue;
        val = ((xs[i]-(xs[present_xs] + (ncol - index_col[present_pxs])*xxshift + (nrow - index_row[present_pxs])*yxshift))*
               (xs[i]-(xs[present_xs] + (ncol - index_col[present_pxs])*xxshift + (nrow - index_row[present_pxs])*yxshift)))
            +
            ((ys[i]-(ys[present_xs] + (ncol - index_col[present_pxs])*xyshift + (nrow - index_row[present_pxs])*yyshift))*
             (ys[i]-(ys[present_xs] + (ncol - index_col[present_pxs])*xyshift + (nrow - index_row[present_pxs])*yyshift)));

        if (val < ((xxshift*xxshift + yyshift*yyshift)/4.0) && min > val) {
            min = val;
            pos = i;
            found = 1;
        }
    }
    if (!found) {
       // printf("Nothing seen here\n");
        return;
    }

    if (is_indexed[pos]) {
    //    printf("oh, we'v been at (%d %d) already\n", ncol, nrow);
        return;
    }

    //printf("next at (%d %d): %g %g\n", ncol, nrow, xs[pos], ys[pos]);

    if ((ncol - index_col[(*nind)-1]) == 1 && (nrow - index_row[(*nind)-1]) == 0) { //right
          avs[0] -= pxs[(*nind)-1] - xs[pos];
          avs[1] -= pys[(*nind)-1] - ys[pos];
          navs[0]++;
          navs[1]++;
    }
    else if ((ncol - index_col[(*nind)-1]) == -1 && (nrow - index_row[(*nind)-1]) == 0) { //right
          avs[0] += pxs[(*nind)-1] - xs[pos];
          avs[1] += pys[(*nind)-1] - ys[pos];
          navs[0]++;
          navs[1]++;
    }
    else if ((nrow - index_row[(*nind)-1]) == 1 && (ncol - index_col[(*nind)-1])==0) { //top
          avs[2] -= pxs[(*nind)-1] - xs[pos];
          avs[3] -= pys[(*nind)-1] - ys[pos];
          navs[2]++;
          navs[3]++;
    }
    else if ((nrow - index_row[(*nind)-1]) == -1 && (ncol - index_col[(*nind)-1])==0) { //down
          avs[2] += pxs[(*nind)-1] - xs[pos];
          avs[3] += pys[(*nind)-1] - ys[pos];
          navs[2]++;
          navs[3]++;
    }

    pxs[*nind] = xs[pos];
    pys[*nind] = ys[pos];
    is_indexed[pos] = 1;
    present_pxs = *nind;
    index_col[*nind] = ncol;
    index_row[*nind] = nrow;
    (*nind)+=1;


    //printf("field summary: ###############################\n");
    //for (i=0; i<(*nind); i++)
    //{
    //    printf("No. %d, index %d %d, pos %g %g\n", i, index_col[i], index_row[i], pxs[i], pys[i]);
    //}


    /*search for neighbors*/
    find_next(xs, ys, pxs, pys, is_indexed,
              index_col, index_row, xxshift, xyshift, yxshift, yyshift, pos, present_pxs,
              ncol+1, nrow, n, nind, avs, navs);

    find_next(xs, ys, pxs, pys, is_indexed,
              index_col, index_row, xxshift, xyshift, yxshift, yyshift, pos, present_pxs,
              ncol-1, nrow, n, nind, avs, navs);

    find_next(xs, ys, pxs, pys, is_indexed,
              index_col, index_row, xxshift, xyshift, yxshift, yyshift, pos, present_pxs,
              ncol, nrow+1, n, nind, avs, navs);

    find_next(xs, ys, pxs, pys, is_indexed,
              index_col, index_row, xxshift, xyshift, yxshift, yyshift, pos, present_pxs,
              ncol, nrow-1, n, nind, avs, navs);

}


static void
xoffset_changed_cb(GtkAdjustment *adj,
                 SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xoffset = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
yoffset_changed_cb(GtkAdjustment *adj,
                 SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->yoffset = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
xperiod_changed_cb(GtkAdjustment *adj,
                 SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xperiod = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
yperiod_changed_cb(GtkAdjustment *adj,
                 SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->yperiod = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;

}


static void
zoffset_changed_cb(GtkAdjustment *adj,
                 SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zoffset = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
threshold_changed_cb(GtkAdjustment *adj,
                 SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->threshold = gtk_adjustment_get_value(adj);
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;

    simple_data_cb(GWY_DATA_CHOOSER(controls->data[0]),
                          controls);


}


static void
xyexponent_changed_cb(GtkWidget *combo,
                      SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xyexponent = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    args->xoffset = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->xoffset))
                  * pow10(args->xyexponent);
    args->yoffset = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->yoffset))
                  * pow10(args->xyexponent);
    args->xperiod = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->xperiod)) 
                  * pow10(args->xyexponent);
    args->yperiod = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->yperiod)) 
                  * pow10(args->xyexponent);
    
 
    simple_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
zexponent_changed_cb(GtkWidget *combo,
                      SimpleControls *controls)
{
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zexponent = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    args->zoffset = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zoffset))
                  * pow10(args->zexponent);


    simple_dialog_update(controls, args);
    controls->in_update = FALSE;
}


static void
units_change_cb(GtkWidget *button,
                SimpleControls *controls)
{
    GtkWidget *dialog, *hbox, *label, *entry;
    const gchar *id, *unit;
    gint response;
    SimpleArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;

    id = g_object_get_data(G_OBJECT(button), "id");
    dialog = gtk_dialog_new_with_buttons(_("Change Units"),
                                         NULL,
                                         GTK_DIALOG_MODAL
                                         | GTK_DIALOG_NO_SEPARATOR,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    hbox = gtk_hbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("New _units:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        controls->in_update = FALSE;
        return;
    }

    unit = gtk_entry_get_text(GTK_ENTRY(entry));

    if (gwy_strequal(id, "xy")) {
        set_combo_from_unit(controls->xyexponent, unit, 0);
     }
    else if (gwy_strequal(id, "z")) {
        set_combo_from_unit(controls->zexponent, unit, 0);
    }

    gtk_widget_destroy(dialog);

    simple_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
set_combo_from_unit(GtkWidget *combo,
                    const gchar *str,
                    gint basepower)
{
    GwySIUnit *unit;
    gint power10;

    unit = gwy_si_unit_new_parse(str, &power10);
    power10 += basepower;
    gwy_combo_box_metric_unit_set_unit(GTK_COMBO_BOX(combo),
                                       power10 - 6, power10 + 6, unit);
    g_object_unref(unit);
}

static void
draw_cross(GwyDataField *mask, gint size, gint xpos, gint ypos)
{
    gint i;
    for (i = MAX(0, xpos-size); i<MIN(gwy_data_field_get_xres(mask), xpos+size); i++)
        gwy_data_field_set_val(mask, i, ypos, 1);
    for (i = MAX(0, ypos-size); i<MIN(gwy_data_field_get_yres(mask), ypos+size); i++)
        gwy_data_field_set_val(mask, xpos, i, 1);


}
static void
draw_times(GwyDataField *mask, gint size, gint xpos, gint ypos)
{
    gint i, j;

    for (i = MAX(0, xpos-size); i<MIN(gwy_data_field_get_xres(mask), xpos+size); i++)
        for (j = MAX(0, ypos-size); j<MIN(gwy_data_field_get_yres(mask), ypos+size); j++)
        {
            if (abs(i-xpos)==abs(j-ypos)) gwy_data_field_set_val(mask, i, j, 1);
        }

}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

