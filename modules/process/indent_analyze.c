/*
 *  @(#) $Id$
 *  Copyright (C) 2006 Lukas Chvatal, David Necas (Yeti), Petr Klapetek.
 *  E-mail: chvatal@physics.muni.cz, yeti@gwyddion.net, klapetek@gwyddion.net.
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

/* TODO
controls_changed => computed = FALSE

*/

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libprocess/gwyprocess.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/gwyapp.h>
#include <gtk/gtk.h>

#define  SURE_IMPRESSION_COEFF   0.3

#define INDENT_ANALYZE_RUN_MODES \
   GWY_RUN_INTERACTIVE

enum {
    PREVIEW_SIZE = 320
};

enum {
    GWY_PLANE_NONE = 0,
    GWY_PLANE_LEVEL,
    GWY_PLANE_ROTATE
};

typedef enum {
    NORMAL,
    CENTRAL,
    DIRECTION
} FloodFillMode;

enum {
    GWY_HOW_MARK_NEW = 0,
    GWY_HOW_MARK_AND,
    GWY_HOW_MARK_OR,
    GWY_HOW_MARK_NOT,
    GWY_HOW_MARK_XOR,
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
typedef enum {
    GWY_INDENTOR_VICKERS     = 0,
    GWY_INDENTOR_BERKOVICH   = 1,
    GWY_INDENTOR_BERKOVICH_M = 2,
    GWY_INDENTOR_KNOOP       = 3,
    GWY_INDENTOR_BRINELL     = 4,
    GWY_INDENTOR_ROCKWELL    = 5,
    GWY_INDENTOR_CUBECORNER  = 6
} GwyIndentorType;


typedef struct {
    gdouble x, y, z;
} GwyVec;

/* Data for this function. */
typedef struct {

    gint minx;
    gint miny;
    gint maxx;
    gint maxy;

    gdouble max_val;
    gdouble min_val;

    gdouble plane_tol;  /* what to consider as belonging to plane/percents
                           of max-min */
    gdouble phi_tol;
    gdouble theta_tol;    /* XXX: not implemented */

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
    gint plane_correct;    /* XXX: not implemented */

    GwyIndentorType indentor;
    gint nof_sides;
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

    // id of datafield which the mask will be applied to
    gint dfield_id;

} IndentAnalyzeControls;

typedef struct {
    FloodFillMode mode;
    GwyVec v;                /* normal, direction or (centre-position) vector */
    gdouble cos_t1, cos_t2;
    gdouble cos_f1, cos_f2;
    gint seed;               /* side of averaging sqare */
} FloodFillInfo;


static gboolean module_register(void);

static gboolean indent_analyze(GwyContainer *data, GwyRunType run);

static gboolean indent_analyze_dialog(GwyContainer *data,
                                      IndentAnalyzeArgs *args);

static void dialog_update(IndentAnalyzeControls *controls,
                          IndentAnalyzeArgs *args);

/*static void plane_correct_cb(GtkWidget *item, IndentAnalyzeControls *controls);*/
static void how_mark_cb(GtkWidget *item, IndentAnalyzeControls *controls);
static void what_mark_cb(GtkWidget *item, IndentAnalyzeControls *controls);
static void indentor_changed_cb(GtkWidget *item,
                                IndentAnalyzeControls *controls);

static void set_mask_at(GwyDataField *mask, gint x, gint y, gdouble m,
                        gint how);
static void level_data(IndentAnalyzeControls *c);
static void gwy_data_field_get_min_pos(GwyDataField *dfield, gdouble *min, gint *posx,
                            gint *posy);
static void gwy_data_field_get_max_pos(GwyDataField *dfield, gdouble *max, gint *posx,
                            gint *posy);
static void data_field_average_normal_vector(GwyDataField *dfield, gint x,
                                             gint y, gint r, GwyVec *vec);
static gdouble data_field_compute_ds(GwyDataField *dfield, gint x_pos,
                                     gint y_pos);
static void indentmask_flood_fill(GwyDataField *indentmask, gint i, gint j,
                                  GwyDataField *dfield, FloodFillInfo *ffi);
static void compute_expected_indent(IndentAnalyzeControls *c);

static gboolean indent_analyze_do_the_hard_work(IndentAnalyzeControls *controls);
static void compute_and_preview(IndentAnalyzeControls *controls);
static gboolean indent_analyze_ok(GwyContainer *data,
                                  IndentAnalyzeControls *controls);

static void save_statistics_dialog(IndentAnalyzeControls *c,
                                   GtkWidget *dialog);

static void read_data_from_controls(IndentAnalyzeControls *c);
static void update_data_labels(IndentAnalyzeControls *c);

static void load_args(GwyContainer *container, IndentAnalyzeArgs * args);
static void save_args(GwyContainer *container, IndentAnalyzeArgs * args);
static void sanitize_args(IndentAnalyzeArgs * args);

static GwyVec gwy_vec_cross(GwyVec v1, GwyVec v2);
static gdouble gwy_vec_dot(GwyVec v1, GwyVec v2);
static GwyVec gwy_vec_times(GwyVec v, gdouble c);
static gdouble gwy_vec_abs(GwyVec v);
//static gdouble gwy_vec_arg_phi(GwyVec v);
//static gdouble gwy_vec_arg_theta(GwyVec v);
static gdouble gwy_vec_cos(GwyVec v1, GwyVec v2);
static void gwy_vec_normalize(GwyVec *v);

GwyEnum plane_correct_enum[] = {
    { N_("Do nothing"),   GWY_PLANE_NONE   },
    { N_("Plane level"),  GWY_PLANE_LEVEL  },
    { N_("Plane rotate"), GWY_PLANE_ROTATE },
};

GwyEnum how_mark_enum[] = {
    { N_("New"), GWY_HOW_MARK_NEW },
    { N_("AND"), GWY_HOW_MARK_AND },
    { N_("OR"),  GWY_HOW_MARK_OR  },
    { N_("NOT"), GWY_HOW_MARK_NOT },
    { N_("XOR"), GWY_HOW_MARK_XOR },
};

GwyEnum what_mark_enum[] = {
    { N_("Nothing"),        GWY_WHAT_MARK_NOTHING     },
    { N_("Above"),          GWY_WHAT_MARK_ABOVE       },
    { N_("Bellow"),         GWY_WHAT_MARK_BELOW       },
    { N_("Plane"),          GWY_WHAT_MARK_PLANE       },
    { N_("Impression"),     GWY_WHAT_MARK_INDENTATION },
    { N_("Inner Pile-up"),  GWY_WHAT_MARK_INNERPILEUP },
    { N_("Outer Pile-up"),  GWY_WHAT_MARK_OUTERPILEUP },
    { N_("Special points"), GWY_WHAT_MARK_POINTS      },
    { N_("Faces border"),   GWY_WHAT_MARK_FACESBORDER },
};

GwyEnum indentor_enum[] = {
    { N_("Vickers"),    GWY_INDENTOR_VICKERS, },
    { N_("Berkovich"),  GWY_INDENTOR_BERKOVICH, },
    { N_("Berkovich (modified)"),  GWY_INDENTOR_BERKOVICH_M, },
    { N_("Knoop"),      GWY_INDENTOR_KNOOP, },
    { N_("Brinell"),   GWY_INDENTOR_BRINELL, },
    { N_("Cube corner"), GWY_INDENTOR_CUBECORNER, },
    { N_("Rockwell"),   GWY_INDENTOR_ROCKWELL, },
};




/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    (GwyModuleRegisterFunc) &module_register,
    N_("Analyses nanoindentation structure (volumes, surfaces, ...)."),
    "Lukáš Chvátal <chvatal@physics.muni.cz>",
    "0.1.4",
    "Lukáš Chvátal",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)


