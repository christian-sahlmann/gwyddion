# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la

libgwymoduleincludedir = $(pkgincludedir)/libgwymodule

libgwymoduleinclude_HEADERS = \
	gwymodule.h \
	gwymodule-file.h \
	gwymodule-graph.h \
	gwymodule-layer.h \
	gwymodule-process.h \
	gwymodule-tool.h \
	gwymodulebrowser.h \
	gwymoduleenums.h \
	gwymoduleloader.h

lib_LTLIBRARIES = libgwymodule.la

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt \
	libgwymodule.def

noinst_HEADERS = \
	gwymoduleinternal.h

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
#libgwymodule_la_LDFLAGS = @GMODULE_LIBS@ @GTK_LIBS@ @BASIC_LIBS@ -version-info 5:0:2
libgwymodule_la_LDFLAGS = @GMODULE_LIBS@ @GTK_LIBS@ @BASIC_LIBS@ -release @LIBRARY_RELEASE@
libgwymodule_la_SOURCES = \
	gwymodule-file.c \
	gwymodule-graph.c \
	gwymodule-layer.c \
	gwymodule-process.c \
	gwymodule-tool.c \
	gwymodulebrowser.c \
	gwymoduleloader.c

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"GwyModule\"

clean-local:
	rm -f core.* *~
