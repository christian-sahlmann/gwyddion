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

#ifndef __GWY_GWYMACROS_H__
#define __GWY_GWYMACROS_H__

#include <glib/gmacros.h>
#include <glib/gstrfuncs.h>
#include <glib/gmem.h>
#include <glib/gutils.h>
#include <glib/gmessages.h>

#define _(x) (x)

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

#ifdef G_HAVE_GNUC_VARARGS
#  ifdef DEBUG
#    define gwy_debug(format...) \
            gwy_debug_gnu(G_LOG_DOMAIN, __FUNCTION__, format)
#  else
#    define gwy_debug(format...) /* */
#  endif
#elif defined(G_HAVE_ISO_VARARGS)
#  ifdef DEBUG
#    define gwy_debug(...) \
            gwy_debug_gnu(G_LOG_DOMAIN, __FILE__, __VA_ARGS__)
#  else
#    define gwy_debug(...) /* */
#  endif
#else /* no varargs macros FIXME: this is broken, though it's like gutils.h */
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

void gwy_debug_gnu(const gchar *domain,
                   const gchar *funcname,
                   const gchar *format,
                   ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef _MSC_VER
/* Make MSVC more pedantic, this is a recommended pragma list
 * from _Win32_Programming_ by Rector and Newcomer.
 */
#pragma warning(error:4002) /* too many actual parameters for macro */
#pragma warning(error:4003) /* not enough actual parameters for macro */
#pragma warning(1:4010)     /* single-line comment contains line-continuation character */
#pragma warning(error:4013) /* 'function' undefined; assuming extern returning int */
#pragma warning(error:4016) /* no function return type; using int as default */
#pragma warning(error:4020) /* too many actual parameters */
#pragma warning(error:4021) /* too few actual parameters */
#pragma warning(error:4027) /* function declared without formal parameter list */
#pragma warning(error:4029) /* declared formal parameter list different from definition */
#pragma warning(error:4033) /* 'function' must return a value */
#pragma warning(error:4035) /* 'function' : no return value */
#pragma warning(error:4045) /* array bounds overflow */
#pragma warning(error:4047) /* different levels of indirection */
#pragma warning(error:4049) /* terminating line number emission */
#pragma warning(error:4053) /* An expression of type void was used as an operand */
#pragma warning(error:4071) /* no function prototype given */

#pragma warning(disable:4244) /* No possible loss of data warnings */
#pragma warning(disable:4305)   /* No truncation from int to char warnings */
#endif /* _MSC_VER */

#endif /* __GWY_GWYMACROS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

