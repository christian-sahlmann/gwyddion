# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la

libgwyprocess2includedir = $(pkgincludedir)/libprocess

if MODULE_DEPENDENCIES
libgwyprocess_libadd = \
	$(libgwyddion)
endif

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
	gwygrainvalue.h \
	gwyprocess.h \
	gwyprocessenums.h \
	gwyprocesstypes.h \
	hough.h \
	interpolation.h \
	inttrans.h \
	level.h \
	linestats.h \
	simplefft.h \
	spectra.h \
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
MKENUM_ID = GWY_PROCESS_TYPES
MKENUM_HFILES = \
	$(srcdir)/gwyprocessenums.h \
	$(srcdir)/gwygrainvalue.h
include $(top_srcdir)/utils/mkenum.mk

AM_CFLAGS = @COMMON_CFLAGS@ @FFTW3_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir) -DG_LOG_DOMAIN=\"GwyProcess\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwyprocess2_la_LDFLAGS = @BASIC_LIBS@ @FFTW3_LIBS@ -version-info 8:2:8
#libgwyprocess2_la_LDFLAGS = @BASIC_LIBS@ @FFTW3_LIBS@ -release @LIBRARY_RELEASE@
libgwyprocess2_la_LIBADD = $(libgwyprocess_libadd)

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
	gwygrainvalue.c \
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
	spectra.c \
	stats.c \
	tip.c

clean-local:
	rm -f core.* *~

