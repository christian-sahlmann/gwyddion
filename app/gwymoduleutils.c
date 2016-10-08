/*
 *  @(#) $Id$
 *  Copyright (C) 2007-2016 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <app/file.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils.h>

typedef struct {
    const gchar *data;
    gssize len;
} GwySaveAuxiliaryData;

static gchar*
gwy_save_auxiliary_data_create(gpointer user_data,
                               gssize *data_len)
{
    GwySaveAuxiliaryData *savedata = (GwySaveAuxiliaryData*)user_data;

    *data_len = savedata->len;

    return (gchar*)savedata->data;
}

/**
 * gwy_save_auxiliary_data:
 * @title: File chooser dialog title.
 * @parent: Parent window for the file chooser dialog (may be %NULL).
 * @data_len: The length of @data in bytes.  Pass -1 if @data is text, it must
 *            be nul-terminated then and it will be saved in text mode (this
 *            matters if the operating system distinguishes between text and
 *            binary).  A non-negative value causes the data to be saved as
 *            binary.
 * @data: The data to save.
 *
 * Saves a report or other auxiliary data to a user specified file.
 *
 * This is actually a simple gwy_save_auxiliary_with_callback() wrapper, see
 * its description for details.
 *
 * Returns: %TRUE if the data was save, %FALSE if it was not saved for any
 *          reason.
 *
 * Since: 2.3
 **/
gboolean
gwy_save_auxiliary_data(const gchar *title,
                        GtkWindow *parent,
                        gssize data_len,
                        const gchar *data)
{
    GwySaveAuxiliaryData savedata;

    g_return_val_if_fail(data, FALSE);
    savedata.data = data;
    savedata.len = data_len;

    return gwy_save_auxiliary_with_callback(title, parent,
                                            &gwy_save_auxiliary_data_create,
                                            NULL,
                                            &savedata);
}

/**
 * gwy_save_auxiliary_with_callback:
 * @title: File chooser dialog title.
 * @parent: Parent window for the file chooser dialog (may be %NULL).
 * @create: Function to create the data (it will not be called if the user
 *          cancels the saving).
 * @destroy: Function to destroy the data (if will be called iff @create will
 *           be called), it may be %NULL.
 * @user_data: User data passed to @create and @destroy.
 *
 * Saves a report or other auxiliary data to a user specified file.
 *
 * Returns: %TRUE if the data was save, %FALSE if it was not saved for any
 *          reason (I/O error, cancellation, overwrite cancellation, etc.).
 *
 * Since: 2.3
 **/
gboolean
gwy_save_auxiliary_with_callback(const gchar *title,
                                 GtkWindow *parent,
                                 GwySaveAuxiliaryCreate create,
                                 GwySaveAuxiliaryDestroy destroy,
                                 gpointer user_data)
{
    gchar *data;
    gchar *filename_sys, *filename_utf8;
    gssize data_len;
    gint myerrno;
    GtkWidget *dialog, *chooser;
    gint response;
    FILE *fh;

    g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), FALSE);
    g_return_val_if_fail(create, FALSE);

    chooser = gtk_file_chooser_dialog_new(title, parent,
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                          NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_OK);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser),
                                        gwy_app_get_current_directory());
    response = gtk_dialog_run(GTK_DIALOG(chooser));
    filename_sys = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));

    if (!gwy_app_file_confirm_overwrite(chooser))
        response = GTK_RESPONSE_CANCEL;

    gtk_widget_destroy(chooser);

    if (!filename_sys || response != GTK_RESPONSE_OK) {
        g_free(filename_sys);
        return FALSE;
    }

    data_len = -1;
    data = create(user_data, &data_len);
    g_return_val_if_fail(data, FALSE);

    if ((fh = gwy_fopen(filename_sys, "wb"))) {
        gchar *mydata = NULL;

        /* Write everything in binary and just convert the EOLs by manually.
         * This seems to actually work as we want. */
        if (data_len <= 0) {
#ifdef G_OS_WIN32
            mydata = gwy_strreplace(data, "\n", "\r\n", (gsize)-1);
            data_len = strlen(mydata);
#else
            data_len = strlen(data);
#endif
        }
        if (fwrite(mydata ? mydata : data, data_len, 1, fh) != 1) {
            myerrno = errno;
            /* This is just best-effort clean-up */
            fclose(fh);
            g_unlink(filename_sys);
            fh = NULL;
        }
        else
            myerrno = 0;  /* GCC */

        g_free(mydata);
    }
    else
        myerrno = errno;

    if (destroy)
        destroy(data, user_data);

    if (fh) {
        /* Everything went all right. */
        fclose(fh);
        gwy_app_set_current_directory(filename_sys);
        g_free(filename_sys);
        return TRUE;
    }

    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    dialog = gtk_message_dialog_new(parent, 0, GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    _("Saving of `%s' failed"),
                                    filename_utf8);
    g_free(filename_sys);
    g_free(filename_utf8);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                             _("Cannot write to file: %s."),
                                             g_strerror(myerrno));
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return FALSE;
}

