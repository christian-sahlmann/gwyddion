/* @(#) $Id$ */

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include "app.h"

static GtkWidget* gwy_data_arith_window_construct    (void);
static GtkWidget* gwy_data_arith_file_list_construct (void);
static void       gwy_data_arith_cell_renderer       (GtkTreeViewColumn *column,
                                                      GtkCellRenderer *cell,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *piter,
                                                      gpointer user_data);
static void       gwy_data_arith_add_line            (GwyDataWindow *data_window,
                                                      GtkListStore *store);

enum {
    ARITH_DATA_WINDOW,
    ARITH_FILENAME,
    ARITH_LAST
};

static GtkWidget *arith_window = NULL;

void
gwy_app_data_arith(void)
{
    if (!arith_window)
        arith_window = gwy_data_arith_window_construct();
    gtk_window_present(GTK_WINDOW(arith_window));

    /*return arith_window;*/
}

static GtkWidget*
gwy_data_arith_window_construct(void)
{
    GtkWidget *dialog, *hbox, *list;

    dialog = gtk_dialog_new_with_buttons(_("Data Arithmetic"),
                                         GTK_WINDOW(gwy_app_main_window),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         NULL);
    hbox = gtk_hbox_new(8, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);
    list = gwy_data_arith_file_list_construct();
    gtk_box_pack_start(GTK_BOX(hbox), list, TRUE, TRUE, 0);
    list = gwy_data_arith_file_list_construct();
    gtk_box_pack_start(GTK_BOX(hbox), list, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);
    return dialog;
}

static GtkWidget*
gwy_data_arith_file_list_construct(void)
{
    static const struct {
        const gchar *title;
        const guint id;
    }
    columns[] = {
        { "Data", ARITH_FILENAME },
    };

    GtkWidget *tree;
    GtkListStore *store;
    GtkTreeSelection *select;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    gsize i;

    store = gtk_list_store_new(ARITH_LAST,
                               G_TYPE_POINTER, /* data window itself */
                               G_TYPE_STRING   /* file name */
                              );

    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    gwy_app_data_window_foreach((GFunc)gwy_data_arith_add_line, store);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(columns[i].title,
                                                          renderer,
                                                          "text", columns[i].id,
                                                          NULL);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                gwy_data_arith_cell_renderer,
                                                GUINT_TO_POINTER(columns[i].id),
                                                NULL);  /* destroy notify */
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    }

    select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

    return tree;
}

static void
gwy_data_arith_cell_renderer(GtkTreeViewColumn *column,
                             GtkCellRenderer *cell,
                             GtkTreeModel *model,
                             GtkTreeIter *piter,
                             gpointer user_data)
{
    GwyDataWindow *data_window;
    GtkWidget *data_view;
    GwyContainer *data;
    gchar *s;
    gulong id;

    id = GPOINTER_TO_UINT(user_data);
    g_assert(id > ARITH_DATA_WINDOW && id < ARITH_LAST);
    gtk_tree_model_get(model, piter, ARITH_DATA_WINDOW, &data_window, -1);
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    g_return_if_fail(data);
    switch (id) {
        case ARITH_FILENAME:
        if (gwy_container_contains_by_name(data, "/filename")) {
            const gchar *fnm = gwy_container_get_string_by_name(data,
                                                                "/filename");

            s = g_path_get_basename(fnm);
        }
        else {
            gint u = gwy_container_get_int32_by_name(data,
                                                     "/filename/untitled");

            s = g_strdup_printf(_("Untitled-%d"), u);
        }
        g_object_set(cell, "text", s, NULL);
        g_free(s);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
gwy_data_arith_add_line(GwyDataWindow *data_window,
                        GtkListStore *store)
{
    GtkTreeIter iter;

    gtk_list_store_append(store, &iter);
    /* XXX: this requires modality */
    gtk_list_store_set(store, &iter, ARITH_DATA_WINDOW, data_window, -1);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

