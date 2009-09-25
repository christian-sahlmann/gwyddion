/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libprocess/stats.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

/**
 * gwy_app_channel_check_nonsquare:
 * @data: A data container.
 * @id: Data channel id.
 *
 * Sets `realsquare' for a channel highly non-square pixels.
 *
 * The threshold for highly non-square is somewhat arbitrary.
 * Fortunately, most files encoutered in practice have the measure ratio either
 * very close to 1, larger or equal than 2.
 *
 * Returns: %TRUE if the channel was found to have highly non-square pixels
 *          and `realsquare' was set (otherwise it was unset).
 *
 * Since: 2.3
 **/
gboolean
gwy_app_channel_check_nonsquare(GwyContainer *data,
                                gint id)
{
    GwyDataField *dfield;
    gdouble xmeasure, ymeasure, q;
    gboolean nonsquare;
    GQuark quark;
    const gchar *key;
    gchar *s;

    quark = gwy_app_get_data_key_for_id(id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);

    xmeasure = gwy_data_field_get_xmeasure(dfield);
    ymeasure = gwy_data_field_get_ymeasure(dfield);
    q = xmeasure/ymeasure;

    nonsquare = (q > G_SQRT2 || q < 1.0/G_SQRT2);

    key = g_quark_to_string(quark);
    s = g_strconcat(key, "/realsquare", NULL);
    if (nonsquare)
        gwy_container_set_boolean_by_name(data, s, TRUE);
    else
        gwy_container_remove_by_name(data, s);
    g_free(s);

    return nonsquare;
}

/**
 * gwy_app_channel_title_fall_back:
 * @data: A data container.
 * @id: Data channel id.
 *
 * Adds a channel title based on data field units.
 *
 * The guess is very simple, but probably better than `Unknown channel' in
 * most cases.  If there already is a title it is left intact, making use of
 * this function as a fall-back easier.
 *
 * Returns: %TRUE if the title was set (either by this function or before).
 *
 * Since: 2.3
 **/
gboolean
gwy_app_channel_title_fall_back(GwyContainer *data,
                                gint id)
{
    static const struct {
        const gchar *unit;
        const gchar *title;
    }
    map[] = {
        { "m",   "Topography", },
        { "A",   "Current",    },
        { "deg", "Phase",      },
        { "V",   "Voltage",    },
        { "N",   "Force",      },
    };

    GwySIUnit *siunit, *test;
    GwyDataField *dfield;
    const gchar *key, *title;
    GQuark quark;
    guint i;
    gchar *s;

    quark = gwy_app_get_data_key_for_id(id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);

    key = g_quark_to_string(quark);
    s = g_strconcat(key, "/title", NULL);
    quark = g_quark_from_string(s);
    g_free(s);
    if (gwy_container_contains(data, quark))
        return TRUE;

    siunit = gwy_data_field_get_si_unit_z(dfield);
    test = gwy_si_unit_new(NULL);
    title = NULL;

    for (title = NULL, i = 0; i < G_N_ELEMENTS(map) && !title; i++) {
        gwy_si_unit_set_from_string(test, map[i].unit);
        if (gwy_si_unit_equal(siunit, test))
            title = map[i].title;
    }

    g_object_unref(test);

    if (title) {
        gwy_container_set_string(data, quark, g_strdup(title));
        return TRUE;
    }

    return FALSE;
}

/**
 * gwy_app_channel_remove_bad_data:
 * @dfield: A data field.  The values of bad data points are ignored and might
 *          be even left uninitialized.
 * @mfield: A mask field containing 1.0 in place of good data points, 1.0 in
 *          place of bad points.  It will be inverted to become the mask of
 *          bad points.
 *
 * Replaces bad data points with some neutral values.
 *
 * Since Gwyddion has no concept of bad data points, they are usually marked
 * with a mask and replaced with some neutral values upon import, leaving the
 * user to decide how to proceed further.  This helper function performs such
 * replacement, using the average of all good points as the neutral replacement
 * value (at this moment).
 *
 * Returns: The number of bad data points replaced.  If zero is returned, all
 *          points are good and there is no need for masking.
 *
 * Since: 2.14
 **/