/**
 * gwy_set_data_preview_size:
 * @data_view: A data view used for module preview.
 * @max_size: Maximum allowed @data_view size (width and height).
 *
 * Sets up data view zoom to not exceed specified size.
 *
 * Before calling this function, data keys have be set, data fields and layers
 * have to be present and physically square mode set in the container.
 * Sizing of both pixel-wise square and physically square displays is performed
 * correctly.
 *
 * Since: 2.7
 **/
void
gwy_set_data_preview_size(GwyDataView *data_view,
                          gint max_size)
{
    GwyContainer *container;
    GwyDataField *data_field;
    GwyPixmapLayer *layer;
    gdouble zoomval, scale, xreal, yreal;
    gboolean realsquare;
    gint xres, yres;
    const gchar *prefix;
    gchar *key;

    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    g_return_if_fail(max_size >= 2);

    container = gwy_data_view_get_data(data_view);
    g_return_if_fail(GWY_IS_CONTAINER(container));

    layer = gwy_data_view_get_base_layer(data_view);
    g_return_if_fail(GWY_IS_PIXMAP_LAYER(layer));
    prefix = gwy_pixmap_layer_get_data_key(layer);
    g_return_if_fail(prefix);

    data_field = gwy_container_get_object_by_name(container, prefix);
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    prefix = gwy_data_view_get_data_prefix(data_view);
    g_return_if_fail(prefix);
    key = g_strconcat(prefix, "/realsquare", NULL);
    realsquare = FALSE;
    gwy_container_gis_boolean_by_name(container, key, &realsquare);
    g_free(key);

    xres = gwy_data_field_get_xres(data_field);
    yres = gwy_data_field_get_yres(data_field);
    if (!realsquare)
        zoomval = max_size/(gdouble)MAX(xres, yres);
    else {
        xreal = gwy_data_field_get_xreal(data_field);
        yreal = gwy_data_field_get_yreal(data_field);
        scale = MAX(xres/xreal, yres/yreal);
        zoomval = max_size/(scale*MAX(xreal, yreal));
    }
    gwy_data_view_set_zoom(data_view, zoomval);
}

static gboolean
clear_data_id(GwyAppDataId *id)
{
    id->datano = 0;
    id->id = -1;
    return FALSE;
}

/**
 * gwy_app_data_id_verify_channel:
 * @id: Numerical identifiers of a channel in data managed by the data browser.
 *
 * Checks if numerical channel identifiers correspond to an existing channel.
 *
 * If either the data contained referenced in @id or the channel does not exist
 * the structure is cleared to %GWY_APP_DATA_ID_NONE and the function returns
 * %FALSE.  If it represents an existing channel it is kept intact and the
 * function returns %TRUE.
 *
 * Returns: Whether @id refers to an existing channel now.
 *
 * Since: 2.41
 **/
gboolean
gwy_app_data_id_verify_channel(GwyAppDataId *id)
{
    GwyContainer *container;
    GObject *object;
    GQuark quark;

    g_return_val_if_fail(id, FALSE);

    container = gwy_app_data_browser_get(id->datano);
    if (!container)
        return clear_data_id(id);

    quark = gwy_app_get_data_key_for_id(id->id);
    if (!gwy_container_gis_object(container, quark, &object))
        return clear_data_id(id);
    if (!GWY_IS_DATA_FIELD(object))
        return clear_data_id(id);

    return TRUE;
}

