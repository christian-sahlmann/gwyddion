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
#include <libprocess/stats.h>
#include "gwy3dlabels.h"

#define GWY_3D_LABEL_GROUP_TYPE_NAME "Gwy3DLabelGroup"

enum {
    LABEL_CHANGED,
    LAST_SIGNAL
};

typedef struct {
    Gwy3DLabel *label;
    gchar *value;
} LabelData;

static void    gwy_3d_label_group_class_init (Gwy3DLabelGroupClass *klass);
static void    gwy_3d_label_group_init       (Gwy3DLabelGroup *label_group);
static void    gwy_3d_label_group_finalize   (GObject *object);
static void    gwy_3d_label_group_destroy    (GtkObject *object);

static GObjectClass *parent_class = NULL;
static guint label_group_signals[LAST_SIGNAL] = { 0 };
static GQuark container_key_quark = 0;

GType
gwy_3d_label_group_get_type(void)
{
    static GType gwy_3d_label_group_type = 0;

    if (!gwy_3d_label_group_type) {
        static const GTypeInfo gwy_3d_label_group_info = {
            sizeof(Gwy3DLabelGroupClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_3d_label_group_class_init,
            NULL,
            NULL,
            sizeof(Gwy3DLabelGroup),
            0,
            (GInstanceInitFunc)gwy_3d_label_group_init,
            NULL,
        };

        gwy_3d_label_group_type
            = g_type_register_static(GTK_TYPE_OBJECT,
                                     GWY_3D_LABEL_GROUP_TYPE_NAME,
                                     &gwy_3d_label_group_info,
                                     0);
    }

    return gwy_3d_label_group_type;
}


static void
gwy_3d_label_group_class_init(Gwy3DLabelGroupClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_3d_label_group_finalize;

    object_class->destroy = gwy_3d_label_group_destroy;

    klass->label_changed = NULL;

    label_group_signals[LABEL_CHANGED] =
        g_signal_new("label_changed",
                  G_OBJECT_CLASS_TYPE(klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                  G_STRUCT_OFFSET(Gwy3DLabelGroupClass, label_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);
}

static void
gwy_3d_label_group_free_data(LabelData *ld)
{
    g_object_unref(ld->label);
    g_free(ld->value);
    g_free(ld);
}

static void
gwy_3d_label_group_init(Gwy3DLabelGroup *label_group)
{
    label_group->labels = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                NULL,
                                                gwy_3d_label_group_free_data);
    label_group->variables = g_hash_table_new(g_str_hash, g_str_equal,
                                              NULL, g_free);
}

static void
gwy_3d_label_group_finalize(GObject *object)
{
    Gwy3DLabelGroup *label_group = (Gwy3DLabelGroup*)object;

    g_hash_table_destroy(label_group->variables);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_3d_label_group_finalize(GtkObject *object)
{
    Gwy3DLabelGroup *label_group = (Gwy3DLabelGroup*)object;

    g_hash_table_destroy(label_group->labels);
    gwy_object_unref(label_group->container);

    GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

/**
 * gwy_3d_label_group_new:
 * @container: A container to load/save label data from/to.
 *
 * Creates a new 3D label collection object.
 *
 * Returns: A new 3D label group.
 **/
Gwy3DLabelGroup*
gwy_3d_label_group_new(GwyContainer *container)
{
    struct {
        const gchar *key;
        const gchar *default_text;
    }
    labels[] = {
        { "x",   "x: $X", },
        { "y",   "y: $Y", },
        { "min", "$MIN",  },
        { "max", "$MAX",  },
    };
    Gwy3DLabelGroup *label_group;
    guint i;

    g_return_val_if_fail(GWY_IS_CONTAINER(container));

    label_group = g_object_new(GWY_TYPE_3D_LABEL_GROUP, NULL);

    g_object_ref(container);
    label_group->container = container;

    for (i = 0; i < G_N_ELEMENTS(labels); i++) {
        Gwy3DLabel *label;

        
        g_hash_table_insert(label_group->labels,
                            gwy_3d_label_new(labels[i].default_text));
    }


    label_group->label_group_count = 4;
    label_group->label_group = g_new(Gwy3DLabelDescription*, label_group->label_group_count);

    /* XXX: use symbolic constants GWY_3D_LABEL_FOO instead of numbers */
    label_group->label_group[0] = gwy_3d_label_description_new(
                            "x: $X", "/0/3d/label_group/x/", 0, 0, 0.0f, 1, label_group);
    label_group->label_group[1] = gwy_3d_label_description_new(
                            "y: $Y", "/0/3d/label_group/y/", 0, 0, 0.0f, 1, label_group);
    label_group->label_group[2] = gwy_3d_label_description_new(
                            "$MIN", "/0/3d/label_group/min/", 0, 0, 0.0f, 1, label_group);
    label_group->label_group[3] = gwy_3d_label_description_new(
                            "$MAX", "/0/3d/label_group/max/", 0, 0, 0.0f, 1, label_group);

    label_group->variables_count = 4;
    label_group->keys   = g_new(gchar*, label_group->variables_count);
    label_group->values = g_new(gchar*, label_group->variables_count);
    label_group->keys[0] = g_strdup("X");
    label_group->keys[1] = g_strdup("Y");
    label_group->keys[2] = g_strdup("MIN");
    label_group->keys[3] = g_strdup("MAX");
    for (i =0; i < label_group->variables_count; i++)
       label_group->values[i] = NULL;

    label_group->text     = NULL;

    return label_group;
}

/**
 * gwy_3d_label_group_get_description:
 * @gwy3dlabel_group: 3D label collection object
 * @label_name: identifier of the label
 *
 *
 * Returns: a #Gwy3DLabelDescription structure identified by @name from the
 *          @gwy3dlabel_group
 *
 * Since: 1.5
 **/
Gwy3DLabelDescription *
gwy_3d_label_group_get_description(Gwy3DLabelGroup *gwy3dlabel_group,
                              Gwy3DLabelName label_name)
{

    g_return_val_if_fail(GWY_IS_3D_LABEL_GROUP(gwy3dlabel_group), NULL);
    g_return_val_if_fail(label_name <= GWY_3D_VIEW_LABEL_MAX, NULL);

    return gwy3dlabel_group->label_group[label_name];
}

/**
 * gwy_3d_label_group_get_update:
 * @label_group: 3D label collection object
 * @si_unit: SI Unit appended to the numerical values of variables in @text
 *          field of #Gwy3DLabelDescription
 *
 * Updates the numerical values of the variables in @text field of
 * #Gwy3DLabelDescription. Also appends the SI unit to this numerical values.
 **/
void
gwy_3d_label_group_update(Gwy3DLabelGroup *label_group,
                     GwySIUnit *si_unit)
{
    GwySIValueFormat *format;
    gchar buffer[50];
    gdouble xreal, yreal, data_min, data_max, range, maximum;
    GwyDataField *data_field = NULL;
    int i;

    gwy_debug(" ");

    if (!gwy_container_gis_object_by_name(label_group->container,
                                          "/0/data",
                                          (GObject**)&data_field)) {
        g_critical("No data field");
        return;
    }

    for (i = 0; i < label_group->variables_count; i++)
       g_free(label_group->values[i]);

    xreal    = gwy_data_field_get_xreal(data_field);
    yreal    = gwy_data_field_get_yreal(data_field);
    data_min = gwy_data_field_get_min(data_field);
    data_max = gwy_data_field_get_max(data_field);
    range = fabs(data_max - data_min);
    maximum = MAX(fabs(data_min), fabs(data_max));

    format = gwy_si_unit_get_format_with_resolution(
                     si_unit,
                     xreal,
                     xreal/3,
                     NULL);
    g_snprintf(buffer, sizeof(buffer), "%1.1f %s",
                   xreal/format->magnitude,
                   format->units);
    label_group->values[0] = g_strdup(buffer); /* $X */

    gwy_si_unit_get_format_with_resolution(
                     si_unit,
                     yreal,
                     yreal/3,
                     format);
    g_snprintf(buffer, sizeof(buffer), "%1.1f %s",
                   yreal/format->magnitude,
                   format->units);
    label_group->values[1] = g_strdup(buffer); /* $Y */

    gwy_si_unit_get_format_with_resolution(
                     si_unit,
                     maximum,
                     range/3,
                     format);
    g_snprintf(buffer, sizeof(buffer), "%1.0f %s",
                   data_min/format->magnitude,
                   format->units);
    label_group->values[2] = g_strdup(buffer); /* $MIN */

    g_snprintf(buffer, sizeof(buffer), "%1.0f %s",
                   data_max/format->magnitude,
                   format->units);
    label_group->values[3] = g_strdup(buffer); /* $MAX */

    gwy_si_unit_value_format_free(format);

}

/**
 * gwy_3d_label_group_expand_text:
 * @label_group: a 3D label_group collection object.
 * @label_name: identifier of the label
 *
 * Expands the variables within @text field of #Gwy3DLabelDescription to
 * the numerical values. The LabelDescription is identified by @label_name.
 *
 * Returns: The @text field of #Gwy3DLabelDescription after expanding varibles
 *          to their numerical values. The returned string should not be modied,
 *          freed of referenced. It is freed during next call of this function
 *          or during destroying @label_group.
 **/
gchar*
gwy_3d_label_group_expand_text(Gwy3DLabelGroup *label_group,
                          Gwy3DLabelName label_name)
{
    GString *buffer;
    gchar *s, *lb;
    guint i, len;

    lb = gwy_3d_label_group_get_description(label_group, label_name)->text;
    g_return_val_if_fail(lb != NULL, NULL);

    gwy_debug("text: <%s>", lb);
    buffer = g_string_new("");
    while (lb && *lb) {
        if (!(s = strchr(lb, '$'))) {
            g_string_append(buffer, lb);
            break;
        }
        g_string_append_len(buffer, lb, s - lb);
        lb = s + 1;

        for (i = 0; i < label_group->variables_count; i++) {
            len = strlen(label_group->keys[i]);
            if (!g_ascii_strncasecmp(lb, label_group->keys[i], len)
                && !g_ascii_isalpha(lb[len])) {
                g_string_append(buffer, label_group->values[i]);
                lb += len;
                break;
            }
        }
        if (i == label_group->variables_count)
            g_string_append_c(buffer, '$');
    }

    g_free(label_group->text);
    label_group->text = buffer->str;
    g_string_free(buffer, FALSE);

    return label_group->text;
}


/**
 * gwy_3d_label_description_set_text:
 * @label_description: A label desctiption.
 * @text: new value of @text field
 *
 * Sets the @text field of @label_description to the value of %text.
 *
 * Emits the "label_changed" signal.
 *
 * Since: 1.5
 **/
void
gwy_3d_label_description_set_text(Gwy3DLabelDescription * ld,
                                  const gchar* text)
{
    gchar * buffer = g_new(gchar, strlen(ld->base_key) + 20);
    gchar * end;

    strcpy(buffer, ld->base_key);
    end = buffer + strlen(buffer);
    strcpy(end, "text");
    g_free(ld->text);
    ld->text = g_strdup(text);
    gwy_container_set_string_by_name(ld->owner->container,
                                     buffer, g_strdup(ld->text));

    g_free(buffer);
    g_signal_emit(ld->owner, label_group_signals[LABEL_CHANGED], 0);
}

/**
 * gwy_3d_label_description_reset:
 * @label_description: A label desctiption.
 *
 * Resets the values of the @text, @delta_x, @delta_y, @size and @rotation
 * fields of @label_description to their default vaules. @uuto_scale field is
 * set to TRUE.
 *
 * Since: 1.5
 **/
void
gwy_3d_label_description_reset(Gwy3DLabelDescription * label_description)
{
    gwy_debug(" ");
    gwy_3d_label_description_set_text(label_description,
                                      label_description->default_text);
    gwy_3d_label_description_set_autoscale(label_description, TRUE);
    gtk_adjustment_set_value(label_description->delta_x, 0);
    gtk_adjustment_set_value(label_description->delta_y, 0);
    gtk_adjustment_set_value(label_description->size, 14);
    gtk_adjustment_set_value(label_description->rotation, 0);
}

static void
gwy_3d_label_group_adjustment_value_changed(GtkAdjustment* adjustment, gpointer user_data)
{
    Gwy3DLabelGroup * label_group;
    GQuark quark;

    gwy_debug(" ");
    g_return_if_fail(GWY_IS_3D_LABEL_GROUP(user_data));
    label_group =  (Gwy3DLabelGroup*) user_data;
    if ((quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(adjustment),
                                                     container_key_quark))))
        gwy_container_set_double(label_group->container, quark,
                                 gtk_adjustment_get_value(adjustment));

    g_signal_emit(label_group, label_group_signals[LABEL_CHANGED], 0);
}

/**
 * gwy_3d_label_group_user_size:
 * @label_group: a 3D label_group collection object
 * @name: identifier of the label
 * @user_size: automatically calculated size of the text
 *
 * According @auto_scale decides whether to use @user_size or @size->value
 * size of the 3D View label text.
 *
 * Returns: size of the label text in 3D View
 **/
gdouble
gwy_3d_label_group_user_size(Gwy3DLabelGroup *label_group,
                        Gwy3DLabelName name,
                        gint user_size)
{
    Gwy3DLabelDescription * ld = gwy_3d_label_group_get_description(label_group, name);
    if (ld->auto_scale)
    {
        gtk_adjustment_set_value(ld->size, user_size);
        return user_size;
    } else
        return ld->size->value;
}

/**
 * gwy_3d_label_description_get_autoscale:
 * @label_description: A label desctiption.
 *
 *
 * Returns: value of @auto_scale
 *
 * Since: 1.5
 **/
gboolean
gwy_3d_label_description_get_autoscale(Gwy3DLabelDescription* label_description)
{
    return label_description->auto_scale;
}

/**
 * gwy_3d_label_description_set_autoscale:
 * @label_description: A label desctiption.
 * @autoscale: new value of %auto_scale field
 *
 * Sets whether use automatically calculated size of label text or whether  use
 * value of @size->value.
 *
 * Emits "label_changed" signal.
 *
 * Since: 1.5
 **/
void
gwy_3d_label_description_set_autoscale(Gwy3DLabelDescription * ld,
                                       const gboolean autoscale)
{
    gchar * buffer = g_new(gchar, strlen(ld->base_key) + 20);
    gchar * end;

    strcpy(buffer, ld->base_key);
    end = buffer + strlen(buffer);
    strcpy(end, "auto_scale");

    ld->auto_scale = autoscale;
    gwy_container_set_boolean_by_name(ld->owner->container,
                                      buffer, ld->auto_scale);
    g_free(buffer);
    g_signal_emit(ld->owner, label_group_signals[LABEL_CHANGED], 0);
}

/************************** Documentation ****************************/
/**
 * Gwy3DLabelName:
 * @GWY_3D_VIEW_LABEL_X: Identifier of the label describing x-axis
 * @GWY_3D_VIEW_LABEL_Y: Identifier of the label describing y-axis
 * @GWY_3D_VIEW_LABEL_MIN: Identifier of the label
 *                         describing minimal z-value
 * @GWY_3D_VIEW_LABEL_MAX: Identifier of the label
 *                         describing maximal z-value
 *
 * Identifiers of the label_group of the axes within 3D View.
 *
 * Since 1.5
 **/

/**
 * Gwy3DLabelDescription:
 * @base_key: Path to the node within GwyContainer storing the informations
 *            about this LabelDescription.
 *            This field should be considered private and should not be modified.
 * @default_text: Value to which the text would be set after calling
 *                @gwy_3d_label_description_reset.
 * @text: Text of the label. May contain names of the variables. Each word
 *        beginning with $ is considered to be a variable. Currently following
 *        variables can be used: %$X, %$Y, %$MIN and %$MAX. To set the value
 *        of @text use @gwy_3d_label_description_set_text.
 * @auto_scale: Whether the size of text should be calculated automatically or
 *              use the value obtained through @size field and
 *              @gwy_3d_label_group_user_size mathod. To obratin this value it is
 *              possible to use @gwy_3d_label_description_get_autoscale method
 *              and to set @auto_scale use
 *              @gwy_3d_label_description_set_autoscale method.
 * @delta_x: Horizontal displacement of the label within 3DView. The label text
 *          is moved by  @delta_x->value pixels from the calculated point.
 * @delta_y: Vertical displacement of the label within 3DView. The label text
 *          is moved by  @delta_y->value pixels from the calculated point.
 * @rotation: Rotation of the label text within 3DView. The rotations are not
 *           implemented yet.
 * @size: The size of the label text within 3DView (only if @auto_scale == %FALSE)
 * @owner: A pointer to the Gwy3DLabelGroup that created this description.
 *        This field should be considered private and should not be modified.
 *
 * The structure contains informations about individual label_group taking place in
 * the 3DView.
 *
 * This structure is created and owned by Gwy3DLabes class.
 * Note that some of the fields are considered as private and should not
 * be modified by user. Some other fields have the set methods which
 * should be used instead of direct settings
 * of values. Changes of @text, @auto_scale, @delta_x, @delta_x, @rotation or
 * @size field causes emitting the "label_cghanged" signal.
 *
 * Since 1.5
 **/

/**
 * gwy_3d_label_group_get_delta_x:
 * @label_group: a 3D label_group collection object
 * @name: identifier of the label
 *
 * Gets the value of %delta_x displacement (%delta_x->value) of the label
 * identified by %name.
 *
 * Since 1.5
 **/

/**
 * gwy_3d_label_group_get_delta_y:
 * @label_group: a 3D label_group collection object
 * @name: identifier of the label
 *
 * Gets the value of %delta_y displacement (%delta_y->value) of the label
 * identified by %name.
 *
 * Since 1.5
 **/

/**
 * gwy_3d_label_group_get_rotation:
 * @label_group: a 3D label_group collection object
 * @name: identifier of the label
 *
 * Gets the value of angle of rotation (%rotation->value) of the label
 * identified by %name.
 *
 * Since 1.5
 **/

/**
 * gwy_3d_label_group_get_size:
 * @label_group: a 3D label_group collection object
 * @name: identifier of the label
 *
 * Gets the value of label text size (%size->value) of the label
 * identified by %name. Does not take @auto_scale into account.
 *
 * Since 1.5
 **/

