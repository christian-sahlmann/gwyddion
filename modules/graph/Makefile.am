# @(#) $Id$

graphdir = $(pkglibdir)/modules/graph

graph_LTLIBRARIES = \
	graph_cd.la \
	graph_cut.la \
	graph_dos_spectrum.la \
	graph_export_ascii.la \
	graph_export_bitmap.la \
	graph_export_vector.la \
	graph_fdfit.la \
	graph_fit.la \
	graph_level.la

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

graph_export_ascii_la_SOURCES = graph_export_ascii.c
graph_export_bitmap_la_SOURCES = graph_export_bitmap.c
graph_export_vector_la_SOURCES = graph_export_vector.c
graph_fdfit_la_SOURCES = graph_fdfit.c
graph_fit_la_SOURCES = graph_fit.c
graph_cd_la_SOURCES = graph_cd.c
graph_cut_la_SOURCES = graph_cut.c
graph_level_la_SOURCES = graph_level.c
graph_dos_spectrum_la_SOURCES = graph_dos_spectrum.c

MAINTAINERCLEANFILES = makefile.msc

clean-local:
	rm -f core.* *~
