/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include "app.h"
#include "file.h"
#include "settings.h"

#define THUMBNAIL_SIZE 16

typedef enum {
    GWY_MASKCOR_OBJECTS,
    GWY_MASKCOR_MAXIMA,
    GWY_MASKCOR_SCORE,
    GWY_MASKCOR_LAST
} GwyMaskcorResult;

typedef struct {
    GwyMaskcorResult result;
    gdouble threshold;
    GwyDataWindow *win1;
    GwyDataWindow *win2;
} GwyMaskcorArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *result;
} GwyMaskcorControls;

static void       gwy_data_maskcor_load_args         (GwyContainer *settings,
                                                    GwyMaskcorArgs *args);
static void       gwy_data_maskcor_save_args         (GwyContainer *settings,
                                                    GwyMaskcorArgs *args);
static GtkWidget* gwy_data_maskcor_window_construct  (GwyMaskcorArgs *args);
static GtkWidget* gwy_data_maskcor_data_option_menu  (GtkWidget *entry,
                                                    GwyDataWindow **operand);
static void       gwy_data_maskcor_append_line       (GwyDataWindow *data_window,
                                                    GtkWidget *menu);
static void       gwy_data_maskcor_menu_set_history  (GtkWidget *omenu,
                                                    gpointer current);
static void       gwy_data_maskcor_operation_cb      (GtkWidget *item,
                                                    GwyMaskcorArgs *args);
static void       gwy_data_maskcor_data_cb           (GtkWidget *item);
static void       gwy_data_maskcor_entry_cb          (GtkWidget *entry,
                                                    gpointer data);
static gboolean   gwy_data_maskcor_do                (GwyMaskcorArgs *args,
                                                    GtkWidget *maskcor_window);


static const GwyEnum results[] = {
    { "Objects marked",       GWY_MASKCOR_OBJECTS },
    { "Correlation maxima",  GWY_MASKCOR_MAXIMA },
    { "Correlation score",  GWY_MASKCOR_SCORE },
};

static const GwyMaskcorArgs gwy_data_maskcor_defaults = {
    GWY_MASKCOR_OBJECTS, 0.95, NULL, NULL
};

