/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/* NB: Write all estimation and fitting functions for point clouds.  This
 * means we can easily update this module to handle XYZ data later. */
/* TODO:
 * - The fitted data are a bunch of GwyXYZ.  Either they come from a surface
 *   whose data we could directly use, or we would just create such surface
 *   even from field data.  In the first case it would save memory replication.
 *   In the second case we replicate the data anyway now.
 * - Gradient adaptation only works for full-range mapping.  Simply enforce
 *   full-range mapping with adapted gradients?
 * - Align parameter table properly (with UTF-8 string lengths).
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyrandgenset.h>
#include <libgwyddion/gwynlfit.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include <libprocess/arithmetic.h>
#include <libprocess/peaks.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwymodule/gwymodule-xyz.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>
#include "preview.h"

#define FIT_SHAPE_RUN_MODES GWY_RUN_INTERACTIVE

#define FIT_GRADIENT_NAME "__GwyFitDiffGradient"

/* Lower symmetric part indexing */
/* i MUST be greater or equal than j */
#define SLi(a, i, j) a[(i)*((i) + 1)/2 + (j)]

enum { NREDLIM = 4096 };

typedef enum {
    FIT_SHAPE_DISPLAY_DATA   = 0,
    FIT_SHAPE_DISPLAY_RESULT = 1,
    FIT_SHAPE_DISPLAY_DIFF   = 2
} FitShapeDisplayType;

typedef enum {
    FIT_SHAPE_OUTPUT_FIT  = 0,
    FIT_SHAPE_OUTPUT_DIFF = 1,
    FIT_SHAPE_OUTPUT_BOTH = 2,
} FitShapeOutputType;

typedef enum {
    FIT_SHAPE_INITIALISED      = 0,
    FIT_SHAPE_ESTIMATED        = 1,
    FIT_SHAPE_QUICK_FITTED     = 2,
    FIT_SHAPE_FITTED           = 3,
    FIT_SHAPE_USER             = 4,
    FIT_SHAPE_ESTIMATE_FAILED  = 5,
    FIT_SHAPE_QUICK_FIT_FAILED = 6,
    FIT_SHAPE_FIT_FAILED       = 7,
    FIT_SHAPE_FIT_CANCELLED    = 8,
} FitShapeState;

typedef enum {
    FIT_SHAPE_PARAM_ANGLE  = 1 << 0,
    FIT_SHAPE_PARAM_ABSVAL = 1 << 1,
} FitShapeParamFlags;

typedef struct {
    const gchar *function;
    GwyMaskingType masking;
    FitShapeDisplayType display;
    FitShapeOutputType output;
    gboolean diff_colourmap;
    gboolean diff_excluded;
} FitShapeArgs;

typedef struct {
    gboolean have_mean;
    gboolean have_circle;
    gboolean have_zrange;
    gboolean have_zstats;
    /* Plain mean values */
    gdouble xm;
    gdouble ym;
    /* Circumscribed circle. */
    gdouble xc;
    gdouble yc;
    gdouble r;
    /* Value range. */
    gdouble zmin;
    gdouble zmax;
    /* Simple value stats. */
    gdouble zmean;
    gdouble zrms;
    gdouble zskew;
} FitShapeEstimateCache;

typedef gdouble (*FitShapeXYFunc)(gdouble x, gdouble y,
                                  const gdouble *param);

typedef gboolean (*FitShapeEstimate)(const GwyXYZ *xyz,
                                     guint n,
                                     gdouble *param,
                                     FitShapeEstimateCache *estimcache);

typedef gdouble (*FitShapeCalcParam)(const gdouble *param);
typedef gdouble (*FitShapeCalcError)(const gdouble *param,
                                     const gdouble *param_err,
                                     const gdouble *correl);

typedef struct {
    const char *name;
    gint power_xy;
    gint power_z;
    FitShapeParamFlags flags;
} FitShapeParam;

typedef struct {
    const char *name;
    gint power_xy;
    gint power_z;
    FitShapeParamFlags flags;
    FitShapeCalcParam calc;
    FitShapeCalcError calc_err;
} FitShapeSecondary;

typedef struct {
    const gchar *name;
    gboolean needs_same_units;
    FitShapeXYFunc function;
    GwyNLFitIdxFunc fit_function;
    FitShapeEstimate estimate;
    FitShapeEstimate initialise;
    guint nparams;
    guint nsecondary;
    const FitShapeParam *param;
    const FitShapeSecondary *secondary;
} FitShapeFunc;

typedef struct {
    guint nparam;
    gboolean *param_fixed;

    GwySurface *surface;     /* Either ref(surface), or created from image. */
    guint n;
    const GwyXYZ *xyz;       /* Data */
    gdouble *f;              /* Function values. */
} FitShapeContext;

typedef struct {
    GtkWidget *fix;          /* Unused for secondary */
    GtkWidget *name;
    GtkWidget *equals;
    GtkWidget *value;
    GtkWidget *value_unit;
    GtkWidget *pm;
    GtkWidget *error;
    GtkWidget *error_unit;
    gdouble magnitude;       /* Unused for secondary */
} FitParamControl;

typedef struct {
    FitShapeArgs *args;
    /* These are actually non-GUI and could be separated for some
     * non-interactive use. */
    FitShapeContext *ctx;
    FitShapeEstimateCache *estimcache;
    FitShapeState state;
    GwyAppPage pageno;
    gint id;
    gchar *title;
    guint function_id;
    gdouble *param;
    gdouble *alt_param;
    gdouble *param_err;
    gdouble *correl;
    gdouble *secondary;
    gdouble *secondary_err;
    gdouble rss;
    /* This is GUI but we use the fields in mydata. */
    GwyContainer *mydata;
    GwyGradient *diff_gradient;
    GtkWidget *dialogue;
    GtkWidget *view;
    GtkWidget *function;
    GtkWidget *diff_colourmap;
    GtkWidget *diff_excluded;
    GtkWidget *output;
    GSList *display;
    GSList *masking;
    GtkWidget *rss_label;
    GtkWidget *fit_message;
    GtkWidget *revert;
    GtkWidget *recalculate;
    GtkWidget *param_table;
    GtkWidget *correl_table;
    GArray *param_controls;
    GPtrArray *correl_values;
    GPtrArray *correl_hlabels;
    GPtrArray *correl_vlabels;
    GtkWidget *secondary_table;
    GArray *secondary_controls;
} FitShapeControls;

static gboolean      module_register             (void);
static void          fit_shape                   (GwyContainer *data,
                                                  GwyRunType run);
static void          fit_shape_xyz               (GwyContainer *data,
                                                  GwyRunType run);
static void          fit_shape_dialogue          (FitShapeArgs *args,
                                                  GwyContainer *data,
                                                  gint id,
                                                  GwyDataField *dfield,
                                                  GwyDataField *mfield,
                                                  GwySurface *surface);
static void          create_output_fields        (FitShapeControls *controls,
                                                  GwyContainer *data);
static void          create_output_xyz           (FitShapeControls *controls,
                                                  GwyContainer *data);
static GtkWidget*    basic_tab_new               (FitShapeControls *controls,
                                                  GwyDataField *dfield,
                                                  GwyDataField *mfield);
static gint          basic_tab_add_masking       (FitShapeControls *controls,
                                                  GtkWidget *table,
                                                  gint row);
static GtkWidget*    parameters_tab_new          (FitShapeControls *controls);
static void          fit_param_table_resize      (FitShapeControls *controls);
static GtkWidget*    correl_tab_new              (FitShapeControls *controls);
static void          fit_correl_table_resize     (FitShapeControls *controls);
static GtkWidget*    secondary_tab_new           (FitShapeControls *controls);
static void          fit_secondary_table_resize  (FitShapeControls *controls);
static GtkWidget*    function_menu_new           (const gchar *name,
                                                  GwyDataField *dfield,
                                                  FitShapeControls *controls);
static void          function_changed            (GtkComboBox *combo,
                                                  FitShapeControls *controls);
static void          display_changed             (GtkToggleButton *toggle,
                                                  FitShapeControls *controls);
static void          diff_colourmap_changed      (GtkToggleButton *toggle,
                                                  FitShapeControls *controls);
static void          diff_excluded_changed       (GtkToggleButton *toggle,
                                                  FitShapeControls *controls);
static void          output_changed              (GtkComboBox *combo,
                                                  FitShapeControls *controls);
static void          masking_changed             (GtkToggleButton *toggle,
                                                  FitShapeControls *controls);
static void          update_colourmap_key        (FitShapeControls *controls);
static void          fix_changed                 (GtkToggleButton *button,
                                                  FitShapeControls *controls);
static void          param_value_activate        (GtkEntry *entry,
                                                  FitShapeControls *controls);
static void          update_all_param_values     (FitShapeControls *controls);
static void          revert_params               (FitShapeControls *controls);
static void          calculate_secondary_params  (FitShapeControls *controls);
static void          recalculate_image           (FitShapeControls *controls);
static void          update_param_table          (FitShapeControls *controls,
                                                  const gdouble *param,
                                                  const gdouble *param_err);
static void          update_correl_table         (FitShapeControls *controls,
                                                  GwyNLFitter *fitter);
static void          update_secondary_table      (FitShapeControls *controls);
static void          fit_shape_estimate          (FitShapeControls *controls);
static void          fit_shape_reduced_fit       (FitShapeControls *controls);
static void          fit_shape_full_fit          (FitShapeControls *controls);
static void          fit_copy_correl_matrix      (FitShapeControls *controls,
                                                  GwyNLFitter *fitter);
static void          update_fields               (FitShapeControls *controls);
static void          update_diff_gradient        (FitShapeControls *controls);
static void          update_fit_state            (FitShapeControls *controls);
static void          update_fit_results          (FitShapeControls *controls,
                                                  GwyNLFitter *fitter);
static void          update_context_data         (FitShapeControls *controls);
static void          fit_context_resize_params   (FitShapeContext *ctx,
                                                  guint n_param);
static void          fit_context_free            (FitShapeContext *ctx);
static GwyNLFitter*  fit                         (const FitShapeFunc *func,
                                                  const FitShapeContext *ctx,
                                                  gdouble *param,
                                                  gdouble *rss,
                                                  GwySetFractionFunc set_fraction,
                                                  GwySetMessageFunc set_message);
static GwyNLFitter*  fit_reduced                 (const FitShapeFunc *func,
                                                  const FitShapeContext *ctx,
                                                  gdouble *param,
                                                  gdouble *rss);
static void          calculate_field             (const FitShapeFunc *func,
                                                  const gdouble *param,
                                                  GwyDataField *dfield);
static void          calculate_function          (const FitShapeFunc *func,
                                                  const FitShapeContext *ctx,
                                                  const gdouble *param,
                                                  gdouble *z);
static void          reduce_data_size            (const GwyXYZ *xyzsrc,
                                                  guint nsrc,
                                                  GwySurface *dest);
static GString*      create_fit_report           (FitShapeControls *controls);
static void          fit_shape_load_args         (GwyContainer *container,
                                                  FitShapeArgs *args);
static void          fit_shape_save_args         (GwyContainer *container,
                                                  FitShapeArgs *args);

#define DECLARE_SECONDARY(funcname,name) \
    static gdouble funcname##_calc_##name    (const gdouble *param); \
    static gdouble funcname##_calc_err_##name(const gdouble *param, \
                                              const gdouble *param_err, \
                                              const gdouble *correl);

#define DECLARE_SHAPE_FUNC(name) \
    static gdouble name##_func(gdouble x, \
                               gdouble y, \
                               const gdouble *param); \
    static gdouble name##_fitfunc(guint i, \
                                  const gdouble *param, \
                                  gpointer user_data, \
                                  gboolean *fres) \
    { \
        const GwyXYZ *xyz = ((const FitShapeContext*)user_data)->xyz; \
        *fres = TRUE; \
        return name##_func(xyz[i].x, xyz[i].y, param) - xyz[i].z; \
    } \
    static gboolean name##_estimate(const GwyXYZ *xyz, \
                                    guint n, \
                                    gdouble *param, \
                                    FitShapeEstimateCache *estimcache); \
    static gboolean name##_init(const GwyXYZ *xyz, \
                                guint n, \
                                gdouble *param, \
                                FitShapeEstimateCache *estimcache);

/* XXX: This is a dirty trick assuming sizeof(FitShapeSecondary) > sizeof(NULL)
 * so that we get zero nsecondary when name##_secondary is defined to NULL
 * and correct array size otherwise.  It should be safe because
 * FitShapeSecondary is a struct that contains at least two pointers plus other
 * stuff, but it is dirty anyway. */