static gboolean
module_register(void)
{
    gwy_process_func_register("indent_analyze",
                              (GwyProcessFunc)&indent_analyze,
                              N_("/Indento_r/_Analyze..."),
                              NULL,
                              INDENT_ANALYZE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              /* XXX: Make it translatable once there's some
                               * actual tooltip text. */
                              "");

    return TRUE;
}

static gboolean
indent_analyze(GwyContainer *data, GwyRunType run)
{

    IndentAnalyzeArgs args;

    g_return_val_if_fail(run & INDENT_ANALYZE_RUN_MODES, FALSE);

    if (run == GWY_RUN_INTERACTIVE) {
       load_args(gwy_app_settings_get(), &args);
       indent_analyze_dialog(data, &args);
       save_args(gwy_app_settings_get(), &args);
    } else {
       g_warning("Non-interactive mode not supported.");
    }

    return TRUE;
}

/** Create preview container. From given container get /0/data and use it to create
 * /0/data and /0/mask for preview.
 *
 * \param  data source container
 *
 * \return preview container
 */
static GwyContainer*
create_preview_data(IndentAnalyzeControls *controls)
{
    GwyContainer *preview_container = NULL;
    GwyDataField *dfield = NULL, *mask;
    gint oldid;
    gint xres, yres;
    // No zoom, zoom disorder final result
    // gdouble zoomval;
    const GwyRGBA mask_color = { 1.0, 0.0, 0.00, 0.5 };

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    controls->dfield_id = oldid;

    if (dfield) {
      preview_container = gwy_container_new();
      dfield = gwy_data_field_duplicate(dfield);

      xres = gwy_data_field_get_xres(dfield);
      yres = gwy_data_field_get_yres(dfield);
      // No zoom because it disorder mask on original image.
      // zoomval = (gdouble)PREVIEW_SIZE/MAX(xres, yres);
      // gwy_data_field_resample(dfield, xres*zoomval, yres*zoomval,
      //                        GWY_INTERPOLATION_LINEAR);


      gwy_container_set_object_by_name(preview_container, "/0/data", dfield);
      mask = gwy_data_field_new_alike(dfield, TRUE);
      g_object_unref(dfield);
      gwy_container_set_object_by_name(preview_container, "/0/mask", mask);
      g_object_unref(mask);

      /*
      gwy_app_sync_data_items(data, preview_container, oldid, 0, FALSE,
            GWY_DATA_ITEM_GRADIENT, GWY_DATA_ITEM_RANGE,
            GWY_DATA_ITEM_MASK_COLOR, 0);
      */

      gwy_rgba_store_to_container(gwy_rgba_copy(&mask_color),
            preview_container, "/0/mask");
   }

    return preview_container;
}
/** Create table with statistical info.
 *
 * \param  controls build object
 *
 * \return GtkTable object
 */
