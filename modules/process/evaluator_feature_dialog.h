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


#ifndef __GWY_EVALUATOR_POINT_DIALOG_H__
#define __GWY_EVALUATOR_POINT_DIALOG_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtk.h>


G_BEGIN_DECLS

#define GWY_TYPE_EVALUATOR_POINT_DIALOG            (gwy_evaluator_point_dialog_get_type())
#define GWY_EVALUATOR_POINT_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_EVALUATOR_POINT_DIALOG, GwyEvaluatorPointDialog))
#define GWY_EVALUATOR_POINT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_EVALUATOR_POINT_DIALOG, GwyEvaluatorPointDialogClass))
#define GWY_IS_EVALUATOR_POINT_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_EVALUATOR_POINT_DIALOG))
#define GWY_IS_EVALUATOR_POINT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_EVALUATOR_POINT_DIALOG))
#define GWY_EVALUATOR_POINT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_EVALUATOR_POINT_DIALOG, GwyEvaluatorPointDialogClass))

typedef struct _GwyEvaluatorPointDialog      GwyEvaluatorPointDialog;
typedef struct _GwyEvaluatorPointDialogClass GwyEvaluatorPointDialogClass;

struct _GwyEvaluatorPointDialog {
    GtkDialog dialog;

    GObject *width_adj;
    GObject *height_adj;
    gint width;
    gint height;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyEvaluatorPointDialogClass {
    GtkDialogClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
};

GType       gwy_evaluator_point_dialog_get_type (void) G_GNUC_CONST;
GtkWidget*  gwy_evaluator_point_dialog_new      (gint width, gint height);


G_END_DECLS


#define GWY_EVALUATOR_POINT_DIALOG_TYPE_NAME "GwyEvaluatorPointDialog"

static void     gwy_evaluator_point_dialog_class_init       (GwyEvaluatorPointDialogClass *klass);
static void     gwy_evaluator_point_dialog_init             (GwyEvaluatorPointDialog *dialog);
static void     gwy_evaluator_point_dialog_finalize         (GObject *object);
static gboolean gwy_evaluator_point_dialog_delete           (GtkWidget *widget,
                                                          GdkEventAny *event);

//static GtkDialogClass *parent_class = NULL;

GType
gwy_evaluator_point_dialog_get_type(void)
{
    static GType gwy_evaluator_point_dialog_type = 0;

    if (!gwy_evaluator_point_dialog_type) {
        static const GTypeInfo gwy_evaluator_point_dialog_info = {
            sizeof(GwyEvaluatorPointDialogClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_evaluator_point_dialog_class_init,
            NULL,
            NULL,
            sizeof(GwyEvaluatorPointDialog),
            0,
            (GInstanceInitFunc)gwy_evaluator_point_dialog_init,
            NULL,
        };
        gwy_debug("");
        gwy_evaluator_point_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
                                                      GWY_EVALUATOR_POINT_DIALOG_TYPE_NAME,
                                                      &gwy_evaluator_point_dialog_info,
                                                      0);

    }

    return gwy_evaluator_point_dialog_type;
}

static void
gwy_evaluator_point_dialog_class_init(GwyEvaluatorPointDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    gwy_debug("");
    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_evaluator_point_dialog_finalize;
    widget_class->delete_event = gwy_evaluator_point_dialog_delete;
}

static gboolean
gwy_evaluator_point_dialog_delete(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventAny *event)
{
    gwy_debug("");
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_evaluator_point_dialog_init(GwyEvaluatorPointDialog *dialog)
{
    GtkWidget *label, *table, *button;
    gint row = 0;


    table = gtk_table_new(2, 8, FALSE);

    label = gtk_label_new("Search area size:");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    row++;
    dialog->width_adj = gtk_adjustment_new(dialog->width, 0.0, 100.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row++, _("_Width:"), _("pixels"),
                                                 dialog->width_adj);
    
    dialog->height_adj = gtk_adjustment_new(dialog->height, 0.0, 100.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row++, _("_Height:"), _("pixels"),
                                                 dialog->height_adj);
         

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_APPLY, GTK_RESPONSE_APPLY);


    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                      table);
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);

    gtk_window_set_title(GTK_WINDOW(dialog), "Point search area");
}


