# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la

libgwydrawincludedir = $(pkgincludedir)/libdraw

libgwydrawinclude_HEADERS = \
	gwydraw.h \
	gwypalette.h \
	gwypalettedef.h \
	gwypixfield.h \
	gwyrgba.h

lib_LTLIBRARIES = libgwydraw.la

EXTRA_DIST = \
	makefile.msc \
	libgwydraw.def

#noinst_PROGRAMS = test

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwydraw_la_LDFLAGS = -version-info 2:0:1
libgwydraw_la_SOURCES = \
	gwydraw.c \
	gwypalette.c \
	gwypalettedef.c \
	gwypixfield.c \
	gwyrgba.c

#test_SOURCES = \
#	test.c
#test_LDADD = libgwydraw.la \
#	$(libgwyprocess) \
#	$(libgwyddion)

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"GwyDraw\"

clean-local:
	rm -f core.* *~

