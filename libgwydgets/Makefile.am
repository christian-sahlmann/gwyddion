# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la

libgwydgetsincludedir = $(includedir)/gwyddion/libgwydgets
libgwydgetsinclude_HEADERS = \
	gwydgets.h \
	gwydataview.h \
	gwydataviewlayer.h \
	gwydatawindow.h \
	gwygradsphere.h \
	gwyhruler.h \
	gwylayer-basic.h \
	gwylayer-select.h \
	gwyruler.h \
	gwyspherecoords.h \
	gwyvectorshade.h \
	gwyvruler.h \
	gwygraph.h \
	gwygrapharea.h \
	gwygraphlabel.h \
	gwygraphcorner.h \
	gwyaxis.h

lib_LTLIBRARIES = libgwydgets.la

noinst_PROGRAMS = main test create-data

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwydgets_la_LDFLAGS = -version-info 0:0:0
libgwydgets_la_SOURCES = \
	gwydataview.c \
	gwydataviewlayer.c \
	gwydatawindow.c \
	gwygradsphere.c \
	gwyhruler.c \
	gwylayer-basic.c \
	gwylayer-select.c \
	gwyruler.c \
	gwyspherecoords.c \
	gwyvectorshade.c \
	gwyvruler.c \
	gwygraph.c \
	gwygrapharea.c \
	gwygraphlabel.c \
	gwygraphcorner.c \
	gwyaxis.c

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

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DGWY_LOG_DOMAIN=\"Gwydgets\" -DG_LOG_DOMAIN=\"Gwydgets\"

clean-local:
	rm -f core.* *~