#define SHAPE_FUNC_ITEM(name) \
    &name##_func, &name##_fitfunc, &name##_estimate, &name##_init, \
    G_N_ELEMENTS(name##_params), \
    sizeof(name##_secondary)/sizeof(FitShapeSecondary), \
    name##_params, \
    name##_secondary

DECLARE_SHAPE_FUNC(grating);
DECLARE_SHAPE_FUNC(grating3);
DECLARE_SHAPE_FUNC(holes);
DECLARE_SHAPE_FUNC(pring);
DECLARE_SHAPE_FUNC(sphere);
DECLARE_SHAPE_FUNC(cylinder);
DECLARE_SHAPE_FUNC(gaussian);
DECLARE_SHAPE_FUNC(lorentzian);
DECLARE_SHAPE_FUNC(pyramidx);

DECLARE_SECONDARY(sphere, R);
DECLARE_SECONDARY(cylinder, R);
DECLARE_SECONDARY(gaussian, sigma1);
DECLARE_SECONDARY(gaussian, sigma2);
DECLARE_SECONDARY(lorentzian, b1);
DECLARE_SECONDARY(lorentzian, b2);
DECLARE_SECONDARY(grating3, h);
DECLARE_SECONDARY(grating3, L0);
DECLARE_SECONDARY(grating3, L1);
DECLARE_SECONDARY(grating3, L2);
DECLARE_SECONDARY(grating3, L3);
DECLARE_SECONDARY(holes, wouter);
DECLARE_SECONDARY(holes, winner);
DECLARE_SECONDARY(holes, R);

static const FitShapeParam grating_params[] = {
   { "L",             1, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "h",             0, 1, 0,                      },
   { "p",             0, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "z<sub>0</sub>", 0, 1, 0,                      },
   { "x<sub>0</sub>", 1, 0, 0,                      },
   { "φ",             0, 0, FIT_SHAPE_PARAM_ANGLE,  },
   { "c",             0, 0, FIT_SHAPE_PARAM_ABSVAL, },
};

#define grating_secondary NULL

static const FitShapeParam grating3_params[] = {
   { "L",             1, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "h<sub>1</sub>", 0, 1, FIT_SHAPE_PARAM_ABSVAL, },
   { "h<sub>2</sub>", 0, 1, FIT_SHAPE_PARAM_ABSVAL, },
   { "h<sub>3</sub>", 0, 1, FIT_SHAPE_PARAM_ABSVAL, },
   { "p",             0, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "q<sub>1</sub>", 0, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "q<sub>2</sub>", 0, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "q<sub>3</sub>", 0, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "z<sub>0</sub>", 0, 1, 0,                      },
   { "x<sub>0</sub>", 1, 0, 0,                      },
   { "φ",             0, 0, FIT_SHAPE_PARAM_ANGLE,  },
};

static const FitShapeSecondary grating3_secondary[] = {
   { "h",             0, 1, 0, grating3_calc_h,  grating3_calc_err_h,  },
   { "L<sub>0</sub>", 1, 0, 0, grating3_calc_L0, grating3_calc_err_L0, },
   { "L<sub>1</sub>", 1, 0, 0, grating3_calc_L1, grating3_calc_err_L1, },
   { "L<sub>2</sub>", 1, 0, 0, grating3_calc_L2, grating3_calc_err_L2, },
   { "L<sub>3</sub>", 1, 0, 0, grating3_calc_L3, grating3_calc_err_L3, },
};

static const FitShapeParam holes_params[] = {
   { "x<sub>0</sub>",  1, 0, 0,                      },
   { "y<sub>0</sub>",  1, 0, 0,                      },
   { "z<sub>0</sub>",  0, 1, 0,                      },
   { "L",              1, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "p",              0, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "h",              0, 1, 0,                      },
   { "s",              0, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "r",              0, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "φ",              0, 0, FIT_SHAPE_PARAM_ANGLE,  },
};

static const FitShapeSecondary holes_secondary[] = {
   { "w<sub>outer</sub>", 1, 0, 0, holes_calc_wouter, holes_calc_err_wouter, },
   { "w<sub>inner</sub>", 1, 0, 0, holes_calc_winner, holes_calc_err_winner, },
   { "R",                 1, 0, 0, holes_calc_R,      holes_calc_err_R,      },
};

static const FitShapeParam pring_params[] = {
   { "x<sub>0</sub>",  1, 0, 0,                      },
   { "y<sub>0</sub>",  1, 0, 0,                      },
   { "z<sub>0</sub>",  0, 1, 0,                      },
   { "R",              1, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "w",              1, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "h",              0, 1, 0,                      },
   { "s",              0, 1, 0,                      },
   { "b<sub>x</sub>", -1, 1, 0,                      },
   { "b<sub>y</sub>", -1, 1, 0,                      },
};

#define pring_secondary NULL

static const FitShapeParam sphere_params[] = {
   { "x<sub>0</sub>", 1,  0, 0, },
   { "y<sub>0</sub>", 1,  0, 0, },
   { "z<sub>0</sub>", 0,  1, 0, },
   { "C",             0, -1, 0, },
};

static const FitShapeSecondary sphere_secondary[] = {
   { "R", 0, 1, 0, sphere_calc_R, sphere_calc_err_R, },
};

static const FitShapeParam cylinder_params[] = {
   { "x<sub>0</sub>",  1,  0, 0,                     },
   { "z<sub>0</sub>",  0,  1, 0,                     },
   { "C",              0, -1, 0,                     },
   { "φ",              0,  0, FIT_SHAPE_PARAM_ANGLE, },
   { "b<sub>∥</sub>", -1,  1, 0,                     },
};

static const FitShapeSecondary cylinder_secondary[] = {
   { "R", 0, 1, 0, cylinder_calc_R, cylinder_calc_err_R, },
};

static const FitShapeParam gaussian_params[] = {
   { "x<sub>0</sub>",     1, 0, 0,                      },
   { "y<sub>0</sub>",     1, 0, 0,                      },
   { "z<sub>0</sub>",     0, 1, 0,                      },
   { "h",                 0, 1, 0,                      },
   { "σ<sub>mean</sub>",  1, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "a",                 0, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "φ",                 0, 0, FIT_SHAPE_PARAM_ANGLE,  },
   { "b<sub>x</sub>",    -1, 1, 0,                      },
   { "b<sub>y</sub>",    -1, 1, 0,                      },
};

static const FitShapeSecondary gaussian_secondary[] = {
   { "σ<sub>1</sub>", 1, 0, 0, gaussian_calc_sigma1, gaussian_calc_err_sigma1, },
   { "σ<sub>2</sub>", 1, 0, 0, gaussian_calc_sigma2, gaussian_calc_err_sigma2, },
};

static const FitShapeParam lorentzian_params[] = {
   { "x<sub>0</sub>",    1, 0, 0,                      },
   { "y<sub>0</sub>",    1, 0, 0,                      },
   { "z<sub>0</sub>",    0, 1, 0,                      },
   { "h",                0, 1, 0,                      },
   { "β<sub>mean</sub>", 1, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "a",                0, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "φ",                0, 0, FIT_SHAPE_PARAM_ANGLE,  },
   { "b<sub>x</sub>",   -1, 1, 0,                      },
   { "b<sub>y</sub>",   -1, 1, 0,                      },
};

static const FitShapeSecondary lorentzian_secondary[] = {
   { "β<sub>1</sub>", 1, 0, 0, lorentzian_calc_b1, lorentzian_calc_err_b1, },
   { "β<sub>2</sub>", 1, 0, 0, lorentzian_calc_b2, lorentzian_calc_err_b2, },
};

static const FitShapeParam pyramidx_params[] = {
   { "x<sub>0</sub>",  1, 0, 0,                      },
   { "y<sub>0</sub>",  1, 0, 0,                      },
   { "z<sub>0</sub>",  0, 1, 0,                      },
   { "h",              0, 1, 0,                      },
   { "L",              1, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "a",              0, 0, FIT_SHAPE_PARAM_ABSVAL, },
   { "α",              0, 0, FIT_SHAPE_PARAM_ANGLE,  },
   { "b<sub>x</sub>", -1, 1, 0,                      },
   { "b<sub>y</sub>", -1, 1, 0,                      },
};

#define pyramidx_secondary NULL

static const FitShapeFunc functions[] = {
    { N_("Grating (simple)"),  FALSE, SHAPE_FUNC_ITEM(grating),    },
    { N_("Grating (3-level)"), FALSE, SHAPE_FUNC_ITEM(grating3),   },
    { N_("Holes"),             FALSE, SHAPE_FUNC_ITEM(holes),      },
    { N_("Ring"),              FALSE, SHAPE_FUNC_ITEM(pring),      },
    { N_("Sphere"),            TRUE,  SHAPE_FUNC_ITEM(sphere),     },
    { N_("Cylinder (lying)"),  TRUE,  SHAPE_FUNC_ITEM(cylinder),   },
    { N_("Gaussian"),          FALSE, SHAPE_FUNC_ITEM(gaussian),   },
    { N_("Lorentzian"),        FALSE, SHAPE_FUNC_ITEM(lorentzian), },
    { N_("Pyramid (diamond)"), FALSE, SHAPE_FUNC_ITEM(pyramidx),   },
};

/* NB: The default must not require same units because then we could not fall
 * back to it in all cases. */
static const FitShapeArgs fit_shape_defaults = {
    "Grating (simple)", GWY_MASK_IGNORE,
    FIT_SHAPE_DISPLAY_RESULT, FIT_SHAPE_OUTPUT_FIT,
    TRUE, TRUE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Fits predefined geometrical shapes to data."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fit_shape",
                              (GwyProcessFunc)&fit_shape,
                              N_("/_Level/_Fit Shape..."),
                              NULL,
                              FIT_SHAPE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Fit geometrical shapes"));
    gwy_xyz_func_register("xyz_fit_shape",
                          (GwyXYZFunc)&fit_shape_xyz,
                          N_("/_Fit Shape..."),
                          NULL,
                          FIT_SHAPE_RUN_MODES,
                          GWY_MENU_FLAG_XYZ,
                          N_("Fit geometrical shapes"));

    return TRUE;
}

static void
fit_shape(GwyContainer *data, GwyRunType run)
{
    FitShapeArgs args;
    GwyDataField *dfield, *mfield;
    gint id;

    g_return_if_fail(run & FIT_SHAPE_RUN_MODES);

    fit_shape_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    fit_shape_dialogue(&args, data, id, dfield, mfield, NULL);

    fit_shape_save_args(gwy_app_settings_get(), &args);

}

static void
fit_shape_xyz(GwyContainer *data, GwyRunType run)
{
    FitShapeArgs args;
    GwySurface *surface;
    gint id;

    g_return_if_fail(run & FIT_SHAPE_RUN_MODES);

    fit_shape_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_SURFACE, &surface,
                                     GWY_APP_SURFACE_ID, &id,
                                     0);
    g_return_if_fail(surface);

    fit_shape_dialogue(&args, data, id, NULL, NULL, surface);

    fit_shape_save_args(gwy_app_settings_get(), &args);

}

static void
fit_shape_dialogue(FitShapeArgs *args,
                   GwyContainer *data, gint id,
                   GwyDataField *dfield, GwyDataField *mfield,
                   GwySurface *surface)
{
    GtkWidget *dialogue, *notebook, *widget, *vbox, *hbox, *alignment,
              *hbox2, *label;
    FitShapeControls controls;
    FitShapeEstimateCache estimcache;
    FitShapeContext ctx;
    GwyDataField *mydfield = NULL;
    GString *report;
    gint response;

    gwy_clear(&ctx, 1);
    gwy_clear(&controls, 1);
    gwy_clear(&estimcache, 1);
    controls.args = args;
    controls.ctx = &ctx;
    controls.estimcache = &estimcache;
    controls.id = id;

    if (surface) {
        controls.pageno = GWY_PAGE_XYZS;
        controls.title = gwy_app_get_surface_title(data, id);
        mydfield = gwy_data_field_new(1, 1, 1.0, 1.0, FALSE);
        gwy_preview_surface_to_datafield(surface, mydfield,
                                         PREVIEW_SIZE, PREVIEW_SIZE,
                                         GWY_PREVIEW_SURFACE_FILL);
        dfield = mydfield;
    }
    else if (dfield) {
        controls.pageno = GWY_PAGE_CHANNELS;
        controls.title = gwy_app_get_data_field_title(data, id);
    }
    else {
        g_return_if_reached();
    }

    controls.diff_gradient = gwy_inventory_new_item(gwy_gradients(),
                                                    GWY_GRADIENT_DEFAULT,
                                                    FIT_GRADIENT_NAME);
    gwy_resource_use(GWY_RESOURCE(controls.diff_gradient));

    dialogue = gtk_dialog_new_with_buttons(_("Fit Shape"), NULL, 0,
                                           GTK_STOCK_SAVE,
                                           RESPONSE_SAVE,
                                           gwy_sgettext("verb|_Fit"),
                                           RESPONSE_REFINE,
                                           gwy_sgettext("verb|_Quick Fit"),
                                           RESPONSE_CALCULATE,
                                           gwy_sgettext("verb|_Estimate"),
                                           RESPONSE_ESTIMATE,
                                           GTK_STOCK_CANCEL,
                                           GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_OK,
                                           GTK_RESPONSE_OK,
                                           NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialogue), GTK_RESPONSE_OK);
    if (controls.pageno == GWY_PAGE_XYZS)
        gwy_help_add_to_xyz_dialog(GTK_DIALOG(dialogue), GWY_HELP_DEFAULT);
    else
        gwy_help_add_to_proc_dialog(GTK_DIALOG(dialogue), GWY_HELP_DEFAULT);
    controls.dialogue = dialogue;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialogue)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    if (surface) {
        gwy_container_set_object_by_name(controls.mydata,
                                         "/surface/0", surface);
    }
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    if (mfield)
        gwy_container_set_object_by_name(controls.mydata, "/0/mask", mfield);
    dfield = gwy_data_field_duplicate(dfield);
    gwy_container_set_object_by_name(controls.mydata, "/1/data", dfield);
    g_object_unref(dfield);
    dfield = gwy_data_field_duplicate(dfield);
    gwy_container_set_object_by_name(controls.mydata, "/2/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_const_string_by_name(controls.mydata, "/2/base/palette",
                                           FIT_GRADIENT_NAME);
    if (surface) {
        GQuark quark = gwy_app_get_surface_palette_key_for_id(id);
        const guchar *gradient;
        if (gwy_container_gis_string(data, quark, &gradient)) {
            gwy_container_set_const_string_by_name(controls.mydata,
                                                   "/0/base/palette",
                                                   gradient);
        }
    }
    else {
        gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                GWY_DATA_ITEM_MASK_COLOR,
                                GWY_DATA_ITEM_RANGE_TYPE,
                                GWY_DATA_ITEM_REAL_SQUARE,
                                0);
    }
    controls.view = create_preview(controls.mydata, 0, PREVIEW_SIZE, FALSE);
    alignment = GTK_WIDGET(gtk_alignment_new(0.5, 0, 0, 0));
    gtk_container_add(GTK_CONTAINER(alignment), controls.view);
    gtk_box_pack_start(GTK_BOX(hbox), alignment, FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    widget = basic_tab_new(&controls, dfield, mfield);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
                             gtk_label_new(gwy_sgettext("adjective|Basic")));

    widget = parameters_tab_new(&controls);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
                             gtk_label_new(_("Parameters")));

    widget = correl_tab_new(&controls);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
                             gtk_label_new(_("Correlation Matrix")));

    widget = secondary_tab_new(&controls);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget,
                             gtk_label_new(_("Derived Quantities")));

    hbox2 = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    label = gtk_label_new(_("Mean square difference:"));
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 2);

    controls.rss_label = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.rss_label, FALSE, FALSE, 2);

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

    controls.fit_message = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox2), controls.fit_message, FALSE, FALSE, 2);

    update_context_data(&controls);
    function_changed(GTK_COMBO_BOX(controls.function), &controls);
    display_changed(NULL, &controls);

    gtk_widget_show_all(dialogue);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialogue));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialogue);
            case GTK_RESPONSE_NONE:
            goto finalise;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_REFINE:
            fit_shape_full_fit(&controls);
            break;

            case RESPONSE_CALCULATE:
            fit_shape_reduced_fit(&controls);
            break;

            case RESPONSE_ESTIMATE:
            fit_shape_estimate(&controls);
            break;

            case RESPONSE_SAVE:
            report = create_fit_report(&controls);
            gwy_save_auxiliary_data(_("Save Fit Report"), GTK_WINDOW(dialogue),
                                    -1, report->str);
            g_string_free(report, TRUE);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    if (controls.pageno == GWY_PAGE_XYZS)
        create_output_xyz(&controls, data);
    else
        create_output_fields(&controls, data);

    gtk_widget_destroy(dialogue);

finalise:
    gwy_resource_release(GWY_RESOURCE(controls.diff_gradient));
    gwy_inventory_delete_item(gwy_gradients(), FIT_GRADIENT_NAME);
    g_object_unref(controls.mydata);
    gwy_object_unref(mydfield);
    g_free(controls.param);
    g_free(controls.alt_param);
    g_free(controls.param_err);
    g_free(controls.secondary);
    g_free(controls.secondary_err);
    g_free(controls.correl);
    g_free(controls.title);
    g_array_free(controls.param_controls, TRUE);
    g_ptr_array_free(controls.correl_values, TRUE);
    g_ptr_array_free(controls.correl_hlabels, TRUE);
    g_ptr_array_free(controls.correl_vlabels, TRUE);
    g_array_free(controls.secondary_controls, TRUE);
    fit_context_free(controls.ctx);
}

/* NB: We reuse fields from mydata.  It is possible only because they are
 * newly created and we are going to destroy mydata anyway. */
static void
create_output_fields(FitShapeControls *controls, GwyContainer *data)
{
    FitShapeArgs *args = controls->args;
    GwyDataField *dfield;
    gint id = controls->id, newid;

    if (args->output == FIT_SHAPE_OUTPUT_FIT
        || args->output == FIT_SHAPE_OUTPUT_BOTH) {
        dfield = gwy_container_get_object_by_name(controls->mydata, "/1/data");
        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                GWY_DATA_ITEM_REAL_SQUARE,
                                GWY_DATA_ITEM_SELECTIONS,
                                0);
        gwy_app_channel_log_add_proc(data, id, newid);
        gwy_app_set_data_field_title(data, newid, _("Fitted shape"));
    }

    if (args->output == FIT_SHAPE_OUTPUT_DIFF
        || args->output == FIT_SHAPE_OUTPUT_BOTH) {
        dfield = gwy_container_get_object_by_name(controls->mydata, "/2/data");
        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                GWY_DATA_ITEM_REAL_SQUARE,
                                GWY_DATA_ITEM_SELECTIONS,
                                0);
        gwy_app_channel_log_add_proc(data, id, newid);
        gwy_app_set_data_field_title(data, newid, _("Difference"));
    }
}

static void
create_output_xyz(FitShapeControls *controls, GwyContainer *data)
{
    FitShapeArgs *args = controls->args;
    FitShapeContext *ctx = controls->ctx;
    GwySurface *surface, *newsurface;
    gint id = controls->id, newid;
    GQuark quark = gwy_app_get_surface_palette_key_for_id(id);
    const guchar *gradient = NULL;
    const GwyXYZ *sxyz;
    GwyXYZ *xyz;
    guint n, i;

    gwy_container_gis_string(data, quark, &gradient);
    surface = gwy_container_get_object_by_name(controls->mydata, "/surface/0");
    g_return_if_fail(gwy_surface_get_npoints(surface) == ctx->n);
    sxyz = gwy_surface_get_data_const(surface);
    n = ctx->n;

    if (args->output == FIT_SHAPE_OUTPUT_FIT
        || args->output == FIT_SHAPE_OUTPUT_BOTH) {
        newsurface = gwy_surface_duplicate(surface);
        xyz = gwy_surface_get_data(newsurface);
        for (i = 0; i < n; i++)
            xyz[i].z = ctx->f[i];

        newid = gwy_app_data_browser_add_surface(newsurface, data, TRUE);
        gwy_app_xyz_log_add_xyz(data, id, newid);
        gwy_app_set_surface_title(data, newid, _("Fitted shape"));
        g_object_unref(newsurface);
        if (gradient) {
            quark = gwy_app_get_surface_palette_key_for_id(newid);
            gwy_container_set_const_string(data, quark, gradient);
        }
    }

    if (args->output == FIT_SHAPE_OUTPUT_DIFF
        || args->output == FIT_SHAPE_OUTPUT_BOTH) {
        newsurface = gwy_surface_duplicate(surface);
        xyz = gwy_surface_get_data(newsurface);
        for (i = 0; i < n; i++)
            xyz[i].z = sxyz[i].z - ctx->f[i];

        newid = gwy_app_data_browser_add_surface(newsurface, data, TRUE);
        gwy_app_xyz_log_add_xyz(data, id, newid);
        gwy_app_set_surface_title(data, newid, _("Difference"));
        g_object_unref(newsurface);
        if (gradient) {
            quark = gwy_app_get_surface_palette_key_for_id(newid);
            gwy_container_set_const_string(data, quark, gradient);
        }
    }
}

