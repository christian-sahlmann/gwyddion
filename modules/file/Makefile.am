# @(#) $Id$ */

libdir = $(gwymoduledir)/file

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

lib_LTLIBRARIES = \
	libgwyfile.la \
	libpixmap.la \
	libplugin-proxy.la

libgwyfile_la_SOURCES = gwyfile.c
libgwyfile_la_LDFLAGS = -avoid-version -module
libgwyfile_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

libpixmap_la_SOURCES = pixmap.c
libpixmap_la_LDFLAGS = -avoid-version -module
libpixmap_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

libplugin_proxy_la_SOURCES = plugin-proxy.c
libplugin_proxy_la_LDFLAGS = -avoid-version -module
libplugin_proxy_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
