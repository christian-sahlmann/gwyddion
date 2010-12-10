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
	gwyfdcurvepreset.h \
	gwyinventory.h \
	gwymacros.h \
	gwymath.h \
	gwymathfallback.h \
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

if OS_WIN32
no_undefined = -no-undefined
export_symbols = -export-symbols libgwyddion2.def
libgwyddion2_la_DEPENDENCIES = libgwyddion2.def
endif

# Must init
EXTRA_DIST = gwyversion.h.in
CLEANFILES = libgwyddion2.def
MAINTAINERCLEANFILES =
BUILT_SOURCES =

MKENUM_NAME = gwyddiontypes
MKENUM_ID = GWYDDION_TYPES
MKENUM_HFILES = \
	$(srcdir)/gwyddionenums.h \
	$(srcdir)/gwyexpr.h
include $(top_srcdir)/utils/mkenum.mk

libgwyddion2.def: $(libgwyddion2_la_OBJECTS)
	@echo EXPORTS >libgwyddion2.def
	@for lo in $(libgwyddion2_la_OBJECTS); do \
		o=`sed "s/^pic_object='\\(.*\\)'/\\1/;t;d" "$$lo"`; \
		$(NM) --defined-only --demangle --no-sort "$$o" \
		| awk -F' ' '($$2=="T" && $$3 ~ /^gwy_/){print "     ",$$3}'; \
	done | sort >>libgwyddion2.def

AM_CFLAGS = @COMMON_CFLAGS@
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
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
libversion = -version-info 7:0:7
#libversion = -release @LIBRARY_RELEASE@
libgwyddion2_la_LDFLAGS = @BASIC_LIBS@ -export-dynamic $(no_undefined) $(export_symbols) $(libversion)
libgwyddion2_la_SOURCES = \
	gwycontainer.c \
	gwyddion.c \
	gwyddiontypes.c \
	gwydebugobjects.c \
	gwyentities.c \
	gwyenum.c \
	gwyexpr.c \
	gwyfdcurvepreset.c \
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

