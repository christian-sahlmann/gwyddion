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
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/app.h>
#include <app/settings.h>
#include <app/filelist.h>
#include <app/data-browser.h>
#include "gwyappinternal.h"

enum {
    TMS_NORMAL_THUMB_SIZE = 128
};

enum {
    COLUMN_FILETYPE,
    COLUMN_LABEL
};

enum {
    COLUMN_FILEINFO,
    COLUMN_PIXBUF
};

typedef struct {
    GSList *list;
    GwyFileOperationType fileop;
} TypeListData;

typedef struct {
    GSList *channels;
    guint nchannels;
    GSList *graphs;
    guint ngraphs;
    GSList *spectra;
    guint nspectra;
} FileInfoData;

static void       gwy_app_file_chooser_finalize       (GObject *object);
static void       gwy_app_file_chooser_destroy        (GtkObject *object);
static void       gwy_app_file_chooser_hide           (GtkWidget *widget);
static void       gwy_app_file_chooser_setup_filter   (GwyAppFileChooser *chooser);
static void       gwy_app_file_chooser_save_position  (GwyAppFileChooser *chooser);
static void       gwy_app_file_chooser_add_type       (const gchar *name,
                                                       TypeListData *data);
static gint       gwy_app_file_chooser_type_compare   (gconstpointer a,
                                                       gconstpointer b);
static void       gwy_app_file_chooser_add_types      (GtkListStore *store,
                                                       GwyFileOperationType fileop);
static void       gwy_app_file_chooser_add_type_list  (GwyAppFileChooser *chooser);
static void       gwy_app_file_chooser_update_expander(GwyAppFileChooser *chooser);
static void       gwy_app_file_chooser_type_changed   (GwyAppFileChooser *chooser,
                                                       GtkTreeSelection *selection);
static void       gwy_app_file_chooser_filter_toggled (GwyAppFileChooser *chooser,
                                                       GtkToggleButton *check);
static void       gwy_app_file_chooser_expanded       (GwyAppFileChooser *chooser,
                                                       GParamSpec *pspec,
                                                       GtkExpander *expander);
static gboolean   gwy_app_file_chooser_open_filter    (const GtkFileFilterInfo *filter_info,
                                                       gpointer userdata);
static void       gwy_app_file_chooser_add_preview    (GwyAppFileChooser *chooser);
static void       gwy_app_file_chooser_update_preview (GwyAppFileChooser *chooser);
static gboolean   gwy_app_file_chooser_do_full_preview(gpointer user_data);
static void       gwy_app_file_chooser_free_preview   (GwyAppFileChooser *chooser);

G_DEFINE_TYPE(GwyAppFileChooser, _gwy_app_file_chooser,
              GTK_TYPE_FILE_CHOOSER_DIALOG)

static GtkWidget *instance_open = NULL;
static GtkWidget *instance_save = NULL;

static void
_gwy_app_file_chooser_class_init(GwyAppFileChooserClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gobject_class->finalize = gwy_app_file_chooser_finalize;

    object_class->destroy = gwy_app_file_chooser_destroy;

    widget_class->hide = gwy_app_file_chooser_hide;
}

static void
_gwy_app_file_chooser_init(GwyAppFileChooser *chooser)
{
    chooser->filter = gtk_file_filter_new();
    g_object_ref(chooser->filter);
    gtk_object_sink(GTK_OBJECT(chooser->filter));

    chooser->no_filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(chooser->no_filter, "*");
    g_object_ref(chooser->no_filter);
    gtk_object_sink(GTK_OBJECT(chooser->no_filter));
}

static void
gwy_app_file_chooser_finalize(GObject *object)
{
    GwyAppFileChooser *chooser = GWY_APP_FILE_CHOOSER(object);

    g_object_unref(chooser->filter);

    G_OBJECT_CLASS(_gwy_app_file_chooser_parent_class)->finalize(object);
}

static void
gwy_app_file_chooser_destroy(GtkObject *object)
{
    GwyAppFileChooser *chooser = GWY_APP_FILE_CHOOSER(object);

    gwy_app_file_chooser_free_preview(chooser);

    GTK_OBJECT_CLASS(_gwy_app_file_chooser_parent_class)->destroy(object);
}

