# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la

libgwymoduleincludedir = $(includedir)/gwyddion/libgwymodule
libgwymoduleinclude_HEADERS = \
	gwymodule.h \
	gwymodule-file.h \
	gwymodule-graph.h \
	gwymodule-process.h \
	gwymodule-tool.h \
	gwymodulebrowser.h \
	gwymoduleloader.h

lib_LTLIBRARIES = libgwymodule.la

EXTRA_DIST = libgwymodule.def

noinst_HEADERS = \
	gwymoduleinternal.h

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwymodule_la_LDFLAGS = @GMODULE_LIBS@ -version-info 0:0:0
libgwymodule_la_SOURCES = \
	gwymodule-file.c \
	gwymodule-graph.c \
	gwymodule-process.c \
	gwymodule-tool.c \
	gwymodulebrowser.c \
	gwymoduleloader.c

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Gwymodule\"

clean-local:
	rm -f core.* *~
