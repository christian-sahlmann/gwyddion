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


#ifndef __GWY_EVALUATOR_TASK_DIALOG_H__
#define __GWY_EVALUATOR_TASK_DIALOG_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtk.h>


G_BEGIN_DECLS
typedef enum {
    GWY_EVALUATOR_TASK_LINE_MIN = 0,
    GWY_EVALUATOR_TASK_LINE_MAX = 1,
    GWY_EVALUATOR_TASK_LINE_AVERAGE = 2,
    GWY_EVALUATOR_TASK_LINE_SIGMA = 3,
    GWY_EVALUATOR_TASK_LINE_PERIODICITY = 4,
    GWY_EVALUATOR_TASK_POINT_VALUE = 5,
    GWY_EVALUATOR_TASK_POINT_AVERAGE = 6,
    GWY_EVALUATOR_TASK_POINT_NEURAL = 7,
    GWY_EVALUATOR_TASK_LINES_ANGLE = 8,
    GWY_EVALUATOR_TASK_LINES_INTERSECTION_X = 9,
    GWY_EVALUATOR_TASK_LINES_INTERSECTION_Y = 10,
    GWY_EVALUATOR_TASK_POINT_LINE_DISTANCE = 11,
} GwyEvaluatorTaskFunction;

typedef enum {
    GWY_EVALUATOR_THRESHOLD_BIGGER = 0,
    GWY_EVALUATOR_THRESHOLD_SMALLER = 1,
    GWY_EVALUATOR_THRESHOLD_EQUAL = 2,
    GWY_EVALUATOR_THRESHOLD_INTERVAL = 3,
} GwyEvaluatorThresholdFunction; 

#define GWY_TYPE_EVALUATOR_TASK_DIALOG            (gwy_evaluator_task_dialog_get_type())
#define GWY_EVALUATOR_TASK_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_EVALUATOR_TASK_DIALOG, GwyEvaluatorTaskDialog))
#define GWY_EVALUATOR_TASK_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_EVALUATOR_TASK_DIALOG, GwyEvaluatorTaskDialogClass))
#define GWY_IS_EVALUATOR_TASK_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_EVALUATOR_TASK_DIALOG))
#define GWY_IS_EVALUATOR_TASK_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_EVALUATOR_TASK_DIALOG))
#define GWY_EVALUATOR_TASK_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_EVALUATOR_TASK_DIALOG, GwyEvaluatorTaskDialogClass))

typedef struct _GwyEvaluatorTaskDialog      GwyEvaluatorTaskDialog;
typedef struct _GwyEvaluatorTaskDialogClass GwyEvaluatorTaskDialogClass;

struct _GwyEvaluatorTaskDialog {
    GtkDialog dialog;

