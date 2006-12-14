# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la

libgwyprocess2includedir = $(pkgincludedir)/libprocess

libgwyprocess2include_HEADERS = \
	arithmetic.h \
	cdline.h \
	correct.h \
	correlation.h \
	cwt.h \
	datafield.h \
	dataline.h \
	dwt.h \
	elliptic.h \
	filters.h \
	fractals.h \
	grains.h \
	gwyprocess.h \
	gwyprocessenums.h \
	gwyprocesstypes.h \
	hough.h \
	interpolation.h \
	inttrans.h \
	level.h \
	linestats.h \
	simplefft.h \
	stats.h \
	tip.h

noinst_HEADERS = \
	morph_lib.h \
	gwyprocessinternal.h

lib_LTLIBRARIES = libgwyprocess2.la

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt \
	libgwyprocess2.def

# Must init
CLEANFILES =
MAINTAINERCLEANFILES = \
	makefile.msc \
	libgwyprocess2.def
BUILT_SOURCES =

MKENUM_NAME = gwyprocesstypes
MKENUM_HFILES = gwyprocessenums.h
include $(top_srcdir)/utils/mkenum.mk

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwyprocess2_la_LDFLAGS = @BASIC_LIBS@ @FFTW3_LIBS@ -version-info 1:0:1
#libgwyprocess2_la_LDFLAGS = @BASIC_LIBS@ @FFTW3_LIBS@ -release @LIBRARY_RELEASE@
libgwyprocess2_la_SOURCES = \
	arithmetic.c \
	cdline.c \
	correct.c \
	correlation.c \
	cwt.c \
	datafield.c \
	dataline.c \
	dwt.c \
	elliptic.c \
	filters.c \
	fractals.c \
	grains.c \
	gwyprocess.c \
	gwyprocessenums.c \
	gwyprocesstypes.c \
	hough.c \
	interpolation.c \
	inttrans.c \
	level.c \
	linestats.c \
	morph_lib.c \
	simplefft.c \
	stats.c \
	tip.c

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"GwyProcess\"

clean-local:
	rm -f core.* *~

