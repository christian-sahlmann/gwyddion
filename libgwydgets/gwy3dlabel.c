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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwydgets/gwy3dlabel.h>

#define GWY_3D_LABEL_TYPE_NAME "Gwy3DLabel"

#define PSPEC(id, type) GWY_FIND_PSPEC(GWY_TYPE_3D_LABEL, id, type)

enum {
    PROP_0,
    PROP_DELTA_X,
    PROP_DELTA_Y,
    PROP_ROTATION,
    PROP_SIZE,
    PROP_FIXED_SIZE,
    PROP_TEXT,
    PROP_DEFAULT_TEXT
};

static void        gwy_3d_label_finalize         (GObject *object);
static void        gwy_3d_label_serializable_init(GwySerializableIface *iface);
static GByteArray* gwy_3d_label_serialize        (GObject *obj,
                                                  GByteArray *buffer);
static gsize       gwy_3d_label_get_size         (GObject *obj);
static GObject*    gwy_3d_label_deserialize      (const guchar *buffer,
                                                  gsize size,
                                                  gsize *position);
static GObject*    gwy_3d_label_duplicate_real   (GObject *object);
static void        gwy_3d_label_clone_real       (GObject *source,
                                                  GObject *copy);
static void        gwy_3d_label_set_property     (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void        gwy_3d_label_get_property     (GObject*object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);

G_DEFINE_TYPE_EXTENDED
    (Gwy3DLabel, gwy_3d_label, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_3d_label_serializable_init))

static void
gwy_3d_label_serializable_init(GwySerializableIface *iface)
{
    iface->serialize = gwy_3d_label_serialize;
    iface->deserialize = gwy_3d_label_deserialize;
    iface->get_size = gwy_3d_label_get_size;
    iface->duplicate = gwy_3d_label_duplicate_real;
    iface->clone = gwy_3d_label_clone_real;
}

static void
gwy_3d_label_class_init(Gwy3DLabelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_3d_label_finalize;
    gobject_class->set_property = gwy_3d_label_set_property;
    gobject_class->get_property = gwy_3d_label_get_property;

    /**
     * Gwy3DLabel:delta-x:
     *
     * The :delta-x property represents horizontal label offset in pixels
     * (in screen coordinates after mapping from 3D to 2D).
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_DELTA_X,
         g_param_spec_double("delta-x",
                             "Horizontal offset",
                             "Horizontal label offset, in pixels",
                             -1000, 1000, 0.0, G_PARAM_READWRITE));

    /**
     * Gwy3DLabel:delta-y:
     *
     * The :delta-y property represents vertical label offset in pixels
     * (in screen coordinates after mapping from 3D to 2D).
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_DELTA_Y,
         g_param_spec_double("delta-y",
                             "Vertical offset",
                             "Vertical label offset, in pixels",
                             -1000, 1000, 0.0, G_PARAM_READWRITE));

    /**
     * Gwy3DLabel:rotation:
     *
     * The :rotation property represents label rotation in radians,
     * counterclokwise (on screen, after mapping from 3D to 2D).
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_ROTATION,
         g_param_spec_double("rotation",
                             "Rotation",
                             "Label rotation in radians, counterclokwise",
                             -G_PI, G_PI, 0.0, G_PARAM_READWRITE));

    /**
     * Gwy3DLabel:size:
     *
     * The :size property represents label size in pixels.  When :fixed_size
     * is %FALSE, its value is overwritten with automatic size.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_SIZE,
         g_param_spec_double("size",
                             "Font size",
                             "Label font size",
                             1.0, 100.0, 14.0, G_PARAM_READWRITE));

    /**
     * Gwy3DLabel:fixed-size:
     *
     * The :fixed-size property controls whether the :size property is kept and
     * honoured, or conversely ignored and overwritten with automatic size.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_FIXED_SIZE,
         g_param_spec_boolean("fixed-size",
                              "Fixed size",
                              "Whether label size is fixed and doesn't scale",
                              FALSE, G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_TEXT,
         g_param_spec_string("text",
                             "Text",
                             "The label text template",
                             "", G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_DEFAULT_TEXT,
         g_param_spec_string("default-text",
                             "Default text",
                             "Default label text",
                             "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gwy_3d_label_init(Gwy3DLabel *label)
{
    gwy_debug_objects_creation(G_OBJECT(label));

    label->delta_x = PSPEC("delta-x", DOUBLE)->default_value;
    label->delta_y = PSPEC("delta-y", DOUBLE)->default_value;
    label->rotation = PSPEC("rotation", DOUBLE)->default_value;
    label->size = PSPEC("size", DOUBLE)->default_value;
    /* default text is a construction property */
    label->text = g_string_new(NULL);
}