static GtkWidget*
basic_tab_new(FitShapeControls *controls,
              GwyDataField *dfield, GwyDataField *mfield)
{
    static const GwyEnum displays[] = {
        { N_("Data"),         FIT_SHAPE_DISPLAY_DATA,   },
        { N_("Fitted shape"), FIT_SHAPE_DISPLAY_RESULT, },
        { N_("Difference"),   FIT_SHAPE_DISPLAY_DIFF,   },
    };
    static const GwyEnum outputs[] = {
        { N_("Fitted shape"), FIT_SHAPE_OUTPUT_FIT,  },
        { N_("Difference"),   FIT_SHAPE_OUTPUT_DIFF, },
        { N_("Both"),         FIT_SHAPE_OUTPUT_BOTH, },
    };

    GtkWidget *table, *label, *hbox;
    FitShapeArgs *args = controls->args;
    gint row;

    hbox = gtk_hbox_new(FALSE, 0);

    table = gtk_table_new(8 + 4*(!!mfield), 4, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;

    controls->function = function_menu_new(args->function, dfield, controls);
    gwy_table_attach_hscale(table, row, _("_Function type:"), NULL,
                            GTK_OBJECT(controls->function), GWY_HSCALE_WIDGET);
    row++;

    controls->output
        = gwy_enum_combo_box_new(outputs, G_N_ELEMENTS(outputs),
                                 G_CALLBACK(output_changed), controls,
                                 args->output, TRUE);
    gwy_table_attach_hscale(table, row, _("Output _type:"), NULL,
                            GTK_OBJECT(controls->output), GWY_HSCALE_WIDGET);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gtk_label_new(_("Preview:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls->display
        = gwy_radio_buttons_create(displays, G_N_ELEMENTS(displays),
                                   G_CALLBACK(display_changed),
                                   controls, args->display);
    row = gwy_radio_buttons_attach_to_table(controls->display,
                                            GTK_TABLE(table), 3, row);
    row++;

    controls->diff_colourmap
        = gtk_check_button_new_with_mnemonic(_("Show differences with "
                                               "_adapted color map"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->diff_colourmap),
                                 args->diff_colourmap);
    gtk_table_attach(GTK_TABLE(table), controls->diff_colourmap,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls->diff_colourmap, "toggled",
                     G_CALLBACK(diff_colourmap_changed), controls);
    row++;

    if (mfield)
        row = basic_tab_add_masking(controls, table, row);

    return hbox;
}

static gint
basic_tab_add_masking(FitShapeControls *controls, GtkWidget *table, gint row)
{
    GtkWidget *label;
    FitShapeArgs *args = controls->args;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gwy_label_new_header(_("Masking Mode"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls->masking
        = gwy_radio_buttons_create(gwy_masking_type_get_enum(), -1,
                                   G_CALLBACK(masking_changed),
                                   controls, args->masking);
    row = gwy_radio_buttons_attach_to_table(controls->masking,
                                            GTK_TABLE(table), 3, row);
    row++;

    controls->diff_excluded
        = gtk_check_button_new_with_mnemonic(_("Calculate differences "
                                               "for e_xcluded pixels"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->diff_excluded),
                                 args->diff_excluded);
    gtk_table_attach(GTK_TABLE(table), controls->diff_excluded,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls->diff_excluded, "toggled",
                     G_CALLBACK(diff_excluded_changed), controls);
    row++;

    return row;
}

static GtkWidget*
parameters_tab_new(FitShapeControls *controls)
{
    GtkWidget *vbox, *hbox;
    GtkSizeGroup *sizegroup;
    GtkTable *table;

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

    controls->param_table = gtk_table_new(1, 8, FALSE);
    table = GTK_TABLE(controls->param_table);
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 2);
    gtk_table_set_col_spacing(table, 0, 6);
    gtk_table_set_col_spacing(table, 4, 6);
    gtk_table_set_col_spacing(table, 5, 6);
    gtk_table_set_col_spacing(table, 7, 6);
    gtk_box_pack_start(GTK_BOX(vbox), controls->param_table, FALSE, FALSE, 0);

    gtk_table_attach(table, gwy_label_new_header(_("Fix")),
                     0, 1, 0, 1, GTK_FILL, 0, 0, 0);
    gtk_table_attach(table, gwy_label_new_header(_("Parameter")),
                     1, 5, 0, 1, GTK_FILL, 0, 0, 0);
    gtk_table_attach(table, gwy_label_new_header(_("Error")),
                     6, 8, 0, 1, GTK_FILL, 0, 0, 0);

    controls->param_controls = g_array_new(FALSE, FALSE,
                                           sizeof(FitParamControl));

    sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    controls->recalculate = gtk_button_new_with_mnemonic(_("_Recalculate "
                                                           "Image"));
    gtk_size_group_add_widget(sizegroup, controls->recalculate);
    gtk_box_pack_start(GTK_BOX(hbox), controls->recalculate, FALSE, FALSE, 0);
    g_signal_connect_swapped(controls->recalculate, "clicked",
                             G_CALLBACK(recalculate_image), controls);

    controls->revert = gtk_button_new_with_mnemonic(_("Revert to "
                                                      "_Previous Values"));
    gtk_size_group_add_widget(sizegroup, controls->revert);
    gtk_box_pack_start(GTK_BOX(hbox), controls->revert, FALSE, FALSE, 0);
    g_signal_connect_swapped(controls->revert, "clicked",
                             G_CALLBACK(revert_params), controls);

    g_object_unref(sizegroup);

    return vbox;
}

static void
fit_param_table_resize(FitShapeControls *controls)
{
    GtkTable *table;
    const FitShapeFunc *func = functions + controls->function_id;
    guint i, row, old_nparams, nparams;

    old_nparams = controls->param_controls->len;
    nparams = func->nparams;
    gwy_debug("%u -> %u", old_nparams, nparams);
    for (i = old_nparams; i > nparams; i--) {
        FitParamControl *cntrl = &g_array_index(controls->param_controls,
                                                FitParamControl, i-1);
        gtk_widget_destroy(cntrl->fix);
        gtk_widget_destroy(cntrl->name);
        gtk_widget_destroy(cntrl->equals);
        gtk_widget_destroy(cntrl->value);
        gtk_widget_destroy(cntrl->value_unit);
        gtk_widget_destroy(cntrl->pm);
        gtk_widget_destroy(cntrl->error);
        gtk_widget_destroy(cntrl->error_unit);
        g_array_set_size(controls->param_controls, i-1);
    }

    table = GTK_TABLE(controls->param_table);
    gtk_table_resize(table, 1+nparams, 8);
    row = old_nparams + 1;

    for (i = old_nparams; i < nparams; i++) {
        FitParamControl cntrl;

        cntrl.fix = gtk_check_button_new();
        gtk_table_attach(table, cntrl.fix, 0, 1, row, row+1, 0, 0, 0, 0);
        g_object_set_data(G_OBJECT(cntrl.fix), "id", GUINT_TO_POINTER(i));
        g_signal_connect(cntrl.fix, "toggled",
                         G_CALLBACK(fix_changed), controls);

        cntrl.name = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.name), 1.0, 0.5);
        gtk_table_attach(table, cntrl.name,
                         1, 2, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.equals = gtk_label_new("=");
        gtk_table_attach(table, cntrl.equals, 2, 3, row, row+1, 0, 0, 0, 0);

        cntrl.value = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(cntrl.value), 12);
        gtk_table_attach(table, cntrl.value,
                         3, 4, row, row+1, GTK_FILL, 0, 0, 0);
        g_object_set_data(G_OBJECT(cntrl.value), "id", GUINT_TO_POINTER(i));
        g_signal_connect(cntrl.value, "activate",
                         G_CALLBACK(param_value_activate), controls);
        gwy_widget_set_activate_on_unfocus(cntrl.value, TRUE);

        cntrl.value_unit = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.value_unit), 0.0, 0.5);
        gtk_table_attach(table, cntrl.value_unit,
                         4, 5, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.pm = gtk_label_new("±");
        gtk_table_attach(table, cntrl.pm, 5, 6, row, row+1, 0, 0, 0, 0);

        cntrl.error = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.error), 1.0, 0.5);
        gtk_table_attach(table, cntrl.error,
                         6, 7, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.error_unit = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.error_unit), 0.0, 0.5);
        gtk_table_attach(table, cntrl.error_unit,
                         7, 8, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.magnitude = 1.0;
        g_array_append_val(controls->param_controls, cntrl);
        row++;
    }

    for (i = 0; i < nparams; i++) {
        FitParamControl *cntrl = &g_array_index(controls->param_controls,
                                                FitParamControl, i);
        const FitShapeParam *param = func->param + i;

        gtk_label_set_markup(GTK_LABEL(cntrl->name), param->name);
    }

    gtk_widget_show_all(controls->param_table);
}

static GtkWidget*
correl_tab_new(FitShapeControls *controls)
{
    GtkWidget *scwin;
    GtkTable *table;

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scwin), 4);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);

    controls->correl_table = gtk_table_new(1, 1, TRUE);
    table = GTK_TABLE(controls->correl_table);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scwin),
                                          controls->correl_table);

    controls->correl_values = g_ptr_array_new();
    controls->correl_hlabels = g_ptr_array_new();
    controls->correl_vlabels = g_ptr_array_new();

    return scwin;
}

static void
fit_correl_table_resize(FitShapeControls *controls)
{
    GtkTable *table;
    GtkWidget *label;
    const FitShapeFunc *func = functions + controls->function_id;
    guint i, j, nparams;
    const FitShapeParam *param;
    GPtrArray *vlabels = controls->correl_vlabels,
              *hlabels = controls->correl_hlabels,
              *values = controls->correl_values;

    nparams = func->nparams;
    gwy_debug("%u -> %u", hlabels->len, nparams);
    if (hlabels->len != nparams) {
        for (i = 0; i < hlabels->len; i++)
            gtk_widget_destroy((GtkWidget*)g_ptr_array_index(hlabels, i));
        g_ptr_array_set_size(hlabels, 0);

        for (i = 0; i < vlabels->len; i++)
            gtk_widget_destroy((GtkWidget*)g_ptr_array_index(vlabels, i));
        g_ptr_array_set_size(vlabels, 0);

        for (i = 0; i < values->len; i++)
            gtk_widget_destroy((GtkWidget*)g_ptr_array_index(values, i));
        g_ptr_array_set_size(values, 0);

        table = GTK_TABLE(controls->correl_table);
        gtk_table_resize(table, nparams+1, nparams+1);

        for (i = 0; i < nparams; i++) {
            label = gtk_label_new(NULL);
            gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
            gtk_table_attach(table, label, 0, 1, i, i+1, GTK_FILL, 0, 0, 0);
            g_ptr_array_add(vlabels, label);
        }

        for (i = 0; i < nparams; i++) {
            label = gtk_label_new(NULL);
            gtk_table_attach(table, label, i+1, i+2, nparams, nparams+1,
                             GTK_FILL, 0, 0, 0);
            g_ptr_array_add(hlabels, label);
        }

        for (i = 0; i < nparams; i++) {
            for (j = 0; j <= i; j++) {
                label = gtk_label_new(NULL);
                gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
                gtk_table_attach(table, label, j+1, j+2, i, i+1,
                                 GTK_FILL, 0, 0, 0);
                g_ptr_array_add(values, label);
            }
        }
    }

    for (i = 0; i < nparams; i++) {
        param = func->param + i;
        gtk_label_set_markup(g_ptr_array_index(vlabels, i), param->name);
        gtk_label_set_markup(g_ptr_array_index(hlabels, i), param->name);
    }

    gtk_widget_show_all(controls->correl_table);
}

static GtkWidget*
secondary_tab_new(FitShapeControls *controls)
{
    GtkTable *table;

    controls->secondary_table = gtk_table_new(1, 7, FALSE);
    table = GTK_TABLE(controls->secondary_table);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 2);
    gtk_table_set_col_spacing(table, 3, 6);
    gtk_table_set_col_spacing(table, 4, 6);
    gtk_table_set_col_spacing(table, 6, 6);

    controls->secondary_controls = g_array_new(FALSE, FALSE,
                                               sizeof(FitParamControl));
    return controls->secondary_table;
}

static void
fit_secondary_table_resize(FitShapeControls *controls)
{
    GtkTable *table;
    const FitShapeFunc *func = functions + controls->function_id;
    guint i, row, old_nsecondary, nsecondary;

    old_nsecondary = controls->secondary_controls->len;
    nsecondary = func->nsecondary;
    gwy_debug("%u -> %u", old_nsecondary, nsecondary);
    for (i = old_nsecondary; i > nsecondary; i--) {
        FitParamControl *cntrl = &g_array_index(controls->secondary_controls,
                                                FitParamControl, i-1);
        gtk_widget_destroy(cntrl->name);
        gtk_widget_destroy(cntrl->equals);
        gtk_widget_destroy(cntrl->value);
        gtk_widget_destroy(cntrl->value_unit);
        gtk_widget_destroy(cntrl->pm);
        gtk_widget_destroy(cntrl->error);
        gtk_widget_destroy(cntrl->error_unit);
        g_array_set_size(controls->secondary_controls, i-1);
    }

    table = GTK_TABLE(controls->secondary_table);
    gtk_table_resize(table, 1+nsecondary, 8);
    row = old_nsecondary + 1;

    for (i = old_nsecondary; i < nsecondary; i++) {
        FitParamControl cntrl;

        cntrl.name = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.name), 1.0, 0.5);
        gtk_table_attach(table, cntrl.name,
                         0, 1, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.equals = gtk_label_new("=");
        gtk_table_attach(table, cntrl.equals, 1, 2, row, row+1, 0, 0, 0, 0);

        cntrl.value = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.value), 1.0, 0.5);
        gtk_table_attach(table, cntrl.value,
                         2, 3, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.value_unit = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.value_unit), 0.0, 0.5);
        gtk_table_attach(table, cntrl.value_unit,
                         3, 4, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.pm = gtk_label_new("±");
        gtk_table_attach(table, cntrl.pm, 4, 5, row, row+1, 0, 0, 0, 0);

        cntrl.error = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.error), 1.0, 0.5);
        gtk_table_attach(table, cntrl.error,
                         5, 6, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.error_unit = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(cntrl.error_unit), 0.0, 0.5);
        gtk_table_attach(table, cntrl.error_unit,
                         6, 7, row, row+1, GTK_FILL, 0, 0, 0);

        cntrl.magnitude = 1.0;
        g_array_append_val(controls->secondary_controls, cntrl);
        row++;
    }

    for (i = 0; i < nsecondary; i++) {
        FitParamControl *cntrl = &g_array_index(controls->secondary_controls,
                                                FitParamControl, i);
        const FitShapeSecondary *secparam = func->secondary + i;

        gtk_label_set_markup(GTK_LABEL(cntrl->name), secparam->name);
    }

    gtk_widget_show_all(controls->secondary_table);
}

static GtkWidget*
function_menu_new(const gchar *name, GwyDataField *dfield,
                  FitShapeControls *controls)
{
    GwySIUnit *xyunit, *zunit;
    gboolean same_units;
    GArray *entries = g_array_new(FALSE, FALSE, sizeof(GwyEnum));
    GtkWidget *combo;
    GwyEnum *model;
    guint i, n;

    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    zunit = gwy_data_field_get_si_unit_z(dfield);
    same_units = gwy_si_unit_equal(xyunit, zunit);

    /* First try to find function @name. */
    controls->function_id = G_MAXUINT;
    for (i = 0; i < G_N_ELEMENTS(functions); i++) {
        GwyEnum entry;

        if (functions[i].needs_same_units && !same_units)
            continue;

        entry.name = functions[i].name;
        entry.value = i;
        g_array_append_val(entries, entry);
        if (gwy_strequal(entry.name, name))
            controls->function_id = i;
    }

    /* If it was excluded fall back to the default function. */
    if (controls->function_id == G_MAXUINT) {
        name = fit_shape_defaults.function;
        for (i = 0; i < G_N_ELEMENTS(functions); i++) {
            if (functions[i].needs_same_units && !same_units)
                continue;
            if (gwy_strequal(functions[i].name, name))
                controls->function_id = i;
        }
    }
    g_assert(controls->function_id != G_MAXUINT);

    n = entries->len;
    model = (GwyEnum*)g_array_free(entries, FALSE);
    combo = gwy_enum_combo_box_new(model, n,
                                   G_CALLBACK(function_changed), controls,
                                   controls->function_id, TRUE);
    g_object_set_data(G_OBJECT(combo), "model", model);
    g_object_weak_ref(G_OBJECT(combo), (GWeakNotify)g_free, model);

    return combo;
}

static void
function_changed(GtkComboBox *combo, FitShapeControls *controls)
{
    guint i = gwy_enum_combo_box_get_active(combo);
    FitShapeContext *ctx = controls->ctx;
    const FitShapeFunc *func;
    guint nparams;

    controls->function_id = i;
    func = functions + controls->function_id;
    controls->args->function = func->name;
    nparams = func->nparams;

    controls->param = g_renew(gdouble, controls->param, nparams);
    controls->alt_param = g_renew(gdouble, controls->alt_param, nparams);
    controls->param_err = g_renew(gdouble, controls->param_err, nparams);
    controls->secondary = g_renew(gdouble, controls->secondary,
                                  func->nsecondary);
    controls->secondary_err = g_renew(gdouble, controls->secondary_err,
                                      func->nsecondary);
    controls->correl = g_renew(gdouble, controls->correl,
                               (nparams + 1)*nparams/2);
    for (i = 0; i < nparams; i++)
        controls->param_err[i] = -1.0;
    fit_param_table_resize(controls);
    fit_correl_table_resize(controls);
    fit_secondary_table_resize(controls);
    fit_context_resize_params(ctx, nparams);
    func->initialise(ctx->xyz, ctx->n, controls->param,
                     controls->estimcache);
    controls->state = FIT_SHAPE_INITIALISED;
    fit_copy_correl_matrix(controls, NULL);
    memcpy(controls->alt_param, controls->param, nparams*sizeof(gdouble));
    calculate_secondary_params(controls);
    update_param_table(controls, controls->param, NULL);
    update_correl_table(controls, NULL);
    update_fit_results(controls, NULL);
    update_fields(controls);
    update_fit_state(controls);
}

static void
display_changed(GtkToggleButton *toggle, FitShapeControls *controls)
{
    GwyPixmapLayer *player;
    GQuark quark;

    if (toggle && !gtk_toggle_button_get_active(toggle))
        return;

    controls->args->display = gwy_radio_buttons_get_current(controls->display);
    player = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));
    quark = gwy_app_get_data_key_for_id(controls->args->display);
    gwy_pixmap_layer_set_data_key(player, g_quark_to_string(quark));
    update_colourmap_key(controls);
}

static void
diff_colourmap_changed(GtkToggleButton *toggle,
                       FitShapeControls *controls)
{
    controls->args->diff_colourmap = gtk_toggle_button_get_active(toggle);
    update_colourmap_key(controls);
}

static void
diff_excluded_changed(GtkToggleButton *toggle,
                      FitShapeControls *controls)
{
    controls->args->diff_excluded = gtk_toggle_button_get_active(toggle);
    if (controls->args->masking != GWY_MASK_IGNORE)
        update_fields(controls);
}

static void
output_changed(GtkComboBox *combo, FitShapeControls *controls)
{
    controls->args->output = gwy_enum_combo_box_get_active(combo);
}

static void
masking_changed(GtkToggleButton *toggle, FitShapeControls *controls)
{
    if (!gtk_toggle_button_get_active(toggle))
        return;

    controls->args->masking = gwy_radio_buttons_get_current(controls->masking);
    update_context_data(controls);
    controls->state = FIT_SHAPE_INITIALISED;
    update_fit_results(controls, NULL);
    if (!controls->args->diff_excluded)
        update_fields(controls);
    update_fit_state(controls);
}

static void
update_colourmap_key(FitShapeControls *controls)
{
    GwyPixmapLayer *player;

    player = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));
    if (controls->args->diff_colourmap
        && controls->args->display == FIT_SHAPE_DISPLAY_DIFF) {
        gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(player),
                                         "/2/base/palette");
    }
    else {
        gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(player),
                                         "/0/base/palette");
    }
}

static void
fix_changed(GtkToggleButton *button, FitShapeControls *controls)
{
    gboolean fixed = gtk_toggle_button_get_active(button);
    guint i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "id"));
    const FitShapeContext *ctx = controls->ctx;

    ctx->param_fixed[i] = fixed;
}

static void
update_param_value(FitShapeControls *controls, guint i)
{
    const FitShapeFunc *func = functions + controls->function_id;
    FitParamControl *cntrl = &g_array_index(controls->param_controls,
                                            FitParamControl, i);
    GtkEntry *entry = GTK_ENTRY(cntrl->value);

    controls->param[i] = g_strtod(gtk_entry_get_text(entry), NULL);
    controls->param[i] *= cntrl->magnitude;
    if (func->param[i].flags & FIT_SHAPE_PARAM_ANGLE)
        controls->param[i] *= G_PI/180.0;
    if (func->param[i].flags & FIT_SHAPE_PARAM_ABSVAL)
        controls->param[i] = fabs(controls->param[i]);
}

static void
param_value_activate(GtkEntry *entry, FitShapeControls *controls)
{
    guint i = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(entry), "id"));

    update_param_value(controls, i);
    /* This (a) clears error labels in the table (b) reformats the parameter,
     * e.g. by moving the power-of-10 base appropriately. */
    controls->state = FIT_SHAPE_USER;
    calculate_secondary_params(controls);
    update_param_table(controls, controls->param, NULL);
    update_correl_table(controls, NULL);
    update_secondary_table(controls);
    update_fit_state(controls);
}

