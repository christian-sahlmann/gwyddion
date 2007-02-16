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

#ifndef __GWY_MACROS_H__
#define __GWY_MACROS_H__

#include <stdarg.h>
#include <glib.h>
#include <gwyconfig.h>

/* FIXME: move to gwyconfig.h? or just config.h? */
/* XXX: Most of this is available in gi18n.h */
#ifdef ENABLE_NLS
#include <libintl.h>
#else
#define gettext(x) (x)
#define ngettext(sing, plur, n) ((n) == 1 ? (sing) : (plur))
#endif
#define _(x) gettext(x)

#ifdef gettext_noop
#define N_(x) gettext_noop(x)
#else
#define N_(x) (x)
#endif

#define GWY_SWAP(t, x, y) \
    do { \
    t __unsafe_swap; \
    __unsafe_swap = x; \
    x = y; \
    y = __unsafe_swap; \
    } while (0)

#define gwy_strequal(a, b) \
    (!strcmp((a), (b)))

#define GWY_CLAMP(x, low, hi) \
    (G_UNLIKELY((x) > (hi)) ? (hi) : (G_UNLIKELY((x) < (low)) ? (low) : (x)))

#define gwy_object_unref(obj) \
    do { \
        if (obj) \
            g_object_unref(obj); \
        (obj) = NULL; \
    } while (0)

#define gwy_signal_handler_disconnect(obj, hid) \
    do { \
        if (hid && obj) \
            g_signal_handler_disconnect(obj, hid); \
        (hid) = 0; \
    } while (0)

#define GWY_FIND_PSPEC(type, id, spectype) \
    G_PARAM_SPEC_##spectype(g_object_class_find_property \
                                (G_OBJECT_CLASS(g_type_class_peek(type)), id))

G_BEGIN_DECLS

#ifdef G_HAVE_GNUC_VARARGS
#  ifdef DEBUG
#    define gwy_debug(format...) \
            gwy_debug_gnu(G_LOG_DOMAIN,\
                          __FILE__ ":" G_STRINGIFY (__LINE__), \
                          G_STRFUNC, \
                          format)
#  else
#    define gwy_debug(format...) /* */
#  endif
#elif defined(G_HAVE_ISO_VARARGS)
#  ifdef DEBUG
#    define gwy_debug(...) \
            gwy_debug_gnu(G_LOG_DOMAIN, \
                          __FILE__ ":" G_STRINGIFY(__LINE__), \
                          G_STRFUNC, \
                          __VA_ARGS__)
#  else
#    define gwy_debug(...) /* */
#  endif
#else
/* no varargs macros */
#  ifdef DEBUG
static inline void
gwy_debug(const gchar *format, ...)
{
    va_list args;
    va_start(args, format);
    g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}
#  else
static inline void
gwy_debug(const gchar *format, ...)
{
}
#  endif
#endif /* varargs macros */

void gwy_debug_gnu(const gchar *domain,
                   const gchar *fileline,
                   const gchar *funcname,
                   const gchar *format,
                   ...) G_GNUC_PRINTF(4, 5);

G_END_DECLS

#endif /* __GWY_MACROS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

