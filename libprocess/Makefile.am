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
	gwyprocessenums.h \
	gwyprocesstypes.h \
	interpolation.h \
	inttrans.h \
	level.h \
	linestats.h \
	simplefft.h \
	stats.h \
	tip.h

noinst_HEADERS = morph_lib.h

built_sources = \
	gwyprocesstypes.h \
	gwyprocesstypes.c

lib_LTLIBRARIES = libgwyprocess.la

EXTRA_DIST = \
	$(built_sources) \
	gwyprocesstypes.c.template \
	gwyprocesstypes.h.template \
	makefile.msc \
	libgwyprocess.def

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
#libgwyprocess_la_LDFLAGS = @BASIC_LIBS@ @FFTW3_LIBS@ -version-info 8:0:5
libgwyprocess_la_LDFLAGS = @BASIC_LIBS@ @FFTW3_LIBS@ -release @LIBRARY_RELEASE@
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
	gwyprocesstypes.c \
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
	rm -f core.* *~ *.xgen

if MAINTAINER_MODE
enum_headers = gwyprocessenums.h
stamp_files = stamp-gwyprocessenums.h

MAINTAINERCLEANFILES = $(built_sources) $(stamp_files)

BUILT_SOURCES = $(built_sources)

gwyprocesstypes.h: stamp-gwyprocesstypes.h
	@true

stamp-gwyprocesstypes.h: $(enum_headers) gwyprocesstypes.h.template
	( cd $(srcdir) \
	  && glib-mkenums --template gwyprocesstypes.h.template $(enum_headers) ) \
	| sed -e 's/_2_D/_2D/g' >gwyprocesstypes.h.xgen \
	&& (cmp -s gwyprocesstypes.h.xgen gwyprocesstypes.h \
		|| cp gwyprocesstypes.h.xgen gwyprocesstypes.h ) \
	&& rm -f gwyprocesstypes.h.xgen \
	&& echo timestamp >stamp-gwyprocesstypes.h

gwyprocesstypes.c: $(enum_headers) gwyprocesstypes.c.template
	( cd $(srcdir) \
	  && glib-mkenums --template gwyprocesstypes.c.template $(enum_headers) ) \
	>gwyprocesstypes.c.xgen \
	&& cp gwyprocesstypes.c.xgen gwyprocesstypes.c  \
	&& rm -f gwyprocesstypes.c.xgen
endif

#noinst_PROGRAMS = test
#
#test_SOURCES = \
#	test.c
#test_LDADD = libgwyprocess.la \
#	$(libgwyddion)

