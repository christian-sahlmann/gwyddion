# @(#) $Id$

processdir = $(gwyplugindir)/process

process_SCRIPTS = \
	invert_python.py \
	invert_perl.pl \
	yellow.sh

EXTRA_DIST = $(process_SCRIPTS)
INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Plug-in\"

clean-local:
	rm -f core.* *~
