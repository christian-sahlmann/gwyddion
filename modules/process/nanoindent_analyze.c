/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail:
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




/* TO DO
controls_changed => computed = FALSE

*/


#include <string.h>

#include <glib/gstdio.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libprocess/gwyprocessenums.h>
#include <libprocess/stats.h>
#include <libprocess/level.h>
#include <libprocess/correct.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/gwyapp.h>


#define  SURE_IMPRESSION_COEFF   0.3


#define INDENT_ANALYZE_RUN_MODES \
(GWY_RUN_MODAL)
/*    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS) */


/* Data for this function. */
typedef struct {

    gint minx;
    gint miny;
    gint maxx;
    gint maxy;

    gdouble max_val;
    gdouble min_val;

    gdouble plane_tol; /* what to consider as belonging to plane / percents of max-min */
    gdouble phi_tol;
    gdouble theta_tol;

    gdouble plane_x;
    gdouble plane_y;
    gdouble plane_c;

    gdouble volume_above;
    gdouble volume_below;
    gdouble area_above;
    gdouble area_below;
    gdouble area_plane;

    gdouble surface_above;
    gdouble surface_below;

    gdouble volume_indent;
    gdouble surface_indent;
    gdouble area_indent;

    gdouble surface_indent_exp;
    gdouble area_indent_exp;

    gdouble surface_innerpileup;
    gdouble surface_outerpileup;
    gdouble area_innerpileup;
    gdouble area_outerpileup;

    /* dialog args */

    gint what_mark;
    gint how_mark;
    gint plane_correct;

    GwyIndentorType indentor;
    gint  nof_sides;

} IndentAnalyzeArgs;

typedef struct {
    GtkWidget *w_plane_correct;
    GtkWidget *w_how_mark;
    GtkWidget *w_what_mark;
    GtkWidget *w_indentor;

    GtkObject *w_plane_tol;
    GtkObject *w_nof_sides;
    GtkObject *w_phi_tol;
    GtkObject *w_theta_tol;

    GtkWidget *view;
    GwyContainer *mydata;

    IndentAnalyzeArgs *args;
    gboolean computed;

    /* labels */
    GtkWidget *w_min_xy;
    GtkWidget *w_max_xy;
    GtkWidget *w_minmax;

    GtkWidget *w_volume_above;
    GtkWidget *w_volume_below;
    GtkWidget *w_volume_dif;

    GtkWidget *w_area_above;
    GtkWidget *w_area_below;
    GtkWidget *w_area_plane;

    GtkWidget *w_surface_above;
    GtkWidget *w_surface_below;

    GtkWidget *w_volume_indent;
    GtkWidget *w_surface_indent;
    GtkWidget *w_area_indent;

    GtkWidget *w_area_indent_exp;
    GtkWidget *w_surface_indent_exp;

    GtkWidget *w_surface_innerpileup;
    GtkWidget *w_surface_outerpileup;
    GtkWidget *w_area_innerpileup;
    GtkWidget *w_area_outerpileup;

    GtkFileSelection *filesel;

} IndentAnalyzeControls;

typedef  enum {
        NORMAL,
        CENTRAL,
        DIRECTION
} FloodFillMode;

typedef struct
{
    gdouble x,y,z;
} GwyVec;

typedef struct {
    FloodFillMode mode;
    GwyVec v;     /* normal, direction or  (centre-position) vector */
    gdouble cos_t1,cos_t2;
    gdouble cos_f1,cos_f2;
    gint seed;     /* side of averaging sqare */
} FloodFillInfo;


static gboolean         module_register          (const gchar *name);

static gboolean         indent_analyze(GwyContainer *data, GwyRunType run);

static gboolean         indent_analyze_dialog(GwyContainer *data, IndentAnalyzeArgs *args);

static void             dialog_update   (IndentAnalyzeControls *controls,
                                                  IndentAnalyzeArgs *args);

static void             plane_correct_cb(GtkWidget *item, IndentAnalyzeControls *controls);
static void             how_mark_cb(GtkWidget *item, IndentAnalyzeControls *controls);
static void             what_mark_cb(GtkWidget *item, IndentAnalyzeControls *controls);
static void             indentor_changed_cb(GtkWidget *item, IndentAnalyzeControls *controls);

static void     set_mask_at (GwyDataField *mask, gint x, gint y, gdouble  m,  gint how );
static void     level_data (IndentAnalyzeControls *c);
static void     get_field_xymin(GwyDataField *dfield, gdouble *min, gint *posx, gint *posy);
static void     get_field_xymax(GwyDataField *dfield, gdouble *max, gint *posx, gint *posy);
static GwyVec   data_field_average_normal_vector (GwyDataField *dfield, gint x, gint y, gint r);
static gdouble  data_field_compute_ds (GwyDataField *dfield, gint x_pos, gint y_pos);
static void     indentmask_flood_fill (GwyDataField *indentmask, gint i, gint j,
                                       GwyDataField *dfield, FloodFillInfo *ffi);
static void     compute_expected_indent (IndentAnalyzeControls* c);

static gboolean indent_analyze_do_the_hard_work(IndentAnalyzeControls *controls);
static void     compute_and_preview(IndentAnalyzeControls *controls);
static gboolean indent_analyze_ok(GwyContainer *data, IndentAnalyzeControls *controls);

static void save_statistics_dialog (IndentAnalyzeControls* c);

static void read_data_from_controls (IndentAnalyzeControls *c);
static void update_data_labels (IndentAnalyzeControls *c);

static void             load_args                (GwyContainer *container,
                                                 IndentAnalyzeArgs *args);
static void             save_args                (GwyContainer *container,
                                                  IndentAnalyzeArgs *args);
static void             sanitize_args            (IndentAnalyzeArgs *args);

static GwyVec gwy_vec_cross (GwyVec v1, GwyVec v2);
static gdouble gwy_vec_dot (GwyVec v1, GwyVec v2);
static GwyVec gwy_vec_times (GwyVec v, gdouble c);
static gdouble gwy_vec_abs (GwyVec v);
static gdouble gwy_vec_arg_phi (GwyVec v);
static gdouble gwy_vec_arg_theta (GwyVec v);
static gdouble gwy_vec_cos (GwyVec v1, GwyVec v2);
static void  gwy_vec_normalize (GwyVec *v);

enum {
 GWY_PLANE_NONE = 0,
 GWY_PLANE_LEVEL,
 GWY_PLANE_ROTATE
};

GwyEnum plane_correct_enum[] = {
    { N_("Do nothing"),    GWY_PLANE_NONE},
    { N_("Plane level"),   GWY_PLANE_LEVEL},
    { N_("Plane rotate"),  GWY_PLANE_ROTATE},
};


