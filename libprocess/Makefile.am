# @(#) $Id$

include_HEADERS = \
	datafield.h \
	dataline.h \
	interpolation.h \
	simplefft.h
lib_LTLIBRARIES = libprocess.la

noinst_PROGRAMS = test

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libprocess_la_LDFLAGS = -version-info 0:0:0
libprocess_la_SOURCES = \
	datafield.c \
	dataline.c \
	interpolation.c \
	simplefft.c

test_SOURCES = \
	test.c
test_LDADD = libprocess.la ../libgwyddion/libgwyddion.la

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DGWY_LOG_DOMAIN=\"Process\"
libprocessincludedir = $(includedir)/gwyddion/libprocess

clean-local:
	rm -f core.* *~

