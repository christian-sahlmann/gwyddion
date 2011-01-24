/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
#include <libgwyddion/gwymacros.h>
#include <app/validate.h>
#include "gwyappinternal.h"

#define FAIL gwy_data_validation_failure_new

typedef struct {
    GwyDataError error;
    GQuark key;
    gchar *details;
    gpointer object;
} _GwyDataValidationFailure;

typedef struct {
    GwyDataValidateFlags flags;
    GSList *errors;
    /* For reference count check */
    GSList *stack;
    /* For stray secondary data check */
    GArray *channels;
    GArray *graphs;
    GArray *spectra;
} GwyDataValidationInfo;

static GwyDataValidationFailure*
gwy_data_validation_failure_new(GwyDataError type,
                                GQuark key,
                                const gchar *fmt,
                                ...)
{
    _GwyDataValidationFailure *failure;

    failure = g_new0(_GwyDataValidationFailure, 1);
    failure->error = type;
    failure->key = key;

    if (fmt) {
        va_list ap;

        va_start(ap, fmt);
        failure->details = g_strdup_vprintf(fmt, ap);
        va_end(ap);
    }

    return (GwyDataValidationFailure*)failure;
}

static void
gwy_data_validation_failure_free(_GwyDataValidationFailure *failure)
{
    gwy_object_unref(failure->object);
    g_free(failure->details);
    g_free(failure);
}

/**
 * gwy_data_validation_failure_list_free:
 * @list: Failure list returned by gwy_data_validate().
 *
 * Frees a data validation failure list.
 *
 * Since: 2.9
 **/
void
gwy_data_validation_failure_list_free(GSList *list)
{
    GSList *l;

    for (l = list; l; l = g_slist_next(l))
        gwy_data_validation_failure_free(l->data);
    g_slist_free(list);
}

static gboolean
check_utf8(const gchar *string,
           GQuark key,
           GSList **errors)
{
    const gchar *end;

    if (G_LIKELY(g_utf8_validate(string, -1, &end)))
        return TRUE;

    *errors = g_slist_prepend(*errors,
                              FAIL(GWY_DATA_ERROR_NON_UTF8_STRING, key,
                                   _("byte at position %d"),
                                   end - string));
    return FALSE;
}

static gboolean
check_ascii_key(const guchar *strkey,
                GQuark key,
                GSList **errors)
{
    const guchar *end;

    for (end = strkey; *end; end++) {
        if (G_UNLIKELY(*end >= 127U || !g_ascii_isgraph(*end)))
            goto fail;
    }
    return TRUE;

fail:
    *errors = g_slist_prepend(*errors,
                              FAIL(GWY_DATA_ERROR_KEY_CHARACTERS, key,
                                   _("byte at position %d"),
                                   end - strkey));
    return FALSE;
}

static gboolean
check_type(GValue *gvalue,
           GType type,
           GQuark key,
           GSList **errors)
{
    GType vtype;

    /* Simple types */
    if (!g_type_is_a(type, G_TYPE_OBJECT)) {
        if (G_LIKELY(G_VALUE_HOLDS(gvalue, type))) {
            if (type == G_TYPE_STRING)
                return check_utf8(g_value_get_string(gvalue), key, errors);
            return TRUE;
        }
    }

    /* Expecting object but found a simple type */
    if (G_UNLIKELY(!G_VALUE_HOLDS(gvalue, G_TYPE_OBJECT))) {
        *errors = g_slist_prepend(*errors,
                                  FAIL(GWY_DATA_ERROR_ITEM_TYPE, key,
                                       _("%s instead of %s"),
                                       g_type_name(G_VALUE_TYPE(gvalue)),
                                       g_type_name(type)));
        return FALSE;
    }

    /* Object types, check thoroughly */
    vtype = G_TYPE_FROM_INSTANCE(g_value_get_object(gvalue));
    if (G_LIKELY(g_type_is_a(vtype, type)))
        return TRUE;

    *errors = g_slist_prepend(*errors,
                              FAIL(GWY_DATA_ERROR_ITEM_TYPE, key,
                                   _("%s instead of %s"),
                                   g_type_name(G_VALUE_TYPE(gvalue)),
                                   g_type_name(type)));
    return FALSE;
}