void
gwy_app_data_maskcor(void)
{
    static GwyMaskcorArgs *args = NULL;
    static GtkWidget *maskcor_window = NULL;
    static gpointer win1 = NULL, win2 = NULL;
    GwyContainer *settings;
    gboolean ok = FALSE;

    if (!args) {
        args = g_new(GwyMaskcorArgs, 1);
        *args = gwy_data_maskcor_defaults;
    }
    settings = gwy_app_settings_get();
    if (!maskcor_window) {
        args->win1 = win1 ? win1 : gwy_app_data_window_get_current();
        args->win2 = win2 ? win2 : gwy_app_data_window_get_current();

        /* this may set win1, win2 back to NULL is operands are to be scalars */
        gwy_data_maskcor_load_args(settings, args);
        maskcor_window = gwy_data_maskcor_window_construct(args);
    }
    gtk_window_present(GTK_WINDOW(maskcor_window));
    do {
        switch (gtk_dialog_run(GTK_DIALOG(maskcor_window))) {
            case GTK_RESPONSE_CLOSE:
            case GTK_RESPONSE_DELETE_EVENT:
            case GTK_RESPONSE_NONE:
            ok = TRUE;
            break;

            case GTK_RESPONSE_APPLY:
            ok = gwy_data_maskcor_do(args, maskcor_window);
            if (ok) {
                gwy_data_maskcor_save_args(settings, args);
                if (win1)
                    g_object_remove_weak_pointer(G_OBJECT(win1), &win1);
                win1 = args->win1;
                if (win1)
                    g_object_add_weak_pointer(G_OBJECT(win1), &win1);
                if (win2)
                    g_object_remove_weak_pointer(G_OBJECT(win2), &win2);
                win2 = args->win2;
                if (win2)
                    g_object_add_weak_pointer(G_OBJECT(args->win2), &win2);
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    gtk_widget_destroy(maskcor_window);
    maskcor_window = NULL;
}

static GtkWidget*
gwy_data_maskcor_window_construct(GwyMaskcorArgs *args)
{
    GtkWidget *dialog, *table, *omenu, *entry, *label;
    gchar *text;

    dialog = gtk_dialog_new_with_buttons(_("Create mask by correlation"),
                                         GTK_WINDOW(gwy_app_main_window_get()),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         NULL);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 8);

    table = gtk_table_new(2, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);

    /***** First operand *****/
    label = gtk_label_new_with_mnemonic(_("_Data field to modify:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

    entry = gtk_entry_new();
    omenu = gwy_data_maskcor_data_option_menu(entry, &args->win1);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 0, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);


    /***** Second operand *****/
    label = gtk_label_new_with_mnemonic(_("_Correlation kernel"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);


    omenu = gwy_data_maskcor_data_option_menu(entry, &args->win2);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 1, 2);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);

    /**** Parameters ********/
    /*threshold*/
    label = gtk_label_new_with_mnemonic(_("T_hreshold"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);

    entry = gtk_entry_new();
    gtk_table_attach_defaults(GTK_TABLE(table), entry, 1, 2, 2, 3);
    gtk_entry_set_max_length(GTK_ENTRY(entry), 16);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 16);
    text = g_strdup_printf("%g", args->threshold);
    gtk_entry_set_text(GTK_ENTRY(entry), text);
    g_free(text);
    g_object_set_data(G_OBJECT(entry), "scalar", &args->threshold);
    g_signal_connect(entry, "changed",
                     G_CALLBACK(gwy_data_maskcor_entry_cb), NULL);

    
    /***** Result *****/
    label = gtk_label_new_with_mnemonic(_("_Result:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);

    omenu = gwy_option_menu_create(results, G_N_ELEMENTS(results),
                                   "operation",
                                   G_CALLBACK(gwy_data_maskcor_operation_cb),
                                   args,
                                   args->result);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, 3, 4);

    gtk_widget_show_all(dialog);

    return dialog;
}

GtkWidget*
gwy_data_maskcor_data_option_menu(GtkWidget *entry,
                                GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu, *item;

    omenu = gtk_option_menu_new();
    menu = gtk_menu_new();
    g_object_set_data(G_OBJECT(menu), "entry", entry);
    g_object_set_data(G_OBJECT(menu), "operand", operand);
    gwy_app_data_window_foreach((GFunc)gwy_data_maskcor_append_line, menu);
    item = gtk_menu_item_new_with_label(_("(null)"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
    gwy_data_maskcor_menu_set_history(omenu, *operand);
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_data_maskcor_data_cb), menu);

    return omenu;
}

static void
gwy_data_maskcor_append_line(GwyDataWindow *data_window,
                           GtkWidget *menu)
{
    GtkWidget *item, *data_view, *image;
    GdkPixbuf *pixbuf;
    gchar *filename;

    data_view = gwy_data_window_get_data_view(data_window);
    filename = gwy_data_window_get_base_name(data_window);

    pixbuf = gwy_data_view_get_thumbnail(GWY_DATA_VIEW(data_view),
                                         THUMBNAIL_SIZE);
    image = gtk_image_new_from_pixbuf(pixbuf);
    gwy_object_unref(pixbuf);
    item = gtk_image_menu_item_new_with_label(filename);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_data(G_OBJECT(item), "data-window", data_window);
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_data_maskcor_data_cb), menu);
    g_free(filename);
}

static void
gwy_data_maskcor_menu_set_history(GtkWidget *omenu,
                                gpointer current)
{
    GtkWidget *menu;
    GList *l;
    gpointer p;
    gint i;

    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    l = GTK_MENU_SHELL(menu)->children;
    i = 0;
    while (l) {
        p = g_object_get_data(G_OBJECT(l->data), "data-window");
        if (p == current) {
            gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), i);
            return;
        }
        l = g_list_next(l);
        i++;
    }
    g_warning("Cannot select data window %p", current);
}

static void
gwy_data_maskcor_operation_cb(GtkWidget *item, GwyMaskcorArgs *args)
{
    args->result
        = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "operation"));
}

static void
gwy_data_maskcor_data_cb(GtkWidget *item)
{
    GtkWidget *menu, *entry;
    gpointer p, *pp;

    menu = gtk_widget_get_parent(item);
    entry = GTK_WIDGET(g_object_get_data(G_OBJECT(menu), "entry"));

    p = g_object_get_data(G_OBJECT(item), "data-window");
    gtk_widget_set_sensitive(entry, p == NULL);
    pp = (gpointer*)g_object_get_data(G_OBJECT(menu), "operand");
    g_return_if_fail(pp);
    *pp = p;
}

