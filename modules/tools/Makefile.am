# @(#) $Id$

libdir = $(gwymoduledir)/tool

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

lib_LTLIBRARIES = \
	libcrop.la \
	liblevel3.la \
	libpointer.la \
	libprofile.la

libcrop_la_SOURCES = crop.c
libcrop_la_LDFLAGS = -avoid-version -module
libcrop_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

liblevel3_la_SOURCES = level3.c
liblevel3_la_LDFLAGS = -avoid-version -module
liblevel3_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

libpointer_la_SOURCES = pointer.c
libpointer_la_LDFLAGS = -avoid-version -module
libpointer_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

libprofile_la_SOURCES = profile.c
libprofile_la_LDFLAGS = -avoid-version -module
libprofile_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
