# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la

libgwyprocessincludedir = $(includedir)/gwyddion/libprocess
libgwyprocessinclude_HEADERS = \
	correct.h \
	correlation.h \
	cwt.h \
	datafield.h \
	dataline.h \
	filters.h \
	fractals.h \
	grains.h \
	gwyprocess.h \
	interpolation.h \
	inttrans.h \
	linestats.h \
	simplefft.h \
	cdline.h

lib_LTLIBRARIES = libgwyprocess.la

EXTRA_DIST = \
	makefile.msc \
	libgwyprocess.def

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwyprocess_la_LDFLAGS = -version-info 3:0:0
libgwyprocess_la_SOURCES = \
	correct.c \
	correlation.c \
	cwt.c \
	datafield.c \
	dataline.c \
	filters.c \
	fractals.c \
	grains.c \
	interpolation.c \
	inttrans.c \
	linestats.c \
	simplefft.c \
	cdline.c

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

