# @(#) $Id$

libgwyddionincludedir = $(includedir)/gwyddion/libgwyddion
libgwyddioninclude_HEADERS = \
	gwycontainer.h \
	gwyddion.h \
	gwydebugobjects.h \
	gwyentities.h \
	gwymacros.h \
	gwymath.h \
	gwymd5.h \
	gwynlfit.h \
	gwyserializable.h \
	gwyutils.h \
	gwysiunit.h \
	gwywatchable.h \
	gwywin32unistd.h

lib_LTLIBRARIES = libgwyddion.la

EXTRA_DIST = \
	makefile.msc \
	libgwyddion.def

noinst_HEADERS = gwytestser.h

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwyddion_la_LDFLAGS = -version-info 4:0:1
libgwyddion_la_SOURCES = \
	gwycontainer.c \
	gwyddion.c \
	gwydebugobjects.c \
	gwyentities.c \
	gwymath.c \
	gwymd5.c \
	gwynlfit.c \
	gwyserializable.c \
	gwyutils.c \
	gwysiunit.c \
	gwywatchable.c

#noinst_PROGRAMS = main
#main_SOURCES = \
#	main.c \
#	gwytestser.c
#
#main_LDADD = libgwyddion.la
#
#noinst_PROGRAMS = test
#test_SOURCES = test.c
#
#test_LDADD = libgwyddion.la

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Gwyddion\" -DGWY_MODULE_DIR=\"$(gwymoduledir)\" -DGWY_PIXMAP_DIR=\"$(gwypixmapdir)\" -DGWY_PLUGIN_DIR=\"$(gwyplugindir)\"

clean-local:
	rm -f core.* *~ testser.object
