/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *  Copyright (C) 2004 Martin Siler.
 *  E-mail: silerm@physics.muni.cz.
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
#include <string.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/datafield.h>
#include "gwy3dlabels.h"

#define GWY_3D_LABELS_TYPE_NAME "Gwy3DLabels"

enum {
  LABEL_CHANGED,
  LAST_SIGNAL
};

static void    gwy_3d_labels_class_init (Gwy3DLabelsClass *klass);
static void    gwy_3d_labels_init       (Gwy3DLabels *labels);
static void    gwy_3d_labels_finalize   (GObject *object);


static Gwy3DLabelDescription * gwy_3d_label_description_new(
                                    gchar * text,
                                    gint delta_x,
                                    gint delta_y,
                                    gfloat rot,
                                    gint size,
                                    Gwy3DLabels * owner);
static void    gwy_3d_label_description_init(
                                    Gwy3DLabelDescription *label_description);
static void    gwy_3d_label_description_free(
                                    Gwy3DLabelDescription *label_description);
static void    gwy_3d_labels_adjustment_value_changed(
                                    GtkAdjustment* adjustment, gpointer user_data);

static GObjectClass *parent_class = NULL;
static guint labels_signals[LAST_SIGNAL] = { 0 };


GType
gwy_3d_labels_get_type(void)
{
    static GType gwy_3d_labels_type = 0;

    if (!gwy_3d_labels_type) {
        static const GTypeInfo gwy_3d_labels_info = {
            sizeof(Gwy3DLabelsClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_3d_labels_class_init,
            NULL,
            NULL,
            sizeof(Gwy3DLabels),
            0,
            (GInstanceInitFunc)gwy_3d_labels_init,
            NULL,
        };


        gwy_debug("");
        gwy_3d_labels_type = g_type_register_static(G_TYPE_OBJECT,
                                                    GWY_3D_LABELS_TYPE_NAME,
                                                    &gwy_3d_labels_info,
                                                    0);
    }

    return gwy_3d_labels_type;
}


static void
gwy_3d_labels_class_init(Gwy3DLabelsClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);
    gobject_class->finalize = gwy_3d_labels_finalize;
    klass->label_changed = NULL;

    labels_signals[LABEL_CHANGED] =
        g_signal_new("label_changed",
                  G_OBJECT_CLASS_TYPE(klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                  G_STRUCT_OFFSET(Gwy3DLabelsClass, label_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
gwy_3d_label_description_init(Gwy3DLabelDescription *label_description)
{
    label_description->text     = NULL;
    label_description->delta_x  = NULL;
    label_description->delta_y  = NULL;
    label_description->rotation = NULL;
    label_description->size     = NULL;
}

static void
gwy_3d_labels_init(Gwy3DLabels *labels)
{
    labels->labels = NULL;
    labels->keys = NULL;
    labels->values = NULL;
    labels->text = NULL;
    labels->variables_count = 0;
    labels->labels_count = 0;
}

static void
gwy_3d_label_description_free(Gwy3DLabelDescription *label_description)
{
    g_free(label_description->text);

    gwy_object_unref(label_description->delta_x);
    gwy_object_unref(label_description->delta_y);
    gwy_object_unref(label_description->rotation);
    gwy_object_unref(label_description->size);

    g_free(label_description);
}

static void
gwy_3d_labels_finalize(GObject *object)
{
    Gwy3DLabels *labels = (Gwy3DLabels*)object;
    int i;

    gwy_debug("");

    for (i = 0; i < labels->labels_count; ++i)
        gwy_3d_label_description_free(labels->labels[i]);
    g_free(labels->labels);
    for (i = 0; i < labels->variables_count; ++i)
    {
        g_free(labels->keys[i]);
        g_free(labels->values[i]);
    }
    g_free(labels->keys);
    g_free(labels->values);
    g_free(labels->text);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}


static Gwy3DLabelDescription *
gwy_3d_label_description_new(gchar * text, gint delta_x, gint delta_y,
                             gfloat rot, gint size, Gwy3DLabels * owner)
{
    gwy_debug(" ");
    Gwy3DLabelDescription * desc = g_new(Gwy3DLabelDescription, 1);
    gwy_3d_label_description_init(desc);

    desc->text = g_strdup(text);
    desc->delta_x  = (GtkAdjustment*)gtk_adjustment_new(delta_x, -1000, 1000, 1, 10, 0.0);
    desc->delta_y  = (GtkAdjustment*)gtk_adjustment_new(delta_y, -1000, 1000, 1, 10, 0.0);;
    desc->rotation = (GtkAdjustment*)gtk_adjustment_new(rot, -180, 180, 1, 10, 0.0);;
    desc->size     = (GtkAdjustment*)gtk_adjustment_new(size, -1, 100, 1, 5, 0.0);;

    g_object_ref(G_OBJECT(desc->delta_x));
    g_object_ref(G_OBJECT(desc->delta_y));
    g_object_ref(G_OBJECT(desc->rotation));
    g_object_ref(G_OBJECT(desc->size));
    gtk_object_sink(GTK_OBJECT(desc->delta_x));
    gtk_object_sink(GTK_OBJECT(desc->delta_y));
    gtk_object_sink(GTK_OBJECT(desc->rotation));
    gtk_object_sink(GTK_OBJECT(desc->size));

    g_signal_connect(desc->delta_x, "value_changed",
                     G_CALLBACK(gwy_3d_labels_adjustment_value_changed),
                     owner);
    g_signal_connect(desc->delta_y, "value_changed",
                     G_CALLBACK(gwy_3d_labels_adjustment_value_changed),
                     owner);
    g_signal_connect(desc->rotation, "value_changed",
                     G_CALLBACK(gwy_3d_labels_adjustment_value_changed),
                     owner);
    g_signal_connect(desc->size, "value_changed",
                     G_CALLBACK(gwy_3d_labels_adjustment_value_changed),
                     owner);

    return desc;
}

/**
 * gwy_3d_labels_new:
 *
 *
 *
 * Since: 1.5
 **/
Gwy3DLabels * gwy_3d_labels_new(void)
{
    gint i;
    Gwy3DLabels * labels = g_object_new(GWY_TYPE_3D_LABELS, NULL);

    gwy_debug(" ");

    labels->labels_count = 4;
    labels->labels = g_new(Gwy3DLabelDescription*, labels->labels_count);

    labels->labels[0] = gwy_3d_label_description_new("x: $X", 0, 0, 0.0f, -1, labels);
    labels->labels[1] = gwy_3d_label_description_new("y: $Y", 0, 0, 0.0f, -1, labels);
    labels->labels[2] = gwy_3d_label_description_new("$MIN",  0, 0, 0.0f, -1, labels);
    labels->labels[3] = gwy_3d_label_description_new("$MAX",  0, 0, 0.0f, -1, labels);

    labels->variables_count = 4;
    labels->keys   = g_new(gchar*, labels->variables_count);
    labels->values = g_new(gchar*, labels->variables_count);
    labels->keys[0] = g_strdup("X");
    labels->keys[1] = g_strdup("Y");
    labels->keys[2] = g_strdup("MIN");
    labels->keys[3] = g_strdup("MAX");
    for (i =0; i < labels->variables_count; i++)
       labels->values[i] = NULL;

    labels->text     = NULL;

    return labels;
}

/**
 * gwy_3d_labels_get_description:
 *
 *
 *
 * Since: 1.5
 **/
Gwy3DLabelDescription *
gwy_3d_labels_get_description(Gwy3DLabels * labels, Gwy3DLabelName label_name)
{

    g_return_val_if_fail(GWY_IS_3D_LABELS(labels), NULL);
    g_return_val_if_fail(label_name <= GWY_3D_VIEW_LABEL_MAX, NULL);

    return labels->labels[label_name];
}

/**
 * gwy_3d_labels_get_update:
 *
 *
 *
 * Since: 1.5
 **/
void gwy_3d_labels_update(Gwy3DLabels * labels, GwyContainer* container, GwySIUnit * si_unit)
{
    GwySIValueFormat * format;
    gchar buffer[50];
    gdouble xreal, yreal, data_min, data_max;
    GwyDataField * data_field;
    int i;

    gwy_debug(" ");

    g_return_if_fail(gwy_container_gis_object_by_name(container, "/0/data", (GObject**)&data_field));

    for (i = 0; i < labels->variables_count; i++)
       g_free(labels->values[i]);

    xreal    = gwy_data_field_get_xreal(data_field);
    yreal    = gwy_data_field_get_yreal(data_field);
    data_min = gwy_data_field_get_min(data_field);
    data_max = gwy_data_field_get_max(data_field);

    format = gwy_si_unit_get_format_with_resolution(
                     si_unit,
                     xreal,
                     xreal,
                     NULL);
    g_snprintf(buffer, sizeof(buffer), "%1.1f %s",
                   xreal/format->magnitude,
                   format->units);
    labels->values[0] = g_strdup(buffer); /* $X */

    gwy_si_unit_get_format_with_resolution(
                     si_unit,
                     yreal,
                     yreal,
                     format);
    g_snprintf(buffer, sizeof(buffer), "%1.1f %s",
                   yreal/format->magnitude,
                   format->units);
    labels->values[1] = g_strdup(buffer); /* $Y */

    gwy_si_unit_get_format_with_resolution(
                     si_unit,
                     data_min,
                     data_min,
                     format);
    g_snprintf(buffer, sizeof(buffer), "%1.0f %s",
                   data_min/format->magnitude,
                   format->units);
    labels->values[2] = g_strdup(buffer); /* $MIN */

    gwy_si_unit_get_format_with_resolution(
                     si_unit,
                     data_max,
                     data_max,
                     format);
    g_snprintf(buffer, sizeof(buffer), "%1.0f %s",
                   data_max/format->magnitude,
                   format->units);
    labels->values[3] = g_strdup(buffer); /* $MAX */

    gwy_si_unit_value_format_free(format);

}

/**
 * gwy_3d_labels_format_text:
 *
 *
 *
 * Since: 1.5
 **/
gchar * gwy_3d_labels_format_text(Gwy3DLabels * labels, Gwy3DLabelName label_name)
{
#   define LABEL_BUFFER_SIZE 500
    gchar buffer[LABEL_BUFFER_SIZE];
    char *i, *j, *k = NULL;
    int p;
    gchar * lb;

    lb = gwy_3d_labels_get_description(labels, label_name)->text;
    g_return_val_if_fail(lb != NULL, NULL);

    gwy_debug("text:%s" , lb);
    for (i = lb, j = buffer ; *i != '\0'; ++i, ++j)
    {
        if (*i != '$')
            *j = *i;
        else
        {
            for (p = 0; p < labels->variables_count; p++)
                if (g_ascii_strncasecmp(i+1, labels->keys[p], strlen(labels->keys[p])) == 0)
                {
                   k = labels->values[p];
                   i += strlen(labels->keys[p]) ;
                   break;
                }
            if (k != NULL)
            {
                for ( ; *k != '\0' && (j-lb < LABEL_BUFFER_SIZE - 1); ++k, ++j)
                    *j = *k;
                k = NULL;
                --j;
            }
        }
        if (j-lb > LABEL_BUFFER_SIZE - 1)
        {
            j = lb + LABEL_BUFFER_SIZE - 1;
            break;
        }
    }
    *j = '\0';

    g_free(labels->text);
    labels->text = g_strdup(buffer);
#   undef LABEL_BUFFER_SIZE
    return labels->text;
}


/**
 * gwy_3d_label_description_set_text:
 *
 *
 *
 * Since: 1.5
 **/
void
gwy_3d_label_description_set_text(Gwy3DLabelDescription * label_description,
                                  const gchar* text)
{
    g_free(label_description->text);
    label_description->text = g_strdup(text);
    g_signal_emit_by_name(label_description->delta_x, "value_changed", 0);
}

static void
gwy_3d_labels_adjustment_value_changed(GtkAdjustment* adjustment, gpointer user_data)
{
    Gwy3DLabels * labels;

    gwy_debug(" ");
    g_return_if_fail(GWY_IS_3D_LABELS(user_data));
    labels =  (Gwy3DLabels*) user_data;

    g_signal_emit(labels, labels_signals[LABEL_CHANGED], 0);
}