static void
gwy_3d_label_finalize(GObject *object)
{
    Gwy3DLabel *label = (Gwy3DLabel*)object;

    g_free(label->default_text);
    if (label->text)
        g_string_free(label->text, TRUE);

    G_OBJECT_CLASS(gwy_3d_label_parent_class)->finalize(object);
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
            { 'd', "delta_x", &label->delta_x, NULL, },
            { 'd', "delta_y", &label->delta_y, NULL, },
            { 'd', "rotation", &label->rotation, NULL, },
            { 'd', "size", &label->size, NULL, },
            { 's', "text", &label->text->str, NULL, },
            { 's', "default_text", &label->default_text, NULL, },
            { 'b', "fixed_size", &label->fixed_size, NULL, },
        };
        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_3D_LABEL_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static gsize
gwy_3d_label_get_size(GObject *obj)
{
    Gwy3DLabel *label;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_3D_LABEL(obj), 0);

    label = GWY_3D_LABEL(obj);
    {
        GwySerializeSpec spec[] = {
            { 'd', "delta_x", &label->delta_x, NULL, },
            { 'd', "delta_y", &label->delta_y, NULL, },
            { 'd', "rotation", &label->rotation, NULL, },
            { 'd', "size", &label->size, NULL, },
            { 's', "text", &label->text->str, NULL, },
            { 's', "default_text", &label->default_text, NULL, },
            { 'b', "fixed_size", &label->fixed_size, NULL, },
        };
        return gwy_serialize_get_struct_size(GWY_3D_LABEL_TYPE_NAME,
                                             G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_3d_label_deserialize(const guchar *buffer,
                         gsize size,
                         gsize *position)
{
    gboolean fixed_size;
    gdouble delta_x, delta_y, scale, rotation;
    gchar *text = NULL, *default_text = NULL;
    Gwy3DLabel *label;
    GwySerializeSpec spec[] = {
        { 'd', "delta_x", &delta_x, NULL, },
        { 'd', "delta_y", &delta_y, NULL, },
        { 'd', "rotation", &rotation, NULL, },
        { 'd', "size", &scale, NULL, },
        { 's', "text", &text, NULL, },
        { 's', "default_text", &default_text, NULL, },
        { 'b', "fixed_size", &fixed_size, NULL, },
    };

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);
    delta_x = PSPEC("delta-x", DOUBLE)->default_value;
    delta_y = PSPEC("delta-y", DOUBLE)->default_value;
    rotation = PSPEC("rotation", DOUBLE)->default_value;
    scale = PSPEC("size", DOUBLE)->default_value;
    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_3D_LABEL_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)
        || !default_text) {
        g_free(text);
        g_free(default_text);
        return NULL;
    }

    label = gwy_3d_label_new(default_text);
    label->fixed_size = fixed_size;
    label->delta_x = delta_x;
    label->delta_y = delta_y;
    label->rotation = rotation;
    label->size = scale;
    g_string_assign(label->text, text);

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
    gwy_serializable_clone(G_OBJECT(label), G_OBJECT(duplicate));

    return (GObject*)duplicate;
}