enum {
 GWY_HOW_MARK_NEW = 0,
 GWY_HOW_MARK_AND,
 GWY_HOW_MARK_OR,
 GWY_HOW_MARK_NOT,
 GWY_HOW_MARK_XOR,
};

GwyEnum how_mark_enum[] = {
    { N_("New"), GWY_HOW_MARK_NEW},
    { N_("AND"),   GWY_HOW_MARK_AND},
    { N_("OR"),  GWY_HOW_MARK_OR},
    { N_("NOT"),  GWY_HOW_MARK_NOT},
    { N_("XOR"),   GWY_HOW_MARK_XOR}
};

enum {
 GWY_WHAT_MARK_NOTHING = 0,
 GWY_WHAT_MARK_ABOVE,
 GWY_WHAT_MARK_BELOW,
 GWY_WHAT_MARK_PLANE,
 GWY_WHAT_MARK_INDENTATION,
 GWY_WHAT_MARK_INNERPILEUP,
 GWY_WHAT_MARK_OUTERPILEUP,

 GWY_WHAT_MARK_POINTS,
 GWY_WHAT_MARK_FACESBORDER,
};

GwyEnum what_mark_enum[] = {
    { N_("Nothing"),  GWY_WHAT_MARK_NOTHING},
    { N_("Above"), GWY_WHAT_MARK_ABOVE},
    { N_("Bellow"),   GWY_WHAT_MARK_BELOW},
    { N_("Plane"),   GWY_WHAT_MARK_PLANE},
    { N_("Impression"),   GWY_WHAT_MARK_INDENTATION},
    { N_("Inner Pile-up"),   GWY_WHAT_MARK_INNERPILEUP},
    { N_("Outer Pile-up"),   GWY_WHAT_MARK_OUTERPILEUP},
    { N_("Special points"),   GWY_WHAT_MARK_POINTS},
    { N_("Faces border"),   GWY_WHAT_MARK_FACESBORDER},
};




/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Analyses nanoindentation structure (volumes, surfaces, ...)"),
    "Lukáš Chvátal <chvatal@physics.muni.cz>",
    "0.1",
    "Lukáš Chvátal",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo indent_analyze_func_info = {
        "indent_analyze",
        N_("/Indento_r/_Analyze..."),
        (GwyProcessFunc)&indent_analyze,
        INDENT_ANALYZE_RUN_MODES ,
        0,
    };

    gwy_process_func_register(name, &indent_analyze_func_info);

    return TRUE;
}

static gboolean
indent_analyze(GwyContainer *data, GwyRunType run)
{

    IndentAnalyzeArgs args;
    gboolean ok;

    g_return_val_if_fail(run & INDENT_ANALYZE_RUN_MODES, FALSE);
    if (run == GWY_RUN_WITH_DEFAULTS) {

    }
    else
        load_args(gwy_app_settings_get(), &args);

    ok = (run != GWY_RUN_MODAL) || indent_analyze_dialog(data, &args);
    if (run == GWY_RUN_MODAL)
        save_args(gwy_app_settings_get(), &args);

    return TRUE;
}

