# @(#) $Id$

layerdir = $(pkglibdir)/modules/layer

layer_LTLIBRARIES = \
	axis.la \
	rectangle.la


noinst_HEADERS = \
	layer.h

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt


INCLUDES = -I$(top_srcdir)

AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\"


if OS_WIN32
no_undefined = -no-undefined
endif

AM_LDFLAGS = -avoid-version -module $(no_undefined)

if MODULE_DEPENDENCIES
module_libadd = \
	$(top_builddir)/app/libgwyapp.la		\
	$(top_builddir)/libdraw/libgwydraw.la		\
	$(top_builddir)/libgwyddion/libgwyddion.la	\
	$(top_builddir)/libgwydgets/libgwydgets.la	\
	$(top_builddir)/libgwymodule/libgwymodule.la	\
	$(top_builddir)/libprocess/libgwyprocess.la	\
	@GTK_LIBS@
endif


axis_la_SOURCES = axis.c
axis_la_LIBADD = $(module_libadd)

#lines_la_SOURCES = lines.c
#lines_la_LIBADD = $(module_libadd)

rectangle_la_SOURCES = rectangle.c
rectangle_la_LIBADD = $(module_libadd)

#pointer_la_SOURCES = pointer.c
#pointer_la_LIBADD = $(module_libadd)
#
#points_la_SOURCES = points.c
#points_la_LIBADD = $(module_libadd)
#
#select_la_SOURCES = select.c
#select_la_LIBADD = $(module_libadd)


clean-local:
	rm -f core.* *~
