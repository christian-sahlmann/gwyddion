/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2011 David Necas (Yeti), Petr Klapetek.
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

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <gobject/gvaluecollector.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>

/* FIXME: We do not need anything from this file, but ./configure touches it
 * and we need to recompile after ./configure to get installation paths right.
 */
#include <libgwyddion/gwyversion.h>

#ifdef G_OS_WIN32
#include <windows.h>

G_WIN32_DLLMAIN_FOR_DLL_NAME(static, dll_name);
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#define TOLOWER(c) ((c) >= 'A' && (c) <= 'Z' ? (c) - 'A' + 'a' : (c))

static GHashTable *mapped_files = NULL;

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
 * gwy_strdiffpos:
 * @s1: A string.
 * @s2: A string.
 *
 * Finds position where two strings differ.
 *
 * Returns: The last position where the strings do not differ yet.
 *          Particularly, -1 is returned if either string is %NULL,
 *          zero-length, or they differ in the very first character.
 **/
gint
gwy_strdiffpos(const gchar *s1, const gchar *s2)
{
    const gchar *ss = s1;

    if (!s1 || !s2)
        return -1;

    while (*s1 && *s1 && *s1 == *s2) {
        s1++;
        s2++;
    }

    return (s1 - ss) - 1;
}

/**
 * gwy_strisident:
 * @s: A NUL-terminated string.
 * @more: List of additional ASCII characters allowed inside identifier, empty
 *        list can be passed as %NULL.
 * @startmore: List of additional ASCII characters allowed as the first
 *             identifier characters, empty list can be passed as %NULL.
 *
 * Checks whether a string is valid identifier.
 *
 * Valid identifier must start with an alphabetic character or a character from
 * @startmore, and it must continue with alphanumeric characters or characters
 * from @more.
 *
 * Note underscore is not allowed by default, you have to pass it in @more
 * and/or @startmore.
 *
 * Returns: %TRUE if @s is valid identifier, %FALSE otherwise.
 **/
gboolean
gwy_strisident(const gchar *s,
               const gchar *more,
               const gchar *startmore)
{
    const gchar *m;

    g_return_val_if_fail(s, FALSE);
    if (!g_ascii_isalpha(*s)) {
        if (!startmore)
            return FALSE;
        for (m = startmore; *m; m++) {
            if (*s == *m)
                break;
        }
        if (!*m)
            return FALSE;
    }
    s++;

    while (*s) {
        if (!g_ascii_isalnum(*s)) {
            if (!more)
                return FALSE;
            for (m = more; *m; m++) {
                if (*s == *m)
                    break;
            }
            if (!*m)
                return FALSE;
        }
        s++;
    }

    return TRUE;
}

/**
 * gwy_ascii_strcase_equal:
 * @v1: String key.
 * @v2: String key to compare with @v1.
 *
 * Compares two strings for equality, ignoring case.
 *
 * The case folding is performed only on ASCII characters.
 *
 * This function is intended to be passed to g_hash_table_new() as
 * @key_equal_func, namely in conjuction with gwy_ascii_strcase_hash() hashing
 * function.
 *
 * Returns: %TRUE if the two string keys match, ignoring case.
 *
 * Since: 2.26
 */
gboolean
gwy_ascii_strcase_equal(gconstpointer v1,
                        gconstpointer v2)
{
    const gchar *s1 = (const gchar*)v1, *s2 = (const gchar*)v2;

    while (*s1 && *s2) {
        if (TOLOWER(*s1) != TOLOWER(*s2))
            return FALSE;
        s1++, s2++;
    }
    return !*s1 && !*s2;
}

/**
 * gwy_ascii_strcase_hash:
 * @v: String key.
 *
 * Converts a string to a hash value, ignoring case.
 *
 * The case folding is performed only on ASCII characters.
 *
 * This function is intended to be passed to g_hash_table_new() as @hash_func,
 * namely in conjuction with gwy_ascii_strcase_equal() comparison function.
 *
 * Returns: The hash value corresponding to the key @v.
 *
 * Since: 2.26
 */
guint
gwy_ascii_strcase_hash(gconstpointer v)
{
    const signed char *p;
    guint32 h = 5381;

    for (p = v; *p != '\0'; p++)
        h = (h << 5) + h + TOLOWER(*p);

    return h;
}

/**
 * gwy_stramong:
 * @str: A string to find.
 * @...: %NULL-terminated list of string to compare @str with.
 *
 * Checks whether a string is equal to any from given list.
 *
 * Returns: Zero if @str does not equal to any string from the list, nozero
 *          othwerise.  More precisely, the position + 1 of the first string
 *          @str equals to is returned in the latter case.
 *
 * Since: 2.11
 **/
guint
gwy_stramong(const gchar *str,
             ...)
{
    va_list ap;
    const gchar *s;
    guint i = 1;

    va_start(ap, str);
    while ((s = va_arg(ap, const gchar*))) {
        if (gwy_strequal(str, s)) {
            va_end(ap);
            return i;
        }
        i++;
    }
    va_end(ap);

    return 0;
}