static gboolean
indent_analyze_dialog(GwyContainer *data, IndentAnalyzeArgs *args)
{

    GtkWidget *dialog, *table, *temp, *hbox;
    IndentAnalyzeControls controls;
    GwyDataField* dfield;
    gint response;
    enum {
        RESPONSE_COMPUTE = 1,
        RESPONSE_SAVE = 2
    };
    gdouble zoomval;
    GwyPixmapLayer *layer;

    gchar siu[30];
    GwySIValueFormat* siformat;
    GwyRGBA rgba;
    gint row;

    controls.args = args;

    /* XXX: move, where it belongs, once someone adds mask color button */
    if (!gwy_rgba_get_from_container(&rgba, data, "/0/mask")) {
        gwy_rgba_get_from_container(&rgba, gwy_app_settings_get(), "/mask");
        gwy_rgba_store_to_container(&rgba, data, "/0/mask");
    }

    dialog = gtk_dialog_new_with_buttons(_("Indentaion statistics"), NULL, 0,
                                         _("_Compute & mark"), RESPONSE_COMPUTE,
                                         _("_Save statistics"), RESPONSE_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);


    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(hbox),
                       FALSE, FALSE, 4);
    /* VIEW */
    controls.mydata = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata,
                                                             "/0/data"));
    if (gwy_data_field_get_xres(dfield) >= gwy_data_field_get_yres(dfield))
        zoomval = 400.0/(gdouble)gwy_data_field_get_xres(dfield);
    else
        zoomval = 400.0/(gdouble)gwy_data_field_get_yres(dfield);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);
    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    /* TABLE */
    table = GTK_WIDGET(gtk_table_new(4, 2, FALSE));
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    controls.w_plane_correct
        = gwy_option_menu_create(plane_correct_enum,
                                 G_N_ELEMENTS(plane_correct_enum), "menu_plane_correct",
                                 G_CALLBACK(plane_correct_cb), &controls,
                                 args->plane_correct);
    gwy_table_attach_row(table, row, _("Data field leveling:"), "",
                         controls.w_plane_correct);
    row++;

    controls.w_what_mark
        = gwy_option_menu_create(what_mark_enum,
                                 G_N_ELEMENTS(what_mark_enum), "menu_what_mark",
                                 G_CALLBACK(what_mark_cb), &controls,
                                 args->what_mark);
    gwy_table_attach_row(table, row, _("Marked areas:"), "",
                         controls.w_what_mark);
    row++;


    controls.w_indentor = gwy_option_menu_indentor (G_CALLBACK(indentor_changed_cb),
                                     &controls, args->indentor);
    gwy_table_attach_row(table, row, _("Indentor type:"), "",controls.w_indentor);
    row++;

    controls.w_how_mark
        = gwy_option_menu_create(how_mark_enum,
                                 G_N_ELEMENTS(how_mark_enum), "menu_how_mark",
                                 G_CALLBACK(how_mark_cb), &controls,
                                 args->how_mark);
    gwy_table_attach_row(table, row, _("Mask creation type:"), "",controls.w_how_mark);
    row++;

    controls.w_plane_tol = gtk_adjustment_new (args->plane_tol, 0, 100, 0.1, 1, 10);
    temp = GTK_WIDGET(gtk_spin_button_new (GTK_ADJUSTMENT (controls.w_plane_tol), 1, 2));
    gwy_table_attach_row(table, row, _("Ref. plane tolerance:"), "\%",temp);
    row++;

    controls.w_phi_tol = gtk_adjustment_new (args->phi_tol, 0, 180, 0.1, 10, 10);
    temp = GTK_WIDGET(gtk_spin_button_new (GTK_ADJUSTMENT (controls.w_phi_tol), 1, 2));
    gwy_table_attach_row(table, row, _("Angle 1 tolerance:"), "deg",temp);
    row++;

    controls.w_theta_tol = gtk_adjustment_new (args->theta_tol, 0, 90, 0.1, 10, 10);
    temp = GTK_WIDGET(gtk_spin_button_new (GTK_ADJUSTMENT (controls.w_theta_tol), 1, 2));
    gwy_table_attach_row(table, row, _("Theta range:"), "deg",temp);
    row++;

    gtk_table_attach (GTK_TABLE (table), gtk_hseparator_new (), 0, 2, row, row+1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
    row++;


    /* "statistical" labels */
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    siformat = gwy_data_field_get_value_format_xy(dfield,
                                                  GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                  NULL);
    strcpy(siu, siformat->units);

    controls.w_min_xy = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Indent centre at"), siu,controls.w_min_xy);
    row++;
    controls.w_max_xy = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Maximum at"), siu,controls.w_max_xy);
    row++;
    controls.w_minmax = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Max-min diference"), siu,controls.w_minmax);
    row++;

    /* XXX XXX XXX What the fuck is this? XXX XXX XXX */
    strcpy(siu, siformat->units);
    strcat(siu, "^2");

    controls.w_surface_indent_exp = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Expected A_d:"), siu,controls.w_surface_indent_exp);
    row++;
    controls.w_area_indent_exp = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Expected A_p:"), siu,controls.w_area_indent_exp);
    row++;


    gtk_table_attach (GTK_TABLE (table), gtk_hseparator_new (), 0, 2, row, row+1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
    row++;


/*
    controls.w_area_above = gtk_label_new ("");
    gtk_label_set_justify( &controls.w_area_above, GTK_JUSTIFY_RIGHT);
    gwy_table_attach_row(table, row, _("Area above"), siu,controls.w_area_above);
    row++;
    controls.w_area_below = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Area below"), siu,controls.w_area_below);
    row++;
    controls.w_area_plane = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Area of plane"), siu,controls.w_area_plane);
    row++;

    controls.w_surface_above = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Surface above"), siu,controls.w_surface_above);
    row++;
    controls.w_surface_below = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Surface below"), siu,controls.w_surface_below);
    row++;
*/

    strcpy(siu, siformat->units);
    strcat(siu, "^3");
/*
    controls.w_volume_above = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Volume above"), siu,controls.w_volume_above);
    row++;
    controls.w_volume_below = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Volume below"), siu,controls.w_volume_below);
    row++;
*/
    controls.w_volume_dif = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Volume above-below"), siu,controls.w_volume_dif);
    row++;


    gtk_table_attach (GTK_TABLE (table), gtk_hseparator_new (), 0, 2, row, row+1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
    row++;

    controls.w_volume_indent = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Indent. volume"), siu,controls.w_volume_indent);
    row++;

    strcpy(siu, siformat->units);
    strcat(siu, "^2");
    controls.w_surface_indent = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Indent. A_d"), siu,controls.w_surface_indent);
    row++;
    controls.w_area_indent = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Indent. A_p"), siu,controls.w_area_indent);
    row++;

    gtk_table_attach (GTK_TABLE (table), gtk_hseparator_new (), 0, 2, row, row+1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
    row++;

    controls.w_surface_innerpileup = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Inner Pile-Up A_d"), siu,controls.w_surface_innerpileup);
    row++;
    controls.w_area_innerpileup = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Inner Pile-Up A_p"), siu,controls.w_area_innerpileup);
    row++;
    controls.w_surface_outerpileup = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Outer Pile-Up A_d"), siu,controls.w_surface_outerpileup);
    row++;
    controls.w_area_outerpileup = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Outer Pile-Up A_p"), siu,controls.w_area_outerpileup);
    row++;

    gwy_si_unit_value_format_free(siformat);

    controls.computed = FALSE;

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

            case RESPONSE_COMPUTE:
            compute_and_preview(&controls);
            update_data_labels(&controls);
            break;

            case RESPONSE_SAVE:
            save_statistics_dialog(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    indent_analyze_ok(data,&controls);

    g_object_unref(controls.mydata);
    gtk_widget_destroy(dialog);

    return controls.computed;
}


/* ====================================================================================== */

static void get_field_slope_from_border (GwyDataField *dfield, gdouble *c, gdouble *bx, gdouble *by)
{
      gint size = 20;
      gdouble cc;

      gwy_data_field_area_fit_plane(dfield, 0, 0, dfield->xres-1, size, &cc, bx, NULL);
      gwy_data_field_area_fit_plane(dfield, 0, 0, size, dfield->yres-1,   c, NULL, by);
      *c = (*c+cc)/2;
}


static void get_field_xymin(GwyDataField *dfield, gdouble *min, gint *posx, gint *posy)
{
   gdouble mm = 1e20;
   gdouble val;
   gint i, j;
   for (i=0; i<dfield->xres; i++) {
       for(j=0; j<dfield->yres; j++) {
           if((val=gwy_data_field_get_val(dfield,i,j)) < mm ) {
              mm = val;
              *posx = i;
              *posy = j;
           }
       }
   }
   *min = mm;
}


static void get_field_xymax(GwyDataField *dfield, gdouble *max, gint *posx, gint *posy)
{
   gdouble mm = -1e20;
   gdouble val;
   gint i, j;
   for (i=0; i<dfield->xres; i++) {
       for(j=0; j<dfield->yres; j++) {
           if((val=gwy_data_field_get_val(dfield,i,j)) > mm ) {
              mm = val;
              *posx = i;
              *posy = j;
           }
       }
   }
   *max = mm;

}

static gdouble data_field_compute_ds (GwyDataField *dfield, gint x_pos, gint y_pos)
{
    gdouble y_tr, y_br, y_tl, y_bl, ds;
    GwyVec v1, v2, s;
    gdouble dx = gwy_data_field_get_xreal(dfield)/(double)gwy_data_field_get_xres(dfield);
    gdouble dy = gwy_data_field_get_yreal(dfield)/(double)gwy_data_field_get_yres(dfield);


                y_tr = gwy_data_field_get_val(dfield,x_pos+1,y_pos);
                y_br = gwy_data_field_get_val(dfield,x_pos+1,y_pos+1);
                y_tl = gwy_data_field_get_val(dfield,x_pos,y_pos);
                y_bl = gwy_data_field_get_val(dfield,x_pos,y_pos+1);

                v1.x = dx;
                v1.y = 0;
                v1.z = y_tl - y_tr;

                v2.x = 0;
                v2.y = dy;
                v2.z = y_bl - y_tl;

                s = gwy_vec_cross(v1,v2);

                ds = gwy_vec_abs(s);

                v1.z = y_bl - y_br;
                v2.z = y_br - y_tr;

                s = gwy_vec_cross(v1,v2);

      return (ds + gwy_vec_abs(s))/2;
}

static GwyVec data_field_average_normal_vector (GwyDataField *dfield, gint x, gint y, gint r)
{
    gdouble n;
    gint i,j;
    GwyVec v;

    v.x = 0;
    v.y = 0;
    v.z = 0;

    n = 0;

    for(i = MAX(x-r,0); i <= MIN(x+r, dfield->xres-1); i++ )
       for(j = MAX(y-r,0); j <= MIN(y+r, dfield->yres-1); j++ )
       {
           v.x += -gwy_data_field_get_xder(dfield, i, j);
           v.y += -gwy_data_field_get_yder(dfield, i, j);
           v.z += 1;

           n++;
       }

    if(n) {
       v.x /= n;
       v.y /= n;
       v.z /= n;
    }

/*
FILE* deb = g_fopen("anv.txt","a");
fprintf(deb,"%lf %lf %lf->", v.x,v.y,v.z);

    if(r) {
        gwy_data_field_area_fit_plane(dfield, x-r, y-r,2*r+1, 2*r+1, NULL, &a, &b);
        v.x = -a;
        v.y = -b;
        v.z = 1;
    }
    else {
        v.x = -gwy_data_field_get_xder(dfield, x, y);
        v.y = -gwy_data_field_get_yder(dfield, x, y);
        v.z = 1;
    }

fprintf(deb,"%lf %lf %lf\n", v.x,v.y,v.z);
fclose(deb);
*/
    return v;
}

static void set_mask_at (GwyDataField *mask, gint x, gint y, gdouble m,  gint how )
{
    gint act_mask = (int)gwy_data_field_get_val (mask, x,y);
    gint im = (int) m;

    switch(how)
    {
        case GWY_HOW_MARK_NEW:
        act_mask = im;
        break;

        case GWY_HOW_MARK_AND:
        act_mask = (act_mask && im);
        break;

        case GWY_HOW_MARK_OR:
        act_mask = (act_mask || im);
   /*    if(act_mask){
             FILE* fl = g_fopen ("setmask.txt","a");
                            fprintf(fl, "[%d %d]%d %d %lf\n", x,y,act_mask, im, (double)(act_mask || im));
                            fclose(fl);
                            }
                            gwy_data_field_area_fill(mask,10,10,20,20,(double)act_mask);
             */
        break;

        case GWY_HOW_MARK_NOT:
        act_mask = !im;
        break;

        case GWY_HOW_MARK_XOR:
        act_mask = !(act_mask || im);
        break;
    }
    gwy_data_field_set_val(mask,x,y,(double)act_mask);
}

void level_data (IndentAnalyzeControls *c)
{
    gint iter = 3;
    IndentAnalyzeArgs *args= c->args;
    GwyDataField *dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(c->mydata, "/0/data"));

    get_field_slope_from_border (dfield, &(args->plane_c), &(args->plane_x), &(args->plane_y));


    while (iter--) {
               gwy_data_field_plane_level(dfield, args->plane_c, args->plane_x, args->plane_y);
               get_field_slope_from_border (dfield, &(args->plane_c), &(args->plane_x), &(args->plane_y));
            }

/*
    switch(args->plane_correct)
    {
        case GWY_PLANE_LEVEL:
            while (iter--) {
               gwy_data_field_plane_level(dfield, args->plane_c, args->plane_x, args->plane_y);
               get_field_slope_from_border (dfield, &(args->plane_c), &(args->plane_x), &(args->plane_y));
            }
            break;

        case GWY_PLANE_ROTATE:
            gwy_data_field_plane_rotate(dfield,
                                180/G_PI*atan2(args->plane_x, 1),
                                180/G_PI*atan2(args->plane_y, 1),
                                GWY_INTERPOLATION_BILINEAR);
            break;
    }
*/

}


typedef struct {
    gint x;
    gint y;
} FFPoint;

#define FLOOD_MAX_POINTS    500
#define FLOOD_QUEUED        1.0
#define  INDENT_INSIDE   100.0
#define  INDENT_BORDER   2.0

#define   FLOOD_MAX_DEPTH  1000

static void  indentmask_flood_fill (GwyDataField *indentmask, gint i, gint j,
                                   GwyDataField *dfield, FloodFillInfo* ffi)
{
    gdouble val;
    gint test = 0;
    gdouble c_f, c_t;
    GwyVec v;
    GwyVec e_z = { 0,0,1 };
    GwyVec r;
    GwyVec tmp;
    gint rr, s;

    FFPoint* pq; /* points queue */
    FFPoint* tail, *head;     /* head points on free position */
    gint count;

    val = gwy_data_field_get_val(indentmask, i, j);

    pq = g_new(FFPoint, FLOOD_MAX_POINTS);
    count = 0;
    tail = pq;
    head = pq;

    v = data_field_average_normal_vector(dfield, i, j, ffi->seed);
    head->x = i;
    head->y = j;
    gwy_data_field_set_val(indentmask,i,j,FLOOD_QUEUED);
    head++;

    while (head != tail) {

        v = data_field_average_normal_vector(dfield, tail->x, tail->y, ffi->seed);
        test = 0;
        switch(ffi->mode)
        {
        case NORMAL:
                c_f = gwy_vec_cos(v,ffi->v);
                if((c_f >= ffi->cos_f1) && (c_f <= ffi->cos_f2))
                   test = 1;
                break;
        case DIRECTION:
                c_f = gwy_vec_cos(v,e_z);
                v.z = 0;
                c_t = gwy_vec_cos(v,ffi->v);
                if( (c_f >= ffi->cos_f1) && (c_f <= ffi->cos_f2) &&
                    (c_t >= ffi->cos_t1) && (c_t <= ffi->cos_t2) )
                   test = 1;
                break;
        case CENTRAL:
                r.x = ffi->v.x - tail->x;
                r.y = ffi->v.y - tail->y;
                r.z = 0;
                gwy_vec_normalize(&r);
                tmp = gwy_vec_times(r, gwy_vec_dot(v,r));
                tmp.z = v.z;
                c_f = gwy_vec_cos(tmp,r);
                if( (c_f >= ffi->cos_f1) && (c_f <= ffi->cos_f2) )
                   test = 1;
                 break;
        }

        if(test)
        {
            gwy_data_field_set_val(indentmask, tail->x, tail->y, INDENT_INSIDE);
            for(rr = -1; rr <= 1; rr++) {
                for(s = -1; s <= 1; s++) {
                   if( s || rr ) {
                      val = gwy_data_field_get_val(indentmask,tail->x + rr, tail->y + s);
                      if(!val){
                         head->x = tail->x + rr;
                         head->y = tail->y + s;
                         if(pq + FLOOD_MAX_POINTS-1 > head) {
                             head++;
                         }
                         else {
                             head = pq;
                         }
                        gwy_data_field_set_val(indentmask,tail->x + rr, tail->y+s, FLOOD_QUEUED);
                      }
                   }
                }
             }
        }
        else {
            gwy_data_field_set_val(indentmask, tail->x, tail->y, INDENT_BORDER);
        }

        if(tail == pq + FLOOD_MAX_POINTS-1) {
            tail = pq;
        }
        else
           tail++;

    }  /* while */
    g_free(pq);
}

static gboolean indent_analyze_do_the_hard_work(IndentAnalyzeControls *controls)
{
    GwyContainer *data = controls->mydata;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    GwyDataField *mask = NULL;
    IndentAnalyzeArgs *args = controls->args;
    GwyDataLine *derdist;

    gint mark_it = 0;
    gint i, j;
    gdouble dx,dy,ds;
    gdouble total_area = 0;
    gdouble flat_area = 0;
    gdouble sx;
    gdouble sy;
    gdouble side_r;
    gdouble side_dir = 0;
    gdouble minmax;
    gdouble height;

    GwyVec avg_vec[10];
    gint avg_diam = 3;
    gdouble cos_phi_tol = cos(args->phi_tol*G_PI/180.);

    gdouble derdist_max;
    GwyDataField *indentmask = NULL;
    gdouble ok;
    FloodFillInfo ffi;


    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    gwy_container_gis_object_by_name(data, "/0/mask", (GObject**)&mask);
    if (!mask) {
            mask = gwy_data_field_duplicate(dfield);
            siunit = GWY_SI_UNIT(gwy_si_unit_new(""));
            gwy_data_field_set_si_unit_z(mask, siunit);
            g_object_unref(siunit);
            gwy_container_set_object_by_name(data, "/0/mask", (GObject*)mask);
            g_object_unref(mask);
    }
    read_data_from_controls (controls);
    level_data (controls);

    switch(controls->args->indentor) {
       case GWY_INDENTOR_VICKERS:
       case GWY_INDENTOR_KNOOP:
              controls->args->nof_sides = 4;
              break;
       case GWY_INDENTOR_BERKOVICH:
       case GWY_INDENTOR_BERKOVICH_M:
       case GWY_INDENTOR_CUBECORNER:
              controls->args->nof_sides = 3;
              break;
       case GWY_INDENTOR_ROCKWELL:
       case GWY_INDENTOR_BRINELL:
              controls->args->nof_sides = 0;
              break;
    }

    get_field_xymin(dfield, &args->min_val, &args->minx, &args->miny);
    get_field_xymax(dfield, &args->max_val, &args->maxx, &args->maxy);
    if(args->what_mark == GWY_WHAT_MARK_POINTS) {
        gwy_data_field_area_fill(mask,args->minx-2,args->miny-2,args->minx+2,args->miny+2,1.0);
        gwy_data_field_area_fill(mask,args->maxx-2,args->maxy-2,args->maxx+2,args->maxy+2,1.0);
    }
    minmax = (args->max_val - args->min_val)*args->plane_tol/100.0;

    dx = gwy_data_field_get_xreal(dfield)/(double)gwy_data_field_get_xres(dfield);
    dy = gwy_data_field_get_yreal(dfield)/(double)gwy_data_field_get_yres(dfield);
    ds = dx*dy;

    total_area = 0;
    flat_area = 0;

    args->volume_above =0;
    args->volume_below =0;
    args->area_above =0;
    args->area_below =0;
    args->surface_above =0;
    args->surface_below =0;

    args->volume_indent =0;
    args->surface_indent =0;
    args->area_indent =0;
    args->area_plane =0;

    args->surface_innerpileup = 0;
    args->surface_outerpileup = 0;
    args->area_innerpileup = 0;
    args->area_outerpileup = 0;

      /* computing char. of indentation area */
   sx = (args->minx-args->maxx);
   sy = (args->miny-args->maxy);
   side_r = sqrt(sx*sx+sy*sy);


    derdist = GWY_DATA_LINE(gwy_data_line_new(360, 2*G_PI, FALSE));
    gwy_data_field_slope_distribution(dfield, derdist, 5);

    derdist_max = 0;
    for(i=0;i < 360; i++)
    {
         if(gwy_data_line_get_val(derdist, i) > derdist_max) {
            derdist_max = gwy_data_line_get_val(derdist, i);
            side_dir = i;
        }
    }
    side_dir *= G_PI/180.;

    g_object_unref(derdist);

   /* (marking)  INDENTATION */
   indentmask = gwy_data_field_duplicate(dfield);
   gwy_data_field_fill(indentmask, 0.0);
   for(i = 0; i < args->nof_sides; i++) {
       height = -1e10;
       side_r = 0;
       while (height < SURE_IMPRESSION_COEFF*args->min_val )
       {
           sx = args->minx - side_r*cos(side_dir + i*2*G_PI/args->nof_sides);
           sy = args->miny - side_r*sin(side_dir + i*2*G_PI/args->nof_sides);
           height = gwy_data_field_get_val(dfield,sx,sy);
           side_r++;
       }

       avg_vec[i] = data_field_average_normal_vector(dfield, sx,sy,avg_diam);
       if(args->what_mark == GWY_WHAT_MARK_POINTS) {
           gwy_data_field_area_fill(mask,sx-2-i,sy-2-i,sx+2+i,sy+2+i,1.0);
       }
       else{
       ffi.mode = NORMAL;
       ffi.cos_f1 = cos_phi_tol;
       ffi.cos_f2 = 1.0;
       ffi.v = avg_vec[i];
       ffi.seed = 2;
       indentmask_flood_fill(indentmask, ROUND(sx),ROUND(sy), dfield, &ffi);
       }

   }

   /* we do not expect we should use the last coords...ensurance for computing vectors for surface */
   for (i=1; i<dfield->xres-1; i++) {
       for(j=1; j<dfield->yres-1; j++) {
           height = gwy_data_field_get_val(dfield,i,j);
           mark_it = 0;

           if(height > minmax) {
               args->volume_above += ds*height;
               args->area_above += ds;
               args->surface_above += data_field_compute_ds (dfield, i,j);

               args->area_outerpileup += ds;
               args->surface_outerpileup += data_field_compute_ds (dfield, i,j);
               if(args->what_mark == GWY_WHAT_MARK_ABOVE){
                   mark_it = 1;
               }
               if( args->what_mark == GWY_WHAT_MARK_OUTERPILEUP) {
                   mark_it = 1.0;
               }
           }
           else if(height < -minmax) {
               args->volume_below += ds*(-height);
               args->area_below += ds;
               args->surface_below += data_field_compute_ds (dfield, i,j);
               if(args->what_mark == GWY_WHAT_MARK_BELOW) {
                   mark_it = 1;
               }
               else if( (height < SURE_IMPRESSION_COEFF*args->min_val)
                     && (args->what_mark == GWY_WHAT_MARK_INDENTATION)) {
                   mark_it = 1;
               }
               else if( args->what_mark == GWY_WHAT_MARK_INNERPILEUP ||
                        args->what_mark == GWY_WHAT_MARK_OUTERPILEUP) {
                   mark_it = 0;
               }
           }
           else {
               args->area_plane += ds;
                if(args->what_mark == GWY_WHAT_MARK_PLANE) {
                    mark_it = 1;
                }

           }

           ok = 0;
           if(gwy_data_field_get_val(indentmask,i,j) == INDENT_INSIDE){
                ok = 1;
           }
           else if (gwy_data_field_get_val(indentmask,i+1,j)
                   +gwy_data_field_get_val(indentmask,i,j+1)
                   +gwy_data_field_get_val(indentmask,i,j-1)
                   +gwy_data_field_get_val(indentmask,i-1,j)
                   +gwy_data_field_get_val(indentmask,i+1,+j)
                   +gwy_data_field_get_val(indentmask,i-1,j+1)
                   +gwy_data_field_get_val(indentmask,i-1,j-1)
                   +gwy_data_field_get_val(indentmask,i-1,+j)>= INDENT_INSIDE*4 )
           {
              ok = 1;
           }

           if(ok) {
               height = gwy_data_field_get_val (dfield, i,j);
               args->surface_indent += data_field_compute_ds (dfield, i,j);
               args->volume_indent += ds*(height-args->min_val);
               args->area_indent += ds;
               if(height > 0) {
                   args->area_innerpileup += ds;
                   args->surface_innerpileup += data_field_compute_ds (dfield, i,j);
               }

               if(args->what_mark == GWY_WHAT_MARK_INDENTATION)
                   mark_it = 1.0;
               else if(args->what_mark == GWY_WHAT_MARK_OUTERPILEUP) {
                   mark_it = 0.0;
               }
               else if(args->what_mark == GWY_WHAT_MARK_INNERPILEUP) {
                   mark_it = (height >=0) ? 1.0 : 0.0;
               }
           }

           if(gwy_data_field_get_val(indentmask,i,j) == INDENT_BORDER){
              if(args->what_mark == GWY_WHAT_MARK_FACESBORDER)
                 mark_it = 1.0;
           }
           if(args->what_mark != GWY_WHAT_MARK_NOTHING &&
              args->what_mark != GWY_WHAT_MARK_POINTS)
              set_mask_at (mask, i,j, mark_it, args->how_mark);

       }
   }
    g_object_unref(indentmask);

    gwy_data_field_invalidate(dfield);
    gwy_data_field_invalidate(mask);

    controls->computed = TRUE;
    return TRUE;
}


static gboolean
indent_analyze_ok(GwyContainer *data, IndentAnalyzeControls *controls)
{
    GwyDataField *dfield;
    GObject *maskfield;

    if (!controls->computed) {
        indent_analyze_do_the_hard_work(controls);
    }

    maskfield = gwy_container_get_object_by_name(controls->mydata,"/0/mask");
    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    gwy_container_set_object_by_name(data, "/0/mask", maskfield);

    if (controls->args->plane_correct) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,"/0/data"));
        gwy_app_undo_checkpoint(data, "/0/data", NULL);
        gwy_container_set_object_by_name(data, "/0/data", dfield);
    }

    return TRUE;
}


