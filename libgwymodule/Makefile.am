# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la

libgwymoduleincludedir = $(includedir)/gwyddion/libgwymodule
libgwymoduleinclude_HEADERS = \
	gwymodule.h \
	gwymodule-file.h \
	gwymodule-process.h \
	gwymodulebrowser.h \
	gwymoduleloader.h

lib_LTLIBRARIES = libgwymodule.la

noinst_HEADERS = \
	gwymoduleinternal.h

#noinst_PROGRAMS = main create-data

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwymodule_la_LDFLAGS = @GMODULE_LIBS@ -version-info 0:0:0
libgwymodule_la_SOURCES = \
	gwymodule-file.c \
	gwymodule-process.c \
	gwymodulebrowser.c \
	gwymoduleloader.c

#main_SOURCES = main.c
#main_LDADD = libgwymodule.la ../libgwyddion/libgwyddion.la

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Gwymodule\"

clean-local:
	rm -f core.* *~
