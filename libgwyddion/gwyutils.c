/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* To be able to mmap() files.
 * On Linux we have all, on Win32 we have none, on others who knows */
#if (HAVE_MMAP \
     && HAVE_UNISTD_H && HAVE_SYS_STAT_H && HAVE_SYS_TYPES_H && HAVE_FCNTL_H)
#define USE_MMAP 1
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#else
#undef USE_MMAP
#endif

#include <string.h>
#include "gwymacros.h"
#include "gwyutils.h"

static gchar *gwy_argv0 = NULL;

static GQuark error_domain = 0;

/**
 * gwy_hash_table_to_slist_cb:
 * @unused_key: Hash key (unused).
 * @value: Hash value.
 * @user_data: User data (a pointer to #GSList*).
 *
 * #GHashTable to #GSList convertor.
 *
 * Usble in g_hash_table_foreach(), pass a pointer to a #GSList* as user
 * data to it.
 **/
void
gwy_hash_table_to_slist_cb(G_GNUC_UNUSED gpointer unused_key,
                           gpointer value,
                           gpointer user_data)
{
    GSList **list = (GSList**)user_data;

    *list = g_slist_prepend(*list, value);
}

/**
 * gwy_hash_table_to_list_cb:
 * @unused_key: Hash key (unused).
 * @value: Hash value.
 * @user_data: User data (a pointer to #GList*).
 *
 * #GHashTable to #GList convertor.
 *
 * Usble in g_hash_table_foreach(), pass a pointer to a #GList* as user
 * data to it.
 **/
void
gwy_hash_table_to_list_cb(G_GNUC_UNUSED gpointer unused_key,
                          gpointer value,
                          gpointer user_data)
{
    GList **list = (GList**)user_data;

    *list = g_list_prepend(*list, value);
}

/* FIXME: the enum and flags stuff duplicates GLib functionality.
 * However the GLib stuff requires enum class registration and thus is
 * hardly usable for more-or-less random, or even dynamic stuff. */

/**
 * gwy_string_to_enum:
 * @str: A string containing one of @enum_table string values.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 *
 * Creates an integer representation of a string enum value @str.
 *
 * Returns: The integer enum value (NOT index in the table), or -1 if @str
 *          was not found.
 **/
gint
gwy_string_to_enum(const gchar *str,
                   const GwyEnum *enum_table,
                   gint n)
{
    gint j;

    for (j = n; j && enum_table->name; j--, enum_table++) {
        if (strcmp(str, enum_table->name) == 0)
            return enum_table->value;
    }

    return -1;
}

/**
 * gwy_enum_to_string:
 * @enumval: A one integer value from @enum_table.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 *
 * Creates a string representation of an integer enum value @enumval.
 *
 * Returns: The name as a string from @enum_table, thus it generally should
 *          not be modified or freed, unless @enum_table is supposed to be
 *          modified too. If the value is not found, an empty string is
 *          returned.
 **/
G_CONST_RETURN gchar*
gwy_enum_to_string(gint enumval,
                   const GwyEnum *enum_table,
                   gint n)
{
    gint j;

    for (j = n; j && enum_table->name; j--, enum_table++) {
        if (enumval == enum_table->value)
            return enum_table->name;
    }

    return "";
}

/**
 * gwy_string_to_flags:
 * @str: A string containing one of @enum_table string values.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 * @delimiter: A delimiter to split @str on, when #NULL space is used.
 *
 * Creates an integer flag combination of its string representation @str.
 *
 * Returns: All the flags present in @str, bitwise ORer.
 **/
gint
gwy_string_to_flags(const gchar *str,
                    const GwyEnum *enum_table,
                    gint n,
                    const gchar *delimiter)
{
    gchar **strings;
    gint i, j, enumval;

    strings = g_strsplit(str, delimiter ? delimiter : " ", 0);
    if (!strings)
        return 0;

    enumval = 0;
    for (i = 0; strings[i]; i++) {
        const GwyEnum *e = enum_table;

        for (j = n; j && e->name; j--, e++) {
            if (strcmp(strings[i], e->name) == 0) {
                enumval |= e->value;
                break;
            }
        }
    }
    g_strfreev(strings);

    return enumval;
}

