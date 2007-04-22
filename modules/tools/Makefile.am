# @(#) $Id$

toolsdir = $(pkglibdir)/modules/tool

tools_LTLIBRARIES = \
	crop.la \
	distance.la \
	filter.la \
	grainmeasure.la \
	grainremover.la \
	icolorange.la \
	level3.la \
	linestats.la \
	maskedit.la \
	pathlevel.la \
	polynom.la \
	profile.la \
	readvalue.la \
	roughness.la \
	sfunctions.la \
	spotremove.la \
	spectro.la \
	stats.la

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt

if OS_WIN32
no_undefined = -no-undefined
endif

AM_CPPFLAGS = -I$(top_buildir) -DG_LOG_DOMAIN=\"Module\"
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

crop_la_SOURCES         = crop.c
distance_la_SOURCES     = distance.c
filter_la_SOURCES       = filter.c
grainmeasure_la_SOURCES = grainmeasure.c
grainremover_la_SOURCES = grainremover.c
icolorange_la_SOURCES   = icolorange.c
level3_la_SOURCES       = level3.c
linestats_la_SOURCES    = linestats.c
maskedit_la_SOURCES     = maskedit.c
pathlevel_la_SOURCES    = pathlevel.c
polynom_la_SOURCES      = polynom.c
profile_la_SOURCES      = profile.c
readvalue_la_SOURCES    = readvalue.c
roughness_la_SOURCES    = roughness.c
stats_la_SOURCES        = stats.c
sfunctions_la_SOURCES   = sfunctions.c
spectro_la_SOURCES      = spectro.c
spotremove_la_SOURCES   = spotremove.c

MAINTAINERCLEANFILES = makefile.msc

clean-local:
	rm -f core.* *~