guint
gwy_app_channel_remove_bad_data(GwyDataField *dfield, GwyDataField *mfield)
{
    gdouble *data = gwy_data_field_get_data(dfield);
    gdouble *mdata = gwy_data_field_get_data(mfield);
    gdouble *drow, *mrow;
    gdouble avg;
    guint i, j, mcount, xres, yres;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    avg = gwy_data_field_area_get_avg(dfield, mfield, 0, 0, xres, yres);
    mcount = 0;
    for (i = 0; i < yres; i++) {
        mrow = mdata + i*xres;
        drow = data + i*xres;
        for (j = 0; j < xres; j++) {
            if (!mrow[j]) {
                drow[j] = avg;
                mcount++;
            }
            mrow[j] = 1.0 - mrow[j];
        }
    }

    gwy_debug("mcount = %u", mcount);

    return mcount;
}

static inline guint
chomp(gchar *line,
      guint len)
{
    while (len && g_ascii_isspace(line[len-1]))
        line[--len] = '\0';

    return len;
}

static inline gchar*
strip(gchar *line,
      guint len)
{
    while (len && g_ascii_isspace(*line)) {
        line++;
        len--;
    }
    while (len && g_ascii_isspace(line[len-1]))
        line[--len] = '\0';

    return line;
}

GHashTable*
gwy_parse_text_header(gchar *buffer,
                      const gchar *comment_prefix,
                      const gchar *section_template,
                      const gchar *endsection_template,
                      const gchar *section_accessor,
                      const gchar *line_prefix,
                      const gchar *key_value_separator,
                      GwyTextHeaderItemFunc postprocess,
                      gpointer user_data,
                      GDestroyNotify destroy_value)
{
    GHashTable *hash;
    gchar *line, *section = NULL;
    const gchar *section_prefix = NULL, *section_suffix = NULL,
                *endsect_prefix = NULL, *endsect_suffix = NULL,
                **comments = NULL;
    guint section_prefix_len = 0, section_suffix_len = 0,
          endsect_prefix_len = 0, endsect_suffix_len = 0,
          comment_prefix_len = 0, line_prefix_len = 0;
    gchar equal_sign_char = 0;
    guint i, j, ncomments = 0;
    gboolean free_keys = FALSE, free_values = FALSE;

    /* Split section templates to prefix and suffix. */
    if (section_template) {
        gchar *p;

        if ((p = strchr(section_template, '\x1a'))) {
            section_prefix = section_template;
            section_prefix_len = p - section_template;
            section_suffix = section_prefix + (p+1 - section_template);
            section_suffix_len = strlen(section_suffix);
            free_keys = TRUE;
        }
        else {
            g_warning("Section template lacks substitute character \\x1a.");
            section_template = NULL;
        }
    }

    if (endsection_template) {
        if (section_template) {
            gchar *p;

            if ((p = strchr(endsection_template, '\x1a'))) {
                endsect_prefix = endsection_template;
                endsect_prefix_len = p - endsection_template;
                endsect_suffix = endsect_prefix + (p+1 - endsection_template);
                endsect_suffix_len = strlen(endsect_suffix);
                endsection_template = NULL;
            }
        }
        else {
            g_warning("Endsection template without a section template.");
            endsection_template = NULL;
        }
    }

    if (comment_prefix) {
        comment_prefix_len = strlen(comment_prefix);
        if (strchr(comment_prefix, '\n')) {
            const gchar *p;

            for (j = 0, p = comment_prefix-1; p; p = strchr(p+1, '\n'), j++)
                ;
            ncomments = j;
            comments = g_new(const gchar*, j+1);
            comments[0] = comment_prefix;
            j = 1;
            do {
                if ((p = strchr(comments[j-1], '\n')))
                    comments[j++] = p+1;
            } while (p);
            comments[j] = comment_prefix + comment_prefix_len + 1;
        }
    }

    if (line_prefix)
        line_prefix_len = strlen(line_prefix);
    if (key_value_separator && strlen(key_value_separator) == 1)
        equal_sign_char = key_value_separator[0];
    if (!section_accessor)
        section_accessor = "";
    if (postprocess)
        free_values = TRUE;

    /* Build the hash table */
    hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                 free_keys ? g_free : NULL,
                                 free_values ? destroy_value : NULL);

    for (line = gwy_str_next_line(&buffer), i = 0;
         line;
         line = gwy_str_next_line(&buffer), i++) {
        gchar *key, *value;
        guint len;

        /* Chomp whitespace at the end */
        if (!(len = chomp(line, strlen(line))))
            continue;

        /* Section ends -- before section starts beause they might look
         * similar. */
        if (endsection_template && gwy_strequal(line, endsection_template)) {
            section = NULL;
            continue;
        }
        if (endsect_prefix
            && len > endsect_prefix_len + endsect_suffix_len
            && strncmp(line, endsect_prefix, endsect_prefix_len) == 0
            && gwy_strequal(line + len - endsect_suffix_len, endsect_suffix)) {
            gchar *endsection;

            len -= endsect_suffix_len;
            line[len] = '\0';
            endsection = strip(line + endsect_prefix_len,
                               len - endsect_prefix_len);

            if (!section) {
                g_warning("Section %s ended at line %u but it did not start.",
                          endsection, i);
                continue;
            }

            if (!gwy_strequal(endsection, section)) {
                g_warning("Section %s ended at line %u instead of %s.",
                          endsection, i, section);
                continue;
            }

            section = NULL;
            continue;
        }

        /* Sections starts */
        if (section_prefix
            && len > section_prefix_len + section_suffix_len
            && strncmp(line, section_template, section_prefix_len) == 0
            && gwy_strequal(line + len - section_suffix_len, section_suffix)) {
            gchar *newsection;

            len -= section_suffix_len;
            line[len] = '\0';
            newsection = strip(line + section_prefix_len,
                               len - section_prefix_len);

            if ((endsection_template || endsect_prefix) && section) {
                g_warning("Section %s started at line %u before %s ended.",
                          newsection, i, section);
            }

            if (!*newsection) {
                g_warning("Empty section name at line %u.", i);
                continue;
            }

            section = newsection;
            continue;
        }
        /* Comments */
        if (comments) {
            for (j = 0; j < ncomments; j++) {
                if (strncmp(line, comments[j],
                            comments[j+1] - comments[j] - 1) == 0)
                    break;
            }
            if (j < ncomments)
                continue;
        }
        else if (comment_prefix
                 && strncmp(line, comment_prefix, comment_prefix_len) == 0)
            continue;

        /* Line prefixes */
        if (line_prefix) {
            if (strncmp(line, line_prefix, line_prefix_len) == 0) {
                line += line_prefix_len;
                len -= line_prefix_len;
            }
            else {
                g_warning("Line %u lacks prefix %s.", i, line_prefix);
                continue;
            }
        }
        while (g_ascii_isspace(*line)) {
            line++;
            len--;
        }

        /* Keys and values */
        if (equal_sign_char)
            value = strchr(line, equal_sign_char);
        else if (key_value_separator)
            value = strstr(line, key_value_separator);
        else {
            for (value = line; !g_ascii_isspace(*value); value++) {
                if (!*value) {
                    value = NULL;
                    break;
                }
            }
        }
        if (!value) {
            g_warning("Line %u lacks key-value separator.", i);
            continue;
        }

        *(value++) = '\0';
        if (!chomp(line, value - line - 1)) {
            g_warning("Key at line %u is empty.", i);
            continue;
        }
        key = line;
        value = strip(value, len - (value - line));

        if (section_template) {
            if (section)
                key = g_strconcat(section, section_accessor, key, NULL);
            else
                key = g_strdup(key);
        }

        if (postprocess) {
            gpointer result = postprocess(key, value, user_data);

            if (result)
                g_hash_table_replace(hash, key, result);
        }
        else
            g_hash_table_replace(hash, key, value);
    }

    g_free(comments);

    return hash;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymoduleutils-file
 * @title: file module utils
 * @short_description: Utility functions for file modules
 * @include: app/gwymoduleutils-file.h
 *
 * Functions gwy_app_channel_check_nonsquare() and
 * gwy_app_channel_title_fall_back() perform common tasks improving the
 * imported of channels from foreign data files.  Typically one calls them on
 * all imported channel ids after storing the data fields the the container,
 * if they are useful for a given file type.
 *
 * The group of functions gwy_get_gint16_le(), gwy_get_gint16_be(), etc.
 * is intended to portably read packed binary data structures that are commonly
 * found in SPM files.  They all work identically: the binary data value is
 * read from the buffer, converted if necessary, and the provided
 * buffer pointer is moved to point after the value to faciliate sequential
 * reading.
 *
 * As no buffer size is passed, obviously no buffer size checking is performed.
 * The caller has to ensure the buffer is large enough -- it is expected the
 * caller checks the total buffer size before starting to parse it.
 *
 * For example to portably read the following packed struct stored
 * in big-endian byte order:
 * <informalexample><programlisting>
 * struct {
 *     guint16 xres;
 *     guint16 yres;
 *     gfloat measure;
 * } header;
 * </programlisting></informalexample>
 * one can do (after checking the buffer size):
 * <informalexample><programlisting>
 * const guchar *p = buffer;
 * header.xres    = gwy_get_guint16_be(&amp;p);
 * header.yres    = gwy_get_guint16_be(&amp;p);
 * header.measure = gwy_get_gfloat_be(&amp;p);
 * </programlisting></informalexample>
 * and @p will point after @measure in @buffer after this snippet is finished.
 *
 * The data types used in @header do not matter (provided they are large
 * enough to hold the values), the exact types are determined by the functions
 * used.  Therefore the reading would work identically if @header was defined
 * using common types:
 * <informalexample><programlisting>
 * struct {
 *     gint xres;
 *     gint yres;
 *     gdouble measure;
 * } header;
 * </programlisting></informalexample>
 **/