/**
 * gwy_memmem:
 * @haystack: Memory block to search in.
 * @haystack_len: Size of @haystack, in bytes.
 * @needle: Memory block to find.
 * @needle_len: Size of @needle, in bytes.
 *
 * Find a block of memory in another block of memory.
 *
 * This function is very similar to strstr(), except that it works with
 * arbitrary memory blocks instead of %NUL-terminated strings.
 *
 * If @needle_len is zero, @haystack is always returned.
 *
 * On GNU systems with glibc at least 2.1 this is a just a trivial memmem()
 * wrapper.  On other systems it emulates memmem() behaviour.
 *
 * Returns: Pointer to the first byte of memory block in @haystack that matches
 *          @needle; %NULL if no such block exists.
 *
 * Since: 2.12
 **/
gpointer
gwy_memmem(gconstpointer haystack,
           gsize haystack_len,
           gconstpointer needle,
           gsize needle_len)
{
#ifdef HAVE_MEMMEM
    return memmem(haystack, haystack_len, needle, needle_len);
#else
    const guchar *h = haystack;
    const guchar *s, *p;
    guchar n0;

    /* Empty needle can be found anywhere */
    if (!needle_len)
        return (gpointer)haystack;

    /* Needle that doesn't fit cannot be found anywhere */
    if (needle_len > haystack_len)
        return NULL;

    /* The general case */
    n0 = *(const guchar*)needle;
    s = h + haystack_len - needle_len;
    for (p = h; p && p <= s; p = memchr(p, n0, s-p + 1)) {
        if (memcmp(p, needle, needle_len) == 0)
            return (gpointer)p;
        p++;
    }
    return NULL;
#endif
}

/**
 * gwy_file_get_contents:
 * @filename: A file to read contents of.
 * @buffer: Buffer to store the file contents.
 * @size: Location to store buffer (file) size.
 * @error: Return location for a #GError.
 *
 * Reads or mmaps file @filename into memory.
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
    GMappedFile *mfile;

    mfile = g_mapped_file_new(filename, FALSE, error);
    if (!mfile) {
        *buffer = NULL;
        *size = 0;
        return FALSE;
    }

    *buffer = g_mapped_file_get_contents(mfile);
    *size = g_mapped_file_get_length(mfile);
    if (!mapped_files)
        mapped_files = g_hash_table_new(g_direct_hash, g_direct_equal);

    if (g_hash_table_lookup(mapped_files, *buffer)) {
        g_warning("File `%s' was mapped to address %p where we already have "
                  "mapped a file.  One of the files will leak.",
                  filename, buffer);
    }
    g_hash_table_insert(mapped_files, *buffer, mfile);

    return TRUE;
}

/**
 * gwy_file_abandon_contents:
 * @buffer: Buffer with file contents as created by gwy_file_get_contents().
 * @size: Buffer size.
 * @error: Return location for a #GError.  Since 2.22 no error can occur;
 *         safely pass %NULL.
 *
 * Frees or unmmaps memory allocated by gwy_file_get_contents().
 *
 * Returns: Whether it succeeded.  Since 2.22 it always return %TRUE.
 **/
