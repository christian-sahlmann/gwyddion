# @(#) $Id$

graphdir = $(pkglibdir)/modules/graph

graph_LTLIBRARIES = \
	graph_export_ascii.la \
	graph_export_bitmap.la \
	graph_export_vector.la \
	graph_fit.la	\
	graph_level.la \
	graph_stats.la


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


graph_export_ascii_la_SOURCES = graph_export_ascii.c
graph_export_ascii_la_LIBADD = $(module_libadd)

graph_export_bitmap_la_SOURCES = graph_export_bitmap.c
graph_export_bitmap_la_LIBADD = $(module_libadd)

graph_export_vector_la_SOURCES = graph_export_vector.c
graph_export_vector_la_LIBADD = $(module_libadd)

graph_fit_la_SOURCES = graph_fit.c
graph_fit_la_LIBADD = $(module_libadd)

#graph_cd_la_SOURCES = graph_cd.c
#graph_cd_la_LIBADD = $(module_libadd)

graph_level_la_SOURCES = graph_level.c
graph_level_la_LIBADD = $(module_libadd)

graph_stats_la_SOURCES = graph_stats.c
graph_stats_la_LIBADD = $(module_libadd)


clean-local:
	rm -f core.* *~