static void
compute_and_preview(IndentAnalyzeControls *controls)
{
    GwyDataField *mask, *dfield;
    GwyPixmapLayer *layer;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,"/0/data"));

    read_data_from_controls(controls);

    /*set up the mask*/
    if (gwy_container_gis_object_by_name(controls->mydata, "/0/mask", &mask))
        gwy_data_field_copy(dfield, mask, FALSE);
    else {
        mask = gwy_data_field_duplicate(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/0/mask", mask);
        g_object_unref(mask);
    }

    controls->computed = indent_analyze_do_the_hard_work (controls);
    if (!controls->computed)
        return;

    if (!gwy_data_view_get_alpha_layer(GWY_DATA_VIEW(controls->view))) {
        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, "/0/mask");
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), layer);
    }
    gwy_data_field_data_changed(mask);
}

/* =========== dialog control functions ========================= */

static void
dialog_update(IndentAnalyzeControls *controls,
                       IndentAnalyzeArgs *args)
{

    gwy_option_menu_set_history(controls->w_plane_correct, "menu_plane_correct",
                                args->plane_correct);
    gwy_option_menu_set_history(controls->w_how_mark, "menu_how_mark",
                                args->how_mark);
    gwy_option_menu_set_history(controls->w_what_mark, "menu_what_mark",
                                args->what_mark);



}