static void
gwy_app_file_chooser_hide(GtkWidget *widget)
{
    GwyAppFileChooser *chooser = GWY_APP_FILE_CHOOSER(widget);

    gwy_app_file_chooser_free_preview(chooser);

    GTK_WIDGET_CLASS(_gwy_app_file_chooser_parent_class)->hide(widget);
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
_gwy_app_file_chooser_get(GtkFileChooserAction action)
{
    GtkDialog *dialog;
    GwyAppFileChooser *chooser;
    GtkWidget **instance;
    const gchar *title;

    switch (action) {
        case GTK_FILE_CHOOSER_ACTION_OPEN:
        instance = &instance_open;
        title = _("Open File");
        break;

        case GTK_FILE_CHOOSER_ACTION_SAVE:
        instance = &instance_save;
        title = _("Save File");
        break;

        default:
        g_return_val_if_reached(NULL);
        break;
    }

    if (*instance)
        return *instance;

    chooser = g_object_new(GWY_TYPE_APP_FILE_CHOOSER,
                           "title", title,
                           "action", action,
                           NULL);
    *instance = GTK_WIDGET(chooser);
    dialog = GTK_DIALOG(chooser);
    g_object_add_weak_pointer(G_OBJECT(chooser), (gpointer*)instance);

    gtk_dialog_add_button(dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    switch (action) {
        case GTK_FILE_CHOOSER_ACTION_OPEN:
        chooser->prefix = "/app/file/load";
        gtk_dialog_add_button(dialog, GTK_STOCK_OPEN, GTK_RESPONSE_OK);
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(chooser), TRUE);
        break;

        case GTK_FILE_CHOOSER_ACTION_SAVE:
        chooser->prefix = "/app/file/save";
        gtk_dialog_add_button(dialog, GTK_STOCK_SAVE, GTK_RESPONSE_OK);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);

    gwy_app_file_chooser_setup_filter(chooser);
    gwy_app_file_chooser_add_type_list(chooser);
    gwy_app_file_chooser_add_preview(chooser);

    g_signal_connect(chooser, "response",
                     G_CALLBACK(gwy_app_file_chooser_save_position), NULL);
    gwy_app_restore_window_position(GTK_WINDOW(chooser), chooser->prefix, TRUE);

    return *instance;
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
        /* Nothing? */
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
gwy_app_file_chooser_save_position(GwyAppFileChooser *chooser)
{
    gwy_app_save_window_position(GTK_WINDOW(chooser), chooser->prefix,
                                 FALSE, TRUE);
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
    GtkTreePath *path;
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
            path = gtk_tree_model_get_path(model, &iter);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(chooser->type_list),
                                         path, NULL, TRUE, 0.5, 0.0);
            gtk_tree_path_free(path);
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
gwy_app_file_chooser_update_expander(GwyAppFileChooser *chooser)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *name, *label;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(chooser->type_list));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        name = g_strdup("???");
    else
        gtk_tree_model_get(model, &iter, COLUMN_LABEL, &name, -1);

    if (chooser->filter_enable
        && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chooser->filter_enable)))
        label = g_strdup_printf(_("File _type: %s, filtered"), name);
    else
        label = g_strdup_printf(_("File _type: %s"), name);
    g_free(name);

    gtk_expander_set_label(GTK_EXPANDER(chooser->expander), label);
    g_free(label);
}

static void
gwy_app_file_chooser_type_changed(GwyAppFileChooser *chooser,
                                  GtkTreeSelection *selection)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean active = FALSE;
    gchar *key;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;
    g_free(chooser->filetype);
    gtk_tree_model_get(model, &iter, COLUMN_FILETYPE, &chooser->filetype, -1);
    gwy_app_file_chooser_update_expander(chooser);

    /* XXX: Reset filter and set it again.  There is no way to notify the
     * file chooser dialog the filter has changed. */
    key = g_strconcat(chooser->prefix, "/filter", NULL);
    gwy_container_gis_boolean_by_name(gwy_app_settings_get(), key, &active);
    g_free(key);

    if (active) {
        gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(chooser),
                                    chooser->no_filter);
        gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(chooser),
                                    chooser->filter);
    }
}

static void
gwy_app_file_chooser_filter_toggled(GwyAppFileChooser *chooser,
                                    GtkToggleButton *check)
{
    gboolean active;
    gchar *key;

    active = gtk_toggle_button_get_active(check);
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(chooser),
                                active ? chooser->filter : chooser->no_filter);
    key = g_strconcat(chooser->prefix, "/filter", NULL);
    gwy_container_set_boolean_by_name(gwy_app_settings_get(), key, active);
    g_free(key);
    gwy_app_file_chooser_update_expander(chooser);
}

