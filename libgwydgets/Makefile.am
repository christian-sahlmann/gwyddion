# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la

libgwydgetsincludedir = $(includedir)/gwyddion/libgwydgets
libgwydgetsinclude_HEADERS = \
	gwy3dview.h \
	gwy3dwindow.h \
	gwyaxis.h \
	gwyaxisdialog.h \
	gwycoloraxis.h \
	gwycolorbutton.h \
	gwydataview.h \
	gwydataviewlayer.h \
	gwydatawindow.h \
	gwydgets.h \
	gwyglmaterial.h \
	gwygradsphere.h \
	gwygraph.h \
	gwygrapharea.h \
	gwygraphcorner.h \
	gwygraphcurvemodel.h \
	gwygraphmodel.h \
	gwygraphlabel.h \
	gwyhruler.h \
	gwylayer-basic.h \
	gwylayer-mask.h \
	gwyoptionmenus.h \
	gwypixmaplayer.h \
	gwyradiobuttons.h \
	gwyruler.h \
	gwyscitext.h \
	gwyspherecoords.h \
	gwystatusbar.h \
	gwystock.h \
	gwytoolbox.h \
	gwyvectorlayer.h \
	gwyvectorshade.h \
	gwyvmenubar.h \
	gwyvruler.h \
	gwyvalunit.h

lib_LTLIBRARIES = libgwydgets.la

EXTRA_DIST = \
	makefile.msc \
	libgwydgets.def

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwydgets_la_LDFLAGS = @GTKGLEXT_LIBS@ -version-info 4:0:1
libgwydgets_la_SOURCES = \
	gwy3dview.c \
	gwy3dwindow.c \
	gwyaxis.c \
	gwyaxisdialog.c \
	gwycoloraxis.c \
	gwycolorbutton.c \
	gwydataview.c \
	gwydataviewlayer.c \
	gwydatawindow.c \
	gwydgets.c \
	gwyglmaterial.c \
	gwygradsphere.c \
	gwygraph.c \
	gwygrapharea.c \
	gwygraphcurvemodel.c \
	gwygraphmodel.c \
	gwygraphexport.c \
	gwygraphcorner.c \
	gwygraphlabel.c \
	gwyhruler.c \
	gwylayer-basic.c \
	gwylayer-mask.c \
	gwyoptionmenus.c \
	gwypixmaplayer.c \
	gwyradiobuttons.c \
	gwyruler.c \
	gwyscitext.c \
	gwyspherecoords.c \
	gwystatusbar.c \
	gwystock.c \
	gwytoolbox.c \
	gwyvectorlayer.c \
	gwyvectorshade.c \
	gwyvmenubar.c \
	gwyvruler.c \
	gwyvalunit.c

# Note these are test programs not to be included in the distro
#noinst_PROGRAMS = main test create-data
#
#main_SOURCES = main.c
#main_LDADD = libgwydgets.la \
#	$(libgwydraw) \
#	$(libgwyprocess) \
#	$(libgwyddion)
#
#test_SOURCES = test.c
#test_LDADD = libgwydgets.la \
#	$(libgwydraw) \
#	$(libgwyprocess) \
#	$(libgwyddion)
#
#wtest_SOURCES = wtest.c
#wtest_LDADD = libgwydgets.la \
#	$(libgwydraw) \
#	$(libgwyprocess) \
#	$(libgwyddion)
#
#create_data_SOURCES = create-data.c
#create_data_LDADD = \
#	$(libgwyprocess) \
#	$(libgwyddion)

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Gwydgets\"

clean-local:
	rm -f core.* *~
