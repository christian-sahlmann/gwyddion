include_HEADERS = \
	gwydgets.h \
	gwygradsphere.h \
	gwyspherecoords.h \
	gwyvectorshade.h
lib_LTLIBRARIES = libgwydgets.la

noinst_PROGRAMS = main

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Chaning ifraces     C:   R:   0
libgwydgets_la_LDFLAGS = -version-info 0:0:0
libgwydgets_la_SOURCES = \
	gwygradsphere.c \
	gwyspherecoords.c \
	gwyvectorshade.c

main_SOURCES = \
	main.c
main_LDADD = libgwydgets.la

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DGWY_LOG_DOMAIN=\"Gwydgets\"
gwyddionincludedir = $(includedir)/gwyddion/libgwydgets

clean-local:
	rm -f core.* *~