/**
 * gwy_flags_to_string:
 * @enumval: Some ORed integer flags from @enum_table.
 * @enum_table: A table of corresponding string-integer pairs.
 * @n: The number of elements in @enum_table, may be -1 when @enum_table is
 *     terminated by a #NULL name.
 * @glue: A glue to join string values with, when #NULL space is used.
 *
 * Creates a string representation of integer flag combination @enumval.
 *
 * Returns: The string representation as a newly allocated string.  It should
 *          be freed when no longer used.
 **/
gchar*
gwy_flags_to_string(gint enumval,
                    const GwyEnum *enum_table,
                    gint n,
                    const gchar *glue)
{
    gint j;
    GString *str = NULL;
    gchar *result;

    if (!enumval)
        return "";

    if (!glue)
        glue = " ";

    for (j = n; j && enum_table->name; j--, enum_table++) {
        if (enumval & enum_table->value) {
            if (!str)
                str = g_string_new(enum_table->name);
            else {
                str = g_string_append(str, glue);
                str = g_string_append(str, enum_table->name);
            }
        }
    }
    result = str->str;
    g_string_free(str, FALSE);

    return result;
}

/**
 * gwy_strkill:
 * @s: A NUL-terminated string.
 * @killchars: A string containing characters to kill.
 *
 * Removes characters in @killchars from string @s, modifying it in place.
 *
 * Use gwy_strkill(g_strdup(@s), @killchars) to get a modified copy.
 *
 * Returns: @s itself, the return value is to allow function call nesting.
 *
 * Since: 1.1.
 **/
gchar*
gwy_strkill(gchar *s,
            const gchar *killchars)
{
    gchar *p, *q;
    gchar killc;

    if (!killchars || !*killchars)
        return s;
    killc = *killchars;
    if (killchars[1])
        g_strdelimit(s, killchars+1, killc);
    if ((p = strchr(s, killc))) {
        for (q = p; *p; p++) {
            if (*p != killc) {
                *q = *p;
                q++;
            }
        }
        *q = '\0';
    }

    return s;
}

/**
 * gwy_strreplace:
 * @haystack: A NUL-terminated string to search in.
 * @needle: A NUL-terminated string to search for.
 * @replacement: A NUL-terminated string to replace @needle with.
 * @maxrepl: Maximum number of occurences to replace (use (gsize)-1 to replace
 *           all occurences).
 *
 * Replaces occurences of string @needle in @haystack with @replacement.
 *
 * Returns: A newly allocated string.
 *
 * Since: 1.1.
 **/
gchar*
gwy_strreplace(const gchar *haystack,
               const gchar *needle,
               const gchar *replacement,
               gsize maxrepl)
{
    gsize n, hlen, nlen, rlen, newlen;
    const gchar *p, *pp;
    gchar *dest, *q;

    nlen = strlen(needle);
    g_return_val_if_fail(nlen, NULL);
    n = 0;
    p = haystack;
    while ((p = strstr(p, needle)) && n < maxrepl) {
        p += nlen;
        n++;
    }
    if (!n)
        return g_strdup(haystack);

    hlen = strlen(haystack);
    rlen = strlen(replacement);
    newlen = hlen + n*rlen - n*nlen;

    dest = g_new(gchar, newlen+1);
    pp = haystack;
    q = dest;
    n = 0;
    while ((p = strstr(pp, needle)) && n < maxrepl) {
        memcpy(q, pp, p - pp);
        q += p - pp;
        memcpy(q, replacement, rlen);
        q += rlen;
        pp = p + nlen;
        n++;
    }
    strcpy(q, pp);

    return dest;
}