static GtkWidget*
create_parameters_table(IndentAnalyzeControls * controls)
{
   IndentAnalyzeArgs * args = controls->args;
   int row = 0;
   GtkWidget *table = 0, *label;
   GwyDataField *dfield;
   GwySIValueFormat *siformat;
   GString *siu;

   /* TABLE */
   table = gtk_table_new(8, 3, FALSE);
   gtk_table_set_col_spacings(GTK_TABLE(table), 4);
   gtk_container_set_border_width(GTK_CONTAINER(table), 4);

   /*
    controls.w_plane_correct
    = gwy_option_menu_create(plane_correct_enum,
    G_N_ELEMENTS(plane_correct_enum),
    "menu_plane_correct",
    G_CALLBACK(plane_correct_cb), &controls,
    args->plane_correct);
    gwy_table_attach_hscale(table, row, _("Data field _leveling:"), NULL,
    GTK_OBJECT(controls.w_plane_correct),
    GWY_HSCALE_WIDGET);
    row++;
    */

   controls->w_what_mark = gwy_enum_combo_box_new(what_mark_enum,
         G_N_ELEMENTS(what_mark_enum), G_CALLBACK(what_mark_cb), controls,
         args->what_mark, TRUE);
   gwy_table_attach_hscale(table, row++, _("Marked _areas:"), NULL,
         GTK_OBJECT(controls->w_what_mark), GWY_HSCALE_WIDGET);

   controls->w_indentor = gwy_enum_combo_box_new(indentor_enum,
         G_N_ELEMENTS(indentor_enum), G_CALLBACK(indentor_changed_cb),
         controls, args->indentor, TRUE);
   gwy_table_attach_hscale(table, row++, _("_Indentor type:"), NULL,
         GTK_OBJECT(controls->w_indentor), GWY_HSCALE_WIDGET);

   controls->w_how_mark = gwy_enum_combo_box_new(how_mark_enum,
         G_N_ELEMENTS(how_mark_enum), G_CALLBACK(how_mark_cb), controls,
         args->how_mark, TRUE);
   gwy_table_attach_hscale(table, row++, _("_Mask creation type:"), NULL,
         GTK_OBJECT(controls->w_how_mark), GWY_HSCALE_WIDGET);

   controls->w_plane_tol = gtk_adjustment_new(args->plane_tol, 0, 100, 0.1, 1,
                                              0);
   gwy_table_attach_hscale(table, row++, _("Ref. plane _tolerance:"), "%",
         controls->w_plane_tol, 0);

   controls->w_phi_tol = gtk_adjustment_new(args->phi_tol, 0, 180, 0.1, 10, 0);
   gwy_table_attach_hscale(table, row++, _("Angle _1 tolerance:"), _("deg"),
         controls->w_phi_tol, 0);

   /* XXX: what the hell is this for?
    controls.w_theta_tol = gtk_adjustment_new(args->theta_tol,
    0, 90, 0.1, 10, 0);
    spin = gwy_table_attach_hscale(table, row, _("T_heta range:"), _("deg"),
    controls.w_theta_tol, 0);
    row++;
    */

   /* "statistical" labels */
   dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
         "/0/data"));
   if (!dfield) {
      g_warning("Cannot get /0/data from controls->mydata.");
   }
   siformat = gwy_data_field_get_value_format_xy(dfield,
         GWY_SI_UNIT_FORMAT_PLAIN, NULL);
   if (!siformat) {
      g_warning("Cannot get siformat from /0/data");
   }
   siu = g_string_new(siformat->units);

   controls->w_min_xy = gtk_label_new("");
   gwy_table_attach_row(table, row++, _("Indent centre at"), siu->str,
         controls->w_min_xy);
   controls->w_max_xy = gtk_label_new("");
   gwy_table_attach_row(table, row++, _("Maximum at"), siu->str,
         controls->w_max_xy);
   controls->w_minmax = gtk_label_new("");
   gwy_table_attach_row(table, row++, _("Max-min diference"), siu->str,
         controls->w_minmax);

   g_string_append(siu, "<sup>2</sup>");

   controls->w_surface_indent_exp = gtk_label_new("");
   gwy_table_attach_row(table, row, _("Expected A<sub>d</sub>:"), siu->str,
         controls->w_surface_indent_exp);
   if ( (label = gwy_table_get_child_widget(table, row, 0)) ) {
      gtk_label_set_markup_with_mnemonic(GTK_LABEL(label),
            gtk_label_get_text(GTK_LABEL(label)) );
   }
   row++;
   controls->w_area_indent_exp = gtk_label_new("");
   gwy_table_attach_row(table, row, _("Expected A<sub>p</sub>:"), siu->str,
         controls->w_area_indent_exp);
   if ( (label = gwy_table_get_child_widget(table, row, 0)) ) {
      gtk_label_set_markup_with_mnemonic(GTK_LABEL(label),
            gtk_label_get_text(GTK_LABEL(label)) );
   }
   row++;

   gtk_table_attach(GTK_TABLE(table), gtk_hseparator_new(), 0, 2, row, row + 1,
         (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_EXPAND
               | GTK_FILL), 0, 0);
   row++;

   /*
    controls.w_area_above = gtk_label_new ("");
    gtk_label_set_justify( &controls.w_area_above, GTK_JUSTIFY_RIGHT);
    gwy_table_attach_row(table, row, _("Area above"), siu->str,controls.w_area_above);
    row++;
    controls.w_area_below = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Area below"), siu->str,controls.w_area_below);
    row++;
    controls.w_area_plane = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Area of plane"), siu->str,controls.w_area_plane);
    row++;

    controls.w_surface_above = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Surface above"), siu->str,controls.w_surface_above);
    row++;
    controls.w_surface_below = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Surface below"), siu->str,controls.w_surface_below);
    row++;
    */

   g_string_assign(siu, siformat->units);
   g_string_append(siu, "<sup>3</sup>");
   /*
    controls.w_volume_above = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Volume above"), siu->str,controls.w_volume_above);
    row++;
    controls.w_volume_below = gtk_label_new ("");
    gwy_table_attach_row(table, row, _("Volume below"), siu->str,controls.w_volume_below);
    row++;
    */
   controls->w_volume_dif = gtk_label_new("");
   gwy_table_attach_row(table, row++, _("Volume above-below"), siu->str,
         controls->w_volume_dif);

   gtk_table_attach(GTK_TABLE(table), gtk_hseparator_new(), 0, 2, row, row + 1,
         (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_EXPAND
               | GTK_FILL), 0, 0);
   row++;

   controls->w_volume_indent = gtk_label_new("");
   gwy_table_attach_row(table, row++, _("Indent. volume"), siu->str,
         controls->w_volume_indent);

   g_string_assign(siu, siformat->units);
   g_string_append(siu, "<sup>2</sup>");
   controls->w_surface_indent = gtk_label_new("");
   gwy_table_attach_row(table, row, _("Indent. A<sub>d</sub>"), siu->str,
         controls->w_surface_indent);
   if ( (label = gwy_table_get_child_widget(table, row, 0)) ) {
      gtk_label_set_markup_with_mnemonic(GTK_LABEL(label),
            gtk_label_get_text(GTK_LABEL(label)) );
   }
   row++;

   controls->w_area_indent = gtk_label_new("");
   gwy_table_attach_row(table, row, _("Indent. A<sub>p</sub>"), siu->str,
         controls->w_area_indent);
   if ( (label = gwy_table_get_child_widget(table, row, 0)) ) {
      gtk_label_set_markup_with_mnemonic(GTK_LABEL(label),
            gtk_label_get_text(GTK_LABEL(label)) );
   }
   row++;

   gtk_table_attach(GTK_TABLE(table), gtk_hseparator_new(), 0, 2, row, row + 1,
         (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_EXPAND
               | GTK_FILL), 0, 0);
   row++;

   controls->w_surface_innerpileup = gtk_label_new("");
   gwy_table_attach_row(table, row, _("Inner Pile-Up A<sub>d</sub>"), siu->str,
         controls->w_surface_innerpileup);
   if ( (label = gwy_table_get_child_widget(table, row, 0)) ) {
      gtk_label_set_markup_with_mnemonic(GTK_LABEL(label),
            gtk_label_get_text(GTK_LABEL(label)) );
   }

   row++;
   controls->w_area_innerpileup = gtk_label_new("");
   gwy_table_attach_row(table, row, _("Inner Pile-Up A<sub>p</sub>"), siu->str,
         controls->w_area_innerpileup);
   if ( (label = gwy_table_get_child_widget(table, row, 0)) ) {
      gtk_label_set_markup_with_mnemonic(GTK_LABEL(label),
            gtk_label_get_text(GTK_LABEL(label)) );
   }

   row++;
   controls->w_surface_outerpileup = gtk_label_new("");
   gwy_table_attach_row(table, row, _("Outer Pile-Up A<sub>d</sub>"), siu->str,
         controls->w_surface_outerpileup);
   if ( (label = gwy_table_get_child_widget(table, row, 0)) ) {
      gtk_label_set_markup_with_mnemonic(GTK_LABEL(label),
            gtk_label_get_text(GTK_LABEL(label)) );
   }

   row++;
   controls->w_area_outerpileup = gtk_label_new("");
   gwy_table_attach_row(table, row, _("Outer Pile-Up A<sub>p</sub>"), siu->str,
         controls->w_area_outerpileup);
   if ( (label = gwy_table_get_child_widget(table, row, 0)) ) {
      gtk_label_set_markup_with_mnemonic(GTK_LABEL(label),
            gtk_label_get_text(GTK_LABEL(label)) );
   }

   g_string_free(siu, TRUE);
   gwy_si_unit_value_format_free(siformat);

   return table;
}

/** Create and run analyze dialog
 *
 * \param  data source container
 * \param  args not sure
 *
 * \return not sure
 */
static gboolean
indent_analyze_dialog(GwyContainer *data, IndentAnalyzeArgs * args)
{
    GtkWidget *dialog, *table, *hbox;
    IndentAnalyzeControls controls;
    gint response;
    enum {
        RESPONSE_COMPUTE = 1,
        RESPONSE_SAVE = 2
    };
    //gdouble zoomval;
    GtkObject *layer;

    controls.args = args;
    dialog = gtk_dialog_new_with_buttons(_("Indentaion statistics"), NULL, 0,
                                         _("_Compute & mark"), RESPONSE_COMPUTE,
                                         _("_Save statistics"), RESPONSE_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(hbox),
                       FALSE, FALSE, 4);
    // create mydata
    controls.mydata = create_preview_data(&controls);
    // create view
    controls.view = gwy_data_view_new(controls.mydata);
    // data layer
    layer = GTK_OBJECT(gwy_layer_basic_new());
    gwy_pixmap_layer_set_data_key(GWY_PIXMAP_LAYER(layer), "/0/data");
    //gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));
    // mask layer
    layer = GTK_OBJECT(gwy_layer_mask_new());
    gwy_pixmap_layer_set_data_key(GWY_PIXMAP_LAYER(layer), "/0/mask");
    gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
    gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls.view),
                                  GWY_PIXMAP_LAYER(layer));
    gtk_box_pack_start(GTK_BOX(hbox), controls.view, TRUE, TRUE, 4);
    //gtk_widget_show_all(GTK_WIDGET(controls.view));

    table = create_parameters_table(&controls);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 4);
    gtk_widget_show_all(GTK_WIDGET(hbox));

    controls.computed = FALSE;

    gtk_widget_show_all(dialog);
    do {
      response = gtk_dialog_run(GTK_DIALOG(dialog));
      switch (response) {
         case GTK_RESPONSE_CANCEL:
         case GTK_RESPONSE_DELETE_EVENT:
            g_object_unref(controls.mydata);
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
            save_statistics_dialog(&controls, dialog);
            break;

         default:
            g_assert_not_reached();
            break;
      }
   } while (response != GTK_RESPONSE_OK);

   indent_analyze_ok(data, &controls);

   g_object_unref(controls.mydata);
   gtk_widget_destroy(dialog);

   return controls.computed;
}


