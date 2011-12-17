# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw2.la

libgwydgets2includedir = $(pkgincludedir)/libgwydgets

if MODULE_DEPENDENCIES
libgwydgets_libadd = \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)
endif

libgwydgets2include_HEADERS = \
	gwy3dlabel.h \
	gwy3dsetup.h \
	gwy3dview.h \
	gwy3dwindow.h \
	gwyaxis.h \
	gwycoloraxis.h \
	gwycolorbutton.h \
	gwycombobox.h \
	gwycurve.h \
	gwydataview.h \
	gwydataviewlayer.h \
	gwydatawindow.h \
	gwydgetenums.h \
	gwydgets.h \
	gwydgettypes.h \
	gwydgetutils.h \
	gwygrainvaluemenu.h \
	gwygraph.h \
	gwygrapharea.h \
	gwygraphbasics.h \
	gwygraphcorner.h \
	gwygraphcurvemodel.h \
	gwygraphcurves.h \
	gwygraphdata.h \
	gwygraphlabel.h \
	gwygraphmodel.h \
	gwygraphselections.h \
	gwygraphwindow.h \
	gwyhmarkerbox.h \
	gwyhruler.h \
	gwyinventorystore.h \
	gwylayer-basic.h \
	gwylayer-mask.h \
	gwymarkerbox.h \
	gwynullstore.h \
	gwyoptionmenus.h \
	gwypixmaplayer.h \
	gwyradiobuttons.h \
	gwyruler.h \
	gwyscitext.h \
	gwyselectiongraph1darea.h \
	gwyselectiongrapharea.h \
	gwyselectiongraphline.h \
	gwyselectiongraphpoint.h \
	gwyselectiongraphzoom.h \
	gwysensitivitygroup.h \
	gwyshader.h \
	gwystatusbar.h \
	gwystock.h \
	gwyvectorlayer.h \
	gwyvruler.h

noinst_HEADERS = \
	gwyaxisdialog.h \
	gwydgetmarshals.h \
	gwygraphwindowmeasuredialog.h \
	gwygraphareadialog.h \
	gwygraphlabeldialog.h

lib_LTLIBRARIES = libgwydgets2.la

if OS_WIN32
no_undefined = -no-undefined
export_symbols = -export-symbols libgwydgets2.def
libgwydgets2_la_DEPENDENCIES = libgwydgets2.def
endif

# Must init
EXTRA_DIST =
CLEANFILES = libgwydgets2.def
MAINTAINERCLEANFILES =
BUILT_SOURCES =

MKENUM_NAME = gwydgettypes
MKENUM_ID = GWYDGET_TYPES
MKENUM_HFILES = \
		$(srcdir)/gwydgetenums.h \
		$(srcdir)/gwygrainvaluemenu.h
include $(top_srcdir)/utils/mkenum.mk

GENMARSHAL_NAME = gwydgetmarshals
GENMARSHAL_PREFIX = _gwydget_marshal
include $(top_srcdir)/utils/genmarshal.mk

libgwydgets2.def: $(libgwydgets2_la_OBJECTS)
	@echo EXPORTS >libgwydgets2.def
	@for lo in $(libgwydgets2_la_OBJECTS); do \
		o=`sed "s/^pic_object='\\(.*\\)'/\\1/;t;d" "$$lo"`; \
		$(NM) --defined-only --demangle --no-sort "$$o" \
		| awk -F' ' '($$2=="T" && $$3 ~ /^gwy_/){print "     ",$$3}'; \
	done | sort >>libgwydgets2.def

AM_CFLAGS = @COMMON_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir) -DG_LOG_DOMAIN=\"Gwydgets\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libversion = -version-info 11:0:11
#libversion = -release @LIBRARY_RELEASE@
libgwydgets2_la_LDFLAGS = @GTKGLEXT_LIBS@ @BASIC_LIBS@ -export-dynamic $(no_undefined) $(export_symbols) $(libversion)
libgwydgets2_la_LIBADD = $(libgwydgets_libadd)

libgwydgets2_la_SOURCES = \
	gwy3dlabel.c \
	gwy3dsetup.c \
	gwy3dview.c \
	gwy3dwindow.c \
	gwyaxis.c \
	gwyaxisdialog.c \
	gwycoloraxis.c \
	gwycolorbutton.c \
	gwycombobox.c \
	gwycurve.c \
	gwydataview.c \
	gwydataviewlayer.c \
	gwydatawindow.c \
	gwydgetenums.c \
	gwydgetmarshals.c \
	gwydgets.c \
	gwydgettypes.c \
	gwydgetutils.c \
	gwygrainvaluemenu.c \
	gwygraph.c \
	gwygrapharea.c \
	gwygraphareadialog.c \
	gwygraphbasics.c \
	gwygraphcorner.c \
	gwygraphcurvemodel.c \
	gwygraphcurves.c \
	gwygraphdata.c \
	gwygraphexport.c \
	gwygraphlabel.c \
	gwygraphlabeldialog.c \
	gwygraphmodel.c \
	gwygraphwindow.c \
	gwygraphwindowmeasuredialog.c \
	gwyhmarkerbox.c \
	gwyhruler.c \
	gwyinventorystore.c \
	gwylayer-basic.c \
	gwylayer-mask.c \
	gwymarkerbox.c \
	gwynullstore.c \
	gwyoptionmenus.c \
	gwypixmaplayer.c \
	gwyradiobuttons.c \
	gwyruler.c \
	gwyscitext.c \
	gwyselectiongraph1darea.c \
	gwyselectiongrapharea.c \
	gwyselectiongraphline.c \
	gwyselectiongraphpoint.c \
	gwyselectiongraphzoom.c \
	gwysensitivitygroup.c \
	gwyshader.c \
	gwystatusbar.c \
	gwystock.c \
	gwyvectorlayer.c \
	gwyvruler.c

clean-local:
	rm -f core.* *~