static void
gwy_3d_label_clone_real(GObject *source,
                        GObject *copy)
{
    Gwy3DLabel *label, *clone;

    g_return_if_fail(GWY_IS_3D_LABEL(source));
    g_return_if_fail(GWY_IS_3D_LABEL(copy));

    label = GWY_3D_LABEL(source);
    clone = GWY_3D_LABEL(copy);

    if (!gwy_strequal(label->default_text, clone->default_text)) {
        g_warning("Trying to change construction-only property by cloning");
        g_free(clone->default_text);
        clone->default_text = g_strdup(label->default_text);
    }

    g_object_freeze_notify(copy);
    if (!clone->fixed_size != label->fixed_size) {
        clone->fixed_size = !!label->fixed_size;
        g_object_notify(copy, "fixed-size");
    }
    if (clone->delta_x != label->delta_x) {
        clone->delta_x = label->delta_x;
        g_object_notify(copy, "delta-x");
    }
    if (clone->delta_y != label->delta_y) {
        clone->delta_y = label->delta_y;
        g_object_notify(copy, "delta-y");
    }
    if (clone->rotation != label->rotation) {
        clone->rotation = label->rotation;
        g_object_notify(copy, "rotation");
    }
    if (clone->size != label->size) {
        clone->size = label->size;
        g_object_notify(copy, "size");
    }
    gwy_3d_label_set_text(clone, label->text->str);
    g_object_thaw_notify(copy);
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
        label->delta_x = g_value_get_double(value);
        break;

        case PROP_DELTA_Y:
        label->delta_y = g_value_get_double(value);
        break;

        case PROP_ROTATION:
        label->rotation = g_value_get_double(value);
        break;

        case PROP_SIZE:
        label->size = g_value_get_double(value);
        break;

        case PROP_FIXED_SIZE:
        label->fixed_size = g_value_get_boolean(value);
        break;

        case PROP_TEXT:
        g_string_assign(label->text, g_value_get_string(value));
        break;

        case PROP_DEFAULT_TEXT:
        /* This can happen only on construction */
        g_assert(!label->default_text);
        label->default_text = g_value_dup_string(value);
        g_assert(label->text);
        g_string_assign(label->text, label->default_text);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_3d_label_get_property(GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    Gwy3DLabel *label = GWY_3D_LABEL(object);

    switch (prop_id) {
        case PROP_DELTA_X:
        g_value_set_double(value, label->delta_x);
        break;

        case PROP_DELTA_Y:
        g_value_set_double(value, label->delta_y);
        break;

        case PROP_ROTATION:
        g_value_set_double(value, label->rotation);
        break;

        case PROP_SIZE:
        g_value_set_double(value, label->size);
        break;

        case PROP_FIXED_SIZE:
        g_value_set_boolean(value, label->fixed_size);
        break;

        case PROP_TEXT:
        g_value_set_string(value, label->text->str);
        break;

        case PROP_DEFAULT_TEXT:
        g_value_set_string(value, label->default_text);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_3d_label_new:
 * @default_text: Label default text.
 *
 * Creates a new 3D view label.
 *
 * Returns: A newly created 3D label.
 **/
Gwy3DLabel*
gwy_3d_label_new(const gchar *default_text)
{
    Gwy3DLabel *label;

    label = g_object_new(GWY_TYPE_3D_LABEL, "default-text", default_text, NULL);

    return label;
}

/**
 * gwy_3d_label_set_text:
 * @label: A 3D label.
 * @text: New label text.
 *
 * Sets the text of a 3D label.
 **/
void
gwy_3d_label_set_text(Gwy3DLabel *label,
                      const gchar *text)
{
    g_return_if_fail(GWY_IS_3D_LABEL(label));
    if (gwy_strequal(text, label->text->str))
        return;

    g_string_assign(label->text, text);
    g_object_notify(G_OBJECT(label), "text");
}

/**
 * gwy_3d_label_get_text:
 * @label: A 3D label.
 *
 * Gets the text of a 3D label.
 *
 * Returns: The label text.  The returned string is owned by label and must no
 *          be modified or freed.
 **/
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

    buffer = g_string_new(NULL);
    key = g_string_new(NULL);
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

/**
 * gwy_3d_label_reset:
 * @label: A 3D label.
 *
 * Resets all 3D label properties and text to default values.
 **/
void
gwy_3d_label_reset(Gwy3DLabel *label)
{
    GObject *object;
    gdouble defval;

    g_return_if_fail(GWY_IS_3D_LABEL(label));
    object = G_OBJECT(label);
    g_object_freeze_notify(object);
    if (label->fixed_size) {
        label->fixed_size = FALSE;
        g_object_notify(object, "fixed-size");
    }
    if (label->delta_x != (defval = PSPEC("delta-x", DOUBLE)->default_value)) {
        label->delta_x = defval;
        g_object_notify(object, "delta-x");
    }
    if (label->delta_y != (defval = PSPEC("delta-y", DOUBLE)->default_value)) {
        label->delta_y = defval;
        g_object_notify(object, "delta-y");
    }
    if (label->rotation
        != (defval = PSPEC("rotation", DOUBLE)->default_value)) {
        label->rotation = defval;
        g_object_notify(object, "rotation");
    }
    if (label->size != (defval = PSPEC("size", DOUBLE)->default_value)) {
        label->size = defval;
        g_object_notify(object, "size");
    }
    gwy_3d_label_set_text(label, label->default_text);
    g_object_thaw_notify(object);
}

/**
 * gwy_3d_label_reset_text:
 * @label: A 3D label.
 *
 * Resets 3D label text to default values.
 **/
void
gwy_3d_label_reset_text(Gwy3DLabel *label)
{
    g_return_if_fail(GWY_IS_3D_LABEL(label));
    gwy_3d_label_set_text(label, label->default_text);
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
        return label->size;

    if (label->size != user_size) {
        label->size = user_size;
        g_object_notify(G_OBJECT(label), "size");
    }
    return user_size;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwy3dlabel
 * @title: Gwy3DLabel
 * @short_description: Label on #Gwy3DView
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
