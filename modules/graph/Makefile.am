# @(#) $Id$

processdir = $(gwymoduledir)/graph

module_ldflags = -shared -Wl,-soname

process_PROGRAMS = \
	read.so 


read_so_SOURCES = read.c
read_so_LDFLAGS = $(module_ldflags),read.so


INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
