# @(#) $Id$

filedir = $(gwyplugindir)/file

file_PROGRAMS = \
	dump

dump_SOURCES = \
	dump.c

EXTRA_DIST =
INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Plug-in\"

clean-local:
	rm -f core.* *~
