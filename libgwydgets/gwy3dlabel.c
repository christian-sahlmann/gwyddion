/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwywatchable.h>
#include <libgwyddion/gwydebugobjects.h>
#include "gwy3dlabel.h"

#define GWY_3D_LABEL_TYPE_NAME "Gwy3DLabel"

enum {
    PROP_0,
    PROP_DELTA_X,
    PROP_DELTA_Y,
    PROP_SIZE,
    PROP_FIXED_SIZE,
    PROP_DEFAULT_TEXT,
    PROP_LAST
};

static void        gwy_3d_label_class_init        (Gwy3DLabelClass *klass);
static void        gwy_3d_label_init              (Gwy3DLabel *label);
static void        gwy_3d_label_finalize          (GObject *object);
static void        gwy_3d_label_serializable_init (GwySerializableIface *iface);
static void        gwy_3d_label_watchable_init    (GwyWatchableIface *iface);
static GByteArray* gwy_3d_label_serialize         (GObject *obj,
                                                   GByteArray *buffer);
static GObject*    gwy_3d_label_deserialize       (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject*    gwy_3d_label_duplicate_real    (GObject *object);
static void        gwy_3d_label_set_property      (GObject *object,
                                                   guint prop_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void        gwy_3d_label_get_property      (GObject*object,
                                                   guint prop_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static GtkAdjustment* gwy_3d_label_create_adjustment (Gwy3DLabel *label,
                                                      const gchar *property_id,
                                                      gdouble step,
                                                      gdouble page);
static void        gwy_3d_label_adj_value_changed (Gwy3DLabel *label,
                                                   GtkAdjustment *adj);
static void        gwy_3d_label_adj_changed       (Gwy3DLabel *label,
                                                   GtkAdjustment *adj);

static GObjectClass *parent_class = NULL;

GType
gwy_3d_label_get_type(void)
{
    static GType gwy_3d_label_type = 0;

    if (!gwy_3d_label_type) {
        static const GTypeInfo gwy_3d_label_info = {
            sizeof(Gwy3DLabelClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_3d_label_class_init,
            NULL,
            NULL,
            sizeof(Gwy3DLabel),
            0,
            (GInstanceInitFunc)gwy_3d_label_init,
            NULL,
        };

        GInterfaceInfo gwy_serializable_info = {
            (GInterfaceInitFunc)gwy_3d_label_serializable_init,
            NULL,
            NULL
        };
        GInterfaceInfo gwy_watchable_info = {
            (GInterfaceInitFunc)gwy_3d_label_watchable_init,
            NULL,
            NULL
        };

        gwy_debug("");
        gwy_3d_label_type = g_type_register_static(G_TYPE_OBJECT,
                                                   GWY_3D_LABEL_TYPE_NAME,
                                                   &gwy_3d_label_info,
                                                   0);
        g_type_add_interface_static(gwy_3d_label_type,
                                    GWY_TYPE_SERIALIZABLE,
                                    &gwy_serializable_info);
        g_type_add_interface_static(gwy_3d_label_type,
                                    GWY_TYPE_WATCHABLE,
                                    &gwy_watchable_info);
    }

    return gwy_3d_label_type;
}

static void
gwy_3d_label_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_3d_label_serialize;
    iface->deserialize = gwy_3d_label_deserialize;
    iface->duplicate = gwy_3d_label_duplicate_real;
}

static void
gwy_3d_label_watchable_init(GwyWatchableIface *iface)
{
    iface->value_changed = NULL;
}

static void
gwy_3d_label_class_init(Gwy3DLabelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_3d_label_finalize;
    gobject_class->set_property = gwy_3d_label_set_property;
    gobject_class->get_property = gwy_3d_label_get_property;

    g_object_class_install_property
        (gobject_class,
         PROP_DELTA_X,
         g_param_spec_double("delta_x",
                             "Horizontal offset",
                             "Horizontal label offset, in pixels",
                             -1000, 1000, 0.0, G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class,
         PROP_DELTA_Y,
         g_param_spec_double("delta_y",
                             "Vertical offset",
                             "Vertical label offset, in pixels",
                             -1000, 1000, 0.0, G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class,
         PROP_SIZE,
         g_param_spec_double("size",
                             "Font size",
                             "Label font size",
                             1.0, 100.0, 14.0, G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class,
         PROP_FIXED_SIZE,
         g_param_spec_boolean("fixed_size",
                              "Fixed size",
                              "Whether label size is fixed and doesn't scale",
                              FALSE, G_PARAM_READWRITE));
    g_object_class_install_property
        (gobject_class,
         PROP_DEFAULT_TEXT,
         g_param_spec_string("default_text",
                             "Default text",
                             "Default label text",
                             "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gwy_3d_label_init(Gwy3DLabel *label)
{
    gwy_debug_objects_creation((GObject*)label);
}

static void
gwy_3d_label_finalize(GObject *object)
{
    Gwy3DLabel *label = (Gwy3DLabel*)object;

    g_free(label->default_text);
    if (label->text)
        g_string_free(label->text, TRUE);
    gwy_object_unref(label->delta_x);
    gwy_object_unref(label->delta_y);
    gwy_object_unref(label->size);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static GByteArray*
gwy_3d_label_serialize(GObject *obj,
                       GByteArray *buffer)
{
    Gwy3DLabel *label;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_3D_LABEL(obj), NULL);

    label = GWY_3D_LABEL(obj);
    {
        GwySerializeSpec spec[] = {
            { 's', "text", &label->text->str, NULL, },
            { 's', "default_text", &label->default_text, NULL, },
            { 'b', "fixed_size", &label->fixed_size, NULL, },
            { 'd', "delta_x", &label->delta_x->value, NULL, },
            { 'd', "delta_y", &label->delta_y->value, NULL, },
            { 'd', "size", &label->size->value, NULL, },
        };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_3D_LABEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_3d_label_deserialize(const guchar *buffer,
                         gsize size,
                         gsize *position)
{
    gboolean fixed_size;
    gdouble delta_x = 0.0, delta_y = 0.0, scale = 14.0;
    gchar *text = NULL, *default_text = NULL;
    Gwy3DLabel *label;
    GwySerializeSpec spec[] = {
        { 's', "text", &text, NULL, },
        { 's', "default_text", &default_text, NULL, },
        { 'b', "fixed_size", &fixed_size, NULL, },
        { 'd', "delta_x", &delta_x, NULL, },
        { 'd', "delta_y", &delta_y, NULL, },
        { 'd', "size", &scale, NULL, },
    };

    g_return_val_if_fail(buffer, NULL);
    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_3D_LABEL_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(text);
        g_free(default_text);
        return NULL;
    }

    label = gwy_3d_label_new(default_text);
    g_object_freeze_notify(G_OBJECT(label));
    label->fixed_size = fixed_size;
    gtk_adjustment_set_value(label->delta_x, delta_x);
    gtk_adjustment_set_value(label->delta_y, delta_y);
    gtk_adjustment_set_value(label->size, scale);
    gwy_3d_label_set_text(label, text);
    g_object_thaw_notify(G_OBJECT(label));

    g_free(default_text);
    g_free(text);

    return (GObject*)label;
}

static GObject*
gwy_3d_label_duplicate_real(GObject *object)
{
    Gwy3DLabel *label, *duplicate;

    g_return_val_if_fail(GWY_IS_3D_LABEL(object), NULL);
    label = GWY_3D_LABEL(object);
    duplicate = gwy_3d_label_new(label->default_text);
    g_object_freeze_notify(G_OBJECT(label));
    duplicate->fixed_size = label->fixed_size;
    gtk_adjustment_set_value(duplicate->delta_x, label->delta_x->value);
    gtk_adjustment_set_value(duplicate->delta_y, label->delta_y->value);
    gtk_adjustment_set_value(duplicate->size, label->size->value);
    gwy_3d_label_set_text(duplicate, label->text->str);
    g_object_thaw_notify(G_OBJECT(label));

    return (GObject*)duplicate;
}

static void
gwy_3d_label_set_property(GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    Gwy3DLabel *label = GWY_3D_LABEL(object);

    switch (prop_id) {
        case PROP_DELTA_X:
        gtk_adjustment_set_value(label->delta_x, g_value_get_double(value));
        break;

        case PROP_DELTA_Y:
        gtk_adjustment_set_value(label->delta_y, g_value_get_double(value));
        break;

        case PROP_SIZE:
        gtk_adjustment_set_value(label->size, g_value_get_double(value));
        break;

        case PROP_FIXED_SIZE:
        gwy_3d_label_set_fixed_size(label, g_value_get_boolean(value));
        break;

        case PROP_DEFAULT_TEXT:
        label->default_text = g_value_dup_string(value);
        g_object_notify(G_OBJECT(label), "default_text");
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_3d_label_get_property(GObject*object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    Gwy3DLabel *label = GWY_3D_LABEL(object);

    switch (prop_id) {
        case PROP_DELTA_X:
        g_value_set_double(value, label->delta_x->value);
        break;

        case PROP_DELTA_Y:
        g_value_set_double(value, label->delta_y->value);
        break;

        case PROP_SIZE:
        g_value_set_double(value, label->size->value);
        break;

        case PROP_FIXED_SIZE:
        g_value_set_boolean(value, label->fixed_size);
        break;

        case PROP_DEFAULT_TEXT:
        g_value_set_string(value, label->default_text);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

Gwy3DLabel*
gwy_3d_label_new(const gchar *default_text)
{
    Gwy3DLabel *label;

    label = g_object_new(GWY_TYPE_3D_LABEL, "default_text", default_text, NULL);
    label->delta_x = gwy_3d_label_create_adjustment(label, "delta_x", 1, 10);
    label->delta_y = gwy_3d_label_create_adjustment(label, "delta_y", 1, 10);
    label->size = gwy_3d_label_create_adjustment(label, "size", 1, 5);
    label->text = g_string_new(default_text);

    return label;
}

static GtkAdjustment*
gwy_3d_label_create_adjustment(Gwy3DLabel *label,
                               const gchar *property_id,
                               gdouble step,
                               gdouble page)
{
    GObjectClass *klass;
    GtkAdjustment *adj;
    GParamSpecDouble *pspec;

    klass = G_OBJECT_CLASS(GWY_3D_LABEL_GET_CLASS(label));
    pspec = G_PARAM_SPEC_DOUBLE(g_object_class_find_property(klass,
                                                             property_id));
    adj = GTK_ADJUSTMENT(gtk_adjustment_new(pspec->default_value,
                                            pspec->minimum,
                                            pspec->maximum,
                                            step, page, 0));
    g_object_set_data(G_OBJECT(adj), "gwy-3d-label-property_id",
                      (gpointer)property_id);
    g_signal_connect_swapped(adj, "value_changed",
                             G_CALLBACK(gwy_3d_label_adj_value_changed), label);
    g_signal_connect_swapped(adj, "changed",
                             G_CALLBACK(gwy_3d_label_adj_changed), label);

    return adj;
}

static void
gwy_3d_label_adj_value_changed(Gwy3DLabel *label,
                               GtkAdjustment *adj)
{
    g_object_notify(G_OBJECT(label),
                    g_object_get_data(G_OBJECT(adj),
                                      "gwy-3d-label-property_id"));
}

static void
gwy_3d_label_adj_changed(G_GNUC_UNUSED Gwy3DLabel *label,
                         G_GNUC_UNUSED GtkAdjustment *adj)
{
    g_warning("Changing 3D Label's adjustment parameters is not supported.");
}

void
gwy_3d_label_set_text(Gwy3DLabel *label,
                      const gchar *text)
{
    g_return_if_fail(GWY_IS_3D_LABEL(label));
    g_string_assign(label->text, text);
    g_signal_emit_by_name(label, "value_changed");
}

const gchar*
gwy_3d_label_get_text(Gwy3DLabel *label)
{
    g_return_val_if_fail(GWY_IS_3D_LABEL(label), NULL);
    return label->text->str;
}

/**
 * gwy_3d_label_expand_text:
 * @label: A 3D label.
 * @variables: Hash table with variable values.
 *
 * Substitutes variables in label text.
 *
 * Returns: A newly allocated string with variables from @variables substituted
 *          with values.
 **/
gchar*
gwy_3d_label_expand_text(Gwy3DLabel *label,
                         GHashTable *variables)
{
    GString *buffer, *key;
    gchar *s, *lb;

    g_return_val_if_fail(GWY_IS_3D_LABEL(label), NULL);
    lb = label->text->str;

    buffer = g_string_new("");
    key = g_string_new("");
    while (lb && *lb) {
        if (!(s = strchr(lb, '$'))) {
            g_string_append(buffer, lb);
            break;
        }
        g_string_append_len(buffer, lb, s - lb);
        lb = s + 1;
        if (!g_ascii_isalpha(*lb)) {
            g_string_append_c(buffer, '$');
            continue;
        }

        for (s = lb; g_ascii_isalpha(*s); s++)
            ;
        g_string_append_len(g_string_truncate(key, 0), lb, s-lb);
        g_string_ascii_down(key);
        s = g_hash_table_lookup(variables, key->str);
        if (s) {
            g_string_append(buffer, s);
            lb += strlen(key->str);
        }
        else
            g_string_append_c(buffer, '$');
    }

    s = buffer->str;
    g_string_free(buffer, FALSE);
    g_string_free(key, TRUE);

    return s;
}
void
gwy_3d_label_reset(Gwy3DLabel *label)
{
    g_return_if_fail(GWY_IS_3D_LABEL(label));
    g_object_freeze_notify(G_OBJECT(label));
    gtk_adjustment_set_value(label->delta_x, 0.0);
    gtk_adjustment_set_value(label->delta_y, 0.0);
    gtk_adjustment_set_value(label->size, 14.0);
    gwy_3d_label_set_fixed_size(label, FALSE);
    gwy_3d_label_set_text(label, label->default_text);
    g_object_thaw_notify(G_OBJECT(label));
}

void
gwy_3d_label_reset_text(Gwy3DLabel *label)
{
    g_return_if_fail(GWY_IS_3D_LABEL(label));
    gwy_3d_label_set_text(label, label->default_text);
}

gboolean
gwy_3d_label_get_fixed_size(Gwy3DLabel *label)
{
    g_return_val_if_fail(GWY_IS_3D_LABEL(label), FALSE);
    return label->fixed_size;
}

void
gwy_3d_label_set_fixed_size(Gwy3DLabel *label,
                            gboolean fixed_size)
{
    g_return_if_fail(GWY_IS_3D_LABEL(label));
    label->fixed_size = !!fixed_size;
    g_object_notify(G_OBJECT(label), "fixed_size");
}

/**
 * gwy_3d_label_user_size:
 * @label: A 3D label.
 * @user_size: Size of the text to be set.
 *
 * Eventually sets size of a 3D label.
 *
 * If label size si fixed, it does not change and it is simply returned.
 * Otherwise label size is changed and @user_size itself is returned.
 *
 * Returns: Size of label.
 **/
gdouble
gwy_3d_label_user_size(Gwy3DLabel *label,
                       gdouble user_size)
{
    g_return_val_if_fail(GWY_IS_3D_LABEL(label), 0.0);
    if (label->fixed_size)
        return label->size->value;

    gtk_adjustment_set_value(label->size, user_size);
    return user_size;
}

GtkAdjustment*
gwy_3d_label_get_delta_x_adjustment(Gwy3DLabel *label)
{
    g_return_val_if_fail(GWY_IS_3D_LABEL(label), NULL);
    return label->delta_x;
}

GtkAdjustment*
gwy_3d_label_get_delta_y_adjustment(Gwy3DLabel *label)
{
    g_return_val_if_fail(GWY_IS_3D_LABEL(label), NULL);
    return label->delta_y;
}

GtkAdjustment*
gwy_3d_label_get_size_adjustment(Gwy3DLabel *label)
{
    g_return_val_if_fail(GWY_IS_3D_LABEL(label), NULL);
    return label->size;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
