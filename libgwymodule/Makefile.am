# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la

libgwymoduleincludedir = $(includedir)/gwyddion/libgwymodule
libgwymoduleinclude_HEADERS = \
	gwymodule.h \
	gwymodulebrowser.h

lib_LTLIBRARIES = libgwymodule.la

#noinst_PROGRAMS = main create-data

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwymodule_la_LDFLAGS = @GMODULE_LIBS@ -version-info 0:0:0
libgwymodule_la_SOURCES = \
	gwymodule.c \
	gwymodulebrowser.c

#main_SOURCES = main.c
#main_LDADD = libgwymodule.la ../libgwyddion/libgwyddion.la

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Gwymodule\"

clean-local:
	rm -f core.* *~