gboolean
gwy_file_abandon_contents(guchar *buffer,
                          gsize size,
                          G_GNUC_UNUSED GError **error)
{
    GMappedFile *mfile;

    if (!mapped_files || !(mfile = g_hash_table_lookup(mapped_files, buffer))) {
        g_warning("Don't know anything about mapping to address %p.", buffer);
        return TRUE;
    }

    g_assert(g_mapped_file_get_length(mfile) == size);
    g_mapped_file_free(mfile);
    g_hash_table_remove(mapped_files, buffer);
    return TRUE;
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
 * gwy_sgettext:
 * @msgid: Message id to translate, containing `|'-separated prefix.
 *
 * Translate a message id containing disambiguating prefix ending with `|'.
 *
 * Returns: Translated message, or @msgid itself with all text up to the last
 *          `|' removed if there is no translation.
 **/
gchar*
gwy_sgettext(const gchar *msgid)
{
    char *msgstr, *p;

    msgstr = gettext(msgid);
    if (msgstr == msgid) {
        p = strrchr(msgstr, '|');
        return p ? p+1 : msgstr;
    }

    return msgstr;
}

#ifdef __APPLE__
static gchar *
gwy_osx_find_dir_in_bundle(const gchar *dirname)
{
    static gchar *basedir = NULL;

    if (!basedir) {
        int maxlen = 256;
        int len;
        char *res_url_path;

        CFURLRef res_url_ref = NULL, bundle_url_ref = NULL;

        res_url_ref =
            CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle());
        bundle_url_ref = CFBundleCopyBundleURL(CFBundleGetMainBundle());

        if (res_url_ref
            && bundle_url_ref && !CFEqual(res_url_ref, bundle_url_ref)) {

            res_url_path = malloc(maxlen);

            while (!CFURLGetFileSystemRepresentation(res_url_ref, true,
                                                     (UInt8*) res_url_path,
                                                     maxlen)) {
                maxlen *= 2;
                res_url_path = realloc(res_url_path, maxlen);
            }

            len = strlen(res_url_path);
            basedir = malloc(len + 1);
            strncpy(basedir, res_url_path, len);
            basedir[len] = 0;
            free(res_url_path);
        }
        if (res_url_ref)
            CFRelease(res_url_ref);
        if (bundle_url_ref)
            CFRelease(bundle_url_ref);
    }
    if (gwy_strequal(dirname, "data"))
        dirname = NULL;

    return basedir ? g_build_filename(basedir, dirname, NULL) : NULL;
}
#endif

/**
 * gwy_find_self_dir:
 * @dirname: A gwyddion directory name:
 *           <literal>"modules"</literal>,
 *           <literal>"plugins"</literal>,
 *           <literal>"pixmaps"</literal>,
 *           <literal>"locale"</literal>, or
 *           <literal>"data"</literal>.
 *
 * Finds a system Gwyddion directory.
 *
 * On Unix, a compiled-in path is returned, unless it's overriden with
 * environment variables (see gwyddion manual page).
 *
 * On Win32, the directory where the libgwyddion DLL from which this function
 * was called resides is taken as the base and the location of other Gwyddion
 * directories is calculated from it.
 *
 * The returned value is not actually tested for existence, it's up to caller.
 *
 * To obtain the Gwyddion user directory see gwy_get_user_dir().
 *
 * Returns: The path as a newly allocated string.
 **/
gchar*
gwy_find_self_dir(const gchar *dirname)
{
#ifdef G_OS_UNIX
#ifdef GWY_LIBDIR
    static const struct {
        const gchar *id;
        const gchar *base;
        const gchar *env;
        const gchar *dir;
    }
    paths[] = {
        {
            "modules",
            GWY_LIBDIR,
            "GWYDDION_LIBDIR",
            "gwyddion/modules"
        },
        {
            "plugins",
            GWY_LIBEXECDIR,
            "GWYDDION_LIBEXECDIR",
            "gwyddion/plugins"
        },
        {
            "pixmaps",
            GWY_DATADIR,
            "GWYDDION_DATADIR",
            "gwyddion/pixmaps",
        },
        {
            "data",
            GWY_DATADIR,
            "GWYDDION_DATADIR",
            "gwyddion",
        },
        {
            "locale",
            GWY_LOCALEDIR,
            "GWYDDION_LOCALEDIR",
            NULL,
        },
    };
    gsize i;
    const gchar *base;
#endif    /* GWY_LIBDIR */
#ifdef __APPLE__
    char *ret = gwy_osx_find_dir_in_bundle(dirname);

    if (ret)
        return ret;
#endif    /* __APPLE__ */
#ifdef GWY_LIBDIR
    for (i = 0; i < G_N_ELEMENTS(paths); i++) {
        if (!gwy_strequal(dirname, paths[i].id))
            continue;

        if (!(base = g_getenv(paths[i].env)))
            base = paths[i].base;

        gwy_debug("for <%s> base = <%s>, dir = <%s>",
                  dirname, base, paths[i].dir);
        return g_build_filename(base, paths[i].dir, NULL);
    }
#endif    /* GWY_LIBDIR */
#endif    /* G_OS_UNIX */

#ifdef G_OS_WIN32
    static const struct {
        const gchar *id;
        const gchar *env;
        const gchar *base;
        const gchar *dir;
    }
    paths[] = {
        {
            "modules",
            "GWYDDION_LIBDIR",
            "lib",
            "gwyddion/modules"
        },
        {
            "plugins",
            "GWYDDION_LIBEXECDIR",
            "libexec",
            "gwyddion/plugins"
        },
        {
            "pixmaps",
            "GWYDDION_DATADIR",
            "share",
            "gwyddion/pixmaps",
        },
        {
            "data",
            "GWYDDION_DATADIR",
            "share",
            "gwyddion",
        },
        {
            "locale",
            "GWYDDION_LOCALEDIR",
            "share",
            "locale",
        },
    };
    static gchar *topdir = NULL;
    gsize i;
    const gchar *base;

    if (!topdir)
        topdir = g_win32_get_package_installation_directory(NULL, dll_name);

    for (i = 0; i < G_N_ELEMENTS(paths); i++) {
        if (!gwy_strequal(dirname, paths[i].id))
            continue;

        if ((base = g_getenv(paths[i].env))) {
            gwy_debug("for <%s> base = <%s>, dir = <%s>",
                      dirname, base, paths[i].dir);
            return g_build_filename(base, paths[i].dir, NULL);
        }
        gwy_debug("for <%s> top = <%s>, klass = <%s>, dir = <%s>",
                  dirname, topdir, paths[i].base, paths[i].dir);
        return g_build_filename(topdir, paths[i].base, paths[i].dir, NULL);
    }
#endif    /* G_OS_WIN32 */

    g_critical("Cannot find directory for `%s'", dirname);
    return NULL;
}

/**
 * gwy_get_user_dir:
 *
 * Returns the directory where Gwyddion user settings and data should be stored.
 *
 * On Unix this is usually a dot-directory in user's home directory.  On modern
 * Win32 the returned directory resides in user's Documents and Settings.
 * On silly platforms or silly occasions, silly locations (namely a temporary
 * directory) can be returned as fallback.
 *
 * To obtain a Gwyddion system directory see gwy_find_self_dir().
 *
 * Returns: The directory as a constant string that should not be freed.
 **/
const gchar*
gwy_get_user_dir(void)
{
    const gchar *gwydir =
#ifdef G_OS_WIN32
        "gwyddion";
#else
        ".gwyddion";
#endif
    static gchar *gwyhomedir = NULL;

    if (gwyhomedir)
        return gwyhomedir;

    gwyhomedir = g_build_filename(gwy_get_home_dir(), gwydir, NULL);
    return gwyhomedir;
}