static void
validate_item_pass1(gpointer hash_key,
                    gpointer hash_value,
                    gpointer user_data)
{
    GQuark key = GPOINTER_TO_UINT(hash_key);
    GValue *gvalue = (GValue*)hash_value;
    GwyDataValidationInfo *info = (GwyDataValidationInfo*)user_data;
    GSList **errors;
    const gchar *strkey;
    gint id;
    guint len;
    GwyAppKeyType type;

    errors = &info->errors;
    strkey = g_quark_to_string(key);
    check_ascii_key(strkey, key, errors);
    if (strkey[0] != GWY_CONTAINER_PATHSEP)
        *errors = g_slist_prepend(*errors,
                                  FAIL(GWY_DATA_ERROR_KEY_FORMAT, key, NULL));

    id = _gwy_app_analyse_data_key(strkey, &type, &len);
    if (type == KEY_IS_NONE) {
        if (info->flags & GWY_DATA_VALIDATE_UNKNOWN)
            *errors = g_slist_prepend(*errors,
                                      FAIL(GWY_DATA_ERROR_KEY_UNKNOWN,
                                           key, NULL));
        return;
    }

    /* Non-id items */
    if (type == KEY_IS_FILENAME) {
        check_type(gvalue, G_TYPE_STRING, key, errors);
        return;
    }
    if (type == KEY_IS_GRAPH_LASTID) {
        check_type(gvalue, G_TYPE_INT, key, errors);
        return;
    }

    /* Items that must have data id.  While ids above 2^20 are technically
     * valid, they indicate a bug or malice. */
    if (id < 0 || id >= (1 << 20))
        *errors = g_slist_prepend(*errors,
                                  FAIL(GWY_DATA_ERROR_KEY_ID, key, NULL));

    /* Types */
    switch (type) {
        case KEY_IS_DATA:
        if (check_type(gvalue, GWY_TYPE_DATA_FIELD, key, errors))
            g_array_append_val(info->channels, id);
        break;

        case KEY_IS_MASK:
        case KEY_IS_SHOW:
        case KEY_IS_CALDATA:
        check_type(gvalue, GWY_TYPE_DATA_FIELD, key, errors);
        break;

        case KEY_IS_GRAPH:
        if (check_type(gvalue, GWY_TYPE_GRAPH_MODEL, key, errors))
            g_array_append_val(info->graphs, id);
        break;

        case KEY_IS_SPECTRA:
        if (check_type(gvalue, GWY_TYPE_SPECTRA, key, errors))
            g_array_append_val(info->spectra, id);
        break;

        case KEY_IS_META:
        check_type(gvalue, GWY_TYPE_CONTAINER, key, errors);
        break;

        case KEY_IS_TITLE:
        case KEY_IS_PALETTE:
        case KEY_IS_3D_PALETTE:
        case KEY_IS_3D_MATERIAL:
        check_type(gvalue, G_TYPE_STRING, key, errors);
        break;

        case KEY_IS_SELECT:
        check_type(gvalue, GWY_TYPE_SELECTION, key, errors);
        break;

        case KEY_IS_RANGE_TYPE:
        case KEY_IS_SPS_REF:
        check_type(gvalue, G_TYPE_INT, key, errors);
        break;

        case KEY_IS_RANGE:
        case KEY_IS_MASK_COLOR:
        check_type(gvalue, G_TYPE_DOUBLE, key, errors);
        break;

        case KEY_IS_REAL_SQUARE:
        case KEY_IS_DATA_VISIBLE:
        case KEY_IS_GRAPH_VISIBLE:
        case KEY_IS_SPECTRA_VISIBLE:
        check_type(gvalue, G_TYPE_BOOLEAN, key, errors);
        break;

        case KEY_IS_3D_SETUP:
        check_type(gvalue, GWY_TYPE_3D_SETUP, key, errors);
        break;

        case KEY_IS_3D_LABEL:
        check_type(gvalue, GWY_TYPE_3D_LABEL, key, errors);
        break;

        default:
        g_warning("Key type %u of %s not handled in validate_item()",
                  type, strkey);
        break;
    }
}

static inline gboolean
in_array(GArray *array,
         gint i)
{
    guint j;

    for (j = 0; j < array->len; j++) {
        if (g_array_index(array, gint, j) == i)
            return TRUE;
    }

    return FALSE;
}

