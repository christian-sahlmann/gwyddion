# @(#) $Id$

libdir = $(gwymoduledir)/process

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

lib_LTLIBRARIES = \
	libbasicops.la \
	libcwt.la \
	libfft.la \
	liblevel.la \
	libplugin-proxy.la \
	librotate.la \
	libscale.la \
	libshade.la

libbasicops_la_SOURCES = basicops.c
libbasicops_la_LDFLAGS = -avoid-version -module
libbasicops_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

libcwt_la_SOURCES = cwt.c
libcwt_la_LDFLAGS = -avoid-version -module
libcwt_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

libfft_la_SOURCES = fft.c
libfft_la_LDFLAGS = -avoid-version -module
libfft_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

liblevel_la_SOURCES = level.c
liblevel_la_LDFLAGS = -avoid-version -module
liblevel_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

libplugin_proxy_la_SOURCES = plugin-proxy.c
libplugin_proxy_la_LDFLAGS = -avoid-version -module
libplugin_proxy_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

librotate_la_SOURCES = rotate.c
librotate_la_LDFLAGS = -avoid-version -module
librotate_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

libscale_la_SOURCES = scale.c
libscale_la_LDFLAGS = -avoid-version -module
libscale_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

libshade_la_SOURCES = shade.c
libshade_la_LDFLAGS = -avoid-version -module
libshade_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@


INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
