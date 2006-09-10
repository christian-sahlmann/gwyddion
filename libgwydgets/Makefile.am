# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw2.la

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
	gwydataview.h \
	gwydataviewlayer.h \
	gwydatawindow.h \
	gwydgets.h \
	gwydgetenums.h \
	gwydgettypes.h \
	gwydgetutils.h \
	gwygraphcurvemodel.h \
	gwygraphdata.h \
	gwygraphmodel.h \
	gwygraphwindow.h \
	gwygrapharea.h \
	gwygraphbasics.h \
	gwygraphcorner.h \
	gwygraph.h \
	gwygraphlabel.h \
	gwygraphselections.h \
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
	gwysensitivitygroup.h \
	gwyshader.h \
	gwystatusbar.h \
	gwystock.h \
	gwyvectorlayer.h \
	gwyvruler.h \
	gwycurve.h

noinst_HEADERS = \
	gwyaxisdialog.h \
	gwygraphwindowmeasuredialog.h \
	gwygraphareadialog.h \
	gwygraphlabeldialog.h

lib_LTLIBRARIES = libgwydgets2.la

MKENUM_NAME = gwydgettypes
MKENUM_HFILES = gwydgetenums.h
include $(top_srcdir)/utils/mkenum.mk

EXTRA_DIST += \
	makefile.msc \
	makefile.msc.gwt \
	libgwydgets2.def

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwydgets2_la_LDFLAGS = @GTKGLEXT_LIBS@ @BASIC_LIBS@ -version-info 0:0:0
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
	gwydataview.c \
	gwydataviewlayer.c \
	gwydatawindow.c \
	gwydgets.c \
	gwydgetenums.c \
	gwydgettypes.c \
	gwydgetutils.c \
	gwygraphcurvemodel.c \
	gwygraphdata.c \
	gwygraphmodel.c \
	gwygraphwindow.c \
	gwygraphwindowmeasuredialog.c \
	gwygraphareadialog.c \
	gwygrapharea.c \
	gwygraphbasics.c \
	gwygraphcorner.c \
	gwygraph.c \
	gwygraphexport.c \
	gwygraphlabeldialog.c \
	gwygraphlabel.c \
	gwygraphselections.c \
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
	gwysensitivitygroup.c \
	gwyshader.c \
	gwystatusbar.c \
	gwystock.c \
	gwyvectorlayer.c \
	gwyvruler.c \
	gwycurve.c

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Gwydgets\"

clean-local:
	rm -f core.* *~

