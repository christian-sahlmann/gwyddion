# @(#) $Id$

libdir = $(gwymoduledir)

module_ldflags = -avoid-version -module
module_libs = $(libgwymodule) @GMODULE_LIBS@

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

lib_LTLIBRARIES = \
	libbasicops.la \
	liblevel.la \
	librotate.la

libbasicops_la_SOURCES = basicops.c
libbasicops_la_LDFLAGS = -avoid-version -module
libbasicops_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

liblevel_la_SOURCES = level.c
liblevel_la_LDFLAGS = -avoid-version -module
liblevel_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

librotate_la_SOURCES = rotate.c
librotate_la_LDFLAGS = -avoid-version -module
librotate_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
