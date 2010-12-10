# @(#) $Id$

filedir = $(libexecdir)/@PACKAGE@/plugins/file

file_PROGRAMS = \
	dump

dump_SOURCES = \
	dump.c

AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Plug-in\"

clean-local:
	rm -f core.* *~
