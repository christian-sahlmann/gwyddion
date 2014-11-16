#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/brick.h>
#include <libprocess/datafield.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule-volume.h>
#include <app/gwyapp.h>

#define DBSCAN_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    RESPONSE_RESET   = 1,
};

typedef struct {
	gdouble MinPts;     /* convergence precision */
	gboolean normalize;  /* normalize brick before dbscan run */
} dbscanArgs;

typedef struct {
    GtkObject *MinPts;
    GtkWidget *normalize;
} dbscanControls;

static gboolean module_register     (void);
static void     volume_dbscan       (GwyContainer *data,
                                     GwyRunType run);
static void     dbscan_dialog       (GwyContainer *data,
                                     dbscanArgs *args);
static void     MinPts_changed_cb  (GtkAdjustment *adj,
                                     dbscanArgs *args);
static void     dbscan_dialog_update(dbscanControls *controls,
									 dbscanArgs *args);
static void     dbscan_values_update(dbscanControls *controls,
									 dbscanArgs *args);
GwyBrick *      normalize_brick     (GwyBrick *brick);
static void     volume_dbscan_do    (GwyContainer *data,
									 dbscanArgs *args);
static void     dbscan_load_args    (GwyContainer *container,
									 dbscanArgs *args);
static void     dbscan_save_args    (GwyContainer *container,
									 dbscanArgs *args);

static const dbscanArgs dbscan_defaults = {
	1,
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
	N_("Calculates dbscan clustering on volume data."),
    "Daniil Bratashov <dn2010@gmail.com> & Evgeniy Ryabov <k1u2r3ka@mail.ru>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek & Daniil Bratashov & Evgeniy Ryabov",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
	gwy_volume_func_register("dbscan",
							  (GwyVolumeFunc)&volume_dbscan,
							  N_("/_dbscan clustering..."),
                              NULL,
                              DBSCAN_RUN_MODES,
                              GWY_MENU_FLAG_VOLUME,
                              N_("Calculate dbscan clustering on volume data"));

    return TRUE;
}

static void
volume_dbscan(GwyContainer *data, GwyRunType run)
{
    dbscanArgs args;
    GwyBrick *brick = NULL;
    gint id;

    g_return_if_fail(run & DBSCAN_RUN_MODES);

    dbscan_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_BRICK, &brick,
                                     GWY_APP_BRICK_ID, &id,
                                     0);
    g_return_if_fail(GWY_IS_BRICK(brick));
    if (run == GWY_RUN_INTERACTIVE) {
		dbscan_dialog(data, &args);
	}
    else if (run == GWY_RUN_IMMEDIATE) {
        volume_dbscan_do(data, &args);
    }
}

static void
dbscan_dialog (GwyContainer *data, dbscanArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    gint response;
    dbscanControls controls;
    gint row = 0;

    dialog = gtk_dialog_new_with_buttons(_("dbscan"),
                                         NULL, 0, NULL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Run"),
                                                          GTK_STOCK_OK),
                                 GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog),
                                    GTK_RESPONSE_CANCEL);
    gwy_help_add_to_volume_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       TRUE, TRUE, 4);


	controls.MinPts = gtk_adjustment_new(args->MinPts,
										  0, 200, 1, 10, 0);
    spin = gwy_table_attach_hscale(table, row,
                                   _("Convergence _Precision:"), NULL,
                                   controls.MinPts, GWY_HSCALE_LOG);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 20);
	g_object_set_data(G_OBJECT(controls.MinPts),
                      "controls", &controls);
	g_signal_connect(controls.MinPts, "value-changed",
                     G_CALLBACK(MinPts_changed_cb), args);
    row++;


    controls.normalize
        = gtk_check_button_new_with_mnemonic(_("_Normalize"));
    gtk_table_attach_defaults(GTK_TABLE(table), controls.normalize,
                              0, 3, row, row+1);

    dbscan_dialog_update(&controls, args);
    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
                dbscan_values_update(&controls, args);
                gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
				dbscan_values_update(&controls, args);
				volume_dbscan_do(data, args);
            break;

            case RESPONSE_RESET:
				*args = dbscan_defaults;
                dbscan_dialog_update(&controls, args);
            break;

            default:
                g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

	dbscan_values_update(&controls, args);
	dbscan_save_args(gwy_app_settings_get(), args);
    gtk_widget_destroy(dialog);
}