static void
update_all_param_values(FitShapeControls *controls)
{
    guint i;
    for (i = 0; i < controls->param_controls->len; i++)
        update_param_value(controls, i);
}

static void
revert_params(FitShapeControls *controls)
{
    const FitShapeFunc *func = functions + controls->function_id;
    guint i, nparams = func->nparams;

    update_all_param_values(controls);
    for (i = 0; i < nparams; i++)
        GWY_SWAP(gdouble, controls->param[i], controls->alt_param[i]);

    controls->state = FIT_SHAPE_USER;
    calculate_secondary_params(controls);
    update_param_table(controls, controls->param, NULL);
    update_correl_table(controls, NULL);
    update_secondary_table(controls);
    update_fit_state(controls);
}

static void
recalculate_image(FitShapeControls *controls)
{
    controls->state = FIT_SHAPE_USER;
    update_all_param_values(controls);
    update_fit_results(controls, NULL);
    update_fields(controls);
    update_fit_state(controls);
}

static void
update_param_table(FitShapeControls *controls,
                   const gdouble *param, const gdouble *param_err)
{
    const FitShapeFunc *func = functions + controls->function_id;
    guint i, nparams = func->nparams;
    GwyDataField *dfield;
    GwySIUnit *unit, *xyunit, *zunit;
    GwySIValueFormat *vf = NULL;

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    zunit = gwy_data_field_get_si_unit_z(dfield);
    unit = gwy_si_unit_new(NULL);

    for (i = 0; i < nparams; i++) {
        FitParamControl *cntrl = &g_array_index(controls->param_controls,
                                                FitParamControl, i);
        const FitShapeParam *fitparam = func->param + i;
        guchar buf[32];
        gdouble v;

        v = param[i];
        if (fitparam->flags & FIT_SHAPE_PARAM_ANGLE) {
            v *= 180.0/G_PI;
            gwy_si_unit_set_from_string(unit, "deg");
        }
        else {
            gwy_si_unit_power_multiply(xyunit, fitparam->power_xy,
                                       zunit, fitparam->power_z,
                                       unit);
        }
        /* If the user enters exact zero, do not update the magnitude because
         * that means an unexpected reset to base units. */
        if (G_UNLIKELY(v == 0.0)) {
            gint power10 = GWY_ROUND(log10(cntrl->magnitude));
            vf = gwy_si_unit_get_format_for_power10(unit,
                                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                    power10, vf);
        }
        else {
            vf = gwy_si_unit_get_format(unit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                        v, vf);
        }
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision+3, v/vf->magnitude);
        gtk_entry_set_text(GTK_ENTRY(cntrl->value), buf);
        gtk_label_set_markup(GTK_LABEL(cntrl->value_unit), vf->units);
        cntrl->magnitude = vf->magnitude;

        if (!param_err) {
            gtk_label_set_text(GTK_LABEL(cntrl->error), "");
            gtk_label_set_text(GTK_LABEL(cntrl->error_unit), "");
            continue;
        }

        v = param_err[i];
        if (fitparam->flags & FIT_SHAPE_PARAM_ANGLE)
            v *= 180.0/G_PI;
        vf = gwy_si_unit_get_format(unit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, v/vf->magnitude);
        gtk_label_set_text(GTK_LABEL(cntrl->error), buf);
        gtk_label_set_markup(GTK_LABEL(cntrl->error_unit), vf->units);
    }

    gwy_si_unit_value_format_free(vf);
    g_object_unref(unit);
}

static void
update_correl_table(FitShapeControls *controls, GwyNLFitter *fitter)
{
    GdkColor gdkcolor_bad = { 0, 51118, 0, 0 };
    GdkColor gdkcolor_warning = { 0, 45056, 20480, 0 };

    const FitShapeFunc *func = functions + controls->function_id;
    guint i, j, nparams = func->nparams;
    GPtrArray *values = controls->correl_values;
    const gboolean *param_fixed = controls->ctx->param_fixed;

    g_assert(values->len == (nparams + 1)*nparams/2);
    gwy_debug("fitter %p", fitter);

    for (i = 0; i < nparams; i++) {
        for (j = 0; j <= i; j++) {
            GtkWidget *label = g_ptr_array_index(values, i*(i + 1)/2 + j);

            if (fitter) {
                gchar buf[16];
                gdouble c = SLi(controls->correl, i, j);

                if (param_fixed[i] || param_fixed[j]) {
                    if (i == j) {
                        g_snprintf(buf, sizeof(buf), "%.3f", 1.0);
                        gtk_label_set_text(GTK_LABEL(label), buf);
                    }
                    else
                        gtk_label_set_text(GTK_LABEL(label), "—");
                    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, NULL);
                    continue;
                }

                g_snprintf(buf, sizeof(buf), "%.3f", c);
                gtk_label_set_text(GTK_LABEL(label), buf);
                if (i != j) {
                    if (fabs(c) >= 0.99)
                        gtk_widget_modify_fg(label, GTK_STATE_NORMAL,
                                             &gdkcolor_bad);
                    else if (fabs(c) >= 0.9)
                        gtk_widget_modify_fg(label, GTK_STATE_NORMAL,
                                             &gdkcolor_warning);
                    else
                        gtk_widget_modify_fg(label, GTK_STATE_NORMAL, NULL);
                }
            }
            else
                gtk_label_set_text(GTK_LABEL(label), "");

        }
    }

    /* For some reason, this does not happen automatically after the set-text
     * call so the labels that had initially zero width remain invisible even
     * though there is a number to display now. */
    if (fitter)
        gtk_widget_queue_resize(controls->correl_table);
}

static void
update_secondary_table(FitShapeControls *controls)
{
    const FitShapeFunc *func = functions + controls->function_id;
    guint i, nsecondary = func->nsecondary;
    GwyDataField *dfield;
    GwySIUnit *unit, *xyunit, *zunit;
    GwySIValueFormat *vf = NULL;
    gboolean is_fitted = (controls->state == FIT_SHAPE_FITTED
                          || controls->state == FIT_SHAPE_QUICK_FITTED);

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    xyunit = gwy_data_field_get_si_unit_xy(dfield);
    zunit = gwy_data_field_get_si_unit_z(dfield);
    unit = gwy_si_unit_new(NULL);

    for (i = 0; i < nsecondary; i++) {
        FitParamControl *cntrl = &g_array_index(controls->secondary_controls,
                                                FitParamControl, i);
        const FitShapeSecondary *secparam = func->secondary + i;
        guchar buf[32];
        gdouble v;

        v = controls->secondary[i];
        if (secparam->flags & FIT_SHAPE_PARAM_ANGLE) {
            v *= 180.0/G_PI;
            gwy_si_unit_set_from_string(unit, "deg");
        }
        else {
            gwy_si_unit_power_multiply(xyunit, secparam->power_xy,
                                       zunit, secparam->power_z,
                                       unit);
        }
        vf = gwy_si_unit_get_format(unit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision+3, v/vf->magnitude);
        gtk_label_set_text(GTK_LABEL(cntrl->value), buf);
        gtk_label_set_markup(GTK_LABEL(cntrl->value_unit), vf->units);

        if (!is_fitted) {
            gtk_label_set_text(GTK_LABEL(cntrl->error), "");
            gtk_label_set_text(GTK_LABEL(cntrl->error_unit), "");
            continue;
        }

        v = controls->secondary_err[i];
        if (secparam->flags & FIT_SHAPE_PARAM_ANGLE)
            v *= 180.0/G_PI;
        vf = gwy_si_unit_get_format(unit, GWY_SI_UNIT_FORMAT_VFMARKUP, v, vf);
        g_snprintf(buf, sizeof(buf), "%.*f", vf->precision, v/vf->magnitude);
        gtk_label_set_text(GTK_LABEL(cntrl->error), buf);
        gtk_label_set_markup(GTK_LABEL(cntrl->error_unit), vf->units);
    }

    if (vf)
        gwy_si_unit_value_format_free(vf);

    g_object_unref(unit);
}

static void
fit_shape_estimate(FitShapeControls *controls)
{
    const FitShapeFunc *func = functions + controls->function_id;
    const FitShapeContext *ctx = controls->ctx;
    guint i, nparams = func->nparams;

    gwy_app_wait_cursor_start(GTK_WINDOW(controls->dialogue));
    gwy_debug("start estimate");
    memcpy(controls->alt_param, controls->param, nparams*sizeof(gdouble));
    if (func->estimate(ctx->xyz, ctx->n, controls->param,
                       controls->estimcache))
        controls->state = FIT_SHAPE_ESTIMATED;
    else
        controls->state = FIT_SHAPE_ESTIMATE_FAILED;

    /* XXX: We honour fixed parameters by reverting to previous values and
     * pretending nothing happened.  Is it OK? */
    for (i = 0; i < nparams; i++) {
        gwy_debug("[%u] %g", i, controls->param[i]);
        if (ctx->param_fixed[i])
            controls->param[i] = controls->alt_param[i];
    }
    update_fit_results(controls, NULL);
    update_fields(controls);
    update_fit_state(controls);
    gwy_app_wait_cursor_finish(GTK_WINDOW(controls->dialogue));
}

static void
fit_shape_reduced_fit(FitShapeControls *controls)
{
    const FitShapeFunc *func = functions + controls->function_id;
    const FitShapeContext *ctx = controls->ctx;
    GwyNLFitter *fitter;
    gdouble rss;

    gwy_app_wait_cursor_start(GTK_WINDOW(controls->dialogue));
    gwy_debug("start reduced fit");
    update_all_param_values(controls);
    memcpy(controls->alt_param, controls->param, func->nparams*sizeof(gdouble));
    fitter = fit_reduced(func, ctx, controls->param, &rss);
    if (rss >= 0.0)
        controls->state = FIT_SHAPE_QUICK_FITTED;
    else
        controls->state = FIT_SHAPE_QUICK_FIT_FAILED;

#ifdef DEBUG
    {
        guint i;
        for (i = 0; i < func->nparams; i++)
            gwy_debug("[%u] %g", i, controls->param[i]);
    }
#endif
    fit_copy_correl_matrix(controls, fitter);
    update_fit_results(controls, fitter);
    update_fields(controls);
    update_fit_state(controls);
    gwy_math_nlfit_free(fitter);
    gwy_app_wait_cursor_finish(GTK_WINDOW(controls->dialogue));
}

static void
fit_shape_full_fit(FitShapeControls *controls)
{
    const FitShapeFunc *func = functions + controls->function_id;
    const FitShapeContext *ctx = controls->ctx;
    GwyNLFitter *fitter;
    gdouble rss;

    gwy_app_wait_start(GTK_WINDOW(controls->dialogue), _("Fitting..."));
    gwy_debug("start fit");
    update_all_param_values(controls);
    memcpy(controls->alt_param, controls->param, func->nparams*sizeof(gdouble));
    fitter = fit(func, ctx, controls->param, &rss,
                 gwy_app_wait_set_fraction, gwy_app_wait_set_message);

    if (rss >= 0.0)
        controls->state = FIT_SHAPE_FITTED;
    else if (rss == -2.0)
        controls->state = FIT_SHAPE_FIT_CANCELLED;
    else
        controls->state = FIT_SHAPE_FIT_FAILED;

#ifdef DEBUG
    {
        guint i;
        for (i = 0; i < func->nparams; i++)
            gwy_debug("[%u] %g", i, controls->param[i]);
    }
#endif
    fit_copy_correl_matrix(controls, fitter);
    update_fit_results(controls, fitter);
    update_fields(controls);
    update_fit_state(controls);
    gwy_math_nlfit_free(fitter);
    gwy_app_wait_finish();
}

static void
fit_copy_correl_matrix(FitShapeControls *controls, GwyNLFitter *fitter)
{
    const FitShapeFunc *func = functions + controls->function_id;
    guint i, j, nparams = func->nparams;
    gboolean is_fitted = (controls->state == FIT_SHAPE_FITTED
                          || controls->state == FIT_SHAPE_QUICK_FITTED);

    gwy_clear(controls->correl, (nparams + 1)*nparams/2);

    if (is_fitted) {
        g_return_if_fail(fitter && fitter->covar);

        for (i = 0; i < nparams; i++) {
            for (j = 0; j <= i; j++) {
                SLi(controls->correl, i, j)
                    = gwy_math_nlfit_get_correlations(fitter, i, j);
            }
        }
    }
}

static void
calculate_secondary_params(FitShapeControls *controls)
{
    const FitShapeFunc *func = functions + controls->function_id;
    guint i, nsecondary = func->nsecondary;
    gboolean is_fitted = (controls->state == FIT_SHAPE_FITTED
                          || controls->state == FIT_SHAPE_QUICK_FITTED);

    for (i = 0; i < nsecondary; i++) {
        const FitShapeSecondary *secparam = func->secondary + i;

        g_return_if_fail(secparam->calc);
        controls->secondary[i] = secparam->calc(controls->param);

        if (is_fitted) {
            g_return_if_fail(secparam->calc_err);
            controls->secondary_err[i] = secparam->calc_err(controls->param,
                                                            controls->param_err,
                                                            controls->correl);
        }
        else
            controls->secondary_err[i] = 0.0;

        gwy_debug("[%u] %g +- %g",
                  i, controls->secondary[i], controls->secondary_err[i]);
    }
}

static void
update_fields(FitShapeControls *controls)
{
    GwyDataField *dfield, *resfield, *difffield, *mask = NULL;
    GwySurface *surface;
    GwyMaskingType masking = controls->args->masking;
    FitShapeContext *ctx = controls->ctx;
    guint xres, yres, n, k;
    const gdouble *m;
    GwyXYZ *xyz;
    gdouble *d;

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    resfield = gwy_container_get_object_by_name(controls->mydata, "/1/data");
    difffield = gwy_container_get_object_by_name(controls->mydata, "/2/data");

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    n = xres*yres;
    gwy_container_gis_object_by_name(controls->mydata, "/0/mask",
                                     (GObject**)&mask);
    if (controls->pageno == GWY_PAGE_CHANNELS
        && (!mask || masking == GWY_MASK_IGNORE)) {
        /* We know ctx->f contains all the theoretical values. */
        g_assert(ctx->n == n);
        gwy_debug("directly copying f[] to result field");
        memcpy(gwy_data_field_get_data(resfield), ctx->f, n*sizeof(gdouble));
    }
    else if (controls->pageno == GWY_PAGE_XYZS) {
        surface = gwy_container_get_object_by_name(controls->mydata,
                                                   "/surface/0");
        surface = gwy_surface_duplicate(surface);
        n = gwy_surface_get_npoints(surface);
        g_assert(ctx->n == n);
        xyz = gwy_surface_get_data(surface);
        for (k = 0; k < n; k++)
            xyz[k].z = ctx->f[k];
        gwy_preview_surface_to_datafield(surface, resfield,
                                         PREVIEW_SIZE, PREVIEW_SIZE,
                                         GWY_PREVIEW_SURFACE_FILL);
        g_object_unref(surface);
    }
    else {
        /* Either the input is XYZ or we are using masking.  Just recalculate
         * everything, even values that are in ctx->f. */
        gwy_debug("recalculating result field the hard way");
        calculate_field(functions + controls->function_id,
                        controls->param, resfield);
    }

    gwy_data_field_data_changed(resfield);
    gwy_data_field_subtract_fields(difffield, dfield, resfield);
    if (!controls->args->diff_excluded && mask && masking != GWY_MASK_IGNORE) {
        m = gwy_data_field_get_data_const(mask);
        d = gwy_data_field_get_data(difffield);
        if (masking == GWY_MASK_INCLUDE) {
            for (k = 0; k < n; k++) {
                if (m[k] <= 0.0)
                    d[k] = 0.0;
            }
        }
        else if (masking == GWY_MASK_EXCLUDE) {
            for (k = 0; k < n; k++) {
                if (m[k] > 0.0)
                    d[k] = 0.0;
            }
        }
    }
    gwy_data_field_data_changed(difffield);
    update_diff_gradient(controls);
}

static void
update_diff_gradient(FitShapeControls *controls)
{
    static const GwyRGBA rgba_negative = { 0.0, 0.0, 1.0, 1.0 };
    static const GwyRGBA rgba_positive = { 1.0, 0.0, 0.0, 1.0 };
    static const GwyRGBA rgba_neutral = { 1.0, 1.0, 1.0, 1.0 };

    GwyDataField *difffield;
    GwyGradient *gradient = controls->diff_gradient;
    gdouble min, max;

    difffield = gwy_container_get_object_by_name(controls->mydata, "/2/data");
    gwy_data_field_get_min_max(difffield, &min, &max);
    gwy_gradient_reset(gradient);

    /* Stretch the scale to the range when all the data are too high or too
     * low. */
    if (min >= 0.0) {
        gwy_gradient_set_point_color(gradient, 0, &rgba_neutral);
        gwy_gradient_set_point_color(gradient, 1, &rgba_positive);
    }
    else if (max <= 0.0) {
        gwy_gradient_set_point_color(gradient, 0, &rgba_negative);
        gwy_gradient_set_point_color(gradient, 1, &rgba_neutral);
    }
    else {
        /* Otherwise make zero neutral and map the two colours to both side,
         * with the same scale. */
        gdouble zero = -min/(max - min);
        GwyGradientPoint zero_pt = { zero, rgba_neutral };
        GwyRGBA rgba;
        gint pos;

        if (zero <= 0.5) {
            gwy_rgba_interpolate(&rgba_neutral, &rgba_negative,
                                 zero/(1.0 - zero), &rgba);
            gwy_gradient_set_point_color(gradient, 0, &rgba);
            gwy_gradient_set_point_color(gradient, 1, &rgba_positive);
        }
        else {
            gwy_gradient_set_point_color(gradient, 0, &rgba_negative);
            gwy_rgba_interpolate(&rgba_neutral, &rgba_positive,
                                 (1.0 - zero)/zero, &rgba);
            gwy_gradient_set_point_color(gradient, 1, &rgba);
        }
        pos = gwy_gradient_insert_point_sorted(gradient, &zero_pt);
        g_assert(pos == 1);
    }
}

