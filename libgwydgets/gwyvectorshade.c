#include <math.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "gwyvectorshade.h"

#define GWY_VECTOR_SHADE_TYPE_NAME "GwyVectorShade"

#define _(x) x

/* Forward declarations */

static void     gwy_vector_shade_class_init     (GwyVectorShadeClass *klass);
static void     gwy_vector_shade_init           (GwyVectorShade *vector_shade);
GtkWidget*      gwy_vector_shade_new            (GwySphereCoords *sphere_coords);
static void     gwy_vector_shade_phi_update     (GtkAdjustment *adj,
                                                 GwyVectorShade *vector_shade);
static void     gwy_vector_shade_theta_update   (GtkAdjustment *adj,
                                                 GwyVectorShade *vector_shade);
static void     gwy_vector_shade_coords_update  (GwySphereCoords *sphere_coords,
                                                 GwyVectorShade *vector_shade);


/* Local data */

GType
gwy_vector_shade_get_type(void)
{
    static GType gwy_vector_shade_type = 0;

    if (!gwy_vector_shade_type) {
        static const GTypeInfo gwy_vector_shade_info = {
            sizeof(GwyVectorShadeClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_vector_shade_class_init,
            NULL,
            NULL,
            sizeof(GwyVectorShade),
            0,
            (GInstanceInitFunc)gwy_vector_shade_init,
            NULL,
        };
        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
        gwy_vector_shade_type = g_type_register_static(GTK_TYPE_TABLE,
                                                       GWY_VECTOR_SHADE_TYPE_NAME,
                                                       &gwy_vector_shade_info,
                                                       0);
    }

    return gwy_vector_shade_type;
}

static void
gwy_vector_shade_class_init(GwyVectorShadeClass *klass)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    /* FIXME: it seems there's nothing to do? */
}

static void
gwy_vector_shade_init(GwyVectorShade *vector_shade)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    vector_shade->grad_sphere = NULL;
    vector_shade->spin_theta = NULL;
    vector_shade->spin_phi = NULL;
    vector_shade->adj_theta = NULL;
    vector_shade->adj_phi = NULL;
}

GtkWidget*
gwy_vector_shade_new(GwySphereCoords *sphere_coords)
{
    GwyVectorShade *vector_shade;
    GtkWidget *label;
    GwyGradSphere *grad_sphere;
    gdouble phi, theta;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    vector_shade
        = GWY_VECTOR_SHADE(gtk_widget_new(GWY_TYPE_VECTOR_SHADE, NULL));

    if (sphere_coords)
        g_return_val_if_fail(GWY_IS_SPHERE_COORDS(sphere_coords), NULL);
    else
        sphere_coords = (GwySphereCoords*)gwy_sphere_coords_new(0.0, 0.0);

    grad_sphere = GWY_GRAD_SPHERE(gwy_grad_sphere_new(sphere_coords));
    vector_shade->grad_sphere = grad_sphere;

    theta = 180.0/M_PI*gwy_sphere_coords_get_theta(sphere_coords);
    phi = 180.0/M_PI*gwy_sphere_coords_get_phi(sphere_coords);

    vector_shade->adj_theta
        = GTK_ADJUSTMENT(gtk_adjustment_new(theta, 0.0, 90.0,
                                            5.0, 15.0, 0.0));
    vector_shade->adj_phi
        = GTK_ADJUSTMENT(gtk_adjustment_new(phi, 0.0, 360.0,
                                            5.0, 30.0, 0.0));

    vector_shade->spin_theta = gtk_spin_button_new(vector_shade->adj_theta,
                                                   1.0, 0);
    vector_shade->spin_phi = gtk_spin_button_new(vector_shade->adj_phi,
                                                 1.0, 0);

    gtk_table_resize(GTK_TABLE(vector_shade), 4, 4);
    gtk_table_set_homogeneous(GTK_TABLE(vector_shade), FALSE);

    gtk_table_attach_defaults(GTK_TABLE(vector_shade),
                              GTK_WIDGET(grad_sphere), 0, 1, 0, 4);

    label = gtk_label_new(" ");
    gtk_table_attach_defaults(GTK_TABLE(vector_shade), label, 1, 2, 0, 1);
    gtk_widget_show(label);
    label = gtk_label_new(" ");
    gtk_table_attach_defaults(GTK_TABLE(vector_shade), label, 1, 2, 3, 4);
    gtk_widget_show(label);

    gtk_table_attach_defaults(GTK_TABLE(vector_shade),
                              vector_shade->spin_theta, 2, 3, 1, 2);
    gtk_widget_show(vector_shade->spin_theta);

    label = gtk_label_new_with_mnemonic(_("_Theta: "));
    gtk_table_attach_defaults(GTK_TABLE(vector_shade), label, 1, 2, 1, 2);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), vector_shade->spin_theta);
    gtk_widget_show(label);

    label = gtk_label_new(_(" deg"));
    gtk_table_attach_defaults(GTK_TABLE(vector_shade), label, 3, 4, 1, 2);
    gtk_widget_show(label);

    gtk_table_attach_defaults(GTK_TABLE(vector_shade),
                              vector_shade->spin_phi, 2, 3, 2, 3);
    gtk_widget_show(vector_shade->spin_phi);

    label = gtk_label_new_with_mnemonic(_("_Phi: "));
    gtk_table_attach_defaults(GTK_TABLE(vector_shade), label, 1, 2, 2, 3);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), vector_shade->spin_phi);
    gtk_widget_show(label);

    label = gtk_label_new(_(" deg"));
    gtk_table_attach_defaults(GTK_TABLE(vector_shade), label, 3, 4, 2, 3);
    gtk_widget_show(label);

    g_signal_connect(G_OBJECT(vector_shade->adj_theta),
                     "value_changed",
                     G_CALLBACK(gwy_vector_shade_theta_update),
                     vector_shade);
    g_signal_connect(G_OBJECT(vector_shade->adj_phi),
                     "value_changed",
                     G_CALLBACK(gwy_vector_shade_phi_update),
                     vector_shade);
    g_signal_connect(G_OBJECT(gwy_grad_sphere_get_sphere_coords(grad_sphere)),
                     "value_changed",
                     G_CALLBACK(gwy_vector_shade_coords_update),
                     vector_shade);

    return GTK_WIDGET(vector_shade);
}