static void
validate_item_pass2(gpointer hash_key,
                    gpointer hash_value,
                    gpointer user_data)
{
    GQuark key = GPOINTER_TO_UINT(hash_key);
    GValue *gvalue = (GValue*)hash_value;
    GwyDataValidationInfo *info = (GwyDataValidationInfo*)user_data;
    GSList **errors;
    const gchar *strkey;
    gint id;
    guint len;
    GwyAppKeyType type;

    errors = &info->errors;
    strkey = g_quark_to_string(key);
    id = _gwy_app_analyse_data_key(strkey, &type, &len);
    if (id < 0 || id >= (1 << 20))
        return;

    /* Types */
    switch (type) {
        case KEY_IS_MASK:
        case KEY_IS_SHOW:
        case KEY_IS_META:
        case KEY_IS_TITLE:
        case KEY_IS_PALETTE:
        case KEY_IS_3D_PALETTE:
        case KEY_IS_3D_MATERIAL:
        case KEY_IS_SELECT:
        case KEY_IS_RANGE_TYPE:
        case KEY_IS_SPS_REF:
        case KEY_IS_RANGE:
        case KEY_IS_MASK_COLOR:
        case KEY_IS_REAL_SQUARE:
        case KEY_IS_DATA_VISIBLE:
        case KEY_IS_3D_SETUP:
        case KEY_IS_3D_LABEL:
        if (!in_array(info->channels, id))
            *errors = g_slist_prepend(*errors,
                                      FAIL(GWY_DATA_ERROR_STRAY_SECONDARY_DATA,
                                           key,
                                           _("no channel %d exists for %s"),
                                           id, strkey));
        break;

        case KEY_IS_GRAPH_VISIBLE:
        if (!in_array(info->graphs, id))
            *errors = g_slist_prepend(*errors,
                                      FAIL(GWY_DATA_ERROR_STRAY_SECONDARY_DATA,
                                           key,
                                           _("no graph %d exists for %s"),
                                           id, strkey));
        break;

        case KEY_IS_SPECTRA_VISIBLE:
        check_type(gvalue, G_TYPE_BOOLEAN, key, errors);
        if (!in_array(info->spectra, id))
            *errors = g_slist_prepend(*errors,
                                      FAIL(GWY_DATA_ERROR_STRAY_SECONDARY_DATA,
                                           key,
                                           _("no spectra %d exists for %s"),
                                           id, strkey));
        break;

        default:
        break;
    }
}

static gchar*
format_stack(GObject *object,
             GSList *stack)
{
    GString *str;
    gchar *retval;

    str = g_string_new(NULL);
    g_string_append(str, G_OBJECT_TYPE_NAME(object));
    while (stack) {
        g_string_append(str, " <- ");
        g_string_append(str, (const gchar*)(stack->data));
        stack = stack->next;
    }
    retval = str->str;
    g_string_free(str, FALSE);

    return retval;
}

#define PUSH(s, x) s = g_slist_prepend(s, (gpointer)(x))
#define POP(s) s = g_slist_delete_link(s, s)
#define CRFC(field, name) \
    if ((child = (GObject*)field)) { \
        PUSH(info->stack, name); \
        check_ref_count(child, key, info->stack, errors); \
        POP(info->stack); \
    } \
    else \
        while (FALSE)

static gboolean
check_ref_count(GObject *object,
                GQuark key,
                GSList *stack,
                GSList **errors)
{
    gchar *s;

    if (!object || object->ref_count == 1)
        return TRUE;

    s = format_stack(object, stack);
    *errors = g_slist_prepend(*errors,
                              FAIL(GWY_DATA_ERROR_REF_COUNT, key,
                                   _("ref_count is %d for %s"),
                                   object->ref_count, s));
    g_free(s);

    return FALSE;
}