static void
gwy_app_file_chooser_expanded(GwyAppFileChooser *chooser,
                              G_GNUC_UNUSED GParamSpec *pspec,
                              GtkExpander *expander)
{
    gchar *key;

    key = g_strconcat(chooser->prefix, "/expanded", NULL);
    gwy_container_set_boolean_by_name(gwy_app_settings_get(), key,
                                      gtk_expander_get_expanded(expander));
    g_free(key);
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
    gboolean expanded = FALSE;
    gboolean filter = FALSE;
    gchar *key;

    g_object_get(chooser, "action", &action, NULL);

    key = g_strconcat(chooser->prefix, "/type", NULL);
    chooser->type_key = g_quark_from_string(key);
    g_free(key);

    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    gtk_list_store_append(store, &iter);
    switch (action) {
        case GTK_FILE_CHOOSER_ACTION_SAVE:
        gtk_list_store_set(store, &iter,
                           COLUMN_FILETYPE, "",
                           COLUMN_LABEL, _("Automatic by extension"),
                           -1);
        gwy_app_file_chooser_add_types(store, GWY_FILE_OPERATION_SAVE);
        gwy_app_file_chooser_add_types(store, GWY_FILE_OPERATION_EXPORT);
        break;

        case GTK_FILE_CHOOSER_ACTION_OPEN:
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
    key = g_strconcat(chooser->prefix, "/expanded", NULL);
    gwy_container_gis_boolean_by_name(gwy_app_settings_get(), key, &expanded);
    g_free(key);
    gtk_expander_set_expanded(GTK_EXPANDER(chooser->expander), expanded);
    g_signal_connect_swapped(chooser->expander, "notify::expanded",
                             G_CALLBACK(gwy_app_file_chooser_expanded),
                             chooser);

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
        key = g_strconcat(chooser->prefix, "/filter", NULL);
        gwy_container_gis_boolean_by_name(gwy_app_settings_get(), key, &filter);
        g_free(key);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chooser->filter_enable),
                                     filter);
        gtk_box_pack_start(GTK_BOX(vbox), chooser->filter_enable,
                           FALSE, FALSE, 0);
        g_signal_connect_swapped(chooser->filter_enable, "toggled",
                                 G_CALLBACK(gwy_app_file_chooser_filter_toggled),
                                 chooser);
        gwy_app_file_chooser_filter_toggled(chooser,
                                            GTK_TOGGLE_BUTTON(chooser->filter_enable));
    }

    /* Give it some reasonable size. FIXME: hack. */
    gtk_widget_show_all(vbox);
    gtk_widget_size_request(scwin, &req);
    gtk_widget_set_size_request(scwin, -1, 3*req.height + 20);

    gwy_app_file_chooser_select_type(chooser);
    gwy_app_file_chooser_type_changed(chooser, selection);
}

static gboolean
gwy_app_file_chooser_open_filter(const GtkFileFilterInfo *filter_info,
                                 gpointer userdata)
{
    GwyAppFileChooser *chooser;
    const gchar *name;
    gint score;

    chooser = GWY_APP_FILE_CHOOSER(userdata);
    if (chooser->filetype && *chooser->filetype)
        return gwy_file_func_run_detect(chooser->filetype,
                                        filter_info->filename,
                                        FALSE);

    name = gwy_file_detect_with_score(filter_info->filename,
                                      FALSE,
                                      GWY_FILE_OPERATION_LOAD,
                                      &score);
    /* To filter out `fallback' importers like rawfile */
    return name != NULL && score >= 5;
}

/***** Preview *************************************************************/

gboolean
_gwy_app_file_chooser_get_previewed_data(GwyAppFileChooser *chooser,
                                         GwyContainer **data,
                                         gchar **filename_utf8,
                                         gchar **filename_sys)
{
    g_return_val_if_fail(GWY_IS_APP_FILE_CHOOSER(chooser), FALSE);

    *data = NULL;
    *filename_utf8 = NULL;
    *filename_sys = NULL;
    if (!chooser->preview_data || !chooser->preview_name_sys)
        return FALSE;

    *data = chooser->preview_data;
    *filename_sys = g_strdup(chooser->preview_name_sys);
    *filename_utf8 = g_filename_to_utf8(chooser->preview_name_sys, -1,
                                        NULL, NULL, NULL);

    return TRUE;
}