static void
gwy_data_maskcor_entry_cb(GtkWidget *entry,
                        gpointer data)
{
    GtkEditable *editable;
    gint pos;
    gchar *s, *end;
    gdouble *scalar;

    scalar = (gdouble*)g_object_get_data(G_OBJECT(entry), "scalar");
    editable = GTK_EDITABLE(entry);
    /* validate whether it looks as a number of something like start of a
     * number */
    s = end = gtk_editable_get_chars(editable, 0, -1);
    for (pos = 0; s[pos]; pos++)
        s[pos] = g_ascii_tolower(s[pos]);
    *scalar = strtod(s, &end);
    if (*end == '-' && end == s)
        end++;
    else if (*end == 'e' && strchr(s, 'e') == end) {
        end++;
        if (*end == '-')
            end++;
    }
    /*gwy_debug("<%s> <%s>", s, end);*/
    if (!*end) {
        g_free(s);
        return;
    }

    g_signal_handlers_block_by_func(editable,
                                    G_CALLBACK(gwy_data_maskcor_entry_cb),
                                    data);
    gtk_editable_delete_text(editable, 0, -1);
    pos = 0;
    gtk_editable_insert_text(editable, s, end - s, &pos);
    g_signal_handlers_unblock_by_func(editable,
                                      G_CALLBACK(gwy_data_maskcor_entry_cb),
                                      data);
    g_free(s);
    g_signal_stop_emission_by_name(editable, "changed");
}

void 
plot_correlated(GwyDataField *retfield, gint xsize, gint ysize, gdouble threshold)
{
    GwyDataField *field;
    gint i, j, k;
    
    field = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(retfield)));
    gwy_data_field_fill(retfield, 0);

    for (i=0; i<retfield->xres; i++)
    {
        for (j=0; j<retfield->yres; j++)
        {
            if ((field->data[i + retfield->xres*j]) > threshold)
                gwy_data_field_area_fill(retfield, i-xsize/2, j-ysize/2, i+xsize/2, j+ysize/2, 1.0);
        }
    }
    
}

void 
plot_maxima(GwyDataField *retfield, gdouble threshold)
{
    GwyDataField *field;
    gint i, j, k;
    
    field = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(retfield)));
    gwy_data_field_fill(retfield, 0);

    for (i=0; i<retfield->xres; i++)
    {
        for (j=0; j<retfield->yres; j++)
        {
                retfield->data[i + retfield->xres*j] = 1;
        }
    }
    
}

static gboolean
gwy_data_maskcor_do(GwyMaskcorArgs *args,
                  GtkWidget *maskcor_window)
{
    GtkWidget *dialog, *data_window;
    GwyContainer *data, *ret, *kernel;
    GwyDataField *dfield, *kernelfield, *retfield;
    GwyDataWindow *operand1, *operand2;

    operand1 = args->win1;
    operand2 = args->win2;
    
    data = gwy_data_window_get_data(operand1);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                             "/0/data"));
    
    ret = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    retfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(ret,
                                                             "/0/data"));

    kernel = gwy_data_window_get_data(operand2);
    kernelfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(kernel, "/0/data"));

    gwy_data_field_correlate(dfield, kernelfield, retfield);
    
    /*score - do new data with score*/
    if (args->result == GWY_MASKCOR_SCORE)
    {
        data_window = gwy_app_data_window_create(ret);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    }
    else /*add mask*/
    {
        if (args->result == GWY_MASKCOR_OBJECTS)
        {
            plot_correlated(retfield, kernelfield->xres, kernelfield->yres, args->threshold);
        }
        else if (args->result == GWY_MASKCOR_MAXIMA)
        {
            plot_maxima(retfield, args->threshold);
        }
        gwy_container_set_object_by_name(data, "/0/mask", G_OBJECT(retfield));
    }
    gwy_app_data_view_update(GWY_DATA_VIEW(gwy_data_window_get_data_view(operand1)));
    
    return TRUE;
}


static const gchar *result_key = "/app/maskcor/result";
static const gchar *threshold_key = "/app/maskcor/threshold";
static const gchar *scalar_is1_key = "/app/maskcor/is1";
static const gchar *scalar_is2_key = "/app/maskcor/is2";

static void
gwy_data_maskcor_load_args(GwyContainer *settings,
                         GwyMaskcorArgs *args)
{
    gboolean b;

    gwy_container_gis_int32_by_name(settings, result_key, &args->result);
    gwy_container_gis_double_by_name(settings, threshold_key, &args->threshold);    
    gwy_container_gis_boolean_by_name(settings, scalar_is1_key, &b);
    if (b)
        args->win1 = NULL;
    gwy_container_gis_boolean_by_name(settings, scalar_is2_key, &b);
    if (b)
        args->win2 = NULL;
}

static void
gwy_data_maskcor_save_args(GwyContainer *settings,
                         GwyMaskcorArgs *args)
{
    gwy_container_set_int32_by_name(settings, result_key, args->result);
    gwy_container_set_double_by_name(settings, threshold_key, args->threshold);
    gwy_container_set_boolean_by_name(settings, scalar_is1_key,
                                      args->win1 == NULL);
    gwy_container_set_boolean_by_name(settings, scalar_is2_key,
                                      args->win2 == NULL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