/**
 * gwy_get_home_dir:
 *
 * Returns home directory, or temporary directory as a fallback.
 *
 * Under normal circumstances the same string as g_get_home_dir() would return
 * is returned.  But on MS Windows, something like "C:\Windows\Temp" can be
 * returned too, as it is as good as anything else (we can write there).
 *
 * Returns: Something usable as user home directory.  It may be silly, but
 *          never %NULL or empty.
 **/
const gchar*
gwy_get_home_dir(void)
{
    const gchar *homedir = NULL;

    if (homedir)
        return homedir;

    homedir = g_get_home_dir();
    if (!homedir || !*homedir)
        homedir = g_get_tmp_dir();
#ifdef G_OS_WIN32
    if (!homedir || !*homedir)
        homedir = "C:\\Windows";  /* XXX :-))) */
#else
    if (!homedir || !*homedir)
        homedir = "/tmp";
#endif

    return homedir;
}

/**
 * gwy_canonicalize_path:
 * @path: A filesystem path.
 *
 * Canonicalizes a filesystem path.
 *
 * Particularly it makes the path absolute, resolves `..' and `.', and fixes
 * slash sequences to single slashes.  On Win32 it also converts all
 * backslashes to slashes along the way.
 *
 * Note this function does NOT resolve symlinks, use g_file_read_link() for
 * that.
 *
 * Returns: The canonical path, as a newly created string.
 **/
gchar*
gwy_canonicalize_path(const gchar *path)
{
    gchar *spath, *p0, *p, *last_slash;
    gsize i;

    g_return_val_if_fail(path, NULL);

    /* absolutize */
    if (!g_path_is_absolute(path)) {
        p = g_get_current_dir();
        spath = g_build_filename(p, path, NULL);
        g_free(p);
    }
    else
        spath = g_strdup(path);
    p = spath;

#ifdef G_OS_WIN32
    /* convert backslashes to slashes */
    while (*p) {
        if (*p == '\\')
            *p = '/';
        p++;
    }
    p = spath;

    /* skip c:, //server */
    if (g_ascii_isalpha(*p) && p[1] == ':')
        p += 2;
    else if (*p == '/' && p[1] == '/') {
        p = strchr(p+2, '/');
        /* silly, but better this than a coredump... */
        if (!p)
            return spath;
    }
    /* now p starts with the `root' / on all systems */
#endif
    g_return_val_if_fail(*p == '/', spath);

    p0 = p;
    while (*p) {
        if (*p == '/') {
            if (p[1] == '.') {
                if (p[2] == '/' || !p[2]) {
                    /* remove from p here */
                    for (i = 0; p[i+2]; i++)
                        p[i] = p[i+2];
                    p[i] = '\0';
                }
                else if (p[2] == '.' && (p[3] == '/' || !p[3])) {
                    /* remove from last_slash here */
                    /* ignore if root element */
                    if (p == p0) {
                        for (i = 0; p[i+3]; i++)
                            p[i] = p[i+3];
                        p[i] = '\0';
                    }
                    else {
                        for (last_slash = p-1; *last_slash != '/'; last_slash--)
                          ;
                        for (i = 0; p[i+3]; i++)
                            last_slash[i] = p[i+3];
                        last_slash[i] = '\0';
                        p = last_slash;
                    }
                }
                else
                    p++;
            }
            else {
                /* remove a continouos sequence of slashes */
                for (last_slash = p; *last_slash == '/'; last_slash++)
                    ;
                last_slash--;
                if (last_slash > p) {
                    for (i = 0; last_slash[i]; i++)
                        p[i] = last_slash[i];
                    p[i] = '\0';
                }
                else
                    p++;
            }
        }
        else
            p++;
    }
    /* a final `..' could fool us into discarding the starting slash */
    if (!*p0) {
      *p0 = '/';
      p0[1] = '\0';
    }

    return spath;
}

/**
 * gwy_filename_ignore:
 * @filename_sys: File name in GLib encoding.
 *
 * Checks whether file should be ignored.
 *
 * This function checks for common file names indicating files that should
 * be normally ignored.  Currently it means backup files (ending with ~ or
 * .bak) and Unix hidden files (starting with a dot).
 *
 * Returns: %TRUE to ignore this file, %FALSE otherwise.
 **/
gboolean
gwy_filename_ignore(const gchar *filename_sys)
{
    if (!filename_sys
        || !*filename_sys
        || filename_sys[0] == '.'
        || g_str_has_suffix(filename_sys, "~")
        || g_str_has_suffix(filename_sys, ".bak")
        || g_str_has_suffix(filename_sys, ".BAK"))
        return TRUE;

    return FALSE;
}

/**
 * gwy_str_next_line:
 * @buffer: Text buffer.
 *
 * Extracts a next line from a character buffer, modifying it in place.
 *
 * @buffer is updated to point after the end of the line and the "\n"
 * (or "\r" or "\r\n") is replaced with "\0", if present.
 *
 * The final line may or may not be terminated with an EOL marker, its contents
 * is returned in either case.  Note, however, that the empty string "" is not
 * interpreted as an empty unterminated line.  Instead, %NULL is immediately
 * returned.
 *
 * The typical usage of gwy_str_next_line() is:
 * |[
 * gchar *p = text;
 * for (gchar *line = gwy_str_next_line(&p);
 *      line;
 *      line = gwy_str_next_line(&p)) {
 *     g_strstrip(line);
 *     // Do something more with line
 * }
 * ]|
 *
 * Returns: The start of the line.  %NULL if the buffer is empty or %NULL.
 *          The return value is <emphasis>not</emphasis> a new string; the
 *          normal return value is the previous value of @buffer.
 **/
