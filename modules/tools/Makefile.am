# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

noinst_LTLIBRARIES = libtools.la

noinst_HEADERS = \
	tools.h

libtools_la_LDFLAGS = -static
libtools_la_SOURCES = \
	crop.c \
	level3.c \
	pointer.c

libtools_la_LIBADD = \
	$(libgwyddion) \
	$(libgwymodule) \
	$(libgwyprocess) \
	$(libgwydraw) \
	$(libgwydgets)

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DGWY_MODULE_DIR=\"$(gwymoduledir)\"

clean-local:
	rm -f core.* *~
