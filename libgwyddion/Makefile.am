# @(#) $Id$

libgwyddionincludedir = $(pkgincludedir)/libgwyddion

libgwyddioninclude_HEADERS = \
	gwycontainer.h \
	gwyddion.h \
	gwyddionenums.h \
	gwyddiontypes.h \
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
	gwywatchable.h

built_sources = \
	gwyddiontypes.h \
	gwyddiontypes.c

lib_LTLIBRARIES = libgwyddion.la

EXTRA_DIST = \
	$(built_sources) \
	gwyddiontypes.c.template \
	gwyddiontypes.h.template \
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

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Gwyddion\" -DGWY_MODULE_DIR=\"$(pkglibdir)/modules\" -DGWY_PIXMAP_DIR=\"$(pkgdatadir)/pixmaps\" -DGWY_PLUGIN_DIR=\"$(libexecdir)/@PACKAGE@/plugins\"

clean-local:
	rm -f core.* *~ testser.object *.xgen

if MAINTAINER_MODE
enum_headers = gwyddionenums.h
stamp_files = stamp-gwyddionenums.h

MAINTAINERCLEANFILES = $(built_sources) $(stamp_files)

BUILT_SOURCES = $(built_sources)

gwyddiontypes.h: stamp-gwyddiontypes.h
	@true

stamp-gwyddiontypes.h: $(enum_headers) gwyddiontypes.h.template
	( cd $(srcdir) \
	  && glib-mkenums --template gwyddiontypes.h.template $(enum_headers) ) \
	  >gwyddiontypes.h.xgen \
	&& (cmp -s gwyddiontypes.h.xgen gwyddiontypes.h \
		|| cp gwyddiontypes.h.xgen gwyddiontypes.h ) \
	&& rm -f gwyddiontypes.h.xgen \
	&& echo timestamp >stamp-gwyddiontypes.h

gwyddiontypes.c: $(enum_headers) gwyddiontypes.c.template
	( cd $(srcdir) \
	  && glib-mkenums --template gwyddiontypes.c.template $(enum_headers) ) \
	>gwyddiontypes.c.xgen \
	&& cp gwyddiontypes.c.xgen gwyddiontypes.c  \
	&& rm -f gwyddiontypes.c.xgen
endif

#noinst_PROGRAMS = main
#main_SOURCES = \
#	main.c \
#	gwytestser.c
#
#main_LDADD = libgwyddion.la
