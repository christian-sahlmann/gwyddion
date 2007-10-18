# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw2.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets2.la

libgwymodule2includedir = $(pkgincludedir)/libgwymodule

if MODULE_DEPENDENCIES
libgwymodule_libadd = \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)
endif

libgwymodule2include_HEADERS = \
	gwymodule.h \
	gwymodule-file.h \
	gwymodule-graph.h \
	gwymodule-layer.h \
	gwymodule-process.h \
	gwymodule-tool.h \
	gwymodulebrowser.h \
	gwymoduleenums.h \
	gwymoduleloader.h \
	gwymoduletypes.h

noinst_HEADERS = \
	gwymoduleinternal.h

lib_LTLIBRARIES = libgwymodule2.la

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt \
	libgwymodule2.def

# Must init
CLEANFILES =
MAINTAINERCLEANFILES = \
	makefile.msc \
	libgwymodule2.def
BUILT_SOURCES =

MKENUM_NAME = gwymoduletypes
MKENUM_ID = GWY_MODULE_TYPES
MKENUM_HFILES = \
	$(srcdir)/gwymoduleenums.h \
	$(srcdir)/gwymoduleloader.h \
	$(srcdir)/gwymodule-file.h
include $(top_srcdir)/utils/mkenum.mk

AM_CFLAGS = @COMMON_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir) -DG_LOG_DOMAIN=\"GwyModule\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libgwymodule2_la_LDFLAGS = @GMODULE_LIBS@ @GTK_LIBS@ @BASIC_LIBS@ -version-info 1:1:1
#libgwymodule2_la_LDFLAGS = @GMODULE_LIBS@ @GTK_LIBS@ @BASIC_LIBS@ -release @LIBRARY_RELEASE@
libgwymodule2_la_LIBADD = $(libgwymodule_libadd)

libgwymodule2_la_SOURCES = \
	gwymodule-file.c \
	gwymodule-graph.c \
	gwymodule-layer.c \
	gwymodule-process.c \
	gwymodule-tool.c \
	gwymodulebrowser.c \
	gwymoduleloader.c \
	gwymoduletypes.c

clean-local:
	rm -f core.* *~