/**
 * gwy_app_data_id_verify_graph:
 * @id: Numerical identifiers of a graph in data managed by the data browser.
 *
 * Checks if numerical graph identifiers correspond to an existing graph.
 *
 * If either the data contained referenced in @id or the graph model does not
 * exist the structure is cleared to %GWY_APP_DATA_ID_NONE and the function
 * returns %FALSE.  If it represents an existing graph it is kept intact and
 * the function returns %TRUE.
 *
 * Returns: Whether @id refers to an existing graph now.
 *
 * Since: 2.41
 **/
gboolean
gwy_app_data_id_verify_graph(GwyAppDataId *id)
{
    GwyContainer *container;
    GObject *object;
    GQuark quark;

    g_return_val_if_fail(id, FALSE);

    container = gwy_app_data_browser_get(id->datano);
    if (!container)
        return clear_data_id(id);

    quark = gwy_app_get_graph_key_for_id(id->id);
    if (!gwy_container_gis_object(container, quark, &object))
        return clear_data_id(id);
    if (!GWY_IS_GRAPH_MODEL(object))
        return clear_data_id(id);

    return TRUE;
}

/**
 * gwy_app_data_id_verify_volume:
 * @id: Numerical identifiers of volume data in data managed by the data
 *      browser.
 *
 * Checks if numerical volume data identifiers correspond to existing volume
 * data.
 *
 * If either the data contained referenced in @id or the volume data does not
 * exist the structure is cleared to %GWY_APP_DATA_ID_NONE and the function
 * returns %FALSE.  If it represents existing volume data it is kept intact
 * and the function returns %TRUE.
 *
 * Returns: Whether @id refers to existing volume data now.
 *
 * Since: 2.41
 **/
gboolean
gwy_app_data_id_verify_volume(GwyAppDataId *id)
{
    GwyContainer *container;
    GObject *object;
    GQuark quark;

    g_return_val_if_fail(id, FALSE);

    container = gwy_app_data_browser_get(id->datano);
    if (!container)
        return clear_data_id(id);

    quark = gwy_app_get_brick_key_for_id(id->id);
    if (!gwy_container_gis_object(container, quark, &object))
        return clear_data_id(id);
    if (!GWY_IS_BRICK(object))
        return clear_data_id(id);

    return TRUE;
}

/**
 * gwy_app_data_id_verify_xyz:
 * @id: Numerical identifiers of XYZ data in data managed by the data
 *      browser.
 *
 * Checks if numerical XYZ data identifiers correspond to existing XYZ data.
 *
 * If either the data contained referenced in @id or the XYZ data does not
 * exist the structure is cleared to %GWY_APP_DATA_ID_NONE and the function
 * returns %FALSE.  If it represents existing XYZ data it is kept intact
 * and the function returns %TRUE.
 *
 * Returns: Whether @id refers to existing XYZ data now.
 *
 * Since: 2.46
 **/
gboolean
gwy_app_data_id_verify_xyz(GwyAppDataId *id)
{
    GwyContainer *container;
    GObject *object;
    GQuark quark;

    g_return_val_if_fail(id, FALSE);

    container = gwy_app_data_browser_get(id->datano);
    if (!container)
        return clear_data_id(id);

    quark = gwy_app_get_surface_key_for_id(id->id);
    if (!gwy_container_gis_object(container, quark, &object))
        return clear_data_id(id);
    if (!GWY_IS_SURFACE(object))
        return clear_data_id(id);

    return TRUE;
}

/**
 * gwy_app_data_id_verify_spectra:
 * @id: Numerical identifiers of spectra in data managed by the data browser.
 *
 * Checks if numerical spectra identifiers correspond to existing spectra.
 *
 * If either the data contained referenced in @id or the spectra does not
 * exist the structure is cleared to %GWY_APP_DATA_ID_NONE and the function
 * returns %FALSE.  If it represents existing spectra it is kept intact and
 * the function return %TRUE.
 *
 * Returns: Whether @id refers to existing spectra now.
 *
 * Since: 2.41
 **/
gboolean
gwy_app_data_id_verify_spectra(GwyAppDataId *id)
{
    GwyContainer *container;
    GObject *object;
    GQuark quark;

    g_return_val_if_fail(id, FALSE);

    container = gwy_app_data_browser_get(id->datano);
    if (!container)
        return clear_data_id(id);

    quark = gwy_app_get_spectra_key_for_id(id->id);
    if (!gwy_container_gis_object(container, quark, &object))
        return clear_data_id(id);
    if (!GWY_IS_SPECTRA(object))
        return clear_data_id(id);

    return TRUE;
}