gchar*
gwy_str_next_line(gchar **buffer)
{
    gchar *p, *q;

    if (!buffer || !*buffer)
        return NULL;

    q = *buffer;
    if (!*q) {
        *buffer = NULL;
        return NULL;
    }

    for (p = q; *p != '\n' && *p != '\r' && *p; p++)
        ;
    if (p[0] == '\r' && p[1] == '\n') {
        *(p++) = '\0';
        *(p++) = '\0';
    }
    else if (p[0]) {
        *(p++) = '\0';
    }

    *buffer = p;
    return q;
}

/**
 * gwy_memcpy_byte_swap:
 * @source: Source memory block.
 * @dest: Destination memory location.
 * @item_size: Size of one copied item, it should be a power of two.
 * @nitems: Number of items of size @item_size to copy.
 * @byteswap: Byte swap pattern.
 *
 * Copies a block of memory swapping bytes along the way.
 *
 * The bits in @byteswap correspond to groups of bytes to swap: if j-th bit is
 * set, adjacent groups of 2j bits are swapped. For example, value 3 means
 * items will be divided into couples (bit 1) of bytes and adjacent couples of
 * bytes swapped, and then divided into single bytes (bit 0) and adjacent bytes
 * swapped. The net effect is reversal of byte order in groups of four bytes.
 * More generally, if you want to reverse byte order in groups of size
 * 2<superscript>j</superscript>, use byte swap pattern j-1.
 *
 * When @byteswap is zero, this function reduces to plain memcpy().
 *
 * Since: 2.1
 **/
void
gwy_memcpy_byte_swap(const guint8 *source,
                     guint8 *dest,
                     gsize item_size,
                     gsize nitems,
                     gsize byteswap)
{
    gsize i, k;

    if (!byteswap) {
        memcpy(dest, source, item_size*nitems);
        return;
    }

    for (i = 0; i < nitems; i++) {
        guint8 *b = dest + i*item_size;

        for (k = 0; k < item_size; k++)
            b[k ^ byteswap] = *(source++);
    }
}

#ifdef __GNUC__
#  define gwy_powi __builtin_powi
#else
static inline double
gwy_powi(double x, int i)
{
    gdouble r = 1.0;
    if (!i)
        return 1.0;
    gboolean negative = FALSE;
    if (i < 0) {
        negative = TRUE;
        i = -i;
    }
    for ( ; ; ) {
        if (i & 1)
            r *= x;
        if (!(i >>= 1))
            break;
        x *= x;
    }
    return negative ? 1.0/r : r;
}
#endif

static inline gdouble
get_pascal_real_le(const guchar *p)
{
    gdouble x;

    if (!p[0])
        return 0.0;

    x = 1.0 + ((((p[1]/256.0 + p[2])/256.0 + p[3])/256.0 + p[4])/256.0
               + (p[5] & 0x7f))/128.0;
    if (p[5] & 0x80)
        x = -x;

    return x*gwy_powi(2.0, (gint)p[0] - 129);
}

static inline gdouble
get_pascal_real_be(const guchar *p)
{
    gdouble x;

    if (!p[5])
        return 0.0;

    x = 1.0 + ((((p[4]/256.0 + p[3])/256.0 + p[2])/256.0 + p[1])/256.0
               + (p[0] & 0x7f))/128.0;
    if (p[0] & 0x80)
        x = -x;

    return x*gwy_powi(2.0, (gint)p[5] - 129);
}

static inline gdouble
get_half_le(const guchar *p)
{
    gdouble x = p[0]/1024.0 + (p[1] & 0x03)/4.0;
    gint exponent = (p[1] >> 2) & 0x1f;

    if (G_UNLIKELY(exponent == 0x1f)) {
        /* XXX: Gwyddion does not work with NaNs.  Should we produce them?*/
        if (x)
            return 0.0/0.0;
        return (p[1] & 0x80) ? -HUGE_VAL : HUGE_VAL;
    }

    if (exponent)
        x = (1.0 + x)*gwy_powi(2.0, exponent - 15);
    else
        x = x/16384.0;

    return (p[1] & 0x80) ? -x : x;
}

static inline gdouble
get_half_be(const guchar *p)
{
    gdouble x = p[1]/1024.0 + (p[0] & 0x03)/4.0;
    gint exponent = (p[0] >> 2) & 0x1f;

    if (G_UNLIKELY(exponent == 0x1f)) {
        /* XXX: Gwyddion does not work with NaNs.  Should we produce them?*/
        if (x)
            return 0.0/0.0;
        return (p[0] & 0x80) ? -HUGE_VAL : HUGE_VAL;
    }

    if (exponent)
        x = (1.0 + x)*gwy_powi(2.0, exponent - 15);
    else
        x = x/16384.0;

    return (p[0] & 0x80) ? -x : x;
}

