# @(#) $Id$

processdir = $(gwymoduledir)/process

module_ldflags = -shared -Wl,-soname

process_PROGRAMS = \
	basicops.so \
	level.so \
	rotate.so \
	scale.so

basicops_so_SOURCES = basicops.c
basicops_so_LDFLAGS = $(module_ldflags),basicops.so

level_so_SOURCES = level.c
level_so_LDFLAGS = $(module_ldflags),level.so

rotate_so_SOURCES = rotate.c
rotate_so_LDFLAGS = $(module_ldflags),rotate.so

scale_so_SOURCES = scale.c
scale_so_LDFLAGS = $(module_ldflags),scale.so

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