static void
gwy_app_file_chooser_add_preview(GwyAppFileChooser *chooser)
{
    GtkListStore *store;
    GtkIconView *preview;
    GtkCellLayout *layout;
    GtkCellRenderer *renderer;
    GtkWidget *scwin, *vbox;
    gint w;

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    store = gtk_list_store_new(2, G_TYPE_STRING, GDK_TYPE_PIXBUF);
    chooser->preview = gtk_icon_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    preview = GTK_ICON_VIEW(chooser->preview);
    layout = GTK_CELL_LAYOUT(preview);
    gtk_icon_view_set_columns(preview, 1);

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(preview), renderer, FALSE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(preview), renderer,
                                  "pixbuf", COLUMN_PIXBUF);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
                 "wrap-mode", PANGO_WRAP_WORD_CHAR,
                 "ellipsize", PANGO_ELLIPSIZE_END,
                 "ellipsize-set", TRUE,
                 NULL);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(preview), renderer, FALSE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(preview), renderer,
                                  "markup", COLUMN_FILEINFO);
    chooser->renderer_fileinfo = G_OBJECT(renderer);

    gtk_icon_view_set_selection_mode(preview, GTK_SELECTION_NONE);
    gtk_icon_view_set_item_width(preview, TMS_NORMAL_THUMB_SIZE);
    w = TMS_NORMAL_THUMB_SIZE + 2*gtk_icon_view_get_margin(preview);
    gtk_widget_set_size_request(chooser->preview, w, -1);
    gtk_container_add(GTK_CONTAINER(scwin), chooser->preview);

    vbox = gtk_vbox_new(FALSE, 2);

    chooser->preview_filename = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(chooser->preview_filename), 0.0, 0.5);
    gtk_label_set_single_line_mode(GTK_LABEL(chooser->preview_filename), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(chooser->preview_filename),
                            PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(vbox), chooser->preview_filename,
                       FALSE, FALSE, 0);

    chooser->preview_type = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(chooser->preview_type), 0.0, 0.5);
    gtk_label_set_single_line_mode(GTK_LABEL(chooser->preview_type), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(chooser->preview_type),
                            PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(vbox), chooser->preview_type,
                       FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), scwin, TRUE, TRUE, 0);
    gtk_widget_show_all(vbox);

    gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(chooser), vbox);
    gtk_file_chooser_set_use_preview_label(GTK_FILE_CHOOSER(chooser), FALSE);
    g_signal_connect(chooser, "update-preview",
                     G_CALLBACK(gwy_app_file_chooser_update_preview), NULL);
}

static void
gwy_app_file_chooser_update_preview(GwyAppFileChooser *chooser)
{
    GtkFileChooser *fchooser;
    GtkTreeModel *model;
    GdkPixbuf *pixbuf;
    GtkTreeIter iter;
    gchar *filename_sys, *basename_sys, *filename_utf8;

    gwy_app_file_chooser_free_preview(chooser);

    model = gtk_icon_view_get_model(GTK_ICON_VIEW(chooser->preview));
    gtk_list_store_clear(GTK_LIST_STORE(model));

    fchooser = GTK_FILE_CHOOSER(chooser);
    filename_sys = gtk_file_chooser_get_preview_filename(fchooser);
    /* It should be UTF-8, but don't convert it just for gwy_debug() */
    gwy_debug("%s", filename_sys);

    /* Never set the preview inactive.  Gtk+ can do all kinds of silly things
     * if you do. */
    if (!filename_sys) {
        gtk_label_set_text(GTK_LABEL(chooser->preview_filename), "");
        gtk_label_set_text(GTK_LABEL(chooser->preview_type), "");
        return;
    }

    /* Preview file name */
    basename_sys = g_path_get_basename(filename_sys);
    filename_utf8 = g_filename_to_utf8(basename_sys, -1, NULL, NULL, NULL);
    g_free(basename_sys);
    if (!filename_utf8)
        filename_utf8 = g_strdup("???");
    gtk_label_set_text(GTK_LABEL(chooser->preview_filename), filename_utf8);
    g_free(filename_utf8);

    /* Let directories fail gracefully */
    if (g_file_test(filename_sys, G_FILE_TEST_IS_DIR)) {
        gtk_label_set_markup(GTK_LABEL(chooser->preview_type),
                             "<small>directory</small>");
        g_free(filename_sys);
        return;
    }
    gtk_label_set_text(GTK_LABEL(chooser->preview_type), "");

    pixbuf = _gwy_app_recent_file_try_thumbnail(filename_sys);
    g_free(filename_sys);

    if (!pixbuf) {
        pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
        gdk_pixbuf_fill(pixbuf, 0x00000000);
        chooser->make_thumbnail = TRUE;
    }
    else
        chooser->make_thumbnail = FALSE;

    g_object_set(chooser->renderer_fileinfo,
                 "ellipsize", PANGO_ELLIPSIZE_NONE,
                 "wrap-width", TMS_NORMAL_THUMB_SIZE,
                 NULL);
    gtk_list_store_insert_with_values(GTK_LIST_STORE(model), &iter, -1,
                                      COLUMN_PIXBUF, pixbuf,
                                      COLUMN_FILEINFO, _("…"),
                                      -1);
    g_object_unref(pixbuf);

    chooser->full_preview_id
        = g_timeout_add_full(G_PRIORITY_LOW, 250,
                             gwy_app_file_chooser_do_full_preview, chooser,
                             NULL);
}