/**
 * gwy_get_gboolean8:
 * @ppv: Pointer to a pointer to boolean (stored as a signle byte)
 *       in a memory buffer.
 *
 * Reads a boolean value stored as a signle byte from a
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gboolean value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint16_le:
 * @ppv: Pointer to a pointer to a little-endian signed 16bit integer
 *       in a memory buffer.
 *
 * Reads a signed 16bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint16 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint16_be:
 * @ppv: Pointer to a pointer to a big-endian signed 16bit integer
 *       in a memory buffer.
 *
 * Reads a signed 16bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint16 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint16_le:
 * @ppv: Pointer to a pointer to a little-endian unsigned 16bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 16bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint16 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint16_be:
 * @ppv: Pointer to a pointer to a big-endian unsigned 16bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 16bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint16 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint32_le:
 * @ppv: Pointer to a pointer to a little-endian signed 32bit integer
 *       in a memory buffer.
 *
 * Reads a signed 32bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint32 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint32_be:
 * @ppv: Pointer to a pointer to a big-endian signed 32bit integer
 *       in a memory buffer.
 *
 * Reads a signed 32bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint32 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint32_le:
 * @ppv: Pointer to a pointer to a little-endian unsigned 32bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 32bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint32 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint32_be:
 * @ppv: Pointer to a pointer to a big-endian unsigned 32bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 32bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint32 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint64_le:
 * @ppv: Pointer to a pointer to a little-endian signed 64bit integer
 *       in a memory buffer.
 *
 * Reads a signed 64bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint64 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gint64_be:
 * @ppv: Pointer to a pointer to a big-endian signed 64bit integer
 *       in a memory buffer.
 *
 * Reads a signed 64bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gint64 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint64_le:
 * @ppv: Pointer to a pointer to a little-endian unsigned 64bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 64bit integer value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint64 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_guint64_be:
 * @ppv: Pointer to a pointer to a big-endian unsigned 64bit integer
 *       in a memory buffer.
 *
 * Reads an unsigned 64bit integer value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #guint64 value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gfloat_le:
 * @ppv: Pointer to a pointer to a little-endian single-precision IEEE float
 *       in a memory buffer.
 *
 * Reads a single-precision IEEE float value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gfloat value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gfloat_be:
 * @ppv: Pointer to a pointer to a big-endian single-precision IEEE float
 *       in a memory buffer.
 *
 * Reads a single-precision IEEE float value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gfloat value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gdouble_le:
 * @ppv: Pointer to a pointer to a little-endian double-precision IEEE float
 *       in a memory buffer.
 *
 * Reads a double-precision IEEE float value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gdouble value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_gdouble_be:
 * @ppv: Pointer to a pointer to a big-endian double-precision IEEE float
 *       in a memory buffer.
 *
 * Reads a double-precision IEEE float value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The #gdouble value read from the buffer.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_pascal_real_le:
 * @ppv: Pointer to a pointer to a little-endian six-byte Pascal Real
 *       in a memory buffer.
 *
 * Reads a six-byte Pascale Real value from a little-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The floating point value read from the buffer as a #gdouble.
 *
 * Since: 2.3
 **/

/**
 * gwy_get_pascal_real_be:
 * @ppv: Pointer to a pointer to a big-endian six-byte Pascal Real
 *       in a memory buffer.
 *
 * Reads a six-byte Pascale Real value from a big-endian
 * binary data buffer, moving the buffer pointer to point just after the value.
 *
 * Returns: The floating point value read from the buffer as a #gdouble.
 *
 * Since: 2.3
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