/**
 * gwy_file_get_contents:
 * @filename: A file to read contents of.
 * @buffer: Buffer to store the file contents.
 * @size: Location to store buffer (file) size.
 * @error: Return location for a #GError.
 *
 * Reads or maps file @filename into memory.
 *
 * The buffer must be treated as read-only and must be freed with
 * gwy_file_abandon_contents().  It is NOT guaranteed to be NUL-terminated,
 * use @size to find its end.
 *
 * Returns: Whether it succeeded.  In case of failure @buffer and @size are
 *          reset too.
 **/
gboolean
gwy_file_get_contents(const gchar *filename,
                      guchar **buffer,
                      gsize *size,
                      GError **error)
{
#ifdef USE_MMAP
    struct stat st;
    int fd;

    if (!error_domain)
        error_domain = g_quark_from_static_string("GWY_UTILS_ERROR");

    *buffer = NULL;
    *size = 0;
    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        g_set_error(error, error_domain, errno, "Cannot open file `%s': %s",
                    filename, g_strerror(errno));
        return FALSE;
    }
    if (fstat(fd, &st) == -1) {
        close(fd);
        g_set_error(error, error_domain, errno, "Cannot stat file `%s': %s",
                    filename, g_strerror(errno));
        return FALSE;
    }
    *buffer = (gchar*)mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (!*buffer) {
        g_set_error(error, error_domain, errno, "Cannot mmap file `%s': %s",
                    filename, g_strerror(errno));
        return FALSE;
    }
    *size = st.st_size;

    return TRUE;
#else
    return g_file_get_contents(filename, (gchar**)buffer, size, error);
#endif
}

/**
 * gwy_file_abandon_contents:
 * @buffer: Buffer with file contents as created by gwy_file_get_contents().
 * @size: Buffer size.
 * @error: Return location for a #GError.
 *
 * Frees or unmaps memory allocated by gwy_file_get_contents().
 *
 * Returns: Whether it succeeded.
 **/
gboolean
gwy_file_abandon_contents(guchar *buffer,
                          G_GNUC_UNUSED gsize size,
                          G_GNUC_UNUSED GError **error)
{
#ifdef USE_MMAP
    if (!error_domain)
        error_domain = g_quark_from_static_string("GWY_UTILS_ERROR");

    if (munmap(buffer, size) == 1) {
        g_set_error(error, error_domain, errno, "Cannot unmap memory: %s",
                    g_strerror(errno));
        return FALSE;
    }
    return TRUE;
#else
    g_free(buffer);

    return TRUE;
#endif
}

/**
 * gwy_debug_gnu:
 * @domain: Log domain.
 * @fileline: File and line info.
 * @funcname: Function name.
 * @format: Message format.
 * @...: Message parameters.
 *
 * Print a debugging message.
 *
 * To be used via gwy_debug(), should not be used directly.
 **/
void
gwy_debug_gnu(const gchar *domain,
              const gchar *fileline,
              const gchar *funcname,
              const gchar *format,
              ...)
{
    gchar *fmt2 = g_strconcat(fileline, ": ", funcname, ": ", format, NULL);
    va_list args;
    va_start(args, format);
    g_logv(domain, G_LOG_LEVEL_DEBUG, fmt2, args);
    va_end(args);
    g_free(fmt2);
}

/**
 * gwy_find_self_dir:
 * @dirname: A gwyddion directory name like "pixmaps" or "modules".
 *
 * Finds some [system] gwyddion directory.
 *
 * This function exists only because of insane Win32 instalation manners
 * where user can decide to put precompiled binaries anywhere.
 * On sane systems it just returns a copy of GWY_PIXMAP_DIR, etc. instead.
 *
 * The returned value is not actually tested for existence, it's up to caller.
 *
 * On Win32, gwy_find_self_set_argv0() must be called before any call to
 * gwy_find_self_dir().
 *
 * Returns: The path as a newly allocated string.
 **/
