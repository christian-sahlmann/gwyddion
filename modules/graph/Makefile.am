# @(#) $Id$

libdir = $(gwymoduledir)/graph

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

lib_LTLIBRARIES = \
	libread.la

libread_la_SOURCES = read.c
libread_la_LDFLAGS = -avoid-version -module
libread_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@


INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
