# @(#) $Id$

libgwydgets2includedir = $(pkgincludedir)/libgwydgets

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

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt \
	libgwydgets2.def

# Must init
CLEANFILES =
MAINTAINERCLEANFILES = \
	makefile.msc \
	libgwydgets2.def
BUILT_SOURCES =

MKENUM_NAME = gwydgettypes
MKENUM_HFILES = gwydgetenums.h
include $(top_srcdir)/utils/mkenum.mk

GENMARSHAL_NAME = gwydgetmarshals
GENMARSHAL_PREFIX = _gwydget_marshal
include $(top_srcdir)/utils/genmarshal.mk

AM_CFLAGS = @COMMON_CFLAGS@
AM_CPPFLAGS = -I$(top_builddir) -DG_LOG_DOMAIN=\"Gwydgets\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
# XXX: 2.5J has 3:1:3
libgwydgets2_la_LDFLAGS = @GTKGLEXT_LIBS@ @BASIC_LIBS@ -version-info 3:0:3
#libgwydgets2_la_LDFLAGS = @GTKGLEXT_LIBS@ @BASIC_LIBS@ -release @LIBRARY_RELEASE@
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

