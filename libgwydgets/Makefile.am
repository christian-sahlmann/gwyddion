# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la

libgwydgetsincludedir = $(includedir)/gwyddion/libgwydgets
libgwydgetsinclude_HEADERS = \
	gwyaxis.h \
	gwydataview.h \
	gwydataviewlayer.h \
	gwydatawindow.h \
	gwydgets.h \
	gwygradsphere.h \
	gwygraph.h \
	gwygrapharea.h \
	gwygraphcorner.h \
	gwygraphlabel.h \
	gwyhruler.h \
	gwylayer-basic.h \
	gwylayer-mask.h \
	gwylayer-lines.h \
	gwylayer-points.h \
	gwylayer-select.h \
	gwyruler.h \
	gwyspherecoords.h \
	gwyvectorshade.h \
	gwyvruler.h

lib_LTLIBRARIES = libgwydgets.la

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwydgets_la_LDFLAGS = -version-info 0:0:0
libgwydgets_la_SOURCES = \
	gwyaxis.c \
	gwydataview.c \
	gwydataviewlayer.c \
	gwydatawindow.c \
	gwydgets.c \
	gwygradsphere.c \
	gwygraph.c \
	gwygrapharea.c \
	gwygraphcorner.c \
	gwygraphlabel.c \
	gwyhruler.c \
	gwylayer-basic.c \
	gwylayer-mask.c \
	gwylayer-lines.c \
	gwylayer-points.c \
	gwylayer-select.c \
	gwyruler.c \
	gwyspherecoords.c \
	gwyvectorshade.c \
	gwyvruler.c
#	mylayout.c

# Note these are test programs not to be included in the distro
#noinst_PROGRAMS = main test create-data layout
noinst_PROGRAMS = main test create-data

main_SOURCES = main.c
main_LDADD = libgwydgets.la \
	$(libgwyddion) \
	$(libgwyprocess) \
	$(libgwydraw)

test_SOURCES = test.c
test_LDADD = libgwydgets.la \
	$(libgwyddion) \
	$(libgwyprocess) \
	$(libgwydraw)

create_data_SOURCES = create-data.c
create_data_LDADD = \
	$(libgwyddion) \
	$(libgwyprocess)

#layout_SOURCES = layout.c
#layout_LDADD = libgwydgets.la \
#	$(libgwyddion) \
#	$(libgwyprocess) \
#	$(libgwydraw)

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Gwydgets\"

clean-local:
	rm -f core.* *~