    GtkWidget *expression;
    GtkWidget *threshold_expression;
    GtkWidget *task_combo;
    GtkWidget *threshold_combo;
    GwyEvaluatorTaskFunction task;
    GwyEvaluatorThresholdFunction threshold;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyEvaluatorTaskDialogClass {
    GtkDialogClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
};

GType       gwy_evaluator_task_dialog_get_type (void) G_GNUC_CONST;
GtkWidget*  gwy_evaluator_task_dialog_new      (void);

void        gwy_evaluator_task_dialog_set_graph_data(GtkWidget *dialog, GObject *model);

G_END_DECLS


#define GWY_EVALUATOR_TASK_DIALOG_TYPE_NAME "GwyEvaluatorTaskDialog"

static void     gwy_evaluator_task_dialog_class_init       (GwyEvaluatorTaskDialogClass *klass);
static void     gwy_evaluator_task_dialog_init             (GwyEvaluatorTaskDialog *dialog);
static void     gwy_evaluator_task_dialog_finalize         (GObject *object);
static gboolean gwy_evaluator_task_dialog_delete           (GtkWidget *widget,
                                                          GdkEventAny *event);
static void        function_changed_cb             (GtkWidget *combo,
                                                GwyEvaluatorTaskDialog *dialog);
static void        function_add_cb                 (GwyEvaluatorTaskDialog *dialog);
static void        threshold_changed_cb             (GtkWidget *combo,
                                                    GwyEvaluatorTaskDialog *dialog);
static void        threshold_add_cb                 (GwyEvaluatorTaskDialog *dialog);





static GtkDialogClass *parent_class = NULL;

GType
gwy_evaluator_task_dialog_get_type(void)
{
    static GType gwy_evaluator_task_dialog_type = 0;

    if (!gwy_evaluator_task_dialog_type) {
        static const GTypeInfo gwy_evaluator_task_dialog_info = {
            sizeof(GwyEvaluatorTaskDialogClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_evaluator_task_dialog_class_init,
            NULL,
            NULL,
            sizeof(GwyEvaluatorTaskDialog),
            0,
            (GInstanceInitFunc)gwy_evaluator_task_dialog_init,
            NULL,
        };
        gwy_debug("");
        gwy_evaluator_task_dialog_type = g_type_register_static(GTK_TYPE_DIALOG,
                                                      GWY_EVALUATOR_TASK_DIALOG_TYPE_NAME,
                                                      &gwy_evaluator_task_dialog_info,
                                                      0);

    }

    return gwy_evaluator_task_dialog_type;
}

static void
gwy_evaluator_task_dialog_class_init(GwyEvaluatorTaskDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    gwy_debug("");
    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_evaluator_task_dialog_finalize;
    widget_class->delete_event = gwy_evaluator_task_dialog_delete;
}

static gboolean
gwy_evaluator_task_dialog_delete(GtkWidget *widget,
                       G_GNUC_UNUSED GdkEventAny *event)
{
    gwy_debug("");
    gtk_widget_hide(widget);

    return TRUE;
}

static void
gwy_evaluator_task_dialog_init(GwyEvaluatorTaskDialog *dialog)
{
    GtkWidget *label, *table, *button;
    gint row = 0;

    static const GwyEnum tasks[] = {
        { N_("Line minimum value (line ID)"),                GWY_EVALUATOR_TASK_LINE_MIN,                },
        { N_("Line maximum value (line ID)"),                GWY_EVALUATOR_TASK_LINE_MAX,                },
        { N_("Line average value (line ID)"),                GWY_EVALUATOR_TASK_LINE_AVERAGE,            },
        { N_("Line RMS (line ID)"),                          GWY_EVALUATOR_TASK_LINE_SIGMA,              },
        { N_("Line main periodicity (line ID)"),             GWY_EVALUATOR_TASK_LINE_PERIODICITY,        },
        { N_("Point value (point ID)"),                      GWY_EVALUATOR_TASK_POINT_VALUE,             },
        { N_("Point average (point ID, pixel radius"),       GWY_EVALUATOR_TASK_POINT_AVERAGE,           },
        { N_("Point neural judge (point ID, network file)"), GWY_EVALUATOR_TASK_POINT_NEURAL,            },
        { N_("X intersection of lines (line ID, line ID)"),  GWY_EVALUATOR_TASK_LINES_INTERSECTION_X,    },
        { N_("Y intersection of lines (line ID, line ID)"),  GWY_EVALUATOR_TASK_LINES_INTERSECTION_Y,    },
        { N_("Angle between lines (line ID, line ID)"),      GWY_EVALUATOR_TASK_LINES_ANGLE,             },
        { N_("Point - line distance(line ID, point ID)"),    GWY_EVALUATOR_TASK_POINT_LINE_DISTANCE,     },
    };
    static const GwyEnum thresholds[] = {
        { N_("Bigger than"),                 GWY_EVALUATOR_THRESHOLD_BIGGER,               },
        { N_("Smaller than"),                GWY_EVALUATOR_THRESHOLD_SMALLER,              },
        { N_("Equal to"),                    GWY_EVALUATOR_THRESHOLD_EQUAL,                },
        { N_("In interval"),                 GWY_EVALUATOR_THRESHOLD_INTERVAL,             },
    };


    table = gtk_table_new(2, 8, FALSE);

    label = gtk_label_new("Layout:");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    dialog->expression = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(dialog->expression), "");
    gtk_table_attach(GTK_TABLE(table), dialog->expression, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), dialog->expression);
    row++;

    dialog->task_combo
        = gwy_enum_combo_box_new(tasks, G_N_ELEMENTS(tasks),
                                 G_CALLBACK(function_changed_cb),
                                 dialog, dialog->task, TRUE);
    gwy_table_attach_row(table, row, _("_Function:"), "",
                         dialog->task_combo);
    row++;