/* ====================================================================================== */

static void
get_field_slope_from_border(GwyDataField *dfield, gdouble *c, gdouble *bx,
                            gdouble *by)
{
    gint size = 20;
    gdouble cc;

    gwy_data_field_area_fit_plane(dfield, NULL, 0, 0, dfield->xres - 1, size,
                                  &cc, bx, NULL);
    gwy_data_field_area_fit_plane(dfield, NULL, 0, 0, size, dfield->yres - 1,
                                  c, NULL, by);
    *c = (*c + cc)/2;
}


static void
gwy_data_field_get_min_pos(GwyDataField *dfield,
                           gdouble *min,
                           gint *posx, gint *posy)
{
    gdouble mm = G_MAXDOUBLE;
    const gdouble *data, *m;
    gint i, xres, yres;

    data = m = gwy_data_field_get_data_const(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    for (i = xres*yres; i; i--, data++) {
        if (G_UNLIKELY(*data < mm)) {
            mm = *data;
            m = data;
        }
    }
    *min = mm;
    *posx = (m - gwy_data_field_get_data_const(dfield)) % xres;
    *posy = (m - gwy_data_field_get_data_const(dfield))/xres;
}

static void
gwy_data_field_get_max_pos(GwyDataField *dfield,
                           gdouble *max,
                           gint *posx, gint *posy)
{
    gdouble mm = -G_MAXDOUBLE;
    const gdouble *data, *m;
    gint i, xres, yres;

    data = m = gwy_data_field_get_data_const(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    for (i = xres*yres; i; i--, data++) {
        if (G_UNLIKELY(*data > mm)) {
            mm = *data;
            m = data;
        }
    }
    *max = mm;
    *posx = (m - gwy_data_field_get_data_const(dfield)) % xres;
    *posy = (m - gwy_data_field_get_data_const(dfield))/xres;
}

static gdouble
data_field_compute_ds(GwyDataField *dfield, gint x_pos, gint y_pos)
{
    gdouble y_tr, y_br, y_tl, y_bl, ds;
    GwyVec v1, v2, s;
    gdouble dx, dy;

    dx = gwy_data_field_get_xreal(dfield)/gwy_data_field_get_xres(dfield);
    dy = gwy_data_field_get_yreal(dfield)/gwy_data_field_get_yres(dfield);

    y_tr = gwy_data_field_get_val(dfield, x_pos + 1, y_pos);
    y_br = gwy_data_field_get_val(dfield, x_pos + 1, y_pos + 1);
    y_tl = gwy_data_field_get_val(dfield, x_pos, y_pos);
    y_bl = gwy_data_field_get_val(dfield, x_pos, y_pos + 1);

    v1.x = dx;
    v1.y = 0;
    v1.z = y_tl - y_tr;

    v2.x = 0;
    v2.y = dy;
    v2.z = y_bl - y_tl;

    s = gwy_vec_cross(v1, v2);

    ds = gwy_vec_abs(s);

    v1.z = y_bl - y_br;
    v2.z = y_br - y_tr;

    s = gwy_vec_cross(v1, v2);

    return (ds + gwy_vec_abs(s))/2;
}

static void
data_field_average_normal_vector(GwyDataField *dfield,
                                 gint x, gint y,
                                 gint r,
                                 GwyVec *vector)
{
    gdouble theta, phi;
    gint col, row, xres, yres;

    col = MAX(x - r, 0);
    row = MAX(y - r, 0);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gwy_data_field_area_get_inclination(dfield, col, row,
                                        MIN(x + r, dfield->xres - 1) - col,
                                        MIN(y + r, dfield->yres - 1) - row,
                                        &theta, &phi);
    vector->x = sin(theta)*cos(phi);
    vector->y = sin(theta)*sin(phi);
    vector->z = cos(theta);
}

static void
set_mask_at(GwyDataField *mask, gint x, gint y, gdouble m, gint how)
{
    gint act_mask = (int)gwy_data_field_get_val(mask, x, y);
    gint im = (int)m;
    //printf("im=%d m=%f\n", im, m);

    switch (how) {
        case GWY_HOW_MARK_NEW:
            act_mask = im;
            break;

        case GWY_HOW_MARK_AND:
            act_mask = (act_mask && im);
            break;

        case GWY_HOW_MARK_OR:
            act_mask = (act_mask || im);
            /*    if(act_mask){
               FILE* fl = fopen ("setmask.txt","a");
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
    gwy_data_field_set_val(mask, x, y, (double)act_mask);
}

void
level_data(IndentAnalyzeControls *c)
{
    gint iter = 3;
    IndentAnalyzeArgs *args = c->args;
    GwyDataField *dfield =
        GWY_DATA_FIELD(gwy_container_get_object_by_name(c->mydata, "/0/data"));

    get_field_slope_from_border(dfield, &(args->plane_c), &(args->plane_x),
                                &(args->plane_y));


    while (iter--) {
        gwy_data_field_plane_level(dfield, args->plane_c, args->plane_x,
                                   args->plane_y);
        get_field_slope_from_border(dfield, &(args->plane_c), &(args->plane_x),
                                    &(args->plane_y));
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
                                GWY_INTERPOLATION_LINEAR);
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

static void indentmask_flood_fill(GwyDataField *indentmask, gint i, gint j,
      GwyDataField *dfield, FloodFillInfo * ffi)
{
   gint test = 0;
   gdouble c_f, c_t;
   gint dfield_xres = gwy_data_field_get_xres(dfield);
   gint dfield_yres = gwy_data_field_get_yres(dfield);
   GwyVec v;
   GwyVec e_z = { 0, 0, 1 };
   GwyVec r;
   GwyVec tmp;
   FFPoint *pq; /* points queue */
   FFPoint *tail, *head; /* head points on free position */

   pq = g_new(FFPoint, FLOOD_MAX_POINTS);
   tail = pq;
   head = pq;

   data_field_average_normal_vector(dfield, i, j, ffi->seed, &v);
   head->x = i;
   head->y = j;
   gwy_data_field_set_val(indentmask, i, j, FLOOD_QUEUED);
   head++;

   while (head != tail) {
      data_field_average_normal_vector(dfield, tail->x, tail->y, ffi->seed, &v);
      test = 0;
      switch (ffi->mode) {
         case NORMAL:
            c_f = gwy_vec_cos(v, ffi->v);
            if ((c_f >= ffi->cos_f1) && (c_f <= ffi->cos_f2))
               test = 1;
            break;
         case DIRECTION:
            c_f = gwy_vec_cos(v, e_z);
            v.z = 0;
            c_t = gwy_vec_cos(v, ffi->v);
            if ((c_f >= ffi->cos_f1) && (c_f <= ffi->cos_f2) && (c_t
                  >= ffi->cos_t1) && (c_t <= ffi->cos_t2))
               test = 1;
            break;
         case CENTRAL:
            r.x = ffi->v.x - tail->x;
            r.y = ffi->v.y - tail->y;
            r.z = 0;
            gwy_vec_normalize(&r);
            tmp = gwy_vec_times(r, gwy_vec_dot(v, r));
            tmp.z = v.z;
            c_f = gwy_vec_cos(tmp, r);
            if ((c_f >= ffi->cos_f1) && (c_f <= ffi->cos_f2))
               test = 1;
            break;
      }

      if (test) {
         gint rr, s;
         gwy_data_field_set_val(indentmask, tail->x, tail->y, INDENT_INSIDE);
         for (rr = -1; rr <= 1; rr++) {
            for (s = -1; s <= 1; s++) {
               if (s || rr) {
                  // check if point is inside datafield
                  if (tail->x + rr >= 0 && tail->x + rr < dfield_xres
                        && tail->y + s >= 0 && tail->y + s < dfield_yres)
                  {
                     gdouble val = gwy_data_field_get_val(indentmask, tail->x
                           + rr, tail->y + s);
                     if (!val) {
                        head->x = tail->x + rr;
                        head->y = tail->y + s;
                        if (pq + FLOOD_MAX_POINTS - 1 > head) {
                           head++;
                        } else {
                           head = pq;
                        }
                        gwy_data_field_set_val(indentmask, tail->x + rr,
                              tail->y + s, FLOOD_QUEUED);
                     }
                  } else {
                     printf("start %d %d, want to set %d %d, dimensions %d %d\n", i, j, tail->x
                           + rr, tail->y + s, dfield_xres, dfield_yres);
                  }
               }
            }
         }
      } else {
         gwy_data_field_set_val(indentmask, tail->x, tail->y, INDENT_BORDER);
      }

      if (tail == pq + FLOOD_MAX_POINTS - 1) {
         tail = pq;
      } else
         tail++;
   } /* while */
   g_free(pq);
}

static void
reset_args(IndentAnalyzeArgs *args)
{
   args->volume_above = 0;
   args->volume_below = 0;
   args->area_above = 0;
   args->area_below = 0;
   args->surface_above = 0;
   args->surface_below = 0;

   args->volume_indent = 0;
   args->surface_indent = 0;
   args->area_indent = 0;
   args->area_plane = 0;

   args->surface_innerpileup = 0;
   args->surface_outerpileup = 0;
   args->area_innerpileup = 0;
   args->area_outerpileup = 0;
}

static gboolean
indent_analyze_do_the_hard_work(IndentAnalyzeControls * controls)
{
    GwyContainer *data = controls->mydata;
    GwyDataField *dfield;
    //GwySIUnit *siunit;
    GwyDataField *mask = NULL;
    IndentAnalyzeArgs *args = controls->args;
    GwyDataLine *derdist;

    gint mark_it = 0;
    gint i, j;
    gdouble dx, dy, ds;
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
    gdouble cos_phi_tol = cos(args->phi_tol * G_PI/180.);

    gdouble derdist_max;
    GwyDataField *indentmask = NULL;
    gdouble ok;
    FloodFillInfo ffi;


    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    // gwy_app_undo_checkpoint(data, "/0/data", NULL); why?
    if (!gwy_container_gis_object_by_name(data, "/0/mask", (GObject **)&mask)) {
       g_warning("Cannot find mask datafield.");
       return FALSE;
    }
    read_data_from_controls(controls);
    level_data(controls);

    switch (controls->args->indentor) {
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

    gwy_data_field_get_min_pos(dfield, &args->min_val,
                               &args->minx, &args->miny);
    gwy_data_field_get_max_pos(dfield, &args->max_val,
                               &args->maxx, &args->maxy);
    if (args->what_mark == GWY_WHAT_MARK_POINTS) {
       gwy_data_field_area_fill(mask, args->minx - 2, args->miny - 2, 4, 4, 1.0);
       gwy_data_field_area_fill(mask, args->maxx - 2, args->maxy - 2, 4, 4, 1.0);
    }
    minmax = (args->max_val - args->min_val) * args->plane_tol/100.0;

    dx = gwy_data_field_get_xreal(dfield) /
        (double)gwy_data_field_get_xres(dfield);
    dy = gwy_data_field_get_yreal(dfield) /
        (double)gwy_data_field_get_yres(dfield);
    ds = dx * dy;

    total_area = 0;
    flat_area = 0;
    // set particular args to zero
    reset_args(args);

    /* computing char. of indentation area */
    sx = (args->minx - args->maxx);
    sy = (args->miny - args->maxy);
    side_r = sqrt(sx * sx + sy * sy);


    derdist = GWY_DATA_LINE(gwy_data_line_new(360, 2 * G_PI, FALSE));
    gwy_data_field_slope_distribution(dfield, derdist, 5);

    derdist_max = 0;
    for (i = 0; i < 360; i++) {
        if (gwy_data_line_get_val(derdist, i) > derdist_max) {
            derdist_max = gwy_data_line_get_val(derdist, i);
            side_dir = i;
        }
    }
    side_dir *= G_PI/180.;

    g_object_unref(derdist);

    /* (marking)  INDENTATION */
    //indentmask = gwy_data_field_duplicate(dfield);
    //gwy_data_field_fill(indentmask, 0.0);
    indentmask = gwy_data_field_new_alike(dfield, TRUE);
    for (i = 0; i < args->nof_sides; i++) {
        height = -1e10;
        side_r = 0;
        while (height < SURE_IMPRESSION_COEFF * args->min_val) {

            sx = args->minx - side_r * cos(side_dir +
                                           i * 2 * G_PI/args->nof_sides);
            sy = args->miny - side_r * sin(side_dir +
                                           i * 2 * G_PI/args->nof_sides);
            height = gwy_data_field_get_val(dfield, sx, sy);
            side_r++;
        }
        data_field_average_normal_vector(dfield, sx, sy, avg_diam, avg_vec + i);
        if (args->what_mark == GWY_WHAT_MARK_POINTS) {
           gwy_data_field_area_fill(mask, (int)sx - 2 - i, (int)sy - 2 - i, 4 + i, 4 + i, 1.0);

        }
        else {
            ffi.mode = NORMAL;
            ffi.cos_f1 = cos_phi_tol;
            ffi.cos_f2 = 1.0;
            ffi.v = avg_vec[i];
            ffi.seed = 2;
            indentmask_flood_fill(indentmask, ROUND(sx), ROUND(sy), dfield,
                                  &ffi);
        }
    }

    /* we do not expect we should use the last coords...ensurance for computing vectors for surface */
    for (i = 1; i < dfield->xres - 1; i++) {
        for (j = 1; j < dfield->yres - 1; j++) {
            height = gwy_data_field_get_val(dfield, i, j);
            mark_it = 0;

            if (height > minmax) {
                args->volume_above += ds * height;
                args->area_above += ds;
                args->surface_above += data_field_compute_ds(dfield, i, j);

                args->area_outerpileup += ds;
                args->surface_outerpileup +=
                    data_field_compute_ds(dfield, i, j);
                if (args->what_mark == GWY_WHAT_MARK_ABOVE
                      || args->what_mark == GWY_WHAT_MARK_OUTERPILEUP)
                {
                   mark_it = 1;
                }
            }
            else if (height < -minmax) {
                args->volume_below += ds * (-height);
                args->area_below += ds;
                args->surface_below += data_field_compute_ds(dfield, i, j);
                if (args->what_mark == GWY_WHAT_MARK_BELOW) {
                    mark_it = 1;
                }
                else if ((height < SURE_IMPRESSION_COEFF * args->min_val)
                         && (args->what_mark == GWY_WHAT_MARK_INDENTATION)) {
                    mark_it = 1;
                }
                else if (args->what_mark == GWY_WHAT_MARK_INNERPILEUP ||
                         args->what_mark == GWY_WHAT_MARK_OUTERPILEUP) {
                    mark_it = 0;
                }
            }
            else {
                args->area_plane += ds;
                if (args->what_mark == GWY_WHAT_MARK_PLANE) {
                    mark_it = 1;
                }
            }

            ok = 0;
            if (gwy_data_field_get_val(indentmask, i, j) == INDENT_INSIDE) {
                ok = 1;
            }
            else if (gwy_data_field_get_val(indentmask, i + 1, j)
                     + gwy_data_field_get_val(indentmask, i, j + 1)
                     + gwy_data_field_get_val(indentmask, i, j - 1)
                     + gwy_data_field_get_val(indentmask, i - 1, j)
                     + gwy_data_field_get_val(indentmask, i + 1, +j)
                     + gwy_data_field_get_val(indentmask, i - 1, j + 1)
                     + gwy_data_field_get_val(indentmask, i - 1, j - 1)
                     + gwy_data_field_get_val(indentmask, i - 1,
                                              +j) >= INDENT_INSIDE * 4) {
                ok = 1;
            }

            if (ok) {
                height = gwy_data_field_get_val(dfield, i, j);
                args->surface_indent += data_field_compute_ds(dfield, i, j);
                args->volume_indent += ds * (height - args->min_val);
                args->area_indent += ds;
                if (height > 0) {
                    args->area_innerpileup += ds;
                    args->surface_innerpileup +=
                        data_field_compute_ds(dfield, i, j);
                }
                switch (args->what_mark) {
                   case GWY_WHAT_MARK_INDENTATION:
                    mark_it = 1.0;
                    break;
                   case GWY_WHAT_MARK_OUTERPILEUP:
                    mark_it = 0.0;
                    break;
                   case GWY_WHAT_MARK_INNERPILEUP:
                    mark_it = (height >= 0) ? 1.0 : 0.0;
                    break;
                }

            }

            if (gwy_data_field_get_val(indentmask, i, j) == INDENT_BORDER) {
                if (args->what_mark == GWY_WHAT_MARK_FACESBORDER)
                    mark_it = 1.0;
            }
            if (args->what_mark != GWY_WHAT_MARK_NOTHING &&
                args->what_mark != GWY_WHAT_MARK_POINTS)
                set_mask_at(mask, i, j, mark_it, args->how_mark);
        }
    }
    g_object_unref(indentmask);

    gwy_data_field_invalidate(dfield);
    gwy_data_field_invalidate(mask);
    gwy_data_field_data_changed(dfield);
    gwy_data_field_data_changed(mask);

    controls->computed = TRUE;
    return TRUE;
}


static gboolean
indent_analyze_ok(GwyContainer *data, IndentAnalyzeControls * controls)
{
    //GwyDataField *dfield;
    GObject *maskfield;
    GString *mask_name = g_string_new("");

    // get right ID of datafield
    g_string_printf(mask_name, "/%d/mask", controls->dfield_id);

    if (!controls->computed) {
        indent_analyze_do_the_hard_work(controls);
    }

    maskfield = gwy_container_get_object_by_name(controls->mydata, "/0/mask");
    // mark undo
    gwy_app_undo_checkpoint(data, mask_name->str, NULL);

    gwy_container_set_object_by_name(data, mask_name->str, maskfield);
    gwy_data_field_data_changed(GWY_DATA_FIELD(maskfield));

    g_string_free(mask_name, TRUE);

    return TRUE;
}


static void
compute_and_preview(IndentAnalyzeControls * controls)
{
    GwyDataField *maskfield = 0, *dfield;
    GwyPixmapLayer *layer;

    dfield =
        GWY_DATA_FIELD(gwy_container_get_object_by_name
                       (controls->mydata, "/0/data"));

    read_data_from_controls(controls);

    /*set up the mask */
    if (gwy_container_contains_by_name(controls->mydata, "/0/mask")) {
        maskfield
            = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                              "/0/mask"));

    }
    else {
       g_warning("Cannot find mask datafield.");
       return;

    }

    controls->computed = indent_analyze_do_the_hard_work(controls);

    if (controls->computed && maskfield) {
        gwy_data_field_data_changed(dfield);
        gwy_data_field_data_changed(maskfield);
        if ( (layer = gwy_data_view_get_alpha_layer(GWY_DATA_VIEW(controls->view))) ) {
            gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));
        }
        //gwy_data_view_update(GWY_DATA_VIEW(controls->view));
    }


}