/**
 * gwy_app_add_graph_or_curves:
 * @gmodel: A graph model with curves to add.
 * @data: Data container where the graph would be added.
 * @target_graph: Graph where curves would be added.
 * @colorstep: Curve block size as in gwy_graph_model_append_curves().
 *
 * Puts the curves of a graph to another graph if possible, or adds the graph
 * as new.
 *
 * If the units of @gmodel are compatible with the units of the graph
 * identified by @target_graph the curves are copied to the target graph with
 * gwy_graph_model_append_curves().
 *
 * In all other cases, including when @target_graph does not refer to any
 * existing graph, the graph model is added to @data as a new graph.
 *
 * Either way, the caller usually need to release its own reference afterwards.
 *
 * This function is useful particularly in modules that create graphs and can
 * be run non-interactively.
 *
 * Returns: The numerical identifier of the newly-created graph of one was
 *          created.  Value -1 is returned if curves were added to
 *          @target_graph.
 *
 * Since: 2.41
 **/
gint
gwy_app_add_graph_or_curves(GwyGraphModel *gmodel,
                            GwyContainer *data,
                            const GwyAppDataId *target_graph,
                            gint colorstep)
{
    GwyAppDataId tgtgraph = *target_graph;

    if (gwy_app_data_id_verify_graph(&tgtgraph)) {
        GQuark quark = gwy_app_get_graph_key_for_id(tgtgraph.id);
        GwyContainer *data2 = gwy_app_data_browser_get(tgtgraph.datano);
        GwyGraphModel *target_gmodel = gwy_container_get_object(data2, quark);

        g_return_val_if_fail(GWY_IS_GRAPH_MODEL(target_gmodel), -1);
        if (gwy_graph_model_units_are_compatible(gmodel, target_gmodel)) {
            gwy_graph_model_append_curves(target_gmodel, gmodel, colorstep);
            return -1;
        }
    }
    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
    return gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
}

/**
 * gwy_preview_surface_to_datafield:
 * @surface: A surface representing a XYZ data.
 * @dfield: A data field to fill with @surface preview.
 * @max_xres: Maximum width of the preview, it must be at least 2.
 * @max_yres: Maximum height of the preview, it must be at least 2.
 * @flags: Flags modifying the behaviour.
 *
 * Renders a preview of a XYZ data surface to a data field.
 *
 * Since: 2.46
 **/