gchar*
gwy_find_self_dir(const gchar *dirname)
{
#ifdef G_OS_WIN32
    gchar *p, *q, *b;

    /* TODO: to be sure, we should put the path to the registry, too */
    /* argv[0] */
    p = g_strdup(gwy_argv0);
    if (!g_path_is_absolute(p)) {
        b = g_get_current_dir();
        q = g_build_filename(b, p, NULL);
        g_free(p);
        g_free(b);
        p = q;
    }
    /* now p contains an absolute path, the dir should be there */
    gwy_debug("gwyddion full path seems to be `%s'", p);
    q = g_path_get_dirname(p);
    g_free(p);
    p = q;
    q = g_build_filename(p, dirname, NULL);
    g_free(p);

    return q;
#endif /* G_OS_WIN32 */

#ifdef G_OS_UNIX
    static const struct { gchar *id; gchar *path; } paths[] = {
        { "modules", GWY_MODULE_DIR },
        { "pixmaps", GWY_PIXMAP_DIR },
        { "plugins", GWY_PLUGIN_DIR },
    };
    gsize i;

    for (i = 0; i < G_N_ELEMENTS(paths); i++) {
        if (strcmp(dirname, paths[i].id) == 0)
            return g_strdup(paths[i].path);
    }
    g_critical("Cannot find directory for `%s'", dirname);
    return NULL;
#endif /* G_OS_UNIX */
}

/**
 * gwy_find_self_set_argv0:
 * @argv0: Program's argv[0].
 *
 * Sets argv0 so that gwy_find_self_dir() can find self.
 **/
void
gwy_find_self_set_argv0(const gchar *argv0)
{
    g_free(gwy_argv0);
    gwy_argv0 = g_strdup(argv0);
}

/**
 * gwy_get_user_dir:
 *
 * Return directory where Gwyddion user settings and data should be stored.
 *
 * On Unix this is normally in home directory.  On silly platforms or silly
 * occasions, silly locations can be returned as fallback.
 *
 * Returns: The directory as a string that should not be freed.
 *
 * Since: 1.3.
 **/
G_CONST_RETURN gchar*
gwy_get_user_dir(void)
{
    const gchar *gwydir =
#ifdef G_OS_WIN32
        "gwyddion";
#else
        ".gwyddion";
#endif
    const gchar *homedir;
    static gchar *gwyhomedir = NULL;

    if (gwyhomedir)
        return gwyhomedir;

    homedir = g_get_home_dir();
    if (!homedir)
        homedir = g_get_tmp_dir();
#ifdef G_OS_WIN32
    if (!homedir)
        homedir = "C:\\Windows";  /* XXX :-))) */
#endif

    gwyhomedir = g_build_filename(homedir, gwydir, NULL);
    return gwyhomedir;
}

/************************** Documentation ****************************/
/* NB: gwymacros.h documentation is also here. */

/**
 * GwyEnum:
 * @name: Value name.
 * @value: The (integer) enum value.
 *
 * Enumerated type with named values.
 **/

/**
 * gwy_debug:
 * @format...: A format string followed by stuff to print.
 *
 * Prints a debugging message.
 *
 * Does nothing if compiled without DEBUG defined.
 **/

/**
 * GWY_SWAP:
 * @t: A C type.
 * @x: A variable of type @t to swap with @x.
 * @y: A variable of type @t to swap with @y.
 *
 * Swaps two variables (more precisely lhs and rhs expressions) of type @t
 * in a single statement.
 */

/**
 * gwy_object_unref:
 * @obj: A pointer to #GObject or %NULL.
 *
 * If @obj is not %NULL, unreferences @obj.  In all cases sets @obj to %NULL.
 *
 * If the object reference count is greater than one, assure it should be
 * referenced elsewhere, otherwise it leaks memory.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

