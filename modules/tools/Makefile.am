# @(#) $Id$

toolsdir = $(pkglibdir)/modules/tool

tools_LTLIBRARIES = \
	distance.la		\
	crop.la			\
	filter.la		\
	grain_remove_manually.la\
	icolorange.la		\
	level3.la		\
	maskedit.la		\
	polynom.la		\
	profile.la		\
	readvalue.la		\
	sfunctions.la		\
	spotremove.la		\
	stats.la


EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt


INCLUDES = -I$(top_srcdir)

AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\" -fPIC


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


#############################################################################
# If you know what an alphabet is, please keep the alphabetic order.
# If you have no idea, please return to basic school.
#############################################################################


crop_la_SOURCES = crop.c
crop_la_libadd = $(module_libadd)

distance_la_SOURCES = distance.c
distance_la_libadd = $(module_libadd)

filter_la_SOURCES = filter.c
filter_la_libadd = $(module_libadd)

grain_remove_manually_la_SOURCES = grain_remove_manually.c
grain_remove_manually_la_libadd = $(module_libadd)

icolorange_la_SOURCES = icolorange.c
icolorange_la_libadd = $(module_libadd)

level3_la_SOURCES = level3.c
level3_la_libadd = $(module_libadd)

maskedit_la_SOURCES = maskedit.c
maskedit_la_libadd = $(module_libadd)

polynom_la_SOURCES = polynom.c
polynom_la_libadd = $(module_libadd)

profile_la_SOURCES = profile.c
profile_la_libadd = $(module_libadd)

readvalue_la_SOURCES = readvalue.c
readvalue_la_libadd = $(module_libadd)

stats_la_SOURCES = stats.c
stats_la_libadd = $(module_libadd)

sfunctions_la_SOURCES = sfunctions.c
sfunctions_la_libadd = $(module_libadd)

spotremove_la_SOURCES = spotremove.c
spotremove_la_libadd = $(module_libadd)


clean-local:
	rm -f core.* *~