/* =========== dialog control functions ========================= */

static void
dialog_update(IndentAnalyzeControls * controls, IndentAnalyzeArgs * args)
{

    /*
    gwy_option_menu_set_history(controls->w_plane_correct, "menu_plane_correct",
                                args->plane_correct);
                                */
   /*
    gwy_option_menu_set_history(controls->w_how_mark, "menu_how_mark",
                                args->how_mark);
    gwy_option_menu_set_history(controls->w_what_mark, "menu_what_mark",
                                args->what_mark);
                                */
}

/*
static void
plane_correct_cb(GtkWidget *item, IndentAnalyzeControls * controls)
{
    controls->args->plane_correct
        =
        GPOINTER_TO_UINT(g_object_get_data
                         (G_OBJECT(item), "menu_plane_correct"));
    dialog_update(controls, controls->args);
}
*/

static void
what_mark_cb(GtkWidget *item, IndentAnalyzeControls * controls)
{
    controls->args->what_mark
        = gtk_combo_box_get_active(GTK_COMBO_BOX(item));
        //= GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item), "menu_what_mark"));
    dialog_update(controls, controls->args);
}

static void
how_mark_cb(GtkWidget *item, IndentAnalyzeControls * controls)
{
    controls->args->how_mark
        = gtk_combo_box_get_active(GTK_COMBO_BOX(item));
        //= GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item), "menu_how_mark"));
    dialog_update(controls, controls->args);
}

