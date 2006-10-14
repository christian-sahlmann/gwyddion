/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
#include <stdarg.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/settings.h>
#include <app/gwyappfilechooser.h>

enum {
    COLUMN_FILETYPE,
    COLUMN_LABEL
};

typedef struct {
    GSList *list;
    GwyFileOperationType fileop;
} TypeListData;

static void       gwy_app_file_chooser_finalize      (GObject *object);
static GtkWidget* gwy_app_file_chooser_new_valist    (const gchar *title,
                                                      GtkWindow *parent,
                                                      GtkFileChooserAction action,
                                                      const gchar *backend,
                                                      const gchar *first_btn_text,
                                                      va_list varargs);
static void       gwy_app_file_chooser_setup_filter  (GwyAppFileChooser *chooser);
static gboolean   gwy_app_file_chooser_open_filter   (const GtkFileFilterInfo *filter_info,
                                                      gpointer data);
static gboolean   gwy_app_file_chooser_save_filter   (const GtkFileFilterInfo *filter_info,
                                                      gpointer data);
static void       gwy_app_file_chooser_add_type      (const gchar *name,
                                                      TypeListData *data);
static gint       gwy_app_file_chooser_type_compare  (gconstpointer a,
                                                      gconstpointer b);
static void       gwy_app_file_chooser_add_types     (GtkListStore *store,
                                                      GwyFileOperationType fileop);
static void       gwy_app_file_chooser_add_type_list (GwyAppFileChooser *chooser);
static void       gwy_app_file_chooser_type_changed  (GwyAppFileChooser *chooser,
                                                      GtkTreeSelection *selection);
static void       gwy_app_file_chooser_filter_toggled(GwyAppFileChooser *chooser,
                                                      GtkCheckButton *check);

G_DEFINE_TYPE(GwyAppFileChooser, _gwy_app_file_chooser,
              GTK_TYPE_FILE_CHOOSER_DIALOG)

static void
_gwy_app_file_chooser_class_init(GwyAppFileChooserClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_app_file_chooser_finalize;
}

static void
_gwy_app_file_chooser_init(GwyAppFileChooser *chooser)
{
    chooser->filter = gtk_file_filter_new();
    g_object_ref(chooser->filter);
    gtk_object_sink(GTK_OBJECT(chooser->filter));
}

static void
gwy_app_file_chooser_finalize(GObject *object)
{
    GwyAppFileChooser *chooser = GWY_APP_FILE_CHOOSER(object);

    g_object_unref(chooser->filter);

    if (G_OBJECT_CLASS(_gwy_app_file_chooser_parent_class)->finalize)
        G_OBJECT_CLASS(_gwy_app_file_chooser_parent_class)->finalize(object);
}

static void
gwy_app_file_chooser_add_type(const gchar *name,
                              TypeListData *data)
{
    if (!(gwy_file_func_get_operations(name) & data->fileop))
        return;

    data->list = g_slist_prepend(data->list, (gpointer)name);
}

static gint
gwy_app_file_chooser_type_compare(gconstpointer a,
                                  gconstpointer b)
{
    return g_utf8_collate(_(gwy_file_func_get_description((const gchar*)a)),
                          _(gwy_file_func_get_description((const gchar*)b)));
}

static void
gwy_app_file_chooser_add_types(GtkListStore *store,
                               GwyFileOperationType fileop)
{
    TypeListData tldata;
    GtkTreeIter iter;
    GSList *l;

    tldata.list = NULL;
    tldata.fileop = fileop;
    gwy_file_func_foreach((GFunc)gwy_app_file_chooser_add_type, &tldata);
    tldata.list = g_slist_sort(tldata.list, gwy_app_file_chooser_type_compare);

    for (l = tldata.list; l; l = g_slist_next(l)) {
        gtk_list_store_insert_with_values
                (store, &iter, G_MAXINT,
                 COLUMN_FILETYPE, l->data,
                 COLUMN_LABEL, gettext(gwy_file_func_get_description(l->data)),
                 -1);
    }

    g_slist_free(tldata.list);
}