static void
update_fit_state(FitShapeControls *controls)
{
    GdkColor gdkcolor = { 0, 51118, 0, 0 };
    const gchar *message = "";

    if (controls->state == FIT_SHAPE_ESTIMATE_FAILED)
        message = _("Parameter estimation failed");
    else if (controls->state == FIT_SHAPE_FIT_FAILED
             || controls->state == FIT_SHAPE_QUICK_FIT_FAILED)
        message = _("Fit failed");
    else if (controls->state == FIT_SHAPE_FIT_CANCELLED)
        message = _("Fit was interruped");

    gtk_widget_modify_fg(controls->fit_message, GTK_STATE_NORMAL, &gdkcolor);
    gtk_label_set_text(GTK_LABEL(controls->fit_message), message);

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialogue),
                                      RESPONSE_SAVE,
                                      controls->state == FIT_SHAPE_FITTED);
}

static void
update_fit_results(FitShapeControls *controls, GwyNLFitter *fitter)
{
    const FitShapeFunc *func = functions + controls->function_id;
    const FitShapeContext *ctx = controls->ctx;
    GwyDataField *dfield;
    gdouble rss = 0.0;
    guint k, n = ctx->n, i, nparams = func->nparams;
    GwySIUnit *zunit;
    GwySIValueFormat *vf;
    gboolean is_fitted = (controls->state == FIT_SHAPE_FITTED
                          || controls->state == FIT_SHAPE_QUICK_FITTED);
    const GwyXYZ *xyz = ctx->xyz;
    guchar buf[48];

    if (is_fitted)
        g_return_if_fail(fitter);

    for (k = 0; k < n; k++) {
        gdouble z = func->function(xyz[k].x, xyz[k].y, controls->param);
        ctx->f[k] = z;
        z -= xyz[k].z;
        rss += z*z;
    }
    controls->rss = sqrt(rss/n);

    if (is_fitted) {
        for (i = 0; i < nparams; i++)
            controls->param_err[i] = gwy_math_nlfit_get_sigma(fitter, i);
    }

    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    zunit = gwy_data_field_get_si_unit_z(dfield);
    vf = gwy_si_unit_get_format(zunit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                controls->rss, NULL);

    g_snprintf(buf, sizeof(buf), "%.*f%s%s",
               vf->precision+1, controls->rss/vf->magnitude,
               *vf->units ? " " : "", vf->units);
    gtk_label_set_markup(GTK_LABEL(controls->rss_label), buf);

    gwy_si_unit_value_format_free(vf);

    calculate_secondary_params(controls);
    update_param_table(controls, controls->param,
                       is_fitted ? controls->param_err : NULL);
    update_correl_table(controls, is_fitted ? fitter : NULL);
    update_secondary_table(controls);
}

static void
update_context_data(FitShapeControls *controls)
{
    GwyDataField *dfield = NULL, *mfield = NULL;
    GwySurface *surface = NULL;
    FitShapeContext *ctx = controls->ctx;

    if (controls->pageno == GWY_PAGE_XYZS) {
        surface = gwy_container_get_object_by_name(controls->mydata,
                                                   "/surface/0");
        if (!ctx->surface) {
            ctx->surface = surface;
            g_object_unref(ctx->surface);
        }
    }
    else {
        dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
        gwy_container_gis_object_by_name(controls->mydata, "/0/mask",
                                         (GObject**)&mfield);
        if (!ctx->surface)
            ctx->surface = gwy_surface_new();
        gwy_surface_set_from_data_field_mask(ctx->surface, dfield,
                                             mfield, controls->args->masking);
    }

    ctx->n = gwy_surface_get_npoints(ctx->surface);
    ctx->f = g_renew(gdouble, ctx->f, ctx->n);
    ctx->xyz = gwy_surface_get_data_const(ctx->surface);

    gwy_clear(controls->estimcache, 1);
}

static void
fit_context_resize_params(FitShapeContext *ctx,
                          guint n_param)
{
    guint i;

    ctx->nparam = n_param;
    ctx->param_fixed = g_renew(gboolean, ctx->param_fixed, n_param);
    for (i = 0; i < n_param; i++) {
        ctx->param_fixed[i] = FALSE;
    }
}

static void
fit_context_free(FitShapeContext *ctx)
{
    gwy_object_unref(ctx->surface);
    g_free(ctx->param_fixed);
    g_free(ctx->f);
    gwy_clear(ctx, 1);
}

static GwyNLFitter*
fit(const FitShapeFunc *func, const FitShapeContext *ctx,
    gdouble *param, gdouble *rss,
    GwySetFractionFunc set_fraction, GwySetMessageFunc set_message)
{
    GwyNLFitter *fitter;
    guint i;

    fitter = gwy_math_nlfit_new_idx(func->fit_function, NULL);
    if (set_fraction || set_message)
        gwy_math_nlfit_set_callbacks(fitter, set_fraction, set_message);

    *rss = gwy_math_nlfit_fit_idx_full(fitter, ctx->n, func->nparams,
                                       param, ctx->param_fixed, NULL,
                                       (gpointer)ctx);
    gwy_debug("rss from nlfit %g", *rss);

    for (i = 0; i < func->nparams; i++) {
        if (func->param[i].flags & FIT_SHAPE_PARAM_ANGLE)
            param[i] = fmod(param[i], 2.0*G_PI);
        if (func->param[i].flags & FIT_SHAPE_PARAM_ABSVAL)
            param[i] = fabs(param[i]);
    }

    return fitter;
}

static GwyNLFitter*
fit_reduced(const FitShapeFunc *func, const FitShapeContext *ctx,
            gdouble *param, gdouble *rss)
{
    GwyNLFitter *fitter;
    FitShapeContext ctxred;
    guint nred = (guint)sqrt(ctx->n*(gdouble)NREDLIM);

    if (nred >= ctx->n)
        return fit(func, ctx, param, rss, NULL, NULL);

    ctxred = *ctx;
    ctxred.n = nred;
    ctxred.surface = gwy_surface_new_sized(nred);
    ctxred.xyz = gwy_surface_get_data_const(ctxred.surface);
    reduce_data_size(gwy_surface_get_data_const(ctx->surface), ctx->n,
                     ctxred.surface);
    fitter = fit(func, &ctxred, param, rss, NULL, NULL);
    g_object_unref(ctxred.surface);

    return fitter;
}

static void
calculate_field(const FitShapeFunc *func,
                const gdouble *param,
                GwyDataField *dfield)
{
    FitShapeContext ctx;

    gwy_clear(&ctx, 1);
    fit_context_resize_params(&ctx, func->nparams);
    ctx.surface = gwy_surface_new();
    gwy_surface_set_from_data_field_mask(ctx.surface, dfield,
                                         NULL, GWY_MASK_IGNORE);
    ctx.n = gwy_surface_get_npoints(ctx.surface);
    ctx.f = g_renew(gdouble, ctx.f, ctx.n);
    ctx.xyz = gwy_surface_get_data_const(ctx.surface);
    calculate_function(func, &ctx, param, gwy_data_field_get_data(dfield));
    fit_context_free(&ctx);
}

static void
calculate_function(const FitShapeFunc *func,
                   const FitShapeContext *ctx,
                   const gdouble *param,
                   gdouble *z)
{
    guint k, n = ctx->n;
    const GwyXYZ *xyz = ctx->xyz;

    for (k = 0; k < n; k++)
        z[k] = func->function(xyz[k].x, xyz[k].y, param);
}

static void
reduce_data_size(const GwyXYZ *xyzsrc, guint nsrc, GwySurface *dest)
{
    GwyRandGenSet *rngset = gwy_rand_gen_set_new(1);
    guint ndest = gwy_surface_get_npoints(dest);
    guint *redindex = gwy_rand_gen_set_choose_shuffle(rngset, 0, nsrc, ndest);
    GwyXYZ *xyzdest = gwy_surface_get_data(dest);
    guint i;

    for (i = 0; i < ndest; i++)
        xyzdest[i] = xyzsrc[redindex[i]];

    g_free(redindex);
    gwy_rand_gen_set_free(rngset);
}

/**************************************************************************
 *
 * General estimator helpers and math support functions.
 *
 **************************************************************************/

#ifdef HAVE_SINCOS
#define _gwy_sincos sincos
#else
static inline void
_gwy_sincos(gdouble x, gdouble *s, gdouble *c)
{
    *s = sin(x);
    *c = cos(x);
}
#endif

/* cosh(x) - 1, safe for small arguments */
static inline gdouble
gwy_coshm1(gdouble x)
{
    gdouble x2 = x*x;
    if (x2 > 3e-5)
        return cosh(x) - 1.0;
    return x2*(0.5 + x2/24.0);
}

#define DEFINE_PHI_CACHE(phi) \
    static gdouble phi##_last = 0.0, cphi_last = 1.0, sphi_last = 0.0

#define HANDLE_PHI_CACHE(phi) \
    do { \
        if (phi == phi##_last) { \
            cphi = cphi_last; \
            sphi = sphi_last; \
        } \
        else { \
            sincos(phi, &sphi, &cphi); \
            cphi_last = cphi; \
            sphi_last = sphi; \
            phi##_last = phi; \
        } \
    } while (0)

/* Mean value of xy point cloud (not necessarily centre, that depends on
 * the density). */
static void
mean_x_y(const GwyXYZ *xyz, guint n, gdouble *pxm, gdouble *pym,
         FitShapeEstimateCache *estimcache)
{
    gdouble xm = 0.0, ym = 0.0;
    guint i;

    if (estimcache && estimcache->have_mean) {
        gwy_debug("using cache %p", estimcache);
        *pxm = estimcache->xm;
        *pym = estimcache->ym;
        return;
    }

    if (!n) {
        *pxm = *pym = 0.0;
        return;
    }

    for (i = 0; i < n; i++) {
        xm += xyz[i].x;
        ym += xyz[i].y;
    }

    *pxm = xm/n;
    *pym = ym/n;

    if (estimcache) {
        gwy_debug("filling cache %p", estimcache);
        estimcache->have_mean = TRUE;
        estimcache->xm = *pxm;
        estimcache->ym = *pym;
    }
}

/* Minimum and maximum of an array of values. */
static void
range_z(const GwyXYZ *xyz, guint n, gdouble *pmin, gdouble *pmax,
        FitShapeEstimateCache *estimcache)
{
    gdouble min, max;
    guint i;

    if (estimcache && estimcache->have_zrange) {
        gwy_debug("using cache %p", estimcache);
        *pmin = estimcache->zmin;
        *pmax = estimcache->zmax;
        return;
    }

    if (!n) {
        *pmin = *pmax = 0.0;
        return;
    }

    min = max = xyz[0].z;
    for (i = 1; i < n; i++) {
        if (xyz[i].z < min)
            min = xyz[i].z;
        if (xyz[i].z > max)
            max = xyz[i].z;
    }

    *pmin = min;
    *pmax = max;

    if (estimcache) {
        gwy_debug("filling cache %p", estimcache);
        estimcache->have_zrange = TRUE;
        estimcache->zmin = *pmin;
        estimcache->zmax = *pmax;
    }
}

/* Simple stats of an array of values. */
static void
stat_z(const GwyXYZ *xyz, guint n,
       gdouble *zmean, gdouble *zrms, gdouble *zskew,
       FitShapeEstimateCache *estimcache)
{
    gdouble s = 0.0, s2 = 0.0, s3 = 0.0;
    guint i;

    if (estimcache && estimcache->have_zstats) {
        gwy_debug("using cache %p", estimcache);
        if (zmean)
            *zmean = estimcache->zmean;
        if (zrms)
            *zrms = estimcache->zrms;
        if (zskew)
            *zskew = estimcache->zskew;
        return;
    }

    if (!n) {
        if (zmean)
            *zmean = 0.0;
        if (zrms)
            *zrms = 0.0;
        if (zskew)
            *zskew = 0.0;
        return;
    }

    for (i = 0; i < n; i++)
        s += xyz[i].z;
    s /= n;

    for (i = 0; i < n; i++) {
        gdouble d = xyz[i].z - s;
        s2 += d*d;
        s3 += d*d*d;
    }

    if (s2) {
        s2 = sqrt(s2/n);
        s3 /= n*s2*s2*s2;
    }

    if (zmean)
        *zmean = s;
    if (zrms)
        *zrms = s2;
    if (zskew)
        *zskew = s3;

    if (estimcache) {
        gwy_debug("filling cache %p", estimcache);
        estimcache->have_zstats = TRUE;
        estimcache->zmean = s;
        estimcache->zrms = s2;
        estimcache->zskew = s3;
    }
}

/* Approximately cicrumscribe a set of points by finding a containing
 * octagon. */
static void
circumscribe_x_y(const GwyXYZ *xyz, guint n,
                 gdouble *pxc, gdouble *pyc, gdouble *pr,
                 FitShapeEstimateCache *estimcache)
{
    gdouble min[4], max[4], r[4];
    guint i, j;

    if (estimcache && estimcache->have_circle) {
        gwy_debug("using cache %p", estimcache);
        *pxc = estimcache->xc;
        *pyc = estimcache->yc;
        *pr = estimcache->r;
        return;
    }

    if (!n) {
        *pxc = *pyc = 0.0;
        *pr = 1.0;
        return;
    }

    for (j = 0; j < 4; j++) {
        min[j] = G_MAXDOUBLE;
        max[j] = -G_MAXDOUBLE;
    }

    for (i = 0; i < n; i++) {
        gdouble x = xyz[i].x, y = xyz[i].y;
        gdouble t[4] = { x, x+y, y, y-x };

        for (j = 0; j < 4; j++) {
            if (t[j] < min[j])
                min[j] = t[j];
            if (t[j] > max[j])
                max[j] = t[j];
        }
    }

    for (j = 0; j < 4; j++) {
        r[j] = sqrt(10.0)/3.0*(max[j] - min[j]);
        if (j % 2)
            r[j] /= G_SQRT2;
    }

    i = 0;
    for (j = 1; j < 4; j++) {
        if (r[j] > r[i])
            i = j;
    }

    *pr = 0.5*r[i];
    if (i % 2) {
        *pxc = (min[1] - min[3] + max[1] - max[3])/4.0;
        *pyc = (min[1] + min[3] + max[1] + max[3])/4.0;
    }
    else {
        *pxc = (min[0] + max[0])/2.0;
        *pyc = (min[2] + max[2])/2.0;
    }

    if (estimcache) {
        gwy_debug("filling cache %p", estimcache);
        estimcache->have_circle = TRUE;
        estimcache->xc = *pxc;
        estimcache->yc = *pyc;
        estimcache->r = *pr;
    }
}

/* Project xyz point cloud to a line rotated by angle phi anti-clockwise
 * from the horizontal line (x axis). */
static gdouble
projection_to_line(const GwyXYZ *xyz,
                   guint n,
                   gdouble phi,
                   gdouble xc, gdouble yc,
                   GwyDataLine *mean_line,
                   GwyDataLine *rms_line,
                   guint *counts)
{
    guint res = gwy_data_line_get_res(mean_line);
    gdouble *mean = gwy_data_line_get_data(mean_line);
    gdouble *rms = rms_line ? gwy_data_line_get_data(rms_line) : NULL;
    gdouble dx = gwy_data_line_get_real(mean_line)/res;
    gdouble off = gwy_data_line_get_offset(mean_line);
    gdouble c = cos(phi), s = sin(phi), total_ms = 0.0;
    guint i, total_n = 0;
    gint j;

    gwy_data_line_clear(mean_line);
    gwy_clear(counts, res);

    for (i = 0; i < n; i++) {
        gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
        x = x*c - y*s;
        j = (gint)floor((x - off)/dx);
        if (j >= 0 && j < res) {
            mean[j] += xyz[i].z;
            counts[j]++;
        }
    }

    for (j = 0; j < res; j++) {
        if (counts[j]) {
            mean[j] /= counts[j];
        }
    }

    if (!rms_line)
        return 0.0;

    gwy_data_line_clear(rms_line);

    for (i = 0; i < n; i++) {
        gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
        x = x*c - y*s;
        j = (gint)floor((x - off)/dx);
        if (j >= 0 && j < res)
            rms[j] += (xyz[i].z - mean[j])*(xyz[i].z - mean[j]);
    }

    for (j = 0; j < res; j++) {
        if (counts[j]) {
            total_ms += rms[j];
            rms[j] = sqrt(rms[j]/counts[j]);
            total_n += counts[j];
        }
    }

    return sqrt(total_ms/total_n);
}

/* Find direction along which projections capture best the shape, i.e. most
 * variance remains in the line-averaged data.  The returned angle is rotation
 * of the axis anti-clockwise with respect to the x-axis. */
static gdouble
estimate_projection_direction(const GwyXYZ *xyz, guint n,
                              FitShapeEstimateCache *estimcache)
{
    enum { NROUGH = 60, NFINE = 8 };

    GwyDataLine *mean_line, *rms_line;
    guint *counts;
    gdouble xc, yc, r, phi, alpha0, alpha_step, rms;
    gdouble best_rms = G_MAXDOUBLE, best_alpha = 0.0;
    guint iter, i, ni, res;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    res = (guint)floor(0.8*sqrt(n) + 1.0);

    mean_line = gwy_data_line_new(res, 2.0*r, FALSE);
    gwy_data_line_set_offset(mean_line, -r);
    rms_line = gwy_data_line_new_alike(mean_line, FALSE);
    counts = g_new(guint, res);

    for (iter = 0; iter < 6; iter++) {
        if (iter == 0) {
            ni = NROUGH;
            alpha_step = G_PI/ni;
            alpha0 = 0.0;
        }
        else {
            /* Choose the fine points so that we do not repeat calculation in
             * any of the rough points. */
            ni = NFINE;
            alpha0 = best_alpha - alpha_step*(NFINE - 1.0)/(NFINE + 1.0);
            alpha_step = 2.0*alpha_step/(NFINE + 1.0);
        }

        for (i = 0; i < ni; i++) {
            phi = alpha0 + i*alpha_step;
            rms = projection_to_line(xyz, n, phi, xc, yc,
                                     mean_line, rms_line, counts);
            gwy_debug("[%u] %g %g", iter, phi, rms);
            if (rms < best_rms) {
                best_rms = rms;
                best_alpha = phi;
            }
        }
    }

    g_object_unref(mean_line);
    g_object_unref(rms_line);
    g_free(counts);

    if (best_alpha > 0.5*G_PI)
        best_alpha += G_PI;

    return best_alpha;
}

/* Estimate projection direction, possibly on reduced data.  This is useful
 * when the estimator does not need reduced data for anything else. */
