/* @(#) $Id$ */

#ifndef __GWY_GWYMACROS_H__
#define __GWY_GWYMACROS_H__

/* FIXME: doesn't behive as a single statement */
#define gwy_object_unref(x) if (x) g_object_unref(x); (x) = NULL

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef G_HAVE_ISO_VARARGS
#define gwy_debug(...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, __VA_ARGS__)
#elif defined(G_HAVE_GNUC_VARARGS)
#define gwy_debug(format...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format)
#else /* no varargs macros */
static void
gwy_debug(const gchar *format,
         ...)
{
  va_list args;
  va_start(args, format);
  g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
  va_end(args);
}
#endif /* varargs macros */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_GWYMACROS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