    button = gtk_button_new_with_label("Add function");
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(function_add_cb), dialog);
    gtk_table_attach(GTK_TABLE(table), button, 0, 3, row, row+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);

    row++;
    dialog->threshold_expression = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(dialog->threshold_expression), "");
    gtk_table_attach(GTK_TABLE(table), dialog->threshold_expression, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), dialog->threshold_expression);
    row++;

    dialog->threshold_combo
        = gwy_enum_combo_box_new(thresholds, G_N_ELEMENTS(thresholds),
                                 G_CALLBACK(threshold_changed_cb),
                                 dialog, dialog->threshold, TRUE);
    gwy_table_attach_row(table, row, _("_Operator:"), "",
                         dialog->threshold_combo);
    row++;

    button = gtk_button_new_with_label("Add operator");
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(threshold_add_cb), dialog);
    gtk_table_attach(GTK_TABLE(table), button, 0, 3, row, row+1,
                                          GTK_EXPAND | GTK_FILL, 0, 2, 2);

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_APPLY, GTK_RESPONSE_APPLY);


    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
                      table);
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);

    gtk_window_set_title(GTK_WINDOW(dialog), "Add task");
}


GtkWidget *
gwy_evaluator_task_dialog_new()
{
    gwy_debug("");
    return GTK_WIDGET (g_object_new (gwy_evaluator_task_dialog_get_type (), NULL));
}

static void
gwy_evaluator_task_dialog_finalize(GObject *object)
{
    gwy_debug("");

    g_return_if_fail(GWY_IS_EVALUATOR_TASK_DIALOG(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void        
function_changed_cb(GtkWidget *combo, GwyEvaluatorTaskDialog *dialog)
{
    dialog->task = (GwyEvaluatorTaskFunction)gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

}

static void        
function_add_cb(GwyEvaluatorTaskDialog *dialog)
{
    GtkEditable *editable;
    gint pos;
    gchar *p;

    static const GwyEnum tasks[] = {
        { N_("LineMin()"),      GWY_EVALUATOR_TASK_LINE_MIN,                },
        { N_("LineMax()"),      GWY_EVALUATOR_TASK_LINE_MAX,                },
        { N_("LineAvg()"),      GWY_EVALUATOR_TASK_LINE_AVERAGE,            },
        { N_("LineRMS()"),      GWY_EVALUATOR_TASK_LINE_SIGMA,              },
        { N_("LineFreq()"),     GWY_EVALUATOR_TASK_LINE_PERIODICITY,        },
        { N_("PointValue()"),   GWY_EVALUATOR_TASK_POINT_VALUE,             },
        { N_("PointAvg()"),     GWY_EVALUATOR_TASK_POINT_AVERAGE,           },
        { N_("PointNeural()"),  GWY_EVALUATOR_TASK_POINT_NEURAL,            },
        { N_("IntersectX()"),   GWY_EVALUATOR_TASK_LINES_INTERSECTION_X,    },
        { N_("IntersectY()"),   GWY_EVALUATOR_TASK_LINES_INTERSECTION_Y,    },
        { N_("Angle()"),        GWY_EVALUATOR_TASK_LINES_ANGLE,             },
        { N_("PLDistance()"),   GWY_EVALUATOR_TASK_POINT_LINE_DISTANCE,     },
    };



    p = gwy_enum_to_string(dialog->task, tasks, G_N_ELEMENTS(tasks));
    
    editable = GTK_EDITABLE(dialog->expression);
    pos = gtk_editable_get_position(editable);
    gtk_editable_insert_text(editable, p, strlen(p), &pos);
    gtk_editable_set_position(editable, pos);

}

static void        
threshold_changed_cb(GtkWidget *combo, GwyEvaluatorTaskDialog *dialog)
{
    dialog->threshold = (GwyEvaluatorThresholdFunction)gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

}

static void        
threshold_add_cb(GwyEvaluatorTaskDialog *dialog)
{
    GtkEditable *editable;
    gint pos;
    gchar *p;

    static const GwyEnum thresholds[] = {
        { N_("> "),                GWY_EVALUATOR_THRESHOLD_BIGGER,               },
        { N_("< "),                GWY_EVALUATOR_THRESHOLD_SMALLER,              },
        { N_("= "),                GWY_EVALUATOR_THRESHOLD_EQUAL,                },
        { N_("Interval()"),        GWY_EVALUATOR_THRESHOLD_INTERVAL,             },
     };

    p = gwy_enum_to_string(dialog->threshold, thresholds, G_N_ELEMENTS(thresholds));
    
    editable = GTK_EDITABLE(dialog->threshold_expression);
    pos = gtk_editable_get_position(editable);
    gtk_editable_insert_text(editable, p, strlen(p), &pos);
    gtk_editable_set_position(editable, pos);

}

#endif /* __GWY_EVALUATOR_TASK_DIALOG_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
