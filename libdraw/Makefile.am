# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la

libgwydrawincludedir = $(includedir)/gwyddion/libdraw
libgwydrawinclude_HEADERS = \
	gwypalette.h \
	gwypalettedef.h \
	gwypixfield.h

lib_LTLIBRARIES = libgwydraw.la

EXTRA_DIST = libgwydraw.def

noinst_PROGRAMS = test

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwydraw_la_LDFLAGS = -version-info 0:0:0
libgwydraw_la_SOURCES = \
	gwypalette.c \
	gwypalettedef.c \
	gwypixfield.c

test_SOURCES = \
	test.c
test_LDADD = libgwydraw.la \
	$(libgwyprocess) \
	$(libgwyddion)

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"GwyDraw\"

clean-local:
	rm -f core.* *~

