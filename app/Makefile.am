# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la

libgwyappincludedir = $(includedir)/gwyddion/app
libgwyappinclude_HEADERS = \
	app.h \
	file.h \
	gwyapp.h \
	menu.h \
	menu-windowlist.h \
	settings.h \
	undo.h \
	unitool.h \
	wait.h

noinst_HEADERS = \
	gwyddion.h

bin_PROGRAMS = gwyddion
lib_LTLIBRARIES = libgwyapp.la

EXTRA_DIST = \
	makefile.msc \
	libgwyapp.def \
	README.objdata

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwyapp_la_LDFLAGS = -version-info 2:0:2
libgwyapp_la_CFLAGS = -DG_LOG_DOMAIN=\"GwyApp\" $(AM_CPPFLAGS)
libgwyapp_la_SOURCES = \
	app.c \
	file.c \
	menu.c \
	menu-windowlist.c \
	settings.c \
	unitool.c \
	undo.c \
	wait.c

gwyddion_SOURCES = \
	about.c \
	gwyddion.c \
	init.c \
	meta.c \
	splash.c \
	toolbox.c

gwyddion_LDADD = libgwyapp.la \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

INCLUDES = -I$(top_srcdir)
#AM_CPPFLAGS = -DDEBUG
AM_CPPFLAGS =

clean-local:
	rm -f core.* *~
