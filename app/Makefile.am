# @(#) $Id$

SUBDIRS = tools

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

appincludedir = $(includedir)/gwyddion/app

bin_PROGRAMS = gwyddion

appinclude_HEADERS = \
	about.h \
	app.h \
	arith.h \
	file.h \
	init.h \
	menu.h \
	meta.h \
	settings.h

gwyddion_SOURCES = \
	about.c \
	app.c \
	arith.c \
	file.c \
	init.c \
	menu.c \
	meta.c \
	settings.c

gwyddion_LDADD = \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DGWY_MODULE_DIR=\"$(gwymoduledir)\"  -DGWY_PLUGIN_DIR=\"$(gwyplugindir)\"

clean-local:
	rm -f core.* *~