static void
validate_item_pass3(gpointer hash_key,
                    gpointer hash_value,
                    gpointer user_data)
{
    GValue *gvalue = (GValue*)hash_value;
    GQuark key = GPOINTER_TO_UINT(hash_key);
    GwyDataValidationInfo *info = (GwyDataValidationInfo*)user_data;
    GSList **errors;
    GObject *object, *child;
    const gchar *typename;
    gint n, i;

    if (!G_VALUE_HOLDS_OBJECT(gvalue))
        return;

    errors = &info->errors;
    object = g_value_get_object(gvalue);
    typename = G_OBJECT_TYPE_NAME(object);

    check_ref_count(object, key, info->stack, errors);

    PUSH(info->stack, typename);
    if (GWY_IS_CONTAINER(object)) {
        gwy_container_foreach((GwyContainer*)object, NULL,
                              &validate_item_pass3, info);
    }
    else if (GWY_IS_DATA_FIELD(object)) {
        GwyDataField *data_field = (GwyDataField*)object;

        CRFC(data_field->si_unit_xy, "si-unit-xy");
        CRFC(data_field->si_unit_z, "si-unit-z");
    }
    else if (GWY_IS_DATA_LINE(object)) {
        GwyDataLine *data_line = (GwyDataLine*)object;

        CRFC(data_line->si_unit_x, "si-unit-x");
        CRFC(data_line->si_unit_y, "si-unit-y");
    }
    else if (GWY_IS_GRAPH_MODEL(object)) {
        GwyGraphModel *graph_model = (GwyGraphModel*)object;

        CRFC(graph_model->x_unit, "si-unit-x");
        CRFC(graph_model->y_unit, "si-unit-y");

        PUSH(info->stack, "curve");
        n = gwy_graph_model_get_n_curves(graph_model);
        for (i = 0; i < n; i++) {
            child = (GObject*)gwy_graph_model_get_curve(graph_model, i);
            check_ref_count(child, key, info->stack, errors);
        }
        POP(info->stack);
    }
    else if (GWY_IS_SPECTRA(object)) {
        GwySpectra *spectra = (GwySpectra*)object;

        CRFC(spectra->si_unit_xy, "si-unit-xy");

        PUSH(info->stack, "spectrum");
        n = gwy_spectra_get_n_spectra(spectra);
        for (i = 0; i < n; i++) {
            GwyDataLine *data_line = gwy_spectra_get_spectrum(spectra, i);

            CRFC(data_line->si_unit_x, "si-unit-x");
            CRFC(data_line->si_unit_y, "si-unit-y");
        }
        POP(info->stack);
    }
    POP(info->stack);
}

static void
gwy_data_correct(GwyContainer *data,
                 GSList *failures)
{
    GwyDataValidationFailure *failure;
    GSList *l;

    for (l = failures; l; l = g_slist_next(l)) {
        failure = (GwyDataValidationFailure*)l->data;
        /* Cannot handle this properly */
        if (failure->error == GWY_DATA_ERROR_REF_COUNT)
            continue;

        /* Everything else can be fixed by removal of the offending items. */
        gwy_container_remove(data, failure->key);
    }
}

/**
 * gwy_data_validate:
 * @data: Data container.  It should not be managed by the data browser (yet)
 *        if flags contain %GWY_DATA_VALIDATE_CORRECT (because things can
 *        in principle break during the correction) or
 *        %GWY_DATA_VALIDATE_REF_COUNT (because the application took some
 *        references).
 * @flags: Validation flags.  Some influence what is checked, some determine
 *         what to do when problems are found.
 * @returns: List of errors found, free
 *           with gwy_data_validation_failure_list_free().
 *           The list is independent on whether %GWY_DATA_VALIDATE_CORRECT is
 *           given in flags, even though the offending items may be no longer
 *           exist in the container after correction.  If
 *           %GWY_DATA_VALIDATE_NO_REPORT is present in flags, %NULL is always
 *           returned.
 *
 * Checks the contents of a data file.
 *
 * If %GWY_DATA_VALIDATE_CORRECT is given in @flags, correctable problems are
 * corrected.  At present correctable problems are those that can be fixed
 * by removal of the offending data.
 *
 * Since: 2.9
 **/
GSList*
gwy_data_validate(GwyContainer *data,
                  GwyDataValidateFlags flags)
{
    GwyDataValidationInfo info;
    GSList *errors;

    if ((flags & GWY_DATA_VALIDATE_NO_REPORT)
        && !(flags & GWY_DATA_VALIDATE_CORRECT)) {
        g_warning("Neither report no correction asked for, "
                  "validation is useless.");
        return NULL;
    }

    memset(&info, 0, sizeof(GwyDataValidationInfo));
    info.flags = flags;
    info.channels = g_array_new(FALSE, FALSE, sizeof(gint));
    info.graphs = g_array_new(FALSE, FALSE, sizeof(gint));
    info.spectra = g_array_new(FALSE, FALSE, sizeof(gint));

    gwy_container_foreach(data, NULL, &validate_item_pass1, &info);
    gwy_container_foreach(data, NULL, &validate_item_pass2, &info);
    if (flags & GWY_DATA_VALIDATE_REF_COUNT)
        gwy_container_foreach(data, NULL, &validate_item_pass3, &info);

    if (flags & GWY_DATA_VALIDATE_CORRECT)
        gwy_data_correct(data, info.errors);

    /* Note this renders info.errors unusable */
    errors = g_slist_reverse(info.errors);

    g_array_free(info.channels, TRUE);
    g_array_free(info.graphs, TRUE);
    g_array_free(info.spectra, TRUE);

    if (flags & GWY_DATA_VALIDATE_NO_REPORT) {
        gwy_data_validation_failure_list_free(errors);
        errors = NULL;
    }

    return errors;
}