static void
add_object_id(gpointer hkey,
              gpointer hvalue,
              gpointer user_data)
{
    GValue *value = (GValue*)hvalue;
    FileInfoData *filedata = (FileInfoData*)user_data;
    const gchar *strkey;
    gchar *end;
    gint id;

    strkey = g_quark_to_string(GPOINTER_TO_UINT(hkey));
    if (!strkey || strkey[0] != '/' || !G_VALUE_HOLDS_OBJECT(value))
        return;

    if ((id = strtol(strkey + 1, &end, 10)) >= 0
        && gwy_strequal(end, "/data")
        && GWY_IS_DATA_FIELD(g_value_get_object(value))) {
        filedata->nchannels++;
        filedata->channels = g_slist_prepend(filedata->channels,
                                             GINT_TO_POINTER(id));
        return;
    }

    if (g_str_has_prefix(strkey, "/0/graph/graph/")
        && (id = strtol(strkey + 15, &end, 10)) >= 0
        && !*end
        && GWY_IS_GRAPH_MODEL(g_value_get_object(value))) {
        filedata->ngraphs++;
        filedata->graphs = g_slist_prepend(filedata->graphs,
                                           GINT_TO_POINTER(id));
        return;
    }

    if (g_str_has_prefix(strkey, "/sps/")
        && (id = strtol(strkey + 5, &end, 10)) >= 0
        && !*end
        && GWY_IS_SPECTRA(g_value_get_object(value))) {
        filedata->nspectra++;
        filedata->spectra = g_slist_prepend(filedata->spectra,
                                            GINT_TO_POINTER(id));
        return;
    }
}

static gint
compare_ids(gconstpointer a, gconstpointer b)
{
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

static void
gwy_app_file_chooser_describe_channel(GwyContainer *container,
                                      gint id,
                                      GString *str)
{
    GwyDataField *dfield;
    GwySIUnit *siunit;
    GwySIValueFormat *vf;
    GQuark quark;
    gint xres, yres;
    gdouble xreal, yreal;
    gchar *s;

    g_string_truncate(str, 0);

    quark = gwy_app_get_data_key_for_id(id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(container, quark));
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));

    s = gwy_app_get_data_field_title(container, id);
    g_string_append(str, s);
    g_free(s);

    siunit = gwy_data_field_get_si_unit_z(dfield);
    s = gwy_si_unit_get_string(siunit, GWY_SI_UNIT_FORMAT_MARKUP);
    g_string_append_printf(str, " [%s]\n", s);
    g_free(s);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    g_string_append_printf(str, "%d×%d px\n", xres, yres);

    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    siunit = gwy_data_field_get_si_unit_xy(dfield);
    vf = gwy_si_unit_get_format(siunit, GWY_SI_UNIT_FORMAT_VFMARKUP,
                                sqrt(xreal*yreal), NULL);
    g_string_append_printf(str, "%.*f×%.*f%s%s",
                          vf->precision, xreal/vf->magnitude,
                          vf->precision, yreal/vf->magnitude,
                          (vf->units && *vf->units) ? " " : "", vf->units);
    gwy_si_unit_value_format_free(vf);
}