GwySphereCoords*
gwy_vector_shade_get_sphere_coords(GwyVectorShade *vector_shade)
{
    g_return_val_if_fail(vector_shade != NULL, NULL);
    g_return_val_if_fail(GWY_IS_VECTOR_SHADE(vector_shade), NULL);

    return gwy_grad_sphere_get_sphere_coords(vector_shade->grad_sphere);
}

#define BLOCK_ALL \
    g_signal_handlers_block_matched(vector_shade->adj_theta, \
                                    G_SIGNAL_MATCH_DATA, \
                                    0, 0, NULL, NULL, vector_shade); \
    g_signal_handlers_block_matched(vector_shade->adj_phi, \
                                    G_SIGNAL_MATCH_DATA, \
                                    0, 0, NULL, NULL, vector_shade); \
    g_signal_handlers_block_matched(sphere_coords, \
                                    G_SIGNAL_MATCH_DATA, \
                                    0, 0, NULL, NULL, vector_shade)
#define UNBLOCK_ALL \
    g_signal_handlers_unblock_matched(vector_shade->adj_theta, \
                                      G_SIGNAL_MATCH_DATA, \
                                      0, 0, NULL, NULL, vector_shade); \
    g_signal_handlers_unblock_matched(vector_shade->adj_phi, \
                                      G_SIGNAL_MATCH_DATA, \
                                      0, 0, NULL, NULL, vector_shade); \
    g_signal_handlers_unblock_matched(sphere_coords, \
                                      G_SIGNAL_MATCH_DATA, \
                                      0, 0, NULL, NULL, vector_shade)

static void
gwy_vector_shade_theta_update(GtkAdjustment *adj,
                              GwyVectorShade *vector_shade)
{
    gdouble theta, phi;
    GwySphereCoords *sphere_coords;

    g_return_if_fail(GWY_IS_VECTOR_SHADE(vector_shade));
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "GwyVectorShade theta adj update");
    #endif

    sphere_coords = gwy_vector_shade_get_sphere_coords(vector_shade);
    theta = M_PI/180.0*gtk_adjustment_get_value(adj);
    phi = gwy_sphere_coords_get_phi(sphere_coords);

    BLOCK_ALL;
    gwy_sphere_coords_set_value(sphere_coords, theta, phi);
    UNBLOCK_ALL;
}

static void
gwy_vector_shade_phi_update(GtkAdjustment *adj,
                            GwyVectorShade *vector_shade)
{
    gdouble theta, phi;
    GwySphereCoords *sphere_coords;

    g_return_if_fail(GWY_IS_VECTOR_SHADE(vector_shade));
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "GwyVectorShade phi adj update");
    #endif

    sphere_coords = gwy_vector_shade_get_sphere_coords(vector_shade);
    phi = M_PI/180.0*gtk_adjustment_get_value(adj);
    theta = gwy_sphere_coords_get_theta(sphere_coords);

    BLOCK_ALL;
    gwy_sphere_coords_set_value(sphere_coords, theta, phi);
    UNBLOCK_ALL;
}

static void
gwy_vector_shade_coords_update(GwySphereCoords *sphere_coords,
                               GwyVectorShade *vector_shade)
{
    gdouble theta, phi;

    g_return_if_fail(GWY_IS_VECTOR_SHADE(vector_shade));
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "GwyVectorShade coords update");
    #endif

    theta = 180.0/M_PI*gwy_sphere_coords_get_theta(sphere_coords);
    phi = 180.0/M_PI*gwy_sphere_coords_get_phi(sphere_coords);

    BLOCK_ALL;
    gtk_adjustment_set_value(vector_shade->adj_theta, theta);
    gtk_adjustment_set_value(vector_shade->adj_phi, phi);
    UNBLOCK_ALL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