static void
MinPts_changed_cb(GtkAdjustment *adj,
				   dbscanArgs *args)
{
    dbscanControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
	args->MinPts = gtk_adjustment_get_value(adj);
	dbscan_dialog_update(controls, args);
}

GwyBrick *
normalize_brick(GwyBrick *brick)
{
    GwyBrick *result;
	gdouble wmin, dataval, integral;
	gint i, j, l, xres, yres, zres;
    const gdouble *olddata;
    gdouble *newdata;

    result = gwy_brick_new_alike(brick, TRUE);
    wmin = gwy_brick_get_min(brick);
    xres = gwy_brick_get_xres(brick);
    yres = gwy_brick_get_yres(brick);
    zres = gwy_brick_get_zres(brick);
    olddata = gwy_brick_get_data_const(brick);
    newdata = gwy_brick_get_data(result);

    for (i = 0; i < xres; i++)
        for (j = 0; j < yres; j++) {
            integral = 0;
            for (l = 0; l < zres; l++) {
                dataval = *(olddata + l * xres * yres + j * xres + i);
                integral += (dataval - wmin);
            }
            for (l = 0; l < zres; l++) {
                dataval = *(olddata + l * xres * yres + j * xres + i);
                if (integral != 0.0) {
                    *(newdata + l * xres * yres + j * xres + i)
                                   = (dataval - wmin) * zres / integral;
                }
            }
        }

    return result;
}

static void
dbscan_values_update(dbscanControls *controls,
                     dbscanArgs *args)
{
	args->MinPts
		= gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->MinPts));
    args->normalize
        = gtk_toggle_button_get_active(
                                GTK_TOGGLE_BUTTON(controls->normalize));
}

static void
dbscan_dialog_update(dbscanControls *controls,
					 dbscanArgs *args)
{
	gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->MinPts),
							 args->MinPts);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->normalize),
                                 args->normalize);
}

