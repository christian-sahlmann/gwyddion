# @(#) $Id$ */

libdir = $(gwymoduledir)

module_ldflags = -avoid-version -module
module_libs = $(libgwymodule) @GMODULE_LIBS@

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la

lib_LTLIBRARIES = libgwyfile.la

libgwyfile_la_SOURCES = gwyfile.c
libgwyfile_la_LDFLAGS = -avoid-version -module
libgwyfile_la_LIBADD = $(libgwymodule) @GMODULE_LIBS@
#libgwyfile_la_LDFLAGS = $(module_ldflags)
#libgwyfile_la_LIBADD = $(module_libs)

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
