/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#ifndef __GWY_GWYMACROS_H__
#define __GWY_GWYMACROS_H__

#include <glib/gmessages.h>
#include <glib/gmacros.h>

#define _(x) (x)

/**
 * gwy_object_unref:
 * @obj: A pointer to #GObject or %NULL.
 *
 * If @obj is not %NULL, unreferences @obj.  In all cases sets @obj to %NULL.
 *
 * If the object reference count is greater than one, assure it't referenced
 * elsewhere.
 **/
#define gwy_object_unref(obj) \
    do { \
    if (obj) \
        g_object_unref(obj); \
    (obj) = NULL; \
    } while (0)

/* FIXME: this breaks on GWY_SWAP(int, a->foo, b->bar);
#define GWY_SWAP(t, x, y) \
    do { \
    t safe ## x ## y; \
    safe ## x ## y = x; \
    x = y; \
    y = safe ## x ## y; \
    } while (0)
*/
/**
 * GWY_SWAP:
 * @t: A C type.
 * @x: A variable of type @t to swap with @x.
 * @y: A variable of type @t to swap with @y.
 *
 * Swaps two variables (more precisely lhs and rhs expressions) of type @t
 * in a single statement.
 */
#define GWY_SWAP(t, x, y) \
    do { \
    t __unsafe_swap; \
    __unsafe_swap = x; \
    x = y; \
    y = __unsafe_swap; \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * gwy_debug:
 * @format...: A format string followed by stuff to print.
 *
 * Prints a debugging message.
 *
 * Does nothing if compiled without DEBUG defined.
 **/
#ifdef G_HAVE_GNUC_VARARGS
#  ifdef DEBUG
#    define gwy_debug(format...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format)
#  else
#    define gwy_debug(format...) /* */
#  endif
#elif defined(G_HAVE_ISO_VARARGS)
#  ifdef DEBUG
#    define gwy_debug(...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, __VA_ARGS__)
#  else
#    define gwy_debug(...) /* */
#  endif
#else /* no varargs macros */
#  ifdef DEBUG
G_INLINE_FUNC void
gwy_debug(const gchar *format, ...)
{
  va_list args;
  va_start(args, format);
  g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
  va_end(args);
}
#  else
G_INLINE_FUNC void
gwy_debug(const gchar *format, ...)
{
}
#  endif
#endif /* varargs macros */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_GWYMACROS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