GtkWidget*
_gwy_app_file_chooser_new(const gchar *title,
                          GtkWindow *parent,
                          GtkFileChooserAction action,
                          const gchar *first_btn_text,
                          ...)
{
    GtkWidget *chooser;
    va_list ap;

    va_start(ap, first_btn_text);
    chooser = gwy_app_file_chooser_new_valist(title, parent, action, NULL,
                                              first_btn_text, ap);
    va_end(ap);

    return chooser;
}

static GtkWidget*
gwy_app_file_chooser_new_valist(const gchar *title,
                                GtkWindow *parent,
                                GtkFileChooserAction action,
                                const gchar *backend,
                                const gchar *first_btn_text,
                                va_list ap)
{
    GtkWidget *chooser;
    const char *label;
    gint response;

    chooser = g_object_new(GWY_TYPE_APP_FILE_CHOOSER,
                           "title", title,
                           "action", action,
                           "file-system-backend", backend,
                           NULL);

    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(chooser), parent);

    for (label = first_btn_text; label; label = va_arg(ap, const gchar*)) {
        response = va_arg(ap, gint);
        gtk_dialog_add_button(GTK_DIALOG(chooser), label, response);
    }

    gwy_app_file_chooser_setup_filter(GWY_APP_FILE_CHOOSER(chooser));
    gwy_app_file_chooser_add_type_list(GWY_APP_FILE_CHOOSER(chooser));

    return chooser;
}

/* FIXME: This fails in init() with
 * g_object_get_property: assertion `G_IS_OBJECT (object)'
 * It seems the object is not initialized enough yet there */
static void
gwy_app_file_chooser_setup_filter(GwyAppFileChooser *chooser)
{
    GtkFileChooserAction action;

    g_object_get(chooser, "action", &action, NULL);
    switch (action) {
        case GTK_FILE_CHOOSER_ACTION_OPEN:
        gtk_file_filter_add_custom(chooser->filter,
                                   GTK_FILE_FILTER_FILENAME,
                                   gwy_app_file_chooser_open_filter,
                                   chooser,
                                   NULL);
        break;

        case GTK_FILE_CHOOSER_ACTION_SAVE:
        gtk_file_filter_add_custom(chooser->filter,
                                   GTK_FILE_FILTER_FILENAME,
                                   gwy_app_file_chooser_save_filter,
                                   chooser,
                                   NULL);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

/**
 * gwy_app_file_chooser_select_type:
 * @selector: File type selection widget.
 *
 * Selects the same file type as the last time.
 *
 * If no information about last selection is available or the type is not
 * present any more, the list item is selected.
 **/
static void
gwy_app_file_chooser_select_type(GwyAppFileChooser *chooser)
{
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreeIter iter, first;
    const guchar *name;
    gboolean ok;
    gchar *s;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(chooser->type_list));
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(chooser->type_list));

    if (!gtk_tree_model_get_iter_first(model, &first))
        return;

    ok = gwy_container_gis_string(gwy_app_settings_get(), chooser->type_key,
                                  &name);
    if (!ok) {
        gtk_tree_selection_select_iter(selection, &first);
        return;
    }

    iter = first;
    do {
        gtk_tree_model_get(model, &iter, COLUMN_FILETYPE, &s, -1);
        ok = gwy_strequal(name, s);
        g_free(s);
        if (ok) {
            gtk_tree_selection_select_iter(selection, &iter);
            return;
        }
    } while (gtk_tree_model_iter_next(model, &iter));

    gtk_tree_selection_select_iter(selection, &first);
}

gchar*
_gwy_app_file_chooser_get_selected_type(GwyAppFileChooser *chooser)
{
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    gchar *s;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(chooser->type_list));

    if (!(gtk_tree_selection_get_selected(selection, &model, &iter)))
        return NULL;
    gtk_tree_model_get(model, &iter, COLUMN_FILETYPE, &s, -1);
    if (!*s) {
        g_free(s);
        gwy_container_remove(gwy_app_settings_get(), chooser->type_key);
        s = NULL;
    }
    else
        gwy_container_set_string(gwy_app_settings_get(), chooser->type_key,
                                 g_strdup(s));

    return s;
}