static void
indentor_changed_cb(GtkWidget *item, IndentAnalyzeControls * controls)
{
    controls->args->indentor =
        gtk_combo_box_get_active(GTK_COMBO_BOX(item));
        //= GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item), "indentor-type"));


    dialog_update(controls, controls->args);
    printf("indentor type %d\n", controls->args->indentor);
    gtk_combo_box_get_active(GTK_COMBO_BOX(item));
}


static const gchar *what_mark_key = "/module/nanoindent/what_mark";
static const gchar *how_mark_key = "/module/nanoindent/how_mark";
static const gchar *plane_correct_key = "/module/nanoindent/plane_correct";
static const gchar *indentor_key = "/module/nanoindent/indentor";

static const gchar *plane_tol_key = "/module/nanoindent/plane_tol";
static const gchar *phi_tol_key = "/module/nanoindent/phi_tol";
static const gchar *theta_tol_key = "/module/nanoindent/theta_tol";

static void
sanitize_args(IndentAnalyzeArgs * args)
{
    args->plane_correct = MIN(args->plane_correct, GWY_PLANE_ROTATE);
    args->what_mark = MIN(args->what_mark, GWY_WHAT_MARK_FACESBORDER);
    args->how_mark = MIN(args->how_mark, GWY_HOW_MARK_XOR);
    args->indentor = MIN(args->indentor, GWY_INDENTOR_CUBECORNER);
    /* TODO */
}

