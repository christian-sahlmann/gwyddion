# @(#) $Id$

include_HEADERS = \
	gwypalette.h \
	gwypalettedef.h \
	gwypixfield.h
lib_LTLIBRARIES = libdraw.la

noinst_PROGRAMS = test

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libdraw_la_LDFLAGS = -version-info 0:0:0
libdraw_la_SOURCES = \
	gwypalette.c \
	gwypalettedef.c \
	gwypixfield.c

test_SOURCES = \
	test.c
test_LDADD = libdraw.la ../libgwyddion/libgwyddion.la ../libprocess/libprocess.la

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Draw\"
libdrawincludedir = $(includedir)/gwyddion/libdraw

clean-local:
	rm -f core.* *~

