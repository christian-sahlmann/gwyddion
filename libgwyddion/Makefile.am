# @(#) $Id$

libgwyddionincludedir = $(includedir)/gwyddion/libgwyddion
libgwyddioninclude_HEADERS = \
	gwycontainer.h \
	gwyddion.h \
	gwyentities.h \
	gwymacros.h \
	gwymath.h \
	gwyserializable.h \
	gwywatchable.h

lib_LTLIBRARIES = libgwyddion.la

noinst_PROGRAMS = main
noinst_HEADERS = gwytestser.h

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwyddion_la_LDFLAGS = -version-info 0:0:0
libgwyddion_la_SOURCES = \
	gwycontainer.c \
	gwyentities.c \
	gwymath.c \
	gwyserializable.c \
	gwywatchable.c

main_SOURCES = \
	main.c \
	gwytestser.c
main_LDADD = libgwyddion.la

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Gwyddion\"

clean-local:
	rm -f core.* *~ testser.object