static void
plane_correct_cb(GtkWidget *item, IndentAnalyzeControls *controls)
{
    controls->args->plane_correct
        = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item), "menu_plane_correct"));
   dialog_update(controls, controls->args);
}

static void
what_mark_cb(GtkWidget *item, IndentAnalyzeControls *controls)
{
    controls->args->what_mark
        = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item),
                                             "menu_what_mark"));
    dialog_update(controls, controls->args);
}

static void
how_mark_cb(GtkWidget *item, IndentAnalyzeControls *controls)
{
    controls->args->how_mark
        = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item),
                                             "menu_how_mark"));
    dialog_update(controls, controls->args);
}

static void
indentor_changed_cb(GtkWidget *item, IndentAnalyzeControls *controls)
{
    controls->args->indentor
        = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item),"indentor-type"));


    dialog_update(controls, controls->args);
}


static const gchar *what_mark_key = "/module/nanoindent/what_mark";
static const gchar *how_mark_key = "/module/nanoindent/how_mark";
static const gchar *plane_correct_key = "/module/nanoindent/plane_correct";
static const gchar *indentor_key = "/module/nanoindent/indentor";

static const gchar *plane_tol_key = "/module/nanoindent/plane_tol";
static const gchar *phi_tol_key = "/module/nanoindent/phi_tol";
static const gchar *theta_tol_key = "/module/nanoindent/theta_tol";

