# @(#) $Id$

layerdir = $(pkglibdir)/modules/layer

layer_LTLIBRARIES = \
	axis.la \
	ellipse.la \
	line.la \
	point.la \
	rectangle.la

noinst_HEADERS = \
	layer.h

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt

if OS_WIN32
no_undefined = -no-undefined
endif

AM_CPPFLAGS = -I$(top_srcdir) -DG_LOG_DOMAIN=\"Module\"
AM_CFLAGS = @COMMON_CFLAGS@
AM_LDFLAGS = -avoid-version -module $(no_undefined) $(module_libadd)

if MODULE_DEPENDENCIES
module_libadd = \
	$(top_builddir)/app/libgwyapp2.la \
	$(top_builddir)/libdraw/libgwydraw2.la \
	$(top_builddir)/libgwyddion/libgwyddion2.la \
	$(top_builddir)/libgwydgets/libgwydgets2.la \
	$(top_builddir)/libgwymodule/libgwymodule2.la \
	$(top_builddir)/libprocess/libgwyprocess2.la \
	@GTK_LIBS@
endif

axis_la_SOURCES = axis.c
ellipse_la_SOURCES = ellipse.c
line_la_SOURCES = line.c
rectangle_la_SOURCES = rectangle.c
point_la_SOURCES = point.c

MAINTAINERCLEANFILES = makefile.msc

clean-local:
	rm -f core.* *~
