# @(#) $Id$

include_HEADERS = \
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

TEST_PROG_LIBS = libgwydgets.la \
	../libgwyddion/libgwyddion.la \
	../libprocess/libgwyprocess.la \
	../libdraw/libgwydraw.la
main_SOURCES = main.c
test_SOURCES = test.c
main_LDADD = $(TEST_PROG_LIBS)
test_LDADD = $(TEST_PROG_LIBS)
create_data_SOURCES = create-data.c
create_data_LDADD = \
	../libgwyddion/libgwyddion.la \
	../libprocess/libgwyprocess.la

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DGWY_LOG_DOMAIN=\"Gwydgets\" -DG_LOG_DOMAIN=\"Gwydgets\"
libgwydgetsincludedir = $(includedir)/gwyddion/libgwydgets

clean-local:
	rm -f core.* *~