static void
sanitize_args(IndentAnalyzeArgs *args)
{

}


static void
load_args(GwyContainer *container,
          IndentAnalyzeArgs *args)
{
    if(!gwy_container_gis_enum_by_name(container, plane_correct_key, &args->plane_correct))
       args->plane_correct = GWY_PLANE_LEVEL;
    if(!gwy_container_gis_enum_by_name(container, what_mark_key, &args->what_mark))
       args->what_mark = GWY_WHAT_MARK_NOTHING;
    if(!gwy_container_gis_enum_by_name(container, how_mark_key, &args->how_mark))
       args->how_mark = GWY_HOW_MARK_NEW;
    if(!gwy_container_gis_enum_by_name(container, indentor_key, &args->indentor)) {
       args->indentor = GWY_INDENTOR_VICKERS;
    }

    if(!gwy_container_gis_double_by_name(container, plane_tol_key, &args->plane_tol))
       args->plane_tol = 1.0;
    if(!gwy_container_gis_double_by_name(container, phi_tol_key, &args->phi_tol))
       args->phi_tol = 8.0;
    if(!gwy_container_gis_double_by_name(container, theta_tol_key, &args->theta_tol))
       args->theta_tol = 8.0;

    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          IndentAnalyzeArgs *args)
{
    gwy_container_set_enum_by_name(container, plane_correct_key, args->plane_correct);
    gwy_container_set_enum_by_name(container, what_mark_key, args->what_mark);
    gwy_container_set_enum_by_name(container, how_mark_key, args->how_mark);
    gwy_container_set_enum_by_name(container, indentor_key, args->indentor);

    gwy_container_set_double_by_name(container, plane_tol_key, args->plane_tol);
    gwy_container_set_double_by_name(container, phi_tol_key, args->phi_tol);
    gwy_container_set_double_by_name(container, theta_tol_key, args->theta_tol);
}


