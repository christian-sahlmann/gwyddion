# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

bin_PROGRAMS = gwyddion

noinst_HEADERS = \
	init.h \
	file.h \
	menu.h \
	app.h

gwyddion_SOURCES = \
	init.c \
	file.c \
	menu.c \
	app.c

gwyddion_LDADD = \
	$(libgwyddion) \
	$(libgwymodule) \
	$(libgwyprocess) \
	$(libgwydraw) \
	$(libgwydgets)

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DGWY_MODULE_DIR=\"$(gwymoduledir)\"

clean-local:
	rm -f core.* *~