/**
 * gwy_data_error_desrcibe:
 * @error: Data validation error type.
 * @returns: Error description as an untranslated string owned by the
 *           library.
 *
 * Describes a data validation error type.
 *
 * Since: 2.9
 **/
const gchar*
gwy_data_error_desrcibe(GwyDataError error)
{
    static const gchar *errors[] = {
        "",
        N_("Invalid item key format"),
        N_("Item key contains invalid characters"),
        N_("Item key does not belong to any known data"),
        N_("Wrong data item id"),
        N_("Unexpected data item type"),
        N_("String value is not valid UTF-8"),
        N_("Object has several references"),
        N_("Secondary data item has no primary data"),
    };

    if (error < 1 || error >= G_N_ELEMENTS(errors))
        return "";

    return errors[error];
}

/************************** Documentation ****************************/

/**
 * SECTION:validate
 * @title: Validate
 * @short_description: Check data sanity and consistency
 *
 * A #GwyContainer can be used to represent all sorts of data.  However,
 * Gwyddion, the application, follows certain conventions in data organization.
 * Function gwy_data_validate() checks whether the data actually follows them.
 * This includes but is not limited to representability of keys in ASCII,
 * conformance to the key naming convention, types of objects and other items
 * corresponding to expectation, string values being valid UTF-8 and no stray
 * secondary data.
 **/

/**
 * GwyDataValidationFailure:
 * @error: Error type.
 * @key: Key of the problematic data item.
 * @details: Error details, may be %NULL for some types of @error.  This is
 *           a dynamically allocated string owned by the caller, however, he
 *           normally frees the complete errors lists with
 *           gwy_data_validation_failure_list_free() which frees these fields
 *           too.
 *
 * Information about one data validate error.
 *
 * Note the structure may contain more private fields.
 *
 * Since: 2.9
 **/

/**
 * GwyDataError:
 * @GWY_DATA_ERROR_KEY_FORMAT: Key format is invalid (e.g. does not start
 *                             with %GWY_CONTAINER_PATHSEP).
 * @GWY_DATA_ERROR_KEY_CHARACTERS: Key contains unprintable characters or
 *                                 characters not representable in ASCII.
 * @GWY_DATA_ERROR_KEY_UNKNOWN: Key does not correspond to any data item known
 *                              to this version of Gwyddion.
 * @GWY_DATA_ERROR_KEY_ID: Key corresponds to a data item with bogus id number.
 * @GWY_DATA_ERROR_ITEM_TYPE: Wrong item type (for instance an integer at key
 *                            <literal>"/0/data"</literal>).
 * @GWY_DATA_ERROR_NON_UTF8_STRING: String value is not valid UTF-8.
 * @GWY_DATA_ERROR_REF_COUNT: Reference count is higher than 1.
 * @GWY_DATA_ERROR_STRAY_SECONDARY_DATA: Secondary data item (e.g. mask,
 *                                       selection or visibility) without a
 *                                       corresponding valid primary data item.
 *
 * Type of data validation errors.
 *
 * Since: 2.9
 **/

/**
 * GwyDataValidateFlags:
 * @GWY_DATA_VALIDATE_UNKNOWN: Report all unknown keys as
 *                             %GWY_DATA_ERROR_KEY_UNKNOWN errors.  Note while
 *                             a data item unknown to the current version of
 *                             Gwyddion can come from a newer version therefore
 *                             it can be in certain sense valid.
 * @GWY_DATA_VALIDATE_REF_COUNT: Report all object items with reference count
 *                               higher than 1 as %GWY_DATA_ERROR_REF_COUNT
 *                               errors.  Obviously this makes sense only with
 *                               `fresh' data containers.
 * @GWY_DATA_VALIDATE_ALL: All above flags combined.
 * @GWY_DATA_VALIDATE_CORRECT: Attempt to correct problems.
 * @GWY_DATA_VALIDATE_NO_REPORT: Do not report problems.
 *
 * Flags controlling gwy_data_validate() behaviour.
 *
 * Note passing @GWY_DATA_VALIDATE_NO_REPORT is allowed only if
 * @GWY_DATA_VALIDATE_CORRECT is present too.
 *
 * Since: 2.9
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