/**
 * gwy_convert_raw_data:
 * @data: Pointer to the input raw data to be converted to doubles.  The data
 *        type is given by @datatype and @byteorder.
 * @nitems: Data block length, i.e. the number of consecutive items to convert.
 * @stride: Item stride in the raw data, measured in raw values.  Pass 1 for
 *          consecutive raw data.  For backward reading, pass -1 and point
 *          @data to the last raw item instead of the first.
 * @datatype: Type of the raw data items.
 * @byteorder: Byte order of the raw data.
 * @target: Array of @nitems to store the converted input data to.
 * @scale: Factor to multiply the data with.
 * @offset: Constant to add to the data after multiplying with @scale.
 *
 * Converts a block of raw data items to doubles.
 *
 * Note that conversion from 64bit integral types may lose information as they
 * have more bits than the mantissa of doubles.  All other conversions should
 * be precise.
 *
 * Since: 2.25
 **/
void
gwy_convert_raw_data(gconstpointer data,
                     gsize nitems,
                     gssize stride,
                     GwyRawDataType datatype,
                     GwyByteOrder byteorder,
                     gdouble *target,
                     gdouble scale,
                     gdouble offset)
{
    gboolean littleendian = (byteorder == GWY_BYTE_ORDER_LITTLE_ENDIAN
                             || (G_BYTE_ORDER == G_LITTLE_ENDIAN
                                 && byteorder == GWY_BYTE_ORDER_NATIVE));
    gboolean byteswap = (byteorder && byteorder != G_BYTE_ORDER);
    gsize i;

    g_return_if_fail(data && target);

    if (datatype == GWY_RAW_DATA_SINT8) {
        const gint8 *s8 = (const gint8*)data;
        for (i = nitems; i; i--, s8 += stride, target++)
            *target = scale*(*s8) + offset;
    }
    else if (datatype == GWY_RAW_DATA_UINT8) {
        const guint8 *u8 = (const guint8*)data;
        for (i = nitems; i; i--, u8 += stride, target++)
            *target = scale*(*u8) + offset;
    }
    else if (datatype == GWY_RAW_DATA_SINT16) {
        const gint16 *s16 = (const gint16*)data;
        if (byteswap) {
            for (i = nitems; i; i--, s16 += stride, target++)
                *target = scale*(gint16)GUINT16_SWAP_LE_BE(*s16) + offset;
        }
        else {
            for (i = nitems; i; i--, s16 += stride, target++)
                *target = scale*(*s16) + offset;
        }
    }
    else if (datatype == GWY_RAW_DATA_UINT16) {
        const guint16 *u16 = (const guint16*)data;
        if (byteswap) {
            for (i = nitems; i; i--, u16 += stride, target++)
                *target = scale*GUINT16_SWAP_LE_BE(*u16) + offset;
        }
        else {
            for (i = nitems; i; i--, u16 += stride, target++)
                *target = scale*(*u16) + offset;
        }
    }
    else if (datatype == GWY_RAW_DATA_SINT32) {
        const gint32 *s32 = (const gint32*)data;
        if (byteswap) {
            for (i = nitems; i; i--, s32 += stride, target++)
                *target = scale*(gint32)GUINT32_SWAP_LE_BE(*s32) + offset;
        }
        else {
            for (i = nitems; i; i--, s32 += stride, target++)
                *target = scale*(*s32) + offset;
        }
    }
    else if (datatype == GWY_RAW_DATA_UINT32) {
        const guint32 *u32 = (const guint32*)data;
        if (byteswap) {
            for (i = nitems; i; i--, u32 += stride, target++)
                *target = scale*GUINT32_SWAP_LE_BE(*u32) + offset;
        }
        else {
            for (i = nitems; i; i--, u32 += stride, target++)
                *target = scale*(*u32) + offset;
        }
    }
    else if (datatype == GWY_RAW_DATA_SINT64) {
        const gint64 *s64 = (const gint64*)data;
        if (byteswap) {
            for (i = nitems; i; i--, s64 += stride, target++)
                *target = scale*(gint64)GUINT64_SWAP_LE_BE(*s64) + offset;
        }
        else {
            for (i = nitems; i; i--, s64 += stride, target++)
                *target = scale*(*s64) + offset;
        }
    }
    else if (datatype == GWY_RAW_DATA_UINT64) {
        const guint64 *u64 = (const guint64*)data;
        if (byteswap) {
            for (i = nitems; i; i--, u64 += stride, target++)
                *target = scale*GUINT64_SWAP_LE_BE(*u64) + offset;
        }
        else {
            for (i = nitems; i; i--, u64 += stride, target++)
                *target = scale*(*u64) + offset;
        }
    }
    else if (datatype == GWY_RAW_DATA_HALF) {
        const guchar *p = (const guchar*)data;
        if (littleendian) {
            for (i = nitems; i; i--, p += 2*stride, target++)
                *target = scale*get_half_le(p) + offset;
        }
        else {
            for (i = nitems; i; i--, p += 2*stride, target++)
                *target = scale*get_half_be(p) + offset;
        }
    }
    else if (datatype == GWY_RAW_DATA_FLOAT) {
        const guint32 *u32 = (const guint32*)data;
        const gfloat *f32 = (const gfloat*)data;
        union { guint32 u; gfloat f; } v;
        if (byteswap) {
            for (i = nitems; i; i--, u32 += stride, target++) {
                v.u = GUINT32_SWAP_LE_BE(*u32);
                *target = scale*v.f + offset;
            }
        }
        else {
            for (i = nitems; i; i--, f32 += stride, target++)
                *target = scale*(*f32) + offset;
        }
    }
    else if (datatype == GWY_RAW_DATA_REAL) {
        const guchar *p = (const guchar*)data;
        if (littleendian) {
            for (i = nitems; i; i--, p += 6*stride, target++)
                *target = scale*get_pascal_real_le(p) + offset;
        }
        else {
            for (i = nitems; i; i--, p += 6*stride, target++)
                *target = scale*get_pascal_real_be(p) + offset;
        }
    }
    else if (datatype == GWY_RAW_DATA_DOUBLE) {
        const guint64 *u64 = (const guint64*)data;
        const gdouble *d64 = (const gdouble*)data;
        union { guint64 u; double d; } v;
        if (byteswap) {
            for (i = nitems; i; i--, u64 += stride, target++) {
                v.u = GUINT64_SWAP_LE_BE(*u64);
                *target = scale*v.d + offset;
            }
        }
        else {
            for (i = nitems; i; i--, d64 += stride, target++)
                *target = scale*(*d64) + offset;
        }
    }
    else {
        g_assert_not_reached();
    }
}