static gdouble
estimate_projection_direction_red(const GwyXYZ *xyz, guint n,
                                  FitShapeEstimateCache *estimcache)
{
    FitShapeEstimateCache estimcachered;
    guint nred = (guint)sqrt(n*(gdouble)NREDLIM);
    GwySurface *surface;
    gdouble phi;

    if (nred >= n)
        return estimate_projection_direction(xyz, n, estimcache);

    surface = gwy_surface_new_sized(nred);
    reduce_data_size(xyz, n, surface);

    /* Make sure caching still works for the reduced data. */
    gwy_clear(&estimcachered, 1);
    phi = estimate_projection_direction(gwy_surface_get_data_const(surface),
                                        nred, &estimcachered);

    g_object_unref(surface);

    return phi;
}

static void
data_line_shorten(GwyDataLine *dline, const guint *counts, guint threshold)
{
    guint res = gwy_data_line_get_res(dline);
    guint from = 0, to = res-1;
    gdouble off;

    while (to > from && counts[to] < threshold)
        to--;
    while (from < to && counts[from] < threshold)
        from++;

    off = (from*gwy_data_line_get_real(dline)/res
           + gwy_data_line_get_offset(dline));

    gwy_data_line_resize(dline, from, to+1);
    gwy_data_line_set_offset(dline, off);
}

/* Estimate the period of a periodic structure, knowing already the rotation.
 * The returned phase is such that if you subtract it from the rotated abscissa
 * value then the projection will have a positive peak (some kind of maximum)
 * centered around zero, whatever that means for specific grating-like
 * structures.  */
static gboolean
estimate_period_and_phase(const GwyXYZ *xyz, guint n,
                          gdouble phi, gdouble *pT, gdouble *poff,
                          FitShapeEstimateCache *estimcache)
{
    GwyDataLine *mean_line, *tmp_line;
    gdouble xc, yc, r, T, t, real, off, a_s, a_c, phi0, av, bv;
    const gdouble *mean, *tmp;
    guint *counts;
    guint res, i, ibest;
    gboolean found;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    /* Using more sqrt(n) than can make the sampling too sparse, causing noise
     * and oscillations. */
    res = (guint)floor(0.8*sqrt(n) + 1.0);

    *pT = r/4.0;
    *poff = 0.0;

    mean_line = gwy_data_line_new(res, 2.0*r, FALSE);
    gwy_data_line_set_offset(mean_line, -r);
    tmp_line = gwy_data_line_new_alike(mean_line, FALSE);
    counts = g_new(guint, res);

    projection_to_line(xyz, n, phi, xc, yc, mean_line, NULL, counts);
    data_line_shorten(mean_line, counts, 4);
    g_free(counts);

    res = gwy_data_line_get_res(mean_line);
    gwy_data_line_get_line_coeffs(mean_line, &av, &bv);
    gwy_data_line_line_level(mean_line, av, bv);
    gwy_data_line_psdf(mean_line, tmp_line,
                       GWY_WINDOWING_HANN, GWY_INTERPOLATION_LINEAR);
    tmp = gwy_data_line_get_data_const(tmp_line);

    found = FALSE;
    ibest = G_MAXUINT;
    for (i = 3; i < MIN(res/3, res-3); i++) {
        if (tmp[i] > tmp[i-2] && tmp[i] > tmp[i-1]
            && tmp[i] > tmp[i+1] && tmp[i] > tmp[i+2]) {
            if (ibest == G_MAXUINT || tmp[i] > tmp[ibest]) {
                found = TRUE;
                ibest = i;
            }
        }
    }
    if (!found)
        goto fail;

    T = *pT = 2.0*G_PI/gwy_data_line_itor(tmp_line, ibest);
    gwy_debug("found period %g", T);

    mean = gwy_data_line_get_data_const(mean_line);
    real = gwy_data_line_get_real(mean_line);
    off = gwy_data_line_get_offset(mean_line);
    a_s = a_c = 0.0;
    for (i = 0; i < res; i++) {
        t = off + real/res*(i + 0.5);
        a_s += sin(2*G_PI*t/T)*mean[i];
        a_c += cos(2*G_PI*t/T)*mean[i];
    }
    gwy_debug("a_s %g, a_c %g", a_s, a_c);

    phi0 = atan2(a_s, a_c);
    *poff = phi0*T/(2.0*G_PI) + xc*cos(phi) - yc*sin(phi);

fail:
    g_object_unref(mean_line);
    g_object_unref(tmp_line);

    return found;
}

/* For a shape that consists of a more or less flat base with some feature
 * on it, estimate the base plane (z0) and feature height (h).  The height
 * can be either positive or negative. */
static gboolean
estimate_feature_height(const GwyXYZ *xyz, guint n,
                        gdouble *pz0, gdouble *ph,
                        gdouble *px, gdouble *py,
                        FitShapeEstimateCache *estimcache)
{
    gdouble xm, ym, xc, yc, r, zmin, zmax;
    gdouble r2_large, r2_small;
    gdouble t, zbest, zmean_large = 0.0, zmean_small = 0.0;
    guint i, n_large = 0, n_small = 0;
    gboolean positive;

    if (!n) {
        *pz0 = *ph = 0.0;
        return FALSE;
    }

    range_z(xyz, n, &zmin, &zmax, estimcache);
    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    r2_large = 0.7*r*r;
    r2_small = 0.1*r*r;

    for (i = 0; i < n; i++) {
        gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
        gdouble r2 = x*x + y*y;

        if (r2 <= r2_small) {
            zmean_small += xyz[i].z;
            n_small++;
        }
        else if (r2 >= r2_large) {
            zmean_large += xyz[i].z;
            n_large++;
        }
    }

    g_assert(n_large);   /* circumscribe_x_y() should ensure this. */
    zmean_large /= n_large;

    if (n_small) {
        zmean_small /= n_small;
        positive = (zmean_small >= zmean_large);
    }
    else
        positive = (fabs(zmean_large - zmin) <= fabs(zmean_large - zmax));

    t = zmax - zmin;
    if (positive) {
        *pz0 = zmin + 0.05*t;
        *ph = 0.9*t;
    }
    else {
        *pz0 = zmax - 0.05*t;
        *ph = -0.9*t;
    }

    xm = 0.0;
    ym = 0.0;
    if (n_small) {
        if (positive) {
            zbest = -G_MAXDOUBLE;
            for (i = 0; i < n; i++) {
                gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
                gdouble r2 = x*x + y*y;

                if (r2 <= r2_small && xyz[i].z > zbest) {
                    zbest = xyz[i].z;
                    xm = x;
                    ym = y;
                }
            }
        }
        else {
            zbest = G_MAXDOUBLE;
            for (i = 0; i < n; i++) {
                gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
                gdouble r2 = x*x + y*y;

                if (r2 <= r2_small && xyz[i].z < zbest) {
                    zbest = xyz[i].z;
                    xm = x;
                    ym = y;
                }
            }
        }
    }
    *px = xc + xm;
    *py = yc + ym;

    return TRUE;
}

static gboolean
common_bump_feature_init(const GwyXYZ *xyz, guint n,
                         gdouble *xc, gdouble *yc, gdouble *z0,
                         gdouble *height, gdouble *size,
                         gdouble *a, gdouble *phi,
                         FitShapeEstimateCache *estimcache)
{
    gdouble xm, ym, r, zmin, zmax;

    circumscribe_x_y(xyz, n, &xm, &ym, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);

    *xc = xm;
    *yc = ym;
    *z0 = zmin;
    *height = zmax - zmin;
    *size = r/3.0;
    *a = 1.0;
    *phi = 0.0;

    return TRUE;
}

static gboolean
common_bump_feature_estimate(const GwyXYZ *xyz, guint n,
                             gdouble *xc, gdouble *yc, gdouble *z0,
                             gdouble *height, gdouble *size,
                             gdouble *a, gdouble *phi,
                             FitShapeEstimateCache *estimcache)
{
    gdouble xm, ym, r;

    /* Just initialise the shape parameters with some sane defaults. */
    *a = 1.0;
    *phi = 0.0;
    circumscribe_x_y(xyz, n, &xm, &ym, &r, estimcache);
    *size = r/3.0;

    return estimate_feature_height(xyz, n, z0, height, xc, yc, estimcache);
}

static gdouble
dotprod_with_correl(const gdouble *diff,
                    const gdouble *param_err,
                    const gdouble *correl,
                    guint n)
{
    guint i, j;
    gdouble s = 0.0;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (diff[j] != 0 && diff[j] != 0) {
                gdouble c_ij = (j <= i) ? SLi(correl, i, j) : SLi(correl, j, i);
                gdouble s_ij = c_ij*param_err[i]*param_err[j];
                s += s_ij*diff[i]*diff[j];
            }
        }
    }

    return sqrt(fmax(s, 0.0));
}

static gdouble
data_line_pearson_coeff(GwyDataLine *dline1, GwyDataLine *dline2)
{
    gdouble avg1 = gwy_data_line_get_avg(dline1);
    gdouble avg2 = gwy_data_line_get_avg(dline2);
    gdouble rms1 = gwy_data_line_get_rms(dline1);
    gdouble rms2 = gwy_data_line_get_rms(dline2);
    const gdouble *d1, *d2;
    gdouble c = 0.0;
    guint res, i;

    if (!rms1 || !rms2)
        return 0.0;

    res = gwy_data_line_get_res(dline1);
    g_return_val_if_fail(gwy_data_line_get_res(dline2) == res, 0.0);
    d1 = gwy_data_line_get_data_const(dline1);
    d2 = gwy_data_line_get_data_const(dline2);
    for (i = 0; i < res; i++)
        c += (d1[i] - avg1)*(d2[i] - avg2);

    c /= res*rms1*rms2;
    gwy_debug("%g", c);
    return c;
}

/**************************************************************************
 *
 * Sphere
 *
 **************************************************************************/

static gdouble
sphere_func(gdouble x, gdouble y, const gdouble *param)
{
    gdouble xc = param[0];
    gdouble yc = param[1];
    gdouble z0 = param[2];
    gdouble kappa = param[3];
    gdouble r2k, t, val;

    x -= xc;
    y -= yc;
    /* Rewrite R - sqrt(R² - r²) as κ*r²/(1 + sqrt(1 - κ²r²)) where
     * r² = x² + y² and κR = 1 to get nice behaviour in the close-to-denegerate
     * cases, including completely flat surface.  The expression 1.0/kappa
     * is safe because we cannot get to this branch for κ → 0 unless
     * simultaneously r → ∞. */
    r2k = kappa*(x*x + y*y);
    t = 1.0 - kappa*r2k;
    if (t > 0.0)
        val = z0 + r2k/(1.0 + sqrt(t));
    else
        val = z0 + 2.0/kappa;

    return val;
}

static gboolean
sphere_init(const GwyXYZ *xyz, guint n, gdouble *param,
            FitShapeEstimateCache *estimcache)
{
    gdouble xc, yc, r, zmin, zmax, zmean;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);
    stat_z(xyz, n, &zmean, NULL, NULL, estimcache);

    param[0] = xc;
    param[1] = yc;
    if (fabs(zmean - zmin) > fabs(zmean - zmax)) {
        param[2] = zmax;
        param[3] = 2.0*(zmin - zmax)/(r*r);
    }
    else {
        param[2] = zmin;
        param[3] = 2.0*(zmax - zmin)/(r*r);
    }

    return TRUE;
}

/* Fit the data with a rotationally symmetric parabola and use its parameters
 * for the spherical surface estimate. */
static gboolean
sphere_estimate(const GwyXYZ *xyz, guint n, gdouble *param,
                FitShapeEstimateCache *estimcache)
{
    gdouble xc, yc;
    /* Linear fit with functions 1, x, y and x²+y². */
    gdouble a[10], b[4];
    guint i;

    /* XXX: Handle the surrounding flat area, which can be a part of the
     * function, better? */

    /* Using centered coodinates improves the condition number. */
    mean_x_y(xyz, n, &xc, &yc, estimcache);
    gwy_clear(a, 10);
    gwy_clear(b, 4);
    for (i = 0; i < n; i++) {
        gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
        gdouble r2 = x*x + y*y;

        b[0] += xyz[i].z;
        b[1] += x*xyz[i].z;
        b[2] += y*xyz[i].z;
        b[3] += r2*xyz[i].z;

        a[2] += x*x;
        a[4] += x*y;
        a[5] += y*y;
        a[6] += r2;
        a[7] += x*r2;
        a[8] += y*r2;
        a[9] += r2*r2;
    }
    a[0] = n;
    a[1] = a[3] = 0.0;

    param[0] = xc;
    param[1] = yc;
    param[2] = b[0]/n;
    param[3] = 0.0;

    if (!gwy_math_choleski_decompose(4, a))
        return FALSE;

    gwy_math_choleski_solve(4, a, b);

    param[3] = 2.0*b[3];
    if (param[3]) {
        param[0] = xc - b[1]/param[3];
        param[1] = yc - b[2]/param[3];
        param[2] = b[0] - 0.5*(b[1]*b[1] + b[2]*b[2])/param[3];
    }

    return TRUE;
}

static gdouble
sphere_calc_R(const gdouble *param)
{
    return 1.0/param[3];
}

static gdouble
sphere_calc_err_R(const gdouble *param,
                  const gdouble *param_err,
                  G_GNUC_UNUSED const gdouble *correl)
{
    return param_err[3]/(param[3]*param[3]);
}

/**************************************************************************
 *
 * Cylinder (lying)
 *
 **************************************************************************/

/* XXX: We might want
 * - finite-size cylinders
 * - cylinders cut elsewhere than in the half
 * Unfortunately, the derivatives by the corresponding parameters are often
 * zero (or something odd) because of the sharp boundaries in the function that
 * either catch a data point inside some region or not. */
static gdouble
cylinder_func(gdouble x, gdouble y, const gdouble *param)
{
    DEFINE_PHI_CACHE(phi);

    gdouble x0 = param[0];
    gdouble z0 = param[1];
    gdouble kappa = param[2];
    gdouble phi = param[3];
    gdouble bparallel = param[4];
    gdouble r2k, t, val, cphi, sphi;

    HANDLE_PHI_CACHE(phi);
    z0 += bparallel*(x*sphi + y*cphi);
    t = x*cphi - y*sphi - x0;
    r2k = kappa*t*t;
    t = 1.0 - kappa*r2k;
    if (t > 0.0)
        val = z0 + r2k/(1.0 + sqrt(t));
    else
        val = z0 + 2.0/kappa;

    return val;
}

static gboolean
cylinder_init(const GwyXYZ *xyz, guint n, gdouble *param,
              FitShapeEstimateCache *estimcache)
{
    gdouble xc, yc, r, zmin, zmax, zmean;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);
    stat_z(xyz, n, &zmean, NULL, NULL, estimcache);

    param[0] = xc;
    if (fabs(zmean - zmin) > fabs(zmean - zmax)) {
        param[1] = zmax;
        param[2] = 2.0*(zmin - zmax)/(r*r);
    }
    else {
        param[1] = zmin;
        param[2] = 2.0*(zmax - zmin)/(r*r);
    }
    param[3] = 0.0;
    param[4] = 0.0;

    return TRUE;
}

/* Fit the data with a rotationally symmetric parabola and use its parameters
 * for the spherical surface estimate. */
static gboolean
cylinder_estimate(const GwyXYZ *xyz, guint n, gdouble *param,
                  FitShapeEstimateCache *estimcache)
{
    GwyDataLine *mean_line;
    gdouble xc, yc, r, zmin, zmax, t, phi;
    guint *counts = NULL;
    guint i, mpos, res;
    gint d2sign;
    const gdouble *d;

    /* First we estimate the orientation (phi). */
    phi = estimate_projection_direction_red(xyz, n, estimcache);

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);

    res = (guint)floor(0.8*sqrt(n) + 1.0);
    mean_line = gwy_data_line_new(res, 2.0*r, FALSE);
    counts = g_new(guint, res);
    gwy_data_line_set_offset(mean_line, -r);
    projection_to_line(xyz, n, phi, xc, yc, mean_line, NULL, counts);
    d = gwy_data_line_get_data(mean_line);

    d2sign = 0;
    for (i = 1; i < res-1; i++) {
        if (d[i] < 0.5*(d[i-1] + d[i+1]))
            d2sign += 1;
        else if (d[i] > 0.5*(d[i-1] + d[i+1]))
            d2sign -= 1;
    }
    gwy_debug("d2sign %d", d2sign);

    if (d2sign > 0)
        gwy_data_line_multiply(mean_line, -1.0);
    mpos = 0;
    for (i = 1; i < res; i++) {
        if (d[i] > d[mpos])
            mpos = i;
    }

    t = 2.0*r/res*mpos - r;
    param[0] = ((xc + t*cos(phi))*cos(phi) - (yc - t*sin(phi))*sin(phi));

    if (d2sign > 0) {
        param[1] = zmin;
        param[2] = 4.0*(zmax - zmin)/(r*r);
    }
    else {
        param[1] = zmax;
        param[2] = 4.0*(zmin - zmax)/(r*r);
    }
    param[3] = phi;
    param[4] = 0.0;

    g_free(counts);
    g_object_unref(mean_line);

    return TRUE;
}

static gdouble
cylinder_calc_R(const gdouble *param)
{
    return 1.0/param[2];
}

static gdouble
cylinder_calc_err_R(const gdouble *param,
                    const gdouble *param_err,
                    G_GNUC_UNUSED const gdouble *correl)
{
    return param_err[2]/(param[2]*param[2]);
}

/**************************************************************************
 *
 * Grating (simple)
 *
 **************************************************************************/

static gdouble
grating_func(gdouble x, gdouble y, const gdouble *param)
{
    static gdouble c_last = 0.0, coshm1_c_last = 1.0;
    DEFINE_PHI_CACHE(phi);

    gdouble L = fabs(param[0]);
    gdouble h = param[1];
    gdouble p = fabs(param[2]);
    gdouble z0 = param[3];
    gdouble x0 = param[4];
    gdouble phi = param[5];
    gdouble c = param[6];
    gdouble t, Lp2, val, coshm1_c, cphi, sphi;

    Lp2 = 0.5*L*p;
    if (G_UNLIKELY(!Lp2))
        return z0;

    HANDLE_PHI_CACHE(phi);
    t = x*cphi - y*sphi - x0 + Lp2;
    t = (t - L*floor(t/L))/Lp2 - 1.0;
    if (fabs(t) < 1.0) {
        if (c == c_last)
            coshm1_c = coshm1_c_last;
        else {
            coshm1_c = coshm1_c_last = gwy_coshm1(c);
            c_last = c;
        }

        val = z0 + h*(1.0 - gwy_coshm1(c*t)/coshm1_c);
    }
    else
        val = z0;

    return val;
}

