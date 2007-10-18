# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la

libgwydraw2includedir = $(pkgincludedir)/libdraw

if MODULE_DEPENDENCIES
libgwydraw_libadd = \
	$(libgwyprocess) \
	$(libgwyddion)
endif

libgwydraw2include_HEADERS = \
	gwydraw.h \
	gwydrawenums.h \
	gwyglmaterial.h \
	gwygradient.h \
	gwypixfield.h \
	gwyrgba.h \
	gwyselection.h

lib_LTLIBRARIES = libgwydraw2.la

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt \
	libgwydraw2.def

noinst_HEADERS = \
	gwydrawinternal.h

AM_CFLAGS = @COMMON_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir) -DG_LOG_DOMAIN=\"GwyDraw\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwydraw2_la_LDFLAGS = @GTK_LIBS@ @BASIC_LIBS@ -version-info 0:2:0
#libgwydraw2_la_LDFLAGS = @GTK_LIBS@ @BASIC_LIBS@ -release @LIBRARY_RELEASE@
libgwydraw2_la_LIBADD = $(libgwydraw_libadd)

libgwydraw2_la_SOURCES = \
	gwydraw.c \
	gwyglmaterial.c \
	gwygradient.c \
	gwypixfield.c \
	gwyrgba.c \
	gwyselection.c

MAINTAINERCLEANFILES = \
	makefile.msc \
	libgwydraw2.def

clean-local:
	rm -f core.* *~

