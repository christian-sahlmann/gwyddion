# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la

libgwyprocessincludedir = $(pkgincludedir)/libprocess

libgwyprocessinclude_HEADERS = \
	arithmetic.h \
	cdline.h \
	correct.h \
	correlation.h \
	cwt.h \
	datafield.h \
	dataline.h \
	dwt.h \
	filters.h \
	fractals.h \
	grains.h \
	gwyprocess.h \
	interpolation.h \
	inttrans.h \
	level.h \
	linestats.h \
	simplefft.h \
	stats.h \
	tip.h

noinst_HEADERS = morph_lib.h

lib_LTLIBRARIES = libgwyprocess.la

EXTRA_DIST = \
	makefile.msc \
	libgwyprocess.def

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwyprocess_la_LDFLAGS = -version-info 8:0:5
libgwyprocess_la_SOURCES = \
	arithmetic.c \
	cdline.c \
	correct.c \
	correlation.c \
	cwt.c \
	datafield.c \
	dataline.c \
	dwt.c \
	filters.c \
	fractals.c \
	grains.c \
	gwyprocess.c \
	interpolation.c \
	inttrans.c \
	level.c \
	linestats.c \
	morph_lib.c \
	simplefft.c \
	stats.c \
	tip.c

#noinst_PROGRAMS = test
#
#test_SOURCES = \
#	test.c
#test_LDADD = libgwyprocess.la \
#	$(libgwyddion)
#
INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"GwyProcess\"

clean-local:
	rm -f core.* *~