/**
 * gwy_raw_data_size:
 * @datatype: Raw data type.
 *
 * Reports the size of a single raw data item.
 *
 * Returns: The size of a single raw data item of type @datatype.
 *
 * Since: 2.25
 **/
guint
gwy_raw_data_size(GwyRawDataType datatype)
{
    static const guint sizes[GWY_RAW_DATA_DOUBLE+1] = {
        1, 1, 2, 2, 4, 4, 8, 8, 2, 4, 6, 8
    };

    g_return_val_if_fail(datatype <= GWY_RAW_DATA_DOUBLE, 0);
    return sizes[datatype];
}

/**
 * gwy_object_set_or_reset:
 * @object: A #GObject.
 * @type: The type whose properties are to reset, may be zero for all types.
 *        The object must be of this type (more precisely @object is-a
 *        @type must hold).
 * @...: %NULL-terminated list of name/value pairs of properties to set as
 *       in g_object_set().  It can be %NULL alone to only reset properties
 *       to defaults.
 *
 * Sets object properties, resetting other properties to defaults.
 *
 * All explicitly specified properties are set.  In addition, all unspecified
 * settable properties of type @type (or all unspecified properties if @type is
 * 0) are reset to defaults.  Settable means the property is writable and not
 * construction-only.
 *
 * The order in which properties are set is undefined beside keeping the
 * relative order of explicitly specified properties, therefore this function
 * is not generally usable for objects with interdependent properties.
 *
 * Unlike g_object_set(), it does not set properties that already have the
 * requested value, as a consequences notifications are emitted only for
 * properties which actually change.
 **/
void
gwy_object_set_or_reset(gpointer object,
                        GType type,
                        ...)
{
    GValue value, cur_value, new_value;
    GObjectClass *klass;
    GParamSpec **pspec;
    gboolean *already_set;
    const gchar *name;
    va_list ap;
    gint nspec, i;

    klass = G_OBJECT_GET_CLASS(object);
    g_return_if_fail(G_IS_OBJECT_CLASS(klass));
    g_return_if_fail(!type || G_TYPE_CHECK_INSTANCE_TYPE(object, type));

    g_object_freeze_notify(object);

    pspec = g_object_class_list_properties(klass, &nspec);
    already_set = g_newa(gboolean, nspec);
    gwy_clear(already_set, nspec);

    gwy_clear(&cur_value, 1);
    gwy_clear(&new_value, 1);

    va_start(ap, type);
    for (name = va_arg(ap, gchar*); name; name = va_arg(ap, gchar*)) {
        gchar *error = NULL;

        for (i = 0; i < nspec; i++) {
            if (gwy_strequal(pspec[i]->name, name))
                break;
        }

        /* Do only minimal checking, because we cannot avoid re-checking in
         * g_object_set_property(), or at least not w/o copying a large
         * excerpt of GObject here.  Considerable amount of work is still
         * performed inside GObject again... */
        if (i == nspec) {
            g_warning("object class `%s' has no property named `%s'",
                      G_OBJECT_TYPE_NAME(object), name);
            continue;
        }

        g_value_init(&new_value, pspec[i]->value_type);
        G_VALUE_COLLECT(&new_value, ap, 0, &error);
        if (error) {
            g_warning("%s", error);
            g_free(error);
            g_value_unset(&new_value);
            break;
        }

        g_value_init(&cur_value, pspec[i]->value_type);
        g_object_get_property(object, name, &cur_value);
        if (g_param_values_cmp(pspec[i], &new_value, &cur_value) != 0)
            g_object_set_property(object, name, &new_value);

        g_value_unset(&cur_value);
        g_value_unset(&new_value);
        already_set[i] = TRUE;
    }
    va_end(ap);

    gwy_clear(&value, 1);
    for (i = 0; i < nspec; i++) {
        if (already_set[i]
            || !(pspec[i]->flags & G_PARAM_WRITABLE)
            || (pspec[i]->flags & G_PARAM_CONSTRUCT_ONLY)
            || (type && pspec[i]->owner_type != type))
            continue;

        g_value_init(&value, pspec[i]->value_type);
        g_object_get_property(object, pspec[i]->name, &value);
        if (!g_param_value_defaults(pspec[i], &value)) {
            g_param_value_set_default(pspec[i], &value);
            g_object_set_property(object, pspec[i]->name, &value);
        }

        g_value_unset(&value);
    }

    g_free(pspec);

    g_object_thaw_notify(object);
}

