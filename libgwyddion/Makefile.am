# @(#) $Id$

libgwyddionincludedir = $(includedir)/gwyddion/libgwyddion
libgwyddioninclude_HEADERS = \
	gwycontainer.h \
	gwyddion.h \
	gwyentities.h \
	gwymacros.h \
	gwymath.h \
	gwyserializable.h \
	gwyutils.h \
	gwywatchable.h \
	gwysiunit.h

lib_LTLIBRARIES = libgwyddion.la

EXTRA_DIST = \
	makefile.msc \
	libgwyddion.def

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
	gwyutils.c \
	gwywatchable.c \
	gwysiunit.c

#noinst_PROGRAMS = test
#main_SOURCES = \
#	main.c \
#	test.c
#	gwytestser.c
#main_LDADD = libgwyddion.la

#test_SOURCES = test.c

#test_LDADD = libgwyddion.la
	
INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Gwyddion\" -DGWY_MODULE_DIR=\"$(gwymoduledir)\" -DGWY_PIXMAP_DIR=\"$(gwypixmapdir)\" -DGWY_PLUGIN_DIR=\"$(gwyplugindir)\"

clean-local:
	rm -f core.* *~ testser.object
