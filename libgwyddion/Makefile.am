# @(#) $Id$

libgwyddionincludedir = $(pkgincludedir)/libgwyddion

libgwyddioninclude_HEADERS = \
	gwycontainer.h \
	gwyddion.h \
	gwydebugobjects.h \
	gwyentities.h \
	gwyexpr.h \
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
#libgwyddion_la_LDFLAGS = -version-info 8:0:5
libgwyddion_la_LDFLAGS = -release @LIBRARY_RELEASE@
libgwyddion_la_SOURCES = \
	gwycontainer.c \
	gwyddion.c \
	gwydebugobjects.c \
	gwyentities.c \
	gwyexpr.c \
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

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Gwyddion\" -DGWY_MODULE_DIR=\"$(pkglibdir)/modules\" -DGWY_PIXMAP_DIR=\"$(pkgdatadir)/pixmaps\" -DGWY_PLUGIN_DIR=\"$(libexecdir)/@PACKAGE@/plugins\"

clean-local:
	rm -f core.* *~ testser.object