void
gwy_preview_surface_to_datafield(GwySurface *surface,
                                 GwyDataField *dfield,
                                 gint max_xres, gint max_yres,
                                 GwyPreviewSurfaceFlags flags)
{
    gdouble q, h, xmin, xmax, ymin, ymax;
    guint n = gwy_surface_get_npoints(surface);
    gboolean do_fill = (flags & GWY_PREVIEW_SURFACE_FILL);
    gboolean want_densitymap = (flags & GWY_PREVIEW_SURFACE_DENSITY);
    gboolean xrange_normal, yrange_normal;
    GwyDataField *densitymap = NULL;
    guint xres, yres;

    gwy_surface_get_xrange(surface, &xmin, &xmax);
    xrange_normal = (xmin < xmax);

    gwy_surface_get_yrange(surface, &ymin, &ymax);
    yrange_normal = (ymin < ymax);

    if (!xrange_normal) {
        if (yrange_normal) {
            xmin = xmax - (ymax - ymin)/n;
            xmax = xmax + (ymax - ymin)/n;
        }
        else if (xmax) {
            xmin = 1.5*xmax;
            xmax = 0.5*xmax;
        }
        else {
            xmin = -1.0;
            xmax = 1.0;
        }
    }

    if (!yrange_normal) {
        if (xrange_normal) {
            ymin = ymax - (xmax - xmin)/n;
            ymax = ymax + (xmax - xmin)/n;
        }
        else if (ymax) {
            ymin = 0.5*ymax;
            ymax = 1.5*ymax;
        }
        else {
            ymin = -1.0;
            ymax = 1.0;
        }
    }

    if (do_fill) {
        h = fmax((xmax - xmin)/(max_xres - 1.0),
                 (ymax - ymin)/(max_yres - 1.0));
        xmin -= 0.5*h;
        ymin -= 0.5*h;
        xmax += 0.5*h;
        ymax += 0.5*h;
        xres = GWY_ROUND((xmax - xmin)/h);
        xres = CLAMP(xres, 2, max_xres);
        yres = GWY_ROUND((ymax - ymin)/h);
        yres = CLAMP(yres, 2, max_yres);
    }
    else {
        q = (ymax - ymin)/(xmax - xmin);
        if (q <= 1.0) {
            yres = GWY_ROUND(sqrt(3.0*q*n));
            yres = MAX(yres, 2);
            h = (ymax - ymin)/yres;
            xres = GWY_ROUND((xmax - xmin)/h);
            if (CLAMP(xres, 2, max_xres) != xres) {
                xres = CLAMP(xres, 2, max_xres);
                h = (xmax - xmin)/xres;
                yres = (gint)ceil((ymax - ymin)/h);
            }
        }
        else {
            xres = GWY_ROUND(sqrt(3.0/q*n));
            xres = MAX(xres, 2);
            h = (xmax - xmin)/xres;
            yres = GWY_ROUND((ymax - ymin)/h);
            if (CLAMP(yres, 2, max_yres) != yres) {
                yres = CLAMP(yres, 2, max_yres);
                h = (ymax - ymin)/yres;
                xres = (gint)ceil((xmax - xmin)/h);
            }
        }

        xmin -= 0.5*h;
        ymin -= 0.5*h;
        xmax += 0.5*h;
        ymax += 0.5*h;
        if ((xmax - xmin)/xres < (ymax - ymin)/yres) {
            gdouble excess = (ymax - ymin)/yres*xres - (xmax - xmin);
            xmin -= 0.5*excess;
            xmax += 0.5*excess;
        }
        else {
            gdouble excess = (xmax - xmin)/xres*yres - (ymax - ymin);
            ymin -= 0.5*excess;
            ymax += 0.5*excess;
        }
    }

    gwy_data_field_resample(dfield, xres, yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_set_xreal(dfield, xmax - xmin);
    gwy_data_field_set_yreal(dfield, ymax - ymin);
    gwy_data_field_set_xoffset(dfield, xmin);
    gwy_data_field_set_yoffset(dfield, ymin);
    if (want_densitymap)
        densitymap = gwy_data_field_new_alike(dfield, FALSE);

    gwy_data_field_average_xyz(dfield, densitymap,
                               gwy_surface_get_data_const(surface), n);
    gwy_serializable_clone(G_OBJECT(gwy_surface_get_si_unit_xy(surface)),
                           G_OBJECT(gwy_data_field_get_si_unit_xy(dfield)));

    if (want_densitymap) {
        gwy_data_field_copy(densitymap, dfield, FALSE);
        GWY_OBJECT_UNREF(densitymap);
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), NULL);
    }
    else {
        gwy_serializable_clone(G_OBJECT(gwy_surface_get_si_unit_z(surface)),
                               G_OBJECT(gwy_data_field_get_si_unit_z(dfield)));
    }
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymoduleutils
 * @title: module utils
 * @short_description: Module utility functions
 * @include: app/gwymoduleutils.h
 **/

/**
 * GwySaveAuxiliaryCreate:
 * @user_data: The data passed to gwy_save_auxiliary_with_callback() as
 *             @user_data.
 * @data_len: The length of the returned data in bytes.  Leaving it unset has
 *            the same effect as setting it to a negative value.  See
 *            gwy_save_auxiliary_data() for details.
 *
 * The type of auxiliary saved data creation function.
 *
 * Returns: The data to save.  It must not return %NULL.
 *
 * Since: 2.3
 **/

/**
 * GwySaveAuxiliaryDestroy:
 * @data: The data returned by the corresponding #GwySaveAuxiliaryCreate
 *        function.
 * @user_data: The data passed to gwy_save_auxiliary_with_callback() as
 *             @user_data.
 *
 * The type of auxiliary saved data destruction function.
 *
 * Since: 2.3
 **/

/**
 * GwyPreviewSurfaceFlags:
 * @GWY_PREVIEW_SURFACE_DENSITY: Render a point density map instead of the
 *                               data.
 * @GWY_PREVIEW_SURFACE_FILL: Make the data field as large as the specified
 *                            resolutions at least in one dimension (it can
 *                            be prevented in the other by different aspect
 *                            ratio).
 *
 * Type of behaviour modifying flags that can be passed to
 * gwy_preview_surface_to_datafield().
 *
 * Since: 2.46
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
