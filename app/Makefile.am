# @(#) $Id$

SUBDIRS = tools

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la
libtools = tools/libtools.la

bin_PROGRAMS = gwyddion

noinst_HEADERS = \
	init.h \
	file.h \
	menu.h \
	settings.h \
	stock.h \
	app.h

gwyddion_SOURCES = \
	init.c \
	file.c \
	menu.c \
	settings.c \
	stock.c \
	app.c

gwyddion_LDADD = \
	$(libgwyddion) \
	$(libgwymodule) \
	$(libgwyprocess) \
	$(libgwydraw) \
	$(libgwydgets) \
	$(libtools)

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DGWY_MODULE_DIR=\"$(gwymoduledir)\" -DGWY_PIXMAP_DIR=\"$(gwypixmapdir)\"

clean-local:
	rm -f core.* *~