static void read_data_from_controls (IndentAnalyzeControls *c)
{
    c->args->plane_tol = gtk_adjustment_get_value(GTK_ADJUSTMENT(c->w_plane_tol));
    c->args->phi_tol = gtk_adjustment_get_value(GTK_ADJUSTMENT(c->w_phi_tol));
    c->args->theta_tol = gtk_adjustment_get_value(GTK_ADJUSTMENT(c->w_theta_tol));

}

static void update_data_labels (IndentAnalyzeControls *c)
{
    GString *str;
    GwyDataField* dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(c->mydata, "/0/data"));
    GwySIValueFormat* siformat = gwy_data_field_get_value_format_xy(dfield,
                                                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                                    NULL);
    gdouble mag = siformat->magnitude;
    gdouble sxy = dfield->xreal*dfield->yreal;

    compute_expected_indent (c);

    str = g_string_new("");
    g_string_printf(str, "[%d, %d]px: %lf",
                    c->args->minx, c->args->miny, c->args->min_val/mag);
    gtk_label_set_text(GTK_LABEL(c->w_min_xy), str->str);

    g_string_printf(str, "[%d, %d]px: %lf",
                    c->args->maxx, c->args->maxy, c->args->max_val/mag);
    gtk_label_set_text(GTK_LABEL(c->w_max_xy), str->str);

    g_string_printf(str, "%lf",
                    (c->args->max_val - c->args->min_val)/mag);
    gtk_label_set_text(GTK_LABEL(c->w_minmax), str->str);
/*
    sprintf (str, "%g (%.1lf %%)", c->args->area_above/mag/mag, 100.*(c->args->area_above/sxy));
    gtk_label_set_text(c->w_area_above, str->str);
    sprintf (str, "%g (%.1lf %%)", c->args->area_below/mag/mag, 100.*(c->args->area_below/sxy));
    gtk_label_set_text(c->w_area_below, str->str);
    sprintf (str, "%g (%.1lf %%)", c->args->area_plane/mag/mag, 100.*(c->args->area_plane/sxy));
    gtk_label_set_text(c->w_area_plane, str->str);

    sprintf (str, "%g (+%.1f %%)", c->args->surface_above/mag/mag, 100.*c->args->surface_above/c->args->area_above);
    gtk_label_set_text(c->w_surface_above, str->str);
    sprintf (str, "%g (+%.1lf %%)", c->args->surface_below/mag/mag, 100.*c->args->surface_below/c->args->surface_below);
    gtk_label_set_text(c->w_surface_below, str->str);

    sprintf (str, "%g", c->args->volume_above/mag/mag/mag);
    gtk_label_set_text(c->w_volume_above, str->str);
    sprintf (str, "%g", c->args->volume_below/mag/mag/mag);
    gtk_label_set_text(c->w_volume_below, str->str);
*/
    g_string_printf(str, "%g",
                    (c->args->volume_above - c->args->volume_below)
                    /mag/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_volume_dif), str->str);

    g_string_printf(str, "%g", c->args->volume_indent/mag/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_volume_indent), str->str);
    g_string_printf(str, "%g", c->args->surface_indent/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_surface_indent), str->str);
    g_string_printf(str, "%g", (c->args->area_indent)/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_area_indent), str->str);

    g_string_printf(str, "%g", c->args->surface_indent_exp/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_surface_indent_exp), str->str);
    g_string_printf(str, "%g", (c->args->area_indent_exp)/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_area_indent_exp), str->str);

    g_string_printf(str, "%g", c->args->surface_innerpileup/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_surface_innerpileup), str->str);
    g_string_printf(str, "%g", (c->args->area_innerpileup)/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_area_innerpileup), str->str);
    g_string_printf(str, "%g", c->args->surface_outerpileup/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_surface_outerpileup), str->str);
    g_string_printf(str, "%g", (c->args->area_outerpileup)/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_area_outerpileup), str->str);

    g_string_free(str, TRUE);
}

