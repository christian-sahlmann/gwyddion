# @(#) $Id$

processdir = $(gwyplugindir)/process

process_SCRIPTS = \
	divide_by_2.py \
	invert_python.py \
	invert_perl.pl \
	cutoff.pl \
	yellow.sh

EXTRA_DIST = $(process_SCRIPTS)
INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Plug-in\"

clean-local:
	rm -f core.* *~
