# @(#) $Id$

processdir = $(gwyplugindir)/process

process_SCRIPTS = \
	invert_python.py \
	invert_perl.pl \
	yellow.sh

process_PROGRAMS = \
	invert_cpp

invert_cpp_SOURCES = \
	dump.cc \
	invert_cpp.cc

noinst_HEADERS = \
	dump.hh \
	plugin-helper.hh

EXTRA_DIST = $(process_SCRIPTS)
INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Plug-in\"

clean-local:
	rm -f core.* *~