static void save_statistics_dialog (IndentAnalyzeControls* c)
{
    IndentAnalyzeArgs* args = c->args;
    GwyDataField* dfield;
    GwySIValueFormat* siformat;
    GtkFileSelection *dialog;
    gchar filename_utf8[200];  /* in UTF-8 */
    gchar filename_sys[200];  /* in system (disk) encoding */
    gdouble mag;
    gdouble sxy;
    gint response;
    FILE *out;
    GtkWidget *g;


    if (!c->computed) /*nothing to output*/
    {
      g = gtk_message_dialog_new (GTK_WINDOW (gwy_app_main_window_get()),
                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_ERROR,
                                  GTK_BUTTONS_CLOSE,
                                  "There is no statistics computed yet.");
      gtk_dialog_run (GTK_DIALOG (g));
      gtk_widget_destroy (g);
      return;
    }

    /* SAVE .TXT statistics */
    dialog = GTK_FILE_SELECTION(gtk_file_selection_new("Save indentation statistics"));
    if (gwy_container_contains_by_name(c->mydata, "/filename")) {
        strcpy(filename_utf8, gwy_container_get_string_by_name(c->mydata, "/filename"));
        strcpy(filename_sys, g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL) );
        strcpy(strchr(filename_sys,'.'), ".TXT");
    }
    else
        strcpy(filename_sys,"NANOINDENT.TXT");

    gtk_file_selection_set_filename(dialog, filename_sys);
    gtk_widget_show_all(GTK_WIDGET(dialog));
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    strcpy(filename_sys,gtk_file_selection_get_filename (GTK_FILE_SELECTION (dialog)));
    gtk_widget_destroy(GTK_WIDGET(dialog));
    if(response != GTK_RESPONSE_OK) {
        return;
    }

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(c->mydata, "/0/data"));
    sxy = dfield->xreal*dfield->yreal;
    siformat = gwy_data_field_get_value_format_xy(dfield,
                                                  GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                  NULL);
    mag = siformat->magnitude;

    out = g_fopen( filename_sys, "w");
    if(out) {

    fprintf (out, "%s\n",  filename_sys );

    fprintf (out, "Indenter:  %s\n", "...to be added");
    fprintf (out, "Length units: %s\n",siformat->units);

    fprintf (out, "Indentation centre at [%d, %d] px:      %lf\n", args->minx, args->miny,args->min_val/mag);
    fprintf (out, "Maximum at [%d, %d] is:                 %lf\n", args->maxx, args->maxy,args->max_val/mag);
    fprintf (out, "Diference max-min:                      %lf\n", (args->max_val-args->min_val)/mag);
    fprintf (out, "\n");

    fprintf (out, "Area (projected) above plane:             %g (%.1lf %%)\n", c->args->area_above/mag/mag, 100.*(c->args->area_above/sxy));
    fprintf (out, "Area (projected) below plane:             %g (%.1lf %%)\n", c->args->area_below/mag/mag, 100.*(c->args->area_below/sxy));
    fprintf (out, "Area (projected) of    plane:             %g (%.1lf %%)\n", c->args->area_plane/mag/mag, 100.*(c->args->area_plane/sxy));
    fprintf (out, "\n");

    fprintf (out, "Area (developed) above %g (+%.1f %%)\n", args->surface_above/mag/mag, 100.*args->surface_above/sxy);
    fprintf (out, "Area (developed) above %g (+%.1lf %%)\n", args->surface_below/mag/mag, 100.*args->surface_below/sxy);

    fprintf (out, "Volume above:     %g\n", c->args->volume_above/mag/mag/mag);
    fprintf (out, "Volume below:     %g\n", c->args->volume_below/mag/mag/mag);
    fprintf (out, "Volume diference  %g\n", (c->args->volume_above-c->args->volume_below)/mag/mag/mag);

    fprintf (out, "\nIndentation\n");
    fprintf (out, "Volume      %g\n", c->args->volume_indent/mag/mag/mag);
    fprintf (out, "A_P         %g\n", c->args->surface_indent/mag/mag);
    fprintf (out, "A_D         %g\n", (c->args->area_indent)/mag/mag);

    fprintf (out, "\nIndentation - Inner Pile-Up\n");
    fprintf (out, "A_P         %g\n", c->args->surface_innerpileup/mag/mag);
    fprintf (out, "A_D         %g\n", (c->args->area_innerpileup)/mag/mag);

    fprintf (out, "Indentation - Outer Pile-Up\n");
    fprintf (out, "A_P         %g\n", c->args->surface_outerpileup/mag/mag);
    fprintf (out, "A_D         %g\n", (c->args->area_outerpileup)/mag/mag);

    }
    fclose(out);
}


static void compute_expected_indent (IndentAnalyzeControls* c)
{
       IndentAnalyzeArgs *args = c->args;
       gdouble h2 = args->min_val* args->min_val;
       if(!(c->computed))
              return;

       switch(args->indentor)
       {
       case GWY_INDENTOR_VICKERS:
              args->area_indent_exp = 24.5042*h2;
              args->surface_indent_exp = 26.43*h2;
              break;
       case GWY_INDENTOR_KNOOP:
              args->area_indent_exp = 0*h2;
              args->surface_indent_exp = 0*h2;
              break;
       case GWY_INDENTOR_BERKOVICH:
              args->area_indent_exp = 23.96*h2;
              args->surface_indent_exp = 26.43*h2;
              break;
       case GWY_INDENTOR_BERKOVICH_M:
              args->area_indent_exp = 24.494*h2;
              args->surface_indent_exp = 26.97*h2;
              break;
       case GWY_INDENTOR_ROCKWELL:
              args->area_indent_exp = 0*h2;
              args->surface_indent_exp = 0*h2;
              break;
       case GWY_INDENTOR_CUBECORNER:
              args->area_indent_exp = 2.598*h2;
              args->surface_indent_exp = 4.50*h2;
              break;
       case GWY_INDENTOR_BRINELL:
              args->area_indent_exp = 0; /*FIXME*/
              args->surface_indent_exp = 0;
              break;
       }

}



GwyVec gwy_vec_cross (GwyVec v1, GwyVec v2)
{
    GwyVec result;
    result.z = v1.y*v2.z - v2.y*v1.z;
    result.y = v1.z*v2.x - v2.z*v1.x;
    result.x = v1.x*v2.y - v2.x*v1.y;
    return result;
}
gdouble gwy_vec_dot (GwyVec v1, GwyVec v2)
{
   return  v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;

}

GwyVec gwy_vec_times (GwyVec v, gdouble c)
{
   GwyVec v1;

   v1.x = v.x*c;
   v1.y = v.y*c;
   v1.z = v.z*c;
   return  v1;

}

gdouble gwy_vec_abs (GwyVec v)
{
   return sqrt (gwy_vec_dot (v,v));
}

gdouble gwy_vec_arg_phi (GwyVec v)
{
   return atan2(v.z, v.x);

}

gdouble gwy_vec_arg_theta (GwyVec v)
{
   return atan2(v.y, v.x);

}

gdouble gwy_vec_cos (GwyVec v1, GwyVec v2)
{
  return (gwy_vec_dot (v1, v2)/
            (gwy_vec_abs(v1)* gwy_vec_abs(v2)) );
}
void  gwy_vec_normalize (GwyVec *v)
{
    gdouble vabs = gwy_vec_abs (*v);
    v->x /= vabs;
    v->y /= vabs;
    v->z /= vabs;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