static gboolean
grating_init(const GwyXYZ *xyz, guint n, gdouble *param,
             FitShapeEstimateCache *estimcache)
{
    gdouble xc, yc, r, zmin, zmax;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);

    param[0] = r/4.0;
    param[1] = zmax - zmin;
    param[2] = 0.5;
    param[3] = zmin;
    param[4] = 0.0;
    param[5] = 0.0;
    param[6] = 5.0;

    return TRUE;
}

static gboolean
grating_estimate(const GwyXYZ *xyz, guint n, gdouble *param,
                 FitShapeEstimateCache *estimcache)
{
    gdouble t;

    /* Just initialise the percentage and shape with some sane defaults. */
    param[2] = 0.5;
    param[6] = 5.0;

    /* Simple height parameter estimate. */
    range_z(xyz, n, param+3, param+1, estimcache);
    t = param[1] - param[3];
    param[1] = 0.9*t;
    param[3] += 0.05*t;

    /* First we estimate the orientation (phi). */
    param[5] = estimate_projection_direction_red(xyz, n, estimcache);

    /* Then we extract a representative profile with this orientation. */
    return estimate_period_and_phase(xyz, n, param[5], param + 0, param + 4,
                                     estimcache);
}

/**************************************************************************
 *
 * Grating (3-level)
 *
 **************************************************************************/

static gdouble
grating3_func(gdouble x, gdouble y, const gdouble *param)
{
    DEFINE_PHI_CACHE(phi);

    gdouble L = fabs(param[0]);
    gdouble h1 = fabs(param[1]);
    gdouble h2 = fabs(param[2]);
    gdouble h3 = fabs(param[3]);
    gdouble p = fmin(fabs(param[4]), 1.0);
    gdouble q1 = fabs(param[5]);
    gdouble q2 = fabs(param[6]);
    gdouble q3 = fabs(param[7]);
    gdouble z0 = param[8];
    gdouble x0 = param[9];
    gdouble phi = param[10];
    gdouble t, Lp2, cphi, sphi, Ll, Lu;

    Lp2 = 0.5*L*p;
    if (G_UNLIKELY(!Lp2))
        return z0;

    HANDLE_PHI_CACHE(phi);
    t = x*cphi - y*sphi - x0 + Lp2;
    t -= L*floor(t/L) + Lp2;
    t = fabs(t);

    Lu = Lp2;
    if (t >= Lu)
        return z0;

    Ll = Lu;
    Lu = Ll/(1.0 + q1);
    if (t >= Lu) {
        if (G_UNLIKELY(Lu == Ll))
            return z0;
        return z0 + h1/(Lu - Ll)*(t - Ll);
    }

    Ll = Lu;
    Lu = Ll/(1.0 + q2);
    z0 += h1;
    if (t >= Lu) {
        if (G_UNLIKELY(Lu == Ll))
            return z0;
        return z0 + h2/(Lu - Ll)*(t - Ll);
    }

    Ll = Lu;
    Lu = Ll/(1.0 + q3);
    z0 += h2;
    if (t >= Lu) {
        if (G_UNLIKELY(Lu == Ll))
            return z0;
        return z0 + h3/(Lu - Ll)*(t - Ll);
    }

    return z0 + h3;
}

static gboolean
grating3_init(const GwyXYZ *xyz, guint n, gdouble *param,
              FitShapeEstimateCache *estimcache)
{
    gdouble xc, yc, r, zmin, zmax;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);

    param[0] = r/4.0;
    param[1] = 0.1*(zmax - zmin);
    param[2] = 0.8*(zmax - zmin);
    param[3] = 0.1*(zmax - zmin);
    param[4] = 0.7;
    param[5] = 0.5;
    param[6] = 0.2;
    param[7] = 0.5;
    param[8] = zmin;
    param[9] = 0.0;
    param[10] = 0.0;

    return TRUE;
}

static gboolean
grating3_estimate(const GwyXYZ *xyz, guint n, gdouble *param,
                  FitShapeEstimateCache *estimcache)
{
    gdouble zmin, zmax;

    /* Just initialise the percentage and shape with some sane defaults. */
    param[4] = 0.7;
    param[5] = 0.5;
    param[6] = 0.2;
    param[7] = 0.5;

    /* Simple height parameter estimate. */
    range_z(xyz, n, &zmin, &zmax, estimcache);
    param[1] = 0.1*(zmax - zmin);
    param[2] = 0.8*(zmax - zmin);
    param[3] = 0.1*(zmax - zmin);
    param[8] = zmin;

    /* First we estimate the orientation (phi). */
    param[10] = estimate_projection_direction_red(xyz, n, estimcache);

    /* Then we extract a representative profile with this orientation. */
    return estimate_period_and_phase(xyz, n, param[10], param + 0, param + 9,
                                     estimcache);
}

static gdouble
grating3_calc_h(const gdouble *param)
{
    return param[1] + param[2] + param[3];
}

