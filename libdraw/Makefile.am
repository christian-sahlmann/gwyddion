# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la

libgwydrawincludedir = $(pkgincludedir)/libdraw

libgwydrawinclude_HEADERS = \
	gwydraw.h \
	gwydrawenums.h \
	gwyglmaterial.h \
	gwygradient.h \
	gwypixfield.h \
	gwyrgba.h \
	gwyselection.h

lib_LTLIBRARIES = libgwydraw.la

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt \
	libgwydraw.def

noinst_HEADERS = \
	gwydrawinternal.h

#noinst_PROGRAMS = test

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
#libgwydraw_la_LDFLAGS = @GTK_LIBS@ @BASIC_LIBS@ -version-info 4:1:3
libgwydraw_la_LDFLAGS = @GTK_LIBS@ @BASIC_LIBS@ -release @LIBRARY_RELEASE@
libgwydraw_la_SOURCES = \
	gwydraw.c \
	gwyglmaterial.c \
	gwygradient.c \
	gwypixfield.c \
	gwyrgba.c \
	gwyselection.c

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"GwyDraw\"

clean-local:
	rm -f core.* *~