/************************** Documentation ****************************/
/* Note: gwymacros.h documentation is also here. */

/**
 * SECTION:gwyutils
 * @title: gwyutils
 * @short_description: Various utility functions
 * @see_also: <link linkend="libgwyddion-gwymacros">gwymacros</link> --
 *            utility macros
 *
 * Various utility functions: creating GLib lists from hash tables
 * gwy_hash_table_to_list_cb()), protably finding Gwyddion application
 * directories (gwy_find_self_dir()), string functions (gwy_strreplace()), path
 * manipulation (gwy_canonicalize_path()).
 **/

/**
 * SECTION:gwymacros
 * @title: gwymacros
 * @short_description: Utility macros
 * @see_also: <link linkend="libgwyddion-gwyutils">gwyutils</link> -- utility
 *            functions
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
 * GWY_FIND_PSPEC:
 * @type: Object type (e.g. %GWY_TYPE_CONTAINER).
 * @id: Property id.
 * @spectype: Param spec type (e.g. <literal>DOUBLE</literal>).
 *
 * A convenience g_object_class_find_property() wrapper.
 *
 * It expands to property spec cast to correct type (@spec).
 **/

/**
 * gwy_strequal:
 * @a: A string.
 * @b: Another string.
 *
 * Expands to %TRUE if strings are equal, to %FALSE otherwise.
 **/

/**
 * GWY_CLAMP:
 * @x: The value to clamp.
 * @low: The minimum value allowed.
 * @hi: The maximum value allowed.
 *
 * Ensures that @x is between the limits set by @low and @hi.
 *
 * This macro differs from GLib's CLAMP() by G_UNLIKELY() assertions on the
 * tests that @x is smaller than @low and larger than @hi.  This makes @x
 * already being in the right range the fast code path.
 *
 * It is supposed to be used on results of floating-point operations that
 * should fall to a known range but may occasionaly fail to due to rounding
 * errors and in similar situations.  Under normal circumstances, use CLAMP().
 **/

/**
 * gwy_clear:
 * @array: Pointer to an array of values to clear.
 *         This argument may be evaluated several times.
 * @n: Number of items to clear.
 *
 * Fills memory block representing an array with zeroes.
 *
 * This is a shorthand for memset, with the number of bytes to fill calculated
 * from the type of the pointer.
 *
 * Since: 2.12
 **/

/**
 * gwy_object_unref:
 * @obj: A pointer to #GObject or %NULL (must be an l-value).
 *
 * Unreferences an object if it exists.
 *
 * If @obj is not %NULL, g_object_unref() is called on it.
 * In all cases @obj is set to %NULL.
 *
 * A useful property of this macro is its idempotence.
 *
 * If the object reference count is greater than one, assure it should be
 * referenced elsewhere, otherwise it leaks memory.
 **/

/**
 * gwy_signal_handler_disconnect:
 * @obj: A pointer to #GObject or %NULL.
 * @hid: An @obj signal handler id or 0 (must be an l-value).
 *
 * Disconnect a signal handler if it exists.
 *
 * If @hid is nonzero and @obj is not %NULL, the signal handler identified by
 * @hid is disconnected.  In all cases @hid is set to 0.
 *
 * A useful property of this macro is its idempotence.
 **/

/**
 * GwyRawDataType:
 * @GWY_RAW_DATA_SINT8: Signed 8bit integer (one byte).
 * @GWY_RAW_DATA_UINT8: Unsigned 8bit integer (one byte).
 * @GWY_RAW_DATA_SINT16: Signed 16bit integer (two bytes).
 * @GWY_RAW_DATA_UINT16: Unsigned 16bit integer (two bytes).
 * @GWY_RAW_DATA_SINT32: Signed 32bit integer (four bytes).
 * @GWY_RAW_DATA_UINT32: Unsigned 32bit integer (four bytes).
 * @GWY_RAW_DATA_SINT64: Signed 64bit integer (eight bytes).
 * @GWY_RAW_DATA_UINT64: Unsigned 64bit integer (eight bytes).
 * @GWY_RAW_DATA_HALF: Half-precision floating point number (two bytes).
 * @GWY_RAW_DATA_FLOAT: Single-precision floating point number (four bytes).
 * @GWY_RAW_DATA_REAL: Pascal ‘real’ floating point number (six bytes).
 * @GWY_RAW_DATA_DOUBLE: Double-precision floating point number (eight bytes).
 *
 * Types of raw data.
 *
 * Multibyte types usually need to be complemented with #GwyByteOrder to get a
 * full type specification.
 *
 * Since: 2.25
 **/

/**
 * GwyByteOrder:
 * @GWY_BYTE_ORDER_NATIVE: Native byte order.
 * @GWY_BYTE_ORDER_LITTLE_ENDIAN: Little endian byte order (the same as
 *                                %G_LITTLE_ENDIAN).
 * @GWY_BYTE_ORDER_BIG_ENDIAN: Big endian byte order (the same as
 *                             %G_BIG_ENDIAN).
 *
 * Type of byte order.
 *
 * Since: 2.25
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

