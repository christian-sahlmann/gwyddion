# @(#) $Id$

SUBDIRS = tools

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

libgwyappincludedir = $(includedir)/gwyddion/app
libgwyappinclude_HEADERS = \
	about.h \
	app.h \
	arith.h \
	file.h \
	init.h \
	menu.h \
	meta.h \
	settings.h

bin_PROGRAMS = gwyddion
lib_LTLIBRARIES = libgwyapp.la

EXTRA_DIST = libgwyapp.def

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwyapp_la_LDFLAGS = -version-info 0:0:0
libgwyapp_la_SOURCES = \
	about.c \
	app.c \
	arith.c \
	file.c \
	init.c \
	menu.c \
	meta.c \
	settings.c

gwyddion_SOURCES = \
	gwyddion.c
gwyddion_LDADD = libgwyapp.la \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DGWY_MODULE_DIR=\"$(gwymoduledir)\"  -DGWY_PLUGIN_DIR=\"$(gwyplugindir)\"

clean-local:
	rm -f core.* *~