static void
load_args(GwyContainer *container, IndentAnalyzeArgs * args)
{
    args->plane_correct = GWY_PLANE_LEVEL;
    args->what_mark = GWY_WHAT_MARK_NOTHING;
    args->how_mark = GWY_HOW_MARK_NEW;
    args->indentor = GWY_INDENTOR_VICKERS;
    args->plane_tol = 1.0;
    args->phi_tol = 8.0;
    args->theta_tol = 8.0;

    gwy_container_gis_enum_by_name(container, plane_correct_key,
                                   &args->plane_correct);
    gwy_container_gis_enum_by_name(container, what_mark_key,
                                   &args->what_mark);
    gwy_container_gis_enum_by_name(container, how_mark_key,
                                   &args->how_mark);
    gwy_container_gis_enum_by_name(container, indentor_key,
                                   &args->indentor);

    gwy_container_gis_double_by_name(container, plane_tol_key,
                                     &args->plane_tol);
    gwy_container_gis_double_by_name(container, phi_tol_key,
                                     &args->phi_tol);
    gwy_container_gis_double_by_name(container, theta_tol_key,
                                     &args->theta_tol);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container, IndentAnalyzeArgs * args)
{
    /*
    gwy_container_set_enum_by_name(container, plane_correct_key,
                                   args->plane_correct);
                                   */
    gwy_container_set_enum_by_name(container, what_mark_key, args->what_mark);
    gwy_container_set_enum_by_name(container, how_mark_key, args->how_mark);
    gwy_container_set_enum_by_name(container, indentor_key, args->indentor);

    gwy_container_set_double_by_name(container, plane_tol_key, args->plane_tol);
    /*
    gwy_container_set_double_by_name(container, phi_tol_key, args->phi_tol);
    gwy_container_set_double_by_name(container, theta_tol_key, args->theta_tol);
    */
}


static void
read_data_from_controls(IndentAnalyzeControls *c)
{
    c->args->plane_tol =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(c->w_plane_tol));
    /*
    c->args->phi_tol = gtk_adjustment_get_value(GTK_ADJUSTMENT(c->w_phi_tol));
    c->args->theta_tol =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(c->w_theta_tol));
     */
}

static void
update_data_labels(IndentAnalyzeControls *c)
{
    const int str_len = 50;
    gchar str[50];
    GwyDataField *dfield =
        GWY_DATA_FIELD(gwy_container_get_object_by_name(c->mydata, "/0/data"));
    GwySIValueFormat *siformat =
        gwy_data_field_get_value_format_xy(dfield, GWY_SI_UNIT_FORMAT_PLAIN, NULL);
    gdouble mag = siformat->magnitude;

    compute_expected_indent(c);

    g_snprintf(str, str_len, "[%d, %d]px: %lf", c->args->minx, c->args->miny,
            c->args->min_val/mag);
    gtk_label_set_text(GTK_LABEL(c->w_min_xy), str);

    g_snprintf(str, str_len, "[%d, %d]px: %lf", c->args->maxx, c->args->maxy,
            c->args->max_val/mag);
    gtk_label_set_text(GTK_LABEL(c->w_max_xy), str);

    g_snprintf(str, str_len, "%lf", (c->args->max_val - c->args->min_val)/mag);
    gtk_label_set_text(GTK_LABEL(c->w_minmax), str);
/*
    sprintf (str, "%g (%.1lf %%)", c->args->area_above/mag/mag, 100.*(c->args->area_above/sxy));
    gtk_label_set_text(c->w_area_above, str);
    sprintf (str, "%g (%.1lf %%)", c->args->area_below/mag/mag, 100.*(c->args->area_below/sxy));
    gtk_label_set_text(c->w_area_below, str);
    sprintf (str, "%g (%.1lf %%)", c->args->area_plane/mag/mag, 100.*(c->args->area_plane/sxy));
    gtk_label_set_text(c->w_area_plane, str);

    sprintf (str, "%g (+%.1f %%)", c->args->surface_above/mag/mag, 100.*c->args->surface_above/c->args->area_above);
    gtk_label_set_text(c->w_surface_above, str);
    sprintf (str, "%g (+%.1lf %%)", c->args->surface_below/mag/mag, 100.*c->args->surface_below/c->args->surface_below);
    gtk_label_set_text(c->w_surface_below, str);

    sprintf (str, "%g", c->args->volume_above/mag/mag/mag);
    gtk_label_set_text(c->w_volume_above, str);
    sprintf (str, "%g", c->args->volume_below/mag/mag/mag);
    gtk_label_set_text(c->w_volume_below, str);
*/
    g_snprintf(str, str_len, "%g",
            (c->args->volume_above - c->args->volume_below)/mag/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_volume_dif), str);

    g_snprintf(str, str_len, "%g", c->args->volume_indent/mag/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_volume_indent), str);
    g_snprintf(str, str_len, "%g", c->args->surface_indent/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_surface_indent), str);
    g_snprintf(str, str_len, "%g", (c->args->area_indent)/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_area_indent), str);

    g_snprintf(str, str_len, "%g", c->args->surface_indent_exp/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_surface_indent_exp), str);
    g_snprintf(str, str_len, "%g", (c->args->area_indent_exp)/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_area_indent_exp), str);

    g_snprintf(str, str_len, "%g", c->args->surface_innerpileup/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_surface_innerpileup), str);
    g_snprintf(str, str_len, "%g", (c->args->area_innerpileup)/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_area_innerpileup), str);
    g_snprintf(str, str_len, "%g", c->args->surface_outerpileup/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_surface_outerpileup), str);
    g_snprintf(str, str_len, "%g", (c->args->area_outerpileup)/mag/mag);
    gtk_label_set_text(GTK_LABEL(c->w_area_outerpileup), str);
}