static void
gwy_app_file_chooser_type_changed(GwyAppFileChooser *chooser,
                                  GtkTreeSelection *selection)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *label, *name;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;
    gtk_tree_model_get(model, &iter, COLUMN_LABEL, &name, -1);
    label = g_strdup_printf(_("File _type: %s"), name);
    g_free(name);
    gtk_expander_set_label(GTK_EXPANDER(chooser->expander), label);
    g_free(label);
}

static void
gwy_app_file_chooser_filter_toggled(GwyAppFileChooser *chooser,
                                    GtkCheckButton *check)
{
}

static void
gwy_app_file_chooser_add_type_list(GwyAppFileChooser *chooser)
{
    GtkWidget *vbox, *scwin;
    GtkTreeView *treeview;
    GtkFileChooserAction action;
    GtkRequisition req;
    GtkTreeSelection *selection;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkListStore *store;
    GtkTreeIter iter;

    g_object_get(chooser, "action", &action, NULL);

    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    gtk_list_store_append(store, &iter);
    switch (action) {
        case GTK_FILE_CHOOSER_ACTION_SAVE:
        chooser->type_key = g_quark_from_static_string("/app/file/save/type");
        gtk_list_store_set(store, &iter,
                           COLUMN_FILETYPE, "",
                           COLUMN_LABEL, _("Automatic by extension"),
                           -1);
        gwy_app_file_chooser_add_types(store, GWY_FILE_OPERATION_SAVE);
        gwy_app_file_chooser_add_types(store, GWY_FILE_OPERATION_EXPORT);
        break;

        case GTK_FILE_CHOOSER_ACTION_OPEN:
        chooser->type_key = g_quark_from_static_string("/app/file/load/type");
        gtk_list_store_set(store, &iter,
                           COLUMN_FILETYPE, "",
                           COLUMN_LABEL, _("Automatically detected"),
                           -1);
        gwy_app_file_chooser_add_types(store, GWY_FILE_OPERATION_LOAD);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    chooser->expander = gtk_expander_new(NULL);
    gtk_expander_set_use_underline(GTK_EXPANDER(chooser->expander), TRUE);
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(chooser),
                                      chooser->expander);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_container_add(GTK_CONTAINER(chooser->expander), vbox);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scwin, TRUE, TRUE, 0);

    chooser->type_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    treeview = GTK_TREE_VIEW(chooser->type_list);
    g_object_unref(store);
    gtk_tree_view_set_headers_visible(treeview, FALSE);
    gtk_container_add(GTK_CONTAINER(scwin), chooser->type_list);

    column = gtk_tree_view_column_new();
    gtk_tree_view_append_column(treeview, column);
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(column), renderer,
                                  "text", COLUMN_LABEL);

    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    g_signal_connect_swapped(selection, "changed",
                             G_CALLBACK(gwy_app_file_chooser_type_changed),
                             chooser);

    if (action == GTK_FILE_CHOOSER_ACTION_OPEN) {
        chooser->filter_enable
            = gtk_check_button_new_with_mnemonic(_("Show only loadable "
                                                   "files of selected type"));
        gtk_box_pack_start(GTK_BOX(vbox), chooser->filter_enable,
                           FALSE, FALSE, 0);
        g_signal_connect_swapped(chooser->filter_enable, "toggled",
                                 G_CALLBACK(gwy_app_file_chooser_filter_toggled),
                                 chooser);
    }

    /* Give it some reasonable size. FIXME: hack. */
    gtk_widget_show_all(vbox);
    gtk_widget_size_request(scwin, &req);
    gtk_widget_set_size_request(scwin, -1, 3*req.height + 20);

    gwy_app_file_chooser_select_type(chooser);
    gwy_app_file_chooser_type_changed(chooser, selection);
}

/* TODO */
static gboolean
gwy_app_file_chooser_open_filter(const GtkFileFilterInfo *filter_info,
                                 gpointer data)
{
    return TRUE;
}

/* TODO */
static gboolean
gwy_app_file_chooser_save_filter(const GtkFileFilterInfo *filter_info,
                                 gpointer data)
{
    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
