# @(#) $Id$

SUBDIRS = tools

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

libgwyappincludedir = $(includedir)/gwyddion/app
libgwyappinclude_HEADERS = \
	app.h \
	file.h \
	menu.h \
	settings.h

noinst_HEADERS = \
	gwyddion.h

bin_PROGRAMS = gwyddion
lib_LTLIBRARIES = libgwyapp.la

EXTRA_DIST = libgwyapp.def

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwyapp_la_LDFLAGS = -version-info 0:0:0
libgwyapp_la_CFLAGS = -DG_LOG_DOMAIN=\"GwyApp\" $(AM_CPPFLAGS)
libgwyapp_la_SOURCES = \
	app.c \
	file.c \
	menu.c \
	settings.c

gwyddion_SOURCES = \
	about.c \
	arith.c \
	gwyddion.c \
	init.c \
	meta.c \
	toolbox.c

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