GtkWidget *
gwy_evaluator_point_dialog_new(gint width, gint height)
{
    GtkWidget *dialog;

    dialog = GTK_WIDGET(g_object_new (gwy_evaluator_point_dialog_get_type (), NULL));
    
    GWY_EVALUATOR_POINT_DIALOG(dialog)->width = width;
    GWY_EVALUATOR_POINT_DIALOG(dialog)->height = height;
   
    gtk_adjustment_set_value(GWY_EVALUATOR_POINT_DIALOG(dialog)->height_adj, height);
    gtk_adjustment_set_value(GWY_EVALUATOR_POINT_DIALOG(dialog)->width_adj, width);
    return dialog;
}

static void
gwy_evaluator_point_dialog_finalize(GObject *object)
{
    gwy_debug("");

    g_return_if_fail(GWY_IS_EVALUATOR_POINT_DIALOG(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}





G_BEGIN_DECLS

#define GWY_TYPE_EVALUATOR_LINE_DIALOG            (gwy_evaluator_line_dialog_get_type())
#define GWY_EVALUATOR_LINE_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_EVALUATOR_LINE_DIALOG, GwyEvaluatorLineDialog))
#define GWY_EVALUATOR_LINE_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_EVALUATOR_LINE_DIALOG, GwyEvaluatorLineDialogClass))
#define GWY_IS_EVALUATOR_LINE_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_EVALUATOR_LINE_DIALOG))
#define GWY_IS_EVALUATOR_LINE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_EVALUATOR_LINE_DIALOG))
#define GWY_EVALUATOR_LINE_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_EVALUATOR_LINE_DIALOG, GwyEvaluatorLineDialogClass))

typedef struct _GwyEvaluatorLineDialog      GwyEvaluatorLineDialog;
typedef struct _GwyEvaluatorLineDialogClass GwyEvaluatorLineDialogClass;

struct _GwyEvaluatorLineDialog {
    GtkDialog dialog;

    GObject *rho_adj;
    GObject *theta_adj;
    gint rho;
    gint theta;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyEvaluatorLineDialogClass {
    GtkDialogClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
};

GType       gwy_evaluator_line_dialog_get_type (void) G_GNUC_CONST;
GtkWidget*  gwy_evaluator_line_dialog_new      (gint rho, gint theta);


G_END_DECLS


#define GWY_EVALUATOR_LINE_DIALOG_TYPE_NAME "GwyEvaluatorLineDialog"

static void     gwy_evaluator_line_dialog_class_init       (GwyEvaluatorLineDialogClass *klass);
static void     gwy_evaluator_line_dialog_init             (GwyEvaluatorLineDialog *dialog);
static void     gwy_evaluator_line_dialog_finalize         (GObject *object);
static gboolean gwy_evaluator_line_dialog_delete           (GtkWidget *widget,
                                                          GdkEventAny *event);

//static GtkDialogClass *parent_class = NULL;

GType
gwy_evaluator_line_dialog_get_type(void)
{
    static GType gwy_evaluator_line_dialog_type = 0;

    if (!gwy_evaluator_line_dialog_type) {
        static const GTypeInfo gwy_evaluator_line_dialog_info = {
            sizeof(GwyEvaluatorLineDialogClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_evaluator_line_dialog_class_init,
            NULL,
            NULL,
            sizeof(GwyEvaluatorLineDialog),
            0,
            (GInstanceInitFunc)gwy_evaluator_line_dialog_init,
            NULL,
        };
        gwy_debug("");
        gwy_evaluator_line_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
                                                      GWY_EVALUATOR_LINE_DIALOG_TYPE_NAME,
                                                      &gwy_evaluator_line_dialog_info,
                                                      0);

    }

    return gwy_evaluator_line_dialog_type;
}

static void
gwy_evaluator_line_dialog_class_init(GwyEvaluatorLineDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    gwy_debug("");
    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_evaluator_line_dialog_finalize;
    widget_class->delete_event = gwy_evaluator_line_dialog_delete;
}

static gboolean
gwy_evaluator_line_dialog_delete(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventAny *event)
{
    gwy_debug("");
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_evaluator_line_dialog_init(GwyEvaluatorLineDialog *dialog)
{
    GtkWidget *label, *table, *button;
    gint row = 0;


    table = gtk_table_new(2, 8, FALSE);

    label = gtk_label_new("Search area size:");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    row++;
    dialog->rho_adj = gtk_adjustment_new(dialog->rho, 0.0, 100.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row++, _("_Rho:"), _("pixels"),
                                                 dialog->rho_adj);
    
    dialog->theta_adj = gtk_adjustment_new(dialog->theta, 0.0, 100.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row++, _("_Theta:"), _("pixels"),
                                                 dialog->theta_adj);
         

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_APPLY, GTK_RESPONSE_APPLY);


    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                      table);
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);

    gtk_window_set_title(GTK_WINDOW(dialog), "Point search area");
}


GtkWidget *
gwy_evaluator_line_dialog_new(gint rho, gint theta)
{
    GtkWidget *dialog;

    dialog = GTK_WIDGET(g_object_new (gwy_evaluator_line_dialog_get_type (), NULL));
    
    GWY_EVALUATOR_LINE_DIALOG(dialog)->rho = rho;
    GWY_EVALUATOR_LINE_DIALOG(dialog)->theta = theta;
   
    gtk_adjustment_set_value(GWY_EVALUATOR_LINE_DIALOG(dialog)->rho_adj, rho);
    gtk_adjustment_set_value(GWY_EVALUATOR_LINE_DIALOG(dialog)->theta_adj, theta);
  
    return dialog;
}

static void
gwy_evaluator_line_dialog_finalize(GObject *object)
{
    gwy_debug("");

    g_return_if_fail(GWY_IS_EVALUATOR_LINE_DIALOG(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}


#define GWY_TYPE_EVALUATOR_CORRELATION_POINT_DIALOG            (gwy_evaluator_correlation_point_dialog_get_type())
#define GWY_EVALUATOR_CORRELATION_POINT_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_EVALUATOR_CORRELATION_POINT_DIALOG, GwyEvaluatorCorrelationPointDialog))
#define GWY_EVALUATOR_CORRELATION_POINT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_EVALUATOR_CORRELATION_POINT_DIALOG, GwyEvaluatorCorrelationPointDialogClass))
#define GWY_IS_EVALUATOR_CORRELATION_POINT_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_EVALUATOR_CORRELATION_POINT_DIALOG))
#define GWY_IS_EVALUATOR_CORRELATION_POINT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_EVALUATOR_CORRELATION_POINT_DIALOG))
#define GWY_EVALUATOR_CORRELATION_POINT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_EVALUATOR_CORRELATION_POINT_DIALOG, GwyEvaluatorCorrelationPointDialogClass))

typedef struct _GwyEvaluatorCorrelationPointDialog      GwyEvaluatorCorrelationPointDialog;
typedef struct _GwyEvaluatorCorrelationPointDialogClass GwyEvaluatorCorrelationPointDialogClass;

struct _GwyEvaluatorCorrelationPointDialog {
    GtkDialog dialog;

    GObject *width_adj;
    GObject *height_adj;
    GObject *swidth_adj;
    GObject *sheight_adj;
     gint width;
    gint height;
    gint swidth;
    gint sheight;


    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyEvaluatorCorrelationPointDialogClass {
    GtkDialogClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
};

GType       gwy_evaluator_correlation_point_dialog_get_type (void) G_GNUC_CONST;
GtkWidget*  gwy_evaluator_correlation_point_dialog_new      (gint width, gint height,
                                                             gint swidth, gint sheight);


G_END_DECLS


#define GWY_EVALUATOR_CORRELATION_POINT_DIALOG_TYPE_NAME "GwyEvaluatorCorrelationPointDialog"

static void     gwy_evaluator_correlation_point_dialog_class_init       (GwyEvaluatorCorrelationPointDialogClass *klass);
static void     gwy_evaluator_correlation_point_dialog_init             (GwyEvaluatorCorrelationPointDialog *dialog);
static void     gwy_evaluator_correlation_point_dialog_finalize         (GObject *object);
static gboolean gwy_evaluator_correlation_point_dialog_delete           (GtkWidget *widget,
                                                          GdkEventAny *event);

//static GtkDialogClass *parent_class = NULL;

GType
gwy_evaluator_correlation_point_dialog_get_type(void)
{
    static GType gwy_evaluator_correlation_point_dialog_type = 0;

    if (!gwy_evaluator_correlation_point_dialog_type) {
        static const GTypeInfo gwy_evaluator_correlation_point_dialog_info = {
            sizeof(GwyEvaluatorCorrelationPointDialogClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_evaluator_correlation_point_dialog_class_init,
            NULL,
            NULL,
            sizeof(GwyEvaluatorCorrelationPointDialog),
            0,
            (GInstanceInitFunc)gwy_evaluator_correlation_point_dialog_init,
            NULL,
        };
        gwy_debug("");
        gwy_evaluator_correlation_point_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
                                                      GWY_EVALUATOR_CORRELATION_POINT_DIALOG_TYPE_NAME,
                                                      &gwy_evaluator_correlation_point_dialog_info,
                                                      0);

    }

    return gwy_evaluator_correlation_point_dialog_type;
}

static void
gwy_evaluator_correlation_point_dialog_class_init(GwyEvaluatorCorrelationPointDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    gwy_debug("");
    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_evaluator_correlation_point_dialog_finalize;
    widget_class->delete_event = gwy_evaluator_correlation_point_dialog_delete;
}

static gboolean
gwy_evaluator_correlation_point_dialog_delete(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventAny *event)
{
    gwy_debug("");
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_evaluator_correlation_point_dialog_init(GwyEvaluatorCorrelationPointDialog *dialog)
{
    GtkWidget *label, *table, *button;
    gint row = 0;


    table = gtk_table_new(2, 8, FALSE);

    label = gtk_label_new("Search area size:");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    row++;
    dialog->width_adj = gtk_adjustment_new(dialog->width, 0.0, 100.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row++, _("_Width:"), _("pixels"),
                                                 dialog->width_adj);
    
    dialog->height_adj = gtk_adjustment_new(dialog->height, 0.0, 100.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row++, _("_Height:"), _("pixels"),
                                                 dialog->height_adj);
         
    dialog->swidth_adj = gtk_adjustment_new(dialog->swidth, 0.0, 100.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row++, _("Se_arch width:"), _("pixels"),
                                                 dialog->swidth_adj);
    
    dialog->sheight_adj = gtk_adjustment_new(dialog->sheight, 0.0, 100.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row++, _("Sea_rch height:"), _("pixels"),
                                                 dialog->sheight_adj);
 
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_APPLY, GTK_RESPONSE_APPLY);


    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                      table);
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);

    gtk_window_set_title(GTK_WINDOW(dialog), "Point search area");
}


GtkWidget *
gwy_evaluator_correlation_point_dialog_new(gint width, gint height,
                                           gint swidth, gint sheight)
{
    GtkWidget *dialog;

    dialog = GTK_WIDGET(g_object_new (gwy_evaluator_correlation_point_dialog_get_type (), NULL));
    
    GWY_EVALUATOR_CORRELATION_POINT_DIALOG(dialog)->width = width;
    GWY_EVALUATOR_CORRELATION_POINT_DIALOG(dialog)->height = height;
   
    gtk_adjustment_set_value(GWY_EVALUATOR_CORRELATION_POINT_DIALOG(dialog)->height_adj, height);
    gtk_adjustment_set_value(GWY_EVALUATOR_CORRELATION_POINT_DIALOG(dialog)->width_adj, width);
    gtk_adjustment_set_value(GWY_EVALUATOR_CORRELATION_POINT_DIALOG(dialog)->sheight_adj, sheight);
    gtk_adjustment_set_value(GWY_EVALUATOR_CORRELATION_POINT_DIALOG(dialog)->swidth_adj, swidth);
     return dialog;
}

static void
gwy_evaluator_correlation_point_dialog_finalize(GObject *object)
{
    gwy_debug("");

    g_return_if_fail(GWY_IS_EVALUATOR_CORRELATION_POINT_DIALOG(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}



#endif /* __GWY_EVALUATOR_POINT_DIALOG_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
