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
#include <stdarg.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/stats.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

struct _GwyTextHeaderContext {
    const GwyTextHeaderParser *parser;
    GHashTable *hash;
    GError *error;
    gchar *section;
    gpointer user_data;
    guint lineno;
};

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
 * @mfield: A mask field containing 1.0 in place of good data points, 0.0 in
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

/**
 * gwy_text_header_error_quark:
 *
 * Returns error domain for expression parsin and evaluation.
 *
 * See and use %GWY_TEXT_HEADER_ERROR.
 *
 * Returns: The error domain.
 *
 * Since: 2.18
 **/
GQuark
gwy_text_header_error_quark(void)
{
    static GQuark error_domain = 0;

    if (!error_domain)
        error_domain = g_quark_from_static_string("gwy-text-header-error-quark");

    return error_domain;
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

static void
check_fatal_error(GwyTextHeaderContext *context)
{
    if (!context->parser->error
        || !context->parser->error(context, context->error, context->user_data))
        g_clear_error(&context->error);
}

static void
make_noise(GwyTextHeaderContext *context,
           GwyTextHeaderError code,
           const gchar *format,
           ...)
{
    gchar *s;
    va_list ap;

    if (!context->parser->error)
        return;

    va_start(ap, format);
    s = g_strdup_vprintf(format, ap);
    va_end(ap);
    g_set_error(&context->error, GWY_TEXT_HEADER_ERROR, code, "%s", s);
    g_free(s);
    check_fatal_error(context);
}

/**
 * gwy_text_header_parse:
 * @header: Text header to parse.  It must be %NULL-terminated and writable.
 *          Its contents will be modified to directly embed the hash keys
 *          and/or values.  It must not be freed while the returned hash
 *          table is in use.
 * @parser: Parser specification.
 * @user_data: User data passed to parser callbacks.
 * @error: Error to set on fatal errors.
 *
 * Parses a line-oriented text header into a hash table.
 *
 * See #GwyTextHeaderParser for details of memory and error handling.
 *
 * Returns: A newly created hash table with values indexed by they keys found
 *          in the header.
 *
 * Since: 2.18.
 **/
GHashTable*
gwy_text_header_parse(gchar *header,
                      const GwyTextHeaderParser *parser,
                      gpointer user_data,
                      GError **error)
{
    GwyTextHeaderContext context;
    gchar *line;
    const gchar *section_prefix = NULL, *section_suffix = NULL,
                *endsect_prefix = NULL, *endsect_suffix = NULL,
                **comments = NULL;
    guint section_prefix_len = 0, section_suffix_len = 0,
          endsect_prefix_len = 0, endsect_suffix_len = 0,
          comment_prefix_len = 0, line_prefix_len = 0;
    gchar equal_sign_char = 0;
    guint j, ncomments = 0;
    gboolean free_keys = FALSE;

    g_return_val_if_fail(parser->section_template
                         || !parser->endsection_template,
                         NULL);
    g_return_val_if_fail(!parser->section_template == !parser->section_accessor,
                         NULL);
    g_return_val_if_fail(parser->item || !parser->destroy_key, NULL);
    g_return_val_if_fail(parser->item || !parser->destroy_value, NULL);

    /* Split section templates to prefix and suffix. */
    if (parser->section_template) {
        gchar *p;

        p = strchr(parser->section_template, '\x1a');
        g_return_val_if_fail(p, NULL);
        section_prefix = parser->section_template;
        section_prefix_len = p - parser->section_template;
        section_suffix = section_prefix + (p+1 - parser->section_template);
        section_suffix_len = strlen(section_suffix);
        free_keys = TRUE;
    }

    if (parser->endsection_template) {
        gchar *p;

        if ((p = strchr(parser->endsection_template, '\x1a'))) {
            endsect_prefix = parser->endsection_template;
            endsect_prefix_len = p - parser->endsection_template;
            endsect_suffix = endsect_prefix + (p+1 - parser->endsection_template);
            endsect_suffix_len = strlen(endsect_suffix);
        }
    }

    if (parser->comment_prefix) {
        comment_prefix_len = strlen(parser->comment_prefix);
        if (strchr(parser->comment_prefix, '\n')) {
            const gchar *p;

            for (j = 0, p = parser->comment_prefix-1;
                 p;
                 p = strchr(p+1, '\n'), j++)
                ;
            ncomments = j;
            comments = g_new(const gchar*, j+1);
            comments[0] = parser->comment_prefix;
            j = 1;
            do {
                if ((p = strchr(comments[j-1], '\n')))
                    comments[j++] = p+1;
            } while (p);
            comments[j] = parser->comment_prefix + comment_prefix_len + 1;
        }
    }

    if (parser->line_prefix)
        line_prefix_len = strlen(parser->line_prefix);
    if (parser->key_value_separator && strlen(parser->key_value_separator) == 1)
        equal_sign_char = parser->key_value_separator[0];

    /* Build the hash table */
    context.lineno = 0;
    context.section = NULL;
    context.error = NULL;
    context.parser = parser;
    context.user_data = user_data;
    context.hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                         parser->destroy_key
                                         ? parser->destroy_key
                                         : (free_keys ? g_free : NULL),
                                         parser->destroy_value
                                         ? parser->destroy_value : NULL);

    for (line = gwy_str_next_line(&header);
         line && !context.error;
         line = gwy_str_next_line(&header), context.lineno++) {
        gchar *key, *value;
        guint len;

        /* Chomp whitespace at the end */
        if (!(len = chomp(line, strlen(line))))
            continue;

        /* Header end marker */
        if (parser->terminator
            && gwy_strequal(line, parser->terminator))
            break;

        /* Section ends -- before section starts beause they might look
         * similar. */
        if (endsect_prefix
            && len > endsect_prefix_len + endsect_suffix_len
            && strncmp(line, endsect_prefix, endsect_prefix_len) == 0
            && gwy_strequal(line + len - endsect_suffix_len, endsect_suffix)) {
            gchar *endsection;

            len -= endsect_suffix_len;
            line[len] = '\0';
            endsection = strip(line + endsect_prefix_len,
                               len - endsect_prefix_len);

            if (!context.section) {
                make_noise(&context, GWY_TEXT_HEADER_ERROR_SECTION_END,
                           _("Section %s ended at line %u but it has "
                             "never started."),
                           endsection, context.lineno);
                continue;
            }

            if (!gwy_strequal(endsection, context.section)) {
                make_noise(&context, GWY_TEXT_HEADER_ERROR_SECTION_END,
                           _("Section %s ended at line %u instead of %s."),
                           endsection, context.lineno, context.section);
                continue;
            }

            if (parser->endsection
                && !parser->endsection(&context, context.section, user_data,
                                       &context.error))
                check_fatal_error(&context);

            context.section = NULL;
            continue;
        }
        else if (parser->endsection_template
                 && gwy_strequal(line, parser->endsection_template)) {
            if (parser->endsection
                && !parser->endsection(&context, context.section, user_data,
                                       &context.error))
                check_fatal_error(&context);
            context.section = NULL;
            continue;
        }

        /* Sections starts */
        if (section_prefix
            && len > section_prefix_len + section_suffix_len
            && strncmp(line, parser->section_template, section_prefix_len) == 0
            && gwy_strequal(line + len - section_suffix_len, section_suffix)) {
            gchar *newsection;

            len -= section_suffix_len;
            line[len] = '\0';
            newsection = strip(line + section_prefix_len,
                               len - section_prefix_len);

            if (parser->endsection_template && context.section) {
                make_noise(&context, GWY_TEXT_HEADER_ERROR_SECTION_START,
                           _("Section %s started at line %u before %s ended."),
                           newsection, context.lineno, context.section);
            }

            if (!*newsection) {
                make_noise(&context, GWY_TEXT_HEADER_ERROR_SECTION_NAME,
                           _("Empty section name at header line %u."),
                           context.lineno);
                continue;
            }

            context.section = newsection;
            if (parser->section
                && !parser->section(&context, context.section, user_data,
                                    &context.error))
                check_fatal_error(&context);
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
        else if (parser->comment_prefix
                 && strncmp(line, parser->comment_prefix,
                            comment_prefix_len) == 0)
            continue;

        /* Line prefixes */
        if (parser->line_prefix) {
            if (strncmp(line, parser->line_prefix, line_prefix_len) == 0) {
                line += line_prefix_len;
                len -= line_prefix_len;
            }
            else {
                make_noise(&context, GWY_TEXT_HEADER_ERROR_PREFIX,
                           _("Header line %u lacks prefix %s."),
                           context.lineno, parser->line_prefix);
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
        else if (parser->key_value_separator)
            value = strstr(line, parser->key_value_separator);
        else {
            for (value = line; !g_ascii_isspace(*value); value++) {
                if (!*value) {
                    value = NULL;
                    break;
                }
            }
        }
        if (!value) {
            make_noise(&context, GWY_TEXT_HEADER_ERROR_GARBAGE,
                       _("Header line %u lacks key-value separator."),
                       context.lineno);
            continue;
        }

        *(value++) = '\0';
        if (!chomp(line, value - line - 1)) {
            make_noise(&context, GWY_TEXT_HEADER_ERROR_KEY,
                       _("Key at header line %u is empty."), context.lineno);
            continue;
        }
        key = line;
        value = strip(value, len - (value - line));

        if (parser->section_template) {
            if (context.section)
                key = g_strconcat(context.section, parser->section_accessor,
                                  key, NULL);
            else
                key = g_strdup(key);
        }

        if (parser->item) {
            if (!parser->item(&context, context.hash, key, value,
                              context.user_data, &context.error))
                check_fatal_error(&context);
        }
        else
            g_hash_table_replace(context.hash, key, value);
    }

    g_free(comments);

    if (parser->terminator && !line && !context.error)
        make_noise(&context, GWY_TEXT_HEADER_ERROR_TERMINATOR,
                   _("Header suddenly ended at line %u; end of header marker "
                     "is missing"),
                   context.lineno);

    if (context.error) {
        g_propagate_error(error, context.error);
        g_hash_table_destroy(context.hash);
        return NULL;
    }

    return context.hash;
}

/**
 * gwy_text_header_context_get_section:
 * @context: Header parsing context.
 *
 * Gets the currently open section.
 *
 * This function may be called if no sectioning is defined.  It simply
 * returns %NULL then.
 *
 * Returns: The name of the currently open section, %NULL if there is none.
 *
 * Since: 2.18
 **/
const gchar*
gwy_text_header_context_get_section(const GwyTextHeaderContext *context)
{
    return context->section;
}

/**
 * gwy_text_header_context_get_lineno:
 * @context: Header parsing context.
 *
 * Gets the current header line.
 *
 * Returns: The current line number, starting from zero.
 *
 * Since: 2.18
 **/
guint
gwy_text_header_context_get_lineno(const GwyTextHeaderContext *context)
{
    return context->lineno;
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

/**
 * GWY_TEXT_HEADER_ERROR:
 *
 * Error domain for text header parsing. Errors in this domain will be from the
 * #GwyTextHeaderError enumeration. See #GError for information on error
 * domains.
 *
 * Since: 2.18
 **/

/**
 * GwyTextHeaderError:
 * @GWY_TEXT_HEADER_ERROR_SECTION_NAME: Section name is invalid.  It is raised
 *                                      by the parser only for an empty section
 *                                      name.
 * @GWY_TEXT_HEADER_ERROR_SECTION_END: Section ended when a different section
 *                                     or no section was open.  Note that
 *                                     gwy_text_header_context_get_section()
 *                                     returns the section being closed at the
 *                                     time this error is raised.
 * @GWY_TEXT_HEADER_ERROR_SECTION_START: Section started before the previous
 *                                       ended.  This is raised only if
 *                                       @endsection_template is set.
 * @GWY_TEXT_HEADER_ERROR_PREFIX: Line lacks the mandatory prefix.
 * @GWY_TEXT_HEADER_ERROR_GARBAGE: Line cannot be parsed into a key-value pair.
 * @GWY_TEXT_HEADER_ERROR_KEY: Key name is invalid, namely empty.
 * @GWY_TEXT_HEADER_ERROR_VALUE: Value is invalid.  This is never raised by
 *                               the parser.
 * @GWY_TEXT_HEADER_ERROR_TERMINATOR: The text header has ended without being
 *                                    terminated by specified terminator.
 *
 * Error codes returned by text header parsing.
 *
 * Some errors, in particular %GWY_TEXT_HEADER_ERROR_KEY and
 * %GWY_TEXT_HEADER_ERROR_VALUE are expected to be raised by user callbacks
 * (they are not restricted to these codes though).
 *
 * Since: 2.18
 **/

/**
 * GwyTextHeaderContext:
 *
 * #GwyTextHeaderContext represents the parsing state.  It is an opaque data
 * structure and should be only manipulated with the functions below.
 *
 * Since: 2.18
 **/

/**
 * GwyTextHeaderParser:
 * @comment_prefix: Prefix denoting comment lines.  It is possible to specify
 *                  multiple prefixes by separating them with newline ("\n").
 * @section_template: Section start template.  It must contain the character
 *                    "\x1a" in the place where the section name apprears.
 *                    Example: "[Section \x1a]".
 * @endsection_template: Section end template.  It may or may not contain the
 *                       substitute character "\x1a" depending on whether the
 *                       section end markers contain the section name.  It is
 *                       invalid to set @endsection_template without setting
 *                       @section_template.  Example: "[Section \x1a End]".
 * @section_accessor: Glue to put between the section name and key when
 *                    constructing hash table keys.  It is invalid to set
 *                    @section_accessor without setting @section_template.
 *                    Typically, "::" is used.
 * @line_prefix: Mandatory prefix of each line.
 * @key_value_separator: The string separating keys and values on each line.
 *                       Typically, "=" or ":" is used.  When left %NULL,
 *                       whitespace plays the role of the separator.  Of
 *                       course, keys cannot contain whitespace then.
 * @terminator: Line that marks end of the header.  It is mandatory if
 *              specified, @GWY_TEXT_HEADER_ERROR_TERMINATOR is raised when
 *              the header ends sooner than @terminator is found.
 * @item: Callback called when a key-value pair is found.  If set it is
 *        responsible for inserting the pair to the hash table with
 *        g_hash_table_replace().  It is free to insert a different pair or
 *        nothing.  It must return %FALSE if it raises an error.
 * @section: Callback called when a section starts.  It must return %FALSE if
 *           it raises an error.
 * @endsection: Callback called when a section end.  It must return %FALSE if
 *              it raises an error.
 * @error: Callback called when an error occurs, including errors raised by
 *         other user callbacks.  If it returns %TRUE the error is considered
 *         fatal and the parsing terminates immediately.  If it is left unset
 *         no errors are fatal hence no errors reported to the caller.
 * @destroy_key: Function to destroy keys, passed to g_hash_table_new_full().
 *               It is invalid to set @destroy_key if @item callback is not
 *               set.
 * @destroy_value: Function to destroy values, passed to
 *                 g_hash_table_new_full().  It is invalid to set
 *                 @destroy_value if @item callback is not set.
 *
 * Text header parser specification.
 *
 * Memory considerations: In general, the parser attempts to reuse the contents
 * of @header directly for the hash keys and values.  There are two cases when
 * it cannot: sectioning implies that keys must be constructed dynamically
 * and the use of @item callback means the parser has no control on what is
 * inserted into the hash table.
 *
 * This means that the @item callback must free @key if sectioning is used and
 * it is not going to actually use it as the hash table key.  And, of course,
 * suitable @destroy_key and @destroy_value functions must be set.
 *
 * Since: 2.18
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