static void
save_statistics_dialog(IndentAnalyzeControls *c,
                       GtkWidget *parent)
{
    IndentAnalyzeArgs *args = c->args;
    GwyDataField *dfield;
    GwySIValueFormat *vf;
    const guchar *filename = "nanoindent";
    gchar *filename_utf8, *filename_sys;
    gdouble mag;
    gdouble sxy;
    gint response;
    FILE *out;
    GtkWidget *dialog;

    if (!c->computed) {         /*nothing to output */
        dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("No statistics has benn "
                                          "computed yet."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    /* SAVE .TXT statistics */
    dialog = gtk_file_selection_new(_("Save Indentation Statistics"));
    gwy_container_gis_string_by_name(c->mydata, "/filename", &filename);
    filename_utf8 = g_strconcat(filename, ".txt", NULL);
    filename_sys = g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL);

    gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog), filename_sys);
    g_free(filename_sys);
    g_free(filename_utf8);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
    filename_sys = g_filename_from_utf8(filename, -1, NULL, NULL, NULL);
    gtk_widget_destroy(dialog);

    dfield =
        GWY_DATA_FIELD(gwy_container_get_object_by_name(c->mydata, "/0/data"));
    sxy = gwy_data_field_get_xreal(dfield) * gwy_data_field_get_yreal(dfield);
    vf = gwy_data_field_get_value_format_xy(dfield, GWY_SI_UNIT_FORMAT_PLAIN, NULL);
    /*
    siunit = gwy_si_unit_new("");
    gwy_si_unit_power(gwy_data_field_get_si_unit_xy(dfield), 2, siunit);
    vf2 = gwy_si_unit_get_format(siunit, args->area_plane, NULL);
    gwy_si_unit_multiply(siunit, gwy_data_field_get_si_unit_z(dfield), siunit);
    vf3 = gwy_si_unit_get_format(siunit, args->volume_indent, NULL);
    */
    mag = vf->magnitude;

    out = fopen(filename_sys, "w");
    if (out) {
        /* FIXME: this is wrong, we whould construct area and volume units
         * properly, but in GWYDDION-1 it's impossible to get plain-text value
         * formats */
        fprintf(out, "%s\n", filename_sys);

        fprintf(out, _("Indentor:  %s\n"),
                _(gwy_enum_to_string(args->indentor,
                                     indentor_enum,
                                     G_N_ELEMENTS(indentor_enum))));
        fprintf(out, _("Length units: %s\n"), vf->units);

        fprintf(out, _("Indentation centre at [%d, %d] px:      %lf\n"),
                args->minx, args->miny, args->min_val/mag);
        fprintf(out, _("Maximum at [%d, %d] is:                 %lf\n"),
                args->maxx, args->maxy, args->max_val/mag);
        fprintf(out, _("Diference max-min:                      %lf\n"),
                (args->max_val - args->min_val)/mag);
        fprintf(out, "\n");

        fprintf(out,
                _("Area (projected) above plane:             %g (%.1lf %%)\n"),
                args->area_above/mag/mag,
                100. * (args->area_above/sxy));
        fprintf(out,
                _("Area (projected) below plane:             %g (%.1lf %%)\n"),
                args->area_below/mag/mag,
                100. * (args->area_below/sxy));
        fprintf(out,
                _("Area (projected) of    plane:             %g (%.1lf %%)\n"),
                args->area_plane/mag/mag,
                100. * (args->area_plane/sxy));
        fprintf(out, "\n");

        fprintf(out, _("Area (developed) above %g (+%.1f %%)\n"),
                args->surface_above/mag/mag,
                100. * args->surface_above/sxy);
        fprintf(out, _("Area (developed) above %g (+%.1lf %%)\n"),
                args->surface_below/mag/mag,
                100. * args->surface_below/sxy);

        fprintf(out, _("Volume above:     %g\n"),
                args->volume_above/mag/mag/mag);
        fprintf(out, _("Volume below:     %g\n"),
                args->volume_below/mag/mag/mag);
        fprintf(out, _("Volume diference  %g\n"),
                (args->volume_above -
                 args->volume_below)/mag/mag/mag);

        fprintf(out, _("\nIndentation\n"));
        fprintf(out, _("Volume      %g\n"),
                args->volume_indent/mag/mag/mag);
        fprintf(out, "A_P         %g\n", args->surface_indent/mag/mag);
        fprintf(out, "A_D         %g\n", (args->area_indent)/mag/mag);

        fprintf(out, _("\nIndentation - Inner Pile-Up\n"));
        fprintf(out, "A_P         %g\n",
                args->surface_innerpileup/mag/mag);
        fprintf(out, "A_D         %g\n",
                (args->area_innerpileup)/mag/mag);

        fprintf(out, _("Indentation - Outer Pile-Up\n"));
        fprintf(out, "A_P         %g\n",
                args->surface_outerpileup/mag/mag);
        fprintf(out, "A_D         %g\n",
                (args->area_outerpileup)/mag/mag);
    }
    fclose(out);
    g_free(filename_sys);
    gwy_si_unit_value_format_free(vf);
}


static void
compute_expected_indent(IndentAnalyzeControls *c)
{
    IndentAnalyzeArgs *args = c->args;
    gdouble h2 = args->min_val * args->min_val;

    if (!(c->computed))
        return;

    switch (args->indentor) {
        case GWY_INDENTOR_VICKERS:
        args->area_indent_exp = 24.5042 * h2;
        args->surface_indent_exp = 26.43 * h2;
        break;

        case GWY_INDENTOR_KNOOP:
        args->area_indent_exp = 0 * h2;
        args->surface_indent_exp = 0 * h2;
        break;

        case GWY_INDENTOR_BERKOVICH:
        args->area_indent_exp = 23.96 * h2;
        args->surface_indent_exp = 26.43 * h2;
        break;

        case GWY_INDENTOR_BERKOVICH_M:
        args->area_indent_exp = 24.494 * h2;
        args->surface_indent_exp = 26.97 * h2;
        break;

        case GWY_INDENTOR_ROCKWELL:
        args->area_indent_exp = 0 * h2;
        args->surface_indent_exp = 0 * h2;
        break;

        case GWY_INDENTOR_CUBECORNER:
        args->area_indent_exp = 2.598 * h2;
        args->surface_indent_exp = 4.50 * h2;
        break;

        case GWY_INDENTOR_BRINELL:
        args->area_indent_exp = 0;
        /*FIXME*/ args->surface_indent_exp = 0;
        break;
    }
}



GwyVec
gwy_vec_cross(GwyVec v1, GwyVec v2)
{
    GwyVec result;

    result.z = v1.y * v2.z - v2.y * v1.z;
    result.y = v1.z * v2.x - v2.z * v1.x;
    result.x = v1.x * v2.y - v2.x * v1.y;

    return result;
}

gdouble
gwy_vec_dot(GwyVec v1, GwyVec v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

GwyVec
gwy_vec_times(GwyVec v, gdouble c)
{
    GwyVec v1;

    v1.x = v.x * c;
    v1.y = v.y * c;
    v1.z = v.z * c;

    return v1;
}

gdouble
gwy_vec_abs(GwyVec v)
{
    return sqrt(gwy_vec_dot(v, v));
}

/*
gdouble
gwy_vec_arg_phi(GwyVec v)
{
    return atan2(v.z, v.x);
}

gdouble
gwy_vec_arg_theta(GwyVec v)
{
    return atan2(v.y, v.x);
}
*/
gdouble
gwy_vec_cos(GwyVec v1, GwyVec v2)
{
    return (gwy_vec_dot(v1, v2)/(gwy_vec_abs(v1) * gwy_vec_abs(v2)));
}

void
gwy_vec_normalize(GwyVec * v)
{
    gdouble vabs = gwy_vec_abs(*v);

    v->x /= vabs;
    v->y /= vabs;
    v->z /= vabs;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