static void
volume_dbscan_do(GwyContainer *container, dbscanArgs *args)
{
    GwyBrick *brick = NULL, *normalized = NULL;
    GwyDataField *dfield = NULL, *errormap = NULL;
    GwyGraphCurveModel *gcmodel;
    GwyGraphModel *gmodel;
    GwyDataLine *calibration = NULL;
    GwySIUnit *siunit;
    const GwyRGBA *rgba;
    gint id;
    gchar *description;
    GRand *rand;
    const gdouble *data;
	gdouble *centers, *sum, *data1, *xdata, *ydata;
	gdouble *errordata;
    gdouble min, dist, xreal, yreal, zreal, xoffset, yoffset, zoffset;
	gdouble MinPts= args->MinPts;
	gint xres, yres, zres, newid, number_of_cluster, elements_no_cluster;
	gint i, j, ii, jj, iii, jjj, l, c, m, t, u;
	gint *npix, *fusion;
	gint k, kk, No_cluster;
	gboolean converged = FALSE;
    gboolean normalize = args->normalize;

    gwy_app_data_browser_get_current(GWY_APP_BRICK, &brick,
                                     GWY_APP_BRICK_ID, &id,
                                     0);
	g_return_if_fail(GWY_IS_BRICK(brick));

    xres = gwy_brick_get_xres(brick);
    yres = gwy_brick_get_yres(brick);
    zres = gwy_brick_get_zres(brick);
    xreal = gwy_brick_get_xreal(brick);
    yreal = gwy_brick_get_yreal(brick);
    zreal = gwy_brick_get_zreal(brick);
    xoffset = gwy_brick_get_xoffset(brick);
    yoffset = gwy_brick_get_yoffset(brick);
    zoffset = gwy_brick_get_zoffset(brick);

    if (normalize) {
        normalized = normalize_brick(brick);
        data = gwy_brick_get_data_const(normalized);
    }
    else {
        data = gwy_brick_get_data_const(brick);
    }

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, TRUE);
    gwy_data_field_set_xoffset(dfield, xoffset);
    gwy_data_field_set_yoffset(dfield, yoffset);

    siunit = gwy_brick_get_si_unit_x(brick);
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    siunit = gwy_si_unit_new(_("Cluster"));
    gwy_data_field_set_si_unit_z(dfield, siunit);
    data1 = gwy_data_field_get_data(dfield);
    fusion = g_malloc0(xres*yres*sizeof(int));
	k = 0;
	kk = 0;
	number_of_cluster = 0;
	elements_no_cluster = 0;
	No_cluster = 0;
	u = 0;

	for (i = 0; i < xres; i++)
		for (j = 0; j < yres; j++)
			for (ii = 0; ii < xres; ii++)
				for (jj = 0; jj < yres; jj++) {
					dist = 0;
					for (l = 0; l < zres; l++) {
						dist+= (*(data + l * xres * yres + j * xres + i)
							  - *(data + l * xres * yres + jj * xres + ii))
							 * (*(data + l * xres * yres + j * xres + i)
							  - *(data + l * xres * yres + jj * xres + ii));
					}
					if (sqrt(dist) <= MinPts) {
						if (( *(data1 + jj * xres + ii) != 0) &&
										 ( *(data1 + j * xres + i) != 0)) {
							m = *(data1 + j * xres + i);
							t = *(data1 + jj * xres + ii);
							if ( *(fusion + m) > *(fusion + t)){
								*(fusion + m) = *(fusion + t);
                            }
							else
								*(fusion + t) = *(fusion + m);
						}
						else
							if (( *(data1 + jj * xres + ii) == 0) &&
										 ( *(data1 + j * xres + i) == 0)) {
								k++;
								*(data1 + j * xres + i) = k;
								*(data1 + jj * xres + ii) = k;
								*(fusion + k) = k;
						}
						else
							if (( *(data1 + jj * xres + ii) != 0) &&
										 ( *(data1 + j * xres + i) == 0)) {
								*(data1 + j * xres + i) = *(data1 + jj * xres + ii);
						}
						else
							*(data1 + jj * xres + ii) = *(data1 + j * xres + i);
					}
				}
	
	centers = g_malloc0(zres*k*sizeof(gdouble));
	sum = g_malloc0(zres*k*sizeof (gdouble));
    npix = g_malloc0(k*sizeof (gint));
	for (i = 0; i < xres; i++)
		for (j = 0; j < yres; j++) {
			c = *(data1 + j * xres + i);
			t=0;
			if (c != 0) {
			while (t == 0){
				m = *(fusion + c);
				if ( *(fusion + m) != *(fusion + c)) {
					*(fusion + c) = *(fusion + m);
					*(data1 + j * xres + i) = *(fusion + m);
				}
				else {
					t++;
					}
				};
				c = *(fusion + c);
				c--;
				(*(npix + c))++ ;
				for(l = 0;  l < zres; l++)
					*(sum + c * zres + l) +=
							*(data + l * xres * yres + j * xres + i);
			}
			else {
				No_cluster++;
			}
		}
	
	for (c = 0; c < k; c++)
		for (l =0; l < zres; l++) {
			if ( *(npix + c) != 0) {
				*(centers + c * zres + l) =
					*(sum + c * zres + l) / (gdouble)(*(npix + c));
			}
			else {
				*(centers + c * zres + l) = 0;
				kk++;
			}
		}
	number_of_cluster = k - kk;
	
    if (container) {
        errormap = gwy_data_field_new_alike(dfield, TRUE);
        siunit = gwy_si_unit_new(_("Error"));
        gwy_data_field_set_si_unit_z(errormap, siunit);
        errordata = gwy_data_field_get_data(errormap);
	
        for (i = 0; i < xres; i++)
            for (j = 0; j < yres; j++) {
                dist = 0.0;
				c = (gint)(*(data1 + j * xres + i));
				if (c != 0) {
					c--;
				for (l = 0; l < zres; l++) {
					dist += (*(data + l * xres * yres + j * xres + i)
						   - *(centers + c * zres + l))
						  * (*(data + l * xres * yres + j * xres + i)
						   - *(centers + c * zres + l));
				}
				*(errordata + j * xres + i) = sqrt(dist);
				}
				else
					*(errordata + j * xres + i) = 0;
			}

        gwy_data_field_add(dfield, 1.0);
        newid = gwy_app_data_browser_add_data_field(dfield,
                                                    container, TRUE);
        g_object_unref(dfield);
        description = gwy_app_get_brick_title(container, id);
        gwy_app_set_data_field_title(container, newid,
									 g_strdup_printf(_("dbscan of %s"),
                                                     description)
                                     );
		gwy_app_channel_log_add(container, -1, newid, "volume::dbscan",
                                NULL);

        newid = gwy_app_data_browser_add_data_field(errormap,
                                                    container, TRUE);
        g_object_unref(errormap);
        gwy_app_set_data_field_title(container, newid,
                                     g_strdup_printf(
											   _("dbscan error of %s"),
                                               description)
                                     );
        g_free(description);
		gwy_app_channel_log_add(container, -1, newid, "volume::dbscan",
                                NULL);

        gmodel = gwy_graph_model_new();
        calibration = gwy_brick_get_zcalibration(brick);
        if (calibration) {
            xdata = gwy_data_line_get_data(calibration);
            siunit = gwy_data_line_get_si_unit_y(calibration);
        }
        else {
            xdata = g_malloc(zres * sizeof(gdouble));
            for (i = 0; i < zres; i++)
                *(xdata + i) = zreal * i / zres + zoffset;
            siunit = gwy_brick_get_si_unit_z(brick);
        }
        for (c = 0; c < k; c++) {
			if ( *(npix + c) != 0) {
				
				ydata = g_memdup(centers + u * zres,
								zres * sizeof(gdouble));
				gcmodel = gwy_graph_curve_model_new();
				gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, zres);
				rgba = gwy_graph_get_preset_color(u);
				g_object_set(gcmodel,
							"mode", GWY_GRAPH_CURVE_LINE,
							"description",
							g_strdup_printf(_("dbscan center %d"), u + 1),
							"color", rgba,
							NULL);
				gwy_graph_model_add_curve(gmodel, gcmodel);
				g_object_unref(gcmodel);
				u++;
			}
        }
        g_object_set(gmodel,
                     "si-unit-x", siunit,
                     "si-unit-y", gwy_brick_get_si_unit_w(brick),
                     "axis-label-bottom", "x",
                     "axis-label-left", "y",
                     NULL);
        gwy_app_data_browser_add_graph_model(gmodel, container, TRUE);
        g_object_unref(gmodel);
    }

    if (normalized) {
        g_object_unref(normalized);
    }
    g_free(npix);
    g_free(sum);

    g_free(centers);

    gwy_app_volume_log_add_volume(container, id, id);
}

static const gchar MinPts_key[]        = "/module/dbscan/MinPts";
static const gchar normalize_key[]      = "/module/dbscan/normalize";

static void
dbscan_sanitize_args(dbscanArgs *args)
{
    args->MinPts = CLAMP(args->MinPts, 0, 200);
    args->normalize = !!args->normalize;

}

static void
dbscan_load_args(GwyContainer *container,
				 dbscanArgs *args)
{
	*args = dbscan_defaults;

	gwy_container_gis_double_by_name(container, MinPts_key,
														&args->MinPts);
	gwy_container_gis_boolean_by_name(container, normalize_key,
                                                      &args->normalize);

	dbscan_sanitize_args(args);
}

static void
dbscan_save_args(GwyContainer *container,
				 dbscanArgs *args)
{
	gwy_container_set_double_by_name(container, MinPts_key,
														 args->MinPts);
    gwy_container_set_boolean_by_name(container, normalize_key,
                                                       args->normalize);
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