static gdouble
grating3_calc_err_h(G_GNUC_UNUSED const gdouble *param,
                    const gdouble *param_err,
                    const gdouble *correl)
{
    gdouble diff[G_N_ELEMENTS(grating3_params)];

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[1] = diff[2] = diff[3] = 1.0;
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

static gdouble
grating3_calc_L0(const gdouble *param)
{
    return param[0]*param[4];
}

static gdouble
grating3_calc_err_L0(const gdouble *param,
                         const gdouble *param_err,
                         const gdouble *correl)
{
    gdouble diff[G_N_ELEMENTS(grating3_params)];

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[0] = param[4];
    diff[4] = param[0];
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

static gdouble
grating3_calc_L1(const gdouble *param)
{
    return grating3_calc_L0(param)/(1.0 + param[5]);
}

static gdouble
grating3_calc_err_L1(const gdouble *param,
                     const gdouble *param_err,
                     const gdouble *correl)
{
    gdouble L1 = grating3_calc_L1(param);
    gdouble diff[G_N_ELEMENTS(grating3_params)];

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[0] = L1/param[0];
    diff[4] = L1/param[4];
    diff[5] = -L1/(1.0 + param[5]);
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

static gdouble
grating3_calc_L2(const gdouble *param)
{
    return grating3_calc_L1(param)/(1.0 + param[6]);
}

static gdouble
grating3_calc_err_L2(const gdouble *param,
                     const gdouble *param_err,
                     const gdouble *correl)
{
    gdouble L2 = grating3_calc_L2(param);
    gdouble diff[G_N_ELEMENTS(grating3_params)];

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[0] = L2/param[0];
    diff[4] = L2/param[4];
    diff[5] = -L2/(1.0 + param[5]);
    diff[6] = -L2/(1.0 + param[6]);
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

static gdouble
grating3_calc_L3(const gdouble *param)
{
    return grating3_calc_L2(param)/(1.0 + param[7]);
}

static gdouble
grating3_calc_err_L3(const gdouble *param,
                     const gdouble *param_err,
                     const gdouble *correl)
{
    gdouble L3 = grating3_calc_L3(param);
    gdouble diff[G_N_ELEMENTS(grating3_params)];

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[0] = L3/param[0];
    diff[4] = L3/param[4];
    diff[5] = -L3/(1.0 + param[5]);
    diff[6] = -L3/(1.0 + param[6]);
    diff[7] = -L3/(1.0 + param[7]);
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

/**************************************************************************
 *
 * Holes
 *
 **************************************************************************/

static inline gdouble
hole_radial_intersection(gdouble q, gdouble A, gdouble R)
{
    gdouble A1q = A*(1.0 - q);
    gdouble q21 = 1.0 + q*q;
    gdouble D = R*R*q21 - A1q*A1q;
    gdouble sqrtD = sqrt(MAX(D, 0.0));
    gdouble x = (1.0 + q)*A + sqrtD;
    return x/sqrt(q21);
}

static gdouble
hole_shape(gdouble x, gdouble y, gdouble size, gdouble slope, gdouble roundness)
{
    gdouble rx, ry, r, rr, rsz;
    gdouble v = 0.0;

    if (roundness) {
        rsz = roundness*size;
        rx = fabs(x) - (size - rsz);
        ry = fabs(y) - (size - rsz);
        r = MAX(rx, ry);
        rr = MIN(rx, ry);
        if (r <= 0.0 || (r <= rsz && rr <= 0.0) || rx*rx + ry*ry <= rsz*rsz)
            v = -1.0;
        else if (slope) {
            gdouble ss = size + slope;
            rsz = roundness*ss;
            rx = fabs(x) - (ss - rsz);
            ry = fabs(y) - (ss - rsz);
            r = MAX(rx, ry);
            rr = MIN(rx, ry);
            if (r <= 0.0 || (r <= rsz && rr <= 0.0)
                || rx*rx + ry*ry <= rsz*rsz) {
                gdouble q = (rr + ss - rsz)/(r + ss - rsz);
                if (q <= 1.0 - roundness)
                    v = (r - rsz)/slope;
                else {
                    r = hole_radial_intersection(q, ss - rsz, rsz);
                    rr = hole_radial_intersection(q, size - roundness*size,
                                                  roundness*size);
                    v = (sqrt(x*x + y*y) - r)/(r - rr);
                }
            }
        }
    }
    else {
        rx = fabs(x) - size;
        ry = fabs(y) - size;
        r = MAX(rx, ry);
        if (r <= 0.0)
            v = -1.0;
        else if (r < slope)
            v = (r - slope)/slope;

        v = sin(x);
    }

    return v;
}

static gdouble
holes_func(gdouble x, gdouble y, const gdouble *param)
{
    DEFINE_PHI_CACHE(phi);

    gdouble xc = param[0];
    gdouble yc = param[1];
    gdouble z0 = param[2];
    gdouble L = fabs(param[3]);
    gdouble p = param[4];
    gdouble h = param[5];
    gdouble s = param[6];
    gdouble r = param[7];
    gdouble phi = param[8];
    gdouble t, cphi, sphi;

    x -= xc;
    y -= yc;

    if (G_UNLIKELY(L*p == 0.0))
        return z0;

    HANDLE_PHI_CACHE(phi);
    t = x*cphi - y*sphi;
    y = x*sphi + y*cphi;
    x = t;

    x -= L*floor(x/L) + 0.5*L;
    y -= L*floor(y/L) + 0.5*L;

    p = 1.0/(1.0 + fabs(p));
    /* Map zero values to no roundness and no slope. */
    s = fabs(s)/(1.0 + fabs(s));
    r = fabs(r)/(1.0 + fabs(s));

    return z0 + h*hole_shape(fabs(x), fabs(y), (1.0 - s)*0.5*L*p, s*0.5*L*p, r);
}

static gboolean
holes_init(const GwyXYZ *xyz, guint n, gdouble *param,
           FitShapeEstimateCache *estimcache)
{
    gdouble xc, yc, r, zmin, zmax;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);

    param[0] = 0;
    param[1] = 0;
    param[2] = zmin;
    param[3] = r/4.0;
    param[4] = 1.0;
    param[5] = zmax - zmin;
    param[6] = 0.1;
    param[7] = 0.1;
    param[8] = 0.0;

    return TRUE;
}

static gboolean
holes_estimate(const GwyXYZ *xyz, guint n, gdouble *param,
               FitShapeEstimateCache *estimcache)
{
    GwyDataLine *mean_line, *rms_line;
    gdouble xc, yc, r, phi, L1, L2, zmin, zmax, u, v;
    guint *counts;
    guint res;
    gboolean ok1, ok2;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);

    param[4] = 1.0;
    param[6] = 0.1;
    param[7] = 0.1;

    param[8] = phi = estimate_projection_direction_red(xyz, n, estimcache);

    ok1 = estimate_period_and_phase(xyz, n, phi, &L1, &u, estimcache);
    ok2 = estimate_period_and_phase(xyz, n, phi - 0.5*G_PI, &L2, &v,
                                    estimcache);
    param[3] = 0.5*(L1 + L2);
    param[0] = u*cos(phi) + v*sin(phi);
    param[1] = u*sin(phi) + v*cos(phi);

    /* Estimate h sign: do projection and if smaller values correlate with
     * large rms then it is holes (h > 0); if larger values correlate with
     * large rms then it is bumps (h < 0).  */
    res = (guint)floor(0.8*sqrt(n) + 1.0);
    mean_line = gwy_data_line_new(res, 2.0*r, FALSE);
    gwy_data_line_set_offset(mean_line, -r);
    rms_line = gwy_data_line_new_alike(mean_line, FALSE);
    counts = g_new(guint, res);
    projection_to_line(xyz, n, phi, xc, yc, mean_line, rms_line, counts);
    data_line_shorten(mean_line, counts, 4);
    data_line_shorten(rms_line, counts, 4);
    g_free(counts);

#if 0
    {
        FILE *fh = fopen("mean.dat", "w");
        GwyDataLine *dl = mean_line;
        guint i_;
        for (i_ = 0; i_ < dl->res; i_++) {
            gdouble t_ = dl->off + dl->real/dl->res*(i_ + 0.5);
            fprintf(fh, "%g %g\n", t_, dl->data[i_]);
        }
        fclose(fh);
    }
#endif

    if (data_line_pearson_coeff(mean_line, rms_line) <= 0.0) {
        gwy_debug("holes");
        param[2] = zmax;
        param[5] = zmax - zmin;
    }
    else {
        gwy_debug("bumps");
        param[2] = zmin;
        param[5] = zmin - zmax;
        /* estimate_period_and_phase() finds maxima but we want minima here */
        param[0] += 0.5*param[3];
        param[1] += 0.5*param[3];
    }

    g_object_unref(rms_line);
    g_object_unref(mean_line);

    return ok1 && ok2;
}

static gdouble
holes_calc_wouter(const gdouble *param)
{
    return param[3]/(1.0 + fabs(param[4]));
}

static gdouble
holes_calc_err_wouter(const gdouble *param,
                      const gdouble *param_err,
                      const gdouble *correl)
{
    gdouble diff[G_N_ELEMENTS(holes_params)];
    gdouble wouter = holes_calc_wouter(param);

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[3] = wouter/param[3];
    diff[4] = -wouter/(1.0 + fabs(param[4]));
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

static gdouble
holes_calc_winner(const gdouble *param)
{
    return param[3]/(1.0 + fabs(param[4]))/(1.0 + fabs(param[6]));
}

static gdouble
holes_calc_err_winner(const gdouble *param,
                      const gdouble *param_err,
                      const gdouble *correl)
{
    gdouble diff[G_N_ELEMENTS(holes_params)];
    gdouble winner = holes_calc_winner(param);

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[3] = winner/param[3];
    diff[4] = -winner/(1.0 + fabs(param[4]));
    diff[6] = -winner/(1.0 + fabs(param[6]));
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

static gdouble
holes_calc_R(const gdouble *param)
{
    return param[3]/(1.0 + fabs(param[4]))/fabs(param[7])/(1.0 + fabs(param[7]));
}

static gdouble
holes_calc_err_R(const gdouble *param,
                 const gdouble *param_err,
                 const gdouble *correl)
{
    gdouble diff[G_N_ELEMENTS(holes_params)];
    gdouble R = holes_calc_R(param);
    gdouble t = 1.0 + fabs(param[7]);

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[3] = R/param[3];
    diff[4] = -R/(1.0 + fabs(param[4]));
    diff[7] = -param[3]/(1.0 + fabs(param[4]))/(t*t);
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

/**************************************************************************
 *
 * Ring
 *
 **************************************************************************/

static gdouble
pring_func(gdouble x, gdouble y, const gdouble *param)
{
    static gdouble s_h_last = 0.0, rinner_last = 1.0, router_last = 1.0;

    gdouble xc = param[0];
    gdouble yc = param[1];
    gdouble z0 = param[2];
    gdouble R = param[3];
    gdouble w = fabs(param[4]);
    gdouble h = param[5];
    gdouble s = param[6];
    gdouble bx = param[7];
    gdouble by = param[8];
    gdouble r, r2, s_h, rinner, router;

    x -= xc;
    y -= yc;
    z0 += bx*x + by*y;
    r2 = x*x + y*y;

    if (G_UNLIKELY(w == 0.0))
        return r2 <= R*R ? z0 - 0.5*s : z0 + 0.5*s;

    if (G_UNLIKELY(h == 0.0)) {
        if (r2 >= R*R)
            return z0 + 0.5*s;
        else if (r2 < (R - w)*(R - w))
            return z0 - 0.5*s;

        r = (R - sqrt(r2))/w;
        return z0 + s*(0.5 - r*r);
    }

    r = sqrt(r2) - R;
    s_h = s/h;
    if (s_h == s_h_last) {
        rinner = rinner_last;
        router = router_last;
    }
    else {
        rinner = rinner_last = sqrt(fmax(1.0 + 0.5*s_h, 0.0));
        router = router_last = sqrt(fmax(1.0 - 0.5*s_h, 0.0));
        s_h_last = s_h;
    }

    rinner *= -0.5*w;
    if (r <= rinner)
        return z0 - 0.5*s;

    router *= 0.5*w;
    if (r >= router)
        return z0 + 0.5*s;

    r *= 2.0/w;
    return z0 + h*(1.0 - r*r);
}

static gboolean
pring_init(const GwyXYZ *xyz, guint n, gdouble *param,
           FitShapeEstimateCache *estimcache)
{
    gdouble xc, yc, r, zmin, zmax;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);

    param[0] = xc;
    param[1] = yc;
    param[2] = zmin;
    param[3] = r/3.0;
    param[4] = r/12.0;
    param[5] = zmax - zmin;
    param[6] = (zmax - zmin)/12.0;
    param[7] = 0.0;
    param[8] = 0.0;

    return TRUE;
}

static gboolean
pring_estimate_projection(const GwyXYZ *xyz, guint n,
                          gdouble xc, gdouble yc, gdouble r,
                          gboolean vertical, gboolean upwards,
                          GwyDataLine *proj, GwyXY *projdata, guint *counts,
                          GwyPeaks *peaks, gdouble *param)
{
    guint i, j, res;
    const gdouble *d;
    gdouble c, real, off, width[2];

    c = (vertical ? yc : xc);
    gwy_data_line_set_real(proj, 2.0*r);
    gwy_data_line_set_offset(proj, -r);
    projection_to_line(xyz, n, vertical ? -0.5*G_PI : 0.0,
                       xc, yc, proj, NULL, counts);
    data_line_shorten(proj, counts, 4);

    if (!upwards)
        gwy_data_line_multiply(proj, -1.0);

    res = gwy_data_line_get_res(proj);
    real = gwy_data_line_get_real(proj);
    off = gwy_data_line_get_offset(proj);
    d = gwy_data_line_get_data(proj);
    for (i = j = 0; i < res; i++) {
        if (counts[i] > 5) {
            projdata[j].x = c + off + (i + 0.5)*real/res;
            projdata[j].y = d[i];
            j++;
        }
    }
    if (gwy_peaks_analyze_xy(peaks, projdata, j, 2) != 2)
        return FALSE;

    gwy_peaks_get_quantity(peaks, GWY_PEAK_ABSCISSA, param);
    gwy_peaks_get_quantity(peaks, GWY_PEAK_WIDTH, width);
    param[2] = 0.5*(width[0] + width[1]);
    return TRUE;
}

static gboolean
pring_estimate(const GwyXYZ *xyz, guint n, gdouble *param,
               FitShapeEstimateCache *estimcache)
{
    GwyDataLine *proj;
    GwyXY *projdata;
    GwyPeaks *peaks;
    gdouble xc, yc, r, zmin, zmax, zskew;
    gdouble xestim[3], yestim[3];
    guint *counts;
    gboolean ok1, ok2;
    guint res;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);
    stat_z(xyz, n, NULL, NULL, &zskew, estimcache);
    res = (guint)floor(0.8*sqrt(n) + 1.0);
    if (zskew < 0.0)
        GWY_SWAP(gdouble, zmin, zmax);

    proj = gwy_data_line_new(res, 2.0*r, FALSE);
    counts = g_new(guint, res);
    projdata = g_new(GwyXY, res);

    peaks = gwy_peaks_new();
    gwy_peaks_set_order(peaks, GWY_PEAK_ORDER_ABSCISSA);
    gwy_peaks_set_background(peaks, GWY_PEAK_BACKGROUND_MMSTEP);

    gwy_data_line_resample(proj, res, GWY_INTERPOLATION_NONE);
    ok1 = pring_estimate_projection(xyz, n, xc, yc, r, FALSE, zskew >= 0.0,
                                    proj, projdata, counts, peaks, xestim);

    gwy_data_line_resample(proj, res, GWY_INTERPOLATION_NONE);
    ok2 = pring_estimate_projection(xyz, n, xc, yc, r, TRUE, zskew >= 0.0,
                                    proj, projdata, counts, peaks, yestim);

    g_free(counts);
    g_object_unref(proj);
    gwy_peaks_free(peaks);

    if (!ok1 || !ok2)
        return FALSE;

    param[0] = 0.5*(xestim[0] + xestim[1]);
    param[1] = 0.5*(yestim[0] + yestim[1]);
    param[2] = zmin;
    param[3] = 0.25*(yestim[1] - yestim[0] + xestim[1] - xestim[0]);
    /* A bit too high value is OK because at least the estimated function
     * does not miss the ring completely. */
    param[4] = 1.5*(xestim[2] + yestim[2]);
    param[5] = zmax - zmin;
    param[6] = (zmax - zmin)/12.0;
    param[7] = 0.0;
    param[8] = 0.0;

    return TRUE;
}

/**************************************************************************
 *
 * Gaussian
 *
 **************************************************************************/

static gdouble
gaussian_func(gdouble x, gdouble y, const gdouble *param)
{
    DEFINE_PHI_CACHE(phi);

    gdouble xc = param[0];
    gdouble yc = param[1];
    gdouble z0 = param[2];
    gdouble h = param[3];
    gdouble sigma = param[4];
    gdouble a = fabs(param[5]);
    gdouble phi = param[6];
    gdouble bx = param[7];
    gdouble by = param[8];
    gdouble t, val, cphi, sphi, s2;

    x -= xc;
    y -= yc;
    z0 += bx*x + by*y;

    s2 = sigma*sigma;
    if (G_UNLIKELY(!s2 || !a))
        return z0;

    HANDLE_PHI_CACHE(phi);
    t = x*cphi - y*sphi;
    y = x*sphi + y*cphi;
    x = t;

    t = 0.5*(x*x*a + y*y/a)/s2;
    val = z0 + h*exp(-t);

    return val;
}

static gboolean
gaussian_init(const GwyXYZ *xyz, guint n, gdouble *param,
              FitShapeEstimateCache *estimcache)
{
    param[7] = 0.0;
    param[8] = 0.0;
    return common_bump_feature_init(xyz, n,
                                    param + 0, param + 1, param + 2,
                                    param + 3, param + 4,
                                    param + 5, param + 6,
                                    estimcache);
}

static gboolean
gaussian_estimate(const GwyXYZ *xyz, guint n, gdouble *param,
                  FitShapeEstimateCache *estimcache)
{
    param[7] = 0.0;
    param[8] = 0.0;
    return common_bump_feature_estimate(xyz, n,
                                        param + 0, param + 1, param + 2,
                                        param + 3, param + 4,
                                        param + 5, param + 6,
                                        estimcache);
}

static gdouble
gaussian_calc_sigma1(const gdouble *param)
{
    return param[4]/sqrt(fabs(param[5]));
}

static gdouble
gaussian_calc_err_sigma1(const gdouble *param,
                         const gdouble *param_err,
                         const gdouble *correl)
{
    gdouble sigma1 = gaussian_calc_sigma1(param);
    gdouble diff[G_N_ELEMENTS(gaussian_params)];

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[4] = sigma1/param[4];
    diff[5] = -0.5*sigma1/param[5];
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

static gdouble
gaussian_calc_sigma2(const gdouble *param)
{
    return param[4]*sqrt(fabs(param[5]));
}

static gdouble
gaussian_calc_err_sigma2(const gdouble *param,
                         const gdouble *param_err,
                         const gdouble *correl)
{
    gdouble sigma1 = gaussian_calc_sigma1(param);
    gdouble diff[G_N_ELEMENTS(gaussian_params)];

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[4] = sigma1/param[4];
    diff[5] = 0.5*sigma1/param[5];
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

/**************************************************************************
 *
 * Lorentzian
 *
 **************************************************************************/

static gdouble
lorentzian_func(gdouble x, gdouble y, const gdouble *param)
{
    DEFINE_PHI_CACHE(phi);

    gdouble xc = param[0];
    gdouble yc = param[1];
    gdouble z0 = param[2];
    gdouble h = param[3];
    gdouble b = param[4];
    gdouble a = fabs(param[5]);
    gdouble phi = param[6];
    gdouble bx = param[7];
    gdouble by = param[8];
    gdouble t, val, cphi, sphi, b2;

    x -= xc;
    y -= yc;
    z0 += bx*x + by*y;

    b2 = b*b;
    if (G_UNLIKELY(!b2 || !a))
        return z0;

    HANDLE_PHI_CACHE(phi);
    t = x*cphi - y*sphi;
    y = x*sphi + y*cphi;
    x = t;

    t = (x*x*a + y*y/a)/b2;
    val = z0 + h/(1.0 + t);

    return val;
}

static gboolean
lorentzian_init(const GwyXYZ *xyz, guint n, gdouble *param,
                FitShapeEstimateCache *estimcache)
{
    param[7] = 0.0;
    param[8] = 0.0;
    return common_bump_feature_init(xyz, n,
                                    param + 0, param + 1, param + 2,
                                    param + 3, param + 4,
                                    param + 5, param + 6,
                                    estimcache);
}

static gboolean
lorentzian_estimate(const GwyXYZ *xyz, guint n, gdouble *param,
                    FitShapeEstimateCache *estimcache)
{
    param[7] = 0.0;
    param[8] = 0.0;
    return common_bump_feature_estimate(xyz, n,
                                        param + 0, param + 1, param + 2,
                                        param + 3, param + 4,
                                        param + 5, param + 6,
                                        estimcache);
}

static gdouble
lorentzian_calc_b1(const gdouble *param)
{
    return param[4]/sqrt(fabs(param[5]));
}

static gdouble
lorentzian_calc_err_b1(const gdouble *param,
                       const gdouble *param_err,
                       const gdouble *correl)
{
    gdouble b1 = lorentzian_calc_b1(param);
    gdouble diff[G_N_ELEMENTS(lorentzian_params)];

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[4] = b1/param[4];
    diff[5] = -0.5*b1/param[5];
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

static gdouble
lorentzian_calc_b2(const gdouble *param)
{
    return param[4]*sqrt(fabs(param[5]));
}

static gdouble
lorentzian_calc_err_b2(const gdouble *param,
                       const gdouble *param_err,
                       const gdouble *correl)
{
    gdouble b1 = lorentzian_calc_b1(param);
    gdouble diff[G_N_ELEMENTS(lorentzian_params)];

    gwy_clear(diff, G_N_ELEMENTS(diff));
    diff[4] = b1/param[4];
    diff[5] = 0.5*b1/param[5];
    return dotprod_with_correl(diff, param_err, correl, G_N_ELEMENTS(diff));
}

/**************************************************************************
 *
 * Pyramid (diamond)
 *
 **************************************************************************/

static gdouble
pyramidx_func(gdouble x, gdouble y, const gdouble *param)
{
    DEFINE_PHI_CACHE(phi);

    gdouble xc = param[0];
    gdouble yc = param[1];
    gdouble z0 = param[2];
    gdouble h = param[3];
    gdouble L = param[4];
    gdouble a = fabs(param[5]);
    gdouble phi = param[6];
    gdouble bx = param[7];
    gdouble by = param[8];
    gdouble t, val, cphi, sphi, q;

    x -= xc;
    y -= yc;
    z0 += bx*x + by*y;

    if (G_UNLIKELY(!L || !a))
        return z0;

    HANDLE_PHI_CACHE(phi);
    t = x*cphi - y*sphi;
    y = x*sphi + y*cphi;
    x = t;

    q = L*sqrt(1.0 + a*a);
    x /= q;
    y *= a/q;
    t = fabs(x) + fabs(y);
    if (t < 1.0)
        val = z0 + h*(1.0 - t);
    else
        val = z0;

    return val;
}

static gboolean
pyramidx_init(const GwyXYZ *xyz, guint n, gdouble *param,
              FitShapeEstimateCache *estimcache)
{
    param[7] = 0.0;
    param[8] = 0.0;
    return common_bump_feature_init(xyz, n,
                                    param + 0, param + 1, param + 2,
                                    param + 3, param + 4,
                                    param + 5, param + 6,
                                    estimcache);
}

static gboolean
pyramidx_estimate(const GwyXYZ *xyz, guint n, gdouble *param,
                  FitShapeEstimateCache *estimcache)
{
    /* XXX: The pyramid has minimum projection when oriented along x and y
     * axes.  But not very deep.  Can we use it to estimate phi? */
    param[7] = 0.0;
    param[8] = 0.0;
    return common_bump_feature_estimate(xyz, n,
                                        param + 0, param + 1, param + 2,
                                        param + 3, param + 4,
                                        param + 5, param + 6,
                                        estimcache);
}

/**************************************************************************
 *
 * Misc.
 *
 **************************************************************************/

static GString*
create_fit_report(FitShapeControls *controls)
{
    const FitShapeFunc *func = functions + controls->function_id;
    GwyDataField *dfield;
    GwySurface *surface;
    GwySIUnit *xyunit, *zunit, *unit;
    gchar *s, *unitstr;
    GString *report;
    guint n, i, j, nparams, nsecondary;
    const gboolean *param_fixed = controls->ctx->param_fixed;

    if (controls->pageno == GWY_PAGE_XYZS) {
        surface = gwy_container_get_object_by_name(controls->mydata,
                                                   "/surface/0");
        xyunit = gwy_surface_get_si_unit_xy(surface);
        zunit = gwy_surface_get_si_unit_z(surface);
        n = gwy_surface_get_npoints(surface);
    }
    else {
        dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
        xyunit = gwy_data_field_get_si_unit_xy(dfield);
        zunit = gwy_data_field_get_si_unit_z(dfield);
        n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
    }
    unit = gwy_si_unit_new(NULL);

    report = g_string_new(NULL);

    g_string_append(report, _("===== Fit Results ====="));
    g_string_append_c(report, '\n');
    g_string_append_printf(report, _("Data:             %s\n"),
                           controls->title);
    g_string_append_printf(report, _("Number of points: %d of %d\n"),
                           controls->ctx->n, n);
    g_string_append_printf(report, _("Fitted function:  %s\n"), func->name);
    g_string_append_c(report, '\n');
    g_string_append_printf(report, _("Results\n"));

    nparams = func->nparams;
    for (i = 0; i < nparams; i++) {
        const FitShapeParam *fitparam = func->param + i;
        gdouble param = controls->param[i], err = controls->param_err[i];

        if (!pango_parse_markup(fitparam->name, -1, 0, NULL, &s, NULL, NULL)) {
            g_warning("Parameter name is not valid Pango markup");
            s = g_strdup(fitparam->name);
        }
        if (fitparam->flags & FIT_SHAPE_PARAM_ANGLE) {
            param *= 180.0/G_PI;
            err *= 180.0/G_PI;
            unitstr = g_strdup("deg");
        }
        else {
            gwy_si_unit_power_multiply(xyunit, fitparam->power_xy,
                                       zunit, fitparam->power_z,
                                       unit);
            unitstr = gwy_si_unit_get_string(unit, GWY_SI_UNIT_FORMAT_PLAIN);
        }

        if (param_fixed[i]) {
            g_string_append_printf(report, "%6s = %g %s%s %s\n",
                                   s, param, *unitstr ? " " : "", unitstr,
                                   _("(fixed)"));
        }
        else {
            g_string_append_printf(report, "%6s = %g ± %g%s%s\n",
                                   s, param, err, *unitstr ? " " : "", unitstr);
        }

        g_free(unitstr);
        g_free(s);
    }
    g_string_append_c(report, '\n');

    unitstr = gwy_si_unit_get_string(zunit, GWY_SI_UNIT_FORMAT_PLAIN);
    g_string_append_printf(report, "%s %g %s\n",
                           _("Mean square difference:"),
                           controls->rss, unitstr);
    g_free(unitstr);
    g_string_append_c(report, '\n');

    g_string_append(report, _("Correlation Matrix"));
    g_string_append_c(report, '\n');

    for (i = 0; i < nparams; i++) {
        g_string_append(report, "  ");
        for (j = 0; j <= i; j++) {
            if (param_fixed[i] || param_fixed[j]) {
                if (i == j)
                    g_string_append_printf(report, "% .03f", 1.0);
                else
                    g_string_append(report, "   ---");
            }
            else {
                g_string_append_printf(report, "% .03f",
                                       SLi(controls->correl, i, j));
            }
            if (j != i)
                g_string_append_c(report, ' ');
        }
        g_string_append_c(report, '\n');
    }
    g_string_append_c(report, '\n');

    nsecondary = func->nsecondary;
    if (nsecondary) {
        g_string_append(report, _("Derived Quantities"));
        g_string_append_c(report, '\n');
    }
    for (i = 0; i < nsecondary; i++) {
        const FitShapeSecondary *secparam = func->secondary + i;
        gdouble param = controls->secondary[i],
                err = controls->secondary_err[i];

        if (!pango_parse_markup(secparam->name, -1, 0, NULL, &s, NULL, NULL)) {
            g_warning("Parameter name is not valid Pango markup");
            s = g_strdup(secparam->name);
        }
        if (secparam->flags & FIT_SHAPE_PARAM_ANGLE) {
            param *= 180.0/G_PI;
            err *= 180.0/G_PI;
            unitstr = g_strdup("deg");
        }
        else {
            gwy_si_unit_power_multiply(xyunit, secparam->power_xy,
                                       zunit, secparam->power_z,
                                       unit);
            unitstr = gwy_si_unit_get_string(unit, GWY_SI_UNIT_FORMAT_PLAIN);
        }
        g_string_append_printf(report, "%6s = %g ± %g%s%s\n",
                               s, param, err, *unitstr ? " " : "", unitstr);
        g_free(unitstr);
        g_free(s);
    }
    if (nsecondary)
        g_string_append_c(report, '\n');

    g_object_unref(unit);

    return report;
}

static const gchar diff_colourmap_key[] = "/module/fit_shape/diff_colourmap";
static const gchar diff_excluded_key[]  = "/module/fit_shape/diff_excluded";
static const gchar display_key[]        = "/module/fit_shape/display";
static const gchar function_key[]       = "/module/fit_shape/function";
static const gchar masking_key[]        = "/module/fit_shape/masking";
static const gchar output_key[]         = "/module/fit_shape/output";

static void
fit_shape_sanitize_args(FitShapeArgs *args)
{
    guint i;
    gboolean ok;

    args->masking = gwy_enum_sanitize_value(args->masking,
                                            GWY_TYPE_MASKING_TYPE);
    args->display = MIN(args->display, FIT_SHAPE_DISPLAY_DIFF);
    args->output = MIN(args->output, FIT_SHAPE_OUTPUT_BOTH);
    args->diff_colourmap = !!args->diff_colourmap;
    args->diff_excluded = !!args->diff_excluded;

    ok = FALSE;
    for (i = 0; i < G_N_ELEMENTS(functions); i++) {
        if (gwy_strequal(args->function, functions[i].name)) {
            ok = TRUE;
            break;
        }
    }
    if (!ok)
        args->function = fit_shape_defaults.function;
}

static void
fit_shape_load_args(GwyContainer *container,
                    FitShapeArgs *args)
{
    *args = fit_shape_defaults;

    gwy_container_gis_string_by_name(container, function_key,
                                     (const guchar**)&args->function);
    gwy_container_gis_enum_by_name(container, display_key, &args->display);
    gwy_container_gis_enum_by_name(container, masking_key, &args->masking);
    gwy_container_gis_enum_by_name(container, output_key, &args->output);
    gwy_container_gis_boolean_by_name(container, diff_colourmap_key,
                                      &args->diff_colourmap);
    gwy_container_gis_boolean_by_name(container, diff_excluded_key,
                                      &args->diff_excluded);
    fit_shape_sanitize_args(args);
}

static void
fit_shape_save_args(GwyContainer *container,
                    FitShapeArgs *args)
{
    gwy_container_set_const_string_by_name(container, function_key,
                                           args->function);
    gwy_container_set_enum_by_name(container, display_key, args->display);
    gwy_container_set_enum_by_name(container, masking_key, args->masking);
    gwy_container_set_enum_by_name(container, output_key, args->output);
    gwy_container_set_boolean_by_name(container, diff_colourmap_key,
                                      args->diff_colourmap);
    gwy_container_set_boolean_by_name(container, diff_excluded_key,
                                      args->diff_excluded);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
