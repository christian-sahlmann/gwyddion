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
	gwyvruler.h
lib_LTLIBRARIES = libgwydgets.la

noinst_PROGRAMS = main create-data

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
	gwyvruler.c

main_SOURCES = main.c
main_LDADD = libgwydgets.la ../libgwyddion/libgwyddion.la ../libprocess/libprocess.la ../libdraw/libdraw.la

create_data_SOURCES = create-data.c
create_data_LDADD = ../libgwyddion/libgwyddion.la ../libprocess/libprocess.la

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DGWY_LOG_DOMAIN=\"Gwydgets\"
libgwydgetsincludedir = $(includedir)/gwyddion/libgwydgets

clean-local:
	rm -f core.* *~
