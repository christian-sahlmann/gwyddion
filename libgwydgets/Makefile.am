# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la

libgwydgetsincludedir = $(pkgincludedir)/libgwydgets

libgwydgetsinclude_HEADERS = \
	gwy3dlabel.h \
	gwy3dview.h \
	gwy3dwindow.h \
	gwyaxis.h \
	gwyaxiser.h \
	gwyaxisdialog.h \
	gwycoloraxis.h \
	gwycolorbutton.h \
	gwydataview.h \
	gwydataviewlayer.h \
	gwydatawindow.h \
	gwydgets.h \
	gwydgetenums.h \
	gwydgettypes.h \
	gwydgetutils.h \
	gwyglmaterial.h \
	gwygraph.h \
	gwygrapharea.h \
	gwygraphcorner.h \
	gwygraphcurvemodel.h \
	gwygraphmodel.h \
	gwygraphlabel.h \
	gwygrapherareadialog.h \
	gwygrapherarea.h \
	gwygrapherbasics.h \
	gwygraphercorner.h \
	gwygrapher.h \
	gwygrapherlabeldialog.h \
	gwygrapherlabel.h \
	gwyhruler.h \
	gwylayer-basic.h \
	gwylayer-mask.h \
	gwyoptionmenus.h \
	gwypixmaplayer.h \
	gwyradiobuttons.h \
	gwyruler.h \
	gwyscitext.h \
	gwyshader.h \
	gwystatusbar.h \
	gwystock.h \
	gwyvectorlayer.h \
	gwyvruler.h \
	gwyvalunit.h

lib_LTLIBRARIES = libgwydgets.la

built_sources = \
	gwydgettypes.h \
	gwydgettypes.c

EXTRA_DIST = \
	$(built_sources) \
	gwydgettypes.c.template \
	gwydgettypes.h.template \
	makefile.msc \
	libgwydgets.def

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
#libgwydgets_la_LDFLAGS = @GTKGLEXT_LIBS@ -version-info 9:0:6
libgwydgets_la_LDFLAGS = @GTKGLEXT_LIBS@ -release @LIBRARY_RELEASE@
libgwydgets_la_SOURCES = \
	gwy3dlabel.c \
	gwy3dview.c \
	gwy3dwindow.c \
	gwyaxis.c \
	gwyaxiser.c \
	gwyaxisdialog.c \
	gwycoloraxis.c \
	gwycolorbutton.c \
	gwydataview.c \
	gwydataviewlayer.c \
	gwydatawindow.c \
	gwydgets.c \
	gwydgettypes.c \
	gwydgetutils.c \
	gwyglmaterial.c \
	gwygraph.c \
	gwygrapharea.c \
	gwygraphcurvemodel.c \
	gwygraphmodel.c \
	gwygraphexport.c \
	gwygraphcorner.c \
	gwygraphlabel.c \
	gwygrapherareadialog.c \
	gwygrapherarea.c \
	gwygrapherbasics.c \
	gwygraphercorner.c \
	gwygrapher.c \
	gwygrapherlabeldialog.c \
	gwygrapherlabel.c \
	gwyhruler.c \
	gwylayer-basic.c \
	gwylayer-mask.c \
	gwyoptionmenus.c \
	gwypixmaplayer.c \
	gwyradiobuttons.c \
	gwyruler.c \
	gwyscitext.c \
	gwyshader.c \
	gwystatusbar.c \
	gwystock.c \
	gwyvectorlayer.c \
	gwyvruler.c \
	gwyvalunit.c

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Gwydgets\"

clean-local:
	rm -f core.* *~ *.xgen

if MAINTAINER_MODE
enum_headers = gwydgetenums.h
stamp_files = stamp-gwyprocessenums.h

MAINTAINERCLEANFILES = $(built_sources) $(stamp_files)

BUILT_SOURCES = $(built_sources)

gwydgettypes.h: stamp-gwydgettypes.h
	@true

stamp-gwydgettypes.h: $(enum_headers) gwydgettypes.h.template
	( cd $(srcdir) \
	  && glib-mkenums --template gwydgettypes.h.template $(enum_headers) ) \
	| sed -e 's/_3_D/_3D_/g' >gwydgettypes.h.xgen \
	&& (cmp -s gwydgettypes.h.xgen gwydgettypes.h \
		|| cp gwydgettypes.h.xgen gwydgettypes.h ) \
	&& rm -f gwydgettypes.h.xgen \
	&& echo timestamp >stamp-gwydgettypes.h

gwydgettypes.c: $(enum_headers) gwydgettypes.c.template
	( cd $(srcdir) \
	  && glib-mkenums --template gwydgettypes.c.template $(enum_headers) ) \
	>gwydgettypes.c.xgen \
	&& cp gwydgettypes.c.xgen gwydgettypes.c  \
	&& rm -f gwydgettypes.c.xgen
endif

# Note these are test programs not to be included in the distro
noinst_PROGRAMS = test
#
#main_SOURCES = main.c
#main_LDADD = libgwydgets.la \
#	$(libgwydraw) \
#	$(libgwyprocess) \
#	$(libgwyddion)
#
test_SOURCES = test.c
test_LDADD = libgwydgets.la \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)
#
#create_data_SOURCES = create-data.c
#create_data_LDADD = \
#	$(libgwyprocess) \
#	$(libgwyddion)

