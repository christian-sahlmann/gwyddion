# @(#) $Id$

libgwyddion2includedir = $(pkgincludedir)/libgwyddion

localedir = $(datadir)/locale

libgwyddion2include_HEADERS = \
	gwycontainer.h \
	gwyddion.h \
	gwyddionenums.h \
	gwyddiontypes.h \
	gwydebugobjects.h \
	gwyentities.h \
	gwyenum.h \
	gwyexpr.h \
	gwyinventory.h \
	gwymacros.h \
	gwymath.h \
	gwymd5.h \
	gwynlfit.h \
	gwynlfitpreset.h \
	gwyresource.h \
	gwyserializable.h \
	gwysiunit.h \
	gwystringlist.h \
	gwyutils.h \
	gwyversion.h

noinst_HEADERS = \
	gwyddioninternal.h

lib_LTLIBRARIES = libgwyddion2.la

EXTRA_DIST = \
	gwyversion.h.in \
	makefile.msc \
	makefile.msc.gwt \
	libgwyddion2.def

# Must init
CLEANFILES =
MAINTAINERCLEANFILES = \
	makefile.msc \
	libgwyddion2.def
BUILT_SOURCES =

MKENUM_NAME = gwyddiontypes
MKENUM_HFILES = gwyddionenums.h
include $(top_srcdir)/utils/mkenum.mk

AM_CFLAGS = @COMMON_CFLAGS@
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-DG_LOG_DOMAIN=\"Gwyddion\" \
	-DGWY_LIBDIR=\"$(libdir)\" \
	-DGWY_LIBEXECDIR=\"$(libexecdir)\" \
	-DGWY_DATADIR=\"$(datadir)\" \
	-DGWY_LOCALEDIR=\"$(localedir)\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwyddion2_la_LDFLAGS = @BASIC_LIBS@ -version-info 2:0:2
#libgwyddion2_la_LDFLAGS = @BASIC_LIBS@ -release @LIBRARY_RELEASE@
libgwyddion2_la_SOURCES = \
	gwycontainer.c \
	gwyddion.c \
	gwyddiontypes.c \
	gwydebugobjects.c \
	gwyentities.c \
	gwyenum.c \
	gwyexpr.c \
	gwyinventory.c \
	gwymath.c \
	gwymd5.c \
	gwynlfit.c \
	gwynlfitpreset.c \
	gwyresource.c \
	gwyserializable.c \
	gwysiunit.c \
	gwystringlist.c \
	gwyutils.c \
	gwyversion.c

clean-local:
	rm -f core.* *~