static gboolean
gwy_app_file_chooser_do_full_preview(gpointer user_data)
{
    GtkFileChooser *fchooser;
    GtkTreeModel *model;
    GtkListStore *store;
    GwyAppFileChooser *chooser;
    FileInfoData filedata;
    GdkPixbuf *pixbuf;
    GtkTreeIter iter;
    const gchar *name;
    GString *str;
    GSList *l;
    gint id;

    chooser = GWY_APP_FILE_CHOOSER(user_data);
    chooser->full_preview_id = 0;

    /* Always no-op here? */
    gwy_app_file_chooser_free_preview(chooser);

    fchooser = GTK_FILE_CHOOSER(chooser);
    chooser->preview_name_sys = gtk_file_chooser_get_preview_filename(fchooser);
    /* We should not be called when gtk_file_chooser_get_preview_filename()
     * returns NULL preview file name */
    if (!chooser->preview_name_sys) {
        g_warning("Full preview invoked with NULL preview file name");
        return FALSE;
    }

    model = gtk_icon_view_get_model(GTK_ICON_VIEW(chooser->preview));
    store = GTK_LIST_STORE(model);

    chooser->preview_data = gwy_file_load(chooser->preview_name_sys,
                                          GWY_RUN_NONINTERACTIVE, NULL);
    if (!chooser->preview_data) {
        gwy_app_file_chooser_free_preview(chooser);
        gtk_tree_model_get_iter_first(model, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_FILEINFO, _("Cannot preview"),
                           -1);

        return FALSE;
    }

    memset(&filedata, 0, sizeof(FileInfoData));
    gwy_container_foreach(chooser->preview_data, NULL,
                          add_object_id, &filedata);
    filedata.channels = g_slist_sort(filedata.channels, compare_ids);
    filedata.graphs = g_slist_sort(filedata.graphs, compare_ids);
    filedata.spectra = g_slist_sort(filedata.spectra, compare_ids);

    str = g_string_new(NULL);
    if (gwy_file_get_data_info(chooser->preview_data, &name, NULL)) {
        /* FIXME: Make this translatable */
        g_string_printf(str, "<small>%s", name);
        if (filedata.nchannels)
            g_string_append_printf(str, ", %d ch", filedata.nchannels);
        if (filedata.graphs)
            g_string_append_printf(str, ", %d gr", filedata.ngraphs);
        if (filedata.spectra)
            g_string_append_printf(str, ", %d sps", filedata.nspectra);
        g_string_append(str, "</small>");
        gtk_label_set_markup(GTK_LABEL(chooser->preview_type), str->str);
    }

    if (!filedata.channels) {
        g_string_free(str, TRUE);
        g_slist_free(filedata.channels);
        g_slist_free(filedata.graphs);
        g_slist_free(filedata.spectra);
        gwy_app_file_chooser_free_preview(chooser);
        return FALSE;
    }

    g_object_set(chooser->renderer_fileinfo,
                 "ellipsize", PANGO_ELLIPSIZE_END,
                 "wrap-width", -1,
                 NULL);

    gtk_list_store_clear(store);
    for (l = filedata.channels; l; l = g_slist_next(l)) {
        id = GPOINTER_TO_INT(l->data);
        pixbuf = gwy_app_get_channel_thumbnail(chooser->preview_data, id,
                                               TMS_NORMAL_THUMB_SIZE,
                                               TMS_NORMAL_THUMB_SIZE);
        if (!pixbuf) {
            g_warning("Cannot make a pixbuf of channel %d", id);
            continue;
        }

        if (chooser->make_thumbnail) {
            _gwy_app_recent_file_write_thumbnail(chooser->preview_name_sys,
                                                 chooser->preview_data,
                                                 id, pixbuf);
            chooser->make_thumbnail = FALSE;
        }

        gwy_app_file_chooser_describe_channel(chooser->preview_data, id, str);
        gtk_list_store_insert_with_values(store, &iter, -1,
                                          COLUMN_PIXBUF, pixbuf,
                                          COLUMN_FILEINFO, str->str,
                                          -1);
        g_object_unref(pixbuf);
    }

    g_slist_free(filedata.channels);
    g_slist_free(filedata.graphs);
    g_slist_free(filedata.spectra);
    g_string_free(str, TRUE);

    return FALSE;
}

static void
gwy_app_file_chooser_free_preview(GwyAppFileChooser *chooser)
{
    if (chooser->full_preview_id) {
        g_source_remove(chooser->full_preview_id);
        chooser->full_preview_id = 0;
    }

    if (chooser->preview_name_sys) {
        gwy_debug("freeing preview of <%s>", chooser->preview_name_sys);
    }
    g_free(chooser->preview_name_sys);
    chooser->preview_name_sys = NULL;

    gwy_object_unref(chooser->preview_data);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
